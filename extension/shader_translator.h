#pragma once

#include <translators/shader/ShaderTranslator.h>

// the process parameter functions are protected member functions...
// so need to do some tricks to not to write this several times

template<typename translator_class>
class VDBShaderParamsTranslator : public translator_class{
public:
    inline void ExportParams(AtNode* shader)
    {
        const unsigned int num_ramp_samples = 256;

        this->ProcessParameter(shader, "scattering_source", AI_TYPE_INT, "scatteringSource");
        this->ProcessParameter(shader, "scattering", AI_TYPE_RGB, "scattering");
        this->ProcessParameter(shader, "scattering_channel", AI_TYPE_STRING, "scatteringChannel");
        this->ProcessParameter(shader, "scattering_color", AI_TYPE_RGB, "scatteringColor");
        this->ProcessParameter(shader, "scattering_intensity", AI_TYPE_FLOAT, "scatteringIntensity");
        this->ProcessParameter(shader, "anisotropy", AI_TYPE_FLOAT, "anisotropy");

        this->ProcessParameter(shader, "attenuation_source", AI_TYPE_INT, "attenuationSource");
        this->ProcessParameter(shader, "attenuation", AI_TYPE_RGB, "attenuation");
        this->ProcessParameter(shader, "attenuation_channel", AI_TYPE_STRING, "attenuationChannel");
        this->ProcessParameter(shader, "attenuation_color", AI_TYPE_RGB, "attenuationColor");
        this->ProcessParameter(shader, "attenuation_intensity", AI_TYPE_FLOAT, "attenuationIntensity");
        this->ProcessParameter(shader, "attenuation_mode", AI_TYPE_INT, "attenuationMode");

        this->ProcessParameter(shader, "emission_source", AI_TYPE_INT, "emissionSource");
        this->ProcessParameter(shader, "emission", AI_TYPE_RGB, "emission");
        this->ProcessParameter(shader, "emission_channel", AI_TYPE_STRING, "emissionChannel");
        this->ProcessParameter(shader, "emission_color", AI_TYPE_RGB, "emissionColor");
        this->ProcessParameter(shader, "emission_intensity", AI_TYPE_FLOAT, "emissionIntensity");

        this->ProcessParameter(shader, "position_offset", AI_TYPE_VECTOR, "positionOffset");
        this->ProcessParameter(shader, "interpolation", AI_TYPE_INT, "interpolation");
        this->ProcessParameter(shader, "compensate_scaling", AI_TYPE_BOOLEAN, "compensateScaling");

        std::array<std::string, 3> gradient_names = {"scattering", "attenuation", "emission"};

        for (const auto& gradient : gradient_names)
        {
            this->ProcessParameter(shader, (gradient + "_channel_mode").c_str(), AI_TYPE_INT, (gradient + "ChannelMode").c_str());
            this->ProcessParameter(shader, (gradient + "_contrast").c_str(), AI_TYPE_FLOAT, (gradient + "Contrast").c_str());
            this->ProcessParameter(shader, (gradient + "_contrast_pivot").c_str(), AI_TYPE_FLOAT, (gradient + "ContrastPivot").c_str());
            this->ProcessParameter(shader, (gradient + "_input_min").c_str(), AI_TYPE_FLOAT, (gradient + "InputMin").c_str());
            this->ProcessParameter(shader, (gradient + "_input_max").c_str(), AI_TYPE_FLOAT, (gradient + "InputMax").c_str());
            this->ProcessParameter(shader, (gradient + "_bias").c_str(), AI_TYPE_FLOAT, (gradient + "Bias").c_str());
            this->ProcessParameter(shader, (gradient + "_gain").c_str(), AI_TYPE_FLOAT, (gradient + "Gain").c_str());
            this->ProcessParameter(shader, (gradient + "_output_min").c_str(), AI_TYPE_FLOAT, (gradient + "OutputMin").c_str());
            this->ProcessParameter(shader, (gradient + "_output_max").c_str(), AI_TYPE_FLOAT, (gradient + "OutputMax").c_str());
            this->ProcessParameter(shader, (gradient + "_clamp_min").c_str(), AI_TYPE_BOOLEAN, (gradient + "ClampMin").c_str());
            this->ProcessParameter(shader, (gradient + "_clamp_max").c_str(), AI_TYPE_BOOLEAN, (gradient + "ClampMax").c_str());
            this->ProcessParameter(shader, (gradient + "_gamma").c_str(), AI_TYPE_FLOAT, (gradient + "Gamma").c_str());
            this->ProcessParameter(shader, (gradient + "_hue_shift").c_str(), AI_TYPE_FLOAT, (gradient + "HueShift").c_str());
            this->ProcessParameter(shader, (gradient + "_saturation").c_str(), AI_TYPE_FLOAT, (gradient + "Saturation").c_str());
            this->ProcessParameter(shader, (gradient + "_exposure").c_str(), AI_TYPE_FLOAT, (gradient + "Exposure").c_str());
            this->ProcessParameter(shader, (gradient + "_multiply").c_str(), AI_TYPE_FLOAT, (gradient + "Multiply").c_str());
            this->ProcessParameter(shader, (gradient + "_add").c_str(), AI_TYPE_FLOAT, (gradient + "Add").c_str());

            MStatus status = MS::kSuccess;
            MPlug plug = this->FindMayaPlug((gradient + "FloatRamp").c_str(), &status);
            if (status && !plug.isNull())
            {
                MRampAttribute ramp_attr(plug);
                MFloatArray samples;
                ramp_attr.sampleValueRamp(num_ramp_samples, samples, &status);
                if (status)
                {
                    AtArray* arr = AiArrayConvert(num_ramp_samples, 1, AI_TYPE_FLOAT, &samples[0]);
                    AiNodeSetArray(shader, (gradient + "_float_ramp").c_str(), arr);
                }
            }

            plug = this->FindMayaPlug((gradient + "RgbRamp").c_str(), &status);
            if (status && !plug.isNull())
            {
                MRampAttribute ramp_attr(plug);
                MColorArray samples;
                ramp_attr.sampleColorRamp(num_ramp_samples, samples, &status);
                if (status)
                {
                    AtArray* arr = AiArrayAllocate(num_ramp_samples, 1, AI_TYPE_RGB);
                    for (unsigned int i = 0; i < num_ramp_samples; ++i)
                    {
                        const MColor& sample = samples[i];
                        AiArraySetRGB(arr, i, AiColorCreate(sample.r, sample.g, sample.b));
                    }
                    AiNodeSetArray(shader, (gradient + "_rgb_ramp").c_str(), arr);
                }
            }
        }
    }
};

class VDBShaderTranslator : public VDBShaderParamsTranslator<CShaderTranslator>{
public:
    static void* creator();

    virtual AtNode* CreateArnoldNodes();
    virtual void Export(AtNode* volume);
};
