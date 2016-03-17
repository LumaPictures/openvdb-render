#include <ai.h>
#include <cstring>

extern AtNodeMethods* openvdbShaderMethods;

namespace {
    enum
    {
        SHADER_OPENVDB
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
        default:
            return false;
    }

    return true;
}
