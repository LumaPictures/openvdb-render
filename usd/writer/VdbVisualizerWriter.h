#ifndef _openvdb_VdbVisualizerWriter_h_
#define _openvdb_VdbVisualizerWriter_h_

#include "pxr/pxr.h"
#include "usdMaya/transformWriter.h"

PXR_NAMESPACE_OPEN_SCOPE

class VdbVisualizerWriter : public UsdMayaTransformWriter {
public:
    VdbVisualizerWriter(const MDagPath & iDag, const SdfPath& uPath, UsdMayaWriteJobContext& jobCtx);
    virtual ~VdbVisualizerWriter();

    virtual void PostExport() override;
    virtual void Write(const UsdTimeCode& usdTime) override;
private:
    bool has_velocity_grids;
};

using VdbVisualizerWriterPtr = std::shared_ptr<VdbVisualizerWriter>;

PXR_NAMESPACE_CLOSE_SCOPE

#endif
