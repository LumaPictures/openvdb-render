#pragma once

#include <translators/shader/ShaderTranslator.h>

#include <algorithm>

template <typename T, typename H> inline void
convert_maya_to_arnold(T& trg, const H& src) {
    trg = src;
};

template <> inline void
convert_maya_to_arnold<AtRGB, MColor>(AtRGB& trg, const MColor& src) {
    trg.r = src.r;
    trg.g = src.g;
    trg.b = src.b;
};

template <typename T> inline void
set_arnold_arr_elem(AtArray* /*arr*/, unsigned int /*a_i*/, const T& /*st*/) {

}

template <> inline void
set_arnold_arr_elem<float>(AtArray* arr, unsigned int a_i, const float& st) {
    AiArraySetFlt(arr, a_i, st);
}

template <> inline void
set_arnold_arr_elem<AtRGB>(AtArray* arr, unsigned int a_i, const AtRGB& st) {
    AiArraySetRGB(arr, a_i, st);
}

template<typename translator_class>
class VDBShaderParamsTranslator : public translator_class {
public:
    template <typename value_array_t, typename arnold_type_t>
    void export_gradient(AtNode* shader, const std::string& ramp_name, const std::string& type_name, uint8_t arnold_value_enum, MPlug plug) {
        MRampAttribute ramp_attr(plug);
        MIntArray indexes;
        MFloatArray positions;
        value_array_t values;
        MIntArray interps;
        ramp_attr.getEntries(indexes, positions, values, interps);
        const auto indexes_size = indexes.length();
        if (indexes_size > 0) {
            const auto float_ramp_interp_name = ramp_name + "_Interpolation";
            const auto float_ramp_knots_name = ramp_name + "_Knots";
            const auto float_ramp_floats_name = ramp_name + "_" + type_name;

            using pair_t = std::pair<float, arnold_type_t>;
            std::vector<pair_t> sorted_values;
            sorted_values.resize(indexes_size);
            for (auto i = 0u; i < indexes_size; ++i) {
                sorted_values[i].first =
#ifdef ARNOLD5
                    AiClamp(positions[i], 0.0f, 1.0f)
#else
                    CLAMP(positions[i], 0.0f, 1.0f)
#endif
                    ;
                convert_maya_to_arnold(sorted_values[i].second, values[i]);
            }
            std::sort(sorted_values.begin(), sorted_values.end(), [](const pair_t& a, const pair_t& b) {
                return a.first < b.first;
            });

            AiNodeSetInt(shader, ramp_name.c_str(), indexes_size + 2);
            AiNodeSetInt(shader, float_ramp_interp_name.c_str(), 0);
            auto* knots_arr = AiArrayAllocate(indexes_size + 2, 1, AI_TYPE_FLOAT);
            auto* values_arr = AiArrayAllocate(indexes_size + 2, 1, arnold_value_enum);
            AiArraySetFlt(knots_arr, 0, 0.0f);
            AiArraySetFlt(knots_arr, indexes_size + 1, 1.0f);
            set_arnold_arr_elem(values_arr, 0, sorted_values.front().second);
            set_arnold_arr_elem(values_arr, indexes_size + 1, sorted_values.back().second);
            for (auto i = 0u; i < indexes_size; ++i) {
                AiArraySetFlt(knots_arr, i + 1, sorted_values[i].first);
                set_arnold_arr_elem(values_arr, i + 1, sorted_values[i].second);
            }
            AiNodeSetArray(shader, float_ramp_knots_name.c_str(), knots_arr);
            AiNodeSetArray(shader, float_ramp_floats_name.c_str(), values_arr);
        }
    }

    void export_gradients(AtNode* shader, const std::vector<std::string>& gradient_names)
    {
        for (const auto& gradient : gradient_names) {
            this->ProcessParameter(shader, (gradient + "_channel_mode").c_str(), AI_TYPE_INT,
                                   (gradient + "ChannelMode").c_str());
            this->ProcessParameter(shader, (gradient + "_contrast").c_str(), AI_TYPE_FLOAT,
                                   (gradient + "Contrast").c_str());
            this->ProcessParameter(shader, (gradient + "_contrast_pivot").c_str(), AI_TYPE_FLOAT,
                                   (gradient + "ContrastPivot").c_str());
            this->ProcessParameter(shader, (gradient + "_input_min").c_str(), AI_TYPE_FLOAT,
                                   (gradient + "InputMin").c_str());
            this->ProcessParameter(shader, (gradient + "_input_max").c_str(), AI_TYPE_FLOAT,
                                   (gradient + "InputMax").c_str());
            this->ProcessParameter(shader, (gradient + "_bias").c_str(), AI_TYPE_FLOAT, (gradient + "Bias").c_str());
            this->ProcessParameter(shader, (gradient + "_gain").c_str(), AI_TYPE_FLOAT, (gradient + "Gain").c_str());
            this->ProcessParameter(shader, (gradient + "_output_min").c_str(), AI_TYPE_FLOAT,
                                   (gradient + "OutputMin").c_str());
            this->ProcessParameter(shader, (gradient + "_output_max").c_str(), AI_TYPE_FLOAT,
                                   (gradient + "OutputMax").c_str());
            this->ProcessParameter(shader, (gradient + "_clamp_min").c_str(), AI_TYPE_BOOLEAN,
                                   (gradient + "ClampMin").c_str());
            this->ProcessParameter(shader, (gradient + "_clamp_max").c_str(), AI_TYPE_BOOLEAN,
                                   (gradient + "ClampMax").c_str());
            this->ProcessParameter(shader, (gradient + "_gamma").c_str(), AI_TYPE_FLOAT, (gradient + "Gamma").c_str());
            this->ProcessParameter(shader, (gradient + "_hue_shift").c_str(), AI_TYPE_FLOAT,
                                   (gradient + "HueShift").c_str());
            this->ProcessParameter(shader, (gradient + "_saturation").c_str(), AI_TYPE_FLOAT,
                                   (gradient + "Saturation").c_str());
            this->ProcessParameter(shader, (gradient + "_exposure").c_str(), AI_TYPE_FLOAT,
                                   (gradient + "Exposure").c_str());
            this->ProcessParameter(shader, (gradient + "_multiply").c_str(), AI_TYPE_FLOAT,
                                   (gradient + "Multiply").c_str());
            this->ProcessParameter(shader, (gradient + "_add").c_str(), AI_TYPE_FLOAT, (gradient + "Add").c_str());

            MStatus status = MS::kSuccess;
            MPlug plug = this->FindMayaPlug((gradient + "FloatRamp").c_str(), &status);
            if (status && !plug.isNull()) {
                export_gradient<MFloatArray, float>(shader, gradient + "_float_ramp", "Floats", AI_TYPE_FLOAT, plug);
            }

            plug = this->FindMayaPlug((gradient + "RgbRamp").c_str(), &status);
            if (status && !plug.isNull()) {
                export_gradient<MColorArray, AtRGB>(shader, gradient + "_rgb_ramp", "Colors", AI_TYPE_RGB, plug);
            }
        }
    }

    inline void ExportSimpleParams(AtNode* shader)
    {

        this->ProcessParameter(shader, "smoke", AI_TYPE_RGB, "smoke");
        this->ProcessParameter(shader, "smoke_channel", AI_TYPE_STRING, "smokeChannel");
        this->ProcessParameter(shader, "smoke_intensity", AI_TYPE_FLOAT, "smokeIntensity");
        this->ProcessParameter(shader, "anisotropy", AI_TYPE_FLOAT, "anisotropy");

        this->ProcessParameter(shader, "opacity", AI_TYPE_RGB, "opacity");
        this->ProcessParameter(shader, "opacity_channel", AI_TYPE_STRING, "opacityChannel");
        this->ProcessParameter(shader, "opacity_intensity", AI_TYPE_FLOAT, "opacityIntensity");
        this->ProcessParameter(shader, "opacity_shadow", AI_TYPE_RGB, "opacityShadow");

        this->ProcessParameter(shader, "fire", AI_TYPE_RGB, "fire");
        this->ProcessParameter(shader, "fire_channel", AI_TYPE_STRING, "fireChannel");
        this->ProcessParameter(shader, "fire_intensity", AI_TYPE_FLOAT, "fireIntensity");

        this->ProcessParameter(shader, "position_offset", AI_TYPE_VECTOR, "positionOffset");
        this->ProcessParameter(shader, "interpolation", AI_TYPE_INT, "interpolation");
        this->ProcessParameter(shader, "compensate_scaling", AI_TYPE_BOOLEAN, "compensateScaling");

        std::vector<std::string> gradient_names = {"simpleSmoke", "simpleOpacity", "simpleFire"};
        export_gradients(shader, gradient_names);
    }
};
