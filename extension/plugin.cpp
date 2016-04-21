#include <extension/Extension.h>
#include <extension/ExtensionsManager.h>

#include "openvdb_translator.h"
#include "sampler_translator.h"
#include "shader_translator.h"

extern "C"
{
    DLLEXPORT void initializeExtension(CExtension& extension)
    {
        extension.Requires("openvdb_render");

        extension.RegisterTranslator("vdb_visualizer",
                                     "",
                                     OpenvdbTranslator::creator);

        extension.RegisterTranslator("vdb_sampler",
                                     "",
                                     SamplerTranslator::creator);

        extension.RegisterTranslator("vdb_shader",
                                     "",
                                     VDBShaderTranslator::creator);
    }

    DLLEXPORT void deinitializeExtension(CExtension&)
    {
    }
}
