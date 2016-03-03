#include "vdb_draw_override.h"

namespace MHWRender {

    namespace{

        struct DrawData : public MUserData {

            DrawData() : MUserData(false)
            {

            }

            void update(const MDagPath& objPath,
                        const MDagPath& cameraPath,
                        const MFrameContext& frameContext)
            {

            }
        };

    }

    VDBDrawOverride::VDBDrawOverride(const MObject& obj) : MPxDrawOverride(obj, draw_callback)
    {

    }

    VDBDrawOverride::~VDBDrawOverride()
    {

    }

    MPxDrawOverride* VDBDrawOverride::creator(const MObject& obj)
    {
        return new VDBDrawOverride(obj);
    }

    void VDBDrawOverride::draw_callback(const MDrawContext& context, const MUserData* data)
    {

    }

    MString VDBDrawOverride::registrantId("VDBVisualizerDrawOverride");

    MHWRender::DrawAPI VDBDrawOverride::supportedDrawAPIs() const
    {
        return kOpenGL;
    }

    MUserData* VDBDrawOverride::prepareForDraw(
            const MDagPath& objPath,
            const MDagPath& cameraPath,
            const MFrameContext& frameContext,
            MUserData* oldData)
    {
        DrawData* data = oldData == 0 ? new DrawData() : reinterpret_cast<DrawData*>(oldData);

        data->update(objPath, cameraPath, frameContext);

        return data;
    }

    bool VDBDrawOverride::isBounded(
            const MDagPath& objPath,
            const MDagPath& cameraPath) const
    {
        return false;
    }

    bool VDBDrawOverride::init_shaders()
    {
        return true;
    }

}