
//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_EDITOR_H
#define PL_EDITOR_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <float.h>
#include "pilotlight.h"
#include "pl_profile.h"
#include "pl_log.h"
#include "pl_ds.h"
#include "pl_os.h"
#include "pl_memory.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extensions
#include "pl_image_ext.h"
#include "pl_stats_ext.h"
#include "pl_graphics_ext.h"
#include "pl_debug_ext.h"
#include "pl_ecs_ext.h"
#include "pl_resource_ext.h"
#include "pl_model_loader_ext.h"
#include "pl_ref_renderer_ext.h"
#include "pl_job_ext.h"
#include "pl_draw_ext.h"
#include "pl_ui_ext.h"

// editor
#include "pl_gizmo.h"
#include "pl_ecs_tools.h"
#include "pl_icons.h"

//-----------------------------------------------------------------------------
// [SECTION] global apis
//-----------------------------------------------------------------------------

static const plWindowI*      gptWindows           = NULL;
static const plStatsI*       gptStats             = NULL;
static const plGraphicsI*    gptGfx               = NULL;
static const plDeviceI*      gptDevice            = NULL;
static const plDebugApiI*    gptDebug             = NULL;
static const plEcsI*         gptEcs               = NULL;
static const plCameraI*      gptCamera            = NULL;
static const plRefRendererI* gptRenderer          = NULL;
static const plModelLoaderI* gptModelLoader       = NULL;
static const plJobI*         gptJobs              = NULL;
static const plDrawI*        gptDraw              = NULL;
static const plUiI*          gptUi                = NULL;
static const plIOI*          gptIO                = NULL;

//-----------------------------------------------------------------------------
// [SECTION] structs & enums
//-----------------------------------------------------------------------------


typedef struct _plEditorData
{

    // windows
    plWindow* ptWindow;

    // ui options
    plDebugApiInfo tDebugInfo;
    bool           bShowUiDemo;
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
    plFontHandle tDefaultFont;

    // experiment
    plEntity tTrackPoint;

} plEditorData;

#endif // PL_EDITOR_H