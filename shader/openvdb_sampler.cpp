#include <ai.h>

#include "gradient.hpp"

namespace {
    struct ShaderData {
        Gradient gradient;
        AtString channel;
        int interpolation;

        void update(AtNode* node
#ifndef ARNOLD5
                    , AtParamValue* params
#endif
        )
        {
            channel = AtString(AiNodeGetStr(node, "channel"));
            interpolation = AiNodeGetInt(node, "interpolation");
            gradient.update("base", node
#ifndef ARNOLD5
                            , params
#endif
            );
        }

        static void* operator new(size_t size)
        {
            return AiMalloc(static_cast<unsigned int>(size));
        }

        static void operator delete(void* d)
        {
            AiFree(d);
        }
    };

    const char* interpolations[] = {"closest", "trilinear", "tricubic", nullptr};
}

AI_SHADER_NODE_EXPORT_METHODS(openvdbSamplerMethods);

node_parameters
{
    AiParameterStr("channel", "");
    AiParameterVec("position_offset", 0, 0, 0);
    AiParameterEnum("interpolation", AI_VOLUME_INTERP_TRILINEAR, interpolations);

#ifdef ARNOLD5
    auto& _mds = nentry;
#else
    auto& _mds = mds;
#endif

    Gradient::parameters("base", params, _mds);

    AiMetaDataSetBool(_mds, 0, "maya.hide", true);
    AiMetaDataSetBool(_mds, "channel", "volume_sample", true);
}

static void Initialize(AtNode* node
#ifndef ARNOLD5
                       , AtParamValue*
#endif
)
//node_initialize
{
    AiNodeSetLocalData(node, new ShaderData());
}

node_update
{
    auto* data = reinterpret_cast<ShaderData*>(AiNodeGetLocalData(node));
    data->update(node
#ifndef ARNOLD5
        , params
#endif
    );
}

node_finish
{
    delete reinterpret_cast<ShaderData*>(AiNodeGetLocalData(node));
}

shader_evaluate
{
    const auto* data = reinterpret_cast<const ShaderData*>(AiNodeGetLocalData(node));
#ifdef ARNOLD5
    sg->out.RGB()
#else
    sg->out.RGB
#endif
        = data->gradient.evaluate_arnold(sg, data->channel, data->interpolation);
}
