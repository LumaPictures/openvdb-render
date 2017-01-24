#include "point_sorter.h"

#include <iostream>

bool cuda_available() {
    int device_count = 0;
    cudaGetDeviceCount(&device_count);

    std::cerr << "[openvdb_render] Checking for CUDA support!" << std::endl;
    for(int i = 0; i < device_count; ++i)
    {
        cudaDeviceProp prop;
        cudaGetDeviceProperties(&prop, i);
        std::cerr << "\t\tFound CUDA device : " << prop.name << std::endl;
    }

    return device_count > 0;
}

void sort_points(PointData* data, size_t point_count) {

}
