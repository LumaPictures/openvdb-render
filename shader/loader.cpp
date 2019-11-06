// Copyright 2019 Luma Pictures
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
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
            node->output_type = AI_TYPE_CLOSURE;
            node->name = "openvdb_simple_shader";
            node->node_type = AI_NODE_SHADER;
            break;
        default:
            return false;
    }

    return true;
}
