/*
   app.c (just experimental & absolute mess)
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
#include "pl_ecs_ext.h"
#include "pl_resource_ext.h"

// misc
#include "helper_windows.h"

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
    plRenderPassLayoutHandle tMainRenderPassLayout;
    plRenderPassLayoutHandle tOffscreenPassLayout;
    plRenderPassHandle   tMainRenderPass;
    plRenderPassHandle   tOffscreenRenderPass;

    // offscreen
    plVec2              tOffscreenTargetSize;
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
    plBindGroupHandle tBindGroup1_0;
    plBindGroupHandle tBindGroup1_1;
    plBindGroupHandle tBindGroup1_2;
    plBindGroupHandle tBindGroup2;
    plBindGroupHandle tBindGroupCompute;
    
    // shaders
    plComputeShaderHandle tComputeShader0;
    plShaderHandle        tShader0;
    plShaderHandle        tShader1;
    plShaderHandle        tShader2;
    plShaderHandle        tOffscreenShader0;
    plShaderHandle        tOffscreenShader1;
    plShaderHandle        tOffscreenShader2;

    // global index/vertex/data buffers
    plBufferHandle tComputeUniformBuffer;
    plBufferHandle tVertexBuffer;
    plBufferHandle tIndexBuffer;
    plBufferHandle tStorageBuffer;
    plBufferHandle tShaderSpecificBuffer;
    plBufferHandle atGlobalBuffers[2];
    plBufferHandle atOffscreenGlobalBuffers[2];

    // ecs
    bool               bShowEntityWindow;
    plComponentLibrary tComponentLibrary;
    plEntity           tMainCamera;
    plEntity           tOffscreenCamera;

    // scene
    plDrawStream   tDrawStream;
    plDrawList3D   t3DDrawList;
    plDrawList3D   tOffscreen3DDrawList;

    plVec3*   sbtVertexPosBuffer;
    plVec4*   sbtVertexDataBuffer;
    uint32_t* sbuIndexBuffer;
    bool      bReloadSwapchain;
} plAppData;

typedef struct _BindGroup_0
{
    plVec4 tCameraPos;
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

enum plShaderTextureFlags
{
    PL_SHADER_TEXTURE_FLAG_BINDING_NONE = 0,
    PL_SHADER_TEXTURE_FLAG_BINDING_0    = 1 << 0,
    PL_SHADER_TEXTURE_FLAG_BINDING_1    = 1 << 1,
    PL_SHADER_TEXTURE_FLAG_BINDING_2    = 1 << 2,
    PL_SHADER_TEXTURE_FLAG_BINDING_3    = 1 << 3,
    PL_SHADER_TEXTURE_FLAG_BINDING_4    = 1 << 4,
    PL_SHADER_TEXTURE_FLAG_BINDING_5    = 1 << 5,
    PL_SHADER_TEXTURE_FLAG_BINDING_6    = 1 << 6,
    PL_SHADER_TEXTURE_FLAG_BINDING_7    = 1 << 7,
    PL_SHADER_TEXTURE_FLAG_BINDING_8    = 1 << 8,
    PL_SHADER_TEXTURE_FLAG_BINDING_9    = 1 << 9,
    PL_SHADER_TEXTURE_FLAG_BINDING_10   = 1 << 10,
    PL_SHADER_TEXTURE_FLAG_BINDING_11   = 1 << 11,
    PL_SHADER_TEXTURE_FLAG_BINDING_12   = 1 << 12,
    PL_SHADER_TEXTURE_FLAG_BINDING_13   = 1 << 13,
    PL_SHADER_TEXTURE_FLAG_BINDING_14   = 1 << 14
};

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
const plImageI*                gptImage             = NULL;
const plDrawStreamI*           gptStream            = NULL;
const plEcsI*                  gptEcs               = NULL;
const plCameraI*               gptCamera            = NULL;
const plResourceI*             gptResource          = NULL;

//-----------------------------------------------------------------------------
// [SECTION] helpers
//-----------------------------------------------------------------------------

void create_offscreen_render_pass(plAppData* ptAppData);
void create_buffers              (plAppData* ptAppData);
void create_offscreen_textures   (plAppData* ptAppData);
void create_textures             (plAppData* ptAppData);
void create_bind_groups          (plAppData* ptAppData);
void create_shaders              (plAppData* ptAppData);
void create_main_render_pass     (plAppData* ptAppData);
void recreate_main_render_pass   (plAppData* ptAppData);

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
        gptStats   = ptApiRegistry->first(PL_API_STATS);
        gptFile    = ptApiRegistry->first(PL_API_FILE);
        gptGfx     = ptApiRegistry->first(PL_API_GRAPHICS);
        gptDevice  = ptApiRegistry->first(PL_API_DEVICE);
        gptDebug   = ptApiRegistry->first(PL_API_DEBUG);
        gptImage   = ptApiRegistry->first(PL_API_IMAGE);
        gptStream  = ptApiRegistry->first(PL_API_DRAW_STREAM);
        gptEcs     = ptApiRegistry->first(PL_API_ECS);
        gptCamera  = ptApiRegistry->first(PL_API_CAMERA);
        gptResource = ptApiRegistry->first(PL_API_RESOURCE);

        return ptAppData;
    }

    plProfileContext* ptProfileCtx = pl_create_profile_context();
    plLogContext*     ptLogCtx     = pl_create_log_context();
    
    // add some context to data registry
    ptAppData = PL_ALLOC(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));
    ptAppData->tOffscreenTargetSize.x = 1024.0f;
    ptAppData->tOffscreenTargetSize.y = 1024.0f;
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
    ptExtensionRegistry->load("pl_ecs_ext",      "pl_load_ecs_ext",      "pl_unload_ecs_ext",      false);
    ptExtensionRegistry->load("pl_resource_ext", "pl_load_resource_ext", "pl_unload_resource_ext", false);

    // load apis
    gptStats    = ptApiRegistry->first(PL_API_STATS);
    gptFile     = ptApiRegistry->first(PL_API_FILE);
    gptGfx      = ptApiRegistry->first(PL_API_GRAPHICS);
    gptDevice   = ptApiRegistry->first(PL_API_DEVICE);
    gptDebug    = ptApiRegistry->first(PL_API_DEBUG);
    gptImage    = ptApiRegistry->first(PL_API_IMAGE);
    gptStream   = ptApiRegistry->first(PL_API_DRAW_STREAM);
    gptEcs      = ptApiRegistry->first(PL_API_ECS);
    gptCamera   = ptApiRegistry->first(PL_API_CAMERA);
    gptResource = ptApiRegistry->first(PL_API_RESOURCE);

    // initialize ecs
    gptEcs->init_component_library(&ptAppData->tComponentLibrary);

    // create command queue
    ptAppData->tGraphics.bValidationActive = true;
    gptGfx->initialize(&ptAppData->tGraphics);
    plDevice* ptDevice = &ptAppData->tGraphics.tDevice;
    gptDataRegistry->set_data("device", ptDevice);

    // create camera
    plIO* ptIO = pl_get_io();
    ptAppData->tMainCamera = gptEcs->create_camera(&ptAppData->tComponentLibrary, "main camera", (plVec3){-6.211f, 3.647f, 0.827f}, PL_PI_3, ptIO->afMainViewportSize[0] / ptIO->afMainViewportSize[1], 0.01f, 400.0f);
    ptAppData->tOffscreenCamera = gptEcs->create_camera(&ptAppData->tComponentLibrary, "offscreen camera", (plVec3){0.0f, 0.0f, 2.0f}, PL_PI_3, 1.0f, 0.01f, 400.0f);
    gptCamera->set_pitch_yaw(gptEcs->get_component(&ptAppData->tComponentLibrary, PL_COMPONENT_TYPE_CAMERA, ptAppData->tOffscreenCamera), 0.0f, PL_PI);
    gptCamera->set_pitch_yaw(gptEcs->get_component(&ptAppData->tComponentLibrary, PL_COMPONENT_TYPE_CAMERA, ptAppData->tMainCamera), -0.244f, 1.488f);
    gptCamera->update(gptEcs->get_component(&ptAppData->tComponentLibrary, PL_COMPONENT_TYPE_CAMERA, ptAppData->tMainCamera));
    
    // create draw list & layers
    pl_register_drawlist(&ptAppData->tDrawlist);
    ptAppData->ptBgDrawLayer = pl_request_layer(&ptAppData->tDrawlist, "Background Layer");
    ptAppData->ptFgDrawLayer = pl_request_layer(&ptAppData->tDrawlist, "Foreground Layer");
    
    // create font atlas
    pl_add_default_font(&ptAppData->tFontAtlas);
    pl_build_font_atlas(&ptAppData->tFontAtlas);

    // 3D tDrawlist
    gptGfx->register_3d_drawlist(&ptAppData->tGraphics, &ptAppData->t3DDrawList);
    gptGfx->register_3d_drawlist(&ptAppData->tGraphics, &ptAppData->tOffscreen3DDrawList);

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
    ptAppData->tMainRenderPassLayout = gptDevice->create_render_pass_layout(ptDevice, &tMainRenderPassLayoutDesc);

    const plRenderPassLayoutDescription tOffscreenRenderPassLayoutDesc = {
        .tDepthTarget = { .tFormat = PL_FORMAT_D32_FLOAT, .tSampleCount = PL_SAMPLE_COUNT_1 },
        .atRenderTargets = {
            { .tFormat = PL_FORMAT_R32G32B32A32_FLOAT, .tSampleCount = PL_SAMPLE_COUNT_1 }
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
    ptAppData->tOffscreenPassLayout = gptDevice->create_render_pass_layout(ptDevice, &tOffscreenRenderPassLayoutDesc);

    create_main_render_pass(ptAppData);
    gptGfx->setup_ui(&ptAppData->tGraphics, ptAppData->tMainRenderPass);

    gptGfx->start_transfers(&ptAppData->tGraphics);
    create_buffers(ptAppData);
    create_textures(ptAppData);
    create_offscreen_textures(ptAppData);
    create_offscreen_render_pass(ptAppData);
    create_bind_groups(ptAppData);
    create_shaders(ptAppData);
    gptGfx->end_transfers(&ptAppData->tGraphics);

    gptGfx->create_font_atlas(&ptAppData->tFontAtlas);
    pl_set_default_font(&ptAppData->tFontAtlas.sbtFonts[0]);
    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    gptEcs->cleanup_component_library(&ptAppData->tComponentLibrary);
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
    gptCamera->set_aspect(gptEcs->get_component(&ptAppData->tComponentLibrary, PL_COMPONENT_TYPE_CAMERA, ptAppData->tMainCamera), ptIO->afMainViewportSize[0] / ptIO->afMainViewportSize[1]);
    gptGfx->resize(&ptAppData->tGraphics);
    recreate_main_render_pass(ptAppData);
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

    if(ptAppData->bReloadSwapchain)
    {
        ptAppData->bReloadSwapchain = false;
        gptGfx->resize(&ptAppData->tGraphics);
        recreate_main_render_pass(ptAppData);
        pl_end_profile_sample();
        pl_end_profile_frame();
        return;
    }

    plIO* ptIO = pl_get_io();
    plDevice* ptDevice = &ptAppData->tGraphics.tDevice;

    plCameraComponent* ptMainCamera = gptEcs->get_component(&ptAppData->tComponentLibrary, PL_COMPONENT_TYPE_CAMERA, ptAppData->tMainCamera);
    plCameraComponent* ptOffscreenCamera = gptEcs->get_component(&ptAppData->tComponentLibrary, PL_COMPONENT_TYPE_CAMERA, ptAppData->tOffscreenCamera);

    if(!gptGfx->begin_frame(&ptAppData->tGraphics))
    {
        recreate_main_render_pass(ptAppData);
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
    static const float fCameraTravelSpeed = 4.0f;
    static const float fCameraRotationSpeed = 0.005f;

    // camera space
    if(pl_is_key_down(PL_KEY_W)) gptCamera->translate(ptMainCamera,  0.0f,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime);
    if(pl_is_key_down(PL_KEY_S)) gptCamera->translate(ptMainCamera,  0.0f,  0.0f, -fCameraTravelSpeed* ptIO->fDeltaTime);
    if(pl_is_key_down(PL_KEY_A)) gptCamera->translate(ptMainCamera, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);
    if(pl_is_key_down(PL_KEY_D)) gptCamera->translate(ptMainCamera,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);

    // world space
    if(pl_is_key_down(PL_KEY_F)) gptCamera->translate(ptMainCamera,  0.0f, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f);
    if(pl_is_key_down(PL_KEY_R)) gptCamera->translate(ptMainCamera,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f);

    bool bOwnMouse = ptIO->bWantCaptureMouse;
    if(!bOwnMouse && pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 1.0f))
    {
        const plVec2 tMouseDelta = pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f);
        gptCamera->rotate(ptMainCamera,  -tMouseDelta.y * fCameraRotationSpeed,  -tMouseDelta.x * fCameraRotationSpeed);
        pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
    }
    gptCamera->update(ptMainCamera);
    gptCamera->update(ptOffscreenCamera);

    plBindGroupLayout tBindGroupLayout0 = {
        .uBufferCount  = 2,
        .aBuffers = {
            {
                .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                .uSlot = 0,
                .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
            },
            {
                .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                .uSlot = 1,
                .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
            },
        }
    };
    plBindGroupHandle tGlobalBindGroup = gptDevice->get_temporary_bind_group(ptDevice, &tBindGroupLayout0);
    plBindGroupHandle tGlobalOffscreenBindGroup = gptDevice->get_temporary_bind_group(ptDevice, &tBindGroupLayout0);
    size_t szBufferRangeSize00[2] = {sizeof(BindGroup_0), pl_sb_size(ptAppData->sbtVertexDataBuffer) * sizeof(plVec4)};

    plBufferHandle atBindGroup0_buffers00[] = {ptAppData->atGlobalBuffers[ptAppData->tGraphics.uCurrentFrameIndex], ptAppData->tStorageBuffer};
    plBufferHandle atBindGroup0_buffers11[] = {ptAppData->atOffscreenGlobalBuffers[ptAppData->tGraphics.uCurrentFrameIndex], ptAppData->tStorageBuffer};
    gptDevice->update_bind_group(ptDevice, &tGlobalBindGroup, 2, atBindGroup0_buffers00, szBufferRangeSize00, 0, NULL);
    gptDevice->update_bind_group(ptDevice, &tGlobalOffscreenBindGroup, 2, atBindGroup0_buffers11, szBufferRangeSize00, 0, NULL);

    const BindGroup_0 tBindGroupBufferOffscreen = {
        .tCameraPos = ptOffscreenCamera->tPos,
        .tCameraProjection = ptOffscreenCamera->tProjMat,
        .tCameraView = ptOffscreenCamera->tViewMat,
        .tCameraViewProjection = pl_mul_mat4(&ptOffscreenCamera->tProjMat, &ptOffscreenCamera->tViewMat)
    };
    memcpy(ptGraphics->sbtBuffersCold[ptAppData->atOffscreenGlobalBuffers[ptAppData->tGraphics.uCurrentFrameIndex].uIndex].tMemoryAllocation.pHostMapped, &tBindGroupBufferOffscreen, sizeof(BindGroup_0));

    const BindGroup_0 tBindGroupBuffer = {
        .tCameraPos = ptMainCamera->tPos,
        .tCameraProjection = ptMainCamera->tProjMat,
        .tCameraView = ptMainCamera->tViewMat,
        .tCameraViewProjection = pl_mul_mat4(&ptMainCamera->tProjMat, &ptMainCamera->tViewMat)
    };
    memcpy(ptGraphics->sbtBuffersCold[ptAppData->atGlobalBuffers[ptAppData->tGraphics.uCurrentFrameIndex].uIndex].tMemoryAllocation.pHostMapped, &tBindGroupBuffer, sizeof(BindGroup_0));

    const plMat4 tMVP = pl_mul_mat4(&ptMainCamera->tProjMat, &ptMainCamera->tViewMat);

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
    *ptDynamicData2 = pl_mat4_translate_vec3(ptMainCamera->tPos);

    plDynamicBinding tDynamicBinding3 = gptDevice->allocate_dynamic_data(ptDevice, sizeof(plMat4));
    plMat4* ptDynamicData3 = (plMat4*)tDynamicBinding3.pcData;
    *ptDynamicData3 = pl_mat4_translate_vec3(ptOffscreenCamera->tPos);

    gptGfx->begin_recording(&ptAppData->tGraphics);
    gptGfx->begin_pass(&ptAppData->tGraphics, ptAppData->tOffscreenRenderPass);

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
        .uBindGroup0          = tGlobalOffscreenBindGroup.uIndex,
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
        .uBindGroup0          = tGlobalOffscreenBindGroup.uIndex,
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
        .uBindGroup0          = tGlobalOffscreenBindGroup.uIndex,
        .uBindGroup1          = ptAppData->tBindGroup1_1.uIndex,
        .uBindGroup2          = ptAppData->tBindGroup2.uIndex,
        .uDynamicBufferOffset = tDynamicBinding1.uByteOffset
    });

    plDrawArea tArea0 = {
       .ptDrawStream = &ptAppData->tDrawStream,
       .tScissor = {
            .uWidth  = (uint32_t)ptAppData->tOffscreenTargetSize.x,
            .uHeight = (uint32_t)ptAppData->tOffscreenTargetSize.y,
       },
       .tViewport = {
            .fWidth  = ptAppData->tOffscreenTargetSize.x,
            .fHeight = ptAppData->tOffscreenTargetSize.y
       }
    };
    gptGfx->draw_areas(&ptAppData->tGraphics, 1, &tArea0);

    const plMat4 tOffscreenMVP = pl_mul_mat4(&ptOffscreenCamera->tProjMat, &ptOffscreenCamera->tViewMat);
    const plMat4 tTransform00 = pl_identity_mat4();
    gptGfx->add_3d_transform(&ptAppData->tOffscreen3DDrawList, &tTransform00, 10.0f, 0.02f);
    gptGfx->add_3d_frustum(&ptAppData->t3DDrawList, &ptOffscreenCamera->tTransformMat, ptOffscreenCamera->fFieldOfView, ptOffscreenCamera->fAspectRatio, ptOffscreenCamera->fNearZ, ptOffscreenCamera->fFarZ, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 0.02f);
    gptGfx->submit_3d_drawlist(&ptAppData->tOffscreen3DDrawList, ptAppData->tOffscreenTargetSize.x, ptAppData->tOffscreenTargetSize.y, &tOffscreenMVP, PL_PIPELINE_FLAG_DEPTH_TEST | PL_PIPELINE_FLAG_DEPTH_WRITE, ptAppData->tOffscreenRenderPass, 1);
    gptGfx->end_pass(&ptAppData->tGraphics);
    gptGfx->begin_main_pass(&ptAppData->tGraphics, ptAppData->tMainRenderPass);

    pl_new_frame();

    if(pl_begin_window("Offscreen", NULL, false))
    {

        const float pfRatios[] = {1.0f};
        pl_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);

        if(pl_button("Read"))
        {
            const plBufferDescription tReadbackBufferDesc = {
                .tMemory              = PL_MEMORY_CPU,
                .uByteSize            = (uint32_t)ptAppData->tOffscreenTargetSize.x * (uint32_t)ptAppData->tOffscreenTargetSize.y * 4 * sizeof(float),
            };
            plBufferHandle tReadbackBuffer = gptDevice->create_buffer(ptDevice, &tReadbackBufferDesc, "readback buffer");
            plBufferHandle tReadbackBuffer2 = gptDevice->create_buffer(ptDevice, &tReadbackBufferDesc, "readback buffer2");
            gptDevice->transfer_image_to_buffer(ptDevice, ptAppData->tOffscreenTexture[ptAppData->tGraphics.uCurrentFrameIndex], tReadbackBuffer);
            gptImage->write_hdr("offscreen.hdr", (int)ptAppData->tOffscreenTargetSize.x, (int)ptAppData->tOffscreenTargetSize.y, 4, (float*)ptGraphics->sbtBuffersCold[tReadbackBuffer.uIndex].tMemoryAllocation.pHostMapped);

            uint32_t* ptComputeInfoBuffer = (uint32_t*)ptGraphics->sbtBuffersCold[ptAppData->tComputeUniformBuffer.uIndex].tMemoryAllocation.pHostMapped;
            *ptComputeInfoBuffer = (uint32_t)ptAppData->tOffscreenTargetSize.x;
            plBufferHandle atBindGroup0_buffers1[3] = {ptAppData->tComputeUniformBuffer, tReadbackBuffer, tReadbackBuffer2};
            size_t szBufferRangeSize[3] = {sizeof(uint32_t), (uint32_t)ptAppData->tOffscreenTargetSize.x * (uint32_t)ptAppData->tOffscreenTargetSize.y*4* sizeof(float), (uint32_t)ptAppData->tOffscreenTargetSize.x * (uint32_t)ptAppData->tOffscreenTargetSize.y*4* sizeof(float)};
            gptDevice->update_bind_group(ptDevice, &ptAppData->tBindGroupCompute, 3, atBindGroup0_buffers1, szBufferRangeSize, 0, NULL);

            plDispatch tDispach = {
                .uBindGroup0 = ptAppData->tBindGroupCompute.uIndex,
                .uGroupCountX = (uint32_t)ptAppData->tOffscreenTargetSize.x,
                .uGroupCountY = (uint32_t)ptAppData->tOffscreenTargetSize.y,
                .uGroupCountZ = 1,
                .uThreadPerGroupX = 1,
                .uThreadPerGroupY = 1,
                .uThreadPerGroupZ = 1,
                .uShaderVariant = ptAppData->tComputeShader0.uIndex
            };
            gptGfx->dispatch(ptGraphics, 1, &tDispach);

            gptImage->write_hdr("offscreen2.hdr", (int)ptAppData->tOffscreenTargetSize.x, (int)ptAppData->tOffscreenTargetSize.y, 4, (float*)ptGraphics->sbtBuffersCold[tReadbackBuffer2.uIndex].tMemoryAllocation.pHostMapped);

            gptDevice->queue_buffer_for_deletion(ptDevice, tReadbackBuffer);
            gptDevice->queue_buffer_for_deletion(ptDevice, tReadbackBuffer2);
        }
        
        pl_image(ptAppData->ptOffscreenTextureID[ptAppData->tGraphics.uCurrentFrameIndex], (plVec2){500.0f, 500.0f});
        pl_end_window();
    }

    pl_set_next_window_pos((plVec2){0, 0}, PL_UI_COND_ONCE);

    if(pl_begin_window("Pilot Light", NULL, false))
    {

        const float pfRatios[] = {1.0f};
        pl_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
        if(pl_collapsing_header("Information"))
        {
            pl_text("Pilot Light %s", PILOTLIGHT_VERSION);
            pl_text("Pilot Light UI %s", PL_UI_VERSION);
            pl_text("Pilot Light DS %s", PL_DS_VERSION);
            if(pl_checkbox("VSync", &ptAppData->tGraphics.tSwapchain.bVSync))
                ptAppData->bReloadSwapchain = true;
            pl_end_collapsing_header();
        }
        if(pl_collapsing_header("Tools"))
        {
            pl_checkbox("Device Memory Analyzer", &ptAppData->tDebugInfo.bShowDeviceMemoryAnalyzer);
            pl_checkbox("Memory Allocations", &ptAppData->tDebugInfo.bShowMemoryAllocations);
            pl_checkbox("Profiling", &ptAppData->tDebugInfo.bShowProfiling);
            pl_checkbox("Statistics", &ptAppData->tDebugInfo.bShowStats);
            pl_checkbox("Logging", &ptAppData->tDebugInfo.bShowLogging);
            pl_checkbox("Entities", &ptAppData->bShowEntityWindow);
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

    if(ptAppData->bShowEntityWindow)
        pl_show_ecs_window(gptEcs, &ptAppData->tComponentLibrary, &ptAppData->bShowEntityWindow);

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
        .uBindGroup0          = tGlobalBindGroup.uIndex,
        .uBindGroup1          = ptAppData->tBindGroup1_2.uIndex,
        .uBindGroup2          = ptAppData->tBindGroup2.uIndex,
        .uDynamicBufferOffset = tDynamicBinding2.uByteOffset
    });

    // object 1
    int aiConstantData0[3] = { (int)PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0 | PL_MESH_FORMAT_FLAG_HAS_COLOR_0, 0, PL_SHADER_TEXTURE_FLAG_BINDING_0 | PL_SHADER_TEXTURE_FLAG_BINDING_1};
    int iFlagCopy = (int)PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0 | PL_MESH_FORMAT_FLAG_HAS_COLOR_0;
    while(iFlagCopy)
    {
        aiConstantData0[1] += iFlagCopy & 1;
        iFlagCopy >>= 1;
    }
    plShaderVariant tVariant = {
        .pTempConstantData = aiConstantData0,
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 1,
            .ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_HAS_POSITION,
            .ulBlendMode          = PL_BLEND_MODE_ALPHA,
            .ulDepthMode          = PL_COMPARE_MODE_LESS_OR_EQUAL,
            .ulCullMode           = PL_CULL_MODE_CULL_BACK,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        }
    };
    gptStream->draw(&ptAppData->tDrawStream, (plDraw)
    {
        .uShaderVariant       = ptAppData->tShader0.uIndex,
        .uDynamicBuffer       = tDynamicBinding0.uBufferHandle,
        .uVertexBuffer        = ptAppData->tVertexBuffer.uIndex,
        .uIndexBuffer         = ptAppData->tIndexBuffer.uIndex,
        .uTriangleCount       = 2,
        .uBindGroup0          = tGlobalBindGroup.uIndex,
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
        .uBindGroup0          = tGlobalBindGroup.uIndex,
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
        recreate_main_render_pass(ptAppData);
    pl_end_profile_sample();
    pl_end_profile_frame();
}

//-----------------------------------------------------------------------------
// [SECTION] helper implementations
//-----------------------------------------------------------------------------

void
create_offscreen_render_pass(plAppData* ptAppData)
{
    plIO* ptIO = pl_get_io();
    plDevice* ptDevice = &ptAppData->tGraphics.tDevice;

    const plRenderPassDescription tOffscreenRenderPassDesc = {
        .tLayout = ptAppData->tOffscreenPassLayout,
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
        },
        .tDimensions = {.x = ptAppData->tOffscreenTargetSize.x, .y = ptAppData->tOffscreenTargetSize.y},
        .uAttachmentCount = 2,
        .uAttachmentSets = 2,
    };

    const plRenderPassAttachments atAttachmentSets[2] = {
        {
            .atViewAttachments = {
                ptAppData->tOffscreenTextureView[0],
                ptAppData->tOffscreenDepthTextureView,
            }
        },
        {
            .atViewAttachments = {
                ptAppData->tOffscreenTextureView[1],
                ptAppData->tOffscreenDepthTextureView,
            }
        },
    };
    ptAppData->tOffscreenRenderPass = gptDevice->create_render_pass(ptDevice, &tOffscreenRenderPassDesc, atAttachmentSets);
}

void
create_main_render_pass(plAppData* ptAppData)
{
    plIO* ptIO = pl_get_io();
    plDevice* ptDevice = &ptAppData->tGraphics.tDevice;

    // render passes
    const plRenderPassDescription tMainRenderPassDesc = {
        .tLayout = ptAppData->tMainRenderPassLayout,
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
            }
        },
        .tDimensions = {.x = ptIO->afMainViewportSize[0], .y = ptIO->afMainViewportSize[1]},
        .uAttachmentCount = 3,
        .uAttachmentSets = ptAppData->tGraphics.tSwapchain.uImageCount,
    };

    plRenderPassAttachments atAttachmentSets[16] = {0};

    for(uint32_t i = 0; i < ptAppData->tGraphics.tSwapchain.uImageCount; i++)
    {
        atAttachmentSets[i].atViewAttachments[0] = ptAppData->tGraphics.tSwapchain.tColorTextureView;
        atAttachmentSets[i].atViewAttachments[1] = ptAppData->tGraphics.tSwapchain.tDepthTextureView;
        atAttachmentSets[i].atViewAttachments[2] = ptAppData->tGraphics.tSwapchain.sbtSwapchainTextureViews[i];
    }
    
    ptAppData->tMainRenderPass = gptDevice->create_render_pass(ptDevice, &tMainRenderPassDesc, atAttachmentSets);
}

void
recreate_main_render_pass(plAppData* ptAppData)
{
    plIO* ptIO = pl_get_io();
    plDevice* ptDevice = &ptAppData->tGraphics.tDevice;

    plRenderPassAttachments atAttachmentSets[16] = {0};

    for(uint32_t i = 0; i < ptAppData->tGraphics.tSwapchain.uImageCount; i++)
    {
        atAttachmentSets[i].atViewAttachments[0] = ptAppData->tGraphics.tSwapchain.tColorTextureView;
        atAttachmentSets[i].atViewAttachments[1] = ptAppData->tGraphics.tSwapchain.tDepthTextureView;
        atAttachmentSets[i].atViewAttachments[2] = ptAppData->tGraphics.tSwapchain.sbtSwapchainTextureViews[i];
    }
    plVec2 tNewDimenstions = {ptIO->afMainViewportSize[0], ptIO->afMainViewportSize[1]};
    gptDevice->update_render_pass_attachments(ptDevice, ptAppData->tMainRenderPass, tNewDimenstions, atAttachmentSets);
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

    // tComputeUniformBuffer
    const plBufferDescription tComputeBufferDesc = {
        .tMemory              = PL_MEMORY_GPU_CPU,
        .tUsage               = PL_BUFFER_USAGE_UNIFORM,
        .uByteSize            = sizeof(uint32_t),
        .uInitialDataByteSize = 0,
        .puInitialData        = NULL
    };
    ptAppData->tComputeUniformBuffer = gptDevice->create_buffer(ptDevice, &tComputeBufferDesc, "compute uniform buffer");

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
    // pl_sb_free(ptAppData->sbtVertexDataBuffer);

    const plBufferDescription atGlobalBuffersDesc = {
        .tMemory              = PL_MEMORY_GPU_CPU,
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
            .tDimensions = {ptAppData->tOffscreenTargetSize.x, ptAppData->tOffscreenTargetSize.y, 1},
            .tFormat = PL_FORMAT_R32G32B32A32_FLOAT,
            .uLayers = 1,
            .uMips = 1,
            .tType = PL_TEXTURE_TYPE_2D,
            .tUsage = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
            .tSamples = PL_SAMPLE_COUNT_1
        };
        ptAppData->tOffscreenTexture[i] = gptDevice->create_texture(ptDevice, tTextureDesc, 0, NULL, "offscreen texture");

        plTextureViewDesc tTextureViewDesc = {
            .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
            .uBaseLayer  = 0,
            .uBaseMip    = 0,
            .uLayerCount = 1
        };
        plSampler tSampler = {
            .tFilter = PL_FILTER_LINEAR,
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
            .tDimensions = {ptAppData->tOffscreenTargetSize.x, ptAppData->tOffscreenTargetSize.y, 1},
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

    plBindGroupLayout tComputeBindGroupLayout0 = {
        .uBufferCount  = 3,
        .aBuffers = {
            {
                .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                .uSlot = 0,
                .tStages = PL_STAGE_COMPUTE
            },
            {
                .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                .uSlot = 1,
                .tStages = PL_STAGE_COMPUTE
            },
            {
                .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                .uSlot = 2,
                .tStages = PL_STAGE_COMPUTE
            },
        }
    };
    ptAppData->tBindGroupCompute = gptDevice->create_bind_group(ptDevice, &tComputeBindGroupLayout0);

    plTextureViewHandle atTextureViews[3] = {ptAppData->tTextureView, ptAppData->tTextureView, ptAppData->tTextureView};

    plBindGroupLayout tBindGroupLayout1_0 = {
        .uTextureCount  = 3,
        .aTextures = {
            {.uSlot = 0, .tStages = PL_STAGE_PIXEL | PL_STAGE_VERTEX},
            {.uSlot = 1, .tStages = PL_STAGE_PIXEL | PL_STAGE_VERTEX},
            {.uSlot = 2, .tStages = PL_STAGE_PIXEL | PL_STAGE_VERTEX},
        }
    };
    ptAppData->tBindGroup1_0 = gptDevice->create_bind_group(ptDevice, &tBindGroupLayout1_0);
    gptDevice->update_bind_group(ptDevice, &ptAppData->tBindGroup1_0, 0, NULL, NULL, 3, atTextureViews);

    plBindGroupLayout tBindGroupLayout1_1 = {
        .uTextureCount  = 3,
        .aTextures = {
            {.uSlot = 0, .tStages = PL_STAGE_PIXEL | PL_STAGE_VERTEX},
            {.uSlot = 1, .tStages = PL_STAGE_PIXEL | PL_STAGE_VERTEX},
            {.uSlot = 2, .tStages = PL_STAGE_PIXEL | PL_STAGE_VERTEX},
        }
    };
    ptAppData->tBindGroup1_1 = gptDevice->create_bind_group(ptDevice, &tBindGroupLayout1_1);
    
    gptDevice->update_bind_group(ptDevice, &ptAppData->tBindGroup1_1, 0, NULL, NULL, 3, atTextureViews);

    plBindGroupLayout tBindGroupLayout1_2 = {
        .uTextureCount  = 1,
        .aTextures = {
            {.uSlot = 0, .tStages = PL_STAGE_PIXEL | PL_STAGE_VERTEX}
        }
    };
    ptAppData->tBindGroup1_2 = gptDevice->create_bind_group(ptDevice, &tBindGroupLayout1_2);
    gptDevice->update_bind_group(ptDevice, &ptAppData->tBindGroup1_2, 0, NULL, NULL, 1, &ptAppData->tSkyboxTextureView);

    plBindGroupLayout tBindGroupLayout2_0 = {
        .uBufferCount  = 1,
        .aBuffers = {
            {
                .tType   = PL_BUFFER_BINDING_TYPE_UNIFORM,
                .uSlot   = 0,
                .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
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

    plComputeShaderDescription tComputeShaderDescription0 = {

#ifdef PL_METAL_BACKEND
        .pcShader = "../shaders/metal/compute.metal",
#else // VULKAN
        .pcShader = "compute.comp.spv",
#endif
        .uConstantCount = 1,
        .atConstants = {
            {
                .uID = 0,
                .uOffset = 0,
                .tType = PL_DATA_TYPE_BOOL
            },
        },
        .tBindGroupLayout = {
            .uBufferCount = 3,
            .aBuffers = {
                {
                    .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                    .uSlot = 0,
                    .tStages = PL_STAGE_COMPUTE
                },
                {
                    .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                    .uSlot = 1,
                    .tStages = PL_STAGE_COMPUTE
                },
                {
                    .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                    .uSlot = 2,
                    .tStages = PL_STAGE_COMPUTE
                }
            },
        }
    };
    int iSwitchChannels = 1;
    tComputeShaderDescription0.pTempConstantData = &iSwitchChannels;
    ptAppData->tComputeShader0 = gptDevice->create_compute_shader(ptDevice, &tComputeShaderDescription0);

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
            .ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_HAS_POSITION,
            .ulBlendMode          = PL_BLEND_MODE_ALPHA,
            .ulDepthMode          = PL_COMPARE_MODE_LESS_OR_EQUAL,
            .ulCullMode           = PL_CULL_MODE_NONE,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .uConstantCount = 3,
        .atConstants = {
            {
                .uID = 0,
                .uOffset = 0,
                .tType = PL_DATA_TYPE_INT
            },
            {
                .uID = 1,
                .uOffset = sizeof(int),
                .tType = PL_DATA_TYPE_INT
            },
            {
                .uID = 2,
                .uOffset = sizeof(int) * 2,
                .tType = PL_DATA_TYPE_INT
            }
        },
        .tRenderPassLayout = ptAppData->tMainRenderPassLayout,
        .uBindGroupLayoutCount = 3,
        .atBindGroupLayouts = {
            {
                .uBufferCount  = 2,
                .aBuffers = {
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                        .uSlot = 0,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    },
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                        .uSlot = 1,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    }
                },
            },
            {
                .uTextureCount = 3,
                .aTextures = {
                    {
                        .uSlot = 0,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    },
                    {
                        .uSlot = 1,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    },
                    {
                        .uSlot = 2,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    }
                 },
            },
            {
                .uBufferCount  = 1,
                .aBuffers = {
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                        .uSlot = 0,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    },
                },
            }
        }
    };
    int aiConstantData0[3] = { (int)PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0 | PL_MESH_FORMAT_FLAG_HAS_COLOR_0, 0, PL_SHADER_TEXTURE_FLAG_BINDING_0 | PL_SHADER_TEXTURE_FLAG_BINDING_1};
    int iFlagCopy = (int)PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0 | PL_MESH_FORMAT_FLAG_HAS_COLOR_0;
    while(iFlagCopy)
    {
        aiConstantData0[1] += iFlagCopy & 1;
        iFlagCopy >>= 1;
    }
    int aiConstantData1[3] = { (int)PL_MESH_FORMAT_FLAG_HAS_COLOR_0, 0, 0};
    iFlagCopy = (int)PL_MESH_FORMAT_FLAG_HAS_COLOR_0;
    while(iFlagCopy)
    {
        aiConstantData1[1] += iFlagCopy & 1;
        iFlagCopy >>= 1;
    }
    tShaderDescription0.pTempConstantData = aiConstantData0;
    const plShaderVariant tVariant = {.tGraphicsState = tShaderDescription0.tGraphicsState, .pTempConstantData = aiConstantData1 };
    ptAppData->tShader0 = gptDevice->create_shader(ptDevice, &tShaderDescription0);
    ptAppData->tShader1 = gptDevice->get_shader_variant(ptDevice, ptAppData->tShader0, &tVariant);
    tShaderDescription0.tRenderPassLayout = ptAppData->tOffscreenPassLayout;
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
            .ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_HAS_POSITION,
            .ulBlendMode          = PL_BLEND_MODE_ALPHA,
            .ulDepthMode          = PL_COMPARE_MODE_LESS_OR_EQUAL,
            .ulCullMode           = PL_CULL_MODE_NONE,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .uConstantCount = 3,
        .atConstants = {
            {
                .uID = 0,
                .uOffset = 0,
                .tType = PL_DATA_TYPE_INT
            },
            {
                .uID = 1,
                .uOffset = sizeof(int),
                .tType = PL_DATA_TYPE_INT
            },
            {
                .uID = 2,
                .uOffset = sizeof(int) * 2,
                .tType = PL_DATA_TYPE_INT
            }
        },
        .tRenderPassLayout = ptAppData->tMainRenderPassLayout,
        .uBindGroupLayoutCount = 3,
        .atBindGroupLayouts = {
            {
                .uBufferCount  = 2,
                .aBuffers = {
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                        .uSlot = 0,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    },
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                        .uSlot = 1,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    },
                },
            },
            {
                .uTextureCount = 3,
                .aTextures = {
                    {
                        .uSlot = 0,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    },
                    {
                        .uSlot = 1,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    },
                    {
                        .uSlot = 2,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    }
                 },
            },
            {
                .uBufferCount  = 1,
                .aBuffers = {
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                        .uSlot = 0,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    },
                },
            }
        }
    };

    tShaderDescription1.pTempConstantData = aiConstantData1;
    
    // ptAppData->tShader1 = gptDevice->create_shader(ptDevice, &tShaderDescription1);
    tShaderDescription1.tRenderPassLayout = ptAppData->tOffscreenPassLayout;
    ptAppData->tOffscreenShader1 = gptDevice->create_shader(ptDevice, &tShaderDescription1);

    plShaderDescription tShaderDescription2 = {
#ifdef PL_METAL_BACKEND
        .pcVertexShader = "../shaders/metal/skybox.metal",
        .pcPixelShader = "../shaders/metal/skybox.metal",
#else // linux
        .pcVertexShader = "skybox.vert.spv",
        .pcPixelShader = "skybox.frag.spv",
        .pcVertexShaderEntryFunc = "main",
        .pcPixelShaderEntryFunc = "main",
#endif
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 0,
            .ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_HAS_POSITION,
            .ulBlendMode          = PL_BLEND_MODE_ALPHA,
            .ulDepthMode          = PL_COMPARE_MODE_LESS_OR_EQUAL,
            .ulCullMode           = PL_CULL_MODE_NONE,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .tRenderPassLayout = ptAppData->tMainRenderPassLayout,
        .uBindGroupLayoutCount = 3,
        .atBindGroupLayouts = {
            {
                .uBufferCount  = 2,
                .aBuffers = {
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                        .uSlot = 0,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    },
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                        .uSlot = 1,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    },
                },
            },
            {
                .uTextureCount = 1,
                .aTextures = {
                    {
                        .uSlot = 0,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    }
                 },
            },
            {
                .uBufferCount  = 1,
                .aBuffers = {
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                        .uSlot = 0,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    },
                },
            }
        }
    };
    ptAppData->tShader2 = gptDevice->create_shader(ptDevice, &tShaderDescription2);
    tShaderDescription2.tRenderPassLayout = ptAppData->tOffscreenPassLayout;
    ptAppData->tOffscreenShader2 = gptDevice->create_shader(ptDevice, &tShaderDescription2);
}
