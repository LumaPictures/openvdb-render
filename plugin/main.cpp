#include <GL/glew.h>

#include <maya/MFnPlugin.h>
#include <maya/MGlobal.h>
#include <maya/MDrawRegistry.h>

#include <iostream>

#include "vdb_draw_override.h"
#include "vdb_visualizer.h"

MStatus initializePlugin(MObject obj)
{
    MStatus status = MS::kFailure;

    if (MGlobal::mayaState() == MGlobal::kInteractive)
    {
        if (glewInit() != GLEW_OK)
        {
            status.perror("Error initializing glew.");
            return status;
        }

        if (!glewIsSupported("GL_EXT_direct_state_access"))
        {
            status.perror("Direct State Access is not available, update your drivers or use NVidia cards!");
            return status;
        }

        if (!MHWRender::VDBDrawOverride::init_shaders())
        {
            status.perror("Error initializing shaders.");
            return status;
        }
    }

    MFnPlugin plugin(obj, "Luma Pictures", "0.0.1", "Any");

    status = plugin.registerShape(VDBVisualizerShape::typeName, VDBVisualizerShape::typeId,
                                  VDBVisualizerShape::creator, VDBVisualizerShape::initialize,
                                  VDBVisualizerShapeUI::creator, &VDBVisualizerShape::drawDbClassification);

    if (!status)
    {
        status.perror("Error registering the VDBVisualizer Node.");
        return status;
    }

    status = MHWRender::MDrawRegistry::registerDrawOverrideCreator(
            VDBVisualizerShape::drawDbClassification,
            MHWRender::VDBDrawOverride::registrantId,
            MHWRender::VDBDrawOverride::creator
    );

    if (!status)
    {
        status.perror("Error registering the VDBVisualizer Draw Override.");
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
        status.perror("Error deregistering the VDBVisualizer Node.");
        return status;
    }

    status = MHWRender::MDrawRegistry::deregisterDrawOverrideCreator(
            VDBVisualizerShape::drawDbClassification,
            MHWRender::VDBDrawOverride::registrantId
    );

    if (!status)
    {
        status.perror("Error deregistering the VDBVisualizer Draw Override.");
        return status;
    }

    return status;
}
