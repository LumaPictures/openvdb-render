#include <ai.h>

#include <string>
#include <assert.h>

#include "gradient.hpp"

AI_SHADER_NODE_EXPORT_METHODS(openvdbShaderMethods);

namespace {
    enum {
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

    template <typename T>
    inline T eval(AtShaderGlobals*, const AtNode*, int) {
        assert("Unimplemented type.");
    }

    template <>
    inline float eval<float>(AtShaderGlobals* sg, const AtNode* node, int p) {
        return AiShaderEvalParamFuncFlt(sg, node, p);
    }

    template <>
    inline AtRGB eval<AtRGB>(AtShaderGlobals* sg, const AtNode* node, int p) {
        return AiShaderEvalParamFuncRGB(sg, node, p);
    }

    template <typename T>
    inline T get(const AtNode*, const char*) {
        assert("Unimplemented type.");
    }

    template <>
    inline float get<float>(const AtNode* node, const char* param_name) {
        return AiNodeGetFlt(node, param_name);
    }

    template <>
    inline AtRGB get<AtRGB>(const AtNode* node, const char* param_name) {
        return AiNodeGetRGB(node, param_name);
    }

    template <typename T>
    inline T zero() {
        assert("Unimplemented type.");
    }

    template <>
    inline float zero<float>() {
        return 0.0f;
    }

    template <>
    inline AtRGB zero<AtRGB>() {
        return AI_RGB_BLACK;
    }

    template<typename T, int p> struct alignas(8)
    Accessor {
        Accessor() : cache(T()), is_linked(false) {}

        inline T operator()(AtShaderGlobals* sg, const AtNode* node) const {
            return is_linked ? eval<T>(sg, node, p) : cache;
        }

        void init(const AtNode* node, const char* param_name) {
            is_linked = AiNodeIsLinked(node, param_name);
            cache = get<T>(node, param_name);
        }

        bool is_zero() const {
            return !is_linked && (cache == zero<T>());
        }

        T cache;
        bool is_linked;
    };

    static const char* scattering_source_labels[] = {"parameter", "channel", nullptr};
    static const char* attenuation_source_labels[] = {"parameter", "channel", "scattering", nullptr};
    static const char* emission_source_labels[] = {"parameter", "channel", nullptr};
    static const char* attenuation_mode_labels[] = {"absorption", "extinction", nullptr};
    static const char* interpolation_labels[] = {"closest", "trilinear", "tricubic", nullptr};

    enum InputSource {
        INPUT_SOURCE_PARAMETER,
        INPUT_SOURCE_CHANNEL,
        INPUT_SOURCE_SCATTERING,
    };

    enum AttenuationMode {
        ATTENUATION_MODE_ABSORPTION,
        ATTENUATION_MODE_EXTINCTION,
    };

    enum InputFrom {
        INPUT_FROM_NONE,
        INPUT_FROM_EVALUATE,
        INPUT_FROM_CACHE,
        INPUT_FROM_CHANNEL,
        INPUT_FROM_SCATTERING,
    };

    struct ShaderData {
        Gradient scattering_gradient;
        Gradient attenuation_gradient;
        Gradient emission_gradient;

        Accessor<AtRGB, p_scattering_color> scattering_color;
        Accessor<AtRGB, p_attenuation_color> attenuation_color;
        Accessor<AtRGB, p_emission_color> emission_color;

        Accessor<float, p_scattering_intensity> scattering_intensity;
        Accessor<float, p_attenuation_intensity> attenuation_intensity;
        Accessor<float, p_emission_intensity> emission_intensity;
        Accessor<float, p_anisotropy> anisotropy;

        AtString scattering_channel;
        AtString attenuation_channel;
        AtString emission_channel;

        AtRGB scattering;
        AtRGB attenuation;
        AtRGB emission;

        AtPoint position_offset;

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
        bool attenuation_is_linked;
        bool emission_is_linked;
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

            if (AiNodeIsLinked(node, "position_offset")) {
                position_offset_from = INPUT_FROM_EVALUATE;
            } else if (position_offset != AI_V3_ZERO) {
                position_offset_from = INPUT_FROM_CACHE;
            } else {
                position_offset_from = INPUT_FROM_NONE;
            }

            auto is_linked = [&] (const char* param_name) -> bool {
                return AiNodeIsLinked(node, param_name);
            };

            scattering_intensity.init(node, "scattering_intensity");
            attenuation_intensity.init(node, "attenuation_intensity");
            emission_intensity.init(node, "emission_intensity");
            anisotropy.init(node, "anisotropy");

            scattering_color.init(node, "scattering_color");
            attenuation_color.init(node, "attenuation_color");
            emission_color.init(node, "emission_color");

            // get linked status
            scattering_is_linked = is_linked("scattering");
            attenuation_is_linked = is_linked("attenuation");
            emission_is_linked = is_linked("emission");

            // cache parameter values
            interpolation = AiNodeGetInt(node, "interpolation");

            scattering_source = AiNodeGetInt(node, "scattering_source");
            scattering = AiNodeGetRGB(node, "scattering");
            scattering_channel = AtString(AiNodeGetStr(node, "scattering_channel"));

            attenuation_source = AiNodeGetInt(node, "attenuation_source");
            attenuation = AiNodeGetRGB(node, "attenuation");
            attenuation_channel = AtString(AiNodeGetStr(node, "attenuation_channel"));
            attenuation_mode = AiNodeGetInt(node, "attenuation_mode");

            emission_source = AiNodeGetInt(node, "emission_source");
            emission = AiNodeGetRGB(node, "emission");
            emission_channel = AtString(AiNodeGetStr(node, "emission_channel"));

            compensate_scaling = AiNodeGetBool(node, "compensate_scaling");

            switch (scattering_source) {
                case INPUT_SOURCE_PARAMETER:
                    scattering_from = scattering_is_linked ? INPUT_FROM_EVALUATE : INPUT_FROM_CACHE;
                    break;
                case INPUT_SOURCE_CHANNEL:
                    scattering_from = INPUT_FROM_CHANNEL;
                    break;
                default:
                    assert("invalid value for scattering_source");
            }

            switch (attenuation_source) {
                case INPUT_SOURCE_PARAMETER:
                    if (attenuation_is_linked) {
                        if (AiNodeGetLink(node, "attenuation") == AiNodeGetLink(node, "scattering")) {
                            attenuation_from = INPUT_FROM_SCATTERING;
                        }
                        else {
                            attenuation_from = INPUT_FROM_EVALUATE;
                        }
                    }
                    else {
                        attenuation_from = INPUT_FROM_CACHE;
                    }
                    break;
                case INPUT_SOURCE_CHANNEL:
                    if (scattering_channel == attenuation_channel) {
                        attenuation_from = INPUT_FROM_SCATTERING;
                    }
                    else {
                        attenuation_from = INPUT_FROM_CHANNEL;
                    }
                    break;
                case INPUT_SOURCE_SCATTERING:
                    attenuation_from = INPUT_FROM_SCATTERING;
                    break;
                default:
                    assert("invalid value for attenuation_source");
            }

            switch (emission_source) {
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
            if (scattering_intensity.is_zero()||
                scattering_color.is_zero()) {
                scattering_from = INPUT_FROM_CACHE;
                scattering = zero<AtRGB>();
            }

            if (attenuation_intensity.is_zero() || attenuation_color.is_zero()) {
                attenuation_from = INPUT_FROM_CACHE;
                attenuation = zero<AtRGB>();
            }

            if (emission_intensity.is_zero() || emission_color.is_zero()) {
                emission_from = INPUT_FROM_NONE;
                emission = zero<AtRGB>();
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
    AiMetaDataSetBool(mds, "scattering_channel", "volume_sample", true);
    AiMetaDataSetBool(mds, "attenuation_channel", "volume_sample", true);
    AiMetaDataSetBool(mds, "emission_channel", "volume_sample", true);
}

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
    delete reinterpret_cast<ShaderData*>(AiNodeGetLocalData(node));
}

shader_evaluate
{
    const ShaderData* data = reinterpret_cast<const ShaderData*>(AiNodeGetLocalData(node));

    // sampling position offset
    const auto Po_orig = sg->Po;

    if (data->position_offset_from == INPUT_FROM_EVALUATE) {
        sg->Po += AiShaderEvalParamVec(p_position_offset);
    } else if (data->position_offset_from == INPUT_FROM_CACHE) {
        sg->Po += data->position_offset;
    }

    AtColor scattering = AI_RGB_BLACK;
    AtColor attenuation = AI_RGB_BLACK;

    AtVector scaled_dir;
    AiM4VectorByMatrixMult(&scaled_dir, sg->Minv, &sg->Rd);
    const float scale_factor = AiV3Length(scaled_dir);

    // scattering
    if (!(sg->Rt & AI_RAY_SHADOW) || (data->attenuation_from == INPUT_FROM_SCATTERING) ||
        (data->attenuation_mode == ATTENUATION_MODE_ABSORPTION)) {
        switch (data->scattering_from) {
            case INPUT_FROM_CHANNEL:
                scattering = data->scattering_gradient.evaluate_arnold(sg, data->scattering_channel,
                                                                       data->interpolation);
                break;
            case INPUT_FROM_EVALUATE:
                scattering = AiShaderEvalParamRGB(p_scattering);
                break;
            case INPUT_FROM_CACHE:
                scattering = data->scattering;
                break;
            default:
                assert("[openvdb_render] Invalid value for data->scattering_from!");
        }

        if (!(sg->Rt & AI_RAY_SHADOW) || (data->attenuation_mode == ATTENUATION_MODE_ABSORPTION)) {
            // color, intensity, anisotropy and clipping
            const auto scattering_color = data->scattering_color(sg, node);
            const auto scattering_intensity = data->scattering_intensity(sg, node);
            const auto anisotropy = data->anisotropy(sg, node);

            AtRGB scattering_result = scattering * scattering_color * scattering_intensity;
            AiColorClipToZero(scattering_result);

            // update volume shader globals
            AiShaderGlobalsSetVolumeScattering(sg, scattering_result *
                                                   (((data->attenuation_mode == ATTENUATION_MODE_ABSORPTION) &&
                                                     data->compensate_scaling) ? scale_factor : 1.0f), anisotropy);
        }
    }

    // attenuation
    switch (data->attenuation_from) {
        case INPUT_FROM_CHANNEL:
            attenuation = data->attenuation_gradient.evaluate_arnold(sg, data->attenuation_channel,
                                                                     data->interpolation);
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
        default:
            assert("[openvdb_render] Invalid value for data->attenuation_from!");
    }

    const auto attenuation_color = data->attenuation_color(sg, node);
    const auto attenuation_intensity = data->attenuation_intensity(sg, node);
    attenuation *= attenuation_color * attenuation_intensity;
    AiColorClipToZero(attenuation);

    // update volume shader globals
    switch (data->attenuation_mode) {
        case ATTENUATION_MODE_ABSORPTION:
            AiShaderGlobalsSetVolumeAbsorption(sg, attenuation * (data->compensate_scaling ? scale_factor : 1.0f));
            break;
        case ATTENUATION_MODE_EXTINCTION:
            AiShaderGlobalsSetVolumeAttenuation(sg, attenuation * (data->compensate_scaling ? scale_factor : 1.0f));
            break;
        default:
            assert("[openvdb_render] Invalid attenuation mode!");
    }

    // emission
    if (!(sg->Rt & AI_RAY_SHADOW) && (data->emission_from != INPUT_FROM_NONE)) {
        AtColor emission = AI_RGB_BLACK;

        switch (data->emission_from) {
            case INPUT_FROM_CHANNEL:
                emission = data->emission_gradient.evaluate_arnold(sg, data->emission_channel, data->interpolation);
                break;
            case INPUT_FROM_EVALUATE:
                emission = AiShaderEvalParamRGB(p_emission);
                break;
            case INPUT_FROM_CACHE:
                emission = data->emission;
                break;
            default:
                assert("[openvdb_render] Invalid value for data->emission_from!");
        }

        const auto emission_color = data->emission_color(sg, node);
        const auto emission_intensity = data->emission_intensity(sg, node);
        emission *= emission_color * emission_intensity;
        AiColorClipToZero(emission);

        // update volume shader globals
        if (!AiColorIsZero(emission)) {
            AiShaderGlobalsSetVolumeEmission(sg, emission);
        }
    }

    // restore sampling position
    if (data->position_offset_from != INPUT_FROM_NONE) {
        sg->Po = Po_orig;
    }
}
