#pragma once

#include <translators/shader/ShaderTranslator.h>

class SamplerTranslator : public CShaderTranslator {
public:
    static void* creator();

    virtual AtNode* CreateArnoldNodes();

    virtual void Export(AtNode* volume);
};
