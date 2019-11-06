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
#include "sampler_translator.h"

namespace {
    const unsigned int num_ramp_samples = 256;
}

void* SamplerTranslator::creator()
{
    return new SamplerTranslator();
}

AtNode* SamplerTranslator::CreateArnoldNodes()
{
    return AddArnoldNode("openvdb_sampler");
}

void SamplerTranslator::Export(AtNode* shader)
{
    ProcessParameter(shader, "channel", AI_TYPE_STRING, "channel");
    ProcessParameter(shader, "position_offset", AI_TYPE_VECTOR, "positionOffset");
    ProcessParameter(shader, "interpolation", AI_TYPE_INT, "interpolation");

    std::vector<std::string> gradient_names = {"base"};
    export_gradients(shader, gradient_names);
}
