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
    gptDataRegistry      = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    gptIOI               = ptApiRegistry->first(PL_API_IO);
    gptImage             = ptApiRegistry->first(PL_API_IMAGE);
    gptExtensionRegistry = ptApiRegistry->first(PL_API_EXTENSION_REGISTRY);
    gptMemory            = ptApiRegistry->first(PL_API_MEMORY);
    gptGpuAllocators     = ptApiRegistry->first(PL_API_GPU_ALLOCATORS);
    gptFile              = ptApiRegistry->first(PL_API_FILE);
    gptThreads           = ptApiRegistry->first(PL_API_THREADS);
    gptAtomics           = ptApiRegistry->first(PL_API_ATOMICS);
    gptIO                = gptIOI->get_io();
    gptStats             = ptApiRegistry->first(PL_API_STATS);
    gptImage             = ptApiRegistry->first(PL_API_IMAGE);
    gptJob               = ptApiRegistry->first(PL_API_JOB);

    // set contexts
    pl_set_profile_context(gptDataRegistry->get_data("profile"));
    pl_set_log_context(gptDataRegistry->get_data("log"));

    pl_load_ecs_ext(ptApiRegistry, bReload);
    gptECS    = ptApiRegistry->first(PL_API_ECS);
    gptCamera = ptApiRegistry->first(PL_API_CAMERA);

    #ifdef PL_CORE_EXTENSION_INCLUDE_SHADER
        gptShader = ptApiRegistry->first(PL_API_SHADER);
    #endif

    #ifdef PL_CORE_EXTENSION_INCLUDE_GRAPHICS
        gptDraw        = ptApiRegistry->first(PL_API_DRAW);
        gptDrawBackend = ptApiRegistry->first(PL_API_DRAW_BACKEND);
        gptGfx         = ptApiRegistry->first(PL_API_GRAPHICS);
        gptUI          = ptApiRegistry->first(PL_API_UI);
        pl_load_resource_ext(ptApiRegistry, bReload);
        gptResource = ptApiRegistry->first(PL_API_RESOURCE);        
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