#pragma once

/*
   editor.h

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
#include "pl_vfs_ext.h"
#include "pl_compress_ext.h"
#include "pl_pak_ext.h"
#include "pl_datetime_ext.h"

// unstable extensions
#include "pl_ecs_ext.h"
#include "pl_mesh_ext.h"
#include "pl_animation_ext.h"
#include "pl_camera_ext.h"
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

// shaders
#include "pl_shader_interop_renderer.h" // PL_MESH_FORMAT_FLAG_XXXX

// dear imgui
#include "pl_dear_imgui_ext.h"
#include "imgui.h"
#include "implot.h"

//-----------------------------------------------------------------------------
// [SECTION] global apis
//-----------------------------------------------------------------------------

const plWindowI*        gptWindows       = nullptr;
const plStatsI*         gptStats         = nullptr;
const plGraphicsI*      gptGfx           = nullptr;
const plToolsI*         gptTools         = nullptr;
const plEcsI*           gptEcs           = nullptr;
const plCameraI*        gptCamera        = nullptr;
const plRendererI*      gptRenderer      = nullptr;
const plModelLoaderI*   gptModelLoader   = nullptr;
const plJobI*           gptJobs          = nullptr;
const plDrawI*          gptDraw          = nullptr;
const plDrawBackendI*   gptDrawBackend   = nullptr;
const plUiI*            gptUI            = nullptr;
const plIOI*            gptIO            = nullptr;
const plShaderI*        gptShader        = nullptr;
const plMemoryI*        gptMemory        = nullptr;
const plNetworkI*       gptNetwork       = nullptr;
const plStringInternI*  gptString        = nullptr;
const plProfileI*       gptProfile       = nullptr;
const plFileI*          gptFile          = nullptr;
const plEcsToolsI*      gptEcsTools      = nullptr;
const plGizmoI*         gptGizmo         = nullptr;
const plConsoleI*       gptConsole       = nullptr;
const plScreenLogI*     gptScreenLog     = nullptr;
const plPhysicsI *      gptPhysics       = nullptr;
const plCollisionI*     gptCollision     = nullptr;
const plBVHI*           gptBvh           = nullptr;
const plConfigI*        gptConfig        = nullptr;
const plDearImGuiI*     gptDearImGui     = nullptr;
const plResourceI*      gptResource      = nullptr;
const plStarterI*       gptStarter       = nullptr;
const plAnimationI*     gptAnimation     = nullptr;
const plMeshI*          gptMesh          = nullptr;
const plShaderVariantI* gptShaderVariant = nullptr;
const plVfsI*           gptVfs           = nullptr;
const plPakI*           gptPak           = nullptr;
const plDateTimeI*      gptDateTime      = nullptr;
const plCompressI*      gptCompress      = nullptr;

#define PL_ALLOC(x)      gptMemory->tracked_realloc(nullptr, (x), __FILE__, __LINE__)
#define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
#define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

#define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(nullptr, (x), __FILE__, __LINE__)
#define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(nullptr, (x), FILE, LINE)
#define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
#include "pl_ds.h"

#define PL_JSON_ALLOC(x) gptMemory->tracked_realloc(nullptr, (x), __FILE__, __LINE__)
#define PL_JSON_FREE(x)  gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plTestModelVariant
{
    char acType[64];
    char acName[128];
    char acFilePath[1024];
} plTestModelVariant;

typedef struct _plTestModel
{
    char acLabel[256];
    char acName[128];
    
    bool bCore;
    bool bExtension;
    bool bTesting;
    bool bSelected;

    uint32_t uVariantCount;
    plTestModelVariant acVariants[8];

} plTestModel;

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
    bool  bSecondaryViewActive;
    bool  bShowBVH;
    bool  bFrustumCulling;
    bool  bShowImGuiDemo;
    bool  bContinuousBVH;
    bool  bShowSkybox;
    bool  bShowGrid;
    bool  bShowPlotDemo;
    bool  bShowUiDemo;
    bool  bShowUiDebug;
    bool  bShowUiStyle;
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
    bool     bFreezeCullCamera;
    plEntity tCullCamera;
    plEntity tMainCamera;
    plEntity tSecondaryCamera;
    bool     bMainViewHovered;

    // scenes/views
    plComponentLibrary* ptCompLibrary;
    plScene* ptScene;
    plView*  ptView;
    plView*  ptSecondaryView;
    plVec2 tView0Offset;
    plVec2 tView0Scale;

    // drawing
    plDrawLayer2D* ptDrawLayer;

    // selection stuff
    plEntity tSelectedEntity;
    
    // fonts
    plFont* tDefaultFont;

    // test models
    ImGuiTextFilter tFilter;
    plTestModel*   sbtTestModels;

    // physics
    bool bPhysicsDebugDraw;

    // misc
    char* sbcTempBuffer;
    ImGuiTextFilter filter;
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] helper forward declarations
//-----------------------------------------------------------------------------

void pl__find_models       (plAppData*);
void pl__create_scene      (plAppData*);
void pl__show_editor_window(plAppData*);
void pl__show_ui_demo_window(plAppData* ptAppData);
void pl__camera_update_imgui(plCamera*);
void pl__show_entity_components(plAppData*, plScene*, plEntity);