/*
   editor.c

   Notes:
     * absolute mess
     * mostly a sandbox for now & testing experimental stuff
     * look at examples for more stable APIs
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] helper functions
// [SECTION] structs & enums
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
// [SECTION] helper function implementations
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
#include "pl_image_ext.h"
#include "pl_stats_ext.h"
#include "pl_graphics_ext.h"
#include "pl_debug_ext.h"
#include "pl_ecs_ext.h"
#include "pl_resource_ext.h"
#include "pl_model_loader_ext.h"
#include "pl_ref_renderer_ext.h"
#include "pl_job_ext.h"
#include "pl_draw_ext.h"
#include "pl_ui_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] helper functions
//-----------------------------------------------------------------------------

typedef struct _plAppData plAppData;
static void pl_show_ecs_window(plAppData*, bool* pbShowWindow);
static void pl_render_translation_gizmo(plDrawList3D*, const plVec3* ptCenter, const plVec3* ptCameraPos);
static void pl_render_rotation_gizmo(plDrawList3D*, const plVec3* ptCenter, const plVec3* ptCameraPos);
static void pl_render_scale_gizmo(plDrawList3D*, const plVec3* ptCenter, const plVec3* ptCameraPos);

//-----------------------------------------------------------------------------
// [SECTION] structs & enums
//-----------------------------------------------------------------------------

typedef enum _plSelectionMode
{
    PL_SELECTION_MODE_NONE,
    PL_SELECTION_MODE_TRANSLATION,
    PL_SELECTION_MODE_ROTATION,
    PL_SELECTION_MODE_SCALE,

    PL_SELECTION_MODE_COUNT,
} plSelectionMode;

typedef struct _plAppData
{

    // windows
    plWindow* ptWindow;

    // ui options
    plDebugApiInfo tDebugInfo;
    bool           bShowUiDemo;
    bool           bShowUiDebug;
    bool           bShowUiStyle;
    bool           bShowEntityWindow;
    bool           bResize;
    bool           bAlwaysResize;

    // scene
    bool         bFreezeCullCamera;
    plEntity     tCullCamera;
    plEntity     tMainCamera;
    plEntity     tSunlight;

    // views
    uint32_t uSceneHandle0;
    uint32_t uViewHandle0;

    // drawing
    plDrawLayer2D* ptDrawLayer;

    // selection stuff
    plEntity tSelectedEntity;
    plSelectionMode tSelectionMode; 

} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] global apis
//-----------------------------------------------------------------------------

const plWindowI*      gptWindows           = NULL;
const plStatsI*       gptStats             = NULL;
const plGraphicsI*    gptGfx               = NULL;
const plDeviceI*      gptDevice            = NULL;
const plDebugApiI*    gptDebug             = NULL;
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

    if(ptAppData) // reload
    {
        pl_set_log_context(ptDataRegistry->get_data("log"));
        pl_set_profile_context(ptDataRegistry->get_data("profile"));

        // reload global apis
        gptWindows     = ptApiRegistry->first(PL_API_WINDOW);
        gptStats       = ptApiRegistry->first(PL_API_STATS);
        gptGfx         = ptApiRegistry->first(PL_API_GRAPHICS);
        gptDevice      = ptApiRegistry->first(PL_API_DEVICE);
        gptDebug       = ptApiRegistry->first(PL_API_DEBUG);
        gptEcs         = ptApiRegistry->first(PL_API_ECS);
        gptCamera      = ptApiRegistry->first(PL_API_CAMERA);
        gptRenderer    = ptApiRegistry->first(PL_API_REF_RENDERER);
        gptJobs        = ptApiRegistry->first(PL_API_JOB);
        gptModelLoader = ptApiRegistry->first(PL_API_MODEL_LOADER);
        gptDraw        = ptApiRegistry->first(PL_API_DRAW);
        gptUi          = ptApiRegistry->first(PL_API_UI);
        gptIO          = ptApiRegistry->first(PL_API_IO);

        return ptAppData;
    }

    plProfileContext* ptProfileCtx = pl_create_profile_context();
    plLogContext*     ptLogCtx     = pl_create_log_context();

    pl_begin_profile_frame();
    
    // add some context to data registry
    ptAppData = PL_ALLOC(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));
    ptAppData->tSelectedEntity.ulData = UINT64_MAX;
    ptAppData->tSelectionMode = PL_SELECTION_MODE_TRANSLATION;

    ptDataRegistry->set_data("profile", ptProfileCtx);
    ptDataRegistry->set_data("log", ptLogCtx);

    // create log context
    pl_add_log_channel("Default", PL_CHANNEL_TYPE_CONSOLE);
    pl_log_info("Setup logging");

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
    gptDebug       = ptApiRegistry->first(PL_API_DEBUG);
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
        .pcName  = "Pilot Light Editor",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 600,
        .uHeight = 600,
    };
    ptAppData->ptWindow = gptWindows->create_window(&tWindowDesc);

    plIO* ptIO = gptIO->get_io();

    // setup reference renderer
    gptRenderer->initialize(ptAppData->ptWindow);

    ptAppData->uSceneHandle0 = gptRenderer->create_scene();

    pl_begin_profile_sample("load environments");
    gptRenderer->load_skybox_from_panorama(ptAppData->uSceneHandle0, "../data/pilotlight-assets-master/environments/helipad.hdr", 256);
    pl_end_profile_sample();

    pl_begin_profile_sample("create scene views");
    ptAppData->uViewHandle0 = gptRenderer->create_view(ptAppData->uSceneHandle0, (plVec2){ptIO->afMainViewportSize[0] , ptIO->afMainViewportSize[1]});
    pl_end_profile_sample();

    // temporary draw layer for submitting fullscreen quad of offscreen render
    ptAppData->ptDrawLayer = gptDraw->request_2d_layer(gptUi->get_draw_list(), "draw layer");

    plComponentLibrary* ptMainComponentLibrary = gptRenderer->get_component_library(ptAppData->uSceneHandle0);

    // create main camera
    plCameraComponent* ptMainCamera = NULL;
    ptAppData->tMainCamera = gptEcs->create_perspective_camera(ptMainComponentLibrary, "main camera", (plVec3){-9.6f, 2.096f, 0.86f}, PL_PI_3, ptIO->afMainViewportSize[0] / ptIO->afMainViewportSize[1], 0.1f, 48.0f, &ptMainCamera);
    gptCamera->set_pitch_yaw(ptMainCamera, -0.245f, 1.816f);
    gptCamera->update(ptMainCamera);
    gptEcs->attach_script(ptMainComponentLibrary, "pl_script_camera", PL_SCRIPT_FLAG_PLAYING, ptAppData->tMainCamera, NULL);

    // create cull camera
    plCameraComponent* ptCullCamera = NULL;
    ptAppData->tCullCamera = gptEcs->create_perspective_camera(ptMainComponentLibrary, "cull camera", (plVec3){0, 0, 5.0f}, PL_PI_3, ptIO->afMainViewportSize[0] / ptIO->afMainViewportSize[1], 0.1f, 50.0f, &ptCullCamera);
    gptCamera->set_pitch_yaw(ptCullCamera, 0.0f, PL_PI);
    gptCamera->update(ptCullCamera);

    // create lights
    gptEcs->create_point_light(ptMainComponentLibrary, "light", (plVec3){6.0f, 4.0f, -3.0f}, NULL);

    plLightComponent* ptLight = NULL;
    ptAppData->tSunlight = gptEcs->create_directional_light(ptMainComponentLibrary, "sunlight", (plVec3){-0.375f, -1.0f, -0.085f}, &ptLight);
    ptLight->uCascadeCount = 4;
    ptLight->tFlags |= PL_LIGHT_FLAG_CAST_SHADOW;

    // load models
    
    plModelLoaderData tLoaderData0 = {0};

    pl_begin_profile_sample("load models 0");
    const plMat4 tTransform = pl_mat4_translate_xyz(5.0f, 0.0F, 0.0f);
    // gptModelLoader->load_gltf(ptMainComponentLibrary, "../data/glTF-Sample-Assets-main/Models/DamagedHelmet/glTF/DamagedHelmet.gltf", NULL, &tLoaderData0);
    gptModelLoader->load_gltf(ptMainComponentLibrary, "../data/glTF-Sample-Assets-main/Models/CesiumMan/glTF/CesiumMan.gltf", &tTransform, &tLoaderData0);
    gptModelLoader->load_gltf(ptMainComponentLibrary, "../data/glTF-Sample-Assets-main/Models/Sponza/glTF/Sponza.gltf", NULL, &tLoaderData0);
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
    pl_begin_profile_sample(__FUNCTION__);

    gptIO->new_frame();

    // for convience
    plGraphics* ptGraphics = gptRenderer->get_graphics();
    plIO* ptIO = gptIO->get_io();

    if(ptAppData->bResize || ptAppData->bAlwaysResize)
    {
        // gptOS->sleep(32);
        gptRenderer->resize_view(ptAppData->uSceneHandle0, ptAppData->uViewHandle0, (plVec2){ptIO->afMainViewportSize[0], ptIO->afMainViewportSize[1]});
        ptAppData->bResize = false;
    }

    if(!gptRenderer->begin_frame())
    {
        pl_end_profile_sample();
        pl_end_profile_frame();
        return;
    }

    gptDraw->new_frame();
    gptUi->new_frame();

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
    gptCamera->update(ptCullCamera);

    // run ecs system
    gptRenderer->run_ecs(ptAppData->uSceneHandle0);

    plEntity tNextEntity = gptRenderer->get_picked_entity();
    if(tNextEntity.ulData == 0)
    {
        ptAppData->tSelectedEntity.ulData = UINT64_MAX;
        gptRenderer->select_entities(ptAppData->uSceneHandle0, 0, NULL);
    }
    else if(tNextEntity.ulData != UINT64_MAX && ptAppData->tSelectedEntity.ulData != tNextEntity.ulData)
    {
        ptAppData->tSelectedEntity = tNextEntity;
        gptRenderer->select_entities(ptAppData->uSceneHandle0, 1, &ptAppData->tSelectedEntity);
    }

    if(gptIO->is_key_pressed(PL_KEY_M, true))
    {
        ptAppData->tSelectionMode = (ptAppData->tSelectionMode + 1) % PL_SELECTION_MODE_COUNT;
    }

    if(ptAppData->bShowEntityWindow)
    {
        pl_show_ecs_window(ptAppData, &ptAppData->bShowEntityWindow);
    }

    if(ptAppData->tSelectedEntity.uIndex != UINT32_MAX)
    {
        plObjectComponent* ptSelectedObject = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_OBJECT, ptAppData->tSelectedEntity);
        plTransformComponent* ptSelectedTransform = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptAppData->tSelectedEntity);
        if(ptSelectedObject)
            ptSelectedTransform = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptSelectedObject->tTransform);
        if(ptSelectedTransform)
        {
            plDrawList3D* ptGizmoDrawlist = gptRenderer->get_gizmo_drawlist(ptAppData->uSceneHandle0, ptAppData->uViewHandle0);
            if(ptAppData->tSelectionMode == PL_SELECTION_MODE_TRANSLATION)
                pl_render_translation_gizmo(ptGizmoDrawlist, &ptSelectedTransform->tWorld.col[3].xyz, &ptCamera->tPos);
            else if(ptAppData->tSelectionMode == PL_SELECTION_MODE_ROTATION)
                pl_render_rotation_gizmo(ptGizmoDrawlist, &ptSelectedTransform->tWorld.col[3].xyz, &ptCamera->tPos);
            else if(ptAppData->tSelectionMode == PL_SELECTION_MODE_SCALE)
                pl_render_scale_gizmo(ptGizmoDrawlist, &ptSelectedTransform->tWorld.col[3].xyz, &ptCamera->tPos);
        }
        
    }

    const plViewOptions tViewOptions = {
        .ptViewCamera = &ptAppData->tMainCamera,
        .ptCullCamera = ptAppData->bFreezeCullCamera ? &ptAppData->tCullCamera : NULL,
        .ptSunLight = &ptAppData->tSunlight
    };
    gptRenderer->render_scene(ptAppData->uSceneHandle0, ptAppData->uViewHandle0, tViewOptions);

    gptUi->set_next_window_pos((plVec2){0, 0}, PL_UI_COND_ONCE);

    if(gptUi->begin_window("Pilot Light", NULL, false))
    {

        const float pfRatios[] = {1.0f};
        gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
        if(gptUi->collapsing_header("Information"))
        {
            
            gptUi->text("Pilot Light %s", PILOTLIGHT_VERSION);
            gptUi->text("Pilot Light UI %s", PL_UI_EXT_VERSION);
            gptUi->text("Pilot Light DS %s", PL_DS_VERSION);
            #ifdef PL_METAL_BACKEND
            gptUi->text("Graphics Backend: Metal");
            #elif PL_VULKAN_BACKEND
            gptUi->text("Graphics Backend: Vulkan");
            #else
            gptUi->text("Graphics Backend: Unknown");
            #endif

            gptUi->end_collapsing_header();
        }
        if(gptUi->collapsing_header("App Options"))
        {
            if(gptUi->checkbox("Freeze Culling Camera", &ptAppData->bFreezeCullCamera))
            {
                *ptCullCamera = *ptCamera;
            }

            plLightComponent* ptLight = gptEcs->get_component(ptMainComponentLibrary,  PL_COMPONENT_TYPE_LIGHT, ptAppData->tSunlight);
            int iCascadeCount  = (int)ptLight->uCascadeCount;
            if(gptUi->slider_int("Sunlight Cascades", &iCascadeCount, 1, 4))
            {
                ptLight->uCascadeCount = (uint32_t)iCascadeCount;
            }

            gptUi->end_collapsing_header();
        }

        gptRenderer->show_graphics_options();

        if(gptUi->collapsing_header("Tools"))
        {
            gptUi->checkbox("Device Memory Analyzer", &ptAppData->tDebugInfo.bShowDeviceMemoryAnalyzer);
            gptUi->checkbox("Memory Allocations", &ptAppData->tDebugInfo.bShowMemoryAllocations);
            gptUi->checkbox("Profiling", &ptAppData->tDebugInfo.bShowProfiling);
            gptUi->checkbox("Statistics", &ptAppData->tDebugInfo.bShowStats);
            gptUi->checkbox("Logging", &ptAppData->tDebugInfo.bShowLogging);
            gptUi->checkbox("Entities", &ptAppData->bShowEntityWindow);
            gptUi->end_collapsing_header();
        }

        if(gptUi->collapsing_header("Debug"))
        {
            if(gptUi->button("resize"))
                ptAppData->bResize = true;
            gptUi->checkbox("Always Resize", &ptAppData->bAlwaysResize);

            plLightComponent* ptLight = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_LIGHT, ptAppData->tSunlight);
            gptUi->slider_float("x", &ptLight->tDirection.x, -1.0f, 1.0f);
            gptUi->slider_float("y", &ptLight->tDirection.y, -1.0f, 1.0f);
            gptUi->slider_float("z", &ptLight->tDirection.z, -1.0f, 1.0f);

            gptUi->end_collapsing_header();
        }

        if(gptUi->collapsing_header("User Interface"))
        {
            gptUi->checkbox("UI Debug", &ptAppData->bShowUiDebug);
            gptUi->checkbox("UI Demo", &ptAppData->bShowUiDemo);
            gptUi->checkbox("UI Style", &ptAppData->bShowUiStyle);
            gptUi->end_collapsing_header();
        }
        gptUi->end_window();
    }

    gptDebug->show_debug_windows(&ptAppData->tDebugInfo);

    if(ptAppData->bShowUiDemo)
    {
        pl_begin_profile_sample("ui demo");
        gptUi->show_demo_window(&ptAppData->bShowUiDemo);
        pl_end_profile_sample();
    }
        
    if(ptAppData->bShowUiStyle)
        gptUi->show_style_editor_window(&ptAppData->bShowUiStyle);

    if(ptAppData->bShowUiDebug)
        gptUi->show_debug_window(&ptAppData->bShowUiDebug);

    // add full screen quad for offscreen render
    gptDraw->add_image(ptAppData->ptDrawLayer, gptRenderer->get_view_color_texture(ptAppData->uSceneHandle0, ptAppData->uViewHandle0), (plVec2){0}, (plVec2){ptIO->afMainViewportSize[0], ptIO->afMainViewportSize[1]});
    gptDraw->submit_2d_layer(ptAppData->ptDrawLayer);

    gptRenderer->end_frame();

    pl_end_profile_sample();
    pl_end_profile_frame();
}

//-----------------------------------------------------------------------------
// [SECTION] helper function implementations
//-----------------------------------------------------------------------------

static void
pl_render_translation_gizmo(plDrawList3D* ptDrawlist, const plVec3* ptCenter, const plVec3* ptCameraPos)
{
    const float fScale = pl_length_vec3(pl_sub_vec3(*ptCenter, *ptCameraPos));
    const float fAxisRadius  = 0.0035f * fScale;
    const float fArrowRadius = 0.0075f * fScale;
    const float fArrowLength = 0.03f * fScale;
    const float fLength = 0.15f * fScale;

    // x arrow head
    plDrawConeDesc tDrawDesc0 = {0};
    gptDraw->fill_cone_desc_default(&tDrawDesc0);
    tDrawDesc0.tColor = (plVec4){1.0f, 0.0f, 0.0f, 1.0f};
    tDrawDesc0.tBasePos = (plVec3){ptCenter->x + fLength - fArrowLength, ptCenter->y, ptCenter->z};
    tDrawDesc0.tTipPos = (plVec3){ptCenter->x + fLength, ptCenter->y, ptCenter->z};
    tDrawDesc0.fRadius = fArrowRadius;
    gptDraw->add_3d_cone_filled_ex(ptDrawlist, &tDrawDesc0);

    // y arrow head
    plDrawConeDesc tDrawDesc1 = {0};
    gptDraw->fill_cone_desc_default(&tDrawDesc1);
    tDrawDesc1.tColor = (plVec4){0.0f, 1.0f, 0.0f, 1.0f};
    tDrawDesc1.tBasePos = (plVec3){ptCenter->x, ptCenter->y + fLength - fArrowLength, ptCenter->z};
    tDrawDesc1.tTipPos = (plVec3){ptCenter->x, ptCenter->y + fLength, ptCenter->z};
    tDrawDesc1.fRadius = fArrowRadius;
    gptDraw->add_3d_cone_filled_ex(ptDrawlist, &tDrawDesc1);

    // z arrow head
    plDrawConeDesc tDrawDesc2 = {0};
    gptDraw->fill_cone_desc_default(&tDrawDesc2);
    tDrawDesc2.tColor = (plVec4){0.0f, 0.0f, 1.0f, 1.0f};
    tDrawDesc2.tBasePos = (plVec3){ptCenter->x, ptCenter->y, ptCenter->z + fLength - fArrowLength};
    tDrawDesc2.tTipPos = (plVec3){ptCenter->x, ptCenter->y, ptCenter->z + fLength};
    tDrawDesc2.fRadius = fArrowRadius;
    gptDraw->add_3d_cone_filled_ex(ptDrawlist, &tDrawDesc2);

    // x axis
    plDrawCylinderDesc tDrawDesc3 = {0};
    gptDraw->fill_cylinder_desc_default(&tDrawDesc3);
    tDrawDesc3.tColor = (plVec4){1.0f, 0.0f, 0.0f, 1.0f};
    tDrawDesc3.tBasePos = *ptCenter;
    tDrawDesc3.tTipPos = (plVec3){ptCenter->x + fLength - fArrowLength, ptCenter->y, ptCenter->z};
    tDrawDesc3.fRadius = fAxisRadius;
    gptDraw->add_3d_cylinder_filled_ex(ptDrawlist, &tDrawDesc3);

    // y axis
    plDrawCylinderDesc tDrawDesc4 = {0};
    gptDraw->fill_cylinder_desc_default(&tDrawDesc4);
    tDrawDesc4.tColor = (plVec4){0.0f, 1.0f, 0.0f, 1.0f};
    tDrawDesc4.tBasePos = *ptCenter;
    tDrawDesc4.tTipPos = (plVec3){ptCenter->x, ptCenter->y + fLength - fArrowLength, ptCenter->z};
    tDrawDesc4.fRadius = fAxisRadius;
    gptDraw->add_3d_cylinder_filled_ex(ptDrawlist, &tDrawDesc4);

    // z axis
    plDrawCylinderDesc tDrawDesc5 = {0};
    gptDraw->fill_cylinder_desc_default(&tDrawDesc5);
    tDrawDesc5.tColor = (plVec4){0.0f, 0.0f, 1.0f, 1.0f};
    tDrawDesc5.tBasePos = *ptCenter;
    tDrawDesc5.tTipPos = (plVec3){ptCenter->x, ptCenter->y, ptCenter->z + fLength - fArrowLength};
    tDrawDesc5.fRadius = fAxisRadius;
    gptDraw->add_3d_cylinder_filled_ex(ptDrawlist, &tDrawDesc5);

    // origin
    gptDraw->add_3d_centered_box_filled(ptDrawlist,
        *ptCenter,
        fAxisRadius * 4,
        fAxisRadius * 4,
        fAxisRadius * 4,
        (plVec4){0.5f, 0.5f, 0.5f, 1.0f});

    // PLANES
    gptDraw->add_3d_plane_xy_filled(ptDrawlist, (plVec3){ptCenter->x + fLength * 0.25f, ptCenter->y + fLength * 0.25f, ptCenter->z}, fLength * 0.25f, fLength * 0.25f, (plVec4){0.0f, 0.0f, 1.0f, 0.5f});
    gptDraw->add_3d_plane_yz_filled(ptDrawlist, (plVec3){ptCenter->x, ptCenter->y + fLength * 0.25f, ptCenter->z + fLength * 0.25f}, fLength * 0.25f, fLength * 0.25f, (plVec4){1.0f, 0.0f, 0.0f, 0.5f});
    gptDraw->add_3d_plane_xz_filled(ptDrawlist, (plVec3){ptCenter->x + fLength * 0.25f, ptCenter->y, ptCenter->z + fLength * 0.25f}, fLength * 0.25f, fLength * 0.25f, (plVec4){0.0f, 1.0f, 0.0f, 0.5f});
}

static void
pl_render_rotation_gizmo(plDrawList3D* ptDrawlist, const plVec3* ptCenter, const plVec3* ptCameraPos)
{
    const float fScale = pl_length_vec3(pl_sub_vec3(*ptCenter, *ptCameraPos));
    
    const float fOuterRadius = 0.15f * fScale;
    const float fInnerRadius = fOuterRadius - 0.03f * fScale;
    gptDraw->add_3d_band_xz_filled(ptDrawlist, *ptCenter, fInnerRadius, fOuterRadius, (plVec4){0.0f, 1.0f, 0.0f, 1.0f}, 36);
    gptDraw->add_3d_band_xy_filled(ptDrawlist, *ptCenter, fInnerRadius, fOuterRadius, (plVec4){0.0f, 0.0f, 1.0f, 1.0f}, 36);
    gptDraw->add_3d_band_yz_filled(ptDrawlist, *ptCenter, fInnerRadius, fOuterRadius, (plVec4){1.0f, 0.0f, 0.0f, 1.0f}, 36);
}

static void
pl_render_scale_gizmo(plDrawList3D* ptDrawlist, const plVec3* ptCenter, const plVec3* ptCameraPos)
{
    const float fScale = pl_length_vec3(pl_sub_vec3(*ptCenter, *ptCameraPos));
    const float fAxisRadius  = 0.0035f * fScale;
    const float fArrowRadius = 0.0075f * fScale;
    const float fLength = 0.15f * fScale;

    // x axis
    plDrawCylinderDesc tDrawDesc3 = {0};
    gptDraw->fill_cylinder_desc_default(&tDrawDesc3);
    tDrawDesc3.tColor = (plVec4){1.0f, 0.0f, 0.0f, 1.0f};
    tDrawDesc3.tBasePos = *ptCenter;
    tDrawDesc3.tTipPos = (plVec3){ptCenter->x + fLength, ptCenter->y, ptCenter->z};
    tDrawDesc3.fRadius = fAxisRadius;
    gptDraw->add_3d_cylinder_filled_ex(ptDrawlist, &tDrawDesc3);

    // y axis
    plDrawCylinderDesc tDrawDesc4 = {0};
    gptDraw->fill_cylinder_desc_default(&tDrawDesc4);
    tDrawDesc4.tColor = (plVec4){0.0f, 1.0f, 0.0f, 1.0f};
    tDrawDesc4.tBasePos = *ptCenter;
    tDrawDesc4.tTipPos = (plVec3){ptCenter->x, ptCenter->y + fLength, ptCenter->z};
    tDrawDesc4.fRadius = fAxisRadius;
    gptDraw->add_3d_cylinder_filled_ex(ptDrawlist, &tDrawDesc4);

    // z axis
    plDrawCylinderDesc tDrawDesc5 = {0};
    gptDraw->fill_cylinder_desc_default(&tDrawDesc5);
    tDrawDesc5.tColor = (plVec4){0.0f, 0.0f, 1.0f, 1.0f};
    tDrawDesc5.tBasePos = *ptCenter;
    tDrawDesc5.tTipPos = (plVec3){ptCenter->x, ptCenter->y, ptCenter->z + fLength};
    tDrawDesc5.fRadius = fAxisRadius;
    gptDraw->add_3d_cylinder_filled_ex(ptDrawlist, &tDrawDesc5);

    // x end
    gptDraw->add_3d_centered_box_filled(ptDrawlist,
        (plVec3){ptCenter->x + fLength, ptCenter->y, ptCenter->z},
        fAxisRadius * 4,
        fAxisRadius * 4,
        fAxisRadius * 4,
        (plVec4){1.0f, 0.0f, 0.0f, 1.0f});

    // y end
    gptDraw->add_3d_centered_box_filled(ptDrawlist,
        (plVec3){ptCenter->x, ptCenter->y + fLength, ptCenter->z},
        fAxisRadius * 4,
        fAxisRadius * 4,
        fAxisRadius * 4,
        (plVec4){0.0f, 1.0f, 0.0f, 1.0f});

    // z end
    gptDraw->add_3d_centered_box_filled(ptDrawlist,
        (plVec3){ptCenter->x, ptCenter->y, ptCenter->z + fLength},
        fAxisRadius * 4,
        fAxisRadius * 4,
        fAxisRadius * 4,
        (plVec4){0.0f, 0.0f, 1.0f, 1.0f});

    // origin
    gptDraw->add_3d_centered_box_filled(ptDrawlist,
        *ptCenter,
        fAxisRadius * 4,
        fAxisRadius * 4,
        fAxisRadius * 4,
        (plVec4){0.5f, 0.5f, 0.5f, 1.0f});
}

static void
pl_show_ecs_window(plAppData* ptAppData, bool* pbShowWindow)
{
    plComponentLibrary* ptLibrary = gptRenderer->get_component_library(ptAppData->uSceneHandle0);

    if(gptUi->begin_window("Entities", pbShowWindow, false))
    {
        const plVec2 tWindowSize = gptUi->get_window_size();
        const float pfRatios[] = {0.5f, 0.5f};
        gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 2, pfRatios);
        gptUi->text("Entities");
        gptUi->text("Components");
        gptUi->layout_dynamic(0.0f, 1);
        gptUi->separator();
        gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, tWindowSize.y - 75.0f, 2, pfRatios);


        if(gptUi->begin_child("Entities"))
        {
            const float pfRatiosInner[] = {1.0f};
            gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatiosInner);

            const uint32_t uEntityCount = pl_sb_size(ptLibrary->tTagComponentManager.sbtEntities);
            plTagComponent* sbtTags = ptLibrary->tTagComponentManager.pComponents;

            plUiClipper tClipper = {(uint32_t)uEntityCount};
            while(gptUi->step_clipper(&tClipper))
            {
                for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
                {
                    bool bSelected = ptAppData->tSelectedEntity.ulData == ptLibrary->tTagComponentManager.sbtEntities[i].ulData;
                    char atBuffer[1024] = {0};
                    pl_sprintf(atBuffer, "%s, %u", sbtTags[i].acName, ptLibrary->tTagComponentManager.sbtEntities[i].uIndex);
                    if(gptUi->selectable(atBuffer, &bSelected))
                    {
                        if(bSelected)
                        {
                            ptAppData->tSelectedEntity = ptLibrary->tTagComponentManager.sbtEntities[i];
                            if(ptAppData->tSelectedEntity.uIndex != UINT32_MAX)
                                gptRenderer->select_entities(ptAppData->uSceneHandle0, 1, &ptAppData->tSelectedEntity);
                        }
                        else
                        {
                            ptAppData->tSelectedEntity.uIndex = UINT32_MAX;
                            ptAppData->tSelectedEntity.uGeneration = UINT32_MAX;
                            gptRenderer->select_entities(ptAppData->uSceneHandle0, 0, NULL);
                        }
                    }
                }
            }
            
            gptUi->end_child();
        }

        if(gptUi->begin_child("Components"))
        {
            const float pfRatiosInner[] = {1.0f};
            gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatiosInner);

            if(ptAppData->tSelectedEntity.ulData != UINT64_MAX)
            {
                plTagComponent*               ptTagComp           = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, ptAppData->tSelectedEntity);
                plTransformComponent*         ptTransformComp     = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptAppData->tSelectedEntity);
                plMeshComponent*              ptMeshComp          = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_MESH, ptAppData->tSelectedEntity);
                plObjectComponent*            ptObjectComp        = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_OBJECT, ptAppData->tSelectedEntity);
                plHierarchyComponent*         ptHierarchyComp     = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_HIERARCHY, ptAppData->tSelectedEntity);
                plMaterialComponent*          ptMaterialComp      = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_MATERIAL, ptAppData->tSelectedEntity);
                plSkinComponent*              ptSkinComp          = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_SKIN, ptAppData->tSelectedEntity);
                plCameraComponent*            ptCameraComp        = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_CAMERA, ptAppData->tSelectedEntity);
                plAnimationComponent*         ptAnimationComp     = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_ANIMATION, ptAppData->tSelectedEntity);
                plInverseKinematicsComponent* ptIKComp            = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_INVERSE_KINEMATICS, ptAppData->tSelectedEntity);
                plLightComponent*             ptLightComp         = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_LIGHT, ptAppData->tSelectedEntity);

                gptUi->text("Entity: %u, %u", ptAppData->tSelectedEntity.uIndex, ptAppData->tSelectedEntity.uGeneration);

                if(ptTagComp && gptUi->collapsing_header("Tag"))
                {
                    gptUi->text("Name: %s", ptTagComp->acName);
                    gptUi->end_collapsing_header();
                }

                if(ptTransformComp && gptUi->collapsing_header("Transform"))
                {
                    gptUi->text("Scale:       (%+0.3f, %+0.3f, %+0.3f)", ptTransformComp->tScale.x, ptTransformComp->tScale.y, ptTransformComp->tScale.z);
                    gptUi->text("Translation: (%+0.3f, %+0.3f, %+0.3f)", ptTransformComp->tTranslation.x, ptTransformComp->tTranslation.y, ptTransformComp->tTranslation.z);
                    gptUi->text("Rotation:    (%+0.3f, %+0.3f, %+0.3f, %+0.3f)", ptTransformComp->tRotation.x, ptTransformComp->tRotation.y, ptTransformComp->tRotation.z, ptTransformComp->tRotation.w);
                    gptUi->vertical_spacing();
                    gptUi->text("Local World: |%+0.3f, %+0.3f, %+0.3f, %+0.3f|", ptTransformComp->tWorld.col[0].x, ptTransformComp->tWorld.col[1].x, ptTransformComp->tWorld.col[2].x, ptTransformComp->tWorld.col[3].x);
                    gptUi->text("            |%+0.3f, %+0.3f, %+0.3f, %+0.3f|", ptTransformComp->tWorld.col[0].y, ptTransformComp->tWorld.col[1].y, ptTransformComp->tWorld.col[2].y, ptTransformComp->tWorld.col[3].y);
                    gptUi->text("            |%+0.3f, %+0.3f, %+0.3f, %+0.3f|", ptTransformComp->tWorld.col[0].z, ptTransformComp->tWorld.col[1].z, ptTransformComp->tWorld.col[2].z, ptTransformComp->tWorld.col[3].z);
                    gptUi->text("            |%+0.3f, %+0.3f, %+0.3f, %+0.3f|", ptTransformComp->tWorld.col[0].w, ptTransformComp->tWorld.col[1].w, ptTransformComp->tWorld.col[2].w, ptTransformComp->tWorld.col[3].w);
                    gptUi->end_collapsing_header();
                }

                if(ptMeshComp && gptUi->collapsing_header("Mesh"))
                {

                    plTagComponent* ptMaterialTagComp = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, ptMeshComp->tMaterial);
                    plTagComponent* ptSkinTagComp = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, ptMeshComp->tSkinComponent);
                    gptUi->text("Material: %s", ptMaterialTagComp->acName);
                    gptUi->text("Skin:     %s", ptSkinTagComp ? ptSkinTagComp->acName : " ");

                    gptUi->vertical_spacing();
                    gptUi->text("Vertex Data (%u verts, %u idx)", pl_sb_size(ptMeshComp->sbtVertexPositions), pl_sb_size(ptMeshComp->sbuIndices));
                    gptUi->indent(15.0f);
                    gptUi->text("%s Positions", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_POSITION ? "ACTIVE" : "     ");
                    gptUi->text("%s Normals", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_NORMAL ? "ACTIVE" : "     ");
                    gptUi->text("%s Tangents", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TANGENT ? "ACTIVE" : "     ");
                    gptUi->text("%s Texture Coordinates 0", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0 ? "ACTIVE" : "     ");
                    gptUi->text("%s Texture Coordinates 1", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1 ? "ACTIVE" : "     ");
                    gptUi->text("%s Colors 0", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_COLOR_0 ? "ACTIVE" : "     ");
                    gptUi->text("%s Colors 1", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_COLOR_1 ? "ACTIVE" : "     ");
                    gptUi->text("%s Joints 0", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_JOINTS_0 ? "ACTIVE" : "     ");
                    gptUi->text("%s Joints 1", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_JOINTS_1 ? "ACTIVE" : "     ");
                    gptUi->text("%s Weights 0", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0 ? "ACTIVE" : "     ");
                    gptUi->text("%s Weights 1", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1 ? "ACTIVE" : "     ");
                    gptUi->unindent(15.0f);
                    gptUi->end_collapsing_header();
                }

                if(ptObjectComp && gptUi->collapsing_header("Object"))
                {
                    plTagComponent* ptMeshTagComp = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, ptObjectComp->tMesh);
                    plTagComponent* ptTransformTagComp = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, ptObjectComp->tTransform);
                    gptUi->text("Mesh Entity:      %s, %u", ptMeshTagComp->acName, ptObjectComp->tMesh.uIndex);
                    gptUi->text("Transform Entity: %s, %u", ptTransformTagComp->acName, ptObjectComp->tTransform.uIndex);
                    gptUi->end_collapsing_header();
                }

                if(ptHierarchyComp && gptUi->collapsing_header("Hierarchy"))
                {
                    plTagComponent* ptParentTagComp = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, ptHierarchyComp->tParent);
                    gptUi->text("Parent Entity: %s , %u", ptParentTagComp->acName, ptHierarchyComp->tParent.uIndex);
                    gptUi->end_collapsing_header();
                }

                if(ptLightComp && gptUi->collapsing_header("Light"))
                {
                    static const char* apcLightTypes[] = {
                        "PL_LIGHT_TYPE_DIRECTIONAL",
                        "PL_LIGHT_TYPE_POINT"
                    };
                    gptUi->text("Type:        %s", apcLightTypes[ptLightComp->tType]);
                    gptUi->text("Position:    (%0.3f, %0.3f, %0.3f)", ptLightComp->tPosition.r, ptLightComp->tPosition.g, ptLightComp->tPosition.b);
                    gptUi->text("Color:       (%0.3f, %0.3f, %0.3f)", ptLightComp->tColor.r, ptLightComp->tColor.g, ptLightComp->tColor.b);
                    gptUi->text("Direction:   (%0.3f, %0.3f, %0.3f)", ptLightComp->tDirection.r, ptLightComp->tDirection.g, ptLightComp->tDirection.b);
                    gptUi->text("Intensity:   %0.3f", ptLightComp->fIntensity);
                    gptUi->text("Cast Shadow: %s", ptLightComp->tFlags & PL_LIGHT_FLAG_CAST_SHADOW ? "true" : "false");
                }

                if(ptMaterialComp && gptUi->collapsing_header("Material"))
                {
                    gptUi->text("Base Color:            (%0.3f, %0.3f, %0.3f, %0.3f)", ptMaterialComp->tBaseColor.r, ptMaterialComp->tBaseColor.g, ptMaterialComp->tBaseColor.b, ptMaterialComp->tBaseColor.a);
                    gptUi->text("Alpha Cutoff:                    %0.3f", ptMaterialComp->fAlphaCutoff);

                    static const char* apcBlendModeNames[] = 
                    {
                        "PL_MATERIAL_BLEND_MODE_OPAQUE",
                        "PL_MATERIAL_BLEND_MODE_ALPHA",
                        "PL_MATERIAL_BLEND_MODE_PREMULTIPLIED",
                        "PL_MATERIAL_BLEND_MODE_ADDITIVE",
                        "PL_MATERIAL_BLEND_MODE_MULTIPLY"
                    };
                    gptUi->text("Blend Mode:                      %s", apcBlendModeNames[ptMaterialComp->tBlendMode]);

                    static const char* apcShaderNames[] = 
                    {
                        "PL_SHADER_TYPE_PBR",
                        "PL_SHADER_TYPE_UNLIT",
                        "PL_SHADER_TYPE_CUSTOM"
                    };
                    gptUi->text("Shader Type:                     %s", apcShaderNames[ptMaterialComp->tShaderType]);
                    gptUi->text("Double Sided:                    %s", ptMaterialComp->tFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED ? "true" : "false");
  
                    gptUi->vertical_spacing();
                    gptUi->text("Texture Maps");
                    gptUi->indent(15.0f);

                    static const char* apcTextureSlotNames[] = 
                    {
                        "PL_TEXTURE_SLOT_BASE_COLOR_MAP",
                        "PL_TEXTURE_SLOT_NORMAL_MAP",
                        "PL_TEXTURE_SLOT_EMISSIVE_MAP",
                        "PL_TEXTURE_SLOT_OCCLUSION_MAP",
                        "PL_TEXTURE_SLOT_METAL_ROUGHNESS_MAP",
                        "PL_TEXTURE_SLOT_CLEARCOAT_MAP",
                        "PL_TEXTURE_SLOT_CLEARCOAT_ROUGHNESS_MAP",
                        "PL_TEXTURE_SLOT_CLEARCOAT_NORMAL_MAP",
                        "PL_TEXTURE_SLOT_SHEEN_COLOR_MAP",
                        "PL_TEXTURE_SLOT_SHEEN_ROUGHNESS_MAP",
                        "PL_TEXTURE_SLOT_TRANSMISSION_MAP",
                        "PL_TEXTURE_SLOT_SPECULAR_MAP",
                        "PL_TEXTURE_SLOT_SPECULAR_COLOR_MAP",
                        "PL_TEXTURE_SLOT_ANISOTROPY_MAP",
                        "PL_TEXTURE_SLOT_SURFACE_MAP",
                        "PL_TEXTURE_SLOT_IRIDESCENCE_MAP",
                        "PL_TEXTURE_SLOT_IRIDESCENCE_THICKNESS_MAP"
                    };

                    for(uint32_t i = 0; i < PL_TEXTURE_SLOT_COUNT; i++)
                    {
                        gptUi->text("%s: %s", apcTextureSlotNames[i], ptMaterialComp->atTextureMaps[i].acName[0] == 0 ? " " : "present");
                    }
                    gptUi->unindent(15.0f);
                    gptUi->end_collapsing_header();
                }

                if(ptSkinComp && gptUi->collapsing_header("Skin"))
                {
                    if(gptUi->tree_node("Joints"))
                    {
                        for(uint32_t i = 0; i < pl_sb_size(ptSkinComp->sbtJoints); i++)
                        {
                            plTagComponent* ptJointTagComp = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, ptSkinComp->sbtJoints[i]);
                            gptUi->text("%s", ptJointTagComp->acName);  
                        }
                        gptUi->tree_pop();
                    }
                    gptUi->end_collapsing_header();
                }

                if(ptCameraComp && gptUi->collapsing_header("Camera"))
                { 
                    gptUi->text("Near Z:                  %+0.3f", ptCameraComp->fNearZ);
                    gptUi->text("Far Z:                   %+0.3f", ptCameraComp->fFarZ);
                    gptUi->text("Vertical Field of View:  %+0.3f", ptCameraComp->fFieldOfView);
                    gptUi->text("Aspect Ratio:            %+0.3f", ptCameraComp->fAspectRatio);
                    gptUi->text("Pitch:                   %+0.3f", ptCameraComp->fPitch);
                    gptUi->text("Yaw:                     %+0.3f", ptCameraComp->fYaw);
                    gptUi->text("Roll:                    %+0.3f", ptCameraComp->fRoll);
                    gptUi->text("Position: (%+0.3f, %+0.3f, %+0.3f)", ptCameraComp->tPos.x, ptCameraComp->tPos.y, ptCameraComp->tPos.z);
                    gptUi->text("Up:       (%+0.3f, %+0.3f, %+0.3f)", ptCameraComp->_tUpVec.x, ptCameraComp->_tUpVec.y, ptCameraComp->_tUpVec.z);
                    gptUi->text("Forward:  (%+0.3f, %+0.3f, %+0.3f)", ptCameraComp->_tForwardVec.x, ptCameraComp->_tForwardVec.y, ptCameraComp->_tForwardVec.z);
                    gptUi->text("Right:    (%+0.3f, %+0.3f, %+0.3f)", ptCameraComp->_tRightVec.x, ptCameraComp->_tRightVec.y, ptCameraComp->_tRightVec.z);
                    gptUi->slider_float("Far Z Plane", &ptCameraComp->fFarZ, 10.0f, 400.0f);
                    gptUi->end_collapsing_header();
                }

                if(ptAnimationComp && gptUi->collapsing_header("Animation"))
                { 
                    bool bPlaying = ptAnimationComp->tFlags & PL_ANIMATION_FLAG_PLAYING;
                    bool bLooped = ptAnimationComp->tFlags & PL_ANIMATION_FLAG_LOOPED;
                    if(bLooped && bPlaying)
                        gptUi->text("Status: playing & looped");
                    else if(bPlaying)
                        gptUi->text("Status: playing");
                    else if(bLooped)
                        gptUi->text("Status: looped");
                    else
                        gptUi->text("Status: not playing");
                    if(gptUi->checkbox("Playing", &bPlaying))
                    {
                        if(bPlaying)
                            ptAnimationComp->tFlags |= PL_ANIMATION_FLAG_PLAYING;
                        else
                            ptAnimationComp->tFlags &= ~PL_ANIMATION_FLAG_PLAYING;
                    }
                    if(gptUi->checkbox("Looped", &bLooped))
                    {
                        if(bLooped)
                            ptAnimationComp->tFlags |= PL_ANIMATION_FLAG_LOOPED;
                        else
                            ptAnimationComp->tFlags &= ~PL_ANIMATION_FLAG_LOOPED;
                    }
                    gptUi->text("Start: %0.3f s", ptAnimationComp->fStart);
                    gptUi->text("End:   %0.3f s", ptAnimationComp->fEnd);
                    gptUi->progress_bar(ptAnimationComp->fTimer / (ptAnimationComp->fEnd - ptAnimationComp->fStart), (plVec2){-1.0f, 0.0f}, NULL);
                    gptUi->text("Speed:   %0.3f s", ptAnimationComp->fSpeed);
                    gptUi->end_collapsing_header();
                }

                if(ptIKComp && gptUi->collapsing_header("Inverse Kinematics"))
                { 
                    plTagComponent* ptTargetComp = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, ptIKComp->tTarget);
                    gptUi->text("Target Entity: %s , %u", ptTargetComp->acName, ptIKComp->tTarget.uIndex);
                    gptUi->text("Chain Length: %u", ptIKComp->uChainLength);
                    gptUi->text("Iterations: %u", ptIKComp->uIterationCount);
                    gptUi->checkbox("Enabled", &ptIKComp->bEnabled);
                    gptUi->end_collapsing_header();
                }
            }
            
            gptUi->end_child();
        }

        gptUi->end_window();
    }
}