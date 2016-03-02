#include "vdb_visualizer.h"

#include <lumaNodeId.h>

const MTypeId VDBVisualizerShape::typeId(ID_OPENVDB_VISUALIZER);
const MString VDBVisualizerShape::typeName("vdb_visualizer");
const MString VDBVisualizerShape::drawDbClassification("drawdb/geometry/fractal/vdb_visualizer");

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
    MStatus status = MS::kSuccess;

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
