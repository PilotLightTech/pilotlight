/*
   example_gfx_3.c
     - demonstrates loading APIs
     - demonstrates loading extensions
     - demonstrates hot reloading
     - demonstrates starter extension
     - demonstrates drawing extension (2D & 3D)
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

#include <string.h> // memset
#include "pl.h"

#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extensions
#include "pl_graphics_ext.h"
#include "pl_draw_ext.h"
#include "pl_draw_backend_ext.h"
#include "pl_starter_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plCamera
{
    plVec3 tPos;
    float  fNearZ;
    float  fFarZ;
    float  fFieldOfView;
    float  fAspectRatio;  // width/height
    plMat4 tViewMat;      // cached
    plMat4 tProjMat;      // cached
    plMat4 tTransformMat; // cached

    // rotations
    float fPitch; // rotation about right vector
    float fYaw;   // rotation about up vector
    float fRoll;  // rotation about forward vector

    // direction vectors
    plVec3 _tUpVec;
    plVec3 _tForwardVec;
    plVec3 _tRightVec;
} plCamera;

typedef struct _plAppData
{
    // window
    plWindow* ptWindow;

    // 3d drawing
    plCamera      tCamera;
    plDrawList3D* pt3dDrawlist;
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

const plIOI*          gptIO          = NULL;
const plWindowI*      gptWindows     = NULL;
const plGraphicsI*    gptGfx         = NULL;
const plDrawI*        gptDraw        = NULL;
const plDrawBackendI* gptDrawBackend = NULL;
const plStarterI*     gptStarter     = NULL;

//-----------------------------------------------------------------------------
// [SECTION] helper function declarations
//-----------------------------------------------------------------------------

// camera helpers
void camera_translate(plCamera*, float fDx, float fDy, float fDz);
void camera_rotate   (plCamera*, float fDPitch, float fDYaw);
void camera_rotate   (plCamera*, float fDPitch, float fDYaw);
void camera_update   (plCamera*);

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
    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    // if "ptAppData" is a valid pointer, then this function is being called
    // during a hot reload.
    if(ptAppData)
    {
        // re-retrieve the apis since we are now in
        // a different dll/so
        gptIO          = pl_get_api_latest(ptApiRegistry, plIOI);
        gptWindows     = pl_get_api_latest(ptApiRegistry, plWindowI);
        gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);
        gptDraw        = pl_get_api_latest(ptApiRegistry, plDrawI);
        gptDrawBackend = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
        gptStarter     = pl_get_api_latest(ptApiRegistry, plStarterI);

        return ptAppData;
    }

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = malloc(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    // retrieve extension registry
    const plExtensionRegistryI* ptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);

    // load extensions (makes their APIs available)
    ptExtensionRegistry->load("pl_unity_ext", NULL, NULL, true);
    ptExtensionRegistry->load("pl_platform_ext", NULL, NULL, false);
    
    // load required apis
    gptIO      = pl_get_api_latest(ptApiRegistry, plIOI);
    gptWindows = pl_get_api_latest(ptApiRegistry, plWindowI);

    // load required apis (these are provided though extensions)
    gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptDraw        = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptDrawBackend = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
    gptStarter     = pl_get_api_latest(ptApiRegistry, plStarterI);

    // use window API to create a window
    plWindowDesc tWindowDesc = {
        .pcTitle = "Example GFX 3",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 600,
        .uHeight = 600,
    };
    gptWindows->create(tWindowDesc, &ptAppData->ptWindow);
    gptWindows->show(ptAppData->ptWindow);

    plStarterInit tStarterInit = {
        .tFlags   = PL_STARTER_FLAGS_ALL_EXTENSIONS,
        .ptWindow = ptAppData->ptWindow
    };

    // we want the starter extension to include a depth buffer
    // when setting up the render pass
    tStarterInit.tFlags |= PL_STARTER_FLAGS_DEPTH_BUFFER;

    // from a graphics standpoint, the starter extension is handling device, swapchain, renderpass
    // etc. which we will get to in later examples
    gptStarter->initialize(tStarterInit);

    // give starter extension chance to do its work now that we
    // setup the shader extension
    gptStarter->finalize();

    // request 3d drawlists
    ptAppData->pt3dDrawlist = gptDraw->request_3d_drawlist();

    // create camera
    ptAppData->tCamera = (plCamera){
        .tPos         = {5.0f, 10.0f, 10.0f},
        .fNearZ       = 0.01f,
        .fFarZ        = 50.0f,
        .fFieldOfView = PL_PI_3,
        .fAspectRatio = 1.0f,
        .fYaw         = PL_PI + PL_PI_4,
        .fPitch       = -PL_PI_4,
    };
    camera_update(&ptAppData->tCamera);

    // return app memory
    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
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

    // for convience
    plIO* ptIO = gptIO->get_io();

    static const float fCameraTravelSpeed = 4.0f;
    static const float fCameraRotationSpeed = 0.005f;

    plCamera* ptCamera = &ptAppData->tCamera;

    // camera space
    if(gptIO->is_key_down(PL_KEY_W)) camera_translate(ptCamera,  0.0f,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime);
    if(gptIO->is_key_down(PL_KEY_S)) camera_translate(ptCamera,  0.0f,  0.0f, -fCameraTravelSpeed* ptIO->fDeltaTime);
    if(gptIO->is_key_down(PL_KEY_A)) camera_translate(ptCamera, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);
    if(gptIO->is_key_down(PL_KEY_D)) camera_translate(ptCamera,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);

    // world space
    if(gptIO->is_key_down(PL_KEY_F)) { camera_translate(ptCamera,  0.0f, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f); }
    if(gptIO->is_key_down(PL_KEY_R)) { camera_translate(ptCamera,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f); }

    if(gptIO->is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 1.0f))
    {
        const plVec2 tMouseDelta = gptIO->get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f);
        camera_rotate(ptCamera,  -tMouseDelta.y * fCameraRotationSpeed,  -tMouseDelta.x * fCameraRotationSpeed);
        gptIO->reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
    }
    camera_update(ptCamera);

    // 3d drawing API usage
    const plMat4 tOrigin = pl_identity_mat4();
    gptDraw->add_3d_transform(ptAppData->pt3dDrawlist, &tOrigin, 10.0f, (plDrawLineOptions){.fThickness = 0.2f});

    plCylinder tCylinderDesc = {
        .fRadius = 1.5f,
        .tBasePos = {-2.5f, 1.0f, 0.0f},
        .tTipPos  = {-2.5f, 4.0f, 0.0f}
    };
    gptDraw->add_3d_cylinder_filled(ptAppData->pt3dDrawlist, tCylinderDesc, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 1.0f, 0.0f, 0.75f)});

    gptDraw->add_3d_triangle_filled(ptAppData->pt3dDrawlist,
        (plVec3){1.0f, 1.0f, 0.0f},
        (plVec3){4.0f, 1.0f, 0.0f},
        (plVec3){1.0f, 4.0f, 0.0f},
        (plDrawSolidOptions){.uColor = PL_COLOR_32_YELLOW});

    gptDraw->add_3d_sphere_filled(ptAppData->pt3dDrawlist,
        (plSphere){.fRadius = 1.0F, .tCenter = {5.5f, 2.5f, 0.0f}}, 0, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 1.0f, 0.0f, 0.75f)});

    gptDraw->add_3d_circle_xz_filled(ptAppData->pt3dDrawlist,
        (plVec3){8.5f, 2.5f, 0.0f}, 1.5f, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 1.0f, 0.0f, 0.75f)});

    gptDraw->add_3d_band_xz_filled(ptAppData->pt3dDrawlist, (plVec3){11.5f, 2.5f, 0.0f}, 0.75f, 1.5f, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 1.0f, 0.0f, 0.75f)});
    gptDraw->add_3d_band_xy_filled(ptAppData->pt3dDrawlist, (plVec3){11.5f, 2.5f, 0.0f}, 0.75f, 1.5f, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 0.0f, 0.0f, 0.75f)});
    gptDraw->add_3d_band_yz_filled(ptAppData->pt3dDrawlist, (plVec3){11.5f, 2.5f, 0.0f}, 0.75f, 1.5f, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 0.0f, 1.0f, 0.75f)});

    // start main pass & return the encoder being used
    plRenderEncoder* ptEncoder = gptStarter->begin_main_pass();

    const plMat4 tMVP = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);

    // submit 3d drawlist
    gptDrawBackend->submit_3d_drawlist(ptAppData->pt3dDrawlist,
        ptEncoder,
        ptIO->tMainViewportSize.x,
        ptIO->tMainViewportSize.y,
        &tMVP,
        PL_DRAW_FLAG_DEPTH_TEST | PL_DRAW_FLAG_DEPTH_WRITE,
        gptGfx->get_swapchain_info(gptStarter->get_swapchain()).tSampleCount);

    // allows the starter extension to handle some things then ends the main pass
    gptStarter->end_main_pass();

    // must be the last function called when using the starter extension
    gptStarter->end_frame(); 
}

//-----------------------------------------------------------------------------
// [SECTION] helper function declarations
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
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~update view~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // world space
    static const plVec4 tOriginalUpVec      = {0.0f, 1.0f, 0.0f, 0.0f};
    static const plVec4 tOriginalForwardVec = {0.0f, 0.0f, 1.0f, 0.0f};
    static const plVec4 tOriginalRightVec   = {-1.0f, 0.0f, 0.0f, 0.0f};

    const plMat4 tXRotMat   = pl_mat4_rotate_vec3(ptCamera->fPitch, tOriginalRightVec.xyz);
    const plMat4 tYRotMat   = pl_mat4_rotate_vec3(ptCamera->fYaw, tOriginalUpVec.xyz);
    const plMat4 tZRotMat   = pl_mat4_rotate_vec3(ptCamera->fRoll, tOriginalForwardVec.xyz);
    const plMat4 tTranslate = pl_mat4_translate_vec3((plVec3){ptCamera->tPos.x, ptCamera->tPos.y, ptCamera->tPos.z});

    // rotations: rotY * rotX * rotZ
    plMat4 tRotations = pl_mul_mat4t(&tXRotMat, &tZRotMat);
    tRotations        = pl_mul_mat4t(&tYRotMat, &tRotations);

    // update camera vectors
    ptCamera->_tRightVec   = pl_norm_vec4(pl_mul_mat4_vec4(&tRotations, tOriginalRightVec)).xyz;
    ptCamera->_tUpVec      = pl_norm_vec4(pl_mul_mat4_vec4(&tRotations, tOriginalUpVec)).xyz;
    ptCamera->_tForwardVec = pl_norm_vec4(pl_mul_mat4_vec4(&tRotations, tOriginalForwardVec)).xyz;

    // update camera transform: translate * rotate
    ptCamera->tTransformMat = pl_mul_mat4t(&tTranslate, &tRotations);

    // update camera view matrix
    ptCamera->tViewMat   = pl_mat4t_invert(&ptCamera->tTransformMat);

    // flip x & y so camera looks down +z and remains right handed (+x to the right)
    const plMat4 tFlipXY = pl_mat4_scale_xyz(-1.0f, -1.0f, 1.0f);
    ptCamera->tViewMat   = pl_mul_mat4t(&tFlipXY, &ptCamera->tViewMat);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~update projection~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    const float fInvtanHalfFovy = 1.0f / tanf(ptCamera->fFieldOfView / 2.0f);
    ptCamera->tProjMat.col[0].x = fInvtanHalfFovy / ptCamera->fAspectRatio;
    ptCamera->tProjMat.col[1].y = fInvtanHalfFovy;
    ptCamera->tProjMat.col[2].z = ptCamera->fFarZ / (ptCamera->fFarZ - ptCamera->fNearZ);
    ptCamera->tProjMat.col[2].w = 1.0f;
    ptCamera->tProjMat.col[3].z = -ptCamera->fNearZ * ptCamera->fFarZ / (ptCamera->fFarZ - ptCamera->fNearZ);
    ptCamera->tProjMat.col[3].w = 0.0f;  
}
