/*
   app.c

   Notes:
     * absolute mess
     * mostly a sandbox for now & testing experimental stuff
     * look at examples for more stable APIs
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

// extensions
#include "pl_stats_ext.h"
#include "pl_graphics_ext.h"
#include "pl_ecs_ext.h"
#include "pl_model_loader_ext.h"
#include "pl_ref_renderer_ext.h"
#include "pl_job_ext.h"
#include "pl_draw_ext.h"
#include "pl_ui_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct plAppData_t
{

    // windows
    plWindow* ptWindow;
    bool      bResize;

    // scene
    plEntity     tMainCamera;
    plEntity     tSunlight;

    // views
    uint32_t uSceneHandle0;
    uint32_t uViewHandle0;

    // drawing
    plDrawLayer2D* ptDrawLayer;
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] global apis
//-----------------------------------------------------------------------------

const plWindowI*      gptWindows           = NULL;
const plStatsI*       gptStats             = NULL;
const plGraphicsI*    gptGfx               = NULL;
const plDeviceI*      gptDevice            = NULL;
const plEcsI*         gptEcs               = NULL;
const plCameraI*      gptCamera            = NULL;
const plRefRendererI* gptRenderer          = NULL;
const plModelLoaderI* gptModelLoader       = NULL;
const plJobI*         gptJobs              = NULL;
const plDrawI*        gptDraw              = NULL;
const plUiI*          gptUi                = NULL;
const plIOI*          gptIO                = NULL;

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryI* ptApiRegistry, plAppData* ptAppData)
{
    const plDataRegistryI* ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(ptDataRegistry->get_data(PL_CONTEXT_MEMORY));

    plProfileContext* ptProfileCtx = pl_create_profile_context();
    plLogContext*     ptLogCtx     = pl_create_log_context();

    // add some context to data registry
    if(ptAppData == NULL)
    {
        ptAppData = PL_ALLOC(sizeof(plAppData));
        memset(ptAppData, 0, sizeof(plAppData));
    }

    ptDataRegistry->set_data("profile", ptProfileCtx);
    ptDataRegistry->set_data("log", ptLogCtx);

    // create log context
    pl_add_log_channel("Default", PL_CHANNEL_TYPE_CONSOLE);

    // load extensions
    const plExtensionRegistryI* ptExtensionRegistry = ptApiRegistry->first(PL_API_EXTENSION_REGISTRY);
    ptExtensionRegistry->load("pl_model_loader_ext", NULL, NULL, false);
    ptExtensionRegistry->load("pl_ref_renderer_ext", NULL, NULL, true);
    ptExtensionRegistry->load("pl_ui_ext",           NULL, NULL, true);
    ptExtensionRegistry->load("pl_debug_ext",        NULL, NULL, true);
    
    // load apis
    gptWindows     = ptApiRegistry->first(PL_API_WINDOW);
    gptStats       = ptApiRegistry->first(PL_API_STATS);
    gptGfx         = ptApiRegistry->first(PL_API_GRAPHICS);
    gptDevice      = ptApiRegistry->first(PL_API_DEVICE);
    gptEcs         = ptApiRegistry->first(PL_API_ECS);
    gptCamera      = ptApiRegistry->first(PL_API_CAMERA);
    gptRenderer    = ptApiRegistry->first(PL_API_REF_RENDERER);
    gptJobs        = ptApiRegistry->first(PL_API_JOB);
    gptModelLoader = ptApiRegistry->first(PL_API_MODEL_LOADER);
    gptDraw        = ptApiRegistry->first(PL_API_DRAW);
    gptUi          = ptApiRegistry->first(PL_API_UI);
    gptIO          = ptApiRegistry->first(PL_API_IO);

    // initialize job system
    gptJobs->initialize(0);

    const plWindowDesc tWindowDesc = {
        .pcName  = "Pilot Light App",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 600,
        .uHeight = 600,
    };
    ptAppData->ptWindow = gptWindows->create_window(&tWindowDesc);

    plIO* ptIO = gptIO->get_io();

    // setup reference renderer
    gptRenderer->initialize(ptAppData->ptWindow);

    // setup draw
    gptDraw->initialize(gptRenderer->get_graphics());
    plFontHandle tDefaultFont = gptDraw->add_default_font();
    gptDraw->build_font_atlas();

    // setup ui
    gptUi->initialize();
    gptUi->set_default_font(tDefaultFont);

    ptAppData->uSceneHandle0 = gptRenderer->create_scene();
    gptRenderer->load_skybox_from_panorama(ptAppData->uSceneHandle0, "../data/pilotlight-assets-master/environments/field.hdr", 256);
    ptAppData->uViewHandle0 = gptRenderer->create_view(ptAppData->uSceneHandle0, (plVec2){ptIO->afMainViewportSize[0] , ptIO->afMainViewportSize[1]});

    // temporary draw layer for submitting fullscreen quad of offscreen render
    ptAppData->ptDrawLayer = gptDraw->request_2d_layer(gptUi->get_draw_list(), "draw layer");

    plComponentLibrary* ptMainComponentLibrary = gptRenderer->get_component_library(ptAppData->uSceneHandle0);

    // create main camera
    plCameraComponent* ptMainCamera = NULL;
    ptAppData->tMainCamera = gptEcs->create_perspective_camera(ptMainComponentLibrary, "main camera", (plVec3){-9.6f, 2.096f, 0.86f}, PL_PI_3, ptIO->afMainViewportSize[0] / ptIO->afMainViewportSize[1], 0.1f, 48.0f, &ptMainCamera);
    gptCamera->set_pitch_yaw(ptMainCamera, -0.245f, 1.816f);
    gptCamera->update(ptMainCamera);
    gptEcs->attach_script(ptMainComponentLibrary, "pl_script_camera", PL_SCRIPT_FLAG_PLAYING, ptAppData->tMainCamera, NULL);

    // create lights
    plLightComponent* ptLight = NULL;
    ptAppData->tSunlight = gptEcs->create_directional_light(ptMainComponentLibrary, "sunlight", (plVec3){-0.375f, -1.0f, -0.085f}, &ptLight);
    ptLight->uCascadeCount = 4;
    ptLight->tFlags |= PL_LIGHT_FLAG_CAST_SHADOW;

    // load models
    plModelLoaderData tLoaderData0 = {0};
    gptModelLoader->load_gltf(ptMainComponentLibrary, "../data/glTF-Sample-Assets-main/Models/Sponza/glTF/Sponza.gltf", NULL, &tLoaderData0);
    gptRenderer->add_drawable_objects_to_scene(ptAppData->uSceneHandle0, tLoaderData0.uOpaqueCount, tLoaderData0.atOpaqueObjects, tLoaderData0.uTransparentCount, tLoaderData0.atTransparentObjects);
    gptModelLoader->free_data(&tLoaderData0);
    gptRenderer->finalize_scene(ptAppData->uSceneHandle0);

    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    gptJobs->cleanup();
    // ensure GPU is finished before cleanup
    gptDevice->flush_device(&gptRenderer->get_graphics()->tDevice);
    gptDraw->cleanup_font_atlas();
    gptUi->cleanup();
    gptDraw->cleanup();
    gptRenderer->cleanup();
    gptWindows->destroy_window(ptAppData->ptWindow);
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
    gptGfx->resize(gptRenderer->get_graphics());
    plIO* ptIO = gptIO->get_io();
    gptCamera->set_aspect(gptEcs->get_component(gptRenderer->get_component_library(ptAppData->uSceneHandle0), PL_COMPONENT_TYPE_CAMERA, ptAppData->tMainCamera), ptIO->afMainViewportSize[0] / ptIO->afMainViewportSize[1]);
    ptAppData->bResize = true;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    // begin profiling frame
    pl_begin_profile_frame();

    gptIO->new_frame();
    plIO* ptIO = gptIO->get_io();

    if(ptAppData->bResize)
    {
        gptRenderer->resize_view(ptAppData->uSceneHandle0, ptAppData->uViewHandle0, (plVec2){ptIO->afMainViewportSize[0], ptIO->afMainViewportSize[1]});
        ptAppData->bResize = false;
    }

    if(!gptRenderer->begin_frame())
    {
        pl_end_profile_frame();
        return;
    }

    gptDraw->new_frame();
    gptUi->new_frame();
    gptStats->new_frame();

    // run ecs system
    gptRenderer->run_ecs(ptAppData->uSceneHandle0);

    const plViewOptions tViewOptions = {
        .ptViewCamera = &ptAppData->tMainCamera,
        .ptSunLight   = &ptAppData->tSunlight
    };
    gptRenderer->render_scene(ptAppData->uSceneHandle0, ptAppData->uViewHandle0, tViewOptions);

    // add full screen quad for offscreen render
    gptDraw->add_image(ptAppData->ptDrawLayer, gptRenderer->get_view_color_texture(ptAppData->uSceneHandle0, ptAppData->uViewHandle0), (plVec2){0}, (plVec2){ptIO->afMainViewportSize[0], ptIO->afMainViewportSize[1]});

    char atBuffer[64] = {0};
    pl_sprintf(atBuffer, "%0.1f (FPS)", ptIO->fFrameRate);
    gptDraw->add_text(ptAppData->ptDrawLayer, (plFontHandle){0}, 13.0f, (plVec2){10.0f, 10.0f}, (plVec4){0.0f, 1.0f, 0.0f, 1.0f}, atBuffer, 0.0f);
    gptDraw->submit_2d_layer(ptAppData->ptDrawLayer);

    gptRenderer->end_frame();

    pl_end_profile_frame();
}