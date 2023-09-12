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
#include "pl_os.h"
#include "pl_memory.h"

#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

#include "pl_ui.h"

// extensions
#include "pl_image_ext.h"
#include "pl_stats_ext.h"
#include "pl_graphics_ext.h"
#include "pl_debug_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct plAppData_t
{
    plDrawList   drawlist;
    plDrawLayer* fgDrawLayer;
    plDrawLayer* bgDrawLayer;
    plFontAtlas  fontAtlas;

    plDebugApiInfo tDebugInfo;
    bool           bShowUiDemo;
    bool           bShowUiDebug;
    bool           bShowUiStyle;

    plGraphics tGraphics;
    plMesh     tMesh;
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] global apis
//-----------------------------------------------------------------------------

const plApiRegistryApiI*       gptApiRegistry       = NULL;
const plDataRegistryApiI*      gptDataRegistry      = NULL;
const plStatsApiI*             gptStats             = NULL;
const plExtensionRegistryApiI* gptExtensionRegistry = NULL;
const plFileApiI*              gptFile              = NULL;
const plGraphicsI*             gptGfx               = NULL;
const plDeviceI*               gptDevice            = NULL;
const plDebugApiI*             gptDebug             = NULL;

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryApiI* ptApiRegistry, plAppData* ptAppData)
{
    gptApiRegistry  = ptApiRegistry;
    gptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(gptDataRegistry->get_data(PL_CONTEXT_MEMORY));
    pl_set_ui_context(gptDataRegistry->get_data("ui"));

    if(ptAppData) // reload
    {
        pl_set_log_context(gptDataRegistry->get_data("log"));
        pl_set_profile_context(gptDataRegistry->get_data("profile"));

        // reload global apis
        gptStats  = ptApiRegistry->first(PL_API_STATS);
        gptFile   = ptApiRegistry->first(PL_API_FILE);
        gptGfx    = ptApiRegistry->first(PL_API_GRAPHICS);
        gptDevice = ptApiRegistry->first(PL_API_DEVICE);
        gptDebug  = ptApiRegistry->first(PL_API_DEBUG);

        return ptAppData;
    }

    plProfileContext* ptProfileCtx = pl_create_profile_context();
    plLogContext*     ptLogCtx     = pl_create_log_context();
    
    // add some context to data registry
    ptAppData = PL_ALLOC(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));
    gptDataRegistry->set_data("profile", ptProfileCtx);
    gptDataRegistry->set_data("log", ptLogCtx);

    // create log context
    pl_add_log_channel("Default", PL_CHANNEL_TYPE_CONSOLE);
    pl_log_info("Setup logging");

    // load extensions
    const plExtensionRegistryApiI* ptExtensionRegistry = ptApiRegistry->first(PL_API_EXTENSION_REGISTRY);
    ptExtensionRegistry->load("pl_image_ext",    "pl_load_image_ext", "pl_unload_image_ext", false);
    ptExtensionRegistry->load("pl_stats_ext",    "pl_load_stats_ext", "pl_unload_stats_ext", false);
    ptExtensionRegistry->load("pl_graphics_ext", "pl_load_ext",       "pl_unload_ext",       false);
    ptExtensionRegistry->load("pl_debug_ext",    "pl_load_debug_ext", "pl_unload_debug_ext", true);

    // load apis
    gptStats  = ptApiRegistry->first(PL_API_STATS);
    gptFile   = ptApiRegistry->first(PL_API_FILE);
    gptGfx    = ptApiRegistry->first(PL_API_GRAPHICS);
    gptDevice = ptApiRegistry->first(PL_API_DEVICE);
    gptDebug  = ptApiRegistry->first(PL_API_DEBUG);

    // create command queue
    gptGfx->initialize(&ptAppData->tGraphics);

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

    // create draw list & layers
    pl_register_drawlist(&ptAppData->drawlist);
    ptAppData->bgDrawLayer = pl_request_layer(&ptAppData->drawlist, "Background Layer");
    ptAppData->fgDrawLayer = pl_request_layer(&ptAppData->drawlist, "Foreground Layer");
    
    // create font atlas
    pl_add_default_font(&ptAppData->fontAtlas);
    pl_build_font_atlas(&ptAppData->fontAtlas);
    gptGfx->create_font_atlas(&ptAppData->fontAtlas);
    pl_set_default_font(&ptAppData->fontAtlas.sbFonts[0]);
    
    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    gptGfx->destroy_font_atlas(&ptAppData->fontAtlas);
    pl_cleanup_font_atlas(&ptAppData->fontAtlas);

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
    if(!gptGfx->begin_frame(&ptAppData->tGraphics))
        return;

    // begin profiling frame
    pl_begin_profile_frame();

    gptStats->new_frame();

    static double* pdFrameTimeCounter = NULL;
    if(!pdFrameTimeCounter)
        pdFrameTimeCounter = gptStats->get_counter("framerate");
    *pdFrameTimeCounter = (double)pl_get_io()->fFrameRate;



    gptGfx->begin_recording(&ptAppData->tGraphics);

    pl_new_frame();

    pl_set_next_window_pos((plVec2){0, 0}, PL_UI_COND_ONCE);

    if(pl_begin_window("Pilot Light", NULL, false))
    {

        const float pfRatios[] = {1.0f};
        pl_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
        
        if(pl_collapsing_header("Tools"))
        {
            // pl_checkbox("Device Memory Analyzer", &ptAppData->tDebugInfo.bShowDeviceMemoryAnalyzer);
            pl_checkbox("Memory Allocations", &ptAppData->tDebugInfo.bShowMemoryAllocations);
            pl_checkbox("Profiling", &ptAppData->tDebugInfo.bShowProfiling);
            pl_checkbox("Statistics", &ptAppData->tDebugInfo.bShowStats);
            pl_checkbox("Logging", &ptAppData->tDebugInfo.bShowLogging);
            pl_end_collapsing_header();
        }

        if(pl_collapsing_header("User Interface"))
        {
            pl_checkbox("UI Debug", &ptAppData->bShowUiDebug);
            pl_checkbox("UI Demo", &ptAppData->bShowUiDemo);
            pl_checkbox("UI Style", &ptAppData->bShowUiStyle);
            pl_end_collapsing_header();
        }
        pl_end_window();
    }

    gptDebug->show_windows(&ptAppData->tDebugInfo);

    if(ptAppData->bShowUiDemo)
    {
        pl_begin_profile_sample("ui demo");
        pl_demo(&ptAppData->bShowUiDemo);
        pl_end_profile_sample();
    }
        
    if(ptAppData->bShowUiStyle)
        pl_style(&ptAppData->bShowUiStyle);

    if(ptAppData->bShowUiDebug)
        pl_debug(&ptAppData->bShowUiDebug);

    pl_add_line(ptAppData->fgDrawLayer, (plVec2){0}, (plVec2){300.0f, 500.0f}, (plVec4){1.0f, 0.0f, 0.0f, 1.0f}, 1.0f);

    // submit draw layers
    pl_begin_profile_sample("Submit draw layers");
    pl_submit_layer(ptAppData->bgDrawLayer);
    pl_submit_layer(ptAppData->fgDrawLayer);
    pl_end_profile_sample();

    pl_render();


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
    gptGfx->draw_lists(&ptAppData->tGraphics, 1, &ptAppData->drawlist);
    gptGfx->draw_lists(&ptAppData->tGraphics, 1, pl_get_draw_list(NULL));
    gptGfx->draw_lists(&ptAppData->tGraphics, 1, pl_get_debug_draw_list(NULL));
    pl_end_profile_sample();

    gptGfx->end_recording(&ptAppData->tGraphics);
    gptGfx->end_frame(&ptAppData->tGraphics);
    pl_end_profile_frame();
}
