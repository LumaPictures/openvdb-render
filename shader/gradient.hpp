#pragma once

#include <ai.h>

#include <string>
#include <vector>
#include <iostream>

class Gradient {
private:
    enum {
        CHANNEL_MODE_RAW = 0,
        CHANNEL_MODE_RGB,
        CHANNEL_MODE_FLOAT,
        CHANNEL_MODE_FLOAT_TO_FLOAT,
        CHANNEL_MODE_FLOAT_TO_RGB
    };

    std::vector<float> m_float_ramp;
    std::vector<AtRGB> m_rgb_ramp;

    float m_contrast;
    float m_contrast_pivot;
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
    float m_inv_input_range;
    float m_output_range;
    float m_inv_bias;
    float m_inv_gain;
    float m_inv_one_minus_gain;
    int m_channel_mode;
    bool m_clamp_min;
    bool m_clamp_max;

    inline float bias(float value) const
    {
        return value / (((m_inv_bias - 2.0f) * (1.0f - value)) + 1.0f);
    }

    inline float gain(float value) const
    {
        if (value < 0.5f)
            return bias(value * 2.0f) * 0.5f;
        else
            return bias(value * 2.0f - 1.0f) * 0.5f + 0.5f;
    }

    inline float apply_float_range(float value) const
    {
        return (value - m_input_min) * m_inv_input_range;
    }

    inline float apply_float_gradient(float value) const
    {
        value = apply_float_range(value);
        if (value < AI_EPSILON)
            return m_float_ramp.front();
        else if (value > (1.0f - AI_EPSILON))
            return m_float_ramp.back();
        else
        {
            const size_t elem_count = m_float_ramp.size();
            const float sample_f = value * static_cast<float>(elem_count);
            const float sample_f_floor = floorf(sample_f);
            const size_t id = static_cast<size_t>(sample_f_floor);
            if (id >= elem_count - 1)
                return m_float_ramp.back();
            const float factor = sample_f - sample_f_floor;
            return LERP(factor, m_float_ramp[id], m_float_ramp[id + 1]);
        }
    }

    inline float apply_float_controls(float value) const
    {
        value = (value - m_contrast_pivot) * m_contrast + m_contrast_pivot;
        if (m_bias != 0.5f)
            value = bias(value);
        if (m_gain != 0.5f)
            value = gain(value);
        value = value * m_output_range + m_output_min;
        if (m_clamp_min)
            value = std::max(value, m_output_min);
        if (m_clamp_max)
            value = std::min(value, m_output_max);
        return value;
    }

    inline AtRGB apply_rgb_gradient(float value) const
    {
        value = apply_float_range(value);
        if (value < AI_EPSILON)
            return m_rgb_ramp.front();
        else if (value > 1.0f - AI_EPSILON)
            return m_rgb_ramp.back();
        else
        {
            const size_t elem_count = m_rgb_ramp.size();
            const float sample_f = value * static_cast<float>(elem_count);
            const float sample_f_floor = floorf(sample_f);
            const size_t id = static_cast<size_t>(sample_f_floor);
            if (id >= elem_count - 1)
                return m_rgb_ramp.back();
            const float factor = sample_f - sample_f_floor;
            return LERP(factor, m_rgb_ramp[id], m_rgb_ramp[id + 1]);
        }
    }

    // don't worry, we are modifying the color, so copying is appropriate
    inline AtRGB apply_rgb_controls(AtRGB color) const
    {
        if (m_gamma != 1.0f)
            AiColorGamma(&color, m_gamma);

        if (m_hue_shift != 0.0f || m_saturation != 1.0f)
        {
            color = convertFromRGB(color);

            color.r += m_hue_shift;
            color.r = color.r - floorf(color.r); // keep hue in [0, 1]

            color.g *= m_saturation;
            color = convertToRGB(color);
        }

        if (m_contrast != 1.0f)
            color = (color - m_contrast_pivot) * m_contrast + m_contrast_pivot;

        if (m_exposure > AI_EPSILON)
            color *= powf(2.0f, m_exposure);

        if (m_multiply != 1.0f)
            color *= m_multiply;

        if (m_add != 0.0f)
            color += m_add;

        return color;
    }

    static AtRGB convertToRGB(const AtColor& color)
    {
        const float hue6 = fmod(color.r, 1.0f) * 6.0f;
        float hue2 = hue6;

        if (hue6 > 4.0f) hue2 -= 4.0f;
        else if (hue6 > 2.0f) hue2 -= 2.0f;

        const float sat = CLAMP(color.g, 0.0f, 1.0f);
        const float chroma = (1.0f - fabsf(2.0f * color.b - 1.0f)) * sat;
        const float component = chroma * (1.0f - fabsf(hue2 - 1.0f));

        AtColor rgb = AI_RGB_BLACK;
        if (hue6 < 1)
            AiColorCreate(rgb, chroma, component, 0.0f);
        else if (hue6 < 2)
            AiColorCreate(rgb, component, chroma, 0.0f);
        else if (hue6 < 3)
            AiColorCreate(rgb, 0.0f, chroma, component);
        else if (hue6 < 4)
            AiColorCreate(rgb, 0.0f, component, chroma);
        else if (hue6 < 5)
            AiColorCreate(rgb, component, 0.0f, chroma);
        else
            AiColorCreate(rgb, chroma, 0.0f, component);

        rgb += color.b - chroma * 0.5f;
        return rgb;
    }

    static AtRGB convertFromRGB(const AtColor& color)
    {
        const float cmax = AiColorMaxRGB(color);
        const float cmin = std::min(std::min(color.r, color.g), color.b);
        const float chroma = cmax - cmin;

        float hue = 0.0f;
        if (chroma == 0.0f)
            hue = 0.0f;
        else if (cmax == color.r)
            hue = (color.g - color.b) / chroma;
        else if (cmax == color.g)
            hue = (color.b - color.r) / chroma + 2.0f;
        else
            hue = (color.r - color.g) / chroma + 4.0f;

        hue *= 1.0f / 6.0f;
        if (hue < 0.0f)
            hue += 1.0f;

        const float lightness = (cmax + cmin) * 0.5f;
        const float saturation = chroma == 0.0f ? 0.0f : chroma / (1.0f - fabsf(2.0f * lightness - 1.0f));
        return AiColor(hue, saturation, lightness);
    }

public:
    Gradient() : m_contrast(1.0f), m_contrast_pivot(0.5f), m_input_min(0.0f), m_input_max(1.0f),
                 m_bias(0.5f), m_gain(0.5f), m_output_min(0.0f), m_output_max(1.0f),
                 m_gamma(1.0f), m_hue_shift(0.0f), m_saturation(1.0f), m_exposure(0.0f),
                 m_multiply(1.0f), m_add(0.0f), m_inv_input_range(1.0f), m_output_range(1.0f),
                 m_inv_bias(2.0f), m_inv_gain(2.0f), m_inv_one_minus_gain(2.0f),
                 m_channel_mode(CHANNEL_MODE_RAW), m_clamp_min(false), m_clamp_max(false)
    {

    }

    ~Gradient()
    {

    }

    static void parameters(const std::string& base, AtList* params, AtMetaDataStore*)
    {
        static const char* gradient_types[] = {"Raw", "Float", "RGB", "Float to Float", "Float to RGB", nullptr};

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

        if (m_float_ramp.size() < 2)
        {
            m_float_ramp.resize(2);
            m_float_ramp[0] = 0.0f;
            m_float_ramp[1] = 1.0f;
        }

        arr = AiNodeGetArray(node, (base + "_rgb_ramp").c_str());
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

        if (m_rgb_ramp.size() < 2)
        {
            m_rgb_ramp.resize(2);
            m_rgb_ramp[0] = AI_RGB_BLACK;
            m_rgb_ramp[1] = AI_RGB_WHITE;
        }

        m_inv_input_range = 1.0f / (m_input_max - m_input_min);
        m_output_range = m_output_max - m_output_min;
        m_inv_bias = 1.0f / m_bias;
        m_inv_gain = 1.0f / m_gain;
        m_inv_one_minus_gain = 1.0f / (1.0f - m_gain);
    }

    inline AtRGB evaluate(AtShaderGlobals* sg, const AtString& channel, int interpolation) const
    {
        if (m_channel_mode == CHANNEL_MODE_RAW)
        {
            AtRGB input = AI_RGB_BLACK;
            if (!AiVolumeSampleRGB(channel, interpolation, &input))
            {
                AiVolumeSampleFlt(channel, interpolation, &input.r);
                input.g = input.b = input.r;
            }
            return input;
        }
        else if (m_channel_mode == CHANNEL_MODE_FLOAT)
        {
            float v = 0.0f;
            if (!AiVolumeSampleFlt(channel, interpolation, &v))
            {
                AtRGB input = AI_RGB_BLACK;
                AiVolumeSampleRGB(channel, interpolation, &input);
                v = (input.r + input.g + input.b) / 3.0f;
            }
            return AI_RGB_WHITE * apply_float_controls(apply_float_range(v));
        }
        else if (m_channel_mode == CHANNEL_MODE_RGB)
        {
            AtRGB input = AI_RGB_BLACK;
            if (!AiVolumeSampleRGB(channel, interpolation, &input))
            {
                AiVolumeSampleFlt(channel, interpolation, &input.r);
                input.g = input.b = input.r;
            }
            return apply_rgb_controls(input);
        }
        else if (m_channel_mode == CHANNEL_MODE_FLOAT_TO_FLOAT)
        {
            float v = 0.0f;
            if (!AiVolumeSampleFlt(channel, interpolation, &v))
            {
                AtRGB input = AI_RGB_BLACK;
                AiVolumeSampleRGB(channel, interpolation, &input);
                v = (input.r + input.g + input.b) / 3.0f;
            }
            return AI_RGB_WHITE * apply_float_controls(apply_float_gradient(v));
        }
        else
        {
            float v = 0.0f;
            if (!AiVolumeSampleFlt(channel, interpolation, &v))
            {
                AtRGB input = AI_RGB_BLACK;
                AiVolumeSampleRGB(channel, interpolation, &input);
                v = (input.r + input.g + input.b) / 3.0f;
            }
            return apply_rgb_controls(apply_rgb_gradient(v));
        }
    }
};
