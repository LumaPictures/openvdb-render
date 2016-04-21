#include "vdb_shader.h"

#include <maya/MFnNumericAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnStringData.h>

#include <lumaNodeId.h>

const MTypeId VDBShaderNode::s_type_id(ID_VDB_VOLUME_SHADER);
const MString VDBShaderNode::s_type_name("vdb_shader");
const MString VDBShaderNode::s_classification("shader/volume");

VDBShaderParams VDBShaderNode::s_shader_params;


VDBShaderParams::VDBShaderParams() : scattering_gradient("scattering"), attenuation_gradient("attenuation"),
                                     emission_gradient("emission")
{

}

void VDBShaderParams::create_params()
{
    MFnEnumAttribute eAttr;
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    MFnStringData sData;

    scattering_source = eAttr.create("scatteringSource", "scattering_source");
    eAttr.addField("parameter", 0);
    eAttr.addField("channel", 1);
    eAttr.setDefault(1);
    MPxNode::addAttribute(scattering_source);

    scattering = nAttr.createColor("scattering", "scattering");
    nAttr.setDefault(1.0, 1.0, 1.0);
    MPxNode::addAttribute(scattering);

    scattering_channel = tAttr.create("scatteringChannel", "scattering_channel", MFnData::kString);
    tAttr.setDefault(sData.create("density"));
    MPxNode::addAttribute(scattering_channel);

    scattering_color = nAttr.createColor("scatteringColor", "scattering_color");
    nAttr.setDefault(1.0, 1.0, 1.0);
    MPxNode::addAttribute(scattering_color);

    scattering_intensity = nAttr.create("scatteringIntensity", "scattering_intensity", MFnNumericData::kFloat);
    nAttr.setDefault(1.0f);
    nAttr.setMin(0.0f);
    nAttr.setSoftMax(1.0f);
    nAttr.setChannelBox(true);
    MPxNode::addAttribute(scattering_intensity);

    anisotropy = nAttr.create("anisotropy", "anisotropy", MFnNumericData::kFloat);
    nAttr.setDefault(0.0f);
    nAttr.setMin(-1.0f);
    nAttr.setMax(1.0f);
    MPxNode::addAttribute(anisotropy);

    attenuation_source = eAttr.create("attenuationSource", "attenuation_source");
    eAttr.addField("parameter", 0);
    eAttr.addField("channel", 1);
    eAttr.addField("scattering", 2);
    eAttr.setDefault(2);
    MPxNode::addAttribute(attenuation_source);

    attenuation = nAttr.createColor("attenuation", "attenuation");
    nAttr.setDefault(1.0, 1.0, 1.0);
    MPxNode::addAttribute(attenuation);

    attenuation_channel = tAttr.create("attenuationChannel", "attenuation_channel", MFnData::kString);
    MPxNode::addAttribute(attenuation_channel);

    attenuation_color = nAttr.createColor("attenuationColor", "attenuation_color");
    nAttr.setDefault(1.0, 1.0, 1.0);
    MPxNode::addAttribute(attenuation_color);

    attenuation_intensity = nAttr.create("attenuationIntensity", "attenuation_intensity", MFnNumericData::kFloat);
    nAttr.setDefault(1.0f);
    nAttr.setMin(0.0f);
    nAttr.setSoftMax(1.0f);
    nAttr.setChannelBox(true);
    MPxNode::addAttribute(attenuation_intensity);

    attenuation_mode = eAttr.create("attenuationMode", "attenuation_mode");
    eAttr.addField("absorption", 0);
    eAttr.addField("extinction", 1);
    eAttr.setChannelBox(true);
    MPxNode::addAttribute(attenuation_mode);

    emission_source = eAttr.create("emissionSource", "emission_source");
    eAttr.addField("parameter", 0);
    eAttr.addField("channel", 1);
    MPxNode::addAttribute(emission_source);

    emission = nAttr.createColor("emission", "emission");
    nAttr.setDefault(0.0, 0.0, 0.0);
    MPxNode::addAttribute(emission);

    emission_channel = tAttr.create("emissionChannel", "emission_channel", MFnData::kString);
    MPxNode::addAttribute(emission_channel);

    emission_color = nAttr.createColor("emissionColor", "emission_color");
    nAttr.setDefault(1.0, 1.0, 1.0);
    MPxNode::addAttribute(emission_color);

    emission_intensity = nAttr.create("emissionIntensity", "emission_intensity", MFnNumericData::kFloat);
    nAttr.setDefault(1.0f);
    nAttr.setMin(0.0f);
    nAttr.setSoftMax(1.0f);
    nAttr.setChannelBox(true);
    MPxNode::addAttribute(emission_intensity);

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

    scattering_gradient.create_params();
    attenuation_gradient.create_params();
    emission_gradient.create_params();
}

void VDBShaderParams::affect_output(MObject& out_object)
{
    MPxNode::attributeAffects(scattering_source, out_object);
    MPxNode::attributeAffects(scattering, out_object);
    MPxNode::attributeAffects(scattering_channel, out_object);
    MPxNode::attributeAffects(scattering_color, out_object);
    MPxNode::attributeAffects(scattering_intensity, out_object);
    MPxNode::attributeAffects(anisotropy, out_object);
    MPxNode::attributeAffects(attenuation_source, out_object);
    MPxNode::attributeAffects(attenuation, out_object);
    MPxNode::attributeAffects(attenuation_channel, out_object);
    MPxNode::attributeAffects(attenuation_color, out_object);
    MPxNode::attributeAffects(attenuation_intensity, out_object);
    MPxNode::attributeAffects(attenuation_mode, out_object);
    MPxNode::attributeAffects(emission_source, out_object);
    MPxNode::attributeAffects(emission, out_object);
    MPxNode::attributeAffects(emission_channel, out_object);
    MPxNode::attributeAffects(emission_color, out_object);
    MPxNode::attributeAffects(emission_intensity, out_object);
    MPxNode::attributeAffects(position_offset, out_object);
    MPxNode::attributeAffects(interpolation, out_object);
    MPxNode::attributeAffects(compensate_scaling, out_object);

    scattering_gradient.affect_output(out_object);
    attenuation_gradient.affect_output(out_object);
    emission_gradient.affect_output(out_object);
}

void* VDBShaderNode::creator()
{
    return new VDBShaderNode();
}

VDBShaderNode::VDBShaderNode()
{

}

VDBShaderNode::~VDBShaderNode()
{

}

MStatus VDBShaderNode::compute(const MPlug&, MDataBlock&)
{
    return MS::kUnknownParameter;
}

MStatus VDBShaderNode::initialize()
{
    MFnNumericAttribute nAttr;

    MObject out_color = nAttr.createColor("outColor", "oc");
    nAttr.setKeyable(false);
    nAttr.setStorable(false);
    nAttr.setReadable(true);
    nAttr.setWritable(false);
    addAttribute(out_color);

    s_shader_params.create_params();

    return MS::kSuccess;
}



