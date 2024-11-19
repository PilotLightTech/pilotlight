
//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_APP_H
#define PL_APP_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <float.h>
#include "pl.h"
#include "pl_memory.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extensions
#include "pl_image_ext.h"
#include "pl_profile_ext.h"
#include "pl_log_ext.h"
#include "pl_stats_ext.h"
#include "pl_ecs_ext.h"
#include "pl_graphics_ext.h"
#include "pl_debug_ext.h"
#include "pl_resource_ext.h"
#include "pl_model_loader_ext.h"
#include "pl_renderer_ext.h"
#include "pl_job_ext.h"
#include "pl_draw_ext.h"
#include "pl_draw_backend_ext.h"
#include "pl_ui_ext.h"
#include "pl_shader_ext.h"
#include "pl_string_intern_ext.h"
#include "pl_network_ext.h"
#include "pl_threads_ext.h"
#include "pl_atomics_ext.h"
#include "pl_window_ext.h"
#include "pl_library_ext.h"
#include "pl_file_ext.h"

// editor
#include "pl_gizmo.h"
#include "pl_ecs_tools.h"
#include "pl_icons.h"

//-----------------------------------------------------------------------------
// [SECTION] global apis
//-----------------------------------------------------------------------------

static const plWindowI*       gptWindows     = NULL;
static const plStatsI*        gptStats       = NULL;
static const plGraphicsI*     gptGfx         = NULL;
static const plDebugApiI*     gptDebug       = NULL;
static const plEcsI*          gptEcs         = NULL;
static const plCameraI*       gptCamera      = NULL;
static const plRendererI*     gptRenderer    = NULL;
static const plModelLoaderI*  gptModelLoader = NULL;
static const plJobI*          gptJobs        = NULL;
static const plDrawI*         gptDraw        = NULL;
static const plDrawBackendI*  gptDrawBackend = NULL;
static const plUiI*           gptUi          = NULL;
static const plIOI*           gptIO          = NULL;
static const plShaderI*       gptShader      = NULL;
static const plMemoryI*       gptMemory      = NULL;
static const plNetworkI*      gptNetwork     = NULL;
static const plStringInternI* gptString      = NULL;
static const plProfileI*      gptProfile     = NULL;

#define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
#define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

#define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
#define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] structs & enums
//-----------------------------------------------------------------------------


typedef struct _plEditorData
{

    // windows
    plWindow* ptWindow;

    // swapchains
    plSwapchain* ptSwap;

    // ui options
    plDebugApiInfo tDebugInfo;
    bool           bShowUiDebug;
    bool           bShowUiStyle;
    bool           bShowEntityWindow;
    bool           bResize;
    bool           bAlwaysResize;

    // scene
    bool         bFreezeCullCamera;
    plEntity     tCullCamera;
    plEntity     tMainCamera;
    plEntity     tSunlight;

    // views
    uint32_t uSceneHandle0;
    uint32_t uViewHandle0;

    // drawing
    plDrawLayer2D* ptDrawLayer;

    // selection stuff
    plEntity tSelectedEntity;
    
    // gizmo data
    plGizmoData* ptGizmoData;

    // fonts
    plFont* tDefaultFont;

    // experiment
    plEntity tTrackPoint;

} plEditorData;

#endif // PL_APP_H