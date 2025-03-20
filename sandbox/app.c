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
#include "pl_console_ext.h"
#include "pl_screen_log_ext.h"

// unstable extensions
#include "pl_ecs_ext.h"
#include "pl_resource_ext.h"
#include "pl_model_loader_ext.h"
#include "pl_renderer_ext.h"
#include "pl_ecs_tools_ext.h"
#include "pl_gizmo_ext.h"
#include "pl_physics_ext.h"
#include "pl_collision_ext.h"

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

    // swapchains
    bool bResize;

    // ui options
    bool  bEditorAttached;
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

    // scenes/views
    uint32_t uSceneHandle0;
    uint32_t uViewHandle0;

    // drawing
    plDrawLayer2D* ptDrawLayer;

    // selection stuff
    plEntity tSelectedEntity;
    
    // fonts
    plFont* tDefaultFont;

    // test models
    plUiTextFilter tFilter;
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
        gptWindows     = pl_get_api_latest(ptApiRegistry, plWindowI);
        gptStats       = pl_get_api_latest(ptApiRegistry, plStatsI);
        gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);
        gptTools       = pl_get_api_latest(ptApiRegistry, plToolsI);
        gptEcs         = pl_get_api_latest(ptApiRegistry, plEcsI);
        gptCamera      = pl_get_api_latest(ptApiRegistry, plCameraI);
        gptRenderer    = pl_get_api_latest(ptApiRegistry, plRendererI);
        gptJobs        = pl_get_api_latest(ptApiRegistry, plJobI);
        gptModelLoader = pl_get_api_latest(ptApiRegistry, plModelLoaderI);
        gptDraw        = pl_get_api_latest(ptApiRegistry, plDrawI);
        gptDrawBackend = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
        gptUI          = pl_get_api_latest(ptApiRegistry, plUiI);
        gptIO          = pl_get_api_latest(ptApiRegistry, plIOI);
        gptShader      = pl_get_api_latest(ptApiRegistry, plShaderI);
        gptMemory      = pl_get_api_latest(ptApiRegistry, plMemoryI);
        gptNetwork     = pl_get_api_latest(ptApiRegistry, plNetworkI);
        gptString      = pl_get_api_latest(ptApiRegistry, plStringInternI);
        gptProfile     = pl_get_api_latest(ptApiRegistry, plProfileI);
        gptFile        = pl_get_api_latest(ptApiRegistry, plFileI);
        gptEcsTools    = pl_get_api_latest(ptApiRegistry, plEcsToolsI);
        gptGizmo       = pl_get_api_latest(ptApiRegistry, plGizmoI);
        gptConsole     = pl_get_api_latest(ptApiRegistry, plConsoleI);
        gptScreenLog   = pl_get_api_latest(ptApiRegistry, plScreenLogI);
        gptPhysics     = pl_get_api_latest(ptApiRegistry, plPhysicsI);
        gptCollision   = pl_get_api_latest(ptApiRegistry, plCollisionI);

        gptScreenLog->add_message_ex(0, 15.0, PL_COLOR_32_MAGENTA, 1.5f, "%s", "App Hot Reloaded");

        return ptAppData;
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~apis & extensions~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // retrieve extension registry
    const plExtensionRegistryI* ptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);

    // load extensions
    ptExtensionRegistry->load("pl_unity_ext", NULL, NULL, true);
    
    // load apis
    gptWindows     = pl_get_api_latest(ptApiRegistry, plWindowI);
    gptStats       = pl_get_api_latest(ptApiRegistry, plStatsI);
    gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptTools       = pl_get_api_latest(ptApiRegistry, plToolsI);
    gptEcs         = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptCamera      = pl_get_api_latest(ptApiRegistry, plCameraI);
    gptRenderer    = pl_get_api_latest(ptApiRegistry, plRendererI);
    gptJobs        = pl_get_api_latest(ptApiRegistry, plJobI);
    gptModelLoader = pl_get_api_latest(ptApiRegistry, plModelLoaderI);
    gptDraw        = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptDrawBackend = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
    gptUI          = pl_get_api_latest(ptApiRegistry, plUiI);
    gptIO          = pl_get_api_latest(ptApiRegistry, plIOI);
    gptShader      = pl_get_api_latest(ptApiRegistry, plShaderI);
    gptMemory      = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptNetwork     = pl_get_api_latest(ptApiRegistry, plNetworkI);
    gptString      = pl_get_api_latest(ptApiRegistry, plStringInternI);
    gptProfile     = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptFile        = pl_get_api_latest(ptApiRegistry, plFileI);
    gptEcsTools    = pl_get_api_latest(ptApiRegistry, plEcsToolsI);
    gptGizmo       = pl_get_api_latest(ptApiRegistry, plGizmoI);
    gptConsole     = pl_get_api_latest(ptApiRegistry, plConsoleI);
    gptScreenLog   = pl_get_api_latest(ptApiRegistry, plScreenLogI);
    gptPhysics     = pl_get_api_latest(ptApiRegistry, plPhysicsI);
    gptCollision   = pl_get_api_latest(ptApiRegistry, plCollisionI);

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = PL_ALLOC(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    // defaults
    ptAppData->tSelectedEntity.ulData = UINT64_MAX;
    ptAppData->uSceneHandle0 = UINT32_MAX;
    ptAppData->bShowPilotLightTool = true;
    ptAppData->bEditorAttached = true;

    // add console variables
    gptConsole->initialize((plConsoleSettings){.tFlags = PL_CONSOLE_FLAGS_POPUP});
    gptConsole->add_toggle_variable("a.PilotLight", &ptAppData->bShowPilotLightTool, "shows main pilot light window", PL_CONSOLE_VARIABLE_FLAGS_CLOSE_CONSOLE);
    gptConsole->add_toggle_variable("a.Entities", &ptAppData->bShowEntityWindow, "shows ecs tool", PL_CONSOLE_VARIABLE_FLAGS_CLOSE_CONSOLE);
    gptConsole->add_toggle_variable("a.FreezeCullCamera", &ptAppData->bFreezeCullCamera, "freezes culling camera", PL_CONSOLE_VARIABLE_FLAGS_CLOSE_CONSOLE);

    // initialize APIs that require it
    gptEcsTools->initialize();
    gptPhysics->initialize((plPhysicsEngineSettings){0});
    
    // initialize shader extension
    static plShaderOptions tDefaultShaderOptions = {
        .apcIncludeDirectories = {
            "../shaders/"
        },
        .apcDirectories = {
            "../shaders/"
        },
        .tFlags = PL_SHADER_FLAGS_AUTO_OUTPUT | PL_SHADER_FLAGS_INCLUDE_DEBUG | PL_SHADER_FLAGS_ALWAYS_COMPILE

    };
    gptShader->initialize(&tDefaultShaderOptions);

    // initialize job system
    gptJobs->initialize(0);

    // create window (only 1 allowed currently)
    plWindowDesc tWindowDesc = {
        .pcTitle = "Pilot Light Sandbox",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 600,
        .uHeight = 600,
    };
    gptWindows->create_window(tWindowDesc, &ptAppData->ptWindow);

    // setup reference renderer
    plRendererSettings tRenderSettings = {
        .ptWindow              = ptAppData->ptWindow,
        .uMaxTextureResolution = 1024
    };
    gptRenderer->initialize(tRenderSettings);

    gptTools->initialize((plToolsInit){.ptDevice = gptRenderer->get_device()});

    // retrieve some console variables
    ptAppData->pbShowLogging              = (bool*)gptConsole->get_variable("t.LogTool", NULL, NULL);
    ptAppData->pbShowStats                = (bool*)gptConsole->get_variable("t.StatTool", NULL, NULL);
    ptAppData->pbShowProfiling            = (bool*)gptConsole->get_variable("t.ProfileTool", NULL, NULL);
    ptAppData->pbShowMemoryAllocations    = (bool*)gptConsole->get_variable("t.MemoryAllocationTool", NULL, NULL);
    ptAppData->pbShowDeviceMemoryAnalyzer = (bool*)gptConsole->get_variable("t.DeviceMemoryAnalyzerTool", NULL, NULL);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~setup draw extensions~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // initialize
    gptDraw->initialize(NULL);
    gptDrawBackend->initialize(gptRenderer->get_device());

    // create font atlas
    plFontAtlas* ptAtlas = gptDraw->create_font_atlas();
    gptDraw->set_font_atlas(ptAtlas);

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
    ptAppData->tDefaultFont = gptDraw->add_font_from_file_ttf(ptAtlas, tFontConfig0, "../data/pilotlight-assets-master/fonts/Cousine-Regular.ttf");

    const plFontRange tIconRange = {
        .iFirstCodePoint = ICON_MIN_FA,
        .uCharCount = ICON_MAX_16_FA - ICON_MIN_FA
    };

    plFontConfig tFontConfig1 = {
        .bSdf           = false,
        .fSize          = 16.0f,
        .uHOverSampling = 1,
        .uVOverSampling = 1,
        .ptMergeFont    = ptAppData->tDefaultFont,
        .ptRanges       = &tIconRange,
        .uRangeCount    = 1
    };
    gptDraw->add_font_from_file_ttf(ptAtlas, tFontConfig1, "../data/pilotlight-assets-master/fonts/fa-solid-900.otf");

    // build font atlas
    plCommandBuffer* ptCmdBuffer = gptGfx->request_command_buffer(gptRenderer->get_command_pool());
    gptDrawBackend->build_font_atlas(ptCmdBuffer, ptAtlas);
    gptGfx->return_command_buffer(ptCmdBuffer);
    
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~message extension~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    gptScreenLog->initialize((plScreenLogSettings){.ptFont = ptAppData->tDefaultFont});

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ui extension~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    gptUI->initialize();
    gptUI->set_default_font(ptAppData->tDefaultFont);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~app stuff~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // temporary draw layer for submitting fullscreen quad of offscreen render
    ptAppData->ptDrawLayer = gptDraw->request_2d_layer(gptUI->get_draw_list());

    pl__find_models(ptAppData);

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
    gptGfx->flush_device(gptRenderer->get_device());
    gptDrawBackend->cleanup_font_atlas(gptDraw->get_current_font_atlas());
    gptUI->cleanup();
    gptEcsTools->cleanup();
    gptPhysics->cleanup();
    gptConsole->cleanup();
    gptScreenLog->cleanup();
    gptDrawBackend->cleanup();
    gptRenderer->cleanup();
    gptWindows->destroy_window(ptAppData->ptWindow);
    gptUI->text_filter_cleanup(&ptAppData->tFilter);
    pl_sb_free(ptAppData->sbtTestModels);
    PL_FREE(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plAppData* ptAppData)
{
    plIO* ptIO = gptIO->get_io();
    if(ptAppData->uSceneHandle0 != UINT32_MAX)
        gptCamera->set_aspect(gptEcs->get_component(gptRenderer->get_component_library(ptAppData->uSceneHandle0), PL_COMPONENT_TYPE_CAMERA, ptAppData->tMainCamera), ptIO->tMainViewportSize.x / ptIO->tMainViewportSize.y);
    ptAppData->bResize = true;
    gptRenderer->resize();
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    gptProfile->begin_frame();
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    gptIO->new_frame();

    // for convience
    plIO* ptIO = gptIO->get_io();

    if(!gptRenderer->begin_frame())
    {
        pl_end_cpu_sample(gptProfile, 0);
        gptProfile->end_frame();
        return;
    }

    if(ptAppData->bResize)
    {
        // gptOS->sleep(32);
        if(ptAppData->uSceneHandle0 != UINT32_MAX)
            gptRenderer->resize_view(ptAppData->uSceneHandle0, ptAppData->uViewHandle0, ptIO->tMainViewportSize);
        ptAppData->bResize = false;
    }

    gptDrawBackend->new_frame();
    gptUI->new_frame();

    // update statistics
    gptStats->new_frame();
    static double* pdFrameTimeCounter = NULL;
    static double* pdMemoryCounter = NULL;
    if(!pdFrameTimeCounter)
        pdFrameTimeCounter = gptStats->get_counter("frametime (ms)");
    if(!pdMemoryCounter)
        pdMemoryCounter = gptStats->get_counter("CPU memory");
    *pdFrameTimeCounter = (double)ptIO->fDeltaTime * 1000.0;
    *pdMemoryCounter = (double)gptMemory->get_memory_usage();

    if(ptAppData->uSceneHandle0 != UINT32_MAX)
    {
        plComponentLibrary* ptMainComponentLibrary = gptRenderer->get_component_library(ptAppData->uSceneHandle0);
        plCameraComponent*  ptCamera = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_CAMERA, ptAppData->tMainCamera);
        plCameraComponent*  ptCullCamera = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_CAMERA, ptAppData->tCullCamera);
        gptCamera->update(ptCullCamera);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~selection stuff~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        if(!gptUI->wants_mouse_capture() && !gptGizmo->active())
        {
            static plVec2 tClickPos = {0};
            if(gptIO->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
            {
                tClickPos = gptIO->get_mouse_pos();
            }
            else if(gptIO->is_mouse_released(PL_MOUSE_BUTTON_LEFT))
            {
                plVec2 tReleasePos = gptIO->get_mouse_pos();

                if(tReleasePos.x == tClickPos.x && tReleasePos.y == tClickPos.y)
                    gptRenderer->update_hovered_entity(ptAppData->uSceneHandle0, ptAppData->uViewHandle0);
            }
        }

        // run ecs system
        gptRenderer->run_ecs(ptAppData->uSceneHandle0);

        plEntity tNextEntity = {0};
        if(gptRenderer->get_hovered_entity(ptAppData->uSceneHandle0, ptAppData->uViewHandle0, &tNextEntity))
        {
            
            if(tNextEntity.ulData == 0)
            {
                ptAppData->tSelectedEntity.ulData = UINT64_MAX;
                gptRenderer->outline_entities(ptAppData->uSceneHandle0, 0, NULL);
            }
            else if(ptAppData->tSelectedEntity.ulData != tNextEntity.ulData)
            {
                gptScreenLog->add_message_ex(565168477883, 5.0, PL_COLOR_32_RED, 1.0f, "Selected Entity {%u, %u}", tNextEntity.uIndex, tNextEntity.uGeneration);
                gptRenderer->outline_entities(ptAppData->uSceneHandle0, 1, &tNextEntity);
                ptAppData->tSelectedEntity = tNextEntity;
                gptPhysics->set_angular_velocity(gptRenderer->get_component_library(ptAppData->uSceneHandle0), tNextEntity, (plVec3){0});
                gptPhysics->set_linear_velocity(gptRenderer->get_component_library(ptAppData->uSceneHandle0), tNextEntity, (plVec3){0});
            }

        }

        if(gptIO->is_key_pressed(PL_KEY_M, true))
            gptGizmo->next_mode();

        if(ptAppData->bShowEntityWindow)
        {
            if(gptEcsTools->show_ecs_window(&ptAppData->tSelectedEntity, ptAppData->uSceneHandle0, &ptAppData->bShowEntityWindow))
            {
                if(ptAppData->tSelectedEntity.ulData == UINT64_MAX)
                {
                    gptRenderer->outline_entities(ptAppData->uSceneHandle0, 0, NULL);
                }
                else
                {
                    gptRenderer->outline_entities(ptAppData->uSceneHandle0, 1, &ptAppData->tSelectedEntity);
                }
            }
        }

        if(ptAppData->tSelectedEntity.uIndex != UINT32_MAX)
        {
            plDrawList3D* ptGizmoDrawlist =  gptRenderer->get_gizmo_drawlist(ptAppData->uSceneHandle0, ptAppData->uViewHandle0);
            plObjectComponent* ptSelectedObject = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_OBJECT, ptAppData->tSelectedEntity);
            plTransformComponent* ptSelectedTransform = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptAppData->tSelectedEntity);
            plTransformComponent* ptParentTransform = NULL;
            plHierarchyComponent* ptHierarchyComp = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_HIERARCHY, ptAppData->tSelectedEntity);
            if(ptHierarchyComp)
            {
                ptParentTransform = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptHierarchyComp->tParent);
            }
            if(ptSelectedTransform)
            {
                gptGizmo->gizmo(ptGizmoDrawlist, ptCamera, ptSelectedTransform, ptParentTransform);
            }
            else if(ptSelectedObject)
            {
                ptSelectedTransform = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptSelectedObject->tTransform);
                gptGizmo->gizmo(ptGizmoDrawlist, ptCamera, ptSelectedTransform, ptParentTransform);
            }
        }

        if(ptAppData->bPhysicsDebugDraw)
        {
            plDrawList3D* ptDrawlist = gptRenderer->get_debug_drawlist(ptAppData->uSceneHandle0, ptAppData->uViewHandle0);
            gptPhysics->draw(ptMainComponentLibrary, ptDrawlist);
        }
    
        // render scene
        const plViewOptions tViewOptions = {
            .ptViewCamera = &ptAppData->tMainCamera,
            .ptCullCamera = ptAppData->bFreezeCullCamera ? &ptAppData->tCullCamera : NULL
        };
        gptRenderer->render_scene(ptAppData->uSceneHandle0, &ptAppData->uViewHandle0, &tViewOptions, 1);
    }

    if(gptIO->is_key_pressed(PL_KEY_F1, false))
    {
        gptConsole->open();
    }

    gptConsole->update();

    // main "editor" debug window
    if(ptAppData->bShowPilotLightTool)
        pl__show_editor_window(ptAppData);

    if(ptAppData->bShowUiDemo)
        pl__show_ui_demo_window(ptAppData);

    gptTools->update();
        
    if(ptAppData->bShowUiStyle)
        gptUI->show_style_editor_window(&ptAppData->bShowUiStyle);

    if(ptAppData->bShowUiDebug)
        gptUI->show_debug_window(&ptAppData->bShowUiDebug);

    // add full screen quad for offscreen render
    if(ptAppData->uSceneHandle0 != UINT32_MAX)
        gptDraw->add_image(ptAppData->ptDrawLayer, gptRenderer->get_view_color_texture(ptAppData->uSceneHandle0, ptAppData->uViewHandle0).uData, (plVec2){0}, ptIO->tMainViewportSize);

    gptDraw->submit_2d_layer(ptAppData->ptDrawLayer);

    gptRenderer->end_frame();

    pl_end_cpu_sample(gptProfile, 0);
    gptProfile->end_frame();
}

//-----------------------------------------------------------------------------
// [SECTION] helper implementations
//-----------------------------------------------------------------------------

void
pl__find_models(plAppData* ptAppData)
{
    if(gptFile->exists("../data/pilotlight-assets-master/models/gltf/humanoid/model.gltf"))
    {
        plTestModel tModel = {
            .uVariantCount = 1,
            .bSelected = true
        };
        strcpy(tModel.acName, "Fembot");
        strcpy(tModel.acVariants[0].acType, "glTF");
        strcpy(tModel.acVariants[0].acFilePath, "../data/pilotlight-assets-master/models/gltf/humanoid/model.gltf");
        pl_sb_push(ptAppData->sbtTestModels, tModel);
    }

    if(gptFile->exists("../data/pilotlight-assets-master/models/gltf/humanoid/floor.gltf"))
    {
        plTestModel tModel = {
            .uVariantCount = 1,
            .bSelected = true
        };
        strcpy(tModel.acName, "Floor");
        strcpy(tModel.acVariants[0].acType, "glTF");
        strcpy(tModel.acVariants[0].acFilePath, "../data/pilotlight-assets-master/models/gltf/humanoid/floor.gltf");
        pl_sb_push(ptAppData->sbtTestModels, tModel);
    }

    if(gptFile->exists("../sandbox/model-index.json"))
    {
        size_t szJsonFileSize = 0;
        gptFile->binary_read("../sandbox/model-index.json", &szJsonFileSize, NULL);
        uint8_t* puFileBuffer = PL_ALLOC(szJsonFileSize + 1);
        memset(puFileBuffer, 0, szJsonFileSize + 1);

        gptFile->binary_read("../sandbox/model-index.json", &szJsonFileSize, puFileBuffer);

        plJsonObject* ptRootJsonObject = NULL;
        pl_load_json((const char*)puFileBuffer, &ptRootJsonObject);

        plJsonObject* ptModelList = pl_json_member_by_index(ptRootJsonObject, 0);

        uint32_t uTestModelCount = 0;
        pl_json_member_list(ptModelList, NULL, &uTestModelCount, NULL);

        pl_sb_reserve(ptAppData->sbtTestModels, pl_sb_size(ptAppData->sbtTestModels) + uTestModelCount);

        for(uint32_t i = 0; i < uTestModelCount; i++)
        {
            plJsonObject* ptModelObject = pl_json_member_by_index(ptModelList, i);

            plTestModel tTestModel = {0};

            pl_json_string_member(ptModelObject, "label", tTestModel.acLabel, 256);
            pl_json_string_member(ptModelObject, "name", tTestModel.acName, 128);

            plJsonObject* ptVariantsObject = pl_json_member(ptModelObject, "variants");

            char* acVariantNames[8] = {0};
            for(uint32_t j = 0; j < 8; j++)
            { 
                acVariantNames[j] = tTestModel.acVariants[j].acType;
            }
            pl_json_member_list(ptVariantsObject, acVariantNames, &tTestModel.uVariantCount, NULL);

            for(uint32_t j = 0; j < tTestModel.uVariantCount; j++)
            {
                pl_json_string_member(ptVariantsObject, tTestModel.acVariants[j].acType, tTestModel.acVariants[j].acName, 128);
                pl_sprintf(tTestModel.acVariants[j].acFilePath, "%s", tTestModel.acVariants[j].acName);
            }

            char acTag0[64] = {0};
            char acTag1[64] = {0};
            char acTag2[64] = {0};
            char acTag3[64] = {0};
            char acTag4[64] = {0};

            char* acTags[] = {
                acTag0,
                acTag1,
                acTag2,
                acTag3,
                acTag4,
            };
            uint32_t uTagCount = 0;
            uint32_t uTagLength = 64;
            pl_json_string_array_member(ptVariantsObject, "tags", acTags, &uTagCount, &uTagLength);

            for(uint32_t j = 0; j < uTagCount; j++)
            {
                if(pl_str_equal(acTags[j], "core"))
                    tTestModel.bCore = true;
                if(pl_str_equal(acTags[j], "extensions"))
                    tTestModel.bExtension = true;
                if(pl_str_equal(acTags[j], "testing"))
                    tTestModel.bTesting = true;
            }

            pl_sb_push(ptAppData->sbtTestModels, tTestModel);
        }

        pl_unload_json(&ptRootJsonObject);

        PL_FREE(puFileBuffer);
    }

    if(gptFile->exists("../data/glTF-Sample-Assets-main/Models/model-index.json"))
    {
        size_t szJsonFileSize = 0;
        gptFile->binary_read("../data/glTF-Sample-Assets-main/Models/model-index.json", &szJsonFileSize, NULL);
        uint8_t* puFileBuffer = PL_ALLOC(szJsonFileSize + 1);
        memset(puFileBuffer, 0, szJsonFileSize + 1);

        gptFile->binary_read("../data/glTF-Sample-Assets-main/Models/model-index.json", &szJsonFileSize, puFileBuffer);

        plJsonObject* ptRootJsonObject = NULL;
        pl_load_json((const char*)puFileBuffer, &ptRootJsonObject);

        plJsonObject* ptModelList = pl_json_member_by_index(ptRootJsonObject, 0);

        uint32_t uTestModelCount = 0;
        pl_json_member_list(ptModelList, NULL, &uTestModelCount, NULL);

        pl_sb_reserve(ptAppData->sbtTestModels, pl_sb_size(ptAppData->sbtTestModels) + uTestModelCount);

        for(uint32_t i = 0; i < uTestModelCount; i++)
        {
            plJsonObject* ptModelObject = pl_json_member_by_index(ptModelList, i);

            plTestModel tTestModel = {0};

            pl_json_string_member(ptModelObject, "label", tTestModel.acLabel, 256);
            pl_json_string_member(ptModelObject, "name", tTestModel.acName, 128);

            plJsonObject* ptVariantsObject = pl_json_member(ptModelObject, "variants");

            char* acVariantNames[8] = {0};
            for(uint32_t j = 0; j < 8; j++)
            { 
                acVariantNames[j] = tTestModel.acVariants[j].acType;
            }
            pl_json_member_list(ptVariantsObject, acVariantNames, &tTestModel.uVariantCount, NULL);

            for(uint32_t j = 0; j < tTestModel.uVariantCount; j++)
            {
                pl_json_string_member(ptVariantsObject, tTestModel.acVariants[j].acType, tTestModel.acVariants[j].acName, 128);
                pl_sprintf(tTestModel.acVariants[j].acFilePath, "../data/glTF-Sample-Assets-main/Models/%s/%s/%s", tTestModel.acName, tTestModel.acVariants[j].acType, tTestModel.acVariants[j].acName);
            }

            char acTag0[64] = {0};
            char acTag1[64] = {0};
            char acTag2[64] = {0};
            char acTag3[64] = {0};
            char acTag4[64] = {0};

            char* acTags[] = {
                acTag0,
                acTag1,
                acTag2,
                acTag3,
                acTag4,
            };
            uint32_t uTagCount = 0;
            uint32_t uTagLength = 64;
            pl_json_string_array_member(ptVariantsObject, "tags", acTags, &uTagCount, &uTagLength);

            for(uint32_t j = 0; j < uTagCount; j++)
            {
                if(pl_str_equal(acTags[j], "core"))
                    tTestModel.bCore = true;
                if(pl_str_equal(acTags[j], "extensions"))
                    tTestModel.bExtension = true;
                if(pl_str_equal(acTags[j], "testing"))
                    tTestModel.bTesting = true;
            }

            pl_sb_push(ptAppData->sbtTestModels, tTestModel);
        }

        pl_unload_json(&ptRootJsonObject);

        PL_FREE(puFileBuffer);
    }
}

void
pl__show_editor_window(plAppData* ptAppData)
{
    plIO* ptIO = gptIO->get_io();

    plUiWindowFlags tWindowFlags = PL_UI_WINDOW_FLAGS_NONE;

    if(ptAppData->bEditorAttached)
    {
        tWindowFlags = PL_UI_WINDOW_FLAGS_NO_TITLE_BAR | PL_UI_WINDOW_FLAGS_NO_RESIZE | PL_UI_WINDOW_FLAGS_HORIZONTAL_SCROLLBAR;
        gptUI->set_next_window_pos((plVec2){0, 0}, PL_UI_COND_ALWAYS);
        gptUI->set_next_window_size((plVec2){600.0f, ptIO->tMainViewportSize.y}, PL_UI_COND_ALWAYS);
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
            gptUI->checkbox("Editor Attached", &ptAppData->bEditorAttached);
            if(ptAppData->uSceneHandle0 != UINT32_MAX)
            {
                if(gptUI->checkbox("Freeze Culling Camera", &ptAppData->bFreezeCullCamera))
                {
                    plComponentLibrary* ptMainComponentLibrary = gptRenderer->get_component_library(ptAppData->uSceneHandle0);
                    plCameraComponent*  ptCamera = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_CAMERA, ptAppData->tMainCamera);
                    plCameraComponent*  ptCullCamera = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_CAMERA, ptAppData->tCullCamera);
                    *ptCullCamera = *ptCamera;
                }
            }

            gptUI->vertical_spacing();

            const float pfWidths[] = {150.0f, 150.0f};
            gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 2, pfWidths);

            bool bLoadScene = false;

            if(ptAppData->uSceneHandle0 != UINT32_MAX)
            {
                if(gptUI->button("Unload Scene"))
                {
                    gptPhysics->reset();
                    gptRenderer->cleanup_scene(ptAppData->uSceneHandle0);
                    ptAppData->uSceneHandle0 = UINT32_MAX;
                }
            }
            else
            {
                if(gptUI->button("Load Scene"))
                {
                    bLoadScene = true;
                }
            }

            if(gptUI->button("Reset Selection"))
            {
                uint32_t uTestModelCount = pl_sb_size(ptAppData->sbtTestModels);
                for(uint32_t i = 0; i < uTestModelCount; i++)
                {
                    ptAppData->sbtTestModels[i].bSelected = false;
                }
            }

            if(ptAppData->uSceneHandle0 == UINT32_MAX)
            {

                static uint32_t uComboSelect = 1;
                static const char* apcEnvMaps[] = {
                    "none",
                    "helipad",
                    "chromatic",
                    "directional",
                    "doge2",
                    "ennis",
                    "field",
                    "footprint_court",
                    "neutral",
                    "papermill",
                    "pisa",
                };
                bool abCombo[11] = {0};
                abCombo[uComboSelect] = true;
                gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
                if(gptUI->begin_combo("Environment", apcEnvMaps[uComboSelect], PL_UI_COMBO_FLAGS_NONE))
                {
                    for(uint32_t i = 0; i < 10; i++)
                    {
                        if(gptUI->selectable(apcEnvMaps[i], &abCombo[i], 0))
                        {
                            uComboSelect = i;
                            gptUI->close_current_popup();
                        }
                    }
                    gptUI->end_combo();
                }

                if(gptUI->input_text_hint(ICON_FA_MAGNIFYING_GLASS, "Filter (inc,-exc)", ptAppData->tFilter.acInputBuffer, 256, 0))
                {
                    gptUI->text_filter_build(&ptAppData->tFilter);
                }

                if(gptUI->begin_child("GLTF Models", 0, 0))
                {
                    uint32_t uTestModelCount = pl_sb_size(ptAppData->sbtTestModels);
                    if(gptUI->text_filter_active(&ptAppData->tFilter))
                    {
                        for(uint32_t i = 0; i < uTestModelCount; i++)
                        {
                            if(gptUI->text_filter_pass(&ptAppData->tFilter, ptAppData->sbtTestModels[i].acName, NULL))
                                gptUI->selectable(ptAppData->sbtTestModels[i].acName, &ptAppData->sbtTestModels[i].bSelected, 0);
                        }
                    }
                    else
                    {
                        plUiClipper tClipper = {(uint32_t)uTestModelCount};
                        while(gptUI->step_clipper(&tClipper))
                        {
                            for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
                            {
                                gptUI->selectable(ptAppData->sbtTestModels[i].acName, &ptAppData->sbtTestModels[i].bSelected, 0);
                            }
                        }
                    }
                    gptUI->end_child();
                }

                gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfWidths);

                if(bLoadScene)
                {

                    pl__create_scene(ptAppData);
                    
                    if(uComboSelect > 0)
                    {
                        char* sbcData = NULL;
                        pl_sb_sprintf(sbcData, "../data/pilotlight-assets-master/environments/%s.hdr", apcEnvMaps[uComboSelect]);
                        gptRenderer->load_skybox_from_panorama(ptAppData->uSceneHandle0, sbcData, 1024);
                        pl_sb_free(sbcData);
                    }

                    ptAppData->uViewHandle0 = gptRenderer->create_view(ptAppData->uSceneHandle0, ptIO->tMainViewportSize);

                    plModelLoaderData tLoaderData0 = {0};

                    uint32_t uTestModelCount = pl_sb_size(ptAppData->sbtTestModels);
                    for(uint32_t i = 0; i < uTestModelCount; i++)
                    {
                        if(ptAppData->sbtTestModels[i].bSelected)
                        {
                            plComponentLibrary* ptMainComponentLibrary = gptRenderer->get_component_library(ptAppData->uSceneHandle0);
                            gptModelLoader->load_gltf(ptMainComponentLibrary, ptAppData->sbtTestModels[i].acVariants[0].acFilePath, NULL, &tLoaderData0);
                        }
                    }

                    gptRenderer->add_drawable_objects_to_scene(ptAppData->uSceneHandle0, tLoaderData0.uObjectCount, tLoaderData0.atObjects);
                    gptModelLoader->free_data(&tLoaderData0);

                    gptRenderer->finalize_scene(ptAppData->uSceneHandle0);
                }

            }

            gptUI->end_collapsing_header();
        }
        
        gptRenderer->show_graphics_options(ICON_FA_DICE_D6 " Graphics");

        if(gptUI->begin_collapsing_header(ICON_FA_BOXES_STACKED " Physics", 0))
        {
            plPhysicsEngineSettings tPhysicsSettings = gptPhysics->get_settings();

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
                gptRenderer->reload_scene_shaders(ptAppData->uSceneHandle0);
            }
            gptUI->end_collapsing_header();
        }
        gptUI->end_window();
    }
}

void
pl__create_scene(plAppData* ptAppData)
{
    plIO* ptIO = gptIO->get_io();
    ptAppData->uSceneHandle0 = gptRenderer->create_scene();

    plComponentLibrary* ptMainComponentLibrary = gptRenderer->get_component_library(ptAppData->uSceneHandle0);

    // create main camera
    plCameraComponent* ptMainCamera = NULL;
    ptAppData->tMainCamera = gptEcs->create_perspective_camera(ptMainComponentLibrary, "main camera", (plVec3){-4.7f, 4.2f, -3.256f}, PL_PI_3, ptIO->tMainViewportSize.x / ptIO->tMainViewportSize.y, 0.1f, 48.0f, true, &ptMainCamera);
    gptCamera->set_pitch_yaw(ptMainCamera, 0.0f, 0.911f);
    gptCamera->update(ptMainCamera);
    gptEcs->attach_script(ptMainComponentLibrary, "pl_script_camera", PL_SCRIPT_FLAG_PLAYING | PL_SCRIPT_FLAG_RELOADABLE, ptAppData->tMainCamera, NULL);

    // create cull camera
    plCameraComponent* ptCullCamera = NULL;
    ptAppData->tCullCamera = gptEcs->create_perspective_camera(ptMainComponentLibrary, "cull camera", (plVec3){0, 0, 5.0f}, PL_PI_3, ptIO->tMainViewportSize.x / ptIO->tMainViewportSize.y, 0.1f, 25.0f, true, &ptCullCamera);
    gptCamera->set_pitch_yaw(ptCullCamera, 0.0f, PL_PI);
    gptCamera->update(ptCullCamera);

    // create lights
    plLightComponent* ptLight = NULL;
    gptEcs->create_directional_light(ptMainComponentLibrary, "direction light", (plVec3){-0.375f, -1.0f, -0.085f}, &ptLight);
    ptLight->uCascadeCount = 4;
    ptLight->fIntensity = 1.0f;
    ptLight->uShadowResolution = 1024;
    ptLight->afCascadeSplits[0] = 0.10f;
    ptLight->afCascadeSplits[1] = 0.25f;
    ptLight->afCascadeSplits[2] = 0.50f;
    ptLight->afCascadeSplits[3] = 1.00f;
    ptLight->tFlags |= PL_LIGHT_FLAG_CAST_SHADOW | PL_LIGHT_FLAG_VISUALIZER;

    plEntity tPointLight = gptEcs->create_point_light(ptMainComponentLibrary, "point light", (plVec3){0.0f, 2.0f, 2.0f}, &ptLight);
    ptLight->uShadowResolution = 1024;
    ptLight->tFlags |= PL_LIGHT_FLAG_CAST_SHADOW | PL_LIGHT_FLAG_VISUALIZER;
    plTransformComponent* ptPLightTransform = gptEcs->add_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, tPointLight);
    ptPLightTransform->tTranslation = (plVec3){0.0f, 1.497f, 2.0f};

    plEntity tSpotLight = gptEcs->create_spot_light(ptMainComponentLibrary, "spot light", (plVec3){0.0f, 4.0f, -1.18f}, (plVec3){0.0, -1.0f, 0.376f}, &ptLight);
    ptLight->uShadowResolution = 1024;
    ptLight->fRange = 5.0f;
    ptLight->fRadius = 0.025f;
    ptLight->fIntensity = 20.0f;
    ptLight->tFlags |= PL_LIGHT_FLAG_CAST_SHADOW | PL_LIGHT_FLAG_VISUALIZER;
    plTransformComponent* ptSLightTransform = gptEcs->add_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, tSpotLight);
    ptSLightTransform->tTranslation = (plVec3){0.0f, 4.0f, -1.18f};

    plEnvironmentProbeComponent* ptProbe = NULL;
    gptEcs->create_environment_probe(ptMainComponentLibrary, "Main Probe", (plVec3){0.0f, 3.0f, 0.0f}, &ptProbe);
    ptProbe->fRange = 30.0f;
    ptProbe->uResolution = 128;
    ptProbe->tFlags |= PL_ENVIRONMENT_PROBE_FLAGS_INCLUDE_SKY;

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
                gptUI->progress_bar(0.75f, (plVec2){-1.0f, 0.0f}, NULL);
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
                    #ifdef PL_METAL_BACKEND
                    gptUI->text("Graphics Backend: Metal");
                    #elif PL_VULKAN_BACKEND
                    gptUI->text("Graphics Backend: Vulkan");
                    #else
                    gptUI->text("Graphics Backend: Unknown");
                    #endif

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
    }
    gptUI->end_window();
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