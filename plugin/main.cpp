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
#include <maya/MFnPlugin.h>
#include <maya/MGlobal.h>
#include <maya/MDrawRegistry.h>

#include "vdb_subscene_override.h"
#include "vdb_query.h"

PLUGIN_EXPORT MStatus initializePlugin(MObject obj)
{
    const bool is_interactive = MGlobal::mayaState() == MGlobal::kInteractive;
    MStatus status = MS::kFailure;

    MFnPlugin plugin(obj, "Luma Pictures", "0.2.0", "Any");

    status = plugin.registerShape(VDBVisualizerShape::typeName, VDBVisualizerShape::typeId,
                                  VDBVisualizerShape::creator, VDBVisualizerShape::initialize,
                                  VDBVisualizerShapeUI::creator, &VDBVisualizerShape::drawDbClassification);

    if (!status) {
        status.perror("[openvdb] Error registering the VDBVisualizer Node.");
        return status;
    }

    status = plugin.registerNode(VDBSamplerNode::s_type_name, VDBSamplerNode::s_type_id,
                                 VDBSamplerNode::creator, VDBSamplerNode::initialize, MPxNode::kDependNode,
                                 &VDBSamplerNode::s_classification);

    if (!status) {
        status.perror("[openvdb] Error registering the VDBSampler Node.");
        return status;
    }

    status = plugin.registerNode(VDBSimpleShaderNode::s_type_name, VDBSimpleShaderNode::s_type_id,
                                 VDBSimpleShaderNode::creator, VDBSimpleShaderNode::initialize, MPxNode::kDependNode,
                                 &VDBSimpleShaderNode::s_classification);

    if (!status) {
        status.perror("[openvdb] Error registering the VDBSimpleShader Node.");
        return status;
    }

    status = MHWRender::MDrawRegistry::registerSubSceneOverrideCreator(
        VDBVisualizerShape::drawDbClassification,
        MHWRender::VDBSubSceneOverride::registrantId,
        MHWRender::VDBSubSceneOverride::creator
    );

    if (!status) {
        status.perror("[openvdb] Error registering the VDBVisualizer Sub Scene Override.");
        return status;
    }

    openvdb::initialize();
    MHWRender::VDBSubSceneOverride::init_gpu();

    status = plugin.registerCommand("vdb_query", VDBQueryCmd::creator, VDBQueryCmd::create_syntax);

    if (!status) {
        status.perror("[openvdb] Error registering the VDBQuery Command.");
        return status;
    }

    status = plugin.registerCommand(VDBVolumeCacheCmd::COMMAND_STRING, VDBVolumeCacheCmd::creator, VDBVolumeCacheCmd::create_syntax);

    if (!status) {
        status.perror("[openvdb] Error registering the VDBVolumeCacheCmd Command.");
        return status;
    }

    if (is_interactive) {
        MGlobal::executePythonCommand(
            "import AEvdb_visualizerTemplate; import AEvdb_samplerTemplate; import AEvdb_shaderTemplate");
    }

    return status;
}

PLUGIN_EXPORT MStatus uninitializePlugin(MObject obj)
{
    MStatus status = MS::kSuccess;

    MFnPlugin plugin(obj);

    status = plugin.deregisterNode(VDBVisualizerShape::typeId);

    if (!status) {
        status.perror("[openvdb] Error deregistering the VDBVisualizer Node.");
        return status;
    }

    status = plugin.deregisterNode(VDBSamplerNode::s_type_id);

    if (!status) {
        status.perror("[openvdb] Error deregistering the VDBSampler Node.");
        return status;
    }

    status = plugin.deregisterNode(VDBSimpleShaderNode::s_type_id);

    if (!status) {
        status.perror("[openvdb] Error deregistering the VDBSimpleShader Node.");
        return status;
    }

    status = MHWRender::MDrawRegistry::deregisterSubSceneOverrideCreator(
        VDBVisualizerShape::drawDbClassification,
        MHWRender::VDBSubSceneOverride::registrantId
    );

    if (!status) {
        status.perror("[openvdb] Error deregistering the VDBVisualizer Sub Scene Override.");
        return status;
    }

    status = plugin.deregisterCommand("vdb_query");

    if (!status) {
        status.perror("[openvdb] Error deregistering the VDBQuery Command.");
        return status;
    }

    status = plugin.deregisterCommand(VDBVolumeCacheCmd::COMMAND_STRING);

    if (!status) {
        status.perror("[openvdb] Error deregistering the VDBVolumeCacheCmd Command.");
        return status;
    }

    openvdb::uninitialize();

    return status;
}
