#include "openvdb_translator.h"

void* OpenvdbTranslator::creator()
{
    return new OpenvdbTranslator();
}

AtNode* OpenvdbTranslator::CreateArnoldNodes()
{
    AtNode* volume = AddArnoldNode("volume");
    AddArnoldNode("openvdb_shader", "shader");
    return volume;
}

void OpenvdbTranslator::Export(AtNode* volume)
{
    AiNodeSetStr(volume, "dso", (std::string(getenv("MTOA_PATH")) + std::string("procedurals/volume_openvdb.so")).c_str());

    AiNodeDeclare(volume, "filename", "constant STRING");
    AiNodeSetStr(volume, "filename", FindMayaPlug("outVdbPath").asString().asChar());

    MString grid_names = FindMayaPlug("gridNames").asString();
    MStringArray grid_names_array;
    grid_names.split(' ', grid_names_array);
    const unsigned int grid_names_length = grid_names_array.length();
    if (grid_names_length > 0)
    {
        AiNodeDeclare(volume, "grids", "constant ARRAY STRING");

        AtArray* arr = AiArrayAllocate(grid_names_length, 1, AI_TYPE_STRING);

        for (unsigned int i = 0; i < grid_names_length; ++i)
            AiArraySetStr(arr, i, grid_names_array[i].asChar());

        AiNodeSetArray(volume, "grids", arr);
    }

    ProcessParameter(volume, "min", AI_TYPE_POINT, "bboxMin");
    ProcessParameter(volume, "max", AI_TYPE_POINT, "bboxMax");

    AtNode* shader = GetArnoldNode("shader");

    AiNodeSetPtr(volume, "shader", shader);

    ProcessParameter(shader, "scattering_source", AI_TYPE_INT, "scatteringSource");
    ProcessParameter(shader, "scattering", AI_TYPE_RGB, "scattering");
    ProcessParameter(shader, "scattering_channel", AI_TYPE_STRING, "scatteringChannel");
    ProcessParameter(shader, "scattering_color", AI_TYPE_RGB, "scatteringColor");
    ProcessParameter(shader, "scattering_intensity", AI_TYPE_FLOAT, "scatteringIntensity");
    ProcessParameter(shader, "anisotropy", AI_TYPE_FLOAT, "anisotropy");

    ProcessParameter(shader, "attenuation_source", AI_TYPE_INT, "attenuationSource");
    ProcessParameter(shader, "attenuation", AI_TYPE_RGB, "attenuation");
    ProcessParameter(shader, "attenuation_channel", AI_TYPE_STRING, "attenuationChannel");
    ProcessParameter(shader, "attenuation_color", AI_TYPE_RGB, "attenuationColor");
    ProcessParameter(shader, "attenuation_intensity", AI_TYPE_FLOAT, "attenuationIntensity");
    ProcessParameter(shader, "attenuation_mode", AI_TYPE_INT, "attenuationMode");

    ProcessParameter(shader, "emission_source", AI_TYPE_INT, "emissionSource");
    ProcessParameter(shader, "emission", AI_TYPE_RGB, "emission");
    ProcessParameter(shader, "emission_channel", AI_TYPE_STRING, "emissionChannel");
    ProcessParameter(shader, "emission_color", AI_TYPE_RGB, "emissionColor");
    ProcessParameter(shader, "emission_intensity", AI_TYPE_FLOAT, "emissionIntensity");

    ProcessParameter(shader, "interpolation", AI_TYPE_INT, "interpolation");
    ProcessParameter(shader, "compensate_scaling", AI_TYPE_BOOLEAN, "compensateScaling");
}
