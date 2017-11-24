#pragma once

#include <maya/MPxNode.h>
#include <maya/MString.h>
#include <maya/MTypeId.h>

#include "vdb_sampler.h"

struct VDBSimpleShaderParams {
    VDBSimpleShaderParams();

    void create_params();

    void affect_output(MObject& out_object);

    bool check_plug(MPlug& plug);

    MObject smoke;
    MObject smoke_channel;
    MObject smoke_intensity;
    MObject anisotropy;
    MObject opacity;
    MObject opacity_channel;
    MObject opacity_intensity;
    MObject opacity_shadow;
    MObject fire;
    MObject fire_channel;
    MObject fire_intensity;
    MObject position_offset;
    MObject interpolation;
    MObject compensate_scaling;

    VDBGradientParams smoke_gradient;
    VDBGradientParams opacity_gradient;
    VDBGradientParams fire_gradient;

};

class VDBSimpleShaderNode : public MPxNode {
private:
    VDBSimpleShaderNode() = default;
public:
    static void* creator();

    VDBSimpleShaderNode(const VDBSimpleShaderNode&) = delete;
    VDBSimpleShaderNode(VDBSimpleShaderNode&&) = delete;
    VDBSimpleShaderNode& operator=(const VDBSimpleShaderNode&) = delete;
    VDBSimpleShaderNode& operator=(VDBSimpleShaderNode&&) = delete;

    ~VDBSimpleShaderNode() override = default;

    MStatus compute(const MPlug& plug, MDataBlock& dataBlock) override;

    static MStatus initialize();

    static const MTypeId s_type_id;
    static const MString s_type_name;
    static const MString s_classification;

    static VDBSimpleShaderParams s_params;
};
