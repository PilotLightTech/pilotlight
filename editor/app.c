/*
   app.c

   Notes:
     * absolute mess
     * mostly a sandbox for now & testing experimental stuff
     * probably better to look at the examples
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] global apis
// [SECTION] structs
// [SECTION] helper forward declarations
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
// [SECTION] helper implementations
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
#include "pl_json.h"

// stable extensions
#include "pl_image_ext.h"
#include "pl_profile_ext.h"
#include "pl_log_ext.h"
#include "pl_stats_ext.h"
#include "pl_graphics_ext.h"
#include "pl_tools_ext.h"
#include "pl_job_ext.h"
#include "pl_draw_ext.h"
#include "pl_draw_backend_ext.h"
#include "pl_ui_ext.h"
#include "pl_shader_ext.h"
#include "pl_string_intern_ext.h"
#include "pl_platform_ext.h"
#include "pl_console_ext.h"
#include "pl_screen_log_ext.h"
#include "pl_starter_ext.h"

// unstable extensions
#include "pl_ecs_ext.h"
#include "pl_config_ext.h"
#include "pl_resource_ext.h"
#include "pl_model_loader_ext.h"
#include "pl_renderer_ext.h"
#include "pl_ecs_tools_ext.h"
#include "pl_gizmo_ext.h"
#include "pl_physics_ext.h"
#include "pl_collision_ext.h"
#include "pl_bvh_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] global apis
//-----------------------------------------------------------------------------

const plWindowI*       gptWindows     = NULL;
const plStatsI*        gptStats       = NULL;
const plGraphicsI*     gptGfx         = NULL;
const plToolsI*        gptTools       = NULL;
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
const plScreenLogI*    gptScreenLog   = NULL;
const plPhysicsI *     gptPhysics     = NULL;
const plCollisionI*    gptCollision   = NULL;
const plBVHI*          gptBvh         = NULL;
const plConfigI*       gptConfig      = NULL;
const plStarterI*      gptStarter     = NULL;

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

    // graphics
    plDevice*    ptDevice;
    plSwapchain* ptSwap;
    plSurface*   ptSurface;

    // swapchains
    bool bResize;

    // ui options
    bool  bEditorAttached;
    bool  bShowEntityWindow;
    bool  bShowPilotLightTool;
    bool* pbShowDeviceMemoryAnalyzer;
    bool* pbShowMemoryAllocations;
    bool* pbShowProfiling;
    bool* pbShowStats;
    bool* pbShowLogging;

    // scene
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

    // physics
    bool bPhysicsDebugDraw;

    // misc
    char* sbcTempBuffer;
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] helper forward declarations
//-----------------------------------------------------------------------------

void pl__show_editor_window(plAppData*);

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

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
        gptTools       = pl_get_api_latest(ptApiRegistry, plToolsI);
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
        gptScreenLog   = pl_get_api_latest(ptApiRegistry, plScreenLogI);
        gptPhysics     = pl_get_api_latest(ptApiRegistry, plPhysicsI);
        gptCollision   = pl_get_api_latest(ptApiRegistry, plCollisionI);
        gptBvh         = pl_get_api_latest(ptApiRegistry, plBVHI);
        gptConfig      = pl_get_api_latest(ptApiRegistry, plConfigI);
        gptStarter     = pl_get_api_latest(ptApiRegistry, plStarterI);

        gptScreenLog->add_message_ex(0, 15.0, PL_COLOR_32_MAGENTA, 1.5f, "%s", "App Hot Reloaded");

        return ptAppData;
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~apis & extensions~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // retrieve extension registry
    const plExtensionRegistryI* ptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);

    // load extensions
    ptExtensionRegistry->load("pl_unity_ext", NULL, NULL, true);
    ptExtensionRegistry->load("pl_platform_ext", NULL, NULL, false);

    // load apis
    gptWindows     = pl_get_api_latest(ptApiRegistry, plWindowI);
    gptStats       = pl_get_api_latest(ptApiRegistry, plStatsI);
    gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptTools       = pl_get_api_latest(ptApiRegistry, plToolsI);
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
    gptScreenLog   = pl_get_api_latest(ptApiRegistry, plScreenLogI);
    gptPhysics     = pl_get_api_latest(ptApiRegistry, plPhysicsI);
    gptCollision   = pl_get_api_latest(ptApiRegistry, plCollisionI);
    gptBvh         = pl_get_api_latest(ptApiRegistry, plBVHI);
    gptConfig      = pl_get_api_latest(ptApiRegistry, plConfigI);
    gptStarter     = pl_get_api_latest(ptApiRegistry, plStarterI);

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = (plAppData*)PL_ALLOC(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    // defaults
    ptAppData->tSelectedEntity.ulData = UINT64_MAX;
    ptAppData->uSceneHandle0 = UINT32_MAX;
    ptAppData->bShowPilotLightTool = true;

    gptConfig->load_from_disk(NULL);
    ptAppData->bEditorAttached = gptConfig->load_bool("bEditorAttached", true);
    ptAppData->bShowEntityWindow = gptConfig->load_bool("bShowEntityWindow", false);
    ptAppData->bPhysicsDebugDraw = gptConfig->load_bool("bPhysicsDebugDraw", false);

    // add console variables
    gptConsole->initialize((plConsoleSettings){.tFlags = PL_CONSOLE_FLAGS_POPUP});
    gptConsole->add_toggle_variable("a.PilotLight", &ptAppData->bShowPilotLightTool, "shows main pilot light window", PL_CONSOLE_VARIABLE_FLAGS_CLOSE_CONSOLE);
    gptConsole->add_toggle_variable("a.Entities", &ptAppData->bShowEntityWindow, "shows ecs tool", PL_CONSOLE_VARIABLE_FLAGS_CLOSE_CONSOLE);

    // initialize APIs that require it
    gptEcsTools->initialize();
    gptPhysics->initialize((plPhysicsEngineSettings){0});
    
    // initialize shader compiler
    static plShaderOptions tDefaultShaderOptions = {
        .apcIncludeDirectories = {
            "../shaders/"
        },
        .apcDirectories = {
            "../shaders/"
        },
        .tFlags = PL_SHADER_FLAGS_AUTO_OUTPUT | PL_SHADER_FLAGS_INCLUDE_DEBUG | PL_SHADER_FLAGS_ALWAYS_COMPILE

    };
    gptShader->initialize(&tDefaultShaderOptions);

    // initialize job system
    gptJobs->initialize(0);

    // create window (only 1 allowed currently)
    plWindowDesc tWindowDesc = {
        .pcTitle = "Pilot Light Sandbox",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 500,
        .uHeight = 500,
    };
    gptWindows->create(tWindowDesc, &ptAppData->ptWindow);
    gptWindows->show(ptAppData->ptWindow);

    // initialize graphics
    plGraphicsInit tGraphicsDesc = {
        .tFlags = PL_GRAPHICS_INIT_FLAGS_SWAPCHAIN_ENABLED | PL_GRAPHICS_INIT_FLAGS_VALIDATION_ENABLED
    };
    gptGfx->initialize(&tGraphicsDesc);

    ptAppData->ptSurface = gptGfx->create_surface(ptAppData->ptWindow);

    ptAppData->ptDevice = gptStarter->create_device(ptAppData->ptSurface);

    // create swapchain
    const plSwapchainInit tSwapInit = {
        .bVSync = true,
        .tSampleCount = 1
    };
    ptAppData->ptSwap = gptGfx->create_swapchain(ptAppData->ptDevice, ptAppData->ptSurface, &tSwapInit);

    // setup reference renderer
    plRendererSettings tRenderSettings = {
        .ptDevice = ptAppData->ptDevice,
        .ptSwap = ptAppData->ptSwap,
        .uMaxTextureResolution = 1024,
    };
    gptRenderer->initialize(tRenderSettings);

    gptTools->initialize((plToolsInit){.ptDevice = gptRenderer->get_device()});

    // retrieve some console variables
    ptAppData->pbShowLogging              = (bool*)gptConsole->get_variable("t.LogTool", NULL, NULL);
    ptAppData->pbShowStats                = (bool*)gptConsole->get_variable("t.StatTool", NULL, NULL);
    ptAppData->pbShowProfiling            = (bool*)gptConsole->get_variable("t.ProfileTool", NULL, NULL);
    ptAppData->pbShowMemoryAllocations    = (bool*)gptConsole->get_variable("t.MemoryAllocationTool", NULL, NULL);
    ptAppData->pbShowDeviceMemoryAnalyzer = (bool*)gptConsole->get_variable("t.DeviceMemoryAnalyzerTool", NULL, NULL);

    *ptAppData->pbShowLogging = gptConfig->load_bool("pbShowLogging", *ptAppData->pbShowLogging);
    *ptAppData->pbShowStats = gptConfig->load_bool("pbShowStats", *ptAppData->pbShowStats);
    *ptAppData->pbShowProfiling = gptConfig->load_bool("pbShowProfiling", *ptAppData->pbShowProfiling);
    *ptAppData->pbShowMemoryAllocations = gptConfig->load_bool("pbShowMemoryAllocations", *ptAppData->pbShowMemoryAllocations);
    *ptAppData->pbShowDeviceMemoryAnalyzer = gptConfig->load_bool("pbShowDeviceMemoryAnalyzer", *ptAppData->pbShowDeviceMemoryAnalyzer);

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
    
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~message extension~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    gptScreenLog->initialize((plScreenLogSettings){.ptFont = ptAppData->tDefaultFont});

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ui extension~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    gptUI->initialize();
    gptUI->set_default_font(ptAppData->tDefaultFont);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~app stuff~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // temporary draw layer for submitting fullscreen quad of offscreen render
    ptAppData->ptDrawLayer = gptDraw->request_2d_layer(gptUI->get_draw_list());

    plIO* ptIO = gptIO->get_io();
    ptAppData->uSceneHandle0 = gptRenderer->create_scene();
    ptAppData->uViewHandle0 = gptRenderer->create_view(ptAppData->uSceneHandle0, ptIO->tMainViewportSize);

    plComponentLibrary* ptMainComponentLibrary = gptRenderer->get_component_library(ptAppData->uSceneHandle0);

    // create main camera
    plCameraComponent* ptMainCamera = NULL;
    ptAppData->tMainCamera = gptEcs->create_perspective_camera(ptMainComponentLibrary, "main camera", pl_create_vec3(-4.7f, 4.2f, -3.256f), PL_PI_3, ptIO->tMainViewportSize.x / ptIO->tMainViewportSize.y, 0.1f, 48.0f, true, &ptMainCamera);
    gptCamera->set_pitch_yaw(ptMainCamera, 0.0f, 0.911f);
    gptCamera->update(ptMainCamera);
    gptEcs->attach_script(ptMainComponentLibrary, "pl_script_camera", PL_SCRIPT_FLAG_PLAYING | PL_SCRIPT_FLAG_RELOADABLE, ptAppData->tMainCamera, NULL);

    // create lights
    plLightComponent* ptLight = NULL;
    gptEcs->create_directional_light(ptMainComponentLibrary, "direction light", pl_create_vec3(-0.375f, -1.0f, -0.085f), &ptLight);
    ptLight->uCascadeCount = 4;
    ptLight->fIntensity = 1.0f;
    ptLight->uShadowResolution = 1024;
    ptLight->afCascadeSplits[0] = 0.10f;
    ptLight->afCascadeSplits[1] = 0.25f;
    ptLight->afCascadeSplits[2] = 0.50f;
    ptLight->afCascadeSplits[3] = 1.00f;
    ptLight->tFlags |= PL_LIGHT_FLAG_CAST_SHADOW | PL_LIGHT_FLAG_VISUALIZER;

    plEntity tPointLight = gptEcs->create_point_light(ptMainComponentLibrary, "point light", pl_create_vec3(0.0f, 2.0f, 2.0f), &ptLight);
    ptLight->uShadowResolution = 1024;
    ptLight->tFlags |= PL_LIGHT_FLAG_CAST_SHADOW | PL_LIGHT_FLAG_VISUALIZER;
    plTransformComponent* ptPLightTransform = (plTransformComponent* )gptEcs->add_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, tPointLight);
    ptPLightTransform->tTranslation = pl_create_vec3(0.0f, 1.497f, 2.0f);

    plEntity tSpotLight = gptEcs->create_spot_light(ptMainComponentLibrary, "spot light", pl_create_vec3(0.0f, 4.0f, -1.18f), pl_create_vec3(0.0, -1.0f, 0.376f), &ptLight);
    ptLight->uShadowResolution = 1024;
    ptLight->fRange = 5.0f;
    ptLight->fRadius = 0.025f;
    ptLight->fIntensity = 20.0f;
    ptLight->tFlags |= PL_LIGHT_FLAG_CAST_SHADOW | PL_LIGHT_FLAG_VISUALIZER;
    plTransformComponent* ptSLightTransform = (plTransformComponent* )gptEcs->add_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, tSpotLight);
    ptSLightTransform->tTranslation = pl_create_vec3(0.0f, 4.0f, -1.18f);

    plEnvironmentProbeComponent* ptProbe = NULL;
    gptEcs->create_environment_probe(ptMainComponentLibrary, "Main Probe", pl_create_vec3(0.0f, 3.0f, 0.0f), &ptProbe);
    ptProbe->fRange = 30.0f;
    ptProbe->uResolution = 128;
    ptProbe->tFlags |= PL_ENVIRONMENT_PROBE_FLAGS_INCLUDE_SKY;

    gptRenderer->load_skybox_from_panorama(ptAppData->uSceneHandle0, "../data/pilotlight-assets-master/environments/helipad.hdr", 1024);

    plModelLoaderData tLoaderData0 = {0};
    gptModelLoader->load_gltf(ptMainComponentLibrary, "../data/pilotlight-assets-master/models/gltf/humanoid/model.gltf", NULL, &tLoaderData0);
    gptModelLoader->load_gltf(ptMainComponentLibrary, "../data/pilotlight-assets-master/models/gltf/humanoid/floor.gltf", NULL, &tLoaderData0);
    gptRenderer->add_drawable_objects_to_scene(ptAppData->uSceneHandle0, tLoaderData0.uObjectCount, tLoaderData0.atObjects);
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
    pl_sb_free(ptAppData->sbcTempBuffer);

    // ensure GPU is finished before cleanup
    gptGfx->flush_device(gptRenderer->get_device());

    gptConfig->set_bool("bEditorAttached", ptAppData->bEditorAttached);
    gptConfig->set_bool("bShowEntityWindow", ptAppData->bShowEntityWindow);
    gptConfig->set_bool("bPhysicsDebugDraw", ptAppData->bPhysicsDebugDraw);
    gptConfig->set_bool("pbShowLogging", *ptAppData->pbShowLogging);
    gptConfig->set_bool("pbShowStats", *ptAppData->pbShowStats);
    gptConfig->set_bool("pbShowProfiling", *ptAppData->pbShowProfiling);
    gptConfig->set_bool("pbShowMemoryAllocations", *ptAppData->pbShowMemoryAllocations);
    gptConfig->set_bool("pbShowDeviceMemoryAnalyzer", *ptAppData->pbShowDeviceMemoryAnalyzer);

    gptConfig->save_to_disk(NULL);
    gptConfig->cleanup();

    gptDrawBackend->cleanup_font_atlas(gptDraw->get_current_font_atlas());
    gptUI->cleanup();
    gptEcsTools->cleanup();
    gptPhysics->cleanup();
    gptShader->cleanup();
    gptConsole->cleanup();
    gptScreenLog->cleanup();
    gptDrawBackend->cleanup();
    gptRenderer->cleanup();
    
    gptGfx->cleanup_swapchain(ptAppData->ptSwap);
    gptGfx->cleanup_surface(ptAppData->ptSurface);
    gptGfx->cleanup_device(ptAppData->ptDevice);
    gptGfx->cleanup();
    gptWindows->destroy(ptAppData->ptWindow);
    PL_FREE(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plWindow* ptWindow, plAppData* ptAppData)
{
    plIO* ptIO = gptIO->get_io();
    if(ptAppData->uSceneHandle0 != UINT32_MAX)
        gptCamera->set_aspect((plCameraComponent*)gptEcs->get_component(gptRenderer->get_component_library(ptAppData->uSceneHandle0), PL_COMPONENT_TYPE_CAMERA, ptAppData->tMainCamera), ptIO->tMainViewportSize.x / ptIO->tMainViewportSize.y);
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


    plComponentLibrary* ptMainComponentLibrary = gptRenderer->get_component_library(ptAppData->uSceneHandle0);
    plCameraComponent*  ptCamera = (plCameraComponent*)gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_CAMERA, ptAppData->tMainCamera);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~selection stuff~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    plVec2 tMousePos = gptIO->get_mouse_pos();

    if(!gptUI->wants_mouse_capture() && !gptGizmo->active())
    {
        static plVec2 tClickPos = {0};
        if(gptIO->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
        {
            tClickPos = tMousePos;
        }
        else if(gptIO->is_mouse_released(PL_MOUSE_BUTTON_LEFT))
        {
            plVec2 tReleasePos = tMousePos;
            plVec2 tHoverOffset = {0};
            plVec2 tHoverScale = {1.0f, 1.0f};

            if(tReleasePos.x == tClickPos.x && tReleasePos.y == tClickPos.y)
                gptRenderer->update_hovered_entity(ptAppData->uSceneHandle0, ptAppData->uViewHandle0, tHoverOffset, tHoverScale);
        }
    }

    // run ecs system
    gptRenderer->run_ecs(ptAppData->uSceneHandle0);

    plEntity tNextEntity = {0};
    if(gptRenderer->get_hovered_entity(ptAppData->uSceneHandle0, ptAppData->uViewHandle0, &tNextEntity))
    {
        
        if(tNextEntity.ulData == 0)
        {
            ptAppData->tSelectedEntity.ulData = UINT64_MAX;
            gptRenderer->outline_entities(ptAppData->uSceneHandle0, 0, NULL);
        }
        else if(ptAppData->tSelectedEntity.ulData != tNextEntity.ulData)
        {
            gptScreenLog->add_message_ex(565168477883, 5.0, PL_COLOR_32_RED, 1.0f, "Selected Entity {%u, %u}", tNextEntity.uIndex, tNextEntity.uGeneration);
            gptRenderer->outline_entities(ptAppData->uSceneHandle0, 1, &tNextEntity);
            ptAppData->tSelectedEntity = tNextEntity;
            gptPhysics->set_angular_velocity(gptRenderer->get_component_library(ptAppData->uSceneHandle0), tNextEntity, pl_create_vec3(0, 0, 0));
            gptPhysics->set_linear_velocity(gptRenderer->get_component_library(ptAppData->uSceneHandle0), tNextEntity, pl_create_vec3(0, 0, 0));
        }

    }

    if(gptIO->is_key_pressed(PL_KEY_M, true))
        gptGizmo->next_mode();

    if(ptAppData->bShowEntityWindow)
    {
        if(gptEcsTools->show_ecs_window(&ptAppData->tSelectedEntity, ptAppData->uSceneHandle0, &ptAppData->bShowEntityWindow))
        {
            if(ptAppData->tSelectedEntity.ulData == UINT64_MAX)
            {
                gptRenderer->outline_entities(ptAppData->uSceneHandle0, 0, NULL);
            }
            else
            {
                gptRenderer->outline_entities(ptAppData->uSceneHandle0, 1, &ptAppData->tSelectedEntity);
            }
        }
    }

    if(ptAppData->tSelectedEntity.uIndex != UINT32_MAX)
    {
        plDrawList3D* ptGizmoDrawlist =  gptRenderer->get_gizmo_drawlist(ptAppData->uSceneHandle0, ptAppData->uViewHandle0);
        plObjectComponent* ptSelectedObject = (plObjectComponent*)gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_OBJECT, ptAppData->tSelectedEntity);
        plTransformComponent* ptSelectedTransform = (plTransformComponent*)gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptAppData->tSelectedEntity);
        plTransformComponent* ptParentTransform = NULL;
        plHierarchyComponent* ptHierarchyComp = (plHierarchyComponent*)gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_HIERARCHY, ptAppData->tSelectedEntity);
        if(ptHierarchyComp)
        {
            ptParentTransform = (plTransformComponent*)gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptHierarchyComp->tParent);
        }
        if(ptSelectedTransform)
        {
            gptGizmo->gizmo(ptGizmoDrawlist, ptCamera, ptSelectedTransform, ptParentTransform, (plVec2){0}, (plVec2){1.0f, 1.0f});
        }
        else if(ptSelectedObject)
        {
            ptSelectedTransform = (plTransformComponent*)gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptSelectedObject->tTransform);
            gptGizmo->gizmo(ptGizmoDrawlist, ptCamera, ptSelectedTransform, ptParentTransform, (plVec2){0}, (plVec2){1.0f, 1.0f});
        }
    }

    if(ptAppData->bPhysicsDebugDraw)
    {
        plDrawList3D* ptDrawlist = gptRenderer->get_debug_drawlist(ptAppData->uSceneHandle0, ptAppData->uViewHandle0);
        gptPhysics->draw(ptMainComponentLibrary, ptDrawlist);
    }

    // render scene
    plViewOptions tViewOptions = PL_ZERO_INIT;
    tViewOptions.ptViewCamera = &ptAppData->tMainCamera;
    tViewOptions.ptCullCamera = NULL;
    gptRenderer->render_scene(ptAppData->uSceneHandle0, &ptAppData->uViewHandle0, &tViewOptions, 1);

    if(gptIO->is_key_pressed(PL_KEY_F1, false))
    {
        gptConsole->open();
    }

    gptConsole->update();

    // main "editor" debug window
    if(ptAppData->bShowPilotLightTool)
        pl__show_editor_window(ptAppData);

    gptTools->update();

    // add full screen quad for offscreen render
    if(ptAppData->uSceneHandle0 != UINT32_MAX)
    {
        plVec2 tStartPos = {0};
        plVec2 tEndPos = ptIO->tMainViewportSize;
        gptDraw->add_image(ptAppData->ptDrawLayer,
            gptRenderer->get_view_color_texture(ptAppData->uSceneHandle0, ptAppData->uViewHandle0).uData,
            tStartPos,
            tEndPos);
    }

    gptDraw->submit_2d_layer(ptAppData->ptDrawLayer);

    plRenderEncoder* ptRenderEncoder = NULL;
    plCommandBuffer* ptCommandBuffer = NULL;
    gptRenderer->begin_final_pass(&ptRenderEncoder, &ptCommandBuffer);

    // render ui
    pl_begin_cpu_sample(gptProfile, 0, "render ui");
    
    gptUI->end_frame();

    float fWidth = ptIO->tMainViewportSize.x;
    float fHeight = ptIO->tMainViewportSize.y;
    gptDrawBackend->submit_2d_drawlist(gptUI->get_draw_list(), ptRenderEncoder, fWidth, fHeight, gptGfx->get_swapchain_info(gptRenderer->get_swapchain()).tSampleCount);
    gptDrawBackend->submit_2d_drawlist(gptUI->get_debug_draw_list(), ptRenderEncoder, fWidth, fHeight, gptGfx->get_swapchain_info(gptRenderer->get_swapchain()).tSampleCount);
    pl_end_cpu_sample(gptProfile, 0);

    plDrawList2D* ptMessageDrawlist = gptScreenLog->get_drawlist(fWidth, fHeight);
    gptDrawBackend->submit_2d_drawlist(ptMessageDrawlist, ptRenderEncoder, fWidth, fHeight, gptGfx->get_swapchain_info(gptRenderer->get_swapchain()).tSampleCount);


    gptRenderer->end_final_pass(ptRenderEncoder, ptCommandBuffer);

    pl_end_cpu_sample(gptProfile, 0);
    gptProfile->end_frame();
}

//-----------------------------------------------------------------------------
// [SECTION] helper implementations
//-----------------------------------------------------------------------------

void
pl__show_editor_window(plAppData* ptAppData)
{
    plIO* ptIO = gptIO->get_io();

    plUiWindowFlags tWindowFlags = PL_UI_WINDOW_FLAGS_NONE;

    if(ptAppData->bEditorAttached)
    {
        tWindowFlags = PL_UI_WINDOW_FLAGS_NO_TITLE_BAR | PL_UI_WINDOW_FLAGS_NO_RESIZE | PL_UI_WINDOW_FLAGS_HORIZONTAL_SCROLLBAR;
        gptUI->set_next_window_pos(pl_create_vec2(0, 0), PL_UI_COND_ALWAYS);
        gptUI->set_next_window_size(pl_create_vec2(600.0f, ptIO->tMainViewportSize.y), PL_UI_COND_ALWAYS);
    }

    if(gptUI->begin_window("Pilot Light", NULL, tWindowFlags))
    {
        gptUI->vertical_spacing();
        // gptUI->vertical_spacing();
        // gptUI->vertical_spacing();

        const float pfRatios[] = {1.0f};
        const float pfRatios2[] = {0.5f, 0.5f};
        gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);

        if(gptUI->begin_collapsing_header(ICON_FA_CIRCLE_INFO " Information", 0))
        {
            gptUI->text("Pilot Light %s", PILOT_LIGHT_CORE_VERSION_STRING);
            gptUI->text("Graphics Backend: %s", gptGfx->get_backend_string());

            gptUI->layout_static(0.0f, 200.0f, 1);
            if(gptUI->button("Show Camera Controls"))
            {
                const char* acMouseInfo = "Camera Controls\n"
                "_______________\n"
                "LMB + Drag: Moves camera forward & backward and rotates left & right.\n\n"
                "RMB + Drag: Rotates camera.\n\n"
                "LMB + RMB + Drag: Pans Camera\n\n"
                "Game style (when holding RMB)\n"
                "_____________________________\n"
                "W    Moves the camera forward.\n"
                "S    Moves the camera backward.\n"
                "A    Moves the camera left.\n"
                "D    Moves the camera right.\n"
                "E    Moves the camera up.\n"
                "Q    Moves the camera down.\n"
                "Z    Zooms the camera out (raises FOV).\n"
                "C    Zooms the camera in (lowers FOV).\n";
                gptScreenLog->add_message_ex(651984984, 45.0, PL_COLOR_32_GREEN, 1.5f, acMouseInfo);
            }
            gptUI->end_collapsing_header();
        }
        if(gptUI->begin_collapsing_header(ICON_FA_SLIDERS " App Options", 0))
        {
            gptUI->checkbox("Editor Attached", &ptAppData->bEditorAttached);

            gptUI->vertical_spacing();

            const float pfWidths[] = {150.0f, 150.0f};
            gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 2, pfWidths);

            gptUI->end_collapsing_header();
        }
        
        if(gptUI->begin_collapsing_header(ICON_FA_DICE_D6 " Graphics", 0))
        {
            plRendererRuntimeOptions* ptRuntimeOptions = gptRenderer->get_runtime_options();
            if(gptUI->checkbox("VSync", &ptRuntimeOptions->bVSync))
                ptRuntimeOptions->bReloadSwapchain = true;
            gptUI->checkbox("Show Origin", &ptRuntimeOptions->bShowOrigin);
            gptUI->checkbox("Show BVH", &ptRuntimeOptions->bShowBVH);
            bool bReloadShaders = false;
            if(gptUI->checkbox("Wireframe", &ptRuntimeOptions->bWireframe)) bReloadShaders = true;
            if(gptUI->checkbox("MultiViewport Shadows", &ptRuntimeOptions->bMultiViewportShadows)) bReloadShaders = true;
            if(gptUI->checkbox("Image Based Lighting", &ptRuntimeOptions->bImageBasedLighting)) bReloadShaders = true;
            if(gptUI->checkbox("Punctual Lighting", &ptRuntimeOptions->bPunctualLighting)) bReloadShaders = true;
            gptUI->checkbox("Show Probes", &ptRuntimeOptions->bShowProbes);
            if(gptUI->checkbox("UI MSAA", &ptRuntimeOptions->bMSAA))
            {
                ptRuntimeOptions->bReloadSwapchain = true;
            }

            if(bReloadShaders)
            {
                gptRenderer->reload_scene_shaders(ptAppData->uSceneHandle0);
            }
            gptUI->checkbox("Frustum Culling", &ptRuntimeOptions->bFrustumCulling);
            gptUI->checkbox("All Bounding Boxes", &ptRuntimeOptions->bDrawAllBoundingBoxes);
            gptUI->checkbox("Visible Bounding Boxes", &ptRuntimeOptions->bDrawVisibleBoundingBoxes);
            gptUI->checkbox("Selected Bounding Box", &ptRuntimeOptions->bShowSelectedBoundingBox);
            
            gptUI->input_float("Depth Bias", &ptRuntimeOptions->fShadowConstantDepthBias, NULL, 0);
            gptUI->input_float("Slope Depth Bias", &ptRuntimeOptions->fShadowSlopeDepthBias, NULL, 0);
            gptUI->slider_uint("Outline Width", &ptRuntimeOptions->uOutlineWidth, 2, 50, 0);
            

            if(ptAppData->uSceneHandle0 != UINT32_MAX)
            {
                if(gptUI->tree_node("Scene", 0))
                {
                    plSceneRuntimeOptions* ptSceneRuntimeOptions = gptRenderer->get_scene_runtime_options(ptAppData->uSceneHandle0);
                    gptUI->checkbox("Show Skybox", &ptSceneRuntimeOptions->bShowSkybox);
                    gptUI->checkbox("Dynamic BVH", &ptSceneRuntimeOptions->bContinuousBVH);
                    if(gptUI->button("Build BVH") || ptSceneRuntimeOptions->bContinuousBVH)
                        gptRenderer->rebuild_scene_bvh(ptAppData->uSceneHandle0);
                    gptUI->tree_pop();
                }
            }
            gptUI->end_collapsing_header();
        }

        if(gptUI->begin_collapsing_header(ICON_FA_BOXES_STACKED " Physics", 0))
        {
            plPhysicsEngineSettings tPhysicsSettings = gptPhysics->get_settings();

            gptUI->checkbox("Enabled", &tPhysicsSettings.bEnabled);
            gptUI->checkbox("Debug Draw", &ptAppData->bPhysicsDebugDraw);
            gptUI->slider_float("Simulation Speed", &tPhysicsSettings.fSimulationMultiplier, 0.01f, 3.0f, 0);
            gptUI->input_float("Sleep Epsilon", &tPhysicsSettings.fSleepEpsilon, "%g", 0);
            gptUI->input_float("Position Epsilon", &tPhysicsSettings.fPositionEpsilon, "%g", 0);
            gptUI->input_float("Velocity Epsilon", &tPhysicsSettings.fVelocityEpsilon, "%g", 0);
            gptUI->input_uint("Max Position Its.", &tPhysicsSettings.uMaxPositionIterations, 0);
            gptUI->input_uint("Max Velocity Its.", &tPhysicsSettings.uMaxVelocityIterations, 0);
            gptUI->input_float("Frame Rate", &tPhysicsSettings.fSimulationFrameRate, "%g", 0);
            if(gptUI->button("Wake All")) gptPhysics->wake_up_all();
            if(gptUI->button("Sleep All")) gptPhysics->sleep_all();

            gptPhysics->set_settings(tPhysicsSettings);

            gptUI->end_collapsing_header();
        }

        gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 2, pfRatios2);
        if(gptUI->begin_collapsing_header(ICON_FA_SCREWDRIVER_WRENCH " Tools", 0))
        {
            gptUI->checkbox("Device Memory", ptAppData->pbShowDeviceMemoryAnalyzer);
            gptUI->checkbox("Memory Allocations", ptAppData->pbShowMemoryAllocations);
            gptUI->checkbox("Profiling", ptAppData->pbShowProfiling);
            gptUI->checkbox("Statistics", ptAppData->pbShowStats);
            gptUI->checkbox("Logging", ptAppData->pbShowLogging);
            gptUI->checkbox("Entities", &ptAppData->bShowEntityWindow);
            gptUI->end_collapsing_header();
        }
        if(gptUI->begin_collapsing_header(ICON_FA_USER_GEAR " User Interface", 0))
        {
            gptUI->end_collapsing_header();
        }

        gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);

        if(gptUI->begin_collapsing_header(ICON_FA_PHOTO_FILM " Renderer", 0))
        {

            if(gptUI->button("Reload Shaders"))
            {
                gptRenderer->reload_scene_shaders(ptAppData->uSceneHandle0);
            }
            gptUI->end_collapsing_header();
        }
        gptUI->end_window();
    }
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