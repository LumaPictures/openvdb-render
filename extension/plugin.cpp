#include <extension/Extension.h>
#include <extension/ExtensionsManager.h>

#include <maya/MFnPlugin.h>

#include "openvdb_translator.h"

extern "C"
{
    DLLEXPORT void initializeExtension(CExtension& extension)
    {
        extension.Requires("openvdb_render");

        extension.RegisterTranslator("vdb_visualizer",
                                     "",
                                     OpenvdbTranslator::creator);
    }

    DLLEXPORT void deinitializeExtension(CExtension&)
    {
    }
}
