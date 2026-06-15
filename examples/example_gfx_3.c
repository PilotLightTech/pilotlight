/*
   example_gfx_3.c
     - demonstrates loading APIs
     - demonstrates loading extensions
     - demonstrates hot reloading
     - demonstrates starter extension
     - demonstrates camera extension
     - demonstrates drawing extension (2D & 3D)
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

#include <string.h> // memset
#include "pl.h"

#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extensions
#include "pl_graphics_ext.h"
#include "pl_draw_ext.h"
#include "pl_starter_ext.h"
#include "pl_camera_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

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
const plStarterI*     gptStarter     = NULL;
const plCameraI*      gptCamera      = NULL;

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
        gptStarter     = pl_get_api_latest(ptApiRegistry, plStarterI);
        gptCamera      = pl_get_api_latest(ptApiRegistry, plCameraI);

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
    ptExtensionRegistry->load("pl_platform_ext", "pl_load_platform_ext", "pl_unload_platform_ext", false);
    
    // load required apis
    gptIO      = pl_get_api_latest(ptApiRegistry, plIOI);
    gptWindows = pl_get_api_latest(ptApiRegistry, plWindowI);

    // load required apis (these are provided though extensions)
    gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptDraw        = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptStarter     = pl_get_api_latest(ptApiRegistry, plStarterI);
    gptCamera      = pl_get_api_latest(ptApiRegistry, plCameraI);

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
        .eFlags   = PL_STARTER_FLAGS_ALL_EXTENSIONS,
        .ptWindow = ptAppData->ptWindow
    };

    // we want the starter extension to include a depth buffer
    // when setting up the render pass
    tStarterInit.eFlags |= PL_STARTER_FLAGS_DEPTH_BUFFER;
    tStarterInit.eFlags |= PL_STARTER_FLAGS_MSAA;

    // from a graphics standpoint, the starter extension is handling device, swapchain, renderpass
    // etc. which we will get to in later examples
    gptStarter->initialize(tStarterInit);

    // give starter extension chance to do its work now that we
    // setup the shader extension
    gptStarter->finalize();

    // request 3d drawlists
    ptAppData->pt3dDrawlist = gptDraw->request_3d_drawlist();

    // create camera
    gptCamera->init(&ptAppData->tCamera);
    plCameraPerspectiveDesc tCameraDesc = {
        .fNearZ       = 0.01f,
        .fFarZ        = 50.0f,
        .fYFov        = PL_PI_3,
        .fAspectRatio = 1.0f,
        .eDepthMode   = PL_CAMERA_DEPTH_MODE_STANDARD
    };
    gptCamera->set_perspective(&ptAppData->tCamera, &tCameraDesc);
    gptCamera->set_position(&ptAppData->tCamera, (plVec3d){5.0, 10.0, 10.0});
    gptCamera->set_euler(&ptAppData->tCamera, -PL_PI_4, PL_PI + PL_PI_4, 0.0f);
    gptCamera->update(&ptAppData->tCamera);

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
    gptCamera->set_viewport(&ptAppData->tCamera, ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y);
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
    if(gptIO->is_key_down(PL_KEY_W)) gptCamera->translate_local(ptCamera, (plVec3d){ 0.0f,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime});
    if(gptIO->is_key_down(PL_KEY_S)) gptCamera->translate_local(ptCamera, (plVec3d){ 0.0f,  0.0f, -fCameraTravelSpeed* ptIO->fDeltaTime});
    if(gptIO->is_key_down(PL_KEY_A)) gptCamera->translate_local(ptCamera, (plVec3d){ fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f});
    if(gptIO->is_key_down(PL_KEY_D)) gptCamera->translate_local(ptCamera, (plVec3d){-fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f});

    // world space
    if(gptIO->is_key_down(PL_KEY_F)) { gptCamera->translate(ptCamera, (plVec3d){ 0.0f, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f }); }
    if(gptIO->is_key_down(PL_KEY_R)) { gptCamera->translate(ptCamera, (plVec3d){ 0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f }); }

    if(gptIO->is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 1.0f))
    {
        const plVec2 tMouseDelta = gptIO->get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f);
        gptCamera->rotate_euler(ptCamera,  -tMouseDelta.y * fCameraRotationSpeed,  -tMouseDelta.x * fCameraRotationSpeed, 0.0f);
        gptIO->reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
    }
    gptCamera->update(ptCamera);

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
    plCommandBuffer* ptCommandBuffer = gptStarter->begin_main_pass();

    // submit 3d drawlist
    plRenderAttachmentInfo tRenderAttachmentInfo = {0};
    gptStarter->get_render_attachment_info(&tRenderAttachmentInfo);
    gptDraw->submit_3d_drawlist(ptAppData->pt3dDrawlist,
        ptCommandBuffer,
        ptIO->tMainViewportSize.x,
        ptIO->tMainViewportSize.y,
        &ptAppData->tCamera.tViewProjMat,
        PL_DRAW_FLAG_DEPTH_TEST | PL_DRAW_FLAG_DEPTH_WRITE,
        gptGfx->get_swapchain_info(gptStarter->get_swapchain()).eSampleCount, &tRenderAttachmentInfo);

    // allows the starter extension to handle some things then ends the main pass
    gptStarter->end_main_pass();

    // must be the last function called when using the starter extension
    gptStarter->end_frame(); 
}