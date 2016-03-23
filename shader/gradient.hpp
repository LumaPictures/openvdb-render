#pragma once

#include <ai.h>

#include <string>

class Gradient {
private:
public:
    Gradient()
    {

    }

    ~Gradient()
    {

    }

    static void parameters(const std::string&, AtList*, AtMetaDataStore*)
    {

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
