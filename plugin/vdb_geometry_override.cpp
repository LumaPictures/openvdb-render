#include "vdb_geometry_override.h"
#include "../util/maya_utils.hpp"

#include <maya/MHWGeometry.h>
#include <maya/MShaderManager.h>

#include <openvdb/tools/Interpolation.h>
#include <openvdb/Exceptions.h>

#include <iostream>

namespace{
    class RGBSampler {
    private:
        MFloatVector default_color;
    public:
        RGBSampler(const MFloatVector& dc = MFloatVector(1.0f, 1.0f, 1.0f)) : default_color(dc)
        { }

        virtual ~RGBSampler()
        { }

        virtual MFloatVector get_rgb(const openvdb::Vec3d&) const
        {
            return default_color;
        }
    };

    class FloatToRGBSampler : public RGBSampler {
    private:
        typedef openvdb::tools::GridSampler<openvdb::FloatGrid, openvdb::tools::BoxSampler> sampler_type;
        sampler_type* p_sampler;
    public:
        FloatToRGBSampler(openvdb::FloatGrid::ConstPtr grid)
        {
            p_sampler = new sampler_type(*grid);
        }

        ~FloatToRGBSampler()
        {
            delete p_sampler;
        }

        MFloatVector get_rgb(const openvdb::Vec3d& wpos) const
        {
            const float value = p_sampler->wsSample(wpos);
            return MFloatVector(value, value, value);
        }
    };

    class Vec3SToRGBSampler : public RGBSampler {
    private:
        typedef openvdb::tools::GridSampler<openvdb::Vec3SGrid, openvdb::tools::BoxSampler> sampler_type;
        sampler_type* p_sampler;
    public:
        Vec3SToRGBSampler(openvdb::Vec3SGrid::ConstPtr grid)
        {
            p_sampler = new sampler_type(*grid);
        }

        ~Vec3SToRGBSampler()
        {
            delete p_sampler;
        }

        MFloatVector get_rgb(const openvdb::Vec3d& wpos) const
        {
            const openvdb::Vec3s value = p_sampler->wsSample(wpos);
            return MFloatVector(value.x(), value.y(), value.z());
        }
    };

    class FloatVoxelIterator {
    protected:
        size_t m_active_voxel_count;
    public:
        FloatVoxelIterator() : m_active_voxel_count(1)
        { }

        virtual ~FloatVoxelIterator()
        { }

        virtual bool is_valid() const
        {
            return false;
        }

        virtual void get_next()
        { }

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
        { }

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
        { }

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
}

namespace MHWRender{
    //const int point_cloud_draw_mode = MHWRender::MGeometry::kShaded | MHWRender::MGeometry::kTextured | MHWRender::MGeometry::kWireframe;

    struct VDBGeometryOverrideData {
        MBoundingBox bbox;

        MFloatVector scattering_color;
        MFloatVector attenuation_color;
        MFloatVector emission_color;

        std::string attenuation_channel;
        std::string scattering_channel;
        std::string emission_channel;

        Gradient scattering_gradient;
        Gradient attenuation_gradient;
        Gradient emission_gradient;

        float point_size;
        float point_jitter;

        int point_skip;
        VDBDisplayMode display_mode;

        std::unique_ptr<openvdb::io::File> vdb_file;
        openvdb::GridBase::ConstPtr scattering_grid;
        openvdb::GridBase::ConstPtr attenuation_grid;
        openvdb::GridBase::ConstPtr emission_grid;

        void clear();
    };

    void VDBGeometryOverrideData::clear()
    {
        scattering_grid = 0;
        attenuation_grid = 0;
        emission_grid = 0;
        vdb_file.reset();
    }

    VDBGeometryOverride::VDBGeometryOverride(const MObject& obj) : MPxGeometryOverride(obj), p_data(new VDBGeometryOverrideData)
    {
        MFnDependencyNode dnode(obj);
        p_vdb_visualizer = dynamic_cast<VDBVisualizerShape*>(dnode.userNode());
    }

    VDBGeometryOverride::~VDBGeometryOverride()
    {

    }

    MPxGeometryOverride* VDBGeometryOverride::creator(const MObject& obj)
    {
        return new VDBGeometryOverride(obj);
    }

    void VDBGeometryOverride::updateDG()
    {
        VDBVisualizerData* vis_data = p_vdb_visualizer->get_update();

        if (vis_data == 0)
            return;

        p_data->display_mode = vis_data->display_mode;
        p_data->bbox = vis_data->bbox;

        if (vis_data->vdb_file == nullptr || !vis_data->vdb_file->isOpen())
        {
            p_data->clear();
            return;
        }

        const std::string filename = vis_data->vdb_file->filename();

        if (p_data->vdb_file != nullptr && filename != p_data->vdb_file->filename())
            p_data->clear();

        try{
            p_data->vdb_file.reset(new openvdb::io::File(filename));
        }
        catch(...){
            p_data->vdb_file.reset();
        }

        p_data->scattering_color = vis_data->scattering_color;
        p_data->attenuation_color = vis_data->attenuation_color;
        p_data->emission_color = vis_data->emission_color;

        p_data->attenuation_channel = vis_data->attenuation_channel;
        p_data->scattering_channel = vis_data->scattering_channel;
        p_data->emission_channel = vis_data->emission_channel;

        p_data->scattering_gradient = vis_data->scattering_gradient;
        p_data->attenuation_gradient = vis_data->attenuation_gradient;
        p_data->emission_gradient = vis_data->emission_gradient;

        p_data->point_size = vis_data->point_size;
        p_data->point_jitter = vis_data->point_jitter;

        p_data->point_skip = vis_data->point_skip;
    }

    void VDBGeometryOverride::updateRenderItems(const MDagPath&, MRenderItemList& list)
    {
        list.clear();

        MHWRender::MRenderer* renderer = MHWRender::MRenderer::theRenderer();
        if (renderer == nullptr)
            return;

        const MHWRender::MShaderManager* shader_manager = renderer->getShaderManager();
        if (shader_manager == nullptr)
            return;

        const bool file_exists = p_data->vdb_file != nullptr;

        const VDBDisplayMode display_mode = p_data->display_mode;

        if (!file_exists || display_mode <= DISPLAY_GRID_BBOX)
        {
            MHWRender::MRenderItem* bounding_box = MHWRender::MRenderItem::Create("bounding_box",
                                                                                  MHWRender::MGeometry::kLines,
                                                                                  MHWRender::MGeometry::kAll,
                                                                                  false);

            list.append(bounding_box);

            MHWRender::MShaderInstance* shader = shader_manager->getStockShader(
                    MHWRender::MShaderManager::k3dSolidShader, nullptr, nullptr);
            if (shader)
            {
                // Set the color on the shader instance using the parameter interface
                static const float color[] = {0.0f, 1.0f, 0.0f, 1.0f};
                shader->setParameter("solidColor", color);

                // Assign the shader to the custom render item
                bounding_box->setShader(shader);
            }
        }
        else
        {
            if (display_mode == DISPLAY_POINT_CLOUD)
            {
                MHWRender::MRenderItem* point_cloud = MHWRender::MRenderItem::Create("point_cloud",
                                                                                     MHWRender::MGeometry::kPoints,
                                                                                     MHWRender::MGeometry::kAll,
                                                                                     false);
                list.append(point_cloud);

                MHWRender::MShaderInstance* shader = shader_manager->getStockShader(
                        MHWRender::MShaderManager::k3dFatPointShader, nullptr, nullptr);
                if (shader)
                    point_cloud->setShader(shader);
            }
        }
    }

    void VDBGeometryOverride::populateGeometry(const MGeometryRequirements& reqs,
                                               const MHWRender::MRenderItemList& list, MGeometry& geo)
    {
        auto set_bbox_indices = [&](const unsigned int num_bboxes){
            const int index = list.indexOf("bounding_box");
            if (index >= 0)
            {
                const MRenderItem* item = list.itemAt(index);
                if (item != nullptr)
                {
                    MIndexBuffer* index_buffer = geo.createIndexBuffer(MGeometry::kUnsignedInt32);
                    unsigned int* indices = reinterpret_cast<unsigned int*>(index_buffer->acquire(24 * num_bboxes, true));
                    unsigned int id = 0;
                    for (unsigned int bbox = 0; bbox < num_bboxes; ++bbox)
                    {
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
                    index_buffer->commit(indices);
                    item->associateWithIndexBuffer(index_buffer);
                }
            }
        };

        auto iterate_reqs = [&] (std::function<void(MVertexBufferDescriptor&)> func) {
            const MVertexBufferDescriptorList& desc_list = reqs.vertexRequirements();
            const int num_desc = desc_list.length();
            for (int i = 0; i < num_desc; ++i)
            {
                MVertexBufferDescriptor desc;
                if (!desc_list.getDescriptor(i, desc))
                    continue;
                func(desc);
            }
        };

        const VDBDisplayMode display_mode = p_data->display_mode;

        if (display_mode == DISPLAY_AXIS_ALIGNED_BBOX || p_data->vdb_file == nullptr)
            iterate_reqs(
                [&](MVertexBufferDescriptor& desc) {
                    if (desc.semantic() != MGeometry::kPosition)
                        return;
                    MVertexBuffer* vertex_buffer = geo.createVertexBuffer(desc);
                    MFloatVector* bbox_vertices = reinterpret_cast<MFloatVector*>(vertex_buffer->acquire(8));
                    MFloatVector min = p_data->bbox.min();
                    MFloatVector max = p_data->bbox.max();
                    bbox_vertices[0] = MFloatVector(min.x, min.y, min.z);
                    bbox_vertices[1] = MFloatVector(min.x, max.y, min.z);
                    bbox_vertices[2] = MFloatVector(min.x, max.y, max.z);
                    bbox_vertices[3] = MFloatVector(min.x, min.y, max.z);
                    bbox_vertices[4] = MFloatVector(max.x, min.y, min.z);
                    bbox_vertices[5] = MFloatVector(max.x, max.y, min.z);
                    bbox_vertices[6] = MFloatVector(max.x, max.y, max.z);
                    bbox_vertices[7] = MFloatVector(max.x, min.y, max.z);
                    vertex_buffer->commit(bbox_vertices);
                    set_bbox_indices(1);
                }
            );
        else if (display_mode == DISPLAY_GRID_BBOX)
            iterate_reqs(
                [&](MVertexBufferDescriptor& desc) {
                    if (desc.semantic() != MGeometry::kPosition)
                        return;
                    try{
                        if (!p_data->vdb_file->isOpen())
                            p_data->vdb_file->open(false);
                        openvdb::GridPtrVecPtr grids = p_data->vdb_file->readAllGridMetadata();
                        if (grids->size() == 0)
                            return;
                        std::vector<MFloatVector> vertices;
                        vertices.reserve(grids->size() * 8);
                        auto push_back_wireframe = [&] (openvdb::GridBase::ConstPtr grid) {
                            std::array<MFloatVector, 8> _vertices;
                            if (read_grid_transformed_bbox_wire(grid, _vertices))
                            {
                                for (int v = 0; v < 8; ++v)
                                    vertices.push_back(_vertices[v]);
                            }
                        };

                        for (openvdb::GridPtrVec::const_iterator it = grids->begin(); it != grids->end(); ++it)
                        {
                            if (openvdb::GridBase::ConstPtr grid = *it)
                                push_back_wireframe(grid);
                        }

                        const unsigned int vertex_count = static_cast<unsigned int>(vertices.size());

                        if (vertex_count > 0)
                        {
                            MVertexBuffer* vertex_buffer = geo.createVertexBuffer(desc);
                            MFloatVector* bbox_vertices = reinterpret_cast<MFloatVector*>(vertex_buffer->acquire(vertex_count));
                            for (unsigned int i = 0; i < vertex_count; ++i)
                                bbox_vertices[i] = vertices[i];
                            vertex_buffer->commit(bbox_vertices);
                            set_bbox_indices(vertex_count / 8);
                        }
                    }
                    catch(...)
                    {
                    }
                }
            );
        else if (display_mode == DISPLAY_POINT_CLOUD)
        {
            const int index = list.indexOf("point_cloud");
            if (index < 0)
                return;

            try{
                if (!p_data->vdb_file->isOpen())
                    p_data->vdb_file->open(false);
                p_data->attenuation_grid = p_data->vdb_file->readGrid(p_data->attenuation_channel);
            }
            catch(...) {
                p_data->attenuation_grid = nullptr;
                p_data->scattering_grid = nullptr;
                p_data->emission_grid = nullptr;
                return;
            }

            const float point_jitter = p_data->point_jitter;
            const float do_jitter = point_jitter > 0.001f;

            const openvdb::Vec3d voxel_size = p_data->attenuation_grid->voxelSize();

            std::minstd_rand generator; // LCG
            std::uniform_real_distribution<float> distributionX(-point_jitter * static_cast<float>(voxel_size.x()), point_jitter * static_cast<float>(voxel_size.x()));
            std::uniform_real_distribution<float> distributionY(-point_jitter * static_cast<float>(voxel_size.y()), point_jitter * static_cast<float>(voxel_size.y()));
            std::uniform_real_distribution<float> distributionZ(-point_jitter * static_cast<float>(voxel_size.z()), point_jitter * static_cast<float>(voxel_size.z()));

            FloatVoxelIterator* iter = nullptr;

            if (p_data->attenuation_grid->valueType() == "float")
                iter = new FloatToFloatVoxelIterator(openvdb::gridConstPtrCast<openvdb::FloatGrid>(p_data->attenuation_grid));
            else if (p_data->attenuation_grid->valueType() == "vec3s")
                iter = new Vec3SToFloatVoxelIterator(openvdb::gridConstPtrCast<openvdb::Vec3SGrid>(p_data->attenuation_grid));
            else
                iter = new FloatVoxelIterator();

            std::vector<MFloatVector> vertices;
            vertices.reserve(iter->get_active_voxels());
            const openvdb::math::Transform attenuation_transform = p_data->attenuation_grid->transform();

            const int point_skip = p_data->point_skip;

            int point_id = 0;
            for (; iter->is_valid(); iter->get_next())
            {
                if ((point_id++ % point_skip) != 0)
                    continue;
                openvdb::Vec3d vdb_pos = attenuation_transform.indexToWorld(iter->get_coord());
                MFloatVector pos(static_cast<float>(vdb_pos.x()), static_cast<float>(vdb_pos.y()),
                                 static_cast<float>(vdb_pos.z()));
                if (do_jitter)
                {
                    pos.x += distributionX(generator);
                    pos.y += distributionY(generator);
                    pos.z += distributionZ(generator);
                }
                vertices.push_back(pos);
            }

            vertices.shrink_to_fit();
            const unsigned int vertex_count = static_cast<unsigned int>(vertices.size());

            if (vertex_count == 0)
                return;
            
            const MRenderItem* item = list.itemAt(index);
            MIndexBuffer* index_buffer = geo.createIndexBuffer(MGeometry::kUnsignedInt32);
            unsigned int* indices = reinterpret_cast<unsigned int*>(index_buffer->acquire(vertex_count, true));
            for (unsigned int i = 0; i < vertex_count; ++i)
                indices[i] = i;
            index_buffer->commit(indices);
            item->associateWithIndexBuffer(index_buffer);

            iterate_reqs(
                [&](MVertexBufferDescriptor& desc) {
                    switch (desc.semantic())
                    {
                    case MGeometry::kPosition:
                    {
                        MVertexBuffer* vertex_buffer = geo.createVertexBuffer(desc);
                        MFloatVector* pc_vertices = reinterpret_cast<MFloatVector*>(vertex_buffer->acquire(static_cast<unsigned int>(vertices.size()), true));
                        for (size_t i = 0; i < vertices.size(); ++i)
                            pc_vertices[i] = vertices[i];
                        vertex_buffer->commit(pc_vertices);
                    }
                        break;
                    case MGeometry::kNormal:
                        std::cerr << "[openvdb_render] Normal semantic found" << std::endl;
                        break;
                    case MGeometry::kTexture:
                        std::cerr << "[openvdb_render] Texture semantic found" << std::endl;
                        break;
                    case MGeometry::kColor:
                        std::cerr << "[openvdb_render] Color semantic found" << std::endl;
                        break;
                    default:
                        std::cerr << "[openvdb_render] Unknown semantic found" << std::endl;
                    }
                }
            );
        }
    }

    void VDBGeometryOverride::cleanUp()
    {
    }

    MString VDBGeometryOverride::registrantId("VDBVisualizerDrawOverride");
}
