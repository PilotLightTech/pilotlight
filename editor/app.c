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
#include "pl_pak_ext.h"
#include "pl_datetime_ext.h"

// unstable extensions
#include "pl_ecs_ext.h"
#include "pl_mesh_ext.h"
#include "pl_camera_ext.h"
#include "pl_animation_ext.h"
#include "pl_config_ext.h"
#include "pl_resource_ext.h"
#include "pl_model_loader_ext.h"
#include "pl_renderer_ext.h"
#include "pl_ecs_tools_ext.h"
#include "pl_gizmo_ext.h"
#include "pl_physics_ext.h"
#include "pl_collision_ext.h"
#include "pl_bvh_ext.h"
#include "pl_shader_variant_ext.h"
#include "pl_vfs_ext.h"
#include "pl_compress_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] global apis
//-----------------------------------------------------------------------------

const plWindowI*            gptWindows       = NULL;
const plStatsI*             gptStats         = NULL;
const plGraphicsI*          gptGfx           = NULL;
const plToolsI*             gptTools         = NULL;
const plEcsI*               gptEcs           = NULL;
const plCameraI*            gptCamera        = NULL;
const plRendererI*          gptRenderer      = NULL;
const plModelLoaderI*       gptModelLoader   = NULL;
const plJobI*               gptJobs          = NULL;
const plDrawI*              gptDraw          = NULL;
const plDrawBackendI*       gptDrawBackend   = NULL;
const plUiI*                gptUI            = NULL;
const plIOI*                gptIO            = NULL;
const plShaderI*            gptShader        = NULL;
const plMemoryI*            gptMemory        = NULL;
const plNetworkI*           gptNetwork       = NULL;
const plStringInternI*      gptString        = NULL;
const plProfileI*           gptProfile       = NULL;
const plFileI*              gptFile          = NULL;
const plEcsToolsI*          gptEcsTools      = NULL;
const plGizmoI*             gptGizmo         = NULL;
const plConsoleI*           gptConsole       = NULL;
const plScreenLogI*         gptScreenLog     = NULL;
const plPhysicsI *          gptPhysics       = NULL;
const plCollisionI*         gptCollision     = NULL;
const plBVHI*               gptBvh           = NULL;
const plConfigI*            gptConfig        = NULL;
const plResourceI*          gptResource      = NULL;
const plStarterI*           gptStarter       = NULL;
const plAnimationI*         gptAnimation     = NULL;
const plMeshI*              gptMesh          = NULL;
const plShaderVariantI*     gptShaderVariant = NULL;
const plVfsI*               gptVfs           = NULL;
const plPakI*               gptPak           = NULL;
const plDateTimeI*          gptDateTime      = NULL;
const plCompressI*          gptCompress      = NULL;

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
    plDevice* ptDevice;
    bool      bMSAA;
    bool      bVSync;

    // swapchains
    bool bResize;

    // ui options
    bool  bContinuousBVH;
    bool  bFrustumCulling;
    bool  bShowSkybox;
    bool  bShowBVH;
    bool  bEditorAttached;
    bool  bShowEntityWindow;
    bool  bShowPilotLightTool;
    bool  bShowDebugLights;
    bool  bDrawAllBoundingBoxes;
    bool* pbShowDeviceMemoryAnalyzer;
    bool* pbShowMemoryAllocations;
    bool* pbShowProfiling;
    bool* pbShowStats;
    bool* pbShowLogging;

    // scene
    plEntity tMainCamera;

    // scenes/views
    plScene* ptScene;
    plView*  ptView;

    // drawing
    plDrawLayer2D* ptDrawLayer;

    // selection stuff
    plEntity tSelectedEntity;

    // physics
    bool bPhysicsDebugDraw;

    // misc
    char* sbcTempBuffer;
    plComponentLibrary* ptComponentLibrary;
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] helper forward declarations
//-----------------------------------------------------------------------------

void pl__show_editor_window(plAppData*);
void pl__load_apis(plApiRegistryI*);

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
        pl__load_apis(ptApiRegistry);

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
    pl__load_apis(ptApiRegistry);

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = (plAppData*)PL_ALLOC(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    gptVfs->mount_directory("/models", "../data/pilotlight-assets-master/models", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/gltf", "../data/glTF-Sample-Assets-main/Models", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/fonts", "../data/pilotlight-assets-master/fonts", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/environments", "../data/pilotlight-assets-master/environments", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/shaders", "../shaders", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/shader-temp", "../shader-temp", PL_VFS_MOUNT_FLAGS_NONE);
    gptFile->create_directory("../shader-temp");

    // defaults
    ptAppData->tSelectedEntity.uData = UINT64_MAX;
    ptAppData->bShowPilotLightTool = true;
    ptAppData->bShowSkybox = true;
    ptAppData->bFrustumCulling = true;
    ptAppData->bVSync = true;
    ptAppData->bShowDebugLights = true;

    gptConfig->load_from_disk(NULL);
    ptAppData->bEditorAttached = gptConfig->load_bool("bEditorAttached", true);
    ptAppData->bShowEntityWindow = gptConfig->load_bool("bShowEntityWindow", false);
    ptAppData->bPhysicsDebugDraw = gptConfig->load_bool("bPhysicsDebugDraw", false);

    // initialize APIs that require it
    gptEcsTools->initialize();
    gptPhysics->initialize((plPhysicsEngineSettings){0});
    
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

    plStarterInit tStarterInit = {
        .tFlags   = PL_STARTER_FLAGS_NONE,
        .ptWindow = ptAppData->ptWindow
    };

    // extensions handled by starter
    tStarterInit.tFlags |= PL_STARTER_FLAGS_GRAPHICS_EXT;
    tStarterInit.tFlags |= PL_STARTER_FLAGS_PROFILE_EXT;
    tStarterInit.tFlags |= PL_STARTER_FLAGS_STATS_EXT;
    tStarterInit.tFlags |= PL_STARTER_FLAGS_CONSOLE_EXT;
    tStarterInit.tFlags |= PL_STARTER_FLAGS_TOOLS_EXT;
    tStarterInit.tFlags |= PL_STARTER_FLAGS_DRAW_EXT;
    tStarterInit.tFlags |= PL_STARTER_FLAGS_UI_EXT;
    tStarterInit.tFlags |= PL_STARTER_FLAGS_SCREEN_LOG_EXT;

    // initial flags
    tStarterInit.tFlags |= PL_STARTER_FLAGS_DEPTH_BUFFER;

    // we handle these
    // tStarterInit.tFlags |= PL_STARTER_FLAGS_SHADER_EXT;

    // from a graphics standpoint, the starter extension is handling device, swapchain, renderpass
    // etc. which we will get to in later examples
    gptStarter->initialize(tStarterInit);

    // initialize shader compiler
    static plShaderOptions tDefaultShaderOptions = {
        .apcIncludeDirectories = {
            "/shaders/"
        },
        .apcDirectories = {
            "/shaders/",
            "/shader-temp/",
        },
        .pcCacheOutputDirectory = "/shader-temp/",
        .tFlags = PL_SHADER_FLAGS_AUTO_OUTPUT | PL_SHADER_FLAGS_INCLUDE_DEBUG | PL_SHADER_FLAGS_ALWAYS_COMPILE
    };
    gptShader->initialize(&tDefaultShaderOptions);

    ptAppData->ptDevice = gptStarter->get_device();

    // initialize job system
    gptJobs->initialize((plJobSystemInit){0});

    const plShaderVariantInit tShaderVariantInit = {
        .ptDevice = ptAppData->ptDevice
    };
    gptShaderVariant->initialize(tShaderVariantInit);

    // setup reference renderer
    plRendererSettings tRenderSettings = {
        .ptDevice = ptAppData->ptDevice,
        .ptSwap = gptStarter->get_swapchain(),
        .uMaxTextureResolution = 1024,
    };
    gptRenderer->initialize(tRenderSettings);

    // set some console variable
    gptConsole->add_toggle_variable("a.PilotLight", &ptAppData->bShowPilotLightTool, "shows main pilot light window", PL_CONSOLE_VARIABLE_FLAGS_CLOSE_CONSOLE);
    gptConsole->add_toggle_variable("a.Entities", &ptAppData->bShowEntityWindow, "shows ecs tool", PL_CONSOLE_VARIABLE_FLAGS_CLOSE_CONSOLE);

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
    plFont* ptDefaultFont = gptDraw->add_font_from_file_ttf(gptDraw->get_current_font_atlas(), tFontConfig0, "/fonts/Cousine-Regular.ttf");

    const plFontRange tIconRange = {
        .iFirstCodePoint = ICON_MIN_FA,
        .uCharCount = ICON_MAX_16_FA - ICON_MIN_FA
    };

    plFontConfig tFontConfig1 = {
        .bSdf           = false,
        .fSize          = 16.0f,
        .uHOverSampling = 1,
        .uVOverSampling = 1,
        .ptMergeFont    = ptDefaultFont,
        .ptRanges       = &tIconRange,
        .uRangeCount    = 1
    };
    gptDraw->add_font_from_file_ttf(gptDraw->get_current_font_atlas(), tFontConfig1, "/fonts/fa-solid-900.otf");
    gptStarter->set_default_font(ptDefaultFont);
    gptUI->set_default_font(ptDefaultFont);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~app stuff~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    gptEcs->initialize((plEcsInit){0});
    gptRenderer->register_ecs_system();
    gptCamera->register_ecs_system();
    gptAnimation->register_ecs_system();
    gptMesh->register_ecs_system();
    gptPhysics->register_ecs_system();
    gptEcs->finalize();
    ptAppData->ptComponentLibrary = gptEcs->get_default_library();

    plIO* ptIO = gptIO->get_io();
    plSceneInit tSceneInit = {
        .ptComponentLibrary = ptAppData->ptComponentLibrary
    };
    ptAppData->ptScene = gptRenderer->create_scene(tSceneInit);
    ptAppData->ptView = gptRenderer->create_view(ptAppData->ptScene, ptIO->tMainViewportSize);

    // create main camera
    plCamera* ptMainCamera = NULL;
    ptAppData->tMainCamera = gptCamera->create_perspective(ptAppData->ptComponentLibrary, "main camera", pl_create_vec3(-4.012f, 2.984f, -1.109f), PL_PI_3, ptIO->tMainViewportSize.x / ptIO->tMainViewportSize.y, 0.1f, 48.0f, true, &ptMainCamera);
    gptCamera->set_pitch_yaw(ptMainCamera, -0.465f, 1.341f);
    gptCamera->update(ptMainCamera);
    gptEcs->attach_script(ptAppData->ptComponentLibrary, "pl_script_camera", PL_SCRIPT_FLAG_PLAYING | PL_SCRIPT_FLAG_RELOADABLE, ptAppData->tMainCamera, NULL);

    // create lights
    plLightComponent* ptLight = NULL;
    gptRenderer->create_directional_light(ptAppData->ptComponentLibrary, "direction light", pl_create_vec3(0.0f, -1.0f, -0.085f), &ptLight);
    ptLight->uCascadeCount = 4;
    ptLight->fIntensity = 1.0f;
    ptLight->uShadowResolution = 1024;
    ptLight->afCascadeSplits[0] = 0.05f;
    ptLight->afCascadeSplits[1] = 0.15f;
    ptLight->afCascadeSplits[2] = 0.50f;
    ptLight->afCascadeSplits[3] = 1.00f;
    ptLight->tFlags |= PL_LIGHT_FLAG_CAST_SHADOW | PL_LIGHT_FLAG_VISUALIZER;

    plEntity tPointLight = gptRenderer->create_point_light(ptAppData->ptComponentLibrary, "point light", pl_create_vec3(0.0f, 2.0f, 2.0f), &ptLight);
    ptLight->uShadowResolution = 1024;
    ptLight->tFlags |= PL_LIGHT_FLAG_CAST_SHADOW | PL_LIGHT_FLAG_VISUALIZER;
    plTransformComponent* ptPLightTransform = (plTransformComponent* )gptEcs->add_component(ptAppData->ptComponentLibrary, gptEcs->get_ecs_type_key_transform(), tPointLight);
    ptPLightTransform->tTranslation = pl_create_vec3(9.316f, 1.497f, -1.042f);

    plEntity tSpotLight = gptRenderer->create_spot_light(ptAppData->ptComponentLibrary, "spot light", pl_create_vec3(0.0f, 4.0f, -1.18f), pl_create_vec3(0.0, -0.390f, 0.368f), &ptLight);
    ptLight->uShadowResolution = 1024;
    ptLight->fRange = 10.0f;
    ptLight->fRadius = 0.025f;
    ptLight->fIntensity = 20.0f;
    ptLight->tFlags |= PL_LIGHT_FLAG_CAST_SHADOW | PL_LIGHT_FLAG_VISUALIZER;
    plTransformComponent* ptSLightTransform = (plTransformComponent* )gptEcs->add_component(ptAppData->ptComponentLibrary, gptEcs->get_ecs_type_key_transform(), tSpotLight);
    ptSLightTransform->tTranslation = pl_create_vec3(0.0f, 4.0f, -1.18f);

    plEnvironmentProbeComponent* ptProbe = NULL;
    gptRenderer->create_environment_probe(ptAppData->ptComponentLibrary, "Main Probe", pl_create_vec3(0.0f, 3.0f, 0.0f), &ptProbe);
    ptProbe->fRange = 30.0f;
    ptProbe->uResolution = 128;
    ptProbe->tFlags |= PL_ENVIRONMENT_PROBE_FLAGS_INCLUDE_SKY;

    gptRenderer->load_skybox_from_panorama(ptAppData->ptScene, "/environments/helipad.hdr", 1024);

    plModelLoaderData tLoaderData0 = {0};
    gptModelLoader->load_gltf(ptAppData->ptComponentLibrary, "/models/gltf/humanoid/model.gltf", NULL, &tLoaderData0);
    gptModelLoader->load_gltf(ptAppData->ptComponentLibrary, "/gltf/Sponza/glTF/Sponza.gltf", NULL, &tLoaderData0);
    // gptModelLoader->load_gltf(ptAppData->ptComponentLibrary, "/gltf/DamagedHelmet/glTF/DamagedHelmet.gltf", NULL, &tLoaderData0);
    gptRenderer->add_drawable_objects_to_scene(ptAppData->ptScene, tLoaderData0.uObjectCount, tLoaderData0.atObjects);
    gptModelLoader->free_data(&tLoaderData0);
    gptRenderer->finalize_scene(ptAppData->ptScene);

    // give starter extension chance to do its work now
    gptStarter->finalize();

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
    pl_sb_free(ptAppData->sbcTempBuffer);

    // ensure GPU is finished before cleanup
    gptGfx->flush_device(ptAppData->ptDevice);

    // plPakFile* ptPak = NULL;
    // gptPak->begin_packing("../data/shaders.pak", 4, &ptPak);
    // PL_ASSERT(gptPak->add_from_disk(ptPak, "deferred_lighting.frag.spv", "../out-temp/deferred_lighting.frag.spv",false));
    // PL_ASSERT(gptPak->add_from_disk(ptPak, "deferred_lighting.vert.spv", "../out-temp/deferred_lighting.vert.spv",false));
    // // PL_ASSERT(gptPak->add_from_disk(ptPak, "draw_2d_sdf.frag.spv", "../out-temp/draw_2d_sdf.vert.spv",false));
    // PL_ASSERT(gptPak->add_from_disk(ptPak, "draw_2d.frag.spv", "../out-temp/draw_2d.frag.spv",false));
    // PL_ASSERT(gptPak->add_from_disk(ptPak, "draw_2d.vert.spv", "../out-temp/draw_2d.vert.spv",false));
    // PL_ASSERT(gptPak->add_from_disk(ptPak, "draw_3d_line.vert.spv", "../out-temp/draw_3d_line.vert.spv",false));
    // PL_ASSERT(gptPak->add_from_disk(ptPak, "draw_3d.frag.spv", "../out-temp/draw_3d.frag.spv",false));
    // PL_ASSERT(gptPak->add_from_disk(ptPak, "draw_3d.vert.spv", "../out-temp/draw_3d.vert.spv",false));
    // PL_ASSERT(gptPak->add_from_disk(ptPak, "filter_environment.comp.spv", "../out-temp/filter_environment.comp.spv",false));
    // PL_ASSERT(gptPak->add_from_disk(ptPak, "forward.frag.spv", "../out-temp/forward.frag.spv",false));
    // PL_ASSERT(gptPak->add_from_disk(ptPak, "forward.vert.spv", "../out-temp/forward.vert.spv",false));
    // PL_ASSERT(gptPak->add_from_disk(ptPak, "full_quad.vert.spv", "../out-temp/full_quad.vert.spv",false));
    // PL_ASSERT(gptPak->add_from_disk(ptPak, "gbuffer_fill.frag.spv", "../out-temp/gbuffer_fill.frag.spv",false));
    // PL_ASSERT(gptPak->add_from_disk(ptPak, "gbuffer_fill.vert.spv", "../out-temp/gbuffer_fill.vert.spv",false));
    // PL_ASSERT(gptPak->add_from_disk(ptPak, "jumpfloodalgo.comp.spv", "../out-temp/jumpfloodalgo.comp.spv",false));
    // PL_ASSERT(gptPak->add_from_disk(ptPak, "panorama_to_cubemap.comp.spv", "../out-temp/panorama_to_cubemap.comp.spv",false));
    // PL_ASSERT(gptPak->add_from_disk(ptPak, "picking.frag.spv", "../out-temp/picking.frag.spv",false));
    // PL_ASSERT(gptPak->add_from_disk(ptPak, "picking.vert.spv", "../out-temp/picking.vert.spv",false));
    // PL_ASSERT(gptPak->add_from_disk(ptPak, "shadow.frag.spv", "../out-temp/shadow.frag.spv",false));
    // PL_ASSERT(gptPak->add_from_disk(ptPak, "shadow.vert.spv", "../out-temp/shadow.vert.spv",false));
    // PL_ASSERT(gptPak->add_from_disk(ptPak, "skinning.comp.spv", "../out-temp/skinning.comp.spv",false));
    // PL_ASSERT(gptPak->add_from_disk(ptPak, "skybox.frag.spv", "../out-temp/skybox.frag.spv",false));
    // PL_ASSERT(gptPak->add_from_disk(ptPak, "skybox.vert.spv", "../out-temp/skybox.vert.spv",false));
    // PL_ASSERT(gptPak->add_from_disk(ptPak, "tonemap.frag.spv", "../out-temp/tonemap.frag.spv",false));
    // PL_ASSERT(gptPak->add_from_disk(ptPak, "uvmap.frag.spv", "../out-temp/uvmap.frag.spv",false));
    // PL_ASSERT(gptPak->add_from_disk(ptPak, "uvmap.vert.spv", "../out-temp/uvmap.vert.spv",false));
    // gptPak->end_packing(&ptPak);

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
    gptEcsTools->cleanup();
    gptPhysics->cleanup();
    gptShader->cleanup();
    gptConsole->cleanup();

    gptRenderer->cleanup_view(ptAppData->ptView);
    gptRenderer->cleanup_scene(ptAppData->ptScene);
    
    gptEcs->cleanup();
    gptRenderer->cleanup();
    gptShaderVariant->cleanup();
    gptStarter->cleanup();
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
    if(ptAppData->ptScene)
        gptCamera->set_aspect((plCamera*)gptEcs->get_component(ptAppData->ptComponentLibrary, gptCamera->get_ecs_type_key(), ptAppData->tMainCamera), ptIO->tMainViewportSize.x / ptIO->tMainViewportSize.y);
    ptAppData->bResize = true;
    gptStarter->resize();
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    if(!gptStarter->begin_frame())
        return;

    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    gptResource->new_frame();
    
    // for convience
    plIO* ptIO = gptIO->get_io();

    gptRenderer->begin_frame();

    if(ptAppData->bResize)
    {
        // gptOS->sleep(32);
        if(ptAppData->ptScene)
            gptRenderer->resize_view(ptAppData->ptView, ptIO->tMainViewportSize);
        ptAppData->bResize = false;
    }

    // update statistics
    gptShaderVariant->update_stats();

    plCamera* ptCamera = (plCamera*)gptEcs->get_component(ptAppData->ptComponentLibrary, gptCamera->get_ecs_type_key(), ptAppData->tMainCamera);

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
                gptRenderer->update_hovered_entity(ptAppData->ptView, tHoverOffset, tHoverScale);
        }
    }

    // run ecs system
    pl_begin_cpu_sample(gptProfile, 0, "Run ECS");
    gptEcs->run_script_update_system(ptAppData->ptComponentLibrary);
    gptAnimation->run_animation_update_system(ptAppData->ptComponentLibrary, ptIO->fDeltaTime);
    gptPhysics->update(ptIO->fDeltaTime, ptAppData->ptComponentLibrary);
    gptEcs->run_transform_update_system(ptAppData->ptComponentLibrary);
    gptEcs->run_hierarchy_update_system(ptAppData->ptComponentLibrary);
    gptRenderer->run_light_update_system(ptAppData->ptComponentLibrary);
    gptCamera->run_ecs(ptAppData->ptComponentLibrary);
    gptAnimation->run_inverse_kinematics_update_system(ptAppData->ptComponentLibrary);
    gptRenderer->run_skin_update_system(ptAppData->ptComponentLibrary);
    gptRenderer->run_object_update_system(ptAppData->ptComponentLibrary);
    gptRenderer->run_environment_probe_update_system(ptAppData->ptComponentLibrary); // run after object update
    pl_end_cpu_sample(gptProfile, 0);

    plEntity tNextEntity = {0};
    if(gptRenderer->get_hovered_entity(ptAppData->ptView, &tNextEntity))
    {
        
        if(tNextEntity.uData == 0)
        {
            ptAppData->tSelectedEntity.uData = UINT64_MAX;
            gptRenderer->outline_entities(ptAppData->ptScene, 0, NULL);
        }
        else if(ptAppData->tSelectedEntity.uData != tNextEntity.uData)
        {
            gptScreenLog->add_message_ex(565168477883, 5.0, PL_COLOR_32_RED, 1.0f, "Selected Entity {%u, %u}", tNextEntity.uIndex, tNextEntity.uGeneration);
            gptRenderer->outline_entities(ptAppData->ptScene, 1, &tNextEntity);
            ptAppData->tSelectedEntity = tNextEntity;
            gptPhysics->set_angular_velocity(ptAppData->ptComponentLibrary, tNextEntity, pl_create_vec3(0, 0, 0));
            gptPhysics->set_linear_velocity(ptAppData->ptComponentLibrary, tNextEntity, pl_create_vec3(0, 0, 0));
        }

    }

    if(gptIO->is_key_pressed(PL_KEY_M, true))
        gptGizmo->next_mode();

    if(ptAppData->bShowEntityWindow)
    {
        if(gptEcsTools->show_ecs_window(ptAppData->ptComponentLibrary, &ptAppData->tSelectedEntity, ptAppData->ptScene, &ptAppData->bShowEntityWindow))
        {
            if(ptAppData->tSelectedEntity.uData == UINT64_MAX)
            {
                gptRenderer->outline_entities(ptAppData->ptScene, 0, NULL);
            }
            else
            {
                gptRenderer->outline_entities(ptAppData->ptScene, 1, &ptAppData->tSelectedEntity);
            }
        }
    }

    if(ptAppData->tSelectedEntity.uIndex != UINT32_MAX)
    {
        plDrawList3D* ptGizmoDrawlist =  gptRenderer->get_gizmo_drawlist(ptAppData->ptView);
        plObjectComponent* ptSelectedObject = (plObjectComponent*)gptEcs->get_component(ptAppData->ptComponentLibrary, gptRenderer->get_ecs_type_key_object(), ptAppData->tSelectedEntity);
        plTransformComponent* ptSelectedTransform = (plTransformComponent*)gptEcs->get_component(ptAppData->ptComponentLibrary, gptEcs->get_ecs_type_key_transform(), ptAppData->tSelectedEntity);
        plTransformComponent* ptParentTransform = NULL;
        plHierarchyComponent* ptHierarchyComp = (plHierarchyComponent*)gptEcs->get_component(ptAppData->ptComponentLibrary, gptEcs->get_ecs_type_key_hierarchy(), ptAppData->tSelectedEntity);
        if(ptHierarchyComp)
        {
            ptParentTransform = (plTransformComponent*)gptEcs->get_component(ptAppData->ptComponentLibrary, gptEcs->get_ecs_type_key_transform(), ptHierarchyComp->tParent);
        }
        if(ptSelectedTransform)
        {
            gptGizmo->gizmo(ptGizmoDrawlist, ptCamera, ptSelectedTransform, ptParentTransform, (plVec2){0}, (plVec2){1.0f, 1.0f});
        }
        else if(ptSelectedObject)
        {
            ptSelectedTransform = (plTransformComponent*)gptEcs->get_component(ptAppData->ptComponentLibrary, gptEcs->get_ecs_type_key_transform(), ptSelectedObject->tTransform);
            gptGizmo->gizmo(ptGizmoDrawlist, ptCamera, ptSelectedTransform, ptParentTransform, (plVec2){0}, (plVec2){1.0f, 1.0f});
        }
    }

    if(ptAppData->bPhysicsDebugDraw)
    {
        plDrawList3D* ptDrawlist = gptRenderer->get_debug_drawlist(ptAppData->ptView);
        gptPhysics->draw(ptAppData->ptComponentLibrary, ptDrawlist);
    }

    // debug rendering
    if(ptAppData->bShowDebugLights)
    {
        plLightComponent* ptLights = NULL;
        const uint32_t uLightCount = gptEcs->get_components(ptAppData->ptComponentLibrary, gptRenderer->get_ecs_type_key_light(), (void**)&ptLights, NULL);
        gptRenderer->debug_draw_lights(ptAppData->ptView, ptLights, uLightCount);
    }

    if(ptAppData->bDrawAllBoundingBoxes)
        gptRenderer->debug_draw_all_bound_boxes(ptAppData->ptView);

    if(ptAppData->bShowSkybox)
        gptRenderer->show_skybox(ptAppData->ptView);

    if(ptAppData->bShowBVH)
        gptRenderer->debug_draw_bvh(ptAppData->ptView);

    // render scene
    gptRenderer->prepare_scene(ptAppData->ptScene);
    gptRenderer->prepare_view(ptAppData->ptView, ptCamera);
    gptRenderer->render_view(ptAppData->ptView, ptCamera, ptAppData->bFrustumCulling ? ptCamera : NULL);

    // main "editor" debug window
    if(ptAppData->bShowPilotLightTool)
        pl__show_editor_window(ptAppData);

    // add full screen quad for offscreen render
    if(ptAppData->ptScene)
    {
        plVec2 tStartPos = {0};
        plVec2 tEndPos = ptIO->tMainViewportSize;
        gptDraw->add_image(ptAppData->ptDrawLayer,
            gptRenderer->get_view_color_texture(ptAppData->ptView).uData,
            tStartPos,
            tEndPos);
    }

    gptDraw->submit_2d_layer(ptAppData->ptDrawLayer);

    plRenderEncoder* ptRenderEncoder = gptStarter->begin_main_pass();
    gptStarter->end_main_pass();
    pl_end_cpu_sample(gptProfile, 0);
    gptStarter->end_frame();
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
            gptUI->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
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
            gptUI->checkbox("Show Debug Lights", &ptAppData->bShowDebugLights);
            gptUI->checkbox("Show Bounding Boxes", &ptAppData->bDrawAllBoundingBoxes);
            gptUI->checkbox("Show Skybox", &ptAppData->bShowSkybox);

            gptUI->vertical_spacing();

            const float pfWidths[] = {150.0f, 150.0f};
            gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 2, pfWidths);

            gptUI->end_collapsing_header();
        }
        
        if(gptUI->begin_collapsing_header(ICON_FA_DICE_D6 " Graphics", 0))
        {
            plRendererRuntimeOptions* ptRuntimeOptions = gptRenderer->get_runtime_options();
            if(gptUI->checkbox("VSync", &ptAppData->bVSync))
            {
                if(ptAppData->bVSync)
                    gptStarter->activate_vsync();
                else
                    gptStarter->deactivate_vsync();
            }

            
            static const char* apcTonemapText[] = {
                "None",
                "Simple",
                "ACES",
                "Reinhard",
            };
            bool abTonemap[PL_ARRAYSIZE(apcTonemapText)] = {0};
            abTonemap[ptRuntimeOptions->tTonemapMode] = true;
            if(gptUI->begin_combo("Tonemapping", apcTonemapText[ptRuntimeOptions->tTonemapMode], PL_UI_COMBO_FLAGS_HEIGHT_REGULAR))
            {
                for(uint32_t i = 0; i < PL_ARRAYSIZE(apcTonemapText); i++)
                {
                    if(gptUI->selectable(apcTonemapText[i], &abTonemap[i], 0))
                    {
                        ptRuntimeOptions->tTonemapMode = i;
                        gptUI->close_current_popup();
                    } 
                }
                gptUI->end_combo();
            }

            bool bReloadShaders = false;

            
            static const char* apcShaderDebugModeText[] = {
                "None",
                "Base Color",
                "Metallic",
                "Roughness",
                "Alpha",
                "Emissive",
                "Occlusion",
                "Shading Normal",
                "Texture Normal",
                "Geometry Normal",
                "Geometry Tangent",
                "Geometry Bitangent",
                "UV 0"
            };
            bool abShaderDebugMode[PL_ARRAYSIZE(apcShaderDebugModeText)] = {0};
            abShaderDebugMode[ptRuntimeOptions->tShaderDebugMode] = true;
            if(gptUI->begin_combo("Shader Debug Mode", apcShaderDebugModeText[ptRuntimeOptions->tShaderDebugMode], PL_UI_COMBO_FLAGS_HEIGHT_REGULAR))
            {
                for(uint32_t i = 0; i < PL_ARRAYSIZE(apcShaderDebugModeText); i++)
                {
                    if(gptUI->selectable(apcShaderDebugModeText[i], &abShaderDebugMode[i], 0))
                    {
                        bReloadShaders = true;
                        if(i == 0)
                            ptRuntimeOptions->tTonemapMode = PL_TONEMAP_MODE_SIMPLE;
                        else
                            ptRuntimeOptions->tTonemapMode = PL_TONEMAP_MODE_NONE;
                        ptRuntimeOptions->tShaderDebugMode = i;
                        gptUI->close_current_popup();
                    } 
                }
                gptUI->end_combo();
            }

            gptUI->slider_float("Exposure", &ptRuntimeOptions->fExposure, 0.0f, 3.0f, 0);
            gptUI->slider_float("Brightness", &ptRuntimeOptions->fBrightness, -1.0f, 1.0f, 0);
            gptUI->slider_float("Contrast", &ptRuntimeOptions->fContrast, 0.0f, 2.0f, 0);
            gptUI->slider_float("Saturation", &ptRuntimeOptions->fSaturation, 0.0f, 2.0f, 0);

            gptUI->checkbox("Show Origin", &ptRuntimeOptions->bShowOrigin);
            gptUI->checkbox("Show BVH", &ptAppData->bShowBVH);
            
            if(gptUI->checkbox("Wireframe", &ptRuntimeOptions->bWireframe)) bReloadShaders = true;
            if(gptUI->checkbox("MultiViewport Shadows", &ptRuntimeOptions->bMultiViewportShadows)) bReloadShaders = true;
            if(gptUI->checkbox("Image Based Lighting", &ptRuntimeOptions->bImageBasedLighting)) bReloadShaders = true;
            if(gptUI->checkbox("Punctual Lighting", &ptRuntimeOptions->bPunctualLighting)) bReloadShaders = true;
            gptUI->checkbox("Show Probes", &ptRuntimeOptions->bShowProbes);
            if(gptUI->checkbox("UI MSAA", &ptAppData->bMSAA))
            {
                if(ptAppData->bMSAA)
                    gptStarter->activate_msaa();
                else
                    gptStarter->deactivate_msaa();
            }

            if(bReloadShaders)
            {
                gptRenderer->reload_scene_shaders(ptAppData->ptScene);
            }
            gptUI->checkbox("Frustum Culling", &ptAppData->bFrustumCulling);
            gptUI->checkbox("Selected Bounding Box", &ptRuntimeOptions->bShowSelectedBoundingBox);
            
            gptUI->input_float("Depth Bias", &ptRuntimeOptions->fShadowConstantDepthBias, NULL, 0);
            gptUI->input_float("Slope Depth Bias", &ptRuntimeOptions->fShadowSlopeDepthBias, NULL, 0);
            gptUI->slider_uint("Outline Width", &ptRuntimeOptions->uOutlineWidth, 2, 50, 0);
            
            if(ptAppData->ptScene)
            {
                if(gptUI->tree_node("Scene", 0))
                {
                    gptUI->checkbox("Dynamic BVH", &ptAppData->bContinuousBVH);
                    if(gptUI->button("Build BVH") || ptAppData->bContinuousBVH)
                        gptRenderer->rebuild_scene_bvh(ptAppData->ptScene);
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
                gptRenderer->reload_scene_shaders(ptAppData->ptScene);
            }
            gptUI->end_collapsing_header();
        }
        gptUI->end_window();
    }
}

void
pl__load_apis(plApiRegistryI* ptApiRegistry)
{
    gptWindows       = pl_get_api_latest(ptApiRegistry, plWindowI);
    gptStats         = pl_get_api_latest(ptApiRegistry, plStatsI);
    gptGfx           = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptTools         = pl_get_api_latest(ptApiRegistry, plToolsI);
    gptEcs           = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptCamera        = pl_get_api_latest(ptApiRegistry, plCameraI);
    gptRenderer      = pl_get_api_latest(ptApiRegistry, plRendererI);
    gptJobs          = pl_get_api_latest(ptApiRegistry, plJobI);
    gptModelLoader   = pl_get_api_latest(ptApiRegistry, plModelLoaderI);
    gptDraw          = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptDrawBackend   = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
    gptUI            = pl_get_api_latest(ptApiRegistry, plUiI);
    gptIO            = pl_get_api_latest(ptApiRegistry, plIOI);
    gptShader        = pl_get_api_latest(ptApiRegistry, plShaderI);
    gptMemory        = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptNetwork       = pl_get_api_latest(ptApiRegistry, plNetworkI);
    gptString        = pl_get_api_latest(ptApiRegistry, plStringInternI);
    gptProfile       = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptFile          = pl_get_api_latest(ptApiRegistry, plFileI);
    gptEcsTools      = pl_get_api_latest(ptApiRegistry, plEcsToolsI);
    gptGizmo         = pl_get_api_latest(ptApiRegistry, plGizmoI);
    gptConsole       = pl_get_api_latest(ptApiRegistry, plConsoleI);
    gptScreenLog     = pl_get_api_latest(ptApiRegistry, plScreenLogI);
    gptPhysics       = pl_get_api_latest(ptApiRegistry, plPhysicsI);
    gptCollision     = pl_get_api_latest(ptApiRegistry, plCollisionI);
    gptBvh           = pl_get_api_latest(ptApiRegistry, plBVHI);
    gptConfig        = pl_get_api_latest(ptApiRegistry, plConfigI);
    gptResource      = pl_get_api_latest(ptApiRegistry, plResourceI);
    gptStarter       = pl_get_api_latest(ptApiRegistry, plStarterI);
    gptAnimation     = pl_get_api_latest(ptApiRegistry, plAnimationI);
    gptMesh          = pl_get_api_latest(ptApiRegistry, plMeshI);
    gptShaderVariant = pl_get_api_latest(ptApiRegistry, plShaderVariantI);
    gptVfs           = pl_get_api_latest(ptApiRegistry, plVfsI);
    gptPak           = pl_get_api_latest(ptApiRegistry, plPakI);
    gptDateTime      = pl_get_api_latest(ptApiRegistry, plDateTimeI);
    gptCompress      = pl_get_api_latest(ptApiRegistry, plCompressI);
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
