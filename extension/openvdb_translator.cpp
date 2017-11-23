#include "openvdb_translator.h"

#include <set>
#include <functional>
#include <array>

#include "shader_params_translator.h"

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
#ifndef ARNOLD5
            case AI_TYPE_POINT:
#endif
                iterate_param_elems<vec_elems.size()>(node, param_name, vec_elems, func);
                break;
#ifdef ARNOLD5
            case AI_TYPE_VECTOR2:
#else
            case AI_TYPE_POINT2:
#endif
                iterate_param_elems<vec2_elems.size()>(node, param_name, vec2_elems, func);
                break;
            default:
                return;
        }
    }


    inline
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
            auto ch = AiNodeGetStr(node, channel)
#ifdef ARNOLD5
            .c_str()
#endif
            ;
            if (ch != nullptr && ch[0] != '\0') {
                out_grids.insert(std::string(ch));
            }
        };

        const auto* node_entry = AiNodeGetNodeEntry(node);
        auto* param_iter = AiNodeEntryGetParamIterator(node_entry);
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
        AiParamIteratorDestroy(param_iter);
    }

    inline
    bool is_instance(const AtNode* node) {
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
        AtNode* volume = AddArnoldNode("volume");
#ifndef ARNOLD5
        if (!FindMayaPlug("overrideShader").asBool()) {
            AddArnoldNode("openvdb_simple_shader", "shader");
        }
#else
        AddArnoldNode("standard_volume", "shader");
#endif
        return volume;
    } else {
        return AddArnoldNode("ginstance");
    }
}

#ifndef ARNOLD5
void OpenvdbTranslator::Export(AtNode* volume)
{
#if MTOA12
    ExportMatrix(volume, 0);
#else
    ExportMatrix(volume);
#endif

    AiNodeSetBool(volume, "matte", FindMayaPlug("matte").asBool());

    AiNodeSetBool(volume, "receive_shadows", FindMayaPlug("receiveShadows").asBool());

    AiNodeSetByte(volume, "visibility", ComputeVisibility());

    ExportLightLinking(volume);

    AtByte visibility = 0;
    if (FindMayaPlug("primaryVisibility").asBool()) {
        visibility |= AI_RAY_CAMERA;
    }
    if (FindMayaPlug("castsShadows").asBool()) {
        visibility |= AI_RAY_SHADOW;
    }
#ifdef ARNOLD5
    if (FindMayaPlug("aiVisibleInDiffuseReflection").asBool()) {
        visibility |= AI_RAY_DIFFUSE_REFLECT;
    }

    if (FindMayaPlug("aiVisibleInSpecularReflection").asBool()) {
        visibility |= AI_RAY_SPECULAR_REFLECT;
    }

    if (FindMayaPlug("aiVisibleInDiffuseTransmission").asBool()) {
        visibility |= AI_RAY_DIFFUSE_TRANSMIT;
    }

    if (FindMayaPlug("aiVisibleInSpecularTransmission").asBool()) {
        visibility &= ~(AI_RAY_SPECULAR_TRANSMIT);
    }
#else
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
#endif

    AiNodeSetByte(volume, "visibility", visibility);
    AiNodeSetBool(volume, "self_shadows", FindMayaPlug("selfShadows").asBool());

    if (is_instance(volume)) {
        const auto master_instance = GetMasterInstance();
        auto* master_node = AiNodeLookUpByName(master_instance.partialPathName().asChar());
        if (master_node == nullptr) { return; }
        AiNodeSetPtr(volume, "node", master_node);
        AiNodeSetBool(volume, "inherit_xform", false);
        return;
    }

#ifndef ARNOLD5
    const char* mtoa_path = getenv("MTOA_PATH");
    if (!mtoa_path)
        mtoa_path = getenv("MTOA_ROOT");
    if (!mtoa_path)
        return;
    AiNodeSetStr(volume, "dso",
                 (std::string(mtoa_path) + std::string("procedurals/volume_openvdb.so")).c_str());
#endif

#ifndef ARNOLD5
    AiNodeDeclare(volume, "filename", "constant STRING");
    AiNodeDeclare(volume, "grids", "constant ARRAY STRING");

    ProcessParameter(volume, "min", AI_TYPE_POINT, "bboxMin");
    ProcessParameter(volume, "max", AI_TYPE_POINT, "bboxMax");
#endif

    AiNodeSetStr(volume, "filename", FindMayaPlug("outVdbPath").asString().asChar());

    AtNode* shader = nullptr;

    if (FindMayaPlug("overrideShader").asBool()) {
        const int instance_num = m_dagPath.isInstanced() ? m_dagPath.instanceNumber() : 0;

        MPlug shading_group_plug = GetNodeShadingGroup(m_dagPath.node(), instance_num);
        if (!shading_group_plug.isNull()) {
#if MTOA12
            shader = ExportNode(shading_group_plug);
#else
            shader = ExportConnectedNode(shading_group_plug);
#endif
            if (shader != nullptr) {
                AiNodeSetPtr(volume, "shader", shader);
            }
        }
    } else {
        shader = GetArnoldNode("shader");
        AiNodeSetPtr(volume, "shader", shader);
        ExportSimpleParams(shader);
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

    MString velocity_grids_string = FindMayaPlug("velocity_grids").asString();
    MStringArray velocity_grids;
    velocity_grids_string.split(' ', velocity_grids);
    const unsigned int velocity_grids_count = velocity_grids.length();
    if (velocity_grids_count > 0) {
#ifndef ARNOLD5
AiNodeDeclare(volume, "velocity_grids", "constant ARRAY STRING");
AiNodeDeclare(volume, "velocity_scale", "constant FLOAT");
AiNodeDeclare(volume, "velocity_fps", "constant FLOAT");
AiNodeDeclare(volume, "velocity_shutter_start", "constant FLOAT");
AiNodeDeclare(volume, "velocity_shutter_end", "constant FLOAT");
#endif
        AtArray* velocity_grid_names = AiArrayAllocate(velocity_grids_count, 1, AI_TYPE_STRING);
        for (unsigned int i = 0; i < velocity_grids_count; ++i)
            AiArraySetStr(velocity_grid_names, i, velocity_grids[i].asChar());
        AiNodeSetArray(volume, "velocity_grids", velocity_grid_names);
        AiNodeSetFlt(volume, "velocity_scale", FindMayaPlug("velocityScale").asFloat());
        AiNodeSetFlt(volume, "velocity_fps", FindMayaPlug("velocityFps").asFloat());
        AiNodeSetFlt(volume, "velocity_shutter_start", FindMayaPlug("velocityShutterStart").asFloat());        
        AiNodeSetFlt(volume, "velocity_shutter_end", FindMayaPlug("velocityShutterEnd").asFloat());
    }

#ifdef ARNOLD5
    AiNodeSetFlt(volume, "volume_padding", FindMayaPlug("boundsSlack").asFloat());
#else
    AiNodeDeclare(volume, "bounds_slack", "constant FLOAT");
    AiNodeSetFlt(volume, "bounds_slack", FindMayaPlug("boundsSlack").asFloat());
#endif

    const float sampling_quality = FindMayaPlug("samplingQuality").asFloat();
    const float voxel_size = FindMayaPlug("voxelSize").asFloat();
    AiNodeSetFlt(volume, "step_size", voxel_size / (sampling_quality / 100.0f));
}
#else
// Export an Arnold 5 volume node and standard_volume shader.
void OpenvdbTranslator::Export(AtNode* volume)
{
    ExportMatrix(volume);
    AiNodeSetStr(volume, "filename", FindMayaPlug("outVdbPath").asString().asChar());

    ProcessParameter(volume, "min", AI_TYPE_VECTOR, "bboxMin");
    ProcessParameter(volume, "max", AI_TYPE_VECTOR, "bboxMax");

    // Grids.
    std::set<std::string> out_grids;
    const auto addIfNotEmpty = [&out_grids](const std::string& grid_name) {
        if (!grid_name.empty())
            out_grids.insert(grid_name);
    };
    addIfNotEmpty(FindMayaPlug("svDensityChannel").asString().asChar());
    addIfNotEmpty(FindMayaPlug("svScatterColorChannel").asString().asChar());
    addIfNotEmpty(FindMayaPlug("svTransparentChannel").asString().asChar());
    addIfNotEmpty(FindMayaPlug("svEmissionChannel").asString().asChar());
    addIfNotEmpty(FindMayaPlug("svTemperatureChannel").asString().asChar());
    AtArray* grid_names = AiArrayAllocate(static_cast<unsigned int>(out_grids.size()), 1, AI_TYPE_STRING);
    unsigned int id = 0;
    for (const auto& out_grid : out_grids) {
        AiArraySetStr(grid_names, id, out_grid.c_str());
        ++id;
    }
    AiNodeSetArray(volume, "grids", grid_names);

    // Velocity grids.
    MString velocity_grids_string = FindMayaPlug("velocity_grids").asString();
    MStringArray velocity_grids;
    velocity_grids_string.split(' ', velocity_grids);
    const unsigned int velocity_grids_count = velocity_grids.length();
    if (velocity_grids_count > 0) {
        AtArray* velocity_grid_names = AiArrayAllocate(velocity_grids_count, 1, AI_TYPE_STRING);
        for (unsigned int i = 0; i < velocity_grids_count; ++i)
            AiArraySetStr(velocity_grid_names, i, velocity_grids[i].asChar());
        AiNodeSetArray(volume, "velocity_grids", velocity_grid_names);
        AiNodeSetFlt(volume, "velocity_scale", FindMayaPlug("velocityScale").asFloat());
        AiNodeSetFlt(volume, "velocity_fps", FindMayaPlug("velocityFps").asFloat());
        AiNodeSetFlt(volume, "motion_start", FindMayaPlug("velocityShutterStart").asFloat());
        AiNodeSetFlt(volume, "motion_end", FindMayaPlug("velocityShutterEnd").asFloat());
    }

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
    if (FindMayaPlug("visibleInDiffuseTransmissions").asBool()) {
        visibility |= AI_RAY_DIFFUSE_TRANSMIT;
    }
    if (FindMayaPlug("visibleInSpecularTransmissions").asBool()) {
        visibility |= AI_RAY_SPECULAR_TRANSMIT;
    }
    if (FindMayaPlug("visibleInVolumes").asBool()) {
        visibility |= AI_RAY_VOLUME;
    }
    if (FindMayaPlug("visibleInDiffuseReflections").asBool()) {
        visibility |= AI_RAY_DIFFUSE_REFLECT;
    }
    if (FindMayaPlug("visibleInSpecularReflections").asBool()) {
        visibility |= AI_RAY_SPECULAR_REFLECT;
    }
    if (FindMayaPlug("visibleInSubsurface").asBool()) {
        visibility |= AI_RAY_SUBSURFACE;
    }
    AiNodeSetByte(volume, "visibility", visibility);
    AiNodeSetBool(volume, "self_shadows", FindMayaPlug("selfShadows").asBool());

    // Export standard volume shader.
    AtNode* shader = GetArnoldNode("shader");
    AiNodeSetPtr(volume, "shader", shader);

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
#endif

#ifdef MTOA12
void OpenvdbTranslator::ExportMotion(AtNode* volume, unsigned int step)
{
    ExportMatrix(volume, step);
}
#else
void OpenvdbTranslator::ExportMotion(AtNode* volume)
{
    ExportMatrix(volume);
}
#endif
