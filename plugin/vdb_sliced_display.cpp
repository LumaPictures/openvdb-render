#include "vdb_sliced_display.h"
#include "vdb_maya_utils.hpp"
#include "volume_sampling.hpp"
#include "blackbody.h"
#include "progress_bar.h"

#include <openvdb/openvdb.h>

#include <maya/MArgDatabase.h>
#include <maya/MArgParser.h>
#include <maya/MBoundingBox.h>
#include <maya/MDrawContext.h>
#include <maya/MFloatVector.h>
#include <maya/MGlobal.h>
#include <maya/MHWGeometry.h>
#include <maya/MPxCommand.h>
#include <maya/MPxSubSceneOverride.h>
#include <maya/MSelectionList.h>
#include <maya/MShaderManager.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MSyntax.h>

#include <map>
#include <memory>
#include <unordered_map>
#include <cassert>


// === Unique pointers to VP2.0 objects ========================================

struct ShaderInstanceDeleter {
    void operator()(MHWRender::MShaderInstance* ptr) const
    {
        if (ptr)
            MHWRender::MRenderer::theRenderer()->getShaderManager()->releaseShader(ptr);
    }
};
typedef std::unique_ptr<MHWRender::MShaderInstance, ShaderInstanceDeleter> ShaderPtr;

struct TextureDeleter {
    void operator()(MHWRender::MTexture* ptr) const
    {
        if (ptr)
            MHWRender::MRenderer::theRenderer()->getTextureManager()->releaseTexture(ptr);
    }
};
typedef std::unique_ptr<MHWRender::MTexture, TextureDeleter> TexturePtr;

struct SamplerStateDeleter {
    void operator()(const MHWRender::MSamplerState* ptr) const
    {
        if (ptr)
            MHWRender::MStateManager::releaseSamplerState(ptr);
    }
};
typedef std::unique_ptr<const MHWRender::MSamplerState, SamplerStateDeleter> SamplerStatePtr;


// === Renderable ==========================================================

struct Renderable {
    MHWRender::MRenderItem* render_item;
    MHWRender::MVertexBufferArray vertex_buffer_array;
    std::unique_ptr<MHWRender::MVertexBuffer> position_buffer;
    std::unique_ptr<MHWRender::MIndexBuffer> index_buffer;

    Renderable() : render_item(nullptr) {}
    void update(MHWRender::MPxSubSceneOverride& subscene_override, const MBoundingBox& bbox);
    operator bool() const { return render_item != nullptr; }
};

void Renderable::update(MHWRender::MPxSubSceneOverride& subscene_override, const MBoundingBox& bbox)
{
    // Note: render item has to be added to the MSubSceneContainer before calling setGeometryForRenderItem.
    CHECK_MSTATUS(subscene_override.setGeometryForRenderItem(*render_item, vertex_buffer_array, *index_buffer, &bbox));
}

// === VDBVolumeSpec =======================================================

struct VDBVolumeSpec {
    std::string vdb_file_name;
    std::string vdb_file_uuid;
    std::string vdb_grid_name;
    openvdb::Coord texture_size;

    VDBVolumeSpec() {}
    VDBVolumeSpec(const std::string& vdb_file_name_, const std::string& vdb_file_uuid_, const std::string& vdb_grid_name_, openvdb::Coord texture_size_)
        : vdb_file_name(vdb_file_name_), vdb_file_uuid(vdb_file_uuid_), vdb_grid_name(vdb_grid_name_), texture_size(texture_size_) {}
};

namespace {
    template<typename T> void hash_combine(size_t& seed, T const& v)
    {
        seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
} // unnamed namespace

namespace std {
    template <> struct hash<VDBVolumeSpec> {
        typedef VDBVolumeSpec argument_type;
        typedef std::size_t result_type;
        result_type operator()(argument_type const& spec) const
        {
            result_type res = std::hash<std::string>{}(spec.vdb_file_name);
            hash_combine(res, spec.vdb_file_uuid);
            hash_combine(res, spec.vdb_grid_name);
            hash_combine(res, spec.texture_size.x());
            hash_combine(res, spec.texture_size.y());
            hash_combine(res, spec.texture_size.z());
            return res;
        }
    };

    bool operator==(const VDBVolumeSpec& lhs, const VDBVolumeSpec& rhs)
    {
        return lhs.vdb_file_name == rhs.vdb_file_name &&
               lhs.vdb_grid_name == rhs.vdb_grid_name &&
               lhs.texture_size == rhs.texture_size;
    }
}

// === SamplerState ========================================================

namespace {
    SamplerStatePtr createSamplerState(
        MHWRender::MSamplerState::TextureFilter filter,
        MHWRender::MSamplerState::TextureAddress address,
        int min_lod = 0, int max_lod = 16)
    {
        MHWRender::MSamplerStateDesc desc;
        desc.filter = filter;
        desc.addressU = address;
        desc.addressV = address;
        desc.addressW = address;
        desc.minLOD = min_lod;
        desc.maxLOD = max_lod;
        desc.mipLODBias = 0;
        memset(desc.borderColor, 0, 4 * sizeof(float));
        return SamplerStatePtr(MHWRender::MStateManager::acquireSamplerState(desc));
    }
} // unnamed namespace

// === VolumeTexture =======================================================

struct VolumeTexture {
    TexturePtr texture_ptr;
    MFloatVector value_range;
    MFloatVector volume_size;
    MFloatVector volume_origin;
    openvdb::Coord extents;

    VolumeTexture() : texture_ptr(nullptr) {}
    VolumeTexture(const VolumeTexture&) = delete;
    VolumeTexture& operator=(const VolumeTexture&) = delete;
    VolumeTexture(VolumeTexture&&) = default;
    VolumeTexture& operator=(VolumeTexture&&) = default;

    template <typename RealType>
    void acquireBuffer(
        const openvdb::Coord& texture_extents,
        const volume_sampling::SampleBufferHeader<RealType>& buffer_header,
        const RealType* buffer_data);
    void clear() { texture_ptr.reset(); }
    bool isValid() const { return texture_ptr.get() != nullptr; }

    void assign(MHWRender::MShaderInstance* shader_instance, const MString& param)
    {
        m_texture_assignment.texture = texture_ptr.get();
        CHECK_MSTATUS(shader_instance->setParameter(param, m_texture_assignment));
    }

private:
    MHWRender::MTextureAssignment m_texture_assignment;
    static std::vector<float> s_staging;
};
std::vector<float> VolumeTexture::s_staging;

namespace {

    template <typename RealType>
    MFloatVector inline mayavecFromArray2(RealType *a)
    {
        return { a[0], a[1] };
    }

    template <typename RealType>
    MFloatVector inline mayavecFromArray3(RealType *a)
    {
        return { a[0], a[1], a[2] };
    }

    MFloatVector inline mayavecFromVec2f(const openvdb::Vec2f& vec)
    {
        return { vec.x(), vec.y() };
    }

    MFloatVector inline mayavecFromVec3f(const openvdb::Vec3f& vec)
    {
        return { vec.x(), vec.y(), vec.z() };
    }

    size_t voxel_count(const openvdb::Coord& extents)
    {
        return extents.x() * extents.y() * extents.z();
    }

} // unnamed namespace

template <typename RealType>
void VolumeTexture::acquireBuffer(
    const openvdb::Coord& texture_extents,
    const volume_sampling::SampleBufferHeader<RealType>& buffer_header,
    const RealType* buffer_data)
{
    const auto renderer = MHWRender::MRenderer::theRenderer();
    if (!renderer)
        return;
    auto texture_manager = renderer->getTextureManager();
    if (!texture_manager)
        return;

    if (!buffer_data) {
        clear();
        return;
    }

    // Set metadata.
    value_range = mayavecFromArray2(buffer_header.value_range);
    volume_size = mayavecFromArray3(buffer_header.size);
    volume_origin = mayavecFromArray3(buffer_header.origin);

    const void* buffer = nullptr;
    if (std::is_same<RealType, float>::value) {
        buffer = buffer_data;
    } else {
        // Convert voxels to float.
        // Note: uploading 'half' voxel data (i.e. using raster type kR16_FLOAT) is
        // SLOWER by a factor of ~2 compared to uploading floats. I have no idea why.
        // If you know the answer, please explain it to me (zoltan.gilian@gmail.com).
        s_staging.resize(voxel_count(texture_extents));
        typedef tbb::blocked_range<size_t> tbb_range;
        const RealType* input = reinterpret_cast<const RealType*>(buffer_data);
        float* output = s_staging.data();
        tbb::parallel_for(tbb_range(0, s_staging.size()),
            [input, output] (const tbb_range& range){
                for (size_t i = range.begin(); i < range.end(); ++i)
                    output[i] = static_cast<float>(input[i]);
            });
        buffer = s_staging.data();
    }

    // If texture size didn't change, texture data can be updated in place,
    // providing there is an actual texture owned by this instance.
    if (extents == texture_extents && texture_ptr.get() != nullptr) {
        texture_ptr->update(buffer, true);
        return;
    }

    // Otherwise a new texture has to be created.
    MHWRender::MTextureDescription texture_desc;
    texture_desc.fWidth = texture_extents.x();
    texture_desc.fHeight = texture_extents.y();
    texture_desc.fDepth = texture_extents.z();
    texture_desc.fBytesPerRow = sizeof(float) * texture_desc.fWidth;
    texture_desc.fBytesPerSlice = texture_desc.fBytesPerRow * texture_desc.fHeight;
    texture_desc.fMipmaps = 0;
    texture_desc.fArraySlices = 1;
    texture_desc.fFormat = MHWRender::kR32_FLOAT;
    texture_desc.fTextureType = MHWRender::kVolumeTexture;
    texture_desc.fEnvMapType = MHWRender::kEnvNone;
    texture_ptr.reset(texture_manager->acquireTexture("", texture_desc, buffer, true));

    extents = texture_extents;
}

// === VolumeCache =========================================================

class VolumeCache {
public:
    static VolumeCache& instance();

    void getVolume(const VDBVolumeSpec& spec, VolumeTexture& output);

    enum class VoxelType { FLOAT, HALF };
    VoxelType getVoxelType() const { return m_voxel_type; }
    void setVoxelType(VoxelType voxel_type);

    void setMemoryLimitBytes(size_t mem_limit_bytes);
    size_t getMemoryLimitBytes() const { return m_mem_limit_bytes; }
    size_t getAllocatedBytes() const { return m_buffer.size(); }

private:
    static size_t s_refcount;

public:
    // Each SubSceneOverride should call registerUsage in its ctor and unregisterUsage in its dtor
    // so that the cache can be cleared if e.g. a new scene is created.
    static void registerUsage() { ++s_refcount; }
    static void unregisterUsage() { --s_refcount; if (s_refcount == 0) instance().clear(); }

private:
    VoxelType m_voxel_type;

    struct BufferRange {
        size_t begin;
        size_t end;
        BufferRange(size_t begin_, size_t end_) : begin(begin_), end(end_) {}
    };

    size_t m_mem_limit_bytes;

    // Objects are stored contiguously in a vector.
    std::vector<uint8_t> m_buffer;
    // FIFO cache eviction policy is used for its simplicity. The m_buffer_head
    // wraps around effectively creating a ring buffer, but objects are always
    // laid out linearly; m_buffer_head will wrap prematurely if the next object
    // doesn't fit into m_buffer.
    size_t m_buffer_head;
    // Associates VDBVolumeSpec values to ranges in m_buffer.
    // This makes the cache addressable by VDBVolumeSpec.
    typedef std::unordered_map<VDBVolumeSpec, BufferRange> BufferMap;
    BufferMap m_buffer_map;
    // m_allocation_map associates buffer range offsets ('begin') to buffer
    // map keys, so that old allocations overlapping a new one
    // can be deleted. The map needs to be ordered.
    std::map<size_t, VDBVolumeSpec> m_allocation_map;

    VolumeCache();
    openvdb::FloatGrid::ConstPtr loadGrid(const VDBVolumeSpec& spec);
    void clear();
    void clearRange(const BufferRange& range);
    void growBuffer(size_t minimum_buffer_size_bytes);

    template <typename RealType>
    void getVolume(const VDBVolumeSpec& spec, VolumeTexture& output);
    template <typename RealType>
    void* allocate(const VDBVolumeSpec& spec);
    template <typename RealType>
    volume_sampling::Result sampleGrid(
        const VDBVolumeSpec& spec,
        const openvdb::FloatGrid& grid,
        volume_sampling::SampleBufferHeader<RealType>& out_header,
        RealType* out_data);

    static const size_t DEFAULT_LIMIT_BYTES;
    static const size_t DEFAULT_SIZE_BYTES;
    static const size_t GROW_AMOUNT_BYTES;
};

size_t VolumeCache::s_refcount = 0;

namespace {

    template <typename T>
    inline MString toMString(const T& value)
    {
        MString res;
        res.set(value);
        return res;
    }

    template <>
    inline MString toMString<std::string>(const std::string& value)
    {
        MString res;
        res.set(value.c_str());
        return res;
    }

    template <>
    inline MString toMString<MString>(const MString& value)
    {
        return value;
    }

    template <typename... Args>
    MString format(const MString& format, Args&&... args)
    {
        MString res;
        CHECK_MSTATUS(res.format(format, toMString(std::forward<Args>(args))...));
        return res;
    }

    class VDBFile {
    public:
        VDBFile(const std::string& file_name) : m_vdb_file(file_name) { m_vdb_file.open(false); }
        ~VDBFile() { m_vdb_file.close(); }
        operator bool() const { return m_vdb_file.isOpen(); }
        openvdb::FloatGrid::ConstPtr loadFloatGrid(const std::string& grid_name);

    private:
        openvdb::io::File m_vdb_file;
    };

    openvdb::FloatGrid::ConstPtr VDBFile::loadFloatGrid(const std::string& grid_name)
    {
        if (!m_vdb_file.isOpen())
            return nullptr;

        openvdb::GridBase::ConstPtr grid_base_ptr;
        try {
            grid_base_ptr = m_vdb_file.readGrid(grid_name);
        } catch (const openvdb::Exception&) {
            return nullptr;
        }

        auto grid_ptr = openvdb::gridConstPtrCast<openvdb::FloatGrid>(grid_base_ptr);
        if (!grid_ptr) {
			MGlobal::displayError(format("[openvdb] Grid '^1s' is not a FloatGrid.", grid_name.c_str()));
            return nullptr;
        }

        return grid_ptr;
    }

    constexpr size_t KILOBYTE = 1024;
    constexpr size_t MEGABYTE = 1024 * KILOBYTE;
    constexpr size_t GIGABYTE = 1024 * MEGABYTE;

} // unnamed namespace

const size_t VolumeCache::DEFAULT_LIMIT_BYTES = 2 * GIGABYTE;
const size_t VolumeCache::DEFAULT_SIZE_BYTES = 256 * MEGABYTE;
const size_t VolumeCache::GROW_AMOUNT_BYTES = 256 * MEGABYTE;

VolumeCache& VolumeCache::instance()
{
    static VolumeCache volume_cache;
    return volume_cache;
}

void VolumeCache::setVoxelType(VoxelType voxel_type)
{
    if (m_voxel_type != voxel_type)
        clear();

    m_voxel_type = voxel_type;
}

VolumeCache::VolumeCache() : m_voxel_type(VoxelType::HALF), m_mem_limit_bytes(DEFAULT_LIMIT_BYTES), m_buffer_head(0)
{
    // Don't allocate anything in the ctor to avoid unnecessary consumption of memory (e.g. batch mode).
}

openvdb::FloatGrid::ConstPtr VolumeCache::loadGrid(const VDBVolumeSpec& spec)
{
    // Open VDB file or bail.
    auto vdb_file = VDBFile(spec.vdb_file_name);
    if (!vdb_file)
        return nullptr;

    return vdb_file.loadFloatGrid(spec.vdb_grid_name);
}

template <typename RealType>
volume_sampling::Result VolumeCache::sampleGrid(
    const VDBVolumeSpec& spec,
    const openvdb::FloatGrid& grid,
    volume_sampling::SampleBufferHeader<RealType>& out_header,
    RealType* out_data)
{
    ProgressBar progress_bar(
        /* message = */ format("vdb_visualizer: sampling grid ^1s", spec.vdb_grid_name),
        /* max_progress = */ spec.texture_size.x() * spec.texture_size.y() * spec.texture_size.z());

    return volume_sampling::sampleGrid(
        grid, spec.texture_size,
        out_header, out_data,
        volume_sampling::FilterMode::AUTO,
        [&progress_bar](uint32_t progress_samples) {
            progress_bar.addProgress(progress_samples);
            return !progress_bar.isCancelled();
        });
}

void VolumeCache::getVolume(const VDBVolumeSpec& spec, VolumeTexture& output)
{
    if (m_voxel_type == VoxelType::HALF)
        getVolume<half>(spec, output);
    else if (m_voxel_type == VoxelType::FLOAT)
        getVolume<float>(spec, output);
}

template <typename RealType>
void VolumeCache::getVolume(const VDBVolumeSpec& spec, VolumeTexture& output)
{
    typedef volume_sampling::SampleBufferHeader<RealType> Header;

    // Check if in cache.
    auto it = m_buffer_map.find(spec);
    if (it != m_buffer_map.end()) {
        // Load from cache.
        const auto& range = it->second;
        const Header& header = *(Header*)(m_buffer.data() + range.begin);
        if (range.end - range.begin == sizeof(header)) {
            // Empty volume; pass a single zero to the volume texture.
            const RealType zero_value = 0;
            output.acquireBuffer<RealType>({1, 1, 1}, header, &zero_value);
        } else {
            // Pass the buffer to the volume texture.
            const RealType* buffer = (RealType*)(&header + 1);
            output.acquireBuffer(spec.texture_size, header, buffer);
        }
        return;
    }

    // Not in cache; try to load the grid.
    const auto grid = loadGrid(spec);
    if (!grid) {
        output.clear();
        return;
    }

    // Allocate space, return if not succesful.
    void* buffer_ptr = allocate<RealType>(spec);
    if (!buffer_ptr) {
        output.clear();
        return;
    }

    // Sample grid, return if not succesful (i.e. user cancelled the sampling procedure).
    Header& header = *(Header*)(buffer_ptr);
    RealType* buffer = (RealType*)(&header + 1);
    const auto status = sampleGrid<RealType>(spec, *grid, header, buffer);

    if (status == volume_sampling::Result::EMPTY_VOLUME) {
        const auto buffer_range_it = m_buffer_map.find(spec);
        // Only keep the header from the allocation.
        auto& range = buffer_range_it->second;
        range.end = range.begin + sizeof(header);
        // Roll back the buffer head.
        m_buffer_head = range.end;
        // Upload a 1x1x1 zero texture.
        const RealType zero_value = 0;
        output.acquireBuffer<RealType>({1, 1, 1}, header, &zero_value);
        return;

    } else if (status != volume_sampling::Result::SUCCESS) {
        // Sampling wasn't successful; clean up.
        output.clear();
        // Delete this allocation.
        const auto buffer_range_it = m_buffer_map.find(spec);
        if (buffer_range_it == m_buffer_map.end())
            // If caching is disabled, the allocation hasn't been recorded, so just return.
            return;
        m_allocation_map.erase(buffer_range_it->second.begin);
        m_buffer_map.erase(buffer_range_it);
        return;
    }

    // Update texture.
    output.acquireBuffer<RealType>(spec.texture_size, header, buffer);

    // Clear buffer if caching is disabled.
    if (m_mem_limit_bytes == 0)
        m_buffer.clear();
}

void VolumeCache::setMemoryLimitBytes(size_t mem_limit_bytes)
{
    m_mem_limit_bytes = mem_limit_bytes;

    // Shrink buffer if requested.
    if (m_mem_limit_bytes < m_buffer.size()) {
        clearRange(BufferRange(m_mem_limit_bytes, m_buffer.size()));
        m_buffer.resize(mem_limit_bytes);
        m_buffer.shrink_to_fit();
    }

    // Reset head if current positions will become invalid.
    if (m_buffer_head >= m_mem_limit_bytes)
        m_buffer_head = 0;
}

namespace {
    size_t GetLSB(size_t val)
    {
        return val & -val;
    }
    size_t RoundUpToAlign(size_t val, size_t alignment)
    {
        // Require power of 2 alignment.
        assert(alignment == GetLSB(alignment));
        return (val + alignment - 1) & ~(alignment - 1);
    }
} // unnamed namespace

template <typename RealType>
void* VolumeCache::allocate(const VDBVolumeSpec& spec)
{
    const size_t item_size_bytes =
        sizeof(volume_sampling::SampleBufferHeader<RealType>) +
        voxel_count(spec.texture_size) * sizeof(RealType);

    if (m_mem_limit_bytes == 0) {
        // Caching is disabled. Use buffer for this request, but perform no accounting.
        m_buffer.resize(item_size_bytes);
        m_buffer.shrink_to_fit();
        return m_buffer.data();
    } else if (item_size_bytes > m_mem_limit_bytes) {
        return nullptr;
    }

    // If this allocation would exceed the memory limit, reset the head index.
    // Otherwise grow the buffer if needed.
    {
        const auto allocation_end = m_buffer_head + item_size_bytes;
        if (m_buffer.size() < allocation_end)
            growBuffer(allocation_end);
    }

    // Allocate buffer range.
    const size_t buffer_begin = RoundUpToAlign(m_buffer_head, alignof(RealType));
    const size_t buffer_end = buffer_begin + item_size_bytes;
    const auto buffer_range = BufferRange(buffer_begin, buffer_end);
    m_buffer_head = buffer_end;

    // Throw away old allocations which overlap [buffer_begin, buffer_end).
    clearRange(buffer_range);

    // Update maps.
    m_buffer_map.insert({spec, buffer_range}).first;
    m_allocation_map.insert(std::make_pair(buffer_begin, spec));
    return m_buffer.data() + buffer_begin;
}

void VolumeCache::growBuffer(size_t minimum_buffer_size_bytes)
{
    // Reset head if requested size exceeds the limit.
    if (minimum_buffer_size_bytes > m_mem_limit_bytes) {
        m_buffer_head = 0;
        return;
    }

    size_t new_size_bytes = m_buffer.size() + GROW_AMOUNT_BYTES;
    new_size_bytes = std::max(minimum_buffer_size_bytes, new_size_bytes);
    new_size_bytes = std::min(m_mem_limit_bytes, new_size_bytes);

    // Bail if buffer size is large enough already.
    if (new_size_bytes <= m_buffer.size())
        return;

    // Try to resize or reset head if failed.
    try {
        m_buffer.resize(new_size_bytes);
    } catch (const std::bad_alloc&) {
        m_buffer_head = 0;
    }
}

void VolumeCache::clear()
{
    m_buffer_head = 0;
    m_buffer.clear();
    m_buffer.shrink_to_fit();
    m_buffer_map.clear();
    m_allocation_map.clear();
}

void VolumeCache::clearRange(const BufferRange& range_to_clear)
{
    // Find first range which begins strictly after the given range.begin.
    auto erase_it = m_allocation_map.upper_bound(range_to_clear.begin);
    // Start erasing at the previous range if it contains range_to_clear.begin.
    if (erase_it != m_allocation_map.begin()) {
        auto prev_it = erase_it;
        --prev_it;
        const auto prev_range_it = m_buffer_map.find(prev_it->second);
        assert(prev_range_it != m_buffer_map.end());
        const auto& prev_range = prev_range_it->second;
        if (prev_range.begin <= range_to_clear.begin && range_to_clear.begin < prev_range.end)
            erase_it = prev_it;
    }
    // The first range to keep is the first one which begins on or after range.end.
    auto erase_end = m_allocation_map.lower_bound(range_to_clear.end);

    while (erase_it != erase_end) {
        m_buffer_map.erase(erase_it->second);
        erase_it = m_allocation_map.erase(erase_it);
    }
}

// === VolumeParam =========================================================

class VolumeParam {
public:
    VolumeParam(const char *shader_param_prefix = nullptr, MHWRender::MShaderInstance *shader_instance = nullptr);
    void setParamPrefix(const char *param_prefix);
    void setShaderInstance(MHWRender::MShaderInstance *shader_instance) { m_shader_instance = shader_instance; }
    void loadVolume(const VDBVolumeSpec& volume_spec);

private:
    MString use_texture_param;
    MString texture_param;
    MString value_range_param;
    MString volume_size_param;
    MString volume_origin_param;
    MHWRender::MShaderInstance *m_shader_instance;
    VolumeTexture m_volume_texture;

    void assign();
};

VolumeParam::VolumeParam(const char *shader_param_prefix, MHWRender::MShaderInstance *shader_instance)
{
    setParamPrefix(shader_param_prefix);
    setShaderInstance(shader_instance);
}

void VolumeParam::setParamPrefix(const char *param_prefix)
{
    if (!param_prefix)
        return;
    use_texture_param = format("use_^1s_texture", param_prefix);
    texture_param = format("^1s_texture", param_prefix);
    value_range_param = format("^1s_value_range", param_prefix);
    volume_size_param = format("^1s_volume_size", param_prefix);
    volume_origin_param = format("^1s_volume_origin", param_prefix);
}

void VolumeParam::loadVolume(const VDBVolumeSpec& volume_spec)
{
    VolumeCache::instance().getVolume(volume_spec, m_volume_texture);
    assign();
}

void VolumeParam::assign()
{
    const bool use_texture = m_volume_texture.isValid();
    CHECK_MSTATUS(m_shader_instance->setParameter(use_texture_param, use_texture));
    if (!use_texture)
        return;

    m_volume_texture.assign(m_shader_instance, texture_param);
    CHECK_MSTATUS(m_shader_instance->setParameter(value_range_param, m_volume_texture.value_range));
    CHECK_MSTATUS(m_shader_instance->setParameter(volume_size_param, m_volume_texture.volume_size));
    CHECK_MSTATUS(m_shader_instance->setParameter(volume_origin_param, m_volume_texture.volume_origin));
}

// === RampTextureBase =====================================================

class RampTextureBase {
public:
    operator bool() const { return m_texture.get() != nullptr; }
    MStatus assignToShader(MHWRender::MShaderInstance* shader, const MString& parameter) const;
    const MSamplerState& getSamplerState() const { return *m_ramp_sampler_state; }

    void setResolution(unsigned int resolution);

protected:
    RampTextureBase(const unsigned int resolution, const MHWRender::MRasterFormat raster_format, const unsigned int bytes_per_pixel);

    unsigned int m_resolution;
    const MHWRender::MRasterFormat m_raster_format;
    const unsigned int m_bytes_per_pixel;
    std::vector<uint8_t> m_staging;

    TexturePtr m_texture;
    const SamplerStatePtr m_ramp_sampler_state;
};

RampTextureBase::RampTextureBase(const unsigned int resolution, const MHWRender::MRasterFormat raster_format, const unsigned int bytes_per_pixel)
    : m_resolution(0)
    , m_raster_format(raster_format)
    , m_bytes_per_pixel(bytes_per_pixel)
    , m_staging(bytes_per_pixel * resolution, 0)
    , m_ramp_sampler_state(createSamplerState(MHWRender::MSamplerState::kMinMagMipLinear, MHWRender::MSamplerState::kTexClamp, 0, 0))
{
    setResolution(resolution);
}

void RampTextureBase::setResolution(unsigned int resolution)
{
    if (m_resolution == resolution)
        return;
    m_resolution = resolution;

	auto renderer = MHWRender::MRenderer::theRenderer();
	if (!renderer)
		return;
	auto tex_man = renderer->getTextureManager();
	if (!tex_man)
		return;

    MHWRender::MTextureDescription ramp_desc;
    ramp_desc.fWidth = resolution;
    ramp_desc.fHeight = 1;
    ramp_desc.fDepth = 1;
    ramp_desc.fBytesPerRow = m_bytes_per_pixel * ramp_desc.fWidth;
    ramp_desc.fBytesPerSlice = ramp_desc.fBytesPerRow;
    ramp_desc.fMipmaps = 1;
    ramp_desc.fArraySlices = 1;
    ramp_desc.fFormat = m_raster_format;
    ramp_desc.fTextureType = MHWRender::kImage1D;
    ramp_desc.fEnvMapType = MHWRender::kEnvNone;
    m_texture.reset(tex_man->acquireTexture("", ramp_desc, m_staging.data(), false));
}

MStatus RampTextureBase::assignToShader(MHWRender::MShaderInstance* shader, const MString& parameter) const
{
    MHWRender::MTextureAssignment texture_assignment;
    texture_assignment.texture = m_texture.get();
    return shader->setParameter(parameter, texture_assignment);
}

// === FloatRampTexture ====================================================

class FloatRampTexture : public RampTextureBase {
public:
    FloatRampTexture(const unsigned int resolution) : RampTextureBase(resolution, MHWRender::MRasterFormat::kR8_UNORM, 1) {}
    void updateSamples(const std::vector<float>& samples);
};

void FloatRampTexture::updateSamples(const std::vector<float>& samples)
{
    if (!m_texture)
        return;

    setResolution(unsigned(samples.size()));
    for (unsigned int i = 0; i < m_resolution; ++i)
        m_staging[i] = uint8_t(samples[i] * 255);
    m_texture->update(m_staging.data(), false);
}

// === RGBRampTexture ======================================================

class RGBRampTexture : public RampTextureBase {
public:
    RGBRampTexture(const unsigned int resolution) : RampTextureBase(resolution, MHWRender::MRasterFormat::kR8G8B8X8, 4) {}
    void updateSamples(const std::vector<MFloatVector>& linear_colors, const float normalizer=1);
};

void RGBRampTexture::updateSamples(const std::vector<MFloatVector>& linear_colors, const float normalizer)
{
    if (!m_texture)
        return;

    setResolution(unsigned(linear_colors.size()));
    for (unsigned int i = 0; i < m_resolution; ++i) {
        auto srgb_color = SRGBFromLinear(linear_colors[i] / normalizer);
        m_staging[4 * i + 0] = uint8_t(srgb_color.x * 255);
        m_staging[4 * i + 1] = uint8_t(srgb_color.y * 255);
        m_staging[4 * i + 2] = uint8_t(srgb_color.z * 255);
        m_staging[4 * i + 3] = 0;
    }
    m_texture->update(m_staging.data(), false);
}

// === BlackbodyLUT ========================================================

struct BlackbodyLUT {
    BlackbodyLUT();
    RGBRampTexture lut;
};

BlackbodyLUT::BlackbodyLUT() : lut(Blackbody::LUT_SIZE)
{
    lut.updateSamples(Blackbody::LUT, Blackbody::LUT_NORMALIZER);
}

// === VolumeShader ========================================================

class VolumeShader {
public:
    VolumeShader();
    ~VolumeShader();
    MHWRender::MShaderInstance *get() { return m_shader_ptr; }
    const MHWRender::MShaderInstance *get() const { return m_shader_ptr; }
    MHWRender::MShaderInstance *operator*() { return get(); }
    const MHWRender::MShaderInstance *operator*() const { return get(); }
    MHWRender::MShaderInstance *operator->() { return get(); }
    const MHWRender::MShaderInstance *operator->() const { return get(); }

    operator bool() const { return get() != nullptr; }

private:
    ShaderPtr m_shader_clone_owner;
    MHWRender::MShaderInstance *m_shader_ptr;

    static void loadShader();
    static void preDrawCallback(MHWRender::MDrawContext& context,
                                const MHWRender::MRenderItemList& renderItemList,
                                MHWRender::MShaderInstance* shaderInstance);

    static ShaderPtr s_volume_shader;
    static size_t s_refcount;
    static const unsigned int MAX_LIGHT_COUNT;
    static const std::string VOLUME_EFFECT_CODE;
};

ShaderPtr VolumeShader::s_volume_shader = nullptr;
size_t VolumeShader::s_refcount = 0;

const unsigned int VolumeShader::MAX_LIGHT_COUNT = 16;

namespace {

    template <typename T>
    MHWRender::MShaderCompileMacro makeMacroDef(const MString& name, const T& value)
    {
        return { name, format("^1s", value) };
    };

    template <>
    MHWRender::MShaderCompileMacro makeMacroDef<float>(const MString& name, const float& value)
    {
#if MAYA_API_VERSION >= 20180000
        return { name, format("^1s", value) };
#else
        return { name, format("^1sf", value) };
#endif
    }

    // ogsfx effect code for the sliced visualization mode.
    // Since MSVC has a 16KB limit on string literals, this have to be broken up
    // into two parts.
    const std::string volume_ogsfx = R"ogsfx(
uniform vec3 view_dir_world;
uniform vec3 view_dir_model;
uniform vec3 view_pos_world;
uniform mat4 world_mat;
uniform mat4 world_inverse_mat;
uniform mat4 world_view_proj_mat;

uniform vec3 volume_size;    // in model space.
uniform vec3 volume_origin;  // in model space.

uniform int slice_count;
uniform int dominant_axis; // 0: x, 1: y, 2:z


// ======== VERTEX SHADER ========

attribute VERT_INPUT
{
    vec3 in_pos : POSITION;
};

attribute VERT_OUTPUT
{
    vec3 pos_model;
    vec3 pos_world;
};

GLSLShader VolumeVertexShader {

vec3 Swizzle(vec3 v, int i)
{
    if (i == 1)
        return v.zxy;
    else if (i == 2)
        return v.yzx;
    else
        return v;
}

float GetComponent(vec3 v, int i)
{
    if (i == 1)
        return v.y;
    else if (i == 2)
        return v.z;
    else
        return v.x;
}

void main()
{
    vec2 pos_slice = in_pos.xy;
    int slice_idx = int(round(in_pos.z));

    // Ensure back-to-front render order.
    if (GetComponent(view_dir_model, dominant_axis) > 0) {
        slice_idx = slice_count - 1 - slice_idx;
        pos_slice.x = 1 - pos_slice.x; // for correct winding order.
    }
    vec3 pos_dom = vec3(float(slice_idx) / (slice_count - 1), pos_slice);
    vec3 pos_model = Swizzle(pos_dom, dominant_axis) * volume_size + volume_origin;
    vec3 pos_world = (world_mat * vec4(pos_model, 1)).xyz;
    vec4 pos_clip = world_view_proj_mat * vec4(pos_model, 1);

    vertex_output.pos_model = pos_model;
    vertex_output.pos_world = pos_world;
    gl_Position = pos_clip;
}

}


// ======== FRAGMENT SHADER ======

// Channels.

#define DENSITY_SOURCE_VALUE 0
#define DENSITY_SOURCE_RAMP  1

uniform float     density = 1.0f;
uniform int       density_source = DENSITY_SOURCE_VALUE;
uniform bool      use_density_texture = false;
uniform vec2      density_value_range;
uniform vec3      density_volume_size;
uniform vec3      density_volume_origin;
uniform texture3D density_texture;
uniform sampler3D density_sampler = sampler_state {
    Texture = <density_texture>;
};

#define COLOR_SOURCE_COLOR 0
#define COLOR_SOURCE_RAMP  1

uniform float     scattering_intensity = 1;
uniform vec3      scattering_color;
uniform int       scattering_color_source = COLOR_SOURCE_COLOR;
uniform float     scattering_anisotropy = 0;
uniform bool      use_scattering_texture = false;
uniform vec2      scattering_value_range;
uniform vec3      scattering_volume_size;
uniform vec3      scattering_volume_origin;
uniform texture3D scattering_texture;
uniform sampler3D scattering_sampler = sampler_state {
    Texture = <scattering_texture>;
};

uniform vec3      transparency;
uniform bool      use_transparency_texture = false;
uniform vec2      transparency_value_range;
uniform vec3      transparency_volume_size;
uniform vec3      transparency_volume_origin;
uniform texture3D transparency_texture;
uniform sampler3D transparency_sampler = sampler_state {
    Texture = <transparency_texture>;
};

#define EMISSION_MODE_NONE                  0
#define EMISSION_MODE_CHANNEL               1
#define EMISSION_MODE_DENSITY               2
#define EMISSION_MODE_BLACKBODY             3
uniform int       emission_mode = 0;
uniform int       emission_color_source = COLOR_SOURCE_COLOR;
uniform bool      use_emission_texture = false;
uniform float     emission_intensity = 1;
uniform vec3      emission_color;
uniform vec2      emission_value_range;
uniform vec3      emission_volume_size;
uniform vec3      emission_volume_origin;
uniform texture3D emission_texture;
uniform sampler3D emission_sampler = sampler_state {
    Texture = <emission_texture>;
};

uniform float     temperature = 5000.0f;
uniform bool      use_temperature_texture = false;
uniform vec2      temperature_value_range;
uniform vec3      temperature_volume_size;
uniform vec3      temperature_volume_origin;
uniform texture3D temperature_texture;
uniform sampler3D temperature_sampler = sampler_state {
    Texture = <temperature_texture>;
};

uniform float     blackbody_intensity = 1.0f;
uniform texture1D blackbody_lut_texture;
uniform sampler1D blackbody_lut_sampler = sampler_state {
    Texture = <blackbody_lut_texture>;
};

// Ramps.

uniform vec2      density_ramp_domain;
uniform texture1D density_ramp_texture;
uniform sampler1D density_ramp_sampler = sampler_state {
    Texture = <density_ramp_texture>;
};

uniform vec2      scattering_ramp_domain;
uniform texture1D scattering_ramp_texture;
uniform sampler1D scattering_ramp_sampler = sampler_state {
    Texture = <scattering_ramp_texture>;
};

uniform vec2      emission_ramp_domain;
uniform texture1D emission_ramp_texture;
uniform sampler1D emission_ramp_sampler = sampler_state {
    Texture = <emission_ramp_texture>;
};

// Lights and shadows.

#define LIGHT_FLAG_POINT_LIGHT       0
#define LIGHT_FLAG_DIRECTIONAL_LIGHT 1
#define LIGHT_FLAG_SPOTLIGHT         2
#define LIGHT_FLAG_MASK_TYPE         3
#define LIGHT_FLAG_CAST_SHADOWS      8

uniform int    light_count;
uniform int    light_flags[MAX_LIGHT_COUNT];
uniform vec3   light_position[MAX_LIGHT_COUNT];
uniform vec3   light_direction[MAX_LIGHT_COUNT];
uniform vec3   light_color[MAX_LIGHT_COUNT];
uniform float  light_intensity[MAX_LIGHT_COUNT];
uniform float  light_decay_exponent[MAX_LIGHT_COUNT];
uniform vec3   light_shadow_color[MAX_LIGHT_COUNT];
uniform float  light_cutoff_costheta1[MAX_LIGHT_COUNT];
uniform float  light_cutoff_costheta2[MAX_LIGHT_COUNT];
uniform float  light_dropoff[MAX_LIGHT_COUNT];

uniform float shadow_gain = 0.2;
uniform int shadow_sample_count = 4;


#define DEBUG_COLOR vec3(1.0, 0.5, 0.5)
#define DEBUG_COLOR4 vec4(1.0, 0.5, 0.5, 1)

#define EPS 1e-7f
#define EPS3 vec3(EPS, EPS, EPS)

#define FRAG_INPUT VERT_OUTPUT

attribute FRAG_OUTPUT
{
    vec4 out_color : COLOR0;
};

)ogsfx" + std::string(R"ogsfx(
GLSLShader VolumeFragmentShader {

vec3 Swizzle(vec3 v, int i)
{
    if (i == 1)
        return v.zxy;
    else if (i == 2)
        return v.yzx;
    else
        return v;
}

float GetComponent(vec3 v, int i)
{
    if (i == 1)
        return v.y;
    else if (i == 2)
        return v.z;
    else
        return v.x;
}

float unlerp(float a, float b, float x)
{
    return (x - a) / (b - a);
}

float MaxComponent(vec3 v)
{
    return max(max(v.x, v.y), v.z);
}

float MinComponent(vec3 v)
{
    return min(min(v.x, v.y), v.z);
}

vec3 LinearFromSRGB(vec3 color)
{
    return pow(color, vec3(2.2f, 2.2f, 2.2f));
}

vec3 SRGBFromLinear(vec3 color)
{
    return pow(color, vec3(1.0f/2.2f, 1.0f/2.2f, 1.0f/2.2f));
}

vec4 SampleTexture1D(sampler1D sampler, float tex_coord, float lod)
{
    return textureLod(sampler, tex_coord, lod);
}

vec4 SampleTexture3D(sampler3D sampler, vec3 tex_coords, float lod)
{
    return textureLod(sampler, tex_coords, lod);
}

float SampleFloatRamp(sampler1D ramp_sampler, float texcoord)
{
    return SampleTexture1D(ramp_sampler, texcoord, 0).x;
}

vec3 SampleColorRamp(sampler1D ramp_sampler, float texcoord)
{
    return LinearFromSRGB(texture(ramp_sampler, texcoord).xyz);
}

#define SQR(x) ((x) * (x))
#define PI 3.14159265f
float BlackbodyRadiance(float temperature)
{
    const float sigma = 5.670367e-8f; // Stefan-Boltzmann constant
    float power = sigma * SQR(SQR(temperature));

    // non-physically correct control to reduce the intensity
    if (blackbody_intensity < 1.0f)
       power = lerp(sigma, power, max(blackbody_intensity, 0.0f));

    // convert power to spectral radiance
    return power * (1e-6f / PI);
}

vec3 BlackbodyColor(float temperature)
{
    float texcoord = (temperature - BLACKBODY_LUT_MIN_TEMP) / (BLACKBODY_LUT_MAX_TEMP - BLACKBODY_LUT_MIN_TEMP);
    return SampleColorRamp(blackbody_lut_sampler, texcoord) * BLACKBODY_LUT_NORMALIZER;
}

float CalcLOD(float distance_model, vec3 size_model)
{
    vec3 distance_voxels = (distance_model / size_model) * float(slice_count - 1);
    float max_component = MaxComponent(distance_voxels) + EPS;
    return clamp(log(max_component) / log(2.f), 0.f, 16.f);
}

float SampleDensityTexture(vec3 pos_model, float lod_scale_model)
{
    if (use_density_texture)
    {
        vec3 tex_coords = (pos_model - density_volume_origin) / density_volume_size;
        float lod = CalcLOD(lod_scale_model, density_volume_size);
        float tex_sample = SampleTexture3D(density_sampler, tex_coords, lod).r;
        float channel_value = lerp(density_value_range.x, density_value_range.y, tex_sample);
        if (density_source == DENSITY_SOURCE_RAMP)
            channel_value *= SampleFloatRamp(density_ramp_sampler, unlerp(density_ramp_domain.x, density_ramp_domain.y, channel_value));
        return density * channel_value;
    }
    else
        return density;
}

vec3 SampleScatteringTexture(vec3 pos_model, float lod_scale_model)
{
    float channel_value = 0;
    if (use_scattering_texture)
    {
        vec3 tex_coords = (pos_model - scattering_volume_origin) / scattering_volume_size;
        float lod = CalcLOD(lod_scale_model, scattering_volume_size);
        float voxel_value = SampleTexture3D(scattering_sampler, tex_coords, lod).r;
        channel_value = lerp(scattering_value_range.x, scattering_value_range.y, voxel_value);
    }

    vec3 res = scattering_color;
    if (scattering_color_source == COLOR_SOURCE_RAMP)
        res = SampleColorRamp(scattering_ramp_sampler, unlerp(scattering_ramp_domain.x, scattering_ramp_domain.y, channel_value));
    res *= scattering_intensity;

    if (use_scattering_texture)
        res *= channel_value;

    return res;
}

vec3 SampleTransparencyTexture(vec3 pos_model, float lod_scale_model)
{
    vec3 res = transparency;
    if (use_transparency_texture)
    {
        vec3 tex_coords = (pos_model - transparency_volume_origin) / transparency_volume_size;
        float lod = CalcLOD(lod_scale_model, transparency_volume_size);
        float voxel_value = SampleTexture3D(transparency_sampler, tex_coords, lod).r;
        res *= lerp(transparency_value_range.x, transparency_value_range.y, voxel_value);
    }

    return clamp(res, vec3(EPS, EPS, EPS), vec3(1, 1, 1));
}

float SampleTemperatureTexture(vec3 pos_model, float lod_scale_model)
{
    float res = temperature;
    if (use_temperature_texture)
    {
        vec3 tex_coords = (pos_model - temperature_volume_origin) / temperature_volume_size;
        float lod = CalcLOD(lod_scale_model, temperature_volume_size);
        float voxel_value = SampleTexture3D(temperature_sampler, tex_coords, lod).r;
        res *= lerp(temperature_value_range.x, temperature_value_range.y, voxel_value);
    }

    return res;
}

vec3 SampleEmissionTexture(vec3 pos_model, float lod_scale_model)
{
    if (emission_mode == EMISSION_MODE_NONE)
        return vec3(0, 0, 0);

    float channel_value = 0;
    if (use_emission_texture)
    {
        vec3 tex_coords = (pos_model - emission_volume_origin) / emission_volume_size;
        float lod = CalcLOD(lod_scale_model, emission_volume_size);
        float voxel_value = SampleTexture3D(emission_sampler, tex_coords, lod).r;
        channel_value = lerp(emission_value_range.x, emission_value_range.y, voxel_value);
    }

    vec3 res = emission_color;
    if (emission_color_source == COLOR_SOURCE_RAMP)
        res = SampleColorRamp(emission_ramp_sampler, unlerp(emission_ramp_domain.x, emission_ramp_domain.y, channel_value));
    res *= emission_intensity;

    if (use_emission_texture)
        res *= channel_value;

    res = max(vec3(0, 0, 0), res);

    if (emission_mode == EMISSION_MODE_BLACKBODY)
    {
        if (!use_temperature_texture)
            return vec3(0, 0, 0);

        float temperature = SampleTemperatureTexture(pos_model, lod_scale_model);
        if (temperature <= 0)
            return vec3(0, 0, 0);

        vec3 blackbody = BlackbodyColor(temperature) * BlackbodyRadiance(temperature);
        res *= blackbody;
    }

    return res;
}

#define ONE_OVER_4PI 0.07957747f
float HGPhase(float costheta)
{
    float g = scattering_anisotropy;
    float g_squared = g * g;
    return ONE_OVER_4PI * (1 - g_squared) / pow(1 + g_squared - 2*g*costheta, 1.5f);
}

vec3 RayTransmittance(vec3 from_world, vec3 to_world)
{
    vec3 step_world = (to_world - from_world) / float(shadow_sample_count + 1);
    float  step_size_world = length(step_world);
    vec3 step_model = (world_inverse_mat * vec4(step_world, 0)).xyz;
    float  step_size_model = length(step_model);

    vec3 from_model = (world_inverse_mat * vec4(from_world, 1)).xyz;

    vec3 transmittance = vec3(1, 1, 1);
    vec3 pos_model = from_model + 0.5f * step_model;
    for (int i = 0; i < shadow_sample_count; ++i) {
        float density = SampleDensityTexture(pos_model, step_size_model);
        vec3 transparency = SampleTransparencyTexture(pos_model, step_size_model);
        float exponent = density * step_size_world / (1.0 + float(i) * step_size_world * shadow_gain);
        transmittance *= pow(transparency, vec3(exponent, exponent, exponent));
        pos_model += step_model;
    }
    return transmittance;
}

vec3 StretchToVolumeSize(vec3 dir_world)
{
    vec3 dir_model = normalize((world_inverse_mat * vec4(dir_world, 0)).xyz);
    float len = MinComponent(abs(volume_size / dir_model));
    return (world_mat * vec4(len * dir_model, 0)).xyz;
}

int LightType(int light_index)
{
    return light_flags[light_index] & LIGHT_FLAG_MASK_TYPE;
}

vec3 ShadowFactor(int light_index, vec3 shadow_ray_begin, vec3 shadow_ray_end)
{
    vec3 transmittance = RayTransmittance(shadow_ray_begin, shadow_ray_end);
    return (vec3(1, 1, 1) - transmittance) * (vec3(1, 1, 1) - light_shadow_color[light_index]);
}

vec3 LightLuminanceDirectional(int light_index, vec3 pos_world, vec3 direction_to_eye_world, vec3 albedo)
{
    // Light luminance at source.
    vec3 lumi = light_color[light_index] * light_intensity[light_index];

    // Albedo.
    lumi *= albedo;

    // Phase.
    vec3 light_dir = light_direction[light_index];
    float phase = HGPhase(dot(direction_to_eye_world, light_dir));
    lumi *= phase;

    // Bail if light casts no shadows or shadowing practically wouldn't affect the outcome.
    if ((light_flags[light_index] & LIGHT_FLAG_CAST_SHADOWS) == 0)
        return lumi;

    // Shadow.
    vec3 shadow_vector = 0.5f * StretchToVolumeSize(-light_dir);
    lumi *= (vec3(1, 1, 1) - ShadowFactor(light_index, pos_world, pos_world + shadow_vector));

    return lumi;
}

vec3 LightLuminancePointSpot(int light_index, vec3 pos_world, vec3 direction_to_eye_world, vec3 albedo)
{
    // Light luminance at source.
    vec3 lumi = light_color[light_index] * light_intensity[light_index];

    // Albedo.
    lumi *= albedo;

    // Phase.
    vec3 vector_to_light_world = light_position[light_index] - pos_world;
    float  distance_to_light_world = max(length(vector_to_light_world), EPS);
    vec3 direction_to_light_world = vector_to_light_world / distance_to_light_world;
    float phase = HGPhase(dot(direction_to_eye_world, -direction_to_light_world));
    lumi *= phase;

    // Decay.
    lumi *= pow(distance_to_light_world, -light_decay_exponent[light_index]);

    // Angular shadowing for spot lights.
    if (LightType(light_index) == LIGHT_FLAG_SPOTLIGHT)
    {
        float costheta = dot(light_direction[light_index], -direction_to_light_world);

        // Cone.
        float cutoff1 = light_cutoff_costheta1[light_index];
        float cutoff2 = light_cutoff_costheta2[light_index];
        if (costheta < cutoff2)
            return vec3(0, 0, 0);
        else if (costheta < cutoff1)
            lumi *= (cutoff2 - costheta) / (cutoff2 - cutoff1);

        // Dropoff.
        lumi *= pow(costheta, light_dropoff[light_index]);
    }

    // Bail if light casts no shadows or shadowing practically wouldn't affect the outcome.
    if ((light_flags[light_index] & LIGHT_FLAG_CAST_SHADOWS) == 0)
        return lumi;

    // Shadow.
    vec3 shadow_vector = vector_to_light_world;
    float  max_distance_world = length(StretchToVolumeSize(vector_to_light_world));
    if (distance_to_light_world > max_distance_world)
        shadow_vector = direction_to_light_world * max_distance_world;
    lumi *= (vec3(1, 1, 1) - ShadowFactor(light_index, pos_world, pos_world + shadow_vector));

    return lumi;
}

vec3 LightLuminance(int light_index, vec3 pos_world, vec3 direction_to_eye_world, vec3 albedo)
{
    int type = LightType(light_index);
    if (type == LIGHT_FLAG_POINT_LIGHT || type == LIGHT_FLAG_SPOTLIGHT)
        return LightLuminancePointSpot(light_index, pos_world, direction_to_eye_world, albedo);
    else if (type == LIGHT_FLAG_DIRECTIONAL_LIGHT)
        return LightLuminanceDirectional(light_index, pos_world, direction_to_eye_world, albedo);
    else
        return DEBUG_COLOR; // Unsupported light.
}

void main()
{
    float density = SampleDensityTexture(fragment_input.pos_model, 0);
    vec3 transparency = SampleTransparencyTexture(fragment_input.pos_model, 0);
    vec3 albedo = SampleScatteringTexture(fragment_input.pos_model, 0);
    // Note: albedo is scattering / extinction. Intuitively light lumi should
    //       be multiplied by scattering, but because
    //         integral(exp(a*t)dt) = 1/a exp(a*t),
    //       and light contribution from in-scattering is
    //         integral_0^t(exp(-extinciton*t)*phase*light_radiance dt)
    //       evaluating the integral will yeild a 1/extinction factor, assuming
    //       piecewise constant phase and light radiance.

    vec3 direction_to_eye_world = normalize(view_pos_world - fragment_input.pos_world);

    vec3 slice_vector_model = Swizzle(vec3(1, 0, 0), dominant_axis) * volume_size / float(slice_count - 1);
    vec3 slice_vector_world = (world_mat * vec4(slice_vector_model, 0)).xyz;
    float ray_distance = dot(slice_vector_world, slice_vector_world) / abs(dot(slice_vector_world, direction_to_eye_world));

    vec3 lumi = vec3(0, 0, 0);

    // In-scattering from lights.

    for (int i = 0; i < light_count; ++i)
        lumi += LightLuminance(i, fragment_input.pos_world, direction_to_eye_world, albedo);

    // Premultiply alpha.
    float exponent = density * ray_distance;
    vec3 transmittance = pow(transparency, vec3(exponent, exponent, exponent));
    float alpha = 1 - dot(transmittance, vec3(1, 1, 1) / 3);
    lumi *= alpha;

    // Emission.

    vec3 emission = SampleEmissionTexture(fragment_input.pos_model, 0);
    if (emission_mode == EMISSION_MODE_DENSITY)
        emission *= density;

    vec3 extinction = density * -log(transparency);
    vec3 x = -ray_distance * extinction;
    // Truncated series of (1 - exp(-dt)) / t; d = ray_distance, t = extinction.
    vec3 emission_factor = ray_distance * (1 - x * (0.5f + x * (1.0f/6.0f - x / 24.0f)));
    emission *= emission_factor;
    lumi += emission;

    out_color = vec4(lumi, alpha);

    if (isnan(out_color.x) || isnan(out_color.y) ||
        isnan(out_color.z) || isnan(out_color.w))
        out_color = DEBUG_COLOR4;
}
}

technique Main < string Transparency = "Transparent"; int isTransparent = 1; string OverridesDrawState = "true"; >
{
    pass P0
    {
        VertexShader (in VERT_INPUT, out VERT_OUTPUT vertex_output) = VolumeVertexShader;
        PixelShader (in FRAG_INPUT fragment_input, out FRAG_OUTPUT) = VolumeFragmentShader;
    }
}

)ogsfx");

} // unnamed namespace

VolumeShader::VolumeShader()
{
    if (s_refcount == 0)
    {
        // Load the shader.
        loadShader();
        // The first instance uses the static MShaderInstance.
        m_shader_ptr = s_volume_shader.get();
    }
    else if (s_volume_shader.get() != nullptr)
    {
        // Not the first instance, clone the static shader.
        m_shader_clone_owner.reset(s_volume_shader->clone());
        m_shader_ptr = m_shader_clone_owner.get();
    }
    ++s_refcount;
}

VolumeShader::~VolumeShader()
{
    --s_refcount;
    if (s_refcount == 0)
        // Last instance; release the static MShaderInstance.
        s_volume_shader.reset();
}

void VolumeShader::loadShader()
{
    const auto renderer = MHWRender::MRenderer::theRenderer();
    if (!renderer)
        return;

    // Only OpenGL core profile is supported.
    if (renderer->drawAPI() != MHWRender::kOpenGLCoreProfile) {
        MGlobal::displayError("[openvdb] the vdb_visualizer node only supports "
                              "the OpenGL Core Profile rendering backend.");
        return;
    }

    const auto shader_manager = renderer->getShaderManager();
    if (!shader_manager)
        return;

    MHWRender::MShaderCompileMacro macros[] = { makeMacroDef("BLACKBODY_LUT_MIN_TEMP",   Blackbody::TEMPERATURE_MIN),
                                                makeMacroDef("BLACKBODY_LUT_MAX_TEMP",   Blackbody::TEMPERATURE_MAX),
                                                makeMacroDef("BLACKBODY_LUT_NORMALIZER", Blackbody::LUT_NORMALIZER),
                                                makeMacroDef("MAX_LIGHT_COUNT",          MAX_LIGHT_COUNT) };
    constexpr int macro_count = sizeof(macros) / sizeof(MHWRender::MShaderCompileMacro);
    s_volume_shader.reset(shader_manager->getEffectsBufferShader(
            volume_ogsfx.c_str(), unsigned(volume_ogsfx.size()),
            "Main", macros, macro_count, /*useEffectCache=*/false, preDrawCallback));
    if (!s_volume_shader) {
        return;
    }

    s_volume_shader->setIsTransparent(true);
}

namespace {

    typedef std::array<float, 3> Float3;

    template <typename ParamSpec>
    void getLightParam(MHWRender::MLightParameterInformation* light_params, const ParamSpec& param_spec, float& output) {
        MFloatArray float_array;
        if (light_params->getParameter(param_spec, float_array) == MStatus::kSuccess)
            output = float_array[0];
    };

    template <typename ParamSpec>
    void getLightParam(MHWRender::MLightParameterInformation* light_params, const ParamSpec& param_spec, Float3& output) {
        MFloatArray float_array;
        if (light_params->getParameter(param_spec, float_array) == MStatus::kSuccess)
            memcpy(&output, &float_array[0], 3 * sizeof(float));
    };

    template <int N>
    union Float3Array
    {
        std::array<float, 3 * N> float_array;
        std::array<Float3, N> float3_array;
    };

} // unnamed namespace

void VolumeShader::preDrawCallback(MHWRender::MDrawContext& context, const MHWRender::MRenderItemList& /*renderItemList*/, MHWRender::MShaderInstance* shader_instance)
{
    // Check for errors.
    if (shader_instance->bind(context) != MStatus::kSuccess) {
        const auto renderer = MHWRender::MRenderer::theRenderer();
        if (!renderer)
            return;
        const auto shader_manager = renderer->getShaderManager();
        if (!shader_manager)
            return;
        std::stringstream ss;
        MGlobal::displayError(format("[openvdb] Failed to compile volume shader: ^1s", shader_manager->getLastError()));
        return;
    }

    // Set states.
    {
        auto state_manager = context.getStateManager();

        // Blend state.
        MHWRender::MBlendStateDesc blend_state_desc;
        blend_state_desc.setDefaults();
        for (int i = 0; i < (blend_state_desc.independentBlendEnable ? MHWRender::MBlendState::kMaxTargets : 1); ++i) {
            blend_state_desc.targetBlends[i].blendEnable = true;
            blend_state_desc.targetBlends[i].sourceBlend = MHWRender::MBlendState::kOne;
            blend_state_desc.targetBlends[i].destinationBlend = MHWRender::MBlendState::kInvSourceAlpha;
            blend_state_desc.targetBlends[i].blendOperation = MHWRender::MBlendState::kAdd;
            blend_state_desc.targetBlends[i].alphaSourceBlend = MHWRender::MBlendState::kOne;
            blend_state_desc.targetBlends[i].alphaDestinationBlend = MHWRender::MBlendState::kInvSourceAlpha;
            blend_state_desc.targetBlends[i].alphaBlendOperation = MHWRender::MBlendState::kAdd;
        }
        blend_state_desc.blendFactor[0] = 1.0f;
        blend_state_desc.blendFactor[1] = 1.0f;
        blend_state_desc.blendFactor[2] = 1.0f;
        blend_state_desc.blendFactor[3] = 1.0f;
        const MHWRender::MBlendState* blend_state = state_manager->acquireBlendState(blend_state_desc);
        CHECK_MSTATUS(state_manager->setBlendState(blend_state));

        // Rasterization state.
        MHWRender::MRasterizerStateDesc raster_state_desc;
        raster_state_desc.setDefaults();
        raster_state_desc.cullMode = MRasterizerState::kCullNone;
        CHECK_MSTATUS(state_manager->setRasterizerState(state_manager->acquireRasterizerState(raster_state_desc)));
    }

    // Set view position.
    {
        MStatus status;
        const auto view_pos = context.getTuple(MHWRender::MFrameContext::kViewPosition, &status);
        CHECK_MSTATUS(status);
        CHECK_MSTATUS(shader_instance->setParameter("view_pos_world", MFloatVector(float(view_pos[0]), float(view_pos[1]), float(view_pos[2]))));
    }

    // Set matrices.
    {
        MStatus status;

        const auto world_mat = context.getMatrix(MHWRender::MFrameContext::kWorldMtx, &status);
        CHECK_MSTATUS(status);
        CHECK_MSTATUS(shader_instance->setParameter("world_mat", world_mat));

        const auto world_inverse_mat = context.getMatrix(MHWRender::MFrameContext::kWorldInverseMtx, &status);
        CHECK_MSTATUS(status);
        CHECK_MSTATUS(shader_instance->setParameter("world_inverse_mat", world_inverse_mat));

        const auto world_view_proj_mat = context.getMatrix(MHWRender::MFrameContext::kWorldViewProjMtx, &status);
        CHECK_MSTATUS(status);
        CHECK_MSTATUS(shader_instance->setParameter("world_view_proj_mat", world_view_proj_mat));
    }

    // Set view direction in model space and dominant axis index (0: x, 1: y, 2: z).
    {
        MStatus status;
        const auto view_dir_world_d = context.getTuple(MHWRender::MFrameContext::kViewDirection, &status);
        auto view_dir_world = MVector(view_dir_world_d[0], view_dir_world_d[1], view_dir_world_d[2]);
        CHECK_MSTATUS(status);
        CHECK_MSTATUS(shader_instance->setParameter("view_dir_world", MFloatVector(view_dir_world)));

        const auto world_inverse_mat = context.getMatrix(MHWRender::MFrameContext::kWorldInverseMtx, &status);
        CHECK_MSTATUS(status);
        auto view_dir_model = view_dir_world;
        view_dir_model *= world_inverse_mat;
        view_dir_model.normalize();
        CHECK_MSTATUS(shader_instance->setParameter("view_dir_model", MFloatVector(view_dir_model)));

        const float ax = std::fabs(float(view_dir_model.x));
        const float ay = std::fabs(float(view_dir_model.y));
        const float az = std::fabs(float(view_dir_model.z));
        int dominant_axis = 0;
        if (ay > ax && ay > az)
            dominant_axis = 1;
        else if (az > ax && az > ay)
            dominant_axis = 2;
        CHECK_MSTATUS(shader_instance->setParameter("dominant_axis", dominant_axis));
    }

    // Collect light data.

    constexpr int LIGHT_FLAG_POINT_LIGHT       = 0;
    constexpr int LIGHT_FLAG_DIRECTIONAL_LIGHT = 1;
    constexpr int LIGHT_FLAG_SPOTLIGHT         = 2;
    constexpr int LIGHT_FLAG_CAST_SHADOWS      = 8;

    Float3Array<MAX_LIGHT_COUNT> light_position;
    Float3Array<MAX_LIGHT_COUNT> light_direction;
    Float3Array<MAX_LIGHT_COUNT> light_color;
    Float3Array<MAX_LIGHT_COUNT> light_shadow_color;
    std::array<int,   MAX_LIGHT_COUNT> light_flags;
    std::array<float, MAX_LIGHT_COUNT> light_intensity;
    std::array<float, MAX_LIGHT_COUNT> light_decay_exponent;
    std::array<float, MAX_LIGHT_COUNT> light_cutoff_costheta1;
    std::array<float, MAX_LIGHT_COUNT> light_cutoff_costheta2;
    std::array<float, MAX_LIGHT_COUNT> light_dropoff;

    using MHWRender::MLightParameterInformation;

    const auto light_count_total = std::min(context.numberOfActiveLights(), MAX_LIGHT_COUNT);
    int shader_light_count = 0;
    for (unsigned int i = 0; i < light_count_total; ++i)
    {
        MIntArray int_array;
        MFloatArray float_array;

        const auto light_params = context.getLightParameterInformation(i);

        // Proceed to next light if this light is not enabled.
        {
            const auto status = light_params->getParameter(MLightParameterInformation::kLightEnabled, float_array);
            if (status != MS::kSuccess || float_array[0] != 1)
                continue;
        }

        light_flags[shader_light_count] = 0;

        // Light type.
        const auto light_type = light_params->lightType();
        if (light_type == "pointLight")
            light_flags[shader_light_count] |= LIGHT_FLAG_POINT_LIGHT;
        else if (light_type == "directionalLight")
            light_flags[shader_light_count] |= LIGHT_FLAG_DIRECTIONAL_LIGHT;
        else if (light_type == "spotLight")
            light_flags[shader_light_count] |= LIGHT_FLAG_SPOTLIGHT;
        else
        {
            MGlobal::displayError("[openvdb] Unsupported light type: " + light_type);
            continue;
        }

        // Position.
        getLightParam(light_params, MLightParameterInformation::kWorldPosition, light_position.float3_array[shader_light_count]);

        // Direction.
        getLightParam(light_params, MLightParameterInformation::kWorldDirection, light_direction.float3_array[shader_light_count]);

        // Color.
        getLightParam(light_params, MLightParameterInformation::kColor, light_color.float3_array[shader_light_count]);

        // Intensity.
        getLightParam(light_params, MLightParameterInformation::kIntensity, light_intensity[shader_light_count]);

        // Shadow.
        {
            // Cast shadows if the ShadowOn param is on, or true by default if there is no such param.
            auto status = light_params->getParameter(MLightParameterInformation::kShadowOn, int_array);
            if (status == MStatus::kSuccess && int_array[0] == 1) {
                light_flags[shader_light_count] |= LIGHT_FLAG_CAST_SHADOWS;
            }
#if MAYA_API_VERSION < 201600
            // ShadowOn attrib of point lights is buggy in Maya 2015, cast shadows by default.
            if (light_type == "pointLight") {
                light_flags[shader_light_count] |= LIGHT_FLAG_CAST_SHADOWS;
            }
#endif
        }

        // Shadow color.
        getLightParam(light_params, MLightParameterInformation::kShadowColor, light_shadow_color.float3_array[shader_light_count]);

        // Decay rate.
        getLightParam(light_params, MLightParameterInformation::kDecayRate, light_decay_exponent[shader_light_count]);

        // Spot light dropoff.
        getLightParam(light_params, MLightParameterInformation::kDropoff, light_dropoff[shader_light_count]);

        // Spot light cutoffs.
        if (light_params->getParameter(MLightParameterInformation::kCosConeAngle, float_array) == MStatus::kSuccess)
        {
            light_cutoff_costheta1[shader_light_count] = float_array[0];
            light_cutoff_costheta2[shader_light_count] = float_array[1];
        }

        shader_light_count++;
    }

#if MAYA_API_VERSION < 201700
    // Convert colors to linear color space.
    LinearFromSRGB(light_color.float_array.data(), 3 * shader_light_count);
    LinearFromSRGB(light_shadow_color.float_array.data(), 3 * shader_light_count);
#endif

    // Set shader params.
    CHECK_MSTATUS(shader_instance->setParameter("light_count", shader_light_count));
    CHECK_MSTATUS(shader_instance->setArrayParameter("light_flags", light_flags.data(), shader_light_count));
    CHECK_MSTATUS(shader_instance->setArrayParameter("light_position", light_position.float_array.data(), shader_light_count));
    CHECK_MSTATUS(shader_instance->setArrayParameter("light_direction", light_direction.float_array.data(), shader_light_count));
    CHECK_MSTATUS(shader_instance->setArrayParameter("light_color", light_color.float_array.data(), shader_light_count));
    CHECK_MSTATUS(shader_instance->setArrayParameter("light_intensity", light_intensity.data(), shader_light_count));
    CHECK_MSTATUS(shader_instance->setArrayParameter("light_shadow_color", light_shadow_color.float_array.data(), shader_light_count));
    CHECK_MSTATUS(shader_instance->setArrayParameter("light_decay_exponent", light_decay_exponent.data(), shader_light_count));
    CHECK_MSTATUS(shader_instance->setArrayParameter("light_cutoff_costheta1", light_cutoff_costheta1.data(), shader_light_count));
    CHECK_MSTATUS(shader_instance->setArrayParameter("light_cutoff_costheta2", light_cutoff_costheta2.data(), shader_light_count));
    CHECK_MSTATUS(shader_instance->setArrayParameter("light_dropoff", light_dropoff.data(), shader_light_count));
}


// === SlicedDisplay =======================================================

class VDBSlicedDisplayImpl {
public:
    VDBSlicedDisplayImpl(MHWRender::MPxSubSceneOverride& parent);
    ~VDBSlicedDisplayImpl();
    bool update(
        MHWRender::MSubSceneContainer& container,
        const openvdb::io::File* vdb_file,
        const MBoundingBox& vdb_bbox,
        const VDBSlicedDisplayData& data,
        VDBSlicedDisplayChangeSet& changes);
    void setWorldMatrices(const MMatrixArray& world_matrices);
    void enable(bool enable);

private:
    bool initRenderItems(MHWRender::MSubSceneContainer& container);

    void updateSliceGeo(const MBoundingBox& bbox, int slice_count);

    MHWRender::MPxSubSceneOverride& m_parent;

    VolumeShader m_volume_shader;

    VolumeParam m_density_channel;
    VolumeParam m_scattering_channel;
    VolumeParam m_emission_channel;
    VolumeParam m_transparency_channel;
    VolumeParam m_temperature_channel;

    // Must be the same as Gradient resolution.
    static const unsigned int RAMP_RESOLUTION;
    FloatRampTexture m_density_ramp;
    RGBRampTexture m_scattering_ramp;
    RGBRampTexture m_emission_ramp;

    BlackbodyLUT m_blackbody_lut;

    Renderable m_slices_renderable;
    SamplerStatePtr m_volume_sampler_state;

    bool m_enabled;
    bool m_selected;
};
const unsigned int VDBSlicedDisplayImpl::RAMP_RESOLUTION = 128;


// === Sliced display mode implementation ===================================

VDBSlicedDisplayImpl::VDBSlicedDisplayImpl(MHWRender::MPxSubSceneOverride& parent)
    : m_parent(parent)
    , m_density_channel("density"), m_scattering_channel("scattering"), m_emission_channel("emission"), m_transparency_channel("transparency"), m_temperature_channel("temperature")
    , m_density_ramp(RAMP_RESOLUTION), m_scattering_ramp(RAMP_RESOLUTION), m_emission_ramp(RAMP_RESOLUTION)
    , m_volume_sampler_state(createSamplerState(MHWRender::MSamplerState::kMinMagMipLinear, MHWRender::MSamplerState::kTexBorder))
    , m_enabled(false), m_selected(false)
{
    if (!m_volume_shader)
        return;

    VolumeCache::registerUsage();

    m_density_channel.setShaderInstance(m_volume_shader.get());
    m_scattering_channel.setShaderInstance(m_volume_shader.get());
    m_emission_channel.setShaderInstance(m_volume_shader.get());
    m_transparency_channel.setShaderInstance(m_volume_shader.get());
    m_temperature_channel.setShaderInstance(m_volume_shader.get());

    // Create sampler state for textures.
    for (MString param : { "density_sampler", "scattering_sampler", "transparency_sampler", "emission_sampler", "temperature_sampler" })
        CHECK_MSTATUS(m_volume_shader->setParameter(param, *m_volume_sampler_state));

    // Assign ramp sampler states.
    (m_volume_shader->setParameter("density_ramp_sampler", m_density_ramp.getSamplerState()));
    (m_volume_shader->setParameter("scattering_ramp_sampler", m_scattering_ramp.getSamplerState()));
    (m_volume_shader->setParameter("emission_ramp_sampler", m_emission_ramp.getSamplerState()));

    // Set up blackbody LUT texture.
    CHECK_MSTATUS(m_volume_shader->setParameter("blackbody_lut_sampler", m_blackbody_lut.lut.getSamplerState()));
    CHECK_MSTATUS(m_blackbody_lut.lut.assignToShader(m_volume_shader.get(), "blackbody_lut_texture"));
}

VDBSlicedDisplayImpl::~VDBSlicedDisplayImpl()
{
    VolumeCache::unregisterUsage();
}

void VDBSlicedDisplayImpl::enable(bool enable)
{
    m_enabled = enable;
    if (m_slices_renderable.render_item)
        m_slices_renderable.render_item->enable(m_enabled);
}

namespace {

    const char *SLICES_RENDER_ITEM_NAME = "vdb_volume_slices";

} // unnamed namespace

bool VDBSlicedDisplayImpl::initRenderItems(MHWRender::MSubSceneContainer& container)
{
    if (!container.find(SLICES_RENDER_ITEM_NAME)) {
        auto render_item = MHWRender::MRenderItem::Create(
                SLICES_RENDER_ITEM_NAME,
                MHWRender::MRenderItem::RenderItemType::MaterialSceneItem,
                MHWRender::MGeometry::kTriangles);
        render_item->setDrawMode(MHWRender::MGeometry::kAll);
        render_item->castsShadows(false);
        render_item->receivesShadows(false);
        if (!render_item->setShader(m_volume_shader.get())) {
            MGlobal::displayError("[openvdb] Could not set shader for volume render item.");
            return false;
        }

        // Create geo buffers.
        // Note: descriptor name (first ctor arg) MUST be "", or setGeometryForRenderItem will return kFailure.
        const MHWRender::MVertexBufferDescriptor pos_desc("", MHWRender::MGeometry::kPosition, MHWRender::MGeometry::kFloat, 3);
        m_slices_renderable.position_buffer.reset(new MHWRender::MVertexBuffer(pos_desc));
        m_slices_renderable.index_buffer.reset(new MHWRender::MIndexBuffer(MHWRender::MGeometry::kUnsignedInt32));

        // Add render item to subscene container.
        if (!container.add(render_item)) {
            MGlobal::displayError("[openvdb] Could not add m_slices_renderable render item.");
            return false;
        }
        m_slices_renderable.render_item = render_item;
    }

    return true;
}

void VDBSlicedDisplayImpl::updateSliceGeo(const MBoundingBox& bbox, int slice_count)
{
    assert(slice_count > 0);

    // - Vertices
    const auto vertex_count = slice_count * 4;
    MFloatVector* positions = reinterpret_cast<MFloatVector*>(m_slices_renderable.position_buffer->acquire(vertex_count, true));
    for (int i = 0; i < slice_count; ++i) {
        const auto z = float(i);
        positions[4 * i + 0] = MFloatVector(0.0, 0.0, z);
        positions[4 * i + 1] = MFloatVector(1.0, 0.0, z);
        positions[4 * i + 2] = MFloatVector(0.0, 1.0, z);
        positions[4 * i + 3] = MFloatVector(1.0, 1.0, z);
    }
    m_slices_renderable.position_buffer->commit(positions);

    // - Indices
    const auto index_count = slice_count * 6;
    unsigned int* indices = reinterpret_cast<unsigned int*>(m_slices_renderable.index_buffer->acquire(index_count, true));
    for (int i = 0; i < slice_count; ++i) {
        indices[6 * i + 0] = 4 * i + 0;
        indices[6 * i + 1] = 4 * i + 1;
        indices[6 * i + 2] = 4 * i + 3;
        indices[6 * i + 3] = 4 * i + 0;
        indices[6 * i + 4] = 4 * i + 3;
        indices[6 * i + 5] = 4 * i + 2;
    }
    m_slices_renderable.index_buffer->commit(indices);

    m_slices_renderable.vertex_buffer_array.clear();
    CHECK_MSTATUS(m_slices_renderable.vertex_buffer_array.addBuffer("pos_model", m_slices_renderable.position_buffer.get()));
    m_slices_renderable.update(m_parent, bbox);

    CHECK_MSTATUS(m_volume_shader->setParameter("slice_count", slice_count));
}

bool VDBSlicedDisplayImpl::update(
        MHWRender::MSubSceneContainer& container,
        const openvdb::io::File* vdb_file,
        const MBoundingBox& vdb_bbox,
        const VDBSlicedDisplayData& data,
        VDBSlicedDisplayChangeSet& changes)
{
    if (changes == VDBSlicedDisplayChangeSet::NO_CHANGES)
        return false;

    if (!m_volume_shader)
        return false;

    initRenderItems(container);
    if (!m_slices_renderable)
        return false;

    m_slices_renderable.render_item->enable(m_enabled);

    // Update slice geometry if the number of slices has changed.
    assert(data.slice_count > 0);
    if (hasChange(changes, VDBSlicedDisplayChangeSet::BOUNDING_BOX | VDBSlicedDisplayChangeSet::SLICE_COUNT))
        updateSliceGeo(vdb_bbox, data.slice_count);

    if (hasChange(changes, VDBSlicedDisplayChangeSet::BOUNDING_BOX)) {
        CHECK_MSTATUS(m_volume_shader->setParameter("volume_origin", vdb_bbox.min()));
        CHECK_MSTATUS(m_volume_shader->setParameter("volume_size", vdb_bbox.max() - vdb_bbox.min()));
    }

    // Update shader params.
    CHECK_MSTATUS(m_volume_shader->setParameter("density", data.density));
    CHECK_MSTATUS(m_volume_shader->setParameter("density_source", int(data.density_source)));
    CHECK_MSTATUS(m_volume_shader->setParameter("scattering_intensity", data.scatter));
    CHECK_MSTATUS(m_volume_shader->setParameter("scattering_color", data.scatter_color));
    CHECK_MSTATUS(m_volume_shader->setParameter("scattering_color_source", int(data.scatter_color_source)));
    CHECK_MSTATUS(m_volume_shader->setParameter("scattering_anisotropy", data.scatter_anisotropy));
    CHECK_MSTATUS(m_volume_shader->setParameter("transparency", data.transparent));
    CHECK_MSTATUS(m_volume_shader->setParameter("emission_mode", int(data.emission_mode)));
    CHECK_MSTATUS(m_volume_shader->setParameter("emission_intensity", data.emission));
    CHECK_MSTATUS(m_volume_shader->setParameter("emission_color", data.emission_color));
    CHECK_MSTATUS(m_volume_shader->setParameter("emission_color_source", int(data.emission_source)));
    CHECK_MSTATUS(m_volume_shader->setParameter("temperature", data.temperature * data.blackbody_kelvin));
    CHECK_MSTATUS(m_volume_shader->setParameter("blackbody_intensity", data.blackbody_intensity));
    CHECK_MSTATUS(m_volume_shader->setParameter("shadow_gain", data.shadow_gain));
    CHECK_MSTATUS(m_volume_shader->setParameter("shadow_sample_count", data.shadow_sample_count));

    // Update ramp domains.
    CHECK_MSTATUS(m_volume_shader->setParameter("density_ramp_domain", MFloatVector(data.density_ramp.input_min, data.density_ramp.input_max)));
    CHECK_MSTATUS(m_volume_shader->setParameter("scattering_ramp_domain", MFloatVector(data.scatter_color_ramp.input_min, data.scatter_color_ramp.input_max)));
    CHECK_MSTATUS(m_volume_shader->setParameter("emission_ramp_domain", MFloatVector(data.emission_ramp.input_min, data.emission_ramp.input_max)));

    // Update ramp textures.
    if (hasChange(changes, VDBSlicedDisplayChangeSet::DENSITY_RAMP_SAMPLES)) {
        m_density_ramp.updateSamples(data.density_ramp.samples);
        CHECK_MSTATUS(m_density_ramp.assignToShader(m_volume_shader.get(), "density_ramp_texture"));
    }
    if (hasChange(changes, VDBSlicedDisplayChangeSet::SCATTER_COLOR_RAMP_SAMPLES)) {
        m_scattering_ramp.updateSamples(data.scatter_color_ramp.samples);
        CHECK_MSTATUS(m_scattering_ramp.assignToShader(m_volume_shader.get(), "scattering_ramp_texture"));
    }
    if (hasChange(changes, VDBSlicedDisplayChangeSet::EMISSION_RAMP_SAMPLES)) {
        m_emission_ramp.updateSamples(data.emission_ramp.samples);
        CHECK_MSTATUS(m_emission_ramp.assignToShader(m_volume_shader.get(), "emission_ramp_texture"));
    }

    // Update volumes.
    const auto extents = openvdb::Coord(data.slice_count, data.slice_count, data.slice_count);
    if (hasChange(changes, VDBSlicedDisplayChangeSet::DENSITY_CHANNEL))
        m_density_channel.loadVolume({ vdb_file->filename(), vdb_file->getUniqueTag(), data.density_channel, extents });
    if (hasChange(changes, VDBSlicedDisplayChangeSet::SCATTER_COLOR_CHANNEL))
        m_scattering_channel.loadVolume({ vdb_file->filename(), vdb_file->getUniqueTag(), data.scatter_color_channel, extents });
    if (hasChange(changes, VDBSlicedDisplayChangeSet::TRANSPARENT_CHANNEL))
        m_transparency_channel.loadVolume({ vdb_file->filename(), vdb_file->getUniqueTag(), data.transparent_channel, extents });
    if (hasChange(changes, VDBSlicedDisplayChangeSet::EMISSION_CHANNEL))
        m_emission_channel.loadVolume({ vdb_file->filename(), vdb_file->getUniqueTag(), data.emission_channel, extents });
    if (hasChange(changes, VDBSlicedDisplayChangeSet::TEMPERATURE_CHANNEL))
        m_temperature_channel.loadVolume({ vdb_file->filename(), vdb_file->getUniqueTag(), data.temperature_channel, extents });

    changes = VDBSlicedDisplayChangeSet::NO_CHANGES;


    return true;
}

void VDBSlicedDisplayImpl::setWorldMatrices(const MMatrixArray& world_matrices)
{
    if (!m_enabled)
        return;
    if (world_matrices.length() == 0)
        return;

    if (m_slices_renderable.render_item) {
        if (world_matrices.length() == 1)
            m_slices_renderable.render_item->setMatrix(&world_matrices[0]);
        else
            m_parent.setInstanceTransformArray(*m_slices_renderable.render_item, world_matrices);
    }
}

MSyntax VDBVolumeCacheCmd::create_syntax()
{
    MSyntax syntax;
    syntax.enableQuery();
    syntax.enableEdit();
    syntax.addFlag("h", "help", MSyntax::kNoArg);
    syntax.addFlag("l", "limit", MSyntax::kLong);
    syntax.makeFlagQueryWithFullArgs("limit", true);
    syntax.addFlag("vt", "voxelType", MSyntax::kString);
    syntax.makeFlagQueryWithFullArgs("voxelType", true);
    return syntax;
}

namespace
{
    MString getVoxelTypeString()
    {
        const auto voxel_type = VolumeCache::instance().getVoxelType();
        if (voxel_type == VolumeCache::VoxelType::HALF)
            return "half";
        else if (voxel_type == VolumeCache::VoxelType::FLOAT)
            return "float";
        else
            return "unknown";
    }

} // unnamed namespace


// === VDBVolumeCacheCmd =======================================================

MStatus VDBVolumeCacheCmd::doIt(const MArgList& args)
{
    MStatus status;
    MArgParser parser(syntax(), args);

    const auto display_error = [](const MString& message) {
        MGlobal::displayError(format("[openvdb] command ^1s: ^2s", COMMAND_STRING, message));
    };

    if (parser.isEdit()) {
        if (parser.isFlagSet("limit")) {
            // Set volume cache limit to the given value in gigabytes.
            const int new_limit_gigabytes = parser.flagArgumentInt("limit", 0, &status);
            if (status != MStatus::kSuccess || new_limit_gigabytes < 0) {
                display_error("In edit mode argument to 'limit' has to be a non-negative integer representing gigabytes.");
                return MS::kFailure;
            }

            VolumeCache::instance().setMemoryLimitBytes(size_t(new_limit_gigabytes) << 30);
        }

        if (parser.isFlagSet("voxelType")) {
            const auto voxel_type_str = parser.flagArgumentString("voxelType", 0, &status);
            if (status != MStatus::kSuccess) {
                display_error("In edit mode the 'voxelType' flag requires a string argument, either 'half' or 'float'.");
                return MS::kFailure;
            }

            if (voxel_type_str == "half")
                VolumeCache::instance().setVoxelType(VolumeCache::VoxelType::HALF);
            else if (voxel_type_str == "float")
                VolumeCache::instance().setVoxelType(VolumeCache::VoxelType::FLOAT);
            else {
                display_error("In edit mode argument to 'voxelType' has to be either 'half' or 'float'.");
                return MS::kFailure;
            }
        }

        return MS::kSuccess;
    } else if (parser.isQuery()) {
        if (parser.isFlagSet("limit")) {
            // Return volume cache limit in gigabytes.
            const size_t limit_bytes = VolumeCache::instance().getMemoryLimitBytes();
            MPxCommand::setResult(unsigned(limit_bytes / (1 << 30)));
            return MS::kSuccess;
        } else if (parser.isFlagSet("voxelType")) {
            // Return the voxel type as string.
            MPxCommand::setResult(getVoxelTypeString());
            return MS::kSuccess;
        }

        display_error("In query mode either 'limit' or 'voxelType' flag has to be specified.");
        return MS::kFailure;
    }

    // Neither edit nor query mode: display info.

    if (parser.isFlagSet("voxelType")) {
        // Display voxel type.
        MGlobal::displayInfo(format("[openvdb] volume cache voxel type is '^1s'.", getVoxelTypeString()));
        return MS::kSuccess;
    } else if (parser.isFlagSet("limit")) {
        // Display allocated bytes and cache limit.
        const auto pretty_string_size = [](size_t size) -> std::string {
            static const std::array<char, 3> prefixes = { { 'G', 'M', 'K' } };

            std::stringstream ss;
            ss << std::setprecision(2) << std::setiosflags(std::ios_base::fixed);

            for (size_t i = 0; i < prefixes.size(); ++i) {
                size_t prefix_size = 1LL << (10 * (prefixes.size() - i));
                if (size < prefix_size)
                    continue;

                ss << double(size) / double(prefix_size) << prefixes[i] << "B";
                return ss.str();
            }

            ss << double(size) << "B";
            return ss.str();
        };

        const size_t limit = VolumeCache::instance().getMemoryLimitBytes();
        if (limit == 0) {
            MGlobal::displayInfo("[openvdb] Volume caching is off.");
            return MS::kSuccess;
        }

        const size_t alloc = VolumeCache::instance().getAllocatedBytes();
        MGlobal::displayInfo(format("[openvdb] Volume cache allocated/total: ^1s/^2s.",
            pretty_string_size(alloc),
            pretty_string_size(limit)));
        return MS::kSuccess;
    }

    // Default: display help.
    MGlobal::displayInfo(format("[openvdb] Usage: ^1s [-h|-help] [-q|-query|-e|-edit] [-vt|-voxelType [\"half\"|\"float\"]] [-l|-limit [<limit_in_gigabytes>]]", COMMAND_STRING));
    return MS::kSuccess;
}

const char *VDBVolumeCacheCmd::COMMAND_STRING = "vdb_visualizer_volume_cache";

VDBSlicedDisplay::VDBSlicedDisplay(MHWRender::MPxSubSceneOverride& parent)
    : m_impl(new VDBSlicedDisplayImpl(parent))
{
}

VDBSlicedDisplay::~VDBSlicedDisplay()
{
}

bool VDBSlicedDisplay::update(
        MHWRender::MSubSceneContainer& container,
        const openvdb::io::File* vdb_file,
        const MBoundingBox& vdb_bbox,
        const VDBSlicedDisplayData& data,
        VDBSlicedDisplayChangeSet& changes)
{
    return m_impl->update(container, vdb_file, vdb_bbox, data, changes);
}

void VDBSlicedDisplay::setWorldMatrices(const MMatrixArray& world_matrices)
{
    m_impl->setWorldMatrices(world_matrices);
}

void VDBSlicedDisplay::enable(bool enable)
{
    m_impl->enable(enable);
}
