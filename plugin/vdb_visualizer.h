#pragma once

#include <maya/MPxSurfaceShape.h>
#include <maya/MPxSurfaceShapeUI.h>
#include <maya/MBoundingBox.h>

#include <openvdb/openvdb.h>

struct VDBVisualizerData{
    MBoundingBox bbox;
    std::string vdb_path;
    openvdb::io::File* vdb_file;
    int update_trigger;

    VDBVisualizerData();
    ~VDBVisualizerData();

    void clear();
};

class VDBVisualizerShape : public MPxSurfaceShape {
public:
    VDBVisualizerShape();
    ~VDBVisualizerShape();

    static void* creator();

    MStatus compute(const MPlug& plug, MDataBlock& dataBlock);
    static MStatus initialize();

    static const MTypeId typeId;
    static const MString typeName;
    static const MString drawDbClassification;

    static MObject s_vdb_path;
    static MObject s_display_mode;
    static MObject s_update_trigger;

    VDBVisualizerData* get_update();
private:
    VDBVisualizerData m_vdb_data;
};

class VDBVisualizerShapeUI : public MPxSurfaceShapeUI {
public:
    VDBVisualizerShapeUI();
    ~VDBVisualizerShapeUI();

    static void* creator();
};
