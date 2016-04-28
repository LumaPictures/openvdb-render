#include "vdb_simple_shader.h"

#include <lumaNodeId.h>

#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnStringData.h>


VDBSimpleShaderParams::VDBSimpleShaderParams() : position_offset(MObject::kNullObj), interpolation(MObject::kNullObj), compensate_scaling(MObject::kNullObj)
{

}

void VDBSimpleShaderParams::create_params(bool add_shared)
{
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    MFnEnumAttribute eAttr;
    MFnStringData sData;

    smoke = nAttr.createColor("smoke", "smoke");
    nAttr.setDefault(1.0, 1.0, 1.0);
    MPxNode::addAttribute(smoke);

    smoke_channel = tAttr.create("smokeChannel", "smoke_channel", MFnData::kString);
    tAttr.setDefault(sData.create("density"));
    MPxNode::addAttribute(smoke_channel);

    smoke_intensity = nAttr.create("smokeIntensity", "smoke_intensity", MFnNumericData::kFloat);
    nAttr.setDefault(1.0f);
    nAttr.setMin(0.0f);
    nAttr.setSoftMax(1.0f);
    nAttr.setChannelBox(true);
    MPxNode::addAttribute(smoke_intensity);

    opacity = nAttr.createColor("opacity", "opacity");
    nAttr.setDefault(1.0, 1.0, 1.0);
    MPxNode::addAttribute(opacity);

    opacity_channel = tAttr.create("opacityChannel", "opacity_channel", MFnData::kString);
    MPxNode::addAttribute(opacity_channel);

    opacity_intensity = nAttr.create("opacityIntensity", "opacity_intensity", MFnNumericData::kFloat);
    nAttr.setDefault(1.0f);
    nAttr.setMin(0.0f);
    nAttr.setSoftMax(1.0f);
    nAttr.setChannelBox(true);
    MPxNode::addAttribute(opacity_intensity);

    fire = nAttr.createColor("fire", "fire");
    nAttr.setDefault(0.0, 0.0, 0.0);
    MPxNode::addAttribute(fire);

    fire_channel = tAttr.create("fireChannel", "fire_channel", MFnData::kString);
    MPxNode::addAttribute(fire_channel);

    fire_intensity = nAttr.create("fireIntensity", "fire_intensity", MFnNumericData::kFloat);
    nAttr.setDefault(1.0f);
    nAttr.setMin(0.0f);
    nAttr.setSoftMax(1.0f);
    nAttr.setChannelBox(true);
    MPxNode::addAttribute(fire_intensity);

    if (add_shared)
    {
        anisotropy = nAttr.create("anisotropy", "anisotropy", MFnNumericData::kFloat);
        nAttr.setDefault(0.0f);
        nAttr.setMin(-1.0f);
        nAttr.setMax(1.0f);
        MPxNode::addAttribute(anisotropy);

        position_offset = nAttr.createPoint("positionOffset", "position_offset");
        nAttr.setDefault(0.0, 0.0, 0.0);
        MPxNode::addAttribute(position_offset);

        interpolation = eAttr.create("interpolation", "interpolation");
        eAttr.addField("Closest", 0);
        eAttr.addField("Trilinear", 1);
        eAttr.addField("Tricubic", 2);
        MPxNode::addAttribute(interpolation);

        compensate_scaling = nAttr.create("compensateScaling", "compensate_scaling", MFnNumericData::kBoolean);
        nAttr.setDefault(true);
        MPxNode::addAttribute(compensate_scaling);
    }
}

void VDBSimpleShaderParams::affect_output(MObject& out_object)
{
    MPxNode::attributeAffects(smoke, out_object);
    MPxNode::attributeAffects(smoke_channel, out_object);
    MPxNode::attributeAffects(smoke_intensity, out_object);
    MPxNode::attributeAffects(anisotropy, out_object);
    MPxNode::attributeAffects(opacity, out_object);
    MPxNode::attributeAffects(opacity_channel, out_object);
    MPxNode::attributeAffects(opacity_intensity, out_object);
    MPxNode::attributeAffects(fire, out_object);
    MPxNode::attributeAffects(fire_channel, out_object);
    MPxNode::attributeAffects(fire_intensity, out_object);

    if (position_offset != MObject::kNullObj)
    {
        MPxNode::attributeAffects(position_offset, out_object);
        MPxNode::attributeAffects(interpolation, out_object);
        MPxNode::attributeAffects(compensate_scaling, out_object);
    }
}

const MTypeId VDBSimpleShaderNode::s_type_id(ID_VDB_SIMPLE_VOLUME_SHADER);
const MString VDBSimpleShaderNode::s_type_name("vdb_simple_shader");
const MString VDBSimpleShaderNode::s_classification("shader/volume");

VDBSimpleShaderParams VDBSimpleShaderNode::s_params;

void* VDBSimpleShaderNode::creator()
{
    return new VDBSimpleShaderNode();
}

VDBSimpleShaderNode::VDBSimpleShaderNode()
{

}

VDBSimpleShaderNode::~VDBSimpleShaderNode()
{

}

MStatus VDBSimpleShaderNode::compute(const MPlug&, MDataBlock&)
{
    return MS::kSuccess;
}

MStatus VDBSimpleShaderNode::initialize()
{
    MFnNumericAttribute nAttr;

    MObject out_color = nAttr.createColor("outColor", "oc");
    nAttr.setKeyable(false);
    nAttr.setStorable(false);
    nAttr.setReadable(true);
    nAttr.setWritable(false);
    addAttribute(out_color);

    s_params.create_params(true);
    return MS::kSuccess;
}
