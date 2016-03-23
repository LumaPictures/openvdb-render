#include <GL/glew.h>

#include <maya/MFnPlugin.h>
#include <maya/MGlobal.h>
#include <maya/MDrawRegistry.h>

#include "vdb_draw_override.h"
#include "vdb_visualizer.h"
#include "vdb_query.h"
#include "vdb_sampler.h"

MStatus initializePlugin(MObject obj)
{
    const bool is_interactive = MGlobal::mayaState() == MGlobal::kInteractive;
    MStatus status = MS::kFailure;

    if (is_interactive)
    {
        if (glewInit() != GLEW_OK)
        {
            status.perror("[openvdb] Error initializing glew.");
            return status;
        }

        if (!glewIsSupported("GL_EXT_direct_state_access"))
        {
            status.perror("[openvdb] Direct State Access is not available, update your drivers or use a newer GPU!");
            return status;
        }

        if (!MHWRender::VDBDrawOverride::init_shaders())
        {
            status.perror("[openvdb] Error initializing shaders.");
            return status;
        }

    }

    MFnPlugin plugin(obj, "Luma Pictures", "0.0.1", "Any");

    status = plugin.registerShape(VDBVisualizerShape::typeName, VDBVisualizerShape::typeId,
                                  VDBVisualizerShape::creator, VDBVisualizerShape::initialize,
                                  VDBVisualizerShapeUI::creator, &VDBVisualizerShape::drawDbClassification);

    if (!status)
    {
        status.perror("[openvdb] Error registering the VDBVisualizer Node.");
        return status;
    }

    status = plugin.registerNode(VDBSamplerNode::s_type_name, VDBSamplerNode::s_type_id,
                                 VDBSamplerNode::creator, VDBSamplerNode::initialize, MPxNode::kDependNode, &VDBSamplerNode::s_classification);

    if (!status)
    {
        status.perror("[openvdb] Error registering the VDBSampler Node.");
        return status;
    }

    status = MHWRender::MDrawRegistry::registerDrawOverrideCreator(
            VDBVisualizerShape::drawDbClassification,
            MHWRender::VDBDrawOverride::registrantId,
            MHWRender::VDBDrawOverride::creator
    );

    if (!status)
    {
        status.perror("[openvdb] Error registering the VDBVisualizer Draw Override.");
        return status;
    }

    openvdb::initialize();

    status = plugin.registerCommand("vdb_query", VDBQueryCmd::creator, VDBQueryCmd::create_syntax);

    if (!status)
    {
        status.perror("[openvdb] Error registering the VDBQuery Command.");
        return status;
    }

    if (is_interactive)
        MGlobal::executePythonCommandOnIdle("import AEvdb_visualizerTemplate; import AEvdb_samplerTemplate");

    return status;
}

MStatus uninitializePlugin(MObject obj)
{
    MStatus status = MS::kSuccess;

    MFnPlugin plugin(obj);

    status = plugin.deregisterNode(VDBVisualizerShape::typeId);

    if (!status)
    {
        status.perror("[openvdb] Error deregistering the VDBVisualizer Node.");
        return status;
    }

    status = plugin.deregisterNode(VDBSamplerNode::s_type_id);

    if (!status)
    {
        status.perror("[openvdb] Error deregistering the VDBSampler Node.");
        return status;
    }

    status = MHWRender::MDrawRegistry::deregisterDrawOverrideCreator(
            VDBVisualizerShape::drawDbClassification,
            MHWRender::VDBDrawOverride::registrantId
    );

    if (!status)
    {
        status.perror("[openvdb] Error deregistering the VDBVisualizer Draw Override.");
        return status;
    }

    status = plugin.deregisterCommand("vdb_query");

    if (!status)
    {
        status.perror("[openvdb] Error deregistering the VDBQuery Command.");
        return status;
    }

    openvdb::uninitialize();

    return status;
}
