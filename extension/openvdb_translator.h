#pragma once

#include <translators/shape/ShapeTranslator.h>

class OpenvdbTranslator : public CShapeTranslator {
public:
    static void* creator();

    virtual AtNode* CreateArnoldNodes();
    virtual void Export(AtNode* volume);
    virtual void ExportMotion(AtNode* volume, unsigned int step);
};
