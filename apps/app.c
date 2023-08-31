/*
   app.c (just an experimental)
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
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
#include "pl_io.h"
#include "pl_os.h"
#include "pl_memory.h"

#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extensions
#include "pl_image_ext.h"
#include "pl_draw_ext.h"
#include "pl_ui_ext.h"
#include "pl_stats_ext.h"
#include "pl_graphics_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct plAppData_t
{
    plDrawList   drawlist;
    plDrawLayer* fgDrawLayer;
    plDrawLayer* bgDrawLayer;
    plFontAtlas  fontAtlas;
    bool         bShowUiDemo;
    bool         bShowUiStyle;
    bool         bShowUiDebug;

    // new
    plGraphics tGraphics;
    plMesh     tMesh;
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] global apis
//-----------------------------------------------------------------------------

const plApiRegistryApiI*       gptApiRegistry       = NULL;
const plDataRegistryApiI*      gptDataRegistry      = NULL;
const plDrawApiI*              gptDraw              = NULL;
const plUiApiI*                gptUi                = NULL;
const plStatsApiI*             gptStats             = NULL;
const plExtensionRegistryApiI* gptExtensionRegistry = NULL;
const plFileApiI*              gptFile              = NULL;
const plGraphicsI*             gptGfx               = NULL;
const plDeviceI*               gptDevice            = NULL;

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryApiI* ptApiRegistry, plAppData* ptAppData)
{
    gptApiRegistry  = ptApiRegistry;
    gptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(gptDataRegistry->get_data(PL_CONTEXT_MEMORY));
    pl_set_io_context(gptDataRegistry->get_data(PL_CONTEXT_IO_NAME));

    if(ptAppData) // reload
    {
        pl_set_log_context(gptDataRegistry->get_data("log"));
        pl_set_profile_context(gptDataRegistry->get_data("profile"));

        // reload global apis
        gptDraw   = ptApiRegistry->first(PL_API_DRAW);
        gptUi     = ptApiRegistry->first(PL_API_UI);
        gptStats  = ptApiRegistry->first(PL_API_STATS);
        gptFile   = ptApiRegistry->first(PL_API_FILE);
        gptGfx    = ptApiRegistry->first(PL_API_GRAPHICS);
        gptDevice = ptApiRegistry->first(PL_API_DEVICE);

        return ptAppData;
    }

    plProfileContext* ptProfileCtx = pl_create_profile_context();
    plLogContext*     ptLogCtx     = pl_create_log_context();
    
    // add some context to data registry
    ptAppData = PL_ALLOC(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));
    gptDataRegistry->set_data("profile", ptProfileCtx);
    gptDataRegistry->set_data("log", ptLogCtx);

    plIOContext* ptIOCtx = pl_get_io_context();

    // create log context
    pl_add_log_channel("Default", PL_CHANNEL_TYPE_CONSOLE);
    pl_log_info("Setup logging");

    // load extensions
    const plExtensionRegistryApiI* ptExtensionRegistry = ptApiRegistry->first(PL_API_EXTENSION_REGISTRY);
    ptExtensionRegistry->load("pl_image_ext",    "pl_load_image_ext", "pl_unload_image_ext", false);
    ptExtensionRegistry->load("pl_draw_ext",     "pl_load_draw_ext",  "pl_unload_draw_ext",  true);
    ptExtensionRegistry->load("pl_ui_ext",       "pl_load_ui_ext",    "pl_unload_ui_ext",    true);
    ptExtensionRegistry->load("pl_stats_ext",    "pl_load_stats_ext", "pl_unload_stats_ext", false);
    ptExtensionRegistry->load("pl_graphics_ext", "pl_load_ext",       "pl_unload_ext",       false);

    // load apis
    gptDraw   = ptApiRegistry->first(PL_API_DRAW);
    gptUi     = ptApiRegistry->first(PL_API_UI);
    gptStats  = ptApiRegistry->first(PL_API_STATS);
    gptFile   = ptApiRegistry->first(PL_API_FILE);
    gptGfx    = ptApiRegistry->first(PL_API_GRAPHICS);
    gptDevice = ptApiRegistry->first(PL_API_DEVICE);

    plUiContext* ptUiContext  = gptUi->create_context();

    // create command queue
    gptGfx->initialize(&ptAppData->tGraphics);

    // create draw list & layers
    plDrawContext* ptDrawCtx = gptDraw->get_context();
    gptDraw->register_drawlist(ptDrawCtx, &ptAppData->drawlist);
    ptAppData->bgDrawLayer = gptDraw->request_layer(&ptAppData->drawlist, "Background Layer");
    ptAppData->fgDrawLayer = gptDraw->request_layer(&ptAppData->drawlist, "Foreground Layer");
    
    // create font atlas
    gptDraw->add_default_font(&ptAppData->fontAtlas);
    gptDraw->build_font_atlas(ptDrawCtx, &ptAppData->fontAtlas);
    gptUi->set_default_font(&ptAppData->fontAtlas.sbFonts[0]);

    // new demo

    // vertex buffer
    const float fVertexBuffer[] = {
        // x, y, z, r, g, b, a
        -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
         0.0f,  0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f,
    };

    ptAppData->tMesh.uVertexBuffer = gptDevice->create_vertex_buffer(&ptAppData->tGraphics.tDevice, sizeof(float) * 21, sizeof(float) * 3, fVertexBuffer, "vertex buffer");


    // index buffer
    const uint32_t uIndexBuffer[] = {
        0, 1, 2
    };
    ptAppData->tMesh.uIndexBuffer = gptDevice->create_index_buffer(&ptAppData->tGraphics.tDevice, sizeof(uint32_t) * 3, uIndexBuffer, "index buffer");

    ptAppData->tMesh.uIndexCount = 3;
    ptAppData->tMesh.uVertexCount = 3;
    
    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    // clean up contexts
    gptDraw->cleanup_font_atlas(&ptAppData->fontAtlas);
    gptDraw->cleanup_context();
    gptUi->destroy_context(NULL);
    gptGfx->cleanup(&ptAppData->tGraphics);
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
    gptGfx->resize(&ptAppData->tGraphics);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    gptUi->new_frame();

    plIOContext* ptIOCtx = pl_get_io_context();

    if(!gptGfx->begin_frame(&ptAppData->tGraphics))
        return;

    // begin profiling frame
    pl_begin_profile_frame();

    gptGfx->begin_recording(&ptAppData->tGraphics);

    if(gptUi->begin_window("Pilot Light", NULL, false))
    {

        const float pfRatios[] = {1.0f};
        gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);

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
    {
        pl_begin_profile_sample("ui demo");
        gptUi->demo(&ptAppData->bShowUiDemo);
        pl_end_profile_sample();
    }

    if(ptAppData->bShowUiStyle)
        gptUi->style(&ptAppData->bShowUiStyle);

    if(ptAppData->bShowUiDebug)
        gptUi->debug(&ptAppData->bShowUiDebug);

    // submit draw layers
    pl_begin_profile_sample("Submit draw layers");
    gptDraw->submit_layer(ptAppData->bgDrawLayer);
    gptDraw->submit_layer(ptAppData->fgDrawLayer);
    pl_end_profile_sample();

    gptUi->render();

    plDraw tDraw = {
        .ptMesh = &ptAppData->tMesh
    };

    plDrawArea tArea = {
        .uDrawOffset = 0,
        .uDrawCount = 1
    };
    gptGfx->draw_areas(&ptAppData->tGraphics, 1, &tArea, &tDraw);

    // submit draw lists
    pl_begin_profile_sample("Submit draw lists");
    gptDraw->get_context()->tFrameBufferScale.x = ptIOCtx->afMainFramebufferScale[0];
    gptDraw->get_context()->tFrameBufferScale.y = ptIOCtx->afMainFramebufferScale[1];
    gptGfx->draw_lists(&ptAppData->tGraphics, 1, &ptAppData->drawlist);
    gptGfx->draw_lists(&ptAppData->tGraphics, 1, gptUi->get_draw_list(NULL));
    gptGfx->draw_lists(&ptAppData->tGraphics, 1, gptUi->get_debug_draw_list(NULL));
    pl_end_profile_sample();

    gptGfx->end_recording(&ptAppData->tGraphics);
    gptGfx->end_frame(&ptAppData->tGraphics);
    pl_end_profile_frame();
}