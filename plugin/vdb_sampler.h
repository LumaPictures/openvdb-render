#pragma once

#include <maya/MPxNode.h>
#include <maya/MString.h>

struct VDBGradientParams {
    explicit VDBGradientParams(const char* _gradient_name);

    void create_params();

    void affect_output(MObject& out_object);

    void post_constructor(MObject tmo);

    bool check_plug(const MPlug& plug);

    MString gradient_name;
    MObject mode;

    MObject contrast;
    MObject contrast_pivot;

    MObject input_min;
    MObject input_max;
    MObject bias;
    MObject gain;
    MObject output_min;
    MObject output_max;
    MObject clamp_min;
    MObject clamp_max;

    MObject gamma;
    MObject hue_shift;
    MObject saturation;
    MObject exposure;
    MObject multiply;
    MObject add;

    MObject float_ramp;
    MObject rgb_ramp;
};

class VDBSamplerNode : public MPxNode {
private:
    VDBSamplerNode() = default;
public:
    static void* creator();

    VDBSamplerNode(const VDBSamplerNode&) = delete;
    VDBSamplerNode(VDBSamplerNode&&) = delete;
    VDBSamplerNode& operator=(const VDBSamplerNode&) = delete;
    VDBSamplerNode& operator=(VDBSamplerNode&&) = delete;

    ~VDBSamplerNode() override = default;

    MStatus compute(const MPlug&, MDataBlock&) override;

    static MStatus initialize();

    void postConstructor() override;

    static const MTypeId s_type_id;
    static const MString s_type_name;
    static const MString s_classification;

    static MObject s_channel;
    static MObject s_position_offset;
    static MObject s_interpolation;
    static VDBGradientParams s_gradient;
};
