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

        virtual MHWRender::DrawAPI supportedDrawAPIs() const;
        virtual void update(MSubSceneContainer& container, const MFrameContext& frameContext);
        virtual bool requiresUpdate(const MSubSceneContainer& container, const MFrameContext& frameContext) const;

        static MString registrantId;
    private:
        VDBVisualizerShape* p_vdb_visualizer;
        std::unique_ptr<VDBSubSceneOverrideData> p_data;

        std::unique_ptr<MVertexBuffer> p_bbox_position;
        std::unique_ptr<MIndexBuffer> p_bbox_indices;

        std::unique_ptr<MVertexBuffer> p_position_buffer;
        std::unique_ptr<MVertexBuffer> p_color_buffer;
        std::unique_ptr<MIndexBuffer> p_index_buffer;
    };
}
