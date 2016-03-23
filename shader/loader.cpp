#include <ai.h>
#include <cstring>

extern AtNodeMethods* openvdbShaderMethods;
extern AtNodeMethods* openvdbSamplerMethods;

namespace {
    enum
    {
        SHADER_OPENVDB,
        SHADER_SAMPLER
    };
}

node_loader
{
    strcpy(node->version, AI_VERSION);

    switch (i)
    {
        case SHADER_OPENVDB:
            node->methods     = openvdbShaderMethods;
            node->output_type = AI_TYPE_RGB;
            node->name        = "openvdb_shader";
            node->node_type   = AI_NODE_SHADER;
            break;
        case SHADER_SAMPLER:
            node->methods     = openvdbSamplerMethods;
            node->output_type = AI_TYPE_RGB;
            node->name        = "openvdb_sampler";
            node->node_type   = AI_NODE_SHADER;
            break;
        default:
            return false;
    }

    return true;
}
