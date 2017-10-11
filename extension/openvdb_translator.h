#pragma once

#include <translators/shape/ShapeTranslator.h>

#include "shader_params_translator.h"

class OpenvdbTranslator : public VDBShaderParamsTranslator<CShapeTranslator> {
public:
    static void* creator();

    AtNode* CreateArnoldNodes() override;

    void Export(AtNode* volume) override;

#if MTOA12
    virtual void ExportMotion(AtNode* volume, unsigned int step) override;
#else
    void ExportMotion(AtNode* volume) override;
#endif

};
