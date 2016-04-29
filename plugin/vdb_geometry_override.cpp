#include "vdb_geometry_override.h"
#include "../util/maya_utils.hpp"

#include <maya/MHWGeometry.h>
#include <maya/MShaderManager.h>

#include <iostream>

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
        const MVertexBufferDescriptorList& desc_list = reqs.vertexRequirements();
        const int num_desc = desc_list.length();
        for (int i = 0; i < num_desc; ++i)
        {
            MVertexBufferDescriptor desc;
            if (!desc_list.getDescriptor(i, desc))
                continue;
            if (desc.semantic() == MGeometry::kPosition)
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

                const VDBDisplayMode display_mode = p_data->display_mode;

                if (display_mode == DISPLAY_AXIS_ALIGNED_BBOX || p_data->vdb_file == nullptr)
                {
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
                else if (display_mode == DISPLAY_GRID_BBOX)
                {

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

                        if (vertices.size() > 0)
                        {
                            std::cerr << "SEtting bbox indices" << std::endl;
                            set_bbox_indices(static_cast<unsigned int>(vertices.size() / 8));
                        }
                        std::cerr << "Finished generating per grid bbox!" << std::endl;
                    }
                    catch(...)
                    {
                        std::cerr << "Exception caught!" << std::endl;
                    }
                }
                else if (display_mode == DISPLAY_POINT_CLOUD)
                {

                }
            }
        }
    }

    void VDBGeometryOverride::cleanUp()
    {
    }

    MString VDBGeometryOverride::registrantId("VDBVisualizerDrawOverride");
}
