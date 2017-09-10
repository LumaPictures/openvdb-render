#pragma once

#include <maya/MPxSubSceneOverride.h>
#include <memory>

#include "vdb_visualizer.h"
#include "vdb_subscene_utils.hpp"

namespace MHWRender {
    struct VDBSubSceneOverrideData;

    class VDBSubSceneOverride : public MHWRender::MPxSubSceneOverride {
    public:
        static MPxSubSceneOverride* creator(const MObject& obj);

        VDBSubSceneOverride(const MObject& obj);

        virtual ~VDBSubSceneOverride();

        virtual MHWRender::DrawAPI supportedDrawAPIs() const;

        virtual void update(MSubSceneContainer& container, const MFrameContext& frameContext);

        virtual bool requiresUpdate(const MSubSceneContainer& container, const MFrameContext& frameContext) const;

        static MString registrantId;

        static void init_gpu();
    private:
        void setup_point_cloud(MRenderItem* point_cloud, const MFloatPoint& camera_pos);

        MObject m_object;
        VDBVisualizerShape* p_vdb_visualizer;
        std::unique_ptr<VDBSubSceneOverrideData> p_data;

        std::unique_ptr<MVertexBuffer> p_bbox_position;
        std::unique_ptr<MIndexBuffer> p_bbox_indices;

        std::unique_ptr<MVertexBuffer> p_position_buffer;
        std::unique_ptr<MVertexBuffer> p_color_buffer;

        struct shader_instance_deleter {
            void operator()(MShaderInstance* p);
        };

        std::unique_ptr<MShaderInstance, shader_instance_deleter> p_point_cloud_shader;
        std::unique_ptr<MShaderInstance, shader_instance_deleter> p_green_wire_shader;
        std::unique_ptr<MShaderInstance, shader_instance_deleter> p_red_wire_shader;
        // max is not constexpr
        static constexpr size_t sampler_mem_size = sizeof(FloatToRGBSampler) > sizeof(Vec3SToRGBSampler)
                                                   ? sizeof(FloatToRGBSampler) : sizeof(Vec3SToRGBSampler);

        typedef std::array<char, sampler_mem_size> sampler_mem_area;
        sampler_mem_area m_scattering_sampler;
        sampler_mem_area m_emission_sampler;
        sampler_mem_area m_attenuation_sampler;
    };

    // we are storing an extra float here to have better memory alignment
    struct PointCloudVertex {
        MFloatPoint position;
        MColor color;

        PointCloudVertex() : position(0.0f, 0.0f, 0.0f, 0.0f), color(0.0f, 0.0f, 0.0f, 0.0f) {

        }

        PointCloudVertex(const MFloatPoint& p) : position(p), color(0.0f, 0.0f, 0.0f, 0.0f) {

        }
    };

    struct VDBSubSceneOverrideData {
        MBoundingBox bbox;

        // We need to handle all the instances
        std::vector<MMatrix> world_matrices;
        std::vector<PointCloudVertex> point_cloud_data;

        MMatrix camera_matrix;
        MVector last_camera_direction;

        MFloatVector scattering_color;
        MFloatVector attenuation_color;
        MFloatVector emission_color;

        std::string attenuation_channel;
        std::string scattering_channel;
        std::string emission_channel;

        Gradient scattering_gradient;
        Gradient attenuation_gradient;
        Gradient emission_gradient;

        std::unique_ptr<openvdb::io::File> vdb_file;
        openvdb::GridBase::ConstPtr scattering_grid;
        openvdb::GridBase::ConstPtr attenuation_grid;
        openvdb::GridBase::ConstPtr emission_grid;

        openvdb::Vec3f voxel_size;

        float point_size;
        float point_jitter;

        int vertex_count;
        int point_skip;
        int update_trigger;
        VDBDisplayMode display_mode;
        VDBShaderMode shader_mode;

        bool data_has_changed;
        bool shader_has_changed;
        bool camera_has_changed;
        bool world_has_changed;
        bool visible;
        bool old_bounding_box_enabled;
        bool old_point_cloud_enabled;

        VDBSubSceneOverrideData();
        ~VDBSubSceneOverrideData();
        void clear();
        bool VDBSubSceneOverrideData::update(const VDBVisualizerData* data, const MObject& obj, const MFrameContext& frame_context);
    };

}
