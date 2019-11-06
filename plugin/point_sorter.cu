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
#include "point_sorter.h"

#include <iostream>
#include <thrust/device_vector.h>
#include <thrust/copy.h>
#include <thrust/sort.h>

bool cuda_available() {
    int device_count = 0;
    cudaGetDeviceCount(&device_count);

    std::cerr << "[openvdb_render] Checking for CUDA support!" << std::endl;
    for(int i = 0; i < device_count; ++i)
    {
        cudaDeviceProp prop;
        cudaGetDeviceProperties(&prop, i);
        std::cerr << "\tFound CUDA device : " << prop.name << std::endl;
    }

    return device_count > 0;
}

struct sort_points_functor
{
    float camera_pos[3];

    sort_points_functor(const float* cp) {
        camera_pos[0] = cp[0];
        camera_pos[1] = cp[1];
        camera_pos[2] = cp[2];
    }

    __host__ __device__
    bool operator()(PointData x, PointData y)
    {
        float rx = x.pos[0] - camera_pos[0];
        rx = rx * rx;
        float t = x.pos[1] - camera_pos[1];
        rx += t * t;
        t = x.pos[2] - camera_pos[2];
        rx += t * t;
        float ry = y.pos[0] - camera_pos[0];
        ry = ry * ry;
        t = y.pos[1] - camera_pos[1];
        ry += t * t;
        t = y.pos[2] - camera_pos[2];
        ry += t * t;
        return rx > ry;
    }
};

void sort_points(PointData* data, size_t point_count, const float* camera_position) {
    thrust::device_vector<PointData> device_vector(point_count);
    thrust::copy(data, data + point_count, device_vector.begin());
    thrust::sort(device_vector.begin(), device_vector.end(), sort_points_functor(camera_position));
    thrust::copy(device_vector.begin(), device_vector.end(), data);
}
