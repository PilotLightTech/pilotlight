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

// dear imgui
#include "pl_dear_imgui_ext.h"
#include "imgui.h"
#include "implot.h"

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
const plDearImGuiI*    gptDearImGui   = NULL;
const plResourceI*     gptResource    = NULL;
const plStarterI*      gptStarter     = NULL;

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
    plSwapchain* ptSwap;
    plSurface*   ptSurface;

    // swapchains
    bool bResize;

    // ui options
    bool  bShowImGuiDemo;
    bool  bShowPlotDemo;
    bool  bShowUiDemo;
    bool  bShowUiDebug;
    bool  bShowUiStyle;
    bool  bShowEntityWindow;
    bool  bShowPilotLightTool;
    bool* pbShowDeviceMemoryAnalyzer;
    bool* pbShowMemoryAllocations;
    bool* pbShowProfiling;
    bool* pbShowStats;
    bool* pbShowLogging;

    // scene
    bool     bFreezeCullCamera;
    plEntity tCullCamera;
    plEntity tMainCamera;
    bool     bMainViewHovered;

    // scenes/views
    uint32_t uSceneHandle0;
    uint32_t uViewHandle0;
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
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] helper forward declarations
//-----------------------------------------------------------------------------

void pl__find_models       (plAppData*);
void pl__create_scene      (plAppData*);
void pl__show_editor_window(plAppData*);
void pl__show_ui_demo_window(plAppData* ptAppData);
void pl__camera_update_imgui(plCameraComponent*);
void pl__show_entity_components(plAppData*, uint32_t, plEntity);