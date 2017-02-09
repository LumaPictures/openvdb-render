#include "openvdb_translator.h"

#include <set>

#include "shader_translator.h"

void* OpenvdbTranslator::creator()
{
    return new OpenvdbTranslator();
}

AtNode* OpenvdbTranslator::CreateArnoldNodes()
{
    AtNode* volume = AddArnoldNode("volume");
    if (!FindMayaPlug("overrideShader").asBool()) {
        if (FindMayaPlug("shaderMode").asShort() == 0) {
            AddArnoldNode("openvdb_shader", "shader");
        } else {
            AddArnoldNode("openvdb_simple_shader", "shader");
        }
    }
    return volume;
}

void check_arnold_nodes(AtNode* node, std::set<AtNode*>& checked_arnold_nodes, std::set<std::string>& out_grids)
{
    if (node == nullptr) {
        return;
    }

    if (checked_arnold_nodes.find(node) != checked_arnold_nodes.end()) {
        return;
    }

    checked_arnold_nodes.insert(node);

    auto check_channel = [&node, &out_grids](const char* channel) {
        const char* ch = AiNodeGetStr(node, channel);
        if (ch != nullptr && strlen(ch) > 0) {
            out_grids.insert(std::string(ch));
        }
    };

    const auto* node_entry = AiNodeGetNodeEntry(node);
    auto* param_iter = AiNodeEntryGetParamIterator(node_entry);
    while (!AiParamIteratorFinished(param_iter)) {
        const auto* param_entry = AiParamIteratorGetNext(param_iter);
        if (AiParamGetType(param_entry) == AI_TYPE_NODE) {
            check_arnold_nodes(reinterpret_cast<AtNode*>(AiNodeGetPtr(node, AiParamGetName(param_entry))),
                               checked_arnold_nodes, out_grids);
        } else {
            if (AiParamGetType(param_entry) == AI_TYPE_STRING) {
                auto volume_sample = false;
                constexpr auto volume_sample_name = "volume_sample";
                if (AiMetaDataGetBool(node_entry, AiParamGetName(param_entry), volume_sample_name, &volume_sample) &&
                    volume_sample) {
                    check_channel(AiParamGetName(param_entry));
                }
            }
            // TODO: check for sub connections
            check_arnold_nodes(AiNodeGetLink(node, AiParamGetName(param_entry)), checked_arnold_nodes, out_grids);
        }
    }
    AiParamIteratorDestroy(param_iter);
}

void OpenvdbTranslator::Export(AtNode* volume)
{
    AiNodeSetStr(volume, "dso",
                 (std::string(getenv("MTOA_PATH")) + std::string("procedurals/volume_openvdb.so")).c_str());

    ExportMatrix(volume, 0);

    AiNodeDeclare(volume, "filename", "constant STRING");
    AiNodeSetStr(volume, "filename", FindMayaPlug("outVdbPath").asString().asChar());

    ProcessParameter(volume, "min", AI_TYPE_POINT, "bboxMin");
    ProcessParameter(volume, "max", AI_TYPE_POINT, "bboxMax");

    AtNode* shader = nullptr;

    if (FindMayaPlug("overrideShader").asBool()) {
        const int instanceNum = m_dagPath.isInstanced() ? m_dagPath.instanceNumber() : 0;

        MPlug shadingGroupPlug = GetNodeShadingGroup(m_dagPath.node(), instanceNum);
        if (!shadingGroupPlug.isNull()) {
            shader = ExportNode(shadingGroupPlug);
            if (shader != 0) {
                AiNodeSetPtr(volume, "shader", shader);
            }
        }
    } else {
        shader = GetArnoldNode("shader");
        AiNodeSetPtr(volume, "shader", shader);
        if (FindMayaPlug("shaderMode").asShort() == 0) {
            ExportArnoldParams(shader);
        } else {
            ExportSimpleParams(shader);
        }
    }

    std::set<std::string> out_grids;
    std::set<AtNode*> checked_arnold_nodes;

    check_arnold_nodes(shader, checked_arnold_nodes, out_grids);

    MString additional_grids_string = FindMayaPlug("additional_channel_export").asString();
    MStringArray additional_grids;
    additional_grids_string.split(' ', additional_grids);
    const unsigned int additional_grids_count = additional_grids.length();
    for (unsigned int i = 0; i < additional_grids_count; ++i) {
        const MString additional_grid = additional_grids[i];
        if (additional_grid.length()) {
            out_grids.insert(additional_grid.asChar());
        }
    }

    AtArray* grid_names = AiArrayAllocate(static_cast<unsigned int>(out_grids.size()), 1, AI_TYPE_STRING);

    unsigned int id = 0;
    for (const auto& out_grid : out_grids) {
        AiArraySetStr(grid_names, id, out_grid.c_str());
        ++id;
    }

    AiNodeDeclare(volume, "grids", "constant ARRAY STRING");
    AiNodeSetArray(volume, "grids", grid_names);

    MString velocity_grids_string = FindMayaPlug("velocity_grids").asString();
    MStringArray velocity_grids;
    velocity_grids_string.split(' ', velocity_grids);
    const unsigned int velocity_grids_count = velocity_grids.length();
    if (velocity_grids_count > 0) {
        AiNodeDeclare(volume, "velocity_grids", "constant ARRAY STRING");
        AtArray* velocity_grid_names = AiArrayAllocate(velocity_grids_count, 1, AI_TYPE_STRING);
        for (unsigned int i = 0; i < velocity_grids_count; ++i)
            AiArraySetStr(velocity_grid_names, i, velocity_grids[i].asChar());
        AiNodeSetArray(volume, "velocity_grids", velocity_grid_names);

        AiNodeDeclare(volume, "velocity_scale", "constant FLOAT");
        AiNodeSetFlt(volume, "velocity_scale", FindMayaPlug("velocityScale").asFloat());

        AiNodeDeclare(volume, "velocity_fps", "constant FLOAT");
        AiNodeSetFlt(volume, "velocity_fps", FindMayaPlug("velocityFps").asFloat());

        AiNodeDeclare(volume, "velocity_shutter_start", "constant FLOAT");
        AiNodeSetFlt(volume, "velocity_shutter_start", FindMayaPlug("velocityShutterStart").asFloat());

        AiNodeDeclare(volume, "velocity_shutter_end", "constant FLOAT");
        AiNodeSetFlt(volume, "velocity_shutter_end", FindMayaPlug("velocityShutterEnd").asFloat());
    }

    AiNodeDeclare(volume, "bounds_slack", "constant FLOAT");
    AiNodeSetFlt(volume, "bounds_slack", FindMayaPlug("boundsSlack").asFloat());

    const float sampling_quality = FindMayaPlug("samplingQuality").asFloat();
    const float voxel_size = FindMayaPlug("voxelSize").asFloat();
    AiNodeSetFlt(volume, "step_size", voxel_size / (sampling_quality / 100.0f));

    AiNodeSetBool(volume, "matte", FindMayaPlug("matte").asBool());

    AiNodeSetBool(volume, "receive_shadows", FindMayaPlug("receiveShadows").asBool());

    AiNodeSetByte(volume, "visibility", ComputeVisibility());

    AtByte visibility = 0;
    if (FindMayaPlug("primaryVisibility").asBool()) {
        visibility |= AI_RAY_CAMERA;
    }
    if (FindMayaPlug("castsShadows").asBool()) {
        visibility |= AI_RAY_SHADOW;
    }
    if (FindMayaPlug("visibleInDiffuse").asBool()) {
        visibility |= AI_RAY_DIFFUSE;
    }
    if (FindMayaPlug("visibleInReflections").asBool()) {
        visibility |= AI_RAY_REFLECTED;
    }
    if (FindMayaPlug("visibleInGlossy").asBool()) {
        visibility |= AI_RAY_GLOSSY;
    }
    if (FindMayaPlug("visibleInRefractions").asBool()) {
        visibility |= AI_RAY_REFRACTED;
    }
    if (FindMayaPlug("visibleInSubsurface").asBool()) {
        visibility |= AI_RAY_SUBSURFACE;
    }

    AiNodeSetByte(volume, "visibility", visibility);
    AiNodeSetBool(volume, "self_shadows", FindMayaPlug("selfShadows").asBool());
}

void OpenvdbTranslator::ExportMotion(AtNode* volume, unsigned int step)
{
    ExportMatrix(volume, step);
}
