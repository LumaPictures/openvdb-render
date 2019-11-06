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
