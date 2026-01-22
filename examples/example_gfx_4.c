/*
   example_pf_ext_3d.c
     - demonstrates 3D pathfinding visualization
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] apis
// [SECTION] helper function declarations
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
// [SECTION] helper function definitions
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdio.h>
#include "pl.h"
#include "pl_ds.h"
#include "pl_memory.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"
#include <math.h>

// extensions
#include "pl_graphics_ext.h"
#include "pl_draw_ext.h"
#include "pl_starter_ext.h"
#include "pl_path_finding_ext.h"
#include "pl_ui_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plCamera
{
    plVec3 tPos;
    float  fNearZ;
    float  fFarZ;
    float  fFieldOfView;
    float  fAspectRatio;
    plMat4 tViewMat;
    plMat4 tProjMat;
    plMat4 tTransformMat;
    float  fPitch;
    float  fYaw;
    float  fRoll;
    plVec3 _tUpVec;
    plVec3 _tForwardVec;
    plVec3 _tRightVec;
} plCamera;

typedef struct _plVoxel
{
    uint32_t tXPos;
    uint32_t tYPos;
    uint32_t tZPos;
    bool   bOccupied;
    bool   bValid;
} plVoxel;

typedef struct _plAppData
{
    plWindow*      ptWindow;
    plDrawList3D*  pt3dDrawlist;
    plCamera       tCamera;

    // pathfinding
    plPathFindingVoxelGrid* ptVoxelGrid;
    plPathFindingResult     tPathResult;
    plPathFindingQuery      tQuery; 

    // options
    bool bShowWireFrame;
    bool bShowOriginAxes;
    bool bShowObstacles;
    bool bShowHelpWindow;
    bool bEditObstacles;
    bool bShowCrossHair;

    // storing variables for drawing 
    plVec3 tOrigin;
    plVec3 tGridEnd;

    // for app features
    plVec3   tObstacles[400]; // some max number of voxels
    uint32_t uObstacleCount;
    uint32_t uPathSegmentsDrawn;
    float    fPathAnimTimer;
    float    fCurrentSegmentProgress;
    

} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

const plIOI*          gptIO          = NULL;
const plWindowI*      gptWindows     = NULL;
const plGraphicsI*    gptGfx         = NULL;
const plDrawI*        gptDraw        = NULL;
const plStarterI*     gptStarter     = NULL;
const plPathFindingI* gptPathFinding = NULL;
const plUiI*          gptUi          = NULL;

//-----------------------------------------------------------------------------
// [SECTION] helper function declarations
//-----------------------------------------------------------------------------

void        camera_translate(plCamera*, float fDx, float fDy, float fDz);
void        camera_rotate(plCamera*, float fDPitch, float fDYaw);
void        camera_update(plCamera*);
static void load_map(plAppData* ptAppData, uint32_t uMapNumber);
static void set_voxels_maze_one(plPathFindingVoxelGrid* ptGrid, plVec3* tObstacles, uint32_t* uObstacleCount);
static void set_voxels_maze_two(plPathFindingVoxelGrid* ptGrid, plVec3* tObstacles, uint32_t* uObstacleCount);
static void set_voxels_maze_three(plPathFindingVoxelGrid* ptGrid, plVec3* tObstacles, uint32_t* uObstacleCount);
static void draw_voxel_grid_wireframe(plDrawList3D *ptDrawlist, plPathFindingVoxelGrid *ptGrid, plDrawSolidOptions tOptions);
bool        is_voxel_occupied(plPathFindingVoxelGrid* ptGrid, uint32_t uVoxelX, uint32_t uVoxelY, uint32_t uVoxelZ); // TODO: could move to ext 
plVoxel     ray_cast(plCamera* ptCamera, plPathFindingVoxelGrid* ptGrid, plVec3 tRayDirection, uint32_t uMaxDepth, int32_t iStepCounter);
plVec3      get_ray_direction(plCamera* ptCamera);
void        draw_voxel(plDrawList3D* ptDrawlist, plVec3 tPos, uint32_t uFillColor, uint32_t uWireColor);

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryI* ptApiRegistry, plAppData* ptAppData)
{
    if(ptAppData)
    {
        gptIO          = pl_get_api_latest(ptApiRegistry, plIOI);
        gptWindows     = pl_get_api_latest(ptApiRegistry, plWindowI);
        gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);
        gptDraw        = pl_get_api_latest(ptApiRegistry, plDrawI);
        gptStarter     = pl_get_api_latest(ptApiRegistry, plStarterI);
        gptPathFinding = pl_get_api_latest(ptApiRegistry, plPathFindingI);
        gptUi          = pl_get_api_latest(ptApiRegistry, plUiI);
        return ptAppData;
    }

    ptAppData = malloc(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));
    const plExtensionRegistryI* ptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);

    ptExtensionRegistry->load("pl_unity_ext", NULL, NULL, true);
    ptExtensionRegistry->load("pl_platform_ext", NULL, NULL, false);
    
    gptIO          = pl_get_api_latest(ptApiRegistry, plIOI);
    gptWindows     = pl_get_api_latest(ptApiRegistry, plWindowI);
    gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptDraw        = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptStarter     = pl_get_api_latest(ptApiRegistry, plStarterI);
    gptPathFinding = pl_get_api_latest(ptApiRegistry, plPathFindingI);
    gptUi          = pl_get_api_latest(ptApiRegistry, plUiI);

    plWindowDesc tWindowDesc = {
        .pcTitle = "3D Pathfinding Example",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 800,
        .uHeight = 800,
    };
    gptWindows->create(tWindowDesc, &ptAppData->ptWindow);
    gptWindows->show(ptAppData->ptWindow);

    plStarterInit tStarterInit = {
        .tFlags   = PL_STARTER_FLAGS_ALL_EXTENSIONS,
        .ptWindow = ptAppData->ptWindow
    };

    gptStarter->initialize(tStarterInit);
    gptStarter->finalize();

    ptAppData->pt3dDrawlist = gptDraw->request_3d_drawlist();

    plIO* ptIO = gptIO->get_io();
    ptAppData->tCamera = (plCamera){
        .tPos         = {10.0f, 20.0f, 30.0f},
        .fNearZ       = 0.01f,
        .fFarZ        = 100.0f,
        .fFieldOfView = PL_PI_3,
        .fAspectRatio = ptIO->tMainViewportSize.x / ptIO->tMainViewportSize.y,
        .fYaw         = PL_PI + PL_PI_4,
        .fPitch       = -PL_PI_4,
    };
    camera_update(&ptAppData->tCamera);
    camera_rotate(&ptAppData->tCamera, -0.10f, -0.80f);

    // create voxel grid 20x1x20 as default
    ptAppData->tOrigin.x   = 0.0f;
    ptAppData->tOrigin.y   = 0.0f;
    ptAppData->tOrigin.z   = 0.0f;
    ptAppData->tGridEnd.x  = 20.0f;
    ptAppData->tGridEnd.y  = 0.0f;
    ptAppData->tGridEnd.z  = 20.0f;
    ptAppData->ptVoxelGrid = gptPathFinding->create_voxel_grid(20, 1, 20, 1.0f, ptAppData->tOrigin);
    
    // set voxel for default 1D maze
    ptAppData->uObstacleCount = 0;
    set_voxels_maze_one(ptAppData->ptVoxelGrid, ptAppData->tObstacles, &ptAppData->uObstacleCount);

    // set path start and end 
    ptAppData->tQuery.tStart = (plVec3){1.5f, 0.5f, 1.5f};
    ptAppData->tQuery.tGoal  = (plVec3){18.5f, 0.5f, 18.5f};

    // find path and set option defaults
    ptAppData->tPathResult = gptPathFinding->find_path(ptAppData->ptVoxelGrid, &ptAppData->tQuery);
    ptAppData->fPathAnimTimer          = 0.0f;
    ptAppData->uPathSegmentsDrawn      = 0;
    ptAppData->fCurrentSegmentProgress = 0.0f;
    ptAppData->bShowWireFrame          = true;
    ptAppData->bShowOriginAxes         = true;
    ptAppData->bShowObstacles          = true;
    ptAppData->bShowCrossHair          = true;

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

    // clean up pathfinding resources
    gptPathFinding->free_result(&ptAppData->tPathResult);
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
    plIO* ptIO = gptIO->get_io();
    ptAppData->tCamera.fAspectRatio = ptIO->tMainViewportSize.x / ptIO->tMainViewportSize.y;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    if(!gptStarter->begin_frame())
        return;

    plIO* ptIO = gptIO->get_io();

    // loading different mazes
    if(gptIO->is_key_pressed(PL_KEY_1, false))
    {
        load_map(ptAppData, 1);
    }
    if(gptIO->is_key_pressed(PL_KEY_2, false))
    {
        load_map(ptAppData, 2);
    }
    if(gptIO->is_key_pressed(PL_KEY_3, false))
    {
        load_map(ptAppData, 3);
    }

    // creating a help window
    ptAppData->bShowHelpWindow = true;
    if(ptAppData->bShowHelpWindow)
    {
        gptUi->set_next_window_pos((plVec2){10.0f, 10.0f}, PL_UI_COND_ALWAYS);
        if(gptUi->begin_window("Help", NULL, PL_UI_WINDOW_FLAGS_AUTO_SIZE))
        {
            gptUi->layout_static(0.0f, 400.0f, 1);
            gptUi->text("Press Z to toggle Objects");
            gptUi->text("Press X to toddle wire frame");
            gptUi->text("Press C to toggle Origin");
            gptUi->text("Press V to toggle crosshair");
            gptUi->text("Press left shift to edit obstacles");
            gptUi->text("  - Q = delete & E = Add");
            gptUi->text("  - scroll for depth control");
            gptUi->vertical_spacing();
            gptUi->text("Press 1 for 1D maze");
            gptUi->text("Press 2 for 3D maze");
            gptUi->text("Press 3 for blank maze");
            gptUi->end_window();
        }
    }

    // camera controls
    static const float fCameraTravelSpeed = 8.0f;
    static const float fCameraRotationSpeed = 0.005f;
    plCamera* ptCamera = &ptAppData->tCamera;
    if(gptIO->is_key_down(PL_KEY_W)) camera_translate(ptCamera,  0.0f,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime);
    if(gptIO->is_key_down(PL_KEY_S)) camera_translate(ptCamera,  0.0f,  0.0f, -fCameraTravelSpeed * ptIO->fDeltaTime);
    if(gptIO->is_key_down(PL_KEY_A)) camera_translate(ptCamera, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);
    if(gptIO->is_key_down(PL_KEY_D)) camera_translate(ptCamera,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);
    if(gptIO->is_key_down(PL_KEY_F)) camera_translate(ptCamera,  0.0f, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f);
    if(gptIO->is_key_down(PL_KEY_R)) camera_translate(ptCamera,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f);

    // mouse camera controls
    if(gptIO->is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 1.0f))
    {
        const plVec2 tMouseDelta = gptIO->get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f);
        camera_rotate(ptCamera, -tMouseDelta.y * fCameraRotationSpeed, -tMouseDelta.x * fCameraRotationSpeed);
        gptIO->reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
    }
    camera_update(ptCamera);

    // draw corsshairs
    if(gptIO->is_key_pressed(PL_KEY_V, false))
    {
        ptAppData->bShowCrossHair = !ptAppData->bShowCrossHair;
    }
    if(ptAppData->bShowCrossHair)
    {
        float fCrosshairSize = 0.1f; // adjust size as needed
        plVec3 tCameraPos = ptCamera->tPos;
        plVec3 tCameraForward = get_ray_direction(ptCamera);

        float fDistance = 3.0f; // distance from camera
        plVec3 tCenter = {
            .x = tCameraPos.x + tCameraForward.x * fDistance,
            .y = tCameraPos.y + tCameraForward.y * fDistance,
            .z = tCameraPos.z + tCameraForward.z * fDistance
        };
        plVec3 tRight = {-tCameraForward.z, 0, tCameraForward.x}; // perpendicular to forward in XZ plane

        gptDraw->add_3d_line(ptAppData->pt3dDrawlist,
            (plVec3){tCenter.x - tRight.x * fCrosshairSize, tCenter.y, tCenter.z - tRight.z * fCrosshairSize},
            (plVec3){tCenter.x + tRight.x * fCrosshairSize, tCenter.y, tCenter.z + tRight.z * fCrosshairSize},
            (plDrawLineOptions){.uColor = PL_COLOR_32_WHITE, .fThickness = 0.01f});

        gptDraw->add_3d_line(ptAppData->pt3dDrawlist,
            (plVec3){tCenter.x, tCenter.y - fCrosshairSize, tCenter.z},
            (plVec3){tCenter.x, tCenter.y + fCrosshairSize, tCenter.z},
            (plDrawLineOptions){.uColor = PL_COLOR_32_WHITE, .fThickness = 0.01f});

    }

    // draw "ground layer" of maze
    plVec3 tCorner1 = {ptAppData->tOrigin.x,  0, ptAppData->tOrigin.z};   // bottom-left
    plVec3 tCorner2 = {ptAppData->tGridEnd.x, 0, ptAppData->tOrigin.z};   // bottom-right
    plVec3 tCorner3 = {ptAppData->tOrigin.x,  0, ptAppData->tGridEnd.z};  // top-left
    plVec3 tCorner4 = {ptAppData->tGridEnd.x, 0, ptAppData->tGridEnd.z};  // top-right

    gptDraw->add_3d_triangle_filled(ptAppData->pt3dDrawlist,
        tCorner1, tCorner2, tCorner3,
        (plDrawSolidOptions){.uColor = PL_COLOR_32_GREY});
    gptDraw->add_3d_triangle_filled(ptAppData->pt3dDrawlist,
        tCorner2, tCorner4, tCorner3, 
        (plDrawSolidOptions){.uColor = PL_COLOR_32_GREY});

    // draw origin axes
    if(gptIO->is_key_pressed(PL_KEY_C, false))
    {
        ptAppData->bShowOriginAxes = !ptAppData->bShowOriginAxes;
    }
    if(ptAppData->bShowOriginAxes)
    {
        const plMat4 tOrigin = pl_identity_mat4();
        gptDraw->add_3d_transform(ptAppData->pt3dDrawlist, &tOrigin, 5.0f, (plDrawLineOptions){.fThickness = 0.1f});
    }
        
    // draw wire frame grid
    if(gptIO->is_key_pressed(PL_KEY_X, false))
    {
        ptAppData->bShowWireFrame = !ptAppData->bShowWireFrame;
    }
    if(ptAppData->bShowWireFrame)
    {
        draw_voxel_grid_wireframe(ptAppData->pt3dDrawlist, ptAppData->ptVoxelGrid,(plDrawSolidOptions){.uColor = PL_COLOR_32_LIGHT_GREY});
    }

    // TODO: add option to move these
    // draw start and goal spheres 
    gptDraw->add_3d_sphere_filled(ptAppData->pt3dDrawlist, 
        (plSphere){.fRadius = 0.35f, 
        .tCenter = {ptAppData->tQuery.tStart.x, 
                    ptAppData->tQuery.tStart.y, 
                    ptAppData->tQuery.tStart.z}},
        0, 0, 
        (plDrawSolidOptions){.uColor = PL_COLOR_32_MAGENTA});
    gptDraw->add_3d_sphere_filled(ptAppData->pt3dDrawlist, 
        (plSphere){.fRadius = 0.35f, 
        .tCenter = {ptAppData->tQuery.tGoal.x, 
                    ptAppData->tQuery.tGoal.y, 
                    ptAppData->tQuery.tGoal.z}},
        0, 0, 
        (plDrawSolidOptions){.uColor = PL_COLOR_32_CYAN});    

    // edit obstacles TODO: fix depth issue with added voxels
    if(gptIO->is_key_pressed(PL_KEY_LEFT_SHIFT, false))
    {
        ptAppData->bEditObstacles = !ptAppData->bEditObstacles;
    }
    if(ptAppData->bEditObstacles)
    {
        // control depth of voxel add/delete
        static int32_t iGhostDepth = 1;
        float fMouseWheel = gptIO->get_mouse_wheel();
        if(fMouseWheel > 0.0f)
            iGhostDepth++;
        else if(fMouseWheel < 0.0f && iGhostDepth > 1)
            iGhostDepth--;

        plVec3 tRayDir = get_ray_direction(ptCamera);
        plVoxel tVoxelHit = ray_cast(ptCamera, ptAppData->ptVoxelGrid, tRayDir, 10, iGhostDepth);

        // draw ghost voxel at that position
        // (use different color/transparency to show it's a preview)
        if(tVoxelHit.bValid)
        {
            plDrawSolidOptions tGhostOptions;
            plDrawSolidOptions tGhostBorderOptions;
            if(tVoxelHit.tYPos == 0) 
            {tGhostOptions.uColor = PL_COLOR_32_RGBA(1.0f, 0.75f, 0.75f, 1.0f); tGhostBorderOptions.uColor = PL_COLOR_32_RGBA(0.545f, 0.0f, 0.0f, 1.0f);}
            else if(tVoxelHit.tYPos == 1) 
            {tGhostOptions.uColor = PL_COLOR_32_RGBA(0.75f, 1.0f, 0.75f, 1.0f); tGhostBorderOptions.uColor = PL_COLOR_32_RGBA(0.0f, 0.545f, 0.0f, 1.0f);}
            else if(tVoxelHit.tYPos == 2) 
            {tGhostOptions.uColor = PL_COLOR_32_RGBA(0.75f, 0.75f, 1.0f, 1.0f); tGhostBorderOptions.uColor = PL_COLOR_32_RGBA(0.0f, 0.0f, 0.545f, 1.0f);}
            else
            {tGhostOptions.uColor = PL_COLOR_32_LIGHT_GREY; tGhostBorderOptions.uColor = PL_COLOR_32_RGBA(0.25f, 0.25f, 0.25f, 1.0f);}
            draw_voxel(ptAppData->pt3dDrawlist, 
                (plVec3){(float)tVoxelHit.tXPos, 
                         (float)tVoxelHit.tYPos, 
                         (float)tVoxelHit.tZPos},
                tGhostOptions.uColor, 
                tGhostBorderOptions.uColor);
        }

        // add and remove voxel from key press
        if(gptIO->is_key_pressed(PL_KEY_E, false) && tVoxelHit.bValid)
        {
            ptAppData->tObstacles[ptAppData->uObstacleCount++] = (plVec3){(float)tVoxelHit.tXPos, 
                                                                          (float)tVoxelHit.tYPos, 
                                                                          (float)tVoxelHit.tZPos};

            gptPathFinding->set_voxel(ptAppData->ptVoxelGrid, tVoxelHit.tXPos, tVoxelHit.tYPos, tVoxelHit.tZPos, true);
            ptAppData->tPathResult = gptPathFinding->find_path(ptAppData->ptVoxelGrid, &ptAppData->tQuery);

            // reset path drawing 
            ptAppData->uPathSegmentsDrawn = 0;
            ptAppData->fCurrentSegmentProgress = 0.0f;
        }   

        if(gptIO->is_key_pressed(PL_KEY_Q, false) && tVoxelHit.bOccupied) // if occupied it should be a valid voxel
        { //TODO: how to draw over occupied voxel to clearly denote we can delete
            // find and remove from obstacles array
            for(uint32_t i = 0; i < ptAppData->uObstacleCount; i++)
            {
                if(ptAppData->tObstacles[i].x == (float)tVoxelHit.tXPos &&
                   ptAppData->tObstacles[i].y == (float)tVoxelHit.tYPos &&
                   ptAppData->tObstacles[i].z == (float)tVoxelHit.tZPos)
                {
                    // swap delete from array
                    ptAppData->tObstacles[i] = ptAppData->tObstacles[ptAppData->uObstacleCount - 1];
                    ptAppData->uObstacleCount--;
                    break;
                }
            }
            gptPathFinding->set_voxel(ptAppData->ptVoxelGrid, tVoxelHit.tXPos, tVoxelHit.tYPos, tVoxelHit.tZPos, false);
            ptAppData->tPathResult = gptPathFinding->find_path(ptAppData->ptVoxelGrid, &ptAppData->tQuery);

            // reset path drawing 
            ptAppData->uPathSegmentsDrawn = 0;
            ptAppData->fCurrentSegmentProgress = 0.0f;
        }
    }

    // obstacle drawing
    if(gptIO->is_key_pressed(PL_KEY_Z, false))
    {
        ptAppData->bShowObstacles = !ptAppData->bShowObstacles;
    } 
    if(ptAppData->bShowObstacles)
    {
        for(uint32_t i = 0; i < ptAppData->uObstacleCount; i++)
        {
            float fCenterX = ptAppData->tObstacles[i].x + 0.5f;
            float fCenterY = ptAppData->tObstacles[i].y + 0.5f;
            float fCenterZ = ptAppData->tObstacles[i].z + 0.5f;

            // change color for each layer
            plDrawSolidOptions tOptions;
            plDrawSolidOptions tDarkOptions;
            if(fCenterY == 0.5f) 
            {tOptions.uColor = PL_COLOR_32_RED; tDarkOptions.uColor = PL_COLOR_32_RGBA(0.545f, 0.0f, 0.0f, 1.0f);}
            else if(fCenterY == 1.5f) 
            {tOptions.uColor = PL_COLOR_32_GREEN; tDarkOptions.uColor = PL_COLOR_32_RGBA(0.0f, 0.545f, 0.0f, 1.0f);}
            else if(fCenterY == 2.5f) 
            {tOptions.uColor = PL_COLOR_32_BLUE; tDarkOptions.uColor = PL_COLOR_32_DARK_BLUE;}
            else // default
            {tOptions.uColor = PL_COLOR_32_WHITE; tDarkOptions.uColor = PL_COLOR_32_RGBA(128, 128, 128, 255);}
            
            draw_voxel(ptAppData->pt3dDrawlist, ptAppData->tObstacles[i], tOptions.uColor, 
                tDarkOptions.uColor);
        }
    }

    // draw path
    // TODO: figure out path depth buffering issue 
    if(ptAppData->tPathResult.bSuccess)
    {
        ptAppData->fPathAnimTimer += ptIO->fDeltaTime;

        // increment progress every frame
        float fSegmentSpeed = 20.0f; // segments per second
        float fProgressIncrement = fSegmentSpeed * ptIO->fDeltaTime;
        ptAppData->fCurrentSegmentProgress += fProgressIncrement;

        if(ptAppData->fCurrentSegmentProgress >= 1.0f && ptAppData->uPathSegmentsDrawn < ptAppData->tPathResult.uWaypointCount - 1)
        {
            ptAppData->uPathSegmentsDrawn++;
            ptAppData->fCurrentSegmentProgress = 0.0f;
        }

        float fRadius = 0.15f; // thickness 
        uint32_t uSegments = 8;

        for(uint32_t i = 0; i < ptAppData->uPathSegmentsDrawn; i++)
        {
            plCylinder tCylinder = {
                .tTipPos = ptAppData->tPathResult.atWaypoints[i], 
                .tBasePos = ptAppData->tPathResult.atWaypoints[i + 1], 
                .fRadius = fRadius
            };
            gptDraw->add_3d_cylinder_filled(ptAppData->pt3dDrawlist, tCylinder, uSegments,
                (plDrawSolidOptions){.uColor = PL_COLOR_32_DARK_BLUE});
        }
        // draw current partial segment
        if(ptAppData->uPathSegmentsDrawn < ptAppData->tPathResult.uWaypointCount - 1)
        {
            plVec3 tStart = ptAppData->tPathResult.atWaypoints[ptAppData->uPathSegmentsDrawn];
            plVec3 tEnd = ptAppData->tPathResult.atWaypoints[ptAppData->uPathSegmentsDrawn + 1];

            plVec3 tPartialEnd = {
                tStart.x + (tEnd.x - tStart.x) * ptAppData->fCurrentSegmentProgress,
                tStart.y + (tEnd.y - tStart.y) * ptAppData->fCurrentSegmentProgress,
                tStart.z + (tEnd.z - tStart.z) * ptAppData->fCurrentSegmentProgress
            };

            plCylinder tCylinder = {.tTipPos = tStart, .tBasePos = tPartialEnd, .fRadius = fRadius};
            gptDraw->add_3d_cylinder_filled(ptAppData->pt3dDrawlist, tCylinder, uSegments,
                (plDrawSolidOptions){.uColor = PL_COLOR_32_DARK_BLUE});
        }
    }

    // submit 3D drawlist
    plRenderEncoder* ptEncoder = gptStarter->begin_main_pass();

    const plMat4 tMVP = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);

    gptDraw->submit_3d_drawlist(ptAppData->pt3dDrawlist,
        ptEncoder,
        ptIO->tMainViewportSize.x,
        ptIO->tMainViewportSize.y,
        &tMVP,
        PL_DRAW_FLAG_DEPTH_TEST | PL_DRAW_FLAG_DEPTH_WRITE | PL_DRAW_FLAG_CULL_FRONT,
        1);

    gptStarter->end_main_pass();
    gptStarter->end_frame(); 
}

//-----------------------------------------------------------------------------
// [SECTION] helper function definitions
//-----------------------------------------------------------------------------

static inline float
wrap_angle(float tTheta)
{
    static const float f2Pi = 2.0f * PL_PI;
    const float fMod = fmodf(tTheta, f2Pi);
    if (fMod > PL_PI)       return fMod - f2Pi;
    else if (fMod < -PL_PI) return fMod + f2Pi;
    return fMod;
}

void
camera_translate(plCamera* ptCamera, float fDx, float fDy, float fDz)
{
    ptCamera->tPos = pl_add_vec3(ptCamera->tPos, pl_mul_vec3_scalarf(ptCamera->_tRightVec, fDx));
    ptCamera->tPos = pl_add_vec3(ptCamera->tPos, pl_mul_vec3_scalarf(ptCamera->_tForwardVec, fDz));
    ptCamera->tPos.y += fDy;
}

void
camera_rotate(plCamera* ptCamera, float fDPitch, float fDYaw)
{
    ptCamera->fPitch += fDPitch;
    ptCamera->fYaw += fDYaw;
    ptCamera->fYaw = wrap_angle(ptCamera->fYaw);
    ptCamera->fPitch = pl_clampf(0.995f * -PL_PI_2, ptCamera->fPitch, 0.995f * PL_PI_2);
}

void
camera_update(plCamera* ptCamera)
{
    static const plVec4 tOriginalUpVec      = {0.0f, 1.0f, 0.0f, 0.0f};
    static const plVec4 tOriginalForwardVec = {0.0f, 0.0f, 1.0f, 0.0f};
    static const plVec4 tOriginalRightVec   = {-1.0f, 0.0f, 0.0f, 0.0f};

    const plMat4 tXRotMat   = pl_mat4_rotate_vec3(ptCamera->fPitch, tOriginalRightVec.xyz);
    const plMat4 tYRotMat   = pl_mat4_rotate_vec3(ptCamera->fYaw, tOriginalUpVec.xyz);
    const plMat4 tZRotMat   = pl_mat4_rotate_vec3(ptCamera->fRoll, tOriginalForwardVec.xyz);
    const plMat4 tTranslate = pl_mat4_translate_vec3((plVec3){ptCamera->tPos.x, ptCamera->tPos.y, ptCamera->tPos.z});

    plMat4 tRotations = pl_mul_mat4t(&tXRotMat, &tZRotMat);
    tRotations        = pl_mul_mat4t(&tYRotMat, &tRotations);

    ptCamera->_tRightVec   = pl_norm_vec4(pl_mul_mat4_vec4(&tRotations, tOriginalRightVec)).xyz;
    ptCamera->_tUpVec      = pl_norm_vec4(pl_mul_mat4_vec4(&tRotations, tOriginalUpVec)).xyz;
    ptCamera->_tForwardVec = pl_norm_vec4(pl_mul_mat4_vec4(&tRotations, tOriginalForwardVec)).xyz;

    ptCamera->tTransformMat = pl_mul_mat4t(&tTranslate, &tRotations);
    ptCamera->tViewMat      = pl_mat4t_invert(&ptCamera->tTransformMat);

    const plMat4 tFlipXY = pl_mat4_scale_xyz(-1.0f, -1.0f, 1.0f);
    ptCamera->tViewMat   = pl_mul_mat4t(&tFlipXY, &ptCamera->tViewMat);

    const float fInvtanHalfFovy = 1.0f / tanf(ptCamera->fFieldOfView / 2.0f);
    ptCamera->tProjMat.col[0].x = fInvtanHalfFovy / ptCamera->fAspectRatio;
    ptCamera->tProjMat.col[1].y = fInvtanHalfFovy;
    ptCamera->tProjMat.col[2].z = ptCamera->fFarZ / (ptCamera->fFarZ - ptCamera->fNearZ);
    ptCamera->tProjMat.col[2].w = 1.0f;
    ptCamera->tProjMat.col[3].z = -ptCamera->fNearZ * ptCamera->fFarZ / (ptCamera->fFarZ - ptCamera->fNearZ);
    ptCamera->tProjMat.col[3].w = 0.0f;  
}

void 
draw_voxel_grid_wireframe(plDrawList3D* ptDrawlist, plPathFindingVoxelGrid* ptGrid, plDrawSolidOptions tOptions)
{
    float fSize = ptGrid->fVoxelSize;
    float fRadius = fSize * 0.01f; // adjust thickness
    uint32_t uSegments = 8; // adjust smoothness
    
    // draw x lines
    for(uint32_t y = 0; y <= ptGrid->uDimY; y++)
    {
        for(uint32_t z = 0; z <= ptGrid->uDimZ; z++)
        {
            plVec3 p0 = {ptGrid->tOrigin.x, ptGrid->tOrigin.y + y * fSize, ptGrid->tOrigin.z + z * fSize};
            plVec3 p1 = {ptGrid->tOrigin.x + ptGrid->uDimX * fSize, ptGrid->tOrigin.y + y * fSize, ptGrid->tOrigin.z + z * fSize};
            plCylinder cylinder = {.tTipPos = p0, .tBasePos = p1, .fRadius = fRadius};
            gptDraw->add_3d_cylinder_filled(ptDrawlist, cylinder, uSegments, tOptions);
        }
    }
    
    // draw y lines
    for(uint32_t x = 0; x <= ptGrid->uDimX; x++)
    {
        for(uint32_t z = 0; z <= ptGrid->uDimZ; z++)
        {
            plVec3 p0 = {ptGrid->tOrigin.x + x * fSize, ptGrid->tOrigin.y, ptGrid->tOrigin.z + z * fSize};
            plVec3 p1 = {ptGrid->tOrigin.x + x * fSize, ptGrid->tOrigin.y + ptGrid->uDimY * fSize, ptGrid->tOrigin.z + z * fSize};
            plCylinder cylinder = {.tTipPos = p0, .tBasePos = p1, .fRadius = fRadius};
            gptDraw->add_3d_cylinder_filled(ptDrawlist, cylinder, uSegments, tOptions);
        }
    }
    
    // draw z lines
    for(uint32_t x = 0; x <= ptGrid->uDimX; x++)
    {
        for(uint32_t y = 0; y <= ptGrid->uDimY; y++)
        {
            plVec3 p0 = {ptGrid->tOrigin.x + x * fSize, ptGrid->tOrigin.y + y * fSize, ptGrid->tOrigin.z};
            plVec3 p1 = {ptGrid->tOrigin.x + x * fSize, ptGrid->tOrigin.y + y * fSize, ptGrid->tOrigin.z + ptGrid->uDimZ * fSize};
            plCylinder cylinder = {.tTipPos = p0, .tBasePos = p1, .fRadius = fRadius};
            gptDraw->add_3d_cylinder_filled(ptDrawlist, cylinder, uSegments, tOptions);
        }
    }
}

static void
load_map(plAppData* ptAppData, uint32_t uMapNumber)
{
    gptPathFinding->destroy_voxel_grid(ptAppData->ptVoxelGrid);
    
    // create new maps grid
    plVec3 tOrigin = {0.0f, 0.0f, 0.0f};
    uint32_t uMapLayers = 0;
    if(uMapNumber == 1) uMapLayers = 1;
    if(uMapNumber == 2) uMapLayers = 3;
    if(uMapNumber == 3) uMapLayers = 1;

    ptAppData->ptVoxelGrid = gptPathFinding->create_voxel_grid(20, uMapLayers, 20, 1.0f, tOrigin);
    
    // load map-specific obstacles
    switch(uMapNumber)
    {
        case 1:
            set_voxels_maze_one(ptAppData->ptVoxelGrid, ptAppData->tObstacles, &ptAppData->uObstacleCount);
            ptAppData->tQuery.tStart = (plVec3){1.5f, 0.5f, 1.5f};
            ptAppData->tQuery.tGoal = (plVec3){18.5f, 0.5f, 18.5f};
            break;
        case 2:
            set_voxels_maze_two(ptAppData->ptVoxelGrid, ptAppData->tObstacles, &ptAppData->uObstacleCount);
            ptAppData->tQuery.tStart = (plVec3){2.5f, 0.5f, 2.5f};
            ptAppData->tQuery.tGoal = (plVec3){18.5f, 2.5f, 18.5f};
            break;
        case 3:
            set_voxels_maze_three(ptAppData->ptVoxelGrid, ptAppData->tObstacles, &ptAppData->uObstacleCount);
            ptAppData->tQuery.tStart = (plVec3){1.5f, 0.5f, 1.5f};
            ptAppData->tQuery.tGoal = (plVec3){18.5f, 0.5f, 18.5f};
            break;
    }
    ptAppData->tPathResult = gptPathFinding->find_path(ptAppData->ptVoxelGrid, &ptAppData->tQuery);
    
    // reset animation and settings
    ptAppData->fPathAnimTimer = 0.0f;
    ptAppData->uPathSegmentsDrawn = 0;
}

static void
set_voxels_maze_one(plPathFindingVoxelGrid* ptGrid, plVec3* tObstacles, uint32_t* uObstacleCount)
{
    *uObstacleCount = 0;
    
    // set voxels and store coordinates
    tObstacles[(*uObstacleCount)++] = (plVec3){2, 0, 0};
    gptPathFinding->set_voxel(ptGrid, 2, 0, 0, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){2, 0, 1};
    gptPathFinding->set_voxel(ptGrid, 2, 0, 1, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){4, 0, 1};
    gptPathFinding->set_voxel(ptGrid, 4, 0, 1, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){7, 0, 1};
    gptPathFinding->set_voxel(ptGrid, 7, 0, 1, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){8, 0, 1};
    gptPathFinding->set_voxel(ptGrid, 8, 0, 1, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){9, 0, 1};
    gptPathFinding->set_voxel(ptGrid, 9, 0, 1, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){10, 0, 1};
    gptPathFinding->set_voxel(ptGrid, 10, 0, 1, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){11, 0, 1};
    gptPathFinding->set_voxel(ptGrid, 11, 0, 1, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){12, 0, 1};
    gptPathFinding->set_voxel(ptGrid, 12, 0, 1, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){13, 0, 1};
    gptPathFinding->set_voxel(ptGrid, 13, 0, 1, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){14, 0, 1};
    gptPathFinding->set_voxel(ptGrid, 14, 0, 1, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){15, 0, 1};
    gptPathFinding->set_voxel(ptGrid, 15, 0, 1, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){16, 0, 1};
    gptPathFinding->set_voxel(ptGrid, 16, 0, 1, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){17, 0, 1};
    gptPathFinding->set_voxel(ptGrid, 17, 0, 1, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){18, 0, 1};
    gptPathFinding->set_voxel(ptGrid, 18, 0, 1, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){2, 0, 2};
    gptPathFinding->set_voxel(ptGrid, 2, 0, 2, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){4, 0, 2};
    gptPathFinding->set_voxel(ptGrid, 4, 0, 2, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){5, 0, 2};
    gptPathFinding->set_voxel(ptGrid, 5, 0, 2, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){6, 0, 2};
    gptPathFinding->set_voxel(ptGrid, 6, 0, 2, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){7, 0, 2};
    gptPathFinding->set_voxel(ptGrid, 7, 0, 2, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){4, 0, 3};
    gptPathFinding->set_voxel(ptGrid, 4, 0, 3, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){9, 0, 3};
    gptPathFinding->set_voxel(ptGrid, 9, 0, 3, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){10, 0, 3};
    gptPathFinding->set_voxel(ptGrid, 10, 0, 3, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){11, 0, 3};
    gptPathFinding->set_voxel(ptGrid, 11, 0, 3, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){12, 0, 3};
    gptPathFinding->set_voxel(ptGrid, 12, 0, 3, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){13, 0, 3};
    gptPathFinding->set_voxel(ptGrid, 13, 0, 3, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){14, 0, 3};
    gptPathFinding->set_voxel(ptGrid, 14, 0, 3, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){15, 0, 3};
    gptPathFinding->set_voxel(ptGrid, 15, 0, 3, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){16, 0, 3};
    gptPathFinding->set_voxel(ptGrid, 16, 0, 3, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){17, 0, 3};
    gptPathFinding->set_voxel(ptGrid, 17, 0, 3, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){18, 0, 3};
    gptPathFinding->set_voxel(ptGrid, 18, 0, 3, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){19, 0, 3};
    gptPathFinding->set_voxel(ptGrid, 19, 0, 3, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){0, 0, 4};
    gptPathFinding->set_voxel(ptGrid, 0, 0, 4, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){1, 0, 4};
    gptPathFinding->set_voxel(ptGrid, 1, 0, 4, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){2, 0, 4};
    gptPathFinding->set_voxel(ptGrid, 2, 0, 4, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){3, 0, 4};
    gptPathFinding->set_voxel(ptGrid, 3, 0, 4, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){4, 0, 4};
    gptPathFinding->set_voxel(ptGrid, 4, 0, 4, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){7, 0, 4};
    gptPathFinding->set_voxel(ptGrid, 7, 0, 4, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){8, 0, 4};
    gptPathFinding->set_voxel(ptGrid, 8, 0, 4, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){9, 0, 4};
    gptPathFinding->set_voxel(ptGrid, 9, 0, 4, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){14, 0, 4};
    gptPathFinding->set_voxel(ptGrid, 14, 0, 4, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){15, 0, 4};
    gptPathFinding->set_voxel(ptGrid, 15, 0, 4, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){16, 0, 4};
    gptPathFinding->set_voxel(ptGrid, 16, 0, 4, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){8, 0, 5};
    gptPathFinding->set_voxel(ptGrid, 8, 0, 5, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){11, 0, 5};
    gptPathFinding->set_voxel(ptGrid, 11, 0, 5, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){16, 0, 5};
    gptPathFinding->set_voxel(ptGrid, 16, 0, 5, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){17, 0, 5};
    gptPathFinding->set_voxel(ptGrid, 17, 0, 5, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){18, 0, 5};
    gptPathFinding->set_voxel(ptGrid, 18, 0, 5, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){1, 0, 6};
    gptPathFinding->set_voxel(ptGrid, 1, 0, 6, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){3, 0, 6};
    gptPathFinding->set_voxel(ptGrid, 3, 0, 6, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){4, 0, 6};
    gptPathFinding->set_voxel(ptGrid, 4, 0, 6, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){5, 0, 6};
    gptPathFinding->set_voxel(ptGrid, 5, 0, 6, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){6, 0, 6};
    gptPathFinding->set_voxel(ptGrid, 6, 0, 6, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){8, 0, 6};
    gptPathFinding->set_voxel(ptGrid, 8, 0, 6, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){10, 0, 6};
    gptPathFinding->set_voxel(ptGrid, 10, 0, 6, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){11, 0, 6};
    gptPathFinding->set_voxel(ptGrid, 11, 0, 6, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){12, 0, 6};
    gptPathFinding->set_voxel(ptGrid, 12, 0, 6, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){13, 0, 6};
    gptPathFinding->set_voxel(ptGrid, 13, 0, 6, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){14, 0, 6};
    gptPathFinding->set_voxel(ptGrid, 14, 0, 6, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){16, 0, 6};
    gptPathFinding->set_voxel(ptGrid, 16, 0, 6, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){18, 0, 6};
    gptPathFinding->set_voxel(ptGrid, 18, 0, 6, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){1, 0, 7};
    gptPathFinding->set_voxel(ptGrid, 1, 0, 7, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){3, 0, 7};
    gptPathFinding->set_voxel(ptGrid, 3, 0, 7, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){6, 0, 7};
    gptPathFinding->set_voxel(ptGrid, 6, 0, 7, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){8, 0, 7};
    gptPathFinding->set_voxel(ptGrid, 8, 0, 7, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){10, 0, 7};
    gptPathFinding->set_voxel(ptGrid, 10, 0, 7, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){14, 0, 7};
    gptPathFinding->set_voxel(ptGrid, 14, 0, 7, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){16, 0, 7};
    gptPathFinding->set_voxel(ptGrid, 16, 0, 7, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){1, 0, 8};
    gptPathFinding->set_voxel(ptGrid, 1, 0, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){3, 0, 8};
    gptPathFinding->set_voxel(ptGrid, 3, 0, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){8, 0, 8};
    gptPathFinding->set_voxel(ptGrid, 8, 0, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){10, 0, 8};
    gptPathFinding->set_voxel(ptGrid, 10, 0, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){12, 0, 8};
    gptPathFinding->set_voxel(ptGrid, 12, 0, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){14, 0, 8};
    gptPathFinding->set_voxel(ptGrid, 14, 0, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){1, 0, 9};
    gptPathFinding->set_voxel(ptGrid, 1, 0, 9, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){3, 0, 9};
    gptPathFinding->set_voxel(ptGrid, 3, 0, 9, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){4, 0, 9};
    gptPathFinding->set_voxel(ptGrid, 4, 0, 9, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){5, 0, 9};
    gptPathFinding->set_voxel(ptGrid, 5, 0, 9, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){6, 0, 9};
    gptPathFinding->set_voxel(ptGrid, 6, 0, 9, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){7, 0, 9};
    gptPathFinding->set_voxel(ptGrid, 7, 0, 9, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){8, 0, 9};
    gptPathFinding->set_voxel(ptGrid, 8, 0, 9, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){10, 0, 9};
    gptPathFinding->set_voxel(ptGrid, 10, 0, 9, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){12, 0, 9};
    gptPathFinding->set_voxel(ptGrid, 12, 0, 9, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){14, 0, 9};
    gptPathFinding->set_voxel(ptGrid, 14, 0, 9, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){15, 0, 9};
    gptPathFinding->set_voxel(ptGrid, 15, 0, 9, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){16, 0, 9};
    gptPathFinding->set_voxel(ptGrid, 16, 0, 9, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){17, 0, 9};
    gptPathFinding->set_voxel(ptGrid, 17, 0, 9, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){18, 0, 9};
    gptPathFinding->set_voxel(ptGrid, 18, 0, 9, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){1, 0, 10};
    gptPathFinding->set_voxel(ptGrid, 1, 0, 10, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){2, 0, 10};
    gptPathFinding->set_voxel(ptGrid, 2, 0, 10, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){3, 0, 10};
    gptPathFinding->set_voxel(ptGrid, 3, 0, 10, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){7, 0, 10};
    gptPathFinding->set_voxel(ptGrid, 7, 0, 10, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){10, 0, 10};
    gptPathFinding->set_voxel(ptGrid, 10, 0, 10, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){11, 0, 10};
    gptPathFinding->set_voxel(ptGrid, 11, 0, 10, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){12, 0, 10};
    gptPathFinding->set_voxel(ptGrid, 12, 0, 10, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){14, 0, 10};
    gptPathFinding->set_voxel(ptGrid, 14, 0, 10, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){1, 0, 11};
    gptPathFinding->set_voxel(ptGrid, 1, 0, 11, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){3, 0, 11};
    gptPathFinding->set_voxel(ptGrid, 3, 0, 11, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){5, 0, 11};
    gptPathFinding->set_voxel(ptGrid, 5, 0, 11, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){7, 0, 11};
    gptPathFinding->set_voxel(ptGrid, 7, 0, 11, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){9, 0, 11};
    gptPathFinding->set_voxel(ptGrid, 9, 0, 11, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){10, 0, 11};
    gptPathFinding->set_voxel(ptGrid, 10, 0, 11, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){16, 0, 11};
    gptPathFinding->set_voxel(ptGrid, 16, 0, 11, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){17, 0, 11};
    gptPathFinding->set_voxel(ptGrid, 17, 0, 11, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){19, 0, 11};
    gptPathFinding->set_voxel(ptGrid, 19, 0, 11, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){1, 0, 12};
    gptPathFinding->set_voxel(ptGrid, 1, 0, 12, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){3, 0, 12};
    gptPathFinding->set_voxel(ptGrid, 3, 0, 12, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){5, 0, 12};
    gptPathFinding->set_voxel(ptGrid, 5, 0, 12, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){9, 0, 12};
    gptPathFinding->set_voxel(ptGrid, 9, 0, 12, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){12, 0, 12};
    gptPathFinding->set_voxel(ptGrid, 12, 0, 12, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){13, 0, 12};
    gptPathFinding->set_voxel(ptGrid, 13, 0, 12, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){14, 0, 12};
    gptPathFinding->set_voxel(ptGrid, 14, 0, 12, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){15, 0, 12};
    gptPathFinding->set_voxel(ptGrid, 15, 0, 12, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){16, 0, 12};
    gptPathFinding->set_voxel(ptGrid, 16, 0, 12, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){17, 0, 12};
    gptPathFinding->set_voxel(ptGrid, 17, 0, 12, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){18, 0, 12};
    gptPathFinding->set_voxel(ptGrid, 18, 0, 12, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){19, 0, 12};
    gptPathFinding->set_voxel(ptGrid, 19, 0, 12, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){1, 0, 13};
    gptPathFinding->set_voxel(ptGrid, 1, 0, 13, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){3, 0, 13};
    gptPathFinding->set_voxel(ptGrid, 3, 0, 13, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){5, 0, 13};
    gptPathFinding->set_voxel(ptGrid, 5, 0, 13, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){6, 0, 13};
    gptPathFinding->set_voxel(ptGrid, 6, 0, 13, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){7, 0, 13};
    gptPathFinding->set_voxel(ptGrid, 7, 0, 13, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){8, 0, 13};
    gptPathFinding->set_voxel(ptGrid, 8, 0, 13, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){9, 0, 13};
    gptPathFinding->set_voxel(ptGrid, 9, 0, 13, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){11, 0, 13};
    gptPathFinding->set_voxel(ptGrid, 11, 0, 13, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){12, 0, 13};
    gptPathFinding->set_voxel(ptGrid, 12, 0, 13, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){14, 0, 13};
    gptPathFinding->set_voxel(ptGrid, 14, 0, 13, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){19, 0, 13};
    gptPathFinding->set_voxel(ptGrid, 19, 0, 13, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){1, 0, 14};
    gptPathFinding->set_voxel(ptGrid, 1, 0, 14, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){3, 0, 14};
    gptPathFinding->set_voxel(ptGrid, 3, 0, 14, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){5, 0, 14};
    gptPathFinding->set_voxel(ptGrid, 5, 0, 14, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){11, 0, 14};
    gptPathFinding->set_voxel(ptGrid, 11, 0, 14, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){12, 0, 14};
    gptPathFinding->set_voxel(ptGrid, 12, 0, 14, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){14, 0, 14};
    gptPathFinding->set_voxel(ptGrid, 14, 0, 14, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){1, 0, 15};
    gptPathFinding->set_voxel(ptGrid, 1, 0, 15, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){5, 0, 15};
    gptPathFinding->set_voxel(ptGrid, 5, 0, 15, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){8, 0, 15};
    gptPathFinding->set_voxel(ptGrid, 8, 0, 15, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){9, 0, 15};
    gptPathFinding->set_voxel(ptGrid, 9, 0, 15, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){10, 0, 15};
    gptPathFinding->set_voxel(ptGrid, 10, 0, 15, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){11, 0, 15};
    gptPathFinding->set_voxel(ptGrid, 11, 0, 15, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){12, 0, 15};
    gptPathFinding->set_voxel(ptGrid, 12, 0, 15, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){14, 0, 15};
    gptPathFinding->set_voxel(ptGrid, 14, 0, 15, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){16, 0, 15};
    gptPathFinding->set_voxel(ptGrid, 16, 0, 15, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){17, 0, 15};
    gptPathFinding->set_voxel(ptGrid, 17, 0, 15, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){18, 0, 15};
    gptPathFinding->set_voxel(ptGrid, 18, 0, 15, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){1, 0, 16};
    gptPathFinding->set_voxel(ptGrid, 1, 0, 16, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){3, 0, 16};
    gptPathFinding->set_voxel(ptGrid, 3, 0, 16, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){4, 0, 16};
    gptPathFinding->set_voxel(ptGrid, 4, 0, 16, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){5, 0, 16};
    gptPathFinding->set_voxel(ptGrid, 5, 0, 16, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){6, 0, 16};
    gptPathFinding->set_voxel(ptGrid, 6, 0, 16, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){8, 0, 16};
    gptPathFinding->set_voxel(ptGrid, 8, 0, 16, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){10, 0, 16};
    gptPathFinding->set_voxel(ptGrid, 10, 0, 16, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){14, 0, 16};
    gptPathFinding->set_voxel(ptGrid, 14, 0, 16, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){16, 0, 16};
    gptPathFinding->set_voxel(ptGrid, 16, 0, 16, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){18, 0, 16};
    gptPathFinding->set_voxel(ptGrid, 18, 0, 16, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){1, 0, 17};
    gptPathFinding->set_voxel(ptGrid, 1, 0, 17, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){3, 0, 17};
    gptPathFinding->set_voxel(ptGrid, 3, 0, 17, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){6, 0, 17};
    gptPathFinding->set_voxel(ptGrid, 6, 0, 17, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){8, 0, 17};
    gptPathFinding->set_voxel(ptGrid, 8, 0, 17, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){10, 0, 17};
    gptPathFinding->set_voxel(ptGrid, 10, 0, 17, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){12, 0, 17};
    gptPathFinding->set_voxel(ptGrid, 12, 0, 17, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){14, 0, 17};
    gptPathFinding->set_voxel(ptGrid, 14, 0, 17, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){16, 0, 17};
    gptPathFinding->set_voxel(ptGrid, 16, 0, 17, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){1, 0, 18};
    gptPathFinding->set_voxel(ptGrid, 1, 0, 18, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){3, 0, 18};
    gptPathFinding->set_voxel(ptGrid, 3, 0, 18, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){6, 0, 18};
    gptPathFinding->set_voxel(ptGrid, 6, 0, 18, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){10, 0, 18};
    gptPathFinding->set_voxel(ptGrid, 10, 0, 18, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){12, 0, 18};
    gptPathFinding->set_voxel(ptGrid, 12, 0, 18, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){14, 0, 18};
    gptPathFinding->set_voxel(ptGrid, 14, 0, 18, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){16, 0, 18};
    gptPathFinding->set_voxel(ptGrid, 16, 0, 18, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){3, 0, 19};
    gptPathFinding->set_voxel(ptGrid, 3, 0, 19, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){8, 0, 19};
    gptPathFinding->set_voxel(ptGrid, 8, 0, 19, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){12, 0, 19};
    gptPathFinding->set_voxel(ptGrid, 12, 0, 19, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){16, 0, 19};
    gptPathFinding->set_voxel(ptGrid, 16, 0, 19, true);

}

static void
set_voxels_maze_two(plPathFindingVoxelGrid* ptGrid, plVec3* tObstacles, uint32_t* uObstacleCount)
{
    *uObstacleCount = 0;
    
    // layer 0 (y = 0)
    tObstacles[(*uObstacleCount)++] = (plVec3){4, 0, 2};
    gptPathFinding->set_voxel(ptGrid, 4, 0, 2, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){7, 0, 2};
    gptPathFinding->set_voxel(ptGrid, 7, 0, 2, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){10, 0, 2};
    gptPathFinding->set_voxel(ptGrid, 10, 0, 2, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){13, 0, 2};
    gptPathFinding->set_voxel(ptGrid, 13, 0, 2, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){16, 0, 2};
    gptPathFinding->set_voxel(ptGrid, 16, 0, 2, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){3, 0, 5};
    gptPathFinding->set_voxel(ptGrid, 3, 0, 5, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){7, 0, 5};
    gptPathFinding->set_voxel(ptGrid, 7, 0, 5, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){11, 0, 5};
    gptPathFinding->set_voxel(ptGrid, 11, 0, 5, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){15, 0, 5};
    gptPathFinding->set_voxel(ptGrid, 15, 0, 5, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){3, 0, 6};
    gptPathFinding->set_voxel(ptGrid, 3, 0, 6, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){7, 0, 6};
    gptPathFinding->set_voxel(ptGrid, 7, 0, 6, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){11, 0, 6};
    gptPathFinding->set_voxel(ptGrid, 11, 0, 6, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){15, 0, 6};
    gptPathFinding->set_voxel(ptGrid, 15, 0, 6, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){3, 0, 7};
    gptPathFinding->set_voxel(ptGrid, 3, 0, 7, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){7, 0, 7};
    gptPathFinding->set_voxel(ptGrid, 7, 0, 7, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){11, 0, 7};
    gptPathFinding->set_voxel(ptGrid, 11, 0, 7, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){15, 0, 7};
    gptPathFinding->set_voxel(ptGrid, 15, 0, 7, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){1, 0, 8};
    gptPathFinding->set_voxel(ptGrid, 1, 0, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){3, 0, 8};
    gptPathFinding->set_voxel(ptGrid, 3, 0, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){7, 0, 8};
    gptPathFinding->set_voxel(ptGrid, 7, 0, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){10, 0, 8};
    gptPathFinding->set_voxel(ptGrid, 10, 0, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){11, 0, 8};
    gptPathFinding->set_voxel(ptGrid, 11, 0, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){13, 0, 8};
    gptPathFinding->set_voxel(ptGrid, 13, 0, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){15, 0, 8};
    gptPathFinding->set_voxel(ptGrid, 15, 0, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){16, 0, 8};
    gptPathFinding->set_voxel(ptGrid, 16, 0, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){3, 0, 9};
    gptPathFinding->set_voxel(ptGrid, 3, 0, 9, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){7, 0, 9};
    gptPathFinding->set_voxel(ptGrid, 7, 0, 9, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){11, 0, 9};
    gptPathFinding->set_voxel(ptGrid, 11, 0, 9, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){15, 0, 9};
    gptPathFinding->set_voxel(ptGrid, 15, 0, 9, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){3, 0, 10};
    gptPathFinding->set_voxel(ptGrid, 3, 0, 10, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){7, 0, 10};
    gptPathFinding->set_voxel(ptGrid, 7, 0, 10, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){11, 0, 10};
    gptPathFinding->set_voxel(ptGrid, 11, 0, 10, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){15, 0, 10};
    gptPathFinding->set_voxel(ptGrid, 15, 0, 10, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){3, 0, 11};
    gptPathFinding->set_voxel(ptGrid, 3, 0, 11, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){7, 0, 11};
    gptPathFinding->set_voxel(ptGrid, 7, 0, 11, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){11, 0, 11};
    gptPathFinding->set_voxel(ptGrid, 11, 0, 11, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){15, 0, 11};
    gptPathFinding->set_voxel(ptGrid, 15, 0, 11, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){3, 0, 12};
    gptPathFinding->set_voxel(ptGrid, 3, 0, 12, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){7, 0, 12};
    gptPathFinding->set_voxel(ptGrid, 7, 0, 12, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){11, 0, 12};
    gptPathFinding->set_voxel(ptGrid, 11, 0, 12, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){15, 0, 12};
    gptPathFinding->set_voxel(ptGrid, 15, 0, 12, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){3, 0, 13};
    gptPathFinding->set_voxel(ptGrid, 3, 0, 13, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){7, 0, 13};
    gptPathFinding->set_voxel(ptGrid, 7, 0, 13, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){11, 0, 13};
    gptPathFinding->set_voxel(ptGrid, 11, 0, 13, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){15, 0, 13};
    gptPathFinding->set_voxel(ptGrid, 15, 0, 13, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){3, 0, 14};
    gptPathFinding->set_voxel(ptGrid, 3, 0, 14, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){7, 0, 14};
    gptPathFinding->set_voxel(ptGrid, 7, 0, 14, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){11, 0, 14};
    gptPathFinding->set_voxel(ptGrid, 11, 0, 14, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){15, 0, 14};
    gptPathFinding->set_voxel(ptGrid, 15, 0, 14, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){4, 0, 16};
    gptPathFinding->set_voxel(ptGrid, 4, 0, 16, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){7, 0, 16};
    gptPathFinding->set_voxel(ptGrid, 7, 0, 16, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){10, 0, 16};
    gptPathFinding->set_voxel(ptGrid, 10, 0, 16, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){13, 0, 16};
    gptPathFinding->set_voxel(ptGrid, 13, 0, 16, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){16, 0, 16};
    gptPathFinding->set_voxel(ptGrid, 16, 0, 16, true);

    // layer 1 (y = 1)
    tObstacles[(*uObstacleCount)++] = (plVec3){1, 1, 2};
    gptPathFinding->set_voxel(ptGrid, 1, 1, 2, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){4, 1, 2};
    gptPathFinding->set_voxel(ptGrid, 4, 1, 2, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){7, 1, 2};
    gptPathFinding->set_voxel(ptGrid, 7, 1, 2, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){10, 1, 2};
    gptPathFinding->set_voxel(ptGrid, 10, 1, 2, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){13, 1, 2};
    gptPathFinding->set_voxel(ptGrid, 13, 1, 2, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){16, 1, 2};
    gptPathFinding->set_voxel(ptGrid, 16, 1, 2, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){5, 1, 3};
    gptPathFinding->set_voxel(ptGrid, 5, 1, 3, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){9, 1, 3};
    gptPathFinding->set_voxel(ptGrid, 9, 1, 3, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){13, 1, 3};
    gptPathFinding->set_voxel(ptGrid, 13, 1, 3, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){17, 1, 3};
    gptPathFinding->set_voxel(ptGrid, 17, 1, 3, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){5, 1, 4};
    gptPathFinding->set_voxel(ptGrid, 5, 1, 4, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){9, 1, 4};
    gptPathFinding->set_voxel(ptGrid, 9, 1, 4, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){13, 1, 4};
    gptPathFinding->set_voxel(ptGrid, 13, 1, 4, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){17, 1, 4};
    gptPathFinding->set_voxel(ptGrid, 17, 1, 4, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){5, 1, 5};
    gptPathFinding->set_voxel(ptGrid, 5, 1, 5, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){9, 1, 5};
    gptPathFinding->set_voxel(ptGrid, 9, 1, 5, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){13, 1, 5};
    gptPathFinding->set_voxel(ptGrid, 13, 1, 5, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){17, 1, 5};
    gptPathFinding->set_voxel(ptGrid, 17, 1, 5, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){5, 1, 6};
    gptPathFinding->set_voxel(ptGrid, 5, 1, 6, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){9, 1, 6};
    gptPathFinding->set_voxel(ptGrid, 9, 1, 6, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){13, 1, 6};
    gptPathFinding->set_voxel(ptGrid, 13, 1, 6, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){17, 1, 6};
    gptPathFinding->set_voxel(ptGrid, 17, 1, 6, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){5, 1, 7};
    gptPathFinding->set_voxel(ptGrid, 5, 1, 7, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){9, 1, 7};
    gptPathFinding->set_voxel(ptGrid, 9, 1, 7, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){13, 1, 7};
    gptPathFinding->set_voxel(ptGrid, 13, 1, 7, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){17, 1, 7};
    gptPathFinding->set_voxel(ptGrid, 17, 1, 7, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){1, 1, 8};
    gptPathFinding->set_voxel(ptGrid, 1, 1, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){5, 1, 8};
    gptPathFinding->set_voxel(ptGrid, 5, 1, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){7, 1, 8};
    gptPathFinding->set_voxel(ptGrid, 7, 1, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){9, 1, 8};
    gptPathFinding->set_voxel(ptGrid, 9, 1, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){10, 1, 8};
    gptPathFinding->set_voxel(ptGrid, 10, 1, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){13, 1, 8};
    gptPathFinding->set_voxel(ptGrid, 13, 1, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){16, 1, 8};
    gptPathFinding->set_voxel(ptGrid, 16, 1, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){17, 1, 8};
    gptPathFinding->set_voxel(ptGrid, 17, 1, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){5, 1, 9};
    gptPathFinding->set_voxel(ptGrid, 5, 1, 9, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){9, 1, 9};
    gptPathFinding->set_voxel(ptGrid, 9, 1, 9, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){13, 1, 9};
    gptPathFinding->set_voxel(ptGrid, 13, 1, 9, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){17, 1, 9};
    gptPathFinding->set_voxel(ptGrid, 17, 1, 9, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){5, 1, 10};
    gptPathFinding->set_voxel(ptGrid, 5, 1, 10, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){9, 1, 10};
    gptPathFinding->set_voxel(ptGrid, 9, 1, 10, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){13, 1, 10};
    gptPathFinding->set_voxel(ptGrid, 13, 1, 10, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){17, 1, 10};
    gptPathFinding->set_voxel(ptGrid, 17, 1, 10, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){5, 1, 11};
    gptPathFinding->set_voxel(ptGrid, 5, 1, 11, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){9, 1, 11};
    gptPathFinding->set_voxel(ptGrid, 9, 1, 11, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){13, 1, 11};
    gptPathFinding->set_voxel(ptGrid, 13, 1, 11, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){17, 1, 11};
    gptPathFinding->set_voxel(ptGrid, 17, 1, 11, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){5, 1, 12};
    gptPathFinding->set_voxel(ptGrid, 5, 1, 12, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){9, 1, 12};
    gptPathFinding->set_voxel(ptGrid, 9, 1, 12, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){13, 1, 12};
    gptPathFinding->set_voxel(ptGrid, 13, 1, 12, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){17, 1, 12};
    gptPathFinding->set_voxel(ptGrid, 17, 1, 12, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){1, 1, 16};
    gptPathFinding->set_voxel(ptGrid, 1, 1, 16, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){4, 1, 16};
    gptPathFinding->set_voxel(ptGrid, 4, 1, 16, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){7, 1, 16};
    gptPathFinding->set_voxel(ptGrid, 7, 1, 16, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){10, 1, 16};
    gptPathFinding->set_voxel(ptGrid, 10, 1, 16, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){13, 1, 16};
    gptPathFinding->set_voxel(ptGrid, 13, 1, 16, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){16, 1, 16};
    gptPathFinding->set_voxel(ptGrid, 16, 1, 16, true);

    // layer 2 (y = 2)
    tObstacles[(*uObstacleCount)++] = (plVec3){1, 2, 2};
    gptPathFinding->set_voxel(ptGrid, 1, 2, 2, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){4, 2, 2};
    gptPathFinding->set_voxel(ptGrid, 4, 2, 2, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){7, 2, 2};
    gptPathFinding->set_voxel(ptGrid, 7, 2, 2, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){10, 2, 2};
    gptPathFinding->set_voxel(ptGrid, 10, 2, 2, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){13, 2, 2};
    gptPathFinding->set_voxel(ptGrid, 13, 2, 2, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){16, 2, 2};
    gptPathFinding->set_voxel(ptGrid, 16, 2, 2, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){2, 2, 4};
    gptPathFinding->set_voxel(ptGrid, 2, 2, 4, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){6, 2, 4};
    gptPathFinding->set_voxel(ptGrid, 6, 2, 4, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){10, 2, 4};
    gptPathFinding->set_voxel(ptGrid, 10, 2, 4, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){14, 2, 4};
    gptPathFinding->set_voxel(ptGrid, 14, 2, 4, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){18, 2, 4};
    gptPathFinding->set_voxel(ptGrid, 18, 2, 4, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){2, 2, 5};
    gptPathFinding->set_voxel(ptGrid, 2, 2, 5, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){6, 2, 5};
    gptPathFinding->set_voxel(ptGrid, 6, 2, 5, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){10, 2, 5};
    gptPathFinding->set_voxel(ptGrid, 10, 2, 5, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){14, 2, 5};
    gptPathFinding->set_voxel(ptGrid, 14, 2, 5, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){18, 2, 5};
    gptPathFinding->set_voxel(ptGrid, 18, 2, 5, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){2, 2, 6};
    gptPathFinding->set_voxel(ptGrid, 2, 2, 6, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){6, 2, 6};
    gptPathFinding->set_voxel(ptGrid, 6, 2, 6, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){10, 2, 6};
    gptPathFinding->set_voxel(ptGrid, 10, 2, 6, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){14, 2, 6};
    gptPathFinding->set_voxel(ptGrid, 14, 2, 6, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){18, 2, 6};
    gptPathFinding->set_voxel(ptGrid, 18, 2, 6, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){2, 2, 7};
    gptPathFinding->set_voxel(ptGrid, 2, 2, 7, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){6, 2, 7};
    gptPathFinding->set_voxel(ptGrid, 6, 2, 7, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){10, 2, 7};
    gptPathFinding->set_voxel(ptGrid, 10, 2, 7, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){14, 2, 7};
    gptPathFinding->set_voxel(ptGrid, 14, 2, 7, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){18, 2, 7};
    gptPathFinding->set_voxel(ptGrid, 18, 2, 7, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){1, 2, 8};
    gptPathFinding->set_voxel(ptGrid, 1, 2, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){2, 2, 8};
    gptPathFinding->set_voxel(ptGrid, 2, 2, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){4, 2, 8};
    gptPathFinding->set_voxel(ptGrid, 4, 2, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){6, 2, 8};
    gptPathFinding->set_voxel(ptGrid, 6, 2, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){7, 2, 8};
    gptPathFinding->set_voxel(ptGrid, 7, 2, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){10, 2, 8};
    gptPathFinding->set_voxel(ptGrid, 10, 2, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){13, 2, 8};
    gptPathFinding->set_voxel(ptGrid, 13, 2, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){14, 2, 8};
    gptPathFinding->set_voxel(ptGrid, 14, 2, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){16, 2, 8};
    gptPathFinding->set_voxel(ptGrid, 16, 2, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){18, 2, 8};
    gptPathFinding->set_voxel(ptGrid, 18, 2, 8, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){2, 2, 9};
    gptPathFinding->set_voxel(ptGrid, 2, 2, 9, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){6, 2, 9};
    gptPathFinding->set_voxel(ptGrid, 6, 2, 9, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){10, 2, 9};
    gptPathFinding->set_voxel(ptGrid, 10, 2, 9, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){14, 2, 9};
    gptPathFinding->set_voxel(ptGrid, 14, 2, 9, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){18, 2, 9};
    gptPathFinding->set_voxel(ptGrid, 18, 2, 9, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){2, 2, 10};
    gptPathFinding->set_voxel(ptGrid, 2, 2, 10, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){6, 2, 10};
    gptPathFinding->set_voxel(ptGrid, 6, 2, 10, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){10, 2, 10};
    gptPathFinding->set_voxel(ptGrid, 10, 2, 10, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){14, 2, 10};
    gptPathFinding->set_voxel(ptGrid, 14, 2, 10, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){18, 2, 10};
    gptPathFinding->set_voxel(ptGrid, 18, 2, 10, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){2, 2, 11};
    gptPathFinding->set_voxel(ptGrid, 2, 2, 11, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){6, 2, 11};
    gptPathFinding->set_voxel(ptGrid, 6, 2, 11, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){10, 2, 11};
    gptPathFinding->set_voxel(ptGrid, 10, 2, 11, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){14, 2, 11};
    gptPathFinding->set_voxel(ptGrid, 14, 2, 11, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){18, 2, 11};
    gptPathFinding->set_voxel(ptGrid, 18, 2, 11, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){2, 2, 12};
    gptPathFinding->set_voxel(ptGrid, 2, 2, 12, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){6, 2, 12};
    gptPathFinding->set_voxel(ptGrid, 6, 2, 12, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){10, 2, 12};
    gptPathFinding->set_voxel(ptGrid, 10, 2, 12, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){14, 2, 12};
    gptPathFinding->set_voxel(ptGrid, 14, 2, 12, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){18, 2, 12};
    gptPathFinding->set_voxel(ptGrid, 18, 2, 12, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){2, 2, 13};
    gptPathFinding->set_voxel(ptGrid, 2, 2, 13, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){6, 2, 13};
    gptPathFinding->set_voxel(ptGrid, 6, 2, 13, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){10, 2, 13};
    gptPathFinding->set_voxel(ptGrid, 10, 2, 13, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){14, 2, 13};
    gptPathFinding->set_voxel(ptGrid, 14, 2, 13, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){18, 2, 13};
    gptPathFinding->set_voxel(ptGrid, 18, 2, 13, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){1, 2, 16};
    gptPathFinding->set_voxel(ptGrid, 1, 2, 16, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){4, 2, 16};
    gptPathFinding->set_voxel(ptGrid, 4, 2, 16, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){7, 2, 16};
    gptPathFinding->set_voxel(ptGrid, 7, 2, 16, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){10, 2, 16};
    gptPathFinding->set_voxel(ptGrid, 10, 2, 16, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){13, 2, 16};
    gptPathFinding->set_voxel(ptGrid, 13, 2, 16, true);
    tObstacles[(*uObstacleCount)++] = (plVec3){16, 2, 16};
    gptPathFinding->set_voxel(ptGrid, 16, 2, 16, true);
}

static void
set_voxels_maze_three(plPathFindingVoxelGrid* ptGrid, plVec3* tObstacles, uint32_t* uObstacleCount)
{
    *uObstacleCount = 0;
}

plVoxel
ray_cast(plCamera* ptCamera, plPathFindingVoxelGrid* ptGrid, plVec3 tRayDirection, uint32_t uMaxDepth, int32_t iStepCounter)
{
    // set to false so we can search until it is true
    plVoxel tVoxel = {
        .bOccupied = false,
        .bValid    = false
    };

    float fVoxelX = (ptCamera->tPos.x - ptGrid->tOrigin.x) / ptGrid->fVoxelSize;
    float fVoxelY = (ptCamera->tPos.y - ptGrid->tOrigin.y) / ptGrid->fVoxelSize;
    float fVoxelZ = (ptCamera->tPos.z - ptGrid->tOrigin.z) / ptGrid->fVoxelSize;
    
    // starting voxel indices
    int32_t iCurrentX = (int32_t)floor(fVoxelX);
    int32_t iCurrentY = (int32_t)floor(fVoxelY);
    int32_t iCurrentZ = (int32_t)floor(fVoxelZ);

    // set algorithm specific variables
    plVec3 tNextBoundary = {
        .x = tRayDirection.x >= 0 ? (float)floor(fVoxelX) + 1.0f : (float)floor(fVoxelX),
        .y = tRayDirection.y >= 0 ? (float)floor(fVoxelY) + 1.0f : (float)floor(fVoxelY),
        .z = tRayDirection.z >= 0 ? (float)floor(fVoxelZ) + 1.0f : (float)floor(fVoxelZ) 
    };

    float tMaxX = (tNextBoundary.x - fVoxelX) / tRayDirection.x;
    float tMaxY = (tNextBoundary.y - fVoxelY) / tRayDirection.y;
    float tMaxZ = (tNextBoundary.z - fVoxelZ) / tRayDirection.z;

    int32_t iStepX = (tRayDirection.x >= 0) ? 1 : -1;
    int32_t iStepY = (tRayDirection.y >= 0) ? 1 : -1;
    int32_t iStepZ = (tRayDirection.z >= 0) ? 1 : -1;

    float tDeltaX = ptGrid->fVoxelSize / fabsf(tRayDirection.x);
    float tDeltaY = ptGrid->fVoxelSize / fabsf(tRayDirection.y);
    float tDeltaZ = ptGrid->fVoxelSize / fabsf(tRayDirection.z);

    int32_t uOutOfGridX = (iStepX > 0) ? ptGrid->uDimX : -1;
    int32_t uOutOfGridY = (iStepY > 0) ? ptGrid->uDimY : -1;
    int32_t uOutOfGridZ = (iStepZ > 0) ? ptGrid->uDimZ : -1;

    int32_t iStepsTaken = 0;
    
    // traverse through grid
    int iteration = 0;
    while(true)
    {           
        if(tMaxX < tMaxY) 
        {
            if(tMaxX < tMaxZ) 
            {
                iCurrentX += iStepX;
                if(iCurrentX == uOutOfGridX)
                    break;
                tMaxX = tMaxX + tDeltaX;
            } 
            else 
            {
                iCurrentZ += iStepZ;
                if(iCurrentZ == uOutOfGridZ)
                    break;
                tMaxZ = tMaxZ + tDeltaZ;
            }
        } 
        else 
        {
            if(tMaxY < tMaxZ) 
            {
                iCurrentY += iStepY;
                if(iCurrentY == uOutOfGridY)
                    break;
                tMaxY = tMaxY + tDeltaY;
            } 
            else    
            {
                iCurrentZ += iStepZ;
                if(iCurrentZ == uOutOfGridZ)
                    break;
                tMaxZ = tMaxZ + tDeltaZ;
            }
        }

        // check if inside grid bounds
        bool bInsideGrid = (iCurrentX >= 0 && iCurrentX < (int32_t)ptGrid->uDimX &&
                            iCurrentY >= 0 && iCurrentY < (int32_t)ptGrid->uDimY &&
                            iCurrentZ >= 0 && iCurrentZ < (int32_t)ptGrid->uDimZ);

        if(bInsideGrid)
        {
            iStepsTaken++;

            // check if we've reached the desired step count
            if(iStepsTaken >= iStepCounter)
            {
                tVoxel.tXPos  = iCurrentX;
                tVoxel.tYPos  = iCurrentY;
                tVoxel.tZPos  = iCurrentZ;
                tVoxel.bValid = true;
                if(is_voxel_occupied(ptGrid, iCurrentX, iCurrentY, iCurrentZ))
                {
                    tVoxel.bOccupied = true;
                }
                break;
            }
        }
    }
    return tVoxel;
}

plVec3 
get_ray_direction(plCamera* ptCamera)
{
    plVec3 tDir = {
        .x = (float)sin(ptCamera->fYaw) * (float)cos(ptCamera->fPitch),
        .y = (float)sin(ptCamera->fPitch),
        .z = (float)cos(ptCamera->fYaw) * (float)cos(ptCamera->fPitch)
    };
    return tDir;
}

bool 
is_voxel_occupied(plPathFindingVoxelGrid* ptGrid, uint32_t uVoxelX, uint32_t uVoxelY, uint32_t uVoxelZ)
{
    // convert 3d grid coordinates to 1d index
    uint32_t uIndex = uVoxelZ * (ptGrid->uDimX * ptGrid->uDimY) + uVoxelY * ptGrid->uDimX + uVoxelX;
    
    uint32_t uArrayIndex = uIndex / 32;
    uint32_t uBitIndex = uIndex % 32;
    
    return (ptGrid->apOccupancyBits[uArrayIndex] & (1u << uBitIndex)) != 0;
}

void
draw_voxel(plDrawList3D* ptDrawlist, plVec3 tPos, uint32_t uFillColor, uint32_t uWireColor)
{
    float fCenterX = tPos.x + 0.5f;
    float fCenterY = tPos.y + 0.5f;
    float fCenterZ = tPos.z + 0.5f;
    float fHalf = 0.5f;
    float fRadius = 0.02f;
    uint32_t uSegments = 6;

    // draw main box
    gptDraw->add_3d_centered_box_filled(ptDrawlist, 
        (plVec3){fCenterX, fCenterY, fCenterZ}, 
        1.0f, 1.0f, 1.0f,
        (plDrawSolidOptions){.uColor = uFillColor});
    
    plDrawSolidOptions tWireOptions = {.uColor = uWireColor};
    
    // bottom edges
    gptDraw->add_3d_cylinder_filled(ptDrawlist, (plCylinder){{fCenterX - fHalf, fCenterY - fHalf, fCenterZ - fHalf}, {fCenterX + fHalf, fCenterY - fHalf, fCenterZ - fHalf}, fRadius}, uSegments, tWireOptions);
    gptDraw->add_3d_cylinder_filled(ptDrawlist, (plCylinder){{fCenterX + fHalf, fCenterY - fHalf, fCenterZ - fHalf}, {fCenterX + fHalf, fCenterY - fHalf, fCenterZ + fHalf}, fRadius}, uSegments, tWireOptions);
    gptDraw->add_3d_cylinder_filled(ptDrawlist, (plCylinder){{fCenterX + fHalf, fCenterY - fHalf, fCenterZ + fHalf}, {fCenterX - fHalf, fCenterY - fHalf, fCenterZ + fHalf}, fRadius}, uSegments, tWireOptions);
    gptDraw->add_3d_cylinder_filled(ptDrawlist, (plCylinder){{fCenterX - fHalf, fCenterY - fHalf, fCenterZ + fHalf}, {fCenterX - fHalf, fCenterY - fHalf, fCenterZ - fHalf}, fRadius}, uSegments, tWireOptions);
    
    // top edges
    gptDraw->add_3d_cylinder_filled(ptDrawlist, (plCylinder){{fCenterX - fHalf, fCenterY + fHalf, fCenterZ - fHalf}, {fCenterX + fHalf, fCenterY + fHalf, fCenterZ - fHalf}, fRadius}, uSegments, tWireOptions);
    gptDraw->add_3d_cylinder_filled(ptDrawlist, (plCylinder){{fCenterX + fHalf, fCenterY + fHalf, fCenterZ - fHalf}, {fCenterX + fHalf, fCenterY + fHalf, fCenterZ + fHalf}, fRadius}, uSegments, tWireOptions);
    gptDraw->add_3d_cylinder_filled(ptDrawlist, (plCylinder){{fCenterX + fHalf, fCenterY + fHalf, fCenterZ + fHalf}, {fCenterX - fHalf, fCenterY + fHalf, fCenterZ + fHalf}, fRadius}, uSegments, tWireOptions);
    gptDraw->add_3d_cylinder_filled(ptDrawlist, (plCylinder){{fCenterX - fHalf, fCenterY + fHalf, fCenterZ + fHalf}, {fCenterX - fHalf, fCenterY + fHalf, fCenterZ - fHalf}, fRadius}, uSegments, tWireOptions);
    
    // vertical edges
    gptDraw->add_3d_cylinder_filled(ptDrawlist, (plCylinder){{fCenterX - fHalf, fCenterY - fHalf, fCenterZ - fHalf}, {fCenterX - fHalf, fCenterY + fHalf, fCenterZ - fHalf}, fRadius}, uSegments, tWireOptions);
    gptDraw->add_3d_cylinder_filled(ptDrawlist, (plCylinder){{fCenterX + fHalf, fCenterY - fHalf, fCenterZ - fHalf}, {fCenterX + fHalf, fCenterY + fHalf, fCenterZ - fHalf}, fRadius}, uSegments, tWireOptions);
    gptDraw->add_3d_cylinder_filled(ptDrawlist, (plCylinder){{fCenterX + fHalf, fCenterY - fHalf, fCenterZ + fHalf}, {fCenterX + fHalf, fCenterY + fHalf, fCenterZ + fHalf}, fRadius}, uSegments, tWireOptions);
    gptDraw->add_3d_cylinder_filled(ptDrawlist, (plCylinder){{fCenterX - fHalf, fCenterY - fHalf, fCenterZ + fHalf}, {fCenterX - fHalf, fCenterY + fHalf, fCenterZ + fHalf}, fRadius}, uSegments, tWireOptions);
}