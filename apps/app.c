/*
   app.c (just experimental)
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdio.h>
#include "pilotlight.h"
#include "pl_profile.h"
#include "pl_log.h"
#include "pl_ds.h"
#include "pl_os.h"
#include "pl_memory.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"
#include "pl_ui.h"

// extensions
#include "pl_image_ext.h"
#include "pl_stats_ext.h"
#include "pl_graphics_ext.h"
#include "pl_debug_ext.h"

// app specific
#include "camera.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct plAppData_t
{
    plGraphics tGraphics;

    // drawing
    plDrawList   tDrawlist;
    plDrawLayer* ptFgDrawLayer;
    plDrawLayer* ptBgDrawLayer;
    plFontAtlas  tFontAtlas;

    // ui options
    plDebugApiInfo tDebugInfo;
    bool           bShowUiDemo;
    bool           bShowUiDebug;
    bool           bShowUiStyle;

    // render pass
    plFrameBufferHandle* sbtMainFrameBuffers;
    plFrameBufferHandle* sbtOffscreenFrameBuffers;
    plRenderPassHandle   tMainRenderPass;
    plRenderPassHandle   tOffscreenRenderPass;

    // offscreen
    plTextureHandle     tOffscreenTexture[2];
    plTextureViewHandle tOffscreenTextureView[2];
    plTextureHandle     tOffscreenDepthTexture;
    plTextureViewHandle tOffscreenDepthTextureView;
    plTextureId*        ptOffscreenTextureID[2];

    // texture
    plTextureHandle     tTexture;
    plTextureViewHandle tTextureView;
    plTextureHandle     tSkyboxTexture;
    plTextureViewHandle tSkyboxTextureView;

    // bind groups
    plBindGroupHandle atBindGroups0[2];
    plBindGroupHandle atBindGroups0Offscreen[2];
    plBindGroupHandle tBindGroup1_0;
    plBindGroupHandle tBindGroup1_1;
    plBindGroupHandle tBindGroup1_2;
    plBindGroupHandle tBindGroup2;
    
    // shaders
    plShaderHandle tShader0;
    plShaderHandle tShader1;
    plShaderHandle tShader2;
    plShaderHandle tOffscreenShader0;
    plShaderHandle tOffscreenShader1;
    plShaderHandle tOffscreenShader2;

    // global index/vertex/data buffers
    plBufferHandle tVertexBuffer;
    plBufferHandle tIndexBuffer;
    plBufferHandle tStorageBuffer;
    plBufferHandle tShaderSpecificBuffer;
    plBufferHandle atGlobalBuffers[2];
    plBufferHandle atOffscreenGlobalBuffers[2];

    // scene
    plDrawStream tDrawStream;
    plCamera     tMainCamera;
    plCamera     tOffscreenCamera;
    plDrawList3D t3DDrawList;
    plDrawList3D tOffscreen3DDrawList;

    plVec3*   sbtVertexPosBuffer;
    plVec4*   sbtVertexDataBuffer;
    uint32_t* sbuIndexBuffer;
} plAppData;

typedef struct _BindGroup_0
{
    plMat4 tCameraView;
    plMat4 tCameraProjection;   
    plMat4 tCameraViewProjection;   
} BindGroup_0;

typedef struct _BindGroup_2
{
    plVec4 tShaderSpecific;
} BindGroup_2;

typedef struct _DynamicData
{
    int    iDataOffset;
    int    iVertexOffset;
    int    iPadding[2];
    plMat4 tModel;
} DynamicData;

//-----------------------------------------------------------------------------
// [SECTION] global apis
//-----------------------------------------------------------------------------

const plApiRegistryApiI*       gptApiRegistry       = NULL;
const plDataRegistryApiI*      gptDataRegistry      = NULL;
const plStatsApiI*             gptStats             = NULL;
const plExtensionRegistryApiI* gptExtensionRegistry = NULL;
const plFileApiI*              gptFile              = NULL;
const plGraphicsI*             gptGfx               = NULL;
const plDeviceI*               gptDevice            = NULL;
const plDebugApiI*             gptDebug             = NULL;
const plImageApiI*             gptImage             = NULL;
const plDrawStreamI*           gptStream            = NULL;

//-----------------------------------------------------------------------------
// [SECTION] helpers
//-----------------------------------------------------------------------------

void create_offscreen_frame_buffer(plAppData* ptAppData);
void create_buffers               (plAppData* ptAppData);
void create_offscreen_textures    (plAppData* ptAppData);
void create_textures              (plAppData* ptAppData);
void create_bind_groups           (plAppData* ptAppData);
void create_shaders               (plAppData* ptAppData);
void create_main_frame_buffer     (plAppData* ptAppData);
void recreate_main_frame_buffer   (plAppData* ptAppData);

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryApiI* ptApiRegistry, plAppData* ptAppData)
{
    gptApiRegistry  = ptApiRegistry;
    gptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(gptDataRegistry->get_data(PL_CONTEXT_MEMORY));
    pl_set_context(gptDataRegistry->get_data("ui"));

    if(ptAppData) // reload
    {
        pl_set_log_context(gptDataRegistry->get_data("log"));
        pl_set_profile_context(gptDataRegistry->get_data("profile"));

        // reload global apis
        gptStats  = ptApiRegistry->first(PL_API_STATS);
        gptFile   = ptApiRegistry->first(PL_API_FILE);
        gptGfx    = ptApiRegistry->first(PL_API_GRAPHICS);
        gptDevice = ptApiRegistry->first(PL_API_DEVICE);
        gptDebug  = ptApiRegistry->first(PL_API_DEBUG);
        gptImage  = ptApiRegistry->first(PL_API_IMAGE);
        gptStream = ptApiRegistry->first(PL_API_DRAW_STREAM);

        return ptAppData;
    }

    plProfileContext* ptProfileCtx = pl_create_profile_context();
    plLogContext*     ptLogCtx     = pl_create_log_context();
    
    // add some context to data registry
    ptAppData = PL_ALLOC(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));
    gptDataRegistry->set_data("profile", ptProfileCtx);
    gptDataRegistry->set_data("log", ptLogCtx);

    // create log context
    pl_add_log_channel("Default", PL_CHANNEL_TYPE_CONSOLE);
    pl_log_info("Setup logging");

    // load extensions
    const plExtensionRegistryApiI* ptExtensionRegistry = ptApiRegistry->first(PL_API_EXTENSION_REGISTRY);
    ptExtensionRegistry->load("pl_image_ext",    "pl_load_image_ext",    "pl_unload_image_ext",    false);
    ptExtensionRegistry->load("pl_stats_ext",    "pl_load_stats_ext",    "pl_unload_stats_ext",    false);
    ptExtensionRegistry->load("pl_graphics_ext", "pl_load_graphics_ext", "pl_unload_graphics_ext", false);
    ptExtensionRegistry->load("pl_debug_ext",    "pl_load_debug_ext",    "pl_unload_debug_ext",    true);

    // load apis
    gptStats  = ptApiRegistry->first(PL_API_STATS);
    gptFile   = ptApiRegistry->first(PL_API_FILE);
    gptGfx    = ptApiRegistry->first(PL_API_GRAPHICS);
    gptDevice = ptApiRegistry->first(PL_API_DEVICE);
    gptDebug  = ptApiRegistry->first(PL_API_DEBUG);
    gptImage  = ptApiRegistry->first(PL_API_IMAGE);
    gptStream = ptApiRegistry->first(PL_API_DRAW_STREAM);

    // create command queue
    gptGfx->initialize(&ptAppData->tGraphics);
    plDevice* ptDevice = &ptAppData->tGraphics.tDevice;
    gptDataRegistry->set_data("device", ptDevice);

    // create camera
    plIO* ptIO = pl_get_io();
    ptAppData->tMainCamera = pl_camera_create((plVec3){-6.211f, 3.647f, 0.827f}, PL_PI_3, ptIO->afMainViewportSize[0] / ptIO->afMainViewportSize[1], 0.01f, 400.0f);
    ptAppData->tOffscreenCamera = pl_camera_create((plVec3){0.0f, 0.0f, 2.0f}, PL_PI_3, 1.0f, 0.01f, 400.0f);
    pl_camera_set_pitch_yaw(&ptAppData->tOffscreenCamera, 0.0f, PL_PI);
    pl_camera_set_pitch_yaw(&ptAppData->tMainCamera, -0.244f, 1.488f);
    pl_camera_update(&ptAppData->tMainCamera);

    // render pass layouts
    const plRenderPassLayoutDescription tMainRenderPassLayoutDesc = {
        .tDepthTarget = {.tFormat = ptAppData->tGraphics.tSwapchain.tDepthFormat, .tSampleCount = ptAppData->tGraphics.tSwapchain.tMsaaSamples},
        .tResolveTarget = { .tFormat = ptAppData->tGraphics.tSwapchain.tFormat, .tSampleCount = PL_SAMPLE_COUNT_1 },
        .atRenderTargets = {
            { .tFormat = ptAppData->tGraphics.tSwapchain.tFormat, .tSampleCount = ptAppData->tGraphics.tSwapchain.tMsaaSamples}
        },
        .atSubpasses = {
            {
                .uRenderTargetCount = 1,
                .auRenderTargets = {0},
                .uSubpassInputCount = 0,
                .bDepthTarget = true,
                .bResolveTarget = true
            }
        }
    };

    const plRenderPassLayoutDescription tOffscreenRenderPassLayoutDesc = {
        .tDepthTarget = { .tFormat = PL_FORMAT_D32_FLOAT, .tSampleCount = PL_SAMPLE_COUNT_1 },
        .atRenderTargets = {
            { .tFormat = PL_FORMAT_R8G8B8A8_UNORM, .tSampleCount = PL_SAMPLE_COUNT_1}
        },
        .atSubpasses = {
            {
                .uRenderTargetCount = 1,
                .auRenderTargets = {0},
                .uSubpassInputCount = 0,
                .bDepthTarget = true
            }
        }
    };

    // render passes
    const plRenderPassDescription tMainRenderPassDesc = {
        .tLayout = gptDevice->create_render_pass_layout(ptDevice, &tMainRenderPassLayoutDesc),
        .tDepthTarget = {
            .tLoadOp         = PL_LOAD_OP_CLEAR,
            .tStoreOp        = PL_STORE_OP_STORE,
            .tStencilLoadOp  = PL_LOAD_OP_CLEAR,
            .tStencilStoreOp = PL_STORE_OP_DONT_CARE,
            .tNextUsage      = PL_TEXTURE_LAYOUT_DEPTH_STENCIL,
            .fClearZ         = 1.0f
        },
        .tResolveTarget = {
                .tLoadOp         = PL_LOAD_OP_DONT_CARE,
                .tStoreOp        = PL_STORE_OP_STORE,
                .tNextUsage      = PL_TEXTURE_LAYOUT_PRESENT,
                .tClearColor     = {0.0f, 0.0f, 0.0f, 1.0f}
        },
        .atRenderTargets = {
            {
                .tLoadOp         = PL_LOAD_OP_CLEAR,
                .tStoreOp        = PL_STORE_OP_MULTISAMPLE_RESOLVE,
                .tNextUsage      = PL_TEXTURE_LAYOUT_RENDER_TARGET,
                .tClearColor     = {0.0f, 0.0f, 0.0f, 1.0f}
            },
            {
                .tLoadOp         = PL_LOAD_OP_DONT_CARE,
                .tStoreOp        = PL_STORE_OP_STORE,
                .tNextUsage      = PL_TEXTURE_LAYOUT_PRESENT,
                .tClearColor     = {0.0f, 0.0f, 0.0f, 1.0f}
            }
        }
    };
    const plRenderPassDescription tOffscreenRenderPassDesc = {
        .tLayout = gptDevice->create_render_pass_layout(ptDevice, &tOffscreenRenderPassLayoutDesc),
        .tDepthTarget = {
                .tLoadOp         = PL_LOAD_OP_CLEAR,
                .tStoreOp        = PL_STORE_OP_DONT_CARE,
                .tStencilLoadOp  = PL_LOAD_OP_CLEAR,
                .tStencilStoreOp = PL_STORE_OP_DONT_CARE,
                .tNextUsage      = PL_TEXTURE_LAYOUT_DEPTH_STENCIL
        },
        .atRenderTargets = {
            {
                .tLoadOp         = PL_LOAD_OP_CLEAR,
                .tStoreOp        = PL_STORE_OP_STORE,
                .tNextUsage      = PL_TEXTURE_LAYOUT_SHADER_READ
            }
        }
    };
    ptAppData->tMainRenderPass = gptDevice->create_render_pass(ptDevice, &tMainRenderPassDesc);
    ptAppData->tOffscreenRenderPass = gptDevice->create_render_pass(ptDevice, &tOffscreenRenderPassDesc);
    gptGfx->setup_ui(&ptAppData->tGraphics, ptAppData->tMainRenderPass);

    create_main_frame_buffer(ptAppData);
    create_buffers(ptAppData);
    create_textures(ptAppData);
    create_offscreen_textures(ptAppData);
    create_offscreen_frame_buffer(ptAppData);
    create_bind_groups(ptAppData);
    create_shaders(ptAppData);

    // create draw list & layers
    pl_register_drawlist(&ptAppData->tDrawlist);
    ptAppData->ptBgDrawLayer = pl_request_layer(&ptAppData->tDrawlist, "Background Layer");
    ptAppData->ptFgDrawLayer = pl_request_layer(&ptAppData->tDrawlist, "Foreground Layer");
    
    // create font atlas
    pl_add_default_font(&ptAppData->tFontAtlas);
    pl_build_font_atlas(&ptAppData->tFontAtlas);
    gptGfx->create_font_atlas(&ptAppData->tFontAtlas);
    pl_set_default_font(&ptAppData->tFontAtlas.sbtFonts[0]);

    // 3D tDrawlist
    gptGfx->register_3d_drawlist(&ptAppData->tGraphics, &ptAppData->t3DDrawList);
    gptGfx->register_3d_drawlist(&ptAppData->tGraphics, &ptAppData->tOffscreen3DDrawList);
    
    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    pl_sb_free(ptAppData->sbtMainFrameBuffers);
    pl_sb_free(ptAppData->sbtOffscreenFrameBuffers);
    gptGfx->destroy_font_atlas(&ptAppData->tFontAtlas);
    pl_cleanup_font_atlas(&ptAppData->tFontAtlas);
    gptStream->cleanup(&ptAppData->tDrawStream);
    gptGfx->cleanup(&ptAppData->tGraphics);
    pl_cleanup_profile_context();
    pl_cleanup_log_context();
    PL_FREE(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plAppData* ptAppData)
{
    plIO* ptIO = pl_get_io();
    plDevice* ptDevice = &ptAppData->tGraphics.tDevice;
    pl_camera_set_aspect(&ptAppData->tMainCamera, ptIO->afMainViewportSize[0] / ptIO->afMainViewportSize[1]);
    gptGfx->resize(&ptAppData->tGraphics);
    recreate_main_frame_buffer(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    // begin profiling frame
    pl_begin_profile_frame();
    pl_begin_profile_sample(__FUNCTION__);

    plIO* ptIO = pl_get_io();
    plDevice* ptDevice = &ptAppData->tGraphics.tDevice;

    if(!gptGfx->begin_frame(&ptAppData->tGraphics))
    {
        recreate_main_frame_buffer(ptAppData);
        pl_end_profile_sample();
        pl_end_profile_frame();
        return;
    }

    plGraphics* ptGraphics = &ptAppData->tGraphics;

    gptStats->new_frame();

    static double* pdFrameTimeCounter = NULL;
    if(!pdFrameTimeCounter)
        pdFrameTimeCounter = gptStats->get_counter("framerate");
    *pdFrameTimeCounter = (double)pl_get_io()->fFrameRate;

    // camera
    static const float fCameraTravelSpeed = 8.0f;

    // camera space
    if(pl_is_key_pressed(PL_KEY_W, true)) pl_camera_translate(&ptAppData->tMainCamera,  0.0f,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime);
    if(pl_is_key_pressed(PL_KEY_S, true)) pl_camera_translate(&ptAppData->tMainCamera,  0.0f,  0.0f, -fCameraTravelSpeed* ptIO->fDeltaTime);
    if(pl_is_key_pressed(PL_KEY_A, true)) pl_camera_translate(&ptAppData->tMainCamera, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);
    if(pl_is_key_pressed(PL_KEY_D, true)) pl_camera_translate(&ptAppData->tMainCamera,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);

    // world space
    if(pl_is_key_pressed(PL_KEY_F, true)) pl_camera_translate(&ptAppData->tMainCamera,  0.0f, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f);
    if(pl_is_key_pressed(PL_KEY_R, true)) pl_camera_translate(&ptAppData->tMainCamera,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f);

    bool bOwnMouse = ptIO->bWantCaptureMouse;
    if(!bOwnMouse && pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 1.0f))
    {
        const plVec2 tMouseDelta = pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f);
        pl_camera_rotate(&ptAppData->tMainCamera,  -tMouseDelta.y * 0.1f * ptIO->fDeltaTime,  -tMouseDelta.x * 0.1f * ptIO->fDeltaTime);
        pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
    }
    pl_camera_update(&ptAppData->tMainCamera);
    pl_camera_update(&ptAppData->tOffscreenCamera);

    const BindGroup_0 tBindGroupBufferOffscreen = {
        .tCameraProjection = ptAppData->tOffscreenCamera.tProjMat,
        .tCameraView = ptAppData->tOffscreenCamera.tViewMat,
        .tCameraViewProjection = pl_mul_mat4(&ptAppData->tOffscreenCamera.tProjMat, &ptAppData->tOffscreenCamera.tViewMat)
    };
    memcpy(ptGraphics->sbtBuffersCold[ptAppData->atOffscreenGlobalBuffers[ptAppData->tGraphics.uCurrentFrameIndex].uIndex].tMemoryAllocation.pHostMapped, &tBindGroupBufferOffscreen, sizeof(BindGroup_0));

    const BindGroup_0 tBindGroupBuffer = {
        .tCameraProjection = ptAppData->tMainCamera.tProjMat,
        .tCameraView = ptAppData->tMainCamera.tViewMat,
        .tCameraViewProjection = pl_mul_mat4(&ptAppData->tMainCamera.tProjMat, &ptAppData->tMainCamera.tViewMat)
    };
    memcpy(ptGraphics->sbtBuffersCold[ptAppData->atGlobalBuffers[ptAppData->tGraphics.uCurrentFrameIndex].uIndex].tMemoryAllocation.pHostMapped, &tBindGroupBuffer, sizeof(BindGroup_0));

    const plMat4 tMVP = pl_mul_mat4(&ptAppData->tMainCamera.tProjMat, &ptAppData->tMainCamera.tViewMat);

    plDynamicBinding tDynamicBinding0 = gptDevice->allocate_dynamic_data(ptDevice, sizeof(DynamicData));
    DynamicData* ptDynamicData0 = (DynamicData*)tDynamicBinding0.pcData;
    ptDynamicData0->iDataOffset = 0;
    ptDynamicData0->iVertexOffset = 0;
    ptDynamicData0->tModel = pl_mat4_rotate_xyz(0.05f * (float)ptIO->dTime, 0.0f, 1.0f, 0.0f);

    plDynamicBinding tDynamicBinding1 = gptDevice->allocate_dynamic_data(ptDevice, sizeof(DynamicData));
    DynamicData* ptDynamicData1 = (DynamicData*)tDynamicBinding1.pcData;
    ptDynamicData1->iDataOffset = 8;
    ptDynamicData1->iVertexOffset = 4;
    ptDynamicData1->tModel = pl_identity_mat4();

    plDynamicBinding tDynamicBinding2 = gptDevice->allocate_dynamic_data(ptDevice, sizeof(plMat4));
    plMat4* ptDynamicData2 = (plMat4*)tDynamicBinding2.pcData;
    *ptDynamicData2 = pl_mat4_translate_vec3(ptAppData->tMainCamera.tPos);

    plDynamicBinding tDynamicBinding3 = gptDevice->allocate_dynamic_data(ptDevice, sizeof(plMat4));
    plMat4* ptDynamicData3 = (plMat4*)tDynamicBinding3.pcData;
    *ptDynamicData3 = pl_mat4_translate_vec3(ptAppData->tOffscreenCamera.tPos);

    gptGfx->begin_recording(&ptAppData->tGraphics);
    gptGfx->begin_pass(&ptAppData->tGraphics, ptAppData->sbtOffscreenFrameBuffers[ptAppData->tGraphics.uCurrentFrameIndex]);

    gptStream->reset(&ptAppData->tDrawStream);

    // object 0
    gptStream->draw(&ptAppData->tDrawStream, (plDraw)
    {
        .uShaderVariant       = ptAppData->tOffscreenShader2.uIndex,
        .uDynamicBuffer       = tDynamicBinding3.uBufferHandle,
        .uVertexBuffer        = ptAppData->tVertexBuffer.uIndex,
        .uIndexBuffer         = ptAppData->tIndexBuffer.uIndex,
        .uTriangleCount       = 12,
        .uIndexOffset         = 12,
        .uBindGroup0          = ptAppData->atBindGroups0Offscreen[ptAppData->tGraphics.uCurrentFrameIndex].uIndex,
        .uBindGroup1          = ptAppData->tBindGroup1_2.uIndex,
        .uBindGroup2          = ptAppData->tBindGroup2.uIndex,
        .uDynamicBufferOffset = tDynamicBinding3.uByteOffset
    });

    // object 1
    gptStream->draw(&ptAppData->tDrawStream, (plDraw)
    {
        .uShaderVariant       = ptAppData->tOffscreenShader0.uIndex,
        .uDynamicBuffer       = tDynamicBinding0.uBufferHandle,
        .uVertexBuffer        = ptAppData->tVertexBuffer.uIndex,
        .uIndexBuffer         = ptAppData->tIndexBuffer.uIndex,
        .uTriangleCount       = 2,
        .uBindGroup0          = ptAppData->atBindGroups0Offscreen[ptAppData->tGraphics.uCurrentFrameIndex].uIndex,
        .uBindGroup1          = ptAppData->tBindGroup1_0.uIndex,
        .uBindGroup2          = ptAppData->tBindGroup2.uIndex,
        .uDynamicBufferOffset = tDynamicBinding0.uByteOffset
    });

    // object 2
    gptStream->draw(&ptAppData->tDrawStream, (plDraw)
    {
        .uShaderVariant       = ptAppData->tOffscreenShader1.uIndex,
        .uDynamicBuffer       = tDynamicBinding1.uBufferHandle,
        .uVertexBuffer        = ptAppData->tVertexBuffer.uIndex,
        .uIndexBuffer         = ptAppData->tIndexBuffer.uIndex,
        .uTriangleCount       = 2,
        .uIndexOffset         = 6,
        .uBindGroup0          = ptAppData->atBindGroups0Offscreen[ptAppData->tGraphics.uCurrentFrameIndex].uIndex,
        .uBindGroup1          = ptAppData->tBindGroup1_1.uIndex,
        .uBindGroup2          = ptAppData->tBindGroup2.uIndex,
        .uDynamicBufferOffset = tDynamicBinding1.uByteOffset
    });

    plDrawArea tArea0 = {
       .ptDrawStream = &ptAppData->tDrawStream,
       .tScissor = {
            .uWidth  = 500,
            .uHeight = 500,
       },
       .tViewport = {
            .fWidth  = 500,
            .fHeight = 500
       }
    };
    gptGfx->draw_areas(&ptAppData->tGraphics, 1, &tArea0);

    const plMat4 tOffscreenMVP = pl_mul_mat4(&ptAppData->tOffscreenCamera.tProjMat, &ptAppData->tOffscreenCamera.tViewMat);
    const plMat4 tTransform00 = pl_identity_mat4();
    gptGfx->add_3d_transform(&ptAppData->tOffscreen3DDrawList, &tTransform00, 10.0f, 0.02f);
    gptGfx->add_3d_frustum(&ptAppData->t3DDrawList, &ptAppData->tOffscreenCamera.tTransformMat, ptAppData->tOffscreenCamera.fFieldOfView, ptAppData->tOffscreenCamera.fAspectRatio, ptAppData->tOffscreenCamera.fNearZ, ptAppData->tOffscreenCamera.fFarZ, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 0.02f);
    gptGfx->submit_3d_drawlist(&ptAppData->tOffscreen3DDrawList, 500.0f, 500.0f, &tOffscreenMVP, PL_PIPELINE_FLAG_DEPTH_TEST | PL_PIPELINE_FLAG_DEPTH_WRITE, ptAppData->tOffscreenRenderPass, 1);
    gptGfx->end_pass(&ptAppData->tGraphics);
    gptGfx->begin_main_pass(&ptAppData->tGraphics, ptAppData->sbtMainFrameBuffers[ptAppData->tGraphics.tSwapchain.uCurrentImageIndex]);

    pl_new_frame();

    if(pl_begin_window("Offscreen", NULL, false))
    {

        const float pfRatios[] = {1.0f};
        pl_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
        
        pl_image(ptAppData->ptOffscreenTextureID[ptAppData->tGraphics.uCurrentFrameIndex], (plVec2){500.0f, 500.0f});
        pl_end_window();
    }

    pl_set_next_window_pos((plVec2){0, 0}, PL_UI_COND_ONCE);

    if(pl_begin_window("Pilot Light", NULL, false))
    {

        const float pfRatios[] = {1.0f};
        pl_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
        
        if(pl_collapsing_header("Tools"))
        {
            pl_checkbox("Device Memory Analyzer", &ptAppData->tDebugInfo.bShowDeviceMemoryAnalyzer);
            pl_checkbox("Memory Allocations", &ptAppData->tDebugInfo.bShowMemoryAllocations);
            pl_checkbox("Profiling", &ptAppData->tDebugInfo.bShowProfiling);
            pl_checkbox("Statistics", &ptAppData->tDebugInfo.bShowStats);
            pl_checkbox("Logging", &ptAppData->tDebugInfo.bShowLogging);
            pl_end_collapsing_header();
        }

        if(pl_collapsing_header("User Interface"))
        {
            pl_checkbox("UI Debug", &ptAppData->bShowUiDebug);
            pl_checkbox("UI Demo", &ptAppData->bShowUiDemo);
            pl_checkbox("UI Style", &ptAppData->bShowUiStyle);
            pl_end_collapsing_header();
        }
        pl_end_window();
    }

    gptDebug->show_windows(&ptAppData->tDebugInfo);

    if(ptAppData->bShowUiDemo)
    {
        pl_begin_profile_sample("ui demo");
        pl_show_demo_window(&ptAppData->bShowUiDemo);
        pl_end_profile_sample();
    }
        
    if(ptAppData->bShowUiStyle)
        pl_show_style_editor_window(&ptAppData->bShowUiStyle);

    if(ptAppData->bShowUiDebug)
        pl_show_debug_window(&ptAppData->bShowUiDebug);

    pl_add_line(ptAppData->ptFgDrawLayer, (plVec2){0}, (plVec2){300.0f, 500.0f}, (plVec4){1.0f, 0.0f, 0.0f, 1.0f}, 1.0f);

    const plMat4 tTransform0 = pl_identity_mat4();
    gptGfx->add_3d_transform(&ptAppData->t3DDrawList, &tTransform0, 10.0f, 0.02f);
    gptGfx->add_3d_transform(&ptAppData->tOffscreen3DDrawList, &tTransform0, 10.0f, 0.02f);
    gptGfx->add_3d_triangle_filled(&ptAppData->t3DDrawList, (plVec3){0}, (plVec3){0.0f, 0.0f, 1.0f}, (plVec3){0.0f, 1.0f, 0.0f}, (plVec4){1.0f, 0.0f, 0.0f, 0.25f});

    gptStream->reset(&ptAppData->tDrawStream);

    // object 0
    gptStream->draw(&ptAppData->tDrawStream, (plDraw)
    {
        .uShaderVariant       = ptAppData->tShader2.uIndex,
        .uDynamicBuffer       = tDynamicBinding2.uBufferHandle,
        .uVertexBuffer        = ptAppData->tVertexBuffer.uIndex,
        .uIndexBuffer         = ptAppData->tIndexBuffer.uIndex,
        .uTriangleCount       = 12,
        .uIndexOffset         = 12,
        .uBindGroup0          = ptAppData->atBindGroups0[ptAppData->tGraphics.uCurrentFrameIndex].uIndex,
        .uBindGroup1          = ptAppData->tBindGroup1_2.uIndex,
        .uBindGroup2          = ptAppData->tBindGroup2.uIndex,
        .uDynamicBufferOffset = tDynamicBinding2.uByteOffset
    });

    // object 1
    gptStream->draw(&ptAppData->tDrawStream, (plDraw)
    {
        .uShaderVariant       = ptAppData->tShader0.uIndex,
        .uDynamicBuffer       = tDynamicBinding0.uBufferHandle,
        .uVertexBuffer        = ptAppData->tVertexBuffer.uIndex,
        .uIndexBuffer         = ptAppData->tIndexBuffer.uIndex,
        .uTriangleCount       = 2,
        .uBindGroup0          = ptAppData->atBindGroups0[ptAppData->tGraphics.uCurrentFrameIndex].uIndex,
        .uBindGroup1          = ptAppData->tBindGroup1_0.uIndex,
        .uBindGroup2          = ptAppData->tBindGroup2.uIndex,
        .uDynamicBufferOffset = tDynamicBinding0.uByteOffset
    });

    // object 2
    gptStream->draw(&ptAppData->tDrawStream, (plDraw)
    {
        .uShaderVariant       = ptAppData->tShader1.uIndex,
        .uDynamicBuffer       = tDynamicBinding1.uBufferHandle,
        .uVertexBuffer        = ptAppData->tVertexBuffer.uIndex,
        .uIndexBuffer         = ptAppData->tIndexBuffer.uIndex,
        .uTriangleCount       = 2,
        .uIndexOffset         = 6,
        .uBindGroup0          = ptAppData->atBindGroups0[ptAppData->tGraphics.uCurrentFrameIndex].uIndex,
        .uBindGroup1          = ptAppData->tBindGroup1_1.uIndex,
        .uBindGroup2          = ptAppData->tBindGroup2.uIndex,
        .uDynamicBufferOffset = tDynamicBinding1.uByteOffset
    });

    plDrawArea tArea = {
       .ptDrawStream = &ptAppData->tDrawStream,
       .tScissor = {
            .uWidth  = (uint32_t)pl_get_io()->afMainViewportSize[0],
            .uHeight = (uint32_t)pl_get_io()->afMainViewportSize[1],
       },
       .tViewport = {
            .fWidth  = pl_get_io()->afMainViewportSize[0],
            .fHeight = pl_get_io()->afMainViewportSize[1]
       }
    };
    gptGfx->draw_areas(&ptAppData->tGraphics, 1, &tArea);

    // submit 3D draw list
    gptGfx->submit_3d_drawlist(&ptAppData->t3DDrawList, pl_get_io()->afMainViewportSize[0], pl_get_io()->afMainViewportSize[1], &tMVP, PL_PIPELINE_FLAG_DEPTH_TEST | PL_PIPELINE_FLAG_DEPTH_WRITE, ptAppData->tMainRenderPass, ptAppData->tGraphics.tSwapchain.tMsaaSamples);

    // submit draw layers
    pl_begin_profile_sample("Submit draw layers");
    pl_submit_layer(ptAppData->ptBgDrawLayer);
    pl_submit_layer(ptAppData->ptFgDrawLayer);
    pl_end_profile_sample();

    pl_render();

    // submit draw lists
    pl_begin_profile_sample("Submit draw lists");
    gptGfx->draw_lists(&ptAppData->tGraphics, 1, &ptAppData->tDrawlist, ptAppData->tMainRenderPass);
    gptGfx->draw_lists(&ptAppData->tGraphics, 1, pl_get_draw_list(NULL), ptAppData->tMainRenderPass);
    gptGfx->draw_lists(&ptAppData->tGraphics, 1, pl_get_debug_draw_list(NULL), ptAppData->tMainRenderPass);
    pl_end_profile_sample();

    gptGfx->end_main_pass(&ptAppData->tGraphics);
    gptGfx->end_recording(&ptAppData->tGraphics);
    if(!gptGfx->end_frame(&ptAppData->tGraphics))
        recreate_main_frame_buffer(ptAppData);
    pl_end_profile_sample();
    pl_end_profile_frame();
}

//-----------------------------------------------------------------------------
// [SECTION] helper implementations
//-----------------------------------------------------------------------------

void
create_offscreen_frame_buffer(plAppData* ptAppData)
{
    plIO* ptIO = pl_get_io();
    plDevice* ptDevice = &ptAppData->tGraphics.tDevice;

    pl_sb_resize(ptAppData->sbtOffscreenFrameBuffers, 2);
    for(uint32_t i = 0; i < 2; i++)
    {
        const plFrameBufferDescription tFrameBufferDesc = {
            .tRenderPass      = ptAppData->tOffscreenRenderPass,
            .uWidth           = 500,
            .uHeight          = 500,
            .uAttachmentCount = 2,
            .atViewAttachments = {
                ptAppData->tOffscreenTextureView[i],
                ptAppData->tOffscreenDepthTextureView,
            }
        };
        ptAppData->sbtOffscreenFrameBuffers[i] = gptDevice->create_frame_buffer(ptDevice, &tFrameBufferDesc);
    }
}

void
create_main_frame_buffer(plAppData* ptAppData)
{
    plIO* ptIO = pl_get_io();
    plDevice* ptDevice = &ptAppData->tGraphics.tDevice;

    pl_sb_resize(ptAppData->sbtMainFrameBuffers, ptAppData->tGraphics.tSwapchain.uImageCount);
    for(uint32_t i = 0; i < ptAppData->tGraphics.tSwapchain.uImageCount; i++)
    {
        const plFrameBufferDescription tFrameBufferDesc = {
            .tRenderPass      = ptAppData->tMainRenderPass,
            .uWidth           = (uint32_t)ptIO->afMainViewportSize[0],
            .uHeight          = (uint32_t)ptIO->afMainViewportSize[1],
            .uAttachmentCount = 3,
            .atViewAttachments = {
                ptAppData->tGraphics.tSwapchain.tColorTextureView,
                ptAppData->tGraphics.tSwapchain.tDepthTextureView,
                ptAppData->tGraphics.tSwapchain.sbtSwapchainTextureViews[i]
            }
        };
        ptAppData->sbtMainFrameBuffers[i] = gptDevice->create_frame_buffer(ptDevice, &tFrameBufferDesc);
    }
}

void
recreate_main_frame_buffer(plAppData* ptAppData)
{
    plIO* ptIO = pl_get_io();
    plDevice* ptDevice = &ptAppData->tGraphics.tDevice;

    for(uint32_t i = 0; i < ptAppData->tGraphics.tSwapchain.uImageCount; i++)
        gptDevice->submit_frame_buffer_for_deletion(ptDevice, ptAppData->sbtMainFrameBuffers[i]);

    create_main_frame_buffer(ptAppData);
}

void
create_buffers(plAppData* ptAppData)
{
    plDevice* ptDevice = &ptAppData->tGraphics.tDevice;

    // shader specific buffer
    const BindGroup_2 tShaderSpecificBufferDesc = {
        .tShaderSpecific = {0}
    };
    const plBufferDescription tShaderBufferDesc = {
        .tMemory              = PL_MEMORY_GPU,
        .tUsage               = PL_BUFFER_USAGE_UNIFORM,
        .uByteSize            = sizeof(BindGroup_2),
        .uInitialDataByteSize = sizeof(BindGroup_2),
        .puInitialData        = (uint8_t*)&tShaderSpecificBufferDesc
    };
    ptAppData->tShaderSpecificBuffer = gptDevice->create_buffer(ptDevice, &tShaderBufferDesc, "shader buffer");

    // mesh 0
    {
        const uint32_t uStartIndex = pl_sb_size(ptAppData->sbuIndexBuffer);

        // indices
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 0);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 1);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 2);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 0);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 2);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 3);

        // vertices (position)
        pl_sb_push(ptAppData->sbtVertexPosBuffer, ((plVec3){-0.5f, -0.5f, 0.0f}));
        pl_sb_push(ptAppData->sbtVertexPosBuffer, ((plVec3){-0.5f, 0.5f, 0.0f}));
        pl_sb_push(ptAppData->sbtVertexPosBuffer, ((plVec3){0.5f, 0.5f, 0.0f}));
        pl_sb_push(ptAppData->sbtVertexPosBuffer, ((plVec3){0.5f, -0.5f, 0.0f}));

        // vertices (data - uv, color)
        pl_sb_push(ptAppData->sbtVertexDataBuffer, ((plVec4){0.0f, 1.0f}));
        pl_sb_push(ptAppData->sbtVertexDataBuffer, ((plVec4){0.0f, 1.0f, 1.0f, 1.0f}));
        pl_sb_push(ptAppData->sbtVertexDataBuffer, ((plVec4){0.0f, 0.0f}));
        pl_sb_push(ptAppData->sbtVertexDataBuffer, ((plVec4){0.0f, 1.0f, 1.0f, 1.0f}));
        pl_sb_push(ptAppData->sbtVertexDataBuffer, ((plVec4){1.0f, 0.0f}));
        pl_sb_push(ptAppData->sbtVertexDataBuffer, ((plVec4){0.0f, 1.0f, 1.0f, 1.0f}));
        pl_sb_push(ptAppData->sbtVertexDataBuffer, ((plVec4){1.0f, 1.0f}));
        pl_sb_push(ptAppData->sbtVertexDataBuffer, ((plVec4){0.0f, 1.0f, 1.0f, 1.0f}));
    }
    
    // mesh 1
    {
        const uint32_t uStartIndex = pl_sb_size(ptAppData->sbtVertexPosBuffer);

        // indices
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 0);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 1);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 2);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 0);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 2);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 3);

        // vertices (position)
        pl_sb_push(ptAppData->sbtVertexPosBuffer, ((plVec3){-0.5f, 1.0f, 0.0f}));
        pl_sb_push(ptAppData->sbtVertexPosBuffer, ((plVec3){-0.5f, 2.0f, 0.0f}));
        pl_sb_push(ptAppData->sbtVertexPosBuffer, ((plVec3){0.5f, 2.0f, 0.0f}));
        pl_sb_push(ptAppData->sbtVertexPosBuffer, ((plVec3){0.5f, 1.0f, 0.0f}));

        // vertices (data - color)
        pl_sb_push(ptAppData->sbtVertexDataBuffer, ((plVec4){1.0f, 1.0f, 1.0f, 1.0f}));
        pl_sb_push(ptAppData->sbtVertexDataBuffer, ((plVec4){1.0f, 1.0f, 1.0f, 1.0f}));
        pl_sb_push(ptAppData->sbtVertexDataBuffer, ((plVec4){1.0f, 1.0f, 1.0f, 1.0f}));
        pl_sb_push(ptAppData->sbtVertexDataBuffer, ((plVec4){1.0f, 1.0f, 1.0f, 1.0f}));
    }

    // mesh 2
    {
        const uint32_t uStartIndex = pl_sb_size(ptAppData->sbtVertexPosBuffer);

        // indices
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 0);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 2);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 1);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 2);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 3);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 1);
        
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 1);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 3);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 5);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 3);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 7);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 5);

        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 2);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 6);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 3);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 3);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 6);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 7);
        
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 4);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 5);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 7);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 4);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 7);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 6);
        
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 0);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 4);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 2);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 2);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 4);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 6);
        
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 0);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 1);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 4);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 1);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 5);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 4);

        // vertices (position)
        const float fCubeSide = 0.5f;
        pl_sb_push(ptAppData->sbtVertexPosBuffer, ((plVec3){-fCubeSide, -fCubeSide, -fCubeSide}));
        pl_sb_push(ptAppData->sbtVertexPosBuffer, ((plVec3){ fCubeSide, -fCubeSide, -fCubeSide}));
        pl_sb_push(ptAppData->sbtVertexPosBuffer, ((plVec3){-fCubeSide,  fCubeSide, -fCubeSide}));
        pl_sb_push(ptAppData->sbtVertexPosBuffer, ((plVec3){ fCubeSide,  fCubeSide, -fCubeSide}));
        pl_sb_push(ptAppData->sbtVertexPosBuffer, ((plVec3){-fCubeSide, -fCubeSide,  fCubeSide}));
        pl_sb_push(ptAppData->sbtVertexPosBuffer, ((plVec3){ fCubeSide, -fCubeSide,  fCubeSide}));
        pl_sb_push(ptAppData->sbtVertexPosBuffer, ((plVec3){-fCubeSide,  fCubeSide,  fCubeSide}));
        pl_sb_push(ptAppData->sbtVertexPosBuffer, ((plVec3){ fCubeSide,  fCubeSide,  fCubeSide}));
    }
    
    const plBufferDescription tIndexBufferDesc = {
        .tMemory              = PL_MEMORY_GPU,
        .tUsage               = PL_BUFFER_USAGE_INDEX,
        .uByteSize            = sizeof(uint32_t) * pl_sb_size(ptAppData->sbuIndexBuffer),
        .uInitialDataByteSize = sizeof(uint32_t) * pl_sb_size(ptAppData->sbuIndexBuffer),
        .puInitialData        = (uint8_t*)ptAppData->sbuIndexBuffer
    };
    ptAppData->tIndexBuffer = gptDevice->create_buffer(ptDevice, &tIndexBufferDesc, "index buffer");
    pl_sb_free(ptAppData->sbuIndexBuffer);

    const plBufferDescription tVertexBufferDesc = {
        .tMemory              = PL_MEMORY_GPU,
        .tUsage               = PL_BUFFER_USAGE_VERTEX,
        .uByteSize            = sizeof(plVec3) * pl_sb_size(ptAppData->sbtVertexPosBuffer),
        .uInitialDataByteSize = sizeof(plVec3) * pl_sb_size(ptAppData->sbtVertexPosBuffer),
        .puInitialData        = (uint8_t*)ptAppData->sbtVertexPosBuffer
    };
    ptAppData->tVertexBuffer = gptDevice->create_buffer(ptDevice, &tVertexBufferDesc, "vertex buffer");
    pl_sb_free(ptAppData->sbtVertexPosBuffer);

    const plBufferDescription tStorageBufferDesc = {
        .tMemory              = PL_MEMORY_GPU,
        .tUsage               = PL_BUFFER_USAGE_STORAGE,
        .uByteSize            = sizeof(plVec4) * pl_sb_size(ptAppData->sbtVertexDataBuffer),
        .uInitialDataByteSize = sizeof(plVec4) * pl_sb_size(ptAppData->sbtVertexDataBuffer),
        .puInitialData        = (uint8_t*)ptAppData->sbtVertexDataBuffer
    };
    ptAppData->tStorageBuffer = gptDevice->create_buffer(ptDevice, &tStorageBufferDesc, "storage buffer");
    pl_sb_free(ptAppData->sbtVertexDataBuffer);

    const plBufferDescription atGlobalBuffersDesc = {
        .tMemory              = PL_MEMORY_CPU,
        .tUsage               = PL_BUFFER_USAGE_UNIFORM,
        .uByteSize            = sizeof(BindGroup_0),
        .uInitialDataByteSize = 0,
        .puInitialData        = NULL
    };
    ptAppData->atGlobalBuffers[0] = gptDevice->create_buffer(ptDevice, &atGlobalBuffersDesc, "global buffer 0");
    ptAppData->atGlobalBuffers[1] = gptDevice->create_buffer(ptDevice, &atGlobalBuffersDesc, "global buffer 1");
    ptAppData->atOffscreenGlobalBuffers[0] = gptDevice->create_buffer(ptDevice, &atGlobalBuffersDesc, "offscreen global buffer 0");
    ptAppData->atOffscreenGlobalBuffers[1] = gptDevice->create_buffer(ptDevice, &atGlobalBuffersDesc, "offscreen global buffer 1");
}

void
create_offscreen_textures(plAppData* ptAppData)
{
    plDevice* ptDevice = &ptAppData->tGraphics.tDevice;

    for(uint32_t i = 0; i < 2; i++)
    {
        plTextureDesc tTextureDesc = {
            .tDimensions = {500.0f, 500.0f, 1},
            .tFormat = PL_FORMAT_R8G8B8A8_UNORM,
            .uLayers = 1,
            .uMips = 1,
            .tType = PL_TEXTURE_TYPE_2D,
            .tUsage = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
            .tSamples = PL_SAMPLE_COUNT_1
        };
        ptAppData->tOffscreenTexture[i] = gptDevice->create_texture(ptDevice, tTextureDesc, 0, NULL, "offscreen texture");

        plTextureViewDesc tTextureViewDesc = {
            .tFormat     = PL_FORMAT_R8G8B8A8_UNORM,
            .uBaseLayer  = 0,
            .uBaseMip    = 0,
            .uLayerCount = 1
        };
        plSampler tSampler = {
            .tFilter = PL_FILTER_NEAREST,
            .fMinMip = 0.0f,
            .fMaxMip = 64.0f,
            .tVerticalWrap = PL_WRAP_MODE_CLAMP,
            .tHorizontalWrap = PL_WRAP_MODE_CLAMP
        };
        ptAppData->tOffscreenTextureView[i] = gptDevice->create_texture_view(ptDevice, &tTextureViewDesc, &tSampler, ptAppData->tOffscreenTexture[i], "offscreen texture view");

        ptAppData->ptOffscreenTextureID[i] = gptGfx->get_ui_texture_handle(&ptAppData->tGraphics, ptAppData->tOffscreenTextureView[i]);
    }

    {

        plTextureDesc tTextureDesc = {
            .tDimensions = {500.0f, 500.0f, 1},
            .tFormat = PL_FORMAT_D32_FLOAT,
            .uLayers = 1,
            .uMips = 1,
            .tType = PL_TEXTURE_TYPE_2D,
            .tUsage = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
            .tSamples = PL_SAMPLE_COUNT_1
        };
        ptAppData->tOffscreenDepthTexture = gptDevice->create_texture(ptDevice, tTextureDesc, 0, NULL, "offscreen depth texture");

        plTextureViewDesc tTextureViewDesc = {
            .tFormat     = PL_FORMAT_D32_FLOAT,
            .uBaseLayer  = 0,
            .uBaseMip    = 0,
            .uLayerCount = 1
        };
        plSampler tSampler = {
            .tFilter = PL_FILTER_NEAREST,
            .fMinMip = 0.0f,
            .fMaxMip = 64.0f,
            .tVerticalWrap = PL_WRAP_MODE_CLAMP,
            .tHorizontalWrap = PL_WRAP_MODE_CLAMP
        };
        ptAppData->tOffscreenDepthTextureView = gptDevice->create_texture_view(ptDevice, &tTextureViewDesc, &tSampler, ptAppData->tOffscreenDepthTexture, "offscreen depth texture view");

    }
}

void
create_textures(plAppData* ptAppData)
{
    plDevice* ptDevice = &ptAppData->tGraphics.tDevice;

    {
        static float image[] = {
            1.0f,   0,   0, 1.0f,
            0, 1.0f,   0, 1.0f,
            0,   0, 1.0f, 1.0f,
            1.0f,   0, 1.0f, 1.0f
        };
        plTextureDesc tTextureDesc = {
            .tDimensions = {2, 2, 1},
            .tFormat = PL_FORMAT_R32G32B32A32_FLOAT,
            .uLayers = 1,
            .uMips = 0,
            .tType = PL_TEXTURE_TYPE_2D,
            .tUsage = PL_TEXTURE_USAGE_SAMPLED,
            .tSamples = PL_SAMPLE_COUNT_1
        };
        ptAppData->tTexture = gptDevice->create_texture(ptDevice, tTextureDesc, sizeof(float) * 4 * 4, image, "texture");

        plTextureViewDesc tTextureViewDesc = {
            .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
            .uBaseLayer  = 0,
            .uBaseMip    = 0,
            .uLayerCount = 1
        };
        plSampler tSampler = {
            .tFilter = PL_FILTER_NEAREST,
            .fMinMip = 0.0f,
            .fMaxMip = 64.0f,
            .tVerticalWrap = PL_WRAP_MODE_CLAMP,
            .tHorizontalWrap = PL_WRAP_MODE_CLAMP
        };
        ptAppData->tTextureView = gptDevice->create_texture_view(ptDevice, &tTextureViewDesc, &tSampler, ptAppData->tTexture, "texture view");

    }

    {
        static float image[] = {
            0.25f,   0,   0, 1.0f,
            0, 0.25f,   0, 1.0f,
            0,   0, 0.25f, 1.0f,
            0.25f,   0, 0.25f, 1.0f,
            0.25f,   0.25, 0.25f, 1.0f,
            0.0f,   0.25, 0.25f, 1.0f
        };
        plTextureDesc tTextureDesc = {
            .tDimensions = {1, 1, 1},
            .tFormat = PL_FORMAT_R32G32B32A32_FLOAT,
            .uLayers = 6,
            .uMips = 1,
            .tType = PL_TEXTURE_TYPE_CUBE,
            .tUsage = PL_TEXTURE_USAGE_SAMPLED,
            .tSamples = PL_SAMPLE_COUNT_1
        };
        ptAppData->tSkyboxTexture = gptDevice->create_texture(ptDevice, tTextureDesc, sizeof(float) * 4 * 6, image, "skybox texture");

        plTextureViewDesc tTextureViewDesc = {
            .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
            .uBaseLayer  = 0,
            .uBaseMip    = 0,
            .uLayerCount = 6
        };
        plSampler tSampler = {
            .tFilter = PL_FILTER_NEAREST,
            .fMinMip = 0.0f,
            .fMaxMip = 64.0f,
            .tVerticalWrap = PL_WRAP_MODE_CLAMP,
            .tHorizontalWrap = PL_WRAP_MODE_CLAMP
        };
        ptAppData->tSkyboxTextureView = gptDevice->create_texture_view(ptDevice, &tTextureViewDesc, &tSampler, ptAppData->tSkyboxTexture, "skybox texture view");
    }
}

void
create_bind_groups(plAppData* ptAppData)
{

    plDevice* ptDevice = &ptAppData->tGraphics.tDevice;

    plBindGroupLayout tBindGroupLayout0 = {
        .uBufferCount  = 2,
        .aBuffers = {
            {
                .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                .uSlot = 0
            },
            {
                .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                .uSlot = 1
            },
        }
    };
    ptAppData->atBindGroups0[0] = gptDevice->create_bind_group(ptDevice, &tBindGroupLayout0);
    ptAppData->atBindGroups0[1] = gptDevice->create_bind_group(ptDevice, &tBindGroupLayout0);
    size_t szBufferRangeSize[2] = {sizeof(BindGroup_0), sizeof(BindGroup_0)};

    plBufferHandle atBindGroup0_buffers0[2] = {ptAppData->atGlobalBuffers[0], ptAppData->tStorageBuffer};
    plBufferHandle atBindGroup0_buffers1[2] = {ptAppData->atGlobalBuffers[1], ptAppData->tStorageBuffer};
    gptDevice->update_bind_group(ptDevice, &ptAppData->atBindGroups0[0], 2, atBindGroup0_buffers0, szBufferRangeSize, 0, NULL);
    gptDevice->update_bind_group(ptDevice, &ptAppData->atBindGroups0[1], 2, atBindGroup0_buffers1, szBufferRangeSize, 0, NULL);

    ptAppData->atBindGroups0Offscreen[0] = gptDevice->create_bind_group(ptDevice, &tBindGroupLayout0);
    ptAppData->atBindGroups0Offscreen[1] = gptDevice->create_bind_group(ptDevice, &tBindGroupLayout0);
    plBufferHandle atBindGroup0_buffers0Off[2] = {ptAppData->atOffscreenGlobalBuffers[0], ptAppData->tStorageBuffer};
    plBufferHandle atBindGroup0_buffers1Off[2] = {ptAppData->atOffscreenGlobalBuffers[1], ptAppData->tStorageBuffer};
    gptDevice->update_bind_group(ptDevice, &ptAppData->atBindGroups0Offscreen[0], 2, atBindGroup0_buffers0Off, szBufferRangeSize, 0, NULL);
    gptDevice->update_bind_group(ptDevice, &ptAppData->atBindGroups0Offscreen[1], 2, atBindGroup0_buffers1Off, szBufferRangeSize, 0, NULL);

    plBindGroupLayout tBindGroupLayout1_0 = {
        .uTextureCount  = 1,
        .aTextures = {
            {.uSlot = 0}
        }
    };
    ptAppData->tBindGroup1_0 = gptDevice->create_bind_group(ptDevice, &tBindGroupLayout1_0);
    gptDevice->update_bind_group(ptDevice, &ptAppData->tBindGroup1_0, 0, NULL, NULL, 1, &ptAppData->tTextureView);

    plBindGroupLayout tBindGroupLayout1_1 = {
        .uTextureCount  = 1,
        .aTextures = {
            {.uSlot = 0}
        }
    };
    ptAppData->tBindGroup1_1 = gptDevice->create_bind_group(ptDevice, &tBindGroupLayout1_1);
    gptDevice->update_bind_group(ptDevice, &ptAppData->tBindGroup1_1, 0, NULL, NULL, 1, &ptAppData->tTextureView);

    plBindGroupLayout tBindGroupLayout1_2 = {
        .uTextureCount  = 1,
        .aTextures = {
            {.uSlot = 0}
        }
    };
    ptAppData->tBindGroup1_2 = gptDevice->create_bind_group(ptDevice, &tBindGroupLayout1_2);
    gptDevice->update_bind_group(ptDevice, &ptAppData->tBindGroup1_2, 0, NULL, NULL, 1, &ptAppData->tSkyboxTextureView);

    plBindGroupLayout tBindGroupLayout2_0 = {
        .uBufferCount  = 1,
        .aBuffers = {
            {
                .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                .uSlot = 0
            },
        }
    };
    ptAppData->tBindGroup2 = gptDevice->create_bind_group(ptDevice, &tBindGroupLayout2_0);
    size_t szGroup2BufferRange = sizeof(BindGroup_2);
    gptDevice->update_bind_group(ptDevice, &ptAppData->tBindGroup2, 1, &ptAppData->tShaderSpecificBuffer, &szGroup2BufferRange, 0, NULL);
}

void
create_shaders(plAppData* ptAppData)
{

    plDevice* ptDevice = &ptAppData->tGraphics.tDevice;
    plShaderDescription tShaderDescription0 = {

#ifdef PL_METAL_BACKEND
        .pcVertexShader = "../shaders/metal/primitive.metal",
        .pcPixelShader = "../shaders/metal/primitive.metal",
#else // VULKAN
        .pcVertexShader = "primitive.vert.spv",
        .pcPixelShader = "primitive.frag.spv",
#endif

        .tGraphicsState = {
            .ulDepthWriteEnabled  = 1,
            .ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0 | PL_MESH_FORMAT_FLAG_HAS_COLOR_0,
            .ulBlendMode          = PL_BLEND_MODE_ALPHA,
            .ulDepthMode          = PL_DEPTH_MODE_LESS_OR_EQUAL,
            .ulCullMode           = PL_CULL_MODE_CULL_BACK,
            .ulShaderTextureFlags = PL_SHADER_TEXTURE_FLAG_BINDING_0,
            .ulStencilMode        = PL_STENCIL_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .tRenderPass = ptAppData->tMainRenderPass,
        .uBindGroupLayoutCount = 3,
        .atBindGroupLayouts = {
            {
                .uBufferCount  = 2,
                .aBuffers = {
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                        .uSlot = 0
                    },
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                        .uSlot = 1
                    }
                },
            },
            {
                .uTextureCount = 1,
                .aTextures = {
                    {
                        .uSlot = 0
                    }
                 },
            },
            {
                .uBufferCount  = 1,
                .aBuffers = {
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                        .uSlot = 0
                    },
                },
            }
        }
    };
    ptAppData->tShader0 = gptDevice->create_shader(ptDevice, &tShaderDescription0);
    tShaderDescription0.tRenderPass = ptAppData->tOffscreenRenderPass;
    ptAppData->tOffscreenShader0 = gptDevice->create_shader(ptDevice, &tShaderDescription0);

    plShaderDescription tShaderDescription1 = {
#ifdef PL_METAL_BACKEND
        .pcVertexShader = "../shaders/metal/primitive.metal",
        .pcPixelShader = "../shaders/metal/primitive.metal",
#else // linux
        .pcVertexShader = "primitive.vert.spv",
        .pcPixelShader = "primitive.frag.spv",
#endif
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 1,
            .ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_HAS_COLOR_0,
            .ulBlendMode          = PL_BLEND_MODE_ALPHA,
            .ulDepthMode          = PL_DEPTH_MODE_LESS_OR_EQUAL,
            .ulCullMode           = PL_CULL_MODE_NONE,
            .ulShaderTextureFlags = PL_SHADER_TEXTURE_FLAG_BINDING_NONE,
            .ulStencilMode        = PL_STENCIL_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .tRenderPass = ptAppData->tMainRenderPass,
        .uBindGroupLayoutCount = 3,
        .atBindGroupLayouts = {
            {
                .uBufferCount  = 2,
                .aBuffers = {
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                        .uSlot = 0
                    },
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                        .uSlot = 1
                    },
                },
            },
            {
                .uTextureCount = 1,
                .aTextures = {
                    {
                        .uSlot = 0
                    }
                 },
            },
            {
                .uBufferCount  = 1,
                .aBuffers = {
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                        .uSlot = 0
                    },
                },
            }
        }
    };
    ptAppData->tShader1 = gptDevice->create_shader(ptDevice, &tShaderDescription1);
    tShaderDescription1.tRenderPass = ptAppData->tOffscreenRenderPass;
    ptAppData->tOffscreenShader1 = gptDevice->create_shader(ptDevice, &tShaderDescription1);

    plShaderDescription tShaderDescription2 = {
#ifdef PL_METAL_BACKEND
        .pcVertexShader = "../shaders/metal/skybox.metal",
        .pcPixelShader = "../shaders/metal/skybox.metal",
#else // linux
        .pcVertexShader = "skybox.vert.spv",
        .pcPixelShader = "skybox.frag.spv",
#endif
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 0,
            .ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_NONE,
            .ulBlendMode          = PL_BLEND_MODE_ALPHA,
            .ulDepthMode          = PL_DEPTH_MODE_LESS_OR_EQUAL,
            .ulCullMode           = PL_CULL_MODE_NONE,
            .ulShaderTextureFlags = PL_SHADER_TEXTURE_FLAG_BINDING_0,
            .ulStencilMode        = PL_STENCIL_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .tRenderPass = ptAppData->tMainRenderPass,
        .uBindGroupLayoutCount = 3,
        .atBindGroupLayouts = {
            {
                .uBufferCount  = 2,
                .aBuffers = {
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                        .uSlot = 0
                    },
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                        .uSlot = 1
                    },
                },
            },
            {
                .uTextureCount = 1,
                .aTextures = {
                    {
                        .uSlot = 0
                    }
                 },
            },
            {
                .uBufferCount  = 1,
                .aBuffers = {
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                        .uSlot = 0
                    },
                },
            }
        }
    };
    ptAppData->tShader2 = gptDevice->create_shader(ptDevice, &tShaderDescription2);
    tShaderDescription2.tRenderPass = ptAppData->tOffscreenRenderPass;
    ptAppData->tOffscreenShader2 = gptDevice->create_shader(ptDevice, &tShaderDescription2);
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#include "camera.c"