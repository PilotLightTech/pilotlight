/*
    pl_ext.c
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

#include "pl_image_ext.c"
#include "pl_rect_pack_ext.c"
#include "pl_stats_ext.c"
#include "pl_job_ext.c"

#ifdef PL_CORE_EXTENSION_INCLUDE_SHADER
    #include "pl_shader_ext.c"
#endif

#ifdef PL_CORE_EXTENSION_INCLUDE_GRAPHICS
    #include "pl_draw_ext.c"
    #include "pl_draw_backend_ext.c"
    #include "pl_gpu_allocators_ext.c"
    #include "pl_ui_ext.c"
    #include "pl_graphics_ext.c"
#endif

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    // core apis
    gptApiRegistry        = ptApiRegistry;
    gptDataRegistry      = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    gptExtensionRegistry = ptApiRegistry->first(PL_API_EXTENSION_REGISTRY);
    gptMemory            = ptApiRegistry->first(PL_API_MEMORY);

    // set contexts
    pl_set_profile_context(gptDataRegistry->get_data("profile"));
    pl_set_log_context(gptDataRegistry->get_data("log"));

    // load os apis
    gptIOI     = ptApiRegistry->first(PL_API_IO);
    gptFile    = ptApiRegistry->first(PL_API_FILE);
    gptThreads = ptApiRegistry->first(PL_API_THREADS);
    gptAtomics = ptApiRegistry->first(PL_API_ATOMICS);
    gptIO      = gptIOI->get_io();

    // first batch (standalone APIs)
    pl_load_image_ext(ptApiRegistry, bReload);
    pl_load_rect_pack_ext(ptApiRegistry, bReload);
    pl_load_stats_ext(ptApiRegistry, bReload);
    pl_load_job_ext(ptApiRegistry, bReload);

    #ifdef PL_CORE_EXTENSION_INCLUDE_SHADER
        pl_load_shader_ext(ptApiRegistry, bReload);
        gptShader = ptApiRegistry->first(PL_API_SHADER);
    #endif

    gptStats = ptApiRegistry->first(PL_API_STATS);
    gptImage = ptApiRegistry->first(PL_API_IMAGE);
    gptJob   = ptApiRegistry->first(PL_API_JOB);
    gptRect  = ptApiRegistry->first(PL_API_RECT_PACK);
    
    #ifdef PL_CORE_EXTENSION_INCLUDE_GRAPHICS
        
        pl_load_graphics_ext(ptApiRegistry, bReload);
        gptGfx = ptApiRegistry->first(PL_API_GRAPHICS);

        pl_load_gpu_allocators_ext(ptApiRegistry, bReload);
        gptGpuAllocators = ptApiRegistry->first(PL_API_GPU_ALLOCATORS);
    
        // third batch
        pl_load_draw_ext(ptApiRegistry, bReload);
        gptDraw = ptApiRegistry->first(PL_API_DRAW);
        pl_load_draw_backend_ext(ptApiRegistry, bReload);
        gptDrawBackend = ptApiRegistry->first(PL_API_DRAW_BACKEND);

        // fourth batch
        pl_load_ui_ext(ptApiRegistry, bReload);
        gptUI = ptApiRegistry->first(PL_API_UI);
    #endif
}

PL_EXPORT void
pl_unload_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    #ifdef PL_CORE_EXTENSION_INCLUDE_SHADER
        pl_unload_shader_ext(ptApiRegistry, bReload);
    #endif
    pl_unload_job_ext(ptApiRegistry, bReload);
    pl_unload_image_ext(ptApiRegistry, bReload);
    pl_unload_rect_pack_ext(ptApiRegistry, bReload);
    pl_unload_stats_ext(ptApiRegistry, bReload);
    #ifdef PL_CORE_EXTENSION_INCLUDE_GRAPHICS
        pl_unload_graphics_ext(ptApiRegistry, bReload);
        pl_unload_gpu_allocators_ext(ptApiRegistry, bReload);
        pl_unload_draw_ext(ptApiRegistry, bReload);
        pl_unload_draw_backend_ext(ptApiRegistry, bReload);
        pl_unload_ui_ext(ptApiRegistry, bReload);
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

#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC(x) PL_ALLOC(x)
#define STBI_FREE(x) PL_FREE(x)
#define STBI_REALLOC(x, y) PL_REALLOC(x, y)
#include "stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_MALLOC(x) PL_ALLOC(x)
#define STBIW_FREE(x) PL_FREE(x)
#define STBIW_REALLOC(x, y) PL_REALLOC(x, y)
#include "stb_image_write.h"
#undef STB_IMAGE_WRITE_IMPLEMENTATION

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
#undef STB_RECT_PACK_IMPLEMENTATION

#ifdef PL_USE_STB_SPRINTF
    #define STB_SPRINTF_IMPLEMENTATION
    #include "stb_sprintf.h"
    #undef STB_SPRINTF_IMPLEMENTATION
#endif

#ifdef PL_CORE_EXTENSION_INCLUDE_GRAPHICS
    #define STB_TRUETYPE_IMPLEMENTATION
    #include "stb_truetype.h"
    #undef STB_TRUETYPE_IMPLEMENTATION
#endif