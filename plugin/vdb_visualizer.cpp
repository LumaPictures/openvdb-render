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

MObject VDBVisualizerShape::s_updateTrigger;
MObject VDBVisualizerShape::s_vdbPath;
MObject VDBVisualizerShape::s_displayMode;

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

MStatus VDBVisualizerShape::initialize()
{
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    MFnEnumAttribute eAttr;

    MStatus status = MS::kSuccess;

    s_updateTrigger = nAttr.create("updateTrigger", "update_trigger", MFnNumericData::kInt);

    s_vdbPath = tAttr.create("vdbPath", "vdb_path", MFnData::kString);

    s_displayMode = eAttr.create("displayMode", "display_mode");
    eAttr.addField("boundingBox", 0);
    eAttr.addField("multiBoundingBox", 1);
    eAttr.addField("pointCloud", 2);
    eAttr.addField("volumetricNonShaded", 3);
    eAttr.addField("volumetricShaded", 4);

    addAttribute(s_vdbPath);
    addAttribute(s_displayMode);

    attributeAffects(s_vdbPath, s_updateTrigger);
    attributeAffects(s_displayMode, s_updateTrigger);

    return status;
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
