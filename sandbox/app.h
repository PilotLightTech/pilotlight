#pragma once

/*
   sandbox.h

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
#include "pl_ecs_tools_ext.h"
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
const plRendererDebugI*     gptRendererDebug    = NULL;
const plRendererEditorI*    gptRendererEditor   = NULL;
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

typedef struct _plSandboxEnvironment
{
    char acName[PL_MAX_PATH_LENGTH];
    char acPath[PL_MAX_PATH_LENGTH];
} plSandboxEnvironment;

typedef struct _plAppData
{
    // windows
    plWindow* ptWindow;

    // graphics
    plDevice*    ptDevice;
    plDeviceInfo tDeviceInfo;

    // swapchains
    bool bResize;
    bool bVSync;

    // ui options
    // bool bContinuousBVH;

    // pilot light ui windows
    bool  bAttached;
    bool  bShowUiDemo;
    bool  bShowUiDebug;
    bool  bShowUiStyle;
    bool  bShowEntityWindow;
    bool* pbShowDeviceMemoryAnalyzer;
    bool* pbShowMemoryAllocations;
    bool* pbShowProfiling;
    bool* pbShowStats;
    bool* pbShowLogging;
    bool* pbShowAssets;

    // scene
    plEntity tMainCamera;
    bool     bMainViewHovered;
    bool     bHasTerrain;

    // scenes/views
    plRenderScene* ptScene;
    plView*  ptView;
    plVec2 tView0Offset;
    plVec2 tView0Scale;
    plAssetHandle tSceneHandle;

    // drawing
    plDrawLayer2D* ptDrawLayer;

    // selection stuff
    plEntity tSelectedEntity;
    
    // fonts
    plFont* tDefaultFont;

    // physics
    // bool bPhysicsDebugDraw;

    // misc
    char* sbcTempBuffer;

    // scene file info
    char acCurrentScene[PL_MAX_PATH_LENGTH];
    plSandboxEnvironment* sbtSceneEnvironments;
    plSandboxSceneFile* sbtSceneFilesCore;
    int iSelectedSceneCore;
    int iSelectedEnvironment;

    // ui options
    bool bMSAA;
    bool bContinuousBVH;
    bool bPhysicsDebugDraw;
    bool bShowBVH;
    bool bFrustumCulling;
    bool bShowDebugLights;
    bool bDrawAllBoundingBoxes;

} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] helper forward declarations
//-----------------------------------------------------------------------------

void pl__show_editor_window(plAppData*);
void pl__show_ui_demo_window(plAppData* ptAppData);

void pl__load_apis(plApiRegistryI*);
void pl__refresh_files(plAppData*);