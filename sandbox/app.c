/*
   app.c
     - template app
     - loads only stable APIs
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] apis
// [SECTION] helper functions forward declarations
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
// [SECTION] helper functions implementations
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdlib.h> // malloc, free
#include <string.h> // memset
#include "pl.h"
#include "pl_memory.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"
#include "pl_icons.h"

// extensions
#include "pl_log_ext.h"
#include "pl_window_ext.h"
#include "pl_shader_ext.h"
#include "pl_draw_ext.h"
#include "pl_ui_ext.h"
#include "pl_graphics_ext.h"
#include "pl_draw_backend_ext.h"
#include "pl_profile_ext.h"
#include "pl_stats_ext.h"
#include "pl_job_ext.h"
#include "pl_string_intern_ext.h"
#include "pl_network_ext.h"
#include "pl_threads_ext.h"
#include "pl_atomics_ext.h"
#include "pl_library_ext.h"
#include "pl_file_ext.h"
#include "pl_rect_pack_ext.h"
#include "pl_gpu_allocators_ext.h"
#include "pl_image_ext.h"
#include "pl_virtual_memory_ext.h"
#include "pl_console_ext.h"
#include "pl_screen_log_ext.h"
#include "pl_tools_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAppData
{
    // window
    plWindow* ptWindow;

    // drawing stuff
    plDrawList2D*  ptDrawlist;
    plDrawLayer2D* ptFGLayer;
    plDrawLayer2D* ptBGLayer;
    plFont*        ptDefaultFont;

    // ui options
    bool  bShowUiDebug;
    bool  bShowUiStyle;
    bool* pbShowDeviceMemoryAnalyzer;
    bool* pbShowMemoryAllocations;
    bool* pbShowProfiling;
    bool* pbShowStats;
    bool* pbShowLogging;

    // graphics & sync objects
    plDevice*                ptDevice;
    plSurface*               ptSurface;
    plSwapchain*             ptSwapchain;
    plTimelineSemaphore*     aptSemaphores[PL_MAX_FRAMES_IN_FLIGHT];
    uint64_t                 aulNextTimelineValue[PL_MAX_FRAMES_IN_FLIGHT];
    plCommandPool*           atCmdPools[PL_MAX_FRAMES_IN_FLIGHT];
    plRenderPassHandle       tMainRenderPass;
    plRenderPassLayoutHandle tMainRenderPassLayout;

} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

const plDataRegistryI*   gptDataRegistry  = NULL;
const plMemoryI*         gptMemory        = NULL;
const plIOI*             gptIO            = NULL;
const plWindowI*         gptWindows       = NULL;
const plGraphicsI*       gptGfx           = NULL;
const plDrawI*           gptDraw          = NULL;
const plUiI*             gptUi            = NULL;
const plShaderI*         gptShader        = NULL;
const plDrawBackendI*    gptDrawBackend   = NULL;
const plProfileI*        gptProfile       = NULL;
const plStatsI*          gptStats         = NULL;
const plToolsI*          gptTools         = NULL;
const plImageI*          gptImage         = NULL;
const plGPUAllocatorsI*  gptGpuAllocators = NULL;
const plJobI*            gptJob           = NULL;
const plThreadsI*        gptThreads       = NULL;
const plAtomicsI*        gptAtomics       = NULL;
const plRectPackI*       gptRect          = NULL;
const plFileI*           gptFile          = NULL;
const plNetworkI*        gptNetwork       = NULL;
const plStringInternI*   gptString        = NULL;
const plLibraryI*        gptLibrary       = NULL;
const plLogI*            gptLog           = NULL;
const plVirtualMemoryI*  gptVirtualMemory = NULL;
const plConsoleI*        gptConsole       = NULL;
const plScreenLogI*      gptScreenLog     = NULL;

// helpers
#define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
#define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

#define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
#define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] helper functions forward declarations
//-----------------------------------------------------------------------------

void pl__setup_graphics_extensions (plAppData*);
void pl__resize_graphics_extensions(plAppData*);
void pl__resize_graphics_extensions(plAppData*);

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryI* ptApiRegistry, plAppData* ptAppData)
{
    // NOTE: on first load, "ptAppData" will be NULL but on reloads
    //       it will be the value returned from this function

    // retrieve the data registry API, this is the API used for sharing data
    // between extensions & the runtime
    gptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    // if "ptAppData" is a valid pointer, then this function is being called
    // during a hot reload.
    if(ptAppData)
    {

        // re-retrieve the apis since we are now in
        // a different dll/so
        gptMemory        = pl_get_api_latest(ptApiRegistry, plMemoryI);
        gptIO            = pl_get_api_latest(ptApiRegistry, plIOI);
        gptWindows       = pl_get_api_latest(ptApiRegistry, plWindowI);
        gptGfx           = pl_get_api_latest(ptApiRegistry, plGraphicsI);
        gptDraw          = pl_get_api_latest(ptApiRegistry, plDrawI);
        gptShader        = pl_get_api_latest(ptApiRegistry, plShaderI);
        gptDrawBackend   = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
        gptUi            = pl_get_api_latest(ptApiRegistry, plUiI);
        gptProfile       = pl_get_api_latest(ptApiRegistry, plProfileI);
        gptStats         = pl_get_api_latest(ptApiRegistry, plStatsI);
        gptTools         = pl_get_api_latest(ptApiRegistry, plToolsI);
        gptImage         = pl_get_api_latest(ptApiRegistry, plImageI);
        gptGpuAllocators = pl_get_api_latest(ptApiRegistry, plGPUAllocatorsI);
        gptJob           = pl_get_api_latest(ptApiRegistry, plJobI);
        gptThreads       = pl_get_api_latest(ptApiRegistry, plThreadsI);
        gptAtomics       = pl_get_api_latest(ptApiRegistry, plAtomicsI);
        gptRect          = pl_get_api_latest(ptApiRegistry, plRectPackI);
        gptFile          = pl_get_api_latest(ptApiRegistry, plFileI);
        gptNetwork       = pl_get_api_latest(ptApiRegistry, plNetworkI);
        gptString        = pl_get_api_latest(ptApiRegistry, plStringInternI);
        gptLibrary       = pl_get_api_latest(ptApiRegistry, plLibraryI);
        gptLog           = pl_get_api_latest(ptApiRegistry, plLogI);
        gptVirtualMemory = pl_get_api_latest(ptApiRegistry, plVirtualMemoryI);
        gptConsole       = pl_get_api_latest(ptApiRegistry, plConsoleI);
        gptScreenLog     = pl_get_api_latest(ptApiRegistry, plScreenLogI);

        gptScreenLog->add_message_ex(0, 15.0, PL_COLOR_32_MAGENTA, 1.5f, "%s", "App Hot Reloaded");

        return ptAppData;
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~apis & extensions~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    // retrieve extension registry
    const plExtensionRegistryI* ptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);

    // load extensions
    ptExtensionRegistry->load("pl_unity_ext", NULL, NULL, true);
    
    // load apis
    gptMemory        = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptIO            = pl_get_api_latest(ptApiRegistry, plIOI);
    gptWindows       = pl_get_api_latest(ptApiRegistry, plWindowI);
    gptGfx           = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptDraw          = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptShader        = pl_get_api_latest(ptApiRegistry, plShaderI);
    gptDrawBackend   = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
    gptUi            = pl_get_api_latest(ptApiRegistry, plUiI);
    gptProfile       = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptStats         = pl_get_api_latest(ptApiRegistry, plStatsI);
    gptTools         = pl_get_api_latest(ptApiRegistry, plToolsI);
    gptImage         = pl_get_api_latest(ptApiRegistry, plImageI);
    gptGpuAllocators = pl_get_api_latest(ptApiRegistry, plGPUAllocatorsI);
    gptJob           = pl_get_api_latest(ptApiRegistry, plJobI);
    gptThreads       = pl_get_api_latest(ptApiRegistry, plThreadsI);
    gptAtomics       = pl_get_api_latest(ptApiRegistry, plAtomicsI);
    gptRect          = pl_get_api_latest(ptApiRegistry, plRectPackI);
    gptFile          = pl_get_api_latest(ptApiRegistry, plFileI);
    gptNetwork       = pl_get_api_latest(ptApiRegistry, plNetworkI);
    gptString        = pl_get_api_latest(ptApiRegistry, plStringInternI);
    gptLibrary       = pl_get_api_latest(ptApiRegistry, plLibraryI);
    gptLog           = pl_get_api_latest(ptApiRegistry, plLogI);
    gptVirtualMemory = pl_get_api_latest(ptApiRegistry, plVirtualMemoryI);
    gptConsole       = pl_get_api_latest(ptApiRegistry, plConsoleI);
    gptScreenLog     = pl_get_api_latest(ptApiRegistry, plScreenLogI);

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = PL_ALLOC(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    // add console variables
    gptConsole->initialize((plConsoleSettings){.tFlags = PL_CONSOLE_FLAGS_POPUP});

    // use window API to create a window
    plWindowDesc tWindowDesc = {
        .pcTitle = "App Template",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 500,
        .uHeight = 500,
    };
    gptWindows->create_window(tWindowDesc, &ptAppData->ptWindow);

    // setup graphics extension
    pl__setup_graphics_extensions(ptAppData);

    // initialize APIs that require it
    gptTools->initialize((plToolsInit){.ptDevice = ptAppData->ptDevice});

    // retrieve some console variables
    ptAppData->pbShowLogging              = (bool*)gptConsole->get_variable("t.LogTool", NULL, NULL);
    ptAppData->pbShowStats                = (bool*)gptConsole->get_variable("t.StatTool", NULL, NULL);
    ptAppData->pbShowProfiling            = (bool*)gptConsole->get_variable("t.ProfileTool", NULL, NULL);
    ptAppData->pbShowMemoryAllocations    = (bool*)gptConsole->get_variable("t.MemoryAllocationTool", NULL, NULL);
    ptAppData->pbShowDeviceMemoryAnalyzer = (bool*)gptConsole->get_variable("t.DeviceMemoryAnalyzerTool", NULL, NULL);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~setup draw extensions~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // initialize
    gptDraw->initialize(NULL);
    gptDrawBackend->initialize(ptAppData->ptDevice);

    // create font atlas
    plFontAtlas* ptAtlas = gptDraw->create_font_atlas();
    gptDraw->set_font_atlas(ptAtlas);

    // create fonts
    plFontRange tFontRange = {
        .iFirstCodePoint = 0x0020,
        .uCharCount = 0x00FF - 0x0020
    };

    plFontConfig tFontConfig0 = {
        .bSdf = false,
        .fSize = 16.0f,
        .uHOverSampling = 1,
        .uVOverSampling = 1,
        .ptRanges = &tFontRange,
        .uRangeCount = 1
    };
    ptAppData->ptDefaultFont = gptDraw->add_font_from_file_ttf(ptAtlas, tFontConfig0, "../data/pilotlight-assets-master/fonts/Cousine-Regular.ttf");

    const plFontRange tIconRange = {
        .iFirstCodePoint = ICON_MIN_FA,
        .uCharCount = ICON_MAX_16_FA - ICON_MIN_FA
    };

    plFontConfig tFontConfig1 = {
        .bSdf           = false,
        .fSize          = 16.0f,
        .uHOverSampling = 1,
        .uVOverSampling = 1,
        .ptMergeFont    = ptAppData->ptDefaultFont,
        .ptRanges       = &tIconRange,
        .uRangeCount    = 1
    };
    gptDraw->add_font_from_file_ttf(ptAtlas, tFontConfig1, "../data/pilotlight-assets-master/fonts/fa-solid-900.otf");

    // build font atlas
    plCommandBuffer* ptCmdBuffer = gptGfx->request_command_buffer(ptAppData->atCmdPools[0]);
    gptDrawBackend->build_font_atlas(ptCmdBuffer, ptAtlas);
    gptGfx->return_command_buffer(ptCmdBuffer);
    
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~message extension~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    gptScreenLog->initialize((plScreenLogSettings){.ptFont = ptAppData->ptDefaultFont});

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ui extension~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    gptUi->initialize();
    gptUi->set_default_font(ptAppData->ptDefaultFont);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~app stuff~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // create drawlist and some layers to draw to
    ptAppData->ptDrawlist = gptDraw->request_2d_drawlist();
    ptAppData->ptFGLayer = gptDraw->request_2d_layer(ptAppData->ptDrawlist);
    ptAppData->ptBGLayer = gptDraw->request_2d_layer(ptAppData->ptDrawlist);

    // return app memory
    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    // ensure GPU is finished before cleanup
    gptGfx->flush_device(ptAppData->ptDevice);
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        gptGfx->cleanup_command_pool(ptAppData->atCmdPools[i]);
        gptGfx->cleanup_semaphore(ptAppData->aptSemaphores[i]);
    }
    gptDrawBackend->cleanup_font_atlas(NULL);
    gptUi->cleanup();
    gptDrawBackend->cleanup();
    gptScreenLog->cleanup();
    gptConsole->cleanup();
    gptGfx->cleanup_swapchain(ptAppData->ptSwapchain);
    gptGfx->cleanup_surface(ptAppData->ptSurface);
    gptGfx->cleanup_device(ptAppData->ptDevice);
    gptGfx->cleanup();
    gptWindows->destroy_window(ptAppData->ptWindow);
    PL_FREE(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plAppData* ptAppData)
{
    pl__resize_graphics_extensions(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    gptProfile->begin_frame();
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    // for convience
    plIO* ptIO = gptIO->get_io();

    // new frame stuff
    gptIO->new_frame();
    gptDrawBackend->new_frame();
    gptUi->new_frame();
    gptStats->new_frame();
    gptGfx->begin_frame(ptAppData->ptDevice);
    plCommandPool* ptCmdPool = ptAppData->atCmdPools[gptGfx->get_current_frame_index()];
    gptGfx->reset_command_pool(ptCmdPool, 0);

    // update statistics
    static double* pdFrameTimeCounter = NULL;
    if(!pdFrameTimeCounter)
        pdFrameTimeCounter = gptStats->get_counter("frametime (ms)");
    *pdFrameTimeCounter = (double)ptIO->fDeltaTime * 1000.0;

    // acquire swapchain image
    if(!gptGfx->acquire_swapchain_image(ptAppData->ptSwapchain))
    {
        pl_app_resize(ptAppData);
        pl_end_cpu_sample(gptProfile, 0);
        gptProfile->end_frame();
        return;
    }

    // just some drawing
    gptDraw->add_circle(ptAppData->ptFGLayer, (plVec2){100.0f, 100.0f}, 50.0f, 12, (plDrawLineOptions){.fThickness = 2.0f, .uColor = PL_COLOR_32_RGBA(1.0f, 0.0f, 1.0f, 1.0f)});

    if(gptIO->is_key_pressed(PL_KEY_F1, false))
        gptConsole->open();

    gptConsole->update();

    if(gptUi->begin_window("Pilot Light", NULL, false))
    {

        const float pfRatios[] = {1.0f};
        gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
        if(gptUi->begin_collapsing_header(ICON_FA_CIRCLE_INFO " Information", 0))
        {
            gptUi->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
            gptUi->text("Graphics Backend: %s", gptGfx->get_backend_string());
            gptUi->end_collapsing_header();
        }
        if(gptUi->begin_collapsing_header(ICON_FA_SCREWDRIVER_WRENCH " Tools", 0))
        {
            gptUi->checkbox("Device Memory Analyzer", ptAppData->pbShowDeviceMemoryAnalyzer);
            gptUi->checkbox("Memory Allocations", ptAppData->pbShowMemoryAllocations);
            gptUi->checkbox("Profiling", ptAppData->pbShowProfiling);
            gptUi->checkbox("Statistics", ptAppData->pbShowStats);
            gptUi->checkbox("Logging", ptAppData->pbShowLogging);
            gptUi->end_collapsing_header();
        }
        if(gptUi->begin_collapsing_header(ICON_FA_USER_GEAR " User Interface", 0))
        {
            gptUi->checkbox("UI Debug", &ptAppData->bShowUiDebug);
            gptUi->checkbox("UI Style", &ptAppData->bShowUiStyle);
            gptUi->end_collapsing_header();
        }
        gptUi->end_window();
    }
        
    if(ptAppData->bShowUiStyle)
        gptUi->show_style_editor_window(&ptAppData->bShowUiStyle);

    if(ptAppData->bShowUiDebug)
        gptUi->show_debug_window(&ptAppData->bShowUiDebug);

    gptTools->update();

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~graphics work~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const uint32_t uCurrentFrameIndex = gptGfx->get_current_frame_index();

    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);

    const plBeginCommandInfo tBeginInfo = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {ptAppData->aptSemaphores[uCurrentFrameIndex]},
        .auWaitSemaphoreValues = {ptAppData->aulNextTimelineValue[uCurrentFrameIndex]},
    };
    gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo);

    // begin main renderpass (directly to swapchain)
    plRenderEncoder* ptEncoder = gptGfx->begin_render_pass(ptCommandBuffer, ptAppData->tMainRenderPass, NULL);

    // submit our layers & drawlist
    gptDraw->submit_2d_layer(ptAppData->ptBGLayer);
    gptDraw->submit_2d_layer(ptAppData->ptFGLayer);
    gptDrawBackend->submit_2d_drawlist(ptAppData->ptDrawlist, ptEncoder, ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y, gptGfx->get_swapchain_info(ptAppData->ptSwapchain).tSampleCount);
    
    // submits UI layers
    gptUi->end_frame();

    // submit UI drawlists
    gptDrawBackend->submit_2d_drawlist(gptUi->get_draw_list(), ptEncoder, ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y, gptGfx->get_swapchain_info(ptAppData->ptSwapchain).tSampleCount);
    gptDrawBackend->submit_2d_drawlist(gptUi->get_debug_draw_list(), ptEncoder, ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y, gptGfx->get_swapchain_info(ptAppData->ptSwapchain).tSampleCount);

    plDrawList2D* ptMessageDrawlist = gptScreenLog->get_drawlist(ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y);
    gptDrawBackend->submit_2d_drawlist(ptMessageDrawlist, ptEncoder, ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y, gptGfx->get_swapchain_info(ptAppData->ptSwapchain).tSampleCount);


    // end render pass
    gptGfx->end_render_pass(ptEncoder);

    // end recording
    gptGfx->end_command_recording(ptCommandBuffer);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~submit work to GPU & present~~~~~~~~~~~~~~~~~~~~~~~

    const plSubmitInfo tSubmitInfo = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {ptAppData->aptSemaphores[uCurrentFrameIndex]},
        .auSignalSemaphoreValues = {++ptAppData->aulNextTimelineValue[uCurrentFrameIndex]},
    };

    if(!gptGfx->present(ptCommandBuffer, &tSubmitInfo, &ptAppData->ptSwapchain, 1))
        pl_app_resize(ptAppData);

    gptGfx->return_command_buffer(ptCommandBuffer);

    pl_end_cpu_sample(gptProfile, 0);
    gptProfile->end_frame();
}

//-----------------------------------------------------------------------------
// [SECTION] helper functions implementations
//-----------------------------------------------------------------------------

void
pl__setup_graphics_extensions(plAppData* ptAppData)
{
    // initialize shader extension (shader compiler)
    static const plShaderOptions tDefaultShaderOptions = {
        .apcIncludeDirectories = {
            "../shaders/"
        },
        .apcDirectories = {
            "../shaders/"
        },
        .tFlags = PL_SHADER_FLAGS_AUTO_OUTPUT
    };
    gptShader->initialize(&tDefaultShaderOptions);

    // initialize graphics system
    const plGraphicsInit tGraphicsInit = {
        .tFlags = PL_GRAPHICS_INIT_FLAGS_VALIDATION_ENABLED | PL_GRAPHICS_INIT_FLAGS_SWAPCHAIN_ENABLED 
    };
    gptGfx->initialize(&tGraphicsInit);
    ptAppData->ptSurface = gptGfx->create_surface(ptAppData->ptWindow);

    // find suitable device
    uint32_t uDeviceCount = 16;
    plDeviceInfo atDeviceInfos[16] = {0};
    gptGfx->enumerate_devices(atDeviceInfos, &uDeviceCount);

    // we will prefer discrete, then integrated
    int iBestDvcIdx = 0;
    int iDiscreteGPUIdx   = -1;
    int iIntegratedGPUIdx = -1;
    for(uint32_t i = 0; i < uDeviceCount; i++)
    {
        
        if(atDeviceInfos[i].tType == PL_DEVICE_TYPE_DISCRETE)
            iDiscreteGPUIdx = i;
        else if(atDeviceInfos[i].tType == PL_DEVICE_TYPE_INTEGRATED)
            iIntegratedGPUIdx = i;
    }

    if(iDiscreteGPUIdx > -1)
        iBestDvcIdx = iDiscreteGPUIdx;
    else if(iIntegratedGPUIdx > -1)
        iBestDvcIdx = iIntegratedGPUIdx;

    // create device
    const plDeviceInit tDeviceInit = {
        .uDeviceIdx = iBestDvcIdx,
        .ptSurface = ptAppData->ptSurface
    };
    ptAppData->ptDevice = gptGfx->create_device(&tDeviceInit);

    gptDataRegistry->set_data("device", ptAppData->ptDevice); // used by debug extension

    // create command pools
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        ptAppData->atCmdPools[i] = gptGfx->create_command_pool(ptAppData->ptDevice, NULL);

    // create swapchain
    plSwapchainInit tSwapInit = {
        .tSampleCount = PL_SAMPLE_COUNT_1
    };
    ptAppData->ptSwapchain = gptGfx->create_swapchain(ptAppData->ptDevice, ptAppData->ptSurface, &tSwapInit);

    uint32_t uImageCount = 0;
    plTextureHandle* atSwapchainImages = gptGfx->get_swapchain_images(ptAppData->ptSwapchain, &uImageCount);

    // create main render pass layout
    const plRenderPassLayoutDesc tMainRenderPassLayoutDesc = {
        .atRenderTargets = {
            { .tFormat = gptGfx->get_swapchain_info(ptAppData->ptSwapchain).tFormat }, // swapchain
        },
        .atSubpasses = {
            {
                .uRenderTargetCount = 1,
                .auRenderTargets = {0}
            }
        },
        .atSubpassDependencies = {
            {
                .uSourceSubpass = UINT32_MAX,
                .uDestinationSubpass = 0,
                .tSourceStageMask = PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS | PL_PIPELINE_STAGE_COMPUTE_SHADER,
                .tDestinationStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tSourceAccessMask = PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
                .tDestinationAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
            },
            {
                .uSourceSubpass = 0,
                .uDestinationSubpass = UINT32_MAX,
                .tSourceStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tDestinationStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS | PL_PIPELINE_STAGE_COMPUTE_SHADER,
                .tSourceAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
                .tDestinationAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
            },
        }
    };
    ptAppData->tMainRenderPassLayout = gptGfx->create_render_pass_layout(ptAppData->ptDevice, &tMainRenderPassLayoutDesc);

    // create main render pass
    const plRenderPassDesc tMainRenderPassDesc = {
        .tLayout = ptAppData->tMainRenderPassLayout,
        .atColorTargets = {
            {
                .tLoadOp       = PL_LOAD_OP_CLEAR,
                .tStoreOp      = PL_STORE_OP_STORE,
                .tCurrentUsage = PL_TEXTURE_USAGE_UNSPECIFIED,
                .tNextUsage    = PL_TEXTURE_USAGE_PRESENT,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
            }
        },
        .tDimensions = {(float)gptGfx->get_swapchain_info(ptAppData->ptSwapchain).uWidth, (float)gptGfx->get_swapchain_info(ptAppData->ptSwapchain).uHeight},
        .ptSwapchain = ptAppData->ptSwapchain
    };

    plRenderPassAttachments atMainAttachmentSets[16] = {0};
    for(uint32_t i = 0; i < uImageCount; i++)
    {
        atMainAttachmentSets[i].atViewAttachments[0] = atSwapchainImages[i];
    }
    ptAppData->tMainRenderPass = gptGfx->create_render_pass(ptAppData->ptDevice, &tMainRenderPassDesc, atMainAttachmentSets);

    // create timeline semaphores to syncronize GPU work submission
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        ptAppData->aptSemaphores[i] = gptGfx->create_semaphore(ptAppData->ptDevice, false);
}

void
pl__resize_graphics_extensions(plAppData* ptAppData)
{
    plIO* ptIO = gptIO->get_io();
    plSwapchainInit tDesc = {
        .bVSync  = true,
        .uWidth  = (uint32_t)ptIO->tMainViewportSize.x,
        .uHeight = (uint32_t)ptIO->tMainViewportSize.y,
        .tSampleCount = gptGfx->get_swapchain_info(ptAppData->ptSwapchain).tSampleCount,
    };
    gptGfx->recreate_swapchain(ptAppData->ptSwapchain, &tDesc);

    uint32_t uImageCount = 0;
    plTextureHandle* atSwapchainImages = gptGfx->get_swapchain_images(ptAppData->ptSwapchain, &uImageCount);

    plRenderPassAttachments atMainAttachmentSets[16] = {0};
    for(uint32_t i = 0; i < uImageCount; i++)
    {
        atMainAttachmentSets[i].atViewAttachments[0] = atSwapchainImages[i];
    }
    gptGfx->update_render_pass_attachments(ptAppData->ptDevice, ptAppData->tMainRenderPass, gptIO->get_io()->tMainViewportSize, atMainAttachmentSets);
}