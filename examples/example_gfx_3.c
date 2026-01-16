/*
   example_pf_ext.c 
     - demonstrates pathfinding extension
     - minimal visualization only
*/

/*
=================================================================================
PATHFINDING EXTENSION - DEVELOPMENT ROADMAP
=================================================================================

PHASE 1: BASIC IMPLEMENTATION ✓
    [x] Voxel grid management (create, destroy, set, query)
    [x] A* pathfinding algorithm with diagonal movement
    [x] Path reconstruction
    [x] Coordinate conversion utilities
    [x] 2D top-down visualization example

PHASE 2: CORE OPTIMIZATIONS
    [x] Min-heap priority queue
        - Replace O(n) pop with O(log n) heap pop
        - Biggest performance gain for smallest effort
    
    [ ] Parent array for path reconstruction
        - Replace O(n) parent search with O(1) index lookup
        - Store parents in array indexed by voxel index
    
    [ ] Hash map for open set lookup - OPTIONAL
        - Replace O(n) pl_pq_find with O(1) hash lookup

PHASE 3: 3D VISUALIZATION - SIMPLE GRID
    [ ] Create new example: example_pf_ext_3d.c
    
    [ ] Draw voxel grid as 3D cubes
        - Occupied voxels: red cubes
        - Empty voxels: wireframe or not drawn
    
    [ ] Setup camera system
        - Perspective camera looking at grid from angle
        - Fixed position initially
    
    [ ] Draw path as 3D lines
        - Connect waypoints with colored lines
    
    [ ] Add basic camera controls
        - Rotate around grid (mouse drag)
        - Zoom in/out (mouse wheel)
        - Pan (WASD or arrow keys)
    
    [ ] Test with larger grids
        - Verify performance is acceptable

PHASE 4: MOUSE INTERACTION
    [ ] Implement ray casting from camera
        - Screen click -> ray in 3D world
        - Ray-voxel intersection test
    
    [ ] Click to set goal
        - Left click: set new goal
        - Recalculate path in real-time
    
    [ ] Click to toggle obstacles
        - Right click: add/remove obstacles
        - Re-path automatically

PHASE 5: MESH VOXELIZATION
    [ ] Study voxelization algorithms
        - Conservative rasterization
        - Triangle-AABB intersection
    
    [ ] Implement pl_voxelize_mesh_impl()
        - Convert mesh triangles to occupied voxels
    
    [ ] Test with simple meshes first
        - Cube, sphere, cylinder
        - Visualize voxelized result
    
    [ ] Load and voxelize Sponza
        - Voxelize at appropriate resolution
        - Balance detail vs performance
    
    [ ] Visualize voxelized Sponza
        - Show both mesh and voxel grid
        - Toggle between views

PHASE 6: INTEGRATION & POLISH
    [ ] Pathfinding on Sponza
        - Click to set start/goal on mesh
        - Draw path overlaid on geometry
    
    [ ] Path smoothing - OPTIONAL
        - String pulling algorithm
        - Reduce waypoint count
    
    [ ] Dynamic obstacles - OPTIONAL
        - Update voxel grid at runtime
        - Re-path when obstacles move
    
    [ ] Animated agent - OPTIONAL
        - Simple model following waypoints
    
    [ ] Performance profiling
        - Measure pathfinding time
        - Optimize bottlenecks

PHASE 7: ADVANCED FEATURES - OPTIONAL
    [ ] Jump Point Search optimization
    [ ] Hierarchical pathfinding
    [ ] Multi-threading
    [ ] Navigation mesh (navmesh)

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

#define GRID_SIZE_X 20
#define GRID_SIZE_Z 20
#define VOXEL_SIZE  1.0f

#define VIEW_OFFSET_X 200.0f
#define VIEW_OFFSET_Y 100.0f
#define CELL_DRAW_SIZE 20.0f

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAppData
{
    // pl Core
    plWindow* ptWindow;

    // draw ext
    plDrawList2D*  ptDrawlist;
    plDrawLayer2D* ptLayer;

    // path finding ext
    plPathFindingVoxelGrid* ptVoxelGrid;
    plPathFindingResult     tPathResult; 

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
    
    // hardcoded obstacles - solvable maze structure
    for(uint32_t i = 0; i < GRID_SIZE_Z; i++)
    {
        gptPathFinding->set_voxel(ptAppData->ptVoxelGrid, 0, 0, i, true);
        gptPathFinding->set_voxel(ptAppData->ptVoxelGrid, 19, 0, i, true);
    }
    for(uint32_t j = 0; j < GRID_SIZE_X; j++)
    {
        gptPathFinding->set_voxel(ptAppData->ptVoxelGrid, j, 0, 0, true);
        gptPathFinding->set_voxel(ptAppData->ptVoxelGrid, j, 0, 19, true);
    }
    // simple maze - creates winding corridors
    // vertical wall 1 - gap at z=5
    for(uint32_t z = 2; z < 15; z++)
    {
        if(z != 5)
            gptPathFinding->set_voxel(ptAppData->ptVoxelGrid, 5, 0, z, true);
    }

    // horizontal wall 1 - gap at x=8
    for(uint32_t x = 5; x < 18; x++)
    {
        if(x != 8)
            gptPathFinding->set_voxel(ptAppData->ptVoxelGrid, x, 0, 8, true);
    }

    // vertical wall 2 - gap at z=12
    for(uint32_t z = 8; z < 18; z++)
    {
        // if(z != 12)
            gptPathFinding->set_voxel(ptAppData->ptVoxelGrid, 12, 0, z, true);
    }

    // horizontal wall 2 - gap at x=15
    for(uint32_t x = 2; x < 12; x++)
    {
        if(x != 6)
            gptPathFinding->set_voxel(ptAppData->ptVoxelGrid, x, 0, 15, true);
    }

    // scattered blocks to add complexity
    gptPathFinding->set_voxel(ptAppData->ptVoxelGrid, 3, 0, 3, true);
    gptPathFinding->set_voxel(ptAppData->ptVoxelGrid, 3, 0, 4, true);
    gptPathFinding->set_voxel(ptAppData->ptVoxelGrid, 10, 0, 5, true);
    gptPathFinding->set_voxel(ptAppData->ptVoxelGrid, 10, 0, 6, true);
    gptPathFinding->set_voxel(ptAppData->ptVoxelGrid, 15, 0, 15, true);
    gptPathFinding->set_voxel(ptAppData->ptVoxelGrid, 16, 0, 15, true);
    gptPathFinding->set_voxel(ptAppData->ptVoxelGrid, 18, 0, 8, true);
    
    // create pathfinding query - guaranteed connected path exists
    plPathFindingQuery tQuery = {0};
    tQuery.tStart = (plVec3){1.5f, 0.5f, 1.5f};   // top-left corner (inside outer walls)
    tQuery.tGoal = (plVec3){18.5f, 0.5f, 18.5f};  // bottom-right corner (inside outer walls)

    // find path
    ptAppData->tPathResult = gptPathFinding->find_path(ptAppData->ptVoxelGrid, &tQuery);

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

    gptPathFinding->destroy_voxel_grid(ptAppData->ptVoxelGrid);
    gptPathFinding->free_result(&ptAppData->tPathResult);

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

    // draw grid cells (xz plane, top-down view, height not used for example)
    for(uint32_t x = 0; x < GRID_SIZE_X; x++)
    {
        for(uint32_t z = 0; z < GRID_SIZE_Z; z++)
        {
            bool bOccupied = gptPathFinding->is_voxel_occupied(ptAppData->ptVoxelGrid, x, 0, z);

            float fScreenX = VIEW_OFFSET_X + x * CELL_DRAW_SIZE;
            float fScreenY = VIEW_OFFSET_Y + z * CELL_DRAW_SIZE;

            plVec2 tMin = {fScreenX, fScreenY};
            plVec2 tMax = {fScreenX + CELL_DRAW_SIZE, fScreenY + CELL_DRAW_SIZE};

            // draw filled rect if occupied by obstacle
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

    // draw path if found
    if(ptAppData->tPathResult.bSuccess)
    {
        for(uint32_t i = 0; i < ptAppData->tPathResult.uWaypointCount - 1; i++)
        {
            // convert world position to screen position
            float fX0 = VIEW_OFFSET_X + ptAppData->tPathResult.atWaypoints[i].x * CELL_DRAW_SIZE;
            float fY0 = VIEW_OFFSET_Y + ptAppData->tPathResult.atWaypoints[i].z * CELL_DRAW_SIZE;

            float fX1 = VIEW_OFFSET_X + ptAppData->tPathResult.atWaypoints[i+1].x * CELL_DRAW_SIZE;
            float fY1 = VIEW_OFFSET_Y + ptAppData->tPathResult.atWaypoints[i+1].z * CELL_DRAW_SIZE;

            // draw line between waypoints
            gptDraw->add_line(ptAppData->ptLayer, (plVec2){fX0, fY0}, (plVec2){fX1, fY1}, 
                    (plDrawLineOptions){.fThickness = 3.0f, .uColor = PL_COLOR_32_GREEN});
        }
    }

    // hardcoded start position (1, 0, 2)
    float fStartX = VIEW_OFFSET_X + 1.0f * CELL_DRAW_SIZE + CELL_DRAW_SIZE * 0.5f;
    float fStartY = VIEW_OFFSET_Y + 1.0f * CELL_DRAW_SIZE + CELL_DRAW_SIZE * 0.5f;
    gptDraw->add_circle_filled(ptAppData->ptLayer, (plVec2){fStartX, fStartY}, CELL_DRAW_SIZE * 0.3f, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_BLUE});

    // hardcoded goal position (8, 0, 7)
    float fGoalX = VIEW_OFFSET_X + 18.0f * CELL_DRAW_SIZE + CELL_DRAW_SIZE * 0.5f;
    float fGoalY = VIEW_OFFSET_Y + 18.0f * CELL_DRAW_SIZE + CELL_DRAW_SIZE * 0.5f;
    gptDraw->add_circle_filled(ptAppData->ptLayer, (plVec2){fGoalX, fGoalY}, CELL_DRAW_SIZE * 0.3f, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_YELLOW});

    // submit drawing
    gptDraw->submit_2d_layer(ptAppData->ptLayer);

    plRenderEncoder* ptEncoder = gptStarter->begin_main_pass();
    plIO* ptIO = gptIO->get_io();
    gptDraw->submit_2d_drawlist(ptAppData->ptDrawlist, ptEncoder, ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y, 1);
    gptStarter->end_main_pass();

    gptStarter->end_frame();
}