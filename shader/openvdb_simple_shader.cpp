#include <ai.h>

#include "gradient.hpp"

AI_SHADER_NODE_EXPORT_METHODS(openvdbSimpleShaderMethods);

namespace {
    struct ShaderData {
        Gradient smoke_gradient;
        Gradient opacity_gradient;
        Gradient fire_gradient;

        AtString smoke_channel;
        AtString opacity_channel;
        AtString fire_channel;

        int interpolation;

        bool sample_smoke;
        bool sample_opacity;
        bool sample_fire;
        bool compensate_scaling;

        void* operator new(size_t size)
        {
            return AiMalloc(static_cast<unsigned int>(size));
        }

        void operator delete(void* d)
        {
            AiFree(d);
        }

        ShaderData() : interpolation(0), sample_smoke(false), sample_opacity(false), sample_fire(
            false), compensate_scaling(true)
        {

        }

        void update(AtNode* node)
        {
            smoke_channel = AtString(AiNodeGetStr(node, "smoke_channel"));
            opacity_channel = AtString(AiNodeGetStr(node, "opacity_channel"));
            fire_channel = AtString(AiNodeGetStr(node, "fire_channel"));

            sample_smoke = smoke_channel.length() > 0;
            sample_opacity = opacity_channel.length() > 0;
            sample_fire = fire_channel.length() > 0;

            interpolation = AiNodeGetInt(node, "interpolation");

            smoke_gradient.update("simpleSmoke", node);
            opacity_gradient.update("simpleOpacity", node);
            fire_gradient.update("simpleFire", node);
        }
    };

    enum {
        p_smoke,
        p_smoke_channel,
        p_smoke_intensity,
        p_anisotropy,
        p_opacity,
        p_opacity_channel,
        p_opacity_intensity,
        p_opacity_shadow,
        p_fire,
        p_fire_channel,
        p_fire_intensity,
        p_position_offset,
        p_interpolation,
        p_compensate_scaling
    };

    const char* interpolations[] = {"closest", "trilinear", "tricubic", nullptr};
}

node_parameters
{
    AiParameterRGB("smoke", 1.0f, 1.0f, 1.0f);
    AiParameterStr("smoke_channel", "");
    AiParameterFlt("smoke_intensity", 1.0f);
    AiParameterFlt("anisotropy", 0.0f);

    AiParameterRGB("opacity", 1.0f, 1.0f, 1.0f);
    AiParameterStr("opacity_channel", "");
    AiParameterFlt("opacity_intensity", 1.0f);
    AiParameterRGB("opacity_shadow", 1.0f, 1.0f, 1.0f);

    AiParameterRGB("fire", 1.0f, 1.0f, 1.0f);
    AiParameterStr("fire_channel", "");
    AiParameterFlt("fire_intensity", 1.0f);

    AiParameterVec("position_offset", 0.0f, 0.0f, 0.0f);
    AiParameterEnum("interpolation", 0, interpolations);
    AiParameterBool("compensate_scaling", true);

    Gradient::parameters("simpleSmoke", params, nentry);
    Gradient::parameters("simpleOpacity", params, nentry);
    Gradient::parameters("simpleFire", params, nentry);

    AiMetaDataSetBool(nentry, nullptr, "maya.hide", true);
    AiMetaDataSetBool(nentry, "smoke_channel", "volume_sample", true);
    AiMetaDataSetBool(nentry, "opacity_channel", "volume_sample", true);
    AiMetaDataSetBool(nentry, "fire_channel", "volume_sample", true);
}

//node_initialize
static void Initialize(AtNode* node)
{
    AiNodeSetLocalData(node, new ShaderData());
}

node_update
{
    auto* data = reinterpret_cast<ShaderData*>(AiNodeGetLocalData(node));
    data->update(node);
}

node_finish
{
    delete reinterpret_cast<ShaderData*>(AiNodeGetLocalData(node));;
}

shader_evaluate
{
    const auto* data = reinterpret_cast<const ShaderData*>(AiNodeGetLocalData(node));

    float scale_factor = 1.0f;
    if (data->compensate_scaling) {
        AtVector scaled_dir;
        scaled_dir = AiM4VectorByMatrixMult(sg->Minv, sg->Rd);
        scale_factor = AiV3Length(scaled_dir);
    }

    if (sg->Rt & AI_RAY_SHADOW) {
        AtRGB opacity = AI_RGB_WHITE;
        if (data->sample_opacity)
            AiVolumeSampleRGB(data->opacity_channel, data->interpolation, &opacity);
        opacity *= (AiShaderEvalParamRGB(p_opacity) * AiShaderEvalParamRGB(p_opacity_shadow)) *
                   (AiShaderEvalParamFlt(p_opacity_intensity) * scale_factor);
        AiColorClipToZero(opacity);
        // TODO: test if this works properly!
        sg->out.CLOSURE() = AiClosureVolumeAbsorption(sg, opacity);
    } else {
        AtRGB opacity = AI_RGB_WHITE;
        if (data->sample_opacity) {
            opacity = data->opacity_gradient.evaluate_arnold(sg, data->opacity_channel, data->interpolation);
        }
        opacity *= AiShaderEvalParamRGB(p_opacity) * (AiShaderEvalParamFlt(p_opacity_intensity) * scale_factor);

        AtRGB smoke = AI_RGB_WHITE;
        if (data->sample_smoke) {
            smoke = data->smoke_gradient.evaluate_arnold(sg, data->smoke_channel, data->interpolation);
        }
        smoke *= opacity * AiShaderEvalParamRGB(p_smoke) * AiShaderEvalParamFlt(p_smoke_intensity);

        AtRGB fire = AI_RGB_WHITE;
        if (data->sample_fire) {
            fire = data->fire_gradient.evaluate_arnold(sg, data->fire_channel, data->interpolation);
        }
        fire *= opacity * AiShaderEvalParamRGB(p_fire) * AiShaderEvalParamFlt(p_fire_intensity);

        AiColorClipToZero(smoke);
        AiColorClipToZero(opacity);
        AiColorClipToZero(fire);
        sg->out.CLOSURE() = AiClosureVolumeHenyeyGreenstein(
            sg, opacity,
            smoke, fire, AiShaderEvalParamFlt(p_anisotropy));
    }
}
