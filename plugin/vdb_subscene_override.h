#pragma once

#include <maya/MPxSubSceneOverride.h>
#include <memory>

#include "vdb_visualizer.h"

namespace MHWRender {
    struct VDBSubSceneOverrideData;
    class VDBSubSceneOverride : public MHWRender::MPxSubSceneOverride {
    public:
        static MPxSubSceneOverride* creator(const MObject& obj);

        VDBSubSceneOverride(const MObject& obj);
        virtual ~VDBSubSceneOverride();

        MHWRender::DrawAPI supportedDrawAPIs() const;
        void update(MSubSceneContainer& container, const MFrameContext& frameContext);
        bool requiresUpdate(const MSubSceneContainer& container, const MFrameContext& frameContext) const;

        static MString registrantId;
    private:
        VDBVisualizerShape* p_vdb_visualizer;
        std::unique_ptr<VDBSubSceneOverrideData> p_data;
    };
}
