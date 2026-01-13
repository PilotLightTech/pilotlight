/*
   example_pf_ext.c
     - demonstrates pathfinding extension
     - minimal visualization only
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] config
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

#include <stdlib.h>
#include <string.h>
#include "pl.h"

#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extensions
#include "pl_draw_ext.h"
#include "pl_starter_ext.h"
#include "pl_graphics_ext.h"
#include "pl_path_finding_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] config
//-----------------------------------------------------------------------------

#define GRID_SIZE_X 10
#define GRID_SIZE_Z 10
#define VOXEL_SIZE  1.0f

#define VIEW_OFFSET_X 100.0f
#define VIEW_OFFSET_Y 100.0f
#define CELL_DRAW_SIZE 40.0f

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAppData
{
    plWindow* ptWindow;

    plDrawList2D*  ptDrawlist;
    plDrawLayer2D* ptLayer;

    plPathFindingVoxelGrid* ptVoxelGrid;

} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

const plIOI*          gptIO          = NULL;
const plWindowI*      gptWindows     = NULL;
const plDrawI*        gptDraw        = NULL;
const plStarterI*     gptStarter     = NULL;
const plGraphicsI*    gptGfx         = NULL;
const plPathFindingI* gptPathFinding = NULL;

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryI* ptApiRegistry, plAppData* ptAppData)
{
    // hot reload
    if(ptAppData)
    {
        gptIO          = pl_get_api_latest(ptApiRegistry, plIOI);
        gptWindows     = pl_get_api_latest(ptApiRegistry, plWindowI);
        gptDraw        = pl_get_api_latest(ptApiRegistry, plDrawI);
        gptStarter     = pl_get_api_latest(ptApiRegistry, plStarterI);
        gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);
        gptPathFinding = pl_get_api_latest(ptApiRegistry, plPathFindingI);
        return ptAppData;
    }

    // first load
    ptAppData = malloc(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    // load extensions
    const plExtensionRegistryI* ptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);
    ptExtensionRegistry->load("pl_unity_ext", NULL, NULL, true);
    ptExtensionRegistry->load("pl_platform_ext", NULL, NULL, false);
    
    // get apis
    gptIO          = pl_get_api_latest(ptApiRegistry, plIOI);
    gptWindows     = pl_get_api_latest(ptApiRegistry, plWindowI);
    gptDraw        = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptStarter     = pl_get_api_latest(ptApiRegistry, plStarterI);
    gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptPathFinding = pl_get_api_latest(ptApiRegistry, plPathFindingI);

    // create window
    plWindowDesc tWindowDesc = {
        .pcTitle = "Pathfinding Test",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 800,
        .uHeight = 600,
    };
    gptWindows->create(tWindowDesc, &ptAppData->ptWindow);
    gptWindows->show(ptAppData->ptWindow);

    // initialize starter
    plStarterInit tStarterInit = {
        .tFlags   = PL_STARTER_FLAGS_ALL_EXTENSIONS,
        .ptWindow = ptAppData->ptWindow
    };
    gptStarter->initialize(tStarterInit);

    // setup drawing
    ptAppData->ptDrawlist = gptDraw->request_2d_drawlist();
    ptAppData->ptLayer = gptDraw->request_2d_layer(ptAppData->ptDrawlist);

    gptStarter->finalize();

    // create voxel grid
    plVec3 tOrigin = {0.0f, 0.0f, 0.0f};
    // height = 1 for now (just ground plane)
    ptAppData->ptVoxelGrid = gptPathFinding->create_voxel_grid(GRID_SIZE_X, 1, GRID_SIZE_Z, VOXEL_SIZE, tOrigin);

    // hardcoded obstacles - simple wall
    gptPathFinding->set_voxel(ptAppData->ptVoxelGrid, 5, 0, 3, true);
    gptPathFinding->set_voxel(ptAppData->ptVoxelGrid, 5, 0, 4, true);
    gptPathFinding->set_voxel(ptAppData->ptVoxelGrid, 5, 0, 5, true);
    gptPathFinding->set_voxel(ptAppData->ptVoxelGrid, 5, 0, 6, true);

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

    if(ptAppData->ptVoxelGrid)
        gptPathFinding->destroy_voxel_grid(ptAppData->ptVoxelGrid);

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

    gptDraw->new_frame();

    // draw grid cells (xz plane, top-down view)
    for(uint32_t x = 0; x < GRID_SIZE_X; x++)
    {
        for(uint32_t z = 0; z < GRID_SIZE_Z; z++)
        {
            bool bOccupied = gptPathFinding->is_voxel_occupied(ptAppData->ptVoxelGrid, x, 0, z);

            float fScreenX = VIEW_OFFSET_X + x * CELL_DRAW_SIZE;
            float fScreenY = VIEW_OFFSET_Y + z * CELL_DRAW_SIZE;

            plVec2 tMin = {fScreenX, fScreenY};
            plVec2 tMax = {fScreenX + CELL_DRAW_SIZE, fScreenY + CELL_DRAW_SIZE};

            // draw filled rect if occupied
            if(bOccupied)
            {
                gptDraw->add_rect_filled(ptAppData->ptLayer, tMin, tMax, 
                    (plDrawSolidOptions){.uColor = PL_COLOR_32_RED});
            }

            // draw grid outline
            gptDraw->add_rect(ptAppData->ptLayer, tMin, tMax, 
                (plDrawLineOptions){.fThickness = 1.0f, .uColor = PL_COLOR_32_GREY});
        }
    }

    // hardcoded start position (1, 0, 2)
    float fStartX = VIEW_OFFSET_X + 1.0f * CELL_DRAW_SIZE + CELL_DRAW_SIZE * 0.5f;
    float fStartY = VIEW_OFFSET_Y + 2.0f * CELL_DRAW_SIZE + CELL_DRAW_SIZE * 0.5f;
    gptDraw->add_circle_filled(ptAppData->ptLayer, (plVec2){fStartX, fStartY}, CELL_DRAW_SIZE * 0.3f, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_BLUE});

    // hardcoded goal position (8, 0, 7)
    float fGoalX = VIEW_OFFSET_X + 8.0f * CELL_DRAW_SIZE + CELL_DRAW_SIZE * 0.5f;
    float fGoalY = VIEW_OFFSET_Y + 7.0f * CELL_DRAW_SIZE + CELL_DRAW_SIZE * 0.5f;
    gptDraw->add_circle_filled(ptAppData->ptLayer, (plVec2){fGoalX, fGoalY}, CELL_DRAW_SIZE * 0.3f, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_YELLOW});

    // submit drawing
    gptDraw->submit_2d_layer(ptAppData->ptLayer);

    plRenderEncoder* ptEncoder = gptStarter->begin_main_pass();
    plIO* ptIO = gptIO->get_io();
    gptDraw->submit_2d_drawlist(ptAppData->ptDrawlist, ptEncoder, ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y, 1);
    gptStarter->end_main_pass();

    gptStarter->end_frame();
}