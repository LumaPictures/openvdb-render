#include "vdb_draw_override.h"

#include <maya/MFnDependencyNode.h>
#include <maya/MDrawContext.h>

namespace MHWRender {

    namespace{

        class DrawData : public MUserData {
        private:
            MBoundingBox m_bbox;
        public:
            DrawData() : MUserData(false)
            {

            }

            void update(VDBVisualizerData* vdb_data)
            {
                if (vdb_data == 0)
                    return;
                m_bbox = vdb_data->bbox;
            }

            void draw(const MDrawContext& context) const
            {
                MStatus status;
                MBoundingBox frustum_box = context.getFrustumBox(&status);

                if (status && !frustum_box.intersects(m_bbox))
                    return;

                const MFloatVector mn = m_bbox.min();
                const MFloatVector mx = m_bbox.max();

                glBegin(GL_LINES);

                glVertex3f(mn.x, mn.y, mn.z);
                glVertex3f(mx.x, mn.y, mn.z);

                glVertex3f(mn.x, mx.y, mn.z);
                glVertex3f(mx.x, mx.y, mn.z);

                glVertex3f(mn.x, mn.y, mx.z);
                glVertex3f(mx.x, mn.y, mx.z);

                glVertex3f(mn.x, mx.y, mx.z);
                glVertex3f(mx.x, mx.y, mx.z);

                // -------------------------

                glVertex3f(mn.x, mn.y, mn.z);
                glVertex3f(mn.x, mx.y, mn.z);

                glVertex3f(mx.x, mn.y, mn.z);
                glVertex3f(mx.x, mx.y, mn.z);

                glVertex3f(mn.x, mn.y, mx.z);
                glVertex3f(mn.x, mx.y, mx.z);

                glVertex3f(mx.x, mn.y, mx.z);
                glVertex3f(mx.x, mx.y, mx.z);

                // -------------------------

                glVertex3f(mn.x, mn.y, mn.z);
                glVertex3f(mn.x, mn.y, mx.z);

                glVertex3f(mn.x, mx.y, mn.z);
                glVertex3f(mn.x, mx.y, mx.z);

                glVertex3f(mx.x, mn.y, mn.z);
                glVertex3f(mx.x, mn.y, mx.z);

                glVertex3f(mx.x, mx.y, mn.z);
                glVertex3f(mx.x, mx.y, mx.z);

                glEnd();
            }
        };

    }

    VDBDrawOverride::VDBDrawOverride(const MObject& obj) : MPxDrawOverride(obj, draw_callback)
    {
        MFnDependencyNode dnode(obj);
        p_vdb_visualizer = dynamic_cast<VDBVisualizerShape*>(dnode.userNode());
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
        const DrawData* draw_data = reinterpret_cast<const DrawData*>(data);
        if (draw_data != 0)
            draw_data->draw(context);
    }

    MString VDBDrawOverride::registrantId("VDBVisualizerDrawOverride");

    MHWRender::DrawAPI VDBDrawOverride::supportedDrawAPIs() const
    {
        return kOpenGL;
    }

    MUserData* VDBDrawOverride::prepareForDraw(
            const MDagPath&,
            const MDagPath&,
            const MFrameContext&,
            MUserData* oldData)
    {
        DrawData* data = oldData == 0 ? new DrawData() : reinterpret_cast<DrawData*>(oldData);

        data->update(p_vdb_visualizer->get_update());

        return data;
    }

    bool VDBDrawOverride::isBounded(
            const MDagPath&,
            const MDagPath&) const
    {
        return false;
    }

    bool VDBDrawOverride::init_shaders()
    {
        return true;
    }

}