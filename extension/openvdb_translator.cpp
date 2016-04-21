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
        AddArnoldNode("openvdb_shader", "shader");
    return volume;
}

void check_maya_node(MObject node, std::set<void*>& checked_maya_nodes, std::set<std::string>& out_grids);

void check_maya_plug(MPlug plug, std::set<void*>& checked_maya_nodes, std::set<std::string>& out_grids)
{
    MPlugArray conns;
    plug.connectedTo(conns, true, false);
    const unsigned int conns_length = conns.length();
    for (unsigned int i = 0; i < conns_length; ++i)
        check_maya_node(conns[i].node(), checked_maya_nodes, out_grids);
}
// todo check the export arnold nodes, that could be faster!
void check_maya_node(MObject node, std::set<void*>& checked_maya_nodes, std::set<std::string>& out_grids)
{
    MStatus status;
    MFnDependencyNode dnode(node, &status);
    if (!status)
        return;
    void* user_node = dnode.userNode();
    if (checked_maya_nodes.find(user_node) != checked_maya_nodes.end())
        return;
    checked_maya_nodes.insert(dnode.userNode());
    // check if the MFnDependencyNode is something we want
    MString type_name = dnode.typeName();
    if (type_name == "aiVolumeSampleFloat" || type_name == "aiVolumeSampleRgb" || type_name == "vdb_sampler")
    {
        MString channel = dnode.findPlug("channel").asString();
        if (channel.length())
            out_grids.insert(channel.asChar());
    }

    // traverse other connections recursively
    const unsigned int attribute_count = dnode.attributeCount();
    for (unsigned int i = 0; i < attribute_count; ++i)
        check_maya_plug(MPlug(node, dnode.attribute(i)), checked_maya_nodes, out_grids);
}

void OpenvdbTranslator::Export(AtNode* volume)
{
    AiNodeSetStr(volume, "dso", (std::string(getenv("MTOA_PATH")) + std::string("procedurals/volume_openvdb.so")).c_str());

    ExportMatrix(volume, 0);

    AiNodeDeclare(volume, "filename", "constant STRING");
    AiNodeSetStr(volume, "filename", FindMayaPlug("outVdbPath").asString().asChar());

    std::set<std::string> out_grids;
    std::set<void*> checked_maya_nodes;

    MFnDependencyNode dnode(GetMayaObject());
    checked_maya_nodes.insert(dnode.userNode());
    const char* channels_to_check[] = {"scattering_channel", "attenuation_channel", "emission_channel"};
    for (auto channel : channels_to_check)
    {
        MString channel_value = FindMayaPlug(channel).asString();
        if (channel_value.length())
            out_grids.insert(channel_value.asChar());
    }

    const char* parameters_to_check[] = {
        "scattering", "scattering_color", "scattering_intensity",
        "attenuation", "attenuation_color", "attenuation_intensity",
        "emission", "emission_color", "emission_intensity",
        "anisotropy"
    };

    for (auto parameter : parameters_to_check)
    {
        MPlug plug = FindMayaPlug(parameter);
        if (!plug.isNull())
            check_maya_plug(plug, checked_maya_nodes, out_grids);
    }

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

    AtArray* grid_names = AiArrayAllocate(out_grids.size(), 1, AI_TYPE_STRING);

    unsigned int id = 0;
    for (auto out_grid : out_grids)
    {
        AiArraySetStr(grid_names, id, out_grid.c_str());
        ++id;
    }

    AiNodeDeclare(volume, "grids", "constant ARRAY STRING");
    AiNodeSetArray(volume, "grids", grid_names);

    ProcessParameter(volume, "min", AI_TYPE_POINT, "bboxMin");
    ProcessParameter(volume, "max", AI_TYPE_POINT, "bboxMax");

    if (FindMayaPlug("overrideShader").asBool())
    {
        const int instanceNum = m_dagPath.isInstanced() ? m_dagPath.instanceNumber() : 0;

        MPlug shadingGroupPlug = GetNodeShadingGroup(m_dagPath.node(), instanceNum);
        if (!shadingGroupPlug.isNull())
        {
            AtNode* shader = ExportNode(shadingGroupPlug);
            if (shader != 0)
            {
                AiNodeSetPtr(volume, "shader", shader);
                AiNodeDeclare(volume, "mtoa_shading_groups", "constant ARRAY NODE");
                AiNodeSetArray(volume, "mtoa_shading_groups", AiArray(1, 1, AI_TYPE_NODE, shader));
            }
        }
    }
    else
    {
        AtNode* shader = GetArnoldNode("shader");

        AiNodeSetPtr(volume, "shader", shader);

        ExportParams(shader);
    }

    const float sampling_quality = FindMayaPlug("samplingQuality").asFloat();
    const float voxel_size = FindMayaPlug("voxelSize").asFloat();
    AiNodeSetFlt(volume, "step_size", voxel_size / (sampling_quality / 100.0f));
}

void OpenvdbTranslator::ExportMotion(AtNode* volume, unsigned int step)
{
    ExportMatrix(volume, step);
}
