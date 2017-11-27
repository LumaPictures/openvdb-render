#include <ai.h>

#include "gradient.hpp"

namespace {
    struct ShaderData {
        Gradient gradient;
        AtString channel;
        int interpolation;

        void update(AtNode* node)
        {
            channel = AtString(AiNodeGetStr(node, "channel"));
            interpolation = AiNodeGetInt(node, "interpolation");
            gradient.update("base", node);
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

    Gradient::parameters("base", params, nentry);

    AiMetaDataSetBool(nentry, 0, "maya.hide", true);
    AiMetaDataSetBool(nentry, "channel", "volume_sample", true);
}

static void Initialize(AtNode* node)
//node_initialize
{
    AiNodeSetLocalData(node, new ShaderData());
}

node_update
{
    auto* data = reinterpret_cast<ShaderData*>(AiNodeGetLocalData(node));
    data->update(node);
}

node_finish
{
    delete reinterpret_cast<ShaderData*>(AiNodeGetLocalData(node));
}

shader_evaluate
{
    const auto* data = reinterpret_cast<const ShaderData*>(AiNodeGetLocalData(node));
    sg->out.RGB() = data->gradient.evaluate_arnold(sg, data->channel, data->interpolation);
}
