/*
 * The update trigger is used to trigger updates for viewport visualization
 * if it's changed that means something have changed, and the viewport data has to be reloaded.
 *
 * This is done, so we only update the node if we are interactively editing the node, also
 * so we can completely separate reading the data from the main node, all it does it's just loading
 * a vdb dataset and reading information about the contained channels, but not loading actual voxel data.
 *
 */

#include "vdb_visualizer.h"

#include <lumaNodeId.h>

#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnEnumAttribute.h>

const MTypeId VDBVisualizerShape::typeId(ID_VDB_VISUALIZER);
const MString VDBVisualizerShape::typeName("vdb_visualizer");
const MString VDBVisualizerShape::drawDbClassification("drawdb/geometry/fractal/vdb_visualizer");

MObject VDBVisualizerShape::s_update_trigger;
MObject VDBVisualizerShape::s_vdb_path;
MObject VDBVisualizerShape::s_display_mode;

VDBVisualizerData::VDBVisualizerData() : vdb_path(""), vdb_file(0), update_trigger(0)
{
}

VDBVisualizerData::~VDBVisualizerData()
{
    clear();
}

void VDBVisualizerData::clear()
{
    if (vdb_file)
    {
        if (vdb_file->isOpen())
            vdb_file->close();
        delete vdb_file;
    }
    vdb_file = 0;
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

    const std::string vdb_path = dataBlock.inputValue(s_vdb_path).asString().asChar();

    if (vdb_path != m_vdb_data.vdb_path)
    {
        m_vdb_data.clear();
        m_vdb_data.vdb_path = vdb_path;
        m_vdb_data.vdb_file = new openvdb::io::File(vdb_path.c_str());
        m_vdb_data.vdb_file->open(false);
        if (!m_vdb_data.vdb_file->isOpen())
        {
            status = MS::kFailure;
            status.perror(MString("[openvdb] Error opening file at : ") + vdb_path.c_str());
            m_vdb_data.clear();
            return status;
        }

        m_vdb_data.bbox = MBoundingBox();

        openvdb::GridPtrVecPtr grids = m_vdb_data.vdb_file->readAllGridMetadata();
        for (openvdb::GridPtrVec::const_iterator it = grids->begin(); it != grids->end(); ++it)
        {
            if (openvdb::GridBase::ConstPtr grid = *it)
            {
                openvdb::Vec3d point_in_bbox = grid->metaValue<openvdb::Vec3i>("file_bbox_min") * grid->voxelSize();
                m_vdb_data.bbox.expand(MPoint(point_in_bbox.x(), point_in_bbox.y(), point_in_bbox.z(), 1.0));
                point_in_bbox = grid->metaValue<openvdb::Vec3i>("file_bbox_max") * grid->voxelSize();
                m_vdb_data.bbox.expand(MPoint(point_in_bbox.x(), point_in_bbox.y(), point_in_bbox.z(), 1.0));
            }
        }

        const int update_trigger = dataBlock.inputValue(s_update_trigger).asInt() + 1;
        MDataHandle update_trigger_handle = dataBlock.outputValue(s_update_trigger);
        update_trigger_handle.setInt(update_trigger);
        update_trigger_handle.setClean();
        m_vdb_data.update_trigger = update_trigger;
    }

    return status;
}

MStatus VDBVisualizerShape::initialize()
{
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    MFnEnumAttribute eAttr;

    MStatus status = MS::kSuccess;

    s_update_trigger = nAttr.create("updateTrigger", "update_trigger", MFnNumericData::kInt);
    nAttr.setDefault(0);
    nAttr.setStorable(false);
    nAttr.setReadable(false);
    nAttr.setWritable(false);

    s_vdb_path = tAttr.create("vdbPath", "vdb_path", MFnData::kString);

    s_display_mode = eAttr.create("displayMode", "display_mode");
    eAttr.addField("boundingBox", 0);
    eAttr.addField("multiBoundingBox", 1);
    eAttr.addField("pointCloud", 2);
    eAttr.addField("volumetricNonShaded", 3);
    eAttr.addField("volumetricShaded", 4);
    eAttr.addField("mesh", 5);

    addAttribute(s_vdb_path);
    addAttribute(s_display_mode);

    attributeAffects(s_vdb_path, s_update_trigger);
    attributeAffects(s_display_mode, s_update_trigger);

    return status;
}

VDBVisualizerData* VDBVisualizerShape::get_update()
{
    const int update_trigger = MPlug(thisMObject(), s_update_trigger).asInt();

    if (update_trigger != m_vdb_data.update_trigger)
    {
        m_vdb_data.update_trigger = update_trigger;
        return &m_vdb_data;
    }
    else
        return 0;
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
