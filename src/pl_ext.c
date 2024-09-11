/*
    pl_ext.c
      * unity build for activated extensions
      * absolute mess and needs to be cleaned up
*/

#define PL_CORE_EXTENSION_VERSION    "0.1.0"
#define PL_CORE_EXTENSION_VERSION_NUM 001000

#include "pl_image_ext.c"
#include "pl_rect_pack_ext.c"
#include "pl_stats_ext.c"
#include "pl_job_ext.c"
#include "pl_ecs_ext.c"

#ifdef PL_CORE_EXTENSION_INCLUDE_SHADER
    #include "pl_shader_ext.c"
#endif

#ifdef PL_CORE_EXTENSION_INCLUDE_GRAPHICS
    #include "pl_draw_ext.c"
    #include "pl_resource_ext.c"
    #include "pl_gpu_allocators_ext.c"
    #include "pl_model_loader_ext.c"
    #include "pl_ui_ext.c"
    #include "pl_graphics_ext.c"

    #include "pl_renderer_ext.c"
    #include "pl_debug_ext.c"
#endif

PL_EXPORT void
pl_load_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    gptApiRegistry = ptApiRegistry;
    gptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    gptExtensionRegistry = ptApiRegistry->first(PL_API_EXTENSION_REGISTRY);

    // set contexts
    pl_set_memory_context(gptDataRegistry->get_data(PL_CONTEXT_MEMORY));
    pl_set_profile_context(gptDataRegistry->get_data("profile"));
    pl_set_log_context(gptDataRegistry->get_data("log"));

    // load os APIs
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

    gptStats  = ptApiRegistry->first(PL_API_STATS);
    gptImage  = ptApiRegistry->first(PL_API_IMAGE);
    gptJob    = ptApiRegistry->first(PL_API_JOB);
    gptRect   = ptApiRegistry->first(PL_API_RECT_PACK);
    
    // second batch (dependent APIs)
    pl_load_ecs_ext(ptApiRegistry, bReload);
    gptECS    = ptApiRegistry->first(PL_API_ECS);
    gptCamera = ptApiRegistry->first(PL_API_CAMERA);

    #ifdef PL_CORE_EXTENSION_INCLUDE_GRAPHICS
        
        pl_load_graphics_ext(ptApiRegistry, bReload);

        gptGfx    = ptApiRegistry->first(PL_API_GRAPHICS);

        pl_load_gpu_allocators_ext(ptApiRegistry, bReload);
        pl_load_resource_ext(ptApiRegistry, bReload);

        gptResource      = ptApiRegistry->first(PL_API_RESOURCE);        
        gptGpuAllocators = ptApiRegistry->first(PL_API_GPU_ALLOCATORS);
    
        // third batch
        pl_load_model_loader_ext(ptApiRegistry, bReload);
        pl_load_draw_ext(ptApiRegistry, bReload);

        gptDraw = ptApiRegistry->first(PL_API_DRAW);

        // fourth batch
        pl_load_ui_ext(ptApiRegistry, bReload);
        gptUI = ptApiRegistry->first(PL_API_UI);

        // final batch
        pl_load_renderer_ext(ptApiRegistry, bReload);
        pl_load_debug_ext(ptApiRegistry, bReload);
    #endif
}

PL_EXPORT void
pl_unload_ext(plApiRegistryI* ptApiRegistry)
{
    #ifdef PL_CORE_EXTENSION_INCLUDE_SHADER
        pl_unload_shader_ext(ptApiRegistry);
    #endif
    pl_unload_image_ext(ptApiRegistry);
    pl_unload_rect_pack_ext(ptApiRegistry);
    pl_unload_stats_ext(ptApiRegistry);
    pl_unload_job_ext(ptApiRegistry);
    pl_unload_ecs_ext(ptApiRegistry);
    #ifdef PL_CORE_EXTENSION_INCLUDE_GRAPHICS
        pl_unload_graphics_ext(ptApiRegistry);
        pl_unload_gpu_allocators_ext(ptApiRegistry);
        pl_unload_resource_ext(ptApiRegistry);
        pl_unload_model_loader_ext(ptApiRegistry);
        pl_unload_draw_ext(ptApiRegistry);
        pl_unload_ui_ext(ptApiRegistry);
        pl_unload_renderer_ext(ptApiRegistry);
        pl_unload_debug_ext(ptApiRegistry);
    #endif
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

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

#ifdef PL_CORE_EXTENSION_INCLUDE_GRAPHICS
    #define STB_TRUETYPE_IMPLEMENTATION
    #include "stb_truetype.h"
    #undef STB_TRUETYPE_IMPLEMENTATION

    #define PL_STL_IMPLEMENTATION
    #include "pl_stl.h"
    #undef PL_STL_IMPLEMENTATION

    #define CGLTF_IMPLEMENTATION
    #include "cgltf.h"
    #undef CGLTF_IMPLEMENTATION
#endif