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
#include "pl_ref_renderer_ext.h"

// misc
#include "helper_windows.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct plAppData_t
{
    // ui options
    plDebugApiInfo tDebugInfo;
    bool           bShowUiDemo;
    bool           bShowUiDebug;
    bool           bShowUiStyle;
    bool           bShowEntityWindow;
    bool           bReloadSwapchain;
    bool           bFrustumCulling;

    // scene
    bool         bDrawAllBoundingBoxes;
    bool         bDrawVisibleBoundingBoxes;
    bool         bFreezeCullCamera;
    plEntity     tCullCamera;
    plEntity     tMainCamera;
    plDrawList3D t3DDrawList;

    // drawing
    plDrawLayer* ptDrawLayer;

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
const plImageI*                gptImage             = NULL;
const plDrawStreamI*           gptStream            = NULL;
const plEcsI*                  gptEcs               = NULL;
const plCameraI*               gptCamera            = NULL;
const plResourceI*             gptResource          = NULL;
const plRefRendererI*          gptRenderer          = NULL;

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryApiI* ptApiRegistry, plAppData* ptAppData)
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
        gptStats    = ptApiRegistry->first(PL_API_STATS);
        gptFile     = ptApiRegistry->first(PL_API_FILE);
        gptGfx      = ptApiRegistry->first(PL_API_GRAPHICS);
        gptDevice   = ptApiRegistry->first(PL_API_DEVICE);
        gptDebug    = ptApiRegistry->first(PL_API_DEBUG);
        gptImage    = ptApiRegistry->first(PL_API_IMAGE);
        gptStream   = ptApiRegistry->first(PL_API_DRAW_STREAM);
        gptEcs      = ptApiRegistry->first(PL_API_ECS);
        gptCamera   = ptApiRegistry->first(PL_API_CAMERA);
        gptResource = ptApiRegistry->first(PL_API_RESOURCE);
        gptRenderer = ptApiRegistry->first(PL_API_REF_RENDERER);

        return ptAppData;
    }

    plProfileContext* ptProfileCtx = pl_create_profile_context();
    plLogContext*     ptLogCtx     = pl_create_log_context();
    
    // add some context to data registry
    ptAppData = PL_ALLOC(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    ptAppData->bFrustumCulling = true;

    gptDataRegistry->set_data("profile", ptProfileCtx);
    gptDataRegistry->set_data("log", ptLogCtx);

    // create log context
    pl_add_log_channel("Default", PL_CHANNEL_TYPE_CONSOLE);
    pl_log_info("Setup logging");

    // load extensions
    const plExtensionRegistryApiI* ptExtensionRegistry = ptApiRegistry->first(PL_API_EXTENSION_REGISTRY);
    ptExtensionRegistry->load("pl_image_ext",    "pl_load_image_ext",    "pl_unload_image_ext",    false);
    ptExtensionRegistry->load("pl_stats_ext",    "pl_load_stats_ext",    "pl_unload_stats_ext",    false);
    ptExtensionRegistry->load("pl_graphics_ext", "pl_load_graphics_ext", "pl_unload_graphics_ext", false);
    ptExtensionRegistry->load("pl_debug_ext",    "pl_load_debug_ext",    "pl_unload_debug_ext",    true);
    ptExtensionRegistry->load("pl_ecs_ext",      "pl_load_ecs_ext",      "pl_unload_ecs_ext",      false);
    ptExtensionRegistry->load("pl_resource_ext", "pl_load_resource_ext", "pl_unload_resource_ext", false);
    ptExtensionRegistry->load("pl_ref_renderer_ext", "pl_load_ext", "pl_unload_ext", true);

    // load apis
    gptStats    = ptApiRegistry->first(PL_API_STATS);
    gptFile     = ptApiRegistry->first(PL_API_FILE);
    gptGfx      = ptApiRegistry->first(PL_API_GRAPHICS);
    gptDevice   = ptApiRegistry->first(PL_API_DEVICE);
    gptDebug    = ptApiRegistry->first(PL_API_DEBUG);
    gptImage    = ptApiRegistry->first(PL_API_IMAGE);
    gptStream   = ptApiRegistry->first(PL_API_DRAW_STREAM);
    gptEcs      = ptApiRegistry->first(PL_API_ECS);
    gptCamera   = ptApiRegistry->first(PL_API_CAMERA);
    gptResource = ptApiRegistry->first(PL_API_RESOURCE);
    gptRenderer = ptApiRegistry->first(PL_API_REF_RENDERER);

    gptRenderer->initialize();
    gptGfx->register_3d_drawlist(gptRenderer->get_graphics(), &ptAppData->t3DDrawList);
    ptAppData->ptDrawLayer = pl_request_layer(pl_get_draw_list(NULL), "draw layer");

    // create main camera
    plIO* ptIO = pl_get_io();
    plComponentLibrary* ptComponentLibrary = gptRenderer->get_component_library();
    ptAppData->tMainCamera = gptEcs->create_perspective_camera(ptComponentLibrary, "main camera", (plVec3){0, 0, 5.0f}, PL_PI_3, ptIO->afMainViewportSize[0] / ptIO->afMainViewportSize[1], 0.01f, 400.0f);
    gptCamera->set_pitch_yaw(gptEcs->get_component(ptComponentLibrary, PL_COMPONENT_TYPE_CAMERA, ptAppData->tMainCamera), 0.0f, PL_PI);
    gptCamera->update(gptEcs->get_component(ptComponentLibrary, PL_COMPONENT_TYPE_CAMERA, ptAppData->tMainCamera));

    ptAppData->tCullCamera = gptEcs->create_perspective_camera(ptComponentLibrary, "cull camera", (plVec3){0, 0, 5.0f}, PL_PI_3, ptIO->afMainViewportSize[0] / ptIO->afMainViewportSize[1], 0.1f, 106.0f);
    gptCamera->set_pitch_yaw(gptEcs->get_component(ptComponentLibrary, PL_COMPONENT_TYPE_CAMERA, ptAppData->tCullCamera), 0.0f, PL_PI);
    gptCamera->update(gptEcs->get_component(ptComponentLibrary, PL_COMPONENT_TYPE_CAMERA, ptAppData->tCullCamera));

    pl_begin_profile_frame();
    pl_begin_profile_sample("load models");
    const plMat4 tTransform0 = pl_mat4_translate_xyz(0.0f, 1.0f, 0.0f);
    gptRenderer->load_skybox_from_panorama("../data/glTF-Sample-Environments-main/ennis.jpg", 1024);
    gptRenderer->load_gltf("../data/glTF-Sample-Assets-main/Models/Sponza/glTF/Sponza.gltf", NULL);
    gptRenderer->load_gltf("../data/glTF-Sample-Assets-main/Models/CesiumMan/glTF/CesiumMan.gltf", NULL);
    gptRenderer->load_stl("../data/pilotlight-assets-master/meshes/monkey.stl", (plVec4){1.0f, 0.0f, 0.0f, 0.80f}, &tTransform0);
    pl_end_profile_sample();

    pl_begin_profile_sample("finalize scene");
    gptRenderer->finalize_scene();
    pl_end_profile_sample();

    pl_end_profile_frame();

    uint32_t uSampleSize = 0;
    plProfileSample* ptSamples = pl_get_last_frame_samples(&uSampleSize);

    const char* pcSpacing = "                    ";

    for(uint32_t i = 0; i < uSampleSize; i++)
    {
        printf("%s %s : %0.6f\n", &pcSpacing[20 - ptSamples[i].uDepth * 2], ptSamples[i].pcName, ptSamples[i].dDuration);
    }

    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    gptRenderer->cleanup();
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
    gptRenderer->resize();
    plIO* ptIO = pl_get_io();
    plComponentLibrary* ptComponentLibrary = gptRenderer->get_component_library();
    gptCamera->set_aspect(gptEcs->get_component(ptComponentLibrary, PL_COMPONENT_TYPE_CAMERA, ptAppData->tMainCamera), ptIO->afMainViewportSize[0] / ptIO->afMainViewportSize[1]);
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
        gptRenderer->resize();
        pl_end_profile_sample();
        pl_end_profile_frame();
        return;
    }

    if(!gptGfx->begin_frame(ptGraphics))
    {
        gptRenderer->resize();
        pl_end_profile_sample();
        pl_end_profile_frame();
        return;
    }

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
    plComponentLibrary* ptComponentLibrary = gptRenderer->get_component_library();

    plCameraComponent* ptCamera = gptEcs->get_component(ptComponentLibrary, PL_COMPONENT_TYPE_CAMERA, ptAppData->tMainCamera);
    plCameraComponent* ptCullCamera = gptEcs->get_component(ptComponentLibrary, PL_COMPONENT_TYPE_CAMERA, ptAppData->tCullCamera);

    static const float fCameraTravelSpeed = 4.0f;
    static const float fCameraRotationSpeed = 0.005f;

    // camera space
    if(pl_is_key_down(PL_KEY_W)) gptCamera->translate(ptCamera,  0.0f,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime);
    if(pl_is_key_down(PL_KEY_S)) gptCamera->translate(ptCamera,  0.0f,  0.0f, -fCameraTravelSpeed* ptIO->fDeltaTime);
    if(pl_is_key_down(PL_KEY_A)) gptCamera->translate(ptCamera, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);
    if(pl_is_key_down(PL_KEY_D)) gptCamera->translate(ptCamera,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);

    // world space
    if(pl_is_key_down(PL_KEY_F)) { gptCamera->translate(ptCamera,  0.0f, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f); }
    if(pl_is_key_down(PL_KEY_R)) { gptCamera->translate(ptCamera,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f); }

    bool bOwnMouse = ptIO->bWantCaptureMouse;
    if(!bOwnMouse && pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 1.0f))
    {
        const plVec2 tMouseDelta = pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f);
        gptCamera->rotate(ptCamera,  -tMouseDelta.y * fCameraRotationSpeed,  -tMouseDelta.x * fCameraRotationSpeed);
        pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
    }

    if(!ptAppData->bFreezeCullCamera)
    {
        ptCullCamera->tPos = ptCamera->tPos;
        ptCullCamera->fYaw = ptCamera->fYaw;
        ptCullCamera->fPitch = ptCamera->fPitch;
    }

    gptCamera->update(ptCamera);
    gptCamera->update(ptCullCamera);

    gptRenderer->run_ecs();

    if(ptAppData->bFrustumCulling)
    {
        if(ptAppData->bFreezeCullCamera)
        {
            gptRenderer->cull_objects(ptCullCamera);
            gptGfx->add_3d_frustum(&ptAppData->t3DDrawList, &ptCullCamera->tTransformMat, ptCullCamera->fFieldOfView, ptCullCamera->fAspectRatio, ptCullCamera->fNearZ, ptCullCamera->fFarZ, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 0.02f);
        }
        else
            gptRenderer->cull_objects(ptCamera);
    }
    else
    {
        gptRenderer->uncull_objects(ptCamera);
    }

    pl_new_frame();

    gptGfx->begin_recording(ptGraphics);

    gptGfx->begin_pass(ptGraphics, gptRenderer->get_offscreen_render_pass());
    gptRenderer->submit_draw_stream(ptCamera);

    if(ptAppData->bDrawAllBoundingBoxes)
        gptRenderer->draw_all_bound_boxes(&ptAppData->t3DDrawList);
    else if(ptAppData->bDrawVisibleBoundingBoxes)
        gptRenderer->draw_visible_bound_boxes(&ptAppData->t3DDrawList);

    const plMat4 tTransform = pl_identity_mat4();
    gptGfx->add_3d_transform(&ptAppData->t3DDrawList, &tTransform, 10.0f, 0.02f);

    const plMat4 tMVP = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);
    gptGfx->submit_3d_drawlist(&ptAppData->t3DDrawList, ptIO->afMainViewportSize[0], ptIO->afMainViewportSize[1], &tMVP, PL_PIPELINE_FLAG_DEPTH_TEST | PL_PIPELINE_FLAG_DEPTH_WRITE, gptRenderer->get_offscreen_render_pass(), 1);


    gptGfx->end_pass(ptGraphics);

    gptGfx->begin_main_pass(ptGraphics, ptGraphics->tMainRenderPass);

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
            pl_checkbox("Freeze Culling Camera", &ptAppData->bFreezeCullCamera);
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

        if(pl_collapsing_header("User Interface"))
        {
            pl_checkbox("UI Debug", &ptAppData->bShowUiDebug);
            pl_checkbox("UI Demo", &ptAppData->bShowUiDemo);
            pl_checkbox("UI Style", &ptAppData->bShowUiStyle);
            pl_end_collapsing_header();
        }
        pl_end_window();
    }

    pl_add_image(ptAppData->ptDrawLayer, gptRenderer->get_offscreen_texture_id(), (plVec2){0}, (plVec2){ptIO->afMainViewportSize[0], ptIO->afMainViewportSize[1]});

    gptDebug->show_windows(&ptAppData->tDebugInfo);

    if(ptAppData->bShowEntityWindow)
        pl_show_ecs_window(gptEcs, gptRenderer->get_component_library(), &ptAppData->bShowEntityWindow);

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

    pl_submit_layer(ptAppData->ptDrawLayer);
    gptRenderer->submit_ui();

    gptGfx->end_main_pass(ptGraphics);
    gptGfx->end_recording(ptGraphics);
    if(!gptGfx->end_frame(ptGraphics))
        gptRenderer->resize();

    pl_end_profile_sample();
    pl_end_profile_frame();
}