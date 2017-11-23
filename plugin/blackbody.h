#include <maya/MFloatVector.h>
#include <vector>

namespace Blackbody {
    constexpr float TEMPERATURE_MIN = 800.0f;
    constexpr float TEMPERATURE_MAX = 12000.0f;
    constexpr int LUT_SIZE = 256;
    extern const float LUT_NORMALIZER;
    extern const std::vector<MFloatVector> LUT;
};
