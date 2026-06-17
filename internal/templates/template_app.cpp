/*
   template_app
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] macros
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

// standard
#include <stdlib.h>
#include <stdio.h>
#include <float.h>

// pilot light
#include "pl.h"

// pilot light libraries
#include "pl_memory.h"
#include "pl_string.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"
#include "pl_json.h"

// stable extensions
#include "pl_unity_ext.h"
#include "pl_image_ext.h"
#include "pl_profile_ext.h"
#include "pl_log_ext.h"
#include "pl_stats_ext.h"
#include "pl_graphics_ext.h"
#include "pl_tools_ext.h"
#include "pl_job_ext.h"
#include "pl_draw_ext.h"
#include "pl_ui_ext.h"
#include "pl_shader_ext.h"
#include "pl_string_intern_ext.h"
#include "pl_platform_ext.h"
#include "pl_console_ext.h"
#include "pl_screen_log_ext.h"
#include "pl_starter_ext.h"
#include "pl_pak_ext.h"
#include "pl_datetime_ext.h"
#include "pl_vfs_ext.h"
#include "pl_ecs_ext.h"
#include "pl_config_ext.h"
#include "pl_resource_ext.h"
#include "pl_compress_ext.h"
#include "pl_dxt_ext.h"
#include "pl_gpu_allocators_ext.h"
#include "pl_image_ext.h"
#include "pl_rect_pack_ext.h"
#include "pl_dds_ext.h"
#include "pl_camera_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAppData
{
    plWindow* ptWindow;
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] macors
//-----------------------------------------------------------------------------

#define PL_ALLOC(x)      pl_memory_tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_REALLOC(x, y) pl_memory_tracked_realloc((x), (y), __FILE__, __LINE__)
#define PL_FREE(x)       pl_memory_tracked_realloc((x), 0, __FILE__, __LINE__)

#define PL_DS_ALLOC(x)                      pl_memory_tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) pl_memory_tracked_realloc(NULL, (x), FILE, LINE)
#define PL_DS_FREE(x)                       pl_memory_tracked_realloc((x), 0, __FILE__, __LINE__)
#include "pl_ds.h"

plApiRegistryI* gptApiRegistry = NULL;

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryI* ptApiRegistry, plAppData* ptAppData)
{
    gptApiRegistry = ptApiRegistry;

    // load extensions
    pl_load_ext(ptApiRegistry, false);
    pl_load_platform_ext(ptApiRegistry, false);

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = (plAppData*)PL_ALLOC(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    // use window API to create a window
    plWindowDesc tWindowDesc = PL_ZERO_INIT;
    tWindowDesc.pcTitle = "App";
    tWindowDesc.iXPos   = 200;
    tWindowDesc.iYPos   = 200;
    tWindowDesc.uWidth  = 600;
    tWindowDesc.uHeight = 600;
    pl_window_create(tWindowDesc, &ptAppData->ptWindow);
    pl_window_show(ptAppData->ptWindow);

    plStarterInit tStarterInit = PL_ZERO_INIT;
    tStarterInit.eFlags   = PL_STARTER_FLAGS_ALL_EXTENSIONS;
    tStarterInit.ptWindow = ptAppData->ptWindow;

    // let starter extension handle a lot of boilerplate
    pl_starter_initialize(tStarterInit);
    pl_starter_finalize();

    // return app memory
    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    pl_starter_cleanup();
    pl_window_destroy(ptAppData->ptWindow);
    PL_FREE(ptAppData);

    // unload extensions
    pl_unload_ext(gptApiRegistry, false);
    pl_unload_platform_ext(gptApiRegistry, false);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plWindow* ptWindow, plAppData* ptAppData)
{
    pl_starter_resize();
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    // this needs to be the first call when using the starter
    // extension. You must return if it returns false (usually a swapchain recreation).
    if(!pl_starter_begin_frame())
        return;

    PL_PROFILE_BEGIN_SAMPLE(0, __FUNCTION__);

    // creating another window
    if(pl_ui_begin_window("Pilot Light", NULL, PL_UI_WINDOW_FLAGS_NONE))
    {
        pl_ui_text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
        pl_ui_end_window();
    }

    PL_PROFILE_END_SAMPLE(0);
    pl_starter_end_frame(); // must be the last function called when using the starter extension
}
