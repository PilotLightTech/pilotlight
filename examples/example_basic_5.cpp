
//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdlib.h> // malloc, free
#include <stdio.h>  // printf
#include <string.h> // memset

// pilot light
#include "pl.h"
#include "pl_memory.h"
#include "pl_string.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"
#include "pl_icons.h"
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
#include "pl_draw_backend_ext.h"
#include "pl_ui_ext.h"
#include "pl_shader_ext.h"
#include "pl_string_intern_ext.h"
#include "pl_platform_ext.h"
#include "pl_console_ext.h"
#include "pl_screen_log_ext.h"
#include "pl_starter_ext.h"
#include "pl_pak_ext.h"
#include "pl_datetime_ext.h"
#include "pl_config_ext.h"
#include "pl_vfs_ext.h"
#include "pl_compress_ext.h"

// dear imgui
#include "pl_dear_imgui_ext.h"
#include "imgui.h"
#include "implot.h"

//-----------------------------------------------------------------------------
// [SECTION] global apis
//-----------------------------------------------------------------------------

const plWindowI*       gptWindows       = nullptr;
const plStatsI*        gptStats         = nullptr;
const plGraphicsI*     gptGfx           = nullptr;
const plToolsI*        gptTools         = nullptr;
const plJobI*          gptJobs          = nullptr;
const plDrawI*         gptDraw          = nullptr;
const plDrawBackendI*  gptDrawBackend   = nullptr;
const plUiI*           gptUI            = nullptr;
const plIOI*           gptIO            = nullptr;
const plShaderI*       gptShader        = nullptr;
const plMemoryI*       gptMemory        = nullptr;
const plNetworkI*      gptNetwork       = nullptr;
const plStringInternI* gptString        = nullptr;
const plProfileI*      gptProfile       = nullptr;
const plFileI*         gptFile          = nullptr;
const plConsoleI*      gptConsole       = nullptr;
const plScreenLogI*    gptScreenLog     = nullptr;
const plConfigI*       gptConfig        = nullptr;
const plStarterI*      gptStarter       = nullptr;
const plVfsI*          gptVfs           = nullptr;
const plPakI*          gptPak           = nullptr;
const plDateTimeI*     gptDateTime      = nullptr;
const plCompressI*     gptCompress      = nullptr;
const plDearImGuiI*    gptDearImGui     = nullptr;

#define PL_ALLOC(x)      gptMemory->tracked_realloc(nullptr, (x), __FILE__, __LINE__)
#define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
#define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

#define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(nullptr, (x), __FILE__, __LINE__)
#define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(nullptr, (x), FILE, LINE)
#define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAppData
{
    plWindow* ptWindow;

    // UI options
    bool bShowImGuiDemo;
    bool bShowImPlotDemo;
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryI* ptApiRegistry, plAppData* ptAppData)
{
    // NOTE: on first load, "pAppData" will be NULL but on reloads
    //       it will be the value returned from this function

    // retrieve the data registry API, this is the API used for sharing data
    // between extensions & the runtime
    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~dear imgui context stuff~~~~~~~~~~~~~~~~~~~~~~~~~~

    // Dear ImGui uses globals for it's content & memory allocators. If you are
    // crossing a dll/so boundary, you must set these. The experimental backend
    // provides these to the data registry with the names below, so they must
    // be retrieved and set.

    // retrieve/set imgui context
    ImGuiContext* ptImguiContext = (ImGuiContext*)ptDataRegistry->get_data("imgui");
    ImGui::SetCurrentContext(ptImguiContext);

    // retrieve/set imgui allocator functions
    ImGuiMemAllocFunc p_alloc_func = (ImGuiMemAllocFunc)ptDataRegistry->get_data("imgui allocate");
    ImGuiMemFreeFunc p_free_func = (ImGuiMemFreeFunc)ptDataRegistry->get_data("imgui free");
    ImGui::SetAllocatorFunctions(p_alloc_func, p_free_func, nullptr);

    // if "ptAppData" is a valid pointer, then this function is being called
    // during a hot reload.
    if(ptAppData)
    {

        // re-retrieve the apis since we are now in
        // a different dll/so
        gptWindows       = pl_get_api_latest(ptApiRegistry, plWindowI);
        gptStats         = pl_get_api_latest(ptApiRegistry, plStatsI);
        gptGfx           = pl_get_api_latest(ptApiRegistry, plGraphicsI);
        gptTools         = pl_get_api_latest(ptApiRegistry, plToolsI);
        gptJobs          = pl_get_api_latest(ptApiRegistry, plJobI);
        gptDraw          = pl_get_api_latest(ptApiRegistry, plDrawI);
        gptDrawBackend   = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
        gptUI            = pl_get_api_latest(ptApiRegistry, plUiI);
        gptIO            = pl_get_api_latest(ptApiRegistry, plIOI);
        gptShader        = pl_get_api_latest(ptApiRegistry, plShaderI);
        gptMemory        = pl_get_api_latest(ptApiRegistry, plMemoryI);
        gptNetwork       = pl_get_api_latest(ptApiRegistry, plNetworkI);
        gptString        = pl_get_api_latest(ptApiRegistry, plStringInternI);
        gptProfile       = pl_get_api_latest(ptApiRegistry, plProfileI);
        gptFile          = pl_get_api_latest(ptApiRegistry, plFileI);
        gptConsole       = pl_get_api_latest(ptApiRegistry, plConsoleI);
        gptScreenLog     = pl_get_api_latest(ptApiRegistry, plScreenLogI);
        gptConfig        = pl_get_api_latest(ptApiRegistry, plConfigI);
        gptStarter       = pl_get_api_latest(ptApiRegistry, plStarterI);
        gptDearImGui     = pl_get_api_latest(ptApiRegistry, plDearImGuiI);
        gptDateTime      = pl_get_api_latest(ptApiRegistry, plDateTimeI);
        gptVfs           = pl_get_api_latest(ptApiRegistry, plVfsI);
        gptPak           = pl_get_api_latest(ptApiRegistry, plPakI);
        gptDateTime      = pl_get_api_latest(ptApiRegistry, plDateTimeI);
        gptCompress      = pl_get_api_latest(ptApiRegistry, plCompressI);

        gptScreenLog->add_message_ex(0, 15.0, PL_COLOR_32_MAGENTA, 1.5f, "%s", "App Hot Reloaded");

        ImPlot::SetCurrentContext((ImPlotContext*)ptDataRegistry->get_data("implot"));

        return ptAppData;
    }

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = (plAppData*)malloc(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    // retrieve extension registry
    const plExtensionRegistryI* ptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);

    // load extensions
    ptExtensionRegistry->load("pl_unity_ext", NULL, NULL, true);
    ptExtensionRegistry->load("pl_platform_ext", NULL, NULL, false);
    ptExtensionRegistry->load("pl_dear_imgui_ext", NULL, NULL, false);
    
    // load required apis
    gptWindows       = pl_get_api_latest(ptApiRegistry, plWindowI);
    gptStats         = pl_get_api_latest(ptApiRegistry, plStatsI);
    gptGfx           = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptTools         = pl_get_api_latest(ptApiRegistry, plToolsI);
    gptJobs          = pl_get_api_latest(ptApiRegistry, plJobI);
    gptDraw          = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptDrawBackend   = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
    gptUI            = pl_get_api_latest(ptApiRegistry, plUiI);
    gptIO            = pl_get_api_latest(ptApiRegistry, plIOI);
    gptShader        = pl_get_api_latest(ptApiRegistry, plShaderI);
    gptMemory        = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptNetwork       = pl_get_api_latest(ptApiRegistry, plNetworkI);
    gptString        = pl_get_api_latest(ptApiRegistry, plStringInternI);
    gptProfile       = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptFile          = pl_get_api_latest(ptApiRegistry, plFileI);
    gptConsole       = pl_get_api_latest(ptApiRegistry, plConsoleI);
    gptScreenLog     = pl_get_api_latest(ptApiRegistry, plScreenLogI);
    gptConfig        = pl_get_api_latest(ptApiRegistry, plConfigI);
    gptStarter       = pl_get_api_latest(ptApiRegistry, plStarterI);
    gptDearImGui     = pl_get_api_latest(ptApiRegistry, plDearImGuiI);
    gptDateTime      = pl_get_api_latest(ptApiRegistry, plDateTimeI);
    gptVfs           = pl_get_api_latest(ptApiRegistry, plVfsI);
    gptPak           = pl_get_api_latest(ptApiRegistry, plPakI);
    gptDateTime      = pl_get_api_latest(ptApiRegistry, plDateTimeI);
    gptCompress      = pl_get_api_latest(ptApiRegistry, plCompressI);

    if(gptIO->get_io()->bHotReloadActive)
    {
        gptVfs->mount_directory("/shaders", "../shaders", PL_VFS_MOUNT_FLAGS_NONE);
        gptVfs->mount_directory("/shader-temp", "../shader-temp", PL_VFS_MOUNT_FLAGS_NONE);
        gptFile->create_directory("../shader-temp");
    }
    else
    {
        gptVfs->mount_pak("/shader-temp", "shaders.pak", PL_VFS_MOUNT_FLAGS_NONE);
    }

    // use window API to create a window

    plWindowDesc tWindowDesc = {
        PL_WINDOW_FLAG_NONE,
        "Tool Template",
        500,
        500,
        200,
        200
    };
    gptWindows->create(tWindowDesc, &ptAppData->ptWindow);
    gptWindows->show(ptAppData->ptWindow);

    // setup starter extension
    plStarterInit tStarterInit = {};
    tStarterInit.tFlags   = PL_STARTER_FLAGS_ALL_EXTENSIONS;
    tStarterInit.ptWindow = ptAppData->ptWindow;
    tStarterInit.tFlags &= ~PL_STARTER_FLAGS_SHADER_EXT;
    gptStarter->initialize(tStarterInit);

    // initialize shader compiler
    static plShaderOptions tDefaultShaderOptions = {};
    tDefaultShaderOptions.apcIncludeDirectories[0] = "/shaders/";
    tDefaultShaderOptions.apcDirectories[0] = "/shaders/";
    tDefaultShaderOptions.pcCacheOutputDirectory = "/shader-temp/";
    tDefaultShaderOptions.tFlags = PL_SHADER_FLAGS_AUTO_OUTPUT | PL_SHADER_FLAGS_INCLUDE_DEBUG;
    gptShader->initialize(&tDefaultShaderOptions);

    // wraps up (i.e. builds font atlas)
    gptStarter->finalize();

    // initializes out Dear ImGui backend (similar to the draw backend extension)
    gptDearImGui->initialize(gptStarter->get_device(), gptStarter->get_swapchain(), gptStarter->get_render_pass());

    // same process for implot as imgui
    ImPlot::SetCurrentContext((ImPlotContext*)ptDataRegistry->get_data("implot"));
    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    plDevice* ptDevice = gptStarter->get_device();
    gptGfx->flush_device(ptDevice);
    gptDearImGui->cleanup();
    gptStarter->cleanup();
    gptWindows->destroy(ptAppData->ptWindow);
    free(ptAppData);
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

    if(!gptStarter->begin_frame())
        return;

    gptDearImGui->new_frame(gptStarter->get_device(), gptStarter->get_render_pass());

    ImGui::DockSpaceOverViewport();
    if(ImGui::BeginMainMenuBar())
    {
        if(ImGui::BeginMenu("File", false)) ImGui::EndMenu();
        if(ImGui::BeginMenu("Edit", false)) ImGui::EndMenu();
        if(ImGui::BeginMenu("Tools", true))
        {
            ImGui::MenuItem("Dear ImGui Demo", nullptr, &ptAppData->bShowImGuiDemo);
            ImGui::MenuItem("ImPlot Demo", nullptr, &ptAppData->bShowImPlotDemo);
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("Help", false)) ImGui::EndMenu();
        ImGui::EndMainMenuBar();
    }

    if(ptAppData->bShowImPlotDemo)
        ImPlot::ShowDemoWindow(&ptAppData->bShowImPlotDemo);
    if(ptAppData->bShowImGuiDemo)
        ImGui::ShowDemoWindow(&ptAppData->bShowImGuiDemo);

    // start main pass & return the encoder being used
    plRenderEncoder* ptEncoder = gptStarter->begin_main_pass();
    gptDearImGui->render(ptEncoder, gptGfx->get_encoder_command_buffer(ptEncoder));
    gptStarter->end_main_pass();
    gptStarter->end_frame(); 
}
