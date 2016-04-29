#pragma once

#include <maya/MPxGeometryOverride.h>

#include "vdb_visualizer.h"

namespace MHWRender{

    class VDBGeometryOverride : public MPxGeometryOverride{
    public:
        VDBGeometryOverride(const MObject& obj);
        ~VDBGeometryOverride();

        static MPxGeometryOverride* creator(const MObject& obj);

        void updateDG();

        void updateRenderItems(
            const MDagPath& path,
            MRenderItemList& list);

        void populateGeometry(
            const MGeometryRequirements& requirements,
            const MHWRender::MRenderItemList& renderItems,
            MGeometry& data);
        void cleanUp();

        static MString registrantId;
    private:
        VDBVisualizerShape* p_vdb_visualizer;
    };
}
