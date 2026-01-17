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

// extensions
#include "pl_graphics_ext.h"
#include "pl_draw_ext.h"
#include "pl_starter_ext.h"
#include "pl_path_finding_ext.h"

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

typedef struct _plAppData
{
    plWindow*      ptWindow;
    plDrawList3D*  pt3dDrawlist;
    plCamera       tCamera;

    // pathfinding
    plPathFindingVoxelGrid* ptVoxelGrid;
    plPathFindingResult     tPathResult;

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

//-----------------------------------------------------------------------------
// [SECTION] helper function declarations
//-----------------------------------------------------------------------------

void camera_translate(plCamera*, float fDx, float fDy, float fDz);
void camera_rotate   (plCamera*, float fDPitch, float fDYaw);
void camera_update   (plCamera*);
void draw_wireframe_cube(plDrawList3D* ptDrawlist, plVec3 tCenter, float fSize, plDrawLineOptions tOptions);
void draw_cube(plDrawList3D* ptDrawlist, plVec3 tCenter, float fSize, uint32_t uColor);

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
        .tPos         = {10.0f, 15.0f, 10.0f},
        .fNearZ       = 0.01f,
        .fFarZ        = 100.0f,
        .fFieldOfView = PL_PI_3,
        .fAspectRatio = ptIO->tMainViewportSize.x / ptIO->tMainViewportSize.y,
        .fYaw         = PL_PI + PL_PI_4,
        .fPitch       = -PL_PI_4,
    };
    camera_update(&ptAppData->tCamera);

    // create voxel grid (20x1x20 for now)
    plVec3 tOrigin = {0.0f, 0.0f, 0.0f};
    ptAppData->ptVoxelGrid = gptPathFinding->create_voxel_grid(20, 1, 20, 1.0f, tOrigin);

    // TODO: add obstacles here
    gptPathFinding->set_voxel(ptAppData->ptVoxelGrid, 3, 0, 1, true);
    gptPathFinding->set_voxel(ptAppData->ptVoxelGrid, 3, 0, 2, true);
    gptPathFinding->set_voxel(ptAppData->ptVoxelGrid, 3, 0, 3, true);
    gptPathFinding->set_voxel(ptAppData->ptVoxelGrid, 3, 0, 4, true);
    gptPathFinding->set_voxel(ptAppData->ptVoxelGrid, 3, 0, 5, true);
    gptPathFinding->set_voxel(ptAppData->ptVoxelGrid, 3, 0, 6, true);

    // TODO: create pathfinding query and find path
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

    if(gptIO->is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 1.0f))
    {
        const plVec2 tMouseDelta = gptIO->get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f);
        camera_rotate(ptCamera, -tMouseDelta.y * fCameraRotationSpeed, -tMouseDelta.x * fCameraRotationSpeed);
        gptIO->reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
    }
    camera_update(ptCamera);

    // draw origin axes
    const plMat4 tOrigin = pl_identity_mat4();
    gptDraw->add_3d_transform(ptAppData->pt3dDrawlist, &tOrigin, 5.0f, (plDrawLineOptions){.fThickness = 0.1f});

    // draw wire frame grid
    for(uint32_t i = 0; i < 21; i++)
    {
        for(uint32_t j = 0; j < 21; j++)
        {
            float fx = (float)i + 0.5f;
            float fz = (float)j + 0.5f;

            if((i + j) % 2)
            {
                draw_wireframe_cube(ptAppData->pt3dDrawlist, (plVec3){fx, 0.5f, fz}, 1, (plDrawLineOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 0.0f, 0.0f, 0.5f), .fThickness = 0.05f});
            }
            else 
                draw_wireframe_cube(ptAppData->pt3dDrawlist, (plVec3){fx, 0.5f, fz}, 1, (plDrawLineOptions){.uColor = PL_COLOR_32_RGBA(0.0f, 1.0f, 0.0f, 0.5f), .fThickness = 0.05f});
        }
    }


    // TODO: draw path as 3D lines
    // Loop through waypoints
    // Connect consecutive waypoints with lines

    // start sphere 
    gptDraw->add_3d_sphere_filled(ptAppData->pt3dDrawlist, (plSphere){.fRadius = 0.35f, .tCenter = {0.5f, 0.5f, 0.5f}}, 0, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_MAGENTA});
    // goal sphere
    gptDraw->add_3d_sphere_filled(ptAppData->pt3dDrawlist, (plSphere){.fRadius = 0.35f, .tCenter = {18.5f, 0.5f, 18.5f}}, 0, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_CYAN});    

    // draw obstacles
    for(uint32_t k = 0; k < 7; k++)
    {
        gptDraw->add_3d_sphere_filled(ptAppData->pt3dDrawlist, (plSphere){.fRadius = 0.35f, .tCenter = {3.5f, 0.5f, (k + 0.5f)}}, 0, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_ORANGE});   
    }

    // draw path
    if(ptAppData->tPathResult.bSuccess)
    {
        for(uint32_t i = 0; i < ptAppData->tPathResult.uWaypointCount - 1; i++)
        {
            float fX0 = ptAppData->tPathResult.atWaypoints[i].x;
            float fZ0 = ptAppData->tPathResult.atWaypoints[i].z;

            float fX1 = ptAppData->tPathResult.atWaypoints[i+1].x;
            float fZ1 = ptAppData->tPathResult.atWaypoints[i+1].z;
            // draw line between waypoints
            gptDraw->add_3d_line(ptAppData->pt3dDrawlist, (plVec3){fX0 + 0.5f, 0.5f, fZ0 + 0.5f}, (plVec3){fX1 + 0.5f, 0.5f, fZ1 + 0.5f}, (plDrawLineOptions){.uColor = PL_COLOR_32_BLUE, .fThickness = 0.05f});
        }
    }

    // draw "ground layer"
    // quad vertex data 
    plVec3 tCorner1 = {0,  0, 0};  // bottom-left
    plVec3 tCorner2 = {20, 0, 0};  // bottom-right
    plVec3 tCorner3 = {0,  0, 20};  // top-left
    plVec3 tCorner4 = {20, 0, 20};  // top-right

    // draw triangle 1
    gptDraw->add_3d_triangle_filled(ptAppData->pt3dDrawlist,
        tCorner1, tCorner2, tCorner3,
        (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(.3, .3, .3, .5)});

    // draw triangle 2
    gptDraw->add_3d_triangle_filled(ptAppData->pt3dDrawlist,
        tCorner2, tCorner4, tCorner3, 
        (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(.3, .3, .3, .5)});

    
    // submit 3D drawlist
    plRenderEncoder* ptEncoder = gptStarter->begin_main_pass();

    const plMat4 tMVP = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);

    gptDraw->submit_3d_drawlist(ptAppData->pt3dDrawlist,
        ptEncoder,
        ptIO->tMainViewportSize.x,
        ptIO->tMainViewportSize.y,
        &tMVP,
        PL_DRAW_FLAG_DEPTH_TEST | PL_DRAW_FLAG_DEPTH_WRITE,
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
draw_wireframe_cube(plDrawList3D* ptDrawlist, plVec3 tCenter, float fSize, plDrawLineOptions tOptions)
{
    // calculate half size for offset from center
    float fHalf = fSize * 0.5f;// * 0.965f; // leaving "air gap" so each cube can be differentiated 
    
    // 8 vertices of cube
    plVec3 v0 = {tCenter.x - fHalf, tCenter.y - fHalf, tCenter.z - fHalf}; // back bottom left
    plVec3 v1 = {tCenter.x + fHalf, tCenter.y - fHalf, tCenter.z - fHalf}; // back bottom right
    plVec3 v2 = {tCenter.x + fHalf, tCenter.y + fHalf, tCenter.z - fHalf}; // back top right
    plVec3 v3 = {tCenter.x - fHalf, tCenter.y + fHalf, tCenter.z - fHalf}; // back top left
    plVec3 v4 = {tCenter.x - fHalf, tCenter.y - fHalf, tCenter.z + fHalf}; // front bottom left
    plVec3 v5 = {tCenter.x + fHalf, tCenter.y - fHalf, tCenter.z + fHalf}; // front bottom right
    plVec3 v6 = {tCenter.x + fHalf, tCenter.y + fHalf, tCenter.z + fHalf}; // front top right
    plVec3 v7 = {tCenter.x - fHalf, tCenter.y + fHalf, tCenter.z + fHalf}; // front top left
    
    // bottom face
    gptDraw->add_3d_line(ptDrawlist, v0, v1, tOptions);
    gptDraw->add_3d_line(ptDrawlist, v1, v5, tOptions);
    gptDraw->add_3d_line(ptDrawlist, v5, v4, tOptions);
    gptDraw->add_3d_line(ptDrawlist, v4, v0, tOptions);
    
    // top face
    gptDraw->add_3d_line(ptDrawlist, v3, v2, tOptions);
    gptDraw->add_3d_line(ptDrawlist, v2, v6, tOptions);
    gptDraw->add_3d_line(ptDrawlist, v6, v7, tOptions);
    gptDraw->add_3d_line(ptDrawlist, v7, v3, tOptions);
    
    // vertical edges
    gptDraw->add_3d_line(ptDrawlist, v0, v3, tOptions);
    gptDraw->add_3d_line(ptDrawlist, v1, v2, tOptions);
    gptDraw->add_3d_line(ptDrawlist, v5, v6, tOptions);
    gptDraw->add_3d_line(ptDrawlist, v4, v7, tOptions);
}

void 
draw_cube(plDrawList3D* ptDrawlist, plVec3 tCenter, float fSize, uint32_t uColor)
{
    // calculate 8 corner vertices
    // draw 12 triangles (2 per face)
}