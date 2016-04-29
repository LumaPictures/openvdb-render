#pragma once

#include <maya/MPxNode.h>
#include <maya/MString.h>
#include <maya/MTypeId.h>

struct VDBSimpleShaderParams{
    VDBSimpleShaderParams();

    void create_params(bool add_shared);
    void affect_output(MObject& out_object);

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
};

class VDBSimpleShaderNode : public MPxNode {
public:
    static void* creator();

    VDBSimpleShaderNode();
    ~VDBSimpleShaderNode();

    virtual MStatus compute(const MPlug& plug, MDataBlock& dataBlock);

    static MStatus initialize();

    static const MTypeId s_type_id;
    static const MString s_type_name;
    static const MString s_classification;

    static VDBSimpleShaderParams s_params;
};
