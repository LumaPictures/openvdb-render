#pragma once

#include <cstddef>

struct PointData {
    float pos[4];
    float color[4];
};

bool cuda_available();
void sort_points(PointData* data, size_t point_count);
