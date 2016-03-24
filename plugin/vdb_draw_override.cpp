#include "vdb_draw_override.h"

#include <maya/MFnDependencyNode.h>
#include <maya/MDrawContext.h>

#include <vector>

#include "../util/maya_utils.hpp"

namespace MHWRender {

    namespace{

        class DrawData : public MUserData {
        private:
            MBoundingBox m_bbox;
            std::vector<MFloatVector> m_wireframe;
        public:
            DrawData() : MUserData(false)
            {

            }

            void clear()
            {
                std::vector<MFloatVector>().swap(m_wireframe);
            }

            void quick_reserve(size_t num_vertices)
            {
                if (m_wireframe.size() != num_vertices)
                    clear();
                else
                    m_wireframe.clear();

                m_wireframe.reserve(num_vertices);
            }

            void add_wire_bounding_box(const MFloatVector& mn, const MFloatVector& mx)
            {
                m_wireframe.push_back(MFloatVector(mn.x, mn.y, mn.z));
                m_wireframe.push_back(MFloatVector(mx.x, mn.y, mn.z));

                m_wireframe.push_back(MFloatVector(mn.x, mx.y, mn.z));
                m_wireframe.push_back(MFloatVector(mx.x, mx.y, mn.z));

                m_wireframe.push_back(MFloatVector(mn.x, mn.y, mx.z));
                m_wireframe.push_back(MFloatVector(mx.x, mn.y, mx.z));

                m_wireframe.push_back(MFloatVector(mn.x, mx.y, mx.z));
                m_wireframe.push_back(MFloatVector(mx.x, mx.y, mx.z));

                m_wireframe.push_back(MFloatVector(mn.x, mn.y, mn.z));
                m_wireframe.push_back(MFloatVector(mn.x, mx.y, mn.z));

                m_wireframe.push_back(MFloatVector(mx.x, mn.y, mn.z));
                m_wireframe.push_back(MFloatVector(mx.x, mx.y, mn.z));

                m_wireframe.push_back(MFloatVector(mn.x, mn.y, mx.z));
                m_wireframe.push_back(MFloatVector(mn.x, mx.y, mx.z));

                m_wireframe.push_back(MFloatVector(mx.x, mn.y, mx.z));
                m_wireframe.push_back(MFloatVector(mx.x, mx.y, mx.z));

                m_wireframe.push_back(MFloatVector(mn.x, mn.y, mn.z));
                m_wireframe.push_back(MFloatVector(mn.x, mn.y, mx.z));

                m_wireframe.push_back(MFloatVector(mn.x, mx.y, mn.z));
                m_wireframe.push_back(MFloatVector(mn.x, mx.y, mx.z));

                m_wireframe.push_back(MFloatVector(mx.x, mn.y, mn.z));
                m_wireframe.push_back(MFloatVector(mx.x, mn.y, mx.z));

                m_wireframe.push_back(MFloatVector(mx.x, mx.y, mn.z));
                m_wireframe.push_back(MFloatVector(mx.x, mx.y, mx.z));
            }

            void update(VDBVisualizerData* vdb_data)
            {
                if (vdb_data == 0)
                    return;
                m_bbox = vdb_data->bbox;
                if (vdb_data->display_mode == DISPLAY_AXIS_ALIGNED_BBOX)
                {
                    quick_reserve(24);

                    add_wire_bounding_box(m_bbox.min(), m_bbox.max());
                }
                else if (vdb_data->display_mode == DISPLAY_GRID_BBOX)
                {
                    if (vdb_data->vdb_file == nullptr || !vdb_data->vdb_file->isOpen())
                    {
                        clear();
                        return;
                    }

                    openvdb::GridPtrVecPtr grids = vdb_data->vdb_file->readAllGridMetadata();
                    const size_t num_vertices = grids->size() * 24;
                    if (num_vertices == 0)
                    {
                        clear();
                        return;
                    }

                    quick_reserve(num_vertices);

                    MFloatVector mn(0.0f, 0.0f, 0.0f), mx(0.0f, 0.0f, 0.0f);
                    for (openvdb::GridPtrVec::const_iterator it = grids->begin(); it != grids->end(); ++it)
                    {
                        if (openvdb::GridBase::ConstPtr grid = *it)
                        {
                            // TODO : do things properly
                            static std::array<MFloatVector, 8> vertices;
                            if (read_grid_transformed_bbox_wire(grid, vertices))
                            {
                                m_wireframe.push_back(vertices[0]);
                                m_wireframe.push_back(vertices[4]);

                                m_wireframe.push_back(vertices[1]);
                                m_wireframe.push_back(vertices[5]);

                                m_wireframe.push_back(vertices[2]);
                                m_wireframe.push_back(vertices[6]);

                                m_wireframe.push_back(vertices[3]);
                                m_wireframe.push_back(vertices[7]);

                                m_wireframe.push_back(vertices[0]);
                                m_wireframe.push_back(vertices[1]);

                                m_wireframe.push_back(vertices[1]);
                                m_wireframe.push_back(vertices[2]);

                                m_wireframe.push_back(vertices[2]);
                                m_wireframe.push_back(vertices[3]);

                                m_wireframe.push_back(vertices[3]);
                                m_wireframe.push_back(vertices[0]);

                                m_wireframe.push_back(vertices[4]);
                                m_wireframe.push_back(vertices[5]);

                                m_wireframe.push_back(vertices[5]);
                                m_wireframe.push_back(vertices[6]);

                                m_wireframe.push_back(vertices[6]);
                                m_wireframe.push_back(vertices[7]);

                                m_wireframe.push_back(vertices[7]);
                                m_wireframe.push_back(vertices[4]);
                            }
                        }
                    }
                }
                else if (m_wireframe.size())
                    clear();
            }

            void draw(const MDrawContext& context) const
            {
                MStatus status;
                MBoundingBox frustum_box = context.getFrustumBox(&status);

                if (status && !frustum_box.intersects(m_bbox))
                    return;

                glBegin(GL_LINES);

                for (const auto& vertex : m_wireframe)
                    glVertex3f(vertex.x, vertex.y, vertex.z);

                glEnd();
            }
        };

    }

    VDBDrawOverride::VDBDrawOverride(const MObject& obj) : MPxDrawOverride(obj, draw_callback)
    {
        MFnDependencyNode dnode(obj);
        p_vdb_visualizer = dynamic_cast<VDBVisualizerShape*>(dnode.userNode());
    }

    VDBDrawOverride::~VDBDrawOverride()
    {

    }

    MPxDrawOverride* VDBDrawOverride::creator(const MObject& obj)
    {
        return new VDBDrawOverride(obj);
    }

    void VDBDrawOverride::draw_callback(const MDrawContext& context, const MUserData* data)
    {
        const DrawData* draw_data = reinterpret_cast<const DrawData*>(data);
        if (draw_data != 0)
            draw_data->draw(context);
    }

    MString VDBDrawOverride::registrantId("VDBVisualizerDrawOverride");

    MHWRender::DrawAPI VDBDrawOverride::supportedDrawAPIs() const
    {
        return kOpenGL;
    }

    MUserData* VDBDrawOverride::prepareForDraw(
            const MDagPath&,
            const MDagPath&,
            const MFrameContext&,
            MUserData* oldData)
    {
        DrawData* data = oldData == 0 ? new DrawData() : reinterpret_cast<DrawData*>(oldData);

        data->update(p_vdb_visualizer->get_update());

        return data;
    }

    bool VDBDrawOverride::isBounded(
            const MDagPath&,
            const MDagPath&) const
    {
        return false;
    }

    bool VDBDrawOverride::init_shaders()
    {
        return true;
    }

}