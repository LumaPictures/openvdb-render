#include <ai.h>

#include <string>
#include <assert.h>

#include "gradient.hpp"

AI_SHADER_NODE_EXPORT_METHODS(openvdbShaderMethods);

namespace {
    enum VolumeCollectorParams
    {
        p_scattering_source,
        p_scattering,
        p_scattering_channel,
        p_scattering_color,
        p_scattering_intensity,
        p_anisotropy,
        p_attenuation_source,
        p_attenuation,
        p_attenuation_channel,
        p_attenuation_color,
        p_attenuation_intensity,
        p_attenuation_mode,
        p_emission_source,
        p_emission,
        p_emission_channel,
        p_emission_color,
        p_emission_intensity,
        p_position_offset,
        p_interpolation,
        p_compensate_scaling
    };

    static const char* scattering_source_labels[] = { "parameter", "channel", NULL };
    static const char* attenuation_source_labels[] = { "parameter", "channel", "scattering", NULL };
    static const char* emission_source_labels[] = { "parameter", "channel", NULL };
    static const char* attenuation_mode_labels[] = { "absorption", "extinction", NULL };
    static const char* interpolation_labels[] = { "closest", "trilinear", "tricubic", NULL };

    enum InputSource
    {
        INPUT_SOURCE_PARAMETER,
        INPUT_SOURCE_CHANNEL,
        INPUT_SOURCE_SCATTERING,
    };

    enum AttenuationMode
    {
        ATTENUATION_MODE_ABSORPTION,
        ATTENUATION_MODE_EXTINCTION,
    };

    enum InputFrom
    {
        INPUT_FROM_NONE,
        INPUT_FROM_EVALUATE,
        INPUT_FROM_CACHE,
        INPUT_FROM_CHANNEL,
        INPUT_FROM_SCATTERING,
    };

    struct ShaderData
    {
        Gradient scattering_gradient;
        Gradient attenuation_gradient;
        Gradient emission_gradient;

        AtString scattering_channel;
        AtString attenuation_channel;
        AtString emission_channel;
        AtRGB scattering;
        AtRGB scattering_color;
        AtRGB attenuation;
        AtRGB attenuation_color;
        AtRGB emission;
        AtRGB emission_color;

        AtPoint position_offset;

        float scattering_intensity;
        float anisotropy;
        float attenuation_intensity;
        float emission_intensity;

        int attenuation_mode;
        int interpolation;
        int scattering_source;
        int attenuation_source;
        int emission_source;

        InputFrom scattering_from;
        InputFrom attenuation_from;
        InputFrom emission_from;
        InputFrom position_offset_from;

        bool scattering_is_linked;
        bool scattering_color_is_linked;
        bool scattering_intensity_is_linked;
        bool anisotropy_is_linked;
        bool attenuation_is_linked;
        bool attenuation_color_is_linked;
        bool attenuation_intensity_is_linked;
        bool emission_is_linked;
        bool emission_color_is_linked;
        bool emission_intensity_is_linked;
        bool compensate_scaling;

        void* operator new(size_t size)
        {
            return AiMalloc(static_cast<unsigned int>(size));
        }

        void operator delete(void* d)
        {
            AiFree(d);
        }

        void update(AtNode* node, AtParamValue* params)
        {
            scattering_gradient.update("scattering", node, params);
            attenuation_gradient.update("attenuation", node, params);
            emission_gradient.update("emission", node, params);

            // position offset
            position_offset = AiNodeGetVec(node, "position_offset");

            if (AiNodeIsLinked(node, "position_offset"))
                position_offset_from = INPUT_FROM_EVALUATE;
            else if (position_offset != AI_V3_ZERO)
                position_offset_from = INPUT_FROM_CACHE;
            else
                position_offset_from = INPUT_FROM_NONE;

            // get linked status
            scattering_is_linked = AiNodeIsLinked(node, "scattering");
            scattering_color_is_linked = AiNodeIsLinked(node, "scattering_color");
            scattering_intensity_is_linked = AiNodeIsLinked(node, "scattering_intensity");
            anisotropy_is_linked = AiNodeIsLinked(node, "anisotropy");

            attenuation_is_linked = AiNodeIsLinked(node, "attenuation");
            attenuation_color_is_linked = AiNodeIsLinked(node, "attenuation_color");
            attenuation_intensity_is_linked = AiNodeIsLinked(node, "attenuation_intensity");

            emission_is_linked = AiNodeIsLinked(node, "emission");
            emission_color_is_linked = AiNodeIsLinked(node, "emission_color");
            emission_intensity_is_linked = AiNodeIsLinked(node, "emission_intensity");

            // cache parameter values
            interpolation = AiNodeGetInt(node, "interpolation");

            scattering_source = AiNodeGetInt(node, "scattering_source");
            scattering = AiNodeGetRGB(node, "scattering");
            scattering_channel = AtString(AiNodeGetStr(node, "scattering_channel"));
            scattering_color = AiNodeGetRGB(node, "scattering_color");
            scattering_intensity = AiNodeGetFlt(node, "scattering_intensity");
            anisotropy = AiNodeGetFlt(node, "anisotropy");

            attenuation_source = AiNodeGetInt(node, "attenuation_source");
            attenuation = AiNodeGetRGB(node, "attenuation");
            attenuation_channel = AtString(AiNodeGetStr(node, "attenuation_channel"));
            attenuation_color = AiNodeGetRGB(node, "attenuation_color");
            attenuation_intensity = AiNodeGetFlt(node, "attenuation_intensity");
            attenuation_mode = AiNodeGetInt(node, "attenuation_mode");

            emission_source = AiNodeGetInt(node, "emission_source");
            emission = AiNodeGetRGB(node, "emission");
            emission_channel = AtString(AiNodeGetStr(node, "emission_channel"));
            emission_color = AiNodeGetRGB(node, "emission_color");
            emission_intensity = AiNodeGetFlt(node, "emission_intensity");

            compensate_scaling = AiNodeGetBool(node, "compensate_scaling");

            switch (scattering_source)
            {
                case INPUT_SOURCE_PARAMETER:
                    scattering_from = scattering_is_linked ? INPUT_FROM_EVALUATE : INPUT_FROM_CACHE;
                    break;
                case INPUT_SOURCE_CHANNEL:
                    scattering_from = INPUT_FROM_CHANNEL;
                    break;
                default:
                    assert("invalid value for scattering_source");
            }

            switch (attenuation_source)
            {
                case INPUT_SOURCE_PARAMETER:
                    if (attenuation_is_linked)
                    {
                        if (AiNodeGetLink(node, "attenuation") == AiNodeGetLink(node, "scattering"))
                            attenuation_from = INPUT_FROM_SCATTERING;
                        else
                            attenuation_from = INPUT_FROM_EVALUATE;
                    }
                    else
                        attenuation_from = INPUT_FROM_CACHE;
                    break;
                case INPUT_SOURCE_CHANNEL:
                    if (scattering_channel == attenuation_channel)
                        attenuation_from = INPUT_FROM_SCATTERING;
                    else
                        attenuation_from = INPUT_FROM_CHANNEL;
                    break;
                case INPUT_SOURCE_SCATTERING:
                    attenuation_from = INPUT_FROM_SCATTERING;
                    break;
                default:
                    assert("invalid value for attenuation_source");
            }

            switch (emission_source)
            {
                case INPUT_SOURCE_PARAMETER:
                    emission_from = emission_is_linked ? INPUT_FROM_EVALUATE : INPUT_FROM_CACHE;
                    break;
                case INPUT_SOURCE_CHANNEL:
                    emission_from = INPUT_FROM_CHANNEL;
                    break;
                default:
                    assert("invalid value for emission_source");
            }

            // detect constant zero values for color and intensity
            if ((!scattering_intensity_is_linked && scattering_intensity == 0.0f) ||
                (!scattering_color_is_linked && AiColorEqual(scattering_color, AI_RGB_BLACK)))
            {
                scattering_from = INPUT_FROM_CACHE;
                scattering = AI_RGB_BLACK;
            }

            if ((!attenuation_intensity_is_linked && attenuation_intensity == 0.0f) ||
                (!attenuation_color_is_linked && AiColorEqual(attenuation_color, AI_RGB_BLACK)))
            {
                attenuation_from = INPUT_FROM_CACHE;
                attenuation = AI_RGB_BLACK;
            }

            if ((!emission_intensity_is_linked && emission_intensity == 0.0f) ||
                (!emission_color_is_linked && AiColorEqual(emission_color, AI_RGB_BLACK)))
            {
                emission_from = INPUT_FROM_NONE;
                emission = AI_RGB_BLACK;
            }
        }
    };
}

node_parameters
{
    AiParameterEnum("scattering_source", INPUT_SOURCE_PARAMETER, scattering_source_labels);
    AiParameterRGB("scattering", 1.0f, 1.0f, 1.0f);
    AiParameterStr("scattering_channel", "");
    AiParameterRGB("scattering_color", 1.0f, 1.0f, 1.0f);
    AiParameterFlt("scattering_intensity", 1.0f);
    AiParameterFlt("anisotropy", 0.0f);
    AiParameterEnum("attenuation_source", INPUT_SOURCE_PARAMETER, attenuation_source_labels);
    AiParameterRGB("attenuation", 1.0f, 1.0f, 1.0f);
    AiParameterStr("attenuation_channel", "");
    AiParameterRGB("attenuation_color", 1.0f, 1.0f, 1.0f);
    AiParameterFlt("attenuation_intensity", 1.0f);
    AiParameterEnum("attenuation_mode", ATTENUATION_MODE_ABSORPTION, attenuation_mode_labels);
    AiParameterEnum("emission_source", INPUT_SOURCE_PARAMETER, emission_source_labels);
    AiParameterRGB("emission", 0.0f, 0.0f, 0.0f);
    AiParameterStr("emission_channel", "");
    AiParameterRGB("emission_color", 1.0f, 1.0f, 1.0f);
    AiParameterFlt("emission_intensity", 1.0f);
    AiParameterVec("position_offset", 0.0f, 0.0f, 0.0f);
    AiParameterEnum("interpolation", AI_VOLUME_INTERP_TRILINEAR, interpolation_labels);
    AiParameterBool("compensate_scaling", true);

    Gradient::parameters("scattering", params, mds);
    Gradient::parameters("attenuation", params, mds);
    Gradient::parameters("emission", params, mds);

    AiMetaDataSetBool(mds, 0, "maya.hide", true);
}

static void Initialize(AtNode* node, AtParamValue*)
//node_initialize
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
    delete reinterpret_cast<ShaderData*>(AiNodeGetLocalData(node));
}

shader_evaluate
{
    const ShaderData* data = reinterpret_cast<const ShaderData*>(AiNodeGetLocalData(node));

    // sampling position offset
    AtPoint Po_orig = AI_V3_ZERO;

    switch (data->position_offset_from)
    {
        case INPUT_FROM_EVALUATE:
            Po_orig = sg->Po;
            sg->Po += AiShaderEvalParamVec(p_position_offset);
            break;
        case INPUT_FROM_CACHE:
            Po_orig = sg->Po;
            sg->Po += data->position_offset;
            break;
        default:
            Po_orig = AI_V3_ZERO;
            break;
    }

    // the values storing the result of AiVolumeSampleRGB() need to be zeroed
    // or NaNs will occur in optimized builds (htoa#374)
    AtColor scattering  = AI_RGB_BLACK;
    AtColor attenuation = AI_RGB_BLACK;

    AtVector scaled_dir;
    AiM4VectorByMatrixMult(&scaled_dir, sg->Minv, &sg->Rd);
    const float scale_factor = AiV3Length(scaled_dir);

    // scattering
    if (!(sg->Rt & AI_RAY_SHADOW) || (data->attenuation_from == INPUT_FROM_SCATTERING) || (data->attenuation_mode == ATTENUATION_MODE_ABSORPTION))
    {
        switch (data->scattering_from)
        {
            case INPUT_FROM_CHANNEL:
                scattering = data->scattering_gradient.evaluate_arnold(sg, data->scattering_channel, data->interpolation);
                break;
            case INPUT_FROM_EVALUATE:
                scattering = AiShaderEvalParamRGB(p_scattering);
                break;
            case INPUT_FROM_CACHE:
                scattering = data->scattering;
                break;
            default:
                assert("invalid value for data->scattering_from");
                break;
        }

        if (!(sg->Rt & AI_RAY_SHADOW) || (data->attenuation_mode == ATTENUATION_MODE_ABSORPTION))
        {
            // color, intensity, anisotropy and clipping
            const AtRGB scattering_color= data->scattering_color_is_linked     ? AiShaderEvalParamRGB(p_scattering_color)     : data->scattering_color;
            const float scattering_intensity = data->scattering_intensity_is_linked ? AiShaderEvalParamFlt(p_scattering_intensity) : data->scattering_intensity;
            const float anisotropy = data->anisotropy_is_linked           ? AiShaderEvalParamFlt(p_anisotropy)           : data->anisotropy;

            AtRGB scattering_result = scattering * scattering_color * scattering_intensity;
            AiColorClipToZero(scattering_result);

            // update volume shader globals
            AiShaderGlobalsSetVolumeScattering(sg, scattering_result * (((data->attenuation_mode == ATTENUATION_MODE_ABSORPTION) && data->compensate_scaling) ? scale_factor : 1.0f), anisotropy);
        }
    }

    // attenuation
    switch (data->attenuation_from)
    {
        case INPUT_FROM_CHANNEL:
            attenuation = data->attenuation_gradient.evaluate_arnold(sg, data->attenuation_channel, data->interpolation);
            break;
        case INPUT_FROM_EVALUATE:
            attenuation = AiShaderEvalParamRGB(p_attenuation);
            break;
        case INPUT_FROM_CACHE:
            attenuation = data->attenuation;
            break;
        case INPUT_FROM_SCATTERING:
            attenuation = scattering;
            break;
        default: assert("invalid value for data->attenuation_from"); break;
    }

    // color, intensity and clipping
    const AtRGB attenuation_color = data->attenuation_color_is_linked ? AiShaderEvalParamRGB(p_attenuation_color) : data->attenuation_color;
    const float attenuation_intensity = data->attenuation_intensity_is_linked ? AiShaderEvalParamFlt(p_attenuation_intensity) : data->attenuation_intensity;
    attenuation *= attenuation_color * attenuation_intensity;
    AiColorClipToZero(attenuation);

    // update volume shader globals
    switch (data->attenuation_mode)
    {
        case ATTENUATION_MODE_ABSORPTION:
            AiShaderGlobalsSetVolumeAbsorption(sg, attenuation * (data->compensate_scaling ? scale_factor : 1.0f));
            break;
        case ATTENUATION_MODE_EXTINCTION:
            AiShaderGlobalsSetVolumeAttenuation(sg, attenuation * (data->compensate_scaling ? scale_factor : 1.0f));
            break;
        default: assert("Invalid attenuation mode!");
    }

    // emission
    if (!(sg->Rt & AI_RAY_SHADOW) && (data->emission_from != INPUT_FROM_NONE))
    {
        AtColor emission = AI_RGB_BLACK;

        switch (data->emission_from)
        {
            case INPUT_FROM_CHANNEL:
                data->emission_gradient.evaluate_arnold(sg, data->emission_channel, data->interpolation);
                break;
            case INPUT_FROM_EVALUATE:
                emission = AiShaderEvalParamRGB(p_emission);
                break;
            case INPUT_FROM_CACHE:
                emission = data->emission;
                break;
            default: assert("invalid value for data->emission_from");
        }

        // color, intensity and clipping
        const AtRGB emission_color = data->emission_color_is_linked ? AiShaderEvalParamRGB(p_emission_color) : data->emission_color;
        const float emission_intensity = data->emission_intensity_is_linked ? AiShaderEvalParamFlt(p_emission_intensity) : data->emission_intensity;
        emission *= emission_color * emission_intensity;
        AiColorClipToZero(emission);

        // update volume shader globals
        if (!AiColorIsZero(emission))
            AiShaderGlobalsSetVolumeEmission(sg, emission);
    }

    // restore sampling position
    if (data->position_offset_from != INPUT_FROM_NONE)
        sg->Po = Po_orig;
}
