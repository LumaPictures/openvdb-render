#pragma once

#include <ai.h>

#include <string>
#include <vector>
#include <iostream>

#include "../util/gradient_base.hpp"

class Gradient : public GradientBase<AtRGB> {
public:
    Gradient() : GradientBase<AtRGB>()
    { }

    ~Gradient()
    {

    }

    static void parameters(const std::string& base, AtList* params, AtMetaDataStore*)
    {
        static const char* gradient_types[] = {"Raw", "Float", "RGB", "Float Ramp", "RGB Ramp", nullptr};

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
        AiParameterArray((base + "_float_ramp").c_str(), AiArray(2, 1, AI_TYPE_FLOAT, 0.0f, 1.0f));
        AiParameterArray((base + "_rgb_ramp").c_str(), AiArray(2, 1, AI_TYPE_RGB, AI_RGB_BLACK, AI_RGB_WHITE));
    }

    void update(const std::string& base, AtNode* node, AtParamValue*)
    {
        m_contrast = AiNodeGetFlt(node, (base + "_contrast").c_str());
        m_contrast_pivot = AiNodeGetFlt(node, (base + "_contrast_pivot").c_str());
        m_input_min = AiNodeGetFlt(node, (base + "_input_min").c_str());
        m_input_max = AiNodeGetFlt(node, (base + "_input_max").c_str());
        m_bias = AiNodeGetFlt(node, (base + "_bias").c_str());
        if (m_bias < AI_EPSILON)
            m_bias = 0.5f;
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

        if (m_channel_mode == CHANNEL_MODE_FLOAT_RAMP)
        {
            AtArray* arr = AiNodeGetArray(node, (base + "_float_ramp").c_str());
            if (arr != nullptr)
            {
                const unsigned int nelements = arr->nelements;
                if (m_float_ramp.size() != nelements)
                    std::vector<float>().swap(m_float_ramp);
                else
                    m_float_ramp.clear();
                if (nelements > 0)
                {
                    m_float_ramp.reserve(nelements);
                    for (unsigned int i = 0; i < nelements; ++i)
                        m_float_ramp.push_back(AiArrayGetFlt(arr, i));
                }
            }
            else
                std::vector<float>().swap(m_float_ramp);
        }
        else if (m_channel_mode == CHANNEL_MODE_RGB_RAMP)
        {
            AtArray* arr = AiNodeGetArray(node, (base + "_rgb_ramp").c_str());
            if (arr != nullptr)
            {
                const unsigned nelements = arr->nelements;
                if (m_rgb_ramp.size() != nelements)
                    std::vector<AtRGB>().swap(m_rgb_ramp);
                else
                    m_rgb_ramp.clear();
                if (nelements > 0)
                {
                    m_rgb_ramp.reserve(nelements);
                    for (unsigned int i = 0; i < nelements; ++i)
                        m_rgb_ramp.push_back(AiArrayGetRGB(arr, i));
                }
            }
            else
                std::vector<AtRGB>().swap(m_rgb_ramp);
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

template <>
inline AtRGB GradientBase<AtRGB>::make_color(float r, float g, float b) const
{
    return AiColorCreate(r, g, b);
}
