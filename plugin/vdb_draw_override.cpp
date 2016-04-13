/*
 * TODO
 * * Replace the stipple call with a shader that can do the stippling or whatever
 *   in world space.
 */

#include "vdb_draw_override.h"

#include <maya/MFnDependencyNode.h>
#include <maya/MDrawContext.h>
#include <maya/MHWGeometryUtilities.h>

#include <vector>

#include "../util/maya_utils.hpp"

namespace MHWRender {

    namespace{

        class DrawData : public MUserData {
        private:
            MBoundingBox m_bbox;
            float m_wireframe_color[4];
            std::vector<MFloatVector> m_wireframe;
            bool m_is_empty;
        public:
            DrawData() : MUserData(false), m_is_empty(false)
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

            void update(const MDagPath& obj_path, VDBVisualizerData* vdb_data)
            {
                MColor color = MHWRender::MGeometryUtilities::wireframeColor(obj_path);
                m_wireframe_color[0] = color.r;
                m_wireframe_color[1] = color.g;
                m_wireframe_color[2] = color.b;
                m_wireframe_color[3] = color.a;

                if (vdb_data == 0)
                    return;
                m_bbox = vdb_data->bbox;
                m_is_empty = vdb_data->vdb_file == nullptr || !vdb_data->vdb_file->isOpen();

                if (m_is_empty || vdb_data->display_mode == DISPLAY_AXIS_ALIGNED_BBOX)
                {
                    quick_reserve(24);
                    add_wire_bounding_box(m_bbox.min(), m_bbox.max());
                }
                else if (vdb_data->display_mode == DISPLAY_GRID_BBOX)
                {
                    openvdb::GridPtrVecPtr grids = vdb_data->vdb_file->readAllGridMetadata();
                    const size_t num_vertices = grids->size() * 24;
                    if (num_vertices == 0)
                    {
                        m_is_empty = true;
                        m_bbox = MBoundingBox(MPoint(-1.0, -1.0, -1.0), MPoint(1.0, 1.0, 1.0));
                        quick_reserve(24);
                        add_wire_bounding_box(m_bbox.min(), m_bbox.max());
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

                float world_view_mat[4][4];
                context.getMatrix(MHWRender::MDrawContext::kWorldViewMtx).get(world_view_mat);

                glMatrixMode(GL_MODELVIEW);
                glPushMatrix();
                glLoadMatrixf(&world_view_mat[0][0]);

                glPushAttrib(GL_CURRENT_BIT);

                if (m_is_empty)
                {
                    glPushAttrib(GL_ENABLE_BIT);
                    glLineStipple(4, 0xAAAA);
                    glEnable(GL_LINE_STIPPLE);
                    glColor4f(0.5f, 0.5f, 0.5f, 1.0f);
                }
                else
                    glColor4fv(m_wireframe_color);

                glBegin(GL_LINES);

                for (const auto& vertex : m_wireframe)
                    glVertex3f(vertex.x, vertex.y, vertex.z);

                glEnd();

                if (m_is_empty)
                    glPopAttrib();

                glPopAttrib();

                glPopMatrix();
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
            const MDagPath& obj_path,
            const MDagPath&,
            const MFrameContext&,
            MUserData* oldData)
    {
        DrawData* data = oldData == 0 ? new DrawData() : reinterpret_cast<DrawData*>(oldData);

        data->update(obj_path, p_vdb_visualizer->get_update());

        return data;
    }

    bool VDBDrawOverride::isBounded(
            const MDagPath&,
            const MDagPath&) const
    {
        return false;
    }

    MBoundingBox VDBDrawOverride::boundingBox(
            const MDagPath&,
            const MDagPath&) const
    {
        return p_vdb_visualizer->boundingBox();
    }

    bool VDBDrawOverride::init_shaders()
    {
        return true;
    }
}
