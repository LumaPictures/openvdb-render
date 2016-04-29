#pragma once

#include <maya/MPxNode.h>
#include <maya/MString.h>

struct VDBGradientParams{
    VDBGradientParams(const char* _gradient_name);

    void create_params();
    void affect_output(MObject& out_object);
    void post_constructor(MObject tmo);

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
public:
    static void* creator();

    VDBSamplerNode();
    ~VDBSamplerNode();

    virtual MStatus compute(const MPlug& plug, MDataBlock& dataBlock);

    static MStatus initialize();

    virtual void postConstructor();

    static const MTypeId s_type_id;
    static const MString s_type_name;
    static const MString s_classification;

    static MObject s_channel;
    static MObject s_position_offset;
    static MObject s_interpolation;
    static VDBGradientParams s_gradient;
};
