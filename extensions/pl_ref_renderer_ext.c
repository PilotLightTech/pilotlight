/*
   pl_ref_renderer_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal structs
// [SECTION] global data & apis
// [SECTION] internal API
// [SECTION] implementation
// [SECTION] public API implementation
// [SECTION] extension loading
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
#include "pl_memory.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"
#include "pl_ui.h"

// extensions
#include "pl_graphics_ext.h"
#include "pl_ecs_ext.h"
#include "pl_resource_ext.h"
#include "pl_image_ext.h"
#include "pl_stats_ext.h"
#include "pl_gpu_allocators_ext.h"
#include "pl_job_ext.h"

#define PL_MAX_VIEWS_PER_SCENE 4

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef int plBlendMode;

enum _plBlendMode
{
    PL_BLEND_MODE_NONE,
    PL_BLEND_MODE_ALPHA,
    PL_BLEND_MODE_ADDITIVE,
    PL_BLEND_MODE_PREMULTIPLY,
    PL_BLEND_MODE_MULTIPLY,
    PL_BLEND_MODE_CLIP_MASK,
    
    PL_BLEND_MODE_COUNT
};

typedef struct _plShaderVariant
{
    plGraphicsState tGraphicsState;
    const void*     pTempConstantData;
} plShaderVariant;

typedef struct _plComputeShaderVariant
{
    const void* pTempConstantData;
} plComputeShaderVariant;

typedef struct _plOBB
{
    plVec3 tCenter;
    plVec3 tExtents;
    plVec3 atAxes[3]; // Orthonormal basis
} plOBB;

typedef struct _plSkinData
{
    plEntity            tEntity;
    plTextureHandle     atDynamicTexture[PL_FRAMES_IN_FLIGHT];
    plBindGroupHandle   tTempBindGroup;
} plSkinData;

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
    uint32_t uSkinIndex;
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

typedef struct _plRefView
{
    // plRenderPassHandle       tDrawingRenderPass;
    plRenderPassHandle       tRenderPass;
    plVec2                   tTargetSize;
    plTextureHandle          tTexture[PL_FRAMES_IN_FLIGHT];
    plTextureHandle          tAlbedoTexture[PL_FRAMES_IN_FLIGHT];
    plTextureHandle          tPositionTexture[PL_FRAMES_IN_FLIGHT];
    plTextureHandle          tNormalTexture[PL_FRAMES_IN_FLIGHT];
    plTextureHandle          tDepthTexture[PL_FRAMES_IN_FLIGHT];
    plTextureId              tTextureID[PL_FRAMES_IN_FLIGHT];

    // lighting
    plBindGroupHandle tLightingBindGroup[PL_FRAMES_IN_FLIGHT];

    // GPU buffers
    plBufferHandle atGlobalBuffers[PL_FRAMES_IN_FLIGHT];

    // misc
    plDrawable* sbtVisibleDrawables;

    // drawing api
    plDrawList3D t3DDrawList;
} plRefView;

typedef struct _plSceneLoader
{
    plComponentLibrary tComponentLibrary;
    plDrawable*        sbtOpaqueDrawables;
    plDrawable*        sbtTransparentDrawables;
} plSceneLoader;

typedef struct _plRefScene
{

    // plRenderPassLayoutHandle tDrawingRenderPassLayout;
    plRenderPassLayoutHandle tRenderPassLayout;

    // shader templates
    plShaderHandle           tShader;
    uint32_t                 uVariantCount;
    const plShaderVariant*   ptVariants;
    plHashMap                tVariantHashmap;
    plShaderHandle*          _sbtVariantHandles; // needed for cleanup

    // lighting
    plDrawable tLightingDrawable;

    // skybox
    plDrawable          tSkyboxDrawable;
    plTextureHandle     tSkyboxTexture;
    plBindGroupHandle   tSkyboxBindGroup;

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

    // misc
    plDrawable* sbtSubmittedDrawables;

    uint32_t  uViewCount;
    plRefView atViews[PL_MAX_VIEWS_PER_SCENE];
    plSkinData* sbtSkinData;
    plSceneLoader tLoaderData;

} plRefScene;

typedef struct _plRefRendererData
{
    uint32_t uLogChannel;

    plGraphics tGraphics;

    // allocators
    plDeviceMemoryAllocatorI* ptLocalDedicatedAllocator;
    plDeviceMemoryAllocatorI* ptLocalBuddyAllocator;
    plDeviceMemoryAllocatorI* ptStagingUnCachedAllocator;

    // misc textures
    plSamplerHandle tDefaultSampler;
    plTextureHandle tDummyTexture;

    // shaders
    plShaderHandle tSkyboxShader;
    plShaderHandle tLightingShader;

    // compute shaders
    plComputeShaderHandle          tPanoramaShader;
    const plComputeShaderVariant*  ptVariants;
    uint32_t                       uVariantCount;

    // offscreen
    plRefScene* sbtScenes;

    // draw stream
    plDrawStream tDrawStream;

    // gltf data
    plBindGroupHandle tNullSkinBindgroup;

    // temp
    plBufferHandle tStagingBufferHandle[PL_FRAMES_IN_FLIGHT];
} plRefRendererData;

//-----------------------------------------------------------------------------
// [SECTION] global data & apis
//-----------------------------------------------------------------------------

// context data
static plRefRendererData* gptData = NULL;

// apis
static const plDataRegistryI*  gptDataRegistry  = NULL;
static const plResourceI*      gptResource      = NULL;
static const plEcsI*           gptECS           = NULL;
static const plFileI*          gptFile          = NULL;
static const plDeviceI*        gptDevice        = NULL;
static const plGraphicsI*      gptGfx           = NULL;
static const plCameraI*        gptCamera        = NULL;
static const plDrawStreamI*    gptStream        = NULL;
static const plImageI*         gptImage         = NULL;
static const plStatsI*         gptStats         = NULL;
static const plGPUAllocatorsI* gptGpuAllocators = NULL;
static const plThreadsI*       gptThreads       = NULL;
static const plJobI*           gptJob           = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal API
//-----------------------------------------------------------------------------

// internal general helpers
static void pl__add_drawable_data_to_global_buffer(plRefScene* ptScene, uint32_t uDrawableIndex);

// internal gltf helpers

static bool pl__sat_visibility_test(plCameraComponent* ptCamera, const plAABB* aabb);

// shader variants
static plShaderHandle pl__get_shader_variant(uint32_t uSceneHandle, plShaderHandle tHandle, const plShaderVariant* ptVariant);
static size_t pl__get_data_type_size(plDataType tType);
static plBlendState pl__get_blend_state(plBlendMode tBlendMode);

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

static void
pl_refr_initialize(plWindow* ptWindow)
{

    // shader default values
    gptData->tSkyboxShader    = (plShaderHandle){UINT32_MAX, UINT32_MAX};
    gptData->tLightingShader  = (plShaderHandle){UINT32_MAX, UINT32_MAX};

    // compute shader default values
    gptData->tPanoramaShader = (plComputeShaderHandle){UINT32_MAX, UINT32_MAX};

    // misc textures
    gptData->tDummyTexture     = (plTextureHandle){UINT32_MAX, UINT32_MAX};
    gptData->tDefaultSampler   = (plSamplerHandle){UINT32_MAX, UINT32_MAX};

    gptData->tNullSkinBindgroup = (plBindGroupHandle){UINT32_MAX, UINT32_MAX};

    // for convience
    plGraphics* ptGraphics = &gptData->tGraphics;

    // load allocators
    gptData->ptLocalBuddyAllocator = gptGpuAllocators->create_local_buddy_allocator(&ptGraphics->tDevice);
    gptData->ptLocalDedicatedAllocator = gptGpuAllocators->create_local_dedicated_allocator(&ptGraphics->tDevice);
    gptData->ptStagingUnCachedAllocator = gptGpuAllocators->create_staging_uncached_allocator(&ptGraphics->tDevice);

    // initialize graphics
    ptGraphics->bValidationActive = true;
    gptGfx->initialize(ptWindow, ptGraphics);
    gptDataRegistry->set_data("device", &ptGraphics->tDevice); // used by debug extension

    // create main render pass
    plIO* ptIO = pl_get_io();

    // create staging buffer
    const plBufferDescription tStagingBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_STAGING,
        .uByteSize = 268435456
    };
    for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
    {
        gptData->tStagingBufferHandle[i] = gptDevice->create_buffer(&ptGraphics->tDevice, &tStagingBufferDesc, "staging buffer");
        plBuffer* ptBuffer = gptDevice->get_buffer(&ptGraphics->tDevice, gptData->tStagingBufferHandle[i]);
        plDeviceMemoryAllocation tAllocation = gptData->ptStagingUnCachedAllocator->allocate(gptData->ptStagingUnCachedAllocator->ptInst, ptBuffer->tMemoryRequirements.uMemoryTypeBits, ptBuffer->tMemoryRequirements.ulSize, ptBuffer->tMemoryRequirements.ulAlignment, "staging buffer");
        gptDevice->bind_buffer_to_memory(&ptGraphics->tDevice, gptData->tStagingBufferHandle[i], &tAllocation);
    }
    plBuffer* ptStagingBuffer = gptDevice->get_buffer(&ptGraphics->tDevice, gptData->tStagingBufferHandle[0]);

    // create dummy texture
    plTextureDesc tTextureDesc = {
        .tDimensions   = {2, 2, 1},
        .tFormat       = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED,
    };
    gptData->tDummyTexture = gptDevice->create_texture(&ptGraphics->tDevice, &tTextureDesc, "dummy texture");
    {
        plTexture* ptTexture = gptDevice->get_texture(&ptGraphics->tDevice, gptData->tDummyTexture);
        plDeviceMemoryAllocation tAllocation = gptData->ptLocalBuddyAllocator->allocate(gptData->ptLocalBuddyAllocator->ptInst, ptTexture->tMemoryRequirements.uMemoryTypeBits, ptTexture->tMemoryRequirements.ulSize, ptTexture->tMemoryRequirements.ulAlignment, "dummy texture");
        gptDevice->bind_texture_to_memory(&ptGraphics->tDevice, gptData->tDummyTexture, &tAllocation);
    }

    // copy data to dummy texture
    static float image[] = {
        1.0f,   0,   0, 1.0f,
        0, 1.0f,   0, 1.0f,
        0,   0, 1.0f, 1.0f,
        1.0f,   0, 1.0f, 1.0f
    };
    memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, image, sizeof(float) * 4 * 4);
    
    plBufferImageCopy tBufferImageCopy = {
        .tImageExtent = {2, 2, 1},
        .uLayerCount  = 1
    };
    plCommandBuffer tCommandBuffer = gptGfx->begin_command_recording(ptGraphics, NULL);
    plBlitEncoder tBlitEncoder = gptGfx->begin_blit_pass(ptGraphics, &tCommandBuffer);
    gptGfx->copy_buffer_to_texture(&tBlitEncoder, gptData->tStagingBufferHandle[0], gptData->tDummyTexture, 1, &tBufferImageCopy);
    gptGfx->end_blit_pass(&tBlitEncoder);
    gptGfx->end_command_recording(ptGraphics, &tCommandBuffer);
    gptGfx->submit_command_buffer(ptGraphics, &tCommandBuffer, NULL);

    plSamplerDesc tSamplerDesc = {
        .tFilter         = PL_FILTER_LINEAR,
        .fMinMip         = 0.0f,
        .fMaxMip         = 1.0f,
        .tVerticalWrap   = PL_WRAP_MODE_WRAP,
        .tHorizontalWrap = PL_WRAP_MODE_WRAP
    };
    gptData->tDefaultSampler = gptDevice->create_sampler(&ptGraphics->tDevice, &tSamplerDesc, "default sampler");

    // create null skin bind group (to be bound when skinning isn't enabled)
    plBindGroupLayout tBindGroupLayout1 = {
        .uTextureCount  = 1,
        .aTextures = {{.uSlot =  0, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}}
    };
    gptData->tNullSkinBindgroup = gptDevice->create_bind_group(&ptGraphics->tDevice, &tBindGroupLayout1, "null skin bind group");
    plBindGroupUpdateData tBGData = {
        .atTextureViews = &gptData->tDummyTexture
    };
    gptDevice->update_bind_group(&ptGraphics->tDevice, &gptData->tNullSkinBindgroup, &tBGData);
}

static uint32_t
pl_refr_create_scene(void)
{
    const uint32_t uSceneHandle = pl_sb_size(gptData->sbtScenes);
    plRefScene tScene = {0};
    pl_sb_push(gptData->sbtScenes, tScene);
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];

    // initialize ecs
    gptECS->init_component_library(&ptScene->tLoaderData.tComponentLibrary);

    // shaders default valies
    ptScene->tShader = (plShaderHandle){UINT32_MAX, UINT32_MAX};

    // buffer default values
    ptScene->tVertexBuffer         = (plBufferHandle){UINT32_MAX, UINT32_MAX};
    ptScene->tIndexBuffer          = (plBufferHandle){UINT32_MAX, UINT32_MAX};
    ptScene->tStorageBuffer        = (plBufferHandle){UINT32_MAX, UINT32_MAX};
    ptScene->tMaterialDataBuffer   = (plBufferHandle){UINT32_MAX, UINT32_MAX};

    // skybox resources default values
    ptScene->tSkyboxTexture     = (plTextureHandle){UINT32_MAX, UINT32_MAX};
    ptScene->tSkyboxBindGroup   = (plBindGroupHandle){UINT32_MAX, UINT32_MAX};

    // create offscreen render pass layout
    const plRenderPassLayoutDescription tRenderPassLayoutDesc = {
        .atRenderTargets = {
            { .tFormat = PL_FORMAT_D32_FLOAT_S8_UINT },  // depth buffer
            { .tFormat = PL_FORMAT_R32G32B32A32_FLOAT }, // final output
            { .tFormat = PL_FORMAT_R32G32B32A32_FLOAT }, // albedo
            { .tFormat = PL_FORMAT_R32G32B32A32_FLOAT }, // normal
            { .tFormat = PL_FORMAT_R32G32B32A32_FLOAT }, // position
        },
        .uSubpassCount = 3,
        .atSubpasses = {
            
            // G-buffer fill
            {
                .uRenderTargetCount = 4,
                .auRenderTargets = {0, 2, 3, 4}
            },

            // lighting
            {
                .uRenderTargetCount = 1,
                .auRenderTargets = {1},
                .uSubpassInputCount = 4,
                .auSubpassInputs = {0, 2, 3, 4},
            },

            // skybox
            {
                .uRenderTargetCount = 2,
                .auRenderTargets = {0, 1},
                .uSubpassInputCount = 0
            },
        }
    };
    ptScene->tRenderPassLayout = gptDevice->create_render_pass_layout(&gptData->tGraphics.tDevice, &tRenderPassLayoutDesc);

    // create template shader
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
            .ulDepthMode          = PL_COMPARE_MODE_LESS_OR_EQUAL,
            .ulCullMode           = PL_CULL_MODE_CULL_BACK,
            .ulWireframe          = 0,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .tVertexBufferBinding = {
            .uByteStride = sizeof(float) * 3,
            .atAttributes = { {.uByteOffset = 0, .tFormat = PL_FORMAT_R32G32B32_FLOAT}}
        },
        .uConstantCount = 5,
        .atBlendStates = {
            pl__get_blend_state(PL_BLEND_MODE_ALPHA),
            pl__get_blend_state(PL_BLEND_MODE_NONE),
            pl__get_blend_state(PL_BLEND_MODE_NONE)
        },
        .uBlendStateCount = 3,
        .tRenderPassLayout = ptScene->tRenderPassLayout,
        .uSubpassIndex = 0,
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
                },
                .uSamplerCount = 1,
                .atSamplers = { {.uSlot = 3, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}}
            },
            {
                .uTextureCount  = 2,
                .aTextures = {
                    {.uSlot =  0, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
                    {.uSlot =  1, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
                }
            },
            {
                .uTextureCount  = 1,
                .aTextures = {
                    {.uSlot =  0, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
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

    int aiConstantData[5] = {0};
    aiConstantData[2] = 0;
    aiConstantData[3] = 0;
    aiConstantData[4] = 0;
    
    aiConstantData[0] = (int)PL_MESH_FORMAT_FLAG_HAS_NORMAL;
    int iFlagCopy = (int)PL_MESH_FORMAT_FLAG_HAS_NORMAL;
    while(iFlagCopy)
    {
        aiConstantData[1] += iFlagCopy & 1;
        iFlagCopy >>= 1;
    }
    tShaderDescription.pTempConstantData = aiConstantData;
    ptScene->tShader = gptDevice->create_shader(&gptData->tGraphics.tDevice, &tShaderDescription);

    return uSceneHandle;
}

static uint32_t
pl_refr_create_view(uint32_t uSceneHandle, plVec2 tDimensions)
{

    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    const uint32_t uViewHandle = ptScene->uViewCount++;
    PL_ASSERT(uViewHandle < PL_MAX_VIEWS_PER_SCENE);
    plRefView* ptView = &ptScene->atViews[uViewHandle];

    ptView->tTargetSize = tDimensions;

    // buffer default values
    for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
        ptView->atGlobalBuffers[i]    = (plBufferHandle){UINT32_MAX, UINT32_MAX};

    // for convience
    plGraphics* ptGraphics = &gptData->tGraphics;

    // create skybox shader
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
            .ulDepthMode          = PL_COMPARE_MODE_LESS_OR_EQUAL,
            .ulCullMode           = PL_CULL_MODE_NONE,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .tVertexBufferBinding = {
            .uByteStride = sizeof(float) * 3,
            .atAttributes = { {.uByteOffset = 0, .tFormat = PL_FORMAT_R32G32B32_FLOAT}}
        },
        .atBlendStates = {
            pl__get_blend_state(PL_BLEND_MODE_ALPHA)
        },
        .uBlendStateCount = 1,
        .tRenderPassLayout = ptScene->tRenderPassLayout,
        .uSubpassIndex = 2,
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
                },
                .uSamplerCount = 1,
                .atSamplers = { {.uSlot = 3, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}}
            },
            {
                .uTextureCount = 1,
                .aTextures = {
                    {
                        .uSlot = 0,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL,
                        .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED
                    }
                 },
            },
            {
                .uTextureCount = 1,
                .aTextures = {
                    {
                        .uSlot = 0,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL,
                        .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED
                    }
                 },
            }
        }
    };
    gptData->tSkyboxShader = gptDevice->create_shader(&ptGraphics->tDevice, &tSkyboxShaderDesc);

    // create lighting shader
    plShaderDescription tLightingShaderDesc = {
#ifdef PL_METAL_BACKEND
        .pcVertexShader = "../shaders/metal/lighting.metal",
        .pcPixelShader = "../shaders/metal/lighting.metal",
        .pcVertexShaderEntryFunc = "vertex_main",
        .pcPixelShaderEntryFunc = "fragment_main",
#else
        .pcVertexShader = "lighting.vert.spv",
        .pcPixelShader = "lighting.frag.spv",
        .pcVertexShaderEntryFunc = "main",
        .pcPixelShaderEntryFunc = "main",
#endif

        .tGraphicsState = {
            .ulDepthWriteEnabled  = 0,
            .ulDepthMode          = PL_COMPARE_MODE_ALWAYS,
            .ulCullMode           = PL_CULL_MODE_NONE,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .tVertexBufferBinding = {
            .uByteStride = 12,
            .atAttributes = { {.uByteOffset = 0, .tFormat = PL_FORMAT_R32G32B32_FLOAT}}
        },
        .atBlendStates = {
            pl__get_blend_state(PL_BLEND_MODE_NONE)
        },
        .uBlendStateCount = 1,
        .uSubpassIndex = 1,
        .tRenderPassLayout = ptScene->tRenderPassLayout,
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
                },
                .uSamplerCount = 1,
                .atSamplers = { {.uSlot = 3, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}}
            },
            {
                .uTextureCount = 4,
                .aTextures = {
                    { .uSlot = 0, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
                    { .uSlot = 1, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
                    { .uSlot = 2, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
                    { .uSlot = 3, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT}
                 },
            },
            {
                .uTextureCount = 1,
                .aTextures = {
                    { .uSlot = 0, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
                 },
            }
        }
    };
    gptData->tLightingShader = gptDevice->create_shader(&ptGraphics->tDevice, &tLightingShaderDesc);

    // create offscreen color & depth textures
    const plTextureDesc tTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .tInitialUsage = PL_TEXTURE_USAGE_SAMPLED
    };
    const plTextureDesc tTextureDesc2 = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .tInitialUsage = PL_TEXTURE_USAGE_COLOR_ATTACHMENT
    };
    for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
    {
        ptView->tTexture[i] = gptDevice->create_texture(&ptGraphics->tDevice, &tTextureDesc, "offscreen texture original");
        ptView->tAlbedoTexture[i] = gptDevice->create_texture(&ptGraphics->tDevice, &tTextureDesc2, "albedo texture original");
        ptView->tNormalTexture[i] = gptDevice->create_texture(&ptGraphics->tDevice, &tTextureDesc2, "normal texture original");
        ptView->tPositionTexture[i] = gptDevice->create_texture(&ptGraphics->tDevice, &tTextureDesc2, "position texture original");

        plTexture* ptTexture0 = gptDevice->get_texture(&ptGraphics->tDevice, ptView->tTexture[i]);
        plDeviceMemoryAllocatorI* ptAllocator = gptData->ptLocalBuddyAllocator;
        if(ptTexture0->tMemoryRequirements.ulSize > PL_DEVICE_BUDDY_BLOCK_SIZE)
            ptAllocator = gptData->ptLocalDedicatedAllocator;
        plDeviceMemoryAllocation tAllocation0 = ptAllocator->allocate(ptAllocator->ptInst, ptTexture0->tMemoryRequirements.uMemoryTypeBits, ptTexture0->tMemoryRequirements.ulSize, ptTexture0->tMemoryRequirements.ulAlignment, "offscreen texture original");
        plDeviceMemoryAllocation tAllocation1 = ptAllocator->allocate(ptAllocator->ptInst, ptTexture0->tMemoryRequirements.uMemoryTypeBits, ptTexture0->tMemoryRequirements.ulSize, ptTexture0->tMemoryRequirements.ulAlignment, "albedo texture original");
        plDeviceMemoryAllocation tAllocation2 = ptAllocator->allocate(ptAllocator->ptInst, ptTexture0->tMemoryRequirements.uMemoryTypeBits, ptTexture0->tMemoryRequirements.ulSize, ptTexture0->tMemoryRequirements.ulAlignment, "normal texture original");
        plDeviceMemoryAllocation tAllocation3 = ptAllocator->allocate(ptAllocator->ptInst, ptTexture0->tMemoryRequirements.uMemoryTypeBits, ptTexture0->tMemoryRequirements.ulSize, ptTexture0->tMemoryRequirements.ulAlignment, "position texture original");
        gptDevice->bind_texture_to_memory(&ptGraphics->tDevice, ptView->tTexture[i], &tAllocation0);
        gptDevice->bind_texture_to_memory(&ptGraphics->tDevice, ptView->tAlbedoTexture[i], &tAllocation1);
        gptDevice->bind_texture_to_memory(&ptGraphics->tDevice, ptView->tNormalTexture[i], &tAllocation2);
        gptDevice->bind_texture_to_memory(&ptGraphics->tDevice, ptView->tPositionTexture[i], &tAllocation3);

    }

    const plTextureDesc tDepthTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_D32_FLOAT_S8_UINT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT | PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .tInitialUsage = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT
    };
    for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
    {
        ptView->tDepthTexture[i] = gptDevice->create_texture(&ptGraphics->tDevice, &tDepthTextureDesc, "offscreen depth texture original");
        plTexture* ptTexture0 = gptDevice->get_texture(&ptGraphics->tDevice, ptView->tDepthTexture[i]);
        plDeviceMemoryAllocatorI* ptAllocator = gptData->ptLocalBuddyAllocator;
        if(ptTexture0->tMemoryRequirements.ulSize > PL_DEVICE_BUDDY_BLOCK_SIZE)
            ptAllocator = gptData->ptLocalDedicatedAllocator;
        plDeviceMemoryAllocation tAllocation0 = ptAllocator->allocate(ptAllocator->ptInst, ptTexture0->tMemoryRequirements.uMemoryTypeBits, ptTexture0->tMemoryRequirements.ulSize, ptTexture0->tMemoryRequirements.ulAlignment, "offscreen depth texture original");
        gptDevice->bind_texture_to_memory(&ptGraphics->tDevice, ptView->tDepthTexture[i], &tAllocation0);
    }

    for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
    {
        ptView->tTextureID[i] = gptGfx->get_ui_texture_handle(ptGraphics, ptView->tTexture[i], gptData->tDefaultSampler);
    }

    plBindGroupLayout tLightingBindGroupLayout = {
        .uTextureCount  = 4,
        .aTextures = { 
            {.uSlot = 0, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 1, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 2, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 3, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
        }
    };

    plTempAllocator tTempAllocator = {0};
    for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
    {
        ptView->tLightingBindGroup[i] = gptDevice->create_bind_group(&ptGraphics->tDevice, &tLightingBindGroupLayout, pl_temp_allocator_sprintf(&tTempAllocator, "lighting bind group%u", i));
        plTextureHandle atLightingTextureViews[] = {ptView->tAlbedoTexture[i], ptView->tNormalTexture[i], ptView->tPositionTexture[i], ptView->tDepthTexture[i]};
        plBindGroupUpdateData tBGData = {
            .atTextureViews = atLightingTextureViews
        };
        gptDevice->update_bind_group(&ptGraphics->tDevice, &ptView->tLightingBindGroup[i], &tBGData);
    }
    pl_temp_allocator_free(&tTempAllocator);

    // create offscreen render pass
    plRenderPassAttachments atAttachmentSets[PL_FRAMES_IN_FLIGHT] = {0};

    for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
    {
        atAttachmentSets[i].atViewAttachments[0] = ptView->tDepthTexture[i];
        atAttachmentSets[i].atViewAttachments[1] = ptView->tTexture[i];
        atAttachmentSets[i].atViewAttachments[2] = ptView->tAlbedoTexture[i];
        atAttachmentSets[i].atViewAttachments[3] = ptView->tNormalTexture[i];
        atAttachmentSets[i].atViewAttachments[4] = ptView->tPositionTexture[i];
    }

    const plRenderPassDescription tRenderPassDesc = {
        .tLayout = ptScene->tRenderPassLayout,
        .tDepthTarget = {
                .tLoadOp         = PL_LOAD_OP_CLEAR,
                .tStoreOp        = PL_STORE_OP_STORE,
                .tStencilLoadOp  = PL_LOAD_OP_CLEAR,
                .tStencilStoreOp = PL_STORE_OP_DONT_CARE,
                .tCurrentUsage   = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
                .tNextUsage      = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
                .fClearZ         = 1.0f
        },
        .atColorTargets = {
            {
                .tLoadOp       = PL_LOAD_OP_CLEAR,
                .tStoreOp      = PL_STORE_OP_STORE,
                .tCurrentUsage = PL_TEXTURE_USAGE_SAMPLED,
                .tNextUsage    = PL_TEXTURE_USAGE_SAMPLED,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
            },
            {
                .tLoadOp       = PL_LOAD_OP_CLEAR,
                .tStoreOp      = PL_STORE_OP_STORE,
                .tCurrentUsage = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .tNextUsage    = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
            },
            {
                .tLoadOp       = PL_LOAD_OP_CLEAR,
                .tStoreOp      = PL_STORE_OP_STORE,
                .tCurrentUsage = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .tNextUsage    = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
            },
            {
                .tLoadOp       = PL_LOAD_OP_CLEAR,
                .tStoreOp      = PL_STORE_OP_STORE,
                .tCurrentUsage = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .tNextUsage    = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
            }
        },
        .tDimensions = {.x = ptView->tTargetSize.x, .y = ptView->tTargetSize.y}
    };
    ptView->tRenderPass = gptDevice->create_render_pass(&ptGraphics->tDevice, &tRenderPassDesc, atAttachmentSets);

    // register debug 3D drawlist
    gptGfx->register_3d_drawlist(ptGraphics, &ptView->t3DDrawList);

    const plBufferDescription atGlobalBuffersDesc = {
        .tUsage               = PL_BUFFER_USAGE_UNIFORM | PL_BUFFER_USAGE_STAGING,
        .uByteSize            = PL_DEVICE_ALLOCATION_BLOCK_SIZE
    };
    for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
    {
        ptView->atGlobalBuffers[i] = gptDevice->create_buffer(&ptGraphics->tDevice, &atGlobalBuffersDesc, "global buffer");
        plBuffer* ptBuffer = gptDevice->get_buffer(&ptGraphics->tDevice, ptView->atGlobalBuffers[i]);
        plDeviceMemoryAllocation tAllocation = gptData->ptStagingUnCachedAllocator->allocate(gptData->ptStagingUnCachedAllocator->ptInst, ptBuffer->tMemoryRequirements.uMemoryTypeBits, ptBuffer->tMemoryRequirements.ulSize, ptBuffer->tMemoryRequirements.ulAlignment, "global buffer");
        gptDevice->bind_buffer_to_memory(&ptGraphics->tDevice, ptView->atGlobalBuffers[i], &tAllocation);
    }

    const uint32_t uStartIndex2     = pl_sb_size(ptScene->sbtVertexPosBuffer);
    const uint32_t uIndexStart2     = pl_sb_size(ptScene->sbuIndexBuffer);
    const uint32_t uDataStartIndex2 = pl_sb_size(ptScene->sbtVertexDataBuffer);

    const plDrawable tDrawable2 = {
        .uIndexCount   = 6,
        .uVertexCount  = 4,
        .uIndexOffset  = uIndexStart2,
        .uVertexOffset = uStartIndex2,
        .uDataOffset   = uDataStartIndex2,
    };
    ptScene->tLightingDrawable = tDrawable2;

    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex2 + 0);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex2 + 1);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex2 + 2);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex2 + 0);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex2 + 2);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex2 + 3);

    pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){-1.0f, -1.0f}));
    pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){-1.0f,  1.0f}));
    pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){1.0f,  1.0f}));
    pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){1.0f,  -1.0f}));

    pl_sb_push(ptScene->sbtVertexDataBuffer, ((plVec4){ 0.0f, 0.0f})); 
    pl_sb_push(ptScene->sbtVertexDataBuffer, ((plVec4){ 0.0f, 1.0f})); 
    pl_sb_push(ptScene->sbtVertexDataBuffer, ((plVec4){ 1.0f, 1.0f})); 
    pl_sb_push(ptScene->sbtVertexDataBuffer, ((plVec4){ 1.0f, 0.0f})); 

    return uViewHandle;
}

static void
pl_refr_resize_view(uint32_t uSceneHandle, uint32_t uViewHandle, plVec2 tDimensions)
{
    // for convience
    plGraphics* ptGraphics = &gptData->tGraphics;
    plDevice* ptDevice = &ptGraphics->tDevice;
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plRefView* ptView = &ptScene->atViews[uViewHandle];

    // update offscreen size to match viewport
    ptView->tTargetSize = tDimensions;

    // queue old textures & texture views for deletion
    for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
    {
        gptDevice->queue_texture_for_deletion(ptDevice, ptView->tTexture[i]);
        gptDevice->queue_texture_for_deletion(ptDevice, ptView->tAlbedoTexture[i]);
        gptDevice->queue_texture_for_deletion(ptDevice, ptView->tNormalTexture[i]);
        gptDevice->queue_texture_for_deletion(ptDevice, ptView->tPositionTexture[i]);
        gptDevice->queue_texture_for_deletion(ptDevice, ptView->tDepthTexture[i]);
        gptDevice->queue_bind_group_for_deletion(ptDevice, ptView->tLightingBindGroup[i]);
    }

    // recreate offscreen color & depth textures
    const plTextureDesc tOffscreenTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .tInitialUsage = PL_TEXTURE_USAGE_SAMPLED
    };

    const plTextureDesc tOffscreenTextureDesc2 = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .tInitialUsage = PL_TEXTURE_USAGE_COLOR_ATTACHMENT
    };

    plTempAllocator tTempAllocator = {0};
    for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
    {
        ptView->tTexture[i] = gptDevice->create_texture(&ptGraphics->tDevice, &tOffscreenTextureDesc, pl_temp_allocator_sprintf(&tTempAllocator, "offscreen texture %u", i));
        ptView->tAlbedoTexture[i] = gptDevice->create_texture(&ptGraphics->tDevice, &tOffscreenTextureDesc2, pl_temp_allocator_sprintf(&tTempAllocator, "albedo texture %u", i));
        ptView->tNormalTexture[i] = gptDevice->create_texture(&ptGraphics->tDevice, &tOffscreenTextureDesc2, pl_temp_allocator_sprintf(&tTempAllocator, "normal texture %u", i));
        ptView->tPositionTexture[i] = gptDevice->create_texture(&ptGraphics->tDevice, &tOffscreenTextureDesc2, pl_temp_allocator_sprintf(&tTempAllocator, "position texture %u", i));

        plTexture* ptTexture0 = gptDevice->get_texture(&ptGraphics->tDevice, ptView->tTexture[i]);
        plDeviceMemoryAllocatorI* ptAllocator = gptData->ptLocalBuddyAllocator;
        if(ptTexture0->tMemoryRequirements.ulSize > PL_DEVICE_BUDDY_BLOCK_SIZE)
            ptAllocator = gptData->ptLocalDedicatedAllocator;
        plDeviceMemoryAllocation tAllocation0 = ptAllocator->allocate(ptAllocator->ptInst, ptTexture0->tMemoryRequirements.uMemoryTypeBits, ptTexture0->tMemoryRequirements.ulSize, ptTexture0->tMemoryRequirements.ulAlignment, "offscreen texture");
        plDeviceMemoryAllocation tAllocation1 = ptAllocator->allocate(ptAllocator->ptInst, ptTexture0->tMemoryRequirements.uMemoryTypeBits, ptTexture0->tMemoryRequirements.ulSize, ptTexture0->tMemoryRequirements.ulAlignment, "albedo texture");
        plDeviceMemoryAllocation tAllocation2 = ptAllocator->allocate(ptAllocator->ptInst, ptTexture0->tMemoryRequirements.uMemoryTypeBits, ptTexture0->tMemoryRequirements.ulSize, ptTexture0->tMemoryRequirements.ulAlignment, "normal texture");
        plDeviceMemoryAllocation tAllocation3 = ptAllocator->allocate(ptAllocator->ptInst, ptTexture0->tMemoryRequirements.uMemoryTypeBits, ptTexture0->tMemoryRequirements.ulSize, ptTexture0->tMemoryRequirements.ulAlignment, "position texture");
        gptDevice->bind_texture_to_memory(&ptGraphics->tDevice, ptView->tTexture[i], &tAllocation0);
        gptDevice->bind_texture_to_memory(&ptGraphics->tDevice, ptView->tAlbedoTexture[i], &tAllocation1);
        gptDevice->bind_texture_to_memory(&ptGraphics->tDevice, ptView->tNormalTexture[i], &tAllocation2);
        gptDevice->bind_texture_to_memory(&ptGraphics->tDevice, ptView->tPositionTexture[i], &tAllocation3);
    }
    pl_temp_allocator_free(&tTempAllocator);

    const plTextureDesc tOffscreenDepthTextureDesc = {
        .tDimensions = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat     = PL_FORMAT_D32_FLOAT_S8_UINT,
        .uLayers     = 1,
        .uMips       = 1,
        .tType       = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT | PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .tInitialUsage = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT
    };
    for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
    {
        ptView->tDepthTexture[i] = gptDevice->create_texture(&ptGraphics->tDevice, &tOffscreenDepthTextureDesc, "offscreen depth texture");
        plTexture* ptTexture0 = gptDevice->get_texture(&ptGraphics->tDevice, ptView->tDepthTexture[i]);
        plDeviceMemoryAllocatorI* ptAllocator = gptData->ptLocalBuddyAllocator;
        if(ptTexture0->tMemoryRequirements.ulSize > PL_DEVICE_BUDDY_BLOCK_SIZE)
            ptAllocator = gptData->ptLocalDedicatedAllocator;
        plDeviceMemoryAllocation tAllocation0 = ptAllocator->allocate(ptAllocator->ptInst, ptTexture0->tMemoryRequirements.uMemoryTypeBits, ptTexture0->tMemoryRequirements.ulSize, ptTexture0->tMemoryRequirements.ulAlignment, "offscreen depth texture");
        gptDevice->bind_texture_to_memory(&ptGraphics->tDevice, ptView->tDepthTexture[i], &tAllocation0);
    }

    for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
    {
        ptView->tTextureID[i] = gptGfx->get_ui_texture_handle(ptGraphics, ptView->tTexture[i], gptData->tDefaultSampler);
    }

    plBindGroupLayout tLightingBindGroupLayout = {
        .uTextureCount  = 4,
        .aTextures = { 
            {.uSlot = 0, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 1, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 2, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 3, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
        }
    };

    for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
    {
        ptView->tLightingBindGroup[i] = gptDevice->create_bind_group(&ptGraphics->tDevice, &tLightingBindGroupLayout, pl_temp_allocator_sprintf(&tTempAllocator, "lighting bind group%u", i));
        plTextureHandle atLightingTextureViews[] = {ptView->tAlbedoTexture[i], ptView->tNormalTexture[i], ptView->tPositionTexture[i], ptView->tDepthTexture[i]};
        plBindGroupUpdateData tBGData = {
            .atTextureViews = atLightingTextureViews
        };
        gptDevice->update_bind_group(&ptGraphics->tDevice, &ptView->tLightingBindGroup[i], &tBGData);
    }
    pl_temp_allocator_free(&tTempAllocator);

    // update offscreen render pass attachments
    plRenderPassAttachments atAttachmentSets[PL_FRAMES_IN_FLIGHT] = {0};

    for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
    {
        atAttachmentSets[i].atViewAttachments[0] = ptView->tDepthTexture[i];
        atAttachmentSets[i].atViewAttachments[1] = ptView->tTexture[i];
        atAttachmentSets[i].atViewAttachments[2] = ptView->tAlbedoTexture[i];
        atAttachmentSets[i].atViewAttachments[3] = ptView->tNormalTexture[i];
        atAttachmentSets[i].atViewAttachments[4] = ptView->tPositionTexture[i];
    }
    gptDevice->update_render_pass_attachments(ptDevice, ptView->tRenderPass, ptView->tTargetSize, atAttachmentSets);
}

static void
pl_refr_cleanup(void)
{
    
    gptStream->cleanup(&gptData->tDrawStream);

    for(uint32_t i = 0; i < pl_sb_size(gptData->sbtScenes); i++)
    {
        plRefScene* ptScene = &gptData->sbtScenes[i];
        for(uint32_t j = 0; j < ptScene->uViewCount; j++)
        {
            plRefView* ptView = &ptScene->atViews[j];
            pl_sb_free(ptView->sbtVisibleDrawables);
        }
        pl_sb_free(ptScene->sbtVertexPosBuffer);
        pl_sb_free(ptScene->sbtVertexDataBuffer);
        pl_sb_free(ptScene->sbuIndexBuffer);
        pl_sb_free(ptScene->sbtMaterialBuffer);
        pl_sb_free(ptScene->sbtSubmittedDrawables);
        pl_sb_free(ptScene->tLoaderData.sbtOpaqueDrawables);
        pl_sb_free(ptScene->tLoaderData.sbtTransparentDrawables);
        pl_sb_free(ptScene->sbtSkinData);
        pl_sb_free(ptScene->_sbtVariantHandles);
        pl_hm_free(&ptScene->tVariantHashmap);
        gptECS->cleanup_component_library(&ptScene->tLoaderData.tComponentLibrary);
    }
    gptDevice->flush_device(&gptData->tGraphics.tDevice);
    gptGpuAllocators->cleanup_allocators(&gptData->tGraphics.tDevice);
    gptGfx->cleanup(&gptData->tGraphics);

    // must be cleaned up after graphics since 3D drawlist are registered as pointers
    pl_sb_free(gptData->sbtScenes);
    PL_FREE(gptData);
}

static plComponentLibrary*
pl_refr_get_component_library(uint32_t uSceneHandle)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    return &ptScene->tLoaderData.tComponentLibrary;
}

static plGraphics*
pl_refr_get_graphics(void)
{
    return &gptData->tGraphics;
}

static void
pl_refr_load_skybox_from_panorama(uint32_t uSceneHandle, const char* pcPath, int iResolution)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plGraphics* ptGraphics = &gptData->tGraphics;
    plDevice* ptDevice = &ptGraphics->tDevice;

    int iPanoramaWidth = 0;
    int iPanoramaHeight = 0;
    int iUnused = 0;
    float* pfPanoramaData = gptImage->load_hdr(pcPath, &iPanoramaWidth, &iPanoramaHeight, &iUnused, 4);
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
        .tUsage               = PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_STAGING,
        .uByteSize            = PL_DEVICE_ALLOCATION_BLOCK_SIZE
    };
    atComputeBuffers[0] = gptDevice->create_buffer(ptDevice, &tInputBufferDesc, "panorama input");
    {
        plBuffer* ptComputeBuffer = gptDevice->get_buffer(ptDevice, atComputeBuffers[0]);
        plDeviceMemoryAllocation tAllocation = gptData->ptStagingUnCachedAllocator->allocate(gptData->ptStagingUnCachedAllocator->ptInst, ptComputeBuffer->tMemoryRequirements.uMemoryTypeBits, ptComputeBuffer->tMemoryRequirements.ulSize, ptComputeBuffer->tMemoryRequirements.ulAlignment, "panorama input");
        gptDevice->bind_buffer_to_memory(&ptGraphics->tDevice, atComputeBuffers[0], &tAllocation);
        memcpy(ptComputeBuffer->tMemoryAllocation.pHostMapped, pfPanoramaData, iPanoramaWidth * iPanoramaHeight * 4 * sizeof(float));
    }

    const size_t uFaceSize = ((size_t)iResolution * (size_t)iResolution) * 4 * sizeof(float);
    const plBufferDescription tOutputBufferDesc = {
        .tUsage               = PL_BUFFER_USAGE_STORAGE,
        .uByteSize            = PL_DEVICE_ALLOCATION_BLOCK_SIZE
    };
    
    for(uint32_t i = 0; i < 6; i++)
    {
        atComputeBuffers[i + 1] = gptDevice->create_buffer(ptDevice, &tOutputBufferDesc, "panorama output");
        plBuffer* ptBuffer = gptDevice->get_buffer(&ptGraphics->tDevice, atComputeBuffers[i + 1]);
        plDeviceMemoryAllocation tAllocation = gptData->ptStagingUnCachedAllocator->allocate(gptData->ptStagingUnCachedAllocator->ptInst, ptBuffer->tMemoryRequirements.uMemoryTypeBits, ptBuffer->tMemoryRequirements.ulSize, ptBuffer->tMemoryRequirements.ulAlignment, "panorama output");
        gptDevice->bind_buffer_to_memory(&ptGraphics->tDevice, atComputeBuffers[i + 1], &tAllocation);
    }

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
    plBindGroupHandle tComputeBindGroup = gptDevice->get_temporary_bind_group(ptDevice, &tComputeBindGroupLayout, "compute bind group");
    size_t szBufferRangeSize[] = {(size_t)uPanoramaSize, uFaceSize, uFaceSize, uFaceSize, uFaceSize, uFaceSize, uFaceSize};
    plBindGroupUpdateData tBGData0 = {
        .aszBufferRanges = szBufferRangeSize,
        .atBuffers = atComputeBuffers
    };
    gptDevice->update_bind_group(ptDevice, &tComputeBindGroup, &tBGData0);

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

    plCommandBuffer tCommandBuffer = gptGfx->begin_command_recording(ptGraphics, NULL);
    plComputeEncoder tComputeEncoder = gptGfx->begin_compute_pass(ptGraphics, &tCommandBuffer);
    gptGfx->dispatch(&tComputeEncoder, 1, &tDispach);
    gptGfx->end_compute_pass(&tComputeEncoder);
    gptGfx->end_command_recording(ptGraphics, &tCommandBuffer);
    gptGfx->submit_command_buffer_blocking(ptGraphics, &tCommandBuffer, NULL);

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

    plBuffer* ptStagingBuffer = gptDevice->get_buffer(&ptGraphics->tDevice, gptData->tStagingBufferHandle[0]);
    memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, pcResultData, uFaceSize * 6);

    plTextureDesc tTextureDesc = {
        .tDimensions = {(float)iResolution, (float)iResolution, 1},
        .tFormat = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers = 6,
        .uMips = 1,
        .tType = PL_TEXTURE_TYPE_CUBE,
        .tUsage = PL_TEXTURE_USAGE_SAMPLED
    };
    ptScene->tSkyboxTexture = gptDevice->create_texture(ptDevice, &tTextureDesc, "skybox texture");
    plTexture* ptTexture0 = gptDevice->get_texture(&ptGraphics->tDevice, ptScene->tSkyboxTexture);
    plDeviceMemoryAllocatorI* ptAllocator = gptData->ptLocalBuddyAllocator;
    if(ptTexture0->tMemoryRequirements.ulSize > PL_DEVICE_BUDDY_BLOCK_SIZE)
        ptAllocator = gptData->ptLocalDedicatedAllocator;
    plDeviceMemoryAllocation tAllocation0 = ptAllocator->allocate(ptAllocator->ptInst, ptTexture0->tMemoryRequirements.uMemoryTypeBits, ptTexture0->tMemoryRequirements.ulSize, ptTexture0->tMemoryRequirements.ulAlignment, "skybox texture");
    gptDevice->bind_texture_to_memory(&ptGraphics->tDevice, ptScene->tSkyboxTexture, &tAllocation0);

    plBufferImageCopy atBufferImageCopy[6] = {0};
    for(uint32_t i = 0; i < 6; i++)
    {
        atBufferImageCopy[i].tImageExtent = (plExtent){iResolution, iResolution, 1};
        atBufferImageCopy[i].uLayerCount = 1;
        atBufferImageCopy[i].szBufferOffset = i * uFaceSize;
        atBufferImageCopy[i].uBaseArrayLayer = i;
    }
    tCommandBuffer = gptGfx->begin_command_recording(ptGraphics, NULL);
    plBlitEncoder tBlitEncoder = gptGfx->begin_blit_pass(ptGraphics, &tCommandBuffer);
    gptGfx->copy_buffer_to_texture(&tBlitEncoder, gptData->tStagingBufferHandle[0], ptScene->tSkyboxTexture, 6, atBufferImageCopy);
    gptGfx->end_blit_pass(&tBlitEncoder);
    gptGfx->end_command_recording(ptGraphics, &tCommandBuffer);
    gptGfx->submit_command_buffer_blocking(ptGraphics, &tCommandBuffer, NULL);

    // cleanup
    PL_FREE(pcResultData);
    
    for(uint32_t i = 0; i < 7; i++)
        gptDevice->destroy_buffer(ptDevice, atComputeBuffers[i]);

    gptImage->free(pfPanoramaData);

    plBindGroupLayout tSkyboxBindGroupLayout = {
        .uTextureCount  = 1,
        .aTextures = { {.uSlot = 0, .tStages = PL_STAGE_PIXEL | PL_STAGE_VERTEX, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}}
    };
    ptScene->tSkyboxBindGroup = gptDevice->create_bind_group(ptDevice, &tSkyboxBindGroupLayout, "skybox bind group");
    plBindGroupUpdateData tBGData1 = {
            .atTextureViews = &ptScene->tSkyboxTexture
    };
    gptDevice->update_bind_group(ptDevice, &ptScene->tSkyboxBindGroup, &tBGData1);

    const uint32_t uStartIndex     = pl_sb_size(ptScene->sbtVertexPosBuffer);
    const uint32_t uIndexStart     = pl_sb_size(ptScene->sbuIndexBuffer);
    const uint32_t uDataStartIndex = pl_sb_size(ptScene->sbtVertexDataBuffer);

    const plDrawable tDrawable = {
        .uIndexCount   = 36,
        .uVertexCount  = 8,
        .uIndexOffset  = uIndexStart,
        .uVertexOffset = uStartIndex,
        .uDataOffset   = uDataStartIndex,
    };
    ptScene->tSkyboxDrawable = tDrawable;

    // indices
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 0);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 2);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 1);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 2);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 3);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 1);
    
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 1);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 3);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 5);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 3);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 7);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 5);

    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 2);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 6);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 3);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 3);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 6);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 7);
    
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 4);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 5);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 7);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 4);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 7);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 6);
    
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 0);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 4);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 2);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 2);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 4);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 6);
    
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 0);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 1);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 4);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 1);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 5);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 4);

    // vertices (position)
    const float fCubeSide = 0.5f;
    pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){-fCubeSide, -fCubeSide, -fCubeSide}));
    pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){ fCubeSide, -fCubeSide, -fCubeSide}));
    pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){-fCubeSide,  fCubeSide, -fCubeSide}));
    pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){ fCubeSide,  fCubeSide, -fCubeSide}));
    pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){-fCubeSide, -fCubeSide,  fCubeSide}));
    pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){ fCubeSide, -fCubeSide,  fCubeSide}));
    pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){-fCubeSide,  fCubeSide,  fCubeSide}));
    pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){ fCubeSide,  fCubeSide,  fCubeSide})); 
}

static plTextureHandle
pl__create_texture_helper(plMaterialComponent* ptMaterial, plTextureSlot tSlot, bool bHdr, int iMips)
{
    plDevice* ptDevice = &gptData->tGraphics.tDevice;

    if(gptResource->is_resource_valid(ptMaterial->atTextureMaps[tSlot].tResource) == false)
        return gptData->tDummyTexture;
    
    size_t szResourceSize = 0;
    const char* pcFileData = gptResource->get_file_data(ptMaterial->atTextureMaps[tSlot].tResource, &szResourceSize);
    int texWidth, texHeight, texNumChannels;
    int texForceNumChannels = 4;

    plBuffer* ptStagingBuffer = gptDevice->get_buffer(ptDevice, gptData->tStagingBufferHandle[0]);
    
    plTextureHandle tTexture = {0};

    plCommandBuffer tCommandBuffer = gptGfx->begin_command_recording(ptDevice->ptGraphics, NULL);
    plBlitEncoder tBlitEncoder = gptGfx->begin_blit_pass(ptDevice->ptGraphics, &tCommandBuffer);

    if(bHdr)
    {

        float* rawBytes = gptImage->load_hdr_from_memory((unsigned char*)pcFileData, (int)szResourceSize, &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
        PL_ASSERT(rawBytes);

        memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, rawBytes, sizeof(float) * texWidth * texHeight * 4);
        gptImage->free(rawBytes);

        plTextureDesc tTextureDesc = {
            .tDimensions = {(float)texWidth, (float)texHeight, 1},
            .tFormat = PL_FORMAT_R32G32B32A32_FLOAT,
            .uLayers = 1,
            .uMips = iMips,
            .tType = PL_TEXTURE_TYPE_2D,
            .tUsage = PL_TEXTURE_USAGE_SAMPLED
        };
        tTexture = gptDevice->create_texture(ptDevice, &tTextureDesc, ptMaterial->atTextureMaps[tSlot].acName);
        plTexture* ptTexture0 = gptDevice->get_texture(ptDevice, tTexture);
        plDeviceMemoryAllocatorI* ptAllocator = gptData->ptLocalBuddyAllocator;
        if(ptTexture0->tMemoryRequirements.ulSize > PL_DEVICE_BUDDY_BLOCK_SIZE)
            ptAllocator = gptData->ptLocalDedicatedAllocator;
        plDeviceMemoryAllocation tAllocation0 = ptAllocator->allocate(ptAllocator->ptInst, ptTexture0->tMemoryRequirements.uMemoryTypeBits, ptTexture0->tMemoryRequirements.ulSize, ptTexture0->tMemoryRequirements.ulAlignment, ptMaterial->atTextureMaps[tSlot].acName);
        gptDevice->bind_texture_to_memory(ptDevice, tTexture, &tAllocation0);

        plBufferImageCopy tBufferImageCopy = {
            .tImageExtent = {texWidth, texHeight, 1},
            .uLayerCount = 1
        };

        gptGfx->copy_buffer_to_texture(&tBlitEncoder, gptData->tStagingBufferHandle[0], tTexture, 1, &tBufferImageCopy);
        gptGfx->generate_mipmaps(&tBlitEncoder, tTexture);
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
            .tUsage = PL_TEXTURE_USAGE_SAMPLED
        };
        tTexture = gptDevice->create_texture(ptDevice, &tTextureDesc, ptMaterial->atTextureMaps[tSlot].acName);
        plTexture* ptTexture0 = gptDevice->get_texture(ptDevice, tTexture);
        plDeviceMemoryAllocatorI* ptAllocator = gptData->ptLocalBuddyAllocator;
        if(ptTexture0->tMemoryRequirements.ulSize > PL_DEVICE_BUDDY_BLOCK_SIZE)
            ptAllocator = gptData->ptLocalDedicatedAllocator;
        plDeviceMemoryAllocation tAllocation0 = ptAllocator->allocate(ptAllocator->ptInst, ptTexture0->tMemoryRequirements.uMemoryTypeBits, ptTexture0->tMemoryRequirements.ulSize, ptTexture0->tMemoryRequirements.ulAlignment, ptMaterial->atTextureMaps[tSlot].acName);
        gptDevice->bind_texture_to_memory(ptDevice, tTexture, &tAllocation0);

        plBufferImageCopy tBufferImageCopy = {
            .tImageExtent = {texWidth, texHeight, 1},
            .uLayerCount = 1
        };

        gptGfx->copy_buffer_to_texture(&tBlitEncoder, gptData->tStagingBufferHandle[0], tTexture, 1, &tBufferImageCopy);
        gptGfx->generate_mipmaps(&tBlitEncoder, tTexture);
    }

    gptGfx->end_blit_pass(&tBlitEncoder);
    gptGfx->end_command_recording(ptDevice->ptGraphics, &tCommandBuffer);
    gptGfx->submit_command_buffer_blocking(ptDevice->ptGraphics, &tCommandBuffer, NULL);

    return tTexture;
}

static void
pl__add_drawable_data_to_global_buffer(plRefScene* ptScene, uint32_t uDrawableIndex)
{
    plEntity tEntity = ptScene->sbtSubmittedDrawables[uDrawableIndex].tEntity;

    // get actual components
    plObjectComponent*   ptObject   = gptECS->get_component(&ptScene->tLoaderData.tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tEntity);
    plMeshComponent*     ptMesh     = gptECS->get_component(&ptScene->tLoaderData.tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptObject->tMesh);
    plMaterialComponent* ptMaterial = gptECS->get_component(&ptScene->tLoaderData.tComponentLibrary, PL_COMPONENT_TYPE_MATERIAL, ptMesh->tMaterial);

    const uint32_t uVertexPosStartIndex  = pl_sb_size(ptScene->sbtVertexPosBuffer);
    const uint32_t uIndexBufferStart     = pl_sb_size(ptScene->sbuIndexBuffer);
    const uint32_t uVertexDataStartIndex = pl_sb_size(ptScene->sbtVertexDataBuffer);
    const uint32_t uIndexCount           = pl_sb_size(ptMesh->sbuIndices);
    const uint32_t uVertexCount          = pl_sb_size(ptMesh->sbtVertexPositions);
    const uint32_t uMaterialIndex        = pl_sb_size(ptScene->sbtMaterialBuffer);

    // add index buffer data
    pl_sb_add_n(ptScene->sbuIndexBuffer, uIndexCount);
    for(uint32_t j = 0; j < uIndexCount; j++)
        ptScene->sbuIndexBuffer[uIndexBufferStart + j] = uVertexPosStartIndex + ptMesh->sbuIndices[j];

    // add vertex position data
    pl_sb_add_n(ptScene->sbtVertexPosBuffer, uVertexCount);
    memcpy(&ptScene->sbtVertexPosBuffer[uVertexPosStartIndex], ptMesh->sbtVertexPositions, sizeof(plVec3) * uVertexCount);

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

    pl_sb_add_n(ptScene->sbtVertexDataBuffer, uStride * uVertexCount);

    // current attribute offset
    uint32_t uOffset = 0;

    // normals
    const uint32_t uVertexNormalCount = pl_sb_size(ptMesh->sbtVertexNormals);
    for(uint32_t i = 0; i < uVertexNormalCount; i++)
    {
        ptMesh->sbtVertexNormals[i] = pl_norm_vec3(ptMesh->sbtVertexNormals[i]);
        const plVec3* ptNormal = &ptMesh->sbtVertexNormals[i];
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride].x = ptNormal->x;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride].y = ptNormal->y;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride].z = ptNormal->z;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride].w = 0.0f;
    }

    if(uVertexNormalCount > 0)
        uOffset += 1;

    // tangents
    const uint32_t uVertexTangentCount = pl_sb_size(ptMesh->sbtVertexTangents);
    for(uint32_t i = 0; i < uVertexTangentCount; i++)
    {
        const plVec4* ptTangent = &ptMesh->sbtVertexTangents[i];
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].x = ptTangent->x;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].y = ptTangent->y;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].z = ptTangent->z;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].w = ptTangent->w;
    }

    if(uVertexTangentCount > 0)
        uOffset += 1;

    // texture coordinates 0
    const uint32_t uVertexTexCount = pl_sb_size(ptMesh->sbtVertexTextureCoordinates[0]);
    for(uint32_t i = 0; i < uVertexTexCount; i++)
    {
        const plVec2* ptTextureCoordinates = &(ptMesh->sbtVertexTextureCoordinates[0])[i];
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].x = ptTextureCoordinates->u;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].y = ptTextureCoordinates->v;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].z = 0.0f;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].w = 0.0f;

    }

    if(uVertexTexCount > 0)
        uOffset += 1;

    // color 0
    const uint32_t uVertexColorCount = pl_sb_size(ptMesh->sbtVertexColors[0]);
    for(uint32_t i = 0; i < uVertexColorCount; i++)
    {
        const plVec4* ptColor = &ptMesh->sbtVertexColors[0][i];
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].x = ptColor->r;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].y = ptColor->g;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].z = ptColor->b;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].w = ptColor->a;
    }

    if(uVertexColorCount > 0)
        uOffset += 1;

    // joints 0
    const uint32_t uVertexJoint0Count = pl_sb_size(ptMesh->sbtVertexJoints[0]);
    for(uint32_t i = 0; i < uVertexJoint0Count; i++)
    {
        const plVec4* ptJoint = &ptMesh->sbtVertexJoints[0][i];
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].x = ptJoint->x;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].y = ptJoint->y;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].z = ptJoint->z;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].w = ptJoint->w;
    }

    if(uVertexJoint0Count > 0)
        uOffset += 1;

    // weights 0
    const uint32_t uVertexWeights0Count = pl_sb_size(ptMesh->sbtVertexWeights[0]);
    for(uint32_t i = 0; i < uVertexWeights0Count; i++)
    {
        const plVec4* ptWeight = &ptMesh->sbtVertexWeights[0][i];
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].x = ptWeight->x;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].y = ptWeight->y;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].z = ptWeight->z;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].w = ptWeight->w;
    }

    if(uVertexWeights0Count > 0)
        uOffset += 1;

    PL_ASSERT(uOffset == uStride && "sanity check");

    plMaterial tMaterial = {
        .tColor = ptMaterial->tBaseColor
    };
    pl_sb_push(ptScene->sbtMaterialBuffer, tMaterial);

    ptScene->sbtSubmittedDrawables[uDrawableIndex].uIndexCount      = uIndexCount;
    ptScene->sbtSubmittedDrawables[uDrawableIndex].uVertexCount     = uVertexCount;
    ptScene->sbtSubmittedDrawables[uDrawableIndex].uIndexOffset     = uIndexBufferStart;
    ptScene->sbtSubmittedDrawables[uDrawableIndex].uVertexOffset    = uVertexPosStartIndex;
    ptScene->sbtSubmittedDrawables[uDrawableIndex].uDataOffset      = uVertexDataStartIndex;
    ptScene->sbtSubmittedDrawables[uDrawableIndex].uMaterialIndex   = uMaterialIndex;
}

static void
pl_refr_finalize_scene(uint32_t uSceneHandle)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];

    plGraphics* ptGraphics = &gptData->tGraphics;
    plDevice* ptDevice = &ptGraphics->tDevice;

    plBuffer* ptStagingBuffer = gptDevice->get_buffer(ptDevice, gptData->tStagingBufferHandle[0]);

    // add opaque items
    const uint32_t uOpaqueDrawableCount = pl_sb_size(ptScene->tLoaderData.sbtOpaqueDrawables);
    for(uint32_t i = 0; i < uOpaqueDrawableCount; i++)
        pl_sb_push(ptScene->sbtSubmittedDrawables, ptScene->tLoaderData.sbtOpaqueDrawables[i]);

    // add transparent items
    const uint32_t uTransparentDrawableCount = pl_sb_size(ptScene->tLoaderData.sbtTransparentDrawables);
    for(uint32_t i = 0; i < uTransparentDrawableCount; i++)
        pl_sb_push(ptScene->sbtSubmittedDrawables, ptScene->tLoaderData.sbtTransparentDrawables[i]);

    pl_begin_profile_sample("load materials");
    plHashMap tMaterialBindGroupDict = {0};
    plBindGroupHandle* sbtMaterialBindGroups = NULL;
    plMaterialComponent* sbtMaterials = (plMaterialComponent*)ptScene->tLoaderData.tComponentLibrary.tMaterialComponentManager.pComponents;
    const uint32_t uMaterialCount = pl_sb_size(sbtMaterials);
    pl_sb_resize(sbtMaterialBindGroups, uMaterialCount);
    for(uint32_t i = 0; i < uMaterialCount; i++)
    {
        plMaterialComponent* ptMaterial = &sbtMaterials[i];

        plBindGroupLayout tMaterialBindGroupLayout = {
            .uTextureCount = 2,
            .aTextures = {
                {.uSlot = 0, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
                {.uSlot = 1, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
            }
        };
        sbtMaterialBindGroups[i] = gptDevice->create_bind_group(ptDevice, &tMaterialBindGroupLayout, "material bind group");

        plTextureHandle atMaterialTextureViews[] = {
            pl__create_texture_helper(ptMaterial, PL_TEXTURE_SLOT_BASE_COLOR_MAP, true, 0),
            pl__create_texture_helper(ptMaterial, PL_TEXTURE_SLOT_NORMAL_MAP, false, 0)
        };
        plBindGroupUpdateData tBGData0 = {
            .atTextureViews = atMaterialTextureViews
        };
        gptDevice->update_bind_group(ptDevice, &sbtMaterialBindGroups[i], &tBGData0);
        pl_hm_insert(&tMaterialBindGroupDict, (uint64_t)ptMaterial, (uint64_t)i);
    }
    pl_end_profile_sample();

    // fill CPU buffers & drawable list
    pl_begin_profile_sample("fill CPU buffers");
    const uint32_t uDrawableCount = pl_sb_size(ptScene->sbtSubmittedDrawables);
    for(uint32_t uDrawableIndex = 0; uDrawableIndex < uDrawableCount; uDrawableIndex++)
    {

        ptScene->sbtSubmittedDrawables[uDrawableIndex].uSkinIndex = UINT32_MAX;
        plEntity tEntity = ptScene->sbtSubmittedDrawables[uDrawableIndex].tEntity;

        // get actual components
        plObjectComponent*   ptObject   = gptECS->get_component(&ptScene->tLoaderData.tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tEntity);
        plMeshComponent*     ptMesh     = gptECS->get_component(&ptScene->tLoaderData.tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptObject->tMesh);
        plMaterialComponent* ptMaterial = gptECS->get_component(&ptScene->tLoaderData.tComponentLibrary, PL_COMPONENT_TYPE_MATERIAL, ptMesh->tMaterial);

        plBindGroupLayout tMaterialBindGroupLayout = {
            .uTextureCount = 2,
            .aTextures = {
                {.uSlot = 0, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
                {.uSlot = 1, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
            }
        };
        const uint64_t ulMaterialIndex = pl_hm_lookup(&tMaterialBindGroupDict, (uint64_t)ptMaterial);
        ptScene->sbtSubmittedDrawables[uDrawableIndex].tMaterialBindGroup = sbtMaterialBindGroups[ulMaterialIndex];

        // add data to global buffers
        pl__add_drawable_data_to_global_buffer(ptScene, uDrawableIndex);

        // choose shader variant
        int aiConstantData0[5] = {0};
        aiConstantData0[0] = (int)ptMesh->ulVertexStreamMask;
        aiConstantData0[2] = (int)(ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_BASE_COLOR_MAP].acName[0] != 0); // PL_HAS_BASE_COLOR_MAP;
        aiConstantData0[3] = (int)(ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_NORMAL_MAP].acName[0] != 0); // PL_HAS_NORMAL_MAP
        aiConstantData0[4] = (int)(ptMesh->tSkinComponent.uIndex != UINT32_MAX);
        int iFlagCopy0 = (int)ptMesh->ulVertexStreamMask;
        while(iFlagCopy0)
        {
            aiConstantData0[1] += iFlagCopy0 & 1;
            iFlagCopy0 >>= 1;
        }

        const plShaderVariant tVariant = {
            .pTempConstantData = aiConstantData0,
            .tGraphicsState = {
                .ulDepthWriteEnabled  = 1,
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

        ptScene->sbtSubmittedDrawables[uDrawableIndex].uShader = pl__get_shader_variant(uSceneHandle, ptScene->tShader, &tVariant).uIndex;

        if(ptMesh->tSkinComponent.uIndex != UINT32_MAX)
        {

            plSkinData tSkinData = {.tEntity = ptMesh->tSkinComponent};

            plSkinComponent* ptSkinComponent = gptECS->get_component(&ptScene->tLoaderData.tComponentLibrary, PL_COMPONENT_TYPE_SKIN, ptMesh->tSkinComponent);
            unsigned int textureWidth = (unsigned int)ceilf(sqrtf((float)(pl_sb_size(ptSkinComponent->sbtJoints) * 8)));
            pl_sb_resize(ptSkinComponent->sbtTextureData, textureWidth * textureWidth);
            plTextureDesc tTextureDesc = {
                .tDimensions = {(float)textureWidth, (float)textureWidth, 1},
                .tFormat = PL_FORMAT_R32G32B32A32_FLOAT,
                .uLayers = 1,
                .uMips = 1,
                .tType = PL_TEXTURE_TYPE_2D,
                .tUsage = PL_TEXTURE_USAGE_SAMPLED
            };

            for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
            {
                tSkinData.atDynamicTexture[i] = gptDevice->create_texture(ptDevice, &tTextureDesc, "joint texture");
                plTexture* ptTexture0 = gptDevice->get_texture(ptDevice, tSkinData.atDynamicTexture[i]);
                plDeviceMemoryAllocatorI* ptAllocator = gptData->ptLocalBuddyAllocator;
                if(ptTexture0->tMemoryRequirements.ulSize > PL_DEVICE_BUDDY_BLOCK_SIZE)
                    ptAllocator = gptData->ptLocalDedicatedAllocator;
                plDeviceMemoryAllocation tAllocation0 = ptAllocator->allocate(ptAllocator->ptInst, ptTexture0->tMemoryRequirements.uMemoryTypeBits, ptTexture0->tMemoryRequirements.ulSize, ptTexture0->tMemoryRequirements.ulAlignment, "joint texture");
                gptDevice->bind_texture_to_memory(ptDevice, tSkinData.atDynamicTexture[i], &tAllocation0);
            }

            plBufferImageCopy tBufferImageCopy = {
                .tImageExtent = {textureWidth, textureWidth, 1},
                .uLayerCount = 1
            };
            memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, ptSkinComponent->sbtTextureData, sizeof(float) * 4 * textureWidth * textureWidth);

            plCommandBuffer tCommandBuffer = gptGfx->begin_command_recording(ptDevice->ptGraphics, NULL);
            plBlitEncoder tBlitEncoder = gptGfx->begin_blit_pass(ptDevice->ptGraphics, &tCommandBuffer);
            for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
                gptGfx->copy_buffer_to_texture(&tBlitEncoder, gptData->tStagingBufferHandle[0], tSkinData.atDynamicTexture[i], 1, &tBufferImageCopy);
            gptGfx->end_blit_pass(&tBlitEncoder);
            gptGfx->end_command_recording(ptDevice->ptGraphics, &tCommandBuffer);
            gptGfx->submit_command_buffer(ptDevice->ptGraphics, &tCommandBuffer, NULL);

            ptScene->sbtSubmittedDrawables[uDrawableIndex].uSkinIndex = pl_sb_size(ptScene->sbtSkinData);
            pl_sb_push(ptScene->sbtSkinData, tSkinData);
        }
    }
    pl_end_profile_sample();

    pl_hm_free(&tMaterialBindGroupDict);
    pl_sb_free(sbtMaterialBindGroups);

    // create GPU buffers

    pl_begin_profile_sample("fill GPU buffers");
    plCommandBuffer tCommandBuffer = gptGfx->begin_command_recording(ptGraphics, NULL);

    const plBufferDescription tShaderBufferDesc = {
        .tUsage               = PL_BUFFER_USAGE_STORAGE,
        .uByteSize            = sizeof(plMaterial) * pl_sb_size(ptScene->sbtMaterialBuffer)
    };
    memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, ptScene->sbtMaterialBuffer, sizeof(plMaterial) * pl_sb_size(ptScene->sbtMaterialBuffer));
    ptScene->tMaterialDataBuffer = gptDevice->create_buffer(ptDevice, &tShaderBufferDesc, "shader buffer");
    {
        plBuffer* ptBuffer = gptDevice->get_buffer(&ptGraphics->tDevice, ptScene->tMaterialDataBuffer);
        plDeviceMemoryAllocatorI* ptAllocator = gptData->ptLocalBuddyAllocator;
        if(ptBuffer->tMemoryRequirements.ulSize > PL_DEVICE_BUDDY_BLOCK_SIZE)
            ptAllocator = gptData->ptLocalDedicatedAllocator;
        plDeviceMemoryAllocation tAllocation = ptAllocator->allocate(ptAllocator->ptInst, ptBuffer->tMemoryRequirements.uMemoryTypeBits, ptBuffer->tMemoryRequirements.ulSize, ptBuffer->tMemoryRequirements.ulAlignment, "shader buffer");
        gptDevice->bind_buffer_to_memory(&ptGraphics->tDevice, ptScene->tMaterialDataBuffer , &tAllocation);
    }

    plBlitEncoder tEncoder = gptGfx->begin_blit_pass(ptGraphics, &tCommandBuffer);
    gptGfx->copy_buffer(&tEncoder, gptData->tStagingBufferHandle[0], ptScene->tMaterialDataBuffer, 0, 0, tShaderBufferDesc.uByteSize);
    gptGfx->end_blit_pass(&tEncoder);
    gptGfx->end_command_recording(ptGraphics, &tCommandBuffer);
    gptGfx->submit_command_buffer_blocking(ptGraphics, &tCommandBuffer, NULL);
    ptStagingBuffer = gptDevice->get_buffer(ptDevice, gptData->tStagingBufferHandle[0]);

    const plBufferDescription tIndexBufferDesc = {
        .tUsage               = PL_BUFFER_USAGE_INDEX,
        .uByteSize            = sizeof(uint32_t) * pl_sb_size(ptScene->sbuIndexBuffer)
    };
    memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, ptScene->sbuIndexBuffer, sizeof(uint32_t) * pl_sb_size(ptScene->sbuIndexBuffer));
    ptScene->tIndexBuffer = gptDevice->create_buffer(ptDevice, &tIndexBufferDesc, "index buffer");
    {
        plBuffer* ptBuffer = gptDevice->get_buffer(&ptGraphics->tDevice, ptScene->tIndexBuffer);
        plDeviceMemoryAllocatorI* ptAllocator = gptData->ptLocalBuddyAllocator;
        if(ptBuffer->tMemoryRequirements.ulSize > PL_DEVICE_BUDDY_BLOCK_SIZE)
            ptAllocator = gptData->ptLocalDedicatedAllocator;
        plDeviceMemoryAllocation tAllocation = ptAllocator->allocate(ptAllocator->ptInst, ptBuffer->tMemoryRequirements.uMemoryTypeBits, ptBuffer->tMemoryRequirements.ulSize, ptBuffer->tMemoryRequirements.ulAlignment, "index buffer");
        gptDevice->bind_buffer_to_memory(&ptGraphics->tDevice, ptScene->tIndexBuffer , &tAllocation);
    }

    tCommandBuffer = gptGfx->begin_command_recording(ptGraphics, NULL);
    tEncoder = gptGfx->begin_blit_pass(ptGraphics, &tCommandBuffer);
    gptGfx->copy_buffer(&tEncoder, gptData->tStagingBufferHandle[0], ptScene->tIndexBuffer, 0, 0, tIndexBufferDesc.uByteSize);
    gptGfx->end_blit_pass(&tEncoder);
    gptGfx->end_command_recording(ptGraphics, &tCommandBuffer);
    gptGfx->submit_command_buffer_blocking(ptGraphics, &tCommandBuffer, NULL);
    ptStagingBuffer = gptDevice->get_buffer(ptDevice, gptData->tStagingBufferHandle[0]);

    const plBufferDescription tVertexBufferDesc = {
        .tUsage               = PL_BUFFER_USAGE_VERTEX,
        .uByteSize            = sizeof(plVec3) * pl_sb_size(ptScene->sbtVertexPosBuffer)
    };
    memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, ptScene->sbtVertexPosBuffer, sizeof(plVec3) * pl_sb_size(ptScene->sbtVertexPosBuffer));
    ptScene->tVertexBuffer = gptDevice->create_buffer(ptDevice, &tVertexBufferDesc, "vertex buffer");
    {
        plBuffer* ptBuffer = gptDevice->get_buffer(&ptGraphics->tDevice, ptScene->tVertexBuffer);
        plDeviceMemoryAllocatorI* ptAllocator = gptData->ptLocalBuddyAllocator;
        if(ptBuffer->tMemoryRequirements.ulSize > PL_DEVICE_BUDDY_BLOCK_SIZE)
            ptAllocator = gptData->ptLocalDedicatedAllocator;
        plDeviceMemoryAllocation tAllocation = ptAllocator->allocate(ptAllocator->ptInst, ptBuffer->tMemoryRequirements.uMemoryTypeBits, ptBuffer->tMemoryRequirements.ulSize, ptBuffer->tMemoryRequirements.ulAlignment, "vertex buffer");
        gptDevice->bind_buffer_to_memory(&ptGraphics->tDevice, ptScene->tVertexBuffer , &tAllocation);
    }
    
    tCommandBuffer = gptGfx->begin_command_recording(ptGraphics, NULL);
    tEncoder = gptGfx->begin_blit_pass(ptGraphics, &tCommandBuffer);
    gptGfx->copy_buffer(&tEncoder, gptData->tStagingBufferHandle[0], ptScene->tVertexBuffer, 0, 0, tVertexBufferDesc.uByteSize);
    gptGfx->end_blit_pass(&tEncoder);
    gptGfx->end_command_recording(ptGraphics, &tCommandBuffer);
    gptGfx->submit_command_buffer_blocking(ptGraphics, &tCommandBuffer, NULL);
    ptStagingBuffer = gptDevice->get_buffer(ptDevice, gptData->tStagingBufferHandle[0]);

    const plBufferDescription tStorageBufferDesc = {
        .tUsage               = PL_BUFFER_USAGE_STORAGE,
        .uByteSize            = sizeof(plVec4) * pl_sb_size(ptScene->sbtVertexDataBuffer)
    };
    memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, ptScene->sbtVertexDataBuffer, sizeof(plVec4) * pl_sb_size(ptScene->sbtVertexDataBuffer));
    ptScene->tStorageBuffer = gptDevice->create_buffer(ptDevice, &tStorageBufferDesc, "storage buffer");
    {
        plBuffer* ptBuffer = gptDevice->get_buffer(&ptGraphics->tDevice, ptScene->tStorageBuffer);
        plDeviceMemoryAllocatorI* ptAllocator = gptData->ptLocalBuddyAllocator;
        if(ptBuffer->tMemoryRequirements.ulSize > PL_DEVICE_BUDDY_BLOCK_SIZE)
            ptAllocator = gptData->ptLocalDedicatedAllocator;
        plDeviceMemoryAllocation tAllocation = ptAllocator->allocate(ptAllocator->ptInst, ptBuffer->tMemoryRequirements.uMemoryTypeBits, ptBuffer->tMemoryRequirements.ulSize, ptBuffer->tMemoryRequirements.ulAlignment, "storage buffer");
        gptDevice->bind_buffer_to_memory(&ptGraphics->tDevice, ptScene->tStorageBuffer , &tAllocation);
    }

    tCommandBuffer = gptGfx->begin_command_recording(ptGraphics, NULL);
    tEncoder = gptGfx->begin_blit_pass(ptGraphics, &tCommandBuffer);
    gptGfx->copy_buffer(&tEncoder, gptData->tStagingBufferHandle[0], ptScene->tStorageBuffer, 0, 0, tStorageBufferDesc.uByteSize);
    gptGfx->end_blit_pass(&tEncoder);
    gptGfx->end_command_recording(ptGraphics, &tCommandBuffer);
    gptGfx->submit_command_buffer_blocking(ptGraphics, &tCommandBuffer, NULL);
    pl_end_profile_sample();
}

static void
pl_refr_run_ecs(uint32_t uSceneHandle)
{
    pl_begin_profile_sample(__FUNCTION__);
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    gptECS->run_animation_update_system(&ptScene->tLoaderData.tComponentLibrary, pl_get_io()->fDeltaTime);
    gptECS->run_transform_update_system(&ptScene->tLoaderData.tComponentLibrary);
    gptECS->run_hierarchy_update_system(&ptScene->tLoaderData.tComponentLibrary);
    gptECS->run_inverse_kinematics_update_system(&ptScene->tLoaderData.tComponentLibrary);
    gptECS->run_skin_update_system(&ptScene->tLoaderData.tComponentLibrary);
    gptECS->run_object_update_system(&ptScene->tLoaderData.tComponentLibrary);
    pl_end_profile_sample();
}

static bool
pl__sat_visibility_test(plCameraComponent* ptCamera, const plAABB* ptAABB)
{
    const float fTanFov = tanf(0.5f * ptCamera->fFieldOfView);

    const float fZNear = ptCamera->fNearZ;
    const float fZFar = ptCamera->fFarZ;

    // half width, half height
    const float fXNear = ptCamera->fAspectRatio * ptCamera->fNearZ * fTanFov;
    const float fYNear = ptCamera->fNearZ * fTanFov;

    // consider four adjacent corners of the AABB
    plVec3 atCorners[] = {
        {ptAABB->tMin.x, ptAABB->tMin.y, ptAABB->tMin.z},
        {ptAABB->tMax.x, ptAABB->tMin.y, ptAABB->tMin.z},
        {ptAABB->tMin.x, ptAABB->tMax.y, ptAABB->tMin.z},
        {ptAABB->tMin.x, ptAABB->tMin.y, ptAABB->tMax.z},
    };

    // transform corners
    for (size_t i = 0; i < 4; i++)
        atCorners[i] = pl_mul_mat4_vec3(&ptCamera->tViewMat, atCorners[i]);

    // Use transformed atCorners to calculate center, axes and extents
    plOBB tObb = {
        .atAxes = {
            pl_sub_vec3(atCorners[1], atCorners[0]),
            pl_sub_vec3(atCorners[2], atCorners[0]),
            pl_sub_vec3(atCorners[3], atCorners[0])
        },
    };

    tObb.tCenter = pl_add_vec3(atCorners[0], pl_mul_vec3_scalarf((pl_add_vec3(tObb.atAxes[0], pl_add_vec3(tObb.atAxes[1], tObb.atAxes[2]))), 0.5f));
    tObb.tExtents = (plVec3){ pl_length_vec3(tObb.atAxes[0]), pl_length_vec3(tObb.atAxes[1]), pl_length_vec3(tObb.atAxes[2]) };

    // normalize
    tObb.atAxes[0] = pl_div_vec3_scalarf(tObb.atAxes[0], tObb.tExtents.x);
    tObb.atAxes[1] = pl_div_vec3_scalarf(tObb.atAxes[1], tObb.tExtents.y);
    tObb.atAxes[2] = pl_div_vec3_scalarf(tObb.atAxes[2], tObb.tExtents.z);
    tObb.tExtents = pl_mul_vec3_scalarf(tObb.tExtents, 0.5f);

    // axis along frustum
    {
        // Projected center of our OBB
        const float fMoC = tObb.tCenter.z;

        // Projected size of OBB
        float fRadius = 0.0f;
        for (size_t i = 0; i < 3; i++)
            fRadius += fabsf(tObb.atAxes[i].z) * tObb.tExtents.d[i];

        const float fObbMin = fMoC - fRadius;
        const float fObbMax = fMoC + fRadius;

        if (fObbMin > fZFar || fObbMax < fZNear)
            return false;
    }


    // other normals of frustum
    {
        const plVec3 atM[] = {
            { fZNear, 0.0f, fXNear }, // Left Plane
            { -fZNear, 0.0f, fXNear }, // Right plane
            { 0.0, -fZNear, fYNear }, // Top plane
            { 0.0, fZNear, fYNear }, // Bottom plane
        };
        for (size_t m = 0; m < 4; m++)
        {
            const float fMoX = fabsf(atM[m].x);
            const float fMoY = fabsf(atM[m].y);
            const float fMoZ = atM[m].z;
            const float fMoC = pl_dot_vec3(atM[m], tObb.tCenter);

            float fObbRadius = 0.0f;
            for (size_t i = 0; i < 3; i++)
                fObbRadius += fabsf(pl_dot_vec3(atM[m], tObb.atAxes[i])) * tObb.tExtents.d[i];

            const float fObbMin = fMoC - fObbRadius;
            const float fObbMax = fMoC + fObbRadius;

            const float fP = fXNear * fMoX + fYNear * fMoY;

            float fTau0 = fZNear * fMoZ - fP;
            float fTau1 = fZNear * fMoZ + fP;

            if (fTau0 < 0.0f)
                fTau0 *= fZFar / fZNear;

            if (fTau1 > 0.0f)
                fTau1 *= fZFar / fZNear;

            if (fObbMin > fTau1 || fObbMax < fTau0)
                return false;
        }
    }

    // OBB axes
    {
        for (size_t m = 0; m < 3; m++)
        {
            const plVec3* ptM = &tObb.atAxes[m];
            const float fMoX = fabsf(ptM->x);
            const float fMoY = fabsf(ptM->y);
            const float fMoZ = ptM->z;
            const float fMoC = pl_dot_vec3(*ptM, tObb.tCenter);

            const float fObbRadius = tObb.tExtents.d[m];

            const float fObbMin = fMoC - fObbRadius;
            const float fObbMax = fMoC + fObbRadius;

            // frustum projection
            const float fP = fXNear * fMoX + fYNear * fMoY;
            float fTau0 = fZNear * fMoZ - fP;
            float fTau1 = fZNear * fMoZ + fP;

            if (fTau0 < 0.0f)
                fTau0 *= fZFar / fZNear;

            if (fTau1 > 0.0f)
                fTau1 *= fZFar / fZNear;

            if (fObbMin > fTau1 || fObbMax < fTau0)
                return false;
        }
    }

    // cross products between the edges
    // first R x A_i
    {
        for (size_t m = 0; m < 3; m++)
        {
            const plVec3 tM = { 0.0f, -tObb.atAxes[m].z, tObb.atAxes[m].y };
            const float fMoX = 0.0f;
            const float fMoY = fabsf(tM.y);
            const float fMoZ = tM.z;
            const float fMoC = tM.y * tObb.tCenter.y + tM.z * tObb.tCenter.z;

            float fObbRadius = 0.0f;
            for (size_t i = 0; i < 3; i++)
                fObbRadius += fabsf(pl_dot_vec3(tM, tObb.atAxes[i])) * tObb.tExtents.d[i];

            const float fObbMin = fMoC - fObbRadius;
            const float fObbMax = fMoC + fObbRadius;

            // frustum projection
            const float fP = fXNear * fMoX + fYNear * fMoY;
            float fTau0 = fZNear * fMoZ - fP;
            float fTau1 = fZNear * fMoZ + fP;

            if (fTau0 < 0.0f)
                fTau0 *= fZFar / fZNear;

            if (fTau1 > 0.0f)
                fTau1 *= fZFar / fZNear;

            if (fObbMin > fTau1 || fObbMax < fTau0)
                return false;
        }
    }

    // U x A_i
    {
        for (size_t m = 0; m < 3; m++)
        {
            const plVec3 tM = { tObb.atAxes[m].z, 0.0f, -tObb.atAxes[m].x };
            const float fMoX = fabsf(tM.x);
            const float fMoY = 0.0f;
            const float fMoZ = tM.z;
            const float fMoC = tM.x * tObb.tCenter.x + tM.z * tObb.tCenter.z;

            float fObbRadius = 0.0f;
            for (size_t i = 0; i < 3; i++)
                fObbRadius += fabsf(pl_dot_vec3(tM, tObb.atAxes[i])) * tObb.tExtents.d[i];

            const float fObbMin = fMoC - fObbRadius;
            const float fObbMax = fMoC + fObbRadius;

            // frustum projection
            const float fP = fXNear * fMoX + fYNear * fMoY;
            float fTau0 = fZNear * fMoZ - fP;
            float fTau1 = fZNear * fMoZ + fP;

            if (fTau0 < 0.0f)
                fTau0 *= fZFar / fZNear;

            if (fTau1 > 0.0f)
                fTau1 *= fZFar / fZNear;

            if (fObbMin > fTau1 || fObbMax < fTau0)
                return false;
        }
    }

    // frustum Edges X Ai
    {
        for (size_t obb_edge_idx = 0; obb_edge_idx < 3; obb_edge_idx++)
        {
            const plVec3 atM[] = {
                pl_cross_vec3((plVec3){-fXNear, 0.0f, fZNear}, tObb.atAxes[obb_edge_idx]), // Left Plane
                pl_cross_vec3((plVec3){ fXNear, 0.0f, fZNear }, tObb.atAxes[obb_edge_idx]), // Right plane
                pl_cross_vec3((plVec3){ 0.0f, fYNear, fZNear }, tObb.atAxes[obb_edge_idx]), // Top plane
                pl_cross_vec3((plVec3){ 0.0, -fYNear, fZNear }, tObb.atAxes[obb_edge_idx]) // Bottom plane
            };

            for (size_t m = 0; m < 4; m++)
            {
                const float fMoX = fabsf(atM[m].x);
                const float fMoY = fabsf(atM[m].y);
                const float fMoZ = atM[m].z;

                const float fEpsilon = 1e-4f;
                if (fMoX < fEpsilon && fMoY < fEpsilon && fabsf(fMoZ) < fEpsilon) continue;

                const float fMoC = pl_dot_vec3(atM[m], tObb.tCenter);

                float fObbRadius = 0.0f;
                for (size_t i = 0; i < 3; i++)
                    fObbRadius += fabsf(pl_dot_vec3(atM[m], tObb.atAxes[i])) * tObb.tExtents.d[i];

                const float fObbMin = fMoC - fObbRadius;
                const float fObbMax = fMoC + fObbRadius;

                // frustum projection
                const float fP = fXNear * fMoX + fYNear * fMoY;
                float fTau0 = fZNear * fMoZ - fP;
                float fTau1 = fZNear * fMoZ + fP;

                if (fTau0 < 0.0f)
                    fTau0 *= fZFar / fZNear;

                if (fTau1 > 0.0f)
                    fTau1 *= fZFar / fZNear;

                if (fObbMin > fTau1 || fObbMax < fTau0)
                    return false;
            }
        }
    }

    // no intersections detected
    return true;
}

static void
pl_refr_update_scene(plCommandBuffer tCommandBuffer, uint32_t uSceneHandle)
{
    pl_begin_profile_sample(__FUNCTION__);
    plGraphics* ptGraphics = &gptData->tGraphics;
    plDevice* ptDevice = &ptGraphics->tDevice;
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plBlitEncoder tBlitEncoder = gptGfx->begin_blit_pass(ptDevice->ptGraphics, &tCommandBuffer);

    // update skin textures
    const uint32_t uSkinCount = pl_sb_size(ptScene->sbtSkinData);
    for(uint32_t i = 0; i < uSkinCount; i++)
    {
        plBindGroupLayout tBindGroupLayout1 = {
            .uTextureCount  = 1,
            .aTextures = {
                {.uSlot =  0, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
            }
        };
        ptScene->sbtSkinData[i].tTempBindGroup = gptDevice->get_temporary_bind_group(ptDevice, &tBindGroupLayout1, "skin temporary bind group");
        plBindGroupUpdateData tBGData0 = {
            .atTextureViews = &ptScene->sbtSkinData[i].atDynamicTexture[ptGraphics->uCurrentFrameIndex]
        };
        gptDevice->update_bind_group(&ptGraphics->tDevice, &ptScene->sbtSkinData[i].tTempBindGroup, &tBGData0);

        plBuffer* ptStagingBuffer = gptDevice->get_buffer(ptDevice, gptData->tStagingBufferHandle[ptGraphics->uCurrentFrameIndex]);

        plTexture* ptSkinTexture = gptDevice->get_texture(ptDevice, ptScene->sbtSkinData[i].atDynamicTexture[ptGraphics->uCurrentFrameIndex]);
        plBufferImageCopy tBufferImageCopy = {
            .tImageExtent = {(size_t)ptSkinTexture->tDesc.tDimensions.x, (size_t)ptSkinTexture->tDesc.tDimensions.y, 1},
            .uLayerCount = 1,
            .szBufferOffset = uSceneHandle * sizeof(float) * 4 * (size_t)ptSkinTexture->tDesc.tDimensions.x * (size_t)ptSkinTexture->tDesc.tDimensions.y
        };
        
        plSkinComponent* ptSkinComponent = gptECS->get_component(&ptScene->tLoaderData.tComponentLibrary, PL_COMPONENT_TYPE_SKIN, ptScene->sbtSkinData[i].tEntity);
        memcpy(&ptStagingBuffer->tMemoryAllocation.pHostMapped[uSceneHandle * sizeof(float) * 4 * (size_t)ptSkinTexture->tDesc.tDimensions.x * (size_t)ptSkinTexture->tDesc.tDimensions.y], ptSkinComponent->sbtTextureData, sizeof(float) * 4 * (size_t)ptSkinTexture->tDesc.tDimensions.x * (size_t)ptSkinTexture->tDesc.tDimensions.y);
        // memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, ptSkinComponent->sbtTextureData, sizeof(float) * 4 * (size_t)ptSkinTexture->tDesc.tDimensions.x * (size_t)ptSkinTexture->tDesc.tDimensions.y);
        gptGfx->copy_buffer_to_texture(&tBlitEncoder, gptData->tStagingBufferHandle[ptGraphics->uCurrentFrameIndex], ptScene->sbtSkinData[i].atDynamicTexture[ptGraphics->uCurrentFrameIndex], 1, &tBufferImageCopy);
    }
    gptGfx->end_blit_pass(&tBlitEncoder);
    pl_end_profile_sample();
}

static void
pl_refr_render_scene(plCommandBuffer tCommandBuffer, uint32_t uSceneHandle, uint32_t uViewHandle, plViewOptions tOptions)
{
    pl_begin_profile_sample(__FUNCTION__);

    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plRefView* ptView = &ptScene->atViews[uViewHandle];
    plCameraComponent* ptCamera = tOptions.ptViewCamera;

    // handle culling
    
    const uint32_t uDrawableCount = pl_sb_size(ptScene->sbtSubmittedDrawables);
    pl_begin_profile_sample("cull operations");
    if(tOptions.ptCullCamera)
    {
        pl_sb_reset(ptView->sbtVisibleDrawables);
        for(uint32_t uDrawableIndex = 0; uDrawableIndex < uDrawableCount; uDrawableIndex++)
        {
            const plDrawable tDrawable = ptScene->sbtSubmittedDrawables[uDrawableIndex];
            plMeshComponent* ptMesh = gptECS->get_component(&ptScene->tLoaderData.tComponentLibrary, PL_COMPONENT_TYPE_MESH, tDrawable.tEntity);

            if(pl__sat_visibility_test(tOptions.ptCullCamera, &ptMesh->tAABBFinal))
            {
                pl_sb_push(ptView->sbtVisibleDrawables, tDrawable);
            }
        }
        
    }
    else if(pl_sb_size(ptView->sbtVisibleDrawables) != uDrawableCount)
    {
        pl_sb_resize(ptView->sbtVisibleDrawables, uDrawableCount);
        memcpy(ptView->sbtVisibleDrawables, ptScene->sbtSubmittedDrawables, sizeof(plDrawable) * uDrawableCount);
    }
    pl_end_profile_sample();

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
    memcpy(ptGraphics->sbtBuffersCold[ptView->atGlobalBuffers[ptGraphics->uCurrentFrameIndex].uIndex].tMemoryAllocation.pHostMapped, &tBindGroupBuffer, sizeof(BindGroup_0));

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
        },
        .uSamplerCount = 1,
        .atSamplers = { {.uSlot = 3, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}}
    };
    plBindGroupHandle tGlobalBG = gptDevice->get_temporary_bind_group(ptDevice, &tBindGroupLayout0, "temporary global bind group");
    size_t szBufferRangeSize[] = {sizeof(BindGroup_0), sizeof(plVec4) * pl_sb_size(ptScene->sbtVertexDataBuffer), sizeof(plMaterial) * pl_sb_size(ptScene->sbtMaterialBuffer)};

    plBufferHandle atBindGroup0_buffers0[] = {ptView->atGlobalBuffers[ptGraphics->uCurrentFrameIndex], ptScene->tStorageBuffer, ptScene->tMaterialDataBuffer};
    plBindGroupUpdateData tBGData0 = {
        .aszBufferRanges = szBufferRangeSize,
        .atBuffers = atBindGroup0_buffers0,
        .atSamplers = &gptData->tDefaultSampler
    };
    gptDevice->update_bind_group(&ptGraphics->tDevice, &tGlobalBG, &tBGData0);

    gptStream->reset(ptStream);

    const plVec2 tDimensions = ptGraphics->sbtRenderPassesCold[ptView->tRenderPass.uIndex].tDesc.tDimensions;

    plDrawArea tArea = {
       .ptDrawStream = ptStream,
       .tScissor = {
            .uWidth  = (uint32_t)tDimensions.x,
            .uHeight = (uint32_t)tDimensions.y,
       },
       .tViewport = {
            .fWidth  = tDimensions.x,
            .fHeight = tDimensions.y,
            .fMaxDepth = 1.0f
       }
    };

    plRenderEncoder tEncoder = gptGfx->begin_render_pass(ptGraphics, &tCommandBuffer, ptView->tRenderPass);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~visible meshes~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    static double* pdVisibleObjects = NULL;
    if(!pdVisibleObjects)
        pdVisibleObjects = gptStats->get_counter("visible objects");

    const uint32_t uVisibleDrawCount = pl_sb_size(ptView->sbtVisibleDrawables);

    if(tOptions.bCullStats)
        *pdVisibleObjects = (double)uVisibleDrawCount;

    for(uint32_t i = 0; i < uVisibleDrawCount; i++)
    {
        const plDrawable tDrawable = ptView->sbtVisibleDrawables[i];
        plObjectComponent* ptObject = gptECS->get_component(&ptScene->tLoaderData.tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
        plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tLoaderData.tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
        
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
            .uVertexBuffer        = ptScene->tVertexBuffer.uIndex,
            .uIndexBuffer         = tDrawable.uIndexCount == 0 ? UINT32_MAX : ptScene->tIndexBuffer.uIndex,
            .uIndexOffset         = tDrawable.uIndexOffset,
            .uTriangleCount       = tDrawable.uIndexCount == 0 ? tDrawable.uVertexCount / 3 : tDrawable.uIndexCount / 3,
            .uBindGroup0          = tGlobalBG.uIndex,
            .uBindGroup1          = tDrawable.tMaterialBindGroup.uIndex,
            .uBindGroup2          = tDrawable.uSkinIndex == UINT32_MAX ? gptData->tNullSkinBindgroup.uIndex : ptScene->sbtSkinData[tDrawable.uSkinIndex].tTempBindGroup.uIndex,
            .uDynamicBufferOffset = tDynamicBinding.uByteOffset,
            .uInstanceStart       = 0,
            .uInstanceCount       = 1
        });
    }

    gptGfx->draw_subpass(&tEncoder, 1, &tArea);

    gptStream->reset(ptStream);
    

    typedef struct _plLightingDynamicData{
        int iDataOffset;
        int iVertexOffset;
        int unused[2];
    } plLightingDynamicData;
    plDynamicBinding tLightingDynamicData = gptDevice->allocate_dynamic_data(ptDevice, sizeof(plLightingDynamicData));
    plLightingDynamicData* ptLightingDynamicData = (plLightingDynamicData*)tLightingDynamicData.pcData;
    ptLightingDynamicData->iDataOffset = ptScene->tLightingDrawable.uDataOffset;
    ptLightingDynamicData->iVertexOffset = ptScene->tLightingDrawable.uVertexOffset;

    gptStream->draw(ptStream, (plDraw)
    {
        .uShaderVariant       = gptData->tLightingShader.uIndex,
        .uDynamicBuffer       = tLightingDynamicData.uBufferHandle,
        .uVertexBuffer        = ptScene->tVertexBuffer.uIndex,
        .uIndexBuffer         = ptScene->tIndexBuffer.uIndex,
        .uIndexOffset         = ptScene->tLightingDrawable.uIndexOffset,
        .uTriangleCount       = 2,
        .uBindGroup0          = tGlobalBG.uIndex,
        .uBindGroup1          = ptView->tLightingBindGroup[ptGraphics->uCurrentFrameIndex].uIndex,
        .uBindGroup2          = gptData->tNullSkinBindgroup.uIndex,
        .uDynamicBufferOffset = tLightingDynamicData.uByteOffset,
        .uInstanceStart       = 0,
        .uInstanceCount       = 1
    });
    gptGfx->next_subpass(&tEncoder);
    gptGfx->draw_subpass(&tEncoder, 1, &tArea);
    gptStream->reset(ptStream);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~skybox~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    if(ptScene->tSkyboxTexture.uIndex != UINT32_MAX)
    {
        
        plDynamicBinding tSkyboxDynamicData = gptDevice->allocate_dynamic_data(ptDevice, sizeof(plMat4));
        plMat4* ptSkyboxDynamicData = (plMat4*)tSkyboxDynamicData.pcData;
        *ptSkyboxDynamicData = pl_mat4_translate_vec3(ptCamera->tPos);

        gptStream->draw(ptStream, (plDraw)
        {
            .uShaderVariant       = gptData->tSkyboxShader.uIndex,
            .uDynamicBuffer       = tSkyboxDynamicData.uBufferHandle,
            .uVertexBuffer        = ptScene->tVertexBuffer.uIndex,
            .uIndexBuffer         = ptScene->tIndexBuffer.uIndex,
            .uIndexOffset         = ptScene->tSkyboxDrawable.uIndexOffset,
            .uTriangleCount       = ptScene->tSkyboxDrawable.uIndexCount / 3,
            .uBindGroup0          = tGlobalBG.uIndex,
            .uBindGroup1          = ptScene->tSkyboxBindGroup.uIndex,
            .uBindGroup2          = gptData->tNullSkinBindgroup.uIndex,
            .uDynamicBufferOffset = tSkyboxDynamicData.uByteOffset,
            .uInstanceStart       = 0,
            .uInstanceCount       = 1
        });

        gptGfx->next_subpass(&tEncoder);
        gptGfx->draw_subpass(&tEncoder, 1, &tArea);
    }

    gptStream->reset(ptStream);

    if(tOptions.bShowAllBoundingBoxes)
    {
        for(uint32_t i = 0; i < uDrawableCount; i++)
        {
            plMeshComponent* ptMesh = gptECS->get_component(&ptScene->tLoaderData.tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptScene->sbtSubmittedDrawables[i].tEntity);

            gptGfx->add_3d_aabb(&ptView->t3DDrawList, ptMesh->tAABBFinal.tMin, ptMesh->tAABBFinal.tMax, (plVec4){1.0f, 0.0f, 0.0f, 1.0f}, 0.01f);
        }
    }
    else if(tOptions.bShowVisibleBoundingBoxes)
    {
        for(uint32_t i = 0; i < uVisibleDrawCount; i++)
        {
            plMeshComponent* ptMesh = gptECS->get_component(&ptScene->tLoaderData.tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptView->sbtVisibleDrawables[i].tEntity);

            gptGfx->add_3d_aabb(&ptView->t3DDrawList, ptMesh->tAABBFinal.tMin, ptMesh->tAABBFinal.tMax, (plVec4){1.0f, 0.0f, 0.0f, 1.0f}, 0.01f);
        }
    }

    if(tOptions.bShowOrigin)
    {
        const plMat4 tTransform = pl_identity_mat4();
        gptGfx->add_3d_transform(&ptView->t3DDrawList, &tTransform, 10.0f, 0.02f);
    }

    if(tOptions.ptCullCamera && tOptions.ptCullCamera != tOptions.ptViewCamera)
    {
        gptGfx->add_3d_frustum(&ptView->t3DDrawList, &tOptions.ptCullCamera->tTransformMat, tOptions.ptCullCamera->fFieldOfView, tOptions.ptCullCamera->fAspectRatio, tOptions.ptCullCamera->fNearZ, tOptions.ptCullCamera->fFarZ, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 0.02f);
    }

    const plMat4 tMVP = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);

    gptGfx->submit_3d_drawlist(&ptView->t3DDrawList, tEncoder, tDimensions.x, tDimensions.y, &tMVP, PL_PIPELINE_FLAG_DEPTH_TEST | PL_PIPELINE_FLAG_DEPTH_WRITE, 1);
    gptGfx->end_render_pass(&tEncoder);
    pl_end_profile_sample();
}

static plTextureId
pl_refr_get_view_texture_id(uint32_t uSceneHandle, uint32_t uViewHandle)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plRefView* ptView = &ptScene->atViews[uViewHandle];
    return ptView->tTextureID[gptData->tGraphics.uCurrentFrameIndex];
}

static plShaderHandle
pl__get_shader_variant(uint32_t uSceneHandle, plShaderHandle tHandle, const plShaderVariant* ptVariant)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plGraphics* ptGraphics = &gptData->tGraphics;
    plDevice* ptDevice = &ptGraphics->tDevice;

    plShader* ptShader = &ptGraphics->sbtShadersCold[tHandle.uIndex];

    size_t szSpecializationSize = 0;
    for(uint32_t i = 0; i < ptShader->tDescription.uConstantCount; i++)
    {
        const plSpecializationConstant* ptConstant = &ptShader->tDescription.atConstants[i];
        szSpecializationSize += pl__get_data_type_size(ptConstant->tType);
    }

    const uint64_t ulVariantHash = pl_hm_hash(ptVariant->pTempConstantData, szSpecializationSize, ptVariant->tGraphicsState.ulValue);
    const uint64_t ulIndex = pl_hm_lookup(&ptScene->tVariantHashmap, ulVariantHash);

    if(ulIndex != UINT64_MAX)
        return ptScene->_sbtVariantHandles[ulIndex];

    plShaderDescription tDesc = ptShader->tDescription;
    tDesc.tGraphicsState = ptVariant->tGraphicsState;
    tDesc.pTempConstantData = ptVariant->pTempConstantData;

    plShaderHandle tShader = gptDevice->create_shader(ptDevice, &tDesc);

    pl_hm_insert(&ptScene->tVariantHashmap, ulVariantHash, pl_sb_size(ptScene->_sbtVariantHandles));
    pl_sb_push(ptScene->_sbtVariantHandles, tShader);
    return tShader;
}

static size_t
pl__get_data_type_size(plDataType tType)
{
    switch(tType)
    {
        case PL_DATA_TYPE_BOOL:   return sizeof(int);
        case PL_DATA_TYPE_BOOL2:  return 2 * sizeof(int);
        case PL_DATA_TYPE_BOOL3:  return 3 * sizeof(int);
        case PL_DATA_TYPE_BOOL4:  return 4 * sizeof(int);
        
        case PL_DATA_TYPE_FLOAT:  return sizeof(float);
        case PL_DATA_TYPE_FLOAT2: return 2 * sizeof(float);
        case PL_DATA_TYPE_FLOAT3: return 3 * sizeof(float);
        case PL_DATA_TYPE_FLOAT4: return 4 * sizeof(float);

        case PL_DATA_TYPE_UNSIGNED_BYTE:
        case PL_DATA_TYPE_BYTE:  return sizeof(uint8_t);

        case PL_DATA_TYPE_UNSIGNED_SHORT:
        case PL_DATA_TYPE_SHORT: return sizeof(uint16_t);

        case PL_DATA_TYPE_UNSIGNED_INT:
        case PL_DATA_TYPE_INT:   return sizeof(uint32_t);

        case PL_DATA_TYPE_UNSIGNED_LONG:
        case PL_DATA_TYPE_LONG:  return sizeof(uint64_t);

        case PL_DATA_TYPE_UNSIGNED_BYTE2:
        case PL_DATA_TYPE_BYTE2:  return 2 * sizeof(uint8_t);

        case PL_DATA_TYPE_UNSIGNED_SHORT2:
        case PL_DATA_TYPE_SHORT2: return 2 * sizeof(uint16_t);

        case PL_DATA_TYPE_UNSIGNED_INT2:
        case PL_DATA_TYPE_INT2:   return 2 * sizeof(uint32_t);

        case PL_DATA_TYPE_UNSIGNED_LONG2:
        case PL_DATA_TYPE_LONG2:  return 2 * sizeof(uint64_t);

        case PL_DATA_TYPE_UNSIGNED_BYTE3:
        case PL_DATA_TYPE_BYTE3:  return 3 * sizeof(uint8_t);

        case PL_DATA_TYPE_UNSIGNED_SHORT3:
        case PL_DATA_TYPE_SHORT3: return 3 * sizeof(uint16_t);

        case PL_DATA_TYPE_UNSIGNED_INT3:
        case PL_DATA_TYPE_INT3:   return 3 * sizeof(uint32_t);

        case PL_DATA_TYPE_UNSIGNED_LONG3:
        case PL_DATA_TYPE_LONG3:  return 3 * sizeof(uint64_t);

        case PL_DATA_TYPE_UNSIGNED_BYTE4:
        case PL_DATA_TYPE_BYTE4:  return 4 * sizeof(uint8_t);

        case PL_DATA_TYPE_UNSIGNED_SHORT4:
        case PL_DATA_TYPE_SHORT4: return 4 * sizeof(uint16_t);

        case PL_DATA_TYPE_UNSIGNED_INT4:
        case PL_DATA_TYPE_INT4:   return 4 * sizeof(uint32_t);

        case PL_DATA_TYPE_UNSIGNED_LONG4:
        case PL_DATA_TYPE_LONG4:  return 4 * sizeof(uint64_t);
    }

    PL_ASSERT(false && "Unsupported data type");
    return 0;
}

static plBlendState
pl__get_blend_state(plBlendMode tBlendMode)
{

    static const plBlendState atStateMap[PL_BLEND_MODE_COUNT] =
    {
        // PL_BLEND_MODE_NONE
        { 
            .bBlendEnabled = false,
        },

        // PL_BLEND_MODE_ALPHA
        {
            .bBlendEnabled   = true,
            .tSrcColorFactor = PL_BLEND_FACTOR_SRC_ALPHA,
            .tDstColorFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .tColorOp        = PL_BLEND_OP_ADD,
            .tSrcAlphaFactor = PL_BLEND_FACTOR_SRC_ALPHA,
            .tDstAlphaFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .tAlphaOp        = PL_BLEND_OP_ADD
        },

        // PL_BLEND_MODE_ADDITIVE
        {
            .bBlendEnabled   = true,
            .tSrcColorFactor = PL_BLEND_FACTOR_SRC_ALPHA,
            .tDstColorFactor = PL_BLEND_FACTOR_ONE,
            .tColorOp        = PL_BLEND_OP_ADD,
            .tSrcAlphaFactor = PL_BLEND_FACTOR_SRC_ALPHA,
            .tDstAlphaFactor = PL_BLEND_FACTOR_ONE,
            .tAlphaOp        = PL_BLEND_OP_ADD
        },

        // PL_BLEND_MODE_PREMULTIPLY
        {
            .bBlendEnabled   = true,
            .tSrcColorFactor = PL_BLEND_FACTOR_ONE,
            .tDstColorFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .tColorOp        = PL_BLEND_OP_ADD,
            .tSrcAlphaFactor = PL_BLEND_FACTOR_ONE,
            .tDstAlphaFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .tAlphaOp        = PL_BLEND_OP_ADD
        },

        // PL_BLEND_MODE_MULTIPLY
        {
            .bBlendEnabled   = true,
            .tSrcColorFactor = PL_BLEND_FACTOR_DST_COLOR,
            .tDstColorFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .tColorOp        = PL_BLEND_OP_ADD,
            .tSrcAlphaFactor = PL_BLEND_FACTOR_DST_ALPHA,
            .tDstAlphaFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .tAlphaOp        = PL_BLEND_OP_ADD
        },

        // PL_BLEND_MODE_CLIP_MASK
        {
            .bBlendEnabled   = true,
            .tSrcColorFactor = PL_BLEND_FACTOR_ZERO,
            .tDstColorFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .tColorOp        = PL_BLEND_OP_ADD,
            .tSrcAlphaFactor = PL_BLEND_FACTOR_DST_ALPHA,
            .tDstAlphaFactor = PL_BLEND_FACTOR_ZERO,
            .tAlphaOp        = PL_BLEND_OP_ADD
        }
    };

    PL_ASSERT(tBlendMode < PL_BLEND_MODE_COUNT && "blend mode out of range");
    return atStateMap[tBlendMode];
}

static void
pl_add_drawable_objects_to_scene(uint32_t uSceneHandle, uint32_t uOpaqueCount, const plEntity* atOpaqueObjects, uint32_t uTransparentCount, const plEntity* atTransparentObjects)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];

    const uint32_t uTransparentStart = pl_sb_size(ptScene->tLoaderData.sbtTransparentDrawables);
    pl_sb_add_n(ptScene->tLoaderData.sbtTransparentDrawables, uTransparentCount);

    const uint32_t uOpaqueStart = pl_sb_size(ptScene->tLoaderData.sbtOpaqueDrawables);
    pl_sb_add_n(ptScene->tLoaderData.sbtOpaqueDrawables, uOpaqueCount);

    for(uint32_t i = 0; i < uOpaqueCount; i++)
        ptScene->tLoaderData.sbtOpaqueDrawables[uOpaqueStart + i].tEntity = atOpaqueObjects[i];

    for(uint32_t i = 0; i < uTransparentCount; i++)
        ptScene->tLoaderData.sbtTransparentDrawables[uTransparentStart + i].tEntity = atTransparentObjects[i];
}

//-----------------------------------------------------------------------------
// [SECTION] public API implementation
//-----------------------------------------------------------------------------

const plRefRendererI*
pl_load_ref_renderer_api(void)
{
    static const plRefRendererI tApi = {
        .initialize                    = pl_refr_initialize,
        .cleanup                       = pl_refr_cleanup,
        .create_scene                  = pl_refr_create_scene,
        .add_drawable_objects_to_scene = pl_add_drawable_objects_to_scene,
        .create_view                   = pl_refr_create_view,
        .run_ecs                       = pl_refr_run_ecs,
        .get_component_library         = pl_refr_get_component_library,
        .get_graphics                  = pl_refr_get_graphics,
        .load_skybox_from_panorama     = pl_refr_load_skybox_from_panorama,
        .finalize_scene                = pl_refr_finalize_scene,
        .update_scene                  = pl_refr_update_scene,
        .render_scene                  = pl_refr_render_scene,
        .get_view_texture_id           = pl_refr_get_view_texture_id,
        .resize_view                   = pl_refr_resize_view,
    };
    return &tApi;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
   gptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
   pl_set_memory_context(gptDataRegistry->get_data(PL_CONTEXT_MEMORY));
   pl_set_profile_context(gptDataRegistry->get_data("profile"));
   pl_set_log_context(gptDataRegistry->get_data("log"));
   pl_set_context(gptDataRegistry->get_data("ui"));

   // apis
   gptResource      = ptApiRegistry->first(PL_API_RESOURCE);
   gptECS           = ptApiRegistry->first(PL_API_ECS);
   gptFile          = ptApiRegistry->first(PL_API_FILE);
   gptDevice        = ptApiRegistry->first(PL_API_DEVICE);
   gptGfx           = ptApiRegistry->first(PL_API_GRAPHICS);
   gptCamera        = ptApiRegistry->first(PL_API_CAMERA);
   gptStream        = ptApiRegistry->first(PL_API_DRAW_STREAM);
   gptImage         = ptApiRegistry->first(PL_API_IMAGE);
   gptStats         = ptApiRegistry->first(PL_API_STATS);
   gptGpuAllocators = ptApiRegistry->first(PL_API_GPU_ALLOCATORS);
   gptThreads       = ptApiRegistry->first(PL_API_THREADS);
   gptJob           = ptApiRegistry->first(PL_API_JOB);

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
pl_unload_ext(plApiRegistryI* ptApiRegistry)
{
}