#pragma once

#include <maya/MPxNode.h>
#include <maya/MString.h>
#include <maya/MTypeId.h>

#include "vdb_sampler.h"

struct VDBShaderParams{
    VDBShaderParams();

    void create_params();
    void affect_output(MObject& out_object);

    MObject scattering_source;
    MObject scattering;
    MObject scattering_channel;
    MObject scattering_color;
    MObject scattering_intensity;
    MObject anisotropy;
    MObject attenuation_source;
    MObject attenuation;
    MObject attenuation_channel;
    MObject attenuation_color;
    MObject attenuation_intensity;
    MObject attenuation_mode;
    MObject emission_source;
    MObject emission;
    MObject emission_channel;
    MObject emission_color;
    MObject emission_intensity;
    MObject position_offset;
    MObject interpolation;
    MObject compensate_scaling;

    VDBGradientParams scattering_gradient;
    VDBGradientParams attenuation_gradient;
    VDBGradientParams emission_gradient;
};

class VDBShaderNode : public MPxNode {
public:
    static void* creator();

    VDBShaderNode();
    ~VDBShaderNode();

    virtual MStatus compute(const MPlug& plug, MDataBlock& dataBlock);

    static MStatus initialize();

    static const MTypeId s_type_id;
    static const MString s_type_name;
    static const MString s_classification;

    static VDBShaderParams s_shader_params;
};
