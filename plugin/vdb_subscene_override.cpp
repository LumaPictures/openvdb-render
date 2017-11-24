#include "vdb_subscene_override.h"

#include "vdb_maya_utils.hpp"
#ifdef USE_CUDA
#include "point_sorter.h"
#endif

#include <maya/MGlobal.h>
#include <maya/MFnDagNode.h>
#include <maya/MDrawContext.h>
#include <maya/MFnDagNode.h>

#include <tbb/task_scheduler_init.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_sort.h>

#include <GL/glext.h>

#include <new>
#include <random>
#include <algorithm>

namespace {
    // We have to options to code shaders, either cgfx, which is deprecated since 2012
    // or ogsfx, which is a glslfx like thing and severely underdocumented.
    // I decided to go with ogsfx, that can be reused easier later on in other
    // packages like katana. -- Pal
    // Best example for ogsfx https://knowledge.autodesk.com/search-result/caas/CloudHelp/cloudhelp/2016/ENU/Maya-SDK/files/GUID-94505429-12F9-4F04-A4D9-B80880AD0BA1-htm.html

    // Fun part comes, when maya is not giving you error messages when the shader is invalid.
    // Awesome, right?

    // For random numbers : http://www.reedbeta.com/blog/quick-and-easy-gpu-random-numbers-in-d3d11/
    const char* point_cloud_technique = R"ogsfx(
uniform mat4 wv_mat : WorldView;
uniform mat4 p_mat : Projection;
uniform float point_size;
uniform float voxel_size;
uniform float half_viewport_size;
uniform vec3 jitter_size;
uniform int vertex_count;

attribute vs_input
{
    vec3 in_position : POSITION;
    vec4 in_color : TEXCOORD0;
};

attribute vs_to_ps
{
    vec4 point_color;
};

attribute ps_output
{
    vec4 out_color : COLOR0;
}

GLSLShader VS
{
    float rand_xorshift(uint seed)
    {
        seed ^= (seed << 13);
        seed ^= (seed << 17);
        seed ^= (seed << 5);
        return float(seed) * (1.0 / 4294967296.0);
    }

    void main()
    {
        vec4 pos = vec4(in_position, 1.0);
        pos.x += jitter_size.x * 2.0 * rand_xorshift(uint(gl_VertexID)) - jitter_size.x;
        pos.y += jitter_size.y * 2.0 * rand_xorshift(uint(gl_VertexID + vertex_count)) - jitter_size.y;
        pos.z += jitter_size.z * 2.0 * rand_xorshift(uint(gl_VertexID + vertex_count * 2)) - jitter_size.z;
        pos = wv_mat * pos;
        vec4 proj_pos = p_mat * vec4(pos.x + point_size * voxel_size, pos.y, pos.z, pos.w);
        gl_Position = p_mat * pos;
        gl_PointSize = abs(proj_pos.x / proj_pos.w - gl_Position.x / gl_Position.w) * half_viewport_size;
        vsOut.point_color = vec4(in_color.xyz, in_color.w * voxel_size);
    }
}

GLSLShader PS
{
    void main()
    {
        out_color = psIn.point_color;
    }
}

technique Main
{
    pass p0
    {
        VertexShader(in vs_input, out vs_to_ps vsOut) = VS;
        PixelShader(in vs_to_ps psIn, out ps_output) = PS;
    }
}
    )ogsfx";

    const MHWRender::MShaderManager* get_shader_manager()
    {
        auto renderer = MHWRender::MRenderer::theRenderer();
        if (renderer == nullptr) {
            return nullptr;
        }

        auto shader_manager = renderer->getShaderManager();
        if (shader_manager == nullptr) {
            return nullptr;
        }

        return shader_manager;
    }

    bool cuda_enabled = false;

    // This is a hacky workaround for having a callback specific dataset
    // We expect maya not to run other draw calls between the pre and post renders
    bool point_size_enabled = false;

    void pre_point_cloud_render(MHWRender::MDrawContext& context,
                                const MHWRender::MRenderItemList& /*renderItemList*/,
                                MHWRender::MShaderInstance* shaderInstance)
    {
        int origin_x = 0;
        int origin_y = 0;
        int width = 0;
        int height = 0;
        context.getViewportDimensions(origin_x, origin_y, width, height);
        shaderInstance->setParameter("half_viewport_size", static_cast<float>(width) * 0.5f);
        point_size_enabled = glIsEnabled(GL_VERTEX_PROGRAM_POINT_SIZE) != 0;
        if (!point_size_enabled) {
            glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
        }
    }

    void post_point_cloud_render(MHWRender::MDrawContext& /*context*/,
                                 const MHWRender::MRenderItemList& /*renderItemList*/,
                                 MHWRender::MShaderInstance* /*shaderInstance*/)
    {
        if (!point_size_enabled) {
            glDisable(GL_VERTEX_PROGRAM_POINT_SIZE);
        }
    }
}

namespace MHWRender {
    void VDBSubSceneOverride::shader_instance_deleter::operator()(MShaderInstance* p)
    {
        auto shmgr = get_shader_manager();
        if (shmgr != nullptr) {
            shmgr->releaseShader(p);
        }
    }

#ifdef USE_CUDA
    static_assert(sizeof(PointCloudVertex) == sizeof(PointData), "CPU and GPU data structures differ in size for point clouds!");
#endif

    VDBSubSceneOverrideData::VDBSubSceneOverrideData() :
        last_camera_direction(0.0, 0.0, 0.0),
        voxel_size(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(),
                   std::numeric_limits<float>::infinity()),
        point_size(std::numeric_limits<float>::infinity()), point_jitter(std::numeric_limits<float>::infinity()),
        vertex_count(0), point_skip(-1), update_trigger(-1),
        display_mode(DISPLAY_AXIS_ALIGNED_BBOX), shader_mode(SHADER_MODE_SIMPLE),
        sliced_display_changes(VDBSlicedDisplayChangeSet::NO_CHANGES),
        data_has_changed(false), shader_has_changed(false), camera_has_changed(false), world_has_changed(false),
        visible(true), old_bounding_box_enabled(true), old_point_cloud_enabled(true)
    {
        for (unsigned int x = 0; x < 4; ++x) {
            for (unsigned int y = 0; y < 4; ++y) {
                camera_matrix(x, y) = std::numeric_limits<float>::infinity();
            }
        }
    }

    VDBSubSceneOverrideData::~VDBSubSceneOverrideData()
    {
        clear();
    }

    void VDBSubSceneOverrideData::clear()
    {
        scattering_grid = nullptr;
        attenuation_grid = nullptr;
        emission_grid = nullptr;
        vdb_file.reset();
    }

    bool VDBSubSceneOverrideData::update(const VDBVisualizerData* data, const MObject& obj, const MFrameContext& frame_context)
    {
        MFnDagNode dgNode(obj);

        auto path_is_visible = [](const MDagPath& dg_in) -> bool {
            MDagPath dg_copy = dg_in;
            MFnDagNode dg_node;
            for (MStatus status = MS::kSuccess; status; status = dg_copy.pop()) {
                dg_node.setObject(dg_copy.node());
                if (dg_node.isIntermediateObject()) {
                    return false;
                }
                if (!dg_node.findPlug("visibility").asBool()) {
                    return false;
                }
            }
            return true;
        };

        static std::vector<MMatrix> inc_world_matrices;
        static MDagPathArray paths;
        inc_world_matrices.clear();
        paths.clear();
        dgNode.getAllPaths(paths);
        const auto pathCount = paths.length();
        inc_world_matrices.reserve(pathCount);
        for (auto i = decltype(pathCount){0}; i < pathCount; ++i) {
            const auto path = paths[i];
            if (path_is_visible(path)) {
                inc_world_matrices.push_back(path.inclusiveMatrix());
            }
        }

        if (world_matrices != inc_world_matrices) {
            world_has_changed = true;
            world_matrices = inc_world_matrices;
        }

        const auto inc_camera_matrix = frame_context.getMatrix(MFrameContext::kViewMtx);
        if (camera_matrix != inc_camera_matrix) {
            camera_has_changed = true;
            camera_matrix = inc_camera_matrix;
        }
        const bool matrix_changed = world_has_changed || camera_has_changed;
        // TODO: we can limit some of the comparisons to the display mode
        // ie, we don't need to compare certain things if we are using the bounding
        // box mode

        const bool visibility_changed = setup_parameter(visible, !inc_world_matrices.empty());

        if (data == nullptr || update_trigger == data->update_trigger) {
            return matrix_changed || visibility_changed;
        }

        update_trigger = data->update_trigger;

        const std::string& filename = data->vdb_path;
        bool file_has_changed = false;
        auto open_file = [&]() {
            file_has_changed = true;
            clear();

            try {
                vdb_file.reset(new openvdb::io::File(filename));
            }
            catch (...) {
                vdb_file.reset();
            }
        };

        const auto old_filename = this->vdb_file ? this->vdb_file->filename() : "";
        const auto filename_changed = old_filename != filename;
        const auto old_uuid = this->vdb_file ? this->vdb_file->getUniqueTag() : "";
        const auto uuid_changed = data->vdb_file != nullptr && !data->vdb_file->isIdentical(old_uuid);
        if (filename_changed || uuid_changed) {
            open_file();
        } else if (filename.empty() && this->vdb_file != nullptr) {
            file_has_changed = true;
            clear();
        }
        data_has_changed |= file_has_changed;

        const bool display_mode_changed = setup_parameter(display_mode, data->display_mode);
        const bool bbox_changed = setup_parameter(bbox, data->bbox);
        data_has_changed |= display_mode_changed || bbox_changed;
        data_has_changed |= setup_parameter(shader_mode, data->shader_mode);
        data_has_changed |= setup_parameter(scattering_color, data->scattering_color);
        data_has_changed |= setup_parameter(attenuation_color, data->attenuation_color);
        data_has_changed |= setup_parameter(emission_color, data->emission_color);
        data_has_changed |= setup_parameter(attenuation_channel, data->attenuation_channel);
        data_has_changed |= setup_parameter(scattering_channel, data->scattering_channel);
        data_has_changed |= setup_parameter(emission_channel, data->emission_channel);
        data_has_changed |= setup_parameter(scattering_gradient, data->scattering_gradient);
        data_has_changed |= setup_parameter(attenuation_gradient, data->attenuation_gradient);
        data_has_changed |= setup_parameter(emission_gradient, data->emission_gradient);
        data_has_changed |= setup_parameter(point_skip, data->point_skip);

        shader_has_changed |= setup_parameter(point_size, data->point_size);
        shader_has_changed |= setup_parameter(point_jitter, data->point_jitter);

        if (display_mode == DISPLAY_SLICED) {
            if (display_mode_changed) {
                sliced_display_changes = VDBSlicedDisplayChangeSet::ALL;
            }

            bool shader_param_changed = false;
            shader_param_changed |= setup_parameter(sliced_display_data.density, data->sliced_display_data.density);
            shader_param_changed |= setup_parameter(sliced_display_data.density_ramp.input_min, data->sliced_display_data.density_ramp.input_min);
            shader_param_changed |= setup_parameter(sliced_display_data.density_ramp.input_max, data->sliced_display_data.density_ramp.input_max);
            shader_param_changed |= setup_parameter(sliced_display_data.density_source, data->sliced_display_data.density_source);
            shader_param_changed |= setup_parameter(sliced_display_data.scatter, data->sliced_display_data.scatter);
            shader_param_changed |= setup_parameter(sliced_display_data.scatter_color, data->sliced_display_data.scatter_color);
            shader_param_changed |= setup_parameter(sliced_display_data.scatter_color_ramp.input_min, data->sliced_display_data.scatter_color_ramp.input_min);
            shader_param_changed |= setup_parameter(sliced_display_data.scatter_color_ramp.input_max, data->sliced_display_data.scatter_color_ramp.input_max);
            shader_param_changed |= setup_parameter(sliced_display_data.scatter_color_source, data->sliced_display_data.scatter_color_source);
            shader_param_changed |= setup_parameter(sliced_display_data.scatter_anisotropy, data->sliced_display_data.scatter_anisotropy);
            shader_param_changed |= setup_parameter(sliced_display_data.transparent, data->sliced_display_data.transparent);
            shader_param_changed |= setup_parameter(sliced_display_data.emission_mode, data->sliced_display_data.emission_mode);
            shader_param_changed |= setup_parameter(sliced_display_data.emission, data->sliced_display_data.emission);
            shader_param_changed |= setup_parameter(sliced_display_data.emission_color, data->sliced_display_data.emission_color);
            shader_param_changed |= setup_parameter(sliced_display_data.emission_ramp.input_min, data->sliced_display_data.emission_ramp.input_min);
            shader_param_changed |= setup_parameter(sliced_display_data.emission_ramp.input_max, data->sliced_display_data.emission_ramp.input_max);
            shader_param_changed |= setup_parameter(sliced_display_data.emission_source, data->sliced_display_data.emission_source);
            shader_param_changed |= setup_parameter(sliced_display_data.temperature, data->sliced_display_data.temperature);
            shader_param_changed |= setup_parameter(sliced_display_data.blackbody_kelvin, data->sliced_display_data.blackbody_kelvin);
            shader_param_changed |= setup_parameter(sliced_display_data.blackbody_intensity, data->sliced_display_data.blackbody_intensity);
            shader_param_changed |= setup_parameter(sliced_display_data.shadow_sample_count, data->sliced_display_data.shadow_sample_count);
            shader_param_changed |= setup_parameter(sliced_display_data.shadow_gain, data->sliced_display_data.shadow_gain);
            if (shader_param_changed)
                sliced_display_changes |= VDBSlicedDisplayChangeSet::SHADER_PARAM;

            if (setup_parameter(sliced_display_data.slice_count, data->sliced_display_data.slice_count)) {
                sliced_display_changes |= VDBSlicedDisplayChangeSet::SLICE_COUNT;
                sliced_display_changes |= VDBSlicedDisplayChangeSet::ALL_CHANNELS;
            }

            if (bbox_changed)
                sliced_display_changes |= VDBSlicedDisplayChangeSet::BOUNDING_BOX;

            if (setup_parameter(sliced_display_data.density_ramp.samples, data->sliced_display_data.density_ramp.samples))
                sliced_display_changes |= VDBSlicedDisplayChangeSet::DENSITY_RAMP_SAMPLES;
            if (setup_parameter(sliced_display_data.scatter_color_ramp.samples, data->sliced_display_data.scatter_color_ramp.samples))
                sliced_display_changes |= VDBSlicedDisplayChangeSet::SCATTER_COLOR_RAMP_SAMPLES;
            if (setup_parameter(sliced_display_data.emission_ramp.samples, data->sliced_display_data.emission_ramp.samples))
                sliced_display_changes |= VDBSlicedDisplayChangeSet::EMISSION_RAMP_SAMPLES;

            if (setup_parameter(sliced_display_data.density_channel, data->sliced_display_data.density_channel))
                sliced_display_changes |= VDBSlicedDisplayChangeSet::DENSITY_CHANNEL;
            if (setup_parameter(sliced_display_data.scatter_color_channel, data->sliced_display_data.scatter_color_channel))
                sliced_display_changes |= VDBSlicedDisplayChangeSet::SCATTER_COLOR_CHANNEL;
            if (setup_parameter(sliced_display_data.transparent_channel, data->sliced_display_data.transparent_channel))
                sliced_display_changes |= VDBSlicedDisplayChangeSet::TRANSPARENT_CHANNEL;
            if (setup_parameter(sliced_display_data.emission_channel, data->sliced_display_data.emission_channel))
                sliced_display_changes |= VDBSlicedDisplayChangeSet::EMISSION_CHANNEL;
            if (setup_parameter(sliced_display_data.temperature_channel, data->sliced_display_data.temperature_channel))
                sliced_display_changes |= VDBSlicedDisplayChangeSet::TEMPERATURE_CHANNEL;

            if (file_has_changed) {
                sliced_display_changes |= VDBSlicedDisplayChangeSet::ALL_CHANNELS;
            }

            data_has_changed |= (sliced_display_changes != VDBSlicedDisplayChangeSet::NO_CHANGES);
        }

        return data_has_changed || shader_has_changed || matrix_changed || visibility_changed;
    }

    MString VDBSubSceneOverride::registrantId("VDBVisualizerSubSceneOverride");

    MPxSubSceneOverride* VDBSubSceneOverride::creator(const MObject& obj)
    {
        MFnDagNode dgNode(obj);
        MDagPath dg;
        dgNode.getPath(dg);
        return new VDBSubSceneOverride(obj);
    }

    VDBSubSceneOverride::VDBSubSceneOverride(const MObject& obj) : MPxSubSceneOverride(obj),
                                                                   p_data(new VDBSubSceneOverrideData),
                                                                   m_sliced_display(*this)
    {
        m_object = obj;
        MFnDependencyNode dnode(obj);
        p_vdb_visualizer = dynamic_cast<VDBVisualizerShape*>(dnode.userNode());
        auto shader_manager = get_shader_manager();
        if (shader_manager != nullptr) {
            p_point_cloud_shader.reset(shader_manager->getEffectsBufferShader(
                point_cloud_technique, static_cast<unsigned int>(strlen(point_cloud_technique)), "Main", 0, 0, false,
                pre_point_cloud_render, post_point_cloud_render));

            if (p_point_cloud_shader != nullptr) {
                p_point_cloud_shader->setIsTransparent(true);
            } else {
                MGlobal::displayError(MString("[vdb_subscene_override] Error compiling point cloud shader!"));
            }

            p_green_wire_shader.reset(shader_manager->getStockShader(
                MHWRender::MShaderManager::k3dSolidShader, nullptr, nullptr));
            if (p_green_wire_shader) {
                // Set the color on the shader instance using the parameter interface
                static const float color[] = {0.0f, 1.0f, 0.0f, 1.0f};
                p_green_wire_shader->setParameter("solidColor", color);
            }

            p_red_wire_shader.reset(shader_manager->getStockShader(
                MHWRender::MShaderManager::k3dDashLineShader, nullptr, nullptr));
            if (p_red_wire_shader) {
                // Set the color on the shader instance using the parameter interface
                static const float color[] = {1.0f, 0.0f, 0.0f, 1.0f};
                p_red_wire_shader->setParameter("solidColor", color);
            }
        }
    }

    /*VDBSubSceneOverride::~VDBSubSceneOverride()
    {
    }*/

    MHWRender::DrawAPI VDBSubSceneOverride::supportedDrawAPIs() const
    {
#if MAYA_API_VERSION >= 201600
        return kOpenGLCoreProfile | kOpenGL;
#else
        return kOpenGL;
#endif
    }

    void VDBSubSceneOverride::update(MSubSceneContainer& container, const MFrameContext& frameContext)
    {
        VDBSubSceneOverrideData* data = p_data.get();

        MHWRender::MRenderer* renderer = MHWRender::MRenderer::theRenderer();
        if (renderer == nullptr) {
            return;
        }

        const MHWRender::MShaderManager* shader_manager = renderer->getShaderManager();
        if (shader_manager == nullptr) {
            return;
        }

        MRenderItem* bounding_box = container.find("bounding_box");
        if (bounding_box == nullptr) {
            bounding_box = MHWRender::MRenderItem::Create("bounding_box",
                                                          MRenderItem::NonMaterialSceneItem,
                                                          MGeometry::kLines);
            bounding_box->enable(false);
            bounding_box->setDrawMode(MGeometry::kAll);
            bounding_box->depthPriority(MRenderItem::sDormantWireDepthPriority);

            if (p_green_wire_shader) {
                bounding_box->setShader(p_green_wire_shader.get());
            }

            container.add(bounding_box);
        }

        MHWRender::MRenderItem* selection_bounding_box = container.find("selection_bounding_box");
        if (selection_bounding_box == nullptr) {
            selection_bounding_box = MHWRender::MRenderItem::Create(
                "selection_bounding_box",
                MHWRender::MRenderItem::NonMaterialSceneItem,
                MHWRender::MGeometry::kTriangles);
            selection_bounding_box->enable(true);
            selection_bounding_box->setDrawMode(MHWRender::MGeometry::kSelectionOnly);
            selection_bounding_box->depthPriority(MHWRender::MRenderItem::sSelectionDepthPriority);

            MHWRender::MShaderInstance* shader = shader_manager->getStockShader(
                MHWRender::MShaderManager::k3dSolidShader, nullptr, nullptr);
            if (shader != nullptr) {
                selection_bounding_box->setShader(shader);
            }

            container.add(selection_bounding_box);
        }

        MHWRender::MRenderItem* point_cloud = container.find("point_cloud");
        if (point_cloud == nullptr) {
            point_cloud = MHWRender::MRenderItem::Create("point_cloud",
                                                         MHWRender::MGeometry::kPoints,
                                                         MHWRender::MGeometry::kAll,
                                                         false);
            point_cloud->enable(false);
            point_cloud->setDrawMode(MGeometry::kAll);
            point_cloud->depthPriority(MRenderItem::sActivePointDepthPriority);
            point_cloud->setSupportsAdvancedTransparency(true);

            if (p_point_cloud_shader == nullptr) {
                MHWRender::MShaderInstance* shader = shader_manager->getStockShader(
                    MHWRender::MShaderManager::k3dCPVFatPointShader, nullptr, nullptr);
                if (shader != nullptr) {
                    point_cloud->setShader(shader);
                }
            } else {
                point_cloud->setShader(p_point_cloud_shader.get());
            }

            container.add(point_cloud);
        }

        tbb::task_scheduler_init task_init;

        if (!data->visible) {
            data->old_point_cloud_enabled = point_cloud->isEnabled();
            data->old_bounding_box_enabled = bounding_box->isEnabled();
            point_cloud->enable(false);
            bounding_box->enable(false);
            selection_bounding_box->enable(false);
            m_sliced_display.enable(false);
            return;
        }

        point_cloud->enable(data->old_point_cloud_enabled);
        bounding_box->enable(data->old_bounding_box_enabled);

        auto setup_matrices = [&] () {
            // also because of sorting I'm only displaying the first one.
            point_cloud->setMatrix(&data->world_matrices[0]);

            static MMatrixArray matrix_arr;
            const auto matrix_count = data->world_matrices.size();
            matrix_arr.setLength(static_cast<unsigned int>(matrix_count));
            for (auto i = decltype(matrix_count){0}; i < matrix_count; ++i) {
                matrix_arr[i] = data->world_matrices[i];
            }

            setInstanceTransformArray(*bounding_box, matrix_arr);
            if (matrix_arr.length() == 1)
                selection_bounding_box->setMatrix(&matrix_arr[0]);
            else
                setInstanceTransformArray(*selection_bounding_box, matrix_arr);
            m_sliced_display.setWorldMatrices(matrix_arr);
        };

        if (data->data_has_changed) {
            auto setup_bounding_box = [this, &data, selection_bounding_box]() -> bool {
                auto* bbox_vertices = reinterpret_cast<MFloatVector*>(this->p_bbox_position->acquire(8, true));
                MFloatVector min = data->bbox.min();
                MFloatVector max = data->bbox.max();
                bool ret = true;
                if (min.length() < 0.0001f && max.length() < 0.0001f) { // if the bbox is empty, we consider it invalid
                    min.x = min.y = min.z = -1.0f;
                    max.x = max.y = max.z = 1.0f;
                    ret = false;
                }
                bbox_vertices[0] = MFloatVector(min.x, min.y, min.z);
                bbox_vertices[1] = MFloatVector(min.x, max.y, min.z);
                bbox_vertices[2] = MFloatVector(min.x, max.y, max.z);
                bbox_vertices[3] = MFloatVector(min.x, min.y, max.z);
                bbox_vertices[4] = MFloatVector(max.x, min.y, min.z);
                bbox_vertices[5] = MFloatVector(max.x, max.y, min.z);
                bbox_vertices[6] = MFloatVector(max.x, max.y, max.z);
                bbox_vertices[7] = MFloatVector(max.x, min.y, max.z);
                this->p_bbox_position->commit(bbox_vertices);
                set_bbox_indices(1, this->p_bbox_indices.get());

                // Selection bbox.
                p_selection_bbox_indices.reset(new MHWRender::MIndexBuffer(MHWRender::MGeometry::kUnsignedInt32));
                set_bbox_indices_triangles(1, p_selection_bbox_indices.get());
                MHWRender::MVertexBufferArray vertex_buffers;
                vertex_buffers.addBuffer("", p_bbox_position.get());
                setGeometryForRenderItem(*selection_bounding_box, vertex_buffers, *p_selection_bbox_indices, &data->bbox);

                return ret;
            };

            data->data_has_changed = false;
            std::vector<PointCloudVertex>().swap(data->point_cloud_data);
            const bool file_exists = data->vdb_file != nullptr;

            const static MVertexBufferDescriptor position_buffer_desc("", MGeometry::kPosition, MGeometry::kFloat, 3);
            const static MVertexBufferDescriptor color_buffer_desc("", MGeometry::kTexture, MGeometry::kFloat, 4);

            if (!file_exists || data->display_mode <= DISPLAY_GRID_BBOX) {
                point_cloud->enable(false);
                bounding_box->enable(true);
                m_sliced_display.enable(false);
                data->old_point_cloud_enabled = false;
                data->old_bounding_box_enabled = true;

                MVertexBufferArray vertex_buffers;
                p_bbox_position.reset(new MVertexBuffer(position_buffer_desc));
                p_bbox_indices.reset(new MIndexBuffer(MGeometry::kUnsignedInt32));

                if ((data->display_mode == DISPLAY_AXIS_ALIGNED_BBOX) || !file_exists) {
                    if (!setup_bounding_box() || !file_exists) {
                        bounding_box->setShader(p_red_wire_shader.get());
                    } else {
                        bounding_box->setShader(p_green_wire_shader.get());
                    }
                } else if (data->display_mode == DISPLAY_GRID_BBOX) {
                    try {
                        if (!data->vdb_file->isOpen()) {
                            data->vdb_file->open(false);
                        }
                        openvdb::GridPtrVecPtr grids = data->vdb_file->readAllGridMetadata();
                        if (grids->empty()) {
                            throw std::exception();
                        }
                        std::vector<MFloatVector> vertices;
                        vertices.reserve(grids->size() * 8);

                        for (openvdb::GridPtrVec::const_iterator it = grids->begin(); it != grids->end(); ++it) {
                            if (openvdb::GridBase::ConstPtr grid = *it) {
                                std::array<MFloatVector, 8> _vertices;
                                if (read_grid_transformed_bbox_wire(grid, _vertices)) {
                                    for (int v = 0; v < 8; ++v) {
                                        vertices.push_back(_vertices[v]);
                                    }
                                }
                            }
                        }

                        const auto vertex_count = static_cast<unsigned int>(vertices.size());

                        if (vertex_count > 0) {
                            p_bbox_position.reset(new MVertexBuffer(position_buffer_desc));
                            p_bbox_indices.reset(new MIndexBuffer(MGeometry::kUnsignedInt32));
                            auto* bbox_vertices = reinterpret_cast<MFloatVector*>(p_bbox_position->acquire(
                                vertex_count, true));
                            for (unsigned int i = 0; i < vertex_count; ++i) {
                                bbox_vertices[i] = vertices[i];
                            }
                            p_bbox_position->commit(bbox_vertices);
                            set_bbox_indices(vertex_count / 8, p_bbox_indices.get());
                        } else {
                            throw std::exception();
                        }

                        bounding_box->setShader(p_green_wire_shader.get());
                    }
                    catch (...) {
                        setup_bounding_box();
                        bounding_box->setShader(p_red_wire_shader.get());
                    }
                }

                vertex_buffers.addBuffer("", p_bbox_position.get());
                setGeometryForRenderItem(*bounding_box, vertex_buffers, *p_bbox_indices, &data->bbox);
            } else {
                data->old_bounding_box_enabled = false;
                bounding_box->enable(false);
                data->old_point_cloud_enabled = false;
                point_cloud->enable(false);
                m_sliced_display.enable(false);
                if (data->display_mode == DISPLAY_POINT_CLOUD) {
                    try {
                        if (!data->vdb_file->isOpen()) {
                            data->vdb_file->open(false);
                        }
                        if (data->attenuation_grid == nullptr ||
                            data->attenuation_grid->getName() != data->attenuation_channel) {
                            data->attenuation_grid = data->vdb_file->readGrid(data->attenuation_channel);
                        }
                    }
                    catch (...) {
                        data->attenuation_grid = nullptr;
                        data->scattering_grid = nullptr;
                        data->emission_grid = nullptr;
                        data->old_bounding_box_enabled = true;
                        bounding_box->enable(true);

                        MVertexBufferArray vertex_buffers;
                        p_bbox_position.reset(new MVertexBuffer(position_buffer_desc));
                        p_bbox_indices.reset(new MIndexBuffer(MGeometry::kUnsignedInt32));
                        setup_bounding_box();
                        vertex_buffers.addBuffer("", p_bbox_position.get());
                        setGeometryForRenderItem(*bounding_box, vertex_buffers, *p_bbox_indices, &data->bbox);
                        bounding_box->setShader(p_red_wire_shader.get());
                        return;
                    }

                    data->old_point_cloud_enabled = true;
                    point_cloud->enable(true);

                    data->voxel_size = data->attenuation_grid->voxelSize();

                    FloatVoxelIterator* iter = nullptr;

                    if (data->attenuation_grid->valueType() == "float") {
                        iter = new FloatToFloatVoxelIterator(
                            openvdb::gridConstPtrCast<openvdb::FloatGrid>(data->attenuation_grid));
                    } else if (data->attenuation_grid->valueType() == "vec3s") {
                        iter = new Vec3SToFloatVoxelIterator(
                            openvdb::gridConstPtrCast<openvdb::Vec3SGrid>(data->attenuation_grid));
                    } else {
                        iter = new FloatVoxelIterator();
                    }

                    // setting up vertex buffers
                    data->point_cloud_data.reserve(iter->get_active_voxels());
                    const openvdb::math::Transform attenuation_transform = data->attenuation_grid->transform();

                    std::mt19937 mt_generator;
                    std::uniform_real_distribution<float> uniform_0_1_dist(0.0f, 1.0f);
                    const float point_skip_ratio = 1.0f / static_cast<float>(std::max(data->point_skip, 1));
                    for (; iter->is_valid(); iter->get_next()) {
                        // this gives a better distribution than skipping based on index
                        if (uniform_0_1_dist(mt_generator) > point_skip_ratio) {
                            continue;
                        }
                        openvdb::Vec3f vdb_pos = attenuation_transform.indexToWorld(iter->get_coord());
                        data->point_cloud_data.emplace_back(
                            MFloatVector(vdb_pos.x(), vdb_pos.y(),
                                         vdb_pos.z()));
                    }

                    data->point_cloud_data.shrink_to_fit();
                    const auto vertex_count = static_cast<unsigned int>(data->point_cloud_data.size());

                    delete iter;

                    if (vertex_count == 0) {
                        return;
                    }

                    data->vertex_count = static_cast<int>(vertex_count);

                    // setting up color buffers

                    try {
                        if (data->scattering_grid == nullptr ||
                            data->scattering_grid->getName() != data->scattering_channel) {
                            data->scattering_grid = data->vdb_file->readGrid(data->scattering_channel);
                        }
                    } catch (...) {
                        data->scattering_grid = nullptr;
                    }

                    RGBSampler* scattering_sampler = nullptr;

                    if (data->scattering_grid == nullptr) {
                        scattering_sampler = new(m_scattering_sampler.data()) RGBSampler();
                    } else {
                        if (data->scattering_grid->valueType() == "float") {
                            scattering_sampler = new(m_scattering_sampler.data()) FloatToRGBSampler(
                                openvdb::gridConstPtrCast<openvdb::FloatGrid>(data->scattering_grid));
                        } else if (data->scattering_grid->valueType() == "vec3s") {
                            scattering_sampler = new(m_scattering_sampler.data()) Vec3SToRGBSampler(
                                openvdb::gridConstPtrCast<openvdb::Vec3SGrid>(data->scattering_grid));
                        } else {
                            scattering_sampler = new(m_scattering_sampler.data()) RGBSampler();
                        }
                    }

                    try {
                        if (data->emission_grid == nullptr ||
                            data->emission_grid->getName() != data->emission_channel) {
                            data->emission_grid = data->vdb_file->readGrid(data->emission_channel);
                        }
                    }
                    catch (...) {
                        data->emission_grid = nullptr;
                    }

                    RGBSampler* emission_sampler = nullptr;

                    if (data->emission_grid == nullptr) {
                        emission_sampler = new(m_emission_sampler.data()) RGBSampler(MFloatVector(0.0f, 0.0f, 0.0f));
                    } else {
                        if (data->emission_grid->valueType() == "float") {
                            emission_sampler = new(m_emission_sampler.data()) FloatToRGBSampler(
                                openvdb::gridConstPtrCast<openvdb::FloatGrid>(data->emission_grid));
                        } else if (data->emission_grid->valueType() == "vec3s") {
                            emission_sampler = new(m_emission_sampler.data()) Vec3SToRGBSampler(
                                openvdb::gridConstPtrCast<openvdb::Vec3SGrid>(data->emission_grid));
                        } else {
                            emission_sampler = new(m_emission_sampler.data()) RGBSampler(
                                MFloatVector(0.0f, 0.0f, 0.0f));
                        }
                    }

                    RGBSampler* attenuation_sampler = nullptr;

                    if (data->attenuation_grid->valueType() == "float") {
                        attenuation_sampler = new(m_attenuation_sampler.data()) FloatToRGBSampler(
                            openvdb::gridConstPtrCast<openvdb::FloatGrid>(data->attenuation_grid));
                    } else if (data->attenuation_grid->valueType() == "vec3s") {
                        attenuation_sampler = new(m_attenuation_sampler.data()) Vec3SToRGBSampler(
                            openvdb::gridConstPtrCast<openvdb::Vec3SGrid>(data->attenuation_grid));
                    } else {
                        attenuation_sampler = new(m_attenuation_sampler.data()) RGBSampler(
                            MFloatVector(1.0f, 1.0f, 1.0f));
                    }

                    tbb::parallel_for(tbb::blocked_range<unsigned int>(0, vertex_count),
                                      [&](const tbb::blocked_range<unsigned int>& r) {
                                          for (unsigned int i = r.begin(); i < r.end(); ++i) {
                                              auto& vertex = data->point_cloud_data[i];
                                              const openvdb::Vec3d pos(vertex.position.x, vertex.position.y,
                                                                       vertex.position.z);
                                              const MFloatVector scattering_color = data->scattering_gradient.evaluate(
                                                  scattering_sampler->get_rgb(pos));
                                              const MFloatVector emission_color = data->emission_gradient.evaluate(
                                                  emission_sampler->get_rgb(pos));
                                              const MFloatVector attenuation_color = data->attenuation_gradient.evaluate(
                                                  attenuation_sampler->get_rgb(pos));
                                              vertex.color.r = scattering_color.x * data->scattering_color.x +
                                                               emission_color.x * data->emission_color.x;
                                              vertex.color.g = scattering_color.y * data->scattering_color.y +
                                                               emission_color.y * data->emission_color.y;
                                              vertex.color.b = scattering_color.z * data->scattering_color.z +
                                                               emission_color.z * data->emission_color.z;
                                              vertex.color.a = (attenuation_color.x * data->attenuation_color.x +
                                                                attenuation_color.y * data->attenuation_color.y +
                                                                attenuation_color.z * data->attenuation_color.z) / 3.0f;
                                          }
                                      });

                    scattering_sampler->~RGBSampler();
                    emission_sampler->~RGBSampler();
                    attenuation_sampler->~RGBSampler();

                    const auto camera_matrix = frameContext.getMatrix(MFrameContext::kViewInverseMtx);
                    MFloatPoint camera_pos = MPoint(0.0f, 0.0f, 0.0f, 1.0f) * (camera_matrix * data->world_matrices[0].inverse());
                    setup_point_cloud(point_cloud, camera_pos);
                    data->camera_has_changed = false;
                    data->world_has_changed = false;

                    p_point_cloud_shader->setParameter("vertex_count", data->vertex_count);
                    p_point_cloud_shader->setParameter("voxel_size",
                                                       std::max(data->voxel_size.x(),
                                                                std::max(data->voxel_size.y(), data->voxel_size.z())));
                    p_point_cloud_shader->setParameter("jitter_size", MFloatVector(
                        data->voxel_size.x(), data->voxel_size.y(), data->voxel_size.y()) * data->point_jitter);

                } else if (data->display_mode == DISPLAY_SLICED) {
                    if (!data->vdb_file->isOpen()) {
                        data->vdb_file->open(false);
                    }
                    if (hasChange(data->sliced_display_changes, VDBSlicedDisplayChangeSet::BOUNDING_BOX)) {
                        p_bbox_position.reset(new MVertexBuffer(position_buffer_desc));
                        p_bbox_indices.reset(new MIndexBuffer(MGeometry::kUnsignedInt32));
                        setup_bounding_box();
                    }
                    selection_bounding_box->enable(true);
                    m_sliced_display.enable(true);
                    m_sliced_display.update(container, data->vdb_file.get(), data->bbox, data->sliced_display_data, data->sliced_display_changes);
                }
            }

            setup_matrices();
        }

        // Setting up shader parameters
        if (data->shader_has_changed && p_point_cloud_shader) {
            p_point_cloud_shader->setParameter("point_size", data->point_size);
            p_point_cloud_shader->setParameter("jitter_size", MFloatVector(
                data->voxel_size.x(), data->voxel_size.y(), data->voxel_size.y()) * data->point_jitter);
            data->shader_has_changed = false;
        }

        if (data->camera_has_changed || data->world_has_changed) {
            if (data->display_mode == DISPLAY_POINT_CLOUD) {
                const auto camera_matrix = frameContext.getMatrix(MFrameContext::kViewInverseMtx);
                const auto camera_pos = MPoint(0.0f, 0.0f, 0.0f, 1.0f) * (camera_matrix * data->world_matrices[0].inverse());
                MVector camera_dir(
                    camera_pos.x, camera_pos.y, camera_pos.z);
                camera_dir.normalize();
                constexpr double rotation_limit = 0.2;
                if (data->last_camera_direction.length() < 0.0001 || (camera_dir.angle(data->last_camera_direction) > rotation_limit)) {
                    data->last_camera_direction = camera_dir;
                    setup_point_cloud(point_cloud, camera_pos);
                }
            }

            if (data->world_has_changed) {
                setup_matrices();
            }

            data->camera_has_changed = false;
            data->world_has_changed = false;
        }
    }

    bool VDBSubSceneOverride::requiresUpdate(const MSubSceneContainer& /*container*/,
                                             const MFrameContext& frameContext) const
    {
        MFnDagNode dgNode(m_object);
        MDagPath dg;
        dgNode.getPath(dg);
        return p_data->update(p_vdb_visualizer->get_update(), m_object, frameContext);
    }

    void VDBSubSceneOverride::setup_point_cloud(MRenderItem* point_cloud, const MFloatPoint& camera_pos)
    {
        const static MVertexBufferDescriptor position_buffer_desc("", MGeometry::kPosition, MGeometry::kFloat, 3);
        const static MVertexBufferDescriptor color_buffer_desc("", MGeometry::kTexture, MGeometry::kFloat, 4);

        VDBSubSceneOverrideData* data = p_data.get();

        if (data->point_cloud_data.empty()) {
            return;
        }

        const auto sorting_mode = MPlug(p_vdb_visualizer->thisMObject(), VDBVisualizerShape::s_point_sort).asShort();

        const auto sorting_function = [camera_pos](const PointCloudVertex& a, const PointCloudVertex& b) -> bool {
            const float distance_a[3] = {
                a.position.x - camera_pos.x, a.position.y - camera_pos.y, a.position.z - camera_pos.z
            };
            const float distance_b[3] = {
                b.position.x - camera_pos.x, b.position.y - camera_pos.y, b.position.z - camera_pos.z
            };
            return (distance_a[0] * distance_a[0] + distance_a[1] * distance_a[1] + distance_a[2] * distance_a[2]) >
                   (distance_b[0] * distance_b[0] + distance_b[1] * distance_b[1] + distance_b[2] * distance_b[2]);
        };

        if (sorting_mode == POINT_SORT_CPU) {
            tbb::parallel_sort(data->point_cloud_data.begin(), data->point_cloud_data.end(), sorting_function);
        } else if (sorting_mode == POINT_SORT_GPU_CPU) {
            if (cuda_enabled) {
#ifdef USE_CUDA
                sort_points(reinterpret_cast<PointData*>(data->point_cloud_data.data()), data->point_cloud_data.size(), &camera_pos.x);
#endif
            } else {
                tbb::parallel_sort(data->point_cloud_data.begin(), data->point_cloud_data.end(), sorting_function);
            }
        } else if (sorting_mode == POINT_SORT_GPU) {
            if (cuda_enabled) {
#ifdef USE_CUDA
                sort_points(reinterpret_cast<PointData*>(data->point_cloud_data.data()), data->point_cloud_data.size(), &camera_pos.x);
#endif
            }
        }

        const auto vertex_count = static_cast<unsigned int>(data->point_cloud_data.size());

        p_position_buffer.reset(new MVertexBuffer(position_buffer_desc));
        p_color_buffer.reset(new MVertexBuffer(color_buffer_desc));

        auto* vertices = reinterpret_cast<MFloatVector*>(p_position_buffer->acquire(
            vertex_count, true));
        auto* colors = reinterpret_cast<MColor*>(p_color_buffer->acquire(vertex_count, true));

        tbb::parallel_for(tbb::blocked_range<unsigned int>(0, vertex_count),
                          [&](const tbb::blocked_range<unsigned int>& r) {
                              for (auto i = r.begin(); i != r.end(); ++i) {
                                  vertices[i] = data->point_cloud_data[i].position;
                                  colors[i] = data->point_cloud_data[i].color;
                              }
                          });

        p_position_buffer->commit(vertices);
        p_color_buffer->commit(colors);

        MVertexBufferArray vertex_buffers;
        vertex_buffers.addBuffer("", p_position_buffer.get());
        vertex_buffers.addBuffer("", p_color_buffer.get());
        MIndexBuffer index_buffer(MGeometry::kUnsignedInt32);
        setGeometryForRenderItem(*point_cloud, vertex_buffers, index_buffer, &data->bbox);
    }

    void VDBSubSceneOverride::init_gpu() {
#ifdef USE_CUDA
        cuda_enabled = cuda_available();
#else
        cuda_enabled = false;
#endif
    }
}
