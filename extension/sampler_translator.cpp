#include "sampler_translator.h"

namespace {
    const unsigned int num_ramp_samples = 256;
}

void* SamplerTranslator::creator()
{
    return new SamplerTranslator();
}

AtNode* SamplerTranslator::CreateArnoldNodes()
{
    return AddArnoldNode("openvdb_sampler");
}

void SamplerTranslator::Export(AtNode* shader)
{
    ProcessParameter(shader, "channel", AI_TYPE_STRING, "channel");
    ProcessParameter(shader, "position_offset", AI_TYPE_VECTOR, "positionOffset");
    ProcessParameter(shader, "interpolation", AI_TYPE_INT, "interpolation");
    ProcessParameter(shader, "base_channel_mode", AI_TYPE_INT, "baseChannelMode");
    ProcessParameter(shader, "base_contrast", AI_TYPE_FLOAT, "baseContrast");
    ProcessParameter(shader, "base_contrast_pivot", AI_TYPE_FLOAT, "baseContrastPivot");
    ProcessParameter(shader, "base_input_min", AI_TYPE_FLOAT, "baseInputMin");
    ProcessParameter(shader, "base_input_max", AI_TYPE_FLOAT, "baseInputMax");
    ProcessParameter(shader, "base_bias", AI_TYPE_FLOAT, "baseBias");
    ProcessParameter(shader, "base_gain", AI_TYPE_FLOAT, "baseGain");
    ProcessParameter(shader, "base_output_min", AI_TYPE_FLOAT, "baseOutputMin");
    ProcessParameter(shader, "base_output_max", AI_TYPE_FLOAT, "baseOutputMax");
    ProcessParameter(shader, "base_clamp_min", AI_TYPE_BOOLEAN, "baseClampMin");
    ProcessParameter(shader, "base_clamp_max", AI_TYPE_BOOLEAN, "baseClampMax");
    ProcessParameter(shader, "base_gamma", AI_TYPE_FLOAT, "baseGamma");
    ProcessParameter(shader, "base_hue_shift", AI_TYPE_FLOAT, "baseHueShift");
    ProcessParameter(shader, "base_saturation", AI_TYPE_FLOAT, "baseSaturation");
    ProcessParameter(shader, "base_exposure", AI_TYPE_FLOAT, "baseExposure");
    ProcessParameter(shader, "base_multiply", AI_TYPE_FLOAT, "baseMultiply");
    ProcessParameter(shader, "base_add", AI_TYPE_FLOAT, "baseAdd");

    MStatus status = MS::kSuccess;
    MPlug plug = FindMayaPlug("baseFloatRamp", &status);
    if (status && !plug.isNull()) {
        MRampAttribute ramp_attr(plug);
        MFloatArray samples;
        ramp_attr.sampleValueRamp(num_ramp_samples, samples, &status);
        if (status) {
            AtArray* arr = AiArrayConvert(num_ramp_samples, 1, AI_TYPE_FLOAT, &samples[0]);
            AiNodeSetArray(shader, "base_float_ramp", arr);
        }
    }

    plug = FindMayaPlug("baseRgbRamp", &status);
    if (status && !plug.isNull()) {
        MRampAttribute ramp_attr(plug);
        MColorArray samples;
        ramp_attr.sampleColorRamp(num_ramp_samples, samples, &status);
        if (status) {
            AtArray* arr = AiArrayAllocate(num_ramp_samples, 1, AI_TYPE_RGB);
            for (unsigned int i = 0; i < num_ramp_samples; ++i) {
                const MColor& sample = samples[i];
                AiArraySetRGB(arr, i, AiColorCreate(sample.r, sample.g, sample.b));
            }
            AiNodeSetArray(shader, "base_rgb_ramp", arr);
        }
    }
}
