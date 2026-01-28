/*
   example_gfx_3.c
     - demonstrates loading APIs
     - demonstrates loading extensions
     - demonstrates hot reloading
     - demonstrates starter extension
     - demonstrates drawing extension (2D & 3D)
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] apis
// [SECTION] helper function declarations
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
// [SECTION] helper function definitions
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

// standard
#include <stdlib.h>
#include <stdio.h>
#include <float.h>
#include <math.h>

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
#include "pl_freelist_ext.h"

// extensions
#include "pl_graphics_ext.h"
#include "pl_draw_ext.h"
#include "pl_starter_ext.h"

// new extension
#include "pl_terrain_ext.h"
#include "pl_terrain_processor_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

const plWindowI*            gptWindows       = NULL;
const plStatsI*             gptStats         = NULL;
const plGraphicsI*          gptGfx           = NULL;
const plToolsI*             gptTools         = NULL;
const plEcsI*               gptEcs           = NULL;
const plCameraI*            gptCamera        = NULL;
const plRendererI*          gptRenderer      = NULL;
const plModelLoaderI*       gptModelLoader   = NULL;
const plJobI*               gptJobs          = NULL;
const plDrawI*              gptDraw          = NULL;
const plUiI*                gptUI            = NULL;
const plIOI*                gptIO            = NULL;
const plShaderI*            gptShader        = NULL;
const plMemoryI*            gptMemory        = NULL;
const plNetworkI*           gptNetwork       = NULL;
const plStringInternI*      gptString        = NULL;
const plProfileI*           gptProfile       = NULL;
const plFileI*              gptFile          = NULL;
const plEcsToolsI*          gptEcsTools      = NULL;
const plGizmoI*             gptGizmo         = NULL;
const plConsoleI*           gptConsole       = NULL;
const plScreenLogI*         gptScreenLog     = NULL;
const plPhysicsI *          gptPhysics       = NULL;
const plCollisionI*         gptCollision     = NULL;
const plBVHI*               gptBvh           = NULL;
const plConfigI*            gptConfig        = NULL;
const plResourceI*          gptResource      = NULL;
const plStarterI*           gptStarter       = NULL;
const plAnimationI*         gptAnimation     = NULL;
const plMeshI*              gptMesh          = NULL;
const plShaderVariantI*     gptShaderVariant = NULL;
const plVfsI*               gptVfs           = NULL;
const plPakI*               gptPak           = NULL;
const plDateTimeI*          gptDateTime      = NULL;
const plCompressI*          gptCompress      = NULL;
const plMaterialI*          gptMaterial      = NULL;
const plScriptI*            gptScript        = NULL;
const plMeshBuilderI*       gptMeshBuilder   = NULL;
const plImageI*             gptImage         = NULL;
const plTerrainI*           gptTerrain          = NULL;
const plTerrainProcessorI*  gptTerrainProcessor = NULL;

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


typedef struct _plAppData
{
    // window
    plWindow* ptWindow;

    // 3d drawing
    plCamera      tCamera;
    plDrawList3D* pt3dDrawlist;

    // heightmap
    plDynamicDataBlock tCurrentDynamicBufferBlock;

    // visual options
    bool bShowOrigin;

    // meshing
    bool bShowResidency;

    bool* pbShowDeviceMemoryAnalyzer;
    bool* pbShowMemoryAllocations;
    bool* pbShowProfiling;
    bool* pbShowStats;
    bool* pbShowLogging;

    plTerrain* ptTerrain;
    float fTau;
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] helper function declarations
//-----------------------------------------------------------------------------

void pl__load_apis(plApiRegistryI*);

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryI* ptApiRegistry, plAppData* ptAppData)
{
    // NOTE: on first load, "pAppData" will be NULL but on reloads
    //       it will be the value returned from this function

    // retrieve the data registry API, this is the API used for sharing data
    // between extensions & the runtime
    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    // if "ptAppData" is a valid pointer, then this function is being called
    // during a hot reload.
    if(ptAppData)
    {
        // re-retrieve the apis since we are now in
        // a different dll/so
        pl__load_apis(ptApiRegistry);

        return ptAppData;
    }

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = malloc(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));
    ptAppData->fTau = 0.2f;

    // retrieve extension registry
    const plExtensionRegistryI* ptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);

    // load extensions (makes their APIs available)
    ptExtensionRegistry->load("pl_unity_ext", NULL, NULL, true);
    ptExtensionRegistry->load("pl_platform_ext", NULL, NULL, false);
    
    pl__load_apis(ptApiRegistry);

    // use window API to create a window
    plWindowDesc tWindowDesc = {
        .pcTitle = "Example GFX 3",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 600,
        .uHeight = 600,
    };
    gptWindows->create(tWindowDesc, &ptAppData->ptWindow);
    gptWindows->show(ptAppData->ptWindow);

    plStarterInit tStarterInit = {
        .tFlags   = PL_STARTER_FLAGS_ALL_EXTENSIONS,
        .ptWindow = ptAppData->ptWindow
    };

    // we want the starter extension to include a depth buffer
    // when setting up the render pass
    tStarterInit.tFlags |= PL_STARTER_FLAGS_DEPTH_BUFFER;
    tStarterInit.tFlags |= PL_STARTER_FLAGS_REVERSE_Z;
    tStarterInit.tFlags &= ~PL_STARTER_FLAGS_SHADER_EXT;

    // from a graphics standpoint, the starter extension is handling device, swapchain, renderpass
    // etc. which we will get to in later examples
    gptStarter->initialize(tStarterInit);

    // initialize shader extension (we are doing this ourselves so we can add additional shader directories)
    static const plShaderOptions tDefaultShaderOptions = {
        .apcIncludeDirectories = {
            "../shaders/"
        },
        .apcDirectories = {
            "../shaders/"
        },
        .tFlags = PL_SHADER_FLAGS_AUTO_OUTPUT | PL_SHADER_FLAGS_NEVER_CACHE | PL_SHADER_FLAGS_INCLUDE_DEBUG
    };
    gptShader->initialize(&tDefaultShaderOptions);

    // give starter extension chance to do its work now that we
    // setup the shader extension
    gptStarter->finalize();

    // request 3d drawlists
    ptAppData->pt3dDrawlist = gptDraw->request_3d_drawlist();

    // create camera
    ptAppData->tCamera = (plCamera){
        // .tPos         = {5.0f, 10.0f, 10.0f},
        // .fNearZ       = 0.01f,
        // .fFarZ        = 50000.0f,

        .tPos         = {1738400.0f, 0.0f, 10.0f},
        .fNearZ       = 10.0f,
        .fFarZ        = 1737400.0f * 3.0f,
        // .fFarZ        = 100000.0f,

        .fFieldOfView = PL_PI_3,
        .fAspectRatio = 1.0f,
        .fYaw         = PL_PI + PL_PI_4,
        .fPitch       = -PL_PI_4,
        .tType        = PL_CAMERA_TYPE_PERSPECTIVE_REVERSE_Z
    };
    gptCamera->update(&ptAppData->tCamera);

    // for convience
    plDevice* ptDevice = gptStarter->get_device();

    plTerrainInit tTerrainInit = {
        .ptDevice = ptDevice
    };
    gptTerrain->initialize(tTerrainInit);
    ptAppData->ptTerrain = gptTerrain->create_terrain();

#if 1

    for(uint32_t i = 5; i < 7; i++)
    {
        for(uint32_t j = 5; j < 7; j++)
        {
            char* sbNameBuffer0 = NULL;
            char* sbNameBuffer1 = NULL;
            pl_sb_sprintf(sbNameBuffer0, "../data/moon_%u_%u.png", i, j);
            pl_sb_sprintf(sbNameBuffer1, "moon_%u_%u.chu", i, j);
            plTerrainHeightMapInfo tInfo =
            {
                .iTreeDepth      = 6,
                .fRadius         = 1737400.0f,
                .bEllipsoid      = true,
                .b3dErrorCalc    = true,
                .pcHeightMapFile = sbNameBuffer0,
                .pcOutputFile    = sbNameBuffer1,
                .fMaxHeight      = 14052.0f,
                .fMinHeight      = -18256.0f,
                // .fMaxBaseError   = 324.0f,
                .fMaxBaseError   = 10.0f,
                .fMetersPerPixel = 100.0f, // 4096
                .tCenter = {
                    .x = -1440000.0f + (float)i * 409600.0f  + 409600.0f * 0.5f,
                    .z = -1440000.0f + (float)j * 409600.0f  + 409600.0f * 0.5f
                }
            };

            gptProfile->begin_frame();
            gptProfile->begin_sample(0, "FRAME");
            if(!gptFile->exists(tInfo.pcOutputFile))
            {
                gptProfile->begin_sample(0, "process_heightmap");
                gptTerrainProcessor->process_heightmap(tInfo);
                gptProfile->end_sample(0);
            }

            gptProfile->begin_sample(0, "load_chunk_file");
            gptTerrain->load_chunk_file(ptAppData->ptTerrain, tInfo.pcOutputFile);
            gptProfile->end_sample(0);

            gptProfile->end_sample(0);
            gptProfile->end_frame();

            uint32_t uSampleCount = 0;
            plProfileCpuSample* ptSamples = gptProfile->get_last_frame_samples(0, &uSampleCount);

            const char* acSpaceBuffer = "                                      ";
            // for(uint32_t j = 0; j < uSampleCount; j++)
            // {
            //     printf("%s %s: %0.6f\n", &acSpaceBuffer[39 - ptSamples[j]._uDepth], ptSamples[j].pcName, ptSamples[j].dDuration);
            // }

            pl_sb_free(sbNameBuffer0);
            pl_sb_free(sbNameBuffer1);
        }
    }

#else


    plTerrainHeightMapInfo atHeightMapInit[] = {
        // {
        //     .pcHeightMapFile = "../data/mountains.png",
        //     .pcOutputFile    = "mountains.chu",
        //     .iTreeDepth      = 6,
        //     .fMaxHeight      = 512.0f,
        //     .fMinHeight      = -40.0f,
        //     .fMaxBaseError   = 1.0f,
        //     .fMetersPerPixel = 2.0f,
        // },
        {
            .iTreeDepth      = 5,
            .fRadius         = 1737400.0f,
            .bEllipsoid      = true,
            .b3dErrorCalc    = true,
            .pcHeightMapFile = "../data/moon_q0.png",
            .pcOutputFile    = "moon_q0.chu",
            .fMaxHeight      = 14052.0f,
            .fMinHeight      = -18256.0f,
            .fMaxBaseError   = 10.0f,
            .fMetersPerPixel = 351.5625f, // 4096
            .tCenter = {
                .x = -1440000.0f * 0.5f,
                .z = -1440000.0f * 0.5f
            }
        },
        {
            .iTreeDepth      = 5,
            .fRadius         = 1737400.0f,
            .bEllipsoid      = true,
            .b3dErrorCalc    = true,
            .pcHeightMapFile = "../data/moon_q1.png",
            .pcOutputFile    = "moon_q1.chu",
            .fMaxHeight      = 14052.0f,
            .fMinHeight      = -18256.0f,
            .fMaxBaseError   = 10.0f,
            .fMetersPerPixel = 351.5625f, // 4096
            .tCenter = {
                .x = 1440000.0f * 0.5f,
                .z = -1440000.0f * 0.5f
            }
        },
        {
            .iTreeDepth      = 5,
            .fRadius         = 1737400.0f,
            .bEllipsoid      = true,
            .b3dErrorCalc    = true,
            .pcHeightMapFile = "../data/moon_q2.png",
            .pcOutputFile    = "moon_q2.chu",
            .fMaxHeight      = 14052.0f,
            .fMinHeight      = -18256.0f,
            .fMaxBaseError   = 10.0f,
            .fMetersPerPixel = 351.5625f, // 4096
            .tCenter = {
                .x = 1440000.0f * 0.5f,
                .z = 1440000.0f * 0.5f
            }
        },
        {
            .iTreeDepth      = 5,
            .fRadius         = 1737400.0f,
            .bEllipsoid      = true,
            .b3dErrorCalc    = true,
            .pcHeightMapFile = "../data/moon_q3.png",
            .pcOutputFile    = "moon_q3.chu",
            .fMaxHeight      = 14052.0f,
            .fMinHeight      = -18256.0f,
            .fMaxBaseError   = 10.0f,
            .fMetersPerPixel = 351.5625f, // 4096
            .tCenter = {
                .x = -1440000.0f * 0.5f,
                .z = 1440000.0f * 0.5f
            }
        },
    };

    for(uint32_t i = 0; i < PL_ARRAYSIZE(atHeightMapInit); i++)
    {

        gptProfile->begin_frame();
        gptProfile->begin_sample(0, "FRAME");
        if(!gptFile->exists(atHeightMapInit[i].pcOutputFile))
        {
            gptProfile->begin_sample(0, "process_heightmap");
            gptTerrain->process_heightmap(atHeightMapInit[i]);
            gptProfile->end_sample(0);
        }

        gptProfile->begin_sample(0, "load_chunk_file");
        gptTerrain->load_chunk_file(ptAppData->ptTerrain, atHeightMapInit[i].pcOutputFile);
        gptProfile->end_sample(0);

        gptProfile->end_sample(0);
        gptProfile->end_frame();

        uint32_t uSampleCount = 0;
        plProfileCpuSample* ptSamples = gptProfile->get_last_frame_samples(0, &uSampleCount);

        const char* acSpaceBuffer = "                                      ";
        for(uint32_t j = 0; j < uSampleCount; j++)
        {
            printf("%s %s: %0.6f\n", &acSpaceBuffer[39 - ptSamples[j]._uDepth], ptSamples[j].pcName, ptSamples[j].dDuration);
        }
    }
#endif

    gptTerrain->finalize_terrain(ptAppData->ptTerrain);


    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~shaders~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    ptAppData->pbShowLogging              = (bool*)gptConsole->get_variable("t.LogTool", NULL, NULL);
    ptAppData->pbShowStats                = (bool*)gptConsole->get_variable("t.StatTool", NULL, NULL);
    ptAppData->pbShowProfiling            = (bool*)gptConsole->get_variable("t.ProfileTool", NULL, NULL);
    ptAppData->pbShowMemoryAllocations    = (bool*)gptConsole->get_variable("t.MemoryAllocationTool", NULL, NULL);
    ptAppData->pbShowDeviceMemoryAnalyzer = (bool*)gptConsole->get_variable("t.DeviceMemoryAnalyzerTool", NULL, NULL);


    // return app memory
    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    plDevice* ptDevice = gptStarter->get_device();

    // ensure the GPU is done with our resources
    gptGfx->flush_device(ptDevice);

    gptTerrain->cleanup_terrain(ptAppData->ptTerrain);
    gptShader->cleanup();
    gptStarter->cleanup();
    gptWindows->destroy(ptAppData->ptWindow);
    free(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plWindow* ptWindow, plAppData* ptAppData)
{
    gptStarter->resize();

    plIO* ptIO = gptIO->get_io();
    ptAppData->tCamera.fAspectRatio = ptIO->tMainViewportSize.x / ptIO->tMainViewportSize.y;
}


//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

static float fCameraTravelSpeed = 1000000.0f;
// static float fCameraTravelSpeed = 100.0f;

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    if(!gptStarter->begin_frame())
        return;

    // for convience
    plIO* ptIO = gptIO->get_io();

    static const float fCameraRotationSpeed = 0.005f;

    plCamera* ptCamera = &ptAppData->tCamera;

    if(!gptUI->wants_mouse_capture())
    {

        if(gptIO->is_key_pressed(PL_KEY_MINUS, false)) fCameraTravelSpeed /= 10.0f;
        if(gptIO->is_key_pressed(PL_KEY_EQUAL, false)) fCameraTravelSpeed *= 10.0f;

        // camera space
        if(gptIO->is_key_down(PL_KEY_W)) gptCamera->translate(ptCamera,  0.0f,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime);
        if(gptIO->is_key_down(PL_KEY_S)) gptCamera->translate(ptCamera,  0.0f,  0.0f, -fCameraTravelSpeed* ptIO->fDeltaTime);
        if(gptIO->is_key_down(PL_KEY_A)) gptCamera->translate(ptCamera, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);
        if(gptIO->is_key_down(PL_KEY_D)) gptCamera->translate(ptCamera,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);

        // world space
        if(gptIO->is_key_down(PL_KEY_F)) { gptCamera->translate(ptCamera,  0.0f, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f); }
        if(gptIO->is_key_down(PL_KEY_R)) { gptCamera->translate(ptCamera,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f); }

        if(gptIO->is_key_down(PL_KEY_O))
        {
            ptCamera->fRoll += 0.1f;
            ptCamera->fRoll = fmodf(ptCamera->fRoll, PL_PI * 2.0f);
        }

        if(gptIO->is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 1.0f))
        {
            const plVec2 tMouseDelta = gptIO->get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f);
            gptCamera->rotate(ptCamera,  -tMouseDelta.y * fCameraRotationSpeed,  -tMouseDelta.x * fCameraRotationSpeed);
            gptIO->reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
        }
    }
    gptCamera->update(ptCamera);

    // 3d drawing API usage
    if(ptAppData->bShowOrigin)
    {
        const plMat4 tOrigin = pl_identity_mat4();
        // gptDraw->add_3d_transform(ptAppData->pt3dDrawlist, &tOrigin, 1000.0f, (plDrawLineOptions){.fThickness = 10.0f});
        gptDraw->add_3d_transform(ptAppData->pt3dDrawlist, &tOrigin, 1737400.0f * 1.2f, (plDrawLineOptions){.fThickness = 1000.0f});
    }

    if(gptUI->begin_window("Debug", NULL, 0))
    {
        plTerrainFlags tFlags = gptTerrain->get_flags(ptAppData->ptTerrain);
        bool bShowDebug = tFlags & PL_TERRAIN_FLAGS_SHOW_LEVELS;
        bool bWireframe = tFlags & PL_TERRAIN_FLAGS_WIREFRAME;
        gptUI->checkbox("Show Origin", &ptAppData->bShowOrigin);
        gptUI->checkbox("Show Residency", &ptAppData->bShowResidency);
        gptUI->slider_float("Tau", &ptAppData->fTau, 0.0f, 10.0f, 0);


        if(gptUI->checkbox("Wireframe", &bWireframe))
        {
            if(bWireframe)
                tFlags |= PL_TERRAIN_FLAGS_WIREFRAME;
            else
                tFlags &= ~PL_TERRAIN_FLAGS_WIREFRAME;
        }

        if(gptUI->checkbox("Levels", &bShowDebug))
        {
            if(bShowDebug)
                tFlags |= PL_TERRAIN_FLAGS_SHOW_LEVELS;
            else
                tFlags &= ~PL_TERRAIN_FLAGS_SHOW_LEVELS;
        }

        // gptUI->slider_float("Tau", &ptAppData->fTau, 0.0f, 10.0f, 0);
        if(gptUI->button("Reload Shaders"))
        {
            gptTerrain->reload_shaders(ptAppData->ptTerrain);
        }

        gptTerrain->set_flags(ptAppData->ptTerrain, tFlags);
        gptUI->end_window();
    }

    gptScreenLog->add_message_ex(186, 10.0, PL_COLOR_32_GREEN, 1.0f, "FPS: %0.0f", ptIO->fFrameRate);
    gptScreenLog->add_message_ex(187, 10.0, PL_COLOR_32_GREEN, 1.0f, "Pos: %0.3f, %0.3f, %0.3f", ptCamera->tPos.x, ptCamera->tPos.y, ptCamera->tPos.z);
    
    gptScreenLog->add_message_ex(189, 10.0, PL_COLOR_32_GREEN, 1.0f, "%0.0f meters / second", fCameraTravelSpeed);
    gptScreenLog->add_message_ex(190, 10.0, PL_COLOR_32_GREEN, 1.0f, "Yaw:   %0.0f", pl_degreesf(ptCamera->fYaw));
    gptScreenLog->add_message_ex(191, 10.0, PL_COLOR_32_GREEN, 1.0f, "Pitch: %0.0f", pl_degreesf(ptCamera->fPitch));
    gptScreenLog->add_message_ex(192, 10.0, PL_COLOR_32_GREEN, 1.0f, "Roll:  %0.0f", pl_degreesf(ptCamera->fRoll));
    // gptScreenLog->add_message_ex(193, 10.0, PL_COLOR_32_GREEN, 1.0f, "Index Buffer Usage:  %0.2f %%", 100.0f * (float)ptAppData->tIndexBufferManager.uUsedSpace / (float)ptAppData->tIndexBufferManager.uSize);
    // gptScreenLog->add_message_ex(194, 10.0, PL_COLOR_32_GREEN, 1.0f, "Vertex Buffer Usage:  %0.2f %%", 100.0f * (float)ptAppData->tVertexBufferManager.uUsedSpace / (float)ptAppData->tVertexBufferManager.uSize);
    // gptScreenLog->add_message_ex(194, 10.0, PL_COLOR_32_GREEN, 1.0f, "Queued Chunks:  %d",pl_sb_size(ptAppData->sbtResidencyRequests));

    gptTerrain->prepare_terrain(ptAppData->ptTerrain);

    if(ptAppData->bShowResidency)
        gptTerrain->draw_residency(ptAppData->ptTerrain, gptStarter->get_foreground_layer(), (plVec2){1000.0f, 20.0f}, 3.0f);

    // start main pass & return the encoder being used
    plRenderEncoder* ptEncoder = gptStarter->begin_main_pass();

    plDevice* ptDevice = gptStarter->get_device();
    const plMat4 tMVP = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);

    ptAppData->tCurrentDynamicBufferBlock = gptGfx->allocate_dynamic_data_block(ptDevice);

    gptTerrain->render_terrain(ptAppData->ptTerrain, ptEncoder, ptCamera, &ptAppData->tCurrentDynamicBufferBlock, ptAppData->fTau);

    // submit 3d drawlist
    gptDraw->submit_3d_drawlist(ptAppData->pt3dDrawlist,
        ptEncoder,
        ptIO->tMainViewportSize.x,
        ptIO->tMainViewportSize.y,
        &tMVP,
        PL_DRAW_FLAG_DEPTH_TEST | PL_DRAW_FLAG_DEPTH_WRITE | PL_DRAW_FLAG_REVERSE_Z_DEPTH,
        gptGfx->get_swapchain_info(gptStarter->get_swapchain()).tSampleCount);

    // allows the starter extension to handle some things then ends the main pass
    gptStarter->end_main_pass();

    // must be the last function called when using the starter extension
    gptStarter->end_frame(); 
}

//-----------------------------------------------------------------------------
// [SECTION] helper function declarations
//-----------------------------------------------------------------------------

void
pl__load_apis(plApiRegistryI* ptApiRegistry)
{
    gptWindows       = pl_get_api_latest(ptApiRegistry, plWindowI);
    gptStats         = pl_get_api_latest(ptApiRegistry, plStatsI);
    gptGfx           = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptTools         = pl_get_api_latest(ptApiRegistry, plToolsI);
    gptEcs           = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptCamera        = pl_get_api_latest(ptApiRegistry, plCameraI);
    gptRenderer      = pl_get_api_latest(ptApiRegistry, plRendererI);
    gptJobs          = pl_get_api_latest(ptApiRegistry, plJobI);
    gptModelLoader   = pl_get_api_latest(ptApiRegistry, plModelLoaderI);
    gptDraw          = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptUI            = pl_get_api_latest(ptApiRegistry, plUiI);
    gptIO            = pl_get_api_latest(ptApiRegistry, plIOI);
    gptShader        = pl_get_api_latest(ptApiRegistry, plShaderI);
    gptMemory        = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptNetwork       = pl_get_api_latest(ptApiRegistry, plNetworkI);
    gptString        = pl_get_api_latest(ptApiRegistry, plStringInternI);
    gptProfile       = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptFile          = pl_get_api_latest(ptApiRegistry, plFileI);
    gptEcsTools      = pl_get_api_latest(ptApiRegistry, plEcsToolsI);
    gptGizmo         = pl_get_api_latest(ptApiRegistry, plGizmoI);
    gptConsole       = pl_get_api_latest(ptApiRegistry, plConsoleI);
    gptScreenLog     = pl_get_api_latest(ptApiRegistry, plScreenLogI);
    gptPhysics       = pl_get_api_latest(ptApiRegistry, plPhysicsI);
    gptCollision     = pl_get_api_latest(ptApiRegistry, plCollisionI);
    gptBvh           = pl_get_api_latest(ptApiRegistry, plBVHI);
    gptConfig        = pl_get_api_latest(ptApiRegistry, plConfigI);
    gptResource      = pl_get_api_latest(ptApiRegistry, plResourceI);
    gptStarter       = pl_get_api_latest(ptApiRegistry, plStarterI);
    gptAnimation     = pl_get_api_latest(ptApiRegistry, plAnimationI);
    gptMesh          = pl_get_api_latest(ptApiRegistry, plMeshI);
    gptShaderVariant = pl_get_api_latest(ptApiRegistry, plShaderVariantI);
    gptVfs           = pl_get_api_latest(ptApiRegistry, plVfsI);
    gptPak           = pl_get_api_latest(ptApiRegistry, plPakI);
    gptDateTime      = pl_get_api_latest(ptApiRegistry, plDateTimeI);
    gptCompress      = pl_get_api_latest(ptApiRegistry, plCompressI);
    gptMaterial      = pl_get_api_latest(ptApiRegistry, plMaterialI);
    gptScript        = pl_get_api_latest(ptApiRegistry, plScriptI);
    gptMeshBuilder   = pl_get_api_latest(ptApiRegistry, plMeshBuilderI);
    gptImage         = pl_get_api_latest(ptApiRegistry, plImageI);
    gptTerrain         = pl_get_api_latest(ptApiRegistry, plTerrainI);
    gptTerrainProcessor = pl_get_api_latest(ptApiRegistry, plTerrainProcessorI);
}
