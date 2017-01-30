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
    ExportSimpleParams(shader);
}
