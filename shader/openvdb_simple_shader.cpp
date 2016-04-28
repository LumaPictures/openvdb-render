#include <ai.h>

AI_SHADER_NODE_EXPORT_METHODS(openvdbSimpleShaderMethods);

namespace {
    struct ShaderData{
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

        ShaderData() : interpolation(0), sample_smoke(false), sample_opacity(false), sample_fire(false), compensate_scaling(true)
        {

        }

        void update(AtNode* node, AtParamValue*)
        {
            smoke_channel = AtString(AiNodeGetStr(node, "smoke_channel"));
            opacity_channel = AtString(AiNodeGetStr(node, "opacity_channel"));
            fire_channel = AtString(AiNodeGetStr(node, "fire_channel"));

            sample_smoke = smoke_channel.length() > 0;
            sample_opacity = opacity_channel.length() > 0;
            sample_fire = fire_channel.length() > 0;
        }
    };

    enum{
        p_smoke,
        p_smoke_channel,
        p_smoke_intensity,
        p_anisotropy,
        p_opacity,
        p_opacity_channel,
        p_opacity_intensity,
        p_fire,
        p_fire_channel,
        p_fire_intensity,
        p_position_offset,
        p_interpolation,
        p_compensate_scaling
    };

    const char* interpolations[] = { "closest", "trilinear", "tricubic", nullptr};
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

    AiParameterRGB("fire", 1.0f, 1.0f, 1.0f);
    AiParameterStr("fire_channel", "");
    AiParameterFlt("fire_intensity", 1.0f);

    AiParameterVec("position_offset", 0.0f, 0.0f, 0.0f);
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

        AtRGB smoke = AI_RGB_WHITE;
        if (data->sample_smoke)
            AiVolumeSampleRGB(data->smoke_channel, data->interpolation, &smoke);
        smoke *= opacity * AiShaderEvalParamRGB(p_smoke) * AiShaderEvalParamFlt(p_smoke_intensity);

        AtRGB fire = AI_RGB_WHITE;
        if (data->sample_fire)
            AiVolumeSampleRGB(data->fire_channel, data->interpolation, &fire);
        fire *= opacity * AiShaderEvalParamRGB(p_fire) * AiShaderEvalParamFlt(p_fire_intensity);

        AiShaderGlobalsSetVolumeScattering(sg, smoke, AiShaderEvalParamFlt(p_anisotropy));
        AiShaderGlobalsSetVolumeAttenuation(sg, opacity);
        AiShaderGlobalsSetVolumeEmission(sg, fire);
    }
}
