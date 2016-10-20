#include "vdb_subscene_override.h"

#include <maya/MHWGeometry.h>
#include <maya/MShaderManager.h>

namespace {
    static bool operator!=(const MBoundingBox& a, const MBoundingBox& b)
    {
        return a.min() != b.min() || a.max() != b.max();
    }

    static bool operator!=(const Gradient& a, const Gradient& b)
    {
        return a.is_different(b);
    }

    template <typename T>
    bool setup_parameter(T& target, const T& source)
    {
        if (target != source)
        {
            target = source;
            return true;
        }
        else
            return false;
    }
}

namespace MHWRender {
    struct VDBSubSceneOverrideData {
        MBoundingBox bbox;

        MFloatVector scattering_color;
        MFloatVector attenuation_color;
        MFloatVector emission_color;

        std::string attenuation_channel;
        std::string scattering_channel;
        std::string emission_channel;

        Gradient scattering_gradient;
        Gradient attenuation_gradient;
        Gradient emission_gradient;

        std::unique_ptr<openvdb::io::File> vdb_file;
        openvdb::GridBase::ConstPtr scattering_grid;
        openvdb::GridBase::ConstPtr attenuation_grid;
        openvdb::GridBase::ConstPtr emission_grid;

        float point_size;
        float point_jitter;

        int point_skip;
        VDBDisplayMode display_mode;

        bool has_changed;

        VDBSubSceneOverrideData()
        {

        }

        ~VDBSubSceneOverrideData()
        {
            clear();
        }

        void clear()
        {
            scattering_grid = 0;
            attenuation_grid = 0;
            emission_grid = 0;
            vdb_file.reset();
        }

        bool update(const VDBVisualizerData* data)
        {
            // TODO: we can limit some of the comparisons to the display mode
            // ie, we don't need to compare certain things if we are using the bounding
            // box mode
            bool ret = false;
            ret |= setup_parameter(display_mode, data->display_mode);
            ret |= setup_parameter(bbox, data->bbox);
            ret |= setup_parameter(scattering_color, data->scattering_color);
            ret |= setup_parameter(attenuation_color, data->attenuation_color);
            ret |= setup_parameter(emission_color, data->emission_color);
            ret |= setup_parameter(attenuation_channel, data->attenuation_channel);
            ret |= setup_parameter(scattering_channel, data->scattering_channel);
            ret |= setup_parameter(emission_channel, data->emission_channel);
            ret |= setup_parameter(scattering_gradient, data->scattering_gradient);
            ret |= setup_parameter(attenuation_gradient, data->attenuation_gradient);
            ret |= setup_parameter(emission_gradient, data->emission_gradient);
            ret |= setup_parameter(point_skip, data->point_skip);

            point_size = data->point_size;
            point_jitter = data->point_jitter; // We can jitter in the vertex shader. Hopefully

            const std::string& filename = data->vdb_path;
            auto open_file = [&] () {
                ret |= true;
                clear();

                try{
                    vdb_file.reset(new openvdb::io::File(filename));
                }
                catch(...){
                    vdb_file.reset();
                }
            };

            if (filename.empty())
            {
                ret |= true;
                clear();
            }
            else if (data->vdb_file == nullptr)
                open_file();
            else if (filename != data->vdb_file->filename())
                open_file();

            return ret;
        }
    };

    MString VDBSubSceneOverride::registrantId("VDBVisualizerSubSceneOverride");

    MPxSubSceneOverride* VDBSubSceneOverride::creator(const MObject& obj)
    {
        return new VDBSubSceneOverride(obj);
    }

    VDBSubSceneOverride::VDBSubSceneOverride(const MObject& obj) : MPxSubSceneOverride(obj),
                                                                   p_data(new VDBSubSceneOverrideData)
    {
        MFnDependencyNode dnode(obj);
        p_vdb_visualizer = dynamic_cast<VDBVisualizerShape*>(dnode.userNode());
    }

    VDBSubSceneOverride::~VDBSubSceneOverride()
    {
    }

    MHWRender::DrawAPI VDBSubSceneOverride::supportedDrawAPIs() const
    {
#if MAYA_API_VERSION >= 201600
        return kOpenGLCoreProfile | kOpenGL;
#else
        return kOpenGL;
#endif
    }

    void VDBSubSceneOverride::update(MSubSceneContainer& /*container*/, const MFrameContext& /*frameContext*/)
    {
        static int counter = 0;
        std::cerr << "[openvdb_render] Updating " << ++counter << std::endl;
    }

    bool VDBSubSceneOverride::requiresUpdate(const MSubSceneContainer& /*container*/, const MFrameContext& /*frameContext*/) const
    {
        const VDBVisualizerData* data = p_vdb_visualizer->get_update();
        if (data == nullptr)
        {
            if (p_data->vdb_file != nullptr)
            {
                p_data->clear();
                return true;
            }
            else
                return false;
        }
        else
            return p_data->update(data);
    }
}
