#pragma once

#include <translators/shader/ShaderTranslator.h>

#include "shader_params_translator.h"

class SamplerTranslator : public VDBShaderParamsTranslator<CShaderTranslator>{
public:
    static void* creator();

    virtual AtNode* CreateArnoldNodes();

    virtual void Export(AtNode* volume);
};
