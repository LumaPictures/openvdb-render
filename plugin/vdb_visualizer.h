// Copyright 2019 Luma Pictures
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
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
#include "vdb_simple_shader.h"
#include "shader_mode.h"

enum VDBDisplayMode {
    DISPLAY_AXIS_ALIGNED_BBOX = 0,
    DISPLAY_GRID_BBOX,
    DISPLAY_POINT_CLOUD,
    DISPLAY_NON_SHADED,
    DISPLAY_SHADED,
    DISPLAY_MESH,
    DISPLAY_SLICED
};

enum VDBPointSort {
    POINT_SORT_DISABLED = 0,
    POINT_SORT_CPU,
    POINT_SORT_GPU_CPU,
    POINT_SORT_GPU,
    POINT_SORT_DEFAULT = POINT_SORT_CPU
};

// Data for DISPLAY_SLICED mode.

enum class VDBChannelSource {
    VALUE = 0,
    RAMP = 1
};

enum class VDBEmissionMode {
    NONE = 0,
    CHANNEL = 1,
    DENSITY = 2,
    BLACKBODY = 3,
};

template <typename T>
struct RampData {
    std::vector<T> samples;
    float input_min = -1;
    float input_max = -1;
    RampData() = default;
};

struct VDBSlicedDisplayData {

    // Shader data; the shader approximates aiStandardVolume shader.
    // The visualization also supports ramps.
    float density;
    std::string density_channel;
    RampData<float> density_ramp;
    VDBChannelSource density_source;

    float scatter;
    MFloatVector scatter_color;
    std::string scatter_color_channel;
    RampData<MFloatVector> scatter_color_ramp;
    VDBChannelSource scatter_color_source;
    float scatter_anisotropy;

    MFloatVector transparent;
    std::string transparent_channel;

    VDBEmissionMode emission_mode;
    float emission;
    MFloatVector emission_color;
    std::string emission_channel;
    RampData<MFloatVector> emission_ramp;
    VDBChannelSource emission_source;

    float temperature;
    std::string temperature_channel;
    float blackbody_kelvin;
    float blackbody_intensity;

    // Additional visualization data.
    int   slice_count;
    int   shadow_sample_count;
    float shadow_gain;

    VDBSlicedDisplayData();
};

struct RampParams {
    MObject ramp;
    MObject input_min;
    MObject input_max;
};

struct VDBSlicedDisplayParams {
    MObject density;
    MObject density_channel;
    RampParams density_ramp;
    MObject density_source;
    MObject scatter;
    MObject scatter_color;
    MObject scatter_color_channel;
    RampParams scatter_color_ramp;
    MObject scatter_color_source;
    MObject scatter_anisotropy;
    MObject transparent;
    MObject transparent_channel;
    MObject emission_mode;
    MObject emission;
    MObject emission_color;
    MObject emission_channel;
    RampParams emission_ramp;
    MObject emission_source;
    MObject temperature;
    MObject temperature_channel;
    MObject blackbody_kelvin;
    MObject blackbody_intensity;
    MObject slice_count;
    MObject shadow_sample_count;
    MObject shadow_gain;
};

struct VDBVisualizerData {
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
    VDBShaderMode shader_mode;

    VDBSlicedDisplayData sliced_display_data;

    VDBVisualizerData();
    VDBVisualizerData(const VDBVisualizerData&) = delete;
    VDBVisualizerData(VDBVisualizerData&&) = delete;
    VDBVisualizerData& operator=(const VDBVisualizerData&) = delete;
    VDBVisualizerData& operator=(VDBVisualizerData&&) = delete;

    ~VDBVisualizerData();

    void clear(const MBoundingBox& bb = MBoundingBox());
};

class VDBVisualizerShapeUI : public MPxSurfaceShapeUI {
private:
    VDBVisualizerShapeUI() = default;
public:
    VDBVisualizerShapeUI(const VDBVisualizerShapeUI&) = delete;
    VDBVisualizerShapeUI(VDBVisualizerShapeUI&&) = delete;
    VDBVisualizerShapeUI& operator=(const VDBVisualizerShapeUI&) = delete;
    VDBVisualizerShapeUI& operator=(VDBVisualizerShapeUI&&) = delete;

    ~VDBVisualizerShapeUI() override = default;

    static void* creator();

    bool select(
        MSelectInfo& selectInfo,
        MSelectionList& selectionList,
        MPointArray& worldSpaceSelectPts) const override;

    bool canDrawUV() const override;
};

class VDBVisualizerShape : public MPxSurfaceShape {
private:
    VDBVisualizerShape() = default;
public:
    VDBVisualizerShape(const VDBVisualizerShape&) = delete;
    VDBVisualizerShape(VDBVisualizerShape&&) = delete;
    VDBVisualizerShape& operator=(const VDBVisualizerShape&) = delete;
    VDBVisualizerShape& operator=(VDBVisualizerShape&&) = delete;

    ~VDBVisualizerShape() override;

    static void* creator();

    bool isBounded() const override;

    MBoundingBox boundingBox() const override;

    MStatus compute(const MPlug& plug, MDataBlock& dataBlock) override;

    static MStatus initialize();

    void postConstructor() override;

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
    static MObject s_visible_in_diffuse;
    static MObject s_visible_in_glossy;
    static MObject s_visible_in_subsurface;
    static MObject s_self_shadows;

    // display parameters
    static MObject s_point_size;
    static MObject s_point_jitter;
    static MObject s_point_skip;
    static MObject s_point_sort;
    static VDBSlicedDisplayParams s_sliced_display_params;

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
    static VDBSimpleShaderParams s_simple_shader_params;

    static const boost::regex s_frame_expr;
    static const boost::regex s_hash_expr;

    VDBVisualizerData* get_update();

private:

    VDBVisualizerData m_vdb_data;
    MCallbackId m_time_changed_id;
};
