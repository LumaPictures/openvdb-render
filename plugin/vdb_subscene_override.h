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
}
