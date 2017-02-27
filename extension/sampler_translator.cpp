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
