#pragma once

#include "../util/gradient_base.hpp"
#include "vdb_sampler.h"

#include <maya/MFloatVector.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MRampAttribute.h>

class Gradient : public GradientBase<MFloatVector> {
public:
    Gradient() : GradientBase<MFloatVector>()
    {
    }

    ~Gradient()
    {
    }

    inline void update(const VDBGradientParams& params, const MObject& tmo)
    {
        m_channel_mode = MPlug(tmo, params.mode).asShort();

        if (m_channel_mode > CHANNEL_MODE_RAW) {
            m_contrast = MPlug(tmo, params.contrast).asFloat();
            m_contrast_pivot = MPlug(tmo, params.contrast_pivot).asFloat();
            m_input_min = MPlug(tmo, params.input_min).asFloat();
            m_input_max = MPlug(tmo, params.input_max).asFloat();
            m_bias = MPlug(tmo, params.bias).asFloat();

            m_gain = MPlug(tmo, params.gain).asFloat();
            m_output_min = MPlug(tmo, params.output_min).asFloat();
            m_output_max = MPlug(tmo, params.output_max).asFloat();
            m_gamma = MPlug(tmo, params.gamma).asFloat();
            m_hue_shift = MPlug(tmo, params.hue_shift).asFloat();
            m_saturation = MPlug(tmo, params.saturation).asFloat();
            m_exposure = MPlug(tmo, params.exposure).asFloat();
            m_multiply = MPlug(tmo, params.multiply).asFloat();
            m_add = MPlug(tmo, params.add).asFloat();

            m_clamp_min = MPlug(tmo, params.clamp_min).asBool();
            m_clamp_max = MPlug(tmo, params.clamp_max).asBool();

            static constexpr unsigned int RAMP_SAMPLE_COUNT = 128;

            if (m_channel_mode == CHANNEL_MODE_FLOAT_RAMP) {
                MStatus status = MS::kSuccess;
                MRampAttribute float_ramp(tmo, params.float_ramp);
                MFloatArray float_array;
                float_ramp.sampleValueRamp(RAMP_SAMPLE_COUNT, float_array, &status);
                if (status) {
                    m_float_ramp.resize(RAMP_SAMPLE_COUNT);
                    for (unsigned int i = 0; i < RAMP_SAMPLE_COUNT; ++i) {
                        m_float_ramp[i] = float_array[i];
                    }
                }
            } else if (m_channel_mode == CHANNEL_MODE_RGB_RAMP) {
                MStatus status = MS::kSuccess;
                MRampAttribute color_ramp(tmo, params.rgb_ramp);
                MColorArray color_array;
                color_ramp.sampleColorRamp(RAMP_SAMPLE_COUNT, color_array, &status);
                if (status) {
                    m_rgb_ramp.resize(RAMP_SAMPLE_COUNT);
                    for (unsigned int i = 0; i < RAMP_SAMPLE_COUNT; ++i) {
                        MColor color = color_array[i];
                        m_rgb_ramp[i] = MFloatVector(color.r, color.g, color.b);
                    }
                }
            }

            GradientBase<MFloatVector>::update();
        }
    }
};

template<>
inline float& GradientBase<MFloatVector>::color_r(MFloatVector& color) const
{
    return color.x;
}

template<>
inline float& GradientBase<MFloatVector>::color_g(MFloatVector& color) const
{
    return color.y;
}

template<>
inline float& GradientBase<MFloatVector>::color_b(MFloatVector& color) const
{
    return color.z;
}

template<>
inline const float& GradientBase<MFloatVector>::color_r(const MFloatVector& color) const
{
    return color.x;
}

template<>
inline const float& GradientBase<MFloatVector>::color_g(const MFloatVector& color) const
{
    return color.y;
}

template<>
inline const float& GradientBase<MFloatVector>::color_b(const MFloatVector& color) const
{
    return color.z;
}
