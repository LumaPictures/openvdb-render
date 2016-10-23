#pragma once

#include <maya/MPxSurfaceShape.h>
#include <maya/MPxSurfaceShapeUI.h>
#include <maya/MBoundingBox.h>

#include <openvdb/openvdb.h>
// std regex in gcc 4.8.3 is broken
#include <boost/regex.hpp>
#include <maya/MNodeMessage.h>
#include <maya/MDGMessage.h>

#include "vdb_sampler.h"
#include "gradient.hpp"
#include "vdb_shader.h"
#include "vdb_simple_shader.h"

enum VDBDisplayMode{
    DISPLAY_AXIS_ALIGNED_BBOX = 0,
    DISPLAY_GRID_BBOX,
    DISPLAY_POINT_CLOUD,
    DISPLAY_NON_SHADED,
    DISPLAY_SHADED,
    DISPLAY_MESH
};

struct VDBVisualizerData{
    MBoundingBox bbox;

    MFloatVector scattering_color;
    MFloatVector attenuation_color;
    MFloatVector emission_color;

    std::string vdb_path;
    std::string attenuation_channel;
    std::string scattering_channel;
    std::string emission_channel;

    Gradient scattering_gradient;
    Gradient attenuation_gradient;
    Gradient emission_gradient;

    openvdb::io::File* vdb_file;

    float point_size;
    float point_jitter;

    int point_skip;
    int update_trigger;
    VDBDisplayMode display_mode;

    VDBVisualizerData();
    ~VDBVisualizerData();

    void clear(const MBoundingBox& bb = MBoundingBox());
};

class VDBVisualizerShapeUI : public MPxSurfaceShapeUI {
public:
    VDBVisualizerShapeUI();
    ~VDBVisualizerShapeUI();

    static void* creator();

    virtual bool select(
            MSelectInfo& selectInfo,
            MSelectionList& selectionList,
            MPointArray& worldSpaceSelectPts) const;

    virtual bool canDrawUV() const;
};

class VDBVisualizerShape : public MPxSurfaceShape {
public:
    VDBVisualizerShape();
    ~VDBVisualizerShape();

    static void* creator();

    virtual bool isBounded() const;
    virtual MBoundingBox boundingBox() const;

    MStatus compute(const MPlug& plug, MDataBlock& dataBlock);
    static MStatus initialize();
    void postConstructor();

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
    static MObject s_channel_stats;
    static MObject s_voxel_size;
    static MObject s_matte;

    // display parameters
    static MObject s_point_size;
    static MObject s_point_jitter;
    static MObject s_point_skip;

    static MObject s_override_shader;
    static MObject s_sampling_quality;
    static MObject s_additional_channel_export;

    // Velocity params
    static MObject s_velocity_grids;
    static MObject s_velocity_scale;
    static MObject s_velocity_fps;
    static MObject s_velocity_shutter_start;
    static MObject s_velocity_shutter_end;

    // disp params
    static MObject s_bounds_slack;

    // shader parameters
    static MObject s_shader_mode;
    static VDBShaderParams s_shader_params;
    static VDBSimpleShaderParams s_simple_shader_params;

    static const boost::regex s_frame_expr;
    static const boost::regex s_hash_expr;

    VDBVisualizerData* get_update();
private:

    VDBVisualizerData m_vdb_data;
    MCallbackId m_time_changed_id;
};
