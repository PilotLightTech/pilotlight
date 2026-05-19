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
const plCameraEcsI*         gptCameraEcs        = NULL;
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

typedef struct _plSandboxSceneFile
{
    char acName[PL_MAX_PATH_LENGTH];
    char acTemplate[PL_MAX_PATH_LENGTH];
} plSandboxSceneFile;

typedef struct _plAppData
{

    // windows
    plWindow* ptWindow;

    // graphics
    plDevice* ptDevice;
    bool      bVSync;

    // swapchains
    bool bResize;

    // ui options
    bool  bShowUiDemo;
    bool  bShowUiDebug;
    bool  bShowUiStyle;
    bool  bEditorAttached;
    bool  bShowEntityWindow;
    bool* pbShowDeviceMemoryAnalyzer;
    bool* pbShowMemoryAllocations;
    bool* pbShowProfiling;
    bool* pbShowStats;
    bool* pbShowLogging;

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

    // scene file info
    char acCurrentScene[PL_MAX_PATH_LENGTH];
    plSandboxSceneFile* sbtSceneFiles;

    plTestWorldData tTestWorld;
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] helper forward declarations
//-----------------------------------------------------------------------------

void pl__show_init_window(plAppData*);
void pl__show_editor_window(plAppData*);
void pl__load_apis(plApiRegistryI*);
void pl__refresh_files(plAppData*);
bool pl__verify_scene(plAppData*, const char* pcPath);
void pl__show_ui_demo_window(plAppData*);

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

    gptVfs->mount_directory("/gltf-samples", "../assets/gltf-samples/Models", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/environments", "../assets/development/environments", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/shaders", "../shaders", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/shader-temp", "../shader-temp", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/assets", "../assets", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/cache", "../cache", PL_VFS_MOUNT_FLAGS_NONE);
    gptFile->create_directory("../cache");
    gptFile->create_directory("../shader-temp");
    

    // defaults
    ptAppData->tSelectedEntity.uData = UINT64_MAX;
    ptAppData->tTestWorld.bShowPilotLightTool = true;
    ptAppData->tTestWorld.bFrustumCulling = true;
    ptAppData->bVSync = true;
    ptAppData->tTestWorld.bShowDebugLights = true;

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
    gptConsole->add_toggle_variable("a.PilotLight", &ptAppData->tTestWorld.bShowPilotLightTool, "shows main pilot light window", PL_CONSOLE_VARIABLE_FLAGS_CLOSE_CONSOLE);
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
    plFont* ptDefaultFont = gptDraw->add_font_from_file_ttf(gptDraw->get_current_font_atlas(), tFontConfig0, "/assets/core/fonts/Cousine-Regular.ttf");

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
    gptDraw->add_font_from_file_ttf(gptDraw->get_current_font_atlas(), tFontConfig1, "/assets/core/fonts/fa-solid-900.otf");
    gptStarter->set_default_font(ptDefaultFont);
    gptUI->set_default_font(ptDefaultFont);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~app stuff~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    gptEcs->initialize((plEcsInit){0});
    gptRendererEcs->register_system();
    gptScript->register_ecs_system();
    gptCameraEcs->register_ecs_system();
    gptAnimation->register_ecs_system();
    gptMesh->register_ecs_system();
    gptPhysics->register_ecs_system();
    gptMaterial->register_ecs_system();
    gptEcs->finalize();
    ptAppData->ptComponentLibrary = gptEcs->get_default_library();

    // pl__load_scene(ptAppData, "../internal/sandbox/scene-sponza.json");

    plIO* ptIO = gptIO->get_io();

    for(int i = 0; i < ptIO->iArgc; i++)
    {
        if(strcmp(ptIO->apArgv[i], "-s") == 0)
        {
            pl_sprintf(ptAppData->acCurrentScene, "../assets/core/scenes/scene-%s.json", ptIO->apArgv[i + 1]);
            gptRenderer->load_test_world(ptAppData->acCurrentScene, ptAppData->ptComponentLibrary, &ptAppData->tTestWorld);

            if(ptAppData->tTestWorld.bMSAA)
                gptStarter->activate_msaa();
            else
                gptStarter->deactivate_msaa();

            gptRendererEditor->rebuild_scene_bvh(ptAppData->tTestWorld.ptScene);
        }

    }

    // give starter extension chance to do its work now
    gptStarter->finalize();

    pl__refresh_files(ptAppData);

    // temporary draw layer for submitting fullscreen quad of offscreen render
    ptAppData->ptDrawLayer = gptDraw->request_2d_layer(gptUI->get_draw_list());

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
    pl_sb_free(ptAppData->sbtSceneFiles);

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

    if(ptAppData->tTestWorld.ptScene) gptRenderer->unload_test_world(&ptAppData->tTestWorld);
    
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
    if(ptAppData->tTestWorld.ptScene)
        gptCamera->set_aspect((plCamera*)gptEcs->get_component(ptAppData->ptComponentLibrary, gptCameraEcs->get_ecs_type_key(), ptAppData->tTestWorld.tMainCamera), ptIO->tMainViewportSize.x / ptIO->tMainViewportSize.y);
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

    if(ptAppData->bResize)
    {
        // gptOS->sleep(32);
        if(ptAppData->tTestWorld.ptScene)
            gptRenderer->resize_view(ptAppData->tTestWorld.ptView, ptIO->tMainViewportSize);
        ptAppData->bResize = false;
    }

    // update statistics
    gptShaderVariant->update_stats();

    plCamera* ptCamera = (plCamera*)gptEcs->get_component(ptAppData->ptComponentLibrary, gptCameraEcs->get_ecs_type_key(), ptAppData->tTestWorld.tMainCamera);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~selection stuff~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    plVec2 tMousePos = gptIO->get_mouse_pos();

    if(ptAppData->tTestWorld.ptScene && !gptUI->wants_mouse_capture() && !gptGizmo->active())
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
                gptRendererEditor->update_hovered_entity(ptAppData->tTestWorld.ptView, tHoverOffset, tHoverScale);
        }
    }

    // run ecs system
    if(ptAppData->tTestWorld.ptScene)
    {
        PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, "Run ECS");
        gptScript->run_update_system(ptAppData->ptComponentLibrary);
        gptAnimation->run_animation_update_system(ptAppData->ptComponentLibrary, ptIO->fDeltaTime);
        gptPhysics->update(ptIO->fDeltaTime, ptAppData->ptComponentLibrary);
        gptEcs->run_transform_update_system(ptAppData->ptComponentLibrary);
        gptEcs->run_hierarchy_update_system(ptAppData->ptComponentLibrary);
        gptRendererEcs->run_light_update_system(ptAppData->ptComponentLibrary);
        gptCameraEcs->run_ecs(ptAppData->ptComponentLibrary);
        gptAnimation->run_inverse_kinematics_update_system(ptAppData->ptComponentLibrary);
        gptRendererEcs->run_skin_update_system(ptAppData->ptComponentLibrary);
        gptRendererEcs->run_object_update_system(ptAppData->ptComponentLibrary);
        gptRendererEcs->run_environment_probe_update_system(ptAppData->ptComponentLibrary); // run after object update
        PL_PROFILE_END_SAMPLE_API(gptProfile, 0);


        plEntity tNextEntity = {0};
        if(gptRendererEditor->get_hovered_entity(ptAppData->tTestWorld.ptView, &tNextEntity))
        {
            
            if(tNextEntity.uData == 0)
            {
                ptAppData->tSelectedEntity.uData = UINT64_MAX;
                gptRendererEditor->outline_entities(ptAppData->tTestWorld.ptScene, 0, NULL);
            }
            else if(ptAppData->tSelectedEntity.uData != tNextEntity.uData)
            {
                gptScreenLog->add_message_ex(565168477883, 5.0, PL_COLOR_32_RED, 1.0f, "Selected Entity {%u, %u}", tNextEntity.uIndex, tNextEntity.uGeneration);
                gptRendererEditor->outline_entities(ptAppData->tTestWorld.ptScene, 1, &tNextEntity);
                ptAppData->tSelectedEntity = tNextEntity;
                gptPhysics->set_angular_velocity(ptAppData->ptComponentLibrary, tNextEntity, pl_create_vec3(0, 0, 0));
                gptPhysics->set_linear_velocity(ptAppData->ptComponentLibrary, tNextEntity, pl_create_vec3(0, 0, 0));
            }

        }

        if(gptIO->is_key_pressed(PL_KEY_M, true))
            gptGizmo->next_mode();

        if(ptAppData->bShowEntityWindow)
        {
            if(gptEcsTools->show_window(ptAppData->ptComponentLibrary, &ptAppData->tSelectedEntity, ptAppData->tTestWorld.ptScene, &ptAppData->bShowEntityWindow))
            {
                if(ptAppData->tSelectedEntity.uData == UINT64_MAX)
                {
                    gptRendererEditor->outline_entities(ptAppData->tTestWorld.ptScene, 0, NULL);
                }
                else
                {
                    gptRendererEditor->outline_entities(ptAppData->tTestWorld.ptScene, 1, &ptAppData->tSelectedEntity);
                }
            }
        }


        if(ptAppData->tSelectedEntity.uIndex != UINT32_MAX)
        {
            plDrawList3D* ptGizmoDrawlist =  gptRendererEditor->get_gizmo_drawlist(ptAppData->tTestWorld.ptView);
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
            plDrawList3D* ptDrawlist = gptRendererDebug->get_drawlist(ptAppData->tTestWorld.ptView);
            gptPhysics->draw(ptAppData->ptComponentLibrary, ptDrawlist);
        }

        // debug rendering
        if(ptAppData->tTestWorld.bShowDebugLights)
        {
            plLightComponent* ptLights = NULL;
            const uint32_t uLightCount = gptEcs->get_components(ptAppData->ptComponentLibrary, gptRendererEcs->get_ecs_type_key_light(), (void**)&ptLights, NULL);
            gptRendererDebug->draw_lights(ptAppData->tTestWorld.ptView, ptLights, uLightCount);
        }

        if(ptAppData->tTestWorld.bDrawAllBoundingBoxes)
            gptRendererDebug->draw_all_bound_boxes(ptAppData->tTestWorld.ptView);

        if(ptAppData->tTestWorld.bShowBVH)
            gptRendererDebug->draw_bvh(ptAppData->tTestWorld.ptView);

        // render scene
        gptRenderer->prepare_scene(ptAppData->tTestWorld.ptScene);
        gptRenderer->prepare_view(ptAppData->tTestWorld.ptView, ptCamera);
        plRenderViewDesc tViewDesc0 = {
            .ptCamera = ptCamera,
            .ptCullCamera = ptAppData->tTestWorld.bFrustumCulling ? ptCamera : NULL
        };
        gptRenderer->render_view(ptAppData->tTestWorld.ptView, &tViewDesc0);

    }

    // main "editor" debug window
    if(ptAppData->tTestWorld.ptScene && ptAppData->tTestWorld.bShowPilotLightTool)
        pl__show_editor_window(ptAppData);
    else
        pl__show_init_window(ptAppData);

    if(ptAppData->bShowUiDemo)
        pl__show_ui_demo_window(ptAppData);

    if(ptAppData->bShowUiStyle)
        gptUI->show_style_editor_window(&ptAppData->bShowUiStyle);

    if(ptAppData->bShowUiDebug)
        gptUI->show_debug_window(&ptAppData->bShowUiDebug);

    // add full screen quad for offscreen render
    if(ptAppData->tTestWorld.ptScene)
    {
        plVec2 tStartPos = {0};
        plVec2 tEndPos = ptIO->tMainViewportSize;
        plVec2 tUV = {0};
        plBindGroupHandle tTexture = gptRenderer->get_view_color_bind_group(ptAppData->tTestWorld.ptView, &tUV);
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
pl__show_init_window(plAppData* ptAppData)
{
    plIO* ptIO = gptIO->get_io();
    plUiWindowFlags tWindowFlags = PL_UI_WINDOW_FLAGS_NONE;
    if(ptAppData->bEditorAttached)
    {
        tWindowFlags = PL_UI_WINDOW_FLAGS_NO_TITLE_BAR | PL_UI_WINDOW_FLAGS_NO_RESIZE | PL_UI_WINDOW_FLAGS_HORIZONTAL_SCROLLBAR;
        gptUI->set_next_window_pos(pl_create_vec2(0, 0), PL_UI_COND_ALWAYS);
        gptUI->set_next_window_size(pl_create_vec2(600.0f, ptIO->tMainViewportSize.y), PL_UI_COND_ALWAYS);
    }

    if(gptUI->begin_window("Scene Selection", NULL, tWindowFlags))
    {
        gptUI->text("Select a Scene");
        gptUI->layout_static(0.0f, 75.0f, 1);
        if(gptUI->button("Refresh")) pl__refresh_files(ptAppData);
        gptUI->layout_dynamic(0.0f, 1);
        
        gptUI->dummy((plVec2){100.0f, 25.0f});
        gptUI->separator_text("Scenes");
        bool bPlaceHolder = false;
        for(uint32_t i = 0; i < pl_sb_size(ptAppData->sbtSceneFiles); i++)
        {
            if(gptUI->selectable(ptAppData->sbtSceneFiles[i].acName, &bPlaceHolder, 0))
            {
                pl_sprintf(ptAppData->acCurrentScene, "%s", ptAppData->sbtSceneFiles[i].acTemplate);
                gptRenderer->load_test_world(ptAppData->acCurrentScene, ptAppData->ptComponentLibrary, &ptAppData->tTestWorld);

                if(ptAppData->tTestWorld.bMSAA)
                    gptStarter->activate_msaa();
                else
                    gptStarter->deactivate_msaa();

                gptRendererEditor->rebuild_scene_bvh(ptAppData->tTestWorld.ptScene);

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

                    sprintf(tTile.acOutputFile, "/cache/mountains.chu");
                    sprintf(tTile.acHeightMapFile, "/assets/core/textures/mountains.png");

                    gptTerrain->process(&tTerrainInfo);
                    ptAppData->ptTerrain = gptRendererTerrain->create(ptCmdBuffer, &tTerrainInfo);
                    gptStarter->submit_temporary_command_buffer(ptCmdBuffer);
                    gptRendererTerrain->set(ptAppData->tTestWorld.ptScene, ptAppData->ptTerrain);
                #endif
            }
        }

        gptUI->end_window();
    }
}

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
    
    gptRenderer->get_bloom_options(ptAppData->tTestWorld.ptView, &tBloomOptions);
    gptRenderer->get_shadow_options(ptAppData->tTestWorld.ptScene, &tShadowOptions);
    gptRenderer->get_lighting_options(ptAppData->tTestWorld.ptScene, &tLightingOptions);
    gptRenderer->get_tonemap_options(ptAppData->tTestWorld.ptView, &tTonemapOptions);
    gptRendererEditor->get_scene_options(ptAppData->tTestWorld.ptScene, &tEditorSceneOptions);
    gptRendererEditor->get_view_options(ptAppData->tTestWorld.ptView, &tEditorViewOptions);
    gptRendererDebug->get_scene_options(ptAppData->tTestWorld.ptScene, &tDebugOptions);
    gptRenderer->get_fog_options(ptAppData->tTestWorld.ptScene, &tFogOptions);

    bool bReloadShaders = false;
    bool bReloadScene = false;

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

            if(gptUI->button("Reload Scene"))
            {
                gptRenderer->unload_test_world(&ptAppData->tTestWorld);
                gptRenderer->load_test_world(ptAppData->acCurrentScene, ptAppData->ptComponentLibrary, &ptAppData->tTestWorld);

                if(ptAppData->tTestWorld.bMSAA)
                    gptStarter->activate_msaa();
                else
                    gptStarter->deactivate_msaa();

                gptRendererEditor->rebuild_scene_bvh(ptAppData->tTestWorld.ptScene);
                bReloadScene = true;
            }

            if(gptUI->button("Unload Scene"))
            {
                gptRenderer->unload_test_world(&ptAppData->tTestWorld);
                pl__refresh_files(ptAppData);
            }

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
            gptUI->checkbox("Show Debug Lights", &ptAppData->tTestWorld.bShowDebugLights);
            gptUI->checkbox("Show Bounding Boxes", &ptAppData->tTestWorld.bDrawAllBoundingBoxes);

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
            gptUI->checkbox("Show BVH", &ptAppData->tTestWorld.bShowBVH);
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
            gptUI->checkbox("Show Probe Range", &tDebugOptions.bShowProbeRange);
            if(gptUI->checkbox("UI MSAA", &ptAppData->tTestWorld.bMSAA))
            {
                if(ptAppData->tTestWorld.bMSAA)
                    gptStarter->activate_msaa();
                else
                    gptStarter->deactivate_msaa();
            }

            gptUI->checkbox("Frustum Culling", &ptAppData->tTestWorld.bFrustumCulling);
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

            gptUI->checkbox("Dynamic BVH", &ptAppData->tTestWorld.bContinuousBVH);
            if(gptUI->button("Build BVH") || ptAppData->tTestWorld.bContinuousBVH)
                gptRendererEditor->rebuild_scene_bvh(ptAppData->tTestWorld.ptScene);
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
            gptUI->checkbox("UI Demo", &ptAppData->bShowUiDemo);
            gptUI->checkbox("UI Debug", &ptAppData->bShowUiDebug);
            gptUI->checkbox("UI Style", &ptAppData->bShowUiStyle);
            gptUI->end_collapsing_header();
        }

        gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);

        if(gptUI->begin_collapsing_header(ICON_FA_PHOTO_FILM " Renderer", 0))
        {

            if(gptUI->button("Reload Shaders"))
            {
                gptRendererEditor->reload_scene_shaders(ptAppData->tTestWorld.ptScene);
            }
            gptUI->end_collapsing_header();
        }
        gptUI->end_window();
    }

    if(!bReloadScene)
    {
        gptRenderer->set_tonemap_options(ptAppData->tTestWorld.ptView, &tTonemapOptions);
        gptRenderer->set_lighting_options(ptAppData->tTestWorld.ptScene, &tLightingOptions);
        gptRendererEditor->set_scene_options(ptAppData->tTestWorld.ptScene, &tEditorSceneOptions);
        gptRendererEditor->set_view_options(ptAppData->tTestWorld.ptView, &tEditorViewOptions);
        gptRenderer->set_bloom_options(ptAppData->tTestWorld.ptView, &tBloomOptions);
        gptRenderer->set_fog_options(ptAppData->tTestWorld.ptScene, &tFogOptions);
        gptRenderer->set_shadow_options(ptAppData->tTestWorld.ptScene, &tShadowOptions);
        gptRendererDebug->set_scene_options(ptAppData->tTestWorld.ptScene, &tDebugOptions);
    }

    if(bReloadShaders)
    {
        gptRendererEditor->reload_scene_shaders(ptAppData->tTestWorld.ptScene);
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
    gptCameraEcs        = pl_get_api_latest(ptApiRegistry, plCameraEcsI);
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

void
pl__refresh_files(plAppData* ptAppData)
{
    pl_sb_reset(ptAppData->sbtSceneFiles);

    // local
    {
        plDirectoryInfo tDirectoryInfo = {0};
        gptFile->get_directory_info("../assets/core/scenes/", &tDirectoryInfo);
        pl_sb_reserve(ptAppData->sbtSceneFiles, tDirectoryInfo.uFileCount);
        for(uint32_t i = 0; i < tDirectoryInfo.uFileCount; i++)
        {
            if(tDirectoryInfo.sbtEntries[i].tType == PL_DIRECTORY_ENTRY_TYPE_FILE)
            {
                char acExtensionBuffer[16] = {0};
                char acFileNameOnly[PL_MAX_PATH_LENGTH] = {0};
                pl_str_get_file_extension(tDirectoryInfo.sbtEntries[i].acName, acExtensionBuffer, 16);
                if(pl_str_equal("json", acExtensionBuffer))
                {
                    pl_str_get_file_name_only(tDirectoryInfo.sbtEntries[i].acName, acFileNameOnly, 128);
                    char acCurrentScene[PL_MAX_PATH_LENGTH] = {0};
                    char acFullPath[PL_MAX_PATH_LENGTH] = {0};
                    pl_sprintf(acCurrentScene, "../assets/core/scenes/%s.json", acFileNameOnly);
                    pl_sprintf(acFullPath, "../assets/core/scenes/%s.json", acFileNameOnly);
                    if(pl__verify_scene(ptAppData, acCurrentScene))
                    {
                        pl_sb_add(ptAppData->sbtSceneFiles);
                        strncpy(pl_sb_back(ptAppData->sbtSceneFiles).acName, &acFileNameOnly[6], PL_MAX_PATH_LENGTH);
                        strncpy(pl_sb_back(ptAppData->sbtSceneFiles).acTemplate, acFullPath, PL_MAX_PATH_LENGTH);
                    }
                }
            }
        }
        gptFile->cleanup_directory_info(&tDirectoryInfo);
    }

    // development
    {
        plDirectoryInfo tDirectoryInfo = {0};
        gptFile->get_directory_info("../assets/development/scenes/", &tDirectoryInfo);
        pl_sb_reserve(ptAppData->sbtSceneFiles, tDirectoryInfo.uFileCount);
        for(uint32_t i = 0; i < tDirectoryInfo.uFileCount; i++)
        {
            if(tDirectoryInfo.sbtEntries[i].tType == PL_DIRECTORY_ENTRY_TYPE_FILE)
            {
                char acExtensionBuffer[16] = {0};
                char acFileNameOnly[PL_MAX_PATH_LENGTH] = {0};
                pl_str_get_file_extension(tDirectoryInfo.sbtEntries[i].acName, acExtensionBuffer, 16);
                if(pl_str_equal("json", acExtensionBuffer))
                {
                    pl_str_get_file_name_only(tDirectoryInfo.sbtEntries[i].acName, acFileNameOnly, 128);
                    char acCurrentScene[PL_MAX_PATH_LENGTH] = {0};
                    char acFullPath[PL_MAX_PATH_LENGTH] = {0};
                    pl_sprintf(acCurrentScene, "../assets/development/scenes/%s.json", acFileNameOnly);
                    pl_sprintf(acFullPath, "../assets/development/scenes/%s.json", acFileNameOnly);
                    if(pl__verify_scene(ptAppData, acCurrentScene))
                    {
                        pl_sb_add(ptAppData->sbtSceneFiles);
                        strncpy(pl_sb_back(ptAppData->sbtSceneFiles).acName, &acFileNameOnly[6], PL_MAX_PATH_LENGTH);
                        strncpy(pl_sb_back(ptAppData->sbtSceneFiles).acTemplate, acFullPath, PL_MAX_PATH_LENGTH);
                    }
                }
            }
        }
        gptFile->cleanup_directory_info(&tDirectoryInfo);
    }
}

bool
pl__verify_scene(plAppData* ptAppData, const char* pcPath)
{
    bool bResult = true;

    size_t szJsonFileSize = gptVfs->get_file_size_str(pcPath);
    uint8_t* puFileBuffer = (uint8_t*)PL_ALLOC(szJsonFileSize + 1);
    memset(puFileBuffer, 0, szJsonFileSize + 1);

    plVfsFileHandle tHandle = gptVfs->open_file(pcPath, PL_VFS_FILE_MODE_READ);
    gptVfs->read_file(tHandle, puFileBuffer, &szJsonFileSize);
    gptVfs->close_file(tHandle);

    plJsonObject* ptRootJsonObject = NULL;
    pl_load_json((const char*)puFileBuffer, &ptRootJsonObject);

    plJsonObject* ptAppObject = pl_json_member(ptRootJsonObject, "app");

    char acFlag0[256] = {0};
    char acFlag1[256] = {0};
    char* aacFlags[] = {acFlag0, acFlag1};
    uint32_t auLengths[] = {256, 256};
    uint32_t uDependencyCount = 0;
    pl_json_string_array_member(ptRootJsonObject, "dependencies", aacFlags, &uDependencyCount, auLengths);
    for(uint32_t k = 0; k < uDependencyCount; k++)
    {
        if(!gptFile->directory_exists(acFlag0))
        {
            bResult = false;
            break;
        }
    }

    pl_unload_json(&ptRootJsonObject);
    PL_FREE(puFileBuffer);
    return bResult;
}

void
pl__show_ui_demo_window(plAppData* ptAppData)
{
    if(gptUI->begin_window("UI Demo", &ptAppData->bShowUiDemo, PL_UI_WINDOW_FLAGS_HORIZONTAL_SCROLLBAR))
    {

        static const float pfRatios0[] = {1.0f};
        gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios0);

        if(gptUI->begin_collapsing_header("Help", 0))
        {
            gptUI->text("Under construction");
            gptUI->end_collapsing_header();
        }
    
        if(gptUI->begin_collapsing_header("Window Options", 0))
        {
            gptUI->text("Under construction");
            gptUI->end_collapsing_header();
        }

        if(gptUI->begin_collapsing_header("Widgets", 0))
        {
            if(gptUI->tree_node("Basic", 0))
            {

                gptUI->layout_static(0.0f, 100, 2);
                gptUI->button("Button");
                gptUI->checkbox("Checkbox", NULL);

                gptUI->layout_dynamic(0.0f, 2);
                gptUI->button("Button");
                gptUI->checkbox("Checkbox", NULL);

                gptUI->layout_dynamic(0.0f, 1);
                static char buff[64] = {'c', 'a', 'a'};
                gptUI->input_text("label 0", buff, 64, 0);
                static char buff2[64] = {'c', 'c', 'c'};
                gptUI->input_text_hint("label 1", "hint", buff2, 64, 0);

                static float fValue = 3.14f;
                static int iValue117 = 117;

                gptUI->input_float("label 2", &fValue, "%0.3f", 0);
                gptUI->input_int("label 3", &iValue117, 0);

                static int iValue = 0;
                gptUI->layout_row_begin(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 3);

                gptUI->layout_row_push(0.33f);
                gptUI->radio_button("Option 1", &iValue, 0);

                gptUI->layout_row_push(0.33f);
                gptUI->radio_button("Option 2", &iValue, 1);

                gptUI->layout_row_push(0.34f);
                gptUI->radio_button("Option 3", &iValue, 2);

                gptUI->layout_row_end();

                const float pfRatios[] = {1.0f};
                gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
                gptUI->separator();
                gptUI->labeled_text("Label", "Value");
                static int iValue1 = 0;
                static float fValue1 = 23.0f;
                static float fValue2 = 100.0f;
                static int iValue2 = 3;
                gptUI->slider_float("float slider 1", &fValue1, 0.0f, 100.0f, 0);
                gptUI->slider_float("float slider 2", &fValue2, -50.0f, 100.0f, 0);
                gptUI->slider_int("int slider 1", &iValue1, 0, 10, 0);
                gptUI->slider_int("int slider 2", &iValue2, -5, 10, 0);
                gptUI->drag_float("float drag", &fValue2, 1.0f, -100.0f, 100.0f, 0);
                static int aiIntArray[4] = {0};
                gptUI->input_int2("input int 2", aiIntArray, 0);
                gptUI->input_int3("input int 3", aiIntArray, 0);
                gptUI->input_int4("input int 4", aiIntArray, 0);

                static float afFloatArray[4] = {0};
                gptUI->input_float2("input float 2", afFloatArray, "%0.3f", 0);
                gptUI->input_float3("input float 3", afFloatArray, "%0.3f", 0);
                gptUI->input_float4("input float 4", afFloatArray, "%0.3f", 0);

                if(gptUI->menu_item("Menu item 0", NULL, false, true))
                {
                    printf("menu item 0\n");
                }

                if(gptUI->menu_item("Menu item selected", "CTRL+M", true, true))
                {
                    printf("menu item selected\n");
                }

                if(gptUI->menu_item("Menu item disabled", NULL, false, false))
                {
                    printf("menu item disabled\n");
                }

                static bool bMenuSelection = false;
                if(gptUI->menu_item_toggle("Menu item toggle", NULL, &bMenuSelection, true))
                {
                    printf("menu item toggle\n");
                }

                if(gptUI->begin_menu("menu (not ready)", true))
                {

                    if(gptUI->menu_item("Menu item 0", NULL, false, true))
                    {
                        printf("menu item 0\n");
                    }

                    if(gptUI->menu_item("Menu item selected", "CTRL+M", true, true))
                    {
                        printf("menu item selected\n");
                    }

                    if(gptUI->menu_item("Menu item disabled", NULL, false, false))
                    {
                        printf("menu item disabled\n");
                    }
                    if(gptUI->begin_menu("sub menu", true))
                    {

                        if(gptUI->menu_item("Menu item 0", NULL, false, true))
                        {
                            printf("menu item 0\n");
                        }
                        gptUI->end_menu();
                    }
                    gptUI->end_menu();
                }


                static uint32_t uComboSelect = 0;
                static const char* apcCombo[] = {
                    "Tomato",
                    "Onion",
                    "Carrot",
                    "Lettuce",
                    "Fish"
                };
                bool abCombo[5] = {0};
                abCombo[uComboSelect] = true;
                if(gptUI->begin_combo("Combo", apcCombo[uComboSelect], PL_UI_COMBO_FLAGS_NONE))
                {
                    for(uint32_t i = 0; i < 5; i++)
                    {
                        if(gptUI->selectable(apcCombo[i], &abCombo[i], 0))
                        {
                            uComboSelect = i;
                            gptUI->close_current_popup();
                        }
                    }
                    gptUI->end_combo();
                }

                const float pfRatios22[] = {200.0f, 120.0f};
                gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 2, pfRatios22);
                gptUI->button("Hover me!");
                if(gptUI->was_last_item_hovered())
                {
                    gptUI->begin_tooltip();
                    gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios22);
                    gptUI->text("I'm a tooltip!");
                    gptUI->end_tooltip();
                }
                gptUI->button("Just a button");

                gptUI->tree_pop();
            }

            if(gptUI->tree_node("Selectables", 0))
            {
                static bool bSelectable0 = false;
                static bool bSelectable1 = false;
                static bool bSelectable2 = false;
                gptUI->selectable("Selectable 1", &bSelectable0, 0);
                gptUI->selectable("Selectable 2", &bSelectable1, 0);
                gptUI->selectable("Selectable 3", &bSelectable2, 0);
                gptUI->tree_pop();
            }

            if(gptUI->tree_node("Combo", 0))
            {
                plUiComboFlags tComboFlags = PL_UI_COMBO_FLAGS_NONE;

                static bool bComboHeightSmall = false;
                static bool bComboHeightRegular = false;
                static bool bComboHeightLarge = false;
                static bool bComboNoArrow = false;

                gptUI->checkbox("PL_UI_COMBO_FLAGS_HEIGHT_SMALL", &bComboHeightSmall);
                gptUI->checkbox("PL_UI_COMBO_FLAGS_HEIGHT_REGULAR", &bComboHeightRegular);
                gptUI->checkbox("PL_UI_COMBO_FLAGS_HEIGHT_LARGE", &bComboHeightLarge);
                gptUI->checkbox("PL_UI_COMBO_FLAGS_NO_ARROW_BUTTON", &bComboNoArrow);

                if(bComboHeightSmall)   tComboFlags |= PL_UI_COMBO_FLAGS_HEIGHT_SMALL;
                if(bComboHeightRegular) tComboFlags |= PL_UI_COMBO_FLAGS_HEIGHT_REGULAR;
                if(bComboHeightLarge)   tComboFlags |= PL_UI_COMBO_FLAGS_HEIGHT_LARGE;
                if(bComboNoArrow)       tComboFlags |= PL_UI_COMBO_FLAGS_NO_ARROW_BUTTON;

                static uint32_t uComboSelect = 0;
                static const char* apcCombo[] = {
                    "Tomato",
                    "Onion",
                    "Carrot",
                    "Lettuce",
                    "Fish",
                    "Beef",
                    "Chicken",
                    "Cereal",
                    "Wheat",
                    "Cane",
                };
                bool abCombo[10] = {0};
                abCombo[uComboSelect] = true;
                if(gptUI->begin_combo("Combo", apcCombo[uComboSelect], tComboFlags))
                {
                    for(uint32_t i = 0; i < 10; i++)
                    {
                        if(gptUI->selectable(apcCombo[i], &abCombo[i], 0))
                        {
                            uComboSelect = i;
                            gptUI->close_current_popup();
                        }
                    }
                    gptUI->end_combo();
                }
                gptUI->tree_pop();
            }

            if(gptUI->tree_node("Plotting", 0))
            {
                gptUI->progress_bar(0.75f, pl_create_vec2(-1.0f, 0.0f), NULL);
                gptUI->tree_pop();
            }

            if(gptUI->tree_node("Trees", 0))
            {
                
                if(gptUI->tree_node("Root Node", 0))
                {
                    if(gptUI->tree_node("Child 1", 0))
                    {
                        gptUI->button("Press me");
                        gptUI->tree_pop();
                    }
                    if(gptUI->tree_node("Child 2", 0))
                    {
                        gptUI->button("Press me");
                        gptUI->tree_pop();
                    }
                    gptUI->tree_pop();
                }
                gptUI->tree_pop();
            }

            if(gptUI->tree_node("Tabs", 0))
            {
                if(gptUI->begin_tab_bar("Tabs1", 0))
                {
                    if(gptUI->begin_tab("Tab 0", 0))
                    {
                        static bool bSelectable0 = false;
                        static bool bSelectable1 = false;
                        static bool bSelectable2 = false;
                        gptUI->selectable("Selectable 1", &bSelectable0, 0);
                        gptUI->selectable("Selectable 2", &bSelectable1, 0);
                        gptUI->selectable("Selectable 3", &bSelectable2, 0);
                        gptUI->end_tab();
                    }

                    if(gptUI->begin_tab("Tab 1", 0))
                    {
                        static int iValue = 0;
                        gptUI->radio_button("Option 1", &iValue, 0);
                        gptUI->radio_button("Option 2", &iValue, 1);
                        gptUI->radio_button("Option 3", &iValue, 2);
                        gptUI->end_tab();
                    }

                    if(gptUI->begin_tab("Tab 2", 0))
                    {
                        if(gptUI->begin_child("CHILD2", 0, 0))
                        {
                            const float pfRatios3[] = {600.0f};
                            gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios3);

                            for(uint32_t i = 0; i < 25; i++)
                                gptUI->text("Long text is happening11111111111111111111111111111111111111111111111111111111123456789");
                            gptUI->end_child();
                        }
                        
                        gptUI->end_tab();
                    }
                    gptUI->end_tab_bar();
                }
                gptUI->tree_pop();
            }
            gptUI->end_collapsing_header();
        }

        if(gptUI->begin_collapsing_header("Scrolling", 0))
        {
            const float pfRatios2[] = {0.5f, 0.50f};
            const float pfRatios3[] = {600.0f};

            gptUI->layout_static(0.0f, 200, 1);
            static bool bUseClipper = true;
            gptUI->checkbox("Use Clipper", &bUseClipper);
            
            gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 300.0f, 2, pfRatios2);
            if(gptUI->begin_child("CHILD", 0, 0))
            {

                gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 2, pfRatios2);


                if(bUseClipper)
                {
                    plUiClipper tClipper = {1000000};
                    while(gptUI->step_clipper(&tClipper))
                    {
                        for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
                        {
                            gptUI->text("%u Label", i);
                            gptUI->text("%u Value", i);
                        } 
                    }
                }
                else
                {
                    for(uint32_t i = 0; i < 1000000; i++)
                    {
                            gptUI->text("%u Label", i);
                            gptUI->text("%u Value", i);
                    }
                }


                gptUI->end_child();
            }
            

            if(gptUI->begin_child("CHILD2", 0, 0))
            {
                gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios3);

                for(uint32_t i = 0; i < 25; i++)
                    gptUI->text("Long text is happening11111111111111111111111111111111111111111111111111111111123456789");

                gptUI->end_child();
            }
            

            gptUI->end_collapsing_header();
        }

        if(gptUI->begin_collapsing_header("Layout Systems", 0))
        {
            gptUI->text("General Notes");
            gptUI->text("  - systems ordered by increasing flexibility");
            gptUI->separator();

            if(gptUI->tree_node("System 1 - simple dynamic", 0))
            {
                static int iWidgetCount = 5;
                static float fWidgetHeight = 0.0f;
                gptUI->separator_text("Notes");
                gptUI->text("  - wraps (i.e. will add rows)");
                gptUI->text("  - evenly spaces widgets based on available space");
                gptUI->text("  - height of 0.0f sets row height equal to minimum height");
                gptUI->text("    of maximum height widget");
                gptUI->vertical_spacing();

                gptUI->separator_text("Options");
                gptUI->slider_int("Widget Count", &iWidgetCount, 1, 10, 0);
                gptUI->slider_float("Height", &fWidgetHeight, 0.0f, 100.0f, 0);
                gptUI->vertical_spacing();

                gptUI->separator_text("Example");
                gptUI->layout_dynamic(fWidgetHeight, (uint32_t)iWidgetCount);
                gptUI->vertical_spacing();
                for(int i = 0; i < iWidgetCount * 2; i++)
                {
                    pl_sb_sprintf(ptAppData->sbcTempBuffer, "Button %d", i);
                    gptUI->button(ptAppData->sbcTempBuffer);
                    pl_sb_reset(ptAppData->sbcTempBuffer);
                }
                gptUI->tree_pop();
            }

            if(gptUI->tree_node("System 2 - simple static", 0))
            {
                static int iWidgetCount = 5;
                static float fWidgetWidth = 100.0f;
                static float fWidgetHeight = 0.0f;
                gptUI->separator_text("Notes");
                gptUI->text("  - wraps (i.e. will add rows)");
                gptUI->text("  - provides each widget with the same specified width");
                gptUI->text("  - height of 0.0f sets row height equal to minimum height");
                gptUI->text("    of maximum height widget");
                gptUI->vertical_spacing();

                gptUI->separator_text("Options");
                gptUI->slider_int("Widget Count", &iWidgetCount, 1, 10, 0);
                gptUI->slider_float("Width", &fWidgetWidth, 50.0f, 500.0f, 0);
                gptUI->slider_float("Height", &fWidgetHeight, 0.0f, 100.0f, 0);
                gptUI->vertical_spacing();

                gptUI->separator_text("Example");
                gptUI->layout_static(fWidgetHeight, fWidgetWidth, (uint32_t)iWidgetCount);
                gptUI->vertical_spacing();
                for(int i = 0; i < iWidgetCount * 2; i++)
                {
                    pl_sb_sprintf(ptAppData->sbcTempBuffer, "Button %d", i);
                    gptUI->button(ptAppData->sbcTempBuffer);
                    pl_sb_reset(ptAppData->sbcTempBuffer);
                }
                gptUI->tree_pop();
            }

            if(gptUI->tree_node("System 3 - single system row", 0))
            {
                static bool bDynamicRow = false;
                static int iWidgetCount = 2;
                static float afWidgetStaticWidths[4] = {
                    100.0f, 100.0f, 100.0f, 100.0f
                };
                static float afWidgetDynamicWidths[4] = {
                    0.25f, 0.25f, 0.25f, 0.25f
                };

                static float fWidgetHeight = 0.0f;

                gptUI->separator_text("Notes");
                gptUI->text("  - does not wrap (i.e. will not add rows)");
                gptUI->text("  - allows user to change widget widths individually");
                gptUI->text("  - widths interpreted as ratios of available width when");
                gptUI->text("    using PL_UI_LAYOUT_ROW_TYPE_DYNAMIC");
                gptUI->text("  - widths interpreted as pixel width when using PL_UI_LAYOUT_ROW_TYPE_STATIC");
                gptUI->text("  - height of 0.0f sets row height equal to minimum height");
                gptUI->text("    of maximum height widget");
                gptUI->vertical_spacing();

                gptUI->separator_text("Options");
                gptUI->checkbox("Dynamic", &bDynamicRow);
                gptUI->slider_int("Widget Count", &iWidgetCount, 1, 4, 0);
                gptUI->slider_float("Height", &fWidgetHeight, 0.0f, 100.0f, 0);

                if(bDynamicRow)
                {
                    for(int i = 0; i < iWidgetCount; i++)
                    {
                        gptUI->push_id_uint((uint32_t)i);
                        gptUI->slider_float("Widget Width", &afWidgetDynamicWidths[i], 0.05f, 1.2f, 0);
                        gptUI->pop_id();
                    }
                }
                else
                {
                    for(int i = 0; i < iWidgetCount; i++)
                    {
                        gptUI->push_id_uint((uint32_t)i);
                        gptUI->slider_float("Widget Width", &afWidgetStaticWidths[i], 50.0f, 500.0f, 0);
                        gptUI->pop_id();
                    }
                }
                gptUI->vertical_spacing();

                gptUI->separator_text("Example");
                gptUI->layout_row_begin(bDynamicRow ? PL_UI_LAYOUT_ROW_TYPE_DYNAMIC : PL_UI_LAYOUT_ROW_TYPE_STATIC, fWidgetHeight, (uint32_t)iWidgetCount);
                float* afWidgetWidths = bDynamicRow ? afWidgetDynamicWidths : afWidgetStaticWidths;
                for(int i = 0; i < iWidgetCount; i++)
                {
                    gptUI->layout_row_push(afWidgetWidths[i]);
                    pl_sb_sprintf(ptAppData->sbcTempBuffer, "Button %d", i);
                    gptUI->button(ptAppData->sbcTempBuffer);
                    pl_sb_reset(ptAppData->sbcTempBuffer);
                }
                gptUI->layout_row_end();
                gptUI->vertical_spacing();
                gptUI->tree_pop();
            }

            if(gptUI->tree_node("System 4 - single system row (array form)", 0))
            {
                static bool bDynamicRow = false;
                static int iWidgetCount = 2;
                static float afWidgetStaticWidths[4] = {
                    100.0f, 100.0f, 100.0f, 100.0f
                };
                static float afWidgetDynamicWidths[4] = {
                    0.25f, 0.25f, 0.25f, 0.25f
                };

                static float fWidgetHeight = 0.0f;

                gptUI->separator_text("Notes");
                gptUI->text("  - same as System 3 but array form");
                gptUI->text("  - wraps (i.e. will add rows)");
                gptUI->text("  - allows user to change widget widths individually");
                gptUI->text("  - widths interpreted as ratios of available width when");
                gptUI->text("    using PL_UI_LAYOUT_ROW_TYPE_DYNAMIC");
                gptUI->text("  - widths interpreted as pixel width when using PL_UI_LAYOUT_ROW_TYPE_STATIC");
                gptUI->text("  - height of 0.0f sets row height equal to minimum height");
                gptUI->text("    of maximum height widget");
                gptUI->vertical_spacing();

                gptUI->separator_text("Options");
                gptUI->checkbox("Dynamic", &bDynamicRow);
                gptUI->slider_int("Widget Count", &iWidgetCount, 1, 4, 0);
                gptUI->slider_float("Height", &fWidgetHeight, 0.0f, 100.0f, 0);

                if(bDynamicRow)
                {
                    for(int i = 0; i < iWidgetCount; i++)
                    {
                        gptUI->push_id_uint((uint32_t)i);
                        gptUI->slider_float("Widget Width", &afWidgetDynamicWidths[i], 0.05f, 1.2f, 0);
                        gptUI->pop_id();
                    }
                }
                else
                {
                    for(int i = 0; i < iWidgetCount; i++)
                    {
                        gptUI->push_id_uint((uint32_t)i);
                        gptUI->slider_float("Widget Width", &afWidgetStaticWidths[i], 50.0f, 500.0f, 0);
                        gptUI->pop_id();
                    }
                }
                gptUI->vertical_spacing();

                gptUI->separator_text("Example");
                float* afWidgetWidths = bDynamicRow ? afWidgetDynamicWidths : afWidgetStaticWidths;
                gptUI->layout_row(bDynamicRow ? PL_UI_LAYOUT_ROW_TYPE_DYNAMIC : PL_UI_LAYOUT_ROW_TYPE_STATIC, fWidgetHeight, (uint32_t)iWidgetCount, afWidgetWidths);
                for(int i = 0; i < iWidgetCount * 2; i++)
                {
                    pl_sb_sprintf(ptAppData->sbcTempBuffer, "Button %d", i);
                    gptUI->button(ptAppData->sbcTempBuffer);
                    pl_sb_reset(ptAppData->sbcTempBuffer);
                }
                gptUI->vertical_spacing();
                gptUI->tree_pop();
            }

            if(gptUI->tree_node("System 5 - template", 0))
            {
                static int iWidgetCount = 6;
                static float fWidgetHeight = 0.0f;

                gptUI->separator_text("Notes");
                gptUI->text("  - most complex and second most flexible system");
                gptUI->text("  - wraps (i.e. will add rows)");
                gptUI->text("  - allows user to change widget systems individually");
                gptUI->text("    - dynamic: changes based on available space");
                gptUI->text("    - variable: same as dynamic but minimum width specified by user");
                gptUI->text("    - static: pixel width explicitely specified by user");
                gptUI->text("  - height of 0.0f sets row height equal to minimum height");
                gptUI->text("    of maximum height widget");
                gptUI->vertical_spacing();

                gptUI->separator_text("Options");
                gptUI->slider_float("Height", &fWidgetHeight, 0.0f, 100.0f, 0);
                gptUI->vertical_spacing();

                gptUI->separator_text("Example 0");

                gptUI->layout_template_begin(fWidgetHeight);
                gptUI->layout_template_push_dynamic();
                gptUI->layout_template_push_variable(150.0f);
                gptUI->layout_template_push_static(150.0f);
                gptUI->layout_template_end();
                gptUI->button("dynamic##0");
                gptUI->button("variable 150.0f##0");
                gptUI->button("static 150.0f##0");
                gptUI->checkbox("dynamic##1", NULL);
                gptUI->checkbox("variable 150.0f##1", NULL);
                gptUI->checkbox("static 150.0f##1", NULL);
                gptUI->vertical_spacing();

                gptUI->layout_dynamic(0.0f, 1);
                gptUI->separator_text("Example 1");
                gptUI->layout_template_begin(fWidgetHeight);
                gptUI->layout_template_push_static(150.0f);
                gptUI->layout_template_push_variable(150.0f);
                gptUI->layout_template_push_dynamic();
                gptUI->layout_template_end();
                gptUI->button("static 150.0f##2");
                gptUI->button("variable 150.0f##2");
                gptUI->button("dynamic##2");
                gptUI->checkbox("static 150.0f##3", NULL);
                gptUI->checkbox("variable 150.0f##3", NULL);
                gptUI->checkbox("dynamic##3", NULL);

                gptUI->layout_dynamic(0.0f, 1);
                gptUI->separator_text("Example 2");
                gptUI->layout_template_begin(fWidgetHeight);
                gptUI->layout_template_push_variable(150.0f);
                gptUI->layout_template_push_variable(300.0f);
                gptUI->layout_template_push_dynamic();
                gptUI->layout_template_end();
                gptUI->button("variable 150.0f##4");
                gptUI->button("variable 300.0f##4");
                gptUI->button("dynamic##4");
                gptUI->checkbox("static 150.0f##5", NULL);
                gptUI->button("variable 300.0f##5");
                gptUI->checkbox("dynamic##5", NULL);
                
                gptUI->vertical_spacing();
                gptUI->tree_pop();
            }

            if(gptUI->tree_node("System 6 - space", 0))
            {
                gptUI->separator_text("Notes");
                gptUI->text("  - most flexible system");
                gptUI->vertical_spacing();

                gptUI->separator_text("Example - static");

                gptUI->layout_space_begin(PL_UI_LAYOUT_ROW_TYPE_STATIC, 500.0f, UINT32_MAX);

                gptUI->layout_space_push(0.0f, 0.0f, 100.0f, 100.0f);
                gptUI->button("w100 h100");

                gptUI->layout_space_push(105.0f, 105.0f, 300.0f, 100.0f);
                gptUI->button("x105 y105 w300 h100");

                gptUI->layout_space_end();

                gptUI->layout_dynamic(0.0f, 1);
                gptUI->separator_text("Example - dynamic");

                gptUI->layout_space_begin(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 300.0f, 2);

                gptUI->layout_space_push(0.0f, 0.0f, 0.5f, 0.5f);
                gptUI->button("x0 y0 w0.5 h0.5");

                gptUI->layout_space_push(0.5f, 0.5f, 0.5f, 0.5f);
                gptUI->button("x0.5 y0.5 w0.5 h0.5");

                gptUI->layout_space_end();

                gptUI->tree_pop();
            }

            if(gptUI->tree_node("Misc. Testing", 0))
            {
                const float pfRatios[] = {1.0f};
                const float pfRatios2[] = {0.5f, 0.5f};
                const float pfRatios3[] = {0.5f * 0.5f, 0.25f * 0.5f, 0.25f * 0.5f};
                gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 2, pfRatios2);
                if(gptUI->begin_collapsing_header("Information", 0))
                {
                    gptUI->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
                    gptUI->text("Graphics Backend: %s", gptGfx->get_backend_string());

                    gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 3, pfRatios3);
                    if(gptUI->begin_collapsing_header("sub0", 0))
                    {
                        gptUI->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
                        gptUI->end_collapsing_header();
                    }
                    if(gptUI->begin_collapsing_header("sub1", 0))
                    {
                        gptUI->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
                        gptUI->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
                        gptUI->end_collapsing_header();
                    }
                    if(gptUI->begin_collapsing_header("sub2", 0))
                    {
                        gptUI->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
                        gptUI->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
                        gptUI->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
                        gptUI->end_collapsing_header();
                    }

                    gptUI->end_collapsing_header();
                }
                if(gptUI->begin_collapsing_header("App Options", 0))
                {
                    gptUI->checkbox("Freeze Culling Camera", NULL);
                    int iCascadeCount  = 2;
                    gptUI->slider_int("Sunlight Cascades", &iCascadeCount, 1, 4, 0);

                    gptUI->end_collapsing_header();
                }
                
                if(gptUI->begin_collapsing_header("Graphics", 0))
                {
                    gptUI->checkbox("Freeze Culling Camera", NULL);
                    int iCascadeCount  = 2;
                    gptUI->slider_int("Sunlight Cascades", &iCascadeCount, 1, 4, 0);

                    gptUI->end_collapsing_header();
                }
                if(gptUI->begin_tab_bar("tab bar2", 0))
                {
                    if(gptUI->begin_tab("tab0000000000", 0))
                    {
                        gptUI->checkbox("Entities", NULL);
                        gptUI->end_tab();
                    }
                    if(gptUI->begin_tab("tab1", 0))
                    {
                        gptUI->checkbox("Profiling", NULL);
                        gptUI->checkbox("Profiling", NULL);
                        gptUI->checkbox("Profiling", NULL);
                        gptUI->checkbox("Profiling", NULL);
                        gptUI->end_tab();
                    }
                    gptUI->end_tab_bar();
                }

                gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
                if(gptUI->begin_collapsing_header("Tools", 0))
                {
                    gptUI->checkbox("Device Memory Analyzer", NULL);
                    gptUI->checkbox("Device Memory Analyzer", NULL);
                    gptUI->end_collapsing_header();
                }

                if(gptUI->begin_collapsing_header("Debug", 0))
                {
                    gptUI->button("resize");
                    gptUI->checkbox("Always Resize", NULL);
                    gptUI->end_collapsing_header();
                }

                gptUI->tree_pop();
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


#define PL_JSON_IMPLEMENTATION
#include "pl_json.h"