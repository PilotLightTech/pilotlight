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
#include "pl_shader_library_ext.h"
#include "pl_material_ext.h"
#include "pl_script_ext.h"
#include "pl_asset_ext.h"
#include "pl_ik_ext.h"
#include "pl_skeleton_ext.h"
#include "pl_texture_ext.h"
#include "pl_terrain_ext.h"
#include "pl_stl_ext.h"
#include "pl_dialog_ext.h"

// shaders
#include "pl_shader_interop_renderer.h" // PL_MESH_FORMAT_FLAG_XXXX

//-----------------------------------------------------------------------------
// [SECTION] global apis
//-----------------------------------------------------------------------------

const plWindowI*            gptWindows          = NULL;
const plStatsI*             gptStats            = NULL;
const plLogI*               gptLog              = NULL;
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
const plShaderLibraryI*     gptShaderLibrary    = NULL;
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
const plDialogI*            gptDialog           = NULL;

static uint64_t guLogChannel;
static bool gbLogFileErrorReported;

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

static const char* gacImportFiles[] = {
    "/resources/DamagedHelmet.glb",
    "/resources/humanoid.gltf",
    // "/resources/gltf-samples/Models/Sponza/glTF/sponza.gltf",
    // "/resources/gltf-samples/Models/ABeautifulGame/glTF/ABeautifulGame.gltf",
    // "/resources/gltf-samples/Models/ClearCoatWicker/glTF/ClearCoatWicker.gltf",
    // "/resources/gltf-samples/Models/CarConcept/glTF/CarConcept.gltf",
    // "/resources/gltf-samples/Models/AlphaBLendModeTest/glTF/AlphaBLendModeTest.gltf",
    // "/resources/gltf-samples/Models/CommercialRefrigerator/glTF/CommercialRefrigerator.gltf",
    // "/resources/gltf-samples/Models/InterpolationTest/glTF/InterpolationTest.gltf",
    // "/resources/gltf-samples/Models/AnisotropyDiscTest/glTF/AnisotropyDiscTest.gltf",
    // "/resources/gltf-samples/Models/AnisotropyRotationTest/glTF/AnisotropyRotationTest.gltf",
    // "/resources/gltf-samples/Models/AnisotropyBarnLamp/glTF/AnisotropyBarnLamp.gltf",
};

typedef struct _plSandboxSceneFile
{
    char acName[PL_MAX_PATH_LENGTH];
    char acTemplate[PL_MAX_PATH_LENGTH];
} plSandboxSceneFile;

typedef struct _plAppData
{
    // windows
    plWindow* ptWindow;
    plWindowTitleBarDesc tTitleBar;
    bool bLoading;

    // graphics
    plDevice*    ptDevice;
    plDeviceInfo tDeviceInfo;

    // docked scene viewport
    uint32_t uSceneDock;
    uint32_t uToolsDock;
    bool bDockLayoutInitialized;
    bool bGameMode;
    plVec2 tViewOffset;
    plVec2 tViewSize;
    plVec2 tRenderSize;
    bool bSceneClickStarted;
    plVec2 tSceneClickPos;

    // optional second viewport (camera pose is independent of the scene asset)
    bool bShowSecondaryView;
    bool bSecondaryDockInitialized;
    bool bSecondaryHovered;
    uint32_t uSecondaryDock;
    plView* ptSecondaryView;
    plCamera tSecondaryCamera;
    plVec2 tSecondaryOffset;
    plVec2 tSecondarySize;
    float fSecondarySpeed;

    // ui options
    // bool bContinuousBVH;

    // pilot light ui windows
    bool  bShowUiDemo;
    bool* pbShowDeviceMemoryAnalyzer;
    bool* pbShowMemoryAllocations;
    bool* pbShowProfiling;
    bool* pbShowGpuProfiling;
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
    bool bShowImportOptions;

    // selection stuff
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

void         pl__show_ui_demo_window(plAppData*);
void         pl__load_apis(plApiRegistryI*);
static void  pl__initialize_logging(void);
void         pl__refresh_files(plAppData*);
void         pl__load_assets(plAppData*);
static void  pl__update_title_bar(plAppData*);
static void  pl__set_loading(plAppData*, bool bLoading);
static float pl__show_main_menu(plAppData*);
static bool  pl__show_secondary_view(plAppData*);
static void  pl__update_secondary_camera(plAppData*);
static void  pl__show_sandbox_tools(plAppData*);

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
        pl__initialize_logging();
        pl__update_title_bar(ptAppData);
        gptLog->info(guLogChannel, "App hot reloaded");

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
    pl__initialize_logging();

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

    // create sandbox window
    gptLog->info(guLogChannel, "Creating sandbox window and initializing graphics");
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

    const plWindowCapabilities* ptWindowCapabilities = gptWindows->get_capabilities();
    if(ptWindowCapabilities->tFlags & (PL_WINDOW_CAPABILITY_FLAGS_WINDOW_ICON | PL_WINDOW_CAPABILITY_FLAGS_APP_ICON))
    {
        const plImageI* ptImage = pl_get_api_latest(ptApiRegistry, plImageI);
        const char* apcIconPaths[] = {"/resources/pilot-light-32.png", "/resources/pilot-light-256.png"};
        plWindowIcon atIcons[2] = {0};
        uint32_t uIconCount = 0;
        for(uint32_t i = 0; i < PL_ARRAYSIZE(apcIconPaths); i++)
        {
            int iWidth = 0;
            int iHeight = 0;
            int iChannels = 0;
            unsigned char* pucPixels = ptImage->load_from_file(apcIconPaths[i], &iWidth, &iHeight, &iChannels, 4);
            if(pucPixels)
                atIcons[uIconCount++] = (plWindowIcon){(uint32_t)iWidth, (uint32_t)iHeight, pucPixels};
        }
        if(!uIconCount || !gptWindows->set_icon(ptAppData->ptWindow, uIconCount, atIcons))
            gptLog->warn(guLogChannel, "Unable to set the Pilot Light window icon");
        for(uint32_t i = 0; i < uIconCount; i++)
            ptImage->free((void*)atIcons[i].pucPixels);
    }

    pl__set_loading(ptAppData, true);
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
    gptLog->info_p(guLogChannel, "Graphics ready: %s, device: %s", gptGfx->get_backend_string(), gptGfx->get_device_info(ptAppData->ptDevice)->acName);

    // initialize job system
    plJobSystemInit tJobInit = {0};
    gptJobs->initialize(tJobInit);

    const plShaderLibraryInit tShaderVariantInit = {
        ptAppData->ptDevice
    };
    gptShaderLibrary->initialize(tShaderVariantInit);

    // setup reference renderer
    plRendererInit tRenderSettings = PL_ZERO_INIT;
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

    // Generate missing built-in meshes once; no Python setup is required.
    if(!gptVfs->does_file_exist("/assets/meshes/sphere.plmesh"))
    {
        plMesh tMesh = {0};
        gptMesh->create_sphere(1.0f, 16, 16, &tMesh);
        gptMesh->calculate_bounds(&tMesh);
        tMesh.atSubmeshes[0].tMaterial = gptAsset->load("/assets/materials/default.plmaterial");
        const plAssetDesc tMeshAsset = {
            .tType = gptMesh->get_asset_type_key(),
            .pcPath = "/assets/meshes/sphere.plmesh"
        };
        // Asset creation takes ownership of the mesh allocation.
        plAssetHandle tMeshHandle = gptAsset->create(&tMeshAsset, &tMesh);
        gptAsset->save(tMeshHandle, PL_ASSET_ENCODING_TEXT);
    }

    if(!gptVfs->does_file_exist("/assets/meshes/probe.plmesh"))
    {
        plMesh tMesh = {0};
        gptMesh->create_sphere(0.25f, 32, 32, &tMesh);
        gptMesh->calculate_bounds(&tMesh);
        tMesh.atSubmeshes[0].tMaterial = gptAsset->load("/assets/materials/probe.plmaterial");
        const plAssetDesc tMeshAsset = {
            .tType = gptMesh->get_asset_type_key(),
            .pcPath = "/assets/meshes/probe.plmesh"
        };
        // Asset creation takes ownership of the mesh allocation.
        plAssetHandle tMeshHandle = gptAsset->create(&tMeshAsset, &tMesh);
        gptAsset->save(tMeshHandle, PL_ASSET_ENCODING_TEXT);
    }

    if(!gptVfs->does_file_exist("/assets/meshes/cube.plmesh"))
    {
        plMesh tMesh = {0};
        gptMesh->create_cube(&tMesh);
        gptMesh->calculate_bounds(&tMesh);
        tMesh.atSubmeshes[0].tMaterial = gptAsset->load("/assets/materials/default.plmaterial");
        const plAssetDesc tMeshAsset = {
            .tType = gptMesh->get_asset_type_key(),
            .pcPath = "/assets/meshes/cube.plmesh"
        };
        // Asset creation takes ownership of the mesh allocation.
        plAssetHandle tMeshHandle = gptAsset->create(&tMeshAsset, &tMesh);
        gptAsset->save(tMeshHandle, PL_ASSET_ENCODING_TEXT);
    }

    if(!gptVfs->does_file_exist("/assets/meshes/plane.plmesh"))
    {
        plMesh tMesh = {0};
        gptMesh->create_plane(&tMesh);
        gptMesh->calculate_bounds(&tMesh);
        tMesh.atSubmeshes[0].tMaterial = gptAsset->load("/assets/materials/default.plmaterial");
        const plAssetDesc tMeshAsset = {
            .tType = gptMesh->get_asset_type_key(),
            .pcPath = "/assets/meshes/plane.plmesh"
        };
        // Asset creation takes ownership of the mesh allocation.
        plAssetHandle tMeshHandle = gptAsset->create(&tMeshAsset, &tMesh);
        gptAsset->save(tMeshHandle, PL_ASSET_ENCODING_TEXT);
    }

    if(!gptVfs->does_file_exist("/assets/meshes/water_grid.plmesh"))
    {
        const uint32_t uCells = 256;
        const uint32_t uWidth = uCells + 1;
        plMesh tWaterGrid = {0};
        const plSubmeshAllocationDesc tAllocation = {
            .uVertexStreamMask = PL_MESH_FORMAT_FLAG_HAS_NORMAL | PL_MESH_FORMAT_FLAG_HAS_TANGENT | PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0,
            .szVertexCount = uWidth * uWidth,
            .szIndexCount = uCells * uCells * 6
        };
        gptMesh->allocate(&tWaterGrid, &tAllocation, 1);
        plSubmesh* ptSubmesh = &tWaterGrid.atSubmeshes[0];
        for(uint32_t uZ = 0; uZ < uWidth; uZ++)
        {
            for(uint32_t uX = 0; uX < uWidth; uX++)
            {
                const uint32_t uVertex = uZ * uWidth + uX;
                const float fU = (float)uX / (float)uCells;
                const float fV = (float)uZ / (float)uCells;
                ptSubmesh->ptVertexPositions[uVertex] = (plVec3){fU - 0.5f, 0.0f, fV - 0.5f};
                ptSubmesh->ptVertexNormals[uVertex] = (plVec3){0.0f, 1.0f, 0.0f};
                ptSubmesh->ptVertexTangents[uVertex] = (plVec4){1.0f, 0.0f, 0.0f, -1.0f};
                ptSubmesh->ptVertexTextureCoordinates[0][uVertex] = (plVec2){fU, fV};
            }
        }
        uint32_t uIndex = 0;
        for(uint32_t uZ = 0; uZ < uCells; uZ++)
        {
            for(uint32_t uX = 0; uX < uCells; uX++)
            {
                const uint32_t uVertex = uZ * uWidth + uX;
                ptSubmesh->puIndices[uIndex++] = uVertex;
                ptSubmesh->puIndices[uIndex++] = uVertex + uWidth;
                ptSubmesh->puIndices[uIndex++] = uVertex + 1;
                ptSubmesh->puIndices[uIndex++] = uVertex + 1;
                ptSubmesh->puIndices[uIndex++] = uVertex + uWidth;
                ptSubmesh->puIndices[uIndex++] = uVertex + uWidth + 1;
            }
        }
        gptMesh->calculate_bounds(&tWaterGrid);
        ptSubmesh->tMaterial = gptAsset->load("/assets/materials/water.plmaterial");
        const plAssetDesc tMeshAsset = {
            .tType = gptMesh->get_asset_type_key(),
            .pcPath = "/assets/meshes/water_grid.plmesh"
        };
        // Asset creation takes ownership of the mesh allocation.
        plAssetHandle tWaterAsset = gptAsset->create(&tMeshAsset, &tWaterGrid);
        gptAsset->save(tWaterAsset, PL_ASSET_ENCODING_TEXT);
    }

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

    // sbtImportFiles

    // if(!gptVfs->does_file_exist("/assets/models/humanoid_Scene.plscene"))
    // {
    //     gptGltf->import("/resources/DamagedHelmet.glb", NULL);
    //     gptGltf->import("/resources/humanoid.gltf", NULL);

    //     plMesh tUnitSphere = {0};
    //     gptMesh->create_sphere(1.0f, 16, 16, &tUnitSphere);
    //     tUnitSphere.atSubmeshes[0].tMaterial = gptAsset->load("/assets/materials/default.plmaterial");
    //     plAssetDesc tMeshAsset = {
    //         .tType = gptMesh->get_asset_type_key(),
    //         .pcPath = "/assets/meshes/sphere.plmesh"
    //     };
    //     plAssetHandle tBlah = gptAsset->create(&tMeshAsset, &tUnitSphere);
    //     gptAsset->save(tBlah, PL_ASSET_ENCODING_TEXT);
    // }
    // gptGltf->import("/resources/gltf-samples/Models/Sponza/glTF/sponza.gltf", NULL);

    // // animations
    // gptGltf->import("/resources/gltf-samples/Models/InterpolationTest/glTF/InterpolationTest.gltf", NULL);
    // gptGltf->import("/resources/gltf-samples/Models/CesiumMan/glTF/CesiumMan.gltf", NULL);
    // // gptGltf->import("/resources/gltf-samples/Models/BrainStem/glTF/BrainStem.gltf", NULL); // broke
    // gptGltf->import("/resources/gltf-samples/Models/CommercialRefrigerator/glTF/CommercialRefrigerator.gltf", NULL);

    // // anisotropy
    // gptGltf->import("/resources/gltf-samples/Models/AnisotropyBarnLamp/glTF/AnisotropyBarnLamp.gltf", NULL);
    // gptGltf->import("/resources/gltf-samples/Models/AnisotropyDiscTest/glTF/AnisotropyDiscTest.gltf", NULL);
    // // gptGltf->import("/resources/gltf-samples/Models/AnisotropyStrengthTest/glTF/AnisotropyStrengthTest.gltf", NULL); // broke
    // gptGltf->import("/resources/gltf-samples/Models/AnisotropyRotationTest/glTF/AnisotropyRotationTest.gltf", NULL);
    
    // // basic tests
    // gptGltf->import("/resources/gltf-samples/Models/AlphaBLendModeTest/glTF/AlphaBLendModeTest.gltf", NULL);
    // gptGltf->import("/resources/gltf-samples/Models/OrientationTest/glTF/OrientationTest.gltf", NULL);
    // gptGltf->import("/resources/gltf-samples/Models/EnvironmentTest/glTF/EnvironmentTest.gltf", NULL);

    // // car
    // gptGltf->import("/resources/gltf-samples/Models/CarConcept/glTF/CarConcept.gltf", NULL);
    
    // // chess
    // gptGltf->import("/resources/gltf-samples/Models/ABeautifulGame/glTF/ABeautifulGame.gltf", NULL);

    // // clear coat
    // gptGltf->import("/resources/gltf-samples/Models/ClearCoatWicker/glTF/ClearCoatWicker.gltf", NULL);
    

    plToolsInit tToolsInit = {
        .ptDevice = ptAppData->ptDevice
    };
    gptTools->initialize(tToolsInit);

    // retrieve some console variables
    ptAppData->pbShowLogging              = (bool*)gptConsole->get_variable("t.LogTool", NULL, NULL);
    ptAppData->pbShowStats                = (bool*)gptConsole->get_variable("t.StatTool", NULL, NULL);
    ptAppData->pbShowProfiling            = (bool*)gptConsole->get_variable("t.ProfileTool", NULL, NULL);
    ptAppData->pbShowGpuProfiling         = (bool*)gptConsole->get_variable("t.GPUProfileTool", NULL, NULL);
    ptAppData->pbShowMemoryAllocations    = (bool*)gptConsole->get_variable("t.MemoryAllocationTool", NULL, NULL);
    ptAppData->pbShowDeviceMemoryAnalyzer = (bool*)gptConsole->get_variable("t.DeviceMemoryAnalyzerTool", NULL, NULL);
    ptAppData->pbShowAssets               = (bool*)gptConsole->get_variable("t.Assets", NULL, NULL);
    ptAppData->pbShowRenderer             = (bool*)gptConsole->get_variable("t.Renderer", NULL, NULL);

    *ptAppData->pbShowLogging = gptConfig->load_bool("pbShowLogging", *ptAppData->pbShowLogging);
    *ptAppData->pbShowStats = gptConfig->load_bool("pbShowStats", *ptAppData->pbShowStats);
    *ptAppData->pbShowProfiling = gptConfig->load_bool("pbShowProfiling", *ptAppData->pbShowProfiling);
    *ptAppData->pbShowGpuProfiling = gptConfig->load_bool("pbShowGpuProfiling", *ptAppData->pbShowGpuProfiling);
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

    // pl__load_assets(ptAppData);
    pl__refresh_files(ptAppData);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~app stuff~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // Keep the sandbox layout separate from other Pilot Light applications.
    gptUI->set_settings_file("../cache/sandbox_ui.ini");

    gptLog->info(guLogChannel, "Sandbox ready");
    pl__set_loading(ptAppData, false);
    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    gptLog->info(guLogChannel, "Sandbox shutdown started");
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
    gptConfig->set_bool("pbShowGpuProfiling", *ptAppData->pbShowGpuProfiling);
    gptConfig->set_bool("pbShowMemoryAllocations", *ptAppData->pbShowMemoryAllocations);
    gptConfig->set_bool("pbShowDeviceMemoryAnalyzer", *ptAppData->pbShowDeviceMemoryAnalyzer);

    gptConfig->save_to_disk(NULL);
    gptConfig->cleanup();
    gptAssetTools->cleanup();
    gptPhysics->cleanup();

    if(ptAppData->ptScene)
    {
        if(ptAppData->ptSecondaryView)
            gptRenderer->destroy_view(ptAppData->ptSecondaryView);
        gptRenderer->destroy_view(ptAppData->ptView);
        gptRenderer->destroy_scene(ptAppData->ptScene);
    }

    gptAsset->cleanup();
    gptEcs->cleanup();
    
    gptRenderer->cleanup();
    gptStarter->cleanup();
    gptWindows->destroy(ptAppData->ptWindow);
    PL_FREE(ptAppData);
    gptLog->info(guLogChannel, "Sandbox shutdown complete");
    if(!gptLog->close_file(PL_LOG_ALL_CHANNELS))
        gptLog->warn(guLogChannel, "Failed to finish writing ../cache/sandbox.log; the file may be incomplete");
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plWindow* ptWindow, plAppData* ptAppData)
{
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

    if(!gbLogFileErrorReported && gptLog->has_file_error(PL_LOG_ALL_CHANNELS))
    {
        gbLogFileErrorReported = true;
        gptLog->warn(guLogChannel, "File logging failed for ../cache/sandbox.log; console and Log Tool remain available");
        gptScreenLog->add_message_ex(0, 10.0, PL_COLOR_32_RED, 1.0f, "%s", "File logging failed; see Log Tool");
    }
        
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);
    
    // for convience
    plIO* ptIO = gptIO->get_io();

    gptRenderer->begin_frame();

    const bool bWasGameMode = ptAppData->bGameMode;
    if(!ptAppData->ptScene)
        ptAppData->bGameMode = false;
    else if(gptIO->is_key_pressed(PL_KEY_F5, false) && !gptGizmo->active())
        ptAppData->bGameMode = !ptAppData->bGameMode;
    const float fMenuHeight = ptAppData->bGameMode ? 0.0f : pl__show_main_menu(ptAppData);
    if(bWasGameMode != ptAppData->bGameMode)
    {
        ptAppData->bSceneClickStarted = false;
        gptIO->reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
        gptIO->reset_mouse_drag_delta(PL_MOUSE_BUTTON_RIGHT);
    }
    gptUI->set_docking_enabled(!ptAppData->bGameMode);
    if(!ptAppData->bGameMode)
    {
        const uint32_t uDockRoot = gptUI->dock_space("Sandbox Dockspace", (plVec2){0, fMenuHeight}, (plVec2){ptIO->tMainViewportSize.x, pl_maxf(1.0f, ptIO->tMainViewportSize.y - fMenuHeight)});
        if(!ptAppData->bDockLayoutInitialized)
        {
            ptAppData->uSceneDock = uDockRoot;
            uint32_t uCenter = 0;
            ptAppData->uToolsDock = gptUI->dock_split(uDockRoot, 0, 0.22f, &uCenter);
            if(uCenter)
                ptAppData->uSceneDock = uCenter;
            // Saved window docking overrides these ONCE defaults on subsequent runs.
            ptAppData->bDockLayoutInitialized = true;
        }

        pl__show_sandbox_tools(ptAppData);
    }
    gptAssetTools->run();

    if(!ptAppData->bShowSecondaryView && ptAppData->ptSecondaryView)
    {
        gptRenderer->destroy_view(ptAppData->ptSecondaryView);
        ptAppData->ptSecondaryView = NULL;
    }
    const bool bSecondaryVisible = !ptAppData->bGameMode && ptAppData->ptScene && ptAppData->bShowSecondaryView && pl__show_secondary_view(ptAppData);
    bool bSceneWindow = ptAppData->bGameMode;
    if(ptAppData->ptScene && !ptAppData->bGameMode)
    {
        gptUI->set_next_window_dock(ptAppData->uSceneDock, PL_UI_COND_ONCE);
        bSceneWindow = gptUI->begin_window("Scene", NULL,
            PL_UI_WINDOW_FLAGS_NO_COLLAPSE | PL_UI_WINDOW_FLAGS_NO_SCROLLBAR | PL_UI_WINDOW_FLAGS_NO_INPUT_CAPTURE);
    }
    if(ptAppData->ptScene)
    {
        if(bSceneWindow)
        {
            if(!ptAppData->bGameMode)
                ptAppData->uSceneDock = gptUI->get_window_dock();
            ptAppData->tViewOffset = ptAppData->bGameMode ? (plVec2){0} : gptUI->get_cursor_pos();
            const plVec2 tAvailable = ptAppData->bGameMode ? ptIO->tMainViewportSize : gptUI->get_content_region_available();
            ptAppData->tViewSize = (plVec2){pl_maxf(1.0f, floorf(tAvailable.x)), pl_maxf(1.0f, floorf(tAvailable.y))};
            if(ptAppData->tRenderSize.x != ptAppData->tViewSize.x || ptAppData->tRenderSize.y != ptAppData->tViewSize.y)
            {
                gptRenderer->resize_view(ptAppData->ptView, ptAppData->tViewSize);
                ptAppData->tRenderSize = ptAppData->tViewSize;
            }
        }
        const plVec2 tViewScale = pl_div_vec2(ptAppData->tViewSize, ptIO->tMainViewportSize);
        plComponentLibrary* ptLibrary = (plComponentLibrary*)gptAsset->get_data(ptAppData->tSceneHandle);
        plCamera* ptCamera = (plCamera*)gptEcs->get_component(ptLibrary, gptCameraEcs->get_ecs_type_key(), ptAppData->tMainCamera);

        if(bSceneWindow)
            gptCamera->set_viewport(ptCamera, ptAppData->tViewSize.x, ptAppData->tViewSize.y);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~selection stuff~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        plVec2 tMousePos = gptIO->get_mouse_pos();

        const bool bViewHovered = bSceneWindow && (ptAppData->bGameMode ? !gptUI->wants_mouse_capture() : gptUI->is_window_hovered()) && gptIO->is_mouse_hovering_rect(ptAppData->tViewOffset,
            pl_add_vec2(ptAppData->tViewOffset, ptAppData->tViewSize));
        if(!ptAppData->bGameMode && bViewHovered && !gptUI->wants_mouse_capture() && !gptGizmo->active())
        {
            if(gptIO->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
            {
                ptAppData->tSceneClickPos = tMousePos;
                ptAppData->bSceneClickStarted = true;
            }
            else if(gptIO->is_mouse_released(PL_MOUSE_BUTTON_LEFT))
            {
                plVec2 tReleasePos = tMousePos;

                if(ptAppData->bSceneClickStarted && tReleasePos.x == ptAppData->tSceneClickPos.x && tReleasePos.y == ptAppData->tSceneClickPos.y)
                    // Picking uses display/render scale. Both sizes match; the gizmo
                    // instead takes display/main-window scale below.
                    gptRenderer->update_hovered_entity(ptAppData->ptView, ptAppData->tViewOffset, (plVec2){1.0f, 1.0f});
            }
        }
        if(gptIO->is_mouse_released(PL_MOUSE_BUTTON_LEFT) || gptGizmo->active())
            ptAppData->bSceneClickStarted = false;

        // run ecs system
        PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, "Run ECS");
        // Pause only the main camera controller while another viewport owns input.
        // Other scene scripts and simulation still run once per frame.
        plScriptComponent* ptCameraScript = gptEcs->get_component(ptLibrary, gptScript->get_ecs_type_key(), ptAppData->tMainCamera);
        const plScriptFlags tCameraScriptFlags = ptCameraScript ? ptCameraScript->tFlags : 0;
        if(ptCameraScript && !bViewHovered)
            ptCameraScript->tFlags = PL_SCRIPT_FLAG_NONE;
        gptScript->run_update_system(ptLibrary);
        if(ptCameraScript && !bViewHovered)
        {
            ptCameraScript = gptEcs->get_component(ptLibrary, gptScript->get_ecs_type_key(), ptAppData->tMainCamera);
            if(ptCameraScript)
                ptCameraScript->tFlags = tCameraScriptFlags;
        }
        if(bSecondaryVisible)
            pl__update_secondary_camera(ptAppData);
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


            plObjectComponent* ptSelectedObject = gptEcs->get_component(ptLibrary, gptRenderer->get_ecs_type_key_object(), ptAppData->tSelectedEntity);
            if(ptSelectedObject)
            {
                ptSelectedObject->tFlags &= ~PL_OBJECT_FLAGS_OUTLINE;
                ptSelectedObject->tFlags |= PL_OBJECT_FLAGS_DIRTY;
            }
            
            if(!gptEcs->is_entity_valid(ptLibrary, tNextEntity))
            {
                ptAppData->tSelectedEntity.uData = UINT64_MAX;
                gptScreenLog->add_message_ex(565168477883, 1.0, PL_COLOR_32_RED, 1.0f, "Unselected Entity");
            }
            else if(ptAppData->tSelectedEntity.uData != tNextEntity.uData)
            {
                plTagComponent* ptSelectedTag = gptEcs->get_component(ptLibrary, gptEcs->get_ecs_type_key_tag(), tNextEntity);
                gptScreenLog->add_message_ex(565168477883, -1.0, PL_COLOR_32_GREEN, 1.0f, "Selected Entity \"%s\" {%u, %u}", ptSelectedTag ? ptSelectedTag->pcName : "No Name", tNextEntity.uIndex, tNextEntity.uGeneration);
                ptAppData->tSelectedEntity = tNextEntity;
                gptPhysics->set_angular_velocity(ptLibrary, tNextEntity, pl_create_vec3(0, 0, 0));
                gptPhysics->set_linear_velocity(ptLibrary, tNextEntity, pl_create_vec3(0, 0, 0));


            }
            ptSelectedObject = gptEcs->get_component(ptLibrary, gptRenderer->get_ecs_type_key_object(), ptAppData->tSelectedEntity);
            if(ptSelectedObject)
            {
                ptSelectedObject->tFlags |= PL_OBJECT_FLAGS_OUTLINE;
                ptSelectedObject->tFlags |= PL_OBJECT_FLAGS_DIRTY;
            }
        }

        if(!gptUI->wants_keyboard_capture())
        {
            if(gptIO->is_key_pressed(PL_KEY_M, false)) gptGizmo->next_mode();
            if(gptIO->is_key_pressed(PL_KEY_L, false))
                gptGizmo->set_space(gptGizmo->get_space() == PL_GIZMO_SPACE_LOCAL ? PL_GIZMO_SPACE_WORLD : PL_GIZMO_SPACE_LOCAL);
        }

        plEcsChange* atEcsChanges = NULL;
        uint32_t uEcsChangeCount = 0;
        plEcsTypeKey* atEcsTypes = NULL;

        gptEcs->get_changes(ptLibrary, &atEcsChanges, &uEcsChangeCount, &atEcsTypes);

        // Structural edits can remove an object and add its replacements in one batch.
        // Remove stale drawables first so instancing never examines a removed component.
        for(uint32_t i = 0; i < uEcsChangeCount; i++)
        {
            const plEcsChange* ptChange = &atEcsChanges[i];
            if(ptChange->eType == PL_ECS_CHANGE_ENTITY_REMOVED)
                gptRenderer->remove_entity_from_scene(ptAppData->ptScene, ptChange->tEntity);
            else if(ptChange->eType == PL_ECS_CHANGE_COMPONENT_REMOVED)
                gptRenderer->remove_component_from_scene(ptAppData->ptScene, ptChange->tEntity, ptChange->tComponentType);
        }
        for(uint32_t i = 0; i < uEcsChangeCount; i++)
        {
            const plEcsChange* ptChange = &atEcsChanges[i];
            if(ptChange->eType == PL_ECS_CHANGE_COMPONENT_ADDED &&
               gptEcs->has_component(ptLibrary, ptChange->tComponentType, ptChange->tEntity))
                gptRenderer->add_component_to_scene(ptAppData->ptScene, ptChange->tEntity, ptChange->tComponentType);
        }

        gptEcs->clear_changes(ptLibrary);


        plAssetChange* atAssetChanges = NULL;
        uint32_t uAssetChangeCount = 0;

        gptAsset->get_changes(&atAssetChanges, &uAssetChangeCount);
        for(uint32_t i = 0; i < uAssetChangeCount; i++)
        {
            switch(atAssetChanges[i].eType)
            {
                case PL_ASSET_CHANGE_CHANGED:
                {
                    gptRenderer->update_scene_asset(ptAppData->ptScene, atAssetChanges[i].tAsset);
                    break;
                }
                default:
                    break;
            }
        }

        gptAsset->clear_changes();

        bool bGizmoWasActive = gptGizmo->active();
        if(!ptAppData->bGameMode && bSceneWindow && (bViewHovered || bGizmoWasActive) && gptEcs->is_entity_valid(ptLibrary, ptAppData->tSelectedEntity))
        {
            plDrawList3D* ptGizmoDrawlist =  gptRenderer->get_gizmo_drawlist(ptAppData->ptView);
            
            if(gptEcs->has_component(ptLibrary, gptTransform->get_ecs_type_key_transform(), ptAppData->tSelectedEntity))
            {
                gptGizmo->gizmo(ptLibrary, ptAppData->tSelectedEntity, ptGizmoDrawlist, ptCamera, ptAppData->tViewOffset, tViewScale);
            }
            else if(gptEcs->has_component(ptLibrary, gptRenderer->get_ecs_type_key_object(), ptAppData->tSelectedEntity))
            {
                plObjectComponent* ptSelectedObject = gptEcs->get_component(ptLibrary, gptRenderer->get_ecs_type_key_object(), ptAppData->tSelectedEntity);
                gptGizmo->gizmo(ptLibrary, ptSelectedObject->tTransform, ptGizmoDrawlist, ptCamera, ptAppData->tViewOffset, tViewScale);
            }
        }

        // render scene
        const plCamera* atCameras[2];
        uint32_t uCameraCount = 0;
        if(bSceneWindow) atCameras[uCameraCount++] = ptCamera;
        if(bSecondaryVisible) atCameras[uCameraCount++] = &ptAppData->tSecondaryCamera;
        if(uCameraCount)
            gptRenderer->prepare_scene(ptAppData->ptScene, atCameras, uCameraCount);
        
        if(bSceneWindow)
        {
            plRenderViewDesc tViewDesc0 = {
                .ptCamera     = ptCamera
            };
            gptRenderer->prepare_view(ptAppData->ptView, ptCamera);

            gptRenderer->render_view(ptAppData->ptView, &tViewDesc0);

            plVec2 tUV = {0};
            plBindGroupHandle tTexture = gptRenderer->get_view_color_bind_group(ptAppData->ptView, &tUV);
            if(ptAppData->bGameMode)
                gptDraw->add_image_ex(gptStarter->get_background_layer(), tTexture.uData,
                    (plVec2){0}, ptAppData->tViewSize, (plVec2){0}, tUV, PL_COLOR_32_WHITE);
            else
            {
                gptUI->image_ex(tTexture.uData, ptAppData->tViewSize, (plVec2){0}, tUV, (plVec4){1,1,1,1}, (plVec4){0});
                gptUI->end_window();
            }
        }
        if(bSecondaryVisible)
        {
            plRenderViewDesc tViewDesc = {.ptCamera = &ptAppData->tSecondaryCamera};
            gptRenderer->prepare_view(ptAppData->ptSecondaryView, &ptAppData->tSecondaryCamera);
            gptRenderer->render_view(ptAppData->ptSecondaryView, &tViewDesc);
        }

        if(!bGizmoWasActive && gptIO->is_key_pressed(PL_KEY_ESCAPE, false))
        {
            if(!gptUI->wants_keyboard_capture())
            {
                gptLog->info_p(guLogChannel, "Unloading scene: %s", ptAppData->acCurrentScene);
                gptPhysics->reset();
                if(ptAppData->ptSecondaryView)
                    gptRenderer->destroy_view(ptAppData->ptSecondaryView);
                ptAppData->ptSecondaryView = NULL;
                gptRenderer->destroy_view(ptAppData->ptView);
                gptRenderer->destroy_scene(ptAppData->ptScene);
                ptAppData->ptView = NULL;
                ptAppData->ptScene = NULL;
                ptAppData->tRenderSize = (plVec2){0};
                ptAppData->bSceneClickStarted = false;
                pl__update_title_bar(ptAppData);
                gptAsset->destroy(ptAppData->tSceneHandle);
                pl__refresh_files(ptAppData);
                gptScreenLog->add_message_ex(565168477883, 2.0, PL_COLOR_32_RED, 0.1f, "reset");
            }
        }
    }


    // ui windows
    if(ptAppData->ptScene == NULL && !bSceneWindow)
    {  
        gptUI->set_next_window_dock(ptAppData->uSceneDock, PL_UI_COND_ONCE);
        if(gptUI->begin_window("Scene", NULL, PL_UI_WINDOW_FLAGS_NO_COLLAPSE | PL_UI_WINDOW_FLAGS_NO_SCROLLBAR))
        { 
            // gptUI->layout_static(0.0f, 100.0f, 2);
            gptUI->layout_dynamic(0.0f, 2);
            if(gptUI->button("Refresh"))
                pl__refresh_files(ptAppData);
            if(gptUI->button("Clear Assets"))
            {
                gptAssetTools->cleanup(); // Close inspectors before their assets and libraries disappear.
                gptAsset->clear_assets();
            }
            if(gptUI->button("Load Assets"))
                pl__load_assets(ptAppData);
            gptUI->checkbox("Imports", &ptAppData->bShowImportOptions);
            gptUI->layout_dynamic(pl_maxf(1.0f, gptUI->get_content_region_available().y), 1);

            if(gptUI->begin_child("Scenes", 0, 0))
            {

                gptUI->layout_dynamic(0.0f, 1);

                if(ptAppData->bShowImportOptions)
                {
                    uint32_t uImportOptions = PL_ARRAYSIZE(gacImportFiles);

                    for (uint32_t n = 0; n < uImportOptions; n++)
                    {
                        bool bPlaceHolder = false;
                        if(gptUI->button(gacImportFiles[n]))
                        {
                            pl__set_loading(ptAppData, true);
                            gptLog->info_p(guLogChannel, "Importing: %s", gacImportFiles[n]);
                            if(gptGltf->import(gacImportFiles[n], NULL))
                            {
                                gptLog->info_p(guLogChannel, "Imported: %s", gacImportFiles[n]);
                                gptScreenLog->add_message_ex(0, 10.0, PL_COLOR_32_GREEN, 1.0f, "Imported: %s", gacImportFiles[n]);
                            }
                            else
                            {
                                gptLog->error_p(guLogChannel, "Import failed: %s", gacImportFiles[n]);
                                gptScreenLog->add_message_ex(0, 10.0, PL_COLOR_32_RED, 1.0f, "Import failed: %s", gacImportFiles[n]);
                            }
                            pl__set_loading(ptAppData, false);
                        }
                    }
                }
                else
                {

                    uint32_t uSceneCount = pl_sb_size(ptAppData->sbtSceneFilesCore);

                    for (uint32_t n = 0; n < uSceneCount; n++)
                    {
                        bool bPlaceHolder = false;
                        if(gptUI->selectable(ptAppData->sbtSceneFilesCore[n].acName, &bPlaceHolder, 0))
                        {
                            pl_sprintf(ptAppData->acCurrentScene, "%s", ptAppData->sbtSceneFilesCore[n].acTemplate);

                            gptLog->info_p(guLogChannel, "Loading scene: %s", ptAppData->acCurrentScene);
                            pl__set_loading(ptAppData, true);
                            ptAppData->tSceneHandle = gptAsset->load(ptAppData->acCurrentScene);

                            if(gptAsset->is_valid(ptAppData->tSceneHandle))
                            {
                                plComponentLibrary* ptLibrary = gptAsset->get_data(ptAppData->tSceneHandle);

                                plSceneDesc tSceneInit = {0};

                                ptAppData->ptScene = gptRenderer->create_scene(&tSceneInit);
                                plViewDesc tViewDesc = PL_ZERO_INIT;
                                tViewDesc.uWidth = (uint32_t)ptIO->tMainViewportSize.x;
                                tViewDesc.uHeight = (uint32_t)ptIO->tMainViewportSize.y;
                                ptAppData->ptView = gptRenderer->create_view(ptAppData->ptScene, &tViewDesc);
                                
                                gptRenderer->load_component_library(ptAppData->ptScene, ptLibrary);
                                gptEcs->clear_changes(ptLibrary);

                                const plEntity* ptCameraEntities = NULL;
                                uint32_t uCameraCount = gptEcs->get_components(ptLibrary, gptCameraEcs->get_ecs_type_key(), NULL, &ptCameraEntities);
                                if(uCameraCount > 0)
                                    ptAppData->tMainCamera = ptCameraEntities[0];
                                gptLog->info_p(guLogChannel, "Scene ready: %s (%u cameras)", ptAppData->acCurrentScene, uCameraCount);
                            }
                            else
                            {
                                gptLog->error_p(guLogChannel, "Failed to load scene: %s; check asset availability/imports", ptAppData->acCurrentScene);
                                gptScreenLog->add_message_ex(0, 5.0, PL_COLOR_32_RED, 1.5f, "%s", "Failed to load scene");
                                gptScreenLog->add_message_ex(0, 5.0, PL_COLOR_32_RED, 1.0f, "%s", "Ensure assets are downloaded");
                                gptScreenLog->add_message_ex(0, 5.0, PL_COLOR_32_RED, 1.0f, "%s", "Ensure assets are imported");
                            }
                            pl__set_loading(ptAppData, false);
                        }
                    }
                }
                gptUI->end_child();
            }
            gptUI->end_window();
        }
    }
    // if(gptUI->begin_window("Debug", NULL, 0))
    // {
    //     if(gptUI->button("Select File"))
    //     {
    //         gptDialog->open_file_dialog(NULL);
    //     }
    //     gptUI->end_window();
    // }

    // if(gptDialog->draw_file_dialog())
    // {
    //     const char* pcPath = gptDialog->get_file_dialog_path();
    //     gptScreenLog->add_message(1.0, pcPath);
    // }


    if(ptAppData->bShowUiDemo)
        pl__show_ui_demo_window(ptAppData);

    gptStarter->begin_main_pass();
    gptStarter->end_main_pass();

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
    gptStarter->end_frame();
}

//-----------------------------------------------------------------------------
// [SECTION] helper implementations
//-----------------------------------------------------------------------------

static bool
pl__show_secondary_view(plAppData* ptAppData)
{
    if(!ptAppData->bSecondaryDockInitialized)
    {
        uint32_t uMainDock = 0;
        ptAppData->uSecondaryDock = gptUI->dock_split(ptAppData->uSceneDock, 1, 0.5f, &uMainDock);
        if(uMainDock)
            ptAppData->uSceneDock = uMainDock;
        ptAppData->bSecondaryDockInitialized = true;
    }
    gptUI->set_next_window_dock(ptAppData->uSecondaryDock, PL_UI_COND_ONCE);
    gptUI->set_next_window_size((plVec2){640, 480}, PL_UI_COND_ONCE);
    ptAppData->bSecondaryHovered = false;
    if(!gptUI->begin_window("Secondary View", &ptAppData->bShowSecondaryView,
        PL_UI_WINDOW_FLAGS_NO_COLLAPSE | PL_UI_WINDOW_FLAGS_NO_SCROLLBAR | PL_UI_WINDOW_FLAGS_NO_INPUT_CAPTURE))
        return false;
    ptAppData->uSecondaryDock = gptUI->get_window_dock();

    plComponentLibrary* ptLibrary = gptAsset->get_data(ptAppData->tSceneHandle);
    const plCamera* ptMainCamera = gptEcs->get_component(ptLibrary, gptCameraEcs->get_ecs_type_key(), ptAppData->tMainCamera);
    gptUI->layout_dynamic(0.0f, 1);
    const bool bCopyCamera = gptUI->button("Copy Main Camera");
    gptUI->text("RMB: look + WASD/QE move | Wheel: speed");
    if(!ptMainCamera)
    {
        gptUI->text("No camera available in this scene.");
        gptUI->end_window();
        return false;
    }
    const plVec2 tAvailable = gptUI->get_content_region_available();
    const plVec2 tSize = {pl_maxf(1.0f, floorf(tAvailable.x)), pl_maxf(1.0f, floorf(tAvailable.y))};
    if(!ptAppData->ptSecondaryView)
    {
        const plViewDesc tDesc = {.uWidth = (uint32_t)tSize.x, .uHeight = (uint32_t)tSize.y};
        ptAppData->ptSecondaryView = gptRenderer->create_view(ptAppData->ptScene, &tDesc);
        ptAppData->tSecondaryCamera = *ptMainCamera;
        ptAppData->fSecondarySpeed = 4.0f;
    }
    else if(tSize.x != ptAppData->tSecondarySize.x || tSize.y != ptAppData->tSecondarySize.y)
        gptRenderer->resize_view(ptAppData->ptSecondaryView, tSize);
    if(bCopyCamera)
        ptAppData->tSecondaryCamera = *ptMainCamera;
    ptAppData->tSecondarySize = tSize;
    ptAppData->tSecondaryOffset = gptUI->get_cursor_pos();
    ptAppData->bSecondaryHovered = gptUI->is_window_hovered() && gptIO->is_mouse_hovering_rect(
        ptAppData->tSecondaryOffset, pl_add_vec2(ptAppData->tSecondaryOffset, tSize));
    gptCamera->set_viewport(&ptAppData->tSecondaryCamera, tSize.x, tSize.y);
    gptCamera->update(&ptAppData->tSecondaryCamera);

    // The view renders later this frame, before the main pass consumes this image.
    plVec2 tUV = {0};
    const plBindGroupHandle tTexture = gptRenderer->get_view_color_bind_group(ptAppData->ptSecondaryView, &tUV);
    gptUI->image_ex(tTexture.uData, tSize, (plVec2){0}, tUV, (plVec4){1,1,1,1}, (plVec4){0});
    gptUI->end_window();
    return true;
}

static void
pl__update_secondary_camera(plAppData* ptAppData)
{
    if(!ptAppData->bSecondaryHovered || gptUI->wants_mouse_capture() ||
       gptUI->wants_keyboard_capture() || gptGizmo->active())
        return;
    plCamera* ptCamera = &ptAppData->tSecondaryCamera;
    const float fWheel = gptIO->get_mouse_wheel();
    if(fWheel != 0.0f)
        ptAppData->fSecondarySpeed = pl_clampf(0.1f, ptAppData->fSecondarySpeed * (fWheel > 0.0f ? 2.0f : 0.5f), 10000.0f);
    if(gptIO->is_mouse_down(PL_MOUSE_BUTTON_RIGHT))
    {
        const float fStep = ptAppData->fSecondarySpeed * gptIO->get_io()->fDeltaTime;
        const plVec2 tDelta = gptIO->get_mouse_drag_delta(PL_MOUSE_BUTTON_RIGHT, 1.0f);
        gptCamera->rotate_euler(ptCamera, -tDelta.y * 0.005f, -tDelta.x * 0.005f, 0.0f);
        gptIO->reset_mouse_drag_delta(PL_MOUSE_BUTTON_RIGHT);
        if(gptIO->is_key_down(PL_KEY_W)) gptCamera->translate_local(ptCamera, (plVec3d){0, 0, fStep});
        if(gptIO->is_key_down(PL_KEY_S)) gptCamera->translate_local(ptCamera, (plVec3d){0, 0, -fStep});
        if(gptIO->is_key_down(PL_KEY_A)) gptCamera->translate_local(ptCamera, (plVec3d){fStep, 0, 0});
        if(gptIO->is_key_down(PL_KEY_D)) gptCamera->translate_local(ptCamera, (plVec3d){-fStep, 0, 0});
        if(gptIO->is_key_down(PL_KEY_Q)) gptCamera->translate(ptCamera, (plVec3d){0, -fStep, 0});
        if(gptIO->is_key_down(PL_KEY_E)) gptCamera->translate(ptCamera, (plVec3d){0, fStep, 0});
    }
    gptCamera->update(ptCamera);
}

static float
pl__show_main_menu(plAppData* ptAppData)
{
    float fHeight = 0.0f;
    if(gptUI->begin_main_menu_bar())
    {
        fHeight = gptUI->get_window_size().y;
        if(gptUI->begin_menu("View", true))
        {
            gptUI->menu_item_toggle("Game Mode", "F5", &ptAppData->bGameMode, ptAppData->ptScene != NULL && !gptGizmo->active());
            gptUI->menu_item_toggle("Secondary View", NULL, &ptAppData->bShowSecondaryView, ptAppData->ptScene != NULL);
            gptUI->end_menu();
        }
        if(gptUI->begin_menu("Tools", true))
        {
            gptUI->menu_item_toggle("Assets", NULL, ptAppData->pbShowAssets, true);
            gptUI->menu_item_toggle("Renderer", NULL, ptAppData->pbShowRenderer, true);
            gptUI->separator();
            gptUI->menu_item_toggle("Logging", NULL, ptAppData->pbShowLogging, true);
            gptUI->menu_item_toggle("GPU Profiling", NULL, ptAppData->pbShowGpuProfiling, true);
            gptUI->menu_item_toggle("CPU Profiling", NULL, ptAppData->pbShowProfiling, true);
            gptUI->menu_item_toggle("Statistics", NULL, ptAppData->pbShowStats, true);
            gptUI->menu_item_toggle("Memory Allocations", NULL, ptAppData->pbShowMemoryAllocations, true);
            gptUI->menu_item_toggle("Device Memory", NULL, ptAppData->pbShowDeviceMemoryAnalyzer, true);

            gptUI->separator_text("UI");
            bool* pbStyle = (bool*)gptConsole->get_variable("t.UiStyle", NULL, NULL);
            bool* pbDebug = (bool*)gptConsole->get_variable("t.UiDebug", NULL, NULL);
            gptUI->menu_item_toggle("Demo", NULL, &ptAppData->bShowUiDemo, true);
            gptUI->menu_item_toggle("Style Editor", NULL, pbStyle, pbStyle != NULL);
            gptUI->menu_item_toggle("Debugger", NULL, pbDebug, pbDebug != NULL);
            gptUI->end_menu();
        }
        gptUI->end_main_menu_bar();
    }
    return fHeight;
}

static void
pl__show_sandbox_tools(plAppData* ptAppData)
{
    gptUI->set_next_window_dock(ptAppData->uToolsDock, PL_UI_COND_ONCE);
    if(gptUI->begin_window("Sandbox", NULL, PL_UI_WINDOW_FLAGS_NO_COLLAPSE))
    {
        plIO* ptIO = gptIO->get_io();
        gptUI->layout_dynamic(0.0f, 1);
        gptUI->text("%0.1f FPS / %0.1f ms", ptIO->fFrameRate, ptIO->fDeltaTime * 1000.0f);
        gptUI->text("%s", PILOT_LIGHT_VERSION_STRING);
        gptUI->text("Backend: %s", gptGfx->get_backend_string());
        gptUI->separator_text("Controls");
        gptUI->text("F1: console | Escape: unload scene");
        gptUI->text("F5: game mode");
        gptUI->text("M: gizmo operation | L: local/world");
        gptUI->text("LMB drag: move and turn");
        gptUI->text("RMB drag: look | LMB + RMB: pan");
        gptUI->text("Wheel: camera speed");
        gptUI->text("Hold RMB: WASD move, Q/E down/up");
        gptUI->text("Hold RMB + Z/C: zoom");
        gptUI->end_window();
    }
}

void
pl__load_apis(plApiRegistryI* ptApiRegistry)
{
    gptLog              = pl_get_api_latest(ptApiRegistry, plLogI);
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
    gptShaderLibrary    = pl_get_api_latest(ptApiRegistry, plShaderLibraryI);
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
    gptDialog           = pl_get_api_latest(ptApiRegistry, plDialogI);
}

static void
pl__initialize_logging(void)
{
    guLogChannel = gptLog->get_channel_id("Sandbox");
    if(guLogChannel != SIZE_MAX)
        return; // Keep the existing sink and log contents across hot reloads.

    guLogChannel = gptLog->add_channel("Sandbox", (plLogExtChannelInit){.tType = PL_LOG_CHANNEL_TYPE_CONSOLE | PL_LOG_CHANNEL_TYPE_CYCLIC_BUFFER});
    gptFile->create_directory("../cache");
    // One file per run, including existing and subsequently created extension channels.
    // Flush each entry so diagnostics preceding a crash are not left in the stdio buffer.
    if(!gptLog->set_file(PL_LOG_ALL_CHANNELS, "../cache/sandbox.log", PL_LOG_FILE_FLAGS_FLUSH_EACH_ENTRY))
        gptLog->warn(guLogChannel, "Unable to open ../cache/sandbox.log; continuing with console and Log Tool logging");

    const plDateTime tNow = gptDateTime->now();
    gptLog->info_p(guLogChannel, "Sandbox session started: %04d-%02d-%02d %02d:%02d:%02d (local), Pilot Light %s, backend %s", tNow.tDate.iYear, tNow.tDate.eMonth, tNow.tDate.iDay, tNow.tTime.iHour, tNow.tTime.iMinute, tNow.tTime.iSecond, PILOT_LIGHT_VERSION_STRING, gptGfx->get_backend_string());
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
        for(uint32_t i = 0; i < tDirectoryInfo.uEntryCount; i++)
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

    // local
    {
        plDirectoryInfo tDirectoryInfo = {0};
        gptFile->get_directory_info("../assets/dev/scenes/", &tDirectoryInfo);
        pl_sb_reserve(ptAppData->sbtSceneFilesCore, tDirectoryInfo.uFileCount);
        for(uint32_t i = 0; i < tDirectoryInfo.uEntryCount; i++)
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
                    pl_sprintf(acCurrentScene, "../assets/dev/scenes/%s.plscene", acFileNameOnly);
                    pl_sprintf(acFullPath, "../assets/dev/scenes/%s.plscene", acFileNameOnly);
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
    pl__set_loading(ptAppData, true);
    gptLog->info(guLogChannel, "Bulk asset loading started");
    char* sbcBuffer = NULL;

    {
        plDirectoryInfo tDirectoryInfo = {0};
        gptFile->get_directory_info("../assets/settings/", &tDirectoryInfo);
        for(uint32_t i = 0; i < tDirectoryInfo.uFileCount; i++)
        {
            if(tDirectoryInfo.sbtEntries[i].eType == PL_DIRECTORY_ENTRY_TYPE_FILE)
            {
                pl_sb_reset(sbcBuffer);
                pl_sb_sprintf(sbcBuffer, "/assets/settings/%s", tDirectoryInfo.sbtEntries[i].acName);
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

    pl_sb_free(sbcBuffer);
    gptAsset->clear_changes();
    gptLog->info(guLogChannel, "Bulk asset loading finished; individual failures are reported on the Assets channel");
    pl__set_loading(ptAppData, false);
}

static void
pl__update_title_bar(plAppData* ptAppData)
{
    if(!(gptWindows->get_capabilities()->tFlags & (PL_WINDOW_CAPABILITY_FLAGS_CUSTOM_TITLE_BAR | PL_WINDOW_CAPABILITY_FLAGS_TITLE_BAR_TEXT)))
        return;
    plWindowTitleBarDesc tTitleBar = {
        .uHeight = 38,
        .uBackgroundColor = 0xff1c1917,
        .uInactiveBackgroundColor = 0xff201e1c,
        .uTextColor = 0xffe5e2de,
        .uAccentColor = 0xff383430,
        .uBrandColor = 0xff46a6f2,
        .uStatusColor = 0xffa3b85e,
        .acBrand = "Pilot Light",
        .acTitle = "Sandbox"
    };
    snprintf(tTitleBar.acStatus, sizeof(tTitleBar.acStatus), "%s",
        ptAppData->bLoading ? "LOADING" : "READY");
    if(ptAppData->bLoading)
        tTitleBar.uStatusColor = tTitleBar.uBrandColor;
    if(ptAppData->ptScene)
    {
        const char* pcName = strrchr(ptAppData->acCurrentScene, '/');
        pcName = pcName ? pcName + 1 : ptAppData->acCurrentScene;
        snprintf(tTitleBar.acTitle, sizeof(tTitleBar.acTitle), "Sandbox / %s", pcName);
        char* pcExtension = strrchr(tTitleBar.acTitle, '.');
        if(pcExtension) *pcExtension = 0;
    }
    ptAppData->tTitleBar = tTitleBar;
    if(!gptWindows->set_title_bar(ptAppData->ptWindow, &tTitleBar))
        gptLog->warn(guLogChannel, "Custom title bar unavailable; using the native title bar");
}

static void
pl__set_loading(plAppData* ptAppData, bool bLoading)
{
    ptAppData->bLoading = bLoading;
    pl__update_title_bar(ptAppData);
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
