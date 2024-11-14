/*
    pl_ext.c
      * unity build for stable extensions
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
#include "pl_string_intern_ext.c"

#ifdef PL_CORE_EXTENSION_INCLUDE_SHADER
    #include "pl_shader_ext.c"
#endif

#include "pl_draw_ext.c"
#include "pl_draw_backend_ext.c"
#include "pl_gpu_allocators_ext.c"
#include "pl_ui_ext.c"
#include "pl_graphics_ext.c"

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    gptApiRegistry       = ptApiRegistry;
    gptDataRegistry      = pl_get_api_latest(ptApiRegistry, plDataRegistryI);
    gptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);
    gptMemory            = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptString            = pl_get_api_latest(ptApiRegistry, plStringInternI);
    gptIOI               = pl_get_api_latest(ptApiRegistry, plIOI);
    gptFile              = pl_get_api_latest(ptApiRegistry, plFileI);
    gptThreads           = pl_get_api_latest(ptApiRegistry, plThreadsI);
    gptAtomics           = pl_get_api_latest(ptApiRegistry, plAtomicsI);
    gptStats             = pl_get_api_latest(ptApiRegistry, plStatsI);
    gptImage             = pl_get_api_latest(ptApiRegistry, plImageI);
    gptJob               = pl_get_api_latest(ptApiRegistry, plJobI);
    gptRect              = pl_get_api_latest(ptApiRegistry, plRectPackI);
    gptGfx               = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptGpuAllocators     = pl_get_api_latest(ptApiRegistry, plGPUAllocatorsI);
    gptDraw              = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptDrawBackend       = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
    gptUI                = pl_get_api_latest(ptApiRegistry, plUiI);

    // misc
    pl_set_profile_context(gptDataRegistry->get_data("profile"));
    pl_set_log_context(gptDataRegistry->get_data("log"));

    pl_load_image_ext(ptApiRegistry, bReload);
    pl_load_rect_pack_ext(ptApiRegistry, bReload);
    pl_load_stats_ext(ptApiRegistry, bReload);
    pl_load_job_ext(ptApiRegistry, bReload);
    pl_load_string_intern_ext(ptApiRegistry, bReload);
    pl_load_graphics_ext(ptApiRegistry, bReload);
    pl_load_gpu_allocators_ext(ptApiRegistry, bReload);
    pl_load_draw_ext(ptApiRegistry, bReload);
    pl_load_draw_backend_ext(ptApiRegistry, bReload);
    pl_load_ui_ext(ptApiRegistry, bReload);

    #ifdef PL_CORE_EXTENSION_INCLUDE_SHADER
        pl_load_shader_ext(ptApiRegistry, bReload);
        gptShader = pl_get_api_latest(ptApiRegistry, plShaderI);
    #endif


    gptIO = gptIOI->get_io();
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
    pl_unload_string_intern_ext(ptApiRegistry, bReload);
    pl_unload_graphics_ext(ptApiRegistry, bReload);
    pl_unload_gpu_allocators_ext(ptApiRegistry, bReload);
    pl_unload_draw_ext(ptApiRegistry, bReload);
    pl_unload_draw_backend_ext(ptApiRegistry, bReload);
    pl_unload_ui_ext(ptApiRegistry, bReload);
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

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#undef STB_TRUETYPE_IMPLEMENTATION