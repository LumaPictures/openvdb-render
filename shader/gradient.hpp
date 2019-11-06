// Copyright 2019 Luma Pictures
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once

#include <ai.h>

#include <string>
#include <vector>
#include <iostream>

#include "../util/gradient_base.hpp"

template <typename arnold_t> inline
arnold_t get_array_elem(AtArray* /*arr*/, unsigned int /*i*/) {
    return arnold_t();
}

template <> inline
float get_array_elem<float>(AtArray* arr, unsigned int i) {
    return AiArrayGetFlt(arr, i);
}

template <> inline
AtRGB get_array_elem<AtRGB>(AtArray* arr, unsigned int i) {
    return AiArrayGetRGB(arr, i);
}

class Gradient : public GradientBase<AtRGB> {
public:
    Gradient() : GradientBase<AtRGB>()
    {
    }

    ~Gradient()
    {

    }

    enum InterpolationType {
        INTERPOLATION_LINEAR = 0,
        INTERPOLATION_CATMULL_ROM,
        INTERPOLATION_BSPLINE
    };

    static void parameters(const std::string& base, AtList* params, AtNodeEntry* /*mds*/)
    {
        static const char* gradient_types[] = {"Raw", "Float", "RGB", "Float Ramp", "RGB Ramp", nullptr};
        static const char* ramp_types[] = {"linear", "catmull-rom", "bspline", nullptr};

        AiParameterEnum((base + "_channel_mode").c_str(), CHANNEL_MODE_RAW, gradient_types);
        AiParameterFlt((base + "_contrast").c_str(), 1.0f);
        AiParameterFlt((base + "_contrast_pivot").c_str(), 0.5f);
        AiParameterFlt((base + "_input_min").c_str(), 0.0f);
        AiParameterFlt((base + "_input_max").c_str(), 1.0f);
        AiParameterFlt((base + "_bias").c_str(), 0.5f);
        AiParameterFlt((base + "_gain").c_str(), 0.5f);
        AiParameterFlt((base + "_output_min").c_str(), 0.0f);
        AiParameterFlt((base + "_output_max").c_str(), 1.0f);
        AiParameterBool((base + "_clamp_min").c_str(), false);
        AiParameterBool((base + "_clamp_max").c_str(), false);
        AiParameterFlt((base + "_gamma").c_str(), 1.0f);
        AiParameterFlt((base + "_hue_shift").c_str(), 0.0f);
        AiParameterFlt((base + "_saturation").c_str(), 1.0f);
        AiParameterFlt((base + "_exposure").c_str(), 0.0f);
        AiParameterFlt((base + "_multiply").c_str(), 1.0f);
        AiParameterFlt((base + "_add").c_str(), 0.0f);

        AiParameterInt((base + "_float_ramp").c_str(), 4);
        AiParameterEnum((base + "_float_ramp_Interpolation").c_str(), INTERPOLATION_LINEAR, ramp_types);
        AiParameterArray((base + "_float_ramp_Knots").c_str(),
                         AiArray(4, 1, AI_TYPE_FLOAT, 0.0f, 0.0f, 1.0f, 1.0f));
        AiParameterArray((base + "_float_ramp_Floats").c_str(),
                         AiArray(4, 1, AI_TYPE_FLOAT, 0.0f, 0.0f, 1.0f, 1.0f));

        AiParameterInt((base + "_rgb_ramp").c_str(), 4);
        AiParameterEnum((base + "_rgb_ramp_Interpolation").c_str(), INTERPOLATION_LINEAR, ramp_types);
        AiParameterArray((base + "_rgb_ramp_Knots").c_str(),
                         AiArray(4, 1, AI_TYPE_FLOAT, 0.0f, 0.0f, 1.0f, 1.0f));
        AiParameterArray((base + "_rgb_ramp_Colors").c_str(),
                         AiArray(4, 1, AI_TYPE_RGB, AI_RGB_BLACK, AI_RGB_BLACK, AI_RGB_WHITE, AI_RGB_WHITE));
        // AiMetaDataSetBool(mds, (base + "_rgb_ramp_Colors").c_str(), "always_linear", true);
    }

    template <typename arnold_t> inline
    arnold_t interpolate_value(unsigned int num_knots, AtArray* knots, AtArray* values, float k) {
        if (k <= AI_EPSILON) {
            return get_array_elem<arnold_t>(values, 0);
        } else if (k >= (1.0f - AI_EPSILON)) {
            return get_array_elem<arnold_t>(values, num_knots - 1);
        } else {
            for (auto i = decltype(num_knots){1}; i < (num_knots - 1); ++i) {
                const auto k1 = AiArrayGetFlt(knots, i);
                if (k <= k1) {
                    const auto k0 = AiArrayGetFlt(knots, i - 1);
                    const auto v0 = get_array_elem<arnold_t>(values, i - 1);
                    const auto v1 = get_array_elem<arnold_t>(values, i);
                    const auto kd = k1 - k0;
                    if (kd < AI_EPSILON) {
                        return v0;
                    }
                    const auto t = (k - k0) / kd;
                    return
                        AiLerp(t, v0, v1);
                }
            }
            return get_array_elem<arnold_t>(values, num_knots - 1);
        }
    }

    void update(const std::string& base, AtNode* node)
    {
        m_contrast = AiNodeGetFlt(node, (base + "_contrast").c_str());
        m_contrast_pivot = AiNodeGetFlt(node, (base + "_contrast_pivot").c_str());
        m_input_min = AiNodeGetFlt(node, (base + "_input_min").c_str());
        m_input_max = AiNodeGetFlt(node, (base + "_input_max").c_str());
        m_bias = AiNodeGetFlt(node, (base + "_bias").c_str());
        if (m_bias < AI_EPSILON) {
            m_bias = 0.5f;
        }
        m_gain = AiNodeGetFlt(node, (base + "_gain").c_str());
        m_output_min = AiNodeGetFlt(node, (base + "_output_min").c_str());
        m_output_max = AiNodeGetFlt(node, (base + "_output_max").c_str());
        m_gamma = AiNodeGetFlt(node, (base + "_gamma").c_str());
        m_hue_shift = AiNodeGetFlt(node, (base + "_hue_shift").c_str());
        m_saturation = AiNodeGetFlt(node, (base + "_saturation").c_str());
        m_exposure = AiNodeGetFlt(node, (base + "_exposure").c_str());
        m_multiply = AiNodeGetFlt(node, (base + "_multiply").c_str());
        m_add = AiNodeGetFlt(node, (base + "_add").c_str());
        m_channel_mode = AiNodeGetInt(node, (base + "_channel_mode").c_str());
        m_clamp_min = AiNodeGetBool(node, (base + "_clamp_min").c_str());
        m_clamp_max = AiNodeGetBool(node, (base + "_clamp_max").c_str());

        static constexpr unsigned int bake_size = 512;

        if (m_channel_mode == CHANNEL_MODE_FLOAT_RAMP) {
            m_float_ramp.resize(bake_size);
            const auto num_knots = static_cast<unsigned int>(AiNodeGetInt(node, (base + "_float_ramp").c_str()));
            auto* knots_arr = AiNodeGetArray(node, (base + "_float_ramp_Knots").c_str());
            auto* value_arr = AiNodeGetArray(node, (base + "_float_ramp_Floats").c_str());
            if (num_knots >= 2 && knots_arr != nullptr && value_arr != nullptr) {
                for (auto i = decltype(bake_size){0}; i < bake_size; ++i) {
                    const auto v = static_cast<float>(i) / static_cast<float>(bake_size - 1);
                    m_float_ramp[i] = interpolate_value<float>(num_knots, knots_arr, value_arr, v);
                }
            } else {
                std::vector<float>().swap(m_float_ramp);
            }
        } else if (m_channel_mode == CHANNEL_MODE_RGB_RAMP) {
            m_rgb_ramp.resize(bake_size);
            const auto num_knots = static_cast<unsigned int>(AiNodeGetInt(node, (base + "_rgb_ramp").c_str()));
            auto* knots_arr = AiNodeGetArray(node, (base + "_rgb_ramp_Knots").c_str());
            auto* value_arr = AiNodeGetArray(node, (base + "_rgb_ramp_Colors").c_str());
            if (num_knots >= 2 && knots_arr != nullptr && value_arr != nullptr) {
                for (auto i = decltype(bake_size){0}; i < bake_size; ++i) {
                    const auto v = static_cast<float>(i) / static_cast<float>(bake_size - 1);
                    m_rgb_ramp[i] = interpolate_value<AtRGB>(num_knots, knots_arr, value_arr, v);
                }
            } else {
                std::vector<AtRGB>().swap(m_rgb_ramp);
            }
        }

        GradientBase<AtRGB>::update();
    }

    inline AtRGB evaluate_arnold(AtShaderGlobals* sg, const AtString& channel, int interpolation) const
    {
        AtRGB v = AI_RGB_BLACK;
        AiVolumeSampleRGB(channel, interpolation, &v);
        return evaluate(v);
    }
};

template<>
inline AtRGB GradientBase<AtRGB>::make_color(float r, float g, float b) const
{
    return AtRGB(r, g, b);
}
