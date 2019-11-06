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

#include "VdbVisualizerWriter.h"
#include <usdMaya/primWriterRegistry.h>

PXR_NAMESPACE_USING_DIRECTIVE

TF_REGISTRY_FUNCTION_WITH_TAG(UsdMayaPrimWriterRegistry, partioVisualizerWriter) {
    UsdMayaPrimWriterRegistry::Register("vdb_visualizer",
                 [](const MFnDependencyNode& depNodeFn,
                    const SdfPath& uPath,
                    UsdMayaWriteJobContext& jobCtx) -> UsdMayaPrimWriterSharedPtr { return std::make_shared<VdbVisualizerWriter>(
                    depNodeFn, uPath, jobCtx); });
}

MStatus initializePlugin(MObject /*obj*/)
{
    return MS::kSuccess;
}

MStatus uninitializePlugin(MObject /*obj*/)
{
    return MS::kSuccess;
}
