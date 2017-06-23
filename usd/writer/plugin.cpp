#include <maya/MFnPlugin.h>

#include "VdbVisualizerWriter.h"
#include <usdMaya/primWriterRegistry.h>

PXR_NAMESPACE_USING_DIRECTIVE

TF_REGISTRY_FUNCTION_WITH_TAG(PxrUsdMayaPrimWriterRegistry, partioVisualizerWriter) {
    PxrUsdMayaPrimWriterRegistry::Register("vdbVisualizer",
                 [](const MDagPath& iDag,
                    const SdfPath& uPath, bool instanceSource,
                    usdWriteJobCtx& jobCtx) -> MayaPrimWriterPtr { return std::make_shared<VdbVisualizerWriter>(
                    iDag, uPath, instanceSource, jobCtx); });
}

MStatus initializePlugin(MObject /*obj*/)
{
    return MS::kSuccess;
}

MStatus uninitializePlugin(MObject /*obj*/)
{
    return MS::kSuccess;
}
