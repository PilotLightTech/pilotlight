/*
   app.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] apis
// [SECTION] helper forward declarations
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

// pilot light core
#include "pl.h"

// pilot light libraries
#include "pl_memory.h"
#include "pl_string.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

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
// [SECTION] apis
//-----------------------------------------------------------------------------

const plWindowI*        gptWindows       = NULL;
const plStatsI*         gptStats         = NULL;
const plGraphicsI*      gptGfx           = NULL;
const plToolsI*         gptTools         = NULL;
const plEcsI*           gptEcs           = NULL;
const plJobI*           gptJobs          = NULL;
const plDrawI*          gptDraw          = NULL;
const plUiI*            gptUI            = NULL;
const plIOI*            gptIO            = NULL;
const plShaderI*        gptShader        = NULL;
const plMemoryI*        gptMemory        = NULL;
const plStringInternI*  gptString        = NULL;
const plProfileI*       gptProfile       = NULL;
const plFileI*          gptFile          = NULL;
const plConsoleI*       gptConsole       = NULL;
const plScreenLogI*     gptScreenLog     = NULL;
const plConfigI*        gptConfig        = NULL;
const plResourceI*      gptResource      = NULL;
const plStarterI*       gptStarter       = NULL;
const plVfsI*           gptVfs           = NULL;
const plPakI*           gptPak           = NULL;
const plDateTimeI*      gptDateTime      = NULL;
const plCompressI*      gptCompress      = NULL;
const plLogI*           gptLog           = NULL;
const plDxtI*           gptDxt           = NULL;
const plGPUAllocatorsI* gptGpuAllocators = NULL;
const plImageI*         gptImage         = NULL;
const plThreadsI*       gptThreads       = NULL;
const plAtomicsI*       gptAtomics       = NULL;
const plNetworkI*       gptNetwork       = NULL;
const plVirtualMemoryI* gptVirtualMemory = NULL;
const plRectPackI*      gptRectPack      = NULL;
const plDdsI*           gptDds           = NULL;
const plCameraI*        gptCamera        = NULL;

// helpful macros for memory tracking
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
    // NOTE: on first load, "ptAppData" will be NULL but on reloads
    //       it will be the value returned from this function's first
    //       call

    // if "ptAppData" is a valid pointer, then this function is being called
    // during a hot reload.
    if(ptAppData)
    {

        // re-retrieve the apis since we are now in
        // a different dll/so and we are storing them
        // as global variables
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
    ptExtensionRegistry->load("pl_platform_ext", "pl_load_platform_ext", "pl_unload_platform_ext", false);
    
    pl__load_apis(ptApiRegistry);

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
    gptWindows->create(tWindowDesc, &ptAppData->ptWindow);
    gptWindows->show(ptAppData->ptWindow);

    plStarterInit tStarterInit = PL_ZERO_INIT;
    tStarterInit.eFlags   = PL_STARTER_FLAGS_ALL_EXTENSIONS;
    tStarterInit.ptWindow = ptAppData->ptWindow;

    // let starter extension handle a lot of boilerplate
    gptStarter->initialize(tStarterInit);
    gptStarter->finalize();

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

    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    // creating another window
    if(gptUI->begin_window("Pilot Light", NULL, PL_UI_WINDOW_FLAGS_NONE))
    {
        gptUI->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
        gptUI->end_window();
    }

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
    gptStarter->end_frame(); // must be the last function called when using the starter extension
}

//-----------------------------------------------------------------------------
// [SECTION] helpers
//-----------------------------------------------------------------------------

void
pl__load_apis(plApiRegistryI* ptApiRegistry)
{
    gptWindows       = pl_get_api_latest(ptApiRegistry, plWindowI);
    gptStats         = pl_get_api_latest(ptApiRegistry, plStatsI);
    gptGfx           = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptTools         = pl_get_api_latest(ptApiRegistry, plToolsI);
    gptEcs           = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptJobs          = pl_get_api_latest(ptApiRegistry, plJobI);
    gptDraw          = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptUI            = pl_get_api_latest(ptApiRegistry, plUiI);
    gptIO            = pl_get_api_latest(ptApiRegistry, plIOI);
    gptShader        = pl_get_api_latest(ptApiRegistry, plShaderI);
    gptMemory        = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptNetwork       = pl_get_api_latest(ptApiRegistry, plNetworkI);
    gptString        = pl_get_api_latest(ptApiRegistry, plStringInternI);
    gptFile          = pl_get_api_latest(ptApiRegistry, plFileI);
    gptConsole       = pl_get_api_latest(ptApiRegistry, plConsoleI);
    gptScreenLog     = pl_get_api_latest(ptApiRegistry, plScreenLogI);
    gptConfig        = pl_get_api_latest(ptApiRegistry, plConfigI);
    gptResource      = pl_get_api_latest(ptApiRegistry, plResourceI);
    gptStarter       = pl_get_api_latest(ptApiRegistry, plStarterI);
    gptVfs           = pl_get_api_latest(ptApiRegistry, plVfsI);
    gptPak           = pl_get_api_latest(ptApiRegistry, plPakI);
    gptDateTime      = pl_get_api_latest(ptApiRegistry, plDateTimeI);
    gptCompress      = pl_get_api_latest(ptApiRegistry, plCompressI);
    gptLog           = pl_get_api_latest(ptApiRegistry, plLogI);
    gptProfile       = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptDxt           = pl_get_api_latest(ptApiRegistry, plDxtI);
    gptGpuAllocators = pl_get_api_latest(ptApiRegistry, plGPUAllocatorsI);
    gptImage         = pl_get_api_latest(ptApiRegistry, plImageI);
    gptThreads       = pl_get_api_latest(ptApiRegistry, plThreadsI);
    gptAtomics       = pl_get_api_latest(ptApiRegistry, plAtomicsI);
    gptVirtualMemory = pl_get_api_latest(ptApiRegistry, plVirtualMemoryI);
    gptRectPack      = pl_get_api_latest(ptApiRegistry, plRectPackI);
    gptDds           = pl_get_api_latest(ptApiRegistry, plDdsI);
    gptCamera        = pl_get_api_latest(ptApiRegistry, plCameraI);
}
