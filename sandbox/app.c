/*
   app.c

   Notes:
     * absolute mess
     * mostly a sandbox for now & testing experimental stuff
     * probably better to look at the examples or editor repo
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
#include <inttypes.h>

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
#include "pl_vfs_ext.h"
#include "pl_compress_ext.h"
#include "pl_pak_ext.h"
#include "pl_datetime_ext.h"
#include "pl_ecs_ext.h"
#include "pl_transform_ext.h"

// unstable extensions
#include "pl_mesh_ext.h"
#include "pl_animation_ext.h"
#include "pl_camera_ext.h"
#include "pl_config_ext.h"
#include "pl_resource_ext.h"
#include "pl_gltf_ext.h"
#include "pl_renderer_ext.h"
#include "pl_asset_tools_ext.h"
#include "pl_gizmo_ext.h"
#include "pl_physics_ext.h"
#include "pl_collision_ext.h"
#include "pl_bvh_ext.h"
#include "pl_shader_variant_ext.h"
#include "pl_material_ext.h"
#include "pl_script_ext.h"
#include "pl_asset_ext.h"
#include "pl_ik_ext.h"
#include "pl_skeleton_ext.h"
#include "pl_texture_ext.h"
#include "pl_terrain_ext.h"
#include "pl_stl_ext.h"

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
const plRendererI*          gptRenderer         = NULL;
const plGltfI*              gptGltf             = NULL;
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
const plAssetToolsI*        gptAssetTools       = NULL;
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
const plAssetI*             gptAsset            = NULL;
const plTransformI*         gptTransform        = NULL;
const plIkI*                gptIk               = NULL;
const plSkeletonI*          gptSkeleton         = NULL;
const plTextureI*           gptTexture          = NULL;
const plTerrainI*           gptTerrain          = NULL;
const plStlI*               gptStl              = NULL;

#define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
#define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

#define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
#define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
#include "pl_ds.h"

#define PL_JSON_ALLOC(x) gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_JSON_FREE(x)  gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

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
    plDevice*    ptDevice;
    plDeviceInfo tDeviceInfo;

    // swapchains
    bool bResize;

    // ui options
    // bool bContinuousBVH;

    // pilot light ui windows
    bool  bShowUiDemo;
    bool* pbShowDeviceMemoryAnalyzer;
    bool* pbShowMemoryAllocations;
    bool* pbShowProfiling;
    bool* pbShowStats;
    bool* pbShowLogging;
    bool* pbShowAssets;
    bool* pbShowRenderer;

    // scene
    plEntity tMainCamera;

    // scenes/views
    plScene* ptScene;
    plView*  ptView;
    plAssetHandle tSceneHandle;

    // drawing
    plDrawLayer2D* ptDrawLayer;

    // selection stuff
    plEntityId tSelectedEntityId;
    plEntity tSelectedEntity;
    
    // fonts
    plFont* tDefaultFont;

    // scene file info
    char acCurrentScene[PL_MAX_PATH_LENGTH];
    plSandboxSceneFile* sbtSceneFilesCore;

    char* sbcTempBuffer;

} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] helper forward declarations
//-----------------------------------------------------------------------------

void pl__show_ui_demo_window(plAppData* ptAppData);

void pl__load_apis(plApiRegistryI*);
void pl__refresh_files(plAppData*);
void pl__load_assets(plAppData*);

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
    ptExtensionRegistry->load("pl_shader_ext", "pl_load_shader_ext", "pl_unload_shader_ext", true);
    ptExtensionRegistry->load("pl_graphics_ext", "pl_load_graphics_ext", "pl_unload_graphics_ext", true);

    // load apis
    pl__load_apis(ptApiRegistry);

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = (plAppData*)PL_ALLOC(sizeof(plAppData));
    memset((void*)ptAppData, 0, sizeof(plAppData));

    // mount required directories
    gptVfs->mount_directory("/shaders",   "../shaders",   PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/resources", "../resources", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/cache",     "../cache",     PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/assets",    "../assets",    PL_VFS_MOUNT_FLAGS_NONE);

    // create cache directories
    gptFile->create_directory("../cache");
    gptFile->create_directory("../cache/shaders");
    gptFile->create_directory("../cache/imports");
    gptFile->create_directory("../cache/textures");
    gptFile->create_directory("../cache/terrain");

    // create asset directories
    gptFile->create_directory("../assets/materials");
    gptFile->create_directory("../assets/textures");
    gptFile->create_directory("../assets/models");
    gptFile->create_directory("../assets/meshes");
    gptFile->create_directory("../assets/animations");
    gptFile->create_directory("../assets/skeletons");
    gptFile->create_directory("../assets/skins");
    gptFile->create_directory("../assets/scenes");
    // gptFile->create_directory("../assets/shaders");
    // gptFile->create_directory("../assets/fonts");

    // defaults
    ptAppData->tSelectedEntity.uData = UINT64_MAX;

    gptConfig->load_from_disk(NULL);

    // add console variables
    plConsoleSettings tConsoleSettings = {
        .eFlags = PL_CONSOLE_FLAGS_POPUP
    };
    gptConsole->initialize(tConsoleSettings);
    gptConsole->add_toggle_variable("a.UI_demo", &ptAppData->bShowUiDemo, "shows ui demo", PL_CONSOLE_VARIABLE_FLAGS_CLOSE_CONSOLE);

    // initialize APIs that require it
    gptAssetTools->initialize();

    plPhysicsEngineSettings tPhysicsSettings = {0};
    gptPhysics->initialize(tPhysicsSettings);

    // create window (only 1 allowed currently)
    plWindowDesc tWindowDesc = {
        .tMode = PL_WINDOW_PRESENTATION_MODE_GRAPHICS_API,
        .tFlags = PL_WINDOW_FLAG_NONE,
        .pcTitle = "Pilot Light Sandbox",
        .uWidth = 1500,
        .uHeight = 900,
        .iXPos = 50,
        .iYPos = 50
    };
    gptWindows->create(tWindowDesc, &ptAppData->ptWindow);
    gptWindows->show(ptAppData->ptWindow);

    plFullScreenDesc tFullScreen = {
        .iMonitor = 0
    };
    gptWindows->set_fullscreen(ptAppData->ptWindow, &tFullScreen);

    plStarterInit tStarterInit = {
        .eFlags = PL_STARTER_FLAGS_NONE,
        .ptWindow = ptAppData->ptWindow
    };

    // extensions handled by starter
    tStarterInit.eFlags |= PL_STARTER_FLAGS_GRAPHICS_EXT;
    tStarterInit.eFlags |= PL_STARTER_FLAGS_PROFILE_EXT;
    tStarterInit.eFlags |= PL_STARTER_FLAGS_STATS_EXT;
    tStarterInit.eFlags |= PL_STARTER_FLAGS_CONSOLE_EXT;
    tStarterInit.eFlags |= PL_STARTER_FLAGS_TOOLS_EXT;
    tStarterInit.eFlags |= PL_STARTER_FLAGS_DRAW_EXT;
    tStarterInit.eFlags |= PL_STARTER_FLAGS_UI_EXT;
    tStarterInit.eFlags |= PL_STARTER_FLAGS_RESOURCE_EXT;
    tStarterInit.eFlags |= PL_STARTER_FLAGS_SHADER_EXT;
    tStarterInit.eFlags |= PL_STARTER_FLAGS_SCREEN_LOG_EXT;

    // initial flags
    // tStarterInit.eFlags |= PL_STARTER_FLAGS_DEPTH_BUFFER;

    // from a graphics standpoint, the starter extension is handling device, swapchain, renderpass
    // etc. which we will get to in later examples
    gptStarter->initialize(tStarterInit);
    
    ptAppData->ptDevice = gptStarter->get_device();

    // initialize job system
    plJobSystemInit tJobInit = {0};
    gptJobs->initialize(tJobInit);

    const plShaderVariantInit tShaderVariantInit = {
        ptAppData->ptDevice
    };
    gptShaderVariant->initialize(tShaderVariantInit);

    // setup reference renderer
    plRendererSettings tRenderSettings = PL_ZERO_INIT;
    tRenderSettings.ptDevice              = ptAppData->ptDevice;
    tRenderSettings.ptSwapchain           = gptStarter->get_swapchain();
    gptRenderer->initialize(&tRenderSettings);

    plAssetInit tAssetInit = {0};
    gptAsset->initialize(tAssetInit);
    gptTexture->register_asset_types();
    gptMaterial->register_asset_types();
    gptAnimation->register_asset_types();
    gptMesh->register_asset_types();
    gptSkeleton->register_asset_types();
    gptEcs->register_asset_types();
    gptTerrain->register_asset_types();
    gptRenderer->register_asset_types();
    gptAsset->finalize();

    // initialize ecs component library
    plEcsInit tEcsInit = {0};
    gptEcs->initialize(tEcsInit);
    gptIk->register_ecs_components();
    gptSkeleton->register_ecs_components();
    gptTransform->register_ecs_components();
    gptRenderer->register_ecs_components();
    gptScript->register_ecs_components();
    gptAnimation->register_ecs_components();
    gptCameraEcs->register_ecs_components();
    gptPhysics->register_ecs_components();
    gptEcs->finalize();

    if(!gptVfs->does_file_exist("/assets/models/humanoid_Scene.plscene"))
    {
        gptStl->import("/resources/cube.stl");
        gptGltf->import("/resources/DamagedHelmet.glb", NULL);
        gptGltf->import("/resources/humanoid.gltf", NULL);
    }

    // gptGltf->import("/resources/gltf-samples/Models/Sponza/glTF/sponza.gltf", NULL);

    // animations
    // gptGltf->import("/resources/gltf-samples/Models/InterpolationTest/glTF/InterpolationTest.gltf", NULL);
    // gptGltf->import("/resources/gltf-samples/Models/CesiumMan/glTF/CesiumMan.gltf", NULL);
    // gptGltf->import("/resources/gltf-samples/Models/BrainStem/glTF/BrainStem.gltf", NULL); // broke
    // gptGltf->import("/resources/gltf-samples/Models/CommercialRefrigerator/glTF/CommercialRefrigerator.gltf", NULL);

    // anisotropy
    // gptGltf->import("/resources/gltf-samples/Models/AnisotropyBarnLamp/glTF/AnisotropyBarnLamp.gltf", NULL);
    // gptGltf->import("/resources/gltf-samples/Models/AnisotropyDiscTest/glTF/AnisotropyDiscTest.gltf", NULL);
    // gptGltf->import("/resources/gltf-samples/Models/AnisotropyStrengthTest/glTF/AnisotropyStrengthTest.gltf", NULL); // broke
    // gptGltf->import("/resources/gltf-samples/Models/AnisotropyRotationTest/glTF/AnisotropyRotationTest.gltf", NULL);
    
    // basic tests
    // gptGltf->import("/resources/gltf-samples/Models/AlphaBLendModeTest/glTF/AlphaBLendModeTest.gltf", NULL);
    // gptGltf->import("/resources/gltf-samples/Models/OrientationTest/glTF/OrientationTest.gltf", NULL);
    // gptGltf->import("/resources/gltf-samples/Models/EnvironmentTest/glTF/EnvironmentTest.gltf", NULL);

    // car
    // gptGltf->import("/resources/gltf-samples/Models/CarConcept/glTF/CarConcept.gltf", NULL);
    
    // chess
    // gptGltf->import("/resources/gltf-samples/Models/ABeautifulGame/glTF/ABeautifulGame.gltf", NULL);

    // clear coat
    // gptGltf->import("/resources/gltf-samples/Models/ClearCoatWicker/glTF/ClearCoatWicker.gltf", NULL);

    plToolsInit tToolsInit = {
        .ptDevice = ptAppData->ptDevice
    };
    gptTools->initialize(tToolsInit);

    // retrieve some console variables
    ptAppData->pbShowLogging              = (bool*)gptConsole->get_variable("t.LogTool", NULL, NULL);
    ptAppData->pbShowStats                = (bool*)gptConsole->get_variable("t.StatTool", NULL, NULL);
    ptAppData->pbShowProfiling            = (bool*)gptConsole->get_variable("t.ProfileTool", NULL, NULL);
    ptAppData->pbShowMemoryAllocations    = (bool*)gptConsole->get_variable("t.MemoryAllocationTool", NULL, NULL);
    ptAppData->pbShowDeviceMemoryAnalyzer = (bool*)gptConsole->get_variable("t.DeviceMemoryAnalyzerTool", NULL, NULL);
    ptAppData->pbShowAssets               = (bool*)gptConsole->get_variable("t.Assets", NULL, NULL);
    ptAppData->pbShowRenderer             = (bool*)gptConsole->get_variable("t.Renderer", NULL, NULL);

    *ptAppData->pbShowLogging = gptConfig->load_bool("pbShowLogging", *ptAppData->pbShowLogging);
    *ptAppData->pbShowStats = gptConfig->load_bool("pbShowStats", *ptAppData->pbShowStats);
    *ptAppData->pbShowProfiling = gptConfig->load_bool("pbShowProfiling", *ptAppData->pbShowProfiling);
    *ptAppData->pbShowMemoryAllocations = gptConfig->load_bool("pbShowMemoryAllocations", *ptAppData->pbShowMemoryAllocations);
    *ptAppData->pbShowDeviceMemoryAnalyzer = gptConfig->load_bool("pbShowDeviceMemoryAnalyzer", *ptAppData->pbShowDeviceMemoryAnalyzer);
    *ptAppData->pbShowAssets = gptConfig->load_bool("pbShowAssets", *ptAppData->pbShowAssets);
    *ptAppData->pbShowRenderer = gptConfig->load_bool("pbShowRenderer", *ptAppData->pbShowRenderer);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~setup draw extensions~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // create fonts
    plFontRange tFontRange = PL_ZERO_INIT;
    tFontRange.iFirstCodePoint = 0x0020;
    tFontRange.uCharCount = 0x00FF - 0x0020;

    plFontConfig tFontConfig0 = PL_ZERO_INIT;
    tFontConfig0.bSdf = false;
    tFontConfig0.fSize = 16.0f;
    tFontConfig0.uHOverSampling = 1;
    tFontConfig0.uVOverSampling = 1;
    tFontConfig0.ptRanges = &tFontRange;
    tFontConfig0.uRangeCount = 1;
    ptAppData->tDefaultFont = gptDraw->add_font_from_file_ttf(gptDraw->get_current_font_atlas(), tFontConfig0, "/resources/Cousine-Regular.ttf");

    plFontRange tIconRange = PL_ZERO_INIT;
    tIconRange.iFirstCodePoint = ICON_MIN_FA;
    tIconRange.uCharCount = ICON_MAX_16_FA - ICON_MIN_FA;

    plFontConfig tFontConfig1 = PL_ZERO_INIT;
    tFontConfig1.bSdf           = false;
    tFontConfig1.fSize          = 16.0f;
    tFontConfig1.uHOverSampling = 1;
    tFontConfig1.uVOverSampling = 1;
    tFontConfig1.ptMergeFont    = ptAppData->tDefaultFont;
    tFontConfig1.ptRanges       = &tIconRange;
    tFontConfig1.uRangeCount    = 1;
    gptDraw->add_font_from_file_ttf(gptDraw->get_current_font_atlas(), tFontConfig1, "/resources/fa-solid-900.otf");
    gptStarter->set_default_font(ptAppData->tDefaultFont);
    gptUI->set_default_font(ptAppData->tDefaultFont);

    gptStarter->finalize();

    pl__load_assets(ptAppData);
    pl__refresh_files(ptAppData);

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
    pl_sb_free(ptAppData->sbcTempBuffer);
    pl_sb_free(ptAppData->sbtSceneFilesCore);

    // ensure GPU is finished before cleanup
    gptGfx->flush_device(ptAppData->ptDevice);

    gptConfig->set_bool("pbShowRenderer", *ptAppData->pbShowRenderer);
    gptConfig->set_bool("pbShowAssets", *ptAppData->pbShowAssets);
    gptConfig->set_bool("pbShowLogging", *ptAppData->pbShowLogging);
    gptConfig->set_bool("pbShowStats", *ptAppData->pbShowStats);
    gptConfig->set_bool("pbShowProfiling", *ptAppData->pbShowProfiling);
    gptConfig->set_bool("pbShowMemoryAllocations", *ptAppData->pbShowMemoryAllocations);
    gptConfig->set_bool("pbShowDeviceMemoryAnalyzer", *ptAppData->pbShowDeviceMemoryAnalyzer);

    gptConfig->save_to_disk(NULL);
    gptConfig->cleanup();
    gptAssetTools->cleanup();
    gptPhysics->cleanup();

    if(ptAppData->ptScene)
    {
        gptRenderer->destroy_view(ptAppData->ptView);
        gptRenderer->destroy_scene(ptAppData->ptScene);
    }

    gptAsset->cleanup();
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
    
    // for convience
    plIO* ptIO = gptIO->get_io();

    gptRenderer->begin_frame();

    if(ptAppData->bResize)
    {
        if(ptAppData->ptScene)
            gptRenderer->resize_view(ptAppData->ptView, ptIO->tMainViewportSize);
        ptAppData->bResize = false;
    }

    // update statistics
    gptShaderVariant->update_stats();

    if(ptAppData->ptScene)
    {
        plComponentLibrary* ptLibrary = (plComponentLibrary*)gptAsset->get_data(ptAppData->tSceneHandle);
        plCamera* ptCamera = (plCamera*)gptEcs->get_component(ptLibrary, gptCameraEcs->get_ecs_type_key(), ptAppData->tMainCamera);

        gptCamera->set_viewport(ptCamera, ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y);

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

                if(tReleasePos.x == tClickPos.x && tReleasePos.y == tClickPos.y)
                    gptRenderer->update_hovered_entity(ptAppData->ptView, (plVec2){0}, (plVec2){1.0f, 1.0f});
            }
        }

        // run ecs system
        PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, "Run ECS");
        gptScript->run_update_system(ptLibrary);
        gptAnimation->run_animation_update_system(ptLibrary, ptIO->fDeltaTime);
        plDrawList3D* ptDrawlist = gptRenderer->get_drawlist(ptAppData->ptView);
        gptPhysics->update(ptIO->fDeltaTime, ptLibrary, ptDrawlist);
        gptTransform->run_transform_update_system(ptLibrary);
        gptTransform->run_hierarchy_update_system(ptLibrary);
        gptRenderer->run_light_update_system(ptLibrary);
        gptCameraEcs->run_ecs(ptLibrary);
        gptIk->run_ecs_update_system(ptLibrary);
        gptSkeleton->run_skin_update_system(ptLibrary);
        gptRenderer->run_object_update_system(ptLibrary);
        gptRenderer->run_environment_probe_update_system(ptLibrary); // run after object update
        PL_PROFILE_END_SAMPLE_API(gptProfile, 0);

        plEntity tNextEntity = {0};
        if(gptRenderer->get_hovered_entity(ptAppData->ptView, &tNextEntity))
        {
            
            if(tNextEntity.uData == 0)
            {
                ptAppData->tSelectedEntity.uData = UINT64_MAX;
                gptRenderer->outline_entities(ptAppData->ptScene, 0, NULL);
                gptScreenLog->add_message_ex(565168477883, 1.0, PL_COLOR_32_RED, 1.0f, "Unselected Entity");
            }
            else if(ptAppData->tSelectedEntity.uData != tNextEntity.uData)
            {
                plTagComponent* ptSelectedTag = gptEcs->get_component(ptLibrary, gptEcs->get_ecs_type_key_tag(), tNextEntity);
                gptScreenLog->add_message_ex(565168477883, -1.0, PL_COLOR_32_GREEN, 1.0f, "Selected Entity \"%s\" {%u, %u}", ptSelectedTag ? ptSelectedTag->pcName : "No Name", tNextEntity.uIndex, tNextEntity.uGeneration);
                gptRenderer->outline_entities(ptAppData->ptScene, 1, &tNextEntity);
                ptAppData->tSelectedEntity = tNextEntity;
                gptPhysics->set_angular_velocity(ptLibrary, tNextEntity, pl_create_vec3(0, 0, 0));
                gptPhysics->set_linear_velocity(ptLibrary, tNextEntity, pl_create_vec3(0, 0, 0));
            }

        }

        if(gptIO->is_key_pressed(PL_KEY_M, true))
            gptGizmo->next_mode();

        gptAssetTools->run();

        if(gptEcs->is_entity_valid(ptLibrary, ptAppData->tSelectedEntity))
        {
            plDrawList3D* ptGizmoDrawlist =  gptRenderer->get_gizmo_drawlist(ptAppData->ptView);
            plObjectComponent* ptSelectedObject = (plObjectComponent*)gptEcs->get_component(ptLibrary, gptRenderer->get_ecs_type_key_object(), ptAppData->tSelectedEntity);
            plTransformComponent* ptSelectedTransform = (plTransformComponent*)gptEcs->get_component(ptLibrary, gptTransform->get_ecs_type_key_transform(), ptAppData->tSelectedEntity);
            plTransformComponent* ptParentTransform = NULL;
            plHierarchyComponent* ptHierarchyComp = (plHierarchyComponent*)gptEcs->get_component(ptLibrary, gptTransform->get_ecs_type_key_hierarchy(), ptAppData->tSelectedEntity);
            if(ptHierarchyComp)
            {
                ptParentTransform = (plTransformComponent*)gptEcs->get_component(ptLibrary, gptTransform->get_ecs_type_key_transform(), ptHierarchyComp->tParent);
            }
            if(ptSelectedTransform)
            {
                gptGizmo->gizmo(ptGizmoDrawlist, ptCamera, ptSelectedTransform, ptParentTransform, (plVec2){0}, (plVec2){1.0f, 1.0f});
            }
            else if(ptSelectedObject)
            {
                ptSelectedTransform = (plTransformComponent*)gptEcs->get_component(ptLibrary, gptTransform->get_ecs_type_key_transform(), ptSelectedObject->tTransform);
                gptGizmo->gizmo(ptGizmoDrawlist, ptCamera, ptSelectedTransform, ptParentTransform, (plVec2){0}, (plVec2){1.0f, 1.0f});
            }
        }

        // render scene
        const plCamera* atCameras[] = {ptCamera}; //ptSecondaryCamera};
        gptRenderer->prepare_scene(ptAppData->ptScene, atCameras, 1);
        
        // single view
        plRenderViewDesc tViewDesc0 = {
            .ptCamera     = ptCamera
        };
        gptRenderer->prepare_view(ptAppData->ptView, ptCamera);

        gptRenderer->render_debug_view(ptAppData->ptView, &tViewDesc0);
        gptRenderer->render_view(ptAppData->ptView, &tViewDesc0);
    }

    if(ptAppData->ptScene)
    {
        plVec2 tStartPos = {0};
        plVec2 tEndPos = ptIO->tMainViewportSize;
        plVec2 tUV = {0};
        plBindGroupHandle tTexture = gptRenderer->get_view_color_bind_group(ptAppData->ptView, &tUV);
        gptDraw->add_image_ex(ptAppData->ptDrawLayer, tTexture.uData, tStartPos, tEndPos, (plVec2){0}, tUV, PL_COLOR_32_WHITE);
    }

    // ui windows
    if(ptAppData->ptScene == NULL)
    {
        
        gptUI->push_theme_color(PL_UI_COLOR_WINDOW_BG, (plVec4){0});
        gptUI->push_theme_color(PL_UI_COLOR_TITLE_BG, (plVec4){0});
        gptUI->push_theme_color(PL_UI_COLOR_TITLE_ACTIVE, (plVec4){0});
        gptUI->set_next_window_pos((plVec2){ptIO->tMainViewportSize.x * 0.4f, ptIO->tMainViewportSize.y * 0.25f}, PL_UI_COND_ALWAYS);
        gptUI->set_next_window_size((plVec2){ptIO->tMainViewportSize.x * 0.2f, ptIO->tMainViewportSize.y * 0.5f}, PL_UI_COND_ALWAYS);
        if(gptUI->begin_window("Select Scene", NULL, PL_UI_WINDOW_FLAGS_NO_MOVE | PL_UI_WINDOW_FLAGS_NO_RESIZE | PL_UI_WINDOW_FLAGS_NO_COLLAPSE))
        { 
            gptUI->layout_static(0.0f, 100.0f, 1);
            if(gptUI->button("Refresh"))
                pl__refresh_files(ptAppData);

            gptUI->layout_dynamic(ptIO->tMainViewportSize.y * 0.4f, 1);

            if(gptUI->begin_child("Scenes", 0, 0))
            {
                uint32_t uSceneCount = pl_sb_size(ptAppData->sbtSceneFilesCore);

                gptUI->layout_dynamic(0.0f, 1);

                for (uint32_t n = 0; n < uSceneCount; n++)
                {
                    bool bPlaceHolder = false;
                    if(gptUI->selectable(ptAppData->sbtSceneFilesCore[n].acName, &bPlaceHolder, 0))
                    {
                        pl_sprintf(ptAppData->acCurrentScene, "%s", ptAppData->sbtSceneFilesCore[n].acTemplate);

                        ptAppData->tSceneHandle = gptAsset->load(ptAppData->acCurrentScene);
                        plComponentLibrary* ptLibrary = (plComponentLibrary*)gptAsset->get_data(ptAppData->tSceneHandle);

                        plSceneDesc tSceneInit = {0};

                        ptAppData->ptScene = gptRenderer->create_scene(&tSceneInit);
                        plViewDesc tViewDesc = PL_ZERO_INIT;
                        tViewDesc.uWidth = (uint32_t)ptIO->tMainViewportSize.x;
                        tViewDesc.uHeight = (uint32_t)ptIO->tMainViewportSize.y;
                        ptAppData->ptView = gptRenderer->create_view(ptAppData->ptScene, &tViewDesc);
                        
                        gptRenderer->load_component_library(ptAppData->ptScene, ptLibrary);

                        const plEntity* ptCameraEntities = NULL;
                        uint32_t uCameraCount = gptEcs->get_components(ptLibrary, gptCameraEcs->get_ecs_type_key(), NULL, &ptCameraEntities);
                        if(uCameraCount > 0)
                            ptAppData->tMainCamera = ptCameraEntities[0];
                    }
                }
                gptUI->end_child();
            }
            gptUI->end_window();
        }
        gptUI->pop_theme_color(3);
    }
    else
    {
        plComponentLibrary* ptLibrary = (plComponentLibrary*)gptAsset->get_data(ptAppData->tSceneHandle);

        plUiWindowFlags tWindowFlags = PL_UI_WINDOW_FLAGS_NONE;

        float fWidth = ptIO->tMainViewportSize.x * 0.5f;
        fWidth = pl_clampf(150.0f, fWidth, 500.0f);
        gptUI->set_next_window_pos((plVec2){0.0f, 0.0f}, PL_UI_COND_ALWAYS);
        gptUI->set_next_window_size((plVec2){fWidth, 650.0f}, PL_UI_COND_ALWAYS);
        tWindowFlags= PL_UI_WINDOW_FLAGS_NO_MOVE | PL_UI_WINDOW_FLAGS_NO_RESIZE | PL_UI_WINDOW_FLAGS_NO_COLLAPSE | PL_UI_WINDOW_FLAGS_NO_TITLE_BAR;

        gptUI->push_theme_color(PL_UI_COLOR_WINDOW_BG, (plVec4){0});
        if(gptUI->begin_window("Pilot Light", NULL, tWindowFlags))
        {
            gptUI->layout_dynamic(0.0f, 1);

            gptUI->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
            gptUI->text("Graphics Backend: %s", gptGfx->get_backend_string());

            gptUI->separator_text("Controls");
            gptUI->text("* F1 - bring up console");
            gptUI->text("* M  - change gizmo mode");
            gptUI->separator_text("Camera Controls");
            gptUI->text("* LMB + Drag: Moves camera forward & backward and rotates left & right.");
            gptUI->text("* RMB + Drag: Rotates camera.");
            gptUI->text("* LMB + RMB + Drag: Pans Camera");
            gptUI->text("Mouse Wheel: Speed");
            gptUI->vertical_spacing();
            gptUI->text("Game style (when holding RMB)");
            gptUI->separator();
            gptUI->text("* W    Moves the camera forward.");
            gptUI->text("* S    Moves the camera backward.");
            gptUI->text("* A    Moves the camera left.");
            gptUI->text("* D    Moves the camera right.");
            gptUI->text("* E    Moves the camera up.");
            gptUI->text("* Q    Moves the camera down.");
            gptUI->text("* Z    Zooms the camera out (raises FOV).");
            gptUI->text("* C    Zooms the camera in (lowers FOV).");

            static char acEntityIdBuffer[64] = {0};
            if(gptUI->input_text("Select Entity", acEntityIdBuffer, 64, PL_UI_INPUT_TEXT_FLAGS_CHARS_DECIMAL | PL_UI_INPUT_TEXT_FLAGS_ENTER_RETURNS_TRUE))
            {
                ptAppData->tSelectedEntityId = (uint64_t)strtoull(acEntityIdBuffer, NULL, 0);
                plEntity tNextEntity = gptEcs->get_entity_by_id(ptLibrary, ptAppData->tSelectedEntityId);
                if(gptEcs->is_entity_valid(ptLibrary, tNextEntity))
                {
                    ptAppData->tSelectedEntity = tNextEntity;
                    plTagComponent* ptSelectedTag = gptEcs->get_component(ptLibrary, gptEcs->get_ecs_type_key_tag(), ptAppData->tSelectedEntity);
                    gptScreenLog->add_message_ex(565168477883, -1.0, PL_COLOR_32_GREEN, 1.0f, "Selected Entity \"%s\" {%u, %u}", ptSelectedTag ? ptSelectedTag->pcName : "No Name", ptAppData->tSelectedEntity.uIndex, ptAppData->tSelectedEntity.uGeneration);
                    gptRenderer->outline_entities(ptAppData->ptScene, 1, &ptAppData->tSelectedEntity);
                    gptPhysics->set_angular_velocity(ptLibrary, ptAppData->tSelectedEntity, pl_create_vec3(0, 0, 0));
                    gptPhysics->set_linear_velocity(ptLibrary, ptAppData->tSelectedEntity, pl_create_vec3(0, 0, 0));
                }
                else
                {
                    gptScreenLog->add_message_ex(565168477884, 2.0, PL_COLOR_32_RED, 1.0f, "Invalid Entity Id: %" PRIu64, ptAppData->tSelectedEntityId);
                    memset(acEntityIdBuffer, 0, 64);
                }
            }

            gptUI->layout_static(0.0f, 100.0f, 1);
            if(gptUI->button("Unload"))
            {
                gptPhysics->reset();
                gptRenderer->destroy_view(ptAppData->ptView);
                gptRenderer->destroy_scene(ptAppData->ptScene);
                ptAppData->ptView = NULL;
                ptAppData->ptScene = NULL;
                gptAsset->destroy(ptAppData->tSceneHandle);
                pl__refresh_files(ptAppData);
                gptScreenLog->add_message_ex(565168477883, 2.0, PL_COLOR_32_RED, 0.1f, "reset");
            }

            gptUI->end_window();
        }
        gptUI->pop_theme_color(1);
    }

    if(ptAppData->bShowUiDemo)
        pl__show_ui_demo_window(ptAppData);

    gptDraw->submit_2d_layer(ptAppData->ptDrawLayer);

    plCommandBuffer* ptCommandBuffer = gptStarter->begin_main_pass();

    float fWidth = ptIO->tMainViewportSize.x;
    float fHeight = ptIO->tMainViewportSize.y;
    plRenderAttachmentInfo tRenderInfo = {0};
    gptStarter->get_render_attachment_info(&tRenderInfo);
    gptStarter->end_main_pass();
    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
    gptStarter->end_frame();
}

//-----------------------------------------------------------------------------
// [SECTION] helper implementations
//-----------------------------------------------------------------------------

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
    gptGltf             = pl_get_api_latest(ptApiRegistry, plGltfI);
    gptDraw             = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptUI               = pl_get_api_latest(ptApiRegistry, plUiI);
    gptIO               = pl_get_api_latest(ptApiRegistry, plIOI);
    gptShader           = pl_get_api_latest(ptApiRegistry, plShaderI);
    gptMemory           = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptNetwork          = pl_get_api_latest(ptApiRegistry, plNetworkI);
    gptString           = pl_get_api_latest(ptApiRegistry, plStringInternI);
    gptProfile          = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptFile             = pl_get_api_latest(ptApiRegistry, plFileI);
    gptAssetTools       = pl_get_api_latest(ptApiRegistry, plAssetToolsI);
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
    gptAsset            = pl_get_api_latest(ptApiRegistry, plAssetI);
    gptTransform        = pl_get_api_latest(ptApiRegistry, plTransformI);
    gptIk               = pl_get_api_latest(ptApiRegistry, plIkI);
    gptSkeleton         = pl_get_api_latest(ptApiRegistry, plSkeletonI);
    gptTexture          = pl_get_api_latest(ptApiRegistry, plTextureI);
    gptTerrain          = pl_get_api_latest(ptApiRegistry, plTerrainI);
    gptStl              = pl_get_api_latest(ptApiRegistry, plStlI);
}

void
pl__refresh_files(plAppData* ptAppData)
{
    pl_sb_reset(ptAppData->sbtSceneFilesCore);

    // local
    {
        plDirectoryInfo tDirectoryInfo = {0};
        gptFile->get_directory_info("../assets/scenes/", &tDirectoryInfo);
        pl_sb_reserve(ptAppData->sbtSceneFilesCore, tDirectoryInfo.uFileCount);
        for(uint32_t i = 0; i < tDirectoryInfo.uFileCount; i++)
        {
            if(tDirectoryInfo.sbtEntries[i].eType == PL_DIRECTORY_ENTRY_TYPE_FILE)
            {
                char acExtensionBuffer[16] = {0};
                char acFileNameOnly[PL_MAX_PATH_LENGTH] = {0};
                pl_str_get_file_extension(tDirectoryInfo.sbtEntries[i].acName, acExtensionBuffer, 16);
                if(pl_str_equal("plscene", acExtensionBuffer))
                {
                    pl_str_get_file_name_only(tDirectoryInfo.sbtEntries[i].acName, acFileNameOnly, 128);
                    char acCurrentScene[PL_MAX_PATH_LENGTH] = {0};
                    char acFullPath[PL_MAX_PATH_LENGTH] = {0};
                    pl_sprintf(acCurrentScene, "/assets/scenes/%s.plscene", acFileNameOnly);
                    pl_sprintf(acFullPath, "/assets/scenes/%s.plscene", acFileNameOnly);
                    // if(pl__verify_scene(ptAppData, acCurrentScene))
                    {
                        pl_sb_add(ptAppData->sbtSceneFilesCore);
                        strncpy(pl_sb_back(ptAppData->sbtSceneFilesCore).acName, acFileNameOnly, PL_MAX_PATH_LENGTH);
                        strncpy(pl_sb_back(ptAppData->sbtSceneFilesCore).acTemplate, acFullPath, PL_MAX_PATH_LENGTH);
                    }
                }
            }
        }
        gptFile->cleanup_directory_info(&tDirectoryInfo);
    }
}

void
pl__load_assets(plAppData* ptAppData)
{
    char* sbcBuffer = NULL;

    {
        plDirectoryInfo tDirectoryInfo = {0};
        gptFile->get_directory_info("../assets/textures/", &tDirectoryInfo);
        for(uint32_t i = 0; i < tDirectoryInfo.uFileCount; i++)
        {
            if(tDirectoryInfo.sbtEntries[i].eType == PL_DIRECTORY_ENTRY_TYPE_FILE)
            {
                pl_sb_reset(sbcBuffer);
                pl_sb_sprintf(sbcBuffer, "/assets/textures/%s", tDirectoryInfo.sbtEntries[i].acName);
                gptAsset->load(sbcBuffer);
            }
        }
        gptFile->cleanup_directory_info(&tDirectoryInfo);
    }

    {
        plDirectoryInfo tDirectoryInfo = {0};
        gptFile->get_directory_info("../assets/materials/", &tDirectoryInfo);
        for(uint32_t i = 0; i < tDirectoryInfo.uFileCount; i++)
        {
            if(tDirectoryInfo.sbtEntries[i].eType == PL_DIRECTORY_ENTRY_TYPE_FILE)
            {
                pl_sb_reset(sbcBuffer);
                pl_sb_sprintf(sbcBuffer, "/assets/materials/%s", tDirectoryInfo.sbtEntries[i].acName);
                gptAsset->load(sbcBuffer);
            }
        }
        gptFile->cleanup_directory_info(&tDirectoryInfo);
    }

    {
        plDirectoryInfo tDirectoryInfo = {0};
        gptFile->get_directory_info("../assets/meshes/", &tDirectoryInfo);
        for(uint32_t i = 0; i < tDirectoryInfo.uFileCount; i++)
        {
            if(tDirectoryInfo.sbtEntries[i].eType == PL_DIRECTORY_ENTRY_TYPE_FILE)
            {
                pl_sb_reset(sbcBuffer);
                pl_sb_sprintf(sbcBuffer, "/assets/meshes/%s", tDirectoryInfo.sbtEntries[i].acName);
                gptAsset->load(sbcBuffer);
            }
        }
        gptFile->cleanup_directory_info(&tDirectoryInfo);
    }

    {
        plDirectoryInfo tDirectoryInfo = {0};
        gptFile->get_directory_info("../assets/animations/", &tDirectoryInfo);
        for(uint32_t i = 0; i < tDirectoryInfo.uFileCount; i++)
        {
            if(tDirectoryInfo.sbtEntries[i].eType == PL_DIRECTORY_ENTRY_TYPE_FILE)
            {
                pl_sb_reset(sbcBuffer);
                pl_sb_sprintf(sbcBuffer, "/assets/animations/%s", tDirectoryInfo.sbtEntries[i].acName);
                gptAsset->load(sbcBuffer);
            }
        }
        gptFile->cleanup_directory_info(&tDirectoryInfo);
    }

    {
        plDirectoryInfo tDirectoryInfo = {0};
        gptFile->get_directory_info("../assets/environments/", &tDirectoryInfo);
        for(uint32_t i = 0; i < tDirectoryInfo.uFileCount; i++)
        {
            if(tDirectoryInfo.sbtEntries[i].eType == PL_DIRECTORY_ENTRY_TYPE_FILE)
            {
                pl_sb_reset(sbcBuffer);
                pl_sb_sprintf(sbcBuffer, "/assets/environments/%s", tDirectoryInfo.sbtEntries[i].acName);
                gptAsset->load(sbcBuffer);
            }
        }
        gptFile->cleanup_directory_info(&tDirectoryInfo);
    }

    {
        plDirectoryInfo tDirectoryInfo = {0};
        gptFile->get_directory_info("../assets/skeletons/", &tDirectoryInfo);
        for(uint32_t i = 0; i < tDirectoryInfo.uFileCount; i++)
        {
            if(tDirectoryInfo.sbtEntries[i].eType == PL_DIRECTORY_ENTRY_TYPE_FILE)
            {
                pl_sb_reset(sbcBuffer);
                pl_sb_sprintf(sbcBuffer, "/assets/skeletons/%s", tDirectoryInfo.sbtEntries[i].acName);
                gptAsset->load(sbcBuffer);
            }
        }
        gptFile->cleanup_directory_info(&tDirectoryInfo);
    }

    {
        plDirectoryInfo tDirectoryInfo = {0};
        gptFile->get_directory_info("../assets/skins/", &tDirectoryInfo);
        for(uint32_t i = 0; i < tDirectoryInfo.uFileCount; i++)
        {
            if(tDirectoryInfo.sbtEntries[i].eType == PL_DIRECTORY_ENTRY_TYPE_FILE)
            {
                pl_sb_reset(sbcBuffer);
                pl_sb_sprintf(sbcBuffer, "/assets/skins/%s", tDirectoryInfo.sbtEntries[i].acName);
                gptAsset->load(sbcBuffer);
            }
        }
        gptFile->cleanup_directory_info(&tDirectoryInfo);
    }

    {
        plDirectoryInfo tDirectoryInfo = {0};
        gptFile->get_directory_info("../assets/terrains/", &tDirectoryInfo);
        for(uint32_t i = 0; i < tDirectoryInfo.uFileCount; i++)
        {
            if(tDirectoryInfo.sbtEntries[i].eType == PL_DIRECTORY_ENTRY_TYPE_FILE)
            {
                pl_sb_reset(sbcBuffer);
                pl_sb_sprintf(sbcBuffer, "/assets/terrains/%s", tDirectoryInfo.sbtEntries[i].acName);
                gptAsset->load(sbcBuffer);
            }
        }
        gptFile->cleanup_directory_info(&tDirectoryInfo);
    }

    {
        plDirectoryInfo tDirectoryInfo = {0};
        gptFile->get_directory_info("../assets/scenes/", &tDirectoryInfo);
        for(uint32_t i = 0; i < tDirectoryInfo.uFileCount; i++)
        {
            if(tDirectoryInfo.sbtEntries[i].eType == PL_DIRECTORY_ENTRY_TYPE_FILE)
            {
                pl_sb_reset(sbcBuffer);
                pl_sb_sprintf(sbcBuffer, "/assets/scenes/%s", tDirectoryInfo.sbtEntries[i].acName);
                gptAsset->load(sbcBuffer);
            }
        }
        gptFile->cleanup_directory_info(&tDirectoryInfo);
    }

    pl_sb_free(sbcBuffer);
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#include "pl__ui_demo.cpp"

#ifdef PL_USE_STB_SPRINTF
    #define STB_SPRINTF_IMPLEMENTATION
    #include "stb_sprintf.h"
    #undef STB_SPRINTF_IMPLEMENTATION
#endif

#define PL_STRING_IMPLEMENTATION
#include "pl_string.h"
