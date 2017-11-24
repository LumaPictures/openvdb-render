#pragma once

#include "vdb_visualizer.h"
#include <maya/MPxCommand.h>
#include <memory>

enum class VDBSlicedDisplayChangeSet : uint32_t {
    NO_CHANGES = 0,
    SHADER_PARAM = 1,
    SLICE_COUNT = 2,
    BOUNDING_BOX = 4,
    DENSITY_RAMP_SAMPLES = 8,
    SCATTER_COLOR_RAMP_SAMPLES = 16,
    EMISSION_RAMP_SAMPLES = 32,
    DENSITY_CHANNEL = 64,
    SCATTER_COLOR_CHANNEL = 128,
    TRANSPARENT_CHANNEL = 256,
    EMISSION_CHANNEL = 512,
    TEMPERATURE_CHANNEL = 1024,

    LAST = TEMPERATURE_CHANNEL,
    ALL = 2 * LAST - 1,
    ALL_CHANNELS = DENSITY_CHANNEL | SCATTER_COLOR_CHANNEL |TRANSPARENT_CHANNEL | EMISSION_CHANNEL | TEMPERATURE_CHANNEL
};

inline VDBSlicedDisplayChangeSet&
operator|=(VDBSlicedDisplayChangeSet& lhs, VDBSlicedDisplayChangeSet rhs)
{
    return lhs = VDBSlicedDisplayChangeSet(uint32_t(lhs) | uint32_t(rhs));
}

constexpr inline VDBSlicedDisplayChangeSet
operator|(VDBSlicedDisplayChangeSet lhs, VDBSlicedDisplayChangeSet rhs)
{
    return VDBSlicedDisplayChangeSet(uint32_t(lhs) | uint32_t(rhs));
}

inline VDBSlicedDisplayChangeSet&
operator&=(VDBSlicedDisplayChangeSet& lhs, VDBSlicedDisplayChangeSet rhs)
{
    return lhs = VDBSlicedDisplayChangeSet(uint32_t(lhs) & uint32_t(rhs));
}

constexpr inline VDBSlicedDisplayChangeSet
operator&(VDBSlicedDisplayChangeSet lhs, VDBSlicedDisplayChangeSet rhs)
{
    return VDBSlicedDisplayChangeSet(uint32_t(lhs) & uint32_t(rhs));
}

constexpr inline bool
hasChange(VDBSlicedDisplayChangeSet change_set, VDBSlicedDisplayChangeSet mask)
{
    return (change_set & mask) != VDBSlicedDisplayChangeSet::NO_CHANGES;
}

class VDBSlicedDisplayImpl;

class VDBSlicedDisplay {
public:
    explicit VDBSlicedDisplay(MHWRender::MPxSubSceneOverride& parent);
    VDBSlicedDisplay(const VDBSlicedDisplay&) = delete;
    VDBSlicedDisplay(VDBSlicedDisplay&&) = delete;
    VDBSlicedDisplay& operator=(const VDBSlicedDisplay&) = delete;
    VDBSlicedDisplay& operator=(VDBSlicedDisplay&&) = delete;
    ~VDBSlicedDisplay();
    bool update(
        MHWRender::MSubSceneContainer& container,
        const openvdb::io::File* vdb_file,
        const MBoundingBox& vdb_bbox,
        const VDBSlicedDisplayData& data,
        VDBSlicedDisplayChangeSet& changes);
    void setWorldMatrices(const MMatrixArray& world_matrices);
    void enable(bool enable);
private:
    std::unique_ptr<VDBSlicedDisplayImpl> m_impl;
};

class VDBVolumeCacheCmd : public MPxCommand {
private:
    VDBVolumeCacheCmd() = default;
public:
    VDBVolumeCacheCmd(const VDBVolumeCacheCmd&) = delete;
    VDBVolumeCacheCmd(VDBVolumeCacheCmd&&) = delete;
    VDBVolumeCacheCmd& operator=(const VDBVolumeCacheCmd&) = delete;
    VDBVolumeCacheCmd& operator=(VDBVolumeCacheCmd&&) = delete;
    ~VDBVolumeCacheCmd() override = default;

    static const char* COMMAND_STRING;

    static void* creator() { return new VDBVolumeCacheCmd(); }
    static MSyntax create_syntax();

    MStatus doIt(const MArgList& args) override;
};
