/*
   template_app
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] apis
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


//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAppData
{
    // window
    plWindow* ptWindow;

    // log channel
    uint64_t uAppLogChannel;

    // console variable
    bool bShowHelpWindow;
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] apis
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

    // default values
    ptAppData->bShowHelpWindow = true;

    // use window API to create a window
    plWindowDesc tWindowDesc = {};
    tWindowDesc.pcTitle = "App";
    tWindowDesc.iXPos   = 200;
    tWindowDesc.iYPos   = 200;
    tWindowDesc.uWidth  = 600;
    tWindowDesc.uHeight = 600;
    pl_window_create(tWindowDesc, &ptAppData->ptWindow);
    pl_window_show(ptAppData->ptWindow);

    plStarterInit tStarterInit = {};
    tStarterInit.tFlags   = PL_STARTER_FLAGS_ALL_EXTENSIONS;
    tStarterInit.ptWindow = ptAppData->ptWindow;

    // we will remove this flag so we can handle
    // management of the shader extension
    tStarterInit.tFlags &= ~PL_STARTER_FLAGS_SHADER_EXT;

    // from a graphics standpoint, the starter extension is handling device, swapchain, renderpass
    // etc. which we will get to in later examples
    pl_starter_initialize(tStarterInit);

    // initialize shader extension (we are doing this ourselves so we can add additional shader directories)
    static plShaderOptions tDefaultShaderOptions = {};
    tDefaultShaderOptions.apcIncludeDirectories[0] = "../dependencies/pilotlight/shaders/";
    tDefaultShaderOptions.apcDirectories[0] = "../dependencies/pilotlight/shaders/";
    tDefaultShaderOptions.tFlags = PL_SHADER_FLAGS_AUTO_OUTPUT | PL_SHADER_FLAGS_NEVER_CACHE;
    pl_shader_initialize(&tDefaultShaderOptions);
    pl_starter_finalize();

    // add a log channel
    ptAppData->uAppLogChannel = pl_log_add_channel("App", {PL_LOG_CHANNEL_TYPE_BUFFER});

    // add a console variable
    pl_console_add_toggle_variable("a.HelpWindow", &ptAppData->bShowHelpWindow, "toggle help window", PL_CONSOLE_VARIABLE_FLAGS_NONE);

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
    pl_shader_cleanup();
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

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~stats API~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    // rather than have to lookup the counter every frame, its best to "store" it
    // like this. To update it, just deference it and set the value.
    static double* pdExample2Counter = NULL;
    if(!pdExample2Counter)
        pdExample2Counter = pl_stats_get_counter("example 2 counter");

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~drawing & profile API~~~~~~~~~~~~~~~~~~~~~~~~~~~

    pl_profile_begin_sample(0, "example drawing");
    
    plDrawLayer2D* ptFGLayer = pl_starter_get_foreground_layer();
    pl_draw_add_line(ptFGLayer,
        {0.0f, 0.0f},
        {500.0f, 500.0f}, {PL_COLOR_32_MAGENTA, 1.0f});

    plDrawLayer2D* ptBGLayer = pl_starter_get_background_layer();
    pl_draw_add_triangle_filled(ptBGLayer,
        {50.0f, 100.0f},
        {200.0f},
        {100.0f, 200.0f}, {PL_COLOR_32_RGBA(0.0f, 0.5f, 1.0f, 0.5f)});

    plVec2 points[5] = {
        {100.0f, 100.0f},
        {500.0f, 100.0f},
        {500.0f, 300.0f},
        {300.0f, 500.0f},
        {100.0f, 300.0f},
    };
    pl_draw_add_convex_polygon_filled(ptBGLayer, points, sizeof(points)/sizeof(points[0]), {PL_COLOR_32_RGBA(1.0f, 0.25f, 0.25f, 0.5f)});
    pl_draw_add_polygon(ptBGLayer, points, sizeof(points)/sizeof(points[0]), {PL_COLOR_32_RGBA(1.0f, 1.0f, 1.0f, 0.5f), 30.0f});

    pl_profile_end_sample(0);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~UI & Screen Log API~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // creating a window
    if(ptAppData->bShowHelpWindow)
    {
        if(pl_ui_begin_window("Help", NULL, PL_UI_WINDOW_FLAGS_AUTO_SIZE | PL_UI_WINDOW_FLAGS_NO_COLLAPSE))
        {
            pl_ui_layout_static(0.0f, 500.0f, 1);
            pl_ui_text("Press F1 to bring up console.");
            pl_ui_text("Look for t.StatsTool (we added a stat)");
            pl_ui_text("Look for t.LogTool (we added a log channel)");
            pl_ui_text("Look for t.ProfileTool");
            pl_ui_text("Look for t.MemoryAllocationTool and look for example 2!");
            pl_ui_text("Look for a.HelpWindow (console variable we added)");
            pl_ui_end_window();
        }
    }

    // creating another window
    if(pl_ui_begin_window("Pilot Light", NULL, PL_UI_WINDOW_FLAGS_NONE))
    {
        pl_ui_text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);

        if(pl_ui_button("Log"))
        {
            pl_log_trace(ptAppData->uAppLogChannel, "Log");
            pl_log_debug(ptAppData->uAppLogChannel, "Log");
            pl_log_info(ptAppData->uAppLogChannel, "Log");
            pl_log_warn(ptAppData->uAppLogChannel, "Log");
            pl_log_error(ptAppData->uAppLogChannel, "Log");
            pl_log_fatal(ptAppData->uAppLogChannel, "Log");
        }

        static int iCounter = 0;
        pl_ui_slider_int("Stat Counter Example", &iCounter, -10, 10, 0);
        *pdExample2Counter = iCounter; // setting our stat variable

        pl_ui_layout_row_begin(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 2); // got to pl_ui_ext.h to see layout systems
        
        pl_ui_layout_row_push(0.3f);
        if(pl_ui_button("Log To Screen"))
            pl_screen_log_add_message(5.0, "Cool Message!");

        pl_ui_layout_row_push(0.3f);
        if(pl_ui_button("Big Log To Screen"))
            pl_screen_log_add_message_ex(0, 5, PL_COLOR_32_GREEN, 3.0f, "%s", "Bigger & Greener!");

        pl_ui_layout_row_end();

        pl_ui_end_window();
    }

    // must be the last function called when using the starter extension
    pl_starter_end_frame(); 
}
