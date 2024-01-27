/*
   pl_ref_renderer_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] global data & apis
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <float.h> // FLT_MAX
#include "pilotlight.h"
#include "pl_ref_renderer_ext.h"
#include "pl_os.h"
#include "pl_profile.h"
#include "pl_log.h"
#include "pl_ds.h"
#include "pl_string.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"
#include "pl_ui.h"
#include "pl_stl.h"

// extensions
#include "pl_graphics_ext.h"
#include "pl_ecs_ext.h"
#include "pl_resource_ext.h"
#include "pl_image_ext.h"
#include "pl_stats_ext.h"

// misc
#include "cgltf.h"

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plDrawable
{
    plEntity tEntity;
    plBindGroupHandle tMaterialBindGroup;
    uint32_t uDataOffset;
    uint32_t uVertexOffset;
    uint32_t uVertexCount;
    uint32_t uIndexOffset;
    uint32_t uIndexCount;
    uint32_t uMaterialIndex;
    uint32_t uShader;
} plDrawable;

typedef struct _plMaterial
{
    plVec4 tColor;
} plMaterial;

typedef struct _BindGroup_0
{
    plVec4 tCameraPos;
    plMat4 tCameraView;
    plMat4 tCameraProjection;   
    plMat4 tCameraViewProjection;
} BindGroup_0;

typedef struct _DynamicData
{
    int    iDataOffset;
    int    iVertexOffset;
    int    iMaterialOffset;
    int    iPadding[1];
    plMat4 tModel;
} DynamicData;

typedef struct _plRefRendererData
{
    uint32_t uLogChannel;

    plGraphics tGraphics;

    // misc textures
    plTextureHandle     tDummyTexture;
    plTextureViewHandle tDummyTextureView;

    // CPU buffers
    plVec3*     sbtVertexPosBuffer;
    plVec4*     sbtVertexDataBuffer;
    uint32_t*   sbuIndexBuffer;
    plMaterial* sbtMaterialBuffer;

    // GPU buffers
    plBufferHandle tVertexBuffer;
    plBufferHandle tIndexBuffer;
    plBufferHandle tStorageBuffer;
    plBufferHandle tMaterialDataBuffer;
    plBufferHandle tUnusedShaderBuffer;
    plBufferHandle atGlobalBuffers[2];

    // shaders
    plShaderHandle tShader;
    plShaderHandle tSkyboxShader;

    // compute shaders
    plComputeShaderHandle tPanoramaShader;

    // misc
    plDrawable* sbtDrawables;
    plDrawable* sbtVisibleDrawables;

    // skybox
    plDrawable          tSkyboxDrawable;
    plTextureHandle     tSkyboxTexture;
    plTextureViewHandle tSkyboxTextureView;
    plBindGroupHandle   tSkyboxBindGroup;

    // drawing api
    plFontAtlas tFontAtlas;

    // render pass
    plRenderPassLayoutHandle tMainRenderPassLayout;
    plRenderPassHandle       tMainRenderPass;

    // draw stream
    plDrawStream tDrawStream;

    // ecs
    plComponentLibrary tComponentLibrary;

    // gltf data
    plHashMap tMaterialHashMap;
    plEntity* sbtMaterialEntities;
    plBindGroupHandle* sbtMaterialBindGroups;

} plRefRendererData;

//-----------------------------------------------------------------------------
// [SECTION] global data & apis
//-----------------------------------------------------------------------------

// context data
static plRefRendererData* gptData = NULL;

// apis
static const plDataRegistryApiI* gptDataRegistry = NULL;
static const plResourceI*        gptResource = NULL;
static const plEcsI*             gptECS      = NULL;
static const plFileApiI*         gptFile     = NULL;
static const plDeviceI*          gptDevice   = NULL;
static const plGraphicsI*        gptGfx      = NULL;
static const plCameraI*          gptCamera   = NULL;
static const plDrawStreamI*      gptStream   = NULL;
static const plImageI*           gptImage    = NULL;
static const plStatsApiI*        gptStats    = NULL;

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// main
static void pl_refr_initialize(void);
static void pl_refr_resize(void);
static void pl_refr_cleanup(void);

// per frame
static void pl_refr_run_ecs(void);
static void pl_refr_submit_ui(void);
static void pl_refr_cull_draw_stream(plCameraComponent* ptCamera);
static void pl_refr_submit_draw_stream(plCameraComponent* ptCamera);
static void pl_refr_draw_bound_boxes(plDrawList3D* ptDrawlist);

// loading
static void pl_refr_load_skybox_from_panorama(const char* pcModelPath, int iResolution);
static void pl_refr_load_stl(const char* pcModelPath, plVec4 tColor, const plMat4* ptTransform);
static void pl_refr_load_gltf(const char* pcPath);
static void pl_refr_finalize_scene(void);

// misc
static plRenderPassHandle  pl_refr_get_main_render_pass (void);
static plComponentLibrary* pl_refr_get_component_library(void);
static plGraphics*         pl_refr_get_graphics         (void);

// internal
static void pl__load_gltf_texture(plTextureSlot tSlot, const cgltf_texture_view* ptTexture, const char* pcDirectory, const cgltf_material* ptMaterial, plMaterialComponent* ptMaterialOut);
static void pl__refr_load_material(const char* pcDirectory, plMaterialComponent* ptMaterial, const cgltf_material* ptGltfMaterial);
static void pl__refr_load_attributes(plMeshComponent* ptMesh, const cgltf_primitive* ptPrimitive);
static void pl__refr_load_gltf_object(const char* pcDirectory, plEntity tParentEntity, const cgltf_node* ptNode);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

const plRefRendererI*
pl_load_ref_renderer_api(void)
{
    static const plRefRendererI tApi = {
        .initialize                = pl_refr_initialize,
        .cleanup                   = pl_refr_cleanup,
        .resize                    = pl_refr_resize,
        .run_ecs                   = pl_refr_run_ecs,
        .submit_ui                 = pl_refr_submit_ui,
        .get_component_library     = pl_refr_get_component_library,
        .get_main_render_pass      = pl_refr_get_main_render_pass,
        .get_graphics              = pl_refr_get_graphics,
        .load_skybox_from_panorama = pl_refr_load_skybox_from_panorama,
        .load_stl                  = pl_refr_load_stl,
        .draw_bound_boxes          = pl_refr_draw_bound_boxes,
        .load_gltf                 = pl_refr_load_gltf,
        .cull_draw_stream          = pl_refr_cull_draw_stream,
        .submit_draw_stream        = pl_refr_submit_draw_stream,
        .finalize_scene            = pl_refr_finalize_scene,
    };
    return &tApi;
}

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

static void
pl_refr_initialize(void)
{
    // buffer default values
    gptData->tVertexBuffer         = (plBufferHandle){UINT32_MAX, UINT32_MAX};
    gptData->tIndexBuffer          = (plBufferHandle){UINT32_MAX, UINT32_MAX};
    gptData->tStorageBuffer        = (plBufferHandle){UINT32_MAX, UINT32_MAX};
    gptData->tMaterialDataBuffer   = (plBufferHandle){UINT32_MAX, UINT32_MAX};
    gptData->tUnusedShaderBuffer   = (plBufferHandle){UINT32_MAX, UINT32_MAX};
    gptData->atGlobalBuffers[0]    = (plBufferHandle){UINT32_MAX, UINT32_MAX};
    gptData->atGlobalBuffers[1]    = (plBufferHandle){UINT32_MAX, UINT32_MAX};

    // shader default values
    gptData->tShader       = (plShaderHandle){UINT32_MAX, UINT32_MAX};
    gptData->tSkyboxShader = (plShaderHandle){UINT32_MAX, UINT32_MAX};

    // compute shader default values
    gptData->tPanoramaShader = (plComputeShaderHandle){UINT32_MAX, UINT32_MAX};

    // misc textures
    gptData->tDummyTexture     = (plTextureHandle){UINT32_MAX, UINT32_MAX};
    gptData->tDummyTextureView = (plTextureViewHandle){UINT32_MAX, UINT32_MAX};

    // skybox resources default values
    gptData->tSkyboxTexture     = (plTextureHandle){UINT32_MAX, UINT32_MAX};
    gptData->tSkyboxTextureView = (plTextureViewHandle){UINT32_MAX, UINT32_MAX};
    gptData->tSkyboxBindGroup   = (plBindGroupHandle){UINT32_MAX, UINT32_MAX};

    // for convience
    plGraphics* ptGraphics = &gptData->tGraphics;

    // initialize ecs
    gptECS->init_component_library(&gptData->tComponentLibrary);

    // initialize graphics
    ptGraphics->bValidationActive = true;
    gptGfx->initialize(ptGraphics);
    gptDataRegistry->set_data("device", &ptGraphics->tDevice); // used by debug extension

    // create main render pass layout
    const plRenderPassLayoutDescription tMainRenderPassLayoutDesc = {
        .tDepthTarget = {.tFormat = ptGraphics->tSwapchain.tDepthFormat, .tSampleCount = ptGraphics->tSwapchain.tMsaaSamples},
        .tResolveTarget = { .tFormat = ptGraphics->tSwapchain.tFormat, .tSampleCount = PL_SAMPLE_COUNT_1 },
        .atRenderTargets = {
            { .tFormat = ptGraphics->tSwapchain.tFormat, .tSampleCount = ptGraphics->tSwapchain.tMsaaSamples}
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
    gptData->tMainRenderPassLayout = gptDevice->create_render_pass_layout(&ptGraphics->tDevice, &tMainRenderPassLayoutDesc);

    // create main render pass
    plIO* ptIO = pl_get_io();
    const plRenderPassDescription tMainRenderPassDesc = {
        .tLayout = gptData->tMainRenderPassLayout,
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
        .uAttachmentSets = ptGraphics->tSwapchain.uImageCount,
    };

    plRenderPassAttachments atAttachmentSets[16] = {0};

    for(uint32_t i = 0; i < ptGraphics->tSwapchain.uImageCount; i++)
    {
        atAttachmentSets[i].atViewAttachments[0] = ptGraphics->tSwapchain.tColorTextureView;
        atAttachmentSets[i].atViewAttachments[1] = ptGraphics->tSwapchain.tDepthTextureView;
        atAttachmentSets[i].atViewAttachments[2] = ptGraphics->tSwapchain.sbtSwapchainTextureViews[i];
    }
    
    gptData->tMainRenderPass = gptDevice->create_render_pass(&ptGraphics->tDevice, &tMainRenderPassDesc, atAttachmentSets);

    // setup ui
    pl_add_default_font(&gptData->tFontAtlas);
    pl_build_font_atlas(&gptData->tFontAtlas);
    gptGfx->setup_ui(ptGraphics, gptData->tMainRenderPass);
    gptGfx->create_font_atlas(&gptData->tFontAtlas);
    pl_set_default_font(&gptData->tFontAtlas.sbtFonts[0]);

   plShaderDescription tSkyboxShaderDesc = {
#ifdef PL_METAL_BACKEND
        .pcVertexShader = "../shaders/metal/skybox.metal",
        .pcPixelShader = "../shaders/metal/skybox.metal",
        .pcVertexShaderEntryFunc = "vertex_main",
        .pcPixelShaderEntryFunc = "fragment_main",
#else
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
        .tRenderPassLayout = gptData->tMainRenderPassLayout,
        .uBindGroupLayoutCount = 3,
        .atBindGroupLayouts = {
            {
                .uBufferCount  = 3,
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
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                        .uSlot = 2,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    },
                }
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
    gptData->tSkyboxShader = gptDevice->create_shader(&ptGraphics->tDevice, &tSkyboxShaderDesc);

    // create dummy texture & texture view
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
        .uMips = 1,
        .tType = PL_TEXTURE_TYPE_2D,
        .tUsage = PL_TEXTURE_USAGE_SAMPLED,
        .tSamples = PL_SAMPLE_COUNT_1
    };

    const plBufferDescription tStagingBufferDesc = {
        .tMemory              = PL_MEMORY_GPU_CPU,
        .tUsage               = PL_BUFFER_USAGE_UNSPECIFIED,
        .uByteSize            = 268435456
    };
    plBufferHandle tStagingBufferHandle = gptDevice->create_buffer(&ptGraphics->tDevice, &tStagingBufferDesc, "staging buffer");
    plBuffer* ptStagingBuffer = gptDevice->get_buffer(&ptGraphics->tDevice, tStagingBufferHandle);
    memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, image, sizeof(float) * 4 * 4);

    gptData->tDummyTexture = gptDevice->create_texture(&ptGraphics->tDevice, tTextureDesc, "dummy texture");

    plBufferImageCopy tBufferImageCopy = {
        .tImageExtent = {2, 2, 1},
        .uLayerCount = 1
    };
    gptDevice->copy_buffer_to_texture(&ptGraphics->tDevice, tStagingBufferHandle, gptData->tDummyTexture, 1, &tBufferImageCopy);

    gptDevice->destroy_buffer(&ptGraphics->tDevice, tStagingBufferHandle);

    plTextureViewDesc tTextureViewDesc = {
        .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
        .uBaseLayer  = 0,
        .uBaseMip    = 0,
        .uLayerCount = 1
    };
    plSampler tSampler = {
        .tFilter = PL_FILTER_NEAREST,
        .fMinMip = 0.0f,
        .fMaxMip = 1.0f,
        .tVerticalWrap = PL_WRAP_MODE_CLAMP,
        .tHorizontalWrap = PL_WRAP_MODE_CLAMP
    };
    gptData->tDummyTextureView = gptDevice->create_texture_view(&ptGraphics->tDevice, &tTextureViewDesc, &tSampler, gptData->tDummyTexture, "dummy texture view");
}

static void
pl_refr_resize(void)
{
    // for convience
    plGraphics* ptGraphics = &gptData->tGraphics;
    plDevice* ptDevice = &ptGraphics->tDevice;

    plIO* ptIO = pl_get_io();

    gptGfx->resize(ptGraphics);

    // recreate main render pass
    plRenderPassAttachments atAttachmentSets[16] = {0};

    for(uint32_t i = 0; i < ptGraphics->tSwapchain.uImageCount; i++)
    {
        atAttachmentSets[i].atViewAttachments[0] = ptGraphics->tSwapchain.tColorTextureView;
        atAttachmentSets[i].atViewAttachments[1] = ptGraphics->tSwapchain.tDepthTextureView;
        atAttachmentSets[i].atViewAttachments[2] = ptGraphics->tSwapchain.sbtSwapchainTextureViews[i];
    }
    plVec2 tNewDimensions = {ptIO->afMainViewportSize[0], ptIO->afMainViewportSize[1]};
    gptDevice->update_render_pass_attachments(ptDevice, gptData->tMainRenderPass, tNewDimensions, atAttachmentSets);
}

static void
pl_refr_cleanup(void)
{
    gptECS->cleanup_component_library(&gptData->tComponentLibrary);
    gptStream->cleanup(&gptData->tDrawStream);

    gptGfx->destroy_font_atlas(&gptData->tFontAtlas);
    pl_cleanup_font_atlas(&gptData->tFontAtlas);
    gptGfx->cleanup(&gptData->tGraphics);
}

static void
pl_refr_submit_ui(void)
{
    pl_begin_profile_sample(__FUNCTION__);


    // render ui
    pl_render();

    // submit draw lists
    pl_begin_profile_sample("Submit draw lists");
    gptGfx->draw_lists(&gptData->tGraphics, 1, pl_get_draw_list(NULL), gptData->tMainRenderPass);
    gptGfx->draw_lists(&gptData->tGraphics, 1, pl_get_debug_draw_list(NULL), gptData->tMainRenderPass);
    pl_end_profile_sample();
    pl_end_profile_sample();
}

static plRenderPassHandle
pl_refr_get_main_render_pass(void)
{
    return gptData->tMainRenderPass;
}

static plComponentLibrary*
pl_refr_get_component_library(void)
{
    return &gptData->tComponentLibrary;
}

static plGraphics*
pl_refr_get_graphics(void)
{
    return &gptData->tGraphics;
}

static void
pl_refr_load_skybox_from_panorama(const char* pcPath, int iResolution)
{
    plGraphics* ptGraphics = &gptData->tGraphics;
    plDevice* ptDevice = &ptGraphics->tDevice;

    int iPanoramaWidth = 0;
    int iPanoramaHeight = 0;
    int iUnused = 0;
    float* pfPanoramaData = gptImage->loadf(pcPath, &iPanoramaWidth, &iPanoramaHeight, &iUnused, 4);
    PL_ASSERT(pfPanoramaData);

    plComputeShaderDescription tSkyboxComputeShaderDesc = {
#ifdef PL_METAL_BACKEND
        .pcShader = "panorama_to_cubemap.metal",
        .pcShaderEntryFunc = "kernel_main",
#else
        .pcShader = "panorama_to_cubemap.comp.spv",
        .pcShaderEntryFunc = "main",
#endif
        .uConstantCount = 3,
        .atConstants = {
            { .uID = 0, .uOffset = 0,               .tType = PL_DATA_TYPE_INT},
            { .uID = 1, .uOffset = sizeof(int),     .tType = PL_DATA_TYPE_INT},
            { .uID = 2, .uOffset = 2 * sizeof(int), .tType = PL_DATA_TYPE_INT}
        },
        .tBindGroupLayout = {
            .uBufferCount = 7,
            .aBuffers = {
                { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 0, .tStages = PL_STAGE_COMPUTE},
                { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 1, .tStages = PL_STAGE_COMPUTE},
                { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 2, .tStages = PL_STAGE_COMPUTE},
                { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 3, .tStages = PL_STAGE_COMPUTE},
                { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 4, .tStages = PL_STAGE_COMPUTE},
                { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 5, .tStages = PL_STAGE_COMPUTE},
                { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 6, .tStages = PL_STAGE_COMPUTE},
            },
        }
    };
    int aiSkyboxSpecializationData[] = {iResolution, iPanoramaWidth, iPanoramaHeight};
    tSkyboxComputeShaderDesc.pTempConstantData = aiSkyboxSpecializationData;
    gptData->tPanoramaShader = gptDevice->create_compute_shader(ptDevice, &tSkyboxComputeShaderDesc);

    plBufferHandle atComputeBuffers[7] = {0};
    const uint32_t uPanoramaSize = iPanoramaHeight * iPanoramaWidth * 4 * sizeof(float);
    const plBufferDescription tInputBufferDesc = {
        .tMemory              = PL_MEMORY_GPU_CPU,
        .tUsage               = PL_BUFFER_USAGE_STORAGE,
        .uByteSize            = uPanoramaSize
    };
    atComputeBuffers[0] = gptDevice->create_buffer(ptDevice, &tInputBufferDesc, "panorama input");
    plBuffer* ptComputeBuffer = gptDevice->get_buffer(ptDevice, atComputeBuffers[0]);
    memcpy(ptComputeBuffer->tMemoryAllocation.pHostMapped, pfPanoramaData, iPanoramaWidth * iPanoramaHeight * 4 * sizeof(float));

    const size_t uFaceSize = ((size_t)iResolution * (size_t)iResolution) * 4 * sizeof(float);
    const plBufferDescription tOutputBufferDesc = {
        .tMemory              = PL_MEMORY_GPU_CPU,
        .tUsage               = PL_BUFFER_USAGE_STORAGE,
        .uByteSize            = (uint32_t)uFaceSize
    };
    
    for(uint32_t i = 0; i < 6; i++)
        atComputeBuffers[i + 1] = gptDevice->create_buffer(ptDevice, &tOutputBufferDesc, "panorama output");

    plBindGroupLayout tComputeBindGroupLayout = {
        .uBufferCount = 7,
        .aBuffers = {
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 0, .tStages = PL_STAGE_COMPUTE},
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 1, .tStages = PL_STAGE_COMPUTE},
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 2, .tStages = PL_STAGE_COMPUTE},
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 3, .tStages = PL_STAGE_COMPUTE},
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 4, .tStages = PL_STAGE_COMPUTE},
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 5, .tStages = PL_STAGE_COMPUTE},
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 6, .tStages = PL_STAGE_COMPUTE},
        },
    };
    plBindGroupHandle tComputeBindGroup = gptDevice->get_temporary_bind_group(ptDevice, &tComputeBindGroupLayout);
    size_t szBufferRangeSize[] = {(size_t)uPanoramaSize, uFaceSize, uFaceSize, uFaceSize, uFaceSize, uFaceSize, uFaceSize};
    gptDevice->update_bind_group(ptDevice, &tComputeBindGroup, 7, atComputeBuffers, szBufferRangeSize, 0, NULL);

    plDispatch tDispach = {
        .uBindGroup0      = tComputeBindGroup.uIndex,
        .uGroupCountX     = (uint32_t)iResolution / 16,
        .uGroupCountY     = (uint32_t)iResolution / 16,
        .uGroupCountZ     = 2,
        .uThreadPerGroupX = 16,
        .uThreadPerGroupY = 16,
        .uThreadPerGroupZ = 3,
        .uShaderVariant   = gptData->tPanoramaShader.uIndex
    };
    gptGfx->dispatch(ptGraphics, 1, &tDispach);

    // get data
    char* pcResultData = PL_ALLOC(uFaceSize * 6);
    memset(pcResultData, 0, uFaceSize * 6);
    float* pfBlah0 = (float*)&pcResultData[0];
    float* pfBlah1 = (float*)&pcResultData[uFaceSize];
    float* pfBlah2 = (float*)&pcResultData[uFaceSize * 2];
    float* pfBlah3 = (float*)&pcResultData[uFaceSize * 3];
    float* pfBlah4 = (float*)&pcResultData[uFaceSize * 4];
    float* pfBlah5 = (float*)&pcResultData[uFaceSize * 5];

    for(uint32_t i = 0; i < 6; i++)
    {
        plBuffer* ptBuffer = gptDevice->get_buffer(ptDevice, atComputeBuffers[i + 1]);
        memcpy(&pcResultData[uFaceSize * i], ptBuffer->tMemoryAllocation.pHostMapped, uFaceSize);
    }

    const plBufferDescription tStagingBufferDesc = {
        .tMemory              = PL_MEMORY_GPU_CPU,
        .tUsage               = PL_BUFFER_USAGE_UNSPECIFIED,
        .uByteSize            = 268435456
    };
    plBufferHandle tStagingBufferHandle = gptDevice->create_buffer(&ptGraphics->tDevice, &tStagingBufferDesc, "staging buffer");
    plBuffer* ptStagingBuffer = gptDevice->get_buffer(&ptGraphics->tDevice, tStagingBufferHandle);
    memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, pcResultData, uFaceSize * 6);

    plTextureDesc tTextureDesc = {
        .tDimensions = {(float)iResolution, (float)iResolution, 1},
        .tFormat = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers = 6,
        .uMips = 1,
        .tType = PL_TEXTURE_TYPE_CUBE,
        .tUsage = PL_TEXTURE_USAGE_SAMPLED,
        .tSamples = PL_SAMPLE_COUNT_1
    };
    gptData->tSkyboxTexture = gptDevice->create_texture(ptDevice, tTextureDesc, "skybox texture");

    plBufferImageCopy atBufferImageCopy[6] = {0};
    for(uint32_t i = 0; i < 6; i++)
    {
        atBufferImageCopy[i].tImageExtent = (plExtent){iResolution, iResolution, 1};
        atBufferImageCopy[i].uLayerCount = 1;
        atBufferImageCopy[i].szBufferOffset = i * uFaceSize;
        atBufferImageCopy[i].uBaseArrayLayer = i;
    }
    gptDevice->copy_buffer_to_texture(&ptGraphics->tDevice, tStagingBufferHandle, gptData->tSkyboxTexture, 6, atBufferImageCopy);

    gptDevice->destroy_buffer(&ptGraphics->tDevice, tStagingBufferHandle);

    plTextureViewDesc tTextureViewDesc = {
        .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
        .uBaseLayer  = 0,
        .uBaseMip    = 0,
        .uLayerCount = 6
    };
    plSampler tSampler = {
        .tFilter = PL_FILTER_LINEAR,
        .fMinMip = 0.0f,
        .fMaxMip = PL_MAX_MIPS,
        .tVerticalWrap = PL_WRAP_MODE_WRAP,
        .tHorizontalWrap = PL_WRAP_MODE_WRAP
    };
    gptData->tSkyboxTextureView = gptDevice->create_texture_view(ptDevice, &tTextureViewDesc, &tSampler, gptData->tSkyboxTexture, "skybox texture view"); 

    // cleanup
    PL_FREE(pcResultData);
    
    for(uint32_t i = 0; i < 7; i++)
        gptDevice->destroy_buffer(ptDevice, atComputeBuffers[i]);

    gptImage->free(pfPanoramaData);

    plBindGroupLayout tSkyboxBindGroupLayout = {
        .uTextureCount  = 1,
        .aTextures = { {.uSlot = 0, .tStages = PL_STAGE_PIXEL | PL_STAGE_VERTEX}}
    };
    gptData->tSkyboxBindGroup = gptDevice->create_bind_group(ptDevice, &tSkyboxBindGroupLayout);
    gptDevice->update_bind_group(ptDevice, &gptData->tSkyboxBindGroup, 0, NULL, NULL, 1, &gptData->tSkyboxTextureView);

    const uint32_t uStartIndex     = pl_sb_size(gptData->sbtVertexPosBuffer);
    const uint32_t uIndexStart     = pl_sb_size(gptData->sbuIndexBuffer);
    const uint32_t uDataStartIndex = pl_sb_size(gptData->sbtVertexDataBuffer);

    const plDrawable tDrawable = {
        .uIndexCount   = 36,
        .uVertexCount  = 8,
        .uIndexOffset  = uIndexStart,
        .uVertexOffset = uStartIndex,
        .uDataOffset   = uDataStartIndex,
    };
    gptData->tSkyboxDrawable = tDrawable;

    // indices
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 0);
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 2);
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 1);
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 2);
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 3);
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 1);
    
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 1);
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 3);
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 5);
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 3);
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 7);
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 5);

    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 2);
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 6);
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 3);
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 3);
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 6);
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 7);
    
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 4);
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 5);
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 7);
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 4);
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 7);
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 6);
    
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 0);
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 4);
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 2);
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 2);
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 4);
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 6);
    
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 0);
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 1);
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 4);
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 1);
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 5);
    pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + 4);

    // vertices (position)
    const float fCubeSide = 0.5f;
    pl_sb_push(gptData->sbtVertexPosBuffer, ((plVec3){-fCubeSide, -fCubeSide, -fCubeSide}));
    pl_sb_push(gptData->sbtVertexPosBuffer, ((plVec3){ fCubeSide, -fCubeSide, -fCubeSide}));
    pl_sb_push(gptData->sbtVertexPosBuffer, ((plVec3){-fCubeSide,  fCubeSide, -fCubeSide}));
    pl_sb_push(gptData->sbtVertexPosBuffer, ((plVec3){ fCubeSide,  fCubeSide, -fCubeSide}));
    pl_sb_push(gptData->sbtVertexPosBuffer, ((plVec3){-fCubeSide, -fCubeSide,  fCubeSide}));
    pl_sb_push(gptData->sbtVertexPosBuffer, ((plVec3){ fCubeSide, -fCubeSide,  fCubeSide}));
    pl_sb_push(gptData->sbtVertexPosBuffer, ((plVec3){-fCubeSide,  fCubeSide,  fCubeSide}));
    pl_sb_push(gptData->sbtVertexPosBuffer, ((plVec3){ fCubeSide,  fCubeSide,  fCubeSide})); 
}

static void
pl_refr_load_stl(const char* pcModelPath, plVec4 tColor, const plMat4* ptTransform)
{
    uint32_t uFileSize = 0;
    gptFile->read(pcModelPath, &uFileSize, NULL, "rb");
    char* pcBuffer = PL_ALLOC(uFileSize);
    memset(pcBuffer, 0, uFileSize);
    gptFile->read(pcModelPath, &uFileSize, pcBuffer, "rb");

    plEntity tEntity = gptECS->create_object(&gptData->tComponentLibrary, pcModelPath);
    plMeshComponent* ptMesh = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_MESH, tEntity);
    plTransformComponent* ptTransformComp = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, tEntity);
    ptTransformComp->tWorld = *ptTransform;

    ptMesh->tMaterial = gptECS->create_material(&gptData->tComponentLibrary, pcModelPath);
    plMaterialComponent* ptMaterial = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_MATERIAL, ptMesh->tMaterial);
    ptMaterial->tBaseColor = tColor;
    ptMaterial->tBlendMode = PL_MATERIAL_BLEND_MODE_ALPHA;
    
    plStlInfo tInfo = {0};
    pl_load_stl(pcBuffer, (size_t)uFileSize, NULL, NULL, NULL, &tInfo);

    ptMesh->ulVertexStreamMask = PL_MESH_FORMAT_FLAG_HAS_NORMAL;
    pl_sb_resize(ptMesh->sbtVertexPositions, (uint32_t)(tInfo.szPositionStreamSize / 3));
    pl_sb_resize(ptMesh->sbtVertexNormals, (uint32_t)(tInfo.szNormalStreamSize / 3));
    pl_sb_resize(ptMesh->sbuIndices, (uint32_t)tInfo.szIndexBufferSize);

    pl_load_stl(pcBuffer, (size_t)uFileSize, (float*)ptMesh->sbtVertexPositions, (float*)ptMesh->sbtVertexNormals, (uint32_t*)ptMesh->sbuIndices, &tInfo);

    // calculate AABB
    ptMesh->tAABB.tMax = (plVec3){-FLT_MAX, -FLT_MAX, -FLT_MAX};
    ptMesh->tAABB.tMin = (plVec3){FLT_MAX, FLT_MAX, FLT_MAX};
    
    for(uint32_t i = 0; i < pl_sb_size(ptMesh->sbtVertexPositions); i++)
    {
        if(ptMesh->sbtVertexPositions[i].x > ptMesh->tAABB.tMax.x) ptMesh->tAABB.tMax.x = ptMesh->sbtVertexPositions[i].x;
        if(ptMesh->sbtVertexPositions[i].y > ptMesh->tAABB.tMax.y) ptMesh->tAABB.tMax.y = ptMesh->sbtVertexPositions[i].y;
        if(ptMesh->sbtVertexPositions[i].z > ptMesh->tAABB.tMax.z) ptMesh->tAABB.tMax.z = ptMesh->sbtVertexPositions[i].z;
        if(ptMesh->sbtVertexPositions[i].x < ptMesh->tAABB.tMin.x) ptMesh->tAABB.tMin.x = ptMesh->sbtVertexPositions[i].x;
        if(ptMesh->sbtVertexPositions[i].y < ptMesh->tAABB.tMin.y) ptMesh->tAABB.tMin.y = ptMesh->sbtVertexPositions[i].y;
        if(ptMesh->sbtVertexPositions[i].z < ptMesh->tAABB.tMin.z) ptMesh->tAABB.tMin.z = ptMesh->sbtVertexPositions[i].z;
    }

    PL_FREE(pcBuffer);

    plBindGroupLayout tMaterialBindGroupLayout = {
        .uTextureCount = 2,
        .aTextures = {
            {.uSlot = 0, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
            {.uSlot = 1, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
        }
    };
    plBindGroupHandle tMaterialBindGroup = gptDevice->create_bind_group(&gptData->tGraphics.tDevice, &tMaterialBindGroupLayout);

    plDrawable tDrawable = {
        .tEntity = tEntity,
        .tMaterialBindGroup = tMaterialBindGroup
    };

    pl_sb_push(gptData->sbtDrawables, tDrawable);
    pl_sb_push(gptData->sbtMaterialEntities, ptMesh->tMaterial);
    pl_sb_push(gptData->sbtMaterialBindGroups, tMaterialBindGroup);
}

static void
pl_refr_load_gltf(const char* pcPath)
{
    cgltf_options tGltfOptions = {0};
    cgltf_data* ptGltfData = NULL;

    char acDirectory[1024] = {0};
    pl_str_get_directory(pcPath, acDirectory);

    cgltf_result tGltfResult = cgltf_parse_file(&tGltfOptions, pcPath, &ptGltfData);
    PL_ASSERT(tGltfResult == cgltf_result_success);

    tGltfResult = cgltf_load_buffers(&tGltfOptions, ptGltfData, pcPath);
    PL_ASSERT(tGltfResult == cgltf_result_success);

    // TODO: mark nodes that are joints

    for(size_t i = 0; i < ptGltfData->scenes_count; i++)
    {
        const cgltf_scene* ptScene = &ptGltfData->scenes[i];
        for(size_t j = 0; j < ptScene->nodes_count; j++)
        {
            const cgltf_node* ptNode = ptScene->nodes[j];
            pl__refr_load_gltf_object(acDirectory, (plEntity){UINT32_MAX, UINT32_MAX}, ptNode);
        }
    }
}

static void
pl__load_gltf_texture(plTextureSlot tSlot, const cgltf_texture_view* ptTexture, const char* pcDirectory, const cgltf_material* ptGltfMaterial, plMaterialComponent* ptMaterial)
{
    ptMaterial->atTextureMaps[tSlot].uUVSet = ptTexture->texcoord;

    if(ptTexture->texture->image->buffer_view)
    {
        char* pucBufferData = ptTexture->texture->image->buffer_view->buffer->data;
        char* pucActualBuffer = &pucBufferData[ptTexture->texture->image->buffer_view->offset];
        ptMaterial->atTextureMaps[tSlot].acName[0] = (char)tSlot + 1;
        strncpy(&ptMaterial->atTextureMaps[tSlot].acName[1], pucActualBuffer, 127);
        
        ptMaterial->atTextureMaps[tSlot].tResource = gptResource->load_resource(ptMaterial->atTextureMaps[tSlot].acName, PL_RESOURCE_LOAD_FLAG_RETAIN_DATA, pucActualBuffer, ptTexture->texture->image->buffer_view->size);
    }
    else if(strncmp(ptTexture->texture->image->uri, "data:", 5) == 0)
    {
        const char* comma = strchr(ptTexture->texture->image->uri, ',');

        if (comma && comma - ptTexture->texture->image->uri >= 7 && strncmp(comma - 7, ";base64", 7) == 0)
        {
            cgltf_options tOptions = {0};
            ptMaterial->atTextureMaps[tSlot].acName[0] = (char)tSlot + 1;
            strcpy(&ptMaterial->atTextureMaps[tSlot].acName[1], ptGltfMaterial->name);
            
            void* outData = NULL;
            const char *base64 = comma + 1;
            const size_t szBufferLength = strlen(base64);
            size_t szSize = szBufferLength - szBufferLength / 4;
            if(szBufferLength >= 2)
            {
                szSize -= base64[szBufferLength - 2] == '=';
                szSize -= base64[szBufferLength - 1] == '=';
            }
            cgltf_result res = cgltf_load_buffer_base64(&tOptions, szSize, base64, &outData);
            PL_ASSERT(res == cgltf_result_success);
            ptMaterial->atTextureMaps[tSlot].tResource = gptResource->load_resource(ptMaterial->atTextureMaps[tSlot].acName, PL_RESOURCE_LOAD_FLAG_RETAIN_DATA, outData, szSize);
        }
    }
    else
    {
        strncpy(ptMaterial->atTextureMaps[tSlot].acName, ptTexture->texture->image->uri, PL_MAX_NAME_LENGTH);
        char acFilepath[2048] = {0};
        strcpy(acFilepath, pcDirectory);
        pl_str_concatenate(acFilepath, ptMaterial->atTextureMaps[tSlot].acName, acFilepath, 2048);

        uint32_t uFileSize = 0;
        gptFile->read(acFilepath, &uFileSize, NULL, "rb");
        char* pcBuffer = PL_ALLOC(uFileSize);
        memset(pcBuffer, 0, uFileSize);
        gptFile->read(acFilepath, &uFileSize, pcBuffer, "rb");
        ptMaterial->atTextureMaps[tSlot].tResource = gptResource->load_resource(ptTexture->texture->image->uri, PL_RESOURCE_LOAD_FLAG_RETAIN_DATA, pcBuffer, (size_t)uFileSize);
        PL_FREE(pcBuffer);
    }
}

static void
pl__refr_load_material(const char* pcDirectory, plMaterialComponent* ptMaterial, const cgltf_material* ptGltfMaterial)
{
    ptMaterial->tShaderType = PL_SHADER_TYPE_PBR;
    ptMaterial->tFlags = ptGltfMaterial->double_sided ? PL_MATERIAL_FLAG_DOUBLE_SIDED : PL_MATERIAL_FLAG_NONE;
    ptMaterial->fAlphaCutoff = ptGltfMaterial->alpha_cutoff;

    // blend mode
    if(ptGltfMaterial->alpha_mode == cgltf_alpha_mode_opaque)
        ptMaterial->tBlendMode = PL_MATERIAL_BLEND_MODE_OPAQUE;
    else if(ptGltfMaterial->alpha_mode == cgltf_alpha_mode_mask)
        ptMaterial->tBlendMode = PL_MATERIAL_BLEND_MODE_ALPHA;
    else
        ptMaterial->tBlendMode = PL_MATERIAL_BLEND_MODE_PREMULTIPLIED;

	if(ptGltfMaterial->normal_texture.texture)
		pl__load_gltf_texture(PL_TEXTURE_SLOT_NORMAL_MAP, &ptGltfMaterial->normal_texture, pcDirectory, ptGltfMaterial, ptMaterial);

    if(ptGltfMaterial->has_pbr_metallic_roughness)
    {
        ptMaterial->tBaseColor.x = ptGltfMaterial->pbr_metallic_roughness.base_color_factor[0];
        ptMaterial->tBaseColor.y = ptGltfMaterial->pbr_metallic_roughness.base_color_factor[1];
        ptMaterial->tBaseColor.z = ptGltfMaterial->pbr_metallic_roughness.base_color_factor[2];
        ptMaterial->tBaseColor.w = ptGltfMaterial->pbr_metallic_roughness.base_color_factor[3];

        if(ptGltfMaterial->pbr_metallic_roughness.base_color_texture.texture)
			pl__load_gltf_texture(PL_TEXTURE_SLOT_BASE_COLOR_MAP, &ptGltfMaterial->pbr_metallic_roughness.base_color_texture, pcDirectory, ptGltfMaterial, ptMaterial);
    }
}

static void
pl__refr_load_attributes(plMeshComponent* ptMesh, const cgltf_primitive* ptPrimitive)
{
    const size_t szVertexCount = ptPrimitive->attributes[0].data->count;
    for(size_t szAttributeIndex = 0; szAttributeIndex < ptPrimitive->attributes_count; szAttributeIndex++)
    {
        const cgltf_attribute* ptAttribute = &ptPrimitive->attributes[szAttributeIndex];
        const cgltf_buffer* ptBuffer = ptAttribute->data->buffer_view->buffer;
        const size_t szStride = ptAttribute->data->stride;
        PL_ASSERT(szStride > 0 && "attribute stride must node be zero");

        unsigned char* pucBufferStart = &((unsigned char*)ptBuffer->data)[ptAttribute->data->buffer_view->offset + ptAttribute->data->offset];

        switch(ptAttribute->type)
        {
            case cgltf_attribute_type_position:
            {
                ptMesh->tAABB.tMax = (plVec3){ptAttribute->data->max[0], ptAttribute->data->max[1], ptAttribute->data->max[2]};
                ptMesh->tAABB.tMin = (plVec3){ptAttribute->data->min[0], ptAttribute->data->min[1], ptAttribute->data->min[2]};
                pl_sb_resize(ptMesh->sbtVertexPositions, (uint32_t)szVertexCount);
                for(size_t i = 0; i < szVertexCount; i++)
                {
                    plVec3* ptRawData = (plVec3*)&pucBufferStart[i * szStride];
                    ptMesh->sbtVertexPositions[i] = *ptRawData;
                }
                break;
            }

            case cgltf_attribute_type_normal:
            {
                pl_sb_resize(ptMesh->sbtVertexNormals, (uint32_t)szVertexCount);
                for(size_t i = 0; i < szVertexCount; i++)
                {
                    plVec3* ptRawData = (plVec3*)&pucBufferStart[i * szStride];
                    ptMesh->sbtVertexNormals[i] = *ptRawData;
                }
                break;
            }

            case cgltf_attribute_type_tangent:
            {
                pl_sb_resize(ptMesh->sbtVertexTangents, (uint32_t)szVertexCount);
                for(size_t i = 0; i < szVertexCount; i++)
                {
                    plVec4* ptRawData = (plVec4*)&pucBufferStart[i * szStride];
                    ptMesh->sbtVertexTangents[i] = *ptRawData;
                }
                break;
            }

            case cgltf_attribute_type_texcoord:
            {
                pl_sb_resize(ptMesh->sbtVertexTextureCoordinates[ptAttribute->index], (uint32_t)szVertexCount);
                
                if(ptAttribute->data->component_type == cgltf_component_type_r_32f)
                {
                    for(size_t i = 0; i < szVertexCount; i++)
                    {
                        plVec2* ptRawData = (plVec2*)&pucBufferStart[i * szStride];
                        (ptMesh->sbtVertexTextureCoordinates[ptAttribute->index])[i] = *ptRawData;
                    }
                }
                else if(ptAttribute->data->component_type == cgltf_component_type_r_16u)
                {
                    for(size_t i = 0; i < szVertexCount; i++)
                    {
                        uint16_t* puRawData = (uint16_t*)&pucBufferStart[i * szStride];
                        (ptMesh->sbtVertexTextureCoordinates[ptAttribute->index])[i].x = (float)puRawData[0];
                        (ptMesh->sbtVertexTextureCoordinates[ptAttribute->index])[i].y = (float)puRawData[1];
                    }
                }
                else if(ptAttribute->data->component_type == cgltf_component_type_r_8u)
                {
                    for(size_t i = 0; i < szVertexCount; i++)
                    {
                        uint8_t* puRawData = (uint8_t*)&pucBufferStart[i * szStride];
                        (ptMesh->sbtVertexTextureCoordinates[ptAttribute->index])[i].x = (float)puRawData[0];
                        (ptMesh->sbtVertexTextureCoordinates[ptAttribute->index])[i].y = (float)puRawData[1];
                    }
                }
                break;
            }

            case cgltf_attribute_type_color:
            {
                pl_sb_resize(ptMesh->sbtVertexColors[ptAttribute->index], (uint32_t)szVertexCount);
                
                if(ptAttribute->data->component_type == cgltf_component_type_r_32f)
                {
                    for(size_t i = 0; i < szVertexCount; i++)
                    {
                        plVec4* ptRawData = (plVec4*)&pucBufferStart[i * szStride];
                        (ptMesh->sbtVertexColors[ptAttribute->index])[i] = *ptRawData;
                    }
                }
                else if(ptAttribute->data->component_type == cgltf_component_type_r_16u)
                {
                    const float fConversion = 1.0f / (256.0f * 256.0f);
                    for(size_t i = 0; i < szVertexCount; i++)
                    {
                        uint16_t* puRawData = (uint16_t*)&pucBufferStart[i * szStride];
                        (ptMesh->sbtVertexColors[ptAttribute->index])[i].r = (float)puRawData[0] * fConversion;
                        (ptMesh->sbtVertexColors[ptAttribute->index])[i].g = (float)puRawData[1] * fConversion;
                        (ptMesh->sbtVertexColors[ptAttribute->index])[i].b = (float)puRawData[2] * fConversion;
                        (ptMesh->sbtVertexColors[ptAttribute->index])[i].a = (float)puRawData[3] * fConversion;
                    }
                }
                else
                {
                    PL_ASSERT(false);
                }

                break;
            }

            case cgltf_attribute_type_joints:
            {
                pl_sb_resize(ptMesh->sbtVertexJoints[ptAttribute->index], (uint32_t)szVertexCount);
                
                if(ptAttribute->data->component_type == cgltf_component_type_r_16u)
                {
                    for(size_t i = 0; i < szVertexCount; i++)
                    {
                        uint16_t* puRawData = (uint16_t*)&pucBufferStart[i * szStride];
                        (ptMesh->sbtVertexJoints[ptAttribute->index])[i].x = (float)puRawData[0];
                        (ptMesh->sbtVertexJoints[ptAttribute->index])[i].y = (float)puRawData[1];
                        (ptMesh->sbtVertexJoints[ptAttribute->index])[i].z = (float)puRawData[2];
                        (ptMesh->sbtVertexJoints[ptAttribute->index])[i].w = (float)puRawData[3];
                    }
                }
                else if(ptAttribute->data->component_type == cgltf_component_type_r_8u)
                {
                    for(size_t i = 0; i < szVertexCount; i++)
                    {
                        uint8_t* puRawData = (uint8_t*)&pucBufferStart[i * szStride];
                        (ptMesh->sbtVertexJoints[ptAttribute->index])[i].x = (float)puRawData[0];
                        (ptMesh->sbtVertexJoints[ptAttribute->index])[i].y = (float)puRawData[1];
                        (ptMesh->sbtVertexJoints[ptAttribute->index])[i].z = (float)puRawData[2];
                        (ptMesh->sbtVertexJoints[ptAttribute->index])[i].w = (float)puRawData[3];
                    }
                }
                break;
            }

            case cgltf_attribute_type_weights:
            {
                pl_sb_resize(ptMesh->sbtVertexWeights[ptAttribute->index], (uint32_t)szVertexCount);
                
                if(ptAttribute->data->component_type == cgltf_component_type_r_32f)
                {
                    for(size_t i = 0; i < szVertexCount; i++)
                    {
                        plVec4* ptRawData = (plVec4*)&pucBufferStart[i * szStride];
                        (ptMesh->sbtVertexWeights[ptAttribute->index])[i] = *ptRawData;
                    }
                }
                else if(ptAttribute->data->component_type == cgltf_component_type_r_16u)
                {
                    for(size_t i = 0; i < szVertexCount; i++)
                    {
                        uint16_t* puRawData = (uint16_t*)&pucBufferStart[i * szStride];
                        (ptMesh->sbtVertexWeights[ptAttribute->index])[i].x = (float)puRawData[0];
                        (ptMesh->sbtVertexWeights[ptAttribute->index])[i].y = (float)puRawData[1];
                        (ptMesh->sbtVertexWeights[ptAttribute->index])[i].z = (float)puRawData[2];
                        (ptMesh->sbtVertexWeights[ptAttribute->index])[i].w = (float)puRawData[3];
                    }
                }
                else if(ptAttribute->data->component_type == cgltf_component_type_r_8u)
                {
                    for(size_t i = 0; i < szVertexCount; i++)
                    {
                        uint8_t* puRawData = (uint8_t*)&pucBufferStart[i * szStride];
                        (ptMesh->sbtVertexWeights[ptAttribute->index])[i].x = (float)puRawData[0];
                        (ptMesh->sbtVertexWeights[ptAttribute->index])[i].y = (float)puRawData[1];
                        (ptMesh->sbtVertexWeights[ptAttribute->index])[i].z = (float)puRawData[2];
                        (ptMesh->sbtVertexWeights[ptAttribute->index])[i].w = (float)puRawData[3];
                    }
                }
                break;
            }

            default:
            {
                PL_ASSERT(false && "unknown attribute");
            }
        }
    }

    // index buffer
    if(ptPrimitive->indices)
    {
        pl_sb_resize(ptMesh->sbuIndices, (uint32_t)ptPrimitive->indices->count);
        unsigned char* pucIdexBufferStart = &((unsigned char*)ptPrimitive->indices->buffer_view->buffer->data)[ptPrimitive->indices->buffer_view->offset + ptPrimitive->indices->offset];
        switch(ptPrimitive->indices->component_type)
        {
            case cgltf_component_type_r_32u:
            {
                
                if(ptPrimitive->indices->buffer_view->stride == 0)
                {
                    for(uint32_t i = 0; i < ptPrimitive->indices->count; i++)
                        ptMesh->sbuIndices[i] = *(uint32_t*)&pucIdexBufferStart[i * sizeof(uint32_t)];
                }
                else
                {
                    for(uint32_t i = 0; i < ptPrimitive->indices->count; i++)
                        ptMesh->sbuIndices[i] = *(uint32_t*)&pucIdexBufferStart[i * ptPrimitive->indices->buffer_view->stride];
                }
                break;
            }

            case cgltf_component_type_r_16u:
            {
                if(ptPrimitive->indices->buffer_view->stride == 0)
                {
                    for(uint32_t i = 0; i < ptPrimitive->indices->count; i++)
                        ptMesh->sbuIndices[i] = (uint32_t)*(unsigned short*)&pucIdexBufferStart[i * sizeof(unsigned short)];
                }
                else
                {
                    for(uint32_t i = 0; i < ptPrimitive->indices->count; i++)
                        ptMesh->sbuIndices[i] = (uint32_t)*(unsigned short*)&pucIdexBufferStart[i * ptPrimitive->indices->buffer_view->stride];
                }
                break;
            }
            case cgltf_component_type_r_8u:
            {
                if(ptPrimitive->indices->buffer_view->stride == 0)
                {
                    for(uint32_t i = 0; i < ptPrimitive->indices->count; i++)
                        ptMesh->sbuIndices[i] = (uint32_t)*(uint8_t*)&pucIdexBufferStart[i * sizeof(uint8_t)];
                }
                else
                {
                    for(uint32_t i = 0; i < ptPrimitive->indices->count; i++)
                        ptMesh->sbuIndices[i] = (uint32_t)*(uint8_t*)&pucIdexBufferStart[i * ptPrimitive->indices->buffer_view->stride];
                }
                break;
            }
            default:
            {
                PL_ASSERT(false);
            }
        }
    }


}

static void
pl__refr_load_gltf_object(const char* pcDirectory, plEntity tParentEntity, const cgltf_node* ptNode)
{
    plComponentLibrary* ptLibrary = &gptData->tComponentLibrary;

    plEntity tNewEntity = gptECS->create_transform(ptLibrary, ptNode->name);
    plTransformComponent* ptTransform = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, tNewEntity);

    // transform defaults
    ptTransform->tWorld       = pl_identity_mat4();
    ptTransform->tRotation    = (plVec4){0.0f, 0.0f, 0.0f, 1.0f};
    ptTransform->tScale       = (plVec3){1.0f, 1.0f, 1.0f};
    ptTransform->tTranslation = (plVec3){0.0f, 0.0f, 0.0f};

    if(ptNode->has_rotation)    memcpy(ptTransform->tRotation.d, ptNode->rotation, sizeof(plVec4));
    if(ptNode->has_scale)       memcpy(ptTransform->tScale.d, ptNode->scale, sizeof(plVec3));
    if(ptNode->has_translation) memcpy(ptTransform->tTranslation.d, ptNode->translation, sizeof(plVec3));

    // must use provided matrix, otherwise calculate based on rot, scale, trans
    if(ptNode->has_matrix)
        memcpy(ptTransform->tWorld.d, ptNode->matrix, sizeof(plMat4));
    else
        ptTransform->tWorld = pl_rotation_translation_scale(ptTransform->tRotation, ptTransform->tTranslation, ptTransform->tScale);

    ptTransform->tFinalTransform = ptTransform->tWorld;

    // attach to parent if parent is valid
    if(tParentEntity.uIndex != UINT32_MAX)
        gptECS->attach_component(ptLibrary, tNewEntity, tParentEntity);

    // check if node has attached mesh
    if(ptNode->mesh)
    {
        PL_ASSERT(ptNode->mesh->primitives_count == 1);
        for(size_t szPrimitiveIndex = 0; szPrimitiveIndex < ptNode->mesh->primitives_count; szPrimitiveIndex++)
        {
            // add mesh to our node
            plMeshComponent* ptMesh = gptECS->add_component(ptLibrary, PL_COMPONENT_TYPE_MESH, tNewEntity);
            plObjectComponent* ptObject = gptECS->add_component(ptLibrary, PL_COMPONENT_TYPE_OBJECT, tNewEntity);
            ptObject->tMesh = tNewEntity;
            ptObject->tTransform = tNewEntity;

            const cgltf_primitive* ptPrimitive = &ptNode->mesh->primitives[szPrimitiveIndex];

            // load attributes
            pl__refr_load_attributes(ptMesh, ptPrimitive);

            ptMesh->tMaterial.uIndex      = UINT32_MAX;
            ptMesh->tMaterial.uGeneration = UINT32_MAX;

            // load material
            if(ptPrimitive->material)
            {
                plBindGroupHandle tMaterialBindGroup = {UINT32_MAX, UINT32_MAX};

                // check if the material already exists
                if(pl_hm_has_key(&gptData->tMaterialHashMap, (uint64_t)ptPrimitive->material))
                {
                    const uint64_t ulMaterialIndex = pl_hm_lookup(&gptData->tMaterialHashMap, (uint64_t)ptPrimitive->material);
                    ptMesh->tMaterial = gptData->sbtMaterialEntities[ulMaterialIndex];
                    tMaterialBindGroup = gptData->sbtMaterialBindGroups[ulMaterialIndex];
                }
                else // create new material
                {
                    ptMesh->tMaterial = gptECS->create_material(&gptData->tComponentLibrary, ptPrimitive->material->name);
                    
                    uint64_t ulFreeIndex = pl_hm_get_free_index(&gptData->tMaterialHashMap);
                    if(ulFreeIndex == UINT64_MAX)
                    {
                        ulFreeIndex = pl_sb_size(gptData->sbtMaterialEntities);
                        pl_sb_add(gptData->sbtMaterialEntities);
                        pl_sb_add(gptData->sbtMaterialBindGroups);
                    }

                    gptData->sbtMaterialEntities[ulFreeIndex] = ptMesh->tMaterial;
                    pl_hm_insert(&gptData->tMaterialHashMap, (uint64_t)ptPrimitive->material, ulFreeIndex);

                    plBindGroupLayout tMaterialBindGroupLayout = {
                        .uTextureCount = 2,
                        .aTextures = {
                            {.uSlot = 0, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                            {.uSlot = 1, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                        }
                    };
                    gptData->sbtMaterialBindGroups[ulFreeIndex] = gptDevice->create_bind_group(&gptData->tGraphics.tDevice, &tMaterialBindGroupLayout);
                    tMaterialBindGroup = gptData->sbtMaterialBindGroups[ulFreeIndex];

                    plMaterialComponent* ptMaterial = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_MATERIAL, ptMesh->tMaterial);
                    pl__refr_load_material(pcDirectory, ptMaterial, ptPrimitive->material);
                }

                // TODO: separate by opaque/transparent
                plDrawable tDrawable = {.tEntity = tNewEntity, .tMaterialBindGroup = tMaterialBindGroup};
                pl_sb_push(gptData->sbtDrawables, tDrawable);
            }
        }
    }

    // recurse through children
    for(size_t i = 0; i < ptNode->children_count; i++)
        pl__refr_load_gltf_object(pcDirectory, tNewEntity, ptNode->children[i]);
}

static plTextureViewHandle
pl__create_texture_helper(plMaterialComponent* ptMaterial, plTextureSlot tSlot, bool bHdr, int iMips)
{
    plDevice* ptDevice = &gptData->tGraphics.tDevice;

    if(gptResource->is_resource_valid(ptMaterial->atTextureMaps[tSlot].tResource) == false)
        return gptData->tDummyTextureView;
    
    size_t szResourceSize = 0;
    const char* pcFileData = gptResource->get_file_data(ptMaterial->atTextureMaps[tSlot].tResource, &szResourceSize);
    int texWidth, texHeight, texNumChannels;
    int texForceNumChannels = 4;

    const plBufferDescription tStagingBufferDesc = {
        .tMemory              = PL_MEMORY_GPU_CPU,
        .tUsage               = PL_BUFFER_USAGE_UNSPECIFIED,
        .uByteSize            = 268435456
    };
    plBufferHandle tStagingBufferHandle = gptDevice->create_buffer(ptDevice, &tStagingBufferDesc, "staging buffer");
    plBuffer* ptStagingBuffer = gptDevice->get_buffer(ptDevice, tStagingBufferHandle);
    

    plTextureHandle tTexture = {0};

    plSampler tSampler = {
        .tFilter = PL_FILTER_LINEAR,
        .fMinMip = 0.0f,
        .fMaxMip = PL_MAX_MIPS,
        .tVerticalWrap = PL_WRAP_MODE_WRAP,
        .tHorizontalWrap = PL_WRAP_MODE_WRAP
    };

    plTextureViewHandle tHandle = {UINT32_MAX, UINT32_MAX};

    if(bHdr)
    {

        float* rawBytes = gptImage->loadf_from_memory((unsigned char*)pcFileData, (int)szResourceSize, &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
        PL_ASSERT(rawBytes);

        memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, rawBytes, sizeof(float) * texWidth * texHeight * 4);
        gptImage->free(rawBytes);

        plTextureDesc tTextureDesc = {
            .tDimensions = {(float)texWidth, (float)texHeight, 1},
            .tFormat = PL_FORMAT_R32G32B32A32_FLOAT,
            .uLayers = 1,
            .uMips = iMips,
            .tType = PL_TEXTURE_TYPE_2D,
            .tUsage = PL_TEXTURE_USAGE_SAMPLED,
            .tSamples = PL_SAMPLE_COUNT_1
        };
        tTexture = gptDevice->create_texture(ptDevice, tTextureDesc, ptMaterial->atTextureMaps[tSlot].acName);
        plBufferImageCopy tBufferImageCopy = {
            .tImageExtent = {texWidth, texHeight, 1},
            .uLayerCount = 1
        };
        gptDevice->copy_buffer_to_texture(ptDevice, tStagingBufferHandle, tTexture, 1, &tBufferImageCopy);
        gptDevice->generate_mipmaps(ptDevice, tTexture);
        

        plTextureViewDesc tTextureViewDesc = {
            .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
            .uBaseLayer  = 0,
            .uBaseMip    = 0,
            .uLayerCount = 1
        };
        tHandle = gptDevice->create_texture_view(ptDevice, &tTextureViewDesc, &tSampler, tTexture, ptMaterial->atTextureMaps[tSlot].acName);
    }
    else
    {
        unsigned char* rawBytes = gptImage->load_from_memory((unsigned char*)pcFileData, (int)szResourceSize, &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
        PL_ASSERT(rawBytes);

        memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, rawBytes, texWidth * texHeight * 4);
        gptImage->free(rawBytes);


        plTextureDesc tTextureDesc = {
            .tDimensions = {(float)texWidth, (float)texHeight, 1},
            .tFormat = PL_FORMAT_R8G8B8A8_UNORM,
            .uLayers = 1,
            .uMips = iMips,
            .tType = PL_TEXTURE_TYPE_2D,
            .tUsage = PL_TEXTURE_USAGE_SAMPLED,
            .tSamples = PL_SAMPLE_COUNT_1
        };
        tTexture = gptDevice->create_texture(ptDevice, tTextureDesc, ptMaterial->atTextureMaps[tSlot].acName);
        plBufferImageCopy tBufferImageCopy = {
            .tImageExtent = {texWidth, texHeight, 1},
            .uLayerCount = 1
        };
        gptDevice->copy_buffer_to_texture(ptDevice, tStagingBufferHandle, tTexture, 1, &tBufferImageCopy);
        gptDevice->generate_mipmaps(ptDevice, tTexture);
        
        plTextureViewDesc tTextureViewDesc = {
            .tFormat     = PL_FORMAT_R8G8B8A8_UNORM,
            .uBaseLayer  = 0,
            .uBaseMip    = 0,
            .uLayerCount = 1
        };

        tHandle = gptDevice->create_texture_view(ptDevice, &tTextureViewDesc, &tSampler, tTexture, ptMaterial->atTextureMaps[tSlot].acName);
    }
    return tHandle;
}

static void
pl_refr_finalize_scene(void)
{
    plGraphics* ptGraphics = &gptData->tGraphics;
    plDevice* ptDevice = &ptGraphics->tDevice;

    // create template shader
    {
        plShaderDescription tShaderDescription = {

    #ifdef PL_METAL_BACKEND
            .pcVertexShader = "../shaders/metal/primitive.metal",
            .pcPixelShader = "../shaders/metal/primitive.metal",
    #else
            .pcVertexShader = "primitive.vert.spv",
            .pcPixelShader = "primitive.frag.spv",
    #endif
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
            },
            .uConstantCount = 4,
            .tRenderPassLayout = gptData->tMainRenderPassLayout,
            .uBindGroupLayoutCount = 3,
            .atBindGroupLayouts = {
                {
                    .uBufferCount  = 3,
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
                        {
                            .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                            .uSlot = 2,
                            .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                        }
                    }
                },
                {
                    .uTextureCount  = 2,
                    .aTextures = {
                        {.uSlot =  0, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                        {.uSlot =  1, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                    }
                },
                {
                    .uBufferCount  = 1,
                    .aBuffers = {
                        {
                            .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                            .uSlot = 0,
                            .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                        }
                    }
                }
            }
        };
        for(uint32_t i = 0; i < tShaderDescription.uConstantCount; i++)
        {
            tShaderDescription.atConstants[i].uID = i;
            tShaderDescription.atConstants[i].uOffset = i * sizeof(int);
            tShaderDescription.atConstants[i].tType = PL_DATA_TYPE_INT;
        }

        int aiConstantData[4] = {0};
        aiConstantData[2] = 0;
        aiConstantData[3] = 0;
        
        aiConstantData[0] = (int)PL_MESH_FORMAT_FLAG_HAS_NORMAL;
        int iFlagCopy = (int)PL_MESH_FORMAT_FLAG_HAS_NORMAL;
        while(iFlagCopy)
        {
            aiConstantData[1] += iFlagCopy & 1;
            iFlagCopy >>= 1;
        }
        tShaderDescription.pTempConstantData = aiConstantData;
        gptData->tShader = gptDevice->create_shader(ptDevice, &tShaderDescription);
    }

    // update material bind groups
    const uint32_t uMaterialCount = pl_sb_size(gptData->sbtMaterialEntities);
    for(uint32_t i = 0; i < uMaterialCount; i++)
    {
        plMaterialComponent* ptMaterial = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_MATERIAL, gptData->sbtMaterialEntities[i]);

        plTextureViewHandle atMaterialTextureViews[2] = {0};

        atMaterialTextureViews[0] = pl__create_texture_helper(ptMaterial, PL_TEXTURE_SLOT_BASE_COLOR_MAP, true, 0);
        atMaterialTextureViews[1] = pl__create_texture_helper(ptMaterial, PL_TEXTURE_SLOT_NORMAL_MAP, false, 0);

        plBindGroupLayout tMaterialBindGroupLayout = {
            .uTextureCount = 2,
            .aTextures = {
                {.uSlot = 0, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                {.uSlot = 1, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
            }
        };
        gptDevice->update_bind_group(ptDevice, &gptData->sbtMaterialBindGroups[i], 0, NULL, NULL, 2, atMaterialTextureViews);
    }

    // fill CPU buffers & drawable list
    const uint32_t uDrawableCount = pl_sb_size(gptData->sbtDrawables);
    for(uint32_t uDrawableIndex = 0; uDrawableIndex < uDrawableCount; uDrawableIndex++)
    {
        plEntity tEntity = gptData->sbtDrawables[uDrawableIndex].tEntity;
        plMeshComponent* ptMesh = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_MESH, tEntity);
        plTransformComponent* ptTransformComp = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, tEntity);
        plMaterialComponent* ptMaterial = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_MATERIAL, ptMesh->tMaterial);

        const uint32_t uStartIndex     = pl_sb_size(gptData->sbtVertexPosBuffer);
        const uint32_t uIndexStart     = pl_sb_size(gptData->sbuIndexBuffer);
        const uint32_t uDataStartIndex = pl_sb_size(gptData->sbtVertexDataBuffer);
        const uint32_t uIndexCount    = pl_sb_size(ptMesh->sbuIndices);
        const uint32_t uVertexCount   = pl_sb_size(ptMesh->sbtVertexPositions);

        plMaterial tMaterial = {
            .tColor = ptMaterial->tBaseColor
        };
        pl_sb_push(gptData->sbtMaterialBuffer, tMaterial);

        for(uint32_t j = 0; j < uIndexCount; j++)
            pl_sb_push(gptData->sbuIndexBuffer, uStartIndex + ptMesh->sbuIndices[j]);

        pl_sb_add_n(gptData->sbtVertexPosBuffer, uVertexCount);
        memcpy(&gptData->sbtVertexPosBuffer[uStartIndex], ptMesh->sbtVertexPositions, sizeof(plVec3) * uVertexCount);

        // stride within storage buffer
        uint32_t uStride = 0;

        // calculate vertex stream mask based on provided data
        if(pl_sb_size(ptMesh->sbtVertexNormals) > 0)               { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_NORMAL; }
        if(pl_sb_size(ptMesh->sbtVertexTangents) > 0)              { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TANGENT; }
        if(pl_sb_size(ptMesh->sbtVertexColors[0]) > 0)             { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_COLOR_0; }
        if(pl_sb_size(ptMesh->sbtVertexColors[1]) > 0)             { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_COLOR_1; }
        if(pl_sb_size(ptMesh->sbtVertexWeights[0]) > 0)            { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0; }
        if(pl_sb_size(ptMesh->sbtVertexWeights[1]) > 0)            { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1; }
        if(pl_sb_size(ptMesh->sbtVertexJoints[0]) > 0)             { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_JOINTS_0; }
        if(pl_sb_size(ptMesh->sbtVertexJoints[1]) > 0)             { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_JOINTS_1; }
        if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[0]) > 0) { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0; }
        if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[1]) > 0) { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1; }

        pl_sb_add_n(gptData->sbtVertexDataBuffer, uStride * uVertexCount);


        // current attribute offset
        uint32_t uOffset = 0;

        // normals
        const uint32_t uVertexNormalCount = pl_sb_size(ptMesh->sbtVertexNormals);
        for(uint32_t i = 0; i < uVertexNormalCount; i++)
        {
            ptMesh->sbtVertexNormals[i] = pl_norm_vec3(ptMesh->sbtVertexNormals[i]);
            const plVec3* ptNormal = &ptMesh->sbtVertexNormals[i];
            gptData->sbtVertexDataBuffer[uDataStartIndex + i * uStride].x = ptNormal->x;
            gptData->sbtVertexDataBuffer[uDataStartIndex + i * uStride].y = ptNormal->y;
            gptData->sbtVertexDataBuffer[uDataStartIndex + i * uStride].z = ptNormal->z;
            gptData->sbtVertexDataBuffer[uDataStartIndex + i * uStride].w = 0.0f;
        }

        if(uVertexNormalCount > 0)
            uOffset += 1;

        // tangents
        const uint32_t uVertexTangentCount = pl_sb_size(ptMesh->sbtVertexTangents);
        for(uint32_t i = 0; i < uVertexTangentCount; i++)
        {
            const plVec4* ptTangent = &ptMesh->sbtVertexTangents[i];
            gptData->sbtVertexDataBuffer[uDataStartIndex + i * uStride + uOffset].x = ptTangent->x;
            gptData->sbtVertexDataBuffer[uDataStartIndex + i * uStride + uOffset].y = ptTangent->y;
            gptData->sbtVertexDataBuffer[uDataStartIndex + i * uStride + uOffset].z = ptTangent->z;
            gptData->sbtVertexDataBuffer[uDataStartIndex + i * uStride + uOffset].w = ptTangent->w;
        }

        if(uVertexTangentCount > 0)
            uOffset += 1;

        // texture coordinates 0
        const uint32_t uVertexTexCount = pl_sb_size(ptMesh->sbtVertexTextureCoordinates[0]);
        for(uint32_t i = 0; i < uVertexTexCount; i++)
        {
            const plVec2* ptTextureCoordinates = &(ptMesh->sbtVertexTextureCoordinates[0])[i];
            gptData->sbtVertexDataBuffer[uDataStartIndex + i * uStride + uOffset].x = ptTextureCoordinates->u;
            gptData->sbtVertexDataBuffer[uDataStartIndex + i * uStride + uOffset].y = ptTextureCoordinates->v;
            gptData->sbtVertexDataBuffer[uDataStartIndex + i * uStride + uOffset].z = 0.0f;
            gptData->sbtVertexDataBuffer[uDataStartIndex + i * uStride + uOffset].w = 0.0f;

        }

        if(uVertexTexCount > 0)
            uOffset += 1;

        // color 0
        const uint32_t uVertexColorCount = pl_sb_size(ptMesh->sbtVertexColors[0]);
        for(uint32_t i = 0; i < uVertexColorCount; i++)
        {
            const plVec4* ptColor = &ptMesh->sbtVertexColors[0][i];
            gptData->sbtVertexDataBuffer[uDataStartIndex + i * uStride + uOffset].x = ptColor->r;
            gptData->sbtVertexDataBuffer[uDataStartIndex + i * uStride + uOffset].y = ptColor->g;
            gptData->sbtVertexDataBuffer[uDataStartIndex + i * uStride + uOffset].z = ptColor->b;
            gptData->sbtVertexDataBuffer[uDataStartIndex + i * uStride + uOffset].w = ptColor->a;
        }

        if(uVertexColorCount > 0)
            uOffset += 1;

        // joints 0
        const uint32_t uVertexJoint0Count = pl_sb_size(ptMesh->sbtVertexJoints[0]);
        for(uint32_t i = 0; i < uVertexJoint0Count; i++)
        {
            const plVec4* ptJoint = &ptMesh->sbtVertexJoints[0][i];
            gptData->sbtVertexDataBuffer[uDataStartIndex + i * uStride + uOffset].x = ptJoint->x;
            gptData->sbtVertexDataBuffer[uDataStartIndex + i * uStride + uOffset].y = ptJoint->y;
            gptData->sbtVertexDataBuffer[uDataStartIndex + i * uStride + uOffset].z = ptJoint->z;
            gptData->sbtVertexDataBuffer[uDataStartIndex + i * uStride + uOffset].w = ptJoint->w;
        }

        if(uVertexJoint0Count > 0)
            uOffset += 1;

        // weights 0
        const uint32_t uVertexWeights0Count = pl_sb_size(ptMesh->sbtVertexWeights[0]);
        for(uint32_t i = 0; i < uVertexWeights0Count; i++)
        {
            const plVec4* ptWeight = &ptMesh->sbtVertexWeights[0][i];
            gptData->sbtVertexDataBuffer[uDataStartIndex + i * uStride + uOffset].x = ptWeight->x;
            gptData->sbtVertexDataBuffer[uDataStartIndex + i * uStride + uOffset].y = ptWeight->y;
            gptData->sbtVertexDataBuffer[uDataStartIndex + i * uStride + uOffset].z = ptWeight->z;
            gptData->sbtVertexDataBuffer[uDataStartIndex + i * uStride + uOffset].w = ptWeight->w;
        }

        if(uVertexWeights0Count > 0)
            uOffset += 1;

        PL_ASSERT(uOffset == uStride && "sanity check");

        int aiConstantData[4] = {0};
        aiConstantData[0] = (int)ptMesh->ulVertexStreamMask;
        aiConstantData[2] = (int)(ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_BASE_COLOR_MAP].acName[0] != 0); // PL_HAS_BASE_COLOR_MAP;
        aiConstantData[3] = (int)(ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_NORMAL_MAP].acName[0] != 0); // PL_HAS_NORMAL_MAP
        int iFlagCopy = (int)ptMesh->ulVertexStreamMask;
        while(iFlagCopy)
        {
            aiConstantData[1] += iFlagCopy & 1;
            iFlagCopy >>= 1;
        }

        const plShaderVariant tVariant = {
            .pTempConstantData = aiConstantData,
            .tGraphicsState = {
                .ulDepthWriteEnabled  = 1,
                .ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_HAS_POSITION,
                .ulBlendMode          = ptMaterial->tBlendMode == PL_MATERIAL_BLEND_MODE_OPAQUE ? PL_BLEND_MODE_NONE : PL_BLEND_MODE_ALPHA,
                .ulDepthMode          = PL_COMPARE_MODE_LESS_OR_EQUAL,
                .ulCullMode           = ptMaterial->tBlendMode == PL_MATERIAL_BLEND_MODE_OPAQUE ? PL_CULL_MODE_CULL_BACK : PL_CULL_MODE_NONE,
                .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .ulStencilRef         = 0xff,
                .ulStencilMask        = 0xff,
                .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
                .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .ulStencilOpPass      = PL_STENCIL_OP_KEEP
            }
        };

        gptData->sbtDrawables[uDrawableIndex].uIndexCount    = uIndexCount;
        gptData->sbtDrawables[uDrawableIndex].uVertexCount   = uVertexCount;
        gptData->sbtDrawables[uDrawableIndex].uIndexOffset   = uIndexStart;
        gptData->sbtDrawables[uDrawableIndex].uVertexOffset  = uStartIndex;
        gptData->sbtDrawables[uDrawableIndex].uDataOffset    = uDataStartIndex;
        gptData->sbtDrawables[uDrawableIndex].uMaterialIndex = pl_sb_size(gptData->sbtMaterialBuffer) - 1;
        gptData->sbtDrawables[uDrawableIndex].uShader        = gptDevice->get_shader_variant(ptDevice, gptData->tShader, &tVariant).uIndex;
    }

    const plBufferDescription tStagingBufferDesc = {
        .tMemory              = PL_MEMORY_GPU_CPU,
        .tUsage               = PL_BUFFER_USAGE_UNSPECIFIED,
        .uByteSize            = 268435456
    };
    plBufferHandle tStagingBufferHandle = gptDevice->create_buffer(ptDevice, &tStagingBufferDesc, "staging buffer");
    plBuffer* tStagingBuffer = gptDevice->get_buffer(ptDevice, tStagingBufferHandle);

    // create buffers
    const plBufferDescription tShaderBufferDesc = {
        .tMemory              = PL_MEMORY_GPU,
        .tUsage               = PL_BUFFER_USAGE_STORAGE,
        .uByteSize            = sizeof(plMaterial) * pl_sb_size(gptData->sbtMaterialBuffer)
    };
    memcpy(tStagingBuffer->tMemoryAllocation.pHostMapped, gptData->sbtMaterialBuffer, sizeof(plMaterial) * pl_sb_size(gptData->sbtMaterialBuffer));
    gptData->tMaterialDataBuffer = gptDevice->create_buffer(ptDevice, &tShaderBufferDesc, "shader buffer");
    gptDevice->copy_buffer(ptDevice, tStagingBufferHandle, gptData->tMaterialDataBuffer, 0, 0, tShaderBufferDesc.uByteSize);
    tStagingBuffer = gptDevice->get_buffer(ptDevice, tStagingBufferHandle);

    const plBufferDescription tIndexBufferDesc = {
        .tMemory              = PL_MEMORY_GPU,
        .tUsage               = PL_BUFFER_USAGE_INDEX,
        .uByteSize            = sizeof(uint32_t) * pl_sb_size(gptData->sbuIndexBuffer)
    };
    memcpy(tStagingBuffer->tMemoryAllocation.pHostMapped, gptData->sbuIndexBuffer, sizeof(uint32_t) * pl_sb_size(gptData->sbuIndexBuffer));
    gptData->tIndexBuffer = gptDevice->create_buffer(ptDevice, &tIndexBufferDesc, "index buffer");
    gptDevice->copy_buffer(ptDevice, tStagingBufferHandle, gptData->tIndexBuffer, 0, 0, tIndexBufferDesc.uByteSize);
    tStagingBuffer = gptDevice->get_buffer(ptDevice, tStagingBufferHandle);

    const plBufferDescription tVertexBufferDesc = {
        .tMemory              = PL_MEMORY_GPU,
        .tUsage               = PL_BUFFER_USAGE_VERTEX,
        .uByteSize            = sizeof(plVec3) * pl_sb_size(gptData->sbtVertexPosBuffer)
    };
    memcpy(tStagingBuffer->tMemoryAllocation.pHostMapped, gptData->sbtVertexPosBuffer, sizeof(plVec3) * pl_sb_size(gptData->sbtVertexPosBuffer));
    gptData->tVertexBuffer = gptDevice->create_buffer(ptDevice, &tVertexBufferDesc, "vertex buffer");
    gptDevice->copy_buffer(ptDevice, tStagingBufferHandle, gptData->tVertexBuffer, 0, 0, tVertexBufferDesc.uByteSize);
    tStagingBuffer = gptDevice->get_buffer(ptDevice, tStagingBufferHandle);

    const plBufferDescription tStorageBufferDesc = {
        .tMemory              = PL_MEMORY_GPU,
        .tUsage               = PL_BUFFER_USAGE_STORAGE,
        .uByteSize            = sizeof(plVec4) * pl_sb_size(gptData->sbtVertexDataBuffer)
    };
    memcpy(tStagingBuffer->tMemoryAllocation.pHostMapped, gptData->sbtVertexDataBuffer, sizeof(plVec4) * pl_sb_size(gptData->sbtVertexDataBuffer));
    gptData->tStorageBuffer = gptDevice->create_buffer(ptDevice, &tStorageBufferDesc, "storage buffer");
    gptDevice->copy_buffer(ptDevice, tStagingBufferHandle, gptData->tStorageBuffer, 0, 0, tStorageBufferDesc.uByteSize);
    tStagingBuffer = gptDevice->get_buffer(ptDevice, tStagingBufferHandle);

    plVec4 tUnused = {0};
    const plBufferDescription tUnusedBufferDesc = {
        .tMemory              = PL_MEMORY_GPU,
        .tUsage               = PL_BUFFER_USAGE_UNIFORM,
        .uByteSize            = sizeof(plVec4)
    };
    memcpy(tStagingBuffer->tMemoryAllocation.pHostMapped, &tUnused, sizeof(plVec4));
    gptData->tUnusedShaderBuffer = gptDevice->create_buffer(ptDevice, &tUnusedBufferDesc, "unused shader buffer");
    gptDevice->copy_buffer(ptDevice, tStagingBufferHandle, gptData->tUnusedShaderBuffer, 0, 0, tUnusedBufferDesc.uByteSize);

    const plBufferDescription atGlobalBuffersDesc = {
        .tMemory              = PL_MEMORY_GPU_CPU,
        .tUsage               = PL_BUFFER_USAGE_UNIFORM,
        .uByteSize            = sizeof(BindGroup_0)
    };
    gptData->atGlobalBuffers[0] = gptDevice->create_buffer(ptDevice, &atGlobalBuffersDesc, "global buffer 0");
    gptData->atGlobalBuffers[1] = gptDevice->create_buffer(ptDevice, &atGlobalBuffersDesc, "global buffer 1");

    gptDevice->destroy_buffer(ptDevice, tStagingBufferHandle);
}

static void
pl_refr_run_ecs(void)
{
    pl_begin_profile_sample(__FUNCTION__);
    gptECS->run_transform_update_system(&gptData->tComponentLibrary);
    gptECS->run_hierarchy_update_system(&gptData->tComponentLibrary);
    gptECS->run_skin_update_system(&gptData->tComponentLibrary);
    gptECS->run_object_update_system(&gptData->tComponentLibrary);
    pl_end_profile_sample();
}

inline static bool pl__within(float fMin, float fValue, float fMax)
{
    return fValue >= fMin && fValue <= fMax;
}

static void
pl_refr_cull_draw_stream(plCameraComponent* ptCamera)
{
    // TODO: use separate axis theorem to make this 100% accurate

    pl_begin_profile_sample(__FUNCTION__);

    pl_sb_reset(gptData->sbtVisibleDrawables);

    const plMat4 tMVP = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);

    const uint32_t uDrawableCount = pl_sb_size(gptData->sbtDrawables);
    for(uint32_t uDrawableIndex = 0; uDrawableIndex < uDrawableCount; uDrawableIndex++)
    {
        const plDrawable tDrawable = gptData->sbtDrawables[uDrawableIndex];
        plMeshComponent* ptMesh = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_MESH, tDrawable.tEntity);

        const plVec4 tVerticies[] = {
            {  ptMesh->tAABBFinal.tMin.x, ptMesh->tAABBFinal.tMin.y, ptMesh->tAABBFinal.tMin.z , 1.0f},
            {  ptMesh->tAABBFinal.tMax.x, ptMesh->tAABBFinal.tMin.y, ptMesh->tAABBFinal.tMin.z , 1.0f},
            {  ptMesh->tAABBFinal.tMax.x, ptMesh->tAABBFinal.tMax.y, ptMesh->tAABBFinal.tMin.z , 1.0f},
            {  ptMesh->tAABBFinal.tMin.x, ptMesh->tAABBFinal.tMax.y, ptMesh->tAABBFinal.tMin.z , 1.0f},
            {  ptMesh->tAABBFinal.tMin.x, ptMesh->tAABBFinal.tMin.y, ptMesh->tAABBFinal.tMax.z , 1.0f},
            {  ptMesh->tAABBFinal.tMax.x, ptMesh->tAABBFinal.tMin.y, ptMesh->tAABBFinal.tMax.z , 1.0f},
            {  ptMesh->tAABBFinal.tMax.x, ptMesh->tAABBFinal.tMax.y, ptMesh->tAABBFinal.tMax.z , 1.0f},
            {  ptMesh->tAABBFinal.tMin.x, ptMesh->tAABBFinal.tMax.y, ptMesh->tAABBFinal.tMax.z , 1.0f},
        };

        bool bInside = false;

        for(uint32_t i = 0; i < 8; i++)
        {
            const plVec4 tCorner = pl_mul_mat4_vec4(&tMVP, tVerticies[i]);

            bInside = pl__within(-tCorner.w, tCorner.x, tCorner.w) && pl__within(-tCorner.w, tCorner.y, tCorner.w) && pl__within(0.0f, tCorner.z, tCorner.w);
            if(bInside)
                break;
        }

        if(bInside)
        {
            pl_sb_push(gptData->sbtVisibleDrawables, tDrawable);
        }
    }

    pl_end_profile_sample();
}

static void
pl_refr_submit_draw_stream(plCameraComponent* ptCamera)
{
    pl_begin_profile_sample(__FUNCTION__);

    plGraphics* ptGraphics = &gptData->tGraphics;
    plDevice* ptDevice = &ptGraphics->tDevice;

    plDrawStream* ptStream = &gptData->tDrawStream;

    // update global buffers & bind groups
    const BindGroup_0 tBindGroupBuffer = {
        .tCameraPos            = ptCamera->tPos,
        .tCameraProjection     = ptCamera->tProjMat,
        .tCameraView           = ptCamera->tViewMat,
        .tCameraViewProjection = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat)
    };
    memcpy(ptGraphics->sbtBuffersCold[gptData->atGlobalBuffers[ptGraphics->uCurrentFrameIndex].uIndex].tMemoryAllocation.pHostMapped, &tBindGroupBuffer, sizeof(BindGroup_0));

    plBindGroupLayout tBindGroupLayout0 = {
        .uBufferCount  = 3,
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
            {
                .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                .uSlot = 2,
                .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
            },
        }
    };
    plBindGroupHandle tGlobalBG = gptDevice->get_temporary_bind_group(ptDevice, &tBindGroupLayout0);
    size_t szBufferRangeSize[] = {sizeof(BindGroup_0), sizeof(plVec4) * pl_sb_size(gptData->sbtVertexDataBuffer), sizeof(plMaterial) * pl_sb_size(gptData->sbtMaterialBuffer)};

    plBufferHandle atBindGroup0_buffers0[] = {gptData->atGlobalBuffers[ptGraphics->uCurrentFrameIndex], gptData->tStorageBuffer, gptData->tMaterialDataBuffer};
    gptDevice->update_bind_group(&ptGraphics->tDevice, &tGlobalBG, 3, atBindGroup0_buffers0, szBufferRangeSize, 0, NULL);

    plBindGroupLayout tBindGroupLayout1 = {
        .uBufferCount  = 1,
        .aBuffers = {
            {
                .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                .uSlot = 0,
                .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
            }
        }
    };
    plBindGroupHandle tBG2 = gptDevice->get_temporary_bind_group(ptDevice, &tBindGroupLayout1);
    size_t szBufferRangeSize2[] = {sizeof(plVec4)};
    gptDevice->update_bind_group(&ptGraphics->tDevice, &tBG2, 1, &gptData->tUnusedShaderBuffer, szBufferRangeSize2, 0, NULL);

    gptStream->reset(ptStream);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~skybox~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    if(gptData->tSkyboxTexture.uIndex != UINT32_MAX)
    {

        plDynamicBinding tSkyboxDynamicData = gptDevice->allocate_dynamic_data(ptDevice, sizeof(plMat4));
        plMat4* ptSkyboxDynamicData = (plMat4*)tSkyboxDynamicData.pcData;
        *ptSkyboxDynamicData = pl_mat4_translate_vec3(ptCamera->tPos);

        gptStream->draw(ptStream, (plDraw)
        {
            .uShaderVariant       = gptData->tSkyboxShader.uIndex,
            .uDynamicBuffer       = tSkyboxDynamicData.uBufferHandle,
            .uVertexBuffer        = gptData->tVertexBuffer.uIndex,
            .uIndexBuffer         = gptData->tIndexBuffer.uIndex,
            .uIndexOffset         = gptData->tSkyboxDrawable.uIndexOffset,
            .uTriangleCount       = gptData->tSkyboxDrawable.uIndexCount / 3,
            .uBindGroup0          = tGlobalBG.uIndex,
            .uBindGroup1          = gptData->tSkyboxBindGroup.uIndex,
            .uBindGroup2          = tBG2.uIndex,
            .uDynamicBufferOffset = tSkyboxDynamicData.uByteOffset
        });
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~visible meshes~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    static double* pdVisibleObjects = NULL;
    if(!pdVisibleObjects)
        pdVisibleObjects = gptStats->get_counter("visible objects");
    

    const uint32_t uVisibleDrawCount = pl_sb_size(gptData->sbtVisibleDrawables);
    *pdVisibleObjects = (double)uVisibleDrawCount;
    for(uint32_t i = 0; i < uVisibleDrawCount; i++)
    {
        const plDrawable tDrawable = gptData->sbtVisibleDrawables[i];
        plTransformComponent* ptTransform = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, tDrawable.tEntity);
        
        plDynamicBinding tDynamicBinding = gptDevice->allocate_dynamic_data(ptDevice, sizeof(DynamicData));

        DynamicData* ptDynamicData = (DynamicData*)tDynamicBinding.pcData;
        ptDynamicData->iDataOffset = tDrawable.uDataOffset;
        ptDynamicData->iVertexOffset = tDrawable.uVertexOffset;
        ptDynamicData->tModel = ptTransform->tWorld;
        ptDynamicData->iMaterialOffset = tDrawable.uMaterialIndex;

        gptStream->draw(ptStream, (plDraw)
        {
            .uShaderVariant       = tDrawable.uShader,
            .uDynamicBuffer       = tDynamicBinding.uBufferHandle,
            .uVertexBuffer        = gptData->tVertexBuffer.uIndex,
            .uIndexBuffer         = gptData->tIndexBuffer.uIndex,
            .uIndexOffset         = tDrawable.uIndexOffset,
            .uTriangleCount       = tDrawable.uIndexCount / 3,
            .uBindGroup0          = tGlobalBG.uIndex,
            .uBindGroup1          = tDrawable.tMaterialBindGroup.uIndex,
            .uBindGroup2          = tBG2.uIndex,
            .uDynamicBufferOffset = tDynamicBinding.uByteOffset
        });
    }

    plDrawArea tArea = {
       .ptDrawStream = ptStream,
       .tScissor = {
            .uWidth  = (uint32_t)pl_get_io()->afMainViewportSize[0],
            .uHeight = (uint32_t)pl_get_io()->afMainViewportSize[1],
       },
       .tViewport = {
            .fWidth  = pl_get_io()->afMainViewportSize[0],
            .fHeight = pl_get_io()->afMainViewportSize[1],
            .fMaxDepth = 1.0f
       }
    };
    gptGfx->draw_areas(ptGraphics, 1, &tArea);

    pl_end_profile_sample();
}

static void
pl_refr_draw_bound_boxes(plDrawList3D* ptDrawlist)
{
    pl_begin_profile_sample(__FUNCTION__);

    const uint32_t uDrawableCount = pl_sb_size(gptData->sbtDrawables);
    for(uint32_t i = 0; i < uDrawableCount; i++)
    {
        plMeshComponent* ptMesh = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_MESH, gptData->sbtDrawables[i].tEntity);

        gptGfx->add_3d_aabb(ptDrawlist, ptMesh->tAABBFinal.tMin, ptMesh->tAABBFinal.tMax, (plVec4){1.0f, 0.0f, 0.0f, 1.0f}, 0.02f);
    }

    pl_end_profile_sample();
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_ext(plApiRegistryApiI* ptApiRegistry, bool bReload)
{
   gptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
   pl_set_memory_context(gptDataRegistry->get_data(PL_CONTEXT_MEMORY));
   pl_set_profile_context(gptDataRegistry->get_data("profile"));
   pl_set_log_context(gptDataRegistry->get_data("log"));
   pl_set_context(gptDataRegistry->get_data("ui"));

   // apis
   gptResource = ptApiRegistry->first(PL_API_RESOURCE);
   gptECS      = ptApiRegistry->first(PL_API_ECS);
   gptFile     = ptApiRegistry->first(PL_API_FILE);
   gptDevice   = ptApiRegistry->first(PL_API_DEVICE);
   gptGfx      = ptApiRegistry->first(PL_API_GRAPHICS);
   gptCamera   = ptApiRegistry->first(PL_API_CAMERA);
   gptStream   = ptApiRegistry->first(PL_API_DRAW_STREAM);
   gptImage    = ptApiRegistry->first(PL_API_IMAGE);
   gptStats    = ptApiRegistry->first(PL_API_STATS);

   if(bReload)
   {
      gptData = gptDataRegistry->get_data("ref renderer data");
      ptApiRegistry->replace(ptApiRegistry->first(PL_API_REF_RENDERER), pl_load_ref_renderer_api());
   }
   else
   {
      // allocate renderer data
      gptData = PL_ALLOC(sizeof(plRefRendererData));
      memset(gptData, 0, sizeof(plRefRendererData));

      // register data with registry (for reloads)
      gptDataRegistry->set_data("ref renderer data", gptData);

      // add specific log channel for renderer
      gptData->uLogChannel = pl_add_log_channel("Renderer", PL_CHANNEL_TYPE_BUFFER);

      // register API
      ptApiRegistry->add(PL_API_REF_RENDERER, pl_load_ref_renderer_api());
   }
}

PL_EXPORT void
pl_unload_ext(plApiRegistryApiI* ptApiRegistry)
{
    PL_FREE(gptData);
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#define PL_STL_IMPLEMENTATION
#include "pl_stl.h"
#undef PL_STL_IMPLEMENTATION

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#undef CGLTF_IMPLEMENTATION