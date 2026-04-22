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

const plWindowI*            gptWindows          = NULL;
const plStatsI*             gptStats            = NULL;
const plGraphicsI*          gptGfx              = NULL;
const plToolsI*             gptTools            = NULL;
const plEcsI*               gptEcs              = NULL;
const plJobI*               gptJobs             = NULL;
const plDrawI*              gptDraw             = NULL;
const plUiI*                gptUI               = NULL;
const plIOI*                gptIO               = NULL;
const plShaderI*            gptShader           = NULL;
const plMemoryI*            gptMemory           = NULL;
const plNetworkI*           gptNetwork          = NULL;
const plStringInternI*      gptString           = NULL;
const plProfileI*           gptProfile          = NULL;
const plFileI*              gptFile             = NULL;
const plConsoleI*           gptConsole          = NULL;
const plScreenLogI*         gptScreenLog        = NULL;
const plConfigI*            gptConfig           = NULL;
const plResourceI*          gptResource         = NULL;
const plStarterI*           gptStarter          = NULL;
const plVfsI*               gptVfs              = NULL;
const plPakI*               gptPak              = NULL;
const plDateTimeI*          gptDateTime         = NULL;
const plCompressI*          gptCompress         = NULL;
const plLogI*               gptLog              = NULL;

#define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
#define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

#define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
#define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] helper forward declarations
//-----------------------------------------------------------------------------

void pl__load_apis(plApiRegistryI*);

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
        pl__load_apis(ptApiRegistry);

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
    ptExtensionRegistry->load("pl_platform_ext", "pl_load_platform_ext", "pl_unload_platform_ext", false); // provides the file API used by the drawing ext
    
    pl__load_apis(ptApiRegistry);

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
    gptWindows->create(tWindowDesc, &ptAppData->ptWindow);
    gptWindows->show(ptAppData->ptWindow);

    plStarterInit tStarterInit = {};
    tStarterInit.tFlags   = PL_STARTER_FLAGS_ALL_EXTENSIONS;
    tStarterInit.ptWindow = ptAppData->ptWindow;

    // we will remove this flag so we can handle
    // management of the shader extension
    tStarterInit.tFlags &= ~PL_STARTER_FLAGS_SHADER_EXT;

    // from a graphics standpoint, the starter extension is handling device, swapchain, renderpass
    // etc. which we will get to in later examples
    gptStarter->initialize(tStarterInit);

    // initialize shader extension (we are doing this ourselves so we can add additional shader directories)
    static plShaderOptions tDefaultShaderOptions = {};
    tDefaultShaderOptions.apcIncludeDirectories[0] = "../dependencies/pilotlight/shaders/";
    tDefaultShaderOptions.apcDirectories[0] = "../dependencies/pilotlight/shaders/";
    tDefaultShaderOptions.tFlags = PL_SHADER_FLAGS_AUTO_OUTPUT | PL_SHADER_FLAGS_NEVER_CACHE;
    gptShader->initialize(&tDefaultShaderOptions);
    gptStarter->finalize();

    // add a log channel
    ptAppData->uAppLogChannel = gptLog->add_channel("App", {PL_LOG_CHANNEL_TYPE_BUFFER});

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
    gptShader->cleanup();
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
        {0.0f, 0.0f},
        {500.0f, 500.0f}, {PL_COLOR_32_MAGENTA, 1.0f});

    plDrawLayer2D* ptBGLayer = gptStarter->get_background_layer();
    gptDraw->add_triangle_filled(ptBGLayer,
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
    gptDraw->add_convex_polygon_filled(ptBGLayer, points, sizeof(points)/sizeof(points[0]), {PL_COLOR_32_RGBA(1.0f, 0.25f, 0.25f, 0.5f)});
    gptDraw->add_polygon(ptBGLayer, points, sizeof(points)/sizeof(points[0]), {PL_COLOR_32_RGBA(1.0f, 1.0f, 1.0f, 0.5f), 30.0f});

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
        gptUI->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);

        if(gptUI->button("Log"))
        {
            gptLog->trace(ptAppData->uAppLogChannel, "Log");
            gptLog->debug(ptAppData->uAppLogChannel, "Log");
            gptLog->info(ptAppData->uAppLogChannel, "Log");
            gptLog->warn(ptAppData->uAppLogChannel, "Log");
            gptLog->error(ptAppData->uAppLogChannel, "Log");
            gptLog->fatal(ptAppData->uAppLogChannel, "Log");
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

//-----------------------------------------------------------------------------
// [SECTION] helpers
//-----------------------------------------------------------------------------

void
pl__load_apis(plApiRegistryI* ptApiRegistry)
{
    gptWindows          = pl_get_api_latest(ptApiRegistry, plWindowI);
    gptStats            = pl_get_api_latest(ptApiRegistry, plStatsI);
    gptGfx              = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptTools            = pl_get_api_latest(ptApiRegistry, plToolsI);
    gptEcs              = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptJobs             = pl_get_api_latest(ptApiRegistry, plJobI);
    gptDraw             = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptUI               = pl_get_api_latest(ptApiRegistry, plUiI);
    gptIO               = pl_get_api_latest(ptApiRegistry, plIOI);
    gptShader           = pl_get_api_latest(ptApiRegistry, plShaderI);
    gptMemory           = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptNetwork          = pl_get_api_latest(ptApiRegistry, plNetworkI);
    gptString           = pl_get_api_latest(ptApiRegistry, plStringInternI);
    gptFile             = pl_get_api_latest(ptApiRegistry, plFileI);
    gptConsole          = pl_get_api_latest(ptApiRegistry, plConsoleI);
    gptScreenLog        = pl_get_api_latest(ptApiRegistry, plScreenLogI);
    gptConfig           = pl_get_api_latest(ptApiRegistry, plConfigI);
    gptResource         = pl_get_api_latest(ptApiRegistry, plResourceI);
    gptStarter          = pl_get_api_latest(ptApiRegistry, plStarterI);
    gptVfs              = pl_get_api_latest(ptApiRegistry, plVfsI);
    gptPak              = pl_get_api_latest(ptApiRegistry, plPakI);
    gptDateTime         = pl_get_api_latest(ptApiRegistry, plDateTimeI);
    gptCompress         = pl_get_api_latest(ptApiRegistry, plCompressI);
    gptLog              = pl_get_api_latest(ptApiRegistry, plLogI);
    gptProfile          = pl_get_api_latest(ptApiRegistry, plProfileI);
}
