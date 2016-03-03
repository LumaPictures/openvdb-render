#pragma once

#include <maya/MPxDrawOverride.h>

#include "vdb_visualizer.h"

namespace MHWRender {

    class VDBDrawOverride : public MPxDrawOverride {
    public:
        VDBDrawOverride(const MObject& obj);
        ~VDBDrawOverride();

        static MPxDrawOverride* creator(const MObject& obj);
        static void draw_callback(const MDrawContext& context, const MUserData* data);

        static MString registrantId;

        virtual MHWRender::DrawAPI supportedDrawAPIs() const;

        virtual MUserData* prepareForDraw(
                const MDagPath& objPath,
                const MDagPath& cameraPath,
                const MFrameContext& frameContext,
                MUserData* oldData);

        virtual bool isBounded(
                const MDagPath& objPath,
                const MDagPath& cameraPath) const;

        static bool init_shaders();

    private:
        VDBVisualizerShape* p_vdb_visualizer;
    };

}
