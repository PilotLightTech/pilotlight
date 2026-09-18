/*
   app.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] apis
// [SECTION] helper forward declarations
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

// standard
#include <stdlib.h>
#include <stdio.h>
#include <float.h>

// pilot light core
#include "pl.h"

// pilot light libraries
#include "pl_memory.h"
#include "pl_string.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

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

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAppData
{
    plWindow* ptWindow;
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

const plWindowI*            gptWindows          = nullptr;
const plStatsI*             gptStats            = nullptr;
const plGraphicsI*          gptGfx              = nullptr;
const plToolsI*             gptTools            = nullptr;
const plEcsI*               gptEcs              = nullptr;
const plCameraI*            gptCamera           = nullptr;
const plCameraEcsI*         gptCameraEcs        = nullptr;
const plRendererI*          gptRenderer         = nullptr;
const plJobI*               gptJobs             = nullptr;
const plDrawI*              gptDraw             = nullptr;
const plUiI*                gptUI               = nullptr;
const plIOI*                gptIO               = nullptr;
const plShaderI*            gptShader           = nullptr;
const plMemoryI*            gptMemory           = nullptr;
const plNetworkI*           gptNetwork          = nullptr;
const plStringInternI*      gptString           = nullptr;
const plProfileI*           gptProfile          = nullptr;
const plFileI*              gptFile             = nullptr;
const plAssetToolsI*        gptAssetTools       = nullptr;
const plGizmoI*             gptGizmo            = nullptr;
const plConsoleI*           gptConsole          = nullptr;
const plScreenLogI*         gptScreenLog        = nullptr;
const plPhysicsI *          gptPhysics          = nullptr;
const plCollisionI*         gptCollision        = nullptr;
const plBVHI*               gptBvh              = nullptr;
const plConfigI*            gptConfig           = nullptr;
const plResourceI*          gptResource         = nullptr;
const plStarterI*           gptStarter          = nullptr;
const plAnimationI*         gptAnimation        = nullptr;
const plMeshI*              gptMesh             = nullptr;
const plShaderVariantI*     gptShaderVariant    = nullptr;
const plVfsI*               gptVfs              = nullptr;
const plPakI*               gptPak              = nullptr;
const plDateTimeI*          gptDateTime         = nullptr;
const plCompressI*          gptCompress         = nullptr;
const plMaterialI*          gptMaterial         = nullptr;
const plScriptI*            gptScript           = nullptr;
const plAssetI*             gptAsset            = nullptr;
const plTransformI*         gptTransform        = nullptr;
const plIkI*                gptIk               = nullptr;
const plSkeletonI*          gptSkeleton         = nullptr;
const plTextureI*           gptTexture          = nullptr;
const plTerrainI*           gptTerrain          = nullptr;
const plStlI*               gptStl              = nullptr;

// helpful macros for memory tracking
#define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
#define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

#define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
#define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] helper forward declarations
//-----------------------------------------------------------------------------

void pl__load_apis(plApiRegistryI*);
void pl__load_extensions(plApiRegistryI*);

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryI* ptApiRegistry, plAppData* ptAppData)
{
    // NOTE: on first load, "ptAppData" will be NULL but on reloads
    //       it will be the value returned from this function's first
    //       call

    // if "ptAppData" is a valid pointer, then this function is being called
    // during a hot reload.
    if(ptAppData)
    {

        // re-retrieve the apis since we are now in
        // a different dll/so and we are storing them
        // as global variables
        pl__load_apis(ptApiRegistry);

        return ptAppData;
    }

    // retrieve extension registry
    const plExtensionRegistryI* ptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);

    // load extensions
    //   * first argument is the shared library name WITHOUT the extension
    //   * second & third argument is the load/unload functions names (use NULL for the default of "pl_load_ext" &
    //     "pl_unload_ext")
    //   * fourth argument indicates if the extension is reloadable (should we check for changes and reload if changed)
    pl__load_extensions(ptApiRegistry);
    pl__load_apis(ptApiRegistry);

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = (plAppData*)PL_ALLOC(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

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

    // mount required directories
    gptVfs->mount_directory("/shaders",   "../dependencies/pilotlight/shaders",   PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/resources", "../resources", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/cache",     "../cache",     PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/assets",    "../assets",    PL_VFS_MOUNT_FLAGS_NONE);

    // use window API to create a window
    plWindowDesc tWindowDesc = PL_ZERO_INIT;
    tWindowDesc.pcTitle = "App";
    tWindowDesc.iXPos   = 200;
    tWindowDesc.iYPos   = 200;
    tWindowDesc.uWidth  = 600;
    tWindowDesc.uHeight = 600;
    gptWindows->create(tWindowDesc, &ptAppData->ptWindow);
    gptWindows->show(ptAppData->ptWindow);

    plStarterInit tStarterInit = PL_ZERO_INIT;
    tStarterInit.eFlags   = PL_STARTER_FLAGS_ALL_EXTENSIONS;
    tStarterInit.eFlags   &= ~PL_STARTER_FLAGS_SHADER_EXT;
    tStarterInit.ptWindow = ptAppData->ptWindow;

    // let starter extension handle a lot of boilerplate
    gptStarter->initialize(tStarterInit);
    
    // initialize shader extension (we are doing this ourselves so we can add additional shader directories)
    static plShaderOptions tDefaultShaderOptions = {};
    tDefaultShaderOptions.apcIncludeDirectories[0] = "../dependencies/pilotlight/shaders/";
    tDefaultShaderOptions.apcDirectories[0] = "../dependencies/pilotlight/shaders/";
    tDefaultShaderOptions.eFlags = PL_SHADER_FLAGS_AUTO_OUTPUT | PL_SHADER_FLAGS_NEVER_CACHE;
    gptShader->initialize(&tDefaultShaderOptions);

    gptStarter->finalize();

    // return app memory
    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
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
    gptStarter->resize();
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    // this needs to be the first call when using the starter
    // extension. You must return if it returns false (usually a swapchain recreation).
    if(!gptStarter->begin_frame())
        return;

    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    // creating another window
    if(gptUI->begin_window("Pilot Light", NULL, PL_UI_WINDOW_FLAGS_NONE))
    {
        gptUI->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
        gptUI->end_window();
    }

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
    gptStarter->end_frame(); // must be the last function called when using the starter extension
}

//-----------------------------------------------------------------------------
// [SECTION] helpers
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
pl__load_extensions(plApiRegistryI* ptApiRegistry)
{

    const plExtensionRegistryI* ptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);

    ptExtensionRegistry->load("pl_platform_ext", "pl_load_platform_ext", "pl_unload_platform_ext", false);
    ptExtensionRegistry->load("pl_shader_ext", "pl_load_shader_ext", "pl_unload_shader_ext", false);
    ptExtensionRegistry->load("pl_graphics_ext", "pl_load_graphics_ext", "pl_unload_graphics_ext", false);
    ptExtensionRegistry->load("pl_log_ext", "pl_load_log_ext", "pl_unload_log_ext", false);
    ptExtensionRegistry->load("pl_profile_ext", "pl_load_profile_ext", "pl_unload_profile_ext", false);
    ptExtensionRegistry->load("pl_image_ext", "pl_load_image_ext", "pl_unload_image_ext", false);
    ptExtensionRegistry->load("pl_stats_ext", "pl_load_stats_ext", "pl_unload_stats_ext", false);
    ptExtensionRegistry->load("pl_rect_pack_ext", "pl_load_rect_pack_ext", "pl_unload_rect_pack_ext", false);
    ptExtensionRegistry->load("pl_string_intern_ext", "pl_load_string_intern_ext", "pl_unload_string_intern_ext", false);
    ptExtensionRegistry->load("pl_draw_ext", "pl_load_draw_ext", "pl_unload_draw_ext", false);
    ptExtensionRegistry->load("pl_job_ext", "pl_load_job_ext", "pl_unload_job_ext", false);
    ptExtensionRegistry->load("pl_gpu_allocators_ext", "pl_load_gpu_allocators_ext", "pl_unload_gpu_allocators_ext", false);
    ptExtensionRegistry->load("pl_ecs_ext", "pl_load_ecs_ext", "pl_unload_ecs_ext", false);
    ptExtensionRegistry->load("pl_tools_ext", "pl_load_tools_ext", "pl_unload_tools_ext", false);
    ptExtensionRegistry->load("pl_renderer_ext", "pl_load_renderer_ext", "pl_unload_renderer_ext", false);
    ptExtensionRegistry->load("pl_resource_ext", "pl_load_resource_ext", "pl_unload_resource_ext", false);
    ptExtensionRegistry->load("pl_ui_ext", "pl_load_ui_ext", "pl_unload_ui_ext", false);
    ptExtensionRegistry->load("pl_asset_tools_ext", "pl_load_asset_tools_ext", "pl_unload_asset_tools_ext", false);
    ptExtensionRegistry->load("pl_camera_ext", "pl_load_camera_ext", "pl_unload_camera_ext", false);
    ptExtensionRegistry->load("pl_animation_ext", "pl_load_animation_ext", "pl_unload_animation_ext", false);
    ptExtensionRegistry->load("pl_gizmo_ext", "pl_load_gizmo_ext", "pl_unload_gizmo_ext", false);
    ptExtensionRegistry->load("pl_console_ext", "pl_load_console_ext", "pl_unload_console_ext", false);
    ptExtensionRegistry->load("pl_screen_log_ext", "pl_load_screen_log_ext", "pl_unload_screen_log_ext", false);
    ptExtensionRegistry->load("pl_starter_ext", "pl_load_starter_ext", "pl_unload_starter_ext", false);
    ptExtensionRegistry->load("pl_physics_ext", "pl_load_physics_ext", "pl_unload_physics_ext", false);
    ptExtensionRegistry->load("pl_collision_ext", "pl_load_collision_ext", "pl_unload_collision_ext", false);
    ptExtensionRegistry->load("pl_bvh_ext", "pl_load_bvh_ext", "pl_unload_bvh_ext", false);
    ptExtensionRegistry->load("pl_config_ext", "pl_load_config_ext", "pl_unload_config_ext", false);
    ptExtensionRegistry->load("pl_mesh_ext", "pl_load_mesh_ext", "pl_unload_mesh_ext", false);
    ptExtensionRegistry->load("pl_shader_variant_ext", "pl_load_shader_variant_ext", "pl_unload_shader_variant_ext", false);
    ptExtensionRegistry->load("pl_datetime_ext", "pl_load_datetime_ext", "pl_unload_datetime_ext", false);
    ptExtensionRegistry->load("pl_vfs_ext", "pl_load_vfs_ext", "pl_unload_vfs_ext", false);
    ptExtensionRegistry->load("pl_compress_ext", "pl_load_compress_ext", "pl_unload_compress_ext", false);
    ptExtensionRegistry->load("pl_dds_ext", "pl_load_dds_ext", "pl_unload_dds_ext", false);
    ptExtensionRegistry->load("pl_dxt_ext", "pl_load_dxt_ext", "pl_unload_dxt_ext", false);
    ptExtensionRegistry->load("pl_pak_ext", "pl_load_pak_ext", "pl_unload_pak_ext", false);
    ptExtensionRegistry->load("pl_script_ext", "pl_load_script_ext", "pl_unload_script_ext", false);
    ptExtensionRegistry->load("pl_material_ext", "pl_load_material_ext", "pl_unload_material_ext", false);
    ptExtensionRegistry->load("pl_terrain_ext", "pl_load_terrain_ext", "pl_unload_terrain_ext", false);
    ptExtensionRegistry->load("pl_voxel_ext", "pl_load_voxel_ext", "pl_unload_voxel_ext", false);
    ptExtensionRegistry->load("pl_path_ext", "pl_load_path_ext", "pl_unload_path_ext", false);
    ptExtensionRegistry->load("pl_audio_ext", "pl_load_audio_ext", "pl_unload_audio_ext", false);
    ptExtensionRegistry->load("pl_freelist_ext", "pl_load_freelist_ext", "pl_unload_freelist_ext", false);
    ptExtensionRegistry->load("pl_stage_ext", "pl_load_stage_ext", "pl_unload_stage_ext", false);
    ptExtensionRegistry->load("pl_image_ops_ext", "pl_load_image_ops_ext", "pl_unload_image_ops_ext", false);
    ptExtensionRegistry->load("pl_gjk_ext", "pl_load_gjk_ext", "pl_unload_gjk_ext", false);
    ptExtensionRegistry->load("pl_ik_ext", "pl_load_ik_ext", "pl_unload_ik_ext", false);
    ptExtensionRegistry->load("pl_transform_ext", "pl_load_transform_ext", "pl_unload_transform_ext", false);
    ptExtensionRegistry->load("pl_asset_ext", "pl_load_asset_ext", "pl_unload_asset_ext", false);
    ptExtensionRegistry->load("pl_skeleton_ext", "pl_load_skeleton_ext", "pl_unload_skeleton_ext", false);
    ptExtensionRegistry->load("pl_json_ext", "pl_load_json_ext", "pl_unload_json_ext", false);
    ptExtensionRegistry->load("pl_stl_ext", "pl_load_stl_ext", "pl_unload_stl_ext", false);
    ptExtensionRegistry->load("pl_texture_ext", "pl_load_texture_ext", "pl_unload_texture_ext", false);
}