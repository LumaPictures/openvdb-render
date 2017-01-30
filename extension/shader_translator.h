#pragma once

#include "shader_params_translator.h"

// the process parameter functions are protected member functions...
// so need to do some tricks to not to write this several times



class VDBShaderTranslator : public VDBShaderParamsTranslator<CShaderTranslator> {
public:
    static void* creator();

    virtual AtNode* CreateArnoldNodes();

    virtual void Export(AtNode* volume);
};
