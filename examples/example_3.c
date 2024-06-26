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
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdio.h>
#include "pilotlight.h"
#include "pl_profile.h"
#include "pl_log.h"
#include "pl_ds.h"
#include "pl_os.h"
#include "pl_memory.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extensions
#include "pl_graphics_ext.h"
#include "pl_draw_ext.h"
#include "pl_ui_ext.h"

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
    plGraphics        tGraphics;
    plSemaphoreHandle atSempahore[PL_FRAMES_IN_FLIGHT];
    uint64_t          aulNextTimelineValue[PL_FRAMES_IN_FLIGHT];

} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

const plIOI*       gptIO      = NULL;
const plWindowI*   gptWindows = NULL;
const plGraphicsI* gptGfx     = NULL;
const plDeviceI*   gptDevice  = NULL;
const plDrawI*     gptDraw    = NULL;
const plUiI*       gptUi      = NULL;

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

    // retrieve the memory context (provided by the runtime) and
    // set it to allow for memory tracking when using PL_ALLOC/PL_FREE
    pl_set_memory_context(ptDataRegistry->get_data(PL_CONTEXT_MEMORY));

    // if "ptAppData" is a valid pointer, then this function is being called
    // during a hot reload.
    if(ptAppData)
    {
        // set contexts again since we are now in a
        // differenct dll/so
        pl_set_log_context(ptDataRegistry->get_data("log"));
        pl_set_profile_context(ptDataRegistry->get_data("profile"));

        // re-retrieve the apis since we are now in
        // a different dll/so
        gptIO      = ptApiRegistry->first(PL_API_IO);
        gptWindows = ptApiRegistry->first(PL_API_WINDOW);
        gptGfx     = ptApiRegistry->first(PL_API_GRAPHICS);
        gptDevice  = ptApiRegistry->first(PL_API_DEVICE);
        gptDraw    = ptApiRegistry->first(PL_API_DRAW);
        gptUi      = ptApiRegistry->first(PL_API_UI);

        return ptAppData;
    }

    // this path is taken only during first load, so we
    // allocate app memory here (using PL_ALLOC for memory tracking)
    ptAppData = PL_ALLOC(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    // create profiling & logging contexts (used by extension here)
    plProfileContext* ptProfileCtx = pl_create_profile_context();
    plLogContext*     ptLogCtx     = pl_create_log_context();

    // add log channel (ignoring the return here)
    pl_add_log_channel("Default", PL_CHANNEL_TYPE_CONSOLE);
    pl_log_info("Setup logging");
    
    // add these to data registry so they can be retrieved by extension
    // and subsequent app reloads
    ptDataRegistry->set_data("profile", ptProfileCtx);
    ptDataRegistry->set_data("log", ptLogCtx);

    // retrieve extension registry
    const plExtensionRegistryI* ptExtensionRegistry = ptApiRegistry->first(PL_API_EXTENSION_REGISTRY);

    // load graphics extension (provides graphics & device apis)
    ptExtensionRegistry->load("pl_graphics_ext", NULL, NULL, false);
    ptExtensionRegistry->load("pl_draw_ext",     NULL, NULL, true);
    ptExtensionRegistry->load("pl_ui_ext",       NULL, NULL, true);
    
    // load required apis (NULL if not available)
    gptIO      = ptApiRegistry->first(PL_API_IO);
    gptWindows = ptApiRegistry->first(PL_API_WINDOW);
    gptGfx     = ptApiRegistry->first(PL_API_GRAPHICS);
    gptDevice  = ptApiRegistry->first(PL_API_DEVICE);
    gptDraw    = ptApiRegistry->first(PL_API_DRAW);
    gptUi      = ptApiRegistry->first(PL_API_UI);

    // use window API to create a window
    const plWindowDesc tWindowDesc = {
        .pcName  = "Example 3",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 600,
        .uHeight = 600,
    };
    ptAppData->ptWindow = gptWindows->create_window(&tWindowDesc);

    // initialize graphics system
    const plGraphicsDesc tGraphicsDesc = {
        .bEnableValidation = true
    };
    gptGfx->initialize(ptAppData->ptWindow, &tGraphicsDesc, &ptAppData->tGraphics);

    // setup draw
    gptDraw->initialize(&ptAppData->tGraphics);
    plFontHandle tDefaultFont = gptDraw->add_default_font();
    gptDraw->build_font_atlas();

    // setup ui
    gptUi->initialize();
    gptUi->set_default_font(tDefaultFont);

    // create timeline semaphores to syncronize GPU work submission
    for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
        ptAppData->atSempahore[i] = gptDevice->create_semaphore(&ptAppData->tGraphics.tDevice, false);

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
    gptDevice->flush_device(&ptAppData->tGraphics.tDevice);
    gptDraw->cleanup_font_atlas();
    gptUi->cleanup();
    gptDraw->cleanup();
    gptGfx->cleanup(&ptAppData->tGraphics);
    gptWindows->destroy_window(ptAppData->ptWindow);
    pl_cleanup_profile_context();
    pl_cleanup_log_context();
    PL_FREE(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plAppData* ptAppData)
{
    // perform any operations required during a window resize
    gptGfx->resize(&ptAppData->tGraphics); // recreates swapchain
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    pl_begin_profile_frame();

    gptIO->new_frame();
    gptDraw->new_frame();
    gptUi->new_frame();

    // for convience
    plGraphics* ptGraphics = &ptAppData->tGraphics;

    // begin new frame
    if(!gptGfx->begin_frame(ptGraphics))
    {
        gptGfx->resize(ptGraphics);
        pl_end_profile_frame();
        return;
    }

    // NOTE: UI code can be placed anywhere between the UI "new_frame" & "render"

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~UI~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    if(gptUi->begin_window("Pilot Light", NULL, false))
    {

        const float pfRatios[] = {1.0f};
        gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
        if(gptUi->collapsing_header("Information"))
        {
            
            gptUi->text("Pilot Light %s", PILOTLIGHT_VERSION);
            gptUi->text("Pilot Light UI %s", PL_UI_EXT_VERSION);
            gptUi->text("Pilot Light DS %s", PL_DS_VERSION);
            #ifdef PL_METAL_BACKEND
            gptUi->text("Graphics Backend: Metal");
            #elif PL_VULKAN_BACKEND
            gptUi->text("Graphics Backend: Vulkan");
            #else
            gptUi->text("Graphics Backend: Unknown");
            #endif

            gptUi->end_collapsing_header();
        }
        if(gptUi->collapsing_header("User Interface"))
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
    uint64_t ulValue0 = ptAppData->aulNextTimelineValue[ptGraphics->uCurrentFrameIndex];
    uint64_t ulValue1 = ulValue0 + 1;
    ptAppData->aulNextTimelineValue[ptGraphics->uCurrentFrameIndex] = ulValue1;

    const plBeginCommandInfo tBeginInfo = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {ptAppData->atSempahore[ptGraphics->uCurrentFrameIndex]},
        .auWaitSemaphoreValues = {ulValue0},
    };
    plCommandBuffer tCommandBuffer = gptGfx->begin_command_recording(ptGraphics, &tBeginInfo);

    // begin main renderpass (directly to swapchain)
    plRenderEncoder tEncoder = gptGfx->begin_render_pass(ptGraphics, &tCommandBuffer, ptGraphics->tMainRenderPass);

    // submits UI drawlist/layers
    plIO* ptIO = gptIO->get_io();
    gptUi->render(tEncoder, ptIO->afMainViewportSize[0], ptIO->afMainViewportSize[1], 1);

    // end render pass
    gptGfx->end_render_pass(&tEncoder);

    // end recording
    gptGfx->end_command_recording(ptGraphics, &tCommandBuffer);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~submit work to GPU & present~~~~~~~~~~~~~~~~~~~~~~~

    const plSubmitInfo tSubmitInfo = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {ptAppData->atSempahore[ptGraphics->uCurrentFrameIndex]},
        .auSignalSemaphoreValues = {ulValue1},
    };

    if(!gptGfx->present(ptGraphics, &tCommandBuffer, &tSubmitInfo))
        gptGfx->resize(ptGraphics);

    pl_end_profile_frame();
}