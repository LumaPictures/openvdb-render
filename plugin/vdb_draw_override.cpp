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
#include "draw_utils.h"

namespace {
    std::shared_ptr<GLPipeline> float_point_pipeline;
    //std::shared_ptr<GLPipeline> bit16_point_pipeline = 0;
    //std::shared_ptr<GLPipeline> bit12_point_pipeline = 0;

#pragma pack(0)
    struct Point{
        MFloatVector position;
        unsigned char color[4];

        void set_color(float r, float g, float b, float a) // apply gamma?
        {
            auto convert_channel = [] (float channel) -> unsigned char {
                return static_cast<unsigned char>(std::max(0, std::min(static_cast<int>(channel * 255.0), 255)));
            };
            color[0] = convert_channel(r);
            color[1] = convert_channel(g);
            color[2] = convert_channel(b);
            color[3] = convert_channel(a);
        }
    };
#pragma pack()

    class RGBSampler {
    private:
        MFloatVector default_color;
    public:
        RGBSampler(const MFloatVector& dc = MFloatVector(1.0f, 1.0f, 1.0f)) : default_color(dc)
        { }

        virtual ~RGBSampler()
        { }

        virtual MFloatVector get_rgb(const openvdb::Vec3d&) const
        {
            return default_color;
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

        MFloatVector get_rgb(const openvdb::Vec3d& wpos) const
        {
            const float value = p_sampler->wsSample(wpos);
            return MFloatVector(value, value, value);
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

        MFloatVector get_rgb(const openvdb::Vec3d& wpos) const
        {
            const openvdb::Vec3s value = p_sampler->wsSample(wpos);
            return MFloatVector(value.x(), value.y(), value.z());
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

        const GLuint INVALID_GL_OBJECT = static_cast<GLuint>(-1);

        class DrawData : public MUserData {
        private:
            MBoundingBox m_bbox;
            float m_wireframe_color[4];

            std::vector<MFloatVector> m_wireframe;

            GLuint m_vertex_buffer;
            GLuint m_vertex_array;
            GLsizei m_point_count;

            float m_point_size;

            VDBDisplayMode m_display_mode;
            bool m_is_empty;
        public:
            DrawData() : MUserData(false), m_vertex_buffer(INVALID_GL_OBJECT), m_vertex_array(INVALID_GL_OBJECT), m_point_count(0), m_is_empty(false)
            {

            }

            ~DrawData()
            {
                clear_gpu_buffers();
            }

            void clear_gpu_buffers()
            {
                if (m_vertex_buffer != INVALID_GL_OBJECT)
                    glDeleteBuffers(1, &m_vertex_buffer);
                m_vertex_buffer = INVALID_GL_OBJECT;
                if (m_vertex_array != INVALID_GL_OBJECT)
                    glDeleteVertexArrays(1, &m_vertex_array);
                m_vertex_array = INVALID_GL_OBJECT;
                m_point_count = 0;
            }

            void clear_wireframe()
            {
                std::vector<MFloatVector>().swap(m_wireframe);
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
                clear_gpu_buffers();

                if (m_is_empty)
                {
                    m_display_mode = DISPLAY_AXIS_ALIGNED_BBOX; // to set when it's empty
                    m_bbox = MBoundingBox(MPoint(-1.0, -1.0, -1.0), MPoint(1.0, 1.0, 1.0));
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
                            push_back_wireframe(grid);
                    }
                }
                else if (vdb_data->display_mode == DISPLAY_POINT_CLOUD)
                {

                    openvdb::GridBase::ConstPtr attenuation_grid = nullptr;
                    try
                    {
                        attenuation_grid = vdb_data->vdb_file->readGrid(vdb_data->attenuation_channel);
                    }
                    catch(...)
                    {
                        attenuation_grid = nullptr;
                    }

                    if (attenuation_grid == nullptr)
                    {
                        quick_reserve(24);
                        add_wire_bounding_box(m_bbox.min(), m_bbox.max());
                        return;
                    }

                    quick_reserve(24);
                    push_back_wireframe(attenuation_grid);

                    openvdb::GridBase::ConstPtr scattering_grid = nullptr;

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
                            scattering_grid = nullptr;
                        }
                    }

                    openvdb::GridBase::ConstPtr emission_grid = 0;

                    if (vdb_data->emission_channel == vdb_data->attenuation_channel)
                        emission_grid = attenuation_grid;
                    else if (vdb_data->emission_channel == vdb_data->scattering_channel)
                        emission_grid = scattering_grid;
                    else
                    {
                        try
                        {
                            emission_grid = vdb_data->vdb_file->readGrid(vdb_data->emission_channel);
                        }
                        catch(...)
                        {
                            emission_grid = nullptr;
                        }
                    }

                    m_point_size = vdb_data->point_size;

                    size_t active_voxel_count = attenuation_grid->activeVoxelCount() / (vdb_data->point_skip + 1);
                    std::vector<Point> points;
                    points.reserve(active_voxel_count);

                    const float point_jitter = vdb_data->point_jitter;
                    const float do_jitter = point_jitter > 0.001f;

                    openvdb::Vec3d voxel_size = attenuation_grid->voxelSize();

                    std::minstd_rand generator; // LCG
                    std::uniform_real_distribution<float> distributionX(-point_jitter * static_cast<float>(voxel_size.x()), point_jitter * static_cast<float>(voxel_size.x()));
                    std::uniform_real_distribution<float> distributionY(-point_jitter * static_cast<float>(voxel_size.y()), point_jitter * static_cast<float>(voxel_size.y()));
                    std::uniform_real_distribution<float> distributionZ(-point_jitter * static_cast<float>(voxel_size.z()), point_jitter * static_cast<float>(voxel_size.z()));

                    const float default_alpha = (vdb_data->attenuation_color.x + vdb_data->attenuation_color.y + vdb_data->attenuation_color.z) / 3.0f;

                    openvdb::math::Transform attenuation_transform = attenuation_grid->transform();

                    RGBSampler* scattering_sampler = nullptr;

                    if (scattering_grid == nullptr)
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

                    RGBSampler* emission_sampler = nullptr;

                    if (emission_grid == nullptr)
                        emission_sampler = new RGBSampler(MFloatVector(0.0f, 0.0f, 0.0f));
                    else
                    {
                        if (emission_grid->valueType() == "float")
                        emission_sampler = new FloatToRGBSampler(openvdb::gridConstPtrCast<openvdb::FloatGrid>(emission_grid));
                        else if (emission_grid->valueType() == "vec3s")
                            emission_sampler = new Vec3SToRGBSampler(openvdb::gridConstPtrCast<openvdb::Vec3SGrid>(emission_grid));
                        else
                            emission_sampler = new RGBSampler(MFloatVector(0.0f, 0.0f, 0.0f));
                    }

                    FloatVoxelIterator* iter = nullptr;

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
                        const MFloatVector attenuation = vdb_data->attenuation_gradient.evaluate(iter->get_value()) * default_alpha;
                        const float value = (attenuation.x + attenuation.y + attenuation.z) / 3.0f;
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
                            const MFloatVector scattering_color = vdb_data->scattering_gradient.evaluate(scattering_sampler->get_rgb(vdb_pos));
                            const MFloatVector emission_color = vdb_data->emission_gradient.evaluate(emission_sampler->get_rgb(vdb_pos));
                            point.set_color(scattering_color.x * vdb_data->scattering_color.x + emission_color.x * vdb_data->emission_color.x,
                                            scattering_color.y * vdb_data->scattering_color.y + emission_color.y * vdb_data->emission_color.y,
                                            scattering_color.z * vdb_data->scattering_color.z + emission_color.z * vdb_data->emission_color.z,
                                            value);
                            points.push_back(point);
                        }
                    }

                    delete scattering_sampler;
                    delete emission_sampler;

                    points.shrink_to_fit();

                    if (points.size() > 0)
                    {
                        glGenBuffers(1, &m_vertex_buffer);
                        glBindBuffer(GL_ARRAY_BUFFER, m_vertex_buffer);
                        glBufferData(GL_ARRAY_BUFFER, points.size() * sizeof(Point), &points[0], GL_STATIC_DRAW);
                        glBindBuffer(GL_ARRAY_BUFFER, 0);

                        glGenVertexArrays(1, &m_vertex_array);
                        glBindVertexArray(m_vertex_array);
                        glBindBuffer(GL_ARRAY_BUFFER, m_vertex_buffer);
                        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Point), 0);
                        glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Point), (char*)(0) + 3 * sizeof(float));
                        glBindBuffer(GL_ARRAY_BUFFER, 0);
                        glEnableVertexAttribArray(0);
                        glEnableVertexAttribArray(1);
                        glBindVertexArray(0);

                        m_point_count = static_cast<GLsizei>(points.size());
                    }
                }
                else
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

                if (m_display_mode == DISPLAY_POINT_CLOUD && m_point_count > 0)
                {
                    glColor3f(1.0f, 1.0f, 1.0f);
                    glPointSize(m_point_size);
                    glEnable(GL_POINT_SMOOTH);

                    glDepthMask(GL_FALSE);
                    glEnable(GL_DEPTH_TEST);
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                    {
                        GLPipeline::ScopedSet pipeline_set(*float_point_pipeline);
                        float world_view_proj_mat[4][4];
                        context.getMatrix(MHWRender::MDrawContext::kWorldViewProjMtx).get(world_view_proj_mat);

                        float_point_pipeline->get_program(GL_VERTEX_SHADER)->set_uniform(GL_MATRIX4_ARB, 0, 1, world_view_proj_mat[0]);

                        glBindVertexArray(m_vertex_array);

                        glDrawArrays(GL_POINTS, 0, m_point_count);

                        glBindVertexArray(0);
                    }

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

            void sort(const MDagPath&, const MDagPath&)
            {
                /*if (m_display_mode == DISPLAY_POINT_CLOUD && m_points.size())
                {
                    // test sorting if that helps
                    MPoint camera_pos(0.0, 0.0, 0.0, 1.0);
                    camera_pos *= camera_path.inclusiveMatrix();
                    camera_pos *= object_path.inclusiveMatrixInverse();

                    tbb::task_scheduler_init init;
                    tbb::parallel_sort(m_points.begin(), m_points.end(), [&](const Point& a, const Point& b) -> bool {
                        return camera_pos.distanceTo(a.position) > camera_pos.distanceTo(b.position);
                    });
                }*/
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

    // https://www.opengl.org/wiki/Built-in_Variable_(GLSL) for gl_PerVertex
    bool VDBDrawOverride::init_shaders()
    {
        try {
            auto float_point_vertex = GLProgram::create_program(GL_VERTEX_SHADER, 1, R"glsl(
#version 450 core
uniform mat4 world_view_proj;
layout(location = 0) in vec3 Position;
layout(location = 1) in vec4 ColorAlpha;

layout(location = 0) out vec4 out_ColorAlpha;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main(void)
{
    gl_Position = world_view_proj * vec4(Position, 1.0);
    out_ColorAlpha = ColorAlpha;
}
        )glsl");
            auto point_color_fragment = GLProgram::create_program(GL_FRAGMENT_SHADER, 1, R"glsl(
#version 450 core
layout(location = 0) in vec4 ColorAlpha;
layout(location = 0, index = 0) out vec4 FragColor;
void main(void)
{
    FragColor = ColorAlpha;
}
        )glsl");
            float_point_pipeline = GLPipeline::create_pipeline(2, &float_point_vertex, &point_color_fragment);
        }
        catch(GLProgram::CreateException& ex)
        {
            std::cerr << "[openvdb_render] Error creating shader" << std::endl;
            std::cerr << ex.what() << std::endl;
            return false;
        }
        catch(GLPipeline::ValidateException& ex)
        {
            std::cerr << "[openvdb_render] Error validating pipeline" << std::endl;
            std::cerr << ex.what() << std::endl;
            return false;
        }
        return true;
    }
}
