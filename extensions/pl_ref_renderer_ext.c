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
// [SECTION] internal API implementation
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
#define PL_MAX_LIGHTS 1000

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plShaderVariant
{
    plGraphicsState tGraphicsState;
    const void*     pTempConstantData;
} plShaderVariant;

typedef struct _plComputeShaderVariant
{
    const void* pTempConstantData;
} plComputeShaderVariant;

typedef struct _plSkinData
{
    plEntity              tEntity;
    plTextureHandle       atDynamicTexture[PL_FRAMES_IN_FLIGHT];
    plBindGroupHandle     tTempBindGroup;
    plComputeShaderHandle tShader;
    uint32_t              uVertexCount;
    int                   iSourceDataOffset;
    int                   iDestDataOffset;
    int                   iDestVertexOffset;
} plSkinData;

typedef struct _plDrawable
{
    plEntity          tEntity;
    plBindGroupHandle tMaterialBindGroup;
    uint32_t          uDataOffset;
    uint32_t          uVertexOffset;
    uint32_t          uVertexCount;
    uint32_t          uIndexOffset;
    uint32_t          uIndexCount;
    uint32_t          uMaterialIndex;
    plShaderHandle    tShader;
    uint32_t          uSkinIndex;
    bool              bCulled;
} plDrawable;

typedef struct _plGPUMaterial
{
    // Metallic Roughness
    int   iMipCount;
    float fMetallicFactor;
    float fRoughnessFactor;
    int _unused0[1];
    plVec4 tBaseColorFactor;

    // // Clearcoat
    float fClearcoatFactor;
    float fClearcoatRoughnessFactor;
    int _unused1[2];

    // Specular
    plVec3 tKHR_materials_specular_specularColorFactor;
    float fKHR_materials_specular_specularFactor;

    // // Iridescence
    float fIridescenceFactor;
    float fIridescenceIor;
    float fIridescenceThicknessMinimum;
    float fIridescenceThicknessMaximum;

    // Emissive Strength
    plVec3 tEmissiveFactor;
    float  fEmissiveStrength;
    
    // IOR
    float fIor;

    // Alpha mode
    float fAlphaCutoff;
    float fOcclusionStrength;
    int _unused2[1];

    int iBaseColorUVSet;
    int iNormalUVSet;
    int iEmissiveUVSet;
    int iOcclusionUVSet;
    int iMetallicRoughnessUVSet;
    int iClearcoatUVSet;
    int iClearcoatRoughnessUVSet;
    int iClearcoatNormalUVSet;
    int iSpecularUVSet;
    int iSpecularColorUVSet;
    int iIridescenceUVSet;
    int iIridescenceThicknessUVSet;
} plGPUMaterial;

typedef struct _plGPULight
{
    plVec3 tPosition;
    float  fIntensity;

    plVec3 tDirection;
    int    iType;

    plVec3 tColor;
    float  fRange;
} plGPULight;

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
    // main renderpass
    plRenderPassHandle tRenderPass;
    plVec2             tTargetSize;

    // g-buffer textures
    plTextureHandle tAlbedoTexture[PL_FRAMES_IN_FLIGHT];
    plTextureHandle tPositionTexture[PL_FRAMES_IN_FLIGHT];
    plTextureHandle tNormalTexture[PL_FRAMES_IN_FLIGHT];
    plTextureHandle tDepthTexture[PL_FRAMES_IN_FLIGHT];
    plTextureHandle tEmissiveTexture[PL_FRAMES_IN_FLIGHT];
    plTextureHandle tAOMetalRoughnessTexture[PL_FRAMES_IN_FLIGHT];

    // output texture
    plTextureHandle tFinalTexture[PL_FRAMES_IN_FLIGHT];
    plTextureId     tFinalTextureID[PL_FRAMES_IN_FLIGHT]; // for showing in UI

    // lighting
    plBindGroupHandle tLightingBindGroup[PL_FRAMES_IN_FLIGHT];

    // GPU buffers
    plBufferHandle atGlobalBuffers[PL_FRAMES_IN_FLIGHT];

    // submitted drawables
    plDrawable* sbtVisibleOpaqueDrawables;
    plDrawable* sbtVisibleTransparentDrawables;

    // drawing api
    plDrawList3D t3DDrawList;
    plDrawList3D t3DSelectionDrawList;
} plRefView;

typedef struct _plRefScene
{
    plShaderHandle tLightingShader;

    // skybox resources (optional)
    int               iEnvironmentMips;
    plDrawable        tSkyboxDrawable;
    plTextureHandle   tSkyboxTexture;
    plBindGroupHandle tSkyboxBindGroup;
    plTextureHandle   tGGXLUTTexture;
    plTextureHandle   tLambertianEnvTexture;
    plTextureHandle   tGGXEnvTexture;

    // lighting (final quad to use for composition)
    plDrawable tLightingDrawable;

    // shared bind groups
    plBindGroupHandle tSkinBindGroup0;

    // CPU buffers
    plVec3*        sbtVertexPosBuffer;
    plVec4*        sbtVertexDataBuffer;
    uint32_t*      sbuIndexBuffer;
    plGPUMaterial* sbtMaterialBuffer;
    plVec4*        sbtSkinVertexDataBuffer;
    plGPULight*    sbtLightData;

    // GPU buffers
    plBufferHandle tVertexBuffer;
    plBufferHandle tIndexBuffer;
    plBufferHandle tStorageBuffer;
    plBufferHandle tMaterialDataBuffer;
    plBufferHandle tSkinStorageBuffer;
    plBufferHandle atLightBuffer[PL_MAX_VIEWS_PER_SCENE];

    // views
    uint32_t    uViewCount;
    plRefView   atViews[PL_MAX_VIEWS_PER_SCENE];
    plSkinData* sbtSkinData;

    // ECS component library
    plComponentLibrary tComponentLibrary;

    // drawables (per scene, will be culled by views)
    plDrawable* sbtOpaqueDrawables;
    plDrawable* sbtTransparentDrawables;
    plDrawable* sbtOutlineDrawables;
    plShaderHandle* sbtOutlineDrawablesOldShaders;

    // hashmaps
    plHashMap tOpaqueHashmap;
    plHashMap tTransparentHashmap;

} plRefScene;

typedef struct _plRefRendererData
{

    plGraphics tGraphics;

    // main renderpass layout (used as a template for views)
    plRenderPassLayoutHandle tRenderPassLayout;

    // shader templates (variants are made from these)
    plShaderHandle tOpaqueShader;
    plShaderHandle tTransparentShader;
    plShaderHandle tSkyboxShader;
    plShaderHandle tOutlineShader;

    // graphics shader variant system
    uint32_t               uVariantCount;
    const plShaderVariant* ptVariants;
    plHashMap              tVariantHashmap;
    plShaderHandle*        _sbtVariantHandles; // needed for cleanup

    // renderer specific log channel
    uint32_t uLogChannel;

    // GPU allocators
    plDeviceMemoryAllocatorI* ptLocalDedicatedAllocator;
    plDeviceMemoryAllocatorI* ptLocalBuddyAllocator;
    plDeviceMemoryAllocatorI* ptStagingUnCachedAllocator;

    // default textures & samplers & bindgroups
    plSamplerHandle   tDefaultSampler;
    plSamplerHandle   tEnvSampler;
    plTextureHandle   tDummyTexture;
    plTextureHandle   tDummyTextureCube;

    // scenes
    plRefScene* sbtScenes;

    // draw stream data
    plDrawStream tDrawStream;

    // staging (more robust system should replace this)
    plBufferHandle tStagingBufferHandle[PL_FRAMES_IN_FLIGHT];
    uint32_t uStagingOffset;
    uint32_t uCurrentStagingFrameIndex;
} plRefRendererData;

typedef struct _plMemCpyJobData
{
    plBuffer* ptBuffer;
    void*     pDestination;
    size_t    szSize;
} plMemCpyJobData;

enum _plTextureMappingFlags
{
    PL_HAS_BASE_COLOR_MAP            = 1 << 0,
    PL_HAS_NORMAL_MAP                = 1 << 1,
    PL_HAS_EMISSIVE_MAP              = 1 << 2,
    PL_HAS_OCCLUSION_MAP             = 1 << 3,
    PL_HAS_METALLIC_ROUGHNESS_MAP    = 1 << 4
};

enum _plMaterialInfoFlags
{
    PL_INFO_MATERIAL_METALLICROUGHNESS = 1 << 0,
};

enum _plRenderingFlags
{
    PL_RENDERING_FLAG_USE_PUNCTUAL = 1 << 0,
    PL_RENDERING_FLAG_USE_IBL      = 1 << 1
};

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
static const plImageI*         gptImage         = NULL;
static const plStatsI*         gptStats         = NULL;
static const plGPUAllocatorsI* gptGpuAllocators = NULL;
static const plThreadsI*       gptThreads       = NULL;
static const plJobI*           gptJob           = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal API
//-----------------------------------------------------------------------------

// general helpers
static void pl__add_drawable_skin_data_to_global_buffer(plRefScene*, uint32_t uDrawableIndex, plDrawable* atDrawables);
static void pl__add_drawable_data_to_global_buffer(plRefScene*, uint32_t uDrawableIndex, plDrawable* atDrawables);
static bool pl__sat_visibility_test(plCameraComponent*, const plAABB*);

// shader variant system
static plShaderHandle pl__get_shader_variant(uint32_t uSceneHandle, plShaderHandle tHandle, const plShaderVariant* ptVariant);
static size_t         pl__get_data_type_size(plDataType tType);
static plBlendState   pl__get_blend_state(plBlendMode tBlendMode);

// job system tasks
static void pl__refr_job       (uint32_t uJobIndex, void* pData);
static void pl__refr_cull_job  (uint32_t uJobIndex, void* pData);

// resource creation helpers
static plTextureHandle pl__refr_create_texture          (const plTextureDesc* ptDesc, const char* pcName, uint32_t uIdentifier);
static plTextureHandle pl__refr_create_texture_with_data(const plTextureDesc* ptDesc, const char* pcName, uint32_t uIdentifier, const void* pData, size_t szSize);
static plBufferHandle  pl__refr_create_staging_buffer   (const plBufferDescription* ptDesc, const char* pcName, uint32_t uIdentifier);
static plBufferHandle  pl__refr_create_local_buffer     (const plBufferDescription* ptDesc, const char* pcName, uint32_t uIdentifier, const void* pData);

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

static void
pl_refr_initialize(plWindow* ptWindow)
{

    // for convience
    plGraphics* ptGraphics = &gptData->tGraphics;

    // shader default values
    gptData->tSkyboxShader   = (plShaderHandle){UINT32_MAX, UINT32_MAX};

    // load allocators
    gptData->ptLocalBuddyAllocator      = gptGpuAllocators->get_local_buddy_allocator(&ptGraphics->tDevice);
    gptData->ptLocalDedicatedAllocator  = gptGpuAllocators->get_local_dedicated_allocator(&ptGraphics->tDevice);
    gptData->ptStagingUnCachedAllocator = gptGpuAllocators->get_staging_uncached_allocator(&ptGraphics->tDevice);

    // initialize graphics
    const plGraphicsDesc tGraphicsDesc = {
        .bEnableValidation = true
    };
    gptGfx->initialize(ptWindow, &tGraphicsDesc, ptGraphics);
    gptDataRegistry->set_data("device", &ptGraphics->tDevice); // used by debug extension

    // create staging buffer
    const plBufferDescription tStagingBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_STAGING,
        .uByteSize = 268435456
    };
    for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
        gptData->tStagingBufferHandle[i] = pl__refr_create_staging_buffer(&tStagingBufferDesc, "staging", i);

    // create dummy texture
    const plTextureDesc tDummyTextureDesc = {
        .tDimensions   = {2, 2, 1},
        .tFormat       = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED,
    };
    
    const float afDummyTextureData[] = {
        1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 1.0f
    };
    gptData->tDummyTexture = pl__refr_create_texture_with_data(&tDummyTextureDesc, "dummy", 0, afDummyTextureData, sizeof(afDummyTextureData));

        const plTextureDesc tSkyboxTextureDesc = {
            .tDimensions = {1, 1, 1},
            .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
            .uLayers     = 6,
            .uMips       = 1,
            .tType       = PL_TEXTURE_TYPE_CUBE,
            .tUsage      = PL_TEXTURE_USAGE_SAMPLED
        };
    gptData->tDummyTextureCube = pl__refr_create_texture(&tSkyboxTextureDesc, "dummy cube", 0);

    // create default sampler
    const plSamplerDesc tSamplerDesc = {
        .tFilter         = PL_FILTER_LINEAR,
        .fMinMip         = 0.0f,
        .fMaxMip         = 64.0f,
        .tVerticalWrap   = PL_WRAP_MODE_WRAP,
        .tHorizontalWrap = PL_WRAP_MODE_WRAP
    };
    gptData->tDefaultSampler = gptDevice->create_sampler(&ptGraphics->tDevice, &tSamplerDesc, "default sampler");

    const plSamplerDesc tEnvSamplerDesc = {
        .tFilter         = PL_FILTER_NEAREST,
        .fMinMip         = 0.0f,
        .fMaxMip         = 64.0f,
        .tVerticalWrap   = PL_WRAP_MODE_CLAMP,
        .tHorizontalWrap = PL_WRAP_MODE_CLAMP
    };
    gptData->tEnvSampler = gptDevice->create_sampler(&ptGraphics->tDevice, &tEnvSamplerDesc, "ENV sampler");

    // create main render pass layout
    const plRenderPassLayoutDescription tRenderPassLayoutDesc = {
        .atRenderTargets = {
            { .tFormat = PL_FORMAT_D32_FLOAT_S8_UINT },  // depth buffer
            { .tFormat = PL_FORMAT_R32G32B32A32_FLOAT }, // final output
            { .tFormat = PL_FORMAT_R32G32B32A32_FLOAT }, // albedo
            { .tFormat = PL_FORMAT_R32G32B32A32_FLOAT }, // normal
            { .tFormat = PL_FORMAT_R32G32B32A32_FLOAT }, // position
            { .tFormat = PL_FORMAT_R32G32B32A32_FLOAT }, // emissive
            { .tFormat = PL_FORMAT_R32G32B32A32_FLOAT }, // AO, roughness, metallic, specular weight
        },
        .uSubpassCount = 3,
        .atSubpasses = {
            { // G-buffer fill
                .uRenderTargetCount = 6,
                .auRenderTargets = {0, 2, 3, 4, 5, 6}
            },
            { // lighting
                .uRenderTargetCount = 1,
                .auRenderTargets = {1},
                .uSubpassInputCount = 6,
                .auSubpassInputs = {0, 2, 3, 4, 5, 6},
            },
            { // transparencies
                .uRenderTargetCount = 2,
                .auRenderTargets = {0, 1},
                .uSubpassInputCount = 0,
                .auSubpassInputs = {0}
            },
        }
    };
    gptData->tRenderPassLayout = gptDevice->create_render_pass_layout(&gptData->tGraphics.tDevice, &tRenderPassLayoutDesc);

    // create template shaders

    int aiConstantData[6] = {0, 0, 0, 0, 0, 1};

    plShaderDescription tOpaqueShaderDescription = {

        #ifdef PL_METAL_BACKEND
        .pcVertexShader = "../shaders/metal/primitive.metal",
        .pcPixelShader = "../shaders/metal/primitive.metal",
        #else
        .pcVertexShader = "primitive.vert.spv",
        .pcPixelShader = "primitive.frag.spv",
        #endif
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 1,
            .ulDepthMode          = PL_COMPARE_MODE_LESS,
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
        .pTempConstantData = aiConstantData,
        .atBlendStates = {
            pl__get_blend_state(PL_BLEND_MODE_OPAQUE),
            pl__get_blend_state(PL_BLEND_MODE_OPAQUE),
            pl__get_blend_state(PL_BLEND_MODE_OPAQUE),
            pl__get_blend_state(PL_BLEND_MODE_OPAQUE),
            pl__get_blend_state(PL_BLEND_MODE_OPAQUE)
        },
        .uBlendStateCount = 5,
        .tRenderPassLayout = gptData->tRenderPassLayout,
        .uSubpassIndex = 0,
        .uBindGroupLayoutCount = 2,
        .atBindGroupLayouts = {
            {
                .uBufferBindingCount  = 3,
                .aBufferBindings = {
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
                .uSamplerBindingCount = 2,
                .atSamplerBindings = {
                    {.uSlot = 3, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                    {.uSlot = 4, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                },
                .uTextureBindingCount  = 3,
                .atTextureBindings = {
                    {.uSlot =   5, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   6, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   7, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1}
                }
            },
            {
                .uTextureBindingCount  = 12,
                .atTextureBindings = {
                    {.uSlot =   0, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   1, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   2, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   3, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   4, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   5, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   6, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   7, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   8, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   9, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =  10, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =  11, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                }
            }
        }
    };
    for(uint32_t i = 0; i < tOpaqueShaderDescription.uConstantCount; i++)
    {
        tOpaqueShaderDescription.atConstants[i].uID = i;
        tOpaqueShaderDescription.atConstants[i].uOffset = i * sizeof(int);
        tOpaqueShaderDescription.atConstants[i].tType = PL_DATA_TYPE_INT;
    }
    gptData->tOpaqueShader = gptDevice->create_shader(&gptData->tGraphics.tDevice, &tOpaqueShaderDescription);

    plShaderDescription tTransparentShaderDescription = {

        #ifdef PL_METAL_BACKEND
        .pcVertexShader = "../shaders/metal/transparent.metal",
        .pcPixelShader = "../shaders/metal/transparent.metal",
        #else
        .pcVertexShader = "transparent.vert.spv",
        .pcPixelShader = "transparent.frag.spv",
        #endif
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 0,
            .ulDepthMode          = PL_COMPARE_MODE_LESS_OR_EQUAL,
            .ulCullMode           = PL_CULL_MODE_NONE,
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
        .uConstantCount = 6,
        .pTempConstantData = aiConstantData,
        .atBlendStates = {
            pl__get_blend_state(PL_BLEND_MODE_ALPHA)
        },
        .uBlendStateCount = 1,
        .tRenderPassLayout = gptData->tRenderPassLayout,
        .uSubpassIndex = 2,
        .uBindGroupLayoutCount = 3,
        .atBindGroupLayouts = {
            {
                .uBufferBindingCount  = 3,
                .aBufferBindings = {
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
                .uSamplerBindingCount = 2,
                .atSamplerBindings = {
                    {.uSlot = 3, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                    {.uSlot = 4, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                },
                .uTextureBindingCount  = 3,
                .atTextureBindings = {
                    {.uSlot =   5, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   6, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   7, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1}
                }
            },
            {
                .uBufferBindingCount  = 1,
                .aBufferBindings = {
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                        .uSlot = 0,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    }
                },
            },
            {
                .uTextureBindingCount  = 12,
                .atTextureBindings = {
                    {.uSlot =   0, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   1, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   2, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   3, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   4, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   5, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   6, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   7, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   8, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   9, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =  10, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =  11, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                }
            }
        }
    };
    for(uint32_t i = 0; i < tTransparentShaderDescription.uConstantCount; i++)
    {
        tTransparentShaderDescription.atConstants[i].uID = i;
        tTransparentShaderDescription.atConstants[i].uOffset = i * sizeof(int);
        tTransparentShaderDescription.atConstants[i].tType = PL_DATA_TYPE_INT;
    }
    gptData->tTransparentShader = gptDevice->create_shader(&gptData->tGraphics.tDevice, &tTransparentShaderDescription);

    plShaderDescription tOutlineShaderDescription = {

        #ifdef PL_METAL_BACKEND
        .pcVertexShader = "../shaders/metal/outline.metal",
        .pcPixelShader = "../shaders/metal/outline.metal",
        #else
        .pcVertexShader = "outline.vert.spv",
        .pcPixelShader = "outline.frag.spv",
        #endif
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 0,
            .ulDepthMode          = PL_COMPARE_MODE_ALWAYS,
            .ulCullMode           = PL_CULL_MODE_CULL_FRONT,
            .ulWireframe          = 0,
            .ulStencilMode        = PL_COMPARE_MODE_LESS_OR_EQUAL,
            .ulStencilRef         = 128,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .tVertexBufferBinding = {
            .uByteStride = sizeof(float) * 3,
            .atAttributes = { {.uByteOffset = 0, .tFormat = PL_FORMAT_R32G32B32_FLOAT}}
        },
        .uConstantCount = 2,
        .pTempConstantData = aiConstantData,
        .atBlendStates = {
            pl__get_blend_state(PL_BLEND_MODE_ALPHA)
        },
        .uBlendStateCount = 1,
        .tRenderPassLayout = gptData->tRenderPassLayout,
        .uSubpassIndex = 2,
        .uBindGroupLayoutCount = 1,
        .atBindGroupLayouts = {
            {
                .uBufferBindingCount  = 2,
                .aBufferBindings = {
                    { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                    { .uSlot = 1, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                },
            }
        }
    };
    for(uint32_t i = 0; i < tOutlineShaderDescription.uConstantCount; i++)
    {
        tOutlineShaderDescription.atConstants[i].uID = i;
        tOutlineShaderDescription.atConstants[i].uOffset = i * sizeof(int);
        tOutlineShaderDescription.atConstants[i].tType = PL_DATA_TYPE_INT;
    }
    gptData->tOutlineShader = gptDevice->create_shader(&gptData->tGraphics.tDevice, &tOutlineShaderDescription);
}

static uint32_t
pl_refr_create_scene(void)
{
    const uint32_t uSceneHandle = pl_sb_size(gptData->sbtScenes);
    plRefScene tScene = {0};
    pl_sb_push(gptData->sbtScenes, tScene);
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];

    // initialize ecs library
    gptECS->init_component_library(&ptScene->tComponentLibrary);

    // buffer default values
    ptScene->tVertexBuffer       = (plBufferHandle){UINT32_MAX, UINT32_MAX};
    ptScene->tIndexBuffer        = (plBufferHandle){UINT32_MAX, UINT32_MAX};
    ptScene->tStorageBuffer      = (plBufferHandle){UINT32_MAX, UINT32_MAX};
    ptScene->tMaterialDataBuffer = (plBufferHandle){UINT32_MAX, UINT32_MAX};
    // ptScene->tLightBuffer        = (plBufferHandle){UINT32_MAX, UINT32_MAX};

    // skybox resources default values
    ptScene->tSkyboxTexture   = (plTextureHandle)  {UINT32_MAX, UINT32_MAX};
    ptScene->tSkyboxBindGroup = (plBindGroupHandle){UINT32_MAX, UINT32_MAX};

    // IBL defaults
    ptScene->tGGXLUTTexture        = (plTextureHandle)  {UINT32_MAX, UINT32_MAX};
    ptScene->tGGXEnvTexture        = (plTextureHandle)  {UINT32_MAX, UINT32_MAX};
    ptScene->tLambertianEnvTexture = (plTextureHandle)  {UINT32_MAX, UINT32_MAX};

    return uSceneHandle;
}

static uint32_t
pl_refr_create_view(uint32_t uSceneHandle, plVec2 tDimensions)
{

    // for convience
    plGraphics* ptGraphics = &gptData->tGraphics;
    plRefScene* ptScene    = &gptData->sbtScenes[uSceneHandle];

    // create view
    const uint32_t uViewHandle = ptScene->uViewCount++;
    PL_ASSERT(uViewHandle < PL_MAX_VIEWS_PER_SCENE);    
    plRefView* ptView = &ptScene->atViews[uViewHandle];

    ptView->tTargetSize = tDimensions;

    // create offscreen per-frame resources
    const plTextureDesc tFinalTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .tInitialUsage = PL_TEXTURE_USAGE_SAMPLED
    };

    const plTextureDesc tAttachmentTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .tInitialUsage = PL_TEXTURE_USAGE_COLOR_ATTACHMENT
    };

    const plTextureDesc tDepthTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_D32_FLOAT_S8_UINT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT | PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .tInitialUsage = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT
    };

    const plBufferDescription atGlobalBuffersDesc = {
        .tUsage    = PL_BUFFER_USAGE_UNIFORM | PL_BUFFER_USAGE_STAGING,
        .uByteSize = PL_DEVICE_ALLOCATION_BLOCK_SIZE
    };

    const plBindGroupLayout tLightingBindGroupLayout = {
        .uTextureBindingCount  = 6,
        .atTextureBindings = { 
            {.uSlot = 0, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 1, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 2, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 3, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 4, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 5, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
        }
    };

    // create offscreen render pass
    plRenderPassAttachments atAttachmentSets[PL_FRAMES_IN_FLIGHT] = {0};

    for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
    {
        // textures
        ptView->tFinalTexture[i]            = pl__refr_create_texture(&tFinalTextureDesc,      "offscreen original",       i);
        ptView->tAlbedoTexture[i]           = pl__refr_create_texture(&tAttachmentTextureDesc, "albedo original",          i);
        ptView->tNormalTexture[i]           = pl__refr_create_texture(&tAttachmentTextureDesc, "normal original",          i);
        ptView->tPositionTexture[i]         = pl__refr_create_texture(&tAttachmentTextureDesc, "position original",        i);
        ptView->tEmissiveTexture[i]         = pl__refr_create_texture(&tAttachmentTextureDesc, "emissive original",        i);
        ptView->tAOMetalRoughnessTexture[i] = pl__refr_create_texture(&tAttachmentTextureDesc, "metalroughness original",  i);
        ptView->tDepthTexture[i]            = pl__refr_create_texture(&tDepthTextureDesc,      "offscreen depth original", i);

        // texture IDs
        ptView->tFinalTextureID[i] = gptGfx->get_ui_texture_handle(ptGraphics, ptView->tFinalTexture[i], gptData->tDefaultSampler);

        // buffers
        ptView->atGlobalBuffers[i] = pl__refr_create_staging_buffer(&atGlobalBuffersDesc, "global", i);

        // lighting bind group
        plTempAllocator tTempAllocator = {0};
        ptView->tLightingBindGroup[i] = gptDevice->create_bind_group(&ptGraphics->tDevice, &tLightingBindGroupLayout, pl_temp_allocator_sprintf(&tTempAllocator, "lighting bind group%u", i));

        const plBindGroupUpdateTextureData atBGTextureData[] = {
            {
                .tTexture = ptView->tAlbedoTexture[i],
                .uSlot    = 0,
                .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            },
            {
                .tTexture = ptView->tNormalTexture[i],
                .uSlot    = 1,
                .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            },
            {
                .tTexture = ptView->tPositionTexture[i],
                .uSlot    = 2,
                .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            },
            {
                .tTexture = ptView->tEmissiveTexture[i],
                .uSlot    = 3,
                .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            },
            {
                .tTexture = ptView->tAOMetalRoughnessTexture[i],
                .uSlot    = 4,
                .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            },
            {
                .tTexture = ptView->tDepthTexture[i],
                .uSlot    = 5,
                .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            }
        };
        const plBindGroupUpdateData tBGData = {
            .uTextureCount = 6,
            .atTextures = atBGTextureData
        };
        gptDevice->update_bind_group(&ptGraphics->tDevice, ptView->tLightingBindGroup[i], &tBGData);
        pl_temp_allocator_free(&tTempAllocator);

        // attachment sets
        atAttachmentSets[i].atViewAttachments[0] = ptView->tDepthTexture[i];
        atAttachmentSets[i].atViewAttachments[1] = ptView->tFinalTexture[i];
        atAttachmentSets[i].atViewAttachments[2] = ptView->tAlbedoTexture[i];
        atAttachmentSets[i].atViewAttachments[3] = ptView->tNormalTexture[i];
        atAttachmentSets[i].atViewAttachments[4] = ptView->tPositionTexture[i];
        atAttachmentSets[i].atViewAttachments[5] = ptView->tEmissiveTexture[i];
        atAttachmentSets[i].atViewAttachments[6] = ptView->tAOMetalRoughnessTexture[i];
    }

    const plRenderPassDescription tRenderPassDesc = {
        .tLayout = gptData->tRenderPassLayout,
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
            },
            {
                .tLoadOp       = PL_LOAD_OP_CLEAR,
                .tStoreOp      = PL_STORE_OP_STORE,
                .tCurrentUsage = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .tNextUsage    = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 0.0f}
            },
            {
                .tLoadOp       = PL_LOAD_OP_CLEAR,
                .tStoreOp      = PL_STORE_OP_STORE,
                .tCurrentUsage = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .tNextUsage    = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 0.0f}
            }
        },
        .tDimensions = {.x = ptView->tTargetSize.x, .y = ptView->tTargetSize.y}
    };
    ptView->tRenderPass = gptDevice->create_render_pass(&ptGraphics->tDevice, &tRenderPassDesc, atAttachmentSets);

    // register debug 3D drawlist
    gptGfx->register_3d_drawlist(ptGraphics, &ptView->t3DDrawList);
    gptGfx->register_3d_drawlist(ptGraphics, &ptView->t3DSelectionDrawList);

    // create lighting composition quad
    const uint32_t uVertexStartIndex = pl_sb_size(ptScene->sbtVertexPosBuffer);
    const plDrawable tLightingDrawable = {
        .uIndexCount   = 6,
        .uVertexCount  = 4,
        .uIndexOffset  = pl_sb_size(ptScene->sbuIndexBuffer),
        .uVertexOffset = uVertexStartIndex,
        .uDataOffset   = pl_sb_size(ptScene->sbtVertexDataBuffer),
    };
    ptScene->tLightingDrawable = tLightingDrawable;

    pl_sb_push(ptScene->sbuIndexBuffer, uVertexStartIndex + 0);
    pl_sb_push(ptScene->sbuIndexBuffer, uVertexStartIndex + 1);
    pl_sb_push(ptScene->sbuIndexBuffer, uVertexStartIndex + 2);
    pl_sb_push(ptScene->sbuIndexBuffer, uVertexStartIndex + 0);
    pl_sb_push(ptScene->sbuIndexBuffer, uVertexStartIndex + 2);
    pl_sb_push(ptScene->sbuIndexBuffer, uVertexStartIndex + 3);

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
    plDevice*   ptDevice   = &ptGraphics->tDevice;
    plRefScene* ptScene    = &gptData->sbtScenes[uSceneHandle];
    plRefView*  ptView     = &ptScene->atViews[uViewHandle];

    // update offscreen size to match viewport
    ptView->tTargetSize = tDimensions;

    // recreate offscreen color & depth textures
    const plTextureDesc tFinalTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .tInitialUsage = PL_TEXTURE_USAGE_SAMPLED
    };

    const plTextureDesc tAttachmentTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .tInitialUsage = PL_TEXTURE_USAGE_COLOR_ATTACHMENT
    };

    const plTextureDesc tDepthTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_D32_FLOAT_S8_UINT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT | PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .tInitialUsage = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT
    };

    const plBindGroupLayout tLightingBindGroupLayout = {
        .uTextureBindingCount  = 6,
        .atTextureBindings = { 
            {.uSlot = 0, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 1, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 2, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 3, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 4, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 5, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
        }
    };

    // update offscreen render pass attachments
    plRenderPassAttachments atAttachmentSets[PL_FRAMES_IN_FLIGHT] = {0};

    for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
    {

        // queue old resources for deletion
        gptDevice->queue_texture_for_deletion(ptDevice, ptView->tFinalTexture[i]);
        gptDevice->queue_texture_for_deletion(ptDevice, ptView->tAlbedoTexture[i]);
        gptDevice->queue_texture_for_deletion(ptDevice, ptView->tNormalTexture[i]);
        gptDevice->queue_texture_for_deletion(ptDevice, ptView->tPositionTexture[i]);
        gptDevice->queue_texture_for_deletion(ptDevice, ptView->tDepthTexture[i]);
        gptDevice->queue_bind_group_for_deletion(ptDevice, ptView->tLightingBindGroup[i]);

        // textures
        ptView->tFinalTexture[i]            = pl__refr_create_texture(&tFinalTextureDesc,      "offscreen",       i);
        ptView->tAlbedoTexture[i]           = pl__refr_create_texture(&tAttachmentTextureDesc, "albedo",          i);
        ptView->tNormalTexture[i]           = pl__refr_create_texture(&tAttachmentTextureDesc, "normal",          i);
        ptView->tPositionTexture[i]         = pl__refr_create_texture(&tAttachmentTextureDesc, "position",        i);
        ptView->tEmissiveTexture[i]         = pl__refr_create_texture(&tAttachmentTextureDesc, "emissive",        i);
        ptView->tAOMetalRoughnessTexture[i] = pl__refr_create_texture(&tAttachmentTextureDesc, "metalroughness",  i);
        ptView->tDepthTexture[i]            = pl__refr_create_texture(&tDepthTextureDesc,      "offscreen depth", i);

        // texture IDs
        ptView->tFinalTextureID[i] = gptGfx->get_ui_texture_handle(ptGraphics, ptView->tFinalTexture[i], gptData->tDefaultSampler);

        // lighting bind group
        plTempAllocator tTempAllocator = {0};
        ptView->tLightingBindGroup[i] = gptDevice->create_bind_group(&ptGraphics->tDevice, &tLightingBindGroupLayout, pl_temp_allocator_sprintf(&tTempAllocator, "lighting bind group%u", i));
        const plBindGroupUpdateTextureData atBGTextureData[] = {
            {
                .tTexture = ptView->tAlbedoTexture[i],
                .uSlot    = 0,
                .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            },
            {
                .tTexture = ptView->tNormalTexture[i],
                .uSlot    = 1,
                .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            },
            {
                .tTexture = ptView->tPositionTexture[i],
                .uSlot    = 2,
                .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            },
            {
                .tTexture = ptView->tEmissiveTexture[i],
                .uSlot    = 3,
                .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            },
            {
                .tTexture = ptView->tAOMetalRoughnessTexture[i],
                .uSlot    = 4,
                .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            },
            {
                .tTexture = ptView->tDepthTexture[i],
                .uSlot    = 5,
                .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            }
        };
        const plBindGroupUpdateData tBGData = {
            .uTextureCount = 6,
            .atTextures = atBGTextureData
        };
        gptDevice->update_bind_group(&ptGraphics->tDevice, ptView->tLightingBindGroup[i], &tBGData);
        pl_temp_allocator_free(&tTempAllocator);

        // attachment sets
        atAttachmentSets[i].atViewAttachments[0] = ptView->tDepthTexture[i];
        atAttachmentSets[i].atViewAttachments[1] = ptView->tFinalTexture[i];
        atAttachmentSets[i].atViewAttachments[2] = ptView->tAlbedoTexture[i];
        atAttachmentSets[i].atViewAttachments[3] = ptView->tNormalTexture[i];
        atAttachmentSets[i].atViewAttachments[4] = ptView->tPositionTexture[i];
        atAttachmentSets[i].atViewAttachments[5] = ptView->tEmissiveTexture[i];
        atAttachmentSets[i].atViewAttachments[6] = ptView->tAOMetalRoughnessTexture[i];
    }
    gptDevice->update_render_pass_attachments(ptDevice, ptView->tRenderPass, ptView->tTargetSize, atAttachmentSets);
}

static void
pl_refr_cleanup(void)
{
    gptGfx->cleanup_draw_stream(&gptData->tDrawStream);

    for(uint32_t i = 0; i < pl_sb_size(gptData->sbtScenes); i++)
    {
        plRefScene* ptScene = &gptData->sbtScenes[i];
        for(uint32_t j = 0; j < ptScene->uViewCount; j++)
        {
            plRefView* ptView = &ptScene->atViews[j];
            pl_sb_free(ptView->sbtVisibleOpaqueDrawables);
            pl_sb_free(ptView->sbtVisibleTransparentDrawables);
        }
        pl_sb_free(ptScene->sbtVertexPosBuffer);
        pl_sb_free(ptScene->sbtVertexDataBuffer);
        pl_sb_free(ptScene->sbuIndexBuffer);
        pl_sb_free(ptScene->sbtMaterialBuffer);
        pl_sb_free(ptScene->sbtOpaqueDrawables);
        pl_sb_free(ptScene->sbtTransparentDrawables);
        pl_sb_free(ptScene->sbtSkinData);
        pl_sb_free(ptScene->sbtSkinVertexDataBuffer);
        pl_sb_free(ptScene->sbtOutlineDrawables);
        pl_sb_free(ptScene->sbtOutlineDrawablesOldShaders);
        pl_hm_free(&ptScene->tOpaqueHashmap);
        pl_hm_free(&ptScene->tTransparentHashmap);
        gptECS->cleanup_component_library(&ptScene->tComponentLibrary);
    }
    for(uint32_t i = 0; i < pl_sb_size(gptData->_sbtVariantHandles); i++)
    {
        gptDevice->queue_shader_for_deletion(&gptData->tGraphics.tDevice, gptData->_sbtVariantHandles[i]);
    }
    pl_sb_free(gptData->_sbtVariantHandles);
    pl_hm_free(&gptData->tVariantHashmap);
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
    return &ptScene->tComponentLibrary;
}

static plGraphics*
pl_refr_get_graphics(void)
{
    return &gptData->tGraphics;
}

static void
pl_refr_load_skybox_from_panorama(uint32_t uSceneHandle, const char* pcPath, int iResolution)
{
    pl_begin_profile_sample(__FUNCTION__);
    const int iSamples = 512;
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plGraphics* ptGraphics = &gptData->tGraphics;
    plDevice* ptDevice = &ptGraphics->tDevice;

    // create skybox shader if we haven't
    if(gptData->tSkyboxShader.uIndex == UINT32_MAX)
    {
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
                pl__get_blend_state(PL_BLEND_MODE_OPAQUE)
            },
            .uBlendStateCount = 1,
            .tRenderPassLayout = gptData->tRenderPassLayout,
            .uSubpassIndex = 2,
            .uBindGroupLayoutCount = 2,
            .atBindGroupLayouts = {
                {
                    .uBufferBindingCount  = 3,
                    .aBufferBindings = {
                        { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,  .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                        { .uSlot = 1, .tType = PL_BUFFER_BINDING_TYPE_STORAGE,  .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                        { .uSlot = 2, .tType = PL_BUFFER_BINDING_TYPE_STORAGE,  .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                    },
                    .uSamplerBindingCount = 2,
                    .atSamplerBindings = {
                        {.uSlot = 3, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                        {.uSlot = 4, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                    },
                    .uTextureBindingCount  = 3,
                    .atTextureBindings = {
                        {.uSlot = 5, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                        {.uSlot = 6, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                        {.uSlot = 7, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1}
                    }
                },
                {
                    .uTextureBindingCount = 1,
                    .atTextureBindings = {
                        { .uSlot = 0, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                    },
                }
            }
        };
        gptData->tSkyboxShader = gptDevice->create_shader(&ptGraphics->tDevice, &tSkyboxShaderDesc);
    }

    int iPanoramaWidth = 0;
    int iPanoramaHeight = 0;
    int iUnused = 0;
    float* pfPanoramaData = gptImage->load_hdr(pcPath, &iPanoramaWidth, &iPanoramaHeight, &iUnused, 4);
    PL_ASSERT(pfPanoramaData);

    ptScene->iEnvironmentMips = (uint32_t)floorf(log2f((float)pl_maxi(iResolution, iResolution))) - 3; // guarantee final dispatch during filtering is 16 threads

    const size_t uFaceSize = ((size_t)iResolution * (size_t)iResolution) * 4 * sizeof(float);

    {
        int aiSkyboxSpecializationData[] = {iResolution, iPanoramaWidth, iPanoramaHeight};
        const plComputeShaderDescription tSkyboxComputeShaderDesc = {
            #ifdef PL_METAL_BACKEND
            .pcShader = "panorama_to_cubemap.metal",
            .pcShaderEntryFunc = "kernel_main",
            #else
            .pcShader = "panorama_to_cubemap.comp.spv",
            .pcShaderEntryFunc = "main",
            #endif
            .uConstantCount = 3,
            .pTempConstantData = aiSkyboxSpecializationData,
            .atConstants = {
                { .uID = 0, .uOffset = 0,               .tType = PL_DATA_TYPE_INT},
                { .uID = 1, .uOffset = sizeof(int),     .tType = PL_DATA_TYPE_INT},
                { .uID = 2, .uOffset = 2 * sizeof(int), .tType = PL_DATA_TYPE_INT}
            },
            .uBindGroupLayoutCount = 1,
            .atBindGroupLayouts = {
                {
                    .uBufferBindingCount = 7,
                    .aBufferBindings = {
                        { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 0, .tStages = PL_STAGE_COMPUTE},
                        { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 1, .tStages = PL_STAGE_COMPUTE},
                        { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 2, .tStages = PL_STAGE_COMPUTE},
                        { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 3, .tStages = PL_STAGE_COMPUTE},
                        { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 4, .tStages = PL_STAGE_COMPUTE},
                        { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 5, .tStages = PL_STAGE_COMPUTE},
                        { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 6, .tStages = PL_STAGE_COMPUTE},
                    }
                }
            }
        };
        plComputeShaderHandle tPanoramaShader = gptDevice->create_compute_shader(ptDevice, &tSkyboxComputeShaderDesc);

        plBufferHandle atComputeBuffers[7] = {0};
        const uint32_t uPanoramaSize = iPanoramaHeight * iPanoramaWidth * 4 * sizeof(float);
        const plBufferDescription tInputBufferDesc = {
            .tUsage    = PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_STAGING,
            .uByteSize = uPanoramaSize
        };
        atComputeBuffers[0] = pl__refr_create_staging_buffer(&tInputBufferDesc, "panorama input", 0);
        plBuffer* ptComputeBuffer = gptDevice->get_buffer(ptDevice, atComputeBuffers[0]);
        memcpy(ptComputeBuffer->tMemoryAllocation.pHostMapped, pfPanoramaData, uPanoramaSize);

        const plBufferDescription tOutputBufferDesc = {
            .tUsage    = PL_BUFFER_USAGE_STORAGE,
            .uByteSize = (uint32_t)uFaceSize
        };
        
        for(uint32_t i = 0; i < 6; i++)
            atComputeBuffers[i + 1] = pl__refr_create_local_buffer(&tOutputBufferDesc, "panorama output", i, NULL);

        plBindGroupLayout tComputeBindGroupLayout = {
            .uBufferBindingCount = 7,
            .aBufferBindings = {
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
        const plBindGroupUpdateBufferData atBGBufferData[] = {
            { .uSlot = 0, .tBuffer = atComputeBuffers[0], .szBufferRange = uPanoramaSize},
            { .uSlot = 1, .tBuffer = atComputeBuffers[1], .szBufferRange = uFaceSize},
            { .uSlot = 2, .tBuffer = atComputeBuffers[2], .szBufferRange = uFaceSize},
            { .uSlot = 3, .tBuffer = atComputeBuffers[3], .szBufferRange = uFaceSize},
            { .uSlot = 4, .tBuffer = atComputeBuffers[4], .szBufferRange = uFaceSize},
            { .uSlot = 5, .tBuffer = atComputeBuffers[5], .szBufferRange = uFaceSize},
            { .uSlot = 6, .tBuffer = atComputeBuffers[6], .szBufferRange = uFaceSize}
        };
        const plBindGroupUpdateData tBGData = {
            .uBufferCount = 7,
            .atBuffers = atBGBufferData
        };
        gptDevice->update_bind_group(ptDevice, tComputeBindGroup, &tBGData);

        // calculate cubemap data
        const plDispatch tDispach = {
            .uGroupCountX     = (uint32_t)iResolution / 16,
            .uGroupCountY     = (uint32_t)iResolution / 16,
            .uGroupCountZ     = 2,
            .uThreadPerGroupX = 16,
            .uThreadPerGroupY = 16,
            .uThreadPerGroupZ = 3
        };
        plCommandBuffer tCommandBuffer = gptGfx->begin_command_recording(ptGraphics, NULL);
        plComputeEncoder tComputeEncoder = gptGfx->begin_compute_pass(ptGraphics, &tCommandBuffer);
        gptGfx->bind_compute_bind_groups(&tComputeEncoder, tPanoramaShader, 0, 1, &tComputeBindGroup, NULL);
        gptGfx->bind_compute_shader(&tComputeEncoder, tPanoramaShader);
        gptGfx->dispatch(&tComputeEncoder, 1, &tDispach);
        gptGfx->end_compute_pass(&tComputeEncoder);
        gptGfx->end_command_recording(ptGraphics, &tCommandBuffer);
        gptGfx->submit_command_buffer_blocking(ptGraphics, &tCommandBuffer, NULL);
        gptDevice->queue_compute_shader_for_deletion(ptDevice, tPanoramaShader);

        const plTextureDesc tSkyboxTextureDesc = {
            .tDimensions = {(float)iResolution, (float)iResolution, 1},
            .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
            .uLayers     = 6,
            .uMips       = 1,
            .tType       = PL_TEXTURE_TYPE_CUBE,
            .tUsage      = PL_TEXTURE_USAGE_SAMPLED
        };
        ptScene->tSkyboxTexture = pl__refr_create_texture(&tSkyboxTextureDesc, "skybox texture", uSceneHandle);

        tCommandBuffer = gptGfx->begin_command_recording(ptGraphics, NULL);
        plBlitEncoder tBlitEncoder = gptGfx->begin_blit_pass(ptGraphics, &tCommandBuffer);
        for(uint32_t i = 0; i < 6; i++)
        {
            const plBufferImageCopy tBufferImageCopy = {
                .tImageExtent   = (plExtent){iResolution, iResolution, 1},
                .uLayerCount    = 1,
                .szBufferOffset = 0,
                .uBaseArrayLayer = i,
            };
            gptGfx->copy_buffer_to_texture(&tBlitEncoder, atComputeBuffers[i + 1], ptScene->tSkyboxTexture, 1, &tBufferImageCopy);
        }
        gptGfx->end_blit_pass(&tBlitEncoder);
        gptGfx->end_command_recording(ptGraphics, &tCommandBuffer);
        gptGfx->submit_command_buffer_blocking(ptGraphics, &tCommandBuffer, NULL);
        
        for(uint32_t i = 0; i < 7; i++)
            gptDevice->destroy_buffer(ptDevice, atComputeBuffers[i]);

        gptImage->free(pfPanoramaData);

        plBindGroupLayout tSkyboxBindGroupLayout = {
            .uTextureBindingCount  = 1,
            .atTextureBindings = { {.uSlot = 0, .tStages = PL_STAGE_PIXEL | PL_STAGE_VERTEX, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}}
        };
        ptScene->tSkyboxBindGroup = gptDevice->create_bind_group(ptDevice, &tSkyboxBindGroupLayout, "skybox bind group");
        const plBindGroupUpdateTextureData tTextureData1 = {.tTexture = ptScene->tSkyboxTexture, .uSlot = 0, .uIndex = 0, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED};
        const plBindGroupUpdateData tBGData1 = {
            .uTextureCount = 1,
            .atTextures = &tTextureData1
        };
        gptDevice->update_bind_group(ptDevice, ptScene->tSkyboxBindGroup, &tBGData1);

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

    plComputeShaderDescription tFilterComputeShaderDesc = {
        #ifdef PL_METAL_BACKEND
        .pcShader = "../shaders/metal/filter_environment.metal",
        .pcShaderEntryFunc = "kernel_main",
        #else
        .pcShader = "filter_environment.comp.spv",
        #endif
        .uConstantCount = 8,
        .atConstants = {
            { .uID = 0, .uOffset = 0,  .tType = PL_DATA_TYPE_INT},
            { .uID = 1, .uOffset = 4,  .tType = PL_DATA_TYPE_FLOAT},
            { .uID = 2, .uOffset = 8,  .tType = PL_DATA_TYPE_INT},
            { .uID = 3, .uOffset = 12, .tType = PL_DATA_TYPE_INT},
            { .uID = 4, .uOffset = 16, .tType = PL_DATA_TYPE_FLOAT},
            { .uID = 5, .uOffset = 20, .tType = PL_DATA_TYPE_INT},
            { .uID = 6, .uOffset = 24, .tType = PL_DATA_TYPE_INT},
            { .uID = 7, .uOffset = 28, .tType = PL_DATA_TYPE_INT}
        },
        .uBindGroupLayoutCount = 1,
        .atBindGroupLayouts = {
            {
                .uTextureBindingCount = 1,
                .atTextureBindings = {
                    {.uSlot = 1, .tStages = PL_STAGE_COMPUTE, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
                },
                .uBufferBindingCount = 7,
                .aBufferBindings = {
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 2, .tStages = PL_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 3, .tStages = PL_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 4, .tStages = PL_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 5, .tStages = PL_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 6, .tStages = PL_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 7, .tStages = PL_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 8, .tStages = PL_STAGE_COMPUTE},
                },
                .uSamplerBindingCount = 1,
                .atSamplerBindings = { {.uSlot = 0, .tStages = PL_STAGE_COMPUTE}}
            }
        }
    };

    typedef struct _FilterShaderSpecData{
        int resolution;
        float u_roughness;
        int u_sampleCount;
        int u_width;
        float u_lodBias;
        int u_distribution;
        int u_isGeneratingLUT;
        int currentMipLevel;
    } FilterShaderSpecData;

    FilterShaderSpecData tFilterData0 = {
        .resolution = iResolution,
        .u_roughness = 0.0f,
        .u_width = iResolution,
        .u_distribution = 0,
        .u_lodBias = 0,
        .u_sampleCount = iSamples,
        .u_isGeneratingLUT = 0,
        .currentMipLevel = 0,
    };

    FilterShaderSpecData tFilterDatas[16] = {0};

    tFilterDatas[0].resolution = iResolution;
    tFilterDatas[0].u_roughness = 0.0f;
    tFilterDatas[0].u_width = iResolution;
    tFilterDatas[0].u_distribution = 1;
    tFilterDatas[0].u_lodBias = 1;
    tFilterDatas[0].u_sampleCount = iSamples;
    tFilterDatas[0].u_isGeneratingLUT = 1;
    tFilterDatas[0].currentMipLevel = 0;
    for(int i = 0; i < ptScene->iEnvironmentMips; i++)
    {
        int currentWidth = iResolution >> i;
        tFilterDatas[i + 1].resolution = iResolution;
        tFilterDatas[i + 1].u_roughness = (float)i / (float)(ptScene->iEnvironmentMips - 1);
        tFilterDatas[i + 1].u_width = currentWidth;
        tFilterDatas[i + 1].u_distribution = 1;
        tFilterDatas[i + 1].u_lodBias = 1;
        tFilterDatas[i + 1].u_sampleCount = iSamples;
        tFilterDatas[i + 1].u_isGeneratingLUT = 0; 
        tFilterDatas[i + 1].currentMipLevel = i; 
        // tVariants[i+1].pTempConstantData = &tFilterDatas[i + 1];
    }

    tFilterComputeShaderDesc.pTempConstantData = &tFilterData0;
    plComputeShaderHandle tIrradianceShader = gptDevice->create_compute_shader(ptDevice, &tFilterComputeShaderDesc);

    tFilterComputeShaderDesc.pTempConstantData = &tFilterDatas[0];
    plComputeShaderHandle tLUTShader = gptDevice->create_compute_shader(ptDevice, &tFilterComputeShaderDesc);

    plComputeShaderHandle atSpecularComputeShaders[16] = {0};
    for(int i = 0; i < ptScene->iEnvironmentMips + 1; i++)
    {
        tFilterComputeShaderDesc.pTempConstantData = &tFilterDatas[i + 1];
        atSpecularComputeShaders[i] = gptDevice->create_compute_shader(ptDevice, &tFilterComputeShaderDesc);
    }

    // create lut
    {

        plBufferHandle atLutBuffers[7] = {0};
        const plBufferDescription tInputBufferDesc = {
            .tUsage    = PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_STAGING,
            .uByteSize = (uint32_t)uFaceSize
        };
        atLutBuffers[6] = pl__refr_create_staging_buffer(&tInputBufferDesc, "lut output", 0);

        for(uint32_t i = 0; i < 6; i++)
            atLutBuffers[i] = pl__refr_create_local_buffer(&tInputBufferDesc, "lut output", i, NULL);

        plBindGroupHandle tLutBindGroup = gptDevice->get_temporary_bind_group(ptDevice, &tFilterComputeShaderDesc.atBindGroupLayouts[0], "lut bindgroup");
        const plBindGroupUpdateBufferData atBGBufferData[] = {
            { .uSlot = 2, .tBuffer = atLutBuffers[0], .szBufferRange = uFaceSize},
            { .uSlot = 3, .tBuffer = atLutBuffers[1], .szBufferRange = uFaceSize},
            { .uSlot = 4, .tBuffer = atLutBuffers[2], .szBufferRange = uFaceSize},
            { .uSlot = 5, .tBuffer = atLutBuffers[3], .szBufferRange = uFaceSize},
            { .uSlot = 6, .tBuffer = atLutBuffers[4], .szBufferRange = uFaceSize},
            { .uSlot = 7, .tBuffer = atLutBuffers[5], .szBufferRange = uFaceSize},
            { .uSlot = 8, .tBuffer = atLutBuffers[6], .szBufferRange = uFaceSize},
        };

        const plBindGroupUpdateSamplerData tSamplerData = {
            .tSampler = gptData->tDefaultSampler,
            .uSlot = 0
        };
        const plBindGroupUpdateTextureData tTextureData = {
            .tTexture = ptScene->tSkyboxTexture,
            .uSlot    = 1,
            .tType    = PL_TEXTURE_BINDING_TYPE_SAMPLED
        };
        const plBindGroupUpdateData tBGData = {
            .uBufferCount = 7,
            .atBuffers = atBGBufferData,
            .uSamplerCount = 1,
            .atSamplerBindings = &tSamplerData,
            .uTextureCount = 1,
            .atTextures = &tTextureData
        };
        gptDevice->update_bind_group(ptDevice, tLutBindGroup, &tBGData);

        const plDispatch tDispach = {
            .uGroupCountX     = (uint32_t)iResolution / 16,
            .uGroupCountY     = (uint32_t)iResolution / 16,
            .uGroupCountZ     = 3,
            .uThreadPerGroupX = 16,
            .uThreadPerGroupY = 16,
            .uThreadPerGroupZ = 3
        };

        plCommandBuffer tCommandBuffer = gptGfx->begin_command_recording(ptGraphics, NULL);
        plComputeEncoder tComputeEncoder = gptGfx->begin_compute_pass(ptGraphics, &tCommandBuffer);
        gptGfx->bind_compute_bind_groups(&tComputeEncoder, tLUTShader, 0, 1, &tLutBindGroup, NULL);
        gptGfx->bind_compute_shader(&tComputeEncoder, tLUTShader);
        gptGfx->dispatch(&tComputeEncoder, 1, &tDispach);
        gptGfx->end_compute_pass(&tComputeEncoder);
        gptGfx->end_command_recording(ptGraphics, &tCommandBuffer);
        gptGfx->submit_command_buffer_blocking(ptGraphics, &tCommandBuffer, NULL);
        gptDevice->queue_compute_shader_for_deletion(ptDevice, tLUTShader);

        plBuffer* ptLutBuffer = gptDevice->get_buffer(ptDevice, atLutBuffers[6]);
        
        const plTextureDesc tTextureDesc = {
            .tDimensions = {(float)iResolution, (float)iResolution, 1},
            .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
            .uLayers     = 1,
            .uMips       = 1,
            .tType       = PL_TEXTURE_TYPE_2D,
            .tUsage      = PL_TEXTURE_USAGE_SAMPLED
        };
        ptScene->tGGXLUTTexture = pl__refr_create_texture_with_data(&tTextureDesc, "lut texture", 0, ptLutBuffer->tMemoryAllocation.pHostMapped, uFaceSize);

        tCommandBuffer = gptGfx->begin_command_recording(ptGraphics, NULL);
        tComputeEncoder = gptGfx->begin_compute_pass(ptGraphics, &tCommandBuffer);
        gptGfx->bind_compute_bind_groups(&tComputeEncoder, tIrradianceShader, 0, 1, &tLutBindGroup, NULL);
        gptGfx->bind_compute_shader(&tComputeEncoder, tIrradianceShader);
        gptGfx->dispatch(&tComputeEncoder, 1, &tDispach);
        gptGfx->end_compute_pass(&tComputeEncoder);
        gptGfx->end_command_recording(ptGraphics, &tCommandBuffer);
        gptGfx->submit_command_buffer_blocking(ptGraphics, &tCommandBuffer, NULL);
        gptDevice->queue_compute_shader_for_deletion(ptDevice, tIrradianceShader);

        const plTextureDesc tSpecularTextureDesc = {
            .tDimensions = {(float)iResolution, (float)iResolution, 1},
            .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
            .uLayers     = 6,
            .uMips       = 1,
            .tType       = PL_TEXTURE_TYPE_CUBE,
            .tUsage      = PL_TEXTURE_USAGE_SAMPLED
        };
        ptScene->tLambertianEnvTexture = pl__refr_create_texture(&tSpecularTextureDesc, "specular texture", uSceneHandle);

        tCommandBuffer = gptGfx->begin_command_recording(ptGraphics, NULL);
        plBlitEncoder tBlitEncoder = gptGfx->begin_blit_pass(ptGraphics, &tCommandBuffer);

        for(uint32_t i = 0; i < 6; i++)
        {
            const plBufferImageCopy tBufferImageCopy = {
                .tImageExtent   = (plExtent){iResolution, iResolution, 1},
                .uLayerCount    = 1,
                .szBufferOffset = 0,
                .uBaseArrayLayer = i,
            };
            gptGfx->copy_buffer_to_texture(&tBlitEncoder, atLutBuffers[i], ptScene->tLambertianEnvTexture, 1, &tBufferImageCopy);
        }

        gptGfx->end_blit_pass(&tBlitEncoder);
        gptGfx->end_command_recording(ptGraphics, &tCommandBuffer);
        gptGfx->submit_command_buffer_blocking(ptGraphics, &tCommandBuffer, NULL);

        for(uint32_t i = 0; i < 7; i++)
            gptDevice->destroy_buffer(ptDevice, atLutBuffers[i]);
    }

    {
        const plTextureDesc tTextureDesc = {
            .tDimensions = {(float)iResolution, (float)iResolution, 1},
            .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
            .uLayers     = 6,
            .uMips       = ptScene->iEnvironmentMips,
            .tType       = PL_TEXTURE_TYPE_CUBE,
            .tUsage      = PL_TEXTURE_USAGE_SAMPLED
        };
        ptScene->tGGXEnvTexture = pl__refr_create_texture(&tTextureDesc, "tGGXEnvTexture", uSceneHandle);

        const plBindGroupUpdateSamplerData tSamplerData = {
            .tSampler = gptData->tDefaultSampler,
            .uSlot = 0
        };
        const plBindGroupUpdateTextureData tTextureData = {
            .tTexture = ptScene->tSkyboxTexture,
            .uSlot    = 1,
            .tType    = PL_TEXTURE_BINDING_TYPE_SAMPLED
        };

        const size_t uMaxFaceSize = (size_t)iResolution * (size_t)iResolution * 4 * sizeof(float);
        const plBufferDescription tOutputBufferDesc = {
            .tUsage    = PL_BUFFER_USAGE_STORAGE,
            .uByteSize = (uint32_t)uMaxFaceSize
        };

        plBufferHandle atInnerComputeBuffers[7] = {0};
        for(uint32_t j = 0; j < 7; j++)
            atInnerComputeBuffers[j] = pl__refr_create_local_buffer(&tOutputBufferDesc, "inner buffer", j, NULL);

        plBindGroupHandle tLutBindGroup = gptDevice->get_temporary_bind_group(ptDevice, &tFilterComputeShaderDesc.atBindGroupLayouts[0], "lut bindgroup");
        const plBindGroupUpdateBufferData atBGBufferData[] = {
            { .uSlot = 2, .tBuffer = atInnerComputeBuffers[0], .szBufferRange = uMaxFaceSize},
            { .uSlot = 3, .tBuffer = atInnerComputeBuffers[1], .szBufferRange = uMaxFaceSize},
            { .uSlot = 4, .tBuffer = atInnerComputeBuffers[2], .szBufferRange = uMaxFaceSize},
            { .uSlot = 5, .tBuffer = atInnerComputeBuffers[3], .szBufferRange = uMaxFaceSize},
            { .uSlot = 6, .tBuffer = atInnerComputeBuffers[4], .szBufferRange = uMaxFaceSize},
            { .uSlot = 7, .tBuffer = atInnerComputeBuffers[5], .szBufferRange = uMaxFaceSize},
            { .uSlot = 8, .tBuffer = atInnerComputeBuffers[6], .szBufferRange = uMaxFaceSize}
        };

        const plBindGroupUpdateData tBGData = {
            .uBufferCount = 7,
            .atBuffers = atBGBufferData,
            .uSamplerCount = 1,
            .atSamplerBindings = &tSamplerData,
            .uTextureCount = 1,
            .atTextures = &tTextureData
        };
        gptDevice->update_bind_group(ptDevice, tLutBindGroup, &tBGData);

        for (int i = ptScene->iEnvironmentMips - 1; i != -1; i--)
        {
            int currentWidth = iResolution >> i;

            const size_t uCurrentFaceSize = (size_t)currentWidth * (size_t)currentWidth * 4 * sizeof(float);

            const plDispatch tDispach = {
                .uGroupCountX     = (uint32_t)currentWidth / 16,
                .uGroupCountY     = (uint32_t)currentWidth / 16,
                .uGroupCountZ     = 2,
                .uThreadPerGroupX = 16,
                .uThreadPerGroupY = 16,
                .uThreadPerGroupZ = 3
            };

            plCommandBuffer tCommandBuffer = gptGfx->begin_command_recording(ptGraphics, NULL);
            plComputeEncoder tComputeEncoder = gptGfx->begin_compute_pass(ptGraphics, &tCommandBuffer);
            gptGfx->bind_compute_bind_groups(&tComputeEncoder, atSpecularComputeShaders[i], 0, 1, &tLutBindGroup, NULL);
            gptGfx->bind_compute_shader(&tComputeEncoder, atSpecularComputeShaders[i]);
            gptGfx->dispatch(&tComputeEncoder, 1, &tDispach);
            gptGfx->end_compute_pass(&tComputeEncoder);
            gptGfx->end_command_recording(ptGraphics, &tCommandBuffer);
            gptGfx->submit_command_buffer_blocking(ptGraphics, &tCommandBuffer, NULL);
            gptDevice->queue_compute_shader_for_deletion(ptDevice, atSpecularComputeShaders[i]);

            tCommandBuffer = gptGfx->begin_command_recording(ptGraphics, NULL);
            plBlitEncoder tBlitEncoder = gptGfx->begin_blit_pass(ptDevice->ptGraphics, &tCommandBuffer);

            for(uint32_t j = 0; j < 6; j++)
            {
                const plBufferImageCopy tBufferImageCopy = {
                    .tImageExtent    = (plExtent){currentWidth, currentWidth, 1},
                    .uLayerCount     = 1,
                    .szBufferOffset  = 0,
                    .uBaseArrayLayer = j,
                    .uMipLevel       = i
                };
                gptGfx->copy_buffer_to_texture(&tBlitEncoder, atInnerComputeBuffers[j], ptScene->tGGXEnvTexture, 1, &tBufferImageCopy);
            }
            gptGfx->end_blit_pass(&tBlitEncoder);
            gptGfx->end_command_recording(ptGraphics, &tCommandBuffer);
            gptGfx->submit_command_buffer_blocking(ptGraphics, &tCommandBuffer, NULL);

        }
        for(uint32_t j = 0; j < 7; j++)
            gptDevice->queue_buffer_for_deletion(ptDevice, atInnerComputeBuffers[j]);
    }

    pl_end_profile_sample();
}

static plTextureHandle
pl__create_texture_helper(plMaterialComponent* ptMaterial, plTextureSlot tSlot, bool bHdr, int iMips)
{
    plDevice* ptDevice = &gptData->tGraphics.tDevice;

    if(gptResource->is_resource_valid(ptMaterial->atTextureMaps[tSlot].tResource) == false)
        return gptData->tDummyTexture;
    
    size_t szResourceSize = 0;

    plTextureHandle tTexture = {0};

    if(bHdr)
    {

        const float* rawBytes = gptResource->get_buffer_data(ptMaterial->atTextureMaps[tSlot].tResource, &szResourceSize);
        const plTextureDesc tTextureDesc = {
            .tDimensions = {(float)ptMaterial->atTextureMaps[tSlot].uWidth, (float)ptMaterial->atTextureMaps[tSlot].uHeight, 1},
            .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
            .uLayers     = 1,
            .uMips       = iMips,
            .tType       = PL_TEXTURE_TYPE_2D,
            .tUsage      = PL_TEXTURE_USAGE_SAMPLED
        };
        tTexture = pl__refr_create_texture_with_data(&tTextureDesc, ptMaterial->atTextureMaps[tSlot].acName, 0, rawBytes, szResourceSize);
    }
    else
    {
        const unsigned char* rawBytes = gptResource->get_buffer_data(ptMaterial->atTextureMaps[tSlot].tResource, &szResourceSize);
        plTextureDesc tTextureDesc = {
            .tDimensions = {(float)ptMaterial->atTextureMaps[tSlot].uWidth, (float)ptMaterial->atTextureMaps[tSlot].uHeight, 1},
            .tFormat = PL_FORMAT_R8G8B8A8_UNORM,
            .uLayers = 1,
            .uMips = iMips,
            .tType = PL_TEXTURE_TYPE_2D,
            .tUsage = PL_TEXTURE_USAGE_SAMPLED
        };
        tTexture = pl__refr_create_texture_with_data(&tTextureDesc, ptMaterial->atTextureMaps[tSlot].acName, 0, rawBytes, szResourceSize);
    }

    return tTexture;
}

static void
pl_refr_select_entities(uint32_t uSceneHandle, uint32_t uCount, plEntity* atEntities)
{
    // for convience
    plRefScene* ptScene    = &gptData->sbtScenes[uSceneHandle];
    plGraphics* ptGraphics = &gptData->tGraphics;
    plDevice*   ptDevice   = &ptGraphics->tDevice;

    int iSceneWideRenderingFlags = PL_RENDERING_FLAG_USE_PUNCTUAL;
    if(ptScene->tGGXEnvTexture.uIndex != UINT32_MAX)
        iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_IBL;

    // reset old entities
    const uint32_t uOldSelectedEntityCount = pl_sb_size(ptScene->sbtOutlineDrawables);
    for(uint32_t i = 0; i < uOldSelectedEntityCount; i++)
    {
        plEntity tEntity = ptScene->sbtOutlineDrawables[i].tEntity;
        plShader* ptOutlineShader = gptDevice->get_shader(ptDevice, ptScene->sbtOutlineDrawables[i].tShader);

        plObjectComponent*   ptObject   = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tEntity);
        plMeshComponent*     ptMesh     = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptObject->tMesh);
        plMaterialComponent* ptMaterial = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MATERIAL, ptMesh->tMaterial);

        int iDataStride = 0;
        int iFlagCopy0 = (int)ptMesh->ulVertexStreamMask;
        while(iFlagCopy0)
        {
            iDataStride += iFlagCopy0 & 1;
            iFlagCopy0 >>= 1;
        }

        int iTextureMappingFlags = 0;
        for(uint32_t j = 0; j < PL_TEXTURE_SLOT_COUNT; j++)
        {
            if((ptMaterial->atTextureMaps[j].acName[0] != 0))
                iTextureMappingFlags |= 1 << j; 
        }

        // choose shader variant
        int aiConstantData0[5] = {
            (int)ptMesh->ulVertexStreamMask,
            iDataStride,
            iTextureMappingFlags,
            PL_INFO_MATERIAL_METALLICROUGHNESS,
            iSceneWideRenderingFlags
        };

        // use stencil buffer
        const plGraphicsState tOutlineVariantTemp = {
            .ulDepthWriteEnabled  = 0,
            .ulDepthMode          = PL_COMPARE_MODE_ALWAYS,
            .ulCullMode           = PL_CULL_MODE_CULL_FRONT,
            .ulWireframe          = 0,
            .ulStencilTestEnabled = 1,
            .ulStencilMode        = PL_COMPARE_MODE_GREATER,
            .ulStencilRef         = 128,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        };

        const plShaderVariant tOutlineVariant = {
            .pTempConstantData = aiConstantData0,
            .tGraphicsState    = tOutlineVariantTemp
        };

        size_t szSpecializationSize = 0;
        for(uint32_t j = 0; j < ptOutlineShader->tDescription.uConstantCount; j++)
        {
            const plSpecializationConstant* ptConstant = &ptOutlineShader->tDescription.atConstants[j];
            szSpecializationSize += pl__get_data_type_size(ptConstant->tType);
        }

        const uint64_t ulVariantHash = pl_hm_hash(tOutlineVariant.pTempConstantData, szSpecializationSize, tOutlineVariant.tGraphicsState.ulValue);
        pl_hm_remove(&gptData->tVariantHashmap, ulVariantHash);

        if(pl_hm_has_key(&ptScene->tOpaqueHashmap, tEntity.ulData))
        {
            uint64_t ulIndex = pl_hm_lookup(&ptScene->tOpaqueHashmap, tEntity.ulData);
            plDrawable* ptDrawable = &ptScene->sbtOpaqueDrawables[ulIndex];
            ptDrawable->tShader = ptScene->sbtOutlineDrawablesOldShaders[i];
        }
        else if(pl_hm_has_key(&ptScene->tTransparentHashmap, tEntity.ulData))
        {
            uint64_t ulIndex = pl_hm_lookup(&ptScene->tTransparentHashmap, tEntity.ulData);
            plDrawable* ptDrawable = &ptScene->sbtTransparentDrawables[ulIndex];
            ptDrawable->tShader = ptScene->sbtOutlineDrawablesOldShaders[i];
        }

        gptDevice->queue_shader_for_deletion(ptDevice, ptScene->sbtOutlineDrawables[i].tShader);
    }
    pl_sb_reset(ptScene->sbtOutlineDrawables)
    pl_sb_reset(ptScene->sbtOutlineDrawablesOldShaders)



    for(uint32_t i = 0; i < uCount; i++)
    {
        plEntity tEntity = atEntities[i];

        plObjectComponent* ptObject   = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tEntity);
        if(ptObject == NULL)
            continue;
        plMeshComponent*     ptMesh     = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptObject->tMesh);
        plMaterialComponent* ptMaterial = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MATERIAL, ptMesh->tMaterial);

        ptMaterial->tFlags |= PL_MATERIAL_FLAG_OUTLINE;

        int iDataStride = 0;
        int iFlagCopy0 = (int)ptMesh->ulVertexStreamMask;
        while(iFlagCopy0)
        {
            iDataStride += iFlagCopy0 & 1;
            iFlagCopy0 >>= 1;
        }

        int iTextureMappingFlags = 0;
        for(uint32_t j = 0; j < PL_TEXTURE_SLOT_COUNT; j++)
        {
            if((ptMaterial->atTextureMaps[j].acName[0] != 0))
                iTextureMappingFlags |= 1 << j; 
        }

        // choose shader variant
        int aiConstantData0[5] = {
            (int)ptMesh->ulVertexStreamMask,
            iDataStride,
            iTextureMappingFlags,
            PL_INFO_MATERIAL_METALLICROUGHNESS,
            iSceneWideRenderingFlags
        };

        if(pl_hm_has_key(&ptScene->tOpaqueHashmap, tEntity.ulData))
        {
            uint64_t ulIndex = pl_hm_lookup(&ptScene->tOpaqueHashmap, tEntity.ulData);
            plDrawable* ptDrawable = &ptScene->sbtOpaqueDrawables[ulIndex];
            plShader* ptOldShader = gptDevice->get_shader(ptDevice, ptDrawable->tShader);
            plGraphicsState tVariantTemp = ptOldShader->tDescription.tGraphicsState;

            // write into stencil buffer
            tVariantTemp.ulStencilTestEnabled = 1;
            tVariantTemp.ulStencilMode        = PL_COMPARE_MODE_ALWAYS;
            tVariantTemp.ulStencilRef         = 0xff;
            tVariantTemp.ulStencilMask        = 0xff;
            tVariantTemp.ulStencilOpFail      = PL_STENCIL_OP_REPLACE;
            tVariantTemp.ulStencilOpDepthFail = PL_STENCIL_OP_REPLACE;
            tVariantTemp.ulStencilOpPass      = PL_STENCIL_OP_REPLACE;

            // use stencil buffer
            const plGraphicsState tOutlineVariantTemp = {
                .ulDepthWriteEnabled  = 0,
                .ulDepthMode          = PL_COMPARE_MODE_ALWAYS,
                .ulCullMode           = PL_CULL_MODE_CULL_FRONT,
                .ulWireframe          = 0,
                .ulStencilTestEnabled = 1,
                .ulStencilMode        = PL_COMPARE_MODE_GREATER,
                .ulStencilRef         = 128,
                .ulStencilMask        = 0xff,
                .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
                .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .ulStencilOpPass      = PL_STENCIL_OP_KEEP
            };

            const plShaderVariant tOutlineVariant = {
                .pTempConstantData = aiConstantData0,
                .tGraphicsState    = tOutlineVariantTemp
            };

            plShaderHandle tOutlineShader = pl__get_shader_variant(uSceneHandle, gptData->tOutlineShader, &tOutlineVariant);
            pl_sb_push(ptScene->sbtOutlineDrawables, *ptDrawable);
            ptScene->sbtOutlineDrawables[pl_sb_size(ptScene->sbtOutlineDrawables) - 1].tShader = tOutlineShader;

            const plShaderVariant tVariant = {
                .pTempConstantData = aiConstantData0,
                .tGraphicsState    = tVariantTemp
            };

            pl_sb_push(ptScene->sbtOutlineDrawablesOldShaders, ptDrawable->tShader);
            ptDrawable->tShader = pl__get_shader_variant(uSceneHandle, gptData->tOpaqueShader, &tVariant);
        }
        else if(pl_hm_has_key(&ptScene->tTransparentHashmap, tEntity.ulData))
        {
            uint64_t ulIndex = pl_hm_lookup(&ptScene->tTransparentHashmap, tEntity.ulData);
            plDrawable* ptDrawable = &ptScene->sbtTransparentDrawables[ulIndex];
            plShader* ptOldShader = gptDevice->get_shader(ptDevice, ptDrawable->tShader);
            plGraphicsState tVariantTemp = ptOldShader->tDescription.tGraphicsState;

            // write into stencil buffer
            tVariantTemp.ulStencilTestEnabled = 1;
            tVariantTemp.ulStencilMode        = PL_COMPARE_MODE_ALWAYS;
            tVariantTemp.ulStencilRef         = 0xff;
            tVariantTemp.ulStencilMask        = 0xff;
            tVariantTemp.ulStencilOpFail      = PL_STENCIL_OP_REPLACE;
            tVariantTemp.ulStencilOpDepthFail = PL_STENCIL_OP_REPLACE;
            tVariantTemp.ulStencilOpPass      = PL_STENCIL_OP_REPLACE;

            // use stencil buffer
            const plGraphicsState tOutlineVariantTemp = {
                .ulDepthWriteEnabled  = 0,
                .ulDepthMode          = PL_COMPARE_MODE_ALWAYS,
                .ulCullMode           = PL_CULL_MODE_CULL_FRONT,
                .ulWireframe          = 0,
                .ulStencilTestEnabled = 1,
                .ulStencilMode        = PL_COMPARE_MODE_GREATER,
                .ulStencilRef         = 128,
                .ulStencilMask        = 0xff,
                .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
                .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .ulStencilOpPass      = PL_STENCIL_OP_KEEP
            };

            const plShaderVariant tOutlineVariant = {
                .pTempConstantData = aiConstantData0,
                .tGraphicsState    = tOutlineVariantTemp
            };

            plShaderHandle tOutlineShader = pl__get_shader_variant(uSceneHandle, gptData->tOutlineShader, &tOutlineVariant);
            pl_sb_push(ptScene->sbtOutlineDrawables, *ptDrawable);
            ptScene->sbtOutlineDrawables[pl_sb_size(ptScene->sbtOutlineDrawables) - 1].tShader = tOutlineShader;

            const plShaderVariant tVariant = {
                .pTempConstantData = aiConstantData0,
                .tGraphicsState    = tVariantTemp
            };

            pl_sb_push(ptScene->sbtOutlineDrawablesOldShaders, ptDrawable->tShader);
            ptDrawable->tShader = pl__get_shader_variant(uSceneHandle, gptData->tTransparentShader, &tVariant);
        }
    }
}

static void
pl_refr_finalize_scene(uint32_t uSceneHandle)
{
    // for convience
    plRefScene* ptScene    = &gptData->sbtScenes[uSceneHandle];
    plGraphics* ptGraphics = &gptData->tGraphics;
    plDevice*   ptDevice   = &ptGraphics->tDevice;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~textures~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    pl_begin_profile_sample("load textures");
    plHashMap tMaterialBindGroupDict = {0};
    plBindGroupHandle* sbtMaterialBindGroups = NULL;
    plMaterialComponent* sbtMaterials = ptScene->tComponentLibrary.tMaterialComponentManager.pComponents;
    const uint32_t uMaterialCount = pl_sb_size(sbtMaterials);
    pl_sb_resize(sbtMaterialBindGroups, uMaterialCount);

    plAtomicCounter* ptCounter = NULL;
    plJobDesc tJobDesc = {
        .task  = pl__refr_job,
        .pData = sbtMaterials
    };
    gptJob->dispatch_batch(uMaterialCount, 0, tJobDesc, &ptCounter);
    gptJob->wait_for_counter(ptCounter);
    pl_end_profile_sample();

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~materials~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    pl_begin_profile_sample("load materials");
    for(uint32_t i = 0; i < uMaterialCount; i++)
    {
        plMaterialComponent* ptMaterial = &sbtMaterials[i];

        plBindGroupLayout tMaterialBindGroupLayout = {
            .uTextureBindingCount = 12,
            .atTextureBindings = {
                {.uSlot =  0, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
                {.uSlot =  1, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
                {.uSlot =  2, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
                {.uSlot =  3, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
                {.uSlot =  4, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
                {.uSlot =  5, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
                {.uSlot =  6, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
                {.uSlot =  7, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
                {.uSlot =  8, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
                {.uSlot =  9, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
                {.uSlot = 10, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
                {.uSlot = 11, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
            }
        };
        sbtMaterialBindGroups[i] = gptDevice->create_bind_group(ptDevice, &tMaterialBindGroupLayout, "material bind group");

        const plBindGroupUpdateTextureData tTextureData[] = 
        {
            {.tTexture = pl__create_texture_helper(ptMaterial, PL_TEXTURE_SLOT_BASE_COLOR_MAP, true, 0),              .uSlot =  0, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
            {.tTexture = pl__create_texture_helper(ptMaterial, PL_TEXTURE_SLOT_NORMAL_MAP, false, 0),                 .uSlot =  1, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
            {.tTexture = pl__create_texture_helper(ptMaterial, PL_TEXTURE_SLOT_EMISSIVE_MAP, true, 0),                .uSlot =  2, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
            {.tTexture = pl__create_texture_helper(ptMaterial, PL_TEXTURE_SLOT_METAL_ROUGHNESS_MAP, false, 0),        .uSlot =  3, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
            {.tTexture = pl__create_texture_helper(ptMaterial, PL_TEXTURE_SLOT_OCCLUSION_MAP, false, 1),              .uSlot =  4, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
            {.tTexture = pl__create_texture_helper(ptMaterial, PL_TEXTURE_SLOT_CLEARCOAT_MAP, false, 1),              .uSlot =  5, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
            {.tTexture = pl__create_texture_helper(ptMaterial, PL_TEXTURE_SLOT_CLEARCOAT_ROUGHNESS_MAP, false, 1),    .uSlot =  6, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
            {.tTexture = pl__create_texture_helper(ptMaterial, PL_TEXTURE_SLOT_CLEARCOAT_NORMAL_MAP, false, 1),       .uSlot =  7, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
            {.tTexture = pl__create_texture_helper(ptMaterial, PL_TEXTURE_SLOT_IRIDESCENCE_MAP, false, 1),            .uSlot =  8, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
            {.tTexture = pl__create_texture_helper(ptMaterial, PL_TEXTURE_SLOT_IRIDESCENCE_THICKNESS_MAP, false, 1),  .uSlot =  9, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
            {.tTexture = pl__create_texture_helper(ptMaterial, PL_TEXTURE_SLOT_SPECULAR_MAP, false, 1),               .uSlot = 10, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
            {.tTexture = pl__create_texture_helper(ptMaterial, PL_TEXTURE_SLOT_SPECULAR_COLOR_MAP, false, 1),         .uSlot = 11, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
        };
        const plBindGroupUpdateData tBGData1 = {
            .uTextureCount = 12,
            .atTextures = tTextureData
        };
        gptDevice->update_bind_group(ptDevice, sbtMaterialBindGroups[i], &tBGData1);
        pl_hm_insert(&tMaterialBindGroupDict, (uint64_t)ptMaterial, (uint64_t)i);
    }
    pl_end_profile_sample();

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~CPU Buffers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    int iSceneWideRenderingFlags = PL_RENDERING_FLAG_USE_PUNCTUAL;
    if(ptScene->tGGXEnvTexture.uIndex != UINT32_MAX)
        iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_IBL;

    // fill CPU buffers & drawable list
    pl_begin_profile_sample("create shaders");

    plDrawable* sbtDrawables[] = {
        ptScene->sbtOpaqueDrawables,
        ptScene->sbtTransparentDrawables,
    };

    plShaderHandle atTemplateShaders[] = {
        gptData->tOpaqueShader,
        gptData->tTransparentShader
    };

    plGraphicsState atTemplateVariants[] = {
        {
            .ulDepthWriteEnabled  = 1,
            .ulDepthMode          = PL_COMPARE_MODE_LESS,
            .ulCullMode           = PL_CULL_MODE_CULL_BACK,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        {
            .ulDepthWriteEnabled  = 1,
            .ulDepthMode          = PL_COMPARE_MODE_LESS_OR_EQUAL,
            .ulCullMode           = PL_CULL_MODE_NONE,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        }
    };

    plHashMap* atHashmaps[] = {
        &ptScene->tOpaqueHashmap,
        &ptScene->tTransparentHashmap
    };
    
    const plLightComponent* sbtLights = ptScene->tComponentLibrary.tLightComponentManager.pComponents;

    for(uint32_t uDrawableBatchIndex = 0; uDrawableBatchIndex < 2; uDrawableBatchIndex++)
    {
        plHashMap* ptHashmap = atHashmaps[uDrawableBatchIndex];
        const uint32_t uDrawableCount = pl_sb_size(sbtDrawables[uDrawableBatchIndex]);
        pl_hm_resize(ptHashmap, uDrawableCount);
        for(uint32_t i = 0; i < uDrawableCount; i++)
        {

            (sbtDrawables[uDrawableBatchIndex])[i].uSkinIndex = UINT32_MAX;
            plEntity tEntity = (sbtDrawables[uDrawableBatchIndex])[i].tEntity;
            pl_hm_insert(ptHashmap, tEntity.ulData, i);

            // get actual components
            plObjectComponent*   ptObject   = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tEntity);
            plMeshComponent*     ptMesh     = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptObject->tMesh);
            plMaterialComponent* ptMaterial = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MATERIAL, ptMesh->tMaterial);


            const uint64_t ulMaterialIndex = pl_hm_lookup(&tMaterialBindGroupDict, (uint64_t)ptMaterial);
            (sbtDrawables[uDrawableBatchIndex])[i].tMaterialBindGroup = sbtMaterialBindGroups[ulMaterialIndex];

            // add data to global buffers
            pl__add_drawable_data_to_global_buffer(ptScene, i, (sbtDrawables[uDrawableBatchIndex]));
            pl__add_drawable_skin_data_to_global_buffer(ptScene, i, (sbtDrawables[uDrawableBatchIndex]));

            int iDataStride = 0;
            int iFlagCopy0 = (int)ptMesh->ulVertexStreamMask;
            while(iFlagCopy0)
            {
                iDataStride += iFlagCopy0 & 1;
                iFlagCopy0 >>= 1;
            }

            int iTextureMappingFlags = 0;
            for(uint32_t j = 0; j < PL_TEXTURE_SLOT_COUNT; j++)
            {
                if((ptMaterial->atTextureMaps[j].acName[0] != 0))
                    iTextureMappingFlags |= 1 << j; 
            }

            // choose shader variant
            int aiConstantData0[] = {
                (int)ptMesh->ulVertexStreamMask,
                iDataStride,
                iTextureMappingFlags,
                PL_INFO_MATERIAL_METALLICROUGHNESS,
                iSceneWideRenderingFlags,
                pl_sb_size(sbtLights)
            };

            plGraphicsState tVariantTemp = atTemplateVariants[uDrawableBatchIndex];

            if(ptMaterial->tFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED)
                tVariantTemp.ulCullMode = PL_CULL_MODE_NONE;

            const plShaderVariant tVariant = {
                .pTempConstantData = aiConstantData0,
                .tGraphicsState    = tVariantTemp
            };

            (sbtDrawables[uDrawableBatchIndex])[i].tShader = pl__get_shader_variant(uSceneHandle, atTemplateShaders[uDrawableBatchIndex], &tVariant);
        }
    }

    pl_end_profile_sample();

    pl_hm_free(&tMaterialBindGroupDict);
    pl_sb_free(sbtMaterialBindGroups);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~GPU Buffers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    pl_begin_profile_sample("fill GPU buffers");

    const plBufferDescription tShaderBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_STORAGE,
        .uByteSize = sizeof(plGPUMaterial) * pl_sb_size(ptScene->sbtMaterialBuffer)
    };
    
    const plBufferDescription tIndexBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_INDEX,
        .uByteSize = sizeof(uint32_t) * pl_sb_size(ptScene->sbuIndexBuffer)
    };
    
    const plBufferDescription tVertexBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_VERTEX | PL_BUFFER_USAGE_STORAGE,
        .uByteSize = sizeof(plVec3) * pl_sb_size(ptScene->sbtVertexPosBuffer)
    };
     
    const plBufferDescription tStorageBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_STORAGE,
        .uByteSize = sizeof(plVec4) * pl_sb_size(ptScene->sbtVertexDataBuffer)
    };

    const plBufferDescription tSkinStorageBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_STORAGE,
        .uByteSize = sizeof(plVec4) * pl_sb_size(ptScene->sbtSkinVertexDataBuffer)
    };

    const plBufferDescription tLightBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_UNIFORM,
        .uByteSize = sizeof(plGPULight) * PL_MAX_LIGHTS
    };

    for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
        ptScene->atLightBuffer[i] = pl__refr_create_staging_buffer(&tLightBufferDesc, "light", i);

    ptScene->tMaterialDataBuffer = pl__refr_create_local_buffer(&tShaderBufferDesc,            "shader", uSceneHandle, ptScene->sbtMaterialBuffer);
    ptScene->tIndexBuffer        = pl__refr_create_local_buffer(&tIndexBufferDesc,              "index", uSceneHandle, ptScene->sbuIndexBuffer);
    ptScene->tVertexBuffer       = pl__refr_create_local_buffer(&tVertexBufferDesc,            "vertex", uSceneHandle, ptScene->sbtVertexPosBuffer);
    ptScene->tStorageBuffer      = pl__refr_create_local_buffer(&tStorageBufferDesc,          "storage", uSceneHandle, ptScene->sbtVertexDataBuffer);

    if(tSkinStorageBufferDesc.uByteSize > 0)
    {
        ptScene->tSkinStorageBuffer  = pl__refr_create_local_buffer(&tSkinStorageBufferDesc, "skin storage", uSceneHandle, ptScene->sbtSkinVertexDataBuffer);

        const plBindGroupLayout tSkinBindGroupLayout0 = {
            .uSamplerBindingCount = 1,
            .atSamplerBindings = {
                {.uSlot =  3, .tStages = PL_STAGE_COMPUTE}
            },
            .uBufferBindingCount = 3,
            .aBufferBindings = {
                { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_STAGE_COMPUTE},
                { .uSlot = 1, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_STAGE_COMPUTE},
                { .uSlot = 2, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_STAGE_COMPUTE},
            }
        };
        ptScene->tSkinBindGroup0 = gptDevice->create_bind_group(ptDevice, &tSkinBindGroupLayout0, "skin bind group 0");

        const plBindGroupUpdateSamplerData atSamplerData[] = {
            { .uSlot = 3, .tSampler = gptData->tDefaultSampler}
        };
        const plBindGroupUpdateBufferData atBufferData[] = 
        {
            { .uSlot = 0, .tBuffer = ptScene->tSkinStorageBuffer, .szBufferRange = tSkinStorageBufferDesc.uByteSize},
            { .uSlot = 1, .tBuffer = ptScene->tVertexBuffer,      .szBufferRange = tVertexBufferDesc.uByteSize},
            { .uSlot = 2, .tBuffer = ptScene->tStorageBuffer,     .szBufferRange = tStorageBufferDesc.uByteSize}

        };
        plBindGroupUpdateData tBGData0 = {
            .uBufferCount = 3,
            .atBuffers = atBufferData,
            .uSamplerCount = 1,
            .atSamplerBindings = atSamplerData,
        };
        gptDevice->update_bind_group(&ptGraphics->tDevice, ptScene->tSkinBindGroup0, &tBGData0);
    }

    // create lighting shader
    int aiLightingConstantData[] = {iSceneWideRenderingFlags, pl_sb_size(sbtLights)};
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
            pl__get_blend_state(PL_BLEND_MODE_OPAQUE)
        },
        .uConstantCount = 2,
        .pTempConstantData = aiLightingConstantData,
        .uBlendStateCount = 1,
        .uSubpassIndex = 1,
        .tRenderPassLayout = gptData->tRenderPassLayout,
        .uBindGroupLayoutCount = 3,
        .atBindGroupLayouts = {
            {
                .uBufferBindingCount  = 3,
                .aBufferBindings = {
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
                .uSamplerBindingCount = 2,
                .atSamplerBindings = {
                    {.uSlot = 3, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                    {.uSlot = 4, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                },
                .uTextureBindingCount  = 3,
                .atTextureBindings = {
                    {.uSlot =   5, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   6, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   7, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1}
                }
            },
            {
                .uTextureBindingCount = 6,
                .atTextureBindings = {
                    { .uSlot = 0, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
                    { .uSlot = 1, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
                    { .uSlot = 2, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
                    { .uSlot = 3, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
                    { .uSlot = 4, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
                    { .uSlot = 5, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
                 },
            },
            {
                .uBufferBindingCount  = 1,
                .aBufferBindings = {
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                        .uSlot = 0,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    }
                },
            }
        }
    };
    for(uint32_t i = 0; i < tLightingShaderDesc.uConstantCount; i++)
    {
        tLightingShaderDesc.atConstants[i].uID = i;
        tLightingShaderDesc.atConstants[i].uOffset = i * sizeof(int);
        tLightingShaderDesc.atConstants[i].tType = PL_DATA_TYPE_INT;
    }
    ptScene->tLightingShader = gptDevice->create_shader(&ptGraphics->tDevice, &tLightingShaderDesc);

    pl_end_profile_sample();
}

static void
pl_refr_run_ecs(uint32_t uSceneHandle)
{
    pl_begin_profile_sample(__FUNCTION__);
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    gptECS->run_animation_update_system(&ptScene->tComponentLibrary, pl_get_io()->fDeltaTime);
    gptECS->run_transform_update_system(&ptScene->tComponentLibrary);
    gptECS->run_hierarchy_update_system(&ptScene->tComponentLibrary);
    gptECS->run_inverse_kinematics_update_system(&ptScene->tComponentLibrary);
    gptECS->run_skin_update_system(&ptScene->tComponentLibrary);
    gptECS->run_object_update_system(&ptScene->tComponentLibrary);
    pl_end_profile_sample();
}

static void
pl_refr_update_skin_textures(plCommandBuffer tCommandBuffer, uint32_t uSceneHandle)
{
    pl_begin_profile_sample(__FUNCTION__);
    plGraphics* ptGraphics = &gptData->tGraphics;
    plDevice* ptDevice = &ptGraphics->tDevice;
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plBlitEncoder tBlitEncoder = gptGfx->begin_blit_pass(ptDevice->ptGraphics, &tCommandBuffer);

    // update skin textures
    if(gptData->uCurrentStagingFrameIndex != ptGraphics->uCurrentFrameIndex)
    {
        gptData->uStagingOffset = 0;
        gptData->uCurrentStagingFrameIndex = ptGraphics->uCurrentFrameIndex;
    }
    const uint32_t uSkinCount = pl_sb_size(ptScene->sbtSkinData);
    for(uint32_t i = 0; i < uSkinCount; i++)
    {
        plBindGroupLayout tBindGroupLayout1 = {
            .uTextureBindingCount  = 1,
            .atTextureBindings = {
                {.uSlot =  0, .tStages = PL_STAGE_ALL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
            }
        };
        ptScene->sbtSkinData[i].tTempBindGroup = gptDevice->get_temporary_bind_group(ptDevice, &tBindGroupLayout1, "skin temporary bind group");
        const plBindGroupUpdateTextureData tTextureData = {.tTexture = ptScene->sbtSkinData[i].atDynamicTexture[ptGraphics->uCurrentFrameIndex], .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED};
        plBindGroupUpdateData tBGData0 = {
            .uTextureCount = 1,
            .atTextures = &tTextureData
        };
        gptDevice->update_bind_group(&ptGraphics->tDevice, ptScene->sbtSkinData[i].tTempBindGroup, &tBGData0);

        plBuffer* ptStagingBuffer = gptDevice->get_buffer(ptDevice, gptData->tStagingBufferHandle[ptGraphics->uCurrentFrameIndex]);

        plTexture* ptSkinTexture = gptDevice->get_texture(ptDevice, ptScene->sbtSkinData[i].atDynamicTexture[ptGraphics->uCurrentFrameIndex]);
        plBufferImageCopy tBufferImageCopy = {
            .tImageExtent = {(size_t)ptSkinTexture->tDesc.tDimensions.x, (size_t)ptSkinTexture->tDesc.tDimensions.y, 1},
            .uLayerCount = 1,
            .szBufferOffset = gptData->uStagingOffset
        };
        gptData->uStagingOffset += sizeof(float) * 4 * (size_t)ptSkinTexture->tDesc.tDimensions.x * (size_t)ptSkinTexture->tDesc.tDimensions.y;
        
        plSkinComponent* ptSkinComponent = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_SKIN, ptScene->sbtSkinData[i].tEntity);
        memcpy(&ptStagingBuffer->tMemoryAllocation.pHostMapped[tBufferImageCopy.szBufferOffset], ptSkinComponent->sbtTextureData, sizeof(float) * 4 * (size_t)ptSkinTexture->tDesc.tDimensions.x * (size_t)ptSkinTexture->tDesc.tDimensions.y);
        // memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, ptSkinComponent->sbtTextureData, sizeof(float) * 4 * (size_t)ptSkinTexture->tDesc.tDimensions.x * (size_t)ptSkinTexture->tDesc.tDimensions.y);
        gptGfx->copy_buffer_to_texture(&tBlitEncoder, gptData->tStagingBufferHandle[ptGraphics->uCurrentFrameIndex], ptScene->sbtSkinData[i].atDynamicTexture[ptGraphics->uCurrentFrameIndex], 1, &tBufferImageCopy);
    }
    gptGfx->end_blit_pass(&tBlitEncoder);

    pl_end_profile_sample();
}

static void
pl_refr_perform_skinning(plCommandBuffer tCommandBuffer, uint32_t uSceneHandle)
{
    pl_begin_profile_sample(__FUNCTION__);
    plGraphics* ptGraphics = &gptData->tGraphics;
    plDevice* ptDevice = &ptGraphics->tDevice;
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];

    // update skin textures
    const uint32_t uSkinCount = pl_sb_size(ptScene->sbtSkinData);

    typedef struct _SkinDynamicData
    {
        int iSourceDataOffset;
        int iDestDataOffset;
        int iDestVertexOffset;
        int iUnused;
    } SkinDynamicData;

    plComputeEncoder tComputeEncoder = gptGfx->begin_compute_pass(ptGraphics, &tCommandBuffer);

    for(uint32_t i = 0; i < uSkinCount; i++)
    {
        plDynamicBinding tDynamicBinding = gptDevice->allocate_dynamic_data(ptDevice, sizeof(SkinDynamicData));
        SkinDynamicData* ptDynamicData = (SkinDynamicData*)tDynamicBinding.pcData;
        ptDynamicData->iSourceDataOffset = ptScene->sbtSkinData[i].iSourceDataOffset;
        ptDynamicData->iDestDataOffset = ptScene->sbtSkinData[i].iDestDataOffset;
        ptDynamicData->iDestVertexOffset = ptScene->sbtSkinData[i].iDestVertexOffset;

        const plDispatch tDispach = {
            .uGroupCountX     = ptScene->sbtSkinData[i].uVertexCount,
            .uGroupCountY     = 1,
            .uGroupCountZ     = 1,
            .uThreadPerGroupX = 1,
            .uThreadPerGroupY = 1,
            .uThreadPerGroupZ = 1
        };
        const plBindGroupHandle atBindGroups[] = {
            ptScene->tSkinBindGroup0,
            ptScene->sbtSkinData[i].tTempBindGroup
        };
        gptGfx->bind_compute_bind_groups(&tComputeEncoder, ptScene->sbtSkinData[i].tShader, 0, 2, atBindGroups, &tDynamicBinding);
        gptGfx->bind_compute_shader(&tComputeEncoder, ptScene->sbtSkinData[i].tShader);
        gptGfx->dispatch(&tComputeEncoder, 1, &tDispach);
    }
    gptGfx->end_compute_pass(&tComputeEncoder);
    pl_end_profile_sample();
}

typedef struct _plCullData
{
    plRefScene* ptScene;
    plCameraComponent* ptCullCamera;
    plDrawable* atDrawables;
} plCullData;

static void
pl__refr_cull_job(uint32_t uJobIndex, void* pData)
{
    plCullData* ptCullData = pData;
    plRefScene* ptScene = ptCullData->ptScene;
    plDrawable tDrawable = ptCullData->atDrawables[uJobIndex];
    plMeshComponent* ptMesh = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MESH, tDrawable.tEntity);
    ptCullData->atDrawables[uJobIndex].bCulled = true;
    if(pl__sat_visibility_test(ptCullData->ptCullCamera, &ptMesh->tAABBFinal))
    {
        ptCullData->atDrawables[uJobIndex].bCulled = false;
    }
}

static void
pl_refr_render_scene(plCommandBuffer tCommandBuffer, uint32_t uSceneHandle, uint32_t uViewHandle, plViewOptions tOptions)
{
    pl_begin_profile_sample(__FUNCTION__);

    // for convience
    plGraphics*        ptGraphics = &gptData->tGraphics;
    plDevice*          ptDevice   = &ptGraphics->tDevice;
    plDrawStream*      ptStream   = &gptData->tDrawStream;
    plRefScene*        ptScene    = &gptData->sbtScenes[uSceneHandle];
    plRefView*         ptView     = &ptScene->atViews[uViewHandle];
    plCameraComponent* ptCamera   = tOptions.ptViewCamera;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~culling~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    const uint32_t uOpaqueDrawableCount = pl_sb_size(ptScene->sbtOpaqueDrawables);
    const uint32_t uTransparentDrawableCount = pl_sb_size(ptScene->sbtTransparentDrawables);
    pl_begin_profile_sample("cull operations");
    if(tOptions.ptCullCamera)
    {
        plCullData tOpaqueCullData = {
            .ptScene = ptScene,
            .ptCullCamera = tOptions.ptCullCamera,
            .atDrawables  = ptScene->sbtOpaqueDrawables
        };
        plAtomicCounter* ptOpaqueCounter = NULL;
        
        plJobDesc tOpaqueJobDesc = {
            .task = pl__refr_cull_job,
            .pData = &tOpaqueCullData
        };

        plCullData tTransparentCullData = {
            .ptScene = ptScene,
            .ptCullCamera = tOptions.ptCullCamera,
            .atDrawables  = ptScene->sbtTransparentDrawables
        };
        plAtomicCounter* ptTransparentCounter = NULL;
        plJobDesc tTransparentJobDesc = {
            .task = pl__refr_cull_job,
            .pData = &tTransparentCullData
        };
        gptJob->dispatch_batch(uOpaqueDrawableCount, 0, tOpaqueJobDesc, &ptOpaqueCounter);
        gptJob->dispatch_batch(uTransparentDrawableCount, 0, tTransparentJobDesc, &ptTransparentCounter);
        gptJob->wait_for_counter(ptOpaqueCounter);
        gptJob->wait_for_counter(ptTransparentCounter);

        pl_sb_reset(ptView->sbtVisibleOpaqueDrawables);
        for(uint32_t uDrawableIndex = 0; uDrawableIndex < uOpaqueDrawableCount; uDrawableIndex++)
        {
            const plDrawable tDrawable = ptScene->sbtOpaqueDrawables[uDrawableIndex];
            if(!tDrawable.bCulled)
                pl_sb_push(ptView->sbtVisibleOpaqueDrawables, tDrawable);
        }

        pl_sb_reset(ptView->sbtVisibleTransparentDrawables);
        for(uint32_t uDrawableIndex = 0; uDrawableIndex < uTransparentDrawableCount; uDrawableIndex++)
        {
            const plDrawable tDrawable = ptScene->sbtTransparentDrawables[uDrawableIndex];
            if(!tDrawable.bCulled)
                pl_sb_push(ptView->sbtVisibleTransparentDrawables, tDrawable);
        }
        
    }
    else 
    {
        if(pl_sb_size(ptView->sbtVisibleOpaqueDrawables) != uOpaqueDrawableCount)
        {
            pl_sb_resize(ptView->sbtVisibleOpaqueDrawables, uOpaqueDrawableCount);
            memcpy(ptView->sbtVisibleOpaqueDrawables, ptScene->sbtOpaqueDrawables, sizeof(plDrawable) * uOpaqueDrawableCount);
        }
        if(pl_sb_size(ptView->sbtVisibleTransparentDrawables) != uTransparentDrawableCount)
        {
            pl_sb_resize(ptView->sbtVisibleTransparentDrawables, uTransparentDrawableCount);
            memcpy(ptView->sbtVisibleTransparentDrawables, ptScene->sbtTransparentDrawables, sizeof(plDrawable) * uTransparentDrawableCount);
        }
    }
    pl_end_profile_sample();

    // update global buffers & bind groups
    const BindGroup_0 tBindGroupBuffer = {
        .tCameraPos            = ptCamera->tPos,
        .tCameraProjection     = ptCamera->tProjMat,
        .tCameraView           = ptCamera->tViewMat,
        .tCameraViewProjection = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat)
    };
    memcpy(ptGraphics->sbtBuffersCold[ptView->atGlobalBuffers[ptGraphics->uCurrentFrameIndex].uIndex].tMemoryAllocation.pHostMapped, &tBindGroupBuffer, sizeof(BindGroup_0));

    plBindGroupLayout tBindGroupLayout0 = {
        .uBufferBindingCount  = 3,
        .aBufferBindings = {
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
        .uSamplerBindingCount = 2,
        .atSamplerBindings = {
            {.uSlot = 3, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
            {.uSlot = 4, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
        },
        .uTextureBindingCount  = 3,
        .atTextureBindings = {
            {.uSlot =   5, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
            {.uSlot =   6, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
            {.uSlot =   7, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1}
        }

    };
    plBindGroupHandle tGlobalBG = gptDevice->get_temporary_bind_group(ptDevice, &tBindGroupLayout0, "temporary global bind group");

    plBindGroupUpdateSamplerData tSamplerData[] = {
        {
            .tSampler = gptData->tDefaultSampler,
            .uSlot    = 3
        },
        {
            .tSampler = gptData->tEnvSampler,
            .uSlot    = 4
        }
    };
    const plBindGroupUpdateBufferData atBufferData[] = 
    {
        {
            .tBuffer       = ptView->atGlobalBuffers[ptGraphics->uCurrentFrameIndex],
            .uSlot         = 0,
            .szBufferRange = sizeof(BindGroup_0)
        },
        {
            .tBuffer       = ptScene->tStorageBuffer,
            .uSlot         = 1,
            .szBufferRange = sizeof(plVec4) * pl_sb_size(ptScene->sbtVertexDataBuffer)
        },
        {
            .tBuffer       = ptScene->tMaterialDataBuffer,
            .uSlot         = 2,
            .szBufferRange = sizeof(plGPUMaterial) * pl_sb_size(ptScene->sbtMaterialBuffer)
        },
    };
    const plBindGroupUpdateTextureData tTextureData[] = {
        {
            .tTexture = ptScene->tLambertianEnvTexture.uIndex != UINT32_MAX ? ptScene->tLambertianEnvTexture : gptData->tDummyTextureCube,
            .uSlot    = 5,
            .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED
        },
        {
            .tTexture = ptScene->tGGXEnvTexture.uIndex != UINT32_MAX ? ptScene->tGGXEnvTexture : gptData->tDummyTextureCube,
            .uSlot    = 6,
            .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED
        },
        {
            .tTexture = ptScene->tGGXLUTTexture.uIndex != UINT32_MAX ? ptScene->tGGXLUTTexture : gptData->tDummyTexture,
            .uSlot    = 7,
            .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED
        },
    };
    plBindGroupUpdateData tBGData0 = {
        .uBufferCount = 3,
        .atBuffers = atBufferData,
        .uSamplerCount = 2,
        .atSamplerBindings = tSamplerData,
        .uTextureCount = 3,
        .atTextures = tTextureData
    };
    gptDevice->update_bind_group(&ptGraphics->tDevice, tGlobalBG, &tBGData0);

    gptGfx->reset_draw_stream(ptStream);

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

    static double* pdVisibleOpaqueObjects = NULL;
    static double* pdVisibleTransparentObjects = NULL;
    if(!pdVisibleOpaqueObjects)
    {
        pdVisibleOpaqueObjects = gptStats->get_counter("visible opaque objects");
        pdVisibleTransparentObjects = gptStats->get_counter("visible transparent objects");
    }

    const uint32_t uVisibleOpaqueDrawCount = pl_sb_size(ptView->sbtVisibleOpaqueDrawables);
    const uint32_t uVisibleTransparentDrawCount = pl_sb_size(ptView->sbtVisibleTransparentDrawables);

    if(tOptions.bCullStats)
    {
        *pdVisibleOpaqueObjects = (double)(uVisibleOpaqueDrawCount);
        *pdVisibleTransparentObjects = (double)(uVisibleTransparentDrawCount);
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~subpass 0 - g buffer fill~~~~~~~~~~~~~~~~~~~~~~~~~~

    plRenderEncoder tEncoder = gptGfx->begin_render_pass(ptGraphics, &tCommandBuffer, ptView->tRenderPass);

    for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
    {
        const plDrawable tDrawable = ptView->sbtVisibleOpaqueDrawables[i];
        plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
        plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
        
        plDynamicBinding tDynamicBinding = gptDevice->allocate_dynamic_data(ptDevice, sizeof(DynamicData));

        DynamicData* ptDynamicData = (DynamicData*)tDynamicBinding.pcData;
        ptDynamicData->iDataOffset = tDrawable.uDataOffset;
        ptDynamicData->iVertexOffset = tDrawable.uVertexOffset;
        ptDynamicData->tModel = ptTransform->tWorld;
        ptDynamicData->iMaterialOffset = tDrawable.uMaterialIndex;

        gptGfx->add_to_stream(ptStream, (plStreamDraw)
        {
            .uShaderVariant       = tDrawable.tShader.uIndex,
            .uDynamicBuffer       = tDynamicBinding.uBufferHandle,
            .uVertexBuffer        = ptScene->tVertexBuffer.uIndex,
            .uIndexBuffer         = tDrawable.uIndexCount == 0 ? UINT32_MAX : ptScene->tIndexBuffer.uIndex,
            .uIndexOffset         = tDrawable.uIndexOffset,
            .uTriangleCount       = tDrawable.uIndexCount == 0 ? tDrawable.uVertexCount / 3 : tDrawable.uIndexCount / 3,
            .uBindGroup0          = tGlobalBG.uIndex,
            .uBindGroup1          = tDrawable.tMaterialBindGroup.uIndex,
            .uBindGroup2          = UINT32_MAX,
            .uDynamicBufferOffset = tDynamicBinding.uByteOffset,
            .uInstanceStart       = 0,
            .uInstanceCount       = 1
        });
    }

    gptGfx->draw_stream(&tEncoder, 1, &tArea);
    gptGfx->reset_draw_stream(ptStream);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~subpass 1 - lighting~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    gptGfx->next_subpass(&tEncoder);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~lights~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    const plLightComponent* sbtLights = ptScene->tComponentLibrary.tLightComponentManager.pComponents;
    pl_sb_reset(ptScene->sbtLightData);
    pl_sb_resize(ptScene->sbtLightData, pl_sb_size(sbtLights));

    for(uint32_t i = 0; i < pl_sb_size(sbtLights); i++)
    {
        const plLightComponent* ptLight = &sbtLights[i];

        const plGPULight tLight = {
            .fIntensity = ptLight->fIntensity,
            .fRange     = ptLight->fRange,
            .iType      = ptLight->tType,
            .tPosition  = ptLight->tPosition,
            .tDirection = ptLight->tDirection,
            .tColor     = ptLight->tColor
        };
        ptScene->sbtLightData[i] = tLight;
    }

    const plBindGroupLayout tLightBindGroupLayout2 = {
        .uBufferBindingCount = 1,
        .aBufferBindings = {
            { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .tStages = PL_STAGE_PIXEL | PL_STAGE_VERTEX}
        }
    };
    plBindGroupHandle tLightBindGroup2 = gptDevice->get_temporary_bind_group(ptDevice, &tLightBindGroupLayout2, "light bind group 2");

    const plBindGroupUpdateBufferData atLightBufferData[] = 
    {
        { .uSlot = 0, .tBuffer = ptScene->atLightBuffer[ptGraphics->uCurrentFrameIndex], .szBufferRange = sizeof(plGPULight) * pl_sb_size(ptScene->sbtLightData)}
    };
    plBindGroupUpdateData tBGData2 = {
        .uBufferCount = 1,
        .atBuffers = atLightBufferData,
    };
    gptDevice->update_bind_group(&ptGraphics->tDevice, tLightBindGroup2, &tBGData2);
    plBuffer* ptLightingBuffer = gptDevice->get_buffer(ptDevice, ptScene->atLightBuffer[ptGraphics->uCurrentFrameIndex]);
    memcpy(ptLightingBuffer->tMemoryAllocation.pHostMapped, ptScene->sbtLightData, sizeof(plGPULight) * pl_sb_size(ptScene->sbtLightData));

    typedef struct _plLightingDynamicData{
        int iDataOffset;
        int iVertexOffset;
        int unused[2];
    } plLightingDynamicData;
    plDynamicBinding tLightingDynamicData = gptDevice->allocate_dynamic_data(ptDevice, sizeof(plLightingDynamicData));
    plLightingDynamicData* ptLightingDynamicData = (plLightingDynamicData*)tLightingDynamicData.pcData;
    ptLightingDynamicData->iDataOffset = ptScene->tLightingDrawable.uDataOffset;
    ptLightingDynamicData->iVertexOffset = ptScene->tLightingDrawable.uVertexOffset;

    gptGfx->add_to_stream(ptStream, (plStreamDraw)
    {
        .uShaderVariant       = ptScene->tLightingShader.uIndex,
        .uDynamicBuffer       = tLightingDynamicData.uBufferHandle,
        .uVertexBuffer        = ptScene->tVertexBuffer.uIndex,
        .uIndexBuffer         = ptScene->tIndexBuffer.uIndex,
        .uIndexOffset         = ptScene->tLightingDrawable.uIndexOffset,
        .uTriangleCount       = 2,
        .uBindGroup0          = tGlobalBG.uIndex,
        .uBindGroup1          = ptView->tLightingBindGroup[ptGraphics->uCurrentFrameIndex].uIndex,
        .uBindGroup2          = tLightBindGroup2.uIndex,
        .uDynamicBufferOffset = tLightingDynamicData.uByteOffset,
        .uInstanceStart       = 0,
        .uInstanceCount       = 1
    });
    gptGfx->draw_stream(&tEncoder, 1, &tArea);
    gptGfx->reset_draw_stream(ptStream);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~subpass 2 - forward~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    gptGfx->next_subpass(&tEncoder);

    if(ptScene->tSkyboxTexture.uIndex != UINT32_MAX)
    {
        
        plDynamicBinding tSkyboxDynamicData = gptDevice->allocate_dynamic_data(ptDevice, sizeof(plMat4));
        plMat4* ptSkyboxDynamicData = (plMat4*)tSkyboxDynamicData.pcData;
        *ptSkyboxDynamicData = pl_mat4_translate_vec3(ptCamera->tPos);

        gptGfx->add_to_stream(ptStream, (plStreamDraw)
        {
            .uShaderVariant       = gptData->tSkyboxShader.uIndex,
            .uDynamicBuffer       = tSkyboxDynamicData.uBufferHandle,
            .uVertexBuffer        = ptScene->tVertexBuffer.uIndex,
            .uIndexBuffer         = ptScene->tIndexBuffer.uIndex,
            .uIndexOffset         = ptScene->tSkyboxDrawable.uIndexOffset,
            .uTriangleCount       = ptScene->tSkyboxDrawable.uIndexCount / 3,
            .uBindGroup0          = tGlobalBG.uIndex,
            .uBindGroup1          = ptScene->tSkyboxBindGroup.uIndex,
            .uBindGroup2          = UINT32_MAX,
            .uDynamicBufferOffset = tSkyboxDynamicData.uByteOffset,
            .uInstanceStart       = 0,
            .uInstanceCount       = 1
        });
        gptGfx->draw_stream(&tEncoder, 1, &tArea);
    }
    gptGfx->reset_draw_stream(ptStream);

    // forward rendering
    for(uint32_t i = 0; i < uVisibleTransparentDrawCount; i++)
    {
        const plDrawable tDrawable = ptView->sbtVisibleTransparentDrawables[i];
        plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
        plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
        
        plDynamicBinding tDynamicBinding = gptDevice->allocate_dynamic_data(ptDevice, sizeof(DynamicData));

        DynamicData* ptDynamicData = (DynamicData*)tDynamicBinding.pcData;
        ptDynamicData->iDataOffset = tDrawable.uDataOffset;
        ptDynamicData->iVertexOffset = tDrawable.uVertexOffset;
        ptDynamicData->tModel = ptTransform->tWorld;
        ptDynamicData->iMaterialOffset = tDrawable.uMaterialIndex;

        gptGfx->add_to_stream(ptStream, (plStreamDraw)
        {
            .uShaderVariant       = tDrawable.tShader.uIndex,
            .uDynamicBuffer       = tDynamicBinding.uBufferHandle,
            .uVertexBuffer        = ptScene->tVertexBuffer.uIndex,
            .uIndexBuffer         = tDrawable.uIndexCount == 0 ? UINT32_MAX : ptScene->tIndexBuffer.uIndex,
            .uIndexOffset         = tDrawable.uIndexOffset,
            .uTriangleCount       = tDrawable.uIndexCount == 0 ? tDrawable.uVertexCount / 3 : tDrawable.uIndexCount / 3,
            .uBindGroup0          = tGlobalBG.uIndex,
            .uBindGroup1          = tLightBindGroup2.uIndex,
            .uBindGroup2          = tDrawable.tMaterialBindGroup.uIndex,
            .uDynamicBufferOffset = tDynamicBinding.uByteOffset,
            .uInstanceStart       = 0,
            .uInstanceCount       = 1
        });
    }
    gptGfx->draw_stream(&tEncoder, 1, &tArea);

    // outlines
    const uint32_t uOutlineDrawableCount = pl_sb_size(ptScene->sbtOutlineDrawables);
    if(uOutlineDrawableCount > 0)
    {

        gptGfx->reset_draw_stream(ptStream);

        typedef struct _plOutlineDynamicData
        {
            plVec4 tColor;
            float  fThickness;
            int    iDataOffset;
            int    iVertexOffset;
            int    iPadding[1];
            plMat4 tModel;
        } plOutlineDynamicData;


        plBindGroupLayout tOutlineBindGroupLayout0 = {
            .uBufferBindingCount  = 2,
            .aBufferBindings = {
                { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                { .uSlot = 1, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
            },
        };
        plBindGroupHandle tOutlineGlobalBG = gptDevice->get_temporary_bind_group(ptDevice, &tOutlineBindGroupLayout0, "temporary outline global bind group");

        const plBindGroupUpdateBufferData atOutlineBufferData[] = 
        {
            { .uSlot = 0, .tBuffer = ptView->atGlobalBuffers[ptGraphics->uCurrentFrameIndex], .szBufferRange = sizeof(BindGroup_0)},
            { .uSlot = 1, .tBuffer = ptScene->tStorageBuffer, .szBufferRange = sizeof(plVec4) * pl_sb_size(ptScene->sbtVertexDataBuffer)},

        };

        plBindGroupUpdateData tOutlineBGData0 = {
            .uBufferCount = 2,
            .atBuffers = atOutlineBufferData,
        };
        gptDevice->update_bind_group(&ptGraphics->tDevice, tOutlineGlobalBG, &tOutlineBGData0);

        const plVec4 tOutlineColor = (plVec4){(float)sin(pl_get_io()->dTime * 3.0) * 0.25f + 0.75f, 0.0f, 0.0f, 1.0f};
        const plVec4 tOutlineColor2 = (plVec4){0.0f, tOutlineColor.r, 0.0f, 1.0f};
        for(uint32_t i = 0; i < uOutlineDrawableCount; i++)
        {
            const plDrawable tDrawable = ptScene->sbtOutlineDrawables[i];
            plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
            plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);

            plMeshComponent* ptMesh = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptObject->tMesh);
            gptGfx->add_3d_aabb(&ptView->t3DSelectionDrawList, ptMesh->tAABBFinal.tMin, ptMesh->tAABBFinal.tMax, tOutlineColor2, 0.02f);
            
            plDynamicBinding tDynamicBinding = gptDevice->allocate_dynamic_data(ptDevice, sizeof(plOutlineDynamicData));

            plOutlineDynamicData* ptDynamicData = (plOutlineDynamicData*)tDynamicBinding.pcData;
            ptDynamicData->iDataOffset = tDrawable.uDataOffset;
            ptDynamicData->iVertexOffset = tDrawable.uVertexOffset;
            ptDynamicData->tModel = ptTransform->tWorld;
            ptDynamicData->fThickness = 0.02f;
            ptDynamicData->tColor = tOutlineColor;

            gptGfx->add_to_stream(ptStream, (plStreamDraw)
            {
                .uShaderVariant       = tDrawable.tShader.uIndex,
                .uDynamicBuffer       = tDynamicBinding.uBufferHandle,
                .uVertexBuffer        = ptScene->tVertexBuffer.uIndex,
                .uIndexBuffer         = tDrawable.uIndexCount == 0 ? UINT32_MAX : ptScene->tIndexBuffer.uIndex,
                .uIndexOffset         = tDrawable.uIndexOffset,
                .uTriangleCount       = tDrawable.uIndexCount == 0 ? tDrawable.uVertexCount / 3 : tDrawable.uIndexCount / 3,
                .uBindGroup0          = tOutlineGlobalBG.uIndex,
                .uBindGroup1          = UINT32_MAX,
                .uBindGroup2          = UINT32_MAX,
                .uDynamicBufferOffset = tDynamicBinding.uByteOffset,
                .uInstanceStart       = 0,
                .uInstanceCount       = 1
            });
        }
        gptGfx->draw_stream(&tEncoder, 1, &tArea);
    }

    // light drawing
    for(uint32_t i = 0; i < pl_sb_size(ptScene->sbtLightData); i++)
    {
        if(ptScene->sbtLightData[i].iType == PL_LIGHT_TYPE_POINT)
        {
            const plVec4 tColor = {.rgb = ptScene->sbtLightData[i].tColor, .a = 1.0f};
            gptGfx->add_3d_point(&ptView->t3DDrawList, ptScene->sbtLightData[i].tPosition, tColor, 0.25f, 0.02f);
        }
    }

    // debug drawing
    if(tOptions.bShowAllBoundingBoxes)
    {
        for(uint32_t i = 0; i < uOpaqueDrawableCount; i++)
        {
            plMeshComponent* ptMesh = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptScene->sbtOpaqueDrawables[i].tEntity);

            gptGfx->add_3d_aabb(&ptView->t3DDrawList, ptMesh->tAABBFinal.tMin, ptMesh->tAABBFinal.tMax, (plVec4){1.0f, 0.0f, 0.0f, 1.0f}, 0.02f);
        }
        for(uint32_t i = 0; i < uTransparentDrawableCount; i++)
        {
            plMeshComponent* ptMesh = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptScene->sbtTransparentDrawables[i].tEntity);

            gptGfx->add_3d_aabb(&ptView->t3DDrawList, ptMesh->tAABBFinal.tMin, ptMesh->tAABBFinal.tMax, (plVec4){1.0f, 0.0f, 0.0f, 1.0f}, 0.02f);
        }
    }
    else if(tOptions.bShowVisibleBoundingBoxes)
    {
        for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
        {
            plMeshComponent* ptMesh = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptView->sbtVisibleOpaqueDrawables[i].tEntity);

            gptGfx->add_3d_aabb(&ptView->t3DDrawList, ptMesh->tAABBFinal.tMin, ptMesh->tAABBFinal.tMax, (plVec4){1.0f, 0.0f, 0.0f, 1.0f}, 0.02f);
        }
        for(uint32_t i = 0; i < uVisibleTransparentDrawCount; i++)
        {
            plMeshComponent* ptMesh = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptView->sbtVisibleTransparentDrawables[i].tEntity);

            gptGfx->add_3d_aabb(&ptView->t3DDrawList, ptMesh->tAABBFinal.tMin, ptMesh->tAABBFinal.tMax, (plVec4){1.0f, 0.0f, 0.0f, 1.0f}, 0.02f);
        }
    }

    if(tOptions.bShowOrigin)
    {
        const plMat4 tTransform = pl_identity_mat4();
        gptGfx->add_3d_transform(&ptView->t3DSelectionDrawList, &tTransform, 10.0f, 0.02f);
    }

    if(tOptions.ptCullCamera && tOptions.ptCullCamera != tOptions.ptViewCamera)
    {
        gptGfx->add_3d_frustum(&ptView->t3DSelectionDrawList, &tOptions.ptCullCamera->tTransformMat, tOptions.ptCullCamera->fFieldOfView, tOptions.ptCullCamera->fAspectRatio, tOptions.ptCullCamera->fNearZ, tOptions.ptCullCamera->fFarZ, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 0.02f);
    }

    const plMat4 tMVP = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);

    gptGfx->submit_3d_drawlist(&ptView->t3DDrawList, tEncoder, tDimensions.x, tDimensions.y, &tMVP, PL_PIPELINE_FLAG_DEPTH_TEST | PL_PIPELINE_FLAG_DEPTH_WRITE, 1);
    gptGfx->submit_3d_drawlist(&ptView->t3DSelectionDrawList, tEncoder, tDimensions.x, tDimensions.y, &tMVP, 0, 1);
    gptGfx->end_render_pass(&tEncoder);
    pl_end_profile_sample();
}

static plTextureId
pl_refr_get_view_texture_id(uint32_t uSceneHandle, uint32_t uViewHandle)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plRefView* ptView = &ptScene->atViews[uViewHandle];
    return ptView->tFinalTextureID[gptData->tGraphics.uCurrentFrameIndex];
}

static void
pl_add_drawable_objects_to_scene(uint32_t uSceneHandle, uint32_t uOpaqueCount, const plEntity* atOpaqueObjects, uint32_t uTransparentCount, const plEntity* atTransparentObjects)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];

    #if 1
    const uint32_t uTransparentStart = pl_sb_size(ptScene->sbtTransparentDrawables);
    pl_sb_add_n(ptScene->sbtTransparentDrawables, uTransparentCount);

    const uint32_t uOpaqueStart = pl_sb_size(ptScene->sbtOpaqueDrawables);
    pl_sb_add_n(ptScene->sbtOpaqueDrawables, uOpaqueCount);

    for(uint32_t i = 0; i < uOpaqueCount; i++)
        ptScene->sbtOpaqueDrawables[uOpaqueStart + i].tEntity = atOpaqueObjects[i];

    for(uint32_t i = 0; i < uTransparentCount; i++)
        ptScene->sbtTransparentDrawables[uTransparentStart + i].tEntity = atTransparentObjects[i];
    #endif

    #if 0 // send through forward pass only
    const uint32_t uTransparentStart = pl_sb_size(ptScene->sbtTransparentDrawables);
    pl_sb_add_n(ptScene->sbtTransparentDrawables, uTransparentCount + uOpaqueCount);

    for(uint32_t i = 0; i < uOpaqueCount; i++)
        ptScene->sbtTransparentDrawables[uTransparentStart + i].tEntity = atOpaqueObjects[i];

    for(uint32_t i = 0; i < uTransparentCount; i++)
        ptScene->sbtTransparentDrawables[uOpaqueCount + uTransparentStart + i].tEntity = atTransparentObjects[i];
    #endif

    #if 0 // send through deferred pass only
    const uint32_t uTransparentStart = pl_sb_size(ptScene->sbtOpaqueDrawables);
    pl_sb_add_n(ptScene->sbtOpaqueDrawables, uTransparentCount + uOpaqueCount);

    for(uint32_t i = 0; i < uOpaqueCount; i++)
        ptScene->sbtOpaqueDrawables[uTransparentStart + i].tEntity = atOpaqueObjects[i];

    for(uint32_t i = 0; i < uTransparentCount; i++)
        ptScene->sbtOpaqueDrawables[uOpaqueCount + uTransparentStart + i].tEntity = atTransparentObjects[i];
    #endif
}

//-----------------------------------------------------------------------------
// [SECTION] internal API implementation
//-----------------------------------------------------------------------------

static void
pl__add_drawable_skin_data_to_global_buffer(plRefScene* ptScene, uint32_t uDrawableIndex, plDrawable* atDrawables)
{
    plEntity tEntity = atDrawables[uDrawableIndex].tEntity;

    // get actual components
    plObjectComponent*   ptObject   = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tEntity);
    plMeshComponent*     ptMesh     = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptObject->tMesh);

    if(ptMesh->tSkinComponent.uIndex == UINT32_MAX)
        return;

    const uint32_t uVertexDataStartIndex = pl_sb_size(ptScene->sbtSkinVertexDataBuffer);
    const uint32_t uVertexCount          = pl_sb_size(ptMesh->sbtVertexPositions);

    // stride within storage buffer
    uint32_t uStride = 0;

    uint64_t ulVertexStreamMask = 0;

    // calculate vertex stream mask based on provided data
    if(pl_sb_size(ptMesh->sbtVertexPositions) > 0)  { uStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_POSITION; }
    if(pl_sb_size(ptMesh->sbtVertexNormals) > 0)    { uStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_NORMAL; }
    if(pl_sb_size(ptMesh->sbtVertexTangents) > 0)   { uStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TANGENT; }
    if(pl_sb_size(ptMesh->sbtVertexWeights[0]) > 0) { uStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0; }
    if(pl_sb_size(ptMesh->sbtVertexWeights[1]) > 0) { uStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1; }
    if(pl_sb_size(ptMesh->sbtVertexJoints[0]) > 0)  { uStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_JOINTS_0; }
    if(pl_sb_size(ptMesh->sbtVertexJoints[1]) > 0)  { uStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_JOINTS_1; }

    pl_sb_add_n(ptScene->sbtSkinVertexDataBuffer, uStride * uVertexCount);

    // current attribute offset
    uint32_t uOffset = 0;

    // positions
    const uint32_t uVertexPositionCount = pl_sb_size(ptMesh->sbtVertexPositions);
    for(uint32_t i = 0; i < uVertexPositionCount; i++)
    {
        const plVec3* ptPosition = &ptMesh->sbtVertexPositions[i];
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride].x = ptPosition->x;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride].y = ptPosition->y;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride].z = ptPosition->z;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride].w = 1.0f;
    }

    if(uVertexPositionCount > 0)
        uOffset += 1;

    // normals
    const uint32_t uVertexNormalCount = pl_sb_size(ptMesh->sbtVertexNormals);
    for(uint32_t i = 0; i < uVertexNormalCount; i++)
    {
        ptMesh->sbtVertexNormals[i] = pl_norm_vec3(ptMesh->sbtVertexNormals[i]);
        const plVec3* ptNormal = &ptMesh->sbtVertexNormals[i];
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].x = ptNormal->x;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].y = ptNormal->y;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].z = ptNormal->z;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].w = 0.0f;
    }

    if(uVertexNormalCount > 0)
        uOffset += 1;

    // tangents
    const uint32_t uVertexTangentCount = pl_sb_size(ptMesh->sbtVertexTangents);
    for(uint32_t i = 0; i < uVertexTangentCount; i++)
    {
        const plVec4* ptTangent = &ptMesh->sbtVertexTangents[i];
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].x = ptTangent->x;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].y = ptTangent->y;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].z = ptTangent->z;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].w = ptTangent->w;
    }

    if(uVertexTangentCount > 0)
        uOffset += 1;

    // joints 0
    const uint32_t uVertexJoint0Count = pl_sb_size(ptMesh->sbtVertexJoints[0]);
    for(uint32_t i = 0; i < uVertexJoint0Count; i++)
    {
        const plVec4* ptJoint = &ptMesh->sbtVertexJoints[0][i];
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].x = ptJoint->x;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].y = ptJoint->y;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].z = ptJoint->z;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].w = ptJoint->w;
    }

    if(uVertexJoint0Count > 0)
        uOffset += 1;

    // weights 0
    const uint32_t uVertexWeights0Count = pl_sb_size(ptMesh->sbtVertexWeights[0]);
    for(uint32_t i = 0; i < uVertexWeights0Count; i++)
    {
        const plVec4* ptWeight = &ptMesh->sbtVertexWeights[0][i];
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].x = ptWeight->x;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].y = ptWeight->y;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].z = ptWeight->z;
        ptScene->sbtSkinVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].w = ptWeight->w;
    }

    if(uVertexWeights0Count > 0)
        uOffset += 1;

    PL_ASSERT(uOffset == uStride && "sanity check");

    // stride within storage buffer
    uint32_t uDestStride = 0;

    // calculate vertex stream mask based on provided data
    if(pl_sb_size(ptMesh->sbtVertexNormals) > 0)               { uDestStride += 1; }
    if(pl_sb_size(ptMesh->sbtVertexTangents) > 0)              { uDestStride += 1; }
    if(pl_sb_size(ptMesh->sbtVertexColors[0]) > 0)             { uDestStride += 1; }
    if(pl_sb_size(ptMesh->sbtVertexColors[1]) > 0)             { uDestStride += 1; }
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[0]) > 0) { uDestStride += 1; }
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[1]) > 0) { uDestStride += 1; }
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[2]) > 0) { uDestStride += 1; }
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[3]) > 0) { uDestStride += 1; }
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[4]) > 0) { uDestStride += 1; }
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[5]) > 0) { uDestStride += 1; }
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[6]) > 0) { uDestStride += 1; }
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[7]) > 0) { uDestStride += 1; }

    plSkinData tSkinData = {
        .tEntity = ptMesh->tSkinComponent,
        .uVertexCount = uVertexCount,
        .iSourceDataOffset = uVertexDataStartIndex,
        .iDestDataOffset = atDrawables[uDrawableIndex].uDataOffset,
        .iDestVertexOffset = atDrawables[uDrawableIndex].uVertexOffset,
    };

    plSkinComponent* ptSkinComponent = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_SKIN, ptMesh->tSkinComponent);
    unsigned int textureWidth = (unsigned int)ceilf(sqrtf((float)(pl_sb_size(ptSkinComponent->sbtJoints) * 8)));
    pl_sb_resize(ptSkinComponent->sbtTextureData, textureWidth * textureWidth);
    const plTextureDesc tSkinTextureDesc = {
        .tDimensions = {(float)textureWidth, (float)textureWidth, 1},
        .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers     = 1,
        .uMips       = 1,
        .tType       = PL_TEXTURE_TYPE_2D,
        .tUsage      = PL_TEXTURE_USAGE_SAMPLED
    };

    for(uint32_t uFrameIndex = 0; uFrameIndex < PL_FRAMES_IN_FLIGHT; uFrameIndex++)
        tSkinData.atDynamicTexture[uFrameIndex] = pl__refr_create_texture_with_data(&tSkinTextureDesc, "joint texture", uFrameIndex, ptSkinComponent->sbtTextureData, sizeof(float) * 4 * textureWidth * textureWidth);



    int aiSpecializationData[] = {(int)ulVertexStreamMask, (int)uStride, (int)ptMesh->ulVertexStreamMask, (int)uDestStride};
    const plComputeShaderDescription tComputeShaderDesc = {
        #ifdef PL_METAL_BACKEND
        .pcShader = "../shaders/metal/skinning.metal",
        .pcShaderEntryFunc = "kernel_main",
        #else
        .pcShader = "skinning.comp.spv",
        .pcShaderEntryFunc = "main",
        #endif
        .uConstantCount = 4,
        .pTempConstantData = aiSpecializationData,
        .atConstants = {
            { .uID = 0, .uOffset = 0,               .tType = PL_DATA_TYPE_INT},
            { .uID = 1, .uOffset = sizeof(int),     .tType = PL_DATA_TYPE_INT},
            { .uID = 2, .uOffset = 2 * sizeof(int), .tType = PL_DATA_TYPE_INT},
            { .uID = 3, .uOffset = 3 * sizeof(int), .tType = PL_DATA_TYPE_INT}
        },
        .uBindGroupLayoutCount = 2,
        .atBindGroupLayouts = {
            {
                .uBufferBindingCount = 3,
                .aBufferBindings = {
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 0, .tStages = PL_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 1, .tStages = PL_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 2, .tStages = PL_STAGE_COMPUTE},
                },
                .uSamplerBindingCount = 1,
                .atSamplerBindings = {
                    {.uSlot = 3, .tStages = PL_STAGE_COMPUTE}
                }
            },
            {
                .uTextureBindingCount = 1,
                .atTextureBindings = {
                    {.uSlot =  0, .tStages = PL_STAGE_ALL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
                }
            }
        }
    };
    tSkinData.tShader = gptDevice->create_compute_shader(&gptData->tGraphics.tDevice, &tComputeShaderDesc);
    atDrawables[uDrawableIndex].uSkinIndex = pl_sb_size(ptScene->sbtSkinData);
    pl_sb_push(ptScene->sbtSkinData, tSkinData);
}

static void
pl__add_drawable_data_to_global_buffer(plRefScene* ptScene, uint32_t uDrawableIndex, plDrawable* atDrawables)
{

    plEntity tEntity = atDrawables[uDrawableIndex].tEntity;

    // get actual components
    plObjectComponent*   ptObject   = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tEntity);
    plMeshComponent*     ptMesh     = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptObject->tMesh);
    plMaterialComponent* ptMaterial = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MATERIAL, ptMesh->tMaterial);

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
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[0]) > 0) { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0; }
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[1]) > 0) { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1; }
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[2]) > 0) { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_2; }
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[3]) > 0) { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_3; }
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[4]) > 0) { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_4; }
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[5]) > 0) { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_5; }
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[6]) > 0) { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_6; }
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[7]) > 0) { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_7; }

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
    for(uint32_t i = 0; i < 8; i++)
    {
        const uint32_t uVertexTexCount = pl_sb_size(ptMesh->sbtVertexTextureCoordinates[i]);
        for(uint32_t j = 0; j < uVertexTexCount; j++)
        {
            const plVec2* ptTextureCoordinates = &(ptMesh->sbtVertexTextureCoordinates[i])[j];
            ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + j * uStride + uOffset].x = ptTextureCoordinates->u;
            ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + j * uStride + uOffset].y = ptTextureCoordinates->v;
            ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + j * uStride + uOffset].z = 0.0f;
            ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + j * uStride + uOffset].w = 0.0f;

        }

        if(uVertexTexCount > 0)
            uOffset += 1;
    }

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

    PL_ASSERT(uOffset == uStride && "sanity check");

    plGPUMaterial tMaterial = {
        .iMipCount = ptScene->iEnvironmentMips,
        .fMetallicFactor = ptMaterial->fMetalness,
        .fRoughnessFactor = ptMaterial->fRoughness,
        .tBaseColorFactor = ptMaterial->tBaseColor,
        .tEmissiveFactor = ptMaterial->tEmissiveColor.rgb,
        .fAlphaCutoff = ptMaterial->fAlphaCutoff,
        .fClearcoatFactor = ptMaterial->fClearcoatFactor,
        .fClearcoatRoughnessFactor = ptMaterial->fClearcoatRoughness,
        .fOcclusionStrength = 1.0f,
        .fEmissiveStrength = 1.0f,
        .fIor = ptMaterial->fRefraction,
        .fIridescenceFactor = ptMaterial->fIridescenceFactor,
        .fIridescenceIor = ptMaterial->fIridescenceIor,
        .fIridescenceThicknessMaximum = ptMaterial->fIridescenceThicknessMaximum,
        .fIridescenceThicknessMinimum = ptMaterial->fIridescenceThicknessMinimum,
        .tKHR_materials_specular_specularColorFactor = ptMaterial->tSpecularColor.rgb,
        .fKHR_materials_specular_specularFactor = ptMaterial->fSpecularFactor,
        .iBaseColorUVSet = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_BASE_COLOR_MAP].uUVSet,
        .iNormalUVSet = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_NORMAL_MAP].uUVSet,
        .iEmissiveUVSet = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_EMISSIVE_MAP].uUVSet,
        .iOcclusionUVSet = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_OCCLUSION_MAP].uUVSet,
        .iMetallicRoughnessUVSet = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_METAL_ROUGHNESS_MAP].uUVSet,
        .iClearcoatUVSet = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_CLEARCOAT_MAP].uUVSet,
        .iClearcoatRoughnessUVSet = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_CLEARCOAT_ROUGHNESS_MAP].uUVSet,
        .iClearcoatNormalUVSet = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_CLEARCOAT_NORMAL_MAP].uUVSet,
        .iSpecularUVSet = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_SPECULAR_MAP].uUVSet,
        .iSpecularColorUVSet = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_SPECULAR_COLOR_MAP].uUVSet,
        .iIridescenceUVSet = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_IRIDESCENCE_MAP].uUVSet,
        .iIridescenceThicknessUVSet = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_IRIDESCENCE_THICKNESS_MAP].uUVSet,
    };
    pl_sb_push(ptScene->sbtMaterialBuffer, tMaterial);

    atDrawables[uDrawableIndex].uIndexCount      = uIndexCount;
    atDrawables[uDrawableIndex].uVertexCount     = uVertexCount;
    atDrawables[uDrawableIndex].uIndexOffset     = uIndexBufferStart;
    atDrawables[uDrawableIndex].uVertexOffset    = uVertexPosStartIndex;
    atDrawables[uDrawableIndex].uDataOffset      = uVertexDataStartIndex;
    atDrawables[uDrawableIndex].uMaterialIndex   = uMaterialIndex;
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

    typedef struct _plOBB
    {
        plVec3 tCenter;
        plVec3 tExtents;
        plVec3 atAxes[3]; // Orthonormal basis
    } plOBB;

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
    const uint64_t ulIndex = pl_hm_lookup(&gptData->tVariantHashmap, ulVariantHash);

    if(ulIndex != UINT64_MAX)
        return gptData->_sbtVariantHandles[ulIndex];

    plShaderDescription tDesc = ptShader->tDescription;
    tDesc.tGraphicsState = ptVariant->tGraphicsState;
    tDesc.pTempConstantData = ptVariant->pTempConstantData;

    plShaderHandle tShader = gptDevice->create_shader(ptDevice, &tDesc);

    pl_hm_insert(&gptData->tVariantHashmap, ulVariantHash, pl_sb_size(gptData->_sbtVariantHandles));
    pl_sb_push(gptData->_sbtVariantHandles, tShader);
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
        // PL_BLEND_MODE_OPAQUE
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
pl__refr_job(uint32_t uJobIndex, void* pData)
{
    plMaterialComponent* sbtMaterials = pData;

    plMaterialComponent* ptMaterial = &sbtMaterials[uJobIndex];
    int texWidth, texHeight, texNumChannels;
    int texForceNumChannels = 4;

    for(uint32_t i = 0; i < PL_TEXTURE_SLOT_COUNT; i++)
    {

        if(gptResource->is_resource_valid(ptMaterial->atTextureMaps[i].tResource))
        {
            if(i == PL_TEXTURE_SLOT_BASE_COLOR_MAP || i == PL_TEXTURE_SLOT_EMISSIVE_MAP || i == PL_TEXTURE_SLOT_SPECULAR_COLOR_MAP)
            {
                size_t szResourceSize = 0;
                const char* pcFileData = gptResource->get_file_data(ptMaterial->atTextureMaps[i].tResource, &szResourceSize);
                float* rawBytes = gptImage->load_hdr_from_memory((unsigned char*)pcFileData, (int)szResourceSize, &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
                gptResource->set_buffer_data(ptMaterial->atTextureMaps[i].tResource, sizeof(float) * texWidth * texHeight * 4, rawBytes);
                ptMaterial->atTextureMaps[i].uWidth = texWidth;
                ptMaterial->atTextureMaps[i].uHeight = texHeight;
            }
            else
            {
                size_t szResourceSize = 0;
                const char* pcFileData = gptResource->get_file_data(ptMaterial->atTextureMaps[i].tResource, &szResourceSize);
                unsigned char* rawBytes = gptImage->load_from_memory((unsigned char*)pcFileData, (int)szResourceSize, &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
                PL_ASSERT(rawBytes);
                ptMaterial->atTextureMaps[i].uWidth = texWidth;
                ptMaterial->atTextureMaps[i].uHeight = texHeight;
                gptResource->set_buffer_data(ptMaterial->atTextureMaps[i].tResource, texWidth * texHeight * 4, rawBytes);
            }
        }
    }
}

static plTextureHandle
pl__refr_create_texture(const plTextureDesc* ptDesc, const char* pcName, uint32_t uIdentifier)
{
    // for convience
    plDevice* ptDevice = &gptData->tGraphics.tDevice;
 
    // create texture
    plTempAllocator tTempAllocator = {0};
    const plTextureHandle tHandle = gptDevice->create_texture(ptDevice, ptDesc, pl_temp_allocator_sprintf(&tTempAllocator, "texture %s: %u", pcName, uIdentifier));
    pl_temp_allocator_reset(&tTempAllocator);

    // retrieve new texture
    plTexture* ptTexture = gptDevice->get_texture(ptDevice, tHandle);

    // choose allocator
    plDeviceMemoryAllocatorI* ptAllocator = gptData->ptLocalBuddyAllocator;
    if(ptTexture->tMemoryRequirements.ulSize > PL_DEVICE_BUDDY_BLOCK_SIZE)
        ptAllocator = gptData->ptLocalDedicatedAllocator;

    // allocate memory
    const plDeviceMemoryAllocation tAllocation = ptAllocator->allocate(ptAllocator->ptInst, 
        ptTexture->tMemoryRequirements.uMemoryTypeBits,
        ptTexture->tMemoryRequirements.ulSize,
        ptTexture->tMemoryRequirements.ulAlignment,
        pl_temp_allocator_sprintf(&tTempAllocator, "texture alloc %s: %u", pcName, uIdentifier));

    // bind memory
    gptDevice->bind_texture_to_memory(ptDevice, tHandle, &tAllocation);
    pl_temp_allocator_free(&tTempAllocator);
    return tHandle;
}

static plTextureHandle
pl__refr_create_texture_with_data(const plTextureDesc* ptDesc, const char* pcName, uint32_t uIdentifier, const void* pData, size_t szSize)
{
    // for convience
    plGraphics* ptGraphics = &gptData->tGraphics;
    plDevice* ptDevice = &ptGraphics->tDevice;
 
    // create texture
    plTempAllocator tTempAllocator = {0};
    const plTextureHandle tHandle = gptDevice->create_texture(ptDevice, ptDesc, pl_temp_allocator_sprintf(&tTempAllocator, "texture %s: %u", pcName, uIdentifier));
    pl_temp_allocator_reset(&tTempAllocator);

    // retrieve new texture
    plTexture* ptTexture = gptDevice->get_texture(ptDevice, tHandle);

    // choose allocator
    plDeviceMemoryAllocatorI* ptAllocator = gptData->ptLocalBuddyAllocator;
    if(ptTexture->tMemoryRequirements.ulSize > PL_DEVICE_BUDDY_BLOCK_SIZE)
        ptAllocator = gptData->ptLocalDedicatedAllocator;

    // allocate memory
    const plDeviceMemoryAllocation tAllocation = ptAllocator->allocate(ptAllocator->ptInst, 
        ptTexture->tMemoryRequirements.uMemoryTypeBits,
        ptTexture->tMemoryRequirements.ulSize,
        ptTexture->tMemoryRequirements.ulAlignment,
        pl_temp_allocator_sprintf(&tTempAllocator, "texture alloc %s: %u", pcName, uIdentifier));

    // bind memory
    gptDevice->bind_texture_to_memory(ptDevice, tHandle, &tAllocation);
    pl_temp_allocator_free(&tTempAllocator);

    // if data is presented, upload using staging buffer
    if(pData)
    {
        PL_ASSERT(ptDesc->uLayers == 1); // this is for simple textures right now

        // copy data to staging buffer
        plBuffer* ptStagingBuffer = gptDevice->get_buffer(ptDevice, gptData->tStagingBufferHandle[0]);

        memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, pData, szSize);

        // begin recording
        plCommandBuffer tCommandBuffer = gptGfx->begin_command_recording(ptGraphics, NULL);
        
        // begin blit pass, copy texture, end pass
        plBlitEncoder tEncoder = gptGfx->begin_blit_pass(ptGraphics, &tCommandBuffer);

        const plBufferImageCopy tBufferImageCopy = {
            .tImageExtent = {(uint32_t)ptDesc->tDimensions.x, (uint32_t)ptDesc->tDimensions.y, 1},
            .uLayerCount = 1
        };

        gptGfx->copy_buffer_to_texture(&tEncoder, gptData->tStagingBufferHandle[0], tHandle, 1, &tBufferImageCopy);
        gptGfx->generate_mipmaps(&tEncoder, tHandle);
        gptGfx->end_blit_pass(&tEncoder);

        // finish recording
        gptGfx->end_command_recording(ptGraphics, &tCommandBuffer);

        // submit command buffer
        gptGfx->submit_command_buffer_blocking(ptGraphics, &tCommandBuffer, NULL);
    }
    return tHandle;
}

static plBufferHandle
pl__refr_create_staging_buffer(const plBufferDescription* ptDesc, const char* pcName, uint32_t uIdentifier)
{
    // for convience
    plDevice* ptDevice = &gptData->tGraphics.tDevice;

    // create buffer
    plTempAllocator tTempAllocator = {0};
    const plBufferHandle tHandle = gptDevice->create_buffer(ptDevice, ptDesc, pl_temp_allocator_sprintf(&tTempAllocator, "buffer %s: %u", pcName, uIdentifier));
    pl_temp_allocator_reset(&tTempAllocator);

    // retrieve new buffer
    plBuffer* ptBuffer = gptDevice->get_buffer(ptDevice, tHandle);

    // allocate memory
    const plDeviceMemoryAllocation tAllocation = gptData->ptStagingUnCachedAllocator->allocate(gptData->ptStagingUnCachedAllocator->ptInst, 
        ptBuffer->tMemoryRequirements.uMemoryTypeBits,
        ptBuffer->tMemoryRequirements.ulSize,
        ptBuffer->tMemoryRequirements.ulAlignment,
        pl_temp_allocator_sprintf(&tTempAllocator, "sbuffer alloc %s: %u", pcName, uIdentifier));

    // bind memory
    gptDevice->bind_buffer_to_memory(ptDevice, tHandle, &tAllocation);
    pl_temp_allocator_free(&tTempAllocator);
    return tHandle;
}

static plBufferHandle
pl__refr_create_local_buffer(const plBufferDescription* ptDesc, const char* pcName, uint32_t uIdentifier, const void* pData)
{
    // for convience
    plGraphics* ptGraphics = &gptData->tGraphics;
    plDevice* ptDevice = &ptGraphics->tDevice;
    
    // create buffer
    plTempAllocator tTempAllocator = {0};
    const plBufferHandle tHandle = gptDevice->create_buffer(ptDevice, ptDesc, pl_temp_allocator_sprintf(&tTempAllocator, "buffer %s: %u", pcName, uIdentifier));
    pl_temp_allocator_reset(&tTempAllocator);

    // retrieve new buffer
    plBuffer* ptBuffer = gptDevice->get_buffer(ptDevice, tHandle);

    // choose allocator
    plDeviceMemoryAllocatorI* ptAllocator = gptData->ptLocalBuddyAllocator;
    if(ptBuffer->tMemoryRequirements.ulSize > PL_DEVICE_BUDDY_BLOCK_SIZE)
        ptAllocator = gptData->ptLocalDedicatedAllocator;

    // allocate memory
    const plDeviceMemoryAllocation tAllocation = ptAllocator->allocate(ptAllocator->ptInst, 
        ptBuffer->tMemoryRequirements.uMemoryTypeBits,
        ptBuffer->tMemoryRequirements.ulSize,
        ptBuffer->tMemoryRequirements.ulAlignment,
        pl_temp_allocator_sprintf(&tTempAllocator, "lbuffer alloc %s: %u", pcName, uIdentifier));

    // bind memory
    gptDevice->bind_buffer_to_memory(ptDevice, tHandle, &tAllocation);
    pl_temp_allocator_free(&tTempAllocator);

    // if data is presented, upload using staging buffer
    if(pData)
    {
        // copy data to staging buffer
        plBuffer* ptStagingBuffer = gptDevice->get_buffer(ptDevice, gptData->tStagingBufferHandle[0]);
        memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, pData, ptDesc->uByteSize);

        // begin recording
        plCommandBuffer tCommandBuffer = gptGfx->begin_command_recording(ptGraphics, NULL);
        
        // begin blit pass, copy buffer, end pass
        plBlitEncoder tEncoder = gptGfx->begin_blit_pass(ptGraphics, &tCommandBuffer);
        gptGfx->copy_buffer(&tEncoder, gptData->tStagingBufferHandle[0], tHandle, 0, 0, ptDesc->uByteSize);
        gptGfx->end_blit_pass(&tEncoder);

        // finish recording
        gptGfx->end_command_recording(ptGraphics, &tCommandBuffer);

        // submit command buffer
        gptGfx->submit_command_buffer_blocking(ptGraphics, &tCommandBuffer, NULL);
    }
    return tHandle;
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
        .select_entities               = pl_refr_select_entities,
        .update_skin_textures          = pl_refr_update_skin_textures,
        .perform_skinning              = pl_refr_perform_skinning,
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