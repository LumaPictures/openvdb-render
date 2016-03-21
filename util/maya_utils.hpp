#pragma once

#include <openvdb/openvdb.h>

#include <maya/MBoundingBox.h>

inline void read_bounding_box(openvdb::GridBase::ConstPtr grid, MBoundingBox& bbox)
{
    const openvdb::Vec3i file_bbox_min = grid->metaValue<openvdb::Vec3i>("file_bbox_min");
    if (file_bbox_min.x() == std::numeric_limits<int>::max() ||
        file_bbox_min.y() == std::numeric_limits<int>::max() ||
        file_bbox_min.z() == std::numeric_limits<int>::max())
        return;
    const openvdb::Vec3i file_bbox_max = grid->metaValue<openvdb::Vec3i>("file_bbox_max");
    if (file_bbox_max.x() == std::numeric_limits<int>::min() ||
        file_bbox_max.y() == std::numeric_limits<int>::min() ||
        file_bbox_max.z() == std::numeric_limits<int>::min())
        return;
    const openvdb::Vec3d voxel_size = grid->voxelSize();
    openvdb::Vec3d point_in_bbox = file_bbox_min * voxel_size;
    bbox.expand(MPoint(point_in_bbox.x(), point_in_bbox.y(), point_in_bbox.z(), 1.0));
    point_in_bbox = file_bbox_max * voxel_size;
    bbox.expand(MPoint(point_in_bbox.x(), point_in_bbox.y(), point_in_bbox.z(), 1.0));
}
