#include <ai.h>
#include <cstring>

extern AtNodeMethods* openvdbShaderMethods;
extern AtNodeMethods* openvdbSamplerMethods;
extern AtNodeMethods* openvdbSimpleShaderMethods;

namespace {
    enum {
        SHADER_SAMPLER,
        SHADER_SIMPLE_OPENVDB,
#ifndef ARNOLD5
        SHADER_OPENVDB,
#endif
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
            node->output_type = AI_TYPE_RGB;
            node->name = "openvdb_simple_shader";
            node->node_type = AI_NODE_SHADER;
            break;
#ifndef ARNOLD5
        case SHADER_OPENVDB:
            node->methods = openvdbShaderMethods;
            node->output_type = AI_TYPE_RGB;
            node->name = "openvdb_shader";
            node->node_type = AI_NODE_SHADER;
            break;
#endif
        default:
            return false;
    }

    return true;
}
