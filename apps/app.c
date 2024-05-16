/*
   app.c (just experimental & absolute mess)
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
#include "pl_ecs_ext.h"
#include "pl_resource_ext.h"
#include "pl_model_loader_ext.h"
#include "pl_ref_renderer_ext.h"
#include "pl_job_ext.h"
#include "pl_draw_3d_ext.h"

// misc
#include "helper_windows.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct plAppData_t
{

    // windows
    plWindow* ptWindow;

    // ui options
    plDebugApiInfo tDebugInfo;
    bool           bShowUiDemo;
    bool           bShowUiDebug;
    bool           bShowUiStyle;
    bool           bShowEntityWindow;
    bool           bReloadSwapchain;
    bool           bResize;
    bool           bFrustumCulling;
    bool           bAlwaysResize;


    // selected entityes
    bool bUpdateEntitySelection;
    plEntity tSelectedEntity;


    // scene
    bool         bDrawAllBoundingBoxes;
    bool         bDrawVisibleBoundingBoxes;
    bool         bFreezeCullCamera;
    plEntity     tCullCamera;
    plEntity     tMainCamera;
    plEntity     tSunlight;

    // shadows
    float fCascadeSplitLambda;

    // views
    uint32_t uSceneHandle0;
    uint32_t uViewHandle0;

    // drawing
    plDrawLayer* ptDrawLayer;
    plFontAtlas  tFontAtlas;

    // sync
    plSemaphoreHandle atSempahore[PL_FRAMES_IN_FLIGHT];
    uint64_t aulNextTimelineValue[PL_FRAMES_IN_FLIGHT];

} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] global apis
//-----------------------------------------------------------------------------

const plWindowI*            gptWindows           = NULL;
const plThreadsI*           gptThreads           = NULL;
const plApiRegistryI*       gptApiRegistry       = NULL;
const plDataRegistryI*      gptDataRegistry      = NULL;
const plStatsI*             gptStats             = NULL;
const plExtensionRegistryI* gptExtensionRegistry = NULL;
const plFileI*              gptFile              = NULL;
const plGraphicsI*          gptGfx               = NULL;
const plDeviceI*            gptDevice            = NULL;
const plDebugApiI*          gptDebug             = NULL;
const plImageI*             gptImage             = NULL;
const plEcsI*               gptEcs               = NULL;
const plCameraI*            gptCamera            = NULL;
const plResourceI*          gptResource          = NULL;
const plRefRendererI*       gptRenderer          = NULL;
const plModelLoaderI*       gptModelLoader       = NULL;
const plJobI*               gptJobs              = NULL;
const plDraw3dI*            gptDraw3d            = NULL;

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryI* ptApiRegistry, plAppData* ptAppData)
{
    gptApiRegistry  = ptApiRegistry;
    gptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(gptDataRegistry->get_data(PL_CONTEXT_MEMORY));
    pl_set_context(gptDataRegistry->get_data("ui"));

    if(ptAppData) // reload
    {
        pl_set_log_context(gptDataRegistry->get_data("log"));
        pl_set_profile_context(gptDataRegistry->get_data("profile"));

        // reload global apis
        gptWindows     = ptApiRegistry->first(PL_API_WINDOW);
        gptThreads     = ptApiRegistry->first(PL_API_THREADS);
        gptStats       = ptApiRegistry->first(PL_API_STATS);
        gptFile        = ptApiRegistry->first(PL_API_FILE);
        gptGfx         = ptApiRegistry->first(PL_API_GRAPHICS);
        gptDevice      = ptApiRegistry->first(PL_API_DEVICE);
        gptDebug       = ptApiRegistry->first(PL_API_DEBUG);
        gptImage       = ptApiRegistry->first(PL_API_IMAGE);
        gptEcs         = ptApiRegistry->first(PL_API_ECS);
        gptCamera      = ptApiRegistry->first(PL_API_CAMERA);
        gptResource    = ptApiRegistry->first(PL_API_RESOURCE);
        gptRenderer    = ptApiRegistry->first(PL_API_REF_RENDERER);
        gptJobs        = ptApiRegistry->first(PL_API_JOB);
        gptModelLoader = ptApiRegistry->first(PL_API_MODEL_LOADER);
        gptDraw3d      = ptApiRegistry->first(PL_API_DRAW_3D);

        return ptAppData;
    }

    plProfileContext* ptProfileCtx = pl_create_profile_context();
    plLogContext*     ptLogCtx     = pl_create_log_context();

    pl_begin_profile_frame();
    
    // add some context to data registry
    ptAppData = PL_ALLOC(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    ptAppData->bFrustumCulling = true;
    ptAppData->fCascadeSplitLambda = 0.95f;

    gptDataRegistry->set_data("profile", ptProfileCtx);
    gptDataRegistry->set_data("log", ptLogCtx);

    // create log context
    pl_add_log_channel("Default", PL_CHANNEL_TYPE_CONSOLE);
    pl_log_info("Setup logging");

    // load extensions
    const plExtensionRegistryI* ptExtensionRegistry = ptApiRegistry->first(PL_API_EXTENSION_REGISTRY);
    ptExtensionRegistry->load("pl_image_ext",          NULL, NULL, false);
    ptExtensionRegistry->load("pl_job_ext",            NULL, NULL, false);
    ptExtensionRegistry->load("pl_stats_ext",          NULL, NULL, false);
    ptExtensionRegistry->load("pl_graphics_ext",       NULL, NULL, false);
    ptExtensionRegistry->load("pl_gpu_allocators_ext", NULL, NULL, false);
    ptExtensionRegistry->load("pl_debug_ext",          NULL, NULL, true);
    ptExtensionRegistry->load("pl_ecs_ext",            NULL, NULL, false);
    ptExtensionRegistry->load("pl_resource_ext",       NULL, NULL, false);
    ptExtensionRegistry->load("pl_model_loader_ext",   NULL, NULL, false);
    ptExtensionRegistry->load("pl_draw_3d_ext",        NULL, NULL, true);
    ptExtensionRegistry->load("pl_ref_renderer_ext",   NULL, NULL, true);
    
    // load apis
    gptWindows     = ptApiRegistry->first(PL_API_WINDOW);
    gptThreads     = ptApiRegistry->first(PL_API_THREADS);
    gptStats       = ptApiRegistry->first(PL_API_STATS);
    gptFile        = ptApiRegistry->first(PL_API_FILE);
    gptGfx         = ptApiRegistry->first(PL_API_GRAPHICS);
    gptDevice      = ptApiRegistry->first(PL_API_DEVICE);
    gptDebug       = ptApiRegistry->first(PL_API_DEBUG);
    gptImage       = ptApiRegistry->first(PL_API_IMAGE);
    gptEcs         = ptApiRegistry->first(PL_API_ECS);
    gptCamera      = ptApiRegistry->first(PL_API_CAMERA);
    gptResource    = ptApiRegistry->first(PL_API_RESOURCE);
    gptRenderer    = ptApiRegistry->first(PL_API_REF_RENDERER);
    gptJobs        = ptApiRegistry->first(PL_API_JOB);
    gptModelLoader = ptApiRegistry->first(PL_API_MODEL_LOADER);
    gptDraw3d      = ptApiRegistry->first(PL_API_DRAW_3D);

    // initialize job system
    gptJobs->initialize(0);

    const plWindowDesc tWindowDesc = {
        .pcName  = "Pilot Light Example",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 600,
        .uHeight = 600,
    };
    ptAppData->ptWindow = gptWindows->create_window(&tWindowDesc);

    plIO* ptIO = pl_get_io();

    // setup reference renderer
    gptRenderer->initialize(ptAppData->ptWindow);
    gptDraw3d->initialize(gptRenderer->get_graphics());

    // setup ui
    pl_add_default_font(&ptAppData->tFontAtlas);
    pl_build_font_atlas(&ptAppData->tFontAtlas);
    gptGfx->setup_ui(gptRenderer->get_graphics(), gptRenderer->get_graphics()->tMainRenderPass);
    gptGfx->create_font_atlas(&ptAppData->tFontAtlas);
    pl_set_default_font(&ptAppData->tFontAtlas.sbtFonts[0]);

    // sync
    for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
        ptAppData->atSempahore[i] = gptDevice->create_semaphore(&gptRenderer->get_graphics()->tDevice, false);

    ptAppData->uSceneHandle0 = gptRenderer->create_scene();

    // pl_begin_profile_sample("load environments");
    // gptRenderer->load_skybox_from_panorama(ptAppData->uSceneHandle0, "../data/pilotlight-assets-master/environments/helipad.hdr", 1024);
    // pl_end_profile_sample();

    pl_begin_profile_sample("create scene views");
    ptAppData->uViewHandle0 = gptRenderer->create_view(ptAppData->uSceneHandle0, (plVec2){ptIO->afMainViewportSize[0] , ptIO->afMainViewportSize[1]});
    pl_end_profile_sample();

    // temporary draw layer for submitting fullscreen quad of offscreen render
    ptAppData->ptDrawLayer = pl_request_layer(pl_get_draw_list(NULL), "draw layer");

    plComponentLibrary* ptMainComponentLibrary = gptRenderer->get_component_library(ptAppData->uSceneHandle0);

    // create main camera
    ptAppData->tMainCamera = gptEcs->create_perspective_camera(ptMainComponentLibrary, "main camera", (plVec3){-9.6f, 2.096f, 0.86f}, PL_PI_3, ptIO->afMainViewportSize[0] / ptIO->afMainViewportSize[1], 0.1f, 30.0f);
    gptCamera->set_pitch_yaw(gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_CAMERA, ptAppData->tMainCamera), -0.245f, 1.816f);
    gptCamera->update(gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_CAMERA, ptAppData->tMainCamera));

    // create cull camera
    ptAppData->tCullCamera = gptEcs->create_perspective_camera(ptMainComponentLibrary, "cull camera", (plVec3){0, 0, 5.0f}, PL_PI_3, ptIO->afMainViewportSize[0] / ptIO->afMainViewportSize[1], 0.1f, 50.0f);
    gptCamera->set_pitch_yaw(gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_CAMERA, ptAppData->tCullCamera), 0.0f, PL_PI);
    gptCamera->update(gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_CAMERA, ptAppData->tCullCamera));

    // create lights
    gptEcs->create_point_light(ptMainComponentLibrary, "light", (plVec3){6.0f, 4.0f, -3.0f});
    ptAppData->tSunlight = gptEcs->create_directional_light(ptMainComponentLibrary, "sunlight", (plVec3){-0.375f, -1.0f, -0.085f});
    plLightComponent* ptLight = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_LIGHT, ptAppData->tSunlight);
    ptLight->uCascadeCount = 4;
    ptLight->tFlags |= PL_LIGHT_FLAG_CAST_SHADOW;

    // load models
    
    plModelLoaderData tLoaderData0 = {0};

    pl_begin_profile_sample("load models 0");
    // const plMat4 tTransform0 = pl_mat4_translate_xyz(0.0f, 1.0f, 0.0f);
    // gptModelLoader->load_gltf(ptMainComponentLibrary, "../data/town.gltf", &tTransform0, &tLoaderData0);
    gptModelLoader->load_gltf(ptMainComponentLibrary, "../data/glTF-Sample-Assets-main/Models/Sponza/glTF/Sponza.gltf", NULL, &tLoaderData0);
    gptModelLoader->load_gltf(ptMainComponentLibrary, "../data/glTF-Sample-Assets-main/Models/CesiumMan/glTF/CesiumMan.gltf", NULL, &tLoaderData0);
    gptRenderer->add_drawable_objects_to_scene(ptAppData->uSceneHandle0, tLoaderData0.uOpaqueCount, tLoaderData0.atOpaqueObjects, tLoaderData0.uTransparentCount, tLoaderData0.atTransparentObjects);
    gptModelLoader->free_data(&tLoaderData0);
    pl_end_profile_sample();

    pl_begin_profile_sample("finalize scene 0");
    gptRenderer->finalize_scene(ptAppData->uSceneHandle0);
    pl_end_profile_sample();

    pl_end_profile_frame();

    // temporary for profiling loading procedures
    uint32_t uSampleSize = 0;
    plProfileSample* ptSamples = pl_get_last_frame_samples(&uSampleSize);
    const char* pcSpacing = "                    ";
    for(uint32_t i = 0; i < uSampleSize; i++)
        printf("%s %s : %0.6f\n", &pcSpacing[20 - ptSamples[i].uDepth * 2], ptSamples[i].pcName, ptSamples[i].dDuration);

    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    gptJobs->cleanup();
    gptGfx->destroy_font_atlas(&ptAppData->tFontAtlas); // backend specific cleanup
    pl_cleanup_font_atlas(&ptAppData->tFontAtlas);
    gptDraw3d->cleanup();
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
    plIO* ptIO = pl_get_io();
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
    pl_begin_profile_sample(__FUNCTION__);

    // for convience
    plGraphics* ptGraphics = gptRenderer->get_graphics();
    plIO* ptIO = pl_get_io();

    if(ptAppData->bReloadSwapchain)
    {
        ptAppData->bReloadSwapchain = false;
        gptGfx->resize(ptGraphics);
        pl_end_profile_sample();
        pl_end_profile_frame();
        return;
    }

    if(ptAppData->bResize || ptAppData->bAlwaysResize)
    {
        // gptOS->sleep(32);
        gptRenderer->resize_view(ptAppData->uSceneHandle0, ptAppData->uViewHandle0, (plVec2){ptIO->afMainViewportSize[0], ptIO->afMainViewportSize[1]});
        ptAppData->bResize = false;
    }

    if(!gptGfx->begin_frame(ptGraphics))
    {
        gptGfx->resize(ptGraphics);
        pl_end_profile_sample();
        pl_end_profile_frame();
        return;
    }

    if(ptAppData->bUpdateEntitySelection)
    {
        if(ptAppData->tSelectedEntity.uIndex == UINT32_MAX)
            gptRenderer->select_entities(ptAppData->uSceneHandle0, 0, NULL);
        else
            gptRenderer->select_entities(ptAppData->uSceneHandle0, 1, &ptAppData->tSelectedEntity);
        ptAppData->bUpdateEntitySelection = false;
    }

    gptDraw3d->new_frame();

    // update statistics
    gptStats->new_frame();
    static double* pdFrameTimeCounter = NULL;
    static double* pdMemoryCounter = NULL;
    if(!pdFrameTimeCounter)
        pdFrameTimeCounter = gptStats->get_counter("frametime (ms)");
    if(!pdMemoryCounter)
        pdMemoryCounter = gptStats->get_counter("CPU memory");
    *pdFrameTimeCounter = (double)ptIO->fDeltaTime * 1000.0;
    *pdMemoryCounter = (double)pl_get_memory_context()->szMemoryUsage;

    // handle input
    plComponentLibrary* ptMainComponentLibrary = gptRenderer->get_component_library(ptAppData->uSceneHandle0);

    plCameraComponent* ptCamera = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_CAMERA, ptAppData->tMainCamera);
    plCameraComponent* ptCullCamera = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_CAMERA, ptAppData->tCullCamera);

    static const float fCameraTravelSpeed = 4.0f;
    static const float fCameraRotationSpeed = 0.005f;

    // camera space
    bool bOwnKeyboard = ptIO->bWantCaptureKeyboard;
    if(!bOwnKeyboard)
    {
        if(pl_is_key_down(PL_KEY_W)) gptCamera->translate(ptCamera,  0.0f,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime);
        if(pl_is_key_down(PL_KEY_S)) gptCamera->translate(ptCamera,  0.0f,  0.0f, -fCameraTravelSpeed* ptIO->fDeltaTime);
        if(pl_is_key_down(PL_KEY_A)) gptCamera->translate(ptCamera, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);
        if(pl_is_key_down(PL_KEY_D)) gptCamera->translate(ptCamera,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);

        // world space
        if(pl_is_key_down(PL_KEY_F)) { gptCamera->translate(ptCamera,  0.0f, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f); }
        if(pl_is_key_down(PL_KEY_R)) { gptCamera->translate(ptCamera,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f); }
    }

    bool bOwnMouse = ptIO->bWantCaptureMouse;
    if(!bOwnMouse && pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 1.0f))
    {
        const plVec2 tMouseDelta = pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f);
        gptCamera->rotate(ptCamera,  -tMouseDelta.y * fCameraRotationSpeed,  -tMouseDelta.x * fCameraRotationSpeed);
        pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
    }

    gptCamera->update(ptCamera);
    gptCamera->update(ptCullCamera);

    // run ecs system
    gptRenderer->run_ecs(ptAppData->uSceneHandle0);

    // new ui frame
    pl_new_frame();

    uint64_t ulValue0 = ptAppData->aulNextTimelineValue[ptGraphics->uCurrentFrameIndex];
    uint64_t ulValue1 = ulValue0 + 1;
    uint64_t ulValue2 = ulValue0 + 2;
    uint64_t ulValue3 = ulValue0 + 3;
    uint64_t ulValue4 = ulValue0 + 4;
    uint64_t ulValue5 = ulValue0 + 5;
    ptAppData->aulNextTimelineValue[ptGraphics->uCurrentFrameIndex] = ulValue5;


    // first set of work

    const plBeginCommandInfo tBeginInfo0 = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {ptAppData->atSempahore[ptGraphics->uCurrentFrameIndex]},
        .auWaitSemaphoreValues = {ulValue0},
    };

    plCommandBuffer tCommandBuffer = gptGfx->begin_command_recording(ptGraphics, &tBeginInfo0);

    gptRenderer->update_skin_textures(tCommandBuffer, ptAppData->uSceneHandle0);
    gptGfx->end_command_recording(ptGraphics, &tCommandBuffer);

    const plSubmitInfo tSubmitInfo0 = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {ptAppData->atSempahore[ptGraphics->uCurrentFrameIndex]},
        .auSignalSemaphoreValues = {ulValue1}
    };
    gptGfx->submit_command_buffer(ptGraphics, &tCommandBuffer, &tSubmitInfo0);

    const plBeginCommandInfo tBeginInfo00 = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {ptAppData->atSempahore[ptGraphics->uCurrentFrameIndex]},
        .auWaitSemaphoreValues = {ulValue1},
    };
    tCommandBuffer = gptGfx->begin_command_recording(ptGraphics, &tBeginInfo00);

    gptRenderer->perform_skinning(tCommandBuffer, ptAppData->uSceneHandle0);
    gptGfx->end_command_recording(ptGraphics, &tCommandBuffer);

    const plSubmitInfo tSubmitInfo00 = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {ptAppData->atSempahore[ptGraphics->uCurrentFrameIndex]},
        .auSignalSemaphoreValues = {ulValue2}
    };
    gptGfx->submit_command_buffer(ptGraphics, &tCommandBuffer, &tSubmitInfo00);



    const plBeginCommandInfo tBeginInfo11 = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {ptAppData->atSempahore[ptGraphics->uCurrentFrameIndex]},
        .auWaitSemaphoreValues = {ulValue2}
    };
    tCommandBuffer = gptGfx->begin_command_recording(ptGraphics, &tBeginInfo11);

    gptRenderer->generate_cascaded_shadow_map(tCommandBuffer, ptAppData->uSceneHandle0, ptAppData->uViewHandle0, ptAppData->tMainCamera, ptAppData->tSunlight, ptAppData->fCascadeSplitLambda);

    gptGfx->end_command_recording(ptGraphics, &tCommandBuffer);

    const plSubmitInfo tSubmitInfo11 = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {ptAppData->atSempahore[ptGraphics->uCurrentFrameIndex]},
        .auSignalSemaphoreValues = {ulValue3}
    };
    gptGfx->submit_command_buffer(ptGraphics, &tCommandBuffer, &tSubmitInfo11);

    plViewOptions tViewOptions = {
        .bShowAllBoundingBoxes     = ptAppData->bDrawAllBoundingBoxes,
        .bShowVisibleBoundingBoxes = ptAppData->bDrawVisibleBoundingBoxes,
        .bShowOrigin               = true,
        .bCullStats                = true,
        .ptViewCamera              = ptCamera,
        .ptCullCamera              = ptAppData->bFrustumCulling ? ptCamera : NULL
    };

    if(ptAppData->bFrustumCulling && ptAppData->bFreezeCullCamera)
        tViewOptions.ptCullCamera = ptCullCamera;

    // second set of work

    const plBeginCommandInfo tBeginInfo1 = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {ptAppData->atSempahore[ptGraphics->uCurrentFrameIndex]},
        .auWaitSemaphoreValues = {ulValue3}
    };
    tCommandBuffer = gptGfx->begin_command_recording(ptGraphics, &tBeginInfo1);
    gptRenderer->render_scene(tCommandBuffer, ptAppData->uSceneHandle0, ptAppData->uViewHandle0, tViewOptions);

    gptGfx->end_command_recording(ptGraphics, &tCommandBuffer);

    const plSubmitInfo tSubmitInfo1 = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {ptAppData->atSempahore[ptGraphics->uCurrentFrameIndex]},
        .auSignalSemaphoreValues = {ulValue4}
    };
    gptGfx->submit_command_buffer(ptGraphics, &tCommandBuffer, &tSubmitInfo1);

    // final set of work

    const plBeginCommandInfo tBeginInfo2 = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {ptAppData->atSempahore[ptGraphics->uCurrentFrameIndex]},
        .auWaitSemaphoreValues = {ulValue4},
    };
    tCommandBuffer = gptGfx->begin_command_recording(ptGraphics, &tBeginInfo2);

    pl_set_next_window_pos((plVec2){0, 0}, PL_UI_COND_ONCE);

    if(pl_begin_window("Pilot Light", NULL, false))
    {

        const float pfRatios[] = {1.0f};
        pl_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
        if(pl_collapsing_header("Information"))
        {
            
            pl_text("Pilot Light %s", PILOTLIGHT_VERSION);
            pl_text("Pilot Light UI %s", PL_UI_VERSION);
            pl_text("Pilot Light DS %s", PL_DS_VERSION);
            #ifdef PL_METAL_BACKEND
            pl_text("Graphics Backend: Metal");
            #elif PL_VULKAN_BACKEND
            pl_text("Graphics Backend: Vulkan");
            #else
            pl_text("Graphics Backend: Unknown");
            #endif

            pl_end_collapsing_header();
        }
        if(pl_collapsing_header("General Options"))
        {
            if(pl_checkbox("VSync", &ptGraphics->tSwapchain.bVSync))
                ptAppData->bReloadSwapchain = true;
            pl_checkbox("Frustum Culling", &ptAppData->bFrustumCulling);
            if(pl_checkbox("Freeze Culling Camera", &ptAppData->bFreezeCullCamera))
            {
                *ptCullCamera = *ptCamera;
            }
            pl_checkbox("Draw All Bounding Boxes", &ptAppData->bDrawAllBoundingBoxes);
            pl_checkbox("Draw Visible Bounding Boxes", &ptAppData->bDrawVisibleBoundingBoxes);
            pl_end_collapsing_header();
        }
        if(pl_collapsing_header("Tools"))
        {
            pl_checkbox("Device Memory Analyzer", &ptAppData->tDebugInfo.bShowDeviceMemoryAnalyzer);
            pl_checkbox("Memory Allocations", &ptAppData->tDebugInfo.bShowMemoryAllocations);
            pl_checkbox("Profiling", &ptAppData->tDebugInfo.bShowProfiling);
            pl_checkbox("Statistics", &ptAppData->tDebugInfo.bShowStats);
            pl_checkbox("Logging", &ptAppData->tDebugInfo.bShowLogging);
            pl_checkbox("Entities", &ptAppData->bShowEntityWindow);
            pl_end_collapsing_header();
        }

        if(pl_collapsing_header("Debug"))
        {
            if(pl_button("resize"))
                ptAppData->bResize = true;
            pl_checkbox("Always Resize", &ptAppData->bAlwaysResize);

            plLightComponent* ptLight = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_LIGHT, ptAppData->tSunlight);
            pl_slider_float("split", &ptAppData->fCascadeSplitLambda, 0.0f, 1.0f);
            pl_slider_float("x", &ptLight->tDirection.x, -1.0f, 1.0f);
            pl_slider_float("y", &ptLight->tDirection.y, -1.0f, 1.0f);
            pl_slider_float("z", &ptLight->tDirection.z, -1.0f, 1.0f);

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

    gptDebug->show_debug_windows(&ptAppData->tDebugInfo);

    if(ptAppData->bShowEntityWindow)
    {
        plEntity tNextSelectedEntity = pl_show_ecs_window(gptEcs, gptRenderer->get_component_library(ptAppData->uSceneHandle0), &ptAppData->bShowEntityWindow);
        if(tNextSelectedEntity.ulData != ptAppData->tSelectedEntity.ulData)
            ptAppData->bUpdateEntitySelection = true;
        ptAppData->tSelectedEntity = tNextSelectedEntity;
    }

    if(ptAppData->bShowUiDemo)
    {
        pl_begin_profile_sample("ui demo");
        pl_show_demo_window(&ptAppData->bShowUiDemo);
        pl_end_profile_sample();
    }
        
    if(ptAppData->bShowUiStyle)
        pl_show_style_editor_window(&ptAppData->bShowUiStyle);

    if(ptAppData->bShowUiDebug)
        pl_show_debug_window(&ptAppData->bShowUiDebug);

    // add full screen quad for offscreen render
    pl_add_image(ptAppData->ptDrawLayer, gptRenderer->get_view_texture_id(ptAppData->uSceneHandle0, ptAppData->uViewHandle0), (plVec2){0}, (plVec2){ptIO->afMainViewportSize[0], ptIO->afMainViewportSize[1]});
    pl_submit_layer(ptAppData->ptDrawLayer);

    plRenderEncoder tEncoder = gptGfx->begin_render_pass(ptGraphics, &tCommandBuffer, ptGraphics->tMainRenderPass);

    // render ui
    pl_begin_profile_sample("render ui");
    pl_render();
    gptGfx->draw_lists(ptGraphics, tEncoder, 1, pl_get_draw_list(NULL));
    gptGfx->draw_lists(ptGraphics, tEncoder, 1, pl_get_debug_draw_list(NULL));
    pl_end_profile_sample();

    gptGfx->end_render_pass(&tEncoder);

    const plSubmitInfo tSubmitInfo2 = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {ptAppData->atSempahore[ptGraphics->uCurrentFrameIndex]},
        .auSignalSemaphoreValues = {ulValue5},
    };
    gptGfx->end_command_recording(ptGraphics, &tCommandBuffer);
    if(!gptGfx->present(ptGraphics, &tCommandBuffer, &tSubmitInfo2))
        gptGfx->resize(ptGraphics);

    pl_end_profile_sample();
    pl_end_profile_frame();
}