#pragma once

#include <openvdb/openvdb.h>

#include <maya/MBoundingBox.h>
#include <maya/MMatrix.h>

inline bool read_grid_transformed_bbox_vertices(openvdb::GridBase::ConstPtr grid, MFloatVector& bbox_min, MFloatVector& bbox_max)
{
    const openvdb::Vec3i file_bbox_min = grid->metaValue<openvdb::Vec3i>("file_bbox_min");
    if (file_bbox_min.x() == std::numeric_limits<int>::max() ||
        file_bbox_min.y() == std::numeric_limits<int>::max() ||
        file_bbox_min.z() == std::numeric_limits<int>::max())
        return false;
    const openvdb::Vec3i file_bbox_max = grid->metaValue<openvdb::Vec3i>("file_bbox_max");
    if (file_bbox_max.x() == std::numeric_limits<int>::min() ||
        file_bbox_max.y() == std::numeric_limits<int>::min() ||
        file_bbox_max.z() == std::numeric_limits<int>::min())
        return false;
    const openvdb::math::Transform& transform = grid->transform();
    const openvdb::Vec3d bbmin = transform.indexToWorld(file_bbox_min);
    const openvdb::Vec3d bbmax = transform.indexToWorld(file_bbox_max);

    bbox_min.x = static_cast<float>(bbmin.x());
    bbox_min.y = static_cast<float>(bbmin.y());
    bbox_min.z = static_cast<float>(bbmin.z());

    bbox_max.x = static_cast<float>(bbmax.x());
    bbox_max.y = static_cast<float>(bbmax.y());
    bbox_max.z = static_cast<float>(bbmax.z());

    return true;
}

inline bool read_transformed_bounding_box(openvdb::GridBase::ConstPtr grid, MBoundingBox& bbox)
{
    const openvdb::Vec3i file_bbox_min = grid->metaValue<openvdb::Vec3i>("file_bbox_min");
    if (file_bbox_min.x() == std::numeric_limits<int>::max() ||
        file_bbox_min.y() == std::numeric_limits<int>::max() ||
        file_bbox_min.z() == std::numeric_limits<int>::max())
        return false;
    const openvdb::Vec3i file_bbox_max = grid->metaValue<openvdb::Vec3i>("file_bbox_max");
    if (file_bbox_max.x() == std::numeric_limits<int>::min() ||
        file_bbox_max.y() == std::numeric_limits<int>::min() ||
        file_bbox_max.z() == std::numeric_limits<int>::min())
        return false;
    const openvdb::math::Transform& transform = grid->transform();
    const openvdb::Vec3d bbmin = transform.indexToWorld(file_bbox_min);
    const openvdb::Vec3d bbmax = transform.indexToWorld(file_bbox_max);

    bbox.expand(MPoint(bbmin.x(), bbmin.y(), bbmin.z(), 1.0));

    bbox.expand(MPoint(bbmax.x(), bbmin.y(), bbmin.z(), 1.0));
    bbox.expand(MPoint(bbmin.x(), bbmax.y(), bbmin.z(), 1.0));
    bbox.expand(MPoint(bbmin.x(), bbmin.y(), bbmax.z(), 1.0));

    bbox.expand(MPoint(bbmax.x(), bbmin.y(), bbmax.z(), 1.0));
    bbox.expand(MPoint(bbmax.x(), bbmax.y(), bbmin.z(), 1.0));
    bbox.expand(MPoint(bbmin.x(), bbmax.y(), bbmax.z(), 1.0));

    bbox.expand(MPoint(bbmax.x(), bbmax.y(), bbmax.z(), 1.0));

    return true;
}
