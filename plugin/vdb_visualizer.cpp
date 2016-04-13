/*
 * The update trigger is used to trigger updates for viewport visualization
 * if it's changed that means something have changed, and the viewport data has to be reloaded.
 *
 * This is done, so we only update the node if we are interactively editing the node, also
 * so we can completely separate reading the data from the main node, all it does it's just loading
 * a vdb dataset and reading information about the contained channels, but not loading actual voxel data.
 *
 * TODO
 * * Check if the metadata reads are cached by the file or not, if not then cache it manually
 */

#include "vdb_visualizer.h"

#include <lumaNodeId.h>

#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnUnitAttribute.h>
#include <maya/MRampAttribute.h>
#include <maya/MTime.h>
#include <maya/MPointArray.h>
#include <maya/MSelectionList.h>
#include <maya/MDagPath.h>

#include <boost/regex.hpp>

#include <sstream>
#include <maya/MFnDependencyNode.h>

#include "../util/maya_utils.hpp"

const MTypeId VDBVisualizerShape::typeId(ID_VDB_VISUALIZER);
const MString VDBVisualizerShape::typeName("vdb_visualizer");
const MString VDBVisualizerShape::drawDbClassification("drawdb/geometry/fractal/vdb_visualizer");

MObject VDBVisualizerShape::s_update_trigger;
MObject VDBVisualizerShape::s_vdb_path;
MObject VDBVisualizerShape::s_cache_time;
MObject VDBVisualizerShape::s_cache_playback_start;
MObject VDBVisualizerShape::s_cache_playback_end;
MObject VDBVisualizerShape::s_cache_playback_offset;
MObject VDBVisualizerShape::s_cache_before_mode;
MObject VDBVisualizerShape::s_cache_after_mode;
MObject VDBVisualizerShape::s_display_mode;
MObject VDBVisualizerShape::s_out_vdb_path;
MObject VDBVisualizerShape::s_grid_names;
MObject VDBVisualizerShape::s_bbox_min;
MObject VDBVisualizerShape::s_bbox_max;
MObject VDBVisualizerShape::s_channel_stats;
MObject VDBVisualizerShape::s_voxel_size;

MObject VDBVisualizerShape::s_scattering_source;
MObject VDBVisualizerShape::s_scattering;
MObject VDBVisualizerShape::s_scattering_channel;
MObject VDBVisualizerShape::s_scattering_color;
MObject VDBVisualizerShape::s_scattering_intensity;
MObject VDBVisualizerShape::s_anisotropy;
MObject VDBVisualizerShape::s_attenuation_source;
MObject VDBVisualizerShape::s_attenuation;
MObject VDBVisualizerShape::s_attenuation_channel;
MObject VDBVisualizerShape::s_attenuation_color;
MObject VDBVisualizerShape::s_attenuation_intensity;
MObject VDBVisualizerShape::s_attenuation_mode;
MObject VDBVisualizerShape::s_emission_source;
MObject VDBVisualizerShape::s_emission;
MObject VDBVisualizerShape::s_emission_channel;
MObject VDBVisualizerShape::s_emission_color;
MObject VDBVisualizerShape::s_emission_intensity;
MObject VDBVisualizerShape::s_position_offset;
MObject VDBVisualizerShape::s_interpolation;
MObject VDBVisualizerShape::s_compensate_scaling;
MObject VDBVisualizerShape::s_additional_channel_export;
MObject VDBVisualizerShape::s_sampling_quality;

VDBGradientParams VDBVisualizerShape::s_scattering_gradient("scattering");
VDBGradientParams VDBVisualizerShape::s_attenuation_gradient("attenuation");
VDBGradientParams VDBVisualizerShape::s_emission_gradient("emission");

const boost::regex VDBVisualizerShape::s_frame_expr("[^#]*\\/[^/]+[\\._]#+[\\._][^/]*vdb");
const boost::regex VDBVisualizerShape::s_hash_expr("#+");

namespace {
    enum {
        CACHE_OUT_OF_RANGE_MODE_NONE = 0,
        CACHE_OUT_OF_RANGE_MODE_HOLD,
        CACHE_OUT_OF_RANGE_MODE_REPEAT
    };
}

VDBVisualizerData::VDBVisualizerData() : bbox(MPoint(-1.0, -1.0, -1.0), MPoint(1.0, 1.0, 1.0)),
                                         vdb_path(""), vdb_file(nullptr), update_trigger(0)
{
}

VDBVisualizerData::~VDBVisualizerData()
{
    clear();
}

void VDBVisualizerData::clear(const MBoundingBox& _bbox)
{
    if (vdb_file)
    {
        if (vdb_file->isOpen())
            vdb_file->close();
        delete vdb_file;
    }
    vdb_file = nullptr;
    bbox = _bbox;
}

VDBVisualizerShape::VDBVisualizerShape()
{

}

VDBVisualizerShape::~VDBVisualizerShape()
{

}

void* VDBVisualizerShape::creator()
{
    return new VDBVisualizerShape();
}

MStatus VDBVisualizerShape::compute(const MPlug& plug, MDataBlock& dataBlock)
{
    MStatus status = MS::kSuccess;

    if (plug == s_out_vdb_path)
    {
        std::string vdb_path = dataBlock.inputValue(s_vdb_path).asString().asChar();

        if (boost::regex_match(vdb_path, s_frame_expr))
        {
            const double cache_time = dataBlock.inputValue(s_cache_time).asTime().as(MTime::uiUnit());
            const double cache_playback_offset = dataBlock.inputValue(s_cache_playback_offset).asTime().as(MTime::uiUnit());
            int cache_frame = static_cast<int>(cache_time - cache_playback_offset);
            const int cache_playback_start = dataBlock.inputValue(s_cache_playback_start).asInt();
            const int cache_playback_end = std::max(cache_playback_start, dataBlock.inputValue(s_cache_playback_end).asInt());
            bool frame_in_range = true;
            if (cache_frame < cache_playback_start)
            {
                const short cache_before_mode = dataBlock.inputValue(s_cache_before_mode).asShort();
                if (cache_before_mode == CACHE_OUT_OF_RANGE_MODE_NONE)
                    frame_in_range = false;
                else if (cache_before_mode == CACHE_OUT_OF_RANGE_MODE_HOLD)
                    cache_frame = cache_playback_start;
                else if (cache_before_mode == CACHE_OUT_OF_RANGE_MODE_REPEAT)
                {
                    const int cache_playback_range = cache_playback_end - cache_playback_start;
                    cache_frame = cache_playback_end - (cache_playback_start - cache_frame - 1) % (cache_playback_range + 1);
                }
            }
            else if (cache_frame > cache_playback_end)
            {
                const short cache_after_mode = dataBlock.inputValue(s_cache_after_mode).asShort();
                if (cache_after_mode == CACHE_OUT_OF_RANGE_MODE_NONE)
                    frame_in_range = false;
                else if (cache_after_mode == CACHE_OUT_OF_RANGE_MODE_HOLD)
                    cache_frame = cache_playback_end;
                else if (cache_after_mode == CACHE_OUT_OF_RANGE_MODE_REPEAT)
                {
                    const int cache_playback_range = cache_playback_end - cache_playback_start;
                    cache_frame = cache_playback_start + (cache_frame - cache_playback_end - 1) % (cache_playback_range + 1);
                }
            }
            cache_frame = std::max(0, cache_frame);

            if (frame_in_range)
            {
                size_t hash_count = 0;
                for (auto c : vdb_path)
                {
                    if (c == '#')
                        ++hash_count;
                }
                std::stringstream ss;
                ss.fill('0');
                ss.width(hash_count);
                ss << cache_frame;
                vdb_path = boost::regex_replace(vdb_path, s_hash_expr, ss.str());
            }
            else
                vdb_path = "";
        }

        if (vdb_path != m_vdb_data.vdb_path)
        {
            m_vdb_data.clear();
            m_vdb_data.vdb_path = vdb_path;
            if (m_vdb_data.vdb_file != nullptr)
                delete m_vdb_data.vdb_file;
            m_vdb_data.vdb_file = new openvdb::io::File(vdb_path.c_str());
            m_vdb_data.vdb_file->open(false);
            if (m_vdb_data.vdb_file->isOpen())
            {
                openvdb::GridPtrVecPtr grids = m_vdb_data.vdb_file->readAllGridMetadata();
                for (openvdb::GridPtrVec::const_iterator it = grids->begin(); it != grids->end(); ++it)
                {
                    if (openvdb::GridBase::ConstPtr grid = *it)
                        read_transformed_bounding_box(grid, m_vdb_data.bbox);
                }
            }
            else
                m_vdb_data.clear(MBoundingBox(MPoint(-1.0, -1.0, -1.0), MPoint(1.0, 1.0, 1.0)));
        }
        MDataHandle out_vdb_path_handle = dataBlock.outputValue(s_out_vdb_path);
        out_vdb_path_handle.setString(vdb_path.c_str());
    }
    else
    {
        dataBlock.inputValue(s_out_vdb_path).asString(); // trigger cache reload
        if (plug == s_grid_names)
        {
            MDataHandle grid_names_handle = dataBlock.outputValue(s_grid_names);
            if (m_vdb_data.vdb_file != nullptr && m_vdb_data.vdb_file->isOpen())
            {
                std::stringstream grid_names;
                openvdb::GridPtrVecPtr grids = m_vdb_data.vdb_file->readAllGridMetadata();
                for (openvdb::GridPtrVec::const_iterator it = grids->begin(); it != grids->end(); ++it)
                {
                    if (openvdb::GridBase::ConstPtr grid = *it)
                        grid_names << grid->getName() << " ";
                }
                std::string grid_names_string = grid_names.str();
                grid_names_handle.setString(grid_names_string.length() ? grid_names_string.substr(0, grid_names_string.length() - 1).c_str() : "");
            }
            else
                grid_names_handle.setString("");
        }
        else if (plug == s_update_trigger)
        {
            MDataHandle update_trigger_handle = dataBlock.outputValue(s_update_trigger);
            update_trigger_handle.setInt(m_vdb_data.update_trigger + 1);
        }
        else if (plug == s_bbox_min)
        {
            // TODO : why the MDataBlock is buggy in this case?
            const MPoint mn = m_vdb_data.bbox.min();
            plug.child(0).setDouble(mn.x);
            plug.child(1).setDouble(mn.y);
            plug.child(2).setDouble(mn.z);
        }
        else if (plug == s_bbox_max)
        {
            // TODO : why the MDataBlock is buggy in this case?
            const MPoint mx = m_vdb_data.bbox.max();
            plug.child(0).setDouble(mx.x);
            plug.child(1).setDouble(mx.y);
            plug.child(2).setDouble(mx.z);
        }
        else if (plug == s_channel_stats)
        {
            std::stringstream ss;
            if (m_vdb_data.vdb_file != nullptr && m_vdb_data.vdb_file->isOpen())
            {
                ss << "Bounding box : " << "[ [";
                ss << m_vdb_data.bbox.min().x << ", " << m_vdb_data.bbox.min().y << ", " << m_vdb_data.bbox.min().z;
                ss << " ] [ ";
                ss << m_vdb_data.bbox.max().x << ", " << m_vdb_data.bbox.max().y << ", " << m_vdb_data.bbox.max().z;
                ss << " ] ]" << std::endl;
                ss << "Channels : " << std::endl;
                openvdb::GridPtrVecPtr grids = m_vdb_data.vdb_file->readAllGridMetadata();
                for (openvdb::GridPtrVec::const_iterator it = grids->begin(); it != grids->end(); ++it)
                {
                    if (openvdb::GridBase::ConstPtr grid = *it)
                        ss << " - " << grid->getName() << " (" << grid->valueType() << ")" << std::endl;
                }
            }
            dataBlock.outputValue(s_channel_stats).setString(ss.str().c_str());
        }
        else if (plug == s_voxel_size)
        {
            float voxel_size = std::numeric_limits<float>::max();
            if (m_vdb_data.vdb_file != nullptr && m_vdb_data.vdb_file->isOpen())
            {
                openvdb::GridPtrVecPtr grids = m_vdb_data.vdb_file->readAllGridMetadata();
                for (openvdb::GridPtrVec::const_iterator it = grids->begin(); it != grids->end(); ++it)
                {
                    if (openvdb::GridBase::ConstPtr grid = *it)
                    {
                        openvdb::Vec3d vs = grid->voxelSize();
                        if (vs.x() > 0.0)
                            voxel_size = std::min(static_cast<float>(vs.x()), voxel_size);
                        if (vs.y() > 0.0)
                            voxel_size = std::min(static_cast<float>(vs.y()), voxel_size);
                        if (vs.z() > 0.0)
                            voxel_size = std::min(static_cast<float>(vs.z()), voxel_size);
                    }
                }
            }
            else
                voxel_size = 1.0f;
            dataBlock.outputValue(s_voxel_size).setFloat(voxel_size);
        }
        else
            return MStatus::kUnknownParameter;
    }

    dataBlock.setClean(plug);

    return status;
}

MStatus VDBVisualizerShape::initialize()
{
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    MFnEnumAttribute eAttr;
    MFnUnitAttribute uAttr;

    MStatus status = MS::kSuccess;

    // input params

    s_vdb_path = tAttr.create("vdbPath", "vdb_path", MFnData::kString);

    s_cache_time = uAttr.create("cacheTime", "cache_time", MFnUnitAttribute::kTime, 0.0);

    s_cache_playback_start = nAttr.create("cachePlaybackStart", "cache_playback_start", MFnNumericData::kInt);
    nAttr.setDefault(1);
    nAttr.setMin(0);

    s_cache_playback_end = nAttr.create("cachePlaybackEnd", "cache_playback_end", MFnNumericData::kInt);
    nAttr.setDefault(100);
    nAttr.setMin(0);

    s_cache_playback_offset = uAttr.create("cachePlaybackOffset", "cache_playback_offset", MFnUnitAttribute::kTime, 0.0);

    s_cache_before_mode = eAttr.create("cacheBeforeMode", "cache_before_mode");
    eAttr.addField("None", CACHE_OUT_OF_RANGE_MODE_NONE);
    eAttr.addField("Hold", CACHE_OUT_OF_RANGE_MODE_HOLD);
    eAttr.addField("Repeat", CACHE_OUT_OF_RANGE_MODE_REPEAT);
    eAttr.setDefault(1);

    s_cache_after_mode = eAttr.create("cacheAfterMode", "cache_after_mode");
    eAttr.addField("None", CACHE_OUT_OF_RANGE_MODE_NONE);
    eAttr.addField("Hold", CACHE_OUT_OF_RANGE_MODE_HOLD);
    eAttr.addField("Repeat", CACHE_OUT_OF_RANGE_MODE_REPEAT);
    eAttr.setDefault(1);

    s_display_mode = eAttr.create("displayMode", "display_mode");
    eAttr.addField("Axis Aligned Bounding Box", DISPLAY_AXIS_ALIGNED_BBOX);
    eAttr.addField("Per Grid Bounding Box", DISPLAY_GRID_BBOX);
    eAttr.addField("Point Cloud", DISPLAY_POINT_CLOUD);
    eAttr.addField("Volumetric Non Shaded", DISPLAY_NON_SHADED);
    eAttr.addField("Volumetric Shaded", DISPLAY_SHADED);
    eAttr.addField("Mesh", DISPLAY_MESH);
    eAttr.setDefault(DISPLAY_GRID_BBOX);

    addAttribute(s_display_mode);

    // output params

    s_update_trigger = nAttr.create("updateTrigger", "update_trigger", MFnNumericData::kInt);
    nAttr.setDefault(0);
    nAttr.setStorable(false);
    nAttr.setReadable(false);
    nAttr.setWritable(false);

    s_grid_names = tAttr.create("gridNames", "grid_names", MFnData::kString);
    tAttr.setStorable(false);
    tAttr.setWritable(false);
    tAttr.setReadable(true);

    s_out_vdb_path = tAttr.create("outVdbPath", "out_vdb_path", MFnData::kString);
    tAttr.setStorable(false);
    tAttr.setWritable(false);
    tAttr.setReadable(true);

    s_bbox_min = nAttr.createPoint("bboxMin", "bbox_min");
    nAttr.setStorable(false);
    nAttr.setWritable(false);
    nAttr.setReadable(true);

    s_bbox_max = nAttr.createPoint("bboxMax", "bbox_max");
    nAttr.setStorable(false);
    nAttr.setWritable(false);
    nAttr.setReadable(true);

    s_channel_stats = tAttr.create("channelStats", "channel_stats", MFnData::kString);
    tAttr.setStorable(false);
    tAttr.setWritable(false);
    tAttr.setReadable(true);

    s_voxel_size = nAttr.create("voxelSize", "voxel_size", MFnNumericData::kFloat);
    nAttr.setStorable(false);
    nAttr.setWritable(false);
    nAttr.setReadable(true);

    MObject input_params[] = {
        s_vdb_path, s_cache_time, s_cache_playback_start, s_cache_playback_end,
        s_cache_playback_offset, s_cache_before_mode, s_cache_after_mode
    };

    MObject output_params[] = {
        s_update_trigger, s_grid_names, s_out_vdb_path, s_bbox_min, s_bbox_max, s_channel_stats, s_voxel_size
    };

    for (auto output_param : output_params)
        addAttribute(output_param);

    for (auto input_param : input_params)
    {
        addAttribute(input_param);

        for (auto output_param : output_params)
            attributeAffects(input_param, output_param);
    }

    attributeAffects(s_display_mode, s_update_trigger);

    s_scattering_source = eAttr.create("scatteringSource", "scattering_source");
    eAttr.addField("parameter", 0);
    eAttr.addField("channel", 1);

    s_scattering = nAttr.createColor("scattering", "scattering");
    nAttr.setDefault(1.0, 1.0, 1.0);

    s_scattering_channel = tAttr.create("scatteringChannel", "scattering_channel", MFnData::kString);

    s_scattering_color = nAttr.createColor("scatteringColor", "scattering_color");
    nAttr.setDefault(1.0, 1.0, 1.0);

    s_scattering_intensity = nAttr.create("scatteringIntensity", "scattering_intensity", MFnNumericData::kFloat);
    nAttr.setDefault(1.0f);
    nAttr.setMin(0.0f);
    nAttr.setSoftMax(1.0f);

    s_anisotropy = nAttr.create("anisotropy", "anisotropy", MFnNumericData::kFloat);
    nAttr.setDefault(0.0f);
    nAttr.setMin(-1.0f);
    nAttr.setMax(1.0f);

    s_attenuation_source = eAttr.create("attenuationSource", "attenuation_source");
    eAttr.addField("parameter", 0);
    eAttr.addField("channel", 1);
    eAttr.addField("scattering", 2);

    s_attenuation = nAttr.createColor("attenuation", "attenuation");
    nAttr.setDefault(1.0, 1.0, 1.0);

    s_attenuation_channel = tAttr.create("attenuationChannel", "attenuation_channel", MFnData::kString);

    s_attenuation_color = nAttr.createColor("attenuationColor", "attenuation_color");
    nAttr.setDefault(1.0, 1.0, 1.0);

    s_attenuation_intensity = nAttr.create("attenuationIntensity", "attenuation_intensity", MFnNumericData::kFloat);
    nAttr.setDefault(1.0f);
    nAttr.setMin(0.0f);
    nAttr.setSoftMax(1.0f);

    s_attenuation_mode = eAttr.create("attenuationMode", "attenuation_mode");
    eAttr.addField("absorption", 0);
    eAttr.addField("extinction", 1);

    s_emission_source = eAttr.create("emissionSource", "emission_source");
    eAttr.addField("parameter", 0);
    eAttr.addField("channel", 1);

    s_emission = nAttr.createColor("emission", "emission");
    nAttr.setDefault(0.0, 0.0, 0.0);

    s_emission_channel = tAttr.create("emissionChannel", "emission_channel", MFnData::kString);

    s_emission_color = nAttr.createColor("emissionColor", "emission_color");
    nAttr.setDefault(1.0, 1.0, 1.0);

    s_emission_intensity = nAttr.create("emissionIntensity", "emission_intensity", MFnNumericData::kFloat);
    nAttr.setDefault(1.0f);
    nAttr.setMin(0.0f);
    nAttr.setSoftMax(1.0f);

    s_position_offset = nAttr.createPoint("positionOffset", "position_offset");
    nAttr.setDefault(0.0, 0.0, 0.0);

    s_interpolation = eAttr.create("interpolation", "interpolation");
    eAttr.addField("Closest", 0);
    eAttr.addField("Trilinear", 1);
    eAttr.addField("Tricubic", 2);

    s_compensate_scaling = nAttr.create("compensateScaling", "compensate_scaling", MFnNumericData::kBoolean);
    nAttr.setDefault(true);

    s_sampling_quality = nAttr.create("samplingQuality", "sampling_quality", MFnNumericData::kFloat);
    nAttr.setDefault(100.0f);
    nAttr.setMin(1.0f);
    nAttr.setSoftMax(300.0f);
    addAttribute(s_sampling_quality);

    s_additional_channel_export = tAttr.create("additionalChannelExport", "additional_channel_export", MFnData::kString);
    addAttribute(s_additional_channel_export);

    s_scattering_gradient.create_params();
    s_scattering_gradient.create_params();
    s_scattering_gradient.create_params();

    MObject shader_params[] = {
            s_scattering_source, s_scattering, s_scattering_channel, s_scattering_color,
            s_scattering_intensity, s_anisotropy, s_attenuation_source, s_attenuation,
            s_attenuation_channel, s_attenuation_color, s_attenuation_intensity, s_attenuation_mode,
            s_emission_source, s_emission, s_emission_channel, s_emission_color,
            s_emission_intensity, s_position_offset, s_interpolation, s_compensate_scaling
    };

    for (auto shader_param : shader_params)
    {
        addAttribute(shader_param);
        attributeAffects(shader_param, s_update_trigger);
    }

    s_scattering_gradient.create_params();
    s_attenuation_gradient.create_params();
    s_emission_gradient.create_params();

    s_scattering_gradient.affect_output(s_update_trigger);
    s_attenuation_gradient.affect_output(s_update_trigger);
    s_emission_gradient.affect_output(s_update_trigger);

    return status;
}

VDBVisualizerData* VDBVisualizerShape::get_update()
{
    const int update_trigger = MPlug(thisMObject(), s_update_trigger).asInt();

    if (update_trigger != m_vdb_data.update_trigger)
    {
        m_vdb_data.display_mode = static_cast<VDBDisplayMode>(MPlug(thisMObject(), s_display_mode).asShort());
        m_vdb_data.update_trigger = update_trigger;
        return &m_vdb_data;
    }
    else
        return 0;
}

bool VDBVisualizerShape::isBounded() const
{
    return true;
}

MBoundingBox VDBVisualizerShape::boundingBox() const
{
    MPlug(thisMObject(), s_out_vdb_path).asString();
    return m_vdb_data.bbox;
}

VDBVisualizerShapeUI::VDBVisualizerShapeUI()
{

}

VDBVisualizerShapeUI::~VDBVisualizerShapeUI()
{


}

void* VDBVisualizerShapeUI::creator()
{
    return new VDBVisualizerShapeUI();
}

bool VDBVisualizerShapeUI::select(MSelectInfo& selectInfo, MSelectionList& selectionList, MPointArray& worldSpaceSelectPts) const
{
    if (!selectInfo.isRay())
    {
        const MBoundingBox bbox = surfaceShape()->boundingBox();

        const MPoint min = bbox.min();
        const MPoint max = bbox.max();

        M3dView view = selectInfo.view();

        unsigned int orig_x_u = 0;
        unsigned int orig_y_u = 0;
        unsigned int size_x_u = 0;
        unsigned int size_y_u = 0;
        selectInfo.selectRect(orig_x_u, orig_y_u, size_x_u, size_y_u);
        const MDagPath object_path = selectInfo.selectPath();
        const MMatrix object_matrix = object_path.inclusiveMatrix();

        const short orig_x = static_cast<short>(orig_x_u);
        const short orig_y = static_cast<short>(orig_y_u);
        const short size_x = static_cast<short>(size_x_u);
        const short size_y = static_cast<short>(size_y_u);

        auto check_point_in_selection = [&] (MPoint point) -> bool {
            point *= object_matrix;
            short x_pos = 0; short y_pos = 0;
            view.worldToView(point, x_pos, y_pos);

            const short t_x = x_pos - orig_x;
            const short t_y = y_pos - orig_y;

            if (t_x >= 0 && t_x < size_x &&
                t_y >= 0 && t_y < size_y)
            {
                MSelectionList item;
                item.add(object_path);
                selectInfo.addSelection(item, point, selectionList, worldSpaceSelectPts, MSelectionMask::kSelectMeshes, false);
                return true;
            }
            else return false;
        };

        return check_point_in_selection(min) || check_point_in_selection(max) ||
               check_point_in_selection(MPoint(max.x, min.y, min.z)) ||
               check_point_in_selection(MPoint(min.x, max.y, min.z)) ||
               check_point_in_selection(MPoint(min.x, min.y, max.z)) ||
               check_point_in_selection(MPoint(max.x, max.y, min.z)) ||
               check_point_in_selection(MPoint(max.x, min.y, max.z)) ||
               check_point_in_selection(MPoint(min.x, max.y, max.z));
    }
    else return false;
}

bool VDBVisualizerShapeUI::canDrawUV() const
{
    return false;
}
