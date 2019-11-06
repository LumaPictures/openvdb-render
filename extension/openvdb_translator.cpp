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
#include "openvdb_translator.h"

#include <set>
#include <functional>
#include <array>

#include "shader_params_translator.h"
#include "../plugin/shader_mode.h"

namespace {
    using link_function_t = std::function<void(AtNode*)>;
    template <unsigned num_elems>
    using elems_name_t = std::array<const char*, num_elems>;

    template <unsigned num_elems> inline
    void iterate_param_elems(AtNode* node, const char* param_name, const elems_name_t<num_elems>& elems, const link_function_t& func) {
        // We could use a thread local vector here, and copy things there
        // but that's a bit too much to do.
        const std::string param_name_str(param_name);
        for (auto elem : elems) {
            auto query_name = param_name_str;
            query_name += elem;
            func(AiNodeGetLink(node, query_name.c_str()));
        }
    }

    template <typename T> bool
    is_in_vector(const std::vector<T>& elems, const T& elem) {
        return std::find(elems.begin(), elems.end(), elem) != elems.end();
    }

    inline
    void iterate_param_links(AtNode* node, const char* param_name, int param_type, link_function_t func) {
        constexpr static elems_name_t<3> rgb_elems = {"r", "g", "b"};
        constexpr static elems_name_t<4> rgba_elems = {"r", "g", "b", "a"};
        constexpr static elems_name_t<3> vec_elems = {"x", "y", "z"};
        constexpr static elems_name_t<2> vec2_elems = {"x", "y"};
        func(AiNodeGetLink(node, param_name));
        switch (param_type) {
            case AI_TYPE_RGB:
                iterate_param_elems<rgb_elems.size()>(node, param_name, rgb_elems, func);
                break;
            case AI_TYPE_RGBA:
                iterate_param_elems<rgba_elems.size()>(node, param_name, rgba_elems, func);
                break;
            case AI_TYPE_VECTOR:
                iterate_param_elems<vec_elems.size()>(node, param_name, vec_elems, func);
                break;
            case AI_TYPE_VECTOR2:
                iterate_param_elems<vec2_elems.size()>(node, param_name, vec2_elems, func);
                break;
            default:
                return;
        }
    }

    inline void
    check_arnold_nodes(AtNode* node, std::vector<AtNode*>& checked_arnold_nodes, std::set<std::string>& out_grids)
    {
        if (node == nullptr) {
            return;
        }

        if (is_in_vector(checked_arnold_nodes, node)) {
            return;
        }

        checked_arnold_nodes.push_back(node);

        auto check_channel = [&node, &out_grids](const char* channel) {
            auto ch = AiNodeGetStr(node, channel).c_str();
            if (ch != nullptr && ch[0] != '\0') {
                out_grids.insert(std::string(ch));
            }
        };

        static const AtString standard_volume_str("standard_volume");
        static const std::vector<const char*> standard_volume_channels = {
            "density_channel",
            "scatter_color_channel",
            "transparent_channel",
            "emission_channel",
            "temperature_channel",
        };

        const auto* node_entry = AiNodeGetNodeEntry(node);
        auto* param_iter = AiNodeEntryGetParamIterator(node_entry);

        if (AiNodeIs(node, standard_volume_str)) {
            for (auto channel: standard_volume_channels) {
                check_channel(channel);
            }
            while (!AiParamIteratorFinished(param_iter)) {
                const auto* param_entry = AiParamIteratorGetNext(param_iter);
                auto param_name = AiParamGetName(param_entry);
                const auto param_type = AiParamGetType(param_entry);
                iterate_param_links(node, param_name, param_type, [&checked_arnold_nodes, &out_grids] (AtNode* link) {
                    check_arnold_nodes(link, checked_arnold_nodes, out_grids);
                });
            }
        } else {
            while (!AiParamIteratorFinished(param_iter)) {
                const auto* param_entry = AiParamIteratorGetNext(param_iter);
                auto param_name = AiParamGetName(param_entry);
                const auto param_type = AiParamGetType(param_entry);
                if (param_type == AI_TYPE_STRING) {
                    auto is_volume_sample = false;
                    constexpr auto volume_sample_name = "volume_sample";
                    if (AiMetaDataGetBool(node_entry, AiParamGetName(param_entry), volume_sample_name, &is_volume_sample) &&
                        is_volume_sample) {
                        check_channel(AiParamGetName(param_entry));
                    }
                } else {
                    iterate_param_links(node, param_name, param_type, [&checked_arnold_nodes, &out_grids] (AtNode* link) {
                        check_arnold_nodes(link, checked_arnold_nodes, out_grids);
                    });
                }
            }
        }
        AiParamIteratorDestroy(param_iter);
    }

    inline bool
    is_instance(const AtNode* node) {
        return node == nullptr ? false : (strcmp(
            AiNodeEntryGetName(AiNodeGetNodeEntry(node)), "ginstance" ) == 0);
    }
}

void* OpenvdbTranslator::creator()
{
    return new OpenvdbTranslator();
}

AtNode* OpenvdbTranslator::CreateArnoldNodes()
{
    if (IsMasterInstance()) {
        auto* volume = AddArnoldNode("volume");
        // TODO: !!!
        if (!FindMayaPlug("overrideShader").asBool()) {
            const auto shader_mode = FindMayaPlug("shaderMode").asShort();
            if (shader_mode == SHADER_MODE_SIMPLE) {
                AddArnoldNode("openvdb_simple_shader", "shader");
            } else if (shader_mode == SHADER_MODE_STANDARD_VOLUME) {
                AddArnoldNode("standard_volume", "shader");
            }
        }
        return volume;
    }
    return AddArnoldNode("ginstance");
}

void OpenvdbTranslator::Export(AtNode* volume)
{
    ExportMatrix(volume);
    if (is_instance(volume)) {
        const auto master_instance = GetMasterInstance();
        auto* master_node = AiNodeLookUpByName(master_instance.partialPathName().asChar());
        if (master_node == nullptr) { return; }
        AiNodeSetPtr(volume, "node", master_node);
        AiNodeSetBool(volume, "inherit_xform", false);
        return;
    }

    AiNodeSetStr(volume, "filename", FindMayaPlug("outVdbPath").asString().asChar());
    AiNodeSetBool(volume, "matte", FindMayaPlug("matte").asBool());

    ProcessParameter(volume, "min", AI_TYPE_VECTOR, "bboxMin");
    ProcessParameter(volume, "max", AI_TYPE_VECTOR, "bboxMax");

    // Velocity grids.
    MString velocity_grids_string = FindMayaPlug("velocity_grids").asString();
    MStringArray velocity_grids;
    velocity_grids_string.split(' ', velocity_grids);
    const unsigned int velocity_grids_count = velocity_grids.length();
    if (velocity_grids_count > 0) {
        AtArray* velocity_grid_names = AiArrayAllocate(velocity_grids_count, 1, AI_TYPE_STRING);
        for (unsigned int i = 0; i < velocity_grids_count; ++i) {
            AiArraySetStr(velocity_grid_names, i, velocity_grids[i].asChar());
        }
        AiNodeSetArray(volume, "velocity_grids", velocity_grid_names);
        AiNodeSetFlt(volume, "velocity_scale", FindMayaPlug("velocityScale").asFloat());
        AiNodeSetFlt(volume, "velocity_fps", FindMayaPlug("velocityFps").asFloat());
        AiNodeSetFlt(volume, "motion_start", FindMayaPlug("velocityShutterStart").asFloat());
        AiNodeSetFlt(volume, "motion_end", FindMayaPlug("velocityShutterEnd").asFloat());
    }

    AiNodeSetFlt(volume, "volume_padding", FindMayaPlug("boundsSlack").asFloat());
    const float sampling_quality = FindMayaPlug("samplingQuality").asFloat();
    const float voxel_size = FindMayaPlug("voxelSize").asFloat();
    AiNodeSetFlt(volume, "step_size", voxel_size / (sampling_quality / 100.0f));

    AiNodeSetBool(volume, "receive_shadows", FindMayaPlug("receiveShadows").asBool());
    AiNodeSetBool(volume, "self_shadows", FindMayaPlug("selfShadows").asBool());

    // FIXME: Why do we do this twice?
    AiNodeSetByte(volume, "visibility", ComputeVisibility());

    AtByte visibility = 0;
    static const std::vector<std::pair<const char*, uint8_t>> ray_types {
        {"primaryVisibility", AI_RAY_CAMERA},
        {"castsShadows", AI_RAY_SHADOW},
        {"visibleInDiffuseTransmissions", AI_RAY_DIFFUSE_TRANSMIT},
        {"visibleInSpecularTransmissions", AI_RAY_SPECULAR_TRANSMIT},
        {"visibleInVolumes", AI_RAY_VOLUME},
        {"visibleInDiffuseReflections", AI_RAY_DIFFUSE_REFLECT},
        {"visibleInSpecularReflections", AI_RAY_SPECULAR_REFLECT},
        {"visibleInSubsurface", AI_RAY_SUBSURFACE},
    };
    for (const auto& it: ray_types) {
        const auto plug = FindMayaPlug(it.first);
        if (!plug.isNull() && plug.asBool()) {
            visibility = visibility | it.second;
        }
    }
    AiNodeSetByte(volume, "visibility", visibility);

    AtNode* shader = nullptr;
    if (FindMayaPlug("overrideShader").asBool()) {
        const int instance_num = m_dagPath.isInstanced() ? m_dagPath.instanceNumber() : 0;

        MPlug shading_group_plug = GetNodeShadingGroup(m_dagPath.node(), instance_num);
        if (!shading_group_plug.isNull()) {
            shader = ExportConnectedNode(shading_group_plug);
        }
    } else {
        shader = GetArnoldNode("shader");
        AiNodeSetPtr(volume, "shader", shader);
        const auto shader_mode = FindMayaPlug("shaderMode").asShort();
        if (shader_mode == SHADER_MODE_SIMPLE) {
            ExportSimpleParams(shader);
        } else if (shader_mode == SHADER_MODE_STANDARD_VOLUME) {
            ProcessParameter(shader, "density", AI_TYPE_FLOAT, "svDensity");
            ProcessParameter(shader, "density_channel", AI_TYPE_STRING, "svDensityChannel");

            ProcessParameter(shader, "scatter", AI_TYPE_FLOAT, "svScatter");
            ProcessParameter(shader, "scatter_color", AI_TYPE_RGB, "svScatterColor");
            ProcessParameter(shader, "scatter_color_channel", AI_TYPE_STRING, "svScatterColorChannel");
            ProcessParameter(shader, "scatter_anisotropy", AI_TYPE_FLOAT, "svScatterAnisotropy");

            ProcessParameter(shader, "transparent", AI_TYPE_RGB, "svTransparent");
            ProcessParameter(shader, "transparent_channel", AI_TYPE_STRING, "svTransparentChannel");

            ProcessParameter(shader, "emission_mode", AI_TYPE_INT, "svEmissionMode");
            ProcessParameter(shader, "emission", AI_TYPE_FLOAT, "svEmission");
            ProcessParameter(shader, "emission_color", AI_TYPE_RGB, "svEmissionColor");
            ProcessParameter(shader, "emission_channel", AI_TYPE_STRING, "svEmissionChannel");

            ProcessParameter(shader, "temperature", AI_TYPE_FLOAT, "svTemperature");
            ProcessParameter(shader, "temperature_channel", AI_TYPE_STRING, "svTemperatureChannel");
            ProcessParameter(shader, "blackbody_kelvin", AI_TYPE_FLOAT, "svBlackbodyKelvin");
            ProcessParameter(shader, "blackbody_intensity", AI_TYPE_FLOAT, "svBlackbodyIntensity");

            ProcessParameter(shader, "interpolation", AI_TYPE_INT, "interpolation");
        }
    }

    if (shader != nullptr) {
        AiNodeSetPtr(volume, "shader", shader);

        std::set<std::string> out_grids;
        std::vector<AtNode*> checked_arnold_nodes;

        check_arnold_nodes(shader, checked_arnold_nodes, out_grids);

        MString additional_grids_string = FindMayaPlug("additional_channel_export").asString();
        MStringArray additional_grids;
        additional_grids_string.split(' ', additional_grids);
        const unsigned int additional_grids_count = additional_grids.length();
        for (unsigned int i = 0; i < additional_grids_count; ++i) {
            const MString additional_grid = additional_grids[i];
            if (additional_grid.length() > 0) {
                out_grids.insert(additional_grid.asChar());
            }
        }

        auto* grid_names = AiArrayAllocate(static_cast<unsigned int>(out_grids.size()), 1, AI_TYPE_STRING);

        unsigned int id = 0;
        for (const auto& out_grid : out_grids) {
            AiArraySetStr(grid_names, id, out_grid.c_str());
            ++id;
        }

        AiNodeSetArray(volume, "grids", grid_names);
    }
}

void OpenvdbTranslator::ExportMotion(AtNode* volume)
{
    ExportMatrix(volume);
}
