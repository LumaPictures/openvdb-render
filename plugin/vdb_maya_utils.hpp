#pragma once

#include <openvdb/openvdb.h>

#include <maya/MBoundingBox.h>
#include <maya/MMatrix.h>

inline bool read_grid_transformed_bbox_wire(openvdb::GridBase::ConstPtr grid, std::array<MFloatVector, 8>& vertices)
{
    const openvdb::Vec3i file_bbox_min = grid->metaValue<openvdb::Vec3i>("file_bbox_min");
    if (file_bbox_min.x() == std::numeric_limits<int>::max() ||
        file_bbox_min.y() == std::numeric_limits<int>::max() ||
        file_bbox_min.z() == std::numeric_limits<int>::max()) {
            return false;
    }
    const openvdb::Vec3i file_bbox_max = grid->metaValue<openvdb::Vec3i>("file_bbox_max");
    if (file_bbox_max.x() == std::numeric_limits<int>::min() ||
        file_bbox_max.y() == std::numeric_limits<int>::min() ||
        file_bbox_max.z() == std::numeric_limits<int>::min()) {
            return false;
    }
    const openvdb::math::Transform& transform = grid->transform();

    openvdb::Vec3d pnt = transform.indexToWorld(
        openvdb::Vec3i(file_bbox_min.x(), file_bbox_min.y(), file_bbox_min.z()));
    vertices[0] = MFloatVector(static_cast<float>(pnt.x()), static_cast<float>(pnt.y()), static_cast<float>(pnt.z()));

    pnt = transform.indexToWorld(openvdb::Vec3i(file_bbox_min.x(), file_bbox_max.y(), file_bbox_min.z()));
    vertices[1] = MFloatVector(static_cast<float>(pnt.x()), static_cast<float>(pnt.y()), static_cast<float>(pnt.z()));

    pnt = transform.indexToWorld(openvdb::Vec3i(file_bbox_min.x(), file_bbox_max.y(), file_bbox_max.z()));
    vertices[2] = MFloatVector(static_cast<float>(pnt.x()), static_cast<float>(pnt.y()), static_cast<float>(pnt.z()));

    pnt = transform.indexToWorld(openvdb::Vec3i(file_bbox_min.x(), file_bbox_min.y(), file_bbox_max.z()));
    vertices[3] = MFloatVector(static_cast<float>(pnt.x()), static_cast<float>(pnt.y()), static_cast<float>(pnt.z()));

    pnt = transform.indexToWorld(openvdb::Vec3i(file_bbox_max.x(), file_bbox_min.y(), file_bbox_min.z()));
    vertices[4] = MFloatVector(static_cast<float>(pnt.x()), static_cast<float>(pnt.y()), static_cast<float>(pnt.z()));

    pnt = transform.indexToWorld(openvdb::Vec3i(file_bbox_max.x(), file_bbox_max.y(), file_bbox_min.z()));
    vertices[5] = MFloatVector(static_cast<float>(pnt.x()), static_cast<float>(pnt.y()), static_cast<float>(pnt.z()));

    pnt = transform.indexToWorld(openvdb::Vec3i(file_bbox_max.x(), file_bbox_max.y(), file_bbox_max.z()));
    vertices[6] = MFloatVector(static_cast<float>(pnt.x()), static_cast<float>(pnt.y()), static_cast<float>(pnt.z()));

    pnt = transform.indexToWorld(openvdb::Vec3i(file_bbox_max.x(), file_bbox_min.y(), file_bbox_max.z()));
    vertices[7] = MFloatVector(static_cast<float>(pnt.x()), static_cast<float>(pnt.y()), static_cast<float>(pnt.z()));

    return true;
}

inline bool grid_is_empty(openvdb::GridBase::ConstPtr grid)
{
    const openvdb::Vec3i file_bbox_min = grid->metaValue<openvdb::Vec3i>("file_bbox_min");
    if (file_bbox_min.x() == std::numeric_limits<int>::max() ||
        file_bbox_min.y() == std::numeric_limits<int>::max() ||
        file_bbox_min.z() == std::numeric_limits<int>::max()) {
            return true;
    }
    const openvdb::Vec3i file_bbox_max = grid->metaValue<openvdb::Vec3i>("file_bbox_max");
    return file_bbox_max.x() == std::numeric_limits<int>::min() ||
           file_bbox_max.y() == std::numeric_limits<int>::min() ||
           file_bbox_max.z() == std::numeric_limits<int>::min();
}

inline bool read_transformed_bounding_box(openvdb::GridBase::ConstPtr grid, MBoundingBox& bbox)
{
    const openvdb::Vec3i file_bbox_min = grid->metaValue<openvdb::Vec3i>("file_bbox_min");
    if (file_bbox_min.x() == std::numeric_limits<int>::max() ||
        file_bbox_min.y() == std::numeric_limits<int>::max() ||
        file_bbox_min.z() == std::numeric_limits<int>::max()) {
            return false;
    }
    const openvdb::Vec3i file_bbox_max = grid->metaValue<openvdb::Vec3i>("file_bbox_max");
    if (file_bbox_max.x() == std::numeric_limits<int>::min() ||
        file_bbox_max.y() == std::numeric_limits<int>::min() ||
        file_bbox_max.z() == std::numeric_limits<int>::min()) {
            return false;
    }
    const openvdb::math::Transform& transform = grid->transform();

    openvdb::Vec3d pnt = transform.indexToWorld(
        openvdb::Vec3i(file_bbox_min.x(), file_bbox_min.y(), file_bbox_min.z()));
    bbox.expand(MPoint(pnt.x(), pnt.y(), pnt.z(), 1.0));

    pnt = transform.indexToWorld(openvdb::Vec3i(file_bbox_max.x(), file_bbox_min.y(), file_bbox_min.z()));
    bbox.expand(MPoint(pnt.x(), pnt.y(), pnt.z(), 1.0));

    pnt = transform.indexToWorld(openvdb::Vec3i(file_bbox_min.x(), file_bbox_max.y(), file_bbox_min.z()));
    bbox.expand(MPoint(pnt.x(), pnt.y(), pnt.z(), 1.0));

    pnt = transform.indexToWorld(openvdb::Vec3i(file_bbox_min.x(), file_bbox_min.y(), file_bbox_max.z()));
    bbox.expand(MPoint(pnt.x(), pnt.y(), pnt.z(), 1.0));

    pnt = transform.indexToWorld(openvdb::Vec3i(file_bbox_max.x(), file_bbox_max.y(), file_bbox_min.z()));
    bbox.expand(MPoint(pnt.x(), pnt.y(), pnt.z(), 1.0));

    pnt = transform.indexToWorld(openvdb::Vec3i(file_bbox_max.x(), file_bbox_min.y(), file_bbox_max.z()));
    bbox.expand(MPoint(pnt.x(), pnt.y(), pnt.z(), 1.0));

    pnt = transform.indexToWorld(openvdb::Vec3i(file_bbox_min.x(), file_bbox_max.y(), file_bbox_max.z()));
    bbox.expand(MPoint(pnt.x(), pnt.y(), pnt.z(), 1.0));

    pnt = transform.indexToWorld(openvdb::Vec3i(file_bbox_max.x(), file_bbox_max.y(), file_bbox_max.z()));
    bbox.expand(MPoint(pnt.x(), pnt.y(), pnt.z(), 1.0));

    return true;
}

constexpr float LINEAR_FROM_SRGB_EXPONENT = 2.2f;
inline void LinearFromSRGB(float* data, size_t n)
{
    for (size_t i = 0; i < n; ++i)
        data[i] = std::pow(data[i], LINEAR_FROM_SRGB_EXPONENT);
}

inline MFloatVector LinearFromSRGB(const MFloatVector& color)
{
    return { std::pow(color.x, LINEAR_FROM_SRGB_EXPONENT), std::pow(color.y, LINEAR_FROM_SRGB_EXPONENT), std::pow(color.z, LINEAR_FROM_SRGB_EXPONENT) };
}

constexpr float SRGB_FROM_LINEAR_EXPONENT = 1.0f / LINEAR_FROM_SRGB_EXPONENT;
inline MFloatVector SRGBFromLinear(const MFloatVector& color)
{
    return { std::pow(color.x, SRGB_FROM_LINEAR_EXPONENT), std::pow(color.y, SRGB_FROM_LINEAR_EXPONENT), std::pow(color.z, SRGB_FROM_LINEAR_EXPONENT) };
}
