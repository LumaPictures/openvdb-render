#pragma once

#include <maya/MHWGeometry.h>
#include <maya/MShaderManager.h>

#include <openvdb/tools/Interpolation.h>
#include <openvdb/Exceptions.h>

class RGBSampler {
    MFloatVector default_color;
public:
    RGBSampler(const MFloatVector& dc = MFloatVector(1.0f, 1.0f, 1.0f)) : default_color(dc)
    {
    }

    virtual ~RGBSampler()
    {
    }

    virtual MFloatVector get_rgb(const openvdb::Vec3d&) const
    {
        return default_color;
    }
};

class FloatToRGBSampler : public RGBSampler {
    typedef openvdb::tools::GridSampler<openvdb::FloatGrid, openvdb::tools::BoxSampler> sampler_type;
    sampler_type m_sampler;
public:
    FloatToRGBSampler(openvdb::FloatGrid::ConstPtr grid) : m_sampler(*grid)
    {
    }

    ~FloatToRGBSampler()
    {
    }

    MFloatVector get_rgb(const openvdb::Vec3d& wpos) const
    {
        const float value = m_sampler.wsSample(wpos);
        return MFloatVector(value, value, value);
    }
};

class Vec3SToRGBSampler : public RGBSampler {
    typedef openvdb::tools::GridSampler<openvdb::Vec3SGrid, openvdb::tools::BoxSampler> sampler_type;
    sampler_type m_sampler;
public:
    Vec3SToRGBSampler(openvdb::Vec3SGrid::ConstPtr grid) : m_sampler(*grid)
    {
    }

    ~Vec3SToRGBSampler()
    {
    }

    MFloatVector get_rgb(const openvdb::Vec3d& wpos) const
    {
        const openvdb::Vec3s value = m_sampler.wsSample(wpos);
        return MFloatVector(value.x(), value.y(), value.z());
    }
};

class FloatVoxelIterator {
protected:
    size_t m_active_voxel_count;
public:
    FloatVoxelIterator() : m_active_voxel_count(1)
    {
    }

    virtual ~FloatVoxelIterator()
    {
    }

    virtual bool is_valid() const
    {
        return false;
    }

    virtual void get_next()
    {
    }

    virtual float get_value() const
    {
        return 0.0f;
    }

    virtual openvdb::Coord get_coord() const
    {
        return openvdb::Coord(0, 0, 0);
    }

    size_t get_active_voxels() const
    {
        return m_active_voxel_count;
    }
};

class FloatToFloatVoxelIterator : public FloatVoxelIterator {
    typedef openvdb::FloatGrid grid_type;
    grid_type::ValueOnCIter iter;
public:
    FloatToFloatVoxelIterator(grid_type::ConstPtr grid) : iter(grid->beginValueOn())
    {
        m_active_voxel_count = grid->activeVoxelCount();
    }

    ~FloatToFloatVoxelIterator()
    {
    }

    bool is_valid() const
    {
        return iter.test();
    }

    void get_next()
    {
        ++iter;
    }

    float get_value() const
    {
        return iter.getValue();
    }

    openvdb::Coord get_coord() const
    {
        return iter.getCoord();
    }
};

class Vec3SToFloatVoxelIterator : public FloatVoxelIterator {
    typedef openvdb::Vec3SGrid grid_type;
    grid_type::ValueOnCIter iter;
public:
    Vec3SToFloatVoxelIterator(grid_type::ConstPtr grid) : iter(grid->cbeginValueOn())
    {
        m_active_voxel_count = grid->activeVoxelCount();
    }

    ~Vec3SToFloatVoxelIterator()
    {
    }

    bool is_valid() const
    {
        return iter.test();
    }

    void get_next()
    {
        ++iter;
    }

    float get_value() const
    {
        openvdb::Vec3s value = iter.getValue();
        return (value.x() + value.y() + value.z()) / 3.0f;
    }

    openvdb::Coord get_coord() const
    {
        return iter.getCoord();
    }
};

bool inline operator!=(const MBoundingBox& a, const MBoundingBox& b)
{
    return a.min() != b.min() || a.max() != b.max();
}

bool inline operator!=(const Gradient& a, const Gradient& b)
{
    return a.is_different(b);
}

template<typename T>
inline bool setup_parameter(T& target, const T& source)
{
    if (target != source) {
        target = source;
        return true;
    } else {
        return false;
    }
}

// this is not part of c++11
//template<typename T, typename... Args>
//inline std::unique_ptr<T> make_unique(Args&&... args) {
//    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
//}

inline void set_bbox_indices(const unsigned int num_bboxes, MHWRender::MIndexBuffer* bbox_indices)
{
    unsigned int* indices = reinterpret_cast<unsigned int*>(bbox_indices->acquire(24 * num_bboxes, true));
    unsigned int id = 0;
    for (unsigned int bbox = 0; bbox < num_bboxes; ++bbox) {
        const unsigned int bbox_base = bbox * 8;
        indices[id++] = bbox_base;
        indices[id++] = bbox_base + 1;
        indices[id++] = bbox_base + 1;
        indices[id++] = bbox_base + 2;
        indices[id++] = bbox_base + 2;
        indices[id++] = bbox_base + 3;
        indices[id++] = bbox_base + 3;
        indices[id++] = bbox_base;

        indices[id++] = bbox_base + 4;
        indices[id++] = bbox_base + 5;
        indices[id++] = bbox_base + 5;
        indices[id++] = bbox_base + 6;
        indices[id++] = bbox_base + 6;
        indices[id++] = bbox_base + 7;
        indices[id++] = bbox_base + 7;
        indices[id++] = bbox_base + 4;

        indices[id++] = bbox_base;
        indices[id++] = bbox_base + 4;
        indices[id++] = bbox_base + 1;
        indices[id++] = bbox_base + 5;
        indices[id++] = bbox_base + 2;
        indices[id++] = bbox_base + 6;
        indices[id++] = bbox_base + 3;
        indices[id++] = bbox_base + 7;
    }
    bbox_indices->commit(indices);
}

inline void set_bbox_indices_triangles(const unsigned int num_bboxes, MHWRender::MIndexBuffer* bbox_indices)
{
    unsigned int* indices = reinterpret_cast<unsigned int*>(bbox_indices->acquire(36 * num_bboxes, true));
    unsigned int id = 0;
    for (unsigned int bbox = 0; bbox < num_bboxes; ++bbox)
    {
        const unsigned int bbox_base = bbox * 8;

        indices[id++] = bbox_base + 0; // mmm
        indices[id++] = bbox_base + 1; // mMm
        indices[id++] = bbox_base + 3; // mmM
        indices[id++] = bbox_base + 3; // mmM
        indices[id++] = bbox_base + 1; // mMm
        indices[id++] = bbox_base + 2; // mMM

        indices[id++] = bbox_base + 4; // Mmm
        indices[id++] = bbox_base + 7; // MmM
        indices[id++] = bbox_base + 5; // MMm
        indices[id++] = bbox_base + 5; // MMm
        indices[id++] = bbox_base + 7; // MmM
        indices[id++] = bbox_base + 6; // MMM

        indices[id++] = bbox_base + 0; // mmm
        indices[id++] = bbox_base + 4; // Mmm
        indices[id++] = bbox_base + 1; // mMm
        indices[id++] = bbox_base + 1; // mMm
        indices[id++] = bbox_base + 4; // Mmm
        indices[id++] = bbox_base + 5; // MMm

        indices[id++] = bbox_base + 3; // mmM
        indices[id++] = bbox_base + 2; // mMM
        indices[id++] = bbox_base + 7; // MmM
        indices[id++] = bbox_base + 7; // MmM
        indices[id++] = bbox_base + 2; // mMM
        indices[id++] = bbox_base + 6; // MMM

        indices[id++] = bbox_base + 0; // mmm
        indices[id++] = bbox_base + 3; // mmM
        indices[id++] = bbox_base + 4; // Mmm
        indices[id++] = bbox_base + 4; // Mmm
        indices[id++] = bbox_base + 3; // mmM
        indices[id++] = bbox_base + 7; // MmM

        indices[id++] = bbox_base + 1; // mMm
        indices[id++] = bbox_base + 5; // MMm
        indices[id++] = bbox_base + 2; // mMM
        indices[id++] = bbox_base + 2; // mMM
        indices[id++] = bbox_base + 5; // MMm
        indices[id++] = bbox_base + 6; // MMM
    }
    bbox_indices->commit(indices);
}
