/*
   example_2.c
     - demonstrates loading APIs
     - demonstrates loading extensions
     - demonstrates hot reloading
     - demonstrates minimal use of graphics extension
     - demonstrates drawing extension (2D)
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

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAppData
{
    // window
    plWindow* ptWindow;

    // drawing
    plDrawList2D*  ptDrawlist;
    plDrawLayer2D* ptFGLayer;
    plDrawLayer2D* ptBGLayer;
    plFontHandle tDefaultFont;
    plFontHandle tCousineBitmapFont;
    plFontHandle tCousineSDFFont;

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
    
    // load required apis (NULL if not available)
    gptIO      = ptApiRegistry->first(PL_API_IO);
    gptWindows = ptApiRegistry->first(PL_API_WINDOW);
    gptGfx     = ptApiRegistry->first(PL_API_GRAPHICS);
    gptDevice  = ptApiRegistry->first(PL_API_DEVICE);
    gptDraw    = ptApiRegistry->first(PL_API_DRAW);

    // use window API to create a window
    const plWindowDesc tWindowDesc = {
        .pcName  = "Example 2",
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

    // builtin default font (proggy @ 13)
    gptDraw->add_default_font();

    // typical font range (you can also add individual characters)
    const plFontRange tRange = {
        .iFirstCodePoint = 0x0020,
        .uCharCount = 0x00FF - 0x0020
    };

    // adding another font
    plFontConfig tFontConfig0 = {
        .bSdf = false,
        .fFontSize = 18.0f,
        .uHOverSampling = 1,
        .uVOverSampling = 1,
    };
    pl_sb_push(tFontConfig0.sbtRanges, tRange);
    ptAppData->tCousineBitmapFont = gptDraw->add_font_from_file_ttf(tFontConfig0, "../data/pilotlight-assets-master/fonts/Cousine-Regular.ttf");

    // adding previous font but as a signed distance field
    plFontConfig tFontConfig1 = {
        .bSdf = true, // only works with ttf
        .fFontSize = 18.0f,
        .uHOverSampling = 1,
        .uVOverSampling = 1,
        .ucOnEdgeValue = 180,
        .iSdfPadding = 1
    };
    pl_sb_push(tFontConfig1.sbtRanges, tRange);
    ptAppData->tCousineSDFFont = gptDraw->add_font_from_file_ttf(tFontConfig1, "../data/pilotlight-assets-master/fonts/Cousine-Regular.ttf");

    // build font atlass
    gptDraw->build_font_atlas();

    // register our app drawlist
    ptAppData->ptDrawlist = gptDraw->request_2d_drawlist();
    ptAppData->ptFGLayer = gptDraw->request_2d_layer(ptAppData->ptDrawlist, "foreground layer");
    ptAppData->ptBGLayer = gptDraw->request_2d_layer(ptAppData->ptDrawlist, "background layer");

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

    // for convience
    plGraphics* ptGraphics = &ptAppData->tGraphics;

    // begin new frame
    if(!gptGfx->begin_frame(ptGraphics))
    {
        gptGfx->resize(ptGraphics);
        pl_end_profile_frame();
        return;
    }

    // drawing API usage
    gptDraw->add_circle(ptAppData->ptFGLayer, (plVec2){120.0f, 120.0f}, 50.0f, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 0, 1.0f);
    gptDraw->add_circle_filled(ptAppData->ptBGLayer, (plVec2){100.0f, 100.0f}, 25.0f, (plVec4){1.0f, 0.0f, 1.0f, 1.0f}, 24);
    gptDraw->add_text(ptAppData->ptFGLayer, ptAppData->tDefaultFont, 13.0f, (plVec2){200.0f, 100.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, "Proggy @ 13 (loaded at 13)", 0.0f);
    gptDraw->add_text(ptAppData->ptFGLayer, ptAppData->tDefaultFont, 45.0f, (plVec2){200.0f, 115.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, "Proggy @ 45 (loaded at 13)", 0.0f);

    gptDraw->add_text(ptAppData->ptFGLayer, ptAppData->tCousineBitmapFont, 18.0f, (plVec2){25.0f, 200.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, "Cousine @ 18, bitmap (loaded at 18)", 0.0f);
    gptDraw->add_text(ptAppData->ptFGLayer, ptAppData->tCousineBitmapFont, 100.0f, (plVec2){25.0f, 220.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, "Cousine @ 100, bitmap (loaded at 18)", 0.0f);

    gptDraw->add_text(ptAppData->ptFGLayer, ptAppData->tCousineSDFFont, 18.0f, (plVec2){25.0f, 320.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, "Cousine @ 18, sdf (loaded at 18)", 0.0f);
    gptDraw->add_text(ptAppData->ptFGLayer, ptAppData->tCousineSDFFont, 100.0f, (plVec2){25.0f, 340.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, "Cousine @ 100, sdf (loaded at 18)", 0.0f);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~drawing prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // submit our draw layers
    gptDraw->submit_2d_layer(ptAppData->ptBGLayer);
    gptDraw->submit_2d_layer(ptAppData->ptFGLayer);

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

    // submit drawlists
    plIO* ptIO = gptIO->get_io();
    gptDraw->submit_2d_drawlist(ptAppData->ptDrawlist, tEncoder, ptIO->afMainViewportSize[0], ptIO->afMainViewportSize[1], 1);

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