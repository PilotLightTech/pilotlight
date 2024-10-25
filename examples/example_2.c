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
#include "pl_ds.h"
#include "pl_memory.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extensions
#include "pl_draw_ext.h"
#include "pl_shader_ext.h"
#include "pl_graphics_ext.h" // not yet stable
#include "pl_draw_backend_ext.h" // not yet stable

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
    plFont*        ptDefaultFont;
    plFont*        ptCousineBitmapFont;
    plFont*        ptCousineSDFFont;

    // graphics & sync objects
    plDevice*         ptDevice;
    plSurface*        ptSurface;
    plSwapchain*      ptSwapchain;
    plSemaphoreHandle atSempahore[PL_MAX_FRAMES_IN_FLIGHT];
    uint64_t          aulNextTimelineValue[PL_MAX_FRAMES_IN_FLIGHT];

} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

const plIOI*          gptIO          = NULL;
const plWindowI*      gptWindows     = NULL;
const plGraphicsI*    gptGfx         = NULL;
const plDrawI*        gptDraw        = NULL;
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
        .pcName  = "Example 2",
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

    // setup draw
    gptDraw->initialize(NULL);
    gptDrawBackend->initialize(ptAppData->ptDevice);

    // create font atlas
    plFontAtlas* ptAtlas = gptDraw->create_font_atlas();

    // builtin default font (proggy @ 13)
    ptAppData->ptDefaultFont = gptDraw->add_default_font(ptAtlas);

    // typical font range (you can also add individual characters)
    const plFontRange tRange = {
        .iFirstCodePoint = 0x0020,
        .uCharCount = 0x00FF - 0x0020
    };

    // adding another font
    plFontConfig tFontConfig0 = {
        .bSdf = false,
        .fSize = 18.0f,
        .uHOverSampling = 1,
        .uVOverSampling = 1,
        .uRangeCount = 1,
        .ptRanges = &tRange
    };
    ptAppData->ptCousineBitmapFont = gptDraw->add_font_from_file_ttf(ptAtlas, tFontConfig0, "../data/pilotlight-assets-master/fonts/Cousine-Regular.ttf");

    // adding previous font but as a signed distance field
    plFontConfig tFontConfig1 = {
        .bSdf = true, // only works with ttf
        .fSize = 18.0f,
        .uHOverSampling = 1,
        .uVOverSampling = 1,
        .ucOnEdgeValue = 180,
        .iSdfPadding = 1,
        .uRangeCount = 1,
        .ptRanges = &tRange
    };
    ptAppData->ptCousineSDFFont = gptDraw->add_font_from_file_ttf(ptAtlas, tFontConfig1, "../data/pilotlight-assets-master/fonts/Cousine-Regular.ttf");

    // build & set font atlass
    gptDrawBackend->build_font_atlas(ptAtlas);
    gptDraw->set_font_atlas(ptAtlas);

    // register our app drawlist
    ptAppData->ptDrawlist = gptDraw->request_2d_drawlist();
    ptAppData->ptFGLayer = gptDraw->request_2d_layer(ptAppData->ptDrawlist);
    ptAppData->ptBGLayer = gptDraw->request_2d_layer(ptAppData->ptDrawlist);

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
    gptDrawBackend->cleanup_font_atlas(NULL);
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

    // begin new frame
    if(!gptGfx->begin_frame(ptAppData->ptSwapchain))
    {
        gptGfx->resize(ptAppData->ptSwapchain);
        pl_end_profile_frame();
        return;
    }


    // drawing API usage

    // lines
    float fXCursor = 0.0f;
    gptDraw->add_line(ptAppData->ptFGLayer, (plVec2){fXCursor, 0.0f}, (plVec2){fXCursor + 100.0f, 100.0f}, (plDrawLineOptions){.fThickness = 1.0f, .uColor = PL_COLOR_32_MAGENTA});
    fXCursor += 100.0f;
    gptDraw->add_triangle(ptAppData->ptFGLayer, (plVec2){fXCursor + 50.0f, 0.0f}, (plVec2){fXCursor, 100.0f}, (plVec2){fXCursor + 100.0f, 100.0f}, (plDrawLineOptions){.fThickness = 1.0f, .uColor = PL_COLOR_32_MAGENTA});
    fXCursor += 100.0f;
    gptDraw->add_circle(ptAppData->ptFGLayer, (plVec2){fXCursor + 50.0f, 50.0f}, 50.0f, 0, (plDrawLineOptions){.fThickness = 1.0f, .uColor = PL_COLOR_32_MAGENTA});
    fXCursor += 100.0f;
    gptDraw->add_rect_rounded(ptAppData->ptFGLayer, (plVec2){fXCursor, 5.0f}, (plVec2){fXCursor + 100.0f, 100.0f}, 0, 0, 0, (plDrawLineOptions){.fThickness = 1.0f, .uColor = PL_COLOR_32_MAGENTA});
    fXCursor += 100.0f;
    gptDraw->add_rect_rounded(ptAppData->ptFGLayer, (plVec2){fXCursor, 5.0f}, (plVec2){fXCursor + 100.0f, 100.0f}, 25.0f, 0, 0, (plDrawLineOptions){.fThickness = 1.0f, .uColor = PL_COLOR_32_MAGENTA});
    fXCursor += 100.0f;
    gptDraw->add_rect_rounded(ptAppData->ptFGLayer, (plVec2){fXCursor, 5.0f}, (plVec2){fXCursor + 100.0f, 100.0f}, 25.0f, 0, PL_DRAW_RECT_FLAG_ROUND_CORNERS_TOP_LEFT, (plDrawLineOptions){.fThickness = 1.0f, .uColor = PL_COLOR_32_MAGENTA});
    fXCursor += 100.0f;
    gptDraw->add_quad(ptAppData->ptFGLayer, (plVec2){fXCursor + 5.0f, 5.0f}, (plVec2){fXCursor + 5.0f, 100.0f}, (plVec2){fXCursor + 100.0f, 100.0f}, (plVec2){fXCursor + 100.0f, 5.0f}, (plDrawLineOptions){.fThickness = 1.0f, .uColor = PL_COLOR_32_MAGENTA});
    fXCursor += 100.0f;
    gptDraw->add_bezier_quad(ptAppData->ptFGLayer, (plVec2){fXCursor, 0.0f}, (plVec2){fXCursor + 100.0f, 0.0f}, (plVec2){fXCursor + 100.0f, 100.0f}, 0, (plDrawLineOptions){.fThickness = 1.0f, .uColor = PL_COLOR_32_MAGENTA});
    fXCursor += 100.0f;
    gptDraw->add_bezier_cubic(ptAppData->ptFGLayer, (plVec2){fXCursor, 0.0f}, (plVec2){fXCursor + 100.0f, 0.0f}, (plVec2){fXCursor, 100.0f}, (plVec2){fXCursor + 100.0f, 100.0f}, 0, (plDrawLineOptions){.fThickness = 1.0f, .uColor = PL_COLOR_32_MAGENTA});
    fXCursor += 100.0f;

    // solids
    fXCursor = 100.0f;
    gptDraw->add_triangle_filled(ptAppData->ptFGLayer, (plVec2){fXCursor + 50.0f, 100.0f}, (plVec2){fXCursor, 200.0f}, (plVec2){fXCursor + 100.0f, 200.0f}, (plDrawSolidOptions){.uColor = PL_COLOR_32_MAGENTA});
    fXCursor += 100.0f;
    gptDraw->add_circle_filled(ptAppData->ptFGLayer, (plVec2){fXCursor + 50.0f, 150.0f}, 50.0f, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_MAGENTA});
    fXCursor += 100.0f;
    gptDraw->add_rect_rounded_filled(ptAppData->ptFGLayer, (plVec2){fXCursor, 105.0f}, (plVec2){fXCursor + 100.0f, 200.0f}, 0, 0, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_MAGENTA});
    fXCursor += 100.0f;
    gptDraw->add_rect_rounded_filled(ptAppData->ptFGLayer, (plVec2){fXCursor, 105.0f}, (plVec2){fXCursor + 100.0f, 200.0f}, 25.0f, 0, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_MAGENTA});
    fXCursor += 100.0f;
    gptDraw->add_quad_filled(ptAppData->ptFGLayer, (plVec2){fXCursor + 5.0f, 105.0f}, (plVec2){fXCursor + 5.0f, 200.0f}, (plVec2){fXCursor + 100.0f, 200.0f}, (plVec2){fXCursor + 100.0f, 105.0f}, (plDrawSolidOptions){.uColor = PL_COLOR_32_MAGENTA});
    // gptDraw->add_circle_filled(ptAppData->ptBGLayer, (plVec2){100.0f, 100.0f}, 25.0f, (plVec4){1.0f, 0.0f, 1.0f, 1.0f}, 24);

    // default text
    gptDraw->add_text(ptAppData->ptFGLayer, (plVec2){25.0f, 300.0f}, "Proggy @ 13 (loaded at 13)", (plDrawTextOptions){.ptFont = ptAppData->ptDefaultFont, .uColor = PL_COLOR_32_WHITE});
    gptDraw->add_text(ptAppData->ptFGLayer, (plVec2){25.0f, 315.0f}, "Proggy @ 45 (loaded at 13)", (plDrawTextOptions){.ptFont = ptAppData->ptDefaultFont, .uColor = PL_COLOR_32_WHITE, .fSize = 45.0f});

    // bitmap text
    gptDraw->add_text(ptAppData->ptFGLayer, (plVec2){25.0f, 400.0f}, "Cousine @ 18, bitmap (loaded at 18)", (plDrawTextOptions){.ptFont = ptAppData->ptCousineBitmapFont, .uColor = PL_COLOR_32_WHITE});
    gptDraw->add_text(ptAppData->ptFGLayer, (plVec2){25.0f, 420.0f}, "Cousine @ 100, bitmap (loaded at 18)", (plDrawTextOptions){.ptFont = ptAppData->ptCousineBitmapFont, .uColor = PL_COLOR_32_WHITE, .fSize = 100.0f});

    // sdf text
    gptDraw->add_text(ptAppData->ptFGLayer, (plVec2){25.0f, 520.0f}, "Cousine @ 18, sdf (loaded at 18)", (plDrawTextOptions){.ptFont = ptAppData->ptCousineSDFFont, .uColor = PL_COLOR_32_WHITE, .fSize = 100.0f});
    gptDraw->add_text(ptAppData->ptFGLayer, (plVec2){25.0f, 540.0f}, "Cousine @ 100, sdf (loaded at 18)", (plDrawTextOptions){.ptFont = ptAppData->ptCousineSDFFont, .uColor = PL_COLOR_32_WHITE, .fSize = 100.0f});

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~drawing prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // submit our draw layers
    gptDraw->submit_2d_layer(ptAppData->ptBGLayer);
    gptDraw->submit_2d_layer(ptAppData->ptFGLayer);

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
    plCommandBuffer* ptCommandBuffer = gptGfx->begin_command_recording(ptAppData->ptDevice, &tBeginInfo);

    // begin main renderpass (directly to swapchain)
    plRenderEncoder* ptEncoder = gptGfx->begin_render_pass(ptCommandBuffer, gptGfx->get_main_render_pass(ptAppData->ptDevice));

    // submit drawlists
    plIO* ptIO = gptIO->get_io();
    gptDrawBackend->submit_2d_drawlist(ptAppData->ptDrawlist, ptEncoder, ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y, 1);

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