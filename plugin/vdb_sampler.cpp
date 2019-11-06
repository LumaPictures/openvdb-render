// Copyright 2019 Luma Pictures
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "vdb_sampler.h"

#include "../util/node_ids.h"

#include <maya/MFnEnumAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MRampAttribute.h>

#include <maya/MFnTypedAttribute.h>

VDBGradientParams::VDBGradientParams(const char* _gradient_name) : gradient_name(_gradient_name)
{
}

void VDBGradientParams::create_params()
{
    MFnEnumAttribute eAttr;
    MFnNumericAttribute nAttr;
    MRampAttribute rAttr;

    mode = eAttr.create(gradient_name + "ChannelMode", gradient_name + "_channel_mode");
    eAttr.addField("Raw", 0);
    eAttr.addField("Float", 1);
    eAttr.addField("RGB", 2);
    eAttr.addField("Float Ramp", 3);
    eAttr.addField("RGB Ramp", 4);
    eAttr.setDefault(0);
    MPxNode::addAttribute(mode);

    contrast = nAttr.create(gradient_name + "Contrast", gradient_name + "_contrast", MFnNumericData::kFloat);
    nAttr.setDefault(1.0f);
    nAttr.setConnectable(false);
    MPxNode::addAttribute(contrast);

    contrast_pivot = nAttr.create(gradient_name + "ContrastPivot", gradient_name + "_contrast_pivot",
                                  MFnNumericData::kFloat);
    nAttr.setDefault(0.5f);
    nAttr.setConnectable(false);
    MPxNode::addAttribute(contrast_pivot);

    input_min = nAttr.create(gradient_name + "InputMin", gradient_name + "_input_min", MFnNumericData::kFloat);
    nAttr.setDefault(0.0f);
    nAttr.setSoftMin(0.0f);
    nAttr.setSoftMax(1.0f);
    nAttr.setConnectable(false);
    nAttr.setChannelBox(true);
    MPxNode::addAttribute(input_min);

    input_max = nAttr.create(gradient_name + "InputMax", gradient_name + "_input_max", MFnNumericData::kFloat);
    nAttr.setDefault(1.0f);
    nAttr.setSoftMin(0.0f);
    nAttr.setSoftMax(1.0f);
    nAttr.setConnectable(false);
    nAttr.setChannelBox(true);
    MPxNode::addAttribute(input_max);

    bias = nAttr.create(gradient_name + "Bias", gradient_name + "_bias", MFnNumericData::kFloat);
    nAttr.setDefault(0.5f);
    nAttr.setSoftMin(0.0f);
    nAttr.setSoftMax(1.0f);
    nAttr.setConnectable(false);
    MPxNode::addAttribute(bias);

    gain = nAttr.create(gradient_name + "Gain", gradient_name + "_gain", MFnNumericData::kFloat);
    nAttr.setDefault(0.5f);
    nAttr.setSoftMin(0.0f);
    nAttr.setSoftMax(1.0f);
    nAttr.setConnectable(false);
    MPxNode::addAttribute(gain);

    output_min = nAttr.create(gradient_name + "OutputMin", gradient_name + "_output_min", MFnNumericData::kFloat);
    nAttr.setDefault(0.0f);
    nAttr.setSoftMin(0.0f);
    nAttr.setSoftMax(1.0f);
    nAttr.setConnectable(false);
    nAttr.setChannelBox(true);
    MPxNode::addAttribute(output_min);

    output_max = nAttr.create(gradient_name + "OutputMax", gradient_name + "_output_max", MFnNumericData::kFloat);
    nAttr.setDefault(1.0f);
    nAttr.setSoftMin(0.0f);
    nAttr.setSoftMax(1.0f);
    nAttr.setConnectable(false);
    nAttr.setChannelBox(true);
    MPxNode::addAttribute(output_max);

    clamp_min = nAttr.create(gradient_name + "ClampMin", gradient_name + "_clamp_min", MFnNumericData::kBoolean);
    nAttr.setDefault(false);
    nAttr.setConnectable(false);
    MPxNode::addAttribute(clamp_min);

    clamp_max = nAttr.create(gradient_name + "ClampMax", gradient_name + "_clamp_max", MFnNumericData::kBoolean);
    nAttr.setDefault(false);
    nAttr.setConnectable(false);
    MPxNode::addAttribute(clamp_max);

    gamma = nAttr.create(gradient_name + "Gamma", gradient_name + "_gamma", MFnNumericData::kFloat);
    nAttr.setDefault(1.0f);
    nAttr.setMin(0.0f);
    nAttr.setSoftMax(5.0f);
    nAttr.setConnectable(false);
    MPxNode::addAttribute(gamma);

    hue_shift = nAttr.create(gradient_name + "HueShift", gradient_name + "_hue_shift", MFnNumericData::kFloat);
    nAttr.setDefault(0.0f);
    nAttr.setSoftMin(0.0f);
    nAttr.setSoftMax(1.0f);
    nAttr.setConnectable(false);
    MPxNode::addAttribute(hue_shift);

    saturation = nAttr.create(gradient_name + "Saturation", gradient_name + "_saturation", MFnNumericData::kFloat);
    nAttr.setDefault(1.0f);
    nAttr.setMin(0.0f);
    nAttr.setMax(1.0f);
    nAttr.setConnectable(false);
    MPxNode::addAttribute(saturation);

    exposure = nAttr.create(gradient_name + "Exposure", gradient_name + "_exposure", MFnNumericData::kFloat);
    nAttr.setDefault(0.0f);
    nAttr.setSoftMin(-5.0f);
    nAttr.setSoftMax(5.0f);
    nAttr.setConnectable(false);
    MPxNode::addAttribute(exposure);

    multiply = nAttr.create(gradient_name + "Multiply", gradient_name + "_multiply", MFnNumericData::kFloat);
    nAttr.setDefault(1.0f);
    nAttr.setSoftMin(0.0f);
    nAttr.setSoftMax(10.f);
    nAttr.setConnectable(false);
    MPxNode::addAttribute(multiply);

    add = nAttr.create(gradient_name + "Add", gradient_name + "_add", MFnNumericData::kFloat);
    nAttr.setDefault(0.0f);
    nAttr.setSoftMin(0.0f);
    nAttr.setSoftMax(10.f);
    nAttr.setConnectable(false);
    MPxNode::addAttribute(add);

    float_ramp = rAttr.createCurveRamp(gradient_name + "FloatRamp", gradient_name + "_float_ramp");
    MPxNode::addAttribute(float_ramp);

    rgb_ramp = rAttr.createColorRamp(gradient_name + "RgbRamp", gradient_name + "_rgb_ramp");
    MPxNode::addAttribute(rgb_ramp);
}

void VDBGradientParams::affect_output(MObject& output_object)
{
    MPxNode::attributeAffects(mode, output_object);

    MPxNode::attributeAffects(contrast, output_object);
    MPxNode::attributeAffects(contrast_pivot, output_object);

    MPxNode::attributeAffects(input_min, output_object);
    MPxNode::attributeAffects(input_max, output_object);
    MPxNode::attributeAffects(bias, output_object);
    MPxNode::attributeAffects(gain, output_object);
    MPxNode::attributeAffects(output_min, output_object);
    MPxNode::attributeAffects(output_max, output_object);
    MPxNode::attributeAffects(clamp_min, output_object);
    MPxNode::attributeAffects(clamp_max, output_object);

    MPxNode::attributeAffects(gamma, output_object);
    MPxNode::attributeAffects(hue_shift, output_object);
    MPxNode::attributeAffects(saturation, output_object);
    MPxNode::attributeAffects(exposure, output_object);
    MPxNode::attributeAffects(multiply, output_object);
    MPxNode::attributeAffects(add, output_object);

    MPxNode::attributeAffects(float_ramp, output_object);
    MPxNode::attributeAffects(rgb_ramp, output_object);
}

void VDBGradientParams::post_constructor(MObject tmo)
{
    MPlug float_plug(tmo, float_ramp);
    MPlug rgb_plug(tmo, rgb_ramp);
}

bool VDBGradientParams::check_plug(const MPlug& plug)
{
    return plug == mode || plug == contrast || plug == contrast_pivot || plug == input_min ||
           plug == input_max || plug == bias || plug == gain || plug == output_min ||
           plug == output_max || plug == clamp_min || plug == clamp_max || plug == gamma ||
           plug == hue_shift || plug == saturation || plug == exposure || plug == multiply ||
           plug == add || plug == float_ramp || plug == rgb_ramp;
}


const MTypeId VDBSamplerNode::s_type_id(ID_VDB_VOLUME_SAMPLER);
const MString VDBSamplerNode::s_type_name("vdb_sampler");
const MString VDBSamplerNode::s_classification("utility/general/volume");

MObject VDBSamplerNode::s_channel;
MObject VDBSamplerNode::s_position_offset;
MObject VDBSamplerNode::s_interpolation;
VDBGradientParams VDBSamplerNode::s_gradient("base");

void* VDBSamplerNode::creator()
{
    return new VDBSamplerNode();
}

MStatus VDBSamplerNode::initialize()
{
    MFnNumericAttribute nAttr;
    MFnEnumAttribute eAttr;
    MFnTypedAttribute tAttr;

    MObject out_color = nAttr.createColor("outColor", "oc");
    nAttr.setKeyable(false);
    nAttr.setStorable(false);
    nAttr.setReadable(true);
    nAttr.setWritable(false);
    addAttribute(out_color);

    s_channel = tAttr.create("channel", "channel", MFnData::kString);
    addAttribute(s_channel);

    s_position_offset = nAttr.createPoint("positionOffset", "position_offset");
    addAttribute(s_position_offset);

    s_interpolation = eAttr.create("interpolation", "interpolation");
    eAttr.addField("Closest", 0);
    eAttr.addField("Trilinear", 1);
    eAttr.addField("Tricubic", 2);
    eAttr.setDefault(1);
    addAttribute(s_interpolation);

    s_gradient.create_params();

    return MS::kSuccess;
}

MStatus VDBSamplerNode::compute(const MPlug&, MDataBlock&)
{
    return MS::kUnknownParameter;
}

void VDBSamplerNode::postConstructor()
{
    s_gradient.post_constructor(thisMObject());
}
