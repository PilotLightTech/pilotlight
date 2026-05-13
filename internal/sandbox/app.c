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
#include "pl_material_ext.h"
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
#include "pl_script_ext.h"
#include "pl_terrain_ext.h"

// shaders
#include "pl_shader_interop_renderer.h" // PL_MESH_FORMAT_FLAG_XXXX

//-----------------------------------------------------------------------------
// [SECTION] global apis
//-----------------------------------------------------------------------------

const plWindowI*            gptWindows          = NULL;
const plStatsI*             gptStats            = NULL;
const plGraphicsI*          gptGfx              = NULL;
const plToolsI*             gptTools            = NULL;
const plEcsI*               gptEcs              = NULL;
const plCameraI*            gptCamera           = NULL;
const plModelLoaderI*       gptModelLoader      = NULL;
const plJobI*               gptJobs             = NULL;
const plDrawI*              gptDraw             = NULL;
const plUiI*                gptUI               = NULL;
const plIOI*                gptIO               = NULL;
const plShaderI*            gptShader           = NULL;
const plMemoryI*            gptMemory           = NULL;
const plNetworkI*           gptNetwork          = NULL;
const plStringInternI*      gptString           = NULL;
const plProfileI*           gptProfile          = NULL;
const plFileI*              gptFile             = NULL;
const plEcsToolsI*          gptEcsTools         = NULL;
const plGizmoI*             gptGizmo            = NULL;
const plConsoleI*           gptConsole          = NULL;
const plScreenLogI*         gptScreenLog        = NULL;
const plPhysicsI *          gptPhysics          = NULL;
const plCollisionI*         gptCollision        = NULL;
const plBVHI*               gptBvh              = NULL;
const plConfigI*            gptConfig           = NULL;
const plResourceI*          gptResource         = NULL;
const plStarterI*           gptStarter          = NULL;
const plAnimationI*         gptAnimation        = NULL;
const plMeshI*              gptMesh             = NULL;
const plShaderVariantI*     gptShaderVariant    = NULL;
const plVfsI*               gptVfs              = NULL;
const plPakI*               gptPak              = NULL;
const plDateTimeI*          gptDateTime         = NULL;
const plCompressI*          gptCompress         = NULL;
const plMaterialI*          gptMaterial         = NULL;
const plScriptI*            gptScript           = NULL;
const plTerrainI*           gptTerrain          = NULL;
const plRendererI*          gptRenderer         = NULL;
const plRendererTerrainI*   gptRendererTerrain  = NULL;
const plRendererEcsI*       gptRendererEcs      = NULL;
const plRendererDebugI*     gptRendererDebug    = NULL;
const plRendererEditorI*    gptRendererEditor   = NULL;

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
    
    // terrain
    plTerrain* ptTerrain;

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
    ptExtensionRegistry->load("pl_platform_ext", "pl_load_platform_ext", "pl_unload_platform_ext", false);

    // load apis
    pl__load_apis(ptApiRegistry);

    gptProfile->begin_frame();
    gptProfile->begin_sample(0, "pl_app_load");

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = (plAppData*)PL_ALLOC(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    gptVfs->mount_directory("/models", "../data/pilotlight-assets-master/models", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/gltf", "../data/glTF-Sample-Assets-main/Models", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/fonts", "../data/pilotlight-assets-master/fonts", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/environments", "../data/pilotlight-assets-master/environments", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/textures", "../data/pilotlight-assets-master/terrain", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/shaders", "../shaders", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/shader-temp", "../shader-temp", PL_VFS_MOUNT_FLAGS_NONE);

    gptVfs->mount_directory("/assets", "../data", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/cache", "../cache", PL_VFS_MOUNT_FLAGS_NONE);
    
    gptFile->create_directory("../cache");
    gptFile->create_directory("../shader-temp");
    

    // defaults
    ptAppData->tSelectedEntity.uData = UINT64_MAX;
    ptAppData->bShowPilotLightTool = true;
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
    // tStarterInit.tFlags |= PL_STARTER_FLAGS_VSYNC_OFF;

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

    // plTerrainExtInit tTerrainExtInit = {
    //     .ptDevice = ptAppData->ptDevice
    // };
    // gptTerrain->initialize(tTerrainExtInit);

    // initialize job system
    gptJobs->initialize((plJobSystemInit){0});

    const plShaderVariantInit tShaderVariantInit = {
        .ptDevice = ptAppData->ptDevice
    };
    gptShaderVariant->initialize(tShaderVariantInit);

    // setup reference renderer
    plRendererSettings tRenderSettings = {
        .ptDevice = ptAppData->ptDevice,
        .ptSwapchain = gptStarter->get_swapchain(),
        .uMaxTextureResolution = 1024,
    };
    gptRenderer->initialize(&tRenderSettings);

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
    gptRendererEcs->register_system();
    gptScript->register_ecs_system();
    gptCamera->register_ecs_system();
    gptAnimation->register_ecs_system();
    gptMesh->register_ecs_system();
    gptPhysics->register_ecs_system();
    gptMaterial->register_ecs_system();
    gptEcs->finalize();
    ptAppData->ptComponentLibrary = gptEcs->get_default_library();

    plIO* ptIO = gptIO->get_io();
    plSceneDesc tSceneInit = {
        .ptComponentLibrary = ptAppData->ptComponentLibrary
    };
    ptAppData->ptScene = gptRenderer->create_scene(&tSceneInit);
    plViewDesc tViewDesc = PL_ZERO_INIT;
    tViewDesc.uWidth = (uint32_t)ptIO->tMainViewportSize.x;
    tViewDesc.uHeight = (uint32_t)ptIO->tMainViewportSize.y;
    ptAppData->ptView = gptRenderer->create_view(ptAppData->ptScene, &tViewDesc);

    size_t szJsonFileSize = gptVfs->get_file_size_str("../internal/sandbox/scene-sponza.json");
    uint8_t* puFileBuffer = (uint8_t*)PL_ALLOC(szJsonFileSize + 1);
    memset(puFileBuffer, 0, szJsonFileSize + 1);

    plVfsFileHandle tHandle = gptVfs->open_file("../internal/sandbox/scene-sponza.json", PL_VFS_FILE_MODE_READ);
    gptVfs->read_file(tHandle, puFileBuffer, &szJsonFileSize);
    gptVfs->close_file(tHandle);

    plJsonObject* ptRootJsonObject = NULL;
    pl_load_json((const char*)puFileBuffer, &ptRootJsonObject);

    plJsonObject* ptSceneObject = pl_json_member(ptRootJsonObject, "scene");

    plJsonObject* ptCameraObject = pl_json_member(ptSceneObject, "camera");
    plVec3d tCameraPosition = {0};
    pl_json_double_array_member(ptCameraObject, "position", tCameraPosition.d, NULL);
    float fYFov = pl_json_float_member(ptCameraObject, "yfov", PL_PI_3);
    float fNearZ = pl_json_float_member(ptCameraObject, "near plane", 0.1f);
    float fFarZ = pl_json_float_member(ptCameraObject, "far plane", 1000.0f);
    float fYaw = pl_json_float_member(ptCameraObject, "yaw", 0.0f);
    float fPitch = pl_json_float_member(ptCameraObject, "pitch", 0.0f);
    plCamera* ptMainCamera = NULL;
    ptAppData->tMainCamera = gptCamera->create_perspective(ptAppData->ptComponentLibrary, "main camera", tCameraPosition, fYFov, ptIO->tMainViewportSize.x / ptIO->tMainViewportSize.y, fNearZ, fFarZ, true, &ptMainCamera);
    gptCamera->set_pitch_yaw(ptMainCamera, fPitch, fYaw);
    gptCamera->update(ptMainCamera);
    gptScript->attach(ptAppData->ptComponentLibrary, "pl_script_camera", PL_SCRIPT_FLAG_PLAYING | PL_SCRIPT_FLAG_RELOADABLE, ptAppData->tMainCamera, NULL);

    plJsonObject* ptSkyboxObject = pl_json_member(ptSceneObject, "skybox");
    uint32_t uSkyboxResolution = pl_json_uint_member(ptSkyboxObject, "resolution", 1024);
    char acSkyboxPath[128] = {0};
    pl_json_string_member(ptSkyboxObject, "path", acSkyboxPath, 128);
    gptRendererEcs->load_skybox_from_panorama(ptAppData->ptScene, acSkyboxPath, uSkyboxResolution);

    uint32_t uProbeCount = 0;
    plJsonObject* ptProbesObject = pl_json_array_member(ptSceneObject, "probes", &uProbeCount);
    for(uint32_t i = 0; i < uProbeCount; i++)
    {
        plJsonObject* ptProbeObject = pl_json_member_by_index(ptProbesObject, i);
        plVec3 tProbePosition = {0};
        pl_json_float_array_member(ptProbeObject, "position", tProbePosition.d, NULL);

        plEnvironmentProbeComponent* ptProbe = NULL;
        plEntity tProbeEntity = gptRendererEcs->create_environment_probe(ptAppData->ptComponentLibrary, "Probe", tProbePosition, &ptProbe);
        ptProbe->fRange = pl_json_float_member(ptProbeObject, "range", 30.0);
        ptProbe->uResolution = pl_json_uint_member(ptProbeObject, "resolution", 128);
        ptProbe->uSamples = pl_json_uint_member(ptProbeObject, "samples", 512);
        ptProbe->uInterval = pl_json_uint_member(ptProbeObject, "interval", 1);
        ptProbe->tFlags |= PL_ENVIRONMENT_PROBE_FLAGS_INCLUDE_SKY;
        gptRendererEcs->add_probes_to_scene(ptAppData->ptScene, 1, &tProbeEntity);
    }

    uint32_t uLightCount = 0;
    plJsonObject* ptLightsObject = pl_json_array_member(ptSceneObject, "lights", &uLightCount);
    for(uint32_t i = 0; i < uLightCount; i++)
    {
        plJsonObject* ptLightObject = pl_json_member_by_index(ptLightsObject,i);
        plLightComponent* ptLight = NULL;

        char acType[32] = {0};
        pl_json_string_member(ptLightObject, "type", acType, 32);

        plVec3 tDirection = {0.0f, -1.0f, 0.0f};
        plVec3 tPosition = {0};

        if(acType[0] != 'p')
            pl_json_float_array_member(ptLightObject, "direction", tDirection.d, NULL);

        if(acType[0] != 'd')
            pl_json_float_array_member(ptLightObject, "position", tPosition.d, NULL);

        plVec3 tColor = {1.0f, 1.0f, 1.0f};
        pl_json_float_array_member(ptLightObject, "color", tColor.d, NULL);

        char acName[128] = {0};
        pl_json_string_member(ptLightObject, "name", acName, 128);



        plEntity tLight = {0};

        if(acType[0] == 'd')
        {
            tLight = gptRendererEcs->create_directional_light(ptAppData->ptComponentLibrary, acName, tDirection, &ptLight);
            ptLight->uCascadeCount = pl_json_uint_member(ptLightObject, "cascades", 4);
            ptLight->fShadowLambda = pl_json_float_member(ptLightObject, "shadow lambda", 0.6f);
        }
        else if(acType[0] == 'p')
        {
            tLight = gptRendererEcs->create_point_light(ptAppData->ptComponentLibrary, acName, tPosition, &ptLight);
        }
        else if(acType[0] == 's')
        {
            tLight = gptRendererEcs->create_spot_light(ptAppData->ptComponentLibrary, acName, tPosition, tDirection, &ptLight);
            ptLight->fInnerConeAngle = pl_json_float_member(ptLightObject, "inner cone angle", 0.0f);
            ptLight->fOuterConeAngle = pl_json_float_member(ptLightObject, "outer cone angle", PL_PI_4 / 2);
        }
        ptLight->fRadius = pl_json_float_member(ptLightObject, "radius", 0.25f);
        ptLight->fRange = pl_json_float_member(ptLightObject, "range", 5.0f);
        ptLight->tColor = tColor;
        ptLight->fIntensity = pl_json_float_member(ptLightObject, "intensity", 1.0f);
        ptLight->uShadowResolution = pl_json_uint_member(ptLightObject, "shadow resolution", 512);
        ptLight->tFlags |= PL_LIGHT_FLAG_CAST_SHADOW | PL_LIGHT_FLAG_VISUALIZER;

        if(acType[0] != 'd')
        {
            plTransformComponent* ptSLightTransform = (plTransformComponent* )gptEcs->add_component(ptAppData->ptComponentLibrary, gptEcs->get_ecs_type_key_transform(), tLight);
            ptSLightTransform->tTranslation = tPosition;
        }

        gptRendererEcs->add_lights_to_scene(ptAppData->ptScene, 1, &tLight);
    }

    plModelLoaderData tLoaderData = {0};
    uint32_t uModelCount = 0;
    plJsonObject* ptModelsObject = pl_json_array_member(ptSceneObject, "models", &uModelCount);
    for(uint32_t i = 0; i < uModelCount; i++)
    {
        plJsonObject* ptModelObject = pl_json_member_by_index(ptModelsObject, i);
        plVec3 tScale = {1.0f, 1.0f, 1.0f};
        plVec3 tTranslation = {0};
        plVec4 tRotation = {0};
        pl_json_float_array_member(ptModelObject, "scale", tScale.d, NULL);
        pl_json_float_array_member(ptModelObject, "translation", tTranslation.d, NULL);
        pl_json_float_array_member(ptModelObject, "rotation", tRotation.d, NULL);

        plMat4 tTransformation = pl_rotation_translation_scale(tRotation, tTranslation, tScale);

        char acModelPath[256] = {0};
        pl_json_string_member(ptModelObject, "path", acModelPath, 256);
        gptModelLoader->load_gltf(ptAppData->ptComponentLibrary, acModelPath, &tTransformation, &tLoaderData);
    }
    
    pl_unload_json(&ptRootJsonObject);
    PL_FREE(puFileBuffer);

    bool bResult = gptRendererEcs->add_drawable_objects_to_scene(ptAppData->ptScene, tLoaderData.uObjectCount, tLoaderData.atObjects);

    gptModelLoader->free_data(&tLoaderData);

    // give starter extension chance to do its work now
    gptStarter->finalize();

    // temporary draw layer for submitting fullscreen quad of offscreen render
    ptAppData->ptDrawLayer = gptDraw->request_2d_layer(gptUI->get_draw_list());

    gptRendererEditor->rebuild_scene_bvh(ptAppData->ptScene);

#if 0
    plCommandBuffer* ptCmdBuffer = gptStarter->get_temporary_command_buffer();

    plTerrainProcessTileInfo tTile = {
        .iTreeDepth    = 6,
        .fMaxHeight    = 2000.0f,
        .fMinHeight    = -40.0f,
        .fMaxBaseError = 3.0f,
        .tCenter = {0}
    };
    plTerrainProcessInfo tTerrainInfo = {
        .fMetersPerPixel = 20.0f,
        .uHorizontalTiles = 1,
        .uVerticalTiles = 1,
        .uSize = 4096,
        .uTileCount = 1,
        .atTiles = &tTile
    };

    sprintf(tTile.acOutputFile, "/assets/mountains.chu");
    sprintf(tTile.acHeightMapFile, "/assets/mountains.png");

    gptTerrain->process(&tTerrainInfo);
    ptAppData->ptTerrain = gptRendererTerrain->create(ptCmdBuffer, &tTerrainInfo);
    gptStarter->submit_temporary_command_buffer(ptCmdBuffer);
    gptRendererTerrain->set(ptAppData->ptScene, ptAppData->ptTerrain);
#endif

    gptProfile->end_sample(0);
    gptProfile->end_frame();

    uint32_t uSampleCount = 0;
    plProfileCpuSample* atSamples = gptProfile->get_last_frame_samples(0, &uSampleCount);

    const char* pcSpaceBuffer = "                                                  ";
    for(uint32_t i = 0; i < uSampleCount; i++)
    {
        printf("%s%s Duration: %g \n", &pcSpaceBuffer[49 - atSamples[i]._uDepth * 4], atSamples[i].pcName, atSamples[i].dDuration);
    }

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
    // gptTerrain->cleanup();


    if(ptAppData->ptTerrain)
        gptRendererTerrain->destroy(ptAppData->ptTerrain);
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

    gptRenderer->destroy_view(ptAppData->ptView);
    gptRenderer->destroy_scene(ptAppData->ptScene);
    
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

    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    gptResource->new_frame();
    
    // for convience
    plIO* ptIO = gptIO->get_io();

    gptRenderer->begin_frame();

    // static bool bFirstFrame = true;
    // if(bFirstFrame)
    // {
    //     plMaterialComponent* ptMaterials = NULL;
    //     const plEntity* ptMaterialEntities = NULL;
    //     const uint32_t uMaterialCount = gptEcs->get_components(ptAppData->ptComponentLibrary, gptMaterial->get_ecs_type_key(), (void**)&ptMaterials, &ptMaterialEntities);
    //     gptRenderer->update_scene_material(ptAppData->ptScene, ptMaterialEntities[0]);
    //     gptRenderer->update_scene_material(ptAppData->ptScene, ptMaterialEntities[1]);
    //     gptRenderer->update_scene_material(ptAppData->ptScene, ptMaterialEntities[2]);
    //     gptRenderer->update_scene_material(ptAppData->ptScene, ptMaterialEntities[3]);
    //     bFirstFrame = false;
    // }

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
                gptRendererEditor->update_hovered_entity(ptAppData->ptView, tHoverOffset, tHoverScale);
        }
    }

    // run ecs system
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, "Run ECS");
    gptScript->run_update_system(ptAppData->ptComponentLibrary);
    gptAnimation->run_animation_update_system(ptAppData->ptComponentLibrary, ptIO->fDeltaTime);
    gptPhysics->update(ptIO->fDeltaTime, ptAppData->ptComponentLibrary);
    gptEcs->run_transform_update_system(ptAppData->ptComponentLibrary);
    gptEcs->run_hierarchy_update_system(ptAppData->ptComponentLibrary);
    gptRendererEcs->run_light_update_system(ptAppData->ptComponentLibrary);
    gptCamera->run_ecs(ptAppData->ptComponentLibrary);
    gptAnimation->run_inverse_kinematics_update_system(ptAppData->ptComponentLibrary);
    gptRendererEcs->run_skin_update_system(ptAppData->ptComponentLibrary);
    gptRendererEcs->run_object_update_system(ptAppData->ptComponentLibrary);
    gptRendererEcs->run_environment_probe_update_system(ptAppData->ptComponentLibrary); // run after object update
    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);

    plEntity tNextEntity = {0};
    if(gptRendererEditor->get_hovered_entity(ptAppData->ptView, &tNextEntity))
    {
        
        if(tNextEntity.uData == 0)
        {
            ptAppData->tSelectedEntity.uData = UINT64_MAX;
            gptRendererEditor->outline_entities(ptAppData->ptScene, 0, NULL);
        }
        else if(ptAppData->tSelectedEntity.uData != tNextEntity.uData)
        {
            gptScreenLog->add_message_ex(565168477883, 5.0, PL_COLOR_32_RED, 1.0f, "Selected Entity {%u, %u}", tNextEntity.uIndex, tNextEntity.uGeneration);
            gptRendererEditor->outline_entities(ptAppData->ptScene, 1, &tNextEntity);
            ptAppData->tSelectedEntity = tNextEntity;
            gptPhysics->set_angular_velocity(ptAppData->ptComponentLibrary, tNextEntity, pl_create_vec3(0, 0, 0));
            gptPhysics->set_linear_velocity(ptAppData->ptComponentLibrary, tNextEntity, pl_create_vec3(0, 0, 0));
        }

    }

    if(gptIO->is_key_pressed(PL_KEY_M, true))
        gptGizmo->next_mode();

    if(ptAppData->bShowEntityWindow)
    {
        if(gptEcsTools->show_window(ptAppData->ptComponentLibrary, &ptAppData->tSelectedEntity, ptAppData->ptScene, &ptAppData->bShowEntityWindow))
        {
            if(ptAppData->tSelectedEntity.uData == UINT64_MAX)
            {
                gptRendererEditor->outline_entities(ptAppData->ptScene, 0, NULL);
            }
            else
            {
                gptRendererEditor->outline_entities(ptAppData->ptScene, 1, &ptAppData->tSelectedEntity);
            }
        }
    }

    if(ptAppData->tSelectedEntity.uIndex != UINT32_MAX)
    {
        plDrawList3D* ptGizmoDrawlist =  gptRendererEditor->get_gizmo_drawlist(ptAppData->ptView);
        plObjectComponent* ptSelectedObject = (plObjectComponent*)gptEcs->get_component(ptAppData->ptComponentLibrary, gptRendererEcs->get_ecs_type_key_object(), ptAppData->tSelectedEntity);
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
        plDrawList3D* ptDrawlist = gptRendererDebug->get_drawlist(ptAppData->ptView);
        gptPhysics->draw(ptAppData->ptComponentLibrary, ptDrawlist);
    }

    // debug rendering
    if(ptAppData->bShowDebugLights)
    {
        plLightComponent* ptLights = NULL;
        const uint32_t uLightCount = gptEcs->get_components(ptAppData->ptComponentLibrary, gptRendererEcs->get_ecs_type_key_light(), (void**)&ptLights, NULL);
        gptRendererDebug->draw_lights(ptAppData->ptView, ptLights, uLightCount);
    }

    if(ptAppData->bDrawAllBoundingBoxes)
        gptRendererDebug->draw_all_bound_boxes(ptAppData->ptView);

    if(ptAppData->bShowBVH)
        gptRendererDebug->draw_bvh(ptAppData->ptView);

    // render scene
    gptRenderer->prepare_scene(ptAppData->ptScene);
    gptRenderer->prepare_view(ptAppData->ptView, ptCamera);
    plRenderViewDesc tViewDesc0 = {
        .ptCamera = ptCamera,
        .ptCullCamera = ptAppData->bFrustumCulling ? ptCamera : NULL
    };
    gptRenderer->render_view(ptAppData->ptView, &tViewDesc0);

    // main "editor" debug window
    if(ptAppData->bShowPilotLightTool)
        pl__show_editor_window(ptAppData);

    // add full screen quad for offscreen render
    if(ptAppData->ptScene)
    {
        plVec2 tStartPos = {0};
        plVec2 tEndPos = ptIO->tMainViewportSize;
        plVec2 tUV = {0};
        plBindGroupHandle tTexture = gptRenderer->get_view_color_bind_group(ptAppData->ptView, &tUV);
        gptDraw->add_image_ex(ptAppData->ptDrawLayer,
            tTexture.uData,
            tStartPos,
            tEndPos,
            (plVec2){0},
            tUV,
            PL_COLOR_32_WHITE);
    }

    gptDraw->submit_2d_layer(ptAppData->ptDrawLayer);

    plRenderEncoder* ptRenderEncoder = gptStarter->begin_main_pass();
    gptStarter->end_main_pass();
    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
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

    plRendererEditorSceneOptions tEditorSceneOptions = PL_ZERO_INIT;
    plRendererEditorViewOptions tEditorViewOptions = PL_ZERO_INIT;
    plRendererDebugSceneOptions tDebugOptions = PL_ZERO_INIT;
    plRendererTonemapOptions tTonemapOptions = PL_ZERO_INIT;
    plRendererLightingOptions tLightingOptions = PL_ZERO_INIT;
    plRendererShadowOptions tShadowOptions = PL_ZERO_INIT;
    plRendererBloomOptions tBloomOptions = PL_ZERO_INIT;
    plRendererFogOptions tFogOptions = PL_ZERO_INIT;
    
    gptRenderer->get_bloom_options(ptAppData->ptView, &tBloomOptions);
    gptRenderer->get_shadow_options(ptAppData->ptScene, &tShadowOptions);
    gptRenderer->get_lighting_options(ptAppData->ptScene, &tLightingOptions);
    gptRenderer->get_tonemap_options(ptAppData->ptView, &tTonemapOptions);
    gptRendererEditor->get_scene_options(ptAppData->ptScene, &tEditorSceneOptions);
    gptRendererEditor->get_view_options(ptAppData->ptView, &tEditorViewOptions);
    gptRendererDebug->get_scene_options(ptAppData->ptScene, &tDebugOptions);
    gptRenderer->get_fog_options(ptAppData->ptScene, &tFogOptions);

    bool bReloadShaders = false;

    plTerrainRuntimeOptions* ptRuntimeOptions = gptRendererTerrain->get_runtime_options(ptAppData->ptTerrain);

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
            plScreenLogFlags tScreenLogFlags = gptScreenLog->get_flags();

            // if(gptUI->button("Add Model 0"))
            // {
            //     plModelLoaderData tLoaderData = {0};
            //     plMat4 tModelTranslation = pl_mat4_translate_xyz(0.0f, 0.0f, 0.0f);
            //     gptModelLoader->load_gltf(ptAppData->ptComponentLibrary, "/gltf/DamagedHelmet/glTF/DamagedHelmet.gltf", &tModelTranslation, &tLoaderData);

            //     plMaterialComponent* ptMaterials = NULL;
            //     const plEntity* ptMaterialEntities = NULL;
            //     const uint32_t uMaterialCount = gptEcs->get_components(ptAppData->ptComponentLibrary, gptMaterial->get_ecs_type_key(), (void**)&ptMaterials, &ptMaterialEntities);
            //     gptRenderer->add_materials_to_scene(ptAppData->ptScene, uMaterialCount, ptMaterialEntities);

            //     bool bResult = gptRenderer->add_drawable_objects_to_scene(ptAppData->ptScene, tLoaderData.uObjectCount, tLoaderData.atObjects);
            //     gptModelLoader->free_data(&tLoaderData);
            // }

            bool bHideScreenLog = tScreenLogFlags & PL_SCREEN_LOG_FLAGS_HIDE_MESSAGES;
            if(gptUI->checkbox("Hide Screen Log", &bHideScreenLog))
            {
                if(bHideScreenLog)
                    tScreenLogFlags |= PL_SCREEN_LOG_FLAGS_HIDE_MESSAGES;
                else
                    tScreenLogFlags &= ~PL_SCREEN_LOG_FLAGS_HIDE_MESSAGES;
                gptScreenLog->set_flags(tScreenLogFlags);
            }


            gptUI->checkbox("Editor Attached", &ptAppData->bEditorAttached);
            gptUI->checkbox("Show Debug Lights", &ptAppData->bShowDebugLights);
            gptUI->checkbox("Show Bounding Boxes", &ptAppData->bDrawAllBoundingBoxes);

            gptUI->vertical_spacing();

            if(ptAppData->ptTerrain)
            {
                if(gptUI->tree_node("Terrain", 0))
                {
                    
                    gptUI->slider_float("fTau", &ptRuntimeOptions->fTau, 0.0f, 1.0f, 0);

                    bool bWireframe = ptRuntimeOptions->tFlags & PL_TERRAIN_FLAGS_WIREFRAME;
                    bool bShowLevels = ptRuntimeOptions->tFlags & PL_TERRAIN_FLAGS_SHOW_LEVELS;

                    if(gptUI->checkbox("Wireframe", &bWireframe))
                    {
                        if(bWireframe) ptRuntimeOptions->tFlags |= PL_TERRAIN_FLAGS_WIREFRAME;
                        else           ptRuntimeOptions->tFlags &= ~PL_TERRAIN_FLAGS_WIREFRAME;
                    }

                    if(gptUI->checkbox("Show Levels", &bShowLevels))
                    {
                        if(bShowLevels) ptRuntimeOptions->tFlags |= PL_TERRAIN_FLAGS_SHOW_LEVELS;
                        else           ptRuntimeOptions->tFlags &= ~PL_TERRAIN_FLAGS_SHOW_LEVELS;
                    }

                    gptUI->slider_float("fSlopeStart", &ptRuntimeOptions->fSlopeStart, 0.0f, 1.0f, 0);
                    gptUI->slider_float("fSlopeEnd", &ptRuntimeOptions->fSlopeEnd, 0.0f, 1.0f, 0);

                    gptUI->input_float("Terrain Depth Bias", &ptRuntimeOptions->fTerrainShadowConstantDepthBias, NULL, 0);
                    gptUI->input_float("Terrain Slope Depth Bias", &ptRuntimeOptions->fTerrainShadowSlopeDepthBias, NULL, 0);

                    for(uint32_t i = 0; i < PL_MAX_TERRAIN_ELEVATION_ZONES; i++)
                    {
                        gptUI->push_id_uint(i);
                        gptUI->text("Zone %u", i);
                        gptUI->input_float("fMinElevation", &ptRuntimeOptions->atElevationZones[i].fMinElevation, NULL, 0);
                        gptUI->input_float("fMaxElevation", &ptRuntimeOptions->atElevationZones[i].fMaxElevation, NULL, 0);
                        gptUI->input_float("fBlendSize", &ptRuntimeOptions->atElevationZones[i].fBlendSize, NULL, 0);
                        gptUI->input_float4("Flat Material", ptRuntimeOptions->atElevationZones[i].tFlatMaterial.tBaseColor.d, NULL, 0);
                        gptUI->input_float4("Steep Material", ptRuntimeOptions->atElevationZones[i].tSteepMaterial.tBaseColor.d, NULL, 0);
                        gptUI->pop_id();
                    }

                    gptUI->tree_pop();
                }

            }

            const float pfWidths[] = {150.0f, 150.0f};
            gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 2, pfWidths);

            gptUI->end_collapsing_header();
        }
        
        if(gptUI->begin_collapsing_header(ICON_FA_DICE_D6 " Graphics", 0))
        {

            if(gptUI->checkbox("VSync", &ptAppData->bVSync))
            {
                if(ptAppData->bVSync)
                    gptStarter->activate_vsync();
                else
                    gptStarter->deactivate_vsync();
            }

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
                "UV 0",
                "Clearcoat",
                "Clearcoat Roughness",
                "Clearcoat Normal",
                "Sheen Color",
                "Sheen Roughness",
                "Iridescence Factor",
                "Iridescence Thickness",
                "Anisotropy Strength",
                "Anisotropy Direction",
                "Transmission Strength",
                "Volume Thickness",
                "Diffuse Transmission Strength",
                "Diffuse Transmission Color",
            };
            bool abShaderDebugMode[PL_ARRAYSIZE(apcShaderDebugModeText)] = {0};
            abShaderDebugMode[tDebugOptions.tShaderDebugMode] = true;
            if(gptUI->begin_combo("Shader Debug Mode", apcShaderDebugModeText[tDebugOptions.tShaderDebugMode], PL_UI_COMBO_FLAGS_HEIGHT_REGULAR))
            {
                for(uint32_t i = 0; i < PL_ARRAYSIZE(apcShaderDebugModeText); i++)
                {
                    if(gptUI->selectable(apcShaderDebugModeText[i], &abShaderDebugMode[i], 0))
                    {

                        bReloadShaders = true;
                        if(i == 0)
                            tTonemapOptions.tMode = PL_TONEMAP_MODE_SIMPLE;
                        else
                            tTonemapOptions.tMode = PL_TONEMAP_MODE_NONE;
                        tDebugOptions.tShaderDebugMode = i;
                        gptUI->close_current_popup();
                    } 
                }
                gptUI->end_combo();
            }

            gptUI->checkbox("Show Origin", &tDebugOptions.bShowOrigin);
            gptUI->checkbox("Show BVH", &ptAppData->bShowBVH);
            gptUI->checkbox("Show Skybox", &tEditorViewOptions.bShowSkybox);
            gptUI->checkbox("Show Grid", &tEditorViewOptions.bShowGrid);
            
            if(gptUI->checkbox("Wireframe", &tDebugOptions.bWireframe)) bReloadShaders = true;
            

            bool bImageBasedLighting = tLightingOptions.tFlags & PL_RENDERER_LIGHTING_FLAGS_IMAGE_BASED;
            bool bPunctualLighting = tLightingOptions.tFlags & PL_RENDERER_LIGHTING_FLAGS_PUNCTUAL_LIGHTS;
            bool bNormalMapping = tLightingOptions.tFlags & PL_RENDERER_LIGHTING_FLAGS_NORMAL_MAPPING;
            if(gptUI->checkbox("Image Based Lighting", &bImageBasedLighting))
            {
                if(bImageBasedLighting) tLightingOptions.tFlags |= PL_RENDERER_LIGHTING_FLAGS_IMAGE_BASED;
                else                    tLightingOptions.tFlags &= ~PL_RENDERER_LIGHTING_FLAGS_IMAGE_BASED;
                bReloadShaders = true;
            }

            if(gptUI->checkbox("Punctual Lighting", &bPunctualLighting))
            {
                if(bPunctualLighting) tLightingOptions.tFlags |= PL_RENDERER_LIGHTING_FLAGS_PUNCTUAL_LIGHTS;
                else                  tLightingOptions.tFlags &= ~PL_RENDERER_LIGHTING_FLAGS_PUNCTUAL_LIGHTS;
                bReloadShaders = true;
            }
            
            if(gptUI->checkbox("Normal Mapping", &bNormalMapping))
            {
                if(bNormalMapping) tLightingOptions.tFlags |= PL_RENDERER_LIGHTING_FLAGS_NORMAL_MAPPING;
                else               tLightingOptions.tFlags &= ~PL_RENDERER_LIGHTING_FLAGS_NORMAL_MAPPING;
                bReloadShaders = true;
            }

            gptUI->checkbox("Show Probes", &tDebugOptions.bShowProbes);
            if(gptUI->checkbox("UI MSAA", &ptAppData->bMSAA))
            {
                if(ptAppData->bMSAA)
                    gptStarter->activate_msaa();
                else
                    gptStarter->deactivate_msaa();
            }

            gptUI->checkbox("Frustum Culling", &ptAppData->bFrustumCulling);
            gptUI->checkbox("Selected Bounding Box", &tEditorViewOptions.bShowSelectedBoundingBox);
            
            gptUI->slider_uint("Outline Width", &tEditorViewOptions.uOutlineWidth, 2, 50, 0);

            bool bMultiViewportShadows = tShadowOptions.tFlags & PL_RENDERER_SHADOW_FLAGS_MULTI_VIEWPORT;
            bool bPcfShadows = tShadowOptions.tFlags & PL_RENDERER_SHADOW_FLAGS_PCF;

            if(gptUI->checkbox("MultiViewport Shadows", &bMultiViewportShadows))
            {
                if(bMultiViewportShadows) tShadowOptions.tFlags |= PL_RENDERER_SHADOW_FLAGS_MULTI_VIEWPORT;
                else                      tShadowOptions.tFlags &= ~PL_RENDERER_SHADOW_FLAGS_MULTI_VIEWPORT;
                bReloadShaders = true;
            }

            if(gptUI->checkbox("PCF Shadows", &bPcfShadows))
            {
                if(bPcfShadows) tShadowOptions.tFlags |= PL_RENDERER_SHADOW_FLAGS_PCF;
                else            tShadowOptions.tFlags &= ~PL_RENDERER_SHADOW_FLAGS_PCF;
                bReloadShaders = true;
            }

            gptUI->input_float("Depth Bias", &tShadowOptions.fConstantDepthBias, NULL, 0);
            gptUI->input_float("Slope Depth Bias", &tShadowOptions.fSlopeDepthBias, NULL, 0);
            gptUI->slider_float("Max Shadow Range", &tShadowOptions.fMaxShadowRange, 100.0f, 1000.0f, 0);

            gptUI->checkbox("Dynamic BVH", &ptAppData->bContinuousBVH);
            if(gptUI->button("Build BVH") || ptAppData->bContinuousBVH)
                gptRendererEditor->rebuild_scene_bvh(ptAppData->ptScene);
            gptUI->end_collapsing_header();
        }

        if(gptUI->begin_collapsing_header(ICON_FA_FILE_IMAGE " Post Process", 0))
        {
            static const char* apcTonemapText[] = {
                "None",
                "Simple",
                "ACES Filmic (Narkowicz)",
                "ACES Filmic (Hill)",
                "ACES Filmic (Hill Exposure Boost)",
                "Reinhard",
                "Khronos PBR Neutral",
            };
            bool abTonemap[PL_ARRAYSIZE(apcTonemapText)] = {0};
            abTonemap[tTonemapOptions.tMode] = true;
            if(gptUI->begin_combo("Tonemapping", apcTonemapText[tTonemapOptions.tMode], PL_UI_COMBO_FLAGS_HEIGHT_REGULAR))
            {
                for(uint32_t i = 0; i < PL_ARRAYSIZE(apcTonemapText); i++)
                {
                    if(gptUI->selectable(apcTonemapText[i], &abTonemap[i], 0))
                    {
                        tTonemapOptions.tMode = i;
                        gptUI->close_current_popup();
                    } 
                }
                gptUI->end_combo();
            }

            gptUI->slider_float("Exposure", &tTonemapOptions.fExposure, 0.0f, 3.0f, 0);
            gptUI->slider_float("Brightness", &tTonemapOptions.fBrightness, -1.0f, 1.0f, 0);
            gptUI->slider_float("Contrast", &tTonemapOptions.fContrast, 0.0f, 2.0f, 0);
            gptUI->slider_float("Saturation", &tTonemapOptions.fSaturation, 0.0f, 2.0f, 0);

            gptUI->separator_text("Bloom");
            bool bBloomActive = tBloomOptions.tFlags & PL_RENDERER_BLOOM_FLAGS_ACTIVE;
            gptUI->checkbox("Bloom", &bBloomActive);
            if(bBloomActive)
            {
                gptUI->slider_float("Bloom Radius", &tBloomOptions.fRadius, 0.0f, 10.0f, 0);
                gptUI->slider_float("Bloom Strength", &tBloomOptions.fStrength, 0.0f, 1.0f, 0);
                gptUI->slider_uint("Bloom Chain", &tBloomOptions.uChainLength, 1, 10, 0);
                tBloomOptions.tFlags |= PL_RENDERER_BLOOM_FLAGS_ACTIVE;
            }
            else
                tBloomOptions.tFlags &= ~PL_RENDERER_BLOOM_FLAGS_ACTIVE;
            
            gptUI->separator_text("Fog");
            
            bool bFog = tFogOptions.tFlags & PL_RENDERER_FOG_FLAGS_ACTIVE;
            gptUI->checkbox("Fog", &bFog);
            if(bFog)
            {
                tFogOptions.tFlags |= PL_RENDERER_FOG_FLAGS_ACTIVE;
                gptUI->radio_button("Linear Fog", &tFogOptions.tMode, 0);
                gptUI->radio_button("Exponential Fog", &tFogOptions.tMode, 1);
                gptUI->slider_float("Fog Start", &tFogOptions.fStart, 0.0f, 100.0f, 0);
                gptUI->slider_float("Fog End", &tFogOptions.fCutOffDistance, 0.0f, 10000.0f, 0);
                gptUI->input_float3("Fog Color", tFogOptions.tColor.d, NULL, 0);
                if(tFogOptions.tMode == PL_RENDERER_FOG_MODE_EXPONENTIAL)
                {
                    gptUI->slider_float("Fog Max Opacity", &tFogOptions.fMaxOpacity, 0.0f, 1.0f, 0);
                    gptUI->slider_float("Fog Density", &tFogOptions.fDensity, 0.0f, 1.0f, 0);
                    gptUI->slider_float("Fog Height", &tFogOptions.fHeight, -100.0f, 100.0f, 0);
                    gptUI->slider_float("Fog Height Falloff", &tFogOptions.fHeightFalloff, 0.0f, 1.0f, 0);
                }  
            }
            else
                tFogOptions.tFlags &= ~PL_RENDERER_FOG_FLAGS_ACTIVE;
            
            gptUI->end_collapsing_header();
        }

        if(gptUI->begin_collapsing_header(ICON_FA_BOXES_STACKED " Physics", 0))
        {
            plPhysicsEngineSettings tPhysicsSettings = {0};
            gptPhysics->get_settings(&tPhysicsSettings);

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
                gptRendererEditor->reload_scene_shaders(ptAppData->ptScene);
            }
            gptUI->end_collapsing_header();
        }
        gptUI->end_window();
    }

    gptRenderer->set_tonemap_options(ptAppData->ptView, &tTonemapOptions);
    gptRenderer->set_lighting_options(ptAppData->ptScene, &tLightingOptions);
    gptRendererEditor->set_scene_options(ptAppData->ptScene, &tEditorSceneOptions);
    gptRendererEditor->set_view_options(ptAppData->ptView, &tEditorViewOptions);
    gptRenderer->set_bloom_options(ptAppData->ptView, &tBloomOptions);
    gptRenderer->set_fog_options(ptAppData->ptScene, &tFogOptions);
    gptRenderer->set_shadow_options(ptAppData->ptScene, &tShadowOptions);
    gptRendererDebug->set_scene_options(ptAppData->ptScene, &tDebugOptions);

    if(bReloadShaders)
    {
        gptRendererEditor->reload_scene_shaders(ptAppData->ptScene);
    }
}

void
pl__load_apis(plApiRegistryI* ptApiRegistry)
{
    gptWindows          = pl_get_api_latest(ptApiRegistry, plWindowI);
    gptStats            = pl_get_api_latest(ptApiRegistry, plStatsI);
    gptGfx              = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptTools            = pl_get_api_latest(ptApiRegistry, plToolsI);
    gptEcs              = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptCamera           = pl_get_api_latest(ptApiRegistry, plCameraI);
    gptRenderer         = pl_get_api_latest(ptApiRegistry, plRendererI);
    gptJobs             = pl_get_api_latest(ptApiRegistry, plJobI);
    gptModelLoader      = pl_get_api_latest(ptApiRegistry, plModelLoaderI);
    gptDraw             = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptUI               = pl_get_api_latest(ptApiRegistry, plUiI);
    gptIO               = pl_get_api_latest(ptApiRegistry, plIOI);
    gptShader           = pl_get_api_latest(ptApiRegistry, plShaderI);
    gptMemory           = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptNetwork          = pl_get_api_latest(ptApiRegistry, plNetworkI);
    gptString           = pl_get_api_latest(ptApiRegistry, plStringInternI);
    gptProfile          = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptFile             = pl_get_api_latest(ptApiRegistry, plFileI);
    gptEcsTools         = pl_get_api_latest(ptApiRegistry, plEcsToolsI);
    gptGizmo            = pl_get_api_latest(ptApiRegistry, plGizmoI);
    gptConsole          = pl_get_api_latest(ptApiRegistry, plConsoleI);
    gptScreenLog        = pl_get_api_latest(ptApiRegistry, plScreenLogI);
    gptPhysics          = pl_get_api_latest(ptApiRegistry, plPhysicsI);
    gptCollision        = pl_get_api_latest(ptApiRegistry, plCollisionI);
    gptBvh              = pl_get_api_latest(ptApiRegistry, plBVHI);
    gptConfig           = pl_get_api_latest(ptApiRegistry, plConfigI);
    gptResource         = pl_get_api_latest(ptApiRegistry, plResourceI);
    gptStarter          = pl_get_api_latest(ptApiRegistry, plStarterI);
    gptAnimation        = pl_get_api_latest(ptApiRegistry, plAnimationI);
    gptMesh             = pl_get_api_latest(ptApiRegistry, plMeshI);
    gptShaderVariant    = pl_get_api_latest(ptApiRegistry, plShaderVariantI);
    gptVfs              = pl_get_api_latest(ptApiRegistry, plVfsI);
    gptPak              = pl_get_api_latest(ptApiRegistry, plPakI);
    gptDateTime         = pl_get_api_latest(ptApiRegistry, plDateTimeI);
    gptCompress         = pl_get_api_latest(ptApiRegistry, plCompressI);
    gptMaterial         = pl_get_api_latest(ptApiRegistry, plMaterialI);
    gptScript           = pl_get_api_latest(ptApiRegistry, plScriptI);
    gptTerrain          = pl_get_api_latest(ptApiRegistry, plTerrainI);
    gptRendererTerrain  = pl_get_api_latest(ptApiRegistry, plRendererTerrainI);
    gptRendererEcs      = pl_get_api_latest(ptApiRegistry, plRendererEcsI);
    gptRendererDebug    = pl_get_api_latest(ptApiRegistry, plRendererDebugI);
    gptRendererEditor   = pl_get_api_latest(ptApiRegistry, plRendererEditorI);
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


#define PL_JSON_IMPLEMENTATION
#include "pl_json.h"