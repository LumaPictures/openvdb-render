#pragma once

#include <translators/shape/ShapeTranslator.h>

#include "shader_translator.h"

class OpenvdbTranslator : public VDBShaderParamsTranslator<CShapeTranslator> {
public:
    static void* creator();

    virtual AtNode* CreateArnoldNodes();

    virtual void Export(AtNode* volume) override;

#if MTOA12
    virtual void ExportMotion(AtNode* volume, unsigned int step) override;
#elif MTOA14
    virtual void ExportMotion(AtNode* volume) override;
#endif

};
