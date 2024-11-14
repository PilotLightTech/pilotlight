/*
    pl_ext_experimental.c
      * unity build for unstable extensions
*/

/*
Index of this file:
// [SECTION] unity build #1
// [SECTION] extension loading
// [SECTION] unity build #2
*/

//-----------------------------------------------------------------------------
// [SECTION] unity build #1
//-----------------------------------------------------------------------------

#include "pl_ecs_ext.c"
#include "pl_string_intern_ext.c"
#include "pl_threads_ext.h"
#include "pl_atomics_ext.h"
#include "pl_network_ext.h"
#include "pl_resource_ext.c"
#include "pl_model_loader_ext.c"
#include "pl_renderer_ext.c"
#include "pl_debug_ext.c"

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    // core apis
    gptApiRegistry       = ptApiRegistry;
    gptDataRegistry      = pl_get_api_latest(ptApiRegistry, plDataRegistryI);
    gptIOI               = pl_get_api_latest(ptApiRegistry, plIOI);
    gptImage             = pl_get_api_latest(ptApiRegistry, plImageI);
    gptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);
    gptMemory            = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptGpuAllocators     = pl_get_api_latest(ptApiRegistry, plGPUAllocatorsI);
    gptFile              = pl_get_api_latest(ptApiRegistry, plFileI);
    gptThreads           = pl_get_api_latest(ptApiRegistry, plThreadsI);
    gptAtomics           = pl_get_api_latest(ptApiRegistry, plAtomicsI);
    gptIO                = gptIOI->get_io();
    gptStats             = pl_get_api_latest(ptApiRegistry, plStatsI);
    gptImage             = pl_get_api_latest(ptApiRegistry, plImageI);
    gptJob               = pl_get_api_latest(ptApiRegistry, plJobI);
    gptString            = pl_get_api_latest(ptApiRegistry, plStringInternI);

    // set contexts
    pl_set_profile_context(gptDataRegistry->get_data("profile"));
    pl_set_log_context(gptDataRegistry->get_data("log"));

    
    gptECS         = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptCamera      = pl_get_api_latest(ptApiRegistry, plCameraI);
    gptDraw        = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptDrawBackend = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
    gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptUI          = pl_get_api_latest(ptApiRegistry, plUiI);
    gptResource    = pl_get_api_latest(ptApiRegistry, plResourceI);
    #ifdef PL_CORE_EXTENSION_INCLUDE_SHADER
        gptShader = pl_get_api_latest(ptApiRegistry, plShaderI);
    #endif

    pl_load_ecs_ext(ptApiRegistry, bReload);
    pl_load_resource_ext(ptApiRegistry, bReload);
    pl_load_model_loader_ext(ptApiRegistry, bReload);
    pl_load_renderer_ext(ptApiRegistry, bReload);
    pl_load_debug_ext(ptApiRegistry, bReload);
}

PL_EXPORT void
pl_unload_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    pl_unload_ecs_ext(ptApiRegistry, bReload);
    pl_unload_resource_ext(ptApiRegistry, bReload);
    pl_unload_model_loader_ext(ptApiRegistry, bReload);
    pl_unload_renderer_ext(ptApiRegistry, bReload);
    pl_unload_debug_ext(ptApiRegistry, bReload);
}

//-----------------------------------------------------------------------------
// [SECTION] unity build #2
//-----------------------------------------------------------------------------

#define PL_LOG_IMPLEMENTATION
#include "pl_log.h"
#undef PL_LOG_IMPLEMENTATION

#define PL_PROFILE_IMPLEMENTATION
#include "pl_profile.h"
#undef PL_PROFILE_IMPLEMENTATION

#define PL_STRING_IMPLEMENTATION
#include "pl_string.h"
#undef PL_STRING_IMPLEMENTATION

#define PL_MEMORY_IMPLEMENTATION
#include "pl_memory.h"
#undef PL_MEMORY_IMPLEMENTATION

#ifdef PL_USE_STB_SPRINTF
    #define STB_SPRINTF_IMPLEMENTATION
    #include "stb_sprintf.h"
    #undef STB_SPRINTF_IMPLEMENTATION
#endif

#define PL_STL_IMPLEMENTATION
#include "pl_stl.h"
#undef PL_STL_IMPLEMENTATION

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#undef CGLTF_IMPLEMENTATION