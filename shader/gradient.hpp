#pragma once

#include <ai.h>

#include <string>

class Gradient {
private:
    enum {
        GRADIENT_NONE,
        GRADIENT_FLOAT,
        GRADIENT_RGB
    };



public:
    Gradient()
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

    void update(const std::string&, AtNode*, AtParamValue*)
    {

    }

    AtRGB evaluate(const AtRGB& input) const
    {
        return input;
    }

    AtRGB evaluate(AtNode*, AtShaderGlobals* sg, const AtString& channel, int interpolation) const
    {
        AtRGB input = AI_RGB_BLACK;

        if (true) // check if float gradient
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
