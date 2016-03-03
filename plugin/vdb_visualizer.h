#pragma once

#include <maya/MPxSurfaceShape.h>
#include <maya/MPxSurfaceShapeUI.h>

class VDBVisualizerShape : public MPxSurfaceShape {
public:
    VDBVisualizerShape();
    ~VDBVisualizerShape();

    static void* creator();

    static MStatus initialize();

    static const MTypeId typeId;
    static const MString typeName;
    static const MString drawDbClassification;

    static MObject s_vdbPath;
    static MObject s_displayMode;
    static MObject s_updateTrigger;
};

class VDBVisualizerShapeUI : public MPxSurfaceShapeUI {
public:
    VDBVisualizerShapeUI();
    ~VDBVisualizerShapeUI();

    static void* creator();
};
