#include <ai.h>

AI_SHADER_NODE_EXPORT_METHODS(openvdbSimpleShaderMethods);

namespace {
    struct ShaderData{
        AtString color_channel;
        AtString opacity_channel;
        AtString emission_channel;

        int interpolation;

        bool sample_color;
        bool sample_opacity;
        bool sample_emission;
        bool compensate_scaling;

        void* operator new(size_t size)
        {
            return AiMalloc(static_cast<unsigned int>(size));
        }

        void operator delete(void* d)
        {
            AiFree(d);
        }

        ShaderData() : interpolation(0), sample_color(false), sample_opacity(false), sample_emission(false), compensate_scaling(true)
        {

        }

        void update(AtNode* node, AtParamValue*)
        {
            color_channel = AtString(AiNodeGetStr(node, "color_channel"));
            opacity_channel = AtString(AiNodeGetStr(node, "opacity_channel"));
            emission_channel = AtString(AiNodeGetStr(node, "emission_channel"));

            sample_color = color_channel.length() > 0;
            sample_opacity = opacity_channel.length() > 0;
            sample_emission = emission_channel.length() > 0;
        }
    };

    enum{
        p_color,
        p_color_channel,
        p_color_intensity,
        p_anisotropy,
        p_opacity,
        p_opacity_channel,
        p_opacity_intensity,
        p_emission,
        p_emission_channel,
        p_emission_intensity,
        p_interpolation,
        p_compensate_scaling
    };

    const char* interpolations[] = { "closest", "trilinear", "tricubic", nullptr};
}

node_parameters
{
    AiParameterRGB("color", 1.0f, 1.0f, 1.0f);
    AiParameterStr("color_channel", "");
    AiParameterFlt("color_intensity", 1.0f);
    AiParameterFlt("anisotropy", 0.0f);

    AiParameterRGB("opacity", 1.0f, 1.0f, 1.0f);
    AiParameterStr("opacity_channel", "");
    AiParameterFlt("opacity_intensity", 1.0f);

    AiParameterRGB("emission", 1.0f, 1.0f, 1.0f);
    AiParameterStr("emission_channel", "");
    AiParameterFlt("emission_intensity", 1.0f);

    AiParameterEnum("interpolation", 0, interpolations);
    AiParameterBool("compensate_scaling", true);

    AiMetaDataSetBool(mds, 0, "maya.hide", true);
}

//node_initialize
static void Initialize(AtNode* node, AtParamValue*)
{
    AiNodeSetLocalData(node, new ShaderData());
}

node_update
{
    ShaderData* data = reinterpret_cast<ShaderData*>(AiNodeGetLocalData(node));
    data->update(node, params);
}

node_finish
{
    delete reinterpret_cast<ShaderData*>(AiNodeGetLocalData(node));;
}

shader_evaluate
{
    const ShaderData* data = reinterpret_cast<const ShaderData*>(AiNodeGetLocalData(node));

    float scale_factor = 1.0f;
    if (data->compensate_scaling)
    {
        AtVector scaled_dir;
        AiM4VectorByMatrixMult(&scaled_dir, sg->Minv, &sg->Rd);
        scale_factor = AiV3Length(scaled_dir);
    }

    if (sg->Rt & AI_RAY_SHADOW)
    {
        AtRGB opacity = AI_RGB_WHITE;
        if (data->sample_opacity)
            AiVolumeSampleRGB(data->opacity_channel, data->interpolation, &opacity);
        opacity *= AiShaderEvalParamRGB(p_opacity) * (AiShaderEvalParamFlt(p_opacity_intensity) * scale_factor);
        AiShaderGlobalsSetVolumeAttenuation(sg, opacity);
    }
    else
    {
        AtRGB opacity = AI_RGB_WHITE;
        if (data->sample_opacity)
            AiVolumeSampleRGB(data->opacity_channel, data->interpolation, &opacity);
        opacity *= AiShaderEvalParamRGB(p_opacity) * (AiShaderEvalParamFlt(p_opacity_intensity) * scale_factor);

        AtRGB color = AI_RGB_WHITE;
        if (data->sample_color)
            AiVolumeSampleRGB(data->color_channel, data->interpolation, &color);
        color *= opacity * AiShaderEvalParamRGB(p_color) * AiShaderEvalParamFlt(p_color_intensity);

        AtRGB emission = AI_RGB_WHITE;
        if (data->sample_emission)
            AiVolumeSampleRGB(data->emission_channel, data->interpolation, &emission);
        emission *= opacity * AiShaderEvalParamRGB(p_emission) * AiShaderEvalParamFlt(p_emission_intensity);

        AiShaderGlobalsSetVolumeScattering(sg, color, AiShaderEvalParamFlt(p_anisotropy));
        AiShaderGlobalsSetVolumeAttenuation(sg, opacity);
        AiShaderGlobalsSetVolumeEmission(sg, emission);
    }
}
