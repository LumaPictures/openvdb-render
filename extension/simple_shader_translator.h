#pragma once

#include <translators/shader/ShaderTranslator.h>

class VDBSimpleShaderTranslator : public CShaderTranslator {
public:
    static void* creator();

    virtual AtNode* CreateArnoldNodes();

    virtual void Export(AtNode* shader);
};
