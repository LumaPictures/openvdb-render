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
