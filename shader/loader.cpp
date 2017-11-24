#include <ai.h>

extern AtNodeMethods* openvdbSamplerMethods;
extern AtNodeMethods* openvdbSimpleShaderMethods;

namespace {
    enum {
        SHADER_SAMPLER,
        SHADER_SIMPLE_OPENVDB,
    };
}

node_loader
{
    strcpy(node->version, AI_VERSION);

    switch (i) {
        case SHADER_SAMPLER:
            node->methods = openvdbSamplerMethods;
            node->output_type = AI_TYPE_RGB;
            node->name = "openvdb_sampler";
            node->node_type = AI_NODE_SHADER;
            break;
        case SHADER_SIMPLE_OPENVDB:
            node->methods = openvdbSimpleShaderMethods;
#ifdef ARNOLD5
            node->output_type = AI_TYPE_CLOSURE;
#else
            node->output_type = AI_TYPE_RGB;
#endif
            node->name = "openvdb_simple_shader";
            node->node_type = AI_NODE_SHADER;
            break;
        default:
            return false;
    }

    return true;
}
