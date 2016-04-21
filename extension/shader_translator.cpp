#include "shader_translator.h"

void* VDBShaderTranslator::creator()
{
    return new VDBShaderTranslator();
}

AtNode* VDBShaderTranslator::CreateArnoldNodes()
{
    return AddArnoldNode("openvdb_shader");
}

void VDBShaderTranslator::Export(AtNode* volume)
{
    ExportParams(volume);
}
