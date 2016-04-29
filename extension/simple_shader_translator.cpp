#include "simple_shader_translator.h"

void* VDBSimpleShaderTranslator::creator()
{
    return new VDBSimpleShaderTranslator();
}

AtNode* VDBSimpleShaderTranslator::CreateArnoldNodes()
{
    return AddArnoldNode("openvdb_simple_shader");
}

void VDBSimpleShaderTranslator::Export(AtNode* shader)
{
    ProcessParameter(shader, "smoke", AI_TYPE_RGB, "smoke");
    ProcessParameter(shader, "smoke_channel", AI_TYPE_STRING, "smokeChannel");
    ProcessParameter(shader, "smoke_intensity", AI_TYPE_FLOAT, "smokeIntensity");
    ProcessParameter(shader, "anisotropy", AI_TYPE_FLOAT, "anisotropy");

    ProcessParameter(shader, "opacity", AI_TYPE_RGB, "opacity");
    ProcessParameter(shader, "opacity_channel", AI_TYPE_STRING, "opacityChannel");
    ProcessParameter(shader, "opacity_intensity", AI_TYPE_FLOAT, "opacityIntensity");
    ProcessParameter(shader, "opacity_shadow", AI_TYPE_RGB, "opacityShadow");

    ProcessParameter(shader, "fire", AI_TYPE_RGB, "fire");
    ProcessParameter(shader, "fire_channel", AI_TYPE_STRING, "fireChannel");
    ProcessParameter(shader, "fire_intensity", AI_TYPE_FLOAT, "fireIntensity");

    ProcessParameter(shader, "position_offset", AI_TYPE_VECTOR, "positionOffset");
    ProcessParameter(shader, "interpolation", AI_TYPE_INT, "interpolation");
    ProcessParameter(shader, "compensate_scaling", AI_TYPE_BOOLEAN, "compensateScaling");
}
