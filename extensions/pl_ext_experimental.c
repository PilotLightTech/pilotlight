/*
    pl_ext_experimental.c
      * extensions with unstable APIs
      * unity build for activated extensions
      * absolute mess and needs to be cleaned up
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

#ifdef PL_CORE_EXTENSION_INCLUDE_GRAPHICS
    #include "pl_resource_ext.c"
    #include "pl_model_loader_ext.c"
    #include "pl_renderer_ext.c"
    #include "pl_debug_ext.c"
#endif

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    // core apis
    gptApiRegistry       = ptApiRegistry;
    gptDataRegistry      = pl_get_api(ptApiRegistry, plDataRegistryI);
    gptIOI               = pl_get_api(ptApiRegistry, plIOI);
    gptImage             = pl_get_api(ptApiRegistry, plImageI);
    gptExtensionRegistry = pl_get_api(ptApiRegistry, plExtensionRegistryI);
    gptMemory            = pl_get_api(ptApiRegistry, plMemoryI);
    gptGpuAllocators     = pl_get_api(ptApiRegistry, plGPUAllocatorsI);
    gptFile              = pl_get_api(ptApiRegistry, plFileI);
    gptThreads           = pl_get_api(ptApiRegistry, plThreadsI);
    gptAtomics           = pl_get_api(ptApiRegistry, plAtomicsI);
    gptIO                = gptIOI->get_io();
    gptStats             = pl_get_api(ptApiRegistry, plStatsI);
    gptImage             = pl_get_api(ptApiRegistry, plImageI);
    gptJob               = pl_get_api(ptApiRegistry, plJobI);

    // set contexts
    pl_set_profile_context(gptDataRegistry->get_data("profile"));
    pl_set_log_context(gptDataRegistry->get_data("log"));

    pl_load_ecs_ext(ptApiRegistry, bReload);
    gptECS    = pl_get_api(ptApiRegistry, plEcsI);
    gptCamera = pl_get_api(ptApiRegistry, plCameraI);

    #ifdef PL_CORE_EXTENSION_INCLUDE_SHADER
        gptShader = pl_get_api(ptApiRegistry, plShaderI);
    #endif

    #ifdef PL_CORE_EXTENSION_INCLUDE_GRAPHICS
        gptDraw        = pl_get_api(ptApiRegistry, plDrawI);
        gptDrawBackend = pl_get_api(ptApiRegistry, plDrawBackendI);
        gptGfx         = pl_get_api(ptApiRegistry, plGraphicsI);
        gptUI          = pl_get_api(ptApiRegistry, plUiI);
        pl_load_resource_ext(ptApiRegistry, bReload);
        gptResource = pl_get_api(ptApiRegistry, plResourceI);        
        pl_load_model_loader_ext(ptApiRegistry, bReload);
        pl_load_renderer_ext(ptApiRegistry, bReload);
        pl_load_debug_ext(ptApiRegistry, bReload);
    #endif
}

PL_EXPORT void
pl_unload_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    pl_unload_ecs_ext(ptApiRegistry, bReload);
    #ifdef PL_CORE_EXTENSION_INCLUDE_GRAPHICS
        pl_unload_resource_ext(ptApiRegistry, bReload);
        pl_unload_model_loader_ext(ptApiRegistry, bReload);
        pl_unload_renderer_ext(ptApiRegistry, bReload);
        pl_unload_debug_ext(ptApiRegistry, bReload);
    #endif
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

#ifdef PL_CORE_EXTENSION_INCLUDE_GRAPHICS
    #define PL_STL_IMPLEMENTATION
    #include "pl_stl.h"
    #undef PL_STL_IMPLEMENTATION

    #define CGLTF_IMPLEMENTATION
    #include "cgltf.h"
    #undef CGLTF_IMPLEMENTATION
#endif