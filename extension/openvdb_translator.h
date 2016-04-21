#pragma once

#include <translators/shape/ShapeTranslator.h>

#include "shader_translator.h"

class OpenvdbTranslator : public VDBShaderParamsTranslator<CShapeTranslator> {
public:
    static void* creator();

    virtual AtNode* CreateArnoldNodes();
    virtual void Export(AtNode* volume);
    virtual void ExportMotion(AtNode* volume, unsigned int step);
};
