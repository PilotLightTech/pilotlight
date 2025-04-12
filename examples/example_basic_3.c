/*
   example_basic_3.c
     - demonstrates loading APIs
     - demonstrates loading extensions
     - demonstrates hot reloading
     - demonstrates starter extension
     - demonstrates drawing extension (2D)
     - demonstrates drawing backend extension
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
    The purpose of this example is to demonstrate the drawing extension. We
    will still use the starter extension to handle other boilerplate code but
    we will configure it such that it doesn't manage the drawing extension.

    The draw extension is agnostic to a particular graphics backend. It
    outputs vertex/index buffers (among other things) that allows a user to
    integrate into any existing renderer. We provide a separate extension that
    utilizes our graphics extension to actually do the rendering. This is the
    draw backend extension introduced in this example.
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
#include "pl_window_ext.h"
#include "pl_draw_ext.h"
#include "pl_draw_backend_ext.h"
#include "pl_starter_ext.h"
#include "pl_graphics_ext.h"

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
    plFont*        ptCousineBitmapFont;
    plFont*        ptCousineSDFFont;
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

const plIOI*          gptIO          = NULL;
const plWindowI*      gptWindows     = NULL;
const plDrawI*        gptDraw        = NULL;
const plDrawBackendI* gptDrawBackend = NULL;
const plStarterI*     gptStarter     = NULL;
const plGraphicsI*    gptGfx         = NULL;

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
        gptIO          = pl_get_api_latest(ptApiRegistry, plIOI);
        gptWindows     = pl_get_api_latest(ptApiRegistry, plWindowI);
        gptDraw        = pl_get_api_latest(ptApiRegistry, plDrawI);
        gptDrawBackend = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
        gptStarter     = pl_get_api_latest(ptApiRegistry, plStarterI);
        gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);

        return ptAppData;
    }

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = malloc(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

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
    gptDraw        = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptDrawBackend = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
    gptStarter     = pl_get_api_latest(ptApiRegistry, plStarterI);
    gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);

    // use window API to create a window
    plWindowDesc tWindowDesc = {
        .pcTitle = "Example Basic 3",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 600,
        .uHeight = 600,
    };
    gptWindows->create_window(tWindowDesc, &ptAppData->ptWindow);

    // initialize the starter API
    plStarterInit tStarterInit = {
        .tFlags   = PL_STARTER_FLAGS_ALL_EXTENSIONS,
        .ptWindow = ptAppData->ptWindow
    };

    // we will remove this flag so we can handle
    // management of the UI extension
    tStarterInit.tFlags &= ~PL_STARTER_FLAGS_DRAW_EXT;

    gptStarter->initialize(tStarterInit);

    // initialize the draw extension
    gptDraw->initialize(NULL);

    // initialize the draw backend
    plDevice* ptDevice = gptStarter->get_device();
    gptDrawBackend->initialize(ptDevice);

    // create font atlas
    plFontAtlas* ptAtlas = gptDraw->create_font_atlas();
    gptDraw->set_font_atlas(ptAtlas);

    // typical font range (you can also add individual characters)
    const plFontRange tRange = {
        .iFirstCodePoint = 0x0020,
        .uCharCount = 0x00FF - 0x0020
    };

    // adding another font
    plFontConfig tFontConfig0 = {
        .bSdf           = false,
        .fSize          = 18.0f,
        .uHOverSampling = 1,
        .uVOverSampling = 1,
        .uRangeCount    = 1,
        .ptRanges       = &tRange
    };
    ptAppData->ptCousineBitmapFont = gptDraw->add_font_from_file_ttf(gptDraw->get_current_font_atlas(), tFontConfig0, "../data/pilotlight-assets-master/fonts/Cousine-Regular.ttf");

    // adding previous font but as a signed distance field (SDF)
    plFontConfig tFontConfig1 = {
        .bSdf           = true, // only works with ttf
        .fSize          = 18.0f,
        .uHOverSampling = 1,
        .uVOverSampling = 1,
        .ucOnEdgeValue  = 180,
        .iSdfPadding    = 1,
        .uRangeCount    = 1,
        .ptRanges       = &tRange
    };
    ptAppData->ptCousineSDFFont = gptDraw->add_font_from_file_ttf(gptDraw->get_current_font_atlas(), tFontConfig1, "../data/pilotlight-assets-master/fonts/Cousine-Regular.ttf");

    // register our app drawlist
    ptAppData->ptDrawlist = gptDraw->request_2d_drawlist();

    // request layers (allows drawing out of order)
    ptAppData->ptFGLayer = gptDraw->request_2d_layer(ptAppData->ptDrawlist);
    ptAppData->ptBGLayer = gptDraw->request_2d_layer(ptAppData->ptDrawlist);

    // wraps up
    gptStarter->finalize();

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~font atlas texture~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // draw backend handles creating the font atlas texture and
    // uploading to the GPU but it requires a command buffer (in an non recording state).
    // Later examples will go into command buffers without using the starter ext

    plCommandBuffer* ptCmdBuffer = gptStarter->get_raw_command_buffer(); // not recording

    // actually record, submit, & wait
    gptDrawBackend->build_font_atlas(ptCmdBuffer, gptDraw->get_current_font_atlas());

    // return back to the pool
    gptStarter->return_raw_command_buffer(ptCmdBuffer);

    // return app memory
    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    // ensure device is done with resources
    plDevice* ptDevice = gptStarter->get_device();
    gptGfx->flush_device(ptDevice); // waits for the GPU to be done with all work

    // cleans up texture and other resources
    gptDrawBackend->cleanup_font_atlas(gptDraw->get_current_font_atlas());

    gptStarter->cleanup();
    gptWindows->destroy_window(ptAppData->ptWindow);
    free(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plAppData* ptAppData)
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

    // this must be called now that the starter
    // extension isn't doing it for us
    gptDrawBackend->new_frame();

    plDrawLineOptions tCommonLineOptions = {
        .fThickness = 1.0f,
        .uColor     = PL_COLOR_32_MAGENTA
    };

    plDrawSolidOptions tCommonSolidOptions = {
        .uColor = PL_COLOR_32_MAGENTA
    };

    gptDraw->add_line(ptAppData->ptFGLayer,
        (plVec2){0.0f, 0.0f},
        (plVec2){100.0f, 100.0f}, tCommonLineOptions);

    float fXCursor = 0.0f;
    gptDraw->add_line(ptAppData->ptFGLayer,
        (plVec2){fXCursor, 0.0f},
        (plVec2){fXCursor + 100.0f, 100.0f}, tCommonLineOptions);

    fXCursor += 100.0f;
    gptDraw->add_triangle(ptAppData->ptFGLayer,
        (plVec2){fXCursor + 50.0f, 0.0f },
        (plVec2){fXCursor, 100.0f },
        (plVec2){fXCursor + 100.0f, 100.0f }, tCommonLineOptions);

    fXCursor += 100.0f;
    gptDraw->add_circle(ptAppData->ptFGLayer,
        (plVec2){fXCursor + 50.0f, 50.0f},
        50.0f, 0, tCommonLineOptions);

    fXCursor += 100.0f;
    gptDraw->add_rect_rounded(ptAppData->ptFGLayer,
        (plVec2){fXCursor, 5.0f},
        (plVec2){fXCursor + 100.0f, 100.0f},
        0, 0, 0, tCommonLineOptions);

    fXCursor += 100.0f;
    gptDraw->add_rect_rounded(ptAppData->ptFGLayer,
        (plVec2){fXCursor, 5.0f},
        (plVec2){fXCursor + 100.0f, 100.0f},
        25.0f, 0, 0, tCommonLineOptions);

    fXCursor += 100.0f;
    gptDraw->add_rect_rounded(ptAppData->ptFGLayer,
        (plVec2){fXCursor, 5.0f},
        (plVec2){fXCursor + 100.0f, 100.0f},
        25.0f, 0, PL_DRAW_RECT_FLAG_ROUND_CORNERS_TOP_LEFT, tCommonLineOptions);

    fXCursor += 100.0f;
    gptDraw->add_quad(ptAppData->ptFGLayer,
        (plVec2){fXCursor + 5.0f, 5.0f},
        (plVec2){fXCursor + 5.0f, 100.0f},
        (plVec2){fXCursor + 100.0f, 100.0f},
        (plVec2){fXCursor + 100.0f, 5.0f}, tCommonLineOptions);

    fXCursor += 100.0f;
    gptDraw->add_bezier_quad(ptAppData->ptFGLayer,
        (plVec2){fXCursor, 0.0f},
        (plVec2){fXCursor + 100.0f, 0.0f},
        (plVec2){fXCursor + 100.0f, 100.0f}, 0, tCommonLineOptions);

    fXCursor += 100.0f;
    gptDraw->add_bezier_cubic(ptAppData->ptFGLayer,
        (plVec2){fXCursor, 0.0f},
        (plVec2){fXCursor + 100.0f, 0.0f},
        (plVec2){fXCursor, 100.0f},
        (plVec2){fXCursor + 100.0f, 100.0f},
        0, tCommonLineOptions);

    fXCursor = 100.0f;
    gptDraw->add_triangle_filled(ptAppData->ptFGLayer,
        (plVec2){fXCursor + 50.0f, 100.0f},
        (plVec2){fXCursor, 200.0f},
        (plVec2){fXCursor + 100.0f, 200.0f}, tCommonSolidOptions);

    fXCursor += 100.0f;
    gptDraw->add_circle_filled(ptAppData->ptFGLayer,
        (plVec2){fXCursor + 50.0f, 150.0f},
        50.0f, 0, tCommonSolidOptions);

    fXCursor += 100.0f;
    gptDraw->add_rect_rounded_filled(ptAppData->ptFGLayer,
        (plVec2){fXCursor, 105.0f},
        (plVec2){fXCursor + 100.0f, 200.0f},
        0, 0, 0, tCommonSolidOptions);

    fXCursor += 100.0f;
    gptDraw->add_rect_rounded_filled(ptAppData->ptFGLayer,
        (plVec2){fXCursor, 105.0f},
        (plVec2){fXCursor + 100.0f, 200.0f},
        25.0f, 0, 0, tCommonSolidOptions);

    fXCursor += 100.0f;
    gptDraw->add_quad_filled(ptAppData->ptFGLayer,
        (plVec2){fXCursor + 5.0f, 105.0f},
        (plVec2){fXCursor + 5.0f, 200.0f},
        (plVec2){fXCursor + 100.0f, 200.0f},
        (plVec2){fXCursor + 100.0f, 105.0f}, tCommonSolidOptions);

    // default text
    gptDraw->add_text(ptAppData->ptFGLayer, (plVec2){25.0f, 300.0f}, "Proggy @ 13 (loaded at 13)", (plDrawTextOptions){.ptFont = gptStarter->get_default_font(), .uColor = PL_COLOR_32_WHITE});
    gptDraw->add_text(ptAppData->ptFGLayer, (plVec2){25.0f, 315.0f}, "Proggy @ 45 (loaded at 13)", (plDrawTextOptions){.ptFont = gptStarter->get_default_font(), .uColor = PL_COLOR_32_WHITE, .fSize = 45.0f});

    // bitmap text
    gptDraw->add_text(ptAppData->ptFGLayer, (plVec2){25.0f, 400.0f}, "Cousine @ 18, bitmap (loaded at 18)", (plDrawTextOptions){.ptFont = ptAppData->ptCousineBitmapFont, .uColor = PL_COLOR_32_WHITE});
    gptDraw->add_text(ptAppData->ptFGLayer, (plVec2){25.0f, 420.0f}, "Cousine @ 100, bitmap (loaded at 18)", (plDrawTextOptions){.ptFont = ptAppData->ptCousineBitmapFont, .uColor = PL_COLOR_32_WHITE, .fSize = 100.0f});

    // sdf text
    gptDraw->add_text(ptAppData->ptFGLayer, (plVec2){25.0f, 520.0f}, "Cousine @ 18, sdf (loaded at 18)", (plDrawTextOptions){.ptFont = ptAppData->ptCousineSDFFont, .uColor = PL_COLOR_32_WHITE, .fSize = 18.0f});
    gptDraw->add_text(ptAppData->ptFGLayer, (plVec2){25.0f, 540.0f}, "Cousine @ 100, sdf (loaded at 18)", (plDrawTextOptions){.ptFont = ptAppData->ptCousineSDFFont, .uColor = PL_COLOR_32_WHITE, .fSize = 100.0f});

    // submit our draw layers
    gptDraw->submit_2d_layer(ptAppData->ptBGLayer);
    gptDraw->submit_2d_layer(ptAppData->ptFGLayer);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~submit drawlists~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // the starter extension will call begin/end main_pass if you don't but in this
    // case we are managing the draw backend so we must submit the drawlists
    // ourself. The starter extension still handles alot of the process of dealing
    // with command buffers, syncronization, submission, etc. but that is outside
    // the scope of the draw & draw backend extensions.

    // start main pass & return the encoder being used
    plRenderEncoder* ptEncoder = gptStarter->begin_main_pass();

    // submit our drawlist
    plIO* ptIO = gptIO->get_io();
    gptDrawBackend->submit_2d_drawlist(ptAppData->ptDrawlist, ptEncoder, ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y, 1);

    // allows the starter extension to handle some things then ends the main pass
    gptStarter->end_main_pass();

    // must be the last function called when using the starter extension
    gptStarter->end_frame(); 
}
