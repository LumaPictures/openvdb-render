#include "vdb_geometry_override.h"

#include <iostream>

namespace MHWRender{

    VDBGeometryOverride::VDBGeometryOverride(const MObject& obj) : MPxGeometryOverride(obj)
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
        p_vdb_visualizer->get_update();
        static int i = 0;
        std::cerr << "Update DG " << i++ << std::endl;
    }

    void VDBGeometryOverride::updateRenderItems(const MDagPath&, MRenderItemList&)
    {
    }

    void VDBGeometryOverride::populateGeometry(const MGeometryRequirements&,
                                               const MHWRender::MRenderItemList&, MGeometry&)
    {
    }

    void VDBGeometryOverride::cleanUp()
    {
    }

    MString VDBGeometryOverride::registrantId("VDBVisualizerDrawOverride");
}
