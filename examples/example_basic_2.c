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
    
    Later examples will explain more about the details of creating an extension
    but the important thing to understand now is that an extension just provides
    an implementation of an API. The "load" function allows an extension to
    request APIs it depends on and to register any API it provides. The unload
    function just allows an extension the opportunity to unregister and perform
    any required cleanup.

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

const plIOI*        gptIO        = NULL;
const plWindowI*    gptWindows   = NULL;
const plDrawI*      gptDraw      = NULL;
const plStarterI*   gptStarter   = NULL;
const plUiI*        gptUI        = NULL;
const plScreenLogI* gptScreenLog = NULL;
const plProfileI*   gptProfile   = NULL;
const plStatsI*     gptStats     = NULL;
const plMemoryI*    gptMemory    = NULL;
const plLogI*       gptLog       = NULL;
const plConsoleI*   gptConsole   = NULL;

#define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
#define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryI* ptApiRegistry, plAppData* ptAppData)
{
    // NOTE: on first load, "pAppData" will be NULL but on reloads
    //       it will be the value returned from this function

    // if "ptAppData" is a valid pointer, then this function is being called
    // during a hot reload.
    if(ptAppData)
    {

        // re-retrieve the apis since we are now in
        // a different dll/so
        gptIO        = pl_get_api_latest(ptApiRegistry, plIOI);
        gptWindows   = pl_get_api_latest(ptApiRegistry, plWindowI);
        gptDraw      = pl_get_api_latest(ptApiRegistry, plDrawI);
        gptStarter   = pl_get_api_latest(ptApiRegistry, plStarterI);
        gptUI        = pl_get_api_latest(ptApiRegistry, plUiI);
        gptScreenLog = pl_get_api_latest(ptApiRegistry, plScreenLogI);
        gptProfile   = pl_get_api_latest(ptApiRegistry, plProfileI);
        gptStats     = pl_get_api_latest(ptApiRegistry, plStatsI);
        gptMemory    = pl_get_api_latest(ptApiRegistry, plMemoryI);
        gptLog       = pl_get_api_latest(ptApiRegistry, plLogI);
        gptConsole   = pl_get_api_latest(ptApiRegistry, plConsoleI);

        return ptAppData;
    }

    // retrieve extension registry
    const plExtensionRegistryI* ptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);

    // load extensions
    //   * first argument is the shared library name WITHOUT the extension
    //   * second & third argument is the load/unload functions names (use NULL for the default of "pl_load_ext" &
    //     "pl_unload_ext")
    //   * fourth argument indicates if the extension is reloadable (should we check for changes and reload if changed)
    ptExtensionRegistry->load("pl_unity_ext", NULL, NULL, true);
    ptExtensionRegistry->load("pl_platform_ext", NULL, NULL, false); // provides the file API used by the drawing ext
    
    // load required apis
    gptIO      = pl_get_api_latest(ptApiRegistry, plIOI);
    gptWindows = pl_get_api_latest(ptApiRegistry, plWindowI);

    // load required apis (these are provided though extensions)
    gptDraw      = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptStarter   = pl_get_api_latest(ptApiRegistry, plStarterI);
    gptUI        = pl_get_api_latest(ptApiRegistry, plUiI);
    gptScreenLog = pl_get_api_latest(ptApiRegistry, plScreenLogI);
    gptProfile   = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptStats     = pl_get_api_latest(ptApiRegistry, plStatsI);
    gptMemory    = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptLog       = pl_get_api_latest(ptApiRegistry, plLogI);
    gptConsole   = pl_get_api_latest(ptApiRegistry, plConsoleI);

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
    gptWindows->create(tWindowDesc, &ptAppData->ptWindow);
    gptWindows->show(ptAppData->ptWindow);

    // initialize the starter API (handles alot of boilerplate)
    plStarterInit tStarterInit = {
        .tFlags   = PL_STARTER_FLAGS_ALL_EXTENSIONS,
        .ptWindow = ptAppData->ptWindow
    };
    gptStarter->initialize(tStarterInit);
    gptStarter->finalize();

    // add a log channel
    ptAppData->uExampleLogChannel = gptLog->add_channel("Example 2", (plLogExtChannelInit){.tType = PL_LOG_CHANNEL_TYPE_BUFFER});

    // add a console variable
    gptConsole->add_toggle_variable("a.HelpWindow", &ptAppData->bShowHelpWindow, "toggle help window", PL_CONSOLE_VARIABLE_FLAGS_NONE);

    // return app memory
    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    gptStarter->cleanup();
    gptWindows->destroy(ptAppData->ptWindow);
    PL_FREE(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plWindow* ptWindow, plAppData* ptAppData)
{
    gptStarter->resize();
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    // this needs to be the first call when using the starter
    // extension. You must return if it returns false (usually a swapchain recreation).
    if(!gptStarter->begin_frame())
        return;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~stats API~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    // rather than have to lookup the counter every frame, its best to "store" it
    // like this. To update it, just deference it and set the value.
    static double* pdExample2Counter = NULL;
    if(!pdExample2Counter)
        pdExample2Counter = gptStats->get_counter("example 2 counter");

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~drawing & profile API~~~~~~~~~~~~~~~~~~~~~~~~~~~

    gptProfile->begin_sample(0, "example drawing");
    
    plDrawLayer2D* ptFGLayer = gptStarter->get_foreground_layer();
    gptDraw->add_line(ptFGLayer,
        (plVec2){0.0f, 0.0f},
        (plVec2){500.0f, 500.0f}, (plDrawLineOptions){ .fThickness = 1.0f, .uColor = PL_COLOR_32_MAGENTA});

    plDrawLayer2D* ptBGLayer = gptStarter->get_background_layer();
    gptDraw->add_triangle_filled(ptBGLayer,
        (plVec2){50.0f, 100.0f},
        (plVec2){200.0f},
        (plVec2){100.0f, 200.0f}, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(0.0f, 0.5f, 1.0f, 0.5f)});

    gptProfile->end_sample(0);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~UI & Screen Log API~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // creating a window
    if(ptAppData->bShowHelpWindow)
    {
        if(gptUI->begin_window("Help", NULL, PL_UI_WINDOW_FLAGS_AUTO_SIZE | PL_UI_WINDOW_FLAGS_NO_COLLAPSE))
        {
            gptUI->layout_static(0.0f, 500.0f, 1);
            gptUI->text("Press F1 to bring up console.");
            gptUI->text("Look for t.StatsTool (we added a stat)");
            gptUI->text("Look for t.LogTool (we added a log channel)");
            gptUI->text("Look for t.ProfileTool");
            gptUI->text("Look for t.MemoryAllocationTool and look for example 2!");
            gptUI->text("Look for a.HelpWindow (console variable we added)");
            gptUI->end_window();
        }
    }

    // creating another window
    if(gptUI->begin_window("Pilot Light", NULL, PL_UI_WINDOW_FLAGS_NONE))
    {
        gptUI->text("Pilot Light %s", PILOT_LIGHT_CORE_VERSION_STRING);

        if(gptUI->button("Log"))
        {
            gptLog->trace(ptAppData->uExampleLogChannel, "Log");
            gptLog->debug(ptAppData->uExampleLogChannel, "Log");
            gptLog->info(ptAppData->uExampleLogChannel, "Log");
            gptLog->warn(ptAppData->uExampleLogChannel, "Log");
            gptLog->error(ptAppData->uExampleLogChannel, "Log");
            gptLog->fatal(ptAppData->uExampleLogChannel, "Log");
        }

        static int iCounter = 0;
        gptUI->slider_int("Stat Counter Example", &iCounter, -10, 10, 0);
        *pdExample2Counter = iCounter; // setting our stat variable

        gptUI->layout_row_begin(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 2); // got to pl_ui_ext.h to see layout systems
        
        gptUI->layout_row_push(0.3f);
        if(gptUI->button("Log To Screen"))
            gptScreenLog->add_message(5.0, "Cool Message!");

        gptUI->layout_row_push(0.3f);
        if(gptUI->button("Big Log To Screen"))
            gptScreenLog->add_message_ex(0, 5, PL_COLOR_32_GREEN, 3.0f, "%s", "Bigger & Greener!");

        gptUI->layout_row_end();

        gptUI->end_window();
    }

    // must be the last function called when using the starter extension
    gptStarter->end_frame(); 
}
