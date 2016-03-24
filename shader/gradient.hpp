#pragma once

#include <ai.h>

#include <string>
#include <vector>

class Gradient {
private:
    enum {
        GRADIENT_NONE,
        GRADIENT_FLOAT,
        GRADIENT_RGB
    };

    std::vector<float> m_float_ramp;
    std::vector<AtRGB> m_rgb_ramp;

    float m_contrast;
    float m_input_min;
    float m_input_max;
    float m_bias;
    float m_gain;
    float m_output_min;
    float m_output_max;
    float m_gamma;
    float m_hue_shift;
    float m_saturation;
    float m_exposure;
    float m_multiply;
    float m_add;
    int m_gradient_type;
    bool m_clamp_min;
    bool m_clamp_max;

public:
    Gradient() : m_contrast(1.0f), m_input_min(0.0f), m_input_max(1.0f),
                 m_bias(0.5f), m_gain(0.5f), m_output_min(0.0f), m_output_max(1.0f),
                 m_gamma(1.0f), m_hue_shift(0.0f), m_saturation(1.0f), m_exposure(0.0f),
                 m_multiply(1.0f), m_add(0.0f), m_gradient_type(GRADIENT_NONE), m_clamp_min(false), m_clamp_max(false)
    {

    }

    ~Gradient()
    {

    }

    static void parameters(const std::string& base, AtList* params, AtMetaDataStore*)
    {
        static const char* gradient_types[] = {"None", "Float", "RGB", nullptr};

        AiParameterEnum((base + "_gradient_type").c_str(), GRADIENT_NONE, gradient_types);
        AiParameterFlt((base + "_contrast").c_str(), 1.0f);
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
        AiParameterArray((base + "_float_ramp").c_str(), AiArray(1, 1, AI_TYPE_FLOAT, 0.0f));
        AiParameterArray((base + "_rgb_ramp").c_str(), AiArray(1, 1, AI_TYPE_RGB, AI_RGB_BLACK));
    }

    void update(const std::string& base, AtNode* node, AtParamValue*)
    {

        m_contrast = AiNodeGetFlt(node, (base + "_contrast").c_str());
        m_input_min = AiNodeGetFlt(node, (base + "_input_min").c_str());
        m_input_max = AiNodeGetFlt(node, (base + "_input_max").c_str());
        m_bias = AiNodeGetFlt(node, (base + "_bias").c_str());
        m_gain = AiNodeGetFlt(node, (base + "_gain").c_str());
        m_output_min = AiNodeGetFlt(node, (base + "_output_min").c_str());
        m_output_max = AiNodeGetFlt(node, (base + "_output_max").c_str());
        m_gamma = AiNodeGetFlt(node, (base + "_gamma").c_str());
        m_hue_shift = AiNodeGetFlt(node, (base + "_hue_shift").c_str());
        m_saturation = AiNodeGetFlt(node, (base + "_saturation").c_str());
        m_exposure = AiNodeGetFlt(node, (base + "_exposure").c_str());
        m_multiply = AiNodeGetFlt(node, (base + "_multiply").c_str());
        m_add = AiNodeGetFlt(node, (base + "_add").c_str());
        m_gradient_type = AiNodeGetInt(node, (base + "_gradient_type").c_str());
        m_clamp_min = AiNodeGetBool(node, (base + "_clamp_min").c_str());
        m_clamp_max = AiNodeGetBool(node, (base + "_clamp_max").c_str());

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

        arr = AiNodeGetArray(node, (base + "_float_ramp").c_str());
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

    AtRGB evaluate(const AtRGB& input) const
    {
        return input;
    }

    AtRGB evaluate(AtNode*, AtShaderGlobals* sg, const AtString& channel, int interpolation) const
    {
        AtRGB input = AI_RGB_BLACK;

        if (m_gradient_type == GRADIENT_FLOAT) // check if float gradient
        {
            float v = 0.0f;
            if (!AiVolumeSampleFlt(channel, interpolation, &v))
            {
                AiVolumeSampleRGB(channel, interpolation, &input);
                v = (input.r + input.g + input.b) / 3.0f;
            }
            input = v;
        }
        else
        {
            if (!AiVolumeSampleRGB(channel, interpolation, &input))
            {
                float v = 0.0f;
                AiVolumeSampleFlt(channel, interpolation, &v);
                input = v;
            }
        }

        return evaluate(input);
    }
};
