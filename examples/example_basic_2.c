/*
   example_basic_2.c
     - demonstrates loading APIs
     - demonstrates loading extensions
     - demonstrates hot reloading
     - demonstrates starter extension
     - demonstrates basic drawing extension (2D)
     - demonstrates basic screen log extension
     - demonstrates basic console extension
     - demonstrates basic UI extension
*/

/*
Index of this file:
// [SECTION] quick notes
// [SECTION] includes
// [SECTION] structs
// [SECTION] apis
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
*/

//-----------------------------------------------------------------------------
// [SECTION] quick notes
//-----------------------------------------------------------------------------

/*
    This example is the first to introduce extensions. Extensions are just
    shared libraries that export a "load" and "unload" function. The default
    being:

    * void pl_load_ext  (plApiRegistryI*, bool reload)
    * void pl_unload_ext(plApiRegistryI*, bool reload)
    
    This example is also the first to introduce the "starter" extension. This
    extension acts a bit as a helper extension to remove some common boilerplate
    but is also just useful in general for most applications only needing to use
    UI, plotting, drawing, etc. Or even to just experiment with the lower level
    graphics extension in an isolated manner. Later examples will gradually peel
    away at this extension and others. For this example, we will just demonstrate
    some of the smaller helpful extensions. These include:

    * log
    * profile
    * stat
    * console
    * screen log
    * ui
    
    This will be very light introductions with later examples going into more
    detail. Feel free to open the header file for the extension for more
    information and functionality.
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdlib.h> // malloc, free
#include <string.h> // memset
#include "pl.h"

#include "pl_unity_ext.h" // for loading extension directly to not need api structs

#define PL_MATH_INCLUDE_FUNCTIONS // required to expose some of the color helpers
#include "pl_math.h"

// extensions
#include "pl_draw_ext.h"
#include "pl_starter_ext.h"
#include "pl_ui_ext.h"
#include "pl_screen_log_ext.h"
#include "pl_profile_ext.h"
#include "pl_log_ext.h"
#include "pl_stats_ext.h"
#include "pl_console_ext.h"
#include "pl_platform_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAppData
{
    // window
    plWindow* ptWindow;

    // log channel
    uint64_t uExampleLogChannel;

    // console variable
    bool bShowHelpWindow;
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

plApiRegistryI* gptApiRegistry = NULL;

#define PL_ALLOC(x)      pl_memory_tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_REALLOC(x, y) pl_memory_tracked_realloc((x), (y), __FILE__, __LINE__)
#define PL_FREE(x)       pl_memory_tracked_realloc((x), 0, __FILE__, __LINE__)

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryI* ptApiRegistry, plAppData* ptAppData)
{
    gptApiRegistry = ptApiRegistry; // saving for cleanup later

    // retrieve extension registry
    const plExtensionRegistryI* ptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);

    // load extensions
    pl_load_ext(ptApiRegistry, false);
    pl_load_platform_ext(ptApiRegistry, false);
    
    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = PL_ALLOC(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    // default values
    ptAppData->bShowHelpWindow = true;

    // use window API to create a window
    plWindowDesc tWindowDesc = {
        .pcTitle = "Example Basic 2",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 600,
        .uHeight = 600,
    };
    pl_window_create(tWindowDesc, &ptAppData->ptWindow);
    pl_window_show(ptAppData->ptWindow);

    // initialize the starter API (handles alot of boilerplate)
    plStarterInit tStarterInit = {
        .tFlags   = PL_STARTER_FLAGS_ALL_EXTENSIONS,
        .ptWindow = ptAppData->ptWindow
    };
    pl_starter_initialize(tStarterInit);
    pl_starter_finalize();

    // add a log channel
    ptAppData->uExampleLogChannel = pl_log_add_channel("Example 2", (plLogExtChannelInit){.tType = PL_LOG_CHANNEL_TYPE_BUFFER});

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
    pl_window_destroy(ptAppData->ptWindow);
    
    // unload extensions
    pl_unload_ext(gptApiRegistry, false);
    pl_unload_platform_ext(gptApiRegistry, false);
    PL_FREE(ptAppData);
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
        (plVec2){0.0f, 0.0f},
        (plVec2){500.0f, 500.0f}, (plDrawLineOptions){ .fThickness = 1.0f, .uColor = PL_COLOR_32_MAGENTA});

    plDrawLayer2D* ptBGLayer = pl_starter_get_background_layer();
    pl_draw_add_triangle_filled(ptBGLayer,
        (plVec2){50.0f, 100.0f},
        (plVec2){200.0f},
        (plVec2){100.0f, 200.0f}, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(0.0f, 0.5f, 1.0f, 0.5f)});

    plVec2 points[5] = {
        (plVec2){100.0f, 100.0f},
        (plVec2){500.0f, 100.0f},
        (plVec2){500.0f, 300.0f},
        (plVec2){300.0f, 500.0f},
        (plVec2){100.0f, 300.0f},
    };
    pl_draw_add_convex_polygon_filled(ptBGLayer, points, sizeof(points)/sizeof(points[0]), (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 0.25f, 0.25f, 0.5f)});
    pl_draw_add_polygon(ptBGLayer, points, sizeof(points)/sizeof(points[0]), (plDrawLineOptions){.fThickness = 30.0f, .uColor = PL_COLOR_32_RGBA(1.0f, 1.0f, 1.0f, 0.5f)});

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
            pl_log_trace(ptAppData->uExampleLogChannel, "Log");
            pl_log_debug(ptAppData->uExampleLogChannel, "Log");
            pl_log_info(ptAppData->uExampleLogChannel, "Log");
            pl_log_warn(ptAppData->uExampleLogChannel, "Log");
            pl_log_error(ptAppData->uExampleLogChannel, "Log");
            pl_log_fatal(ptAppData->uExampleLogChannel, "Log");
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
