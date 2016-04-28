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
    if (!FindMayaPlug("overrideShader").asBool())
    {
        if (FindMayaPlug("shaderMode").asShort() == 0)
            AddArnoldNode("openvdb_shader", "shader");
        else
            AddArnoldNode("openvdb_simple_shader", "shader");
    }
    return volume;
}

void check_arnold_nodes(AtNode* node, std::set<AtNode*>& checked_arnold_nodes, std::set<std::string>& out_grids)
{
    if (node == nullptr)
        return;

    if (checked_arnold_nodes.find(node) != checked_arnold_nodes.end())
        return;

    checked_arnold_nodes.insert(node);

    auto check_channel = [&](const char* channel) {
        const char* ch = AiNodeGetStr(node, channel);
        if (ch != 0 && strlen(ch) > 0)
            out_grids.insert(ch);
    };

    if (AiNodeIs(node, "volume_sample_float") || AiNodeIs(node, "volume_sample_rgb") || AiNodeIs(node, "openvdb_sampler"))
    {
        check_channel("channel");
    }
    else if (AiNodeIs(node, "volume_collector") || AiNodeIs(node, "openvdb_shader"))
    {
        check_channel("scattering_channel");
        check_channel("attenuation_channel");
        check_channel("emission_channel");
    }
    else if (AiNodeIs(node, "openvdb_simple_shader"))
    {
        check_channel("smoke_channel");
        check_channel("opacity_channel");
        check_channel("fire_channel");
    }

    AtParamIterator* param_iter = AiNodeEntryGetParamIterator(AiNodeGetNodeEntry(node));
    while (!AiParamIteratorFinished(param_iter))
    {
        const AtParamEntry* param_entry = AiParamIteratorGetNext(param_iter);
        if (AiParamGetType(param_entry) == AI_TYPE_NODE)
            check_arnold_nodes(reinterpret_cast<AtNode*>(AiNodeGetPtr(node, AiParamGetName(param_entry))), checked_arnold_nodes, out_grids);
        else
            check_arnold_nodes(AiNodeGetLink(node, AiParamGetName(param_entry)), checked_arnold_nodes, out_grids);
    }
    AiParamIteratorDestroy(param_iter);
}

void OpenvdbTranslator::Export(AtNode* volume)
{
    AiNodeSetStr(volume, "dso", (std::string(getenv("MTOA_PATH")) + std::string("procedurals/volume_openvdb.so")).c_str());

    ExportMatrix(volume, 0);

    AiNodeDeclare(volume, "filename", "constant STRING");
    AiNodeSetStr(volume, "filename", FindMayaPlug("outVdbPath").asString().asChar());

    ProcessParameter(volume, "min", AI_TYPE_POINT, "bboxMin");
    ProcessParameter(volume, "max", AI_TYPE_POINT, "bboxMax");

    AtNode* shader = nullptr;

    if (FindMayaPlug("overrideShader").asBool())
    {
        const int instanceNum = m_dagPath.isInstanced() ? m_dagPath.instanceNumber() : 0;

        MPlug shadingGroupPlug = GetNodeShadingGroup(m_dagPath.node(), instanceNum);
        if (!shadingGroupPlug.isNull())
        {
            shader = ExportNode(shadingGroupPlug);
            if (shader != 0)
                AiNodeSetPtr(volume, "shader", shader);
        }
    }
    else
    {
        shader = GetArnoldNode("shader");
        AiNodeSetPtr(volume, "shader", shader);
        if (FindMayaPlug("shaderMode").asShort() == 0)
            ExportParams(shader);
        else
        {
            ProcessParameter(shader, "smoke", AI_TYPE_RGB, "smoke");
            ProcessParameter(shader, "smoke_channel", AI_TYPE_STRING, "smokeChannel");
            ProcessParameter(shader, "smoke_intensity", AI_TYPE_FLOAT, "smokeIntensity");
            ProcessParameter(shader, "anisotropy", AI_TYPE_FLOAT, "anisotropy");

            ProcessParameter(shader, "opacity", AI_TYPE_RGB, "opacity");
            ProcessParameter(shader, "opacity_channel", AI_TYPE_STRING, "opacityChannel");
            ProcessParameter(shader, "opacity_intensity", AI_TYPE_FLOAT, "opacityIntensity");
            ProcessParameter(shader, "opacity_mode", AI_TYPE_INT, "opacityMode");

            ProcessParameter(shader, "fire", AI_TYPE_RGB, "fire");
            ProcessParameter(shader, "fire_channel", AI_TYPE_STRING, "fireChannel");
            ProcessParameter(shader, "fire_intensity", AI_TYPE_FLOAT, "fireIntensity");

            ProcessParameter(shader, "position_offset", AI_TYPE_VECTOR, "positionOffset");
            ProcessParameter(shader, "interpolation", AI_TYPE_INT, "interpolation");
            ProcessParameter(shader, "compensate_scaling", AI_TYPE_BOOLEAN, "compensateScaling");
        }
    }

    std::set<std::string> out_grids;
    std::set<AtNode*> checked_arnold_nodes;

    check_arnold_nodes(shader, checked_arnold_nodes, out_grids);

    MString additional_grids_string = FindMayaPlug("additional_channel_export").asString();
    MStringArray additional_grids;
    additional_grids_string.split(' ', additional_grids);
    const unsigned int additional_grids_count = additional_grids.length();
    for (unsigned int i = 0; i < additional_grids_count; ++i)
    {
        const MString additional_grid = additional_grids[i];
        if (additional_grid.length())
            out_grids.insert(additional_grid.asChar());
    }

    AtArray* grid_names = AiArrayAllocate(static_cast<unsigned int>(out_grids.size()), 1, AI_TYPE_STRING);

    unsigned int id = 0;
    for (auto out_grid : out_grids)
    {
        AiArraySetStr(grid_names, id, out_grid.c_str());
        ++id;
    }

    AiNodeDeclare(volume, "grids", "constant ARRAY STRING");
    AiNodeSetArray(volume, "grids", grid_names);

    const float sampling_quality = FindMayaPlug("samplingQuality").asFloat();
    const float voxel_size = FindMayaPlug("voxelSize").asFloat();
    AiNodeSetFlt(volume, "step_size", voxel_size / (sampling_quality / 100.0f));

    AiNodeSetBool(volume, "matte", FindMayaPlug("matte").asBool());
}

void OpenvdbTranslator::ExportMotion(AtNode* volume, unsigned int step)
{
    ExportMatrix(volume, step);
}
