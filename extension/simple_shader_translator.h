#pragma once

#include "shader_params_translator.h"

class VDBSimpleShaderTranslator : public VDBShaderParamsTranslator<CShaderTranslator> {
public:
    static void* creator();

    virtual AtNode* CreateArnoldNodes();

    virtual void Export(AtNode* shader);
};
