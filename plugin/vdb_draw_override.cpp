/*
 * TODO
 * * Replace the stipple call with a shader that can do the stippling or whatever
 *   in world space.
 * * Use proper opengl functions, avoid the old pipe
 */

#include <GL/glew.h>

#include "vdb_draw_override.h"

#include <maya/MFnDependencyNode.h>
#include <maya/MDrawContext.h>
#include <maya/MHWGeometryUtilities.h>

#include <vector>
#include <random>

#include <tbb/parallel_sort.h>
#include <tbb/task_scheduler_init.h>

#include <openvdb/tools/Interpolation.h>

#include "../util/maya_utils.hpp"

namespace {
    struct Point{
        MFloatVector position;
        MColor color;
    };

    class RGBSampler {
    public:
        RGBSampler()
        { }

        virtual ~RGBSampler()
        { }

        virtual MColor get_rgb(const openvdb::Vec3d&) const
        {
            return MColor(1.0f, 1.0f, 1.0f, 1.0f);
        }
    };

    class FloatToRGBSampler : public RGBSampler {
    private:
        typedef openvdb::tools::GridSampler<openvdb::FloatGrid, openvdb::tools::BoxSampler> sampler_type;
        sampler_type* p_sampler;
    public:
        FloatToRGBSampler(openvdb::FloatGrid::ConstPtr grid)
        {
            p_sampler = new sampler_type(*grid);
        }

        ~FloatToRGBSampler()
        {
            delete p_sampler;
        }

        MColor get_rgb(const openvdb::Vec3d& wpos) const
        {
            const float value = p_sampler->wsSample(wpos);
            return MColor(value, value, value, 1.0f);
        }
    };

    class Vec3SToRGBSampler : public RGBSampler {
    private:
        typedef openvdb::tools::GridSampler<openvdb::Vec3SGrid, openvdb::tools::BoxSampler> sampler_type;
        sampler_type* p_sampler;
    public:
        Vec3SToRGBSampler(openvdb::Vec3SGrid::ConstPtr grid)
        {
            p_sampler = new sampler_type(*grid);
        }

        ~Vec3SToRGBSampler()
        {
            delete p_sampler;
        }

        MColor get_rgb(const openvdb::Vec3d& wpos) const
        {
            const openvdb::Vec3s value = p_sampler->wsSample(wpos);
            return MColor(value.x(), value.y(), value.z(), 1.0f);
        }
    };

    class FloatVoxelIterator {
    public:
        FloatVoxelIterator()
        { }

        virtual ~FloatVoxelIterator()
        { }

        virtual bool is_valid() const
        {
            return false;
        }

        virtual void get_next()
        { }

        virtual float get_value() const
        {
            return 0.0f;
        }

        virtual openvdb::Coord get_coord() const
        {
            return openvdb::Coord(0, 0, 0);
        }
    };

    class FloatToFloatVoxelIterator : public FloatVoxelIterator {
        typedef openvdb::FloatGrid grid_type;
        grid_type::ValueOnCIter iter;
    public:
        FloatToFloatVoxelIterator(grid_type::ConstPtr grid) : iter(grid->beginValueOn())
        { }

        ~FloatToFloatVoxelIterator()
        { }

        bool is_valid() const
        {
            return iter.test();
        }

        void get_next()
        {
            ++iter;
        }

        float get_value() const
        {
            return iter.getValue();
        }

        openvdb::Coord get_coord() const
        {
            return iter.getCoord();
        }
    };

    class Vec3SToFloatVoxelIterator : public FloatVoxelIterator {
        typedef openvdb::Vec3SGrid grid_type;
        grid_type::ValueOnCIter iter;
    public:
        Vec3SToFloatVoxelIterator(grid_type::ConstPtr grid) : iter(grid->cbeginValueOn())
        { }

        ~Vec3SToFloatVoxelIterator()
        { }

        bool is_valid() const
        {
            return iter.test();
        }

        void get_next()
        {
            ++iter;
        }

        float get_value() const
        {
            openvdb::Vec3s value = iter.getValue();
            return (value.x() + value.y() + value.z()) / 3.0f;
        }

        openvdb::Coord get_coord() const
        {
            return iter.getCoord();
        }
    };
}

namespace MHWRender {

    namespace{

        class DrawData : public MUserData {
        private:
            MBoundingBox m_bbox;
            float m_wireframe_color[4];

            std::vector<MFloatVector> m_wireframe;
            std::vector<Point> m_points;

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
                std::vector<Point>().swap(m_points);
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

                auto push_back_wireframe = [&] (openvdb::GridBase::ConstPtr grid) {
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
                };

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
                            push_back_wireframe(grid);
                    }
                }
                else if (vdb_data->display_mode == DISPLAY_POINT_CLOUD)
                {
                    m_points.clear();

                    openvdb::GridBase::ConstPtr attenuation_grid = 0;
                    try
                    {
                        attenuation_grid = vdb_data->vdb_file->readGrid(vdb_data->attenuation_channel);
                    }
                    catch(...)
                    {
                        attenuation_grid = 0;
                    }

                    if (attenuation_grid == 0)
                    {
                        clear_points();
                        quick_reserve(24);
                        add_wire_bounding_box(m_bbox.min(), m_bbox.max());
                        return;
                    }

                    quick_reserve(24);
                    push_back_wireframe(attenuation_grid);

                    openvdb::GridBase::ConstPtr scattering_grid = 0;

                    if (vdb_data->scattering_channel == vdb_data->attenuation_channel)
                        scattering_grid = attenuation_grid;
                    else
                    {
                        try
                        {
                            scattering_grid = vdb_data->vdb_file->readGrid(vdb_data->scattering_channel);
                        }
                        catch(...)
                        {
                            scattering_grid = 0;
                        }
                    }

                    m_point_size = vdb_data->point_size;

                    size_t active_voxel_count = attenuation_grid->activeVoxelCount() / (vdb_data->point_skip + 1);
                    m_points.reserve(active_voxel_count);

                    const float point_jitter = vdb_data->point_jitter;
                    const float do_jitter = point_jitter > 0.001f;

                    openvdb::Vec3d voxel_size = attenuation_grid->voxelSize();

                    std::minstd_rand generator; // LCG
                    std::uniform_real_distribution<float> distributionX(-point_jitter * static_cast<float>(voxel_size.x()), point_jitter * static_cast<float>(voxel_size.x()));
                    std::uniform_real_distribution<float> distributionY(-point_jitter * static_cast<float>(voxel_size.y()), point_jitter * static_cast<float>(voxel_size.y()));
                    std::uniform_real_distribution<float> distributionZ(-point_jitter * static_cast<float>(voxel_size.z()), point_jitter * static_cast<float>(voxel_size.z()));

                    MColor point_color(vdb_data->scattering_color.r, vdb_data->scattering_color.g, vdb_data->scattering_color.b,
                                       (vdb_data->attenuation_color.r + vdb_data->attenuation_color.g + vdb_data->attenuation_color.b) / 3.0f);

                    openvdb::math::Transform attenuation_transform = attenuation_grid->transform();

                    RGBSampler* scattering_sampler = 0;

                    if (scattering_grid == 0)
                        scattering_sampler = new RGBSampler();
                    else
                    {
                        if (scattering_grid->valueType() == "float")
                            scattering_sampler = new FloatToRGBSampler(openvdb::gridConstPtrCast<openvdb::FloatGrid>(scattering_grid));
                        else if (scattering_grid->valueType() == "vec3s")
                            scattering_sampler = new Vec3SToRGBSampler(openvdb::gridConstPtrCast<openvdb::Vec3SGrid>(scattering_grid));
                        else
                            scattering_sampler = new RGBSampler();
                    }

                    FloatVoxelIterator* iter = 0;

                    if (attenuation_grid->valueType() == "float")
                        iter = new FloatToFloatVoxelIterator(openvdb::gridConstPtrCast<openvdb::FloatGrid>(attenuation_grid));
                    else if (attenuation_grid->valueType() == "vec3s")
                        iter = new Vec3SToFloatVoxelIterator(openvdb::gridConstPtrCast<openvdb::Vec3SGrid>(attenuation_grid));
                    else
                        iter = new FloatVoxelIterator();

                    int point_id = 0;
                    for (; iter->is_valid(); iter->get_next())
                    {
                        if ((point_id++ % vdb_data->point_skip) != 0)
                            continue;
                        const MColor attenuation = vdb_data->attenuation_gradient.evaluate(iter->get_value() * point_color.a);
                        const float value = (attenuation.r + attenuation.g + attenuation.b) / 3.0f;
                        if (value > 0.0f)
                        {
                            openvdb::Vec3d vdb_pos = attenuation_transform.indexToWorld(iter->get_coord());
                            MFloatVector pos(static_cast<float>(vdb_pos.x()), static_cast<float>(vdb_pos.y()),
                                             static_cast<float>(vdb_pos.z()));
                            if (do_jitter)
                            {
                                pos.x += distributionX(generator);
                                pos.y += distributionY(generator);
                                pos.z += distributionZ(generator);
                            }
                            Point point;
                            point.position = pos;
                            point.color.r = point_color.r;
                            point.color.g = point_color.g;
                            point.color.b = point_color.b;
                            point.color.a = value;
                            point.color *= vdb_data->scattering_gradient.evaluate(scattering_sampler->get_rgb(vdb_pos));
                            m_points.push_back(point);
                        }
                    }

                    delete scattering_sampler;

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


                glBegin(GL_LINES);

                for (const auto& vertex : m_wireframe)
                    glVertex3f(vertex.x, vertex.y, vertex.z);

                glEnd();

                if (m_display_mode == DISPLAY_POINT_CLOUD && m_points.size() > 0)
                {
                    // TODO: something is wrong with the vertex array pointers, some shared gl state is fucked up
                    // this will be avoided once I move to shaders and direct state access
                    glColor3f(1.0f, 1.0f, 1.0f);
                    glPointSize(m_point_size);
                    glEnable(GL_POINT_SMOOTH);

                    glDepthMask(GL_FALSE);
                    glEnable(GL_DEPTH_TEST);
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                    glBegin(GL_POINTS);

                    for (auto point : m_points)
                    {
                        glColor4fv(&point.color.r);
                        glVertex3fv(&point.position.x);
                    }

                    glEnd();

                    glDisable(GL_DEPTH_TEST);
                    glDisable(GL_BLEND);
                    glDisable(GL_POINT_SMOOTH);
                    glDepthMask(GL_TRUE);
                }

                if (m_is_empty)
                    glPopAttrib();

                glPopAttrib();

                glPopMatrix();
            }

            void sort(const MDagPath& camera_path, const MDagPath& object_path)
            {
                if (m_display_mode == DISPLAY_POINT_CLOUD && m_points.size())
                {
                    // test sorting if that helps
                    MPoint camera_pos(0.0, 0.0, 0.0, 1.0);
                    camera_pos *= camera_path.inclusiveMatrix();
                    camera_pos *= object_path.inclusiveMatrixInverse();

                    tbb::task_scheduler_init init;
                    tbb::parallel_sort(m_points.begin(), m_points.end(), [&](const Point& a, const Point& b) -> bool {
                        return camera_pos.distanceTo(a.position) > camera_pos.distanceTo(b.position);
                    });
                }
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
            const MDagPath& camera_path,
            const MFrameContext&,
            MUserData* oldData)
    {
        DrawData* data = oldData == 0 ? new DrawData() : reinterpret_cast<DrawData*>(oldData);

        data->update(obj_path, p_vdb_visualizer->get_update());
        // sadly we need sorting to avoid glitches in the viewport...
        // TODO: move this to the gpu!
        data->sort(camera_path, obj_path);

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
