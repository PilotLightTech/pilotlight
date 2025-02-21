/*
   app.c

   Notes:
     * absolute mess
     * mostly a sandbox for now & testing experimental stuff
     * probably better to look at the examples

   Controls:
     * WASD - translation
     * F - fall
     * R - rise
     * center mouse - rotation
     * right click - selection
     * M - switch gizmo mode
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] global apis
// [SECTION] structs
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

// standard
#include <stdlib.h>
#include <stdio.h>
#include <float.h>

// pilot light
#include "pl.h"
#include "pl_memory.h"
#include "pl_string.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"
#include "pl_icons.h"

// stable extensions
#include "pl_image_ext.h"
#include "pl_profile_ext.h"
#include "pl_log_ext.h"
#include "pl_stats_ext.h"
#include "pl_graphics_ext.h"
#include "pl_debug_ext.h"
#include "pl_job_ext.h"
#include "pl_draw_ext.h"
#include "pl_draw_backend_ext.h"
#include "pl_ui_ext.h"
#include "pl_shader_ext.h"
#include "pl_string_intern_ext.h"
#include "pl_network_ext.h"
#include "pl_threads_ext.h"
#include "pl_atomics_ext.h"
#include "pl_window_ext.h"
#include "pl_library_ext.h"
#include "pl_file_ext.h"

// unstable extensions
#include "pl_console_ext.h"
#include "pl_ecs_ext.h"
#include "pl_resource_ext.h"
#include "pl_model_loader_ext.h"
#include "pl_renderer_ext.h"
#include "pl_ecs_tools_ext.h"
#include "pl_gizmo_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] global apis
//-----------------------------------------------------------------------------

const plWindowI*       gptWindows     = NULL;
const plStatsI*        gptStats       = NULL;
const plGraphicsI*     gptGfx         = NULL;
const plDebugApiI*     gptDebug       = NULL;
const plEcsI*          gptEcs         = NULL;
const plCameraI*       gptCamera      = NULL;
const plRendererI*     gptRenderer    = NULL;
const plModelLoaderI*  gptModelLoader = NULL;
const plJobI*          gptJobs        = NULL;
const plDrawI*         gptDraw        = NULL;
const plDrawBackendI*  gptDrawBackend = NULL;
const plUiI*           gptUI          = NULL;
const plIOI*           gptIO          = NULL;
const plShaderI*       gptShader      = NULL;
const plMemoryI*       gptMemory      = NULL;
const plNetworkI*      gptNetwork     = NULL;
const plStringInternI* gptString      = NULL;
const plProfileI*      gptProfile     = NULL;
const plFileI*         gptFile        = NULL;
const plEcsToolsI*     gptEcsTools    = NULL;
const plGizmoI*        gptGizmo       = NULL;
const plConsoleI*      gptConsole     = NULL;

#define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
#define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

#define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
#define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAppData
{

    // windows
    plWindow* ptWindow;

    // swapchains
    bool bResize;

    // ui options
    plDebugApiInfo tDebugInfo;
    bool           bShowUiDebug;
    bool           bShowUiStyle;
    bool           bShowEntityWindow;
    bool           bShowPilotLightTool;

    // scene
    bool     bFreezeCullCamera;
    plEntity tCullCamera;
    plEntity tMainCamera;

    // scenes/views
    uint32_t uSceneHandle0;
    uint32_t uViewHandle0;

    // drawing
    plDrawLayer2D* ptDrawLayer;

    // selection stuff
    plEntity tSelectedEntity;
    
    // fonts
    plFont* tDefaultFont;
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

void
pl__create_scene(plAppData* ptAppData)
{
    plIO* ptIO = gptIO->get_io();
    ptAppData->uSceneHandle0 = gptRenderer->create_scene();

    plComponentLibrary* ptMainComponentLibrary = gptRenderer->get_component_library(ptAppData->uSceneHandle0);

    // create main camera
    plCameraComponent* ptMainCamera = NULL;
    ptAppData->tMainCamera = gptEcs->create_perspective_camera(ptMainComponentLibrary, "main camera", (plVec3){-4.7f, 4.2f, -3.256f}, PL_PI_3, ptIO->tMainViewportSize.x / ptIO->tMainViewportSize.y, 0.1f, 48.0f, true, &ptMainCamera);
    gptCamera->set_pitch_yaw(ptMainCamera, 0.0f, 0.911f);
    gptCamera->update(ptMainCamera);
    gptEcs->attach_script(ptMainComponentLibrary, "pl_script_camera", PL_SCRIPT_FLAG_PLAYING, ptAppData->tMainCamera, NULL);

    // create cull camera
    plCameraComponent* ptCullCamera = NULL;
    ptAppData->tCullCamera = gptEcs->create_perspective_camera(ptMainComponentLibrary, "cull camera", (plVec3){0, 0, 5.0f}, PL_PI_3, ptIO->tMainViewportSize.x / ptIO->tMainViewportSize.y, 0.1f, 25.0f, true, &ptCullCamera);
    gptCamera->set_pitch_yaw(ptCullCamera, 0.0f, PL_PI);
    gptCamera->update(ptCullCamera);

    // create lights
    plLightComponent* ptLight = NULL;
    gptEcs->create_directional_light(ptMainComponentLibrary, "direction light", (plVec3){-0.375f, -1.0f, -0.085f}, &ptLight);
    ptLight->uCascadeCount = 4;
    ptLight->fIntensity = 1.0f;
    ptLight->uShadowResolution = 1024;
    ptLight->afCascadeSplits[0] = 0.10f;
    ptLight->afCascadeSplits[1] = 0.25f;
    ptLight->afCascadeSplits[2] = 0.50f;
    ptLight->afCascadeSplits[3] = 1.00f;
    ptLight->tFlags |= PL_LIGHT_FLAG_CAST_SHADOW | PL_LIGHT_FLAG_VISUALIZER;

    plEntity tPointLight = gptEcs->create_point_light(ptMainComponentLibrary, "point light", (plVec3){0.0f, 2.0f, 2.0f}, &ptLight);
    ptLight->uShadowResolution = 1024;
    ptLight->tFlags |= PL_LIGHT_FLAG_CAST_SHADOW | PL_LIGHT_FLAG_VISUALIZER;
    plTransformComponent* ptPLightTransform = gptEcs->add_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, tPointLight);
    ptPLightTransform->tTranslation = (plVec3){0.0f, 1.497f, 2.0f};

    plEntity tSpotLight = gptEcs->create_spot_light(ptMainComponentLibrary, "spot light", (plVec3){0.0f, 4.0f, -1.18f}, (plVec3){0.0, -1.0f, 0.376f}, &ptLight);
    ptLight->uShadowResolution = 1024;
    ptLight->fRange = 5.0f;
    ptLight->fRadius = 0.025f;
    ptLight->fIntensity = 20.0f;
    ptLight->tFlags |= PL_LIGHT_FLAG_CAST_SHADOW | PL_LIGHT_FLAG_VISUALIZER;
    plTransformComponent* ptSLightTransform = gptEcs->add_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, tSpotLight);
    ptSLightTransform->tTranslation = (plVec3){0.0f, 4.0f, -1.18f};

    plEnvironmentProbeComponent* ptProbe = NULL;
    gptEcs->create_environment_probe(ptMainComponentLibrary, "Main Probe", (plVec3){0.0f, 3.0f, 0.0f}, &ptProbe);
    ptProbe->fRange = 30.0f;
    ptProbe->uResolution = 128;
    ptProbe->tFlags |= PL_ENVIRONMENT_PROBE_FLAGS_INCLUDE_SKY;
}

PL_EXPORT void*
pl_app_load(plApiRegistryI* ptApiRegistry, plAppData* ptAppData)
{
    // NOTE: on first load, "ptAppData" will be NULL but on reloads
    //       it will be the value returned from this function

    // retrieve the data registry API, this is the API used for sharing data
    // between extensions & the runtime
    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    // if "ptAppData" is a valid pointer, then this function is being called
    // during a hot reload.
    if(ptAppData) // reload
    {

        // re-retrieve the apis since we are now in
        // a different dll/so
        gptWindows     = pl_get_api_latest(ptApiRegistry, plWindowI);
        gptStats       = pl_get_api_latest(ptApiRegistry, plStatsI);
        gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);
        gptDebug       = pl_get_api_latest(ptApiRegistry, plDebugApiI);
        gptEcs         = pl_get_api_latest(ptApiRegistry, plEcsI);
        gptCamera      = pl_get_api_latest(ptApiRegistry, plCameraI);
        gptRenderer    = pl_get_api_latest(ptApiRegistry, plRendererI);
        gptJobs        = pl_get_api_latest(ptApiRegistry, plJobI);
        gptModelLoader = pl_get_api_latest(ptApiRegistry, plModelLoaderI);
        gptDraw        = pl_get_api_latest(ptApiRegistry, plDrawI);
        gptDrawBackend = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
        gptUI          = pl_get_api_latest(ptApiRegistry, plUiI);
        gptIO          = pl_get_api_latest(ptApiRegistry, plIOI);
        gptShader      = pl_get_api_latest(ptApiRegistry, plShaderI);
        gptMemory      = pl_get_api_latest(ptApiRegistry, plMemoryI);
        gptNetwork     = pl_get_api_latest(ptApiRegistry, plNetworkI);
        gptString      = pl_get_api_latest(ptApiRegistry, plStringInternI);
        gptProfile     = pl_get_api_latest(ptApiRegistry, plProfileI);
        gptFile        = pl_get_api_latest(ptApiRegistry, plFileI);
        gptEcsTools    = pl_get_api_latest(ptApiRegistry, plEcsToolsI);
        gptGizmo       = pl_get_api_latest(ptApiRegistry, plGizmoI);
        gptConsole     = pl_get_api_latest(ptApiRegistry, plConsoleI);

        return ptAppData;
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~apis & extensions~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // retrieve extension registry
    const plExtensionRegistryI* ptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);

    // load extensions
    ptExtensionRegistry->load("pl_unity_ext", NULL, NULL, true);
    
    // load apis
    gptWindows     = pl_get_api_latest(ptApiRegistry, plWindowI);
    gptStats       = pl_get_api_latest(ptApiRegistry, plStatsI);
    gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptDebug       = pl_get_api_latest(ptApiRegistry, plDebugApiI);
    gptEcs         = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptCamera      = pl_get_api_latest(ptApiRegistry, plCameraI);
    gptRenderer    = pl_get_api_latest(ptApiRegistry, plRendererI);
    gptJobs        = pl_get_api_latest(ptApiRegistry, plJobI);
    gptModelLoader = pl_get_api_latest(ptApiRegistry, plModelLoaderI);
    gptDraw        = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptDrawBackend = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
    gptUI          = pl_get_api_latest(ptApiRegistry, plUiI);
    gptIO          = pl_get_api_latest(ptApiRegistry, plIOI);
    gptShader      = pl_get_api_latest(ptApiRegistry, plShaderI);
    gptMemory      = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptNetwork     = pl_get_api_latest(ptApiRegistry, plNetworkI);
    gptString      = pl_get_api_latest(ptApiRegistry, plStringInternI);
    gptProfile     = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptFile        = pl_get_api_latest(ptApiRegistry, plFileI);
    gptEcsTools    = pl_get_api_latest(ptApiRegistry, plEcsToolsI);
    gptGizmo       = pl_get_api_latest(ptApiRegistry, plGizmoI);
    gptConsole     = pl_get_api_latest(ptApiRegistry, plConsoleI);

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = PL_ALLOC(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    // defaults
    ptAppData->tSelectedEntity.ulData = UINT64_MAX;
    ptAppData->uSceneHandle0 = UINT32_MAX;
    ptAppData->bShowPilotLightTool = true;

    // initialize APIs that require it
    gptEcsTools->initialize();

    // add console variables
    gptConsole->initialize((plConsoleSettings){0});
    gptConsole->add_toggle_option("Pilot Light", &ptAppData->bShowPilotLightTool, "shows main pilot light window");
    gptConsole->add_toggle_option("Entities", &ptAppData->bShowEntityWindow, "shows ecs tool");
    gptConsole->add_toggle_option("Freeze Cull Camera", &ptAppData->bFreezeCullCamera, "freezes culling camera");
    gptConsole->add_toggle_option("Log Tool", &ptAppData->tDebugInfo.bShowLogging, "shows log tool");
    gptConsole->add_toggle_option("Stats Tool", &ptAppData->tDebugInfo.bShowStats, "shows stats tool");
    gptConsole->add_toggle_option("Profiling Tool", &ptAppData->tDebugInfo.bShowProfiling, "shows profiling tool");
    gptConsole->add_toggle_option("Memory Allocation Tool", &ptAppData->tDebugInfo.bShowMemoryAllocations, "shows memory tool");
    gptConsole->add_toggle_option("Device Memory Analyzer Tool", &ptAppData->tDebugInfo.bShowDeviceMemoryAnalyzer, "shows gpu memory tool");
    
    // initialize shader extension
    static plShaderOptions tDefaultShaderOptions = {
        .apcIncludeDirectories = {
            "../shaders/"
        },
        .apcDirectories = {
            "../shaders/"
        },
        .tFlags = PL_SHADER_FLAGS_AUTO_OUTPUT | PL_SHADER_FLAGS_INCLUDE_DEBUG

    };
    gptShader->initialize(&tDefaultShaderOptions);

    // initialize job system
    gptJobs->initialize(0);

    // create window (only 1 allowed currently)
    plWindowDesc tWindowDesc = {
        .pcTitle = "Pilot Light Sandbox",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 600,
        .uHeight = 600,
    };
    gptWindows->create_window(tWindowDesc, &ptAppData->ptWindow);

    // setup reference renderer
    gptRenderer->initialize(ptAppData->ptWindow);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~setup draw extensions~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // initialize
    gptDraw->initialize(NULL);
    gptDrawBackend->initialize(gptRenderer->get_device());

    // create font atlas
    plFontAtlas* ptAtlas = gptDraw->create_font_atlas();
    gptDraw->set_font_atlas(ptAtlas);

    // create fonts
    plFontRange tFontRange = {
        .iFirstCodePoint = 0x0020,
        .uCharCount = 0x00FF - 0x0020
    };

    plFontConfig tFontConfig0 = {
        .bSdf = false,
        .fSize = 16.0f,
        .uHOverSampling = 1,
        .uVOverSampling = 1,
        .ptRanges = &tFontRange,
        .uRangeCount = 1
    };
    ptAppData->tDefaultFont = gptDraw->add_font_from_file_ttf(ptAtlas, tFontConfig0, "../data/pilotlight-assets-master/fonts/Cousine-Regular.ttf");

    const plFontRange tIconRange = {
        .iFirstCodePoint = ICON_MIN_FA,
        .uCharCount = ICON_MAX_16_FA - ICON_MIN_FA
    };

    plFontConfig tFontConfig1 = {
        .bSdf           = false,
        .fSize          = 16.0f,
        .uHOverSampling = 1,
        .uVOverSampling = 1,
        .ptMergeFont    = ptAppData->tDefaultFont,
        .ptRanges       = &tIconRange,
        .uRangeCount    = 1
    };
    gptDraw->add_font_from_file_ttf(ptAtlas, tFontConfig1, "../data/pilotlight-assets-master/fonts/fa-solid-900.otf");

    // build font atlas
    plCommandBuffer* ptCmdBuffer = gptGfx->request_command_buffer(gptRenderer->get_command_pool());
    gptDrawBackend->build_font_atlas(ptCmdBuffer, ptAtlas);
    gptGfx->return_command_buffer(ptCmdBuffer);
    
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ui extension~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    gptUI->initialize();
    gptUI->set_default_font(ptAppData->tDefaultFont);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~app stuff~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // temporary draw layer for submitting fullscreen quad of offscreen render
    ptAppData->ptDrawLayer = gptDraw->request_2d_layer(gptUI->get_draw_list());

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
    gptGfx->flush_device(gptRenderer->get_device());
    gptDrawBackend->cleanup_font_atlas(gptDraw->get_current_font_atlas());
    gptUI->cleanup();
    gptEcsTools->cleanup();
    gptConsole->cleanup();
    gptDrawBackend->cleanup();
    gptRenderer->cleanup();
    gptWindows->destroy_window(ptAppData->ptWindow);
    PL_FREE(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plAppData* ptAppData)
{
    plIO* ptIO = gptIO->get_io();
    if(ptAppData->uSceneHandle0 != UINT32_MAX)
        gptCamera->set_aspect(gptEcs->get_component(gptRenderer->get_component_library(ptAppData->uSceneHandle0), PL_COMPONENT_TYPE_CAMERA, ptAppData->tMainCamera), ptIO->tMainViewportSize.x / ptIO->tMainViewportSize.y);
    ptAppData->bResize = true;
    gptRenderer->resize();
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    gptProfile->begin_frame();
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    gptIO->new_frame();

    // for convience
    plIO* ptIO = gptIO->get_io();

    if(!gptRenderer->begin_frame())
    {
        pl_end_cpu_sample(gptProfile, 0);
        gptProfile->end_frame();
        return;
    }

    if(ptAppData->bResize)
    {
        // gptOS->sleep(32);
        if(ptAppData->uSceneHandle0 != UINT32_MAX)
            gptRenderer->resize_view(ptAppData->uSceneHandle0, ptAppData->uViewHandle0, ptIO->tMainViewportSize);
        ptAppData->bResize = false;
    }

    gptDrawBackend->new_frame();
    gptUI->new_frame();

    // update statistics
    gptStats->new_frame();
    static double* pdFrameTimeCounter = NULL;
    static double* pdMemoryCounter = NULL;
    if(!pdFrameTimeCounter)
        pdFrameTimeCounter = gptStats->get_counter("frametime (ms)");
    if(!pdMemoryCounter)
        pdMemoryCounter = gptStats->get_counter("CPU memory");
    *pdFrameTimeCounter = (double)ptIO->fDeltaTime * 1000.0;
    *pdMemoryCounter = (double)gptMemory->get_memory_usage();

    if(ptAppData->uSceneHandle0 != UINT32_MAX)
    {
        plComponentLibrary* ptMainComponentLibrary = gptRenderer->get_component_library(ptAppData->uSceneHandle0);
        plCameraComponent*  ptCamera = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_CAMERA, ptAppData->tMainCamera);
        plCameraComponent*  ptCullCamera = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_CAMERA, ptAppData->tCullCamera);
        gptCamera->update(ptCullCamera);

        // run ecs system
        gptRenderer->run_ecs(ptAppData->uSceneHandle0);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~selection stuff~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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
            gptGizmo->next_mode();

        if(ptAppData->bShowEntityWindow)
        {
            if(gptEcsTools->show_ecs_window(&ptAppData->tSelectedEntity, ptAppData->uSceneHandle0, &ptAppData->bShowEntityWindow))
            {
                if(ptAppData->tSelectedEntity.ulData == UINT64_MAX)
                    gptRenderer->select_entities(ptAppData->uSceneHandle0, 0, NULL);
                else
                    gptRenderer->select_entities(ptAppData->uSceneHandle0, 1, &ptAppData->tSelectedEntity);
            }
        }

        if(ptAppData->tSelectedEntity.uIndex != UINT32_MAX)
        {
            plDrawList3D* ptGizmoDrawlist =  gptRenderer->get_gizmo_drawlist(ptAppData->uSceneHandle0, ptAppData->uViewHandle0);
            plObjectComponent* ptSelectedObject = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_OBJECT, ptAppData->tSelectedEntity);
            plTransformComponent* ptSelectedTransform = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptAppData->tSelectedEntity);
            plTransformComponent* ptParentTransform = NULL;
            plHierarchyComponent* ptHierarchyComp = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_HIERARCHY, ptAppData->tSelectedEntity);
            if(ptHierarchyComp)
            {
                ptParentTransform = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptHierarchyComp->tParent);
            }
            if(ptSelectedTransform)
            {
                gptGizmo->gizmo(ptGizmoDrawlist, ptCamera, ptSelectedTransform, ptParentTransform);
                ptSelectedTransform->tFlags |= PL_TRANSFORM_FLAGS_DIRTY;
            }
            else if(ptSelectedObject)
            {
                ptSelectedTransform = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptSelectedObject->tTransform);
                gptGizmo->gizmo(ptGizmoDrawlist, ptCamera, ptSelectedTransform, ptParentTransform);
                ptSelectedTransform->tFlags |= PL_TRANSFORM_FLAGS_DIRTY;
            }
        }
    
        // render scene
        const plViewOptions tViewOptions = {
            .ptViewCamera = &ptAppData->tMainCamera,
            .ptCullCamera = ptAppData->bFreezeCullCamera ? &ptAppData->tCullCamera : NULL
        };
        gptRenderer->render_scene(ptAppData->uSceneHandle0, &ptAppData->uViewHandle0, &tViewOptions, 1);
    }

    if(gptIO->is_key_pressed(PL_KEY_F1, false))
    {
        gptConsole->open();
    }

    gptConsole->update();

    // main "editor" debug window
    if(ptAppData->bShowPilotLightTool)
    {
        gptUI->set_next_window_pos((plVec2){0, 0}, PL_UI_COND_ONCE);
        gptUI->set_next_window_size((plVec2){500.0f, 900.0f}, PL_UI_COND_ONCE);
        if(gptUI->begin_window("Pilot Light", NULL, PL_UI_WINDOW_FLAGS_NONE))
        {

            const float pfRatios[] = {1.0f};
            const float pfRatios2[] = {0.5f, 0.5f};
            gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
            if(gptUI->begin_collapsing_header(ICON_FA_CIRCLE_INFO " Information", 0))
            {
                gptUI->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
                gptUI->text("Graphics Backend: %s", gptGfx->get_backend_string());
                gptUI->end_collapsing_header();
            }
            if(gptUI->begin_collapsing_header(ICON_FA_SLIDERS " App Options", 0))
            {
                if(ptAppData->uSceneHandle0 != UINT32_MAX)
                {
                    if(gptUI->checkbox("Freeze Culling Camera", &ptAppData->bFreezeCullCamera))
                    {
                        plComponentLibrary* ptMainComponentLibrary = gptRenderer->get_component_library(ptAppData->uSceneHandle0);
                        plCameraComponent*  ptCamera = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_CAMERA, ptAppData->tMainCamera);
                        plCameraComponent*  ptCullCamera = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_CAMERA, ptAppData->tCullCamera);
                        *ptCullCamera = *ptCamera;
                    }
                }

                gptUI->end_collapsing_header();
            }
            
            gptRenderer->show_graphics_options(ICON_FA_DICE_D6 " Graphics");

            gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 2, pfRatios2);
            if(gptUI->begin_collapsing_header(ICON_FA_SCREWDRIVER_WRENCH " Tools", 0))
            {
                gptUI->checkbox("Device Memory Analyzer", &ptAppData->tDebugInfo.bShowDeviceMemoryAnalyzer);
                gptUI->checkbox("Memory Allocations", &ptAppData->tDebugInfo.bShowMemoryAllocations);
                gptUI->checkbox("Profiling", &ptAppData->tDebugInfo.bShowProfiling);
                gptUI->checkbox("Statistics", &ptAppData->tDebugInfo.bShowStats);
                gptUI->checkbox("Logging", &ptAppData->tDebugInfo.bShowLogging);
                gptUI->checkbox("Entities", &ptAppData->bShowEntityWindow);
                gptUI->end_collapsing_header();
            }
            if(gptUI->begin_collapsing_header(ICON_FA_USER_GEAR " User Interface", 0))
            {
                gptUI->checkbox("UI Debug", &ptAppData->bShowUiDebug);
                gptUI->checkbox("UI Style", &ptAppData->bShowUiStyle);
                gptUI->end_collapsing_header();
            }

            gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);

            if(gptUI->begin_collapsing_header(ICON_FA_PHOTO_FILM " Renderer", 0))
            {

                gptUI->vertical_spacing();

                const float pfWidths[] = {150.0f, 150.0f};
                gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 2, pfWidths);

                bool bLoadScene = false;

                if(ptAppData->uSceneHandle0 != UINT32_MAX)
                {
                    if(gptUI->button("Unload Scene"))
                    {
                        gptRenderer->cleanup_scene(ptAppData->uSceneHandle0);
                        ptAppData->uSceneHandle0 = UINT32_MAX;
                    }
                }
                else
                {
                    if(gptUI->button("Load Scene"))
                    {
                        bLoadScene = true;
                    }
                }

                if(gptUI->button("Reload Shaders"))
                {
                    gptRenderer->reload_scene_shaders(ptAppData->uSceneHandle0);
                }

                if(ptAppData->uSceneHandle0 == UINT32_MAX)
                {

                    static uint32_t uComboSelect = 1;
                    static const char* apcEnvMaps[] = {
                        "none",
                        "helipad",
                        "chromatic",
                        "directional",
                        "doge2",
                        "ennis",
                        "field",
                        "footprint_court",
                        "neutral",
                        "papermill",
                        "pisa",
                    };
                    bool abCombo[11] = {0};
                    abCombo[uComboSelect] = true;
                    gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
                    if(gptUI->begin_combo("Environment", apcEnvMaps[uComboSelect], PL_UI_COMBO_FLAGS_NONE))
                    {
                        for(uint32_t i = 0; i < 10; i++)
                        {
                            if(gptUI->selectable(apcEnvMaps[i], &abCombo[i], 0))
                            {
                                uComboSelect = i;
                                gptUI->close_current_popup();
                            }
                        }
                        gptUI->end_combo();
                    }

                    static bool abModels[] = {
                        false,
                        false,
                        false,
                        false,
                        true,
                        true,
                        false,
                        false,
                    };

                    static const char* apcModels[] = {
                        "Sponza",
                        "DamagedHelmet",
                        "NormalTangentTest",
                        "NormalTangentMirrorTest",
                        "Humanoid",
                        "Floor",
                        "Environment Test",
                        "Test",
                    };

                    static const char* apcModelPaths[] = {
                        "../data/glTF-Sample-Assets-main/Models/Sponza/glTF/Sponza.gltf",
                        "../data/glTF-Sample-Assets-main/Models/DamagedHelmet/glTF/DamagedHelmet.gltf",
                        "../data/glTF-Sample-Assets-main/Models/NormalTangentTest/glTF/NormalTangentTest.gltf",
                        "../data/glTF-Sample-Assets-main/Models/NormalTangentMirrorTest/glTF/NormalTangentMirrorTest.gltf",
                        "../data/pilotlight-assets-master/models/gltf/humanoid/model.gltf",
                        "../data/pilotlight-assets-master/models/gltf/humanoid/floor.gltf",
                        "../data/glTF-Sample-Assets-main/Models/EnvironmentTest/glTF/EnvironmentTest.gltf",
                        "../data/testing/testing.gltf",
                    };

                    gptUI->separator_text("Test Models");
                    for(uint32_t i = 0; i < 7; i++)
                        gptUI->selectable(apcModels[i], &abModels[i], 0);


                    gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfWidths);

                    if(bLoadScene)
                    {

                        pl__create_scene(ptAppData);
                        
                        if(uComboSelect > 0)
                        {
                            char* sbcData = NULL;
                            pl_sb_sprintf(sbcData, "../data/pilotlight-assets-master/environments/%s.hdr", apcEnvMaps[uComboSelect]);
                            gptRenderer->load_skybox_from_panorama(ptAppData->uSceneHandle0, sbcData, 1024);
                            pl_sb_free(sbcData);
                        }

                        ptAppData->uViewHandle0 = gptRenderer->create_view(ptAppData->uSceneHandle0, ptIO->tMainViewportSize);

                        plModelLoaderData tLoaderData0 = {0};

                        for(uint32_t i = 0; i < 7; i++)
                        {
                            if(abModels[i])
                            {
                                plComponentLibrary* ptMainComponentLibrary = gptRenderer->get_component_library(ptAppData->uSceneHandle0);
                                gptModelLoader->load_gltf(ptMainComponentLibrary, apcModelPaths[i], NULL, &tLoaderData0);
                            }
                        }

                        gptRenderer->add_drawable_objects_to_scene(ptAppData->uSceneHandle0, tLoaderData0.uDeferredCount, tLoaderData0.atDeferredObjects, tLoaderData0.uForwardCount, tLoaderData0.atForwardObjects);
                        gptModelLoader->free_data(&tLoaderData0);

                        gptRenderer->finalize_scene(ptAppData->uSceneHandle0);
                    }

                }
                gptUI->end_collapsing_header();
            }
            gptUI->end_window();
        }
    }

    gptDebug->show_debug_windows(&ptAppData->tDebugInfo);
        
    if(ptAppData->bShowUiStyle)
        gptUI->show_style_editor_window(&ptAppData->bShowUiStyle);

    if(ptAppData->bShowUiDebug)
        gptUI->show_debug_window(&ptAppData->bShowUiDebug);

    // add full screen quad for offscreen render
    if(ptAppData->uSceneHandle0 != UINT32_MAX)
        gptDraw->add_image(ptAppData->ptDrawLayer, gptRenderer->get_view_color_texture(ptAppData->uSceneHandle0, ptAppData->uViewHandle0).uData, (plVec2){0}, ptIO->tMainViewportSize);

    gptDraw->submit_2d_layer(ptAppData->ptDrawLayer);

    gptRenderer->end_frame();

    pl_end_cpu_sample(gptProfile, 0);
    gptProfile->end_frame();
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#ifdef PL_USE_STB_SPRINTF
    #define STB_SPRINTF_IMPLEMENTATION
    #include "stb_sprintf.h"
    #undef STB_SPRINTF_IMPLEMENTATION
#endif

#define PL_STRING_IMPLEMENTATION
#include "pl_string.h"