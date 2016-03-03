#include <GL/glew.h>

#include <maya/MFnPlugin.h>
#include <maya/MGlobal.h>

#include <iostream>

#include "vdb_draw_override.h"
#include "vdb_visualizer.h"

MStatus initializePlugin(MObject obj)
{
    MStatus status = MS::kSuccess;

    if (MGlobal::mayaState() == MGlobal::kInteractive)
    {
        if (glewInit() != GLEW_OK)
        {
            status.perror("Error initializing glew.");
            return status;
        }
    }

    MFnPlugin plugin(obj, "Luma Pictures", "0.0.1", "Any");

    status = plugin.registerShape(VDBVisualizerShape::typeName, VDBVisualizerShape::typeId,
                                  VDBVisualizerShape::creator, VDBVisualizerShape::initialize,
                                  VDBVisualizerShapeUI::creator, &VDBVisualizerShape::drawDbClassification);

    if (!status)
    {
        status.perror("Error registering the VDBVisualizer node.");
        return status;
    }

    return status;
}

MStatus uninitializePlugin(MObject obj)
{
    MStatus status = MS::kSuccess;

    MFnPlugin plugin(obj);

    status = plugin.deregisterNode(VDBVisualizerShape::typeId);

    if (!status)
    {
        status.perror("Error deregistering the VDBVisualizer node.");
        return status;
    }

    return status;
}
