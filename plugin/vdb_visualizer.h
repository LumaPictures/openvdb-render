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
    static MObject s_cache_time;
    static MObject s_cache_playback_start;
    static MObject s_cache_playback_end;
    static MObject s_cache_playback_offset;
    static MObject s_cache_before_mode;
    static MObject s_cache_after_mode;
    static MObject s_display_mode;
    static MObject s_update_trigger;
    // mainly for 3rd party renderers
    static MObject s_out_vdb_path;
    static MObject s_grid_names;
    static MObject s_bbox_min;
    static MObject s_bbox_max;

    // shader parameters
    static MObject s_scattering_source;
    static MObject s_scattering;
    static MObject s_scattering_channel;
    static MObject s_scattering_color;
    static MObject s_scattering_intensity;
    static MObject s_anisotropy;
    static MObject s_attenuation_source;
    static MObject s_attenuation;
    static MObject s_attenuation_channel;
    static MObject s_attenuation_color;
    static MObject s_attenuation_intensity;
    static MObject s_attenuation_mode;
    static MObject s_emission_source;
    static MObject s_emission;
    static MObject s_emission_channel;
    static MObject s_emission_color;
    static MObject s_emission_intensity;
    static MObject s_position_offset;
    static MObject s_interpolation;
    static MObject s_compensate_scaling;

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
