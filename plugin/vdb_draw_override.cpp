/*
 * TODO
 * * Replace the stipple call with a shader that can do the stippling or whatever
 *   in world space.
 * * Use proper opengl functions, avoid the old pipe
 */

#include "vdb_draw_override.h"

#include <maya/MFnDependencyNode.h>
#include <maya/MDrawContext.h>
#include <maya/MHWGeometryUtilities.h>

#include <vector>
#include <random>

#include "../util/maya_utils.hpp"

namespace MHWRender {

    namespace{

        class DrawData : public MUserData {
        private:
            MBoundingBox m_bbox;
            float m_wireframe_color[4];

            std::vector<MFloatVector> m_wireframe;
            std::vector<MFloatVector> m_points;

            float m_point_size;

            VDBDisplayMode m_display_mode;
            bool m_is_empty;
        public:
            DrawData() : MUserData(false), m_is_empty(false)
            {

            }

            void clear_wireframe()
            {
                std::vector<MFloatVector>().swap(m_wireframe);
            }

            void clear_points()
            {
                std::vector<MFloatVector>().swap(m_points);
            }

            void quick_reserve(size_t num_vertices)
            {
                if (m_wireframe.size() != num_vertices)
                    clear_wireframe();
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
                m_display_mode = vdb_data->display_mode;

                if (m_is_empty || vdb_data->display_mode == DISPLAY_AXIS_ALIGNED_BBOX)
                {
                    clear_points();
                    m_display_mode = DISPLAY_AXIS_ALIGNED_BBOX; // to set when it's empty
                    quick_reserve(24);
                    add_wire_bounding_box(m_bbox.min(), m_bbox.max());
                }
                else if (vdb_data->display_mode == DISPLAY_GRID_BBOX)
                {
                    clear_points();
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
                else if (vdb_data->display_mode == DISPLAY_POINT_CLOUD)
                {
                    openvdb::GridPtrVecPtr grids = vdb_data->vdb_file->readAllGridMetadata();
                    if (grids->size() == 0)
                    {
                        clear_points();
                        return;
                    }

                    m_point_size = vdb_data->point_size;

                    openvdb::GridBase::ConstPtr grid = *grids->begin();

                    for (openvdb::GridPtrVec::const_iterator it = grids->begin(); it != grids->end(); ++it)
                    {
                        if (openvdb::GridBase::ConstPtr g = *it)
                        {
                            if (g->getName() == "density")
                            {
                                grid = g;
                                break;
                            }
                        }
                    }

                    // we don't know the number points beforehand, and later on calculating the
                    // required number of points precisely (ie executing a shader) could prove to be really costly
                    // so doing that twice is not an option, we are going to rely on the vector tricks, but we are loosing perf
                    // at the first run
                    m_points.clear();
                    const float point_jitter = vdb_data->point_jitter;
                    const float do_jitter = point_jitter > 0.001f;

                    openvdb::Vec3d voxel_size = grid->voxelSize();

                    std::minstd_rand generator; // LCG
                    std::uniform_real_distribution<float> distributionX(-point_jitter * static_cast<float>(voxel_size.x()), point_jitter * static_cast<float>(voxel_size.x()));
                    std::uniform_real_distribution<float> distributionY(-point_jitter * static_cast<float>(voxel_size.y()), point_jitter * static_cast<float>(voxel_size.y()));
                    std::uniform_real_distribution<float> distributionZ(-point_jitter * static_cast<float>(voxel_size.z()), point_jitter * static_cast<float>(voxel_size.z()));

                    if (grid->valueType() == "float")
                    {
                        openvdb::FloatGrid::ConstPtr grid_data = openvdb::gridConstPtrCast<openvdb::FloatGrid>(
                                vdb_data->vdb_file->readGrid(grid->getName()));
                        openvdb::math::Transform transform = grid_data->transform();

                        for (auto iter = grid_data->beginValueOn(); iter; ++iter)
                        {
                            const double value = static_cast<double>(iter.getValue());
                            if (value > 0.0f)
                            {
                                openvdb::Vec3d vdb_pos = transform.indexToWorld(iter.getCoord());
                                MFloatVector pos(static_cast<float>(vdb_pos.x()), static_cast<float>(vdb_pos.y()),
                                                 static_cast<float>(vdb_pos.z()));
                                if (do_jitter)
                                {
                                    pos.x += distributionX(generator);
                                    pos.y += distributionY(generator);
                                    pos.z += distributionZ(generator);
                                }
                                m_points.push_back(pos);
                            }
                        }
                    }
                    m_points.shrink_to_fit();
                }
                else if (m_wireframe.size())
                    clear_wireframe();
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

                if (m_display_mode == DISPLAY_AXIS_ALIGNED_BBOX || m_display_mode == DISPLAY_GRID_BBOX)
                {
                    glBegin(GL_LINES);

                    for (const auto& vertex : m_wireframe)
                        glVertex3f(vertex.x, vertex.y, vertex.z);

                    glEnd();
                }
                else if (m_display_mode == DISPLAY_POINT_CLOUD && m_points.size() > 0)
                {
                    // TODO: what did I fuck up with the vertex pointer???
                    /*glEnableClientState(GL_VERTEX_ARRAY_POINTER);

                    glVertexPointer(3, GL_FLOAT, 0, &m_points[0]);

                    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_points.size()));

                    glDisableClientState(GL_VERTEX_ARRAY_POINTER);*/

                    glPointSize(m_point_size);

                    glBegin(GL_POINTS);

                    for (const auto& vertex : m_points)
                        glVertex3f(vertex.x, vertex.y, vertex.z);

                    glEnd();
                }

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
