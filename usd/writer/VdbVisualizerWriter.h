#ifndef _openvdb_VdbVisualizerWriter_h_
#define _openvdb_VdbVisualizerWriter_h_

#include "pxr/pxr.h"
#include "usdMaya/MayaTransformWriter.h"

PXR_NAMESPACE_OPEN_SCOPE

class VdbVisualizerWriter : public MayaTransformWriter {
public:
    VdbVisualizerWriter(const MDagPath & iDag, const SdfPath& uPath, bool instanceSource, usdWriteJobCtx& jobCtx);
    virtual ~VdbVisualizerWriter();

    virtual void postExport() override;
    virtual void write(const UsdTimeCode& usdTime) override;
private:
    bool has_velocity_grids;
};

using VdbVisualizerWriterPtr = std::shared_ptr<VdbVisualizerWriter>;

PXR_NAMESPACE_CLOSE_SCOPE

#endif
