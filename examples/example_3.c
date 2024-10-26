/*
   example_3.c
     - demonstrates loading APIs
     - demonstrates loading extensions
     - demonstrates hot reloading
     - demonstrates minimal use of graphics extension
     - demonstrates ui extension
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
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdlib.h> // malloc, free
#include <stdio.h>
#include <string.h> // memset
#include "pl.h"
#include "pl_profile.h"
#include "pl_log.h"
#include "pl_os.h"
#include "pl_memory.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extensions
#include "pl_shader_ext.h"
#include "pl_draw_ext.h"
#include "pl_ui_ext.h" // not yet stable
#include "pl_graphics_ext.h" // not yet stable
#include "pl_draw_backend_ext.h" // not yet stable

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAppData
{
    // window
    plWindow* ptWindow;

    // ui options
    bool bShowUiDemo;
    bool bShowUiDebug;
    bool bShowUiStyle;

    // drawing

    // graphics & sync objects
    plDevice*         ptDevice;
    plSurface*        ptSurface;
    plSwapchain*      ptSwapchain;
    plSemaphoreHandle atSempahore[PL_MAX_FRAMES_IN_FLIGHT];
    uint64_t          aulNextTimelineValue[PL_MAX_FRAMES_IN_FLIGHT];
    plCommandPool*    atCmdPools[PL_MAX_FRAMES_IN_FLIGHT];

} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

const plIOI*          gptIO          = NULL;
const plWindowI*      gptWindows     = NULL;
const plGraphicsI*    gptGfx         = NULL;
const plDrawI*        gptDraw        = NULL;
const plUiI*          gptUi          = NULL;
const plShaderI*      gptShader      = NULL;
const plDrawBackendI* gptDrawBackend = NULL; // not yet stable

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
    const plDataRegistryI* ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);

    // set log & profile contexts
    pl_set_log_context(ptDataRegistry->get_data("log"));
    pl_set_profile_context(ptDataRegistry->get_data("profile"));

    // if "ptAppData" is a valid pointer, then this function is being called
    // during a hot reload.
    if(ptAppData)
    {

        // re-retrieve the apis since we are now in
        // a different dll/so
        gptIO          = ptApiRegistry->first(PL_API_IO);
        gptWindows     = ptApiRegistry->first(PL_API_WINDOW);
        gptGfx         = ptApiRegistry->first(PL_API_GRAPHICS);
        gptDraw        = ptApiRegistry->first(PL_API_DRAW);
        gptUi          = ptApiRegistry->first(PL_API_UI);
        gptShader      = ptApiRegistry->first(PL_API_SHADER);
        gptDrawBackend = ptApiRegistry->first(PL_API_DRAW_BACKEND);

        return ptAppData;
    }

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = malloc(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    // retrieve extension registry
    const plExtensionRegistryI* ptExtensionRegistry = ptApiRegistry->first(PL_API_EXTENSION_REGISTRY);

    // load extensions
    ptExtensionRegistry->load("pilot_light", NULL, NULL, true);
    
    // load required apis (NULL if not available)
    gptIO          = ptApiRegistry->first(PL_API_IO);
    gptWindows     = ptApiRegistry->first(PL_API_WINDOW);
    gptGfx         = ptApiRegistry->first(PL_API_GRAPHICS);
    gptDraw        = ptApiRegistry->first(PL_API_DRAW);
    gptUi          = ptApiRegistry->first(PL_API_UI);
    gptShader      = ptApiRegistry->first(PL_API_SHADER);
    gptDrawBackend = ptApiRegistry->first(PL_API_DRAW_BACKEND);

    // initialize shader compiler
    static const plShaderOptions tDefaultShaderOptions = {
        .uIncludeDirectoriesCount = 1,
        .apcIncludeDirectories = {
            "../shaders/"
        }
    };
    gptShader->initialize(&tDefaultShaderOptions);

    // use window API to create a window
    const plWindowDesc tWindowDesc = {
        .pcName  = "Example 3",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 600,
        .uHeight = 600,
    };
    gptWindows->create_window(&tWindowDesc, &ptAppData->ptWindow);

    // initialize graphics system
    const plGraphicsInit tGraphicsInit = {
        .tFlags = PL_GRAPHICS_INIT_FLAGS_VALIDATION_ENABLED | PL_GRAPHICS_INIT_FLAGS_SWAPCHAIN_ENABLED | PL_GRAPHICS_INIT_FLAGS_LOGGING_WARNING 
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
    atDeviceInfos[iBestDvcIdx].ptSurface = ptAppData->ptSurface;
    ptAppData->ptDevice = gptGfx->create_device(&atDeviceInfos[iBestDvcIdx]);

    // create swapchain
    const plSwapchainInit tSwapInit = {
        .ptSurface = ptAppData->ptSurface
    };
    ptAppData->ptSwapchain = gptGfx->create_swapchain(ptAppData->ptDevice, &tSwapInit);

    // create command pools
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        ptAppData->atCmdPools[i] = gptGfx->create_command_pool(ptAppData->ptDevice, NULL);

    // setup draw
    gptDraw->initialize(NULL);
    gptDrawBackend->initialize(ptAppData->ptDevice);
    plFontAtlas* ptAtlas = gptDraw->create_font_atlas();
    plFont* ptDefaultFont = gptDraw->add_default_font(ptAtlas);
    plCommandPool* ptCmdPool = ptAppData->atCmdPools[gptGfx->get_current_frame_index()];
    plCommandBuffer* ptCmdBuffer = gptGfx->request_command_buffer(ptCmdPool);
    gptDrawBackend->build_font_atlas(ptCmdBuffer, ptAtlas);
    gptGfx->return_command_buffer(ptCmdBuffer);
    gptDraw->set_font_atlas(ptAtlas);

    // setup ui
    gptUi->initialize();
    gptUi->set_default_font(ptDefaultFont);

    // create timeline semaphores to syncronize GPU work submission
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        ptAppData->atSempahore[i] = gptGfx->create_semaphore(ptAppData->ptDevice, false);

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
        gptGfx->cleanup_command_pool(ptAppData->atCmdPools[i]);
    gptDrawBackend->cleanup_font_atlas(NULL);
    gptUi->cleanup();
    gptDrawBackend->cleanup();
    gptGfx->cleanup_swapchain(ptAppData->ptSwapchain);
    gptGfx->cleanup_surface(ptAppData->ptSurface);
    gptGfx->cleanup_device(ptAppData->ptDevice);
    gptGfx->cleanup();
    gptWindows->destroy_window(ptAppData->ptWindow);
    pl_cleanup_profile_context();
    pl_cleanup_log_context();
    free(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plAppData* ptAppData)
{
    // perform any operations required during a window resize
    gptGfx->resize(ptAppData->ptSwapchain); // recreates swapchain
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    pl_begin_profile_frame();

    gptIO->new_frame();
    gptDrawBackend->new_frame();
    gptUi->new_frame();

    // begin new frame
    if(!gptGfx->begin_frame(ptAppData->ptSwapchain))
    {
        gptGfx->resize(ptAppData->ptSwapchain);
        pl_end_profile_frame();
        return;
    }

    plCommandPool* ptCmdPool = ptAppData->atCmdPools[gptGfx->get_current_frame_index()];
    gptGfx->reset_command_pool(ptCmdPool);
    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);

    // NOTE: UI code can be placed anywhere between the UI "new_frame" & "render"

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~UI~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    if(gptUi->begin_window("Pilot Light", NULL, false))
    {

        const float pfRatios[] = {1.0f};
        gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
        if(gptUi->begin_collapsing_header("Information", 0))
        {
            
            gptUi->text("Pilot Light %s", PILOT_LIGHT_VERSION);
            #ifdef PL_METAL_BACKEND
            gptUi->text("Graphics Backend: Metal");
            #elif PL_VULKAN_BACKEND
            gptUi->text("Graphics Backend: Vulkan");
            #else
            gptUi->text("Graphics Backend: Unknown");
            #endif

            gptUi->end_collapsing_header();
        }
        if(gptUi->begin_collapsing_header("User Interface", 0))
        {
            gptUi->checkbox("UI Debug", &ptAppData->bShowUiDebug);
            gptUi->checkbox("UI Demo", &ptAppData->bShowUiDemo);
            gptUi->checkbox("UI Style", &ptAppData->bShowUiStyle);
            gptUi->end_collapsing_header();
        }
        gptUi->end_window();
    }

    if(ptAppData->bShowUiDemo)
        gptUi->show_demo_window(&ptAppData->bShowUiDemo);
        
    if(ptAppData->bShowUiStyle)
        gptUi->show_style_editor_window(&ptAppData->bShowUiStyle);

    if(ptAppData->bShowUiDebug)
        gptUi->show_debug_window(&ptAppData->bShowUiDebug);

    //~~~~~~~~~~~~~~~~~~~~~~~~begin recording command buffer~~~~~~~~~~~~~~~~~~~~~~~

    // expected timeline semaphore values
    const uint32_t uCurrentFrameIndex = gptGfx->get_current_frame_index();
    uint64_t ulValue0 = ptAppData->aulNextTimelineValue[uCurrentFrameIndex];
    uint64_t ulValue1 = ulValue0 + 1;
    ptAppData->aulNextTimelineValue[uCurrentFrameIndex] = ulValue1;

    const plBeginCommandInfo tBeginInfo = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {ptAppData->atSempahore[uCurrentFrameIndex]},
        .auWaitSemaphoreValues = {ulValue0},
    };
    gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo);

    // begin main renderpass (directly to swapchain)
    plRenderEncoder* ptEncoder = gptGfx->begin_render_pass(ptCommandBuffer, gptGfx->get_main_render_pass(ptAppData->ptDevice));

    // submits UI drawlist/layers
    plIO* ptIO = gptIO->get_io();
    gptUi->end_frame();
    gptDrawBackend->submit_2d_drawlist(gptUi->get_draw_list(), ptEncoder, ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y, 1);
    gptDrawBackend->submit_2d_drawlist(gptUi->get_debug_draw_list(), ptEncoder, ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y, 1);

    // end render pass
    gptGfx->end_render_pass(ptEncoder);

    // end recording
    gptGfx->end_command_recording(ptCommandBuffer);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~submit work to GPU & present~~~~~~~~~~~~~~~~~~~~~~~

    const plSubmitInfo tSubmitInfo = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {ptAppData->atSempahore[uCurrentFrameIndex]},
        .auSignalSemaphoreValues = {ulValue1},
    };

    if(!gptGfx->present(ptCommandBuffer, &tSubmitInfo, ptAppData->ptSwapchain))
        gptGfx->resize(ptAppData->ptSwapchain);

    gptGfx->return_command_buffer(ptCommandBuffer);
    pl_end_profile_frame();
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#define PL_LOG_IMPLEMENTATION
#include "pl_log.h"
#undef PL_LOG_IMPLEMENTATION

#define PL_PROFILE_IMPLEMENTATION
#include "pl_profile.h"
#undef PL_PROFILE_IMPLEMENTATION