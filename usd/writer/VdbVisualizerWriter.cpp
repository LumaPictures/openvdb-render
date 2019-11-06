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
#include "VdbVisualizerWriter.h"

#include "usdMaya/writeUtil.h"
#include "pxr/usd/usdAi/aiVolume.h"
#include "pxr/usd/usdAi/aiNodeAPI.h"
#include "pxr/usd/usdAi/aiShapeAPI.h"
#include "pxr/usd/sdf/types.h"
#include "pxr/usd/usd/stage.h"

#include <maya/MDataHandle.h>

#include <type_traits>

PXR_NAMESPACE_OPEN_SCOPE

namespace {
    const TfToken filename_token("filename");
    const TfToken velocity_grids_token("velocity_grids");
    const TfToken velocity_scale_token("velocity_scale");
    const TfToken velocity_fps_token("velocity_fps");
    const TfToken velocity_shutter_start_token("velocity_shutter_start");
    const TfToken velocity_shutter_end_token("velocity_shutter_end");
    const TfToken bounds_slack_token("bounds_slack");

    UsdAttribute get_attribute(UsdPrim& prim, UsdAiNodeAPI& api, const TfToken& attr_name, const SdfValueTypeName& type) {
        if (prim.HasAttribute(attr_name)) {
            return prim.GetAttribute(attr_name);
        } else {
            return api.CreateUserAttribute(attr_name, type);
        }
    }

    UsdAttribute get_attribute(UsdPrim& prim, const TfToken& attr_name) {
        if (prim.HasAttribute(attr_name)) {
            return prim.GetAttribute(attr_name);
        } else {
            return UsdAttribute();
        }
    }

    bool export_grids(UsdPrim& prim, UsdAiNodeAPI& api, const MFnDependencyNode& node, const char* maya_attr_name, const TfToken& usd_attr_name) {
        const auto grids_string = node.findPlug(maya_attr_name).asString();
        MStringArray grids;
        grids_string.split(' ', grids);
        const auto grids_length = grids.length();
        if (grids_length > 0) {
            VtStringArray grid_names;
            grid_names.reserve(grids_length);
            for (std::remove_const<decltype(grids_length)>::type i = 0; i < grids_length; ++i) {
                grid_names.push_back(grids[i].asChar());
            }
            get_attribute(prim, api, usd_attr_name, SdfValueTypeNames->StringArray)
                .Set(grid_names);
            return true;
        } else {
            return false;
        }
    }

    void CleanupAttributeKeys(UsdAttribute attribute, UsdInterpolationType parameterInterpolation = UsdInterpolationTypeLinear) {
        if (!attribute) { return; }
        // without the thread_local it'll get reallocated all the time
        static thread_local std::vector<double> time_samples;
        time_samples.clear();
        attribute.GetTimeSamples(&time_samples);
        const auto num_time_samples = time_samples.size();
        if (parameterInterpolation == UsdInterpolationTypeHeld) {
            if (num_time_samples < 2) { return; }
            const auto max_num_time_samples = num_time_samples - 1;
            VtValue first;
            attribute.Get(&first, time_samples[0]);
            for (size_t i = 1; i < max_num_time_samples; ++i) {
                VtValue next;
                const auto next_time = time_samples[i];
                attribute.Get(&next, next_time);
                if (first.operator==(next)) {
                    attribute.ClearAtTime(next_time);
                } else {
                    first = next;
                }
            }
        } else {
            if (num_time_samples < 3) { return; }
            const auto max_num_time_samples = num_time_samples - 1;
            VtValue first;
            attribute.Get(&first, time_samples[0]);
            for (size_t i = 1; i < max_num_time_samples; ++i) {
                VtValue middle, last;
                const auto middle_time = time_samples[i];
                attribute.Get(&middle, middle_time);
                attribute.Get(&last, time_samples[i + 1]);
                // not the best one, we could do bigger jumps, but this is cleaner code wise
                if (first.operator==(middle) && first.operator==(last)) {
                    attribute.ClearAtTime(middle_time);
                } else {
                    first = middle;
                }
            }
        }
    }
}

VdbVisualizerWriter::VdbVisualizerWriter(const MFnDependencyNode& depNodeFn, const SdfPath& uPath, UsdMayaWriteJobContext& jobCtx) :
    UsdMayaTransformWriter(depNodeFn, uPath, jobCtx), has_velocity_grids(false) {
    UsdAiVolume primSchema =
        UsdAiVolume::Define(GetUsdStage(), GetUsdPath());
    TF_AXIOM(primSchema);
    _usdPrim = primSchema.GetPrim();
    TF_AXIOM(_usdPrim);
}

VdbVisualizerWriter::~VdbVisualizerWriter() {
}

void VdbVisualizerWriter::PostExport() {
    UsdAiVolume primSchema(_usdPrim);
    UsdAiNodeAPI nodeApi(primSchema);
    UsdAiShapeAPI shapeApi(primSchema);
    CleanupAttributeKeys(primSchema.GetStepSizeAttr());
    CleanupAttributeKeys(shapeApi.GetAiMatteAttr(), UsdInterpolationTypeHeld);
    CleanupAttributeKeys(shapeApi.GetAiReceiveShadowsAttr(), UsdInterpolationTypeHeld);
    CleanupAttributeKeys(shapeApi.GetAiSelfShadowsAttr(), UsdInterpolationTypeHeld);
    CleanupAttributeKeys(nodeApi.GetUserAttribute(filename_token), UsdInterpolationTypeHeld);
    CleanupAttributeKeys(nodeApi.GetUserAttribute(velocity_scale_token));
    CleanupAttributeKeys(nodeApi.GetUserAttribute(velocity_fps_token));
    CleanupAttributeKeys(nodeApi.GetUserAttribute(velocity_shutter_start_token));
    CleanupAttributeKeys(nodeApi.GetUserAttribute(velocity_shutter_end_token));
    CleanupAttributeKeys(nodeApi.GetUserAttribute(bounds_slack_token));
}

void VdbVisualizerWriter::Write(const UsdTimeCode& usdTime) {
    UsdAiVolume primSchema(_usdPrim);
    UsdAiNodeAPI nodeApi(primSchema);
    UsdAiShapeAPI shapeApi(primSchema);
    UsdMayaTransformWriter::Write(usdTime);

    const MFnDependencyNode volume_node(GetDagPath().node());

    // some of the attributes that don't need to be animated has to be exported here
    if (usdTime.IsDefault()) {
        has_velocity_grids = export_grids(_usdPrim, nodeApi, volume_node, "velocity_grids", velocity_grids_token);
    }

    if (usdTime.IsDefault() != _GetExportArgs().timeSamples.empty()) {
        return;
    }

    // The node regenerates all kinds of params, so we always need to write these out.
    const auto out_vdb_path = volume_node.findPlug("outVdbPath").asString();
    const auto& bbox_min = volume_node.findPlug("bboxMin").asMDataHandle().asFloat3();
    const auto& bbox_max = volume_node.findPlug("bboxMax").asMDataHandle().asFloat3();
    VtVec3fArray extents(2);
    extents[0] = GfVec3f(bbox_min[0], bbox_min[1], bbox_min[2]);
    extents[1] = GfVec3f(bbox_max[0], bbox_max[1], bbox_max[2]);
    primSchema.CreateExtentAttr().Set(extents, usdTime);

    const auto sampling_quality = volume_node.findPlug("samplingQuality").asFloat();
    primSchema.CreateStepSizeAttr().Set(volume_node.findPlug("voxelSize").asFloat() / (sampling_quality / 100.0f), usdTime);
    shapeApi.CreateAiMatteAttr().Set(volume_node.findPlug("matte").asBool(), usdTime);
    shapeApi.CreateAiReceiveShadowsAttr().Set(volume_node.findPlug("receiveShadows").asBool(), usdTime);
    shapeApi.CreateAiSelfShadowsAttr().Set(volume_node.findPlug("selfShadows").asBool(), usdTime);
    primSchema.CreateFilenameAttr().Set(SdfAssetPath(std::string(out_vdb_path.asChar())), usdTime);

    if (has_velocity_grids) {
        get_attribute(_usdPrim, nodeApi, velocity_scale_token, SdfValueTypeNames->Float)
            .Set(volume_node.findPlug("velocityScale").asFloat(), usdTime);
        get_attribute(_usdPrim, nodeApi, velocity_fps_token, SdfValueTypeNames->Float)
            .Set(volume_node.findPlug("velocityFps").asFloat(), usdTime);
        get_attribute(_usdPrim, nodeApi, velocity_shutter_start_token, SdfValueTypeNames->Float)
            .Set(volume_node.findPlug("velocityShutterStart").asFloat(), usdTime);
        get_attribute(_usdPrim, nodeApi, velocity_shutter_end_token, SdfValueTypeNames->Float)
            .Set(volume_node.findPlug("velocityShutterEnd").asFloat(), usdTime);
    }

    get_attribute(_usdPrim, nodeApi, bounds_slack_token, SdfValueTypeNames->Float)
        .Set(volume_node.findPlug("boundsSlack").asFloat());
}

PXR_NAMESPACE_CLOSE_SCOPE
