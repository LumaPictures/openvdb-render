// Copyright 2019 Luma Pictures
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <extension/Extension.h>
#if MTOA12
#include <extension/ExtensionsManager.h>
#elif MTOA14
#include <extension/Extension.h>
#endif


#include "openvdb_translator.h"
#include "sampler_translator.h"
#include "simple_shader_translator.h"

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

    extension.RegisterTranslator("vdb_simple_shader",
                                 "",
                                 VDBSimpleShaderTranslator::creator);
}

DLLEXPORT void deinitializeExtension(CExtension&)
{
}
}
