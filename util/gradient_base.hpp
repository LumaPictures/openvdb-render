#pragma once

#include <algorithm>
#include <vector>
#include <cmath>

template<typename Color>
class GradientBase {
protected:
    enum {
        CHANNEL_MODE_RAW = 0,
        CHANNEL_MODE_RGB,
        CHANNEL_MODE_FLOAT,
        CHANNEL_MODE_FLOAT_RAMP,
        CHANNEL_MODE_RGB_RAMP
    };

    std::vector<float> m_float_ramp;
    std::vector<Color> m_rgb_ramp;

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

    inline float& color_r(Color& color) const
    {
        return color.r;
    }

    inline float& color_g(Color& color) const
    {
        return color.g;
    }

    inline float& color_b(Color& color) const
    {
        return color.b;
    }

    inline const float& color_r(const Color& color) const
    {
        return color.r;
    }

    inline const float& color_g(const Color& color) const
    {
        return color.g;
    }

    inline const float& color_b(const Color& color) const
    {
        return color.b;
    }

    inline float bias(float value) const
    {
        return value / (((m_inv_bias - 2.0f) * (1.0f - value)) + 1.0f);
    }

    inline float gain(float value) const
    {
        if (value < 0.5f) {
            return bias(value * 2.0f) * 0.5f;
        } else {
            return bias(value * 2.0f - 1.0f) * 0.5f + 0.5f;
        }
    }

    inline float apply_float_range(float value) const
    {
        return (value - m_input_min) * m_inv_input_range;
    }

    inline float apply_float_gradient(float value) const
    {
        value = apply_float_range(value);
        if (value < 0.0001f) {
            return m_float_ramp.front();
        } else if (value > 0.9999f) {
            return m_float_ramp.back();
        } else {
            const size_t elem_count = m_float_ramp.size();
            const float sample_f = value * static_cast<float>(elem_count);
            const float sample_f_floor = floorf(sample_f);
            const size_t id = static_cast<size_t>(sample_f_floor);
            if (id >= elem_count - 1) {
                return m_float_ramp.back();
            }
            const float factor = sample_f - sample_f_floor;
            return (1.0f - factor) * m_float_ramp[id] + factor * m_float_ramp[id + 1];
        }
    }

    inline float apply_float_controls(float value) const
    {
        value = (value - m_contrast_pivot) * m_contrast + m_contrast_pivot;
        if (m_bias != 0.5f) {
            value = bias(value);
        }
        if (m_gain != 0.5f) {
            value = gain(value);
        }
        value = value * m_output_range + m_output_min;
        if (m_clamp_min) {
            value = std::max(value, m_output_min);
        }
        if (m_clamp_max) {
            value = std::min(value, m_output_max);
        }
        return value;
    }

    inline Color apply_rgb_gradient(float value) const
    {
        value = apply_float_range(value);
        if (value < 0.0001f) {
            return m_rgb_ramp.front();
        } else if (value > 0.9999f) {
            return m_rgb_ramp.back();
        } else {
            const size_t elem_count = m_rgb_ramp.size();
            const float sample_f = value * static_cast<float>(elem_count);
            const float sample_f_floor = floorf(sample_f);
            const size_t id = static_cast<size_t>(sample_f_floor);
            if (id >= elem_count - 1) {
                return m_rgb_ramp.back();
            }
            const float factor = sample_f - sample_f_floor;
            return (1.0f - factor) * m_rgb_ramp[id] + factor * m_rgb_ramp[id + 1];
        }
    }

    // don't worry, we are modifying the color, so copying is appropriate
    inline Color apply_rgb_controls(Color color) const
    {
        if (m_gamma != 1.0f) {
            color_r(color) = powf(color_r(color), 1.0f / m_gamma);
            color_g(color) = powf(color_g(color), 1.0f / m_gamma);
            color_b(color) = powf(color_b(color), 1.0f / m_gamma);
        }

        if (m_hue_shift != 0.0f || m_saturation != 1.0f) {
            color = convertFromRGB(color);

            color_r(color) += m_hue_shift;
            color_r(color) = color_r(color) - floorf(color_r(color)); // keep hue in [0, 1]

            color_g(color) *= m_saturation;
            color = convertToRGB(color);
        }

        if (m_contrast != 1.0f) {
            color = (color - make_color(m_contrast_pivot, m_contrast_pivot, m_contrast_pivot)) * m_contrast +
                    make_color(m_contrast_pivot, m_contrast_pivot, m_contrast_pivot);
        }

        if (m_exposure > 0.0001f) {
            color *= powf(2.0f, m_exposure);
        }

        if (m_multiply != 1.0f) {
            color *= m_multiply;
        }

        if (m_add != 0.0f) {
            color += make_color(m_add, m_add, m_add);
        }

        return color;
    }

    inline Color convertToRGB(const Color& color) const
    {
        const float hue6 = fmod(color_r(color), 1.0f) * 6.0f;
        float hue2 = hue6;

        if (hue6 > 4.0f) {
            hue2 -= 4.0f;
        } else if (hue6 > 2.0f) {
            hue2 -= 2.0f;
        }

        const float sat = std::max(std::min(color_g(color), 1.0f), 0.0f);
        const float chroma = (1.0f - fabsf(2.0f * color_b(color) - 1.0f)) * sat;
        const float component = chroma * (1.0f - fabsf(hue2 - 1.0f));

        Color rgb = make_color(0.0f, 0.0f, 0.0f);
        if (hue6 < 1) {
            rgb = make_color(chroma, component, 0.0f);
        } else if (hue6 < 2) {
            rgb = make_color(component, chroma, 0.0f);
        } else if (hue6 < 3) {
            rgb = make_color(0.0f, chroma, component);
        } else if (hue6 < 4) {
            rgb = make_color(0.0f, component, chroma);
        } else if (hue6 < 5) {
            rgb = make_color(component, 0.0f, chroma);
        } else {
            rgb = make_color(chroma, 0.0f, component);
        }

        const float ca = color_b(color) - chroma * 0.5f;
        rgb += make_color(ca, ca, ca);
        return rgb;
    }

    inline float color_max(const Color& color) const
    {
        return std::max(color_r(color), std::max(color_g(color), color_b(color)));
    }

    inline float color_min(const Color& color) const
    {
        return std::min(color_r(color), std::min(color_g(color), color_b(color)));
    }

    inline Color convertFromRGB(const Color& color) const
    {
        const float cmax = color_max(color);
        const float cmin = color_min(color);
        const float chroma = cmax - cmin;

        float hue = 0.0f;
        if (chroma == 0.0f) {
            hue = 0.0f;
        } else if (cmax == color_r(color)) {
            hue = (color_g(color) - color_b(color)) / chroma;
        } else if (cmax == color_g(color)) {
            hue = (color_b(color) - color_r(color)) / chroma + 2.0f;
        } else {
            hue = (color_r(color) - color_g(color)) / chroma + 4.0f;
        }

        hue *= 1.0f / 6.0f;
        if (hue < 0.0f) {
            hue += 1.0f;
        }

        const float lightness = (cmax + cmin) * 0.5f;
        const float saturation = chroma == 0.0f ? 0.0f : chroma / (1.0f - fabsf(2.0f * lightness - 1.0f));
        return make_color(hue, saturation, lightness);
    }

    inline Color make_color(float r, float g, float b) const
    {
        return Color(r, g, b);
    }

    inline void update()
    {
        if (m_float_ramp.size() < 2) {
            m_float_ramp.resize(2);
            m_float_ramp[0] = 0.0f;
            m_float_ramp[1] = 1.0f;
        }

        if (m_rgb_ramp.size() < 2) {
            m_rgb_ramp.resize(2);
            m_rgb_ramp[0] = make_color(0.0f, 0.0f, 0.0f);
            m_rgb_ramp[1] = make_color(1.0f, 1.0f, 1.0f);
        }

        if (m_bias < 0.0001f) {
            m_bias = 0.5f;
        }

        m_inv_input_range = 1.0f / (m_input_max - m_input_min);
        m_output_range = m_output_max - m_output_min;
        m_inv_bias = 1.0f / m_bias;
        m_inv_gain = 1.0f / m_gain;
        m_inv_one_minus_gain = 1.0f / (1.0f - m_gain);
    }

public:
    GradientBase() : m_contrast(1.0f), m_contrast_pivot(0.5f), m_input_min(0.0f), m_input_max(1.0f),
                     m_bias(0.5f), m_gain(0.5f), m_output_min(0.0f), m_output_max(1.0f),
                     m_gamma(1.0f), m_hue_shift(0.0f), m_saturation(1.0f), m_exposure(0.0f),
                     m_multiply(1.0f), m_add(0.0f), m_inv_input_range(1.0f), m_output_range(1.0f),
                     m_inv_bias(2.0f), m_inv_gain(2.0f), m_inv_one_minus_gain(2.0f),
                     m_channel_mode(CHANNEL_MODE_RAW), m_clamp_min(false), m_clamp_max(false)
    {

    }

    void clear()
    {
        m_float_ramp.resize(2);
        m_float_ramp[0] = 0.0f;
        m_float_ramp[1] = 1.0f;

        m_rgb_ramp.resize(2);
        m_rgb_ramp[0] = make_color(0.0f, 0.0f, 0.0f);
        m_rgb_ramp[1] = make_color(1.0f, 1.0f, 1.0f);

        m_channel_mode = CHANNEL_MODE_RAW;
    }

    ~GradientBase()
    {

    }

    inline Color evaluate(float v) const
    {
        if (m_channel_mode == CHANNEL_MODE_RAW) {
            return make_color(v, v, v);
        } else if (m_channel_mode == CHANNEL_MODE_FLOAT) {
            return make_color(1.0f, 1.0f, 1.0f) * apply_float_controls(apply_float_range(v));
        } else if (m_channel_mode == CHANNEL_MODE_RGB) {
            return apply_rgb_controls(make_color(v, v, v));
        } else if (m_channel_mode == CHANNEL_MODE_FLOAT_RAMP) {
            return make_color(1.0f, 1.0f, 1.0f) * apply_float_controls(apply_float_gradient(v));
        } else {
            return apply_rgb_controls(apply_rgb_gradient(v));
        }
    }

    inline Color evaluate(const Color& v) const
    {
        if (m_channel_mode == CHANNEL_MODE_RAW) {
            return v;
        } else if (m_channel_mode == CHANNEL_MODE_FLOAT) {
            return make_color(1.0f, 1.0f, 1.0f) *
                   apply_float_controls(apply_float_range((color_r(v) + color_g(v) + color_b(v)) / 3.0f));
        } else if (m_channel_mode == CHANNEL_MODE_RGB) {
            return apply_rgb_controls(v);
        } else if (m_channel_mode == CHANNEL_MODE_FLOAT_RAMP) {
            return make_color(1.0f, 1.0f, 1.0f) *
                   apply_float_controls(apply_float_gradient((color_r(v) + color_g(v) + color_b(v)) / 3.0f));
        } else {
            return apply_rgb_controls(apply_rgb_gradient((color_r(v) + color_g(v) + color_b(v)) / 3.0f));
        }
    }

    inline bool is_different(const GradientBase<Color>& other) const
    {
        const size_t float_ramp_size = m_float_ramp.size();
        if (float_ramp_size != other.m_float_ramp.size()) {
            return true;
        }
        if (float_ramp_size > 0) {
            if (memcmp(m_float_ramp.data(), other.m_float_ramp.data(), sizeof(float) * float_ramp_size) != 0) {
                return true;
            }
        }

        const size_t rgb_ramp_size = m_rgb_ramp.size();
        if (rgb_ramp_size != other.m_rgb_ramp.size()) {
            return true;
        }
        if (rgb_ramp_size > 0) {
            if (memcmp(m_rgb_ramp.data(), other.m_rgb_ramp.data(), sizeof(Color) * rgb_ramp_size) != 0) {
                return true;
            }
        }

        return m_contrast != other.m_contrast ||
               m_contrast_pivot != other.m_contrast_pivot ||
               m_input_min != other.m_input_min ||
               m_input_max != other.m_input_max ||
               m_bias != other.m_bias ||
               m_gain != other.m_gain ||
               m_gain != other.m_gain ||
               m_output_min != other.m_output_min ||
               m_output_max != other.m_output_max ||
               m_gamma != other.m_gamma ||
               m_hue_shift != other.m_hue_shift ||
               m_saturation != other.m_saturation ||
               m_exposure != other.m_exposure ||
               m_multiply != other.m_multiply ||
               m_add != other.m_add ||
               m_inv_input_range != other.m_inv_input_range ||
               m_output_range != other.m_output_range ||
               m_inv_bias != other.m_inv_bias ||
               m_inv_gain != other.m_inv_gain ||
               m_inv_one_minus_gain != other.m_inv_one_minus_gain ||
               m_channel_mode != other.m_channel_mode ||
               m_clamp_min != other.m_clamp_min ||
               m_clamp_max != other.m_clamp_max;
    }
};
