/*
    pl_unity_ext.c
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

#include "pl.h"

#include "pl_shader_ext.c"
#include "pl_image_ext.c"
#include "pl_rect_pack_ext.c"
#include "pl_stats_ext.c"
#include "pl_job_ext.c"
#include "pl_string_intern_ext.c"
#include "pl_draw_ext.c"
#include "pl_draw_backend_ext.c"
#include "pl_gpu_allocators_ext.c"
#include "pl_ui_ext.c"
#include "pl_graphics_ext.c"
#include "pl_ecs_ext.c"
#include "pl_atomics_ext.h"
#include "pl_network_ext.h"
#include "pl_resource_ext.c"
#include "pl_model_loader_ext.c"
#include "pl_renderer_ext.c"
#include "pl_debug_ext.c"
#include "pl_profile_ext.c"
#include "pl_log_ext.c"
#include "pl_ecs_tools_ext.c"
#include "pl_gizmo_ext.c"
#include "pl_console_ext.c"
#include "pl_screen_log_ext.c"

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
    gptECS               = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptCamera            = pl_get_api_latest(ptApiRegistry, plCameraI);
    gptResource          = pl_get_api_latest(ptApiRegistry, plResourceI);
    gptProfile           = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptLog               = pl_get_api_latest(ptApiRegistry, plLogI);
    gptRenderer          = pl_get_api_latest(ptApiRegistry, plRendererI);
    gptEcsTools          = pl_get_api_latest(ptApiRegistry, plEcsToolsI);
    gptConsole           = pl_get_api_latest(ptApiRegistry, plConsoleI);
    gptGizmo             = pl_get_api_latest(ptApiRegistry, plGizmoI);
    gptScreenLog           = pl_get_api_latest(ptApiRegistry, plScreenLogI);
    gptIO = gptIOI->get_io();

    pl_load_log_ext(ptApiRegistry, bReload);
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
    pl_load_shader_ext(ptApiRegistry, bReload);
    gptShader = pl_get_api_latest(ptApiRegistry, plShaderI);

    pl_load_ecs_ext(ptApiRegistry, bReload);
    pl_load_resource_ext(ptApiRegistry, bReload);
    pl_load_model_loader_ext(ptApiRegistry, bReload);
    pl_load_renderer_ext(ptApiRegistry, bReload);
    pl_load_ecs_tools_ext(ptApiRegistry, bReload);
    pl_load_gizmo_ext(ptApiRegistry, bReload);
    pl_load_console_ext(ptApiRegistry, bReload);
    pl_load_screen_log_ext(ptApiRegistry, bReload);
    pl_load_debug_ext(ptApiRegistry, bReload);
    pl_load_profile_ext(ptApiRegistry, bReload);
}

PL_EXPORT void
pl_unload_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    pl_unload_shader_ext(ptApiRegistry, bReload);
    pl_unload_job_ext(ptApiRegistry, bReload);
    pl_unload_image_ext(ptApiRegistry, bReload);
    pl_unload_rect_pack_ext(ptApiRegistry, bReload);
    pl_unload_stats_ext(ptApiRegistry, bReload);
    pl_unload_string_intern_ext(ptApiRegistry, bReload);
    pl_unload_graphics_ext(ptApiRegistry, bReload);
    pl_unload_gpu_allocators_ext(ptApiRegistry, bReload);
    pl_unload_draw_ext(ptApiRegistry, bReload);
    pl_unload_draw_backend_ext(ptApiRegistry, bReload);
    pl_unload_ecs_tools_ext(ptApiRegistry, bReload);
    pl_unload_ui_ext(ptApiRegistry, bReload);
    pl_unload_ecs_ext(ptApiRegistry, bReload);
    pl_unload_resource_ext(ptApiRegistry, bReload);
    pl_unload_model_loader_ext(ptApiRegistry, bReload);
    pl_unload_renderer_ext(ptApiRegistry, bReload);
    pl_unload_gizmo_ext(ptApiRegistry, bReload);
    pl_unload_console_ext(ptApiRegistry, bReload);
    pl_unload_debug_ext(ptApiRegistry, bReload);
    pl_unload_screen_log_ext(ptApiRegistry, bReload);
    pl_unload_profile_ext(ptApiRegistry, bReload);
    pl_unload_log_ext(ptApiRegistry, bReload);
}

//-----------------------------------------------------------------------------
// [SECTION] unity build #2
//-----------------------------------------------------------------------------

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


#define PL_STL_IMPLEMENTATION
#include "pl_stl.h"
#undef PL_STL_IMPLEMENTATION

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#undef CGLTF_IMPLEMENTATION