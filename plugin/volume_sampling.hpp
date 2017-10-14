#pragma once

#include <openvdb/openvdb.h>
#include <openvdb/tools/MultiResGrid.h>

#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_for.h>

#include <algorithm>
#include <limits>
#include <cassert>
#include <cstdint>


namespace volume_sampling {

template <typename RealType>
struct SampleBufferHeader
{
    RealType value_range[2];
    RealType size[3];
    RealType origin[3];
};

// The AUTO filter mode uses a box filter if the sampling_extents is finer than
// the grid resolution. Otherwise it uses MultiResGrid filtering.
enum class FilterMode { BOX, MULTIRES, AUTO };

// A callable ProgressCallback parameter can be passed to the sampleGrid function.
// The progress callback is called by passing a single uint32_t parameter to
// it containing the number of samples taken since the previous call.
// The callback should return a boolean indicating whether the sampling should
// proceed (false: interrupt sampling).
// The default ProgressCallback does nothing and always returns true.
struct ProgressCallbackNoOp {
    bool operator()(uint32_t) { return true; }
};

// Possible results.
enum class Result { SUCCESS, EMPTY_VOLUME, INTERRUPTED, UNKNOWN_FILTER_MODE };

// Sample an openvdb FloatGrid on a regular 3D grid of points.
// Store the sample values in a contiguous buffer out_data.
template <typename RealType, typename ProgressCallback = ProgressCallbackNoOp>
Result sampleGrid(
        const openvdb::FloatGrid& grid,
        const openvdb::Coord& sampling_extents,
        SampleBufferHeader<RealType>& out_header,
        RealType* out_data,
        FilterMode filter_mode = FilterMode::AUTO,
        ProgressCallback progress_callback = ProgressCallback());


// === Implementation ==========================================================

namespace detail {

inline openvdb::CoordBBox
getIndexSpaceBoundingBox(const openvdb::GridBase& grid)
{
    try {
        const auto file_bbox_min = openvdb::Coord(
            grid.metaValue<openvdb::Vec3i>("file_bbox_min"));
        if (file_bbox_min.x() == std::numeric_limits<int>::max() ||
            file_bbox_min.y() == std::numeric_limits<int>::max() ||
            file_bbox_min.z() == std::numeric_limits<int>::max()) {
            return {};
        }
        const auto file_bbox_max = openvdb::Coord(
            grid.metaValue<openvdb::Vec3i>("file_bbox_max"));

        if (file_bbox_max.x() == std::numeric_limits<int>::min() ||
            file_bbox_max.y() == std::numeric_limits<int>::min() ||
            file_bbox_max.z() == std::numeric_limits<int>::min()) {
            return {};
        }

        return { file_bbox_min, file_bbox_max };
    } catch (openvdb::Exception e) {
        return {};
    }
}

template <typename RealType>
struct ValueRange {
public:
    ValueRange()
        : m_min(std::numeric_limits<RealType>::max())
        , m_max(std::numeric_limits<RealType>::min())
    {}
    ValueRange(RealType min_, RealType max_)
        : m_min(min_)
        , m_max(max_)
    {}

    RealType getMin() const { return m_min; }
    RealType getMax() const { return m_max; }

    void addValue(RealType value)
    {
        m_min = std::min(m_min, value);
        m_max = std::max(m_max, value);
    }

private:
    RealType m_min, m_max;
};
typedef ValueRange<float> FloatRange;

template <typename RealType>
void setHeader(
        const FloatRange& value_range,
        const openvdb::BBoxd& bbox,
        SampleBufferHeader<RealType>& output)
{
    output.value_range[0] = RealType(value_range.getMin());
    output.value_range[1] = RealType(value_range.getMax());
    const auto& extents = bbox.extents();
    output.size[0] = RealType(extents.x());
    output.size[1] = RealType(extents.y());
    output.size[2] = RealType(extents.z());
    const auto& origin = bbox.min();
    output.origin[0] = RealType(origin.x());
    output.origin[1] = RealType(origin.y());
    output.origin[2] = RealType(origin.z());
}


template <typename SamplingFunc, typename SampleType, typename ProgressCallback = ProgressCallbackNoOp>
Result sampleVolume(
        const openvdb::Coord& extents,
        SamplingFunc sampling_func,
        SampleType* out_samples,
        FloatRange& out_value_range,
        ProgressCallback pcb = ProgressCallback())
{
    const auto domain = openvdb::CoordBBox(openvdb::Coord(0, 0, 0),
                                           extents - openvdb::Coord(1, 1, 1));
    if (domain.empty())
        return Result::EMPTY_VOLUME;

    const auto num_voxels = domain.volume();

    // Sample on a lattice.
    typedef tbb::enumerable_thread_specific<FloatRange> PerThreadRange;
    PerThreadRange ranges;
    const openvdb::Vec3i stride = {1, extents.x(), extents.x() * extents.y()};
    tbb::atomic<bool> cancelled;
    cancelled = false;
    tbb::parallel_for(domain,
        [&sampling_func, &stride, &ranges, out_samples, &pcb, &cancelled]
        (const openvdb::CoordBBox& bbox)
    {
        const auto local_extents = bbox.extents();
        const auto progress_step = local_extents.x() * local_extents.y();

        // Loop through local bbox.
        PerThreadRange::reference this_thread_range = ranges.local();
        for (auto z = bbox.min().z(); z <= bbox.max().z(); ++z) {
            for (auto y = bbox.min().y(); y <= bbox.max().y(); ++y) {
                for (auto x = bbox.min().x(); x <= bbox.max().x(); ++x) {
                    if (cancelled)
                        return;
                    const auto domain_index = openvdb::Vec3i(x, y, z);
                    const auto linear_index = domain_index.dot(stride);
                    const auto sample_value = sampling_func(domain_index);
                    out_samples[linear_index] = sample_value;
                    this_thread_range.addValue(sample_value);
                }
            }

            // Invoke progress Callback.
            if (!pcb(progress_step)) {
                cancelled = true;
                return;
            }
        }
    });
    if (cancelled)
        return Result::INTERRUPTED;

    // Merge per-thread value ranges.
    out_value_range = FloatRange();
    for (const FloatRange& per_thread_range : ranges) {
        out_value_range.addValue(per_thread_range.getMin());
        out_value_range.addValue(per_thread_range.getMax());
    }

    // Remap sample values to [0, 1].
    typedef tbb::blocked_range<size_t> tbb_range;
    tbb::parallel_for(tbb_range(0, num_voxels),
        [out_samples, &out_value_range](const tbb_range& range) {
        for (auto i = range.begin(); i < range.end(); ++i) {
            out_samples[i] = unlerp(
                out_value_range.getMin(),
                out_value_range.getMax(),
                out_samples[i]);
        }
    });

    return Result::SUCCESS;
}

template <typename VecT>
inline typename VecT::value_type maxComponentValue(const VecT& v)
{
    return std::max(std::max(v.x(), v.y()), v.z());
}

template <typename VecT>
inline typename VecT::value_type getLOD(const VecT& v)
{
    return std::log2(maxComponentValue(v));
}

template <typename T>
struct identity { typedef T type; };

template <typename T>
T clamp(T val, typename identity<T>::type floor, typename identity<T>::type ceil)
{
    return std::min(ceil, std::max(floor, val));
}

template <typename T>
T unlerp(typename identity<T>::type a, typename identity<T>::type b, T x)
{
    return (x - a) / (b - a);
}

} // namespace detail


template <typename RealType, typename ProgressCallback>
Result sampleGrid(
        const openvdb::FloatGrid& grid,
        const openvdb::Coord& sampling_extents,
        SampleBufferHeader<RealType>& out_header,
        RealType* out_data,
        FilterMode filter_mode,
        ProgressCallback pcb)
{
    assert(out_data);

    const auto grid_bbox_is = detail::getIndexSpaceBoundingBox(grid);
    const auto bbox_world = grid.transform().indexToWorld(grid_bbox_is);

    // Return if the grid bbox is empty.
    if (grid_bbox_is.empty()) {
        detail::setHeader<RealType>({0, 0}, bbox_world, out_header);
        return Result::EMPTY_VOLUME;
    }

    const auto grid_extents = grid_bbox_is.extents().asVec3d();
    const auto max_lod = detail::getLOD(grid_extents);
    const auto num_levels = int(openvdb::math::Ceil(max_lod));

    if (filter_mode == FilterMode::AUTO) {
        if (num_levels > 1) {
            filter_mode = FilterMode::MULTIRES;
        } else {
            filter_mode = FilterMode::BOX;
        }
    }

    const auto domain_extents = sampling_extents.asVec3d();

    if (filter_mode == FilterMode::MULTIRES) {
        // Create multiresolution grid.
        openvdb::tools::MultiResGrid<openvdb::FloatTree> multires(size_t(num_levels), grid);

        // Calculate sampling LoD level.
        const auto coarse_voxel_size = grid_extents / sampling_extents.asVec3d();
        const auto lod_level = detail::clamp(detail::getLOD(coarse_voxel_size), 0, num_levels);

        // Set up sampling func.
        auto sampling_func = [&multires, lod_level, &bbox_world, &domain_extents]
            (const openvdb::Vec3d& domain_index) -> RealType
        {
            const auto pos_ws = bbox_world.min() +
                (domain_index + 0.5) / domain_extents * bbox_world.extents();
            const auto pos_is = multires.transform().worldToIndex(pos_ws);
            return multires.sampleValue<1>(pos_is, lod_level);
        };

        // Sample the MultiResGrid and fill the output variables.
        detail::FloatRange value_range;
        const auto res = detail::sampleVolume(
                sampling_extents,
                sampling_func,
                out_data,
                value_range,
                pcb);
        detail::setHeader<RealType>(value_range, bbox_world, out_header);
        return res;

    } else if (filter_mode == FilterMode::BOX) {
        // Set up sampling func.
        openvdb::tools::GridSampler<
            openvdb::FloatGrid,
            openvdb::tools::BoxSampler> sampler(grid);
        auto sampling_func = [&sampler, &bbox_world, &domain_extents]
            (const openvdb::Vec3d& domain_index) -> RealType
        {
            const auto sample_pos_ws = bbox_world.min() +
                (domain_index + 0.5) / domain_extents * bbox_world.extents();
            return sampler.wsSample(sample_pos_ws);
        };

        // Sample the grid and fill the output variables.
        detail::FloatRange value_range;
        const auto res = detail::sampleVolume(
                sampling_extents,
                sampling_func,
                out_data,
                value_range,
                pcb);
        detail::setHeader<RealType>(value_range, bbox_world, out_header);
        return res;

    } else {
        return Result::UNKNOWN_FILTER_MODE;
    }
}

} // namespace volume_sampling
