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
// [SECTION] internal API implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <float.h> // FLT_MAX
#include "pl.h"
#include "pl_renderer_ext.h"
#include "pl_profile.h"
#include "pl_log.h"
#include "pl_ds.h"
#include "pl_string.h"
#include "pl_memory.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extensions
#include "pl_graphics_ext.h"
#include "pl_ecs_ext.h"
#include "pl_resource_ext.h"
#include "pl_image_ext.h"
#include "pl_stats_ext.h"
#include "pl_gpu_allocators_ext.h"
#include "pl_job_ext.h"
#include "pl_draw_ext.h"
#include "pl_draw_backend_ext.h"
#include "pl_ui_ext.h"
#include "pl_shader_ext.h"
#include "pl_file_ext.h"

#define PL_MAX_VIEWS_PER_SCENE 4
#define PL_MAX_LIGHTS 1000

#ifndef PL_DEVICE_BUDDY_BLOCK_SIZE
    #define PL_DEVICE_BUDDY_BLOCK_SIZE 268435456
#endif

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plMemoryI*  gptMemory = NULL;
    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

    #ifndef PL_DS_ALLOC
        #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
        #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif

    static const plDataRegistryI*  gptDataRegistry  = NULL;
    static const plGraphicsI*      gptGfx           = NULL;
    static const plImageI*         gptImage         = NULL;
    static const plStatsI*         gptStats         = NULL;
    static const plGPUAllocatorsI* gptGpuAllocators = NULL;
    static const plJobI*           gptJob           = NULL;
    static const plDrawI*          gptDraw          = NULL;
    static const plDrawBackendI*   gptDrawBackend   = NULL;
    static const plUiI*            gptUI            = NULL;
    static const plIOI*            gptIOI           = NULL;
    static const plShaderI*        gptShader        = NULL;
    static const plFileI*          gptFile          = NULL;
    
    // experimental
    static const plCameraI*   gptCamera   = NULL;
    static const plResourceI* gptResource = NULL;
    static const plEcsI*      gptECS      = NULL;

    static struct _plIO* gptIO = 0;
#endif

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
    plTextureHandle       atDynamicTexture[PL_MAX_FRAMES_IN_FLIGHT];
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
    plBindGroupHandle tShadowMaterialBindGroup;
    uint32_t          uDataOffset;
    uint32_t          uVertexOffset;
    uint32_t          uVertexCount;
    uint32_t          uIndexOffset;
    uint32_t          uIndexCount;
    uint32_t          uMaterialIndex;
    plShaderHandle    tShader;
    plShaderHandle    tShadowShader;
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

    // Emissive Strength
    plVec3 tEmissiveFactor;
    float  fEmissiveStrength;
    
    // Alpha mode
    float fAlphaCutoff;
    float fOcclusionStrength;
    int _unused1[2];

    int iBaseColorUVSet;
    int iNormalUVSet;
    int iEmissiveUVSet;
    int iOcclusionUVSet;

    int iMetallicRoughnessUVSet;
    int _unused2[3];
} plGPUMaterial;

typedef struct _plGPULight
{
    plVec3 tPosition;
    float  fIntensity;

    plVec3 tDirection;
    int    iType;

    plVec3 tColor;
    float  fRange;

    int iShadowIndex;
    int iCascadeCount;
    int _unused[2];
} plGPULight;

typedef struct _plGPULightShadowData
{
	plVec4 cascadeSplits;
	plMat4 cascadeViewProjMat[4];
} plGPULightShadowData;

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

typedef struct _plShadowData
{
    plVec2          tResolution;
    plTextureHandle tDepthTexture[PL_MAX_FRAMES_IN_FLIGHT];
    plTextureHandle atDepthTextureViews[PL_MAX_SHADOW_CASCADES][PL_MAX_FRAMES_IN_FLIGHT];

    plRenderPassHandle atOpaqueRenderPasses[PL_MAX_SHADOW_CASCADES];

    plBufferHandle atCameraBuffers[PL_MAX_FRAMES_IN_FLIGHT];
} plShadowData;

typedef struct _plRefView
{
    // renderpasses
    plRenderPassHandle tRenderPass;
    plRenderPassHandle tPostProcessRenderPass;
    plRenderPassHandle tPickRenderPass;
    plRenderPassHandle tUVRenderPass;
    plVec2             tTargetSize;

    // g-buffer textures
    plTextureHandle tAlbedoTexture[PL_MAX_FRAMES_IN_FLIGHT];
    plTextureHandle tPositionTexture[PL_MAX_FRAMES_IN_FLIGHT];
    plTextureHandle tNormalTexture[PL_MAX_FRAMES_IN_FLIGHT];
    plTextureHandle tAOMetalRoughnessTexture[PL_MAX_FRAMES_IN_FLIGHT];
    plTextureHandle tRawOutputTexture[PL_MAX_FRAMES_IN_FLIGHT];
    plTextureHandle tDepthTexture[PL_MAX_FRAMES_IN_FLIGHT];

    // picking
    plTextureHandle tPickTexture;
    plTextureHandle tPickDepthTexture;

    // outlining
    plTextureHandle atUVMaskTexture0[PL_MAX_FRAMES_IN_FLIGHT];
    plTextureHandle atUVMaskTexture1[PL_MAX_FRAMES_IN_FLIGHT];
    plTextureHandle tLastUVMask;
    
    // output texture
    plTextureHandle   tFinalTexture[PL_MAX_FRAMES_IN_FLIGHT];
    plBindGroupHandle tFinalTextureHandle[PL_MAX_FRAMES_IN_FLIGHT];

    // lighting
    plBindGroupHandle tLightingBindGroup[PL_MAX_FRAMES_IN_FLIGHT];

    // GPU buffers
    plBufferHandle atGlobalBuffers[PL_MAX_FRAMES_IN_FLIGHT];

    // submitted drawables
    plDrawable* sbtVisibleOpaqueDrawables;
    plDrawable* sbtVisibleTransparentDrawables;

    // drawing api
    plDrawList3D* pt3DGizmoDrawList;
    plDrawList3D* pt3DDrawList;
    plDrawList3D* pt3DSelectionDrawList;

    // shadows
    plShadowData tShadowData;
    plBufferHandle atLightShadowDataBuffer[PL_MAX_FRAMES_IN_FLIGHT];
    plGPULightShadowData* sbtLightShadowData;
} plRefView;

typedef struct _plRefScene
{
    plShaderHandle tLightingShader;
    plShaderHandle tTonemapShader;

    // skybox resources (optional)
    int               iEnvironmentMips;
    plDrawable        tSkyboxDrawable;
    plTextureHandle   tSkyboxTexture;
    plBindGroupHandle tSkyboxBindGroup;
    plTextureHandle   tGGXLUTTexture;
    plTextureHandle   tLambertianEnvTexture;
    plTextureHandle   tGGXEnvTexture;

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

    // entity to drawable hashmaps
    plHashMap* ptOpaqueHashmap;
    plHashMap* ptTransparentHashmap;

    // material bindgroup reuse hashmaps
    plHashMap* ptShadowBindgroupHashmap;

} plRefScene;

typedef struct _plRefRendererData
{
    plDevice* ptDevice;
    plDeviceInfo tDeviceInfo;
    plSwapchain* ptSwap;
    plSurface* ptSurface;
    plTempAllocator tTempAllocator;

    // main render pass stuff
    plRenderPassHandle       tMainRenderPass;
    plRenderPassLayoutHandle tMainRenderPassLayout;
    plTextureHandle          tMSAATexture;

    // bind groups
    plBindGroupPool* ptBindGroupPool;
    plBindGroupPool* aptBindGroupPools[PL_MAX_FRAMES_IN_FLIGHT];

    // picking
    uint32_t uClickedFrame;
    plEntity tPickedEntity;

    // full quad
    plBufferHandle tFullQuadVertexBuffer;
    plBufferHandle tFullQuadIndexBuffer;

    // main renderpass layout (used as a template for views)
    plRenderPassLayoutHandle tRenderPassLayout;
    plRenderPassLayoutHandle tPostProcessRenderPassLayout;
    plRenderPassLayoutHandle tUVRenderPassLayout;
    plRenderPassLayoutHandle tDepthRenderPassLayout;
    plRenderPassLayoutHandle tPickRenderPassLayout;

    // shader templates (variants are made from these)
    plShaderHandle tShadowShader;
    plShaderHandle tAlphaShadowShader;
    plShaderHandle tOpaqueShader;
    plShaderHandle tTransparentShader;
    plShaderHandle tSkyboxShader;
    plShaderHandle tPickShader;

    // outline shaders
    plShaderHandle        tUVShader;
    plComputeShaderHandle tJFAShader;

    // graphics shader variant system
    plHashMap*      ptVariantHashmap;
    plShaderHandle* _sbtVariantHandles; // needed for cleanup

    // renderer specific log channel
    uint64_t uLogChannel;

    // GPU allocators
    plDeviceMemoryAllocatorI* ptLocalDedicatedAllocator;
    plDeviceMemoryAllocatorI* ptLocalBuddyAllocator;
    plDeviceMemoryAllocatorI* ptStagingUnCachedAllocator;
    plDeviceMemoryAllocatorI* ptStagingCachedAllocator;

    // default textures & samplers & bindgroups
    plSamplerHandle   tDefaultSampler;
    plSamplerHandle   tShadowSampler;
    plSamplerHandle   tEnvSampler;
    plTextureHandle   tDummyTexture;
    plTextureHandle   tDummyTextureCube;

    // scenes
    plRefScene* sbtScenes;

    // draw stream data
    plDrawStream tDrawStream;

    // staging (more robust system should replace this)
    plBufferHandle tCachedStagingBuffer;
    plBufferHandle tStagingBufferHandle[PL_MAX_FRAMES_IN_FLIGHT];
    uint32_t uStagingOffset;
    uint32_t uCurrentStagingFrameIndex;

    // sync
    plTimelineSemaphore* aptSemaphores[PL_MAX_FRAMES_IN_FLIGHT];
    uint64_t aulNextTimelineValue[PL_MAX_FRAMES_IN_FLIGHT];

    // command pools
    plCommandPool* atCmdPools[PL_MAX_FRAMES_IN_FLIGHT];

    // dynamic buffer system
    plDynamicDataBlock tCurrentDynamicDataBlock;

    // graphics options
    bool     bReloadSwapchain;
    bool     bVSync;
    float    fLambdaSplit;
    bool     bShowOrigin;
    bool     bFrustumCulling;
    bool     bDrawAllBoundingBoxes;
    bool     bDrawVisibleBoundingBoxes;
    bool     bShowSelectedBoundingBox;
    uint32_t uOutlineWidth;
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

//-----------------------------------------------------------------------------
// [SECTION] internal API
//-----------------------------------------------------------------------------

// dynamic data system

static inline plDynamicBinding
pl__allocate_dynamic_data(plDevice* ptDevice)
{
    return pl_allocate_dynamic_data(gptGfx, gptData->ptDevice, &gptData->tCurrentDynamicDataBlock);
}

// general helpers
static void pl__add_drawable_skin_data_to_global_buffer(plRefScene*, uint32_t uDrawableIndex, plDrawable* atDrawables);
static void pl__add_drawable_data_to_global_buffer(plRefScene*, uint32_t uDrawableIndex, plDrawable* atDrawables);
static bool pl__sat_visibility_test(plCameraComponent*, const plAABB*);

// shader variant system
static plShaderHandle pl__get_shader_variant(uint32_t uSceneHandle, plShaderHandle tHandle, const plShaderVariant* ptVariant);
static size_t         pl__get_data_type_size2(plDataType tType);
static plBlendState   pl__get_blend_state(plBlendMode tBlendMode);

// job system tasks
static void pl__refr_job           (uint32_t uJobIndex, void* pData);
static void pl__refr_cull_job      (uint32_t uJobIndex, void* pData);

// resource creation helpers
static plTextureHandle pl__refr_create_texture              (const plTextureDesc* ptDesc, const char* pcName, uint32_t uIdentifier, plTextureUsage tInitialUsage);
static plTextureHandle pl__refr_create_texture_with_data    (const plTextureDesc* ptDesc, const char* pcName, uint32_t uIdentifier, const void* pData, size_t szSize);
static plBufferHandle  pl__refr_create_staging_buffer       (const plBufferDesc* ptDesc, const char* pcName, uint32_t uIdentifier);
static plBufferHandle  pl__refr_create_cached_staging_buffer(const plBufferDesc* ptDesc, const char* pcName, uint32_t uIdentifier);
static plBufferHandle  pl__refr_create_local_buffer         (const plBufferDesc* ptDesc, const char* pcName, uint32_t uIdentifier, const void* pData);

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

static void
pl_refr_initialize(plWindow* ptWindow)
{

    // allocate renderer data
    gptData = PL_ALLOC(sizeof(plRefRendererData));
    memset(gptData, 0, sizeof(plRefRendererData));

    // register data with registry (for reloads)
    gptDataRegistry->set_data("ref renderer data", gptData);

    // add specific log channel for renderer
    plLogChannelInit tLogInit = {
        .tType       = PL_CHANNEL_TYPE_BUFFER,
        .uEntryCount = 1024
    };
    gptData->uLogChannel = pl_add_log_channel("Renderer", tLogInit);

    // default options
    gptData->bVSync = true;
    gptData->uOutlineWidth = 4;
    gptData->fLambdaSplit = 0.95f;
    gptData->bFrustumCulling = true;

    // picking defaults
    gptData->uClickedFrame = UINT32_MAX;
    gptData->tPickedEntity.ulData = UINT64_MAX;

    // shader default values
    gptData->tSkyboxShader = (plShaderHandle){0};

    // initialize graphics
    plGraphicsInit tGraphicsDesc = {
        .tFlags = PL_GRAPHICS_INIT_FLAGS_SWAPCHAIN_ENABLED
    };
    #ifndef NDEBUG
    tGraphicsDesc.tFlags |= PL_GRAPHICS_INIT_FLAGS_VALIDATION_ENABLED;
    #endif
    gptGfx->initialize(&tGraphicsDesc);
    gptData->ptSurface = gptGfx->create_surface(ptWindow);

    uint32_t uDeviceCount = 16;
    plDeviceInfo atDeviceInfos[16] = {0};
    gptGfx->enumerate_devices(atDeviceInfos, &uDeviceCount);

    // we will prefer discrete, then integrated
    int iBestDvcIdx = 0;
    int iDiscreteGPUIdx   = -1;
    int iIntegratedGPUIdx = -1;
    for(uint32_t i = 0; i < uDeviceCount; i++)
    {
        
        if(atDeviceInfos[i].tType == PL_DEVICE_TYPE_DISCRETE)
            iDiscreteGPUIdx = i;
        else if(atDeviceInfos[i].tType == PL_DEVICE_TYPE_INTEGRATED)
            iIntegratedGPUIdx = i;
    }

    if(iDiscreteGPUIdx > -1)
        iBestDvcIdx = iDiscreteGPUIdx;
    else if(iIntegratedGPUIdx > -1)
        iBestDvcIdx = iIntegratedGPUIdx;

    // create device
    const plDeviceInit tDeviceInit = {
        .uDeviceIdx = iBestDvcIdx,
        .ptSurface = gptData->ptSurface,
        .szDynamicBufferBlockSize = 134217728
    };
    gptData->ptDevice = gptGfx->create_device(&tDeviceInit);

    // create bind group pool
    const plBindGroupPoolDesc tBindGroupPoolDesc = {
        .tFlags                      = PL_BIND_GROUP_POOL_FLAGS_INDIVIDUAL_RESET,
        .szSamplerBindings           = 100000,
        .szUniformBufferBindings     = 100000,
        .szStorageBufferBindings     = 100000,
        .szSampledTextureBindings    = 100000,
        .szStorageTextureBindings    = 100000,
        .szAttachmentTextureBindings = 100000
    };
    gptData->ptBindGroupPool = gptGfx->create_bind_group_pool(gptData->ptDevice, &tBindGroupPoolDesc);

    // create swapchain
    const plSwapchainInit tSwapInit = {
        .bVSync = true,
        .tSampleCount = atDeviceInfos[iBestDvcIdx].tMaxSampleCount
    };
    gptData->ptSwap = gptGfx->create_swapchain(gptData->ptDevice, gptData->ptSurface, &tSwapInit);
    gptDataRegistry->set_data("device", gptData->ptDevice); // used by debug extension

    // create pools
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        gptData->atCmdPools[i] = gptGfx->create_command_pool(gptData->ptDevice, NULL);
        plBindGroupPoolDesc tPoolDesc = {
            .tFlags                      = PL_BIND_GROUP_POOL_FLAGS_NONE,
            .szSamplerBindings           = 10000,
            .szUniformBufferBindings     = 10000,
            .szStorageBufferBindings     = 10000,
            .szSampledTextureBindings    = 10000,
            .szStorageTextureBindings    = 10000,
            .szAttachmentTextureBindings = 10000
        };
        gptData->aptBindGroupPools[i] = gptGfx->create_bind_group_pool(gptData->ptDevice, &tPoolDesc);
    }

    // load gpu allocators
    gptData->ptLocalBuddyAllocator      = gptGpuAllocators->get_local_buddy_allocator(gptData->ptDevice);
    gptData->ptLocalDedicatedAllocator  = gptGpuAllocators->get_local_dedicated_allocator(gptData->ptDevice);
    gptData->ptStagingUnCachedAllocator = gptGpuAllocators->get_staging_uncached_allocator(gptData->ptDevice);
    gptData->ptStagingCachedAllocator   = gptGpuAllocators->get_staging_cached_allocator(gptData->ptDevice);

    // create staging buffers
    const plBufferDesc tStagingBufferDesc = {
        .tUsage     = PL_BUFFER_USAGE_STAGING,
        .szByteSize = 268435456
    };
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        gptData->tStagingBufferHandle[i] = pl__refr_create_staging_buffer(&tStagingBufferDesc, "staging", i);

    // create caching staging buffer
    const plBufferDesc tStagingCachedBufferDesc = {
        .tUsage     = PL_BUFFER_USAGE_STAGING,
        .szByteSize = 268435456
    };
    gptData->tCachedStagingBuffer = pl__refr_create_cached_staging_buffer(&tStagingBufferDesc, "cached staging", 0);

    // create dummy textures
    const plTextureDesc tDummyTextureDesc = {
        .tDimensions   = {2, 2, 1},
        .tFormat       = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED,
        .pcDebugName   = "dummy"
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
        .tUsage      = PL_TEXTURE_USAGE_SAMPLED,
        .pcDebugName = "dummy cube"
    };
    gptData->tDummyTextureCube = pl__refr_create_texture(&tSkyboxTextureDesc, "dummy cube", 0, PL_TEXTURE_USAGE_SAMPLED);

    // create samplers
    const plSamplerDesc tSamplerDesc = {
        .tMagFilter      = PL_FILTER_LINEAR,
        .tMinFilter      = PL_FILTER_LINEAR,
        .fMinMip         = 0.0f,
        .fMaxMip         = 64.0f,
        .tVAddressMode   = PL_ADDRESS_MODE_WRAP,
        .tUAddressMode   = PL_ADDRESS_MODE_WRAP,
        .pcDebugName     = "default sampler"
    };
    gptData->tDefaultSampler = gptGfx->create_sampler(gptData->ptDevice, &tSamplerDesc);

    const plSamplerDesc tShadowSamplerDesc = {
        .tMagFilter      = PL_FILTER_LINEAR,
        .tMinFilter      = PL_FILTER_LINEAR,
        .fMinMip         = 0.0f,
        .fMaxMip         = 1.0f,
        .fMaxAnisotropy  = 1.0f,
        .tVAddressMode   = PL_ADDRESS_MODE_CLAMP,
        .tUAddressMode   = PL_ADDRESS_MODE_CLAMP,
        .tMipmapMode     = PL_MIPMAP_MODE_NEAREST,
        .pcDebugName     = "shadow sampler"
    };
    gptData->tShadowSampler = gptGfx->create_sampler(gptData->ptDevice, &tShadowSamplerDesc);

    const plSamplerDesc tEnvSamplerDesc = {
        .tMagFilter      = PL_FILTER_LINEAR,
        .tMinFilter      = PL_FILTER_LINEAR,
        .fMinMip         = 0.0f,
        .fMaxMip         = 64.0f,
        .tVAddressMode   = PL_ADDRESS_MODE_CLAMP,
        .tUAddressMode   = PL_ADDRESS_MODE_CLAMP,
        .pcDebugName     = "ENV sampler"
    };
    gptData->tEnvSampler = gptGfx->create_sampler(gptData->ptDevice, &tEnvSamplerDesc);

    // create deferred render pass layout
    const plRenderPassLayoutDesc tRenderPassLayoutDesc = {
        .atRenderTargets = {
            { .tFormat = PL_FORMAT_D32_FLOAT_S8_UINT, .bDepth = true },  // depth buffer
            { .tFormat = PL_FORMAT_R32G32B32A32_FLOAT }, // final output
            { .tFormat = PL_FORMAT_R8G8B8A8_SRGB },      // albedo
            { .tFormat = PL_FORMAT_R32G32B32A32_FLOAT }, // normal
            { .tFormat = PL_FORMAT_R32G32B32A32_FLOAT }, // position
            { .tFormat = PL_FORMAT_R32G32B32A32_FLOAT }, // AO, roughness, metallic, specular weight
        },
        .atSubpasses = {
            { // G-buffer fill
                .uRenderTargetCount = 5,
                .auRenderTargets = {0, 2, 3, 4, 5}
            },
            { // lighting
                .uRenderTargetCount = 1,
                .auRenderTargets = {1},
                .uSubpassInputCount = 5,
                .auSubpassInputs = {0, 2, 3, 4, 5},
            },
            { // transparencies
                .uRenderTargetCount = 2,
                .auRenderTargets = {0, 1},
                .uSubpassInputCount = 0,
                .auSubpassInputs = {0}
            },
        }
    };
    gptData->tRenderPassLayout = gptGfx->create_render_pass_layout(gptData->ptDevice, &tRenderPassLayoutDesc);

    // create depth render pass layout
    const plRenderPassLayoutDesc tDepthRenderPassLayoutDesc = {
        .atRenderTargets = {
            { .tFormat = PL_FORMAT_D32_FLOAT, .bDepth = true },  // depth buffer
        },
        .atSubpasses = {
            {
                .uRenderTargetCount = 1,
                .auRenderTargets = {0},
            }
        }
    };
    gptData->tDepthRenderPassLayout = gptGfx->create_render_pass_layout(gptData->ptDevice, &tDepthRenderPassLayoutDesc);

    // create pick render pass layout
    const plRenderPassLayoutDesc tPickRenderPassLayoutDesc = {
        .atRenderTargets = {
            { .tFormat = PL_FORMAT_D32_FLOAT, .bDepth = true },  // depth buffer
            { .tFormat = PL_FORMAT_R8G8B8A8_UNORM }, // final output
        },
        .atSubpasses = {
            {
                .uRenderTargetCount = 2,
                .auRenderTargets = {0, 1}
            }
        }
    };
    gptData->tPickRenderPassLayout = gptGfx->create_render_pass_layout(gptData->ptDevice, &tPickRenderPassLayoutDesc);

    // create post processing render pass
    const plRenderPassLayoutDesc tPostProcessRenderPassLayoutDesc = {
        .atRenderTargets = {
            { .tFormat = PL_FORMAT_D32_FLOAT_S8_UINT, .bDepth = true }, // depth
            { .tFormat = PL_FORMAT_R32G32B32A32_FLOAT },
        },
        .atSubpasses = {
            {
                .uRenderTargetCount = 2,
                .auRenderTargets = {0, 1},
            },
        }
    };
    gptData->tPostProcessRenderPassLayout = gptGfx->create_render_pass_layout(gptData->ptDevice, &tPostProcessRenderPassLayoutDesc);

    const plRenderPassLayoutDesc tUVRenderPassLayoutDesc = {
        .atRenderTargets = {
            { .tFormat = PL_FORMAT_D32_FLOAT_S8_UINT, .bDepth = true }, // depth
            { .tFormat = PL_FORMAT_R32G32_FLOAT},
        },
        .atSubpasses = {
            {
                .uRenderTargetCount = 2,
                .auRenderTargets = {0, 1},
            },
        }
    };
    gptData->tUVRenderPassLayout = gptGfx->create_render_pass_layout(gptData->ptDevice, &tUVRenderPassLayoutDesc);

    // create template shaders

    int aiConstantData[6] = {0, 0, 0, 0, 0, 1};

    plShaderDesc tOpaqueShaderDescription = {
        .tPixelShader  = gptShader->load_glsl("../shaders/primitive.frag", "main", NULL, NULL),
        .tVertexShader = gptShader->load_glsl("../shaders/primitive.vert", "main", NULL, NULL),
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
        .atVertexBufferLayouts = {
            {
                .uByteStride  = sizeof(float) * 3,
                .atAttributes = { {.uByteOffset = 0, .tFormat = PL_FORMAT_R32G32B32_FLOAT}}
            }
        },
        .pTempConstantData = aiConstantData,
        .atBlendStates = {
            pl__get_blend_state(PL_BLEND_MODE_OPAQUE),
            pl__get_blend_state(PL_BLEND_MODE_OPAQUE),
            pl__get_blend_state(PL_BLEND_MODE_OPAQUE),
            pl__get_blend_state(PL_BLEND_MODE_OPAQUE)
        },
        .tRenderPassLayout = gptData->tRenderPassLayout,
        .uSubpassIndex = 0,
        .atBindGroupLayouts = {
            {
                .atBufferBindings = {
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
                .atSamplerBindings = {
                    {.uSlot = 3, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                    {.uSlot = 4, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                },
                .atTextureBindings = {
                    {.uSlot =   5, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   6, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   7, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1}
                }
            },
            {
                .atTextureBindings = {
                    {.uSlot =   0, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   1, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   2, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   3, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   4, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                }
            }
        }
    };
    for(uint32_t i = 0; i < 5; i++)
    {
        tOpaqueShaderDescription.atConstants[i].uID = i;
        tOpaqueShaderDescription.atConstants[i].uOffset = i * sizeof(int);
        tOpaqueShaderDescription.atConstants[i].tType = PL_DATA_TYPE_INT;
    }
    gptData->tOpaqueShader = gptGfx->create_shader(gptData->ptDevice, &tOpaqueShaderDescription);

    plShaderDesc tTransparentShaderDescription = {
        .tPixelShader = gptShader->load_glsl("../shaders/transparent.frag", "main", NULL, NULL),
        .tVertexShader = gptShader->load_glsl("../shaders/transparent.vert", "main", NULL, NULL),
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
        .atVertexBufferLayouts = {
            {
                .uByteStride = sizeof(float) * 3,
                .atAttributes = { {.uByteOffset = 0, .tFormat = PL_FORMAT_R32G32B32_FLOAT}}
            }
        },
        .pTempConstantData = aiConstantData,
        .atBlendStates = {
            pl__get_blend_state(PL_BLEND_MODE_ALPHA)
        },
        .tRenderPassLayout = gptData->tRenderPassLayout,
        .uSubpassIndex = 2,
        .atBindGroupLayouts = {
            {
                .atBufferBindings = {
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
                .atSamplerBindings = {
                    {.uSlot = 3, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                    {.uSlot = 4, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                },
                .atTextureBindings = {
                    {.uSlot =   5, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   6, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   7, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1}
                }
            },
            {
                .atBufferBindings = {
                    { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                    { .uSlot = 1, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                },
                .atTextureBindings = {
                    {.uSlot = 2, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 4},
                },
                .atSamplerBindings = {
                    {.uSlot = 6, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                },
            },
            {
                .atTextureBindings = {
                    {.uSlot =   0, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   1, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   2, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   3, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                    {.uSlot =   4, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1}
                }
            }
        }
    };
    for(uint32_t i = 0; i < 6; i++)
    {
        tTransparentShaderDescription.atConstants[i].uID = i;
        tTransparentShaderDescription.atConstants[i].uOffset = i * sizeof(int);
        tTransparentShaderDescription.atConstants[i].tType = PL_DATA_TYPE_INT;
    }
    gptData->tTransparentShader = gptGfx->create_shader(gptData->ptDevice, &tTransparentShaderDescription);

    plShaderDesc tShadowShaderDescription = {
        .tPixelShader = gptShader->load_glsl("../shaders/shadow.frag", "main", NULL, NULL),
        .tVertexShader = gptShader->load_glsl("../shaders/shadow.vert", "main", NULL, NULL),
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 1,
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
        .atVertexBufferLayouts = {
            {
                .uByteStride = sizeof(float) * 3,
                .atAttributes = { {.uByteOffset = 0, .tFormat = PL_FORMAT_R32G32B32_FLOAT}}
            }
        },
        .pTempConstantData = aiConstantData,
        .atBlendStates = {
            pl__get_blend_state(PL_BLEND_MODE_ALPHA)
        },
        .tRenderPassLayout = gptData->tDepthRenderPassLayout,
        .uSubpassIndex = 0,
        .atBindGroupLayouts = {
            {
                .atBufferBindings = {
                    { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                    { .uSlot = 1, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                    { .uSlot = 2, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                },
                .atSamplerBindings = {
                    {.uSlot = 3, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                },
            },
            {
                .atTextureBindings = {
                    {.uSlot = 0, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                }
            }
        }
    };
    for(uint32_t i = 0; i < 4; i++)
    {
        tShadowShaderDescription.atConstants[i].uID = i;
        tShadowShaderDescription.atConstants[i].uOffset = i * sizeof(int);
        tShadowShaderDescription.atConstants[i].tType = PL_DATA_TYPE_INT;
    }

    gptData->tAlphaShadowShader = gptGfx->create_shader(gptData->ptDevice, &tShadowShaderDescription);
    tShadowShaderDescription.tPixelShader.puCode = NULL;
    tShadowShaderDescription.tPixelShader.szCodeSize = 0;
    gptData->tShadowShader = gptGfx->create_shader(gptData->ptDevice, &tShadowShaderDescription);
        
    const plShaderDesc tPickShaderDescription = {
        .tPixelShader = gptShader->load_glsl("../shaders/picking.frag", "main", NULL, NULL),
        .tVertexShader = gptShader->load_glsl("../shaders/picking.vert", "main", NULL, NULL),
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 1,
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
        .atVertexBufferLayouts = {
            {
                .uByteStride = sizeof(float) * 3,
                .atAttributes = { {.uByteOffset = 0, .tFormat = PL_FORMAT_R32G32B32_FLOAT}}
            }
        },
        .atBlendStates = {
            pl__get_blend_state(PL_BLEND_MODE_OPAQUE)
        },
        .tRenderPassLayout = gptData->tPickRenderPassLayout,
        .uSubpassIndex = 0,
        .atBindGroupLayouts = {
            {
                .atBufferBindings = {
                    { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                },
            }
        }
    };
    gptData->tPickShader = gptGfx->create_shader(gptData->ptDevice, &tPickShaderDescription);

    const plShaderDesc tUVShaderDesc = {
        .tPixelShader = gptShader->load_glsl("../shaders/uvmap.frag", "main", NULL, NULL),
        .tVertexShader = gptShader->load_glsl("../shaders/uvmap.vert", "main", NULL, NULL),
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 0,
            .ulDepthMode          = PL_COMPARE_MODE_ALWAYS,
            .ulCullMode           = PL_CULL_MODE_NONE,
            .ulWireframe          = 0,
            .ulStencilTestEnabled = 1,
            .ulStencilMode        = PL_COMPARE_MODE_LESS,
            .ulStencilRef         = 128,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .atVertexBufferLayouts = {
            {
                .uByteStride = sizeof(float) * 4,
                .atAttributes = {
                    {.uByteOffset = 0,                 .tFormat = PL_FORMAT_R32G32_FLOAT},
                    {.uByteOffset = sizeof(float) * 2, .tFormat = PL_FORMAT_R32G32_FLOAT},
                }
            }
        },
        .atBlendStates = {
            {
                .bBlendEnabled = false
            }
        },
        .tRenderPassLayout = gptData->tUVRenderPassLayout
    };
    gptData->tUVShader = gptGfx->create_shader(gptData->ptDevice, &tUVShaderDesc);

    const plComputeShaderDesc tComputeShaderDesc = {
        .tShader = gptShader->load_glsl("../shaders/jumpfloodalgo.comp", "main", NULL, NULL),
        .atBindGroupLayouts = {
            {
                .atTextureBindings = {
                    {.uSlot = 0, .tStages = PL_STAGE_COMPUTE, .tType = PL_TEXTURE_BINDING_TYPE_STORAGE},
                    {.uSlot = 1, .tStages = PL_STAGE_COMPUTE, .tType = PL_TEXTURE_BINDING_TYPE_STORAGE}
                }
            }
        }
    };
    gptData->tJFAShader = gptGfx->create_compute_shader(gptData->ptDevice, &tComputeShaderDesc);
    pl_temp_allocator_reset(&gptData->tTempAllocator);

    // create full quad
    const uint32_t auFullQuadIndexBuffer[] = {0, 1, 2, 0, 2, 3};
    const plBufferDesc tFullQuadIndexBufferDesc = {
        .tUsage     = PL_BUFFER_USAGE_INDEX,
        .szByteSize = sizeof(uint32_t) * 6
    };
    gptData->tFullQuadIndexBuffer = pl__refr_create_local_buffer(&tFullQuadIndexBufferDesc, "full quad index buffer", 0, auFullQuadIndexBuffer);

    const float afFullQuadVertexBuffer[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 0.0f
    };
    const plBufferDesc tFullQuadVertexBufferDesc = {
        .tUsage     = PL_BUFFER_USAGE_VERTEX,
        .szByteSize = sizeof(float) * 16
    };
    gptData->tFullQuadVertexBuffer = pl__refr_create_local_buffer(&tFullQuadVertexBufferDesc, "full quad vertex buffer", 0, afFullQuadVertexBuffer);

    // create semaphores
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        gptData->aptSemaphores[i] = gptGfx->create_semaphore(gptData->ptDevice, false);

    const plRenderPassLayoutDesc tMainRenderPassLayoutDesc = {
        .atRenderTargets = {
            { .tFormat = gptGfx->get_swapchain_info(gptData->ptSwap).tFormat, .bResolve = true }, // swapchain
            { .tFormat = gptGfx->get_swapchain_info(gptData->ptSwap).tFormat, .tSamples = gptGfx->get_swapchain_info(gptData->ptSwap).tSampleCount}, // msaa
        },
        .atSubpasses = {
            {
                .uRenderTargetCount = 2,
                .auRenderTargets = {0, 1}
            }
        }
    };
    gptData->tMainRenderPassLayout = gptGfx->create_render_pass_layout(gptData->ptDevice, &tMainRenderPassLayoutDesc);

    plSwapchainInfo tInfo = gptGfx->get_swapchain_info(gptData->ptSwap);

    const plTextureDesc tColorTextureDesc = {
        .tDimensions   = {(float)tInfo.uWidth, (float)tInfo.uHeight, 1},
        .tFormat       = gptGfx->get_swapchain_info(gptData->ptSwap).tFormat,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
        .pcDebugName   = "offscreen color texture",
        .tSampleCount  = tInfo.tSampleCount
    };

    // create textures
    gptData->tMSAATexture = gptGfx->create_texture(gptData->ptDevice, &tColorTextureDesc, NULL);

    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(gptData->atCmdPools[0]);
    gptGfx->begin_command_recording(ptCommandBuffer, NULL);

    // begin blit pass, copy buffer, end pass
    plBlitEncoder* ptEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);

    // retrieve textures
    plTexture* ptColorTexture = gptGfx->get_texture(gptData->ptDevice, gptData->tMSAATexture);

    // allocate memory
    const plDeviceMemoryAllocation tColorAllocation = gptGfx->allocate_memory(gptData->ptDevice, 
        ptColorTexture->tMemoryRequirements.ulSize,
        PL_MEMORY_GPU,
        ptColorTexture->tMemoryRequirements.uMemoryTypeBits,
        "color texture memory");

    // bind memory
    gptGfx->bind_texture_to_memory(gptData->ptDevice, gptData->tMSAATexture, &tColorAllocation);

    // set initial usage
    gptGfx->set_texture_usage(ptEncoder, gptData->tMSAATexture, PL_TEXTURE_USAGE_COLOR_ATTACHMENT, 0);

    gptGfx->end_blit_pass(ptEncoder);

    // finish recording
    gptGfx->end_command_recording(ptCommandBuffer);

    // submit command buffer
    gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);
    gptGfx->return_command_buffer(ptCommandBuffer);

    const plRenderPassDesc tMainRenderPassDesc = {
        .tLayout = gptData->tMainRenderPassLayout,
        .tResolveTarget = { // swapchain image
            .tLoadOp       = PL_LOAD_OP_DONT_CARE,
            .tStoreOp      = PL_STORE_OP_STORE,
            .tCurrentUsage = PL_TEXTURE_USAGE_UNSPECIFIED,
            .tNextUsage    = PL_TEXTURE_USAGE_PRESENT,
            .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
        },
        .atColorTargets = { // msaa
            {
                .tLoadOp       = PL_LOAD_OP_CLEAR,
                .tStoreOp      = PL_STORE_OP_STORE_MULTISAMPLE_RESOLVE,
                .tCurrentUsage = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .tNextUsage    = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
            }
        },
        .tDimensions = {(float)tInfo.uWidth, (float)tInfo.uHeight},
        .ptSwapchain = gptData->ptSwap
    };
    uint32_t uImageCount = 0;
    plTextureHandle* atSwapchainImages = gptGfx->get_swapchain_images(gptData->ptSwap, &uImageCount);
    plRenderPassAttachments atMainAttachmentSets[16] = {0};
    for(uint32_t i = 0; i < uImageCount; i++)
    {
        atMainAttachmentSets[i].atViewAttachments[0] = atSwapchainImages[i];
        atMainAttachmentSets[i].atViewAttachments[1] = gptData->tMSAATexture;
    }
    gptData->tMainRenderPass = gptGfx->create_render_pass(gptData->ptDevice, &tMainRenderPassDesc, atMainAttachmentSets);
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

    return uSceneHandle;
}

static uint32_t
pl_refr_create_view(uint32_t uSceneHandle, plVec2 tDimensions)
{

    // for convience
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];

    // create view
    const uint32_t uViewHandle = ptScene->uViewCount++;
    PL_ASSERT(uViewHandle < PL_MAX_VIEWS_PER_SCENE);    
    plRefView* ptView = &ptScene->atViews[uViewHandle];

    ptView->tTargetSize = tDimensions;
    ptView->tShadowData.tResolution.x = 1024.0f * 4.0f;
    ptView->tShadowData.tResolution.y = 1024.0f * 4.0f;

    // create offscreen per-frame resources
    const plTextureDesc tRawOutputTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .pcDebugName   = "offscreen final"
    };

    const plTextureDesc tPickTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R8G8B8A8_UNORM,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
        .pcDebugName   = "pick original"
    };

    const plTextureDesc tPickDepthTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_D32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
        .pcDebugName   = "pick depth original"
    };

    const plTextureDesc tAttachmentTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .pcDebugName   = "g-buffer"
    };

    const plTextureDesc tAlbedoTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R8G8B8A8_SRGB,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .pcDebugName   = "albedo texture"
    };

    const plTextureDesc tDepthTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_D32_FLOAT_S8_UINT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT | PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .pcDebugName   = "offscreen depth texture"
    };

    const plTextureDesc tShadowDepthTextureDesc = {
        .tDimensions   = {ptView->tShadowData.tResolution.x, ptView->tShadowData.tResolution.y, 1},
        .tFormat       = PL_FORMAT_D32_FLOAT,
        .uLayers       = 4,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D_ARRAY,
        .tUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT | PL_TEXTURE_USAGE_SAMPLED,
        .pcDebugName   = "shadow map"
    };

    const plTextureDesc tMaskTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R32G32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_STORAGE,
        .pcDebugName   = "mask texture"
    };

    const plTextureDesc tEmmissiveTexDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .pcDebugName   = "emissive texture"
    };

    const plBufferDesc atGlobalBuffersDesc = {
        .tUsage     = PL_BUFFER_USAGE_UNIFORM | PL_BUFFER_USAGE_STAGING,
        .szByteSize = 134217728,
        .pcDebugName = "global buffer"
    };

    const plBufferDesc atCameraBuffersDesc = {
        .tUsage     = PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_STAGING,
        .szByteSize = 4096,
        .pcDebugName = "camera buffers"
    };

    const plBufferDesc atLightShadowDataBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_STAGING,
        .szByteSize = 134217728,
        .pcDebugName = "shadow data buffer"
    };

    const plBindGroupLayout tLightingBindGroupLayout = {
        .atTextureBindings = { 
            {.uSlot = 0, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 1, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 2, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 3, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 4, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT}
        }
    };

    // create offscreen render pass
    plRenderPassAttachments atPickAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
    plRenderPassAttachments atAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
    plRenderPassAttachments atUVAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
    plRenderPassAttachments atPostProcessAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
    plRenderPassAttachments atShadowAttachmentSets[PL_MAX_SHADOW_CASCADES][PL_MAX_FRAMES_IN_FLIGHT] = {0};

    ptView->tPickTexture = pl__refr_create_texture(&tPickTextureDesc, "pick original", 0, PL_TEXTURE_USAGE_SAMPLED);
    ptView->tPickDepthTexture = pl__refr_create_texture(&tPickDepthTextureDesc, "pick depth original", 0, PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT);

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        // textures
        ptView->tFinalTexture[i]            = pl__refr_create_texture(&tRawOutputTextureDesc,  "offscreen final", i, PL_TEXTURE_USAGE_SAMPLED);
        ptView->tFinalTextureHandle[i]      = gptDrawBackend->create_bind_group_for_texture(ptView->tFinalTexture[i]);
        ptView->tRawOutputTexture[i]        = pl__refr_create_texture(&tRawOutputTextureDesc,  "offscreen raw", i, PL_TEXTURE_USAGE_SAMPLED);
        ptView->tAlbedoTexture[i]           = pl__refr_create_texture(&tAlbedoTextureDesc, "albedo original", i, PL_TEXTURE_USAGE_COLOR_ATTACHMENT);
        ptView->tNormalTexture[i]           = pl__refr_create_texture(&tAttachmentTextureDesc, "normal original", i, PL_TEXTURE_USAGE_COLOR_ATTACHMENT);
        ptView->tPositionTexture[i]         = pl__refr_create_texture(&tAttachmentTextureDesc, "position original", i, PL_TEXTURE_USAGE_COLOR_ATTACHMENT);
        ptView->tAOMetalRoughnessTexture[i] = pl__refr_create_texture(&tEmmissiveTexDesc, "metalroughness original", i, PL_TEXTURE_USAGE_COLOR_ATTACHMENT);
        ptView->tDepthTexture[i]            = pl__refr_create_texture(&tDepthTextureDesc,      "offscreen depth original", i, PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT);
        ptView->atUVMaskTexture0[i]         = pl__refr_create_texture(&tMaskTextureDesc, "uv mask texture 0", i, PL_TEXTURE_USAGE_STORAGE);
        ptView->atUVMaskTexture1[i]         = pl__refr_create_texture(&tMaskTextureDesc, "uv mask texture 1", i, PL_TEXTURE_USAGE_STORAGE);

        // buffers
        ptView->atGlobalBuffers[i] = pl__refr_create_staging_buffer(&atGlobalBuffersDesc, "global", i);
        
        // lighting bind group
        const plBindGroupDesc tLightingBindGroupDesc = {
            .ptPool = gptData->ptBindGroupPool,
            .ptLayout = &tLightingBindGroupLayout,
            .pcDebugName = "lighting bind group"
        };
        ptView->tLightingBindGroup[i] = gptGfx->create_bind_group(gptData->ptDevice, &tLightingBindGroupDesc);

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
                .tTexture = ptView->tAOMetalRoughnessTexture[i],
                .uSlot    = 3,
                .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            },
            {
                .tTexture = ptView->tDepthTexture[i],
                .uSlot    = 4,
                .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            }
        };
        const plBindGroupUpdateData tBGData = {
            .uTextureCount = 5,
            .atTextureBindings = atBGTextureData
        };
        gptGfx->update_bind_group(gptData->ptDevice, ptView->tLightingBindGroup[i], &tBGData);
        pl_temp_allocator_reset(&gptData->tTempAllocator);

        // attachment sets
        atPickAttachmentSets[i].atViewAttachments[0] = ptView->tPickDepthTexture;
        atPickAttachmentSets[i].atViewAttachments[1] = ptView->tPickTexture;

        atAttachmentSets[i].atViewAttachments[0] = ptView->tDepthTexture[i];
        atAttachmentSets[i].atViewAttachments[1] = ptView->tRawOutputTexture[i];
        atAttachmentSets[i].atViewAttachments[2] = ptView->tAlbedoTexture[i];
        atAttachmentSets[i].atViewAttachments[3] = ptView->tNormalTexture[i];
        atAttachmentSets[i].atViewAttachments[4] = ptView->tPositionTexture[i];
        atAttachmentSets[i].atViewAttachments[5] = ptView->tAOMetalRoughnessTexture[i];

        atUVAttachmentSets[i].atViewAttachments[0] = ptView->tDepthTexture[i];
        atUVAttachmentSets[i].atViewAttachments[1] = ptView->atUVMaskTexture0[i];
        
        atPostProcessAttachmentSets[i].atViewAttachments[0] = ptView->tDepthTexture[i];
        atPostProcessAttachmentSets[i].atViewAttachments[1] = ptView->tFinalTexture[i];

        ptView->atLightShadowDataBuffer[i] = pl__refr_create_staging_buffer(&atLightShadowDataBufferDesc, "shadow", i);
        ptView->tShadowData.atCameraBuffers[i] = pl__refr_create_staging_buffer(&atCameraBuffersDesc, "shadow buffer", i);
        ptView->tShadowData.tDepthTexture[i] = pl__refr_create_texture(&tShadowDepthTextureDesc, "shadow map", i, PL_TEXTURE_USAGE_SAMPLED);

        plTextureViewDesc tShadowDepthView = {
            .tFormat     = PL_FORMAT_D32_FLOAT,
            .uBaseMip    = 0,
            .uMips       = 1,
            .uBaseLayer  = 0,
            .uLayerCount = 1,
            .tTexture    = ptView->tShadowData.tDepthTexture[i],
            .pcDebugName = "shadow view"
        };
        for(uint32_t j = 0; j < 4; j++)
        {
            // atDepthTextureViews
            tShadowDepthView.uBaseLayer = j;
            (ptView->tShadowData.atDepthTextureViews[j])[i] = gptGfx->create_texture_view(gptData->ptDevice, &tShadowDepthView);
            (atShadowAttachmentSets[j])[i].atViewAttachments[0] = (ptView->tShadowData.atDepthTextureViews[j])[i];
        }
    }

    const plRenderPassDesc tRenderPassDesc = {
        .tLayout = gptData->tRenderPassLayout,
        .tDepthTarget = {
                .tLoadOp         = PL_LOAD_OP_CLEAR,
                .tStoreOp        = PL_STORE_OP_DONT_CARE,
                .tStencilLoadOp  = PL_LOAD_OP_CLEAR,
                .tStencilStoreOp = PL_STORE_OP_STORE,
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
            }
        },
        .tDimensions = {.x = ptView->tTargetSize.x, .y = ptView->tTargetSize.y}
    };
    ptView->tRenderPass = gptGfx->create_render_pass(gptData->ptDevice, &tRenderPassDesc, atAttachmentSets);

    const plRenderPassDesc tPickRenderPassDesc = {
        .tLayout = gptData->tPickRenderPassLayout,
        .tDepthTarget = {
                .tLoadOp         = PL_LOAD_OP_CLEAR,
                .tStoreOp        = PL_STORE_OP_DONT_CARE,
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
            }
        },
        .tDimensions = {.x = ptView->tTargetSize.x, .y = ptView->tTargetSize.y}
    };
    ptView->tPickRenderPass = gptGfx->create_render_pass(gptData->ptDevice, &tPickRenderPassDesc, atPickAttachmentSets);

    const plRenderPassDesc tPostProcessRenderPassDesc = {
        .tLayout = gptData->tPostProcessRenderPassLayout,
        .tDepthTarget = {
                .tLoadOp         = PL_LOAD_OP_CLEAR,
                .tStoreOp        = PL_STORE_OP_DONT_CARE,
                .tStencilLoadOp  = PL_LOAD_OP_LOAD,
                .tStencilStoreOp = PL_STORE_OP_STORE,
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
            }
        },
        .tDimensions = {.x = ptView->tTargetSize.x, .y = ptView->tTargetSize.y}
    };
    ptView->tPostProcessRenderPass = gptGfx->create_render_pass(gptData->ptDevice, &tPostProcessRenderPassDesc, atPostProcessAttachmentSets);

    // register debug 3D drawlist
    ptView->pt3DDrawList = gptDraw->request_3d_drawlist();
    ptView->pt3DGizmoDrawList = gptDraw->request_3d_drawlist();
    ptView->pt3DSelectionDrawList = gptDraw->request_3d_drawlist();

    const plRenderPassDesc tDepthRenderPassDesc = {
        .tLayout = gptData->tDepthRenderPassLayout,
        .tDepthTarget = {
                .tLoadOp         = PL_LOAD_OP_CLEAR,
                .tStoreOp        = PL_STORE_OP_STORE,
                .tStencilLoadOp  = PL_LOAD_OP_CLEAR,
                .tStencilStoreOp = PL_STORE_OP_DONT_CARE,
                .tCurrentUsage   = PL_TEXTURE_USAGE_SAMPLED,
                .tNextUsage      = PL_TEXTURE_USAGE_SAMPLED,
                .fClearZ         = 1.0f
        },
        .tDimensions = {.x = ptView->tShadowData.tResolution.x, .y = ptView->tShadowData.tResolution.y}
    };

    // create offscreen renderpass
    const plRenderPassDesc tUVRenderPass0Desc = {
        .tLayout = gptData->tUVRenderPassLayout,
        .tDepthTarget = {
                .tLoadOp         = PL_LOAD_OP_CLEAR,
                .tStoreOp        = PL_STORE_OP_DONT_CARE,
                .tStencilLoadOp  = PL_LOAD_OP_LOAD,
                .tStencilStoreOp = PL_STORE_OP_STORE,
                .tCurrentUsage   = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
                .tNextUsage      = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
                .fClearZ         = 1.0f
        },
        .atColorTargets = {
            {
                .tLoadOp       = PL_LOAD_OP_CLEAR,
                .tStoreOp      = PL_STORE_OP_STORE,
                .tCurrentUsage = PL_TEXTURE_USAGE_STORAGE,
                .tNextUsage    = PL_TEXTURE_USAGE_STORAGE,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 0.0f}
            }
        },
        .tDimensions = {.x = ptView->tTargetSize.x, .y = ptView->tTargetSize.y}
    };
    ptView->tUVRenderPass = gptGfx->create_render_pass(gptData->ptDevice, &tUVRenderPass0Desc, atUVAttachmentSets);

    for(uint32_t i = 0; i < 4; i++)
    {
        ptView->tShadowData.atOpaqueRenderPasses[i] = gptGfx->create_render_pass(gptData->ptDevice, &tDepthRenderPassDesc, atShadowAttachmentSets[i]);
    }

    return uViewHandle;
}

static void
pl_refr_resize_view(uint32_t uSceneHandle, uint32_t uViewHandle, plVec2 tDimensions)
{
    // for convience
    plDevice*   ptDevice = gptData->ptDevice;
    plRefScene* ptScene  = &gptData->sbtScenes[uSceneHandle];
    plRefView*  ptView   = &ptScene->atViews[uViewHandle];

    // update offscreen size to match viewport
    ptView->tTargetSize = tDimensions;

    // recreate offscreen color & depth textures
    const plTextureDesc tRawOutputTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT
    };

    const plTextureDesc tPickTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R8G8B8A8_UNORM,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT
    };

    const plTextureDesc tPickDepthTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_D32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT
    };

    const plTextureDesc tAttachmentTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT
    };

    const plTextureDesc tAlbedoTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R8G8B8A8_SRGB,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT
    };

    const plTextureDesc tDepthTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_D32_FLOAT_S8_UINT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT | PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_INPUT_ATTACHMENT
    };

    const plTextureDesc tMaskTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R32G32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_STORAGE
    };

    const plTextureDesc tEmmissiveTexDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT
    };

    const plBindGroupLayout tLightingBindGroupLayout = {
        .atTextureBindings = { 
            {.uSlot = 0, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 1, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 2, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 3, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 4, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT}
        }
    };

    // update offscreen render pass attachments
    plRenderPassAttachments atAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
    plRenderPassAttachments atUVAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
    plRenderPassAttachments atPostProcessAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
    plRenderPassAttachments atPickAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};

    gptGfx->queue_texture_for_deletion(ptDevice, ptView->tPickTexture);
    gptGfx->queue_texture_for_deletion(ptDevice, ptView->tPickDepthTexture);
    ptView->tPickTexture = pl__refr_create_texture(&tPickTextureDesc, "pick", 0, PL_TEXTURE_USAGE_SAMPLED);
    ptView->tPickDepthTexture = pl__refr_create_texture(&tPickDepthTextureDesc, "pick depth", 0, PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT);

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {

        // queue old resources for deletion
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->atUVMaskTexture0[i]);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->atUVMaskTexture1[i]);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tFinalTexture[i]);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tRawOutputTexture[i]);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tAlbedoTexture[i]);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tNormalTexture[i]);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tPositionTexture[i]);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tAOMetalRoughnessTexture[i]);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tDepthTexture[i]);
        gptGfx->queue_bind_group_for_deletion(ptDevice, ptView->tLightingBindGroup[i]);

        // textures
        ptView->tFinalTexture[i]            = pl__refr_create_texture(&tRawOutputTextureDesc,  "offscreen final", i, PL_TEXTURE_USAGE_SAMPLED);
        ptView->tFinalTextureHandle[i]      = gptDrawBackend->create_bind_group_for_texture(ptView->tFinalTexture[i]);
        ptView->tRawOutputTexture[i]        = pl__refr_create_texture(&tRawOutputTextureDesc,  "offscreen raw", i, PL_TEXTURE_USAGE_SAMPLED);
        ptView->tAlbedoTexture[i]           = pl__refr_create_texture(&tAlbedoTextureDesc, "albedo original", i, PL_TEXTURE_USAGE_COLOR_ATTACHMENT);
        ptView->tNormalTexture[i]           = pl__refr_create_texture(&tAttachmentTextureDesc, "normal original", i, PL_TEXTURE_USAGE_COLOR_ATTACHMENT);
        ptView->tPositionTexture[i]         = pl__refr_create_texture(&tAttachmentTextureDesc, "position original", i, PL_TEXTURE_USAGE_COLOR_ATTACHMENT);
        ptView->tAOMetalRoughnessTexture[i] = pl__refr_create_texture(&tEmmissiveTexDesc, "metalroughness original", i, PL_TEXTURE_USAGE_COLOR_ATTACHMENT);
        ptView->tDepthTexture[i]            = pl__refr_create_texture(&tDepthTextureDesc,      "offscreen depth original", i, PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT);
        ptView->atUVMaskTexture0[i]         = pl__refr_create_texture(&tMaskTextureDesc, "uv mask texture 0", i, PL_TEXTURE_USAGE_STORAGE);
        ptView->atUVMaskTexture1[i]         = pl__refr_create_texture(&tMaskTextureDesc, "uv mask texture 1", i, PL_TEXTURE_USAGE_STORAGE);


        // lighting bind group
        plTempAllocator tTempAllocator = {0};
        const plBindGroupDesc tLightingBindGroupDesc = {
            .ptPool = gptData->ptBindGroupPool,
            .ptLayout = &tLightingBindGroupLayout,
            .pcDebugName = "lighting bind group"
        };
        ptView->tLightingBindGroup[i] = gptGfx->create_bind_group(gptData->ptDevice, &tLightingBindGroupDesc);
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
                .tTexture = ptView->tAOMetalRoughnessTexture[i],
                .uSlot    = 3,
                .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            },
            {
                .tTexture = ptView->tDepthTexture[i],
                .uSlot    = 4,
                .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            }
        };
        const plBindGroupUpdateData tBGData = {
            .uTextureCount = 5,
            .atTextureBindings = atBGTextureData
        };
        gptGfx->update_bind_group(gptData->ptDevice, ptView->tLightingBindGroup[i], &tBGData);
        pl_temp_allocator_free(&tTempAllocator);

        // attachment sets
        atPickAttachmentSets[i].atViewAttachments[0] = ptView->tPickDepthTexture;
        atPickAttachmentSets[i].atViewAttachments[1] = ptView->tPickTexture;

        atAttachmentSets[i].atViewAttachments[0] = ptView->tDepthTexture[i];
        atAttachmentSets[i].atViewAttachments[1] = ptView->tRawOutputTexture[i];
        atAttachmentSets[i].atViewAttachments[2] = ptView->tAlbedoTexture[i];
        atAttachmentSets[i].atViewAttachments[3] = ptView->tNormalTexture[i];
        atAttachmentSets[i].atViewAttachments[4] = ptView->tPositionTexture[i];
        atAttachmentSets[i].atViewAttachments[5] = ptView->tAOMetalRoughnessTexture[i];
        
        atPostProcessAttachmentSets[i].atViewAttachments[0] = ptView->tDepthTexture[i];
        atPostProcessAttachmentSets[i].atViewAttachments[1] = ptView->tFinalTexture[i];

        atUVAttachmentSets[i].atViewAttachments[0] = ptView->tDepthTexture[i];
        atUVAttachmentSets[i].atViewAttachments[1] = ptView->atUVMaskTexture0[i];
    }
    gptGfx->update_render_pass_attachments(ptDevice, ptView->tRenderPass, ptView->tTargetSize, atAttachmentSets);
    gptGfx->update_render_pass_attachments(ptDevice, ptView->tPostProcessRenderPass, ptView->tTargetSize, atPostProcessAttachmentSets);
    gptGfx->update_render_pass_attachments(ptDevice, ptView->tPickRenderPass, ptView->tTargetSize, atPickAttachmentSets);
    gptGfx->update_render_pass_attachments(ptDevice, ptView->tUVRenderPass, ptView->tTargetSize, atUVAttachmentSets);
}

static void
pl_refr_cleanup(void)
{
    pl_temp_allocator_free(&gptData->tTempAllocator);
    gptGfx->cleanup_draw_stream(&gptData->tDrawStream);

    for(uint32_t i = 0; i < pl_sb_size(gptData->sbtScenes); i++)
    {
        plRefScene* ptScene = &gptData->sbtScenes[i];
        for(uint32_t j = 0; j < ptScene->uViewCount; j++)
        {
            plRefView* ptView = &ptScene->atViews[j];
            pl_sb_free(ptView->sbtVisibleOpaqueDrawables);
            pl_sb_free(ptView->sbtVisibleTransparentDrawables);
            pl_sb_free(ptView->sbtLightShadowData);
        }
        pl_sb_free(ptScene->sbtLightData);
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
        pl_hm_free(ptScene->ptOpaqueHashmap);
        pl_hm_free(ptScene->ptTransparentHashmap);
        pl_hm_free(ptScene->ptShadowBindgroupHashmap);
        gptECS->cleanup_component_library(&ptScene->tComponentLibrary);
    }
    for(uint32_t i = 0; i < pl_sb_size(gptData->_sbtVariantHandles); i++)
    {
        plShader* ptShader = gptGfx->get_shader(gptData->ptDevice, gptData->_sbtVariantHandles[i]);
        gptGfx->queue_shader_for_deletion(gptData->ptDevice, gptData->_sbtVariantHandles[i]);
    }
    pl_sb_free(gptData->_sbtVariantHandles);
    pl_hm_free(gptData->ptVariantHashmap);
    gptGfx->flush_device(gptData->ptDevice);

    gptGfx->destroy_texture(gptData->ptDevice, gptData->tMSAATexture);

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        gptGfx->cleanup_semaphore(gptData->aptSemaphores[i]);

    gptGfx->cleanup_bind_group_pool(gptData->ptBindGroupPool);
    gptGpuAllocators->cleanup(gptData->ptDevice);
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        gptGfx->cleanup_bind_group_pool(gptData->aptBindGroupPools[i]);
        gptGfx->cleanup_command_pool(gptData->atCmdPools[i]);
    }
    gptGfx->cleanup_swapchain(gptData->ptSwap);
    gptGfx->cleanup_surface(gptData->ptSurface);
    gptGfx->cleanup_device(gptData->ptDevice);
    gptGfx->cleanup();

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

static plDevice*
pl_refr_get_device(void)
{
    return gptData->ptDevice;
}

static plSwapchain*
pl_refr_get_swapchain(void)
{
    return gptData->ptSwap;
}

static void
pl_refr_load_skybox_from_panorama(uint32_t uSceneHandle, const char* pcPath, int iResolution)
{
    pl_begin_profile_sample(0, __FUNCTION__);
    const int iSamples = 512;
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plDevice* ptDevice = gptData->ptDevice;
    plCommandPool* ptCmdPool = gptData->atCmdPools[gptGfx->get_current_frame_index()];

    // create skybox shader if we haven't
    if(gptData->tSkyboxShader.uIndex == 0)
    {
        // create skybox shader
        plShaderDesc tSkyboxShaderDesc = {
            .tPixelShader = gptShader->load_glsl("../shaders/skybox.frag", "main", NULL, NULL),
            .tVertexShader = gptShader->load_glsl("../shaders/skybox.vert", "main", NULL, NULL),
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
            .atVertexBufferLayouts = {
                {
                    .uByteStride = sizeof(float) * 3,
                    .atAttributes = { {.uByteOffset = 0, .tFormat = PL_FORMAT_R32G32B32_FLOAT}}
                }
            },
            .atBlendStates = {
                pl__get_blend_state(PL_BLEND_MODE_OPAQUE)
            },
            .tRenderPassLayout = gptData->tRenderPassLayout,
            .uSubpassIndex = 2,
            .atBindGroupLayouts = {
                {
                    .atBufferBindings = {
                        { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,  .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                        { .uSlot = 1, .tType = PL_BUFFER_BINDING_TYPE_STORAGE,  .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                        { .uSlot = 2, .tType = PL_BUFFER_BINDING_TYPE_STORAGE,  .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                    },
                    .atSamplerBindings = {
                        {.uSlot = 3, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                        {.uSlot = 4, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                    },
                    .atTextureBindings = {
                        {.uSlot = 5, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                        {.uSlot = 6, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                        {.uSlot = 7, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1}
                    }
                },
                {
                    .atTextureBindings = {
                        { .uSlot = 0, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                    },
                }
            }
        };
        gptData->tSkyboxShader = gptGfx->create_shader(gptData->ptDevice, &tSkyboxShaderDesc);
    }

    int iPanoramaWidth = 0;
    int iPanoramaHeight = 0;
    int iUnused = 0;
    pl_begin_profile_sample(0, "load image");
    size_t szImageFileSize = 0;
    gptFile->binary_read(pcPath, &szImageFileSize, NULL);
    unsigned char* pucBuffer = PL_ALLOC(szImageFileSize);
    gptFile->binary_read(pcPath, &szImageFileSize, pucBuffer);
    float* pfPanoramaData = gptImage->load_hdr(pucBuffer, (int)szImageFileSize, &iPanoramaWidth, &iPanoramaHeight, &iUnused, 4);
    PL_FREE(pucBuffer);
    pl_end_profile_sample(0);
    PL_ASSERT(pfPanoramaData);

    ptScene->iEnvironmentMips = (uint32_t)floorf(log2f((float)pl_maxi(iResolution, iResolution))) - 3; // guarantee final dispatch during filtering is 16 threads

    const size_t uFaceSize = ((size_t)iResolution * (size_t)iResolution) * 4 * sizeof(float);

    pl_begin_profile_sample(0, "step 0");
    {
        int aiSkyboxSpecializationData[] = {iResolution, iPanoramaWidth, iPanoramaHeight};
        const plComputeShaderDesc tSkyboxComputeShaderDesc = {
            .tShader = gptShader->load_glsl("../shaders/panorama_to_cubemap.comp", "main", NULL, NULL),
            .pTempConstantData = aiSkyboxSpecializationData,
            .atConstants = {
                { .uID = 0, .uOffset = 0,               .tType = PL_DATA_TYPE_INT},
                { .uID = 1, .uOffset = sizeof(int),     .tType = PL_DATA_TYPE_INT},
                { .uID = 2, .uOffset = 2 * sizeof(int), .tType = PL_DATA_TYPE_INT}
            },
            .atBindGroupLayouts = {
                {
                    .atBufferBindings = {
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
        plComputeShaderHandle tPanoramaShader = gptGfx->create_compute_shader(ptDevice, &tSkyboxComputeShaderDesc);
        pl_temp_allocator_reset(&gptData->tTempAllocator);

        plBufferHandle atComputeBuffers[7] = {0};
        const uint32_t uPanoramaSize = iPanoramaHeight * iPanoramaWidth * 4 * sizeof(float);
        const plBufferDesc tInputBufferDesc = {
            .tUsage     = PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_STAGING,
            .szByteSize = uPanoramaSize,
            .pcDebugName = "panorama input buffer"
        };
        atComputeBuffers[0] = pl__refr_create_staging_buffer(&tInputBufferDesc, "panorama input", 0);
        plBuffer* ptComputeBuffer = gptGfx->get_buffer(ptDevice, atComputeBuffers[0]);
        memcpy(ptComputeBuffer->tMemoryAllocation.pHostMapped, pfPanoramaData, uPanoramaSize);
        
        gptImage->free(pfPanoramaData);

        const plBufferDesc tOutputBufferDesc = {
            .tUsage    = PL_BUFFER_USAGE_STORAGE,
            .szByteSize = uFaceSize,
            .pcDebugName = "panorama output buffer"
        };
        
        for(uint32_t i = 0; i < 6; i++)
            atComputeBuffers[i + 1] = pl__refr_create_local_buffer(&tOutputBufferDesc, "panorama output", i, NULL);

        plBindGroupLayout tComputeBindGroupLayout = {
            .atBufferBindings = {
                { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 0, .tStages = PL_STAGE_COMPUTE},
                { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 1, .tStages = PL_STAGE_COMPUTE},
                { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 2, .tStages = PL_STAGE_COMPUTE},
                { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 3, .tStages = PL_STAGE_COMPUTE},
                { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 4, .tStages = PL_STAGE_COMPUTE},
                { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 5, .tStages = PL_STAGE_COMPUTE},
                { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 6, .tStages = PL_STAGE_COMPUTE},
            },
        };
        const plBindGroupDesc tComputeBindGroupDesc = {
            .ptPool      = gptData->aptBindGroupPools[gptGfx->get_current_frame_index()],
            .ptLayout    = &tComputeBindGroupLayout,
            .pcDebugName = "compute bind group"
        };
        plBindGroupHandle tComputeBindGroup = gptGfx->create_bind_group(ptDevice, &tComputeBindGroupDesc);
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
            .atBufferBindings = atBGBufferData
        };
        gptGfx->update_bind_group(ptDevice, tComputeBindGroup, &tBGData);
        gptGfx->queue_bind_group_for_deletion(ptDevice, tComputeBindGroup);

        // calculate cubemap data
        const plDispatch tDispach = {
            .uGroupCountX     = (uint32_t)iResolution / 16,
            .uGroupCountY     = (uint32_t)iResolution / 16,
            .uGroupCountZ     = 2,
            .uThreadPerGroupX = 16,
            .uThreadPerGroupY = 16,
            .uThreadPerGroupZ = 3
        };
        
        plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
        gptGfx->begin_command_recording(ptCommandBuffer, NULL);
        plComputeEncoder* ptComputeEncoder = gptGfx->begin_compute_pass(ptCommandBuffer);
        gptGfx->bind_compute_bind_groups(ptComputeEncoder, tPanoramaShader, 0, 1, &tComputeBindGroup, 0, NULL);
        gptGfx->bind_compute_shader(ptComputeEncoder, tPanoramaShader);
        gptGfx->dispatch(ptComputeEncoder, 1, &tDispach);
        gptGfx->end_compute_pass(ptComputeEncoder);
        gptGfx->end_command_recording(ptCommandBuffer);
        gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
        gptGfx->wait_on_command_buffer(ptCommandBuffer);
        gptGfx->return_command_buffer(ptCommandBuffer);
        gptGfx->queue_compute_shader_for_deletion(ptDevice, tPanoramaShader);

        const plTextureDesc tSkyboxTextureDesc = {
            .tDimensions = {(float)iResolution, (float)iResolution, 1},
            .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
            .uLayers     = 6,
            .uMips       = 1,
            .tType       = PL_TEXTURE_TYPE_CUBE,
            .tUsage      = PL_TEXTURE_USAGE_SAMPLED
        };
        ptScene->tSkyboxTexture = pl__refr_create_texture(&tSkyboxTextureDesc, "skybox texture", uSceneHandle, PL_TEXTURE_USAGE_SAMPLED);

        ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
        gptGfx->begin_command_recording(ptCommandBuffer, NULL);
        plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
        for(uint32_t i = 0; i < 6; i++)
        {
            const plBufferImageCopy tBufferImageCopy = {
                .uImageWidth = iResolution,
                .uImageHeight = iResolution,
                .uImageDepth = 1,
                .uLayerCount    = 1,
                .szBufferOffset = 0,
                .uBaseArrayLayer = i,
            };
            gptGfx->copy_buffer_to_texture(ptBlitEncoder, atComputeBuffers[i + 1], ptScene->tSkyboxTexture, 1, &tBufferImageCopy);
        }
        gptGfx->end_blit_pass(ptBlitEncoder);
        gptGfx->end_command_recording(ptCommandBuffer);
        gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
        gptGfx->wait_on_command_buffer(ptCommandBuffer);
        gptGfx->return_command_buffer(ptCommandBuffer);
        
        for(uint32_t i = 0; i < 7; i++)
            gptGfx->destroy_buffer(ptDevice, atComputeBuffers[i]);

        plBindGroupLayout tSkyboxBindGroupLayout = {
            .atTextureBindings = { {.uSlot = 0, .tStages = PL_STAGE_PIXEL | PL_STAGE_VERTEX, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}}
        };
    
        const plBindGroupDesc tSkyboxBindGroupDesc = {
            .ptPool = gptData->ptBindGroupPool,
            .ptLayout = &tSkyboxBindGroupLayout,
            .pcDebugName = "skybox bind group"
        };
        ptScene->tSkyboxBindGroup = gptGfx->create_bind_group(ptDevice, &tSkyboxBindGroupDesc);
        const plBindGroupUpdateTextureData tTextureData1 = {.tTexture = ptScene->tSkyboxTexture, .uSlot = 0, .uIndex = 0, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED};
        const plBindGroupUpdateData tBGData1 = {
            .uTextureCount = 1,
            .atTextureBindings = &tTextureData1
        };
        gptGfx->update_bind_group(ptDevice, ptScene->tSkyboxBindGroup, &tBGData1);

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
        const float fCubeSide = 1.0f;
        pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){-fCubeSide, -fCubeSide, -fCubeSide}));
        pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){ fCubeSide, -fCubeSide, -fCubeSide}));
        pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){-fCubeSide,  fCubeSide, -fCubeSide}));
        pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){ fCubeSide,  fCubeSide, -fCubeSide}));
        pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){-fCubeSide, -fCubeSide,  fCubeSide}));
        pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){ fCubeSide, -fCubeSide,  fCubeSide}));
        pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){-fCubeSide,  fCubeSide,  fCubeSide}));
        pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){ fCubeSide,  fCubeSide,  fCubeSide})); 
    }
    pl_end_profile_sample(0);

    pl_begin_profile_sample(0, "step 1");

    plComputeShaderDesc tFilterComputeShaderDesc = {
        .tShader = gptShader->load_glsl("../shaders/filter_environment.comp", "main", NULL, NULL),
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
        .atBindGroupLayouts = {
            {
                .atTextureBindings = {
                    {.uSlot = 1, .tStages = PL_STAGE_COMPUTE, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
                },
                .atBufferBindings = {
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 2, .tStages = PL_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 3, .tStages = PL_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 4, .tStages = PL_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 5, .tStages = PL_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 6, .tStages = PL_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 7, .tStages = PL_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 8, .tStages = PL_STAGE_COMPUTE},
                },
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
    plComputeShaderHandle tIrradianceShader = gptGfx->create_compute_shader(ptDevice, &tFilterComputeShaderDesc);

    tFilterComputeShaderDesc.pTempConstantData = &tFilterDatas[0];
    plComputeShaderHandle tLUTShader = gptGfx->create_compute_shader(ptDevice, &tFilterComputeShaderDesc);

    plComputeShaderHandle atSpecularComputeShaders[16] = {0};
    for(int i = 0; i < ptScene->iEnvironmentMips + 1; i++)
    {
        tFilterComputeShaderDesc.pTempConstantData = &tFilterDatas[i + 1];
        atSpecularComputeShaders[i] = gptGfx->create_compute_shader(ptDevice, &tFilterComputeShaderDesc);
    }
    pl_temp_allocator_reset(&gptData->tTempAllocator);
    pl_end_profile_sample(0);

    // create lut
    
    {

        pl_begin_profile_sample(0, "step 2");
        plBufferHandle atLutBuffers[7] = {0};
        const plBufferDesc tInputBufferDesc = {
            .tUsage    = PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_STAGING,
            .szByteSize = uFaceSize,
            .pcDebugName = "lut output buffer"
        };
        atLutBuffers[6] = pl__refr_create_staging_buffer(&tInputBufferDesc, "lut output", 0);

        for(uint32_t i = 0; i < 6; i++)
            atLutBuffers[i] = pl__refr_create_local_buffer(&tInputBufferDesc, "lut output", i, NULL);

        const plBindGroupDesc tFilterBindGroupDesc = {
            .ptPool      = gptData->aptBindGroupPools[gptGfx->get_current_frame_index()],
            .ptLayout    = &tFilterComputeShaderDesc.atBindGroupLayouts[0],
            .pcDebugName = "lut bind group"
        };
        plBindGroupHandle tLutBindGroup = gptGfx->create_bind_group(ptDevice, &tFilterBindGroupDesc);
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
            .atBufferBindings = atBGBufferData,
            .uSamplerCount = 1,
            .atSamplerBindings = &tSamplerData,
            .uTextureCount = 1,
            .atTextureBindings = &tTextureData
        };
        gptGfx->update_bind_group(ptDevice, tLutBindGroup, &tBGData);
        gptGfx->queue_bind_group_for_deletion(ptDevice, tLutBindGroup);

        const plDispatch tDispach = {
            .uGroupCountX     = (uint32_t)iResolution / 16,
            .uGroupCountY     = (uint32_t)iResolution / 16,
            .uGroupCountZ     = 3,
            .uThreadPerGroupX = 16,
            .uThreadPerGroupY = 16,
            .uThreadPerGroupZ = 3
        };
        pl_end_profile_sample(0);

        pl_begin_profile_sample(0, "step 3");
        plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
        gptGfx->begin_command_recording(ptCommandBuffer, NULL);
        plComputeEncoder* ptComputeEncoder = gptGfx->begin_compute_pass(ptCommandBuffer);
        gptGfx->bind_compute_bind_groups(ptComputeEncoder, tLUTShader, 0, 1, &tLutBindGroup, 0, NULL);
        gptGfx->bind_compute_shader(ptComputeEncoder, tLUTShader);
        gptGfx->dispatch(ptComputeEncoder, 1, &tDispach);
        gptGfx->end_compute_pass(ptComputeEncoder);
        gptGfx->end_command_recording(ptCommandBuffer);
        gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
        gptGfx->wait_on_command_buffer(ptCommandBuffer);
        gptGfx->return_command_buffer(ptCommandBuffer);
        gptGfx->queue_compute_shader_for_deletion(ptDevice, tLUTShader);

        plBuffer* ptLutBuffer = gptGfx->get_buffer(ptDevice, atLutBuffers[6]);
        
        const plTextureDesc tTextureDesc = {
            .tDimensions = {(float)iResolution, (float)iResolution, 1},
            .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
            .uLayers     = 1,
            .uMips       = 1,
            .tType       = PL_TEXTURE_TYPE_2D,
            .tUsage      = PL_TEXTURE_USAGE_SAMPLED
        };
        ptScene->tGGXLUTTexture = pl__refr_create_texture_with_data(&tTextureDesc, "lut texture", 0, ptLutBuffer->tMemoryAllocation.pHostMapped, uFaceSize);
        pl_end_profile_sample(0);

        pl_begin_profile_sample(0, "step 4");
        ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
        gptGfx->begin_command_recording(ptCommandBuffer, NULL);
        ptComputeEncoder = gptGfx->begin_compute_pass(ptCommandBuffer);
        gptGfx->bind_compute_bind_groups(ptComputeEncoder, tIrradianceShader, 0, 1, &tLutBindGroup, 0, NULL);
        gptGfx->bind_compute_shader(ptComputeEncoder, tIrradianceShader);
        gptGfx->dispatch(ptComputeEncoder, 1, &tDispach);
        gptGfx->end_compute_pass(ptComputeEncoder);
        gptGfx->end_command_recording(ptCommandBuffer);
        gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
        gptGfx->wait_on_command_buffer(ptCommandBuffer);
        gptGfx->return_command_buffer(ptCommandBuffer);
        gptGfx->queue_compute_shader_for_deletion(ptDevice, tIrradianceShader);

        const plTextureDesc tSpecularTextureDesc = {
            .tDimensions = {(float)iResolution, (float)iResolution, 1},
            .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
            .uLayers     = 6,
            .uMips       = 1,
            .tType       = PL_TEXTURE_TYPE_CUBE,
            .tUsage      = PL_TEXTURE_USAGE_SAMPLED
        };
        ptScene->tLambertianEnvTexture = pl__refr_create_texture(&tSpecularTextureDesc, "specular texture", uSceneHandle, PL_TEXTURE_USAGE_SAMPLED);

        ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
        gptGfx->begin_command_recording(ptCommandBuffer, NULL);
        plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);

        for(uint32_t i = 0; i < 6; i++)
        {
            const plBufferImageCopy tBufferImageCopy = {
                .uImageWidth = iResolution,
                .uImageHeight = iResolution,
                .uImageDepth = 1,
                .uLayerCount    = 1,
                .szBufferOffset = 0,
                .uBaseArrayLayer = i,
            };
            gptGfx->copy_buffer_to_texture(ptBlitEncoder, atLutBuffers[i], ptScene->tLambertianEnvTexture, 1, &tBufferImageCopy);
        }

        gptGfx->end_blit_pass(ptBlitEncoder);
        gptGfx->end_command_recording(ptCommandBuffer);
        gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
        gptGfx->wait_on_command_buffer(ptCommandBuffer);
        gptGfx->return_command_buffer(ptCommandBuffer);

        for(uint32_t i = 0; i < 7; i++)
            gptGfx->destroy_buffer(ptDevice, atLutBuffers[i]);
        pl_end_profile_sample(0);
    }
    

    pl_begin_profile_sample(0, "step 5");
    {
        const plTextureDesc tTextureDesc = {
            .tDimensions = {(float)iResolution, (float)iResolution, 1},
            .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
            .uLayers     = 6,
            .uMips       = ptScene->iEnvironmentMips,
            .tType       = PL_TEXTURE_TYPE_CUBE,
            .tUsage      = PL_TEXTURE_USAGE_SAMPLED
        };
        ptScene->tGGXEnvTexture = pl__refr_create_texture(&tTextureDesc, "tGGXEnvTexture", uSceneHandle, PL_TEXTURE_USAGE_SAMPLED);

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
        const plBufferDesc tOutputBufferDesc = {
            .tUsage    = PL_BUFFER_USAGE_STORAGE,
            .szByteSize = uMaxFaceSize,
            .pcDebugName = "inner buffer"
        };

        plBufferHandle atInnerComputeBuffers[7] = {0};
        for(uint32_t j = 0; j < 7; j++)
            atInnerComputeBuffers[j] = pl__refr_create_local_buffer(&tOutputBufferDesc, "inner buffer", j, NULL);

        const plBindGroupDesc tFilterComputeBindGroupDesc = {
            .ptPool      = gptData->aptBindGroupPools[gptGfx->get_current_frame_index()],
            .ptLayout    = &tFilterComputeShaderDesc.atBindGroupLayouts[0],
            .pcDebugName = "lut bindgroup"
        };
        plBindGroupHandle tLutBindGroup = gptGfx->create_bind_group(ptDevice, &tFilterComputeBindGroupDesc);
        
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
            .atBufferBindings = atBGBufferData,
            .uSamplerCount = 1,
            .atSamplerBindings = &tSamplerData,
            .uTextureCount = 1,
            .atTextureBindings = &tTextureData
        };
        gptGfx->update_bind_group(ptDevice, tLutBindGroup, &tBGData);
        gptGfx->queue_bind_group_for_deletion(ptDevice, tLutBindGroup);

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

            plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
            gptGfx->begin_command_recording(ptCommandBuffer, NULL);
            plComputeEncoder* ptComputeEncoder = gptGfx->begin_compute_pass(ptCommandBuffer);
            gptGfx->bind_compute_bind_groups(ptComputeEncoder, atSpecularComputeShaders[i], 0, 1, &tLutBindGroup, 0, NULL);
            gptGfx->bind_compute_shader(ptComputeEncoder, atSpecularComputeShaders[i]);
            gptGfx->dispatch(ptComputeEncoder, 1, &tDispach);
            gptGfx->end_compute_pass(ptComputeEncoder);
            gptGfx->end_command_recording(ptCommandBuffer);
            gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
            gptGfx->wait_on_command_buffer(ptCommandBuffer);
            gptGfx->return_command_buffer(ptCommandBuffer);
            gptGfx->queue_compute_shader_for_deletion(ptDevice, atSpecularComputeShaders[i]);

            ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
            gptGfx->begin_command_recording(ptCommandBuffer, NULL);
            plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);

            for(uint32_t j = 0; j < 6; j++)
            {
                const plBufferImageCopy tBufferImageCopy = {
                    .uImageWidth = currentWidth,
                    .uImageHeight = currentWidth,
                    .uImageDepth = 1,
                    .uLayerCount     = 1,
                    .szBufferOffset  = 0,
                    .uBaseArrayLayer = j,
                    .uMipLevel       = i
                };
                gptGfx->copy_buffer_to_texture(ptBlitEncoder, atInnerComputeBuffers[j], ptScene->tGGXEnvTexture, 1, &tBufferImageCopy);
            }
            gptGfx->end_blit_pass(ptBlitEncoder);
            gptGfx->end_command_recording(ptCommandBuffer);
            gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
            gptGfx->wait_on_command_buffer(ptCommandBuffer);
            gptGfx->return_command_buffer(ptCommandBuffer);

        }
        for(uint32_t j = 0; j < 7; j++)
            gptGfx->queue_buffer_for_deletion(ptDevice, atInnerComputeBuffers[j]);
    }

    pl_end_profile_sample(0);
    pl_end_profile_sample(0);
}

static plTextureHandle
pl__create_texture_helper(plMaterialComponent* ptMaterial, plTextureSlot tSlot, bool bHdr, int iMips)
{
    plDevice* ptDevice = gptData->ptDevice;

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
    plDevice*   ptDevice   = gptData->ptDevice;

    if(uCount == 0)
        gptData->tPickedEntity = (plEntity){.ulData = UINT64_MAX};

    int iSceneWideRenderingFlags = PL_RENDERING_FLAG_USE_PUNCTUAL;
    if(ptScene->tGGXEnvTexture.uIndex != 0)
        iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_IBL;

    // reset old entities
    const uint32_t uOldSelectedEntityCount = pl_sb_size(ptScene->sbtOutlineDrawables);
    for(uint32_t i = 0; i < uOldSelectedEntityCount; i++)
    {
        plEntity tEntity = ptScene->sbtOutlineDrawables[i].tEntity;
        plShader* ptOutlineShader = gptGfx->get_shader(ptDevice, ptScene->sbtOutlineDrawables[i].tShader);

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
            .ulStencilMode        = PL_COMPARE_MODE_LESS,
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
        for(uint32_t j = 0; j < ptOutlineShader->tDesc._uConstantCount; j++)
        {
            const plSpecializationConstant* ptConstant = &ptOutlineShader->tDesc.atConstants[j];
            szSpecializationSize += pl__get_data_type_size2(ptConstant->tType);
        }

        // const uint64_t ulVariantHash = pl_hm_hash(tOutlineVariant.pTempConstantData, szSpecializationSize, tOutlineVariant.tGraphicsState.ulValue);
        // pl_hm_remove(&gptData->ptVariantHashmap, ulVariantHash);

        if(pl_hm_has_key(ptScene->ptOpaqueHashmap, tEntity.ulData))
        {
            uint64_t ulIndex = pl_hm_lookup(ptScene->ptOpaqueHashmap, tEntity.ulData);
            plDrawable* ptDrawable = &ptScene->sbtOpaqueDrawables[ulIndex];
            ptDrawable->tShader = ptScene->sbtOutlineDrawablesOldShaders[i];
        }
        else if(pl_hm_has_key(ptScene->ptTransparentHashmap, tEntity.ulData))
        {
            uint64_t ulIndex = pl_hm_lookup(ptScene->ptTransparentHashmap, tEntity.ulData);
            plDrawable* ptDrawable = &ptScene->sbtTransparentDrawables[ulIndex];
            ptDrawable->tShader = ptScene->sbtOutlineDrawablesOldShaders[i];
        }

        // gptGfx->queue_shader_for_deletion(ptDevice, ptScene->sbtOutlineDrawables[i].tShader);
    }
    pl_sb_reset(ptScene->sbtOutlineDrawables)
    pl_sb_reset(ptScene->sbtOutlineDrawablesOldShaders)



    for(uint32_t i = 0; i < uCount; i++)
    {
        plEntity tEntity = atEntities[i];

        gptData->tPickedEntity = tEntity;

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
        const plLightComponent* sbtLights = ptScene->tComponentLibrary.tLightComponentManager.pComponents;
        int aiConstantData0[6] = {
            (int)ptMesh->ulVertexStreamMask,
            iDataStride,
            iTextureMappingFlags,
            PL_INFO_MATERIAL_METALLICROUGHNESS,
            iSceneWideRenderingFlags,
            pl_sb_size(sbtLights)
        };

        if(pl_hm_has_key(ptScene->ptOpaqueHashmap, tEntity.ulData))
        {
            uint64_t ulIndex = pl_hm_lookup(ptScene->ptOpaqueHashmap, tEntity.ulData);
            plDrawable* ptDrawable = &ptScene->sbtOpaqueDrawables[ulIndex];
            plShader* ptOldShader = gptGfx->get_shader(ptDevice, ptDrawable->tShader);
            plGraphicsState tVariantTemp = ptOldShader->tDesc.tGraphicsState;

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
                .ulStencilMode        = PL_COMPARE_MODE_LESS,
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

            // plShaderHandle tOutlineShader = pl__get_shader_variant(uSceneHandle, gptData->tOutlineShader, &tOutlineVariant);
            pl_sb_push(ptScene->sbtOutlineDrawables, *ptDrawable);
            // ptScene->sbtOutlineDrawables[pl_sb_size(ptScene->sbtOutlineDrawables) - 1].tShader = tOutlineShader;

            const plShaderVariant tVariant = {
                .pTempConstantData = aiConstantData0,
                .tGraphicsState    = tVariantTemp
            };

            pl_sb_push(ptScene->sbtOutlineDrawablesOldShaders, ptDrawable->tShader);
            ptDrawable->tShader = pl__get_shader_variant(uSceneHandle, gptData->tOpaqueShader, &tVariant);
        }
        else if(pl_hm_has_key(ptScene->ptTransparentHashmap, tEntity.ulData))
        {
            uint64_t ulIndex = pl_hm_lookup(ptScene->ptTransparentHashmap, tEntity.ulData);
            plDrawable* ptDrawable = &ptScene->sbtTransparentDrawables[ulIndex];
            plShader* ptOldShader = gptGfx->get_shader(ptDevice, ptDrawable->tShader);
            plGraphicsState tVariantTemp = ptOldShader->tDesc.tGraphicsState;

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
                .ulStencilMode        = PL_COMPARE_MODE_LESS,
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

            // plShaderHandle tOutlineShader = pl__get_shader_variant(uSceneHandle, gptData->tOutlineShader, &tOutlineVariant);
            pl_sb_push(ptScene->sbtOutlineDrawables, *ptDrawable);
            // ptScene->sbtOutlineDrawables[pl_sb_size(ptScene->sbtOutlineDrawables) - 1].tShader = tOutlineShader;

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
    plDevice*   ptDevice   = gptData->ptDevice;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~textures~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    pl_begin_profile_sample(0, "load textures");
    plHashMap* ptMaterialBindGroupDict = {0};
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
    pl_end_profile_sample(0);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~materials~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    pl_begin_profile_sample(0, "load materials");
    for(uint32_t i = 0; i < uMaterialCount; i++)
    {
        plMaterialComponent* ptMaterial = &sbtMaterials[i];

        plBindGroupLayout tMaterialBindGroupLayout = {
            .atTextureBindings = {
                {.uSlot =  0, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
                {.uSlot =  1, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
                {.uSlot =  2, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
                {.uSlot =  3, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
                {.uSlot =  4, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
            }
        };
        const plBindGroupDesc tMaterialBindGroupDesc = {
            .ptPool = gptData->ptBindGroupPool,
            .ptLayout = &tMaterialBindGroupLayout,
            .pcDebugName = "material bind group"
        };
        sbtMaterialBindGroups[i] = gptGfx->create_bind_group(ptDevice, &tMaterialBindGroupDesc);

        const plBindGroupUpdateTextureData tTextureData[] = 
        {
            {.tTexture = pl__create_texture_helper(ptMaterial, PL_TEXTURE_SLOT_BASE_COLOR_MAP, true, 0),              .uSlot =  0, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
            {.tTexture = pl__create_texture_helper(ptMaterial, PL_TEXTURE_SLOT_NORMAL_MAP, false, 0),                 .uSlot =  1, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
            {.tTexture = pl__create_texture_helper(ptMaterial, PL_TEXTURE_SLOT_EMISSIVE_MAP, true, 0),                .uSlot =  2, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
            {.tTexture = pl__create_texture_helper(ptMaterial, PL_TEXTURE_SLOT_METAL_ROUGHNESS_MAP, false, 0),        .uSlot =  3, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
            {.tTexture = pl__create_texture_helper(ptMaterial, PL_TEXTURE_SLOT_OCCLUSION_MAP, false, 1),              .uSlot =  4, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
        };
        const plBindGroupUpdateData tBGData1 = {
            .uTextureCount = 5,
            .atTextureBindings = tTextureData
        };
        gptGfx->update_bind_group(ptDevice, sbtMaterialBindGroups[i], &tBGData1);
        pl_hm_insert(ptMaterialBindGroupDict, (uint64_t)ptMaterial, (uint64_t)i);
    }
    pl_end_profile_sample(0);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~CPU Buffers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    int iSceneWideRenderingFlags = PL_RENDERING_FLAG_USE_PUNCTUAL;
    if(ptScene->tGGXEnvTexture.uIndex != 0)
        iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_IBL;

    // fill CPU buffers & drawable list
    pl_begin_profile_sample(0, "create shaders");

    plDrawable* sbtDrawables[] = {
        ptScene->sbtOpaqueDrawables,
        ptScene->sbtTransparentDrawables,
    };

    plShaderHandle atTemplateShaders[] = {
        gptData->tOpaqueShader,
        gptData->tTransparentShader
    };

    plShaderHandle atTemplateShadowShaders[] = {
        gptData->tShadowShader,
        gptData->tAlphaShadowShader
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
        ptScene->ptOpaqueHashmap,
        ptScene->ptTransparentHashmap
    };
    
    const plLightComponent* sbtLights = ptScene->tComponentLibrary.tLightComponentManager.pComponents;
    pl_sb_reserve(ptScene->sbtVertexDataBuffer, 40000000);
    pl_sb_reserve(ptScene->sbtVertexPosBuffer, 15000000);
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


            const uint64_t ulMaterialIndex = pl_hm_lookup(ptMaterialBindGroupDict, (uint64_t)ptMaterial);
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

            if(uDrawableBatchIndex > 0)
            {
                const plShaderVariant tShadowVariant = {
                    .pTempConstantData = aiConstantData0,
                    .tGraphicsState    = {
                        .ulDepthWriteEnabled  = 1,
                        .ulDepthMode          = PL_COMPARE_MODE_LESS_OR_EQUAL,
                        .ulCullMode           = PL_CULL_MODE_NONE,
                        .ulWireframe          = 0,
                        .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
                        .ulStencilRef         = 0xff,
                        .ulStencilMask        = 0xff,
                        .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
                        .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                        .ulStencilOpPass      = PL_STENCIL_OP_KEEP
                    }
                };
                (sbtDrawables[uDrawableBatchIndex])[i].tShadowShader = pl__get_shader_variant(uSceneHandle, atTemplateShadowShaders[uDrawableBatchIndex], &tShadowVariant);

                plBindGroupHandle tShadowMaterialBindGroup = {0};
                if(pl_hm_has_key(ptScene->ptShadowBindgroupHashmap, ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_BASE_COLOR_MAP].tResource.ulData))
                {
                    tShadowMaterialBindGroup.uData = (uint32_t)pl_hm_lookup(ptScene->ptShadowBindgroupHashmap, ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_BASE_COLOR_MAP].tResource.ulData);
                }
                else
                {
                    plBindGroupLayout tMaterialBindGroupLayout = {
                        .atTextureBindings = {
                            {.uSlot =  0, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
                        }
                    };
                    const plBindGroupDesc tMaterialBindGroupDesc = {
                        .ptPool = gptData->ptBindGroupPool,
                        .ptLayout = &tMaterialBindGroupLayout,
                        .pcDebugName = "shadow material bind group"
                    };
                    tShadowMaterialBindGroup = gptGfx->create_bind_group(ptDevice, &tMaterialBindGroupDesc);

                    const plBindGroupUpdateTextureData tTextureData[] = 
                    {
                        {.tTexture = pl__create_texture_helper(ptMaterial, PL_TEXTURE_SLOT_BASE_COLOR_MAP, true, 0), .uSlot =  0, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
                    };
                    const plBindGroupUpdateData tBGData1 = {
                        .uTextureCount = 1,
                        .atTextureBindings = tTextureData
                    };
                    gptGfx->update_bind_group(ptDevice, tShadowMaterialBindGroup, &tBGData1);

                    pl_hm_insert(ptScene->ptShadowBindgroupHashmap, ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_BASE_COLOR_MAP].tResource.ulData, tShadowMaterialBindGroup.uData);
                }
                (sbtDrawables[uDrawableBatchIndex])[i].tShadowMaterialBindGroup = tShadowMaterialBindGroup;
            }
        }
        atHashmaps[uDrawableBatchIndex] = ptHashmap;
    }

    ptScene->ptOpaqueHashmap = atHashmaps[0];
    ptScene->ptTransparentHashmap = atHashmaps[1];

    pl_end_profile_sample(0);

    pl_hm_free(ptMaterialBindGroupDict);
    pl_sb_free(sbtMaterialBindGroups);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~GPU Buffers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    pl_begin_profile_sample(0, "fill GPU buffers");

    const plBufferDesc tShaderBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_STORAGE,
        .szByteSize = sizeof(plGPUMaterial) * pl_sb_size(ptScene->sbtMaterialBuffer),
        .pcDebugName = "shader buffer"
    };
    
    const plBufferDesc tIndexBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_INDEX,
        .szByteSize = sizeof(uint32_t) * pl_sb_size(ptScene->sbuIndexBuffer),
        .pcDebugName = "index buffer"
    };
    
    const plBufferDesc tVertexBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_VERTEX | PL_BUFFER_USAGE_STORAGE,
        .szByteSize = sizeof(plVec3) * pl_sb_size(ptScene->sbtVertexPosBuffer),
        .pcDebugName = "vertex buffer"
    };
     
    const plBufferDesc tStorageBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_STORAGE,
        .szByteSize = sizeof(plVec4) * pl_sb_size(ptScene->sbtVertexDataBuffer),
        .pcDebugName = "storage buffer"
    };

    const plBufferDesc tSkinStorageBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_STORAGE,
        .szByteSize = sizeof(plVec4) * pl_sb_size(ptScene->sbtSkinVertexDataBuffer),
        .pcDebugName = "skin buffer"
    };

    const plBufferDesc tLightBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_UNIFORM,
        .szByteSize = sizeof(plGPULight) * PL_MAX_LIGHTS,
        .pcDebugName = "light buffer"
    };

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        ptScene->atLightBuffer[i] = pl__refr_create_staging_buffer(&tLightBufferDesc, "light", i);

    ptScene->tMaterialDataBuffer = pl__refr_create_local_buffer(&tShaderBufferDesc,            "shader", uSceneHandle, ptScene->sbtMaterialBuffer);
    ptScene->tIndexBuffer        = pl__refr_create_local_buffer(&tIndexBufferDesc,              "index", uSceneHandle, ptScene->sbuIndexBuffer);
    ptScene->tVertexBuffer       = pl__refr_create_local_buffer(&tVertexBufferDesc,            "vertex", uSceneHandle, ptScene->sbtVertexPosBuffer);
    ptScene->tStorageBuffer      = pl__refr_create_local_buffer(&tStorageBufferDesc,          "storage", uSceneHandle, ptScene->sbtVertexDataBuffer);

    if(tSkinStorageBufferDesc.szByteSize > 0)
    {
        ptScene->tSkinStorageBuffer  = pl__refr_create_local_buffer(&tSkinStorageBufferDesc, "skin storage", uSceneHandle, ptScene->sbtSkinVertexDataBuffer);

        const plBindGroupLayout tSkinBindGroupLayout0 = {
            .atSamplerBindings = {
                {.uSlot =  3, .tStages = PL_STAGE_COMPUTE}
            },
            .atBufferBindings = {
                { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_STAGE_COMPUTE},
                { .uSlot = 1, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_STAGE_COMPUTE},
                { .uSlot = 2, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_STAGE_COMPUTE},
            }
        };
        const plBindGroupDesc tSkinBindGroupDesc = {
            .ptPool = gptData->ptBindGroupPool,
            .ptLayout = &tSkinBindGroupLayout0,
            .pcDebugName = "skin bind group"
        };
        ptScene->tSkinBindGroup0 = gptGfx->create_bind_group(ptDevice, &tSkinBindGroupDesc);

        const plBindGroupUpdateSamplerData atSamplerData[] = {
            { .uSlot = 3, .tSampler = gptData->tDefaultSampler}
        };
        const plBindGroupUpdateBufferData atBufferData[] = 
        {
            { .uSlot = 0, .tBuffer = ptScene->tSkinStorageBuffer, .szBufferRange = tSkinStorageBufferDesc.szByteSize},
            { .uSlot = 1, .tBuffer = ptScene->tVertexBuffer,      .szBufferRange = tVertexBufferDesc.szByteSize},
            { .uSlot = 2, .tBuffer = ptScene->tStorageBuffer,     .szBufferRange = tStorageBufferDesc.szByteSize}

        };
        plBindGroupUpdateData tBGData0 = {
            .uBufferCount = 3,
            .atBufferBindings = atBufferData,
            .uSamplerCount = 1,
            .atSamplerBindings = atSamplerData,
        };
        gptGfx->update_bind_group(gptData->ptDevice, ptScene->tSkinBindGroup0, &tBGData0);
    }

    // create lighting shader
    {
        int aiLightingConstantData[] = {iSceneWideRenderingFlags, pl_sb_size(sbtLights)};
        plShaderDesc tLightingShaderDesc = {
            .tPixelShader = gptShader->load_glsl("../shaders/lighting.frag", "main", NULL, NULL),
            .tVertexShader = gptShader->load_glsl("../shaders/lighting.vert", "main", NULL, NULL),
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
            .atVertexBufferLayouts = {
                {
                    .uByteStride = sizeof(float) * 4,
                    .atAttributes = {
                        {.uByteOffset = 0, .tFormat = PL_FORMAT_R32G32_FLOAT},
                        {.uByteOffset = sizeof(float) * 2, .tFormat = PL_FORMAT_R32G32_FLOAT}
                    }
                }
            },
            .atBlendStates = {
                pl__get_blend_state(PL_BLEND_MODE_OPAQUE)
            },
            .pTempConstantData = aiLightingConstantData,
            .uSubpassIndex = 1,
            .tRenderPassLayout = gptData->tRenderPassLayout,
            .atBindGroupLayouts = {
                {
                    .atBufferBindings = {
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
                    .atSamplerBindings = {
                        {.uSlot = 3, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                        {.uSlot = 4, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                    },
                    .atTextureBindings = {
                        {.uSlot =   5, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                        {.uSlot =   6, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                        {.uSlot =   7, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1}
                    }
                },
                {
                    .atTextureBindings = {
                        { .uSlot = 0, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
                        { .uSlot = 1, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
                        { .uSlot = 2, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
                        { .uSlot = 3, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
                        { .uSlot = 4, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT}
                    },
                },
                {
                    .atBufferBindings = {
                        { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                        { .uSlot = 1, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                    },
                    .atTextureBindings = {
                        {.uSlot = 2, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 4},
                    },
                    .atSamplerBindings = {
                        {.uSlot = 6, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                    },
                }
            }
        };
        for(uint32_t i = 0; i < 2; i++)
        {
            tLightingShaderDesc.atConstants[i].uID = i;
            tLightingShaderDesc.atConstants[i].uOffset = i * sizeof(int);
            tLightingShaderDesc.atConstants[i].tType = PL_DATA_TYPE_INT;
        }
        ptScene->tLightingShader = gptGfx->create_shader(gptData->ptDevice, &tLightingShaderDesc);
    }

    const plShaderDesc tTonemapShaderDesc = {
        .tPixelShader = gptShader->load_glsl("../shaders/tonemap.frag", "main", NULL, NULL),
        .tVertexShader = gptShader->load_glsl("../shaders/full_quad.vert", "main", NULL, NULL),
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
        .atVertexBufferLayouts = {
            {
                .uByteStride = sizeof(float) * 4,
                .atAttributes = {
                    {.uByteOffset = 0, .tFormat = PL_FORMAT_R32G32_FLOAT},
                    {.uByteOffset = sizeof(float) * 2, .tFormat = PL_FORMAT_R32G32_FLOAT}
                }
            }
        },
        .atBlendStates = {
            pl__get_blend_state(PL_BLEND_MODE_OPAQUE)
        },
        .tRenderPassLayout = gptData->tPostProcessRenderPassLayout,
        .atBindGroupLayouts = {
            {
                .atSamplerBindings = {
                    { .uSlot = 0, .tStages = PL_STAGE_PIXEL}
                },
                .atTextureBindings = {
                    {.uSlot = 1, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
                    {.uSlot = 2, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
                }
            }
        }
    };
    ptScene->tTonemapShader = gptGfx->create_shader(gptData->ptDevice, &tTonemapShaderDesc);

    pl_sb_free(ptScene->sbtVertexPosBuffer);
    pl_sb_free(ptScene->sbtVertexDataBuffer);
    pl_sb_free(ptScene->sbuIndexBuffer);

    pl_end_profile_sample(0);
}

static void
pl_refr_run_ecs(uint32_t uSceneHandle)
{
    pl_begin_profile_sample(0, __FUNCTION__);
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    gptECS->run_script_update_system(&ptScene->tComponentLibrary);
    gptECS->run_animation_update_system(&ptScene->tComponentLibrary, gptIOI->get_io()->fDeltaTime);
    gptECS->run_transform_update_system(&ptScene->tComponentLibrary);
    gptECS->run_hierarchy_update_system(&ptScene->tComponentLibrary);
    gptECS->run_inverse_kinematics_update_system(&ptScene->tComponentLibrary);
    gptECS->run_skin_update_system(&ptScene->tComponentLibrary);
    gptECS->run_object_update_system(&ptScene->tComponentLibrary);
    pl_end_profile_sample(0);
}

static void
pl_refr_update_skin_textures(plCommandBuffer* ptCommandBuffer, uint32_t uSceneHandle)
{
    pl_begin_profile_sample(0, __FUNCTION__);
    plDevice* ptDevice = gptData->ptDevice;
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
    uint32_t uFrameIdx = gptGfx->get_current_frame_index();

    // update skin textures
    if(gptData->uCurrentStagingFrameIndex != uFrameIdx)
    {
        gptData->uStagingOffset = 0;
        gptData->uCurrentStagingFrameIndex = uFrameIdx;
    }
    const uint32_t uSkinCount = pl_sb_size(ptScene->sbtSkinData);
    for(uint32_t i = 0; i < uSkinCount; i++)
    {
        plBindGroupLayout tBindGroupLayout1 = {
            .atTextureBindings = {
                {.uSlot =  0, .tStages = PL_STAGE_ALL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
            }
        };
        const plBindGroupDesc tBindGroup1Desc = {
            .ptPool      = gptData->aptBindGroupPools[gptGfx->get_current_frame_index()],
            .ptLayout    = &tBindGroupLayout1,
            .pcDebugName = "skin temporary bind group"
        };
        ptScene->sbtSkinData[i].tTempBindGroup = gptGfx->create_bind_group(ptDevice, &tBindGroup1Desc);
        const plBindGroupUpdateTextureData tTextureData = {.tTexture = ptScene->sbtSkinData[i].atDynamicTexture[uFrameIdx], .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED};
        plBindGroupUpdateData tBGData0 = {
            .uTextureCount = 1,
            .atTextureBindings = &tTextureData
        };
        gptGfx->update_bind_group(gptData->ptDevice, ptScene->sbtSkinData[i].tTempBindGroup, &tBGData0);
        gptGfx->queue_bind_group_for_deletion(ptDevice, ptScene->sbtSkinData[i].tTempBindGroup);

        plBuffer* ptStagingBuffer = gptGfx->get_buffer(ptDevice, gptData->tStagingBufferHandle[uFrameIdx]);

        plTexture* ptSkinTexture = gptGfx->get_texture(ptDevice, ptScene->sbtSkinData[i].atDynamicTexture[uFrameIdx]);
        plBufferImageCopy tBufferImageCopy = {
            .uImageWidth = (uint32_t)ptSkinTexture->tDesc.tDimensions.x,
            .uImageHeight = (uint32_t)ptSkinTexture->tDesc.tDimensions.y,
            .uImageDepth = 1,
            .uLayerCount = 1,
            .szBufferOffset = gptData->uStagingOffset
        };
        gptData->uStagingOffset += sizeof(float) * 4 * (size_t)ptSkinTexture->tDesc.tDimensions.x * (size_t)ptSkinTexture->tDesc.tDimensions.y;
        
        plSkinComponent* ptSkinComponent = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_SKIN, ptScene->sbtSkinData[i].tEntity);
        memcpy(&ptStagingBuffer->tMemoryAllocation.pHostMapped[tBufferImageCopy.szBufferOffset], ptSkinComponent->sbtTextureData, sizeof(float) * 4 * (size_t)ptSkinTexture->tDesc.tDimensions.x * (size_t)ptSkinTexture->tDesc.tDimensions.y);
        // memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, ptSkinComponent->sbtTextureData, sizeof(float) * 4 * (size_t)ptSkinTexture->tDesc.tDimensions.x * (size_t)ptSkinTexture->tDesc.tDimensions.y);
        gptGfx->copy_buffer_to_texture(ptBlitEncoder, gptData->tStagingBufferHandle[uFrameIdx], ptScene->sbtSkinData[i].atDynamicTexture[uFrameIdx], 1, &tBufferImageCopy);
    }

    gptGfx->end_blit_pass(ptBlitEncoder);

    pl_end_profile_sample(0);
}

static void
pl_refr_perform_skinning(plCommandBuffer* ptCommandBuffer, uint32_t uSceneHandle)
{
    pl_begin_profile_sample(0, __FUNCTION__);
    plDevice* ptDevice = gptData->ptDevice;
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

    plComputeEncoder* ptComputeEncoder = gptGfx->begin_compute_pass(ptCommandBuffer);

    for(uint32_t i = 0; i < uSkinCount; i++)
    {
        plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);
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
        gptGfx->bind_compute_bind_groups(ptComputeEncoder, ptScene->sbtSkinData[i].tShader, 0, 2, atBindGroups, 1, &tDynamicBinding);
        gptGfx->bind_compute_shader(ptComputeEncoder, ptScene->sbtSkinData[i].tShader);
        gptGfx->dispatch(ptComputeEncoder, 1, &tDispach);
    }
    gptGfx->end_compute_pass(ptComputeEncoder);
    pl_end_profile_sample(0);
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
pl_refr_generate_cascaded_shadow_map(plCommandBuffer* ptCommandBuffer, uint32_t uSceneHandle, uint32_t uViewHandle, plEntity tCamera, plEntity tLight, float fCascadeSplitLambda)
{
    pl_begin_profile_sample(0, __FUNCTION__);

    // for convience
    plDevice*     ptDevice   = gptData->ptDevice;
    plDrawStream* ptStream   = &gptData->tDrawStream;
    plRefScene*   ptScene    = &gptData->sbtScenes[uSceneHandle];
    plRefView*    ptView     = &ptScene->atViews[uViewHandle];
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();

    plCameraComponent* ptSceneCamera = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_CAMERA, tCamera);
    plLightComponent* ptLight = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_LIGHT, tLight);

    pl_sb_reset(ptView->sbtLightShadowData);
    pl_sb_add(ptView->sbtLightShadowData);
    

    if(!(ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW))
    {
        pl_end_profile_sample(0);
        return;
    }

    plGPULightShadowData* ptShadowData = &ptView->sbtLightShadowData[pl_sb_size(ptView->sbtLightShadowData) - 1];
    const float fNearClip = ptSceneCamera->fNearZ;
    const float fFarClip = ptSceneCamera->fFarZ;
    const float fClipRange = fFarClip - fNearClip;

    const float fMinZ = fNearClip;
    const float fMaxZ = fNearClip + fClipRange;

    const float fRange = fMaxZ - fMinZ;
    const float fRatio = fMaxZ / fMinZ;

    BindGroup_0 atBindGroupBuffer[PL_MAX_SHADOW_CASCADES] = {0};
    float fLastSplitDist = 0.0;
    float afLambdaCascadeSplits[PL_MAX_SHADOW_CASCADES] = {0};
    for(uint32_t uCascade = 0; uCascade < ptLight->uCascadeCount; uCascade++)
    {
        float fSplitDist = 0.0f;
        if(fCascadeSplitLambda > 0.0f)
        {
            const float p = (uCascade + 1) / (float)ptLight->uCascadeCount;
            const float fLog = fMinZ * powf(fRatio, p);
            const float fUniform = fMinZ + fRange * p;
            const float fD = fCascadeSplitLambda * (fLog - fUniform) + fUniform;
            afLambdaCascadeSplits[uCascade] = (fD - fNearClip) / fClipRange;
            fSplitDist = afLambdaCascadeSplits[uCascade];
            ptShadowData->cascadeSplits.d[uCascade] = (fNearClip + fSplitDist * fClipRange);
        }
        else
        {
            fSplitDist = ptLight->afCascadeSplits[uCascade] / fClipRange;
            ptShadowData->cascadeSplits.d[uCascade] = ptLight->afCascadeSplits[uCascade];
        }

        plVec3 atCameraCorners[] = {
            { -1.0f,  1.0f, 0.0f },
            { -1.0f, -1.0f, 0.0f },
            {  1.0f, -1.0f, 0.0f },
            {  1.0f,  1.0f, 0.0f },
            { -1.0f,  1.0f, 1.0f },
            { -1.0f, -1.0f, 1.0f },
            {  1.0f, -1.0f, 1.0f },
            {  1.0f,  1.0f, 1.0f },
        };
        plMat4 tCameraInversion = pl_mul_mat4(&ptSceneCamera->tProjMat, &ptSceneCamera->tViewMat);
        tCameraInversion = pl_mat4_invert(&tCameraInversion);
        for(uint32_t i = 0; i < 8; i++)
        {
            plVec4 tInvCorner = pl_mul_mat4_vec4(&tCameraInversion, (plVec4){.xyz = atCameraCorners[i], .w = 1.0f});
            atCameraCorners[i] = pl_div_vec3_scalarf(tInvCorner.xyz, tInvCorner.w);
        }

        for(uint32_t i = 0; i < 4; i++)
        {
            const plVec3 tDist = pl_sub_vec3(atCameraCorners[i + 4], atCameraCorners[i]);
            atCameraCorners[i + 4] = pl_add_vec3(atCameraCorners[i], pl_mul_vec3_scalarf(tDist, fSplitDist));
            atCameraCorners[i] = pl_add_vec3(atCameraCorners[i], pl_mul_vec3_scalarf(tDist, fLastSplitDist));
        }

        // get frustum center
        plVec3 tFrustumCenter = {0};
        for(uint32_t i = 0; i < 8; i++)
            tFrustumCenter = pl_add_vec3(tFrustumCenter, atCameraCorners[i]);
        tFrustumCenter = pl_div_vec3_scalarf(tFrustumCenter, 8.0f);

        float fRadius = 0.0f;
        for (uint32_t i = 0; i < 8; i++)
        {
            float fDistance = pl_length_vec3(pl_sub_vec3(atCameraCorners[i], tFrustumCenter));
            fRadius = pl_max(fRadius, fDistance);
        }
        fRadius = ceilf(fRadius * 16.0f) / 16.0f;

        plVec3 tDirection = ptLight->tDirection;

        tDirection = pl_norm_vec3(tDirection);
        plVec3 tEye = pl_sub_vec3(tFrustumCenter, pl_mul_vec3_scalarf(tDirection, fRadius + 50.0f));

        plCameraComponent tShadowCamera = {
            .tType = PL_CAMERA_TYPE_ORTHOGRAPHIC
        };
        gptCamera->look_at(&tShadowCamera, tEye, tFrustumCenter);
        tShadowCamera.fWidth = fRadius * 2.0f;
        tShadowCamera.fHeight = fRadius * 2.0f;
        tShadowCamera.fNearZ = 0.0f;
        tShadowCamera.fFarZ = fRadius * 2.0f + 50.0f;
        gptCamera->update(&tShadowCamera);
        tShadowCamera.fAspectRatio = 1.0f;
        tShadowCamera.fFieldOfView = atan2f(fRadius, (fRadius + 50.0f));
        tShadowCamera.fNearZ = 0.01f;
        fLastSplitDist = fSplitDist;

        atBindGroupBuffer[uCascade].tCameraPos.xyz = tShadowCamera.tPos;
        atBindGroupBuffer[uCascade].tCameraProjection = tShadowCamera.tProjMat;
        atBindGroupBuffer[uCascade].tCameraView = tShadowCamera.tViewMat;
        atBindGroupBuffer[uCascade].tCameraViewProjection = pl_mul_mat4(&tShadowCamera.tProjMat, &tShadowCamera.tViewMat);
        ptShadowData->cascadeViewProjMat[uCascade] = atBindGroupBuffer[uCascade].tCameraViewProjection;
    }

    char* pcBufferStart = gptGfx->get_buffer(ptDevice, ptView->tShadowData.atCameraBuffers[uFrameIdx])->tMemoryAllocation.pHostMapped;
    memcpy(pcBufferStart, atBindGroupBuffer, sizeof(BindGroup_0) * ptLight->uCascadeCount);

    plBindGroupLayout tBindGroupLayout0 = {
        .atBufferBindings = {
            {
                .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
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
        .atSamplerBindings = {
            {.uSlot = 3, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
        },
    };
    const plBindGroupDesc tGlobalBGDesc = {
        .ptPool      = gptData->aptBindGroupPools[gptGfx->get_current_frame_index()],
        .ptLayout    = &tBindGroupLayout0,
        .pcDebugName = "temporary global bind group 0"
    };
    plBindGroupHandle tGlobalBG = gptGfx->create_bind_group(ptDevice, &tGlobalBGDesc);

    plBuffer* ptStorageBuffer = gptGfx->get_buffer(ptDevice, ptScene->tStorageBuffer);
    const plBindGroupUpdateBufferData atBufferData[] = 
    {
        {
            .tBuffer       = ptView->tShadowData.atCameraBuffers[uFrameIdx],
            .uSlot         = 0,
            .szBufferRange = sizeof(BindGroup_0) * ptLight->uCascadeCount
        },
        {
            .tBuffer       = ptScene->tStorageBuffer,
            .uSlot         = 1,
            .szBufferRange = ptStorageBuffer->tDesc.szByteSize
        },
        {
            .tBuffer       = ptScene->tMaterialDataBuffer,
            .uSlot         = 2,
            .szBufferRange = sizeof(plGPUMaterial) * pl_sb_size(ptScene->sbtMaterialBuffer)
        },
    };

    plBindGroupUpdateSamplerData tSamplerData[] = {
        {
            .tSampler = gptData->tDefaultSampler,
            .uSlot    = 3
        }
    };

    plBindGroupUpdateData tBGData0 = {
        .uBufferCount = 3,
        .atBufferBindings = atBufferData,
        .uSamplerCount = 1,
        .atSamplerBindings = tSamplerData
    };
    gptGfx->update_bind_group(gptData->ptDevice, tGlobalBG, &tBGData0);
    gptGfx->queue_bind_group_for_deletion(ptDevice, tGlobalBG);

    plBindGroupLayout tOpaqueBG1Layout = {
        .atTextureBindings = {
            {.uSlot = 0, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
        }
    };
    const plBindGroupDesc tOpaqueBG1Desc = {
        .ptPool      = gptData->aptBindGroupPools[gptGfx->get_current_frame_index()],
        .ptLayout    = &tOpaqueBG1Layout,
        .pcDebugName = "temporary opaque global bind group"
    };
    plBindGroupHandle tOpaqueBG1 = gptGfx->create_bind_group(ptDevice, &tOpaqueBG1Desc);
    
    const plBindGroupUpdateTextureData tTextureData[] = {
        {
            .tTexture = gptData->tDummyTexture,
            .uSlot    = 0,
            .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED,
            .uIndex = 0
        }
    };

    plBindGroupUpdateData tBGData1 = {
        .uTextureCount = 1,
        .atTextureBindings = tTextureData
    };
    gptGfx->update_bind_group(gptData->ptDevice, tOpaqueBG1, &tBGData1);
    gptGfx->queue_bind_group_for_deletion(ptDevice, tOpaqueBG1);

    const uint32_t uOpaqueDrawableCount = pl_sb_size(ptScene->sbtOpaqueDrawables);
    const uint32_t uTransparentDrawableCount = pl_sb_size(ptScene->sbtTransparentDrawables);

    const plVec2 tDimensions = gptGfx->get_render_pass(ptDevice, ptView->tShadowData.atOpaqueRenderPasses[0])->tDesc.tDimensions;

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

    typedef struct _plShadowDynamicData
    {
        int    iIndex;
        int    iDataOffset;
        int    iVertexOffset;
        int    iMaterialIndex;
        plMat4 tModel;
    } plShadowDynamicData;

    for(uint32_t uCascade = 0; uCascade < ptLight->uCascadeCount; uCascade++)
    {

        const uint32_t uVisibleOpaqueDrawCount = pl_sb_size(ptScene->sbtOpaqueDrawables);
        const uint32_t uVisibleTransparentDrawCount = pl_sb_size(ptScene->sbtTransparentDrawables);

        gptGfx->reset_draw_stream(ptStream, uVisibleOpaqueDrawCount + uVisibleTransparentDrawCount);

        plRenderEncoder* ptEncoder = gptGfx->begin_render_pass(ptCommandBuffer, ptView->tShadowData.atOpaqueRenderPasses[uCascade]);

        for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
        {
            const plDrawable tDrawable = ptScene->sbtOpaqueDrawables[i];
            plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
            plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
            
            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

            plShadowDynamicData* ptDynamicData = (plShadowDynamicData*)tDynamicBinding.pcData;
            ptDynamicData->iDataOffset = tDrawable.uDataOffset;
            ptDynamicData->iVertexOffset = tDrawable.uVertexOffset;
            ptDynamicData->tModel = ptTransform->tWorld;
            ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
            ptDynamicData->iIndex = (int)uCascade;

            pl_add_to_draw_stream(ptStream, (plDrawStreamData)
            {
                .tShader         = gptData->tShadowShader,
                .auDynamicBuffers = {
                    tDynamicBinding.uBufferHandle
                },
                .atVertexBuffers = {
                    ptScene->tVertexBuffer,
                },
                .tIndexBuffer         = tDrawable.uIndexCount == 0 ? (plBufferHandle){0} : ptScene->tIndexBuffer,
                .uIndexOffset         = tDrawable.uIndexOffset,
                .uTriangleCount       = tDrawable.uIndexCount == 0 ? tDrawable.uVertexCount / 3 : tDrawable.uIndexCount / 3,
                .atBindGroups = {
                    tGlobalBG,
                    tOpaqueBG1
                },
                .auDynamicBufferOffsets = {
                    tDynamicBinding.uByteOffset
                },
                .uInstanceOffset = 0,
                .uInstanceCount = 1
            });
        }

        for(uint32_t i = 0; i < uVisibleTransparentDrawCount; i++)
        {
            const plDrawable tDrawable = ptScene->sbtTransparentDrawables[i];
            plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
            plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
            
            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

            plShadowDynamicData* ptDynamicData = (plShadowDynamicData*)tDynamicBinding.pcData;
            ptDynamicData->iDataOffset = tDrawable.uDataOffset;
            ptDynamicData->iVertexOffset = tDrawable.uVertexOffset;
            ptDynamicData->tModel = ptTransform->tWorld;
            ptDynamicData->iMaterialIndex = tDrawable.uMaterialIndex;
            ptDynamicData->iIndex = (int)uCascade;

            pl_add_to_draw_stream(ptStream, (plDrawStreamData)
            {
                .tShader        = tDrawable.tShadowShader,
                .auDynamicBuffers = {
                    tDynamicBinding.uBufferHandle
                },
                .atVertexBuffers = {
                    ptScene->tVertexBuffer,
                },
                .tIndexBuffer         = tDrawable.uIndexCount == 0 ? (plBufferHandle){0} : ptScene->tIndexBuffer,
                .uIndexOffset         = tDrawable.uIndexOffset,
                .uTriangleCount       = tDrawable.uIndexCount == 0 ? tDrawable.uVertexCount / 3 : tDrawable.uIndexCount / 3,
                .atBindGroups = {
                    tGlobalBG,
                    tDrawable.tShadowMaterialBindGroup
                },
                .auDynamicBufferOffsets = {
                    tDynamicBinding.uByteOffset
                },
                .uInstanceOffset = 0,
                .uInstanceCount = 1
            });
        }

        gptGfx->draw_stream(ptEncoder, 1, &tArea);
        gptGfx->end_render_pass(ptEncoder);
    }

    pl_end_profile_sample(0);
}

static plEntity
pl_refr_get_picked_entity(void)
{
    return gptData->tPickedEntity;
}

static void
pl_refr_post_process_scene(plCommandBuffer* ptCommandBuffer, uint32_t uSceneHandle, uint32_t uViewHandle, const plMat4* ptMVP)
{
    pl_begin_profile_sample(0, __FUNCTION__);

    // for convience
    plDevice*     ptDevice   = gptData->ptDevice;
    plDrawStream* ptStream   = &gptData->tDrawStream;
    plRefScene*   ptScene    = &gptData->sbtScenes[uSceneHandle];
    plRefView*    ptView     = &ptScene->atViews[uViewHandle];
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();

    const plVec2 tDimensions = gptGfx->get_render_pass(ptDevice, ptView->tPostProcessRenderPass)->tDesc.tDimensions;

    plRenderEncoder* ptEncoder = gptGfx->begin_render_pass(ptCommandBuffer, ptView->tPostProcessRenderPass);

    plDrawIndex tDraw = {
        .tIndexBuffer   = gptData->tFullQuadIndexBuffer,
        .uIndexCount    = 6,
        .uInstanceCount = 1,
    };

    const plBindGroupLayout tOutlineBindGroupLayout = {
        .atSamplerBindings = {
            { .uSlot = 0, .tStages = PL_STAGE_PIXEL}
        },
        .atTextureBindings = {
            {.uSlot = 1, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
            {.uSlot = 2, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
        }
    };

    // create bind group
    const plBindGroupDesc tOutlineBGDesc = {
        .ptPool      = gptData->aptBindGroupPools[gptGfx->get_current_frame_index()],
        .ptLayout    = &tOutlineBindGroupLayout,
        .pcDebugName = "temp bind group 0"
    };
    plBindGroupHandle tJFABindGroup0 = gptGfx->create_bind_group(gptData->ptDevice, &tOutlineBGDesc);

    const plBindGroupUpdateSamplerData tOutlineSamplerData = {
        .tSampler = gptData->tDefaultSampler,
        .uSlot = 0
    };

    // update bind group (actually point descriptors to GPU resources)
    const plBindGroupUpdateTextureData tOutlineTextureData[] = 
    {
        {
            .tTexture = ptView->tRawOutputTexture[uFrameIdx],
            .uSlot    = 1,
            .tType    = PL_TEXTURE_BINDING_TYPE_SAMPLED
        },
        {
            .tTexture = ptView->tLastUVMask,
            .uSlot    = 2,
            .tType    = PL_TEXTURE_BINDING_TYPE_SAMPLED,
            .tCurrentUsage = PL_TEXTURE_USAGE_STORAGE
        },
    };

    const plBindGroupUpdateData tJFABGData = {
        .uSamplerCount = 1,
        .atSamplerBindings = &tOutlineSamplerData,
        .uTextureCount = 2,
        .atTextureBindings = tOutlineTextureData
    };
    gptGfx->update_bind_group(gptData->ptDevice, tJFABindGroup0, &tJFABGData);
    gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, tJFABindGroup0);

    typedef struct _plPostProcessOptions
    {
        float fTargetWidth;
        int   _padding[3];
        plVec4 tOutlineColor;
    } plPostProcessOptions;


    plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

    plPostProcessOptions* ptDynamicData = (plPostProcessOptions*)tDynamicBinding.pcData;
    const plVec4 tOutlineColor = (plVec4){(float)sin(gptIOI->get_io()->dTime * 3.0) * 0.25f + 0.75f, 0.0f, 0.0f, 1.0f};
    ptDynamicData->fTargetWidth = (float)gptData->uOutlineWidth * tOutlineColor.r + 1.0f;
    ptDynamicData->tOutlineColor = tOutlineColor;

    gptGfx->bind_shader(ptEncoder, ptScene->tTonemapShader);
    gptGfx->bind_vertex_buffer(ptEncoder, gptData->tFullQuadVertexBuffer);
    gptGfx->bind_graphics_bind_groups(ptEncoder, ptScene->tTonemapShader, 0, 1, &tJFABindGroup0, 1, &tDynamicBinding);
    gptGfx->draw_indexed(ptEncoder, 1, &tDraw);

    gptDrawBackend->submit_3d_drawlist(ptView->pt3DGizmoDrawList, ptEncoder, tDimensions.x, tDimensions.y, ptMVP, PL_DRAW_FLAG_DEPTH_TEST | PL_DRAW_FLAG_DEPTH_WRITE, 1);

    gptGfx->end_render_pass(ptEncoder);
    pl_end_profile_sample(0);
}

static void
pl_refr_render_scene(uint32_t uSceneHandle, uint32_t uViewHandle, plViewOptions tOptions)
{
    pl_begin_profile_sample(0, __FUNCTION__);

    // for convience
    plCommandPool*     ptCmdPool = gptData->atCmdPools[gptGfx->get_current_frame_index()];
    plDevice*          ptDevice     = gptData->ptDevice;
    plDrawStream*      ptStream     = &gptData->tDrawStream;
    plRefScene*        ptScene      = &gptData->sbtScenes[uSceneHandle];
    plRefView*         ptView       = &ptScene->atViews[uViewHandle];
    plCameraComponent* ptCamera     = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_CAMERA, *tOptions.ptViewCamera);
    plCameraComponent* ptCullCamera = tOptions.ptCullCamera ? gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_CAMERA, *tOptions.ptCullCamera) : ptCamera;
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();

    const plMat4 tMVP = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);

    if(!gptData->bFrustumCulling)
        ptCullCamera = NULL;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~culling~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    const uint32_t uOpaqueDrawableCount = pl_sb_size(ptScene->sbtOpaqueDrawables);
    const uint32_t uTransparentDrawableCount = pl_sb_size(ptScene->sbtTransparentDrawables);

    plAtomicCounter* ptOpaqueCounter = NULL;
    plAtomicCounter* ptTransparentCounter = NULL;
    
    if(ptCullCamera)
    {
        // opaque objects
        plCullData tOpaqueCullData = {
            .ptScene      = ptScene,
            .ptCullCamera = ptCullCamera,
            .atDrawables  = ptScene->sbtOpaqueDrawables
        };
        
        plJobDesc tOpaqueJobDesc = {
            .task  = pl__refr_cull_job,
            .pData = &tOpaqueCullData
        };

        gptJob->dispatch_batch(uOpaqueDrawableCount, 0, tOpaqueJobDesc, &ptOpaqueCounter);

        // transparent objects
        plCullData tTransparentCullData = {
            .ptScene      = ptScene,
            .ptCullCamera = ptCullCamera,
            .atDrawables  = ptScene->sbtTransparentDrawables
        };
        
        plJobDesc tTransparentJobDesc = {
            .task = pl__refr_cull_job,
            .pData = &tTransparentCullData
        };
        gptJob->dispatch_batch(uTransparentDrawableCount, 0, tTransparentJobDesc, &ptTransparentCounter);
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

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~update bind groups~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const BindGroup_0 tBindGroupBuffer = {
        .tCameraPos            = ptCamera->tPos,
        .tCameraProjection     = ptCamera->tProjMat,
        .tCameraView           = ptCamera->tViewMat,
        .tCameraViewProjection = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat)
    };
    memcpy(gptGfx->get_buffer(ptDevice, ptView->atGlobalBuffers[uFrameIdx])->tMemoryAllocation.pHostMapped, &tBindGroupBuffer, sizeof(BindGroup_0));

    plBindGroupLayout tBindGroupLayout0 = {
        .atBufferBindings = {
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
        .atSamplerBindings = {
            {.uSlot = 3, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
            {.uSlot = 4, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
        },
        .atTextureBindings = {
            {.uSlot =   5, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
            {.uSlot =   6, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
            {.uSlot =   7, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1}
        }

    };
    uint32_t uFrameIndex = gptGfx->get_current_frame_index();
    const plBindGroupDesc tGlobalBGDesc = {
        .ptPool      = gptData->aptBindGroupPools[uFrameIndex],
        .ptLayout    = &tBindGroupLayout0,
        .pcDebugName = "temporary global bind group 1"
    };
    plBindGroupHandle tGlobalBG = gptGfx->create_bind_group(ptDevice, &tGlobalBGDesc);

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

    plBuffer* ptStorageBuffer = gptGfx->get_buffer(ptDevice, ptScene->tStorageBuffer);
    const plBindGroupUpdateBufferData atBufferData[] = 
    {
        {
            .tBuffer       = ptView->atGlobalBuffers[uFrameIdx],
            .uSlot         = 0,
            .szBufferRange = sizeof(BindGroup_0)
        },
        {
            .tBuffer       = ptScene->tStorageBuffer,
            .uSlot         = 1,
            .szBufferRange = ptStorageBuffer->tDesc.szByteSize
        },
        {
            .tBuffer       = ptScene->tMaterialDataBuffer,
            .uSlot         = 2,
            .szBufferRange = sizeof(plGPUMaterial) * pl_sb_size(ptScene->sbtMaterialBuffer)
        },
    };
    const plBindGroupUpdateTextureData tTextureData[] = {
        {
            .tTexture = ptScene->tLambertianEnvTexture.uIndex != 0 ? ptScene->tLambertianEnvTexture : gptData->tDummyTextureCube,
            .uSlot    = 5,
            .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED
        },
        {
            .tTexture = ptScene->tGGXEnvTexture.uIndex != 0 ? ptScene->tGGXEnvTexture : gptData->tDummyTextureCube,
            .uSlot    = 6,
            .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED
        },
        {
            .tTexture = ptScene->tGGXLUTTexture.uIndex != 0 ? ptScene->tGGXLUTTexture : gptData->tDummyTexture,
            .uSlot    = 7,
            .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED
        },
    };
    plBindGroupUpdateData tBGData0 = {
        .uBufferCount = 3,
        .atBufferBindings = atBufferData,
        .uSamplerCount = 2,
        .atSamplerBindings = tSamplerData,
        .uTextureCount = 3,
        .atTextureBindings = tTextureData
    };
    gptGfx->update_bind_group(gptData->ptDevice, tGlobalBG, &tBGData0);
    gptGfx->queue_bind_group_for_deletion(ptDevice, tGlobalBG);
    
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~update skin textures~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    uint64_t ulValue = gptData->aulNextTimelineValue[uFrameIdx];
    plTimelineSemaphore* tSemHandle = gptData->aptSemaphores[uFrameIdx];

    {
        const plBeginCommandInfo tBeginInfo = {
            .uWaitSemaphoreCount   = 1,
            .atWaitSempahores      = {tSemHandle},
            .auWaitSemaphoreValues = {ulValue},
        };

        plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
        gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo);

        pl_refr_update_skin_textures(ptCommandBuffer, uSceneHandle);
        gptGfx->end_command_recording(ptCommandBuffer);

        const plSubmitInfo tSubmitInfo = {
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {tSemHandle},
            .auSignalSemaphoreValues = {++ulValue}
        };
        gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo);
        gptGfx->return_command_buffer(ptCommandBuffer);
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~perform skinning~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    {
        const plBeginCommandInfo tBeginInfo = {
            .uWaitSemaphoreCount   = 1,
            .atWaitSempahores      = {tSemHandle},
            .auWaitSemaphoreValues = {ulValue},
        };

        plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
        gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo);

        pl_refr_perform_skinning(ptCommandBuffer, uSceneHandle);
        gptGfx->end_command_recording(ptCommandBuffer);

        const plSubmitInfo tSubmitInfo = {
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {tSemHandle},
            .auSignalSemaphoreValues = {++ulValue}
        };
        gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo);
        gptGfx->return_command_buffer(ptCommandBuffer);
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~generate shadow maps~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    {
        const plBeginCommandInfo tBeginInfo = {
            .uWaitSemaphoreCount   = 1,
            .atWaitSempahores      = {tSemHandle},
            .auWaitSemaphoreValues = {ulValue},
        };

        plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
        gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo);

        pl_refr_generate_cascaded_shadow_map(ptCommandBuffer, uSceneHandle, uViewHandle, *tOptions.ptViewCamera, *tOptions.ptSunLight, gptData->fLambdaSplit);
        gptGfx->end_command_recording(ptCommandBuffer);

        const plSubmitInfo tSubmitInfo = {
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {tSemHandle},
            .auSignalSemaphoreValues = {++ulValue}
        };
        gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo);
        gptGfx->return_command_buffer(ptCommandBuffer);
    }

    const plVec2 tDimensions = gptGfx->get_render_pass(ptDevice, ptView->tRenderPass)->tDesc.tDimensions;

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
    
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~render scene~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    {
        const plBeginCommandInfo tBeginInfo = {
            .uWaitSemaphoreCount   = 1,
            .atWaitSempahores      = {tSemHandle},
            .auWaitSemaphoreValues = {ulValue},
        };

        plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
        gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~subpass 0 - g buffer fill~~~~~~~~~~~~~~~~~~~~~~~~~~

        plRenderEncoder* ptEncoder = gptGfx->begin_render_pass(ptCommandBuffer, ptView->tRenderPass);

        gptJob->wait_for_counter(ptOpaqueCounter);
        if(ptCullCamera)
        {
            pl_sb_reset(ptView->sbtVisibleOpaqueDrawables);
            for(uint32_t uDrawableIndex = 0; uDrawableIndex < uOpaqueDrawableCount; uDrawableIndex++)
            {
                const plDrawable tDrawable = ptScene->sbtOpaqueDrawables[uDrawableIndex];
                if(!tDrawable.bCulled)
                    pl_sb_push(ptView->sbtVisibleOpaqueDrawables, tDrawable);
            }
        }

        const uint32_t uVisibleOpaqueDrawCount = pl_sb_size(ptView->sbtVisibleOpaqueDrawables);
        gptGfx->reset_draw_stream(ptStream, uVisibleOpaqueDrawCount);
        for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
        {
            const plDrawable tDrawable = ptView->sbtVisibleOpaqueDrawables[i];
            plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
            plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
            
            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

            DynamicData* ptDynamicData = (DynamicData*)tDynamicBinding.pcData;
            ptDynamicData->iDataOffset = tDrawable.uDataOffset;
            ptDynamicData->iVertexOffset = tDrawable.uVertexOffset;
            ptDynamicData->tModel = ptTransform->tWorld;
            ptDynamicData->iMaterialOffset = tDrawable.uMaterialIndex;

            pl_add_to_draw_stream(ptStream, (plDrawStreamData)
            {
                .tShader        = tDrawable.tShader,
                .auDynamicBuffers = {
                    tDynamicBinding.uBufferHandle
                },
                .atVertexBuffers = {
                    ptScene->tVertexBuffer,
                },
                .tIndexBuffer         = tDrawable.uIndexCount == 0 ? (plBufferHandle){0} : ptScene->tIndexBuffer,
                .uIndexOffset         = tDrawable.uIndexOffset,
                .uTriangleCount       = tDrawable.uIndexCount == 0 ? tDrawable.uVertexCount / 3 : tDrawable.uIndexCount / 3,
                .atBindGroups = {
                    tGlobalBG,
                    tDrawable.tMaterialBindGroup
                },
                .auDynamicBufferOffsets = {
                    tDynamicBinding.uByteOffset
                },
                .uInstanceOffset = 0,
                .uInstanceCount = 1
            });
        }

        gptGfx->draw_stream(ptEncoder, 1, &tArea);
        
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~subpass 1 - lighting~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        gptGfx->next_subpass(ptEncoder);

        plBuffer* ptShadowDataBuffer = gptGfx->get_buffer(ptDevice, ptView->atLightShadowDataBuffer[uFrameIdx]);
        memcpy(ptShadowDataBuffer->tMemoryAllocation.pHostMapped, ptView->sbtLightShadowData, sizeof(plGPULightShadowData) * pl_sb_size(ptView->sbtLightShadowData));
        
        const plLightComponent* sbtLights = ptScene->tComponentLibrary.tLightComponentManager.pComponents;
        pl_sb_reset(ptScene->sbtLightData);
        pl_sb_resize(ptScene->sbtLightData, pl_sb_size(sbtLights));
        int iShadowIndex = 0;
        for(uint32_t i = 0; i < pl_sb_size(sbtLights); i++)
        {
            const plLightComponent* ptLight = &sbtLights[i];

            const plGPULight tLight = {
                .fIntensity = ptLight->fIntensity,
                .fRange     = ptLight->fRange,
                .iType      = ptLight->tType,
                .tPosition  = ptLight->tPosition,
                .tDirection = ptLight->tDirection,
                .tColor     = ptLight->tColor,
                .iShadowIndex = ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW ? iShadowIndex++ : 0,
                .iCascadeCount = (int)ptLight->uCascadeCount
            };
            ptScene->sbtLightData[i] = tLight;
        }

        const plBindGroupLayout tLightBindGroupLayout2 = {
            .atBufferBindings = {
                { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .tStages = PL_STAGE_PIXEL | PL_STAGE_VERTEX},
                { .uSlot = 1, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_STAGE_PIXEL | PL_STAGE_VERTEX}
            },
            .atTextureBindings = {
                {.uSlot = 2, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 4},
            },
            .atSamplerBindings = {
                {.uSlot = 6, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
            },
        };
        const plBindGroupDesc tLightBGDesc = {
            .ptPool      = gptData->aptBindGroupPools[gptGfx->get_current_frame_index()],
            .ptLayout    = &tLightBindGroupLayout2,
            .pcDebugName = "light bind group 2"
        };
        plBindGroupHandle tLightBindGroup2 = gptGfx->create_bind_group(ptDevice, &tLightBGDesc);

        const plBindGroupUpdateBufferData atLightBufferData[] = 
        {
            { .uSlot = 0, .tBuffer = ptScene->atLightBuffer[uFrameIdx], .szBufferRange = sizeof(plGPULight) * pl_sb_size(ptScene->sbtLightData)},
            { .uSlot = 1, .tBuffer = ptView->atLightShadowDataBuffer[uFrameIdx], .szBufferRange = sizeof(plGPULightShadowData) * pl_sb_size(ptView->sbtLightShadowData)}
        };
        plBindGroupUpdateTextureData atBGTextureData[4] = {0};
        for(uint32_t i = 0; i < 4; i++)
        {
            atBGTextureData[i].tTexture = (ptView->tShadowData.atDepthTextureViews[i])[uFrameIdx];
            atBGTextureData[i].uSlot = 2;
            atBGTextureData[i].uIndex = i;
            atBGTextureData[i].tType = PL_TEXTURE_BINDING_TYPE_SAMPLED;
        }

        plBindGroupUpdateSamplerData tShadowSamplerData[] = {
            {
                .tSampler = gptData->tShadowSampler,
                .uSlot    = 6
            }
        };

        plBindGroupUpdateData tBGData2 = {
            .uBufferCount = 2,
            .atBufferBindings = atLightBufferData,
            .uTextureCount = 4,
            .atTextureBindings = atBGTextureData,
            .uSamplerCount = 1,
            .atSamplerBindings = tShadowSamplerData
        };
        gptGfx->update_bind_group(gptData->ptDevice, tLightBindGroup2, &tBGData2);
        gptGfx->queue_bind_group_for_deletion(ptDevice, tLightBindGroup2);
        plBuffer* ptLightingBuffer = gptGfx->get_buffer(ptDevice, ptScene->atLightBuffer[uFrameIdx]);
        memcpy(ptLightingBuffer->tMemoryAllocation.pHostMapped, ptScene->sbtLightData, sizeof(plGPULight) * pl_sb_size(ptScene->sbtLightData));

        typedef struct _plLightingDynamicData{
            int iDataOffset;
            int iVertexOffset;
            int unused[2];
        } plLightingDynamicData;
        plDynamicBinding tLightingDynamicData = pl__allocate_dynamic_data(ptDevice);
        
        gptGfx->reset_draw_stream(ptStream, 1);
        pl_add_to_draw_stream(ptStream, (plDrawStreamData)
        {
            .tShader        = ptScene->tLightingShader,
            .auDynamicBuffers = {
                tLightingDynamicData.uBufferHandle
            },
            .atVertexBuffers = {
                gptData->tFullQuadVertexBuffer
            },
            .tIndexBuffer         = gptData->tFullQuadIndexBuffer,
            .uIndexOffset         = 0,
            .uTriangleCount       = 2,
            .atBindGroups = {
                tGlobalBG,
                ptView->tLightingBindGroup[uFrameIdx],
                tLightBindGroup2
            },
            .auDynamicBufferOffsets = {
                tLightingDynamicData.uByteOffset
            },
            .uInstanceOffset = 0,
            .uInstanceCount = 1
        });
        gptGfx->draw_stream(ptEncoder, 1, &tArea);
        
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~subpass 2 - forward~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        gptGfx->next_subpass(ptEncoder);

        if(ptScene->tSkyboxTexture.uIndex != 0)
        {
            
            plDynamicBinding tSkyboxDynamicData = pl__allocate_dynamic_data(ptDevice);
            plMat4* ptSkyboxDynamicData = (plMat4*)tSkyboxDynamicData.pcData;
            *ptSkyboxDynamicData = pl_mat4_translate_vec3(ptCamera->tPos);

            gptGfx->reset_draw_stream(ptStream, 1);
            pl_add_to_draw_stream(ptStream, (plDrawStreamData)
            {
                .tShader        = gptData->tSkyboxShader,
                .auDynamicBuffers = {
                    tSkyboxDynamicData.uBufferHandle
                },
                .atVertexBuffers = {
                    ptScene->tVertexBuffer,
                },
                .tIndexBuffer         = ptScene->tIndexBuffer,
                .uIndexOffset         = ptScene->tSkyboxDrawable.uIndexOffset,
                .uTriangleCount       = ptScene->tSkyboxDrawable.uIndexCount / 3,
                .atBindGroups = {
                    tGlobalBG,
                    ptScene->tSkyboxBindGroup
                },
                .auDynamicBufferOffsets = {
                    tSkyboxDynamicData.uByteOffset
                },
                .uInstanceOffset = 0,
                .uInstanceCount = 1
            });
            gptGfx->draw_stream(ptEncoder, 1, &tArea);
        }
        

        // transparent & complex material objects
        gptJob->wait_for_counter(ptTransparentCounter);
        if(ptCullCamera)
        {
            pl_sb_reset(ptView->sbtVisibleTransparentDrawables);
            for(uint32_t uDrawableIndex = 0; uDrawableIndex < uTransparentDrawableCount; uDrawableIndex++)
            {
                const plDrawable tDrawable = ptScene->sbtTransparentDrawables[uDrawableIndex];
                if(!tDrawable.bCulled)
                    pl_sb_push(ptView->sbtVisibleTransparentDrawables, tDrawable);
            }
        }

        const uint32_t uVisibleTransparentDrawCount = pl_sb_size(ptView->sbtVisibleTransparentDrawables);
        gptGfx->reset_draw_stream(ptStream, uVisibleTransparentDrawCount);
        for(uint32_t i = 0; i < uVisibleTransparentDrawCount; i++)
        {
            const plDrawable tDrawable = ptView->sbtVisibleTransparentDrawables[i];
            plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
            plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
            
            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

            DynamicData* ptDynamicData = (DynamicData*)tDynamicBinding.pcData;
            ptDynamicData->iDataOffset = tDrawable.uDataOffset;
            ptDynamicData->iVertexOffset = tDrawable.uVertexOffset;
            ptDynamicData->tModel = ptTransform->tWorld;
            ptDynamicData->iMaterialOffset = tDrawable.uMaterialIndex;

            pl_add_to_draw_stream(ptStream, (plDrawStreamData)
            {
                .tShader        = tDrawable.tShader,
                .auDynamicBuffers = {
                    tDynamicBinding.uBufferHandle
                },
                .atVertexBuffers = {
                    ptScene->tVertexBuffer,
                },
                .tIndexBuffer         = tDrawable.uIndexCount == 0 ? (plBufferHandle){0} : ptScene->tIndexBuffer,
                .uIndexOffset         = tDrawable.uIndexOffset,
                .uTriangleCount       = tDrawable.uIndexCount == 0 ? tDrawable.uVertexCount / 3 : tDrawable.uIndexCount / 3,
                .atBindGroups = {
                    tGlobalBG,
                    tLightBindGroup2,
                    tDrawable.tMaterialBindGroup
                },
                .auDynamicBufferOffsets = {
                    tDynamicBinding.uByteOffset
                },
                .uInstanceOffset = 0,
                .uInstanceCount = 1
            });
        }
        gptGfx->draw_stream(ptEncoder, 1, &tArea);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~debug drawing~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // bounding boxes
        const uint32_t uOutlineDrawableCount = pl_sb_size(ptScene->sbtOutlineDrawables);
        if(uOutlineDrawableCount > 0 && gptData->bShowSelectedBoundingBox)
        {
            const plVec4 tOutlineColor = (plVec4){0.0f, (float)sin(gptIOI->get_io()->dTime * 3.0) * 0.25f + 0.75f, 0.0f, 1.0f};
            for(uint32_t i = 0; i < uOutlineDrawableCount; i++)
            {
                const plDrawable tDrawable = ptScene->sbtOutlineDrawables[i];
                plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
                plMeshComponent* ptMesh = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptObject->tMesh);
                gptDraw->add_3d_aabb(ptView->pt3DSelectionDrawList, ptMesh->tAABBFinal.tMin, ptMesh->tAABBFinal.tMax, (plDrawLineOptions){.uColor = PL_COLOR_32_VEC4(tOutlineColor), .fThickness = 0.01f});
                
            }
        }

        // light drawing (temporary)
        for(uint32_t i = 0; i < pl_sb_size(ptScene->sbtLightData); i++)
        {
            if(ptScene->sbtLightData[i].iType == PL_LIGHT_TYPE_POINT)
            {
                const plVec4 tColor = {.rgb = ptScene->sbtLightData[i].tColor, .a = 1.0f};
                gptDraw->add_3d_cross(ptView->pt3DDrawList, ptScene->sbtLightData[i].tPosition, 0.02f, (plDrawLineOptions){.uColor = PL_COLOR_32_VEC4(tColor), .fThickness = 0.25f});
            }
        }

        // debug drawing
        if(gptData->bDrawAllBoundingBoxes)
        {
            for(uint32_t i = 0; i < uOpaqueDrawableCount; i++)
            {
                plMeshComponent* ptMesh = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptScene->sbtOpaqueDrawables[i].tEntity);

                gptDraw->add_3d_aabb(ptView->pt3DDrawList, ptMesh->tAABBFinal.tMin, ptMesh->tAABBFinal.tMax, (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(1.0f, 0.0f, 0.0f), .fThickness = 0.02f});
            }
            for(uint32_t i = 0; i < uTransparentDrawableCount; i++)
            {
                plMeshComponent* ptMesh = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptScene->sbtTransparentDrawables[i].tEntity);

                gptDraw->add_3d_aabb(ptView->pt3DDrawList, ptMesh->tAABBFinal.tMin, ptMesh->tAABBFinal.tMax, (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(1.0f, 0.0f, 0.0f), .fThickness = 0.02f});
            }
        }
        else if(gptData->bDrawVisibleBoundingBoxes)
        {
            for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
            {
                plMeshComponent* ptMesh = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptView->sbtVisibleOpaqueDrawables[i].tEntity);

                gptDraw->add_3d_aabb(ptView->pt3DDrawList, ptMesh->tAABBFinal.tMin, ptMesh->tAABBFinal.tMax, (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(1.0f, 0.0f, 0.0f), .fThickness = 0.02f});
            }
            for(uint32_t i = 0; i < uVisibleTransparentDrawCount; i++)
            {
                plMeshComponent* ptMesh = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptView->sbtVisibleTransparentDrawables[i].tEntity);

                gptDraw->add_3d_aabb(ptView->pt3DDrawList, ptMesh->tAABBFinal.tMin, ptMesh->tAABBFinal.tMax, (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(1.0f, 0.0f, 0.0f), .fThickness = 0.02f});
            }
        }

        if(gptData->bShowOrigin)
        {
            const plMat4 tTransform = pl_identity_mat4();
            gptDraw->add_3d_transform(ptView->pt3DDrawList, &tTransform, 10.0f, (plDrawLineOptions){.fThickness = 0.02f});
        }

        if(ptCullCamera && ptCullCamera != ptCamera)
        {
            plDrawFrustumDesc tFrustumDesc = {
                .fAspectRatio = ptCullCamera->fAspectRatio,
                .fFarZ = ptCullCamera->fFarZ,
                .fNearZ = ptCullCamera->fNearZ,
                .fYFov = ptCullCamera->fFieldOfView
            };
            gptDraw->add_3d_frustum(ptView->pt3DSelectionDrawList, &ptCullCamera->tTransformMat, tFrustumDesc, (plDrawLineOptions){.uColor = PL_COLOR_32_YELLOW, .fThickness = 0.02f});
        }

        gptDrawBackend->submit_3d_drawlist(ptView->pt3DDrawList, ptEncoder, tDimensions.x, tDimensions.y, &tMVP, PL_DRAW_FLAG_DEPTH_TEST | PL_DRAW_FLAG_DEPTH_WRITE, 1);
        gptDrawBackend->submit_3d_drawlist(ptView->pt3DSelectionDrawList, ptEncoder, tDimensions.x, tDimensions.y, &tMVP, 0, 1);
        gptGfx->end_render_pass(ptEncoder);

         //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~entity selection~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        if(gptData->uClickedFrame == uFrameIdx)
        {
            gptData->uClickedFrame = UINT32_MAX;
            plTexture* ptTexture = gptGfx->get_texture(ptDevice, ptView->tPickTexture);
            plBuffer* ptCachedStagingBuffer = gptGfx->get_buffer(ptDevice, gptData->tCachedStagingBuffer);
            const plVec2 tMousePos = gptIOI->get_mouse_pos();

            uint32_t uRowWidth = (uint32_t)ptTexture->tDesc.tDimensions.x * 4;
            uint32_t uPos = uRowWidth * (uint32_t)tMousePos.y + (uint32_t)tMousePos.x * 4;

            unsigned char* pucMapping2 = (unsigned char*)ptCachedStagingBuffer->tMemoryAllocation.pHostMapped;
            unsigned char* pucMapping = &pucMapping2[uPos];
            gptData->tPickedEntity.uIndex = pucMapping[0] + 256 * pucMapping[1] + 65536 * pucMapping[2];
            gptData->tPickedEntity.uGeneration = ptScene->tComponentLibrary.sbtEntityGenerations[gptData->tPickedEntity.uIndex];
        }

        bool bOwnMouse = gptUI->wants_mouse_capture();
        if(!bOwnMouse && gptIOI->is_mouse_clicked(PL_MOUSE_BUTTON_RIGHT, false))
        {
            gptData->uClickedFrame = uFrameIdx;

            plBindGroupLayout tPickBindGroupLayout0 = {
                .atBufferBindings = {
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                        .uSlot = 0,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    }
                }

            };
            const plBindGroupDesc tPickBGDesc = {
                .ptPool      = gptData->aptBindGroupPools[gptGfx->get_current_frame_index()],
                .ptLayout    = &tPickBindGroupLayout0,
                .pcDebugName = "temp pick bind group"
            };
            plBindGroupHandle tPickBindGroup = gptGfx->create_bind_group(ptDevice, &tPickBGDesc);

            const plBindGroupUpdateBufferData atPickBufferData[] = 
            {
                {
                    .tBuffer       = ptView->atGlobalBuffers[uFrameIdx],
                    .uSlot         = 0,
                    .szBufferRange = sizeof(BindGroup_0)
                }
            };
            plBindGroupUpdateData tPickBGData0 = {
                .uBufferCount = 1,
                .atBufferBindings = atPickBufferData,
            };
            gptGfx->update_bind_group(gptData->ptDevice, tPickBindGroup, &tPickBGData0);
            gptGfx->queue_bind_group_for_deletion(ptDevice, tPickBindGroup);

            typedef struct _plPickDynamicData
            {
                plVec4 tColor;
                plMat4 tModel;
            } plPickDynamicData;
            ptEncoder = gptGfx->begin_render_pass(ptCommandBuffer, ptView->tPickRenderPass);

            gptGfx->bind_shader(ptEncoder, gptData->tPickShader);
            gptGfx->bind_vertex_buffer(ptEncoder, ptScene->tVertexBuffer);
            for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
            {
                const plDrawable tDrawable = ptView->sbtVisibleOpaqueDrawables[i];

                uint32_t uId = tDrawable.tEntity.uIndex;
                
                plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
                plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
                
                plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);
                plPickDynamicData* ptDynamicData = (plPickDynamicData*)tDynamicBinding.pcData;
                
                ptDynamicData->tColor = (plVec4){
                    ((float)(uId & 0x000000ff) / 255.0f),
                    ((float)((uId & 0x0000ff00) >>  8) / 255.0f),
                    ((float)((uId & 0x00ff0000) >> 16) / 255.0f),
                    1.0f};
                ptDynamicData->tModel = ptTransform->tWorld;

                gptGfx->bind_graphics_bind_groups(ptEncoder, gptData->tPickShader, 0, 1, &tPickBindGroup, 1, &tDynamicBinding);

                plDrawIndex tDraw = {
                    .tIndexBuffer = ptScene->tIndexBuffer,
                    .uIndexCount = tDrawable.uIndexCount,
                    .uIndexStart = tDrawable.uIndexOffset,
                    .uInstanceCount = 1
                };
                gptGfx->draw_indexed(ptEncoder, 1, &tDraw);
            }

            for(uint32_t i = 0; i < uVisibleTransparentDrawCount; i++)
            {
                const plDrawable tDrawable = ptView->sbtVisibleTransparentDrawables[i];

                plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
                plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
                
                plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);
                plPickDynamicData* ptDynamicData = (plPickDynamicData*)tDynamicBinding.pcData;
                const uint32_t uId = tDrawable.tEntity.uIndex;
                ptDynamicData->tColor = (plVec4){
                    ((float)(uId & 0x000000ff) / 255.0f),
                    ((float)((uId & 0x0000ff00) >>  8) / 255.0f),
                    ((float)((uId & 0x00ff0000) >> 16) / 255.0f),
                    1.0f};
                ptDynamicData->tModel = ptTransform->tWorld;

                gptGfx->bind_graphics_bind_groups(ptEncoder, gptData->tPickShader, 0, 1, &tPickBindGroup, 1, &tDynamicBinding);

                plDrawIndex tDraw = {
                    .tIndexBuffer = ptScene->tIndexBuffer,
                    .uIndexCount = tDrawable.uIndexCount,
                    .uIndexStart = tDrawable.uIndexOffset,
                    .uInstanceCount = 1
                };
                gptGfx->draw_indexed(ptEncoder, 1, &tDraw);
            }
            gptGfx->end_render_pass(ptEncoder);

            plBuffer* ptCachedStagingBuffer = gptGfx->get_buffer(ptDevice, gptData->tCachedStagingBuffer);
            memset(ptCachedStagingBuffer->tMemoryAllocation.pHostMapped, 0, ptCachedStagingBuffer->tDesc.szByteSize);

            plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);

            plTexture* ptTexture = gptGfx->get_texture(ptDevice, ptView->tPickTexture);
            const plBufferImageCopy tBufferImageCopy = {
                .uImageWidth = (uint32_t)ptTexture->tDesc.tDimensions.x,
                .uImageHeight = (uint32_t)ptTexture->tDesc.tDimensions.y,
                .uImageDepth = 1,
                .uLayerCount = 1
            };
            gptGfx->copy_texture_to_buffer(ptBlitEncoder, ptView->tPickTexture, gptData->tCachedStagingBuffer, 1, &tBufferImageCopy);
            gptGfx->end_blit_pass(ptBlitEncoder);
        }

        gptGfx->end_command_recording(ptCommandBuffer);

        const plSubmitInfo tSubmitInfo = {
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {tSemHandle},
            .auSignalSemaphoreValues = {++ulValue}
        };
        gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo);
        gptGfx->return_command_buffer(ptCommandBuffer);
    }

     //~~~~~~~~~~~~~~~~~~~~~~~~~~~~uv map pass for JFA~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    {
        const plBeginCommandInfo tBeginInfo = {
            .uWaitSemaphoreCount   = 1,
            .atWaitSempahores      = {tSemHandle},
            .auWaitSemaphoreValues = {ulValue},
        };

        plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
        gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo);

        plRenderEncoder* ptEncoder = gptGfx->begin_render_pass(ptCommandBuffer, ptView->tUVRenderPass);

        // submit nonindexed draw using basic API
        gptGfx->bind_shader(ptEncoder, gptData->tUVShader);
        gptGfx->bind_vertex_buffer(ptEncoder, gptData->tFullQuadVertexBuffer);

        plDrawIndex tDraw = {
            .tIndexBuffer   = gptData->tFullQuadIndexBuffer,
            .uIndexCount    = 6,
            .uInstanceCount = 1,
        };
        gptGfx->draw_indexed(ptEncoder, 1, &tDraw);

        // end render pass
        gptGfx->end_render_pass(ptEncoder);

        gptGfx->end_command_recording(ptCommandBuffer);

        const plSubmitInfo tSubmitInfo = {
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {tSemHandle},
            .auSignalSemaphoreValues = {++ulValue}
        };
        gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo);
        gptGfx->return_command_buffer(ptCommandBuffer);
    }

     //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~jump flood~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    {
        const uint32_t uOutlineDrawableCount = pl_sb_size(ptScene->sbtOutlineDrawables);
        

        // find next power of 2
        uint32_t uJumpDistance = 1;
        uint32_t uHalfWidth = gptData->uOutlineWidth / 2;
        if (uHalfWidth && !(uHalfWidth & (uHalfWidth - 1))) 
            uJumpDistance = uHalfWidth;
        while(uJumpDistance < uHalfWidth)
            uJumpDistance <<= 1;

        // calculate number of jumps necessary
        uint32_t uJumpSteps = 0;
        while(uJumpDistance)
        {
            uJumpSteps++;
            uJumpDistance >>= 1;
        }

        float fJumpDistance = (float)uHalfWidth;
        if(uOutlineDrawableCount == 0)
            uJumpSteps = 1;

        const plDispatch tDispach = {
            .uGroupCountX     = (uint32_t)tDimensions.x / 8,
            .uGroupCountY     = (uint32_t)tDimensions.y / 8,
            .uGroupCountZ     = 1,
            .uThreadPerGroupX = 8,
            .uThreadPerGroupY = 8,
            .uThreadPerGroupZ = 1
        };

        const plBindGroupLayout tJFABindGroupLayout = {
            .atTextureBindings = {
                {.uSlot = 0, .tStages = PL_STAGE_COMPUTE, .tType = PL_TEXTURE_BINDING_TYPE_STORAGE},
                {.uSlot = 1, .tStages = PL_STAGE_COMPUTE, .tType = PL_TEXTURE_BINDING_TYPE_STORAGE}
            }
        };

        const plBindGroupDesc tJFABindGroupDesc = {
            .ptPool      = gptData->aptBindGroupPools[gptGfx->get_current_frame_index()],
            .ptLayout    = &tJFABindGroupLayout,
            .pcDebugName = "temp jfa bind group"
        };
        plBindGroupHandle atJFABindGroups[] = {
            gptGfx->create_bind_group(gptData->ptDevice, &tJFABindGroupDesc),
            gptGfx->create_bind_group(gptData->ptDevice, &tJFABindGroupDesc)
        };

        const plBindGroupUpdateTextureData atJFATextureData0[] = 
        {
            {
                .tTexture = ptView->atUVMaskTexture0[uFrameIdx],
                .uSlot    = 0,
                .tType    = PL_TEXTURE_BINDING_TYPE_STORAGE,
                    .tCurrentUsage = PL_TEXTURE_USAGE_STORAGE
            },
            {
                .tTexture = ptView->atUVMaskTexture1[uFrameIdx],
                .uSlot    = 1,
                .tType    = PL_TEXTURE_BINDING_TYPE_STORAGE,
                    .tCurrentUsage = PL_TEXTURE_USAGE_STORAGE
            }
        };

        const plBindGroupUpdateTextureData atJFATextureData1[] = 
        {
            {
                .tTexture = ptView->atUVMaskTexture1[uFrameIdx],
                .uSlot    = 0,
                .tType    = PL_TEXTURE_BINDING_TYPE_STORAGE,
                    .tCurrentUsage = PL_TEXTURE_USAGE_STORAGE
            },
            {
                .tTexture = ptView->atUVMaskTexture0[uFrameIdx],
                .uSlot    = 1,
                .tType    = PL_TEXTURE_BINDING_TYPE_STORAGE,
                    .tCurrentUsage = PL_TEXTURE_USAGE_STORAGE
            }
        };

        const plBindGroupUpdateData tJFABGData0 = {
            .uTextureCount = 2,
            .atTextureBindings = atJFATextureData0
        };
        const plBindGroupUpdateData tJFABGData1 = {
            .uTextureCount = 2,
            .atTextureBindings = atJFATextureData1
        };
        gptGfx->update_bind_group(gptData->ptDevice, atJFABindGroups[0], &tJFABGData0);
        gptGfx->update_bind_group(gptData->ptDevice, atJFABindGroups[1], &tJFABGData1);
        gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, atJFABindGroups[0]);
        gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, atJFABindGroups[1]);

        for(uint32_t i = 0; i < uJumpSteps; i++)
        {
            const plBeginCommandInfo tBeginInfo = {
                .uWaitSemaphoreCount   = 1,
                .atWaitSempahores      = {tSemHandle},
                .auWaitSemaphoreValues = {ulValue},
            };

            plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
            gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo);

            // begin main renderpass (directly to swapchain)
            plComputeEncoder* ptComputeEncoder = gptGfx->begin_compute_pass(ptCommandBuffer);

            ptView->tLastUVMask = (i % 2 == 0) ? ptView->atUVMaskTexture1[uFrameIdx] : ptView->atUVMaskTexture0[uFrameIdx];

            // submit nonindexed draw using basic API
            gptGfx->bind_compute_shader(ptComputeEncoder, gptData->tJFAShader);

            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);
            plVec4* ptJumpDistance = (plVec4*)tDynamicBinding.pcData;
            ptJumpDistance->x = fJumpDistance;

            gptGfx->bind_compute_bind_groups(ptComputeEncoder, gptData->tJFAShader, 0, 1, &atJFABindGroups[i % 2], 1, &tDynamicBinding);
            gptGfx->dispatch(ptComputeEncoder, 1, &tDispach);

            // end render pass
            gptGfx->end_compute_pass(ptComputeEncoder);

            // end recording
            gptGfx->end_command_recording(ptCommandBuffer);

            const plSubmitInfo tSubmitInfo = {
                .uSignalSemaphoreCount   = 1,
                .atSignalSempahores      = {tSemHandle},
                .auSignalSemaphoreValues = {++ulValue},
            };
            gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo);
            gptGfx->return_command_buffer(ptCommandBuffer);

            fJumpDistance = fJumpDistance / 2.0f;
            if(fJumpDistance < 1.0f)
                fJumpDistance = 1.0f;
        }
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~post process~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    {
        const plBeginCommandInfo tBeginInfo = {
            .uWaitSemaphoreCount   = 1,
            .atWaitSempahores      = {tSemHandle},
            .auWaitSemaphoreValues = {ulValue},
        };

        plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
        gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo);

        pl_refr_post_process_scene(ptCommandBuffer, uSceneHandle, uViewHandle, &tMVP);
        gptGfx->end_command_recording(ptCommandBuffer);

        const plSubmitInfo tSubmitInfo = {
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {tSemHandle},
            .auSignalSemaphoreValues = {++ulValue}
        };
        gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo);
        gptGfx->return_command_buffer(ptCommandBuffer);
    }
    gptData->aulNextTimelineValue[uFrameIdx] = ulValue;

    // update stats
    static double* pdVisibleOpaqueObjects = NULL;
    static double* pdVisibleTransparentObjects = NULL;
    if(!pdVisibleOpaqueObjects)
    {
        pdVisibleOpaqueObjects = gptStats->get_counter("visible opaque objects");
        pdVisibleTransparentObjects = gptStats->get_counter("visible transparent objects");
    }

    // only record stats for first scene
    if(uSceneHandle == 0)
    {
        *pdVisibleOpaqueObjects = (double)(pl_sb_size(ptView->sbtVisibleOpaqueDrawables));
        *pdVisibleTransparentObjects = (double)(pl_sb_size(ptView->sbtVisibleTransparentDrawables));
    }

    pl_end_profile_sample(0);
}

static void
pl_refr_resize(void)
{
    uint32_t uImageCount = 0;
    plTextureHandle* atSwapchainImages = gptGfx->get_swapchain_images(gptData->ptSwap, &uImageCount);

    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(gptData->atCmdPools[0]);
    gptGfx->begin_command_recording(ptCommandBuffer, NULL);

    // begin blit pass, copy buffer, end pass
    plBlitEncoder* ptEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
    plSwapchainInfo tInfo = gptGfx->get_swapchain_info(gptData->ptSwap);
    const plTextureDesc tColorTextureDesc = {
        .tDimensions   = {(float)tInfo.uWidth, (float)tInfo.uHeight, 1},
        .tFormat       = tInfo.tFormat,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
        .pcDebugName   = "offscreen color texture",
        .tSampleCount  = tInfo.tSampleCount
    };

    gptGfx->queue_texture_for_deletion(gptData->ptDevice, gptData->tMSAATexture);

    // create textures
    gptData->tMSAATexture = gptGfx->create_texture(gptData->ptDevice, &tColorTextureDesc, NULL);

    // retrieve textures
    plTexture* ptColorTexture = gptGfx->get_texture(gptData->ptDevice, gptData->tMSAATexture);

    // allocate memory

    const plDeviceMemoryAllocation tColorAllocation = gptGfx->allocate_memory(gptData->ptDevice, 
        ptColorTexture->tMemoryRequirements.ulSize,
        PL_MEMORY_GPU,
        ptColorTexture->tMemoryRequirements.uMemoryTypeBits,
        "color texture memory");

    // bind memory
    gptGfx->bind_texture_to_memory(gptData->ptDevice, gptData->tMSAATexture, &tColorAllocation);

    // set initial usage
    gptGfx->set_texture_usage(ptEncoder, gptData->tMSAATexture, PL_TEXTURE_USAGE_COLOR_ATTACHMENT, 0);

    gptGfx->end_blit_pass(ptEncoder);

    // finish recording
    gptGfx->end_command_recording(ptCommandBuffer);

    // submit command buffer
    gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);
    gptGfx->return_command_buffer(ptCommandBuffer);

    plRenderPassAttachments atMainAttachmentSets[16] = {0};
    for(uint32_t i = 0; i < uImageCount; i++)
    {
        atMainAttachmentSets[i].atViewAttachments[0] = atSwapchainImages[i];
        atMainAttachmentSets[i].atViewAttachments[1] = gptData->tMSAATexture;
    }
    gptGfx->update_render_pass_attachments(gptData->ptDevice, gptData->tMainRenderPass, gptIO->tMainViewportSize, atMainAttachmentSets);
}

static bool
pl_refr_begin_frame(void)
{
    pl_begin_profile_sample(0, __FUNCTION__);

    if(gptData->bReloadSwapchain)
    {
        gptData->bReloadSwapchain = false;
        plSwapchainInit tDesc = {
            .bVSync  = gptData->bVSync,
            .uWidth  = (uint32_t)gptIO->tMainViewportSize.x,
            .uHeight = (uint32_t)gptIO->tMainViewportSize.y,
            .tSampleCount = gptGfx->get_swapchain_info(gptData->ptSwap).tSampleCount
        };
        gptGfx->recreate_swapchain(gptData->ptSwap, &tDesc);
        
        pl_refr_resize();
        pl_end_profile_sample(0);
        return false;
    }

    gptGfx->begin_frame(gptData->ptDevice);
    gptGfx->reset_command_pool(gptData->atCmdPools[gptGfx->get_current_frame_index()], 0);
    gptGfx->reset_bind_group_pool(gptData->aptBindGroupPools[gptGfx->get_current_frame_index()]);
    gptData->tCurrentDynamicDataBlock = gptGfx->allocate_dynamic_data_block(gptData->ptDevice);

    if(!gptGfx->acquire_swapchain_image(gptData->ptSwap))
    {
        plSwapchainInit tDesc = {
            .bVSync  = gptData->bVSync,
            .uWidth  = (uint32_t)gptIO->tMainViewportSize.x,
            .uHeight = (uint32_t)gptIO->tMainViewportSize.y,
            .tSampleCount = gptGfx->get_swapchain_info(gptData->ptSwap).tSampleCount
        };
        gptGfx->recreate_swapchain(gptData->ptSwap, &tDesc);
        pl_refr_resize();
        pl_end_profile_sample(0);
        return false;
    }

    pl_end_profile_sample(0);
    return true;
}

static void
pl_refr_end_frame(void)
{
    pl_begin_profile_sample(0, __FUNCTION__);

    plCommandPool* ptCmdPool = gptData->atCmdPools[gptGfx->get_current_frame_index()];
    plDevice*   ptDevice   = gptData->ptDevice;
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();

    uint64_t ulValue = gptData->aulNextTimelineValue[uFrameIdx];
    plTimelineSemaphore* tSemHandle = gptData->aptSemaphores[uFrameIdx];

    // final work set
    const plBeginCommandInfo tBeginInfo = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {tSemHandle},
        .auWaitSemaphoreValues = {ulValue},
    };
    
    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
    gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo);

    plRenderEncoder* ptEncoder = gptGfx->begin_render_pass(ptCommandBuffer, gptData->tMainRenderPass);

    // render ui
    pl_begin_profile_sample(0, "render ui");
    plIO* ptIO = gptIOI->get_io();
    gptUI->end_frame();
    gptDrawBackend->submit_2d_drawlist(gptUI->get_draw_list(), ptEncoder, ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y, gptGfx->get_swapchain_info(gptData->ptSwap).tSampleCount);
    gptDrawBackend->submit_2d_drawlist(gptUI->get_debug_draw_list(), ptEncoder, ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y, gptGfx->get_swapchain_info(gptData->ptSwap).tSampleCount);
    pl_end_profile_sample(0);

    gptGfx->end_render_pass(ptEncoder);

    gptGfx->end_command_recording(ptCommandBuffer);

    const plSubmitInfo tSubmitInfo = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {tSemHandle},
        .auSignalSemaphoreValues = {++ulValue},
    };
    gptData->aulNextTimelineValue[uFrameIdx] = ulValue;
    if(!gptGfx->present(ptCommandBuffer, &tSubmitInfo, &gptData->ptSwap, 1))
    {
        plSwapchainInit tDesc = {
            .bVSync  = gptData->bVSync,
            .uWidth  = (uint32_t)gptIO->tMainViewportSize.x,
            .uHeight = (uint32_t)gptIO->tMainViewportSize.y,
            .tSampleCount = gptGfx->get_swapchain_info(gptData->ptSwap).tSampleCount
        };
        gptGfx->recreate_swapchain(gptData->ptSwap, &tDesc);
        pl_refr_resize();
    }

    gptGfx->return_command_buffer(ptCommandBuffer);
    pl_end_profile_sample(0);
}

static plDrawList3D*
pl_refr_get_debug_drawlist(uint32_t uSceneHandle, uint32_t uViewHandle)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plRefView* ptView = &ptScene->atViews[uViewHandle];
    return ptView->pt3DDrawList;
}

static plDrawList3D*
pl_refr_get_gizmo_drawlist(uint32_t uSceneHandle, uint32_t uViewHandle)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plRefView* ptView = &ptScene->atViews[uViewHandle];
    return ptView->pt3DGizmoDrawList;
}

static plBindGroupHandle
pl_refr_get_view_color_texture(uint32_t uSceneHandle, uint32_t uViewHandle)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plRefView* ptView = &ptScene->atViews[uViewHandle];
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    return ptView->tFinalTextureHandle[uFrameIdx];
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

static void
pl_show_graphics_options(const char* pcTitle)
{
    if(gptUI->begin_collapsing_header(pcTitle, 0))
    {
        if(gptUI->checkbox("VSync", &gptData->bVSync))
            gptData->bReloadSwapchain = true;
        gptUI->checkbox("Show Origin", &gptData->bShowOrigin);
        gptUI->checkbox("Frustum Culling", &gptData->bFrustumCulling);
        gptUI->slider_float("Lambda Split", &gptData->fLambdaSplit, 0.0f, 1.0f, 0);
        gptUI->checkbox("Draw All Bounding Boxes", &gptData->bDrawAllBoundingBoxes);
        gptUI->checkbox("Draw Visible Bounding Boxes", &gptData->bDrawVisibleBoundingBoxes);
        gptUI->checkbox("Show Selected Bounding Box", &gptData->bShowSelectedBoundingBox);

        int iOutlineWidth  = (int)gptData->uOutlineWidth;
        if(gptUI->slider_int("Outline Width", &iOutlineWidth, 2, 50, 0))
        {
            gptData->uOutlineWidth = (uint32_t)iOutlineWidth;
        }
        gptUI->end_collapsing_header();
    }
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
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[2]) > 0) { uDestStride += 1; }
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[4]) > 0) { uDestStride += 1; }
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[6]) > 0) { uDestStride += 1; }

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

    for(uint32_t uFrameIndex = 0; uFrameIndex < gptGfx->get_frames_in_flight(); uFrameIndex++)
        tSkinData.atDynamicTexture[uFrameIndex] = pl__refr_create_texture_with_data(&tSkinTextureDesc, "joint texture", uFrameIndex, ptSkinComponent->sbtTextureData, sizeof(float) * 4 * textureWidth * textureWidth);

    int aiSpecializationData[] = {(int)ulVertexStreamMask, (int)uStride, (int)ptMesh->ulVertexStreamMask, (int)uDestStride};
    const plComputeShaderDesc tComputeShaderDesc = {
        .tShader = gptShader->load_glsl("../shaders/skinning.comp", "main", NULL, NULL),
        .pTempConstantData = aiSpecializationData,
        .atConstants = {
            { .uID = 0, .uOffset = 0,               .tType = PL_DATA_TYPE_INT},
            { .uID = 1, .uOffset = sizeof(int),     .tType = PL_DATA_TYPE_INT},
            { .uID = 2, .uOffset = 2 * sizeof(int), .tType = PL_DATA_TYPE_INT},
            { .uID = 3, .uOffset = 3 * sizeof(int), .tType = PL_DATA_TYPE_INT}
        },
        .atBindGroupLayouts = {
            {
                .atBufferBindings = {
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 0, .tStages = PL_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 1, .tStages = PL_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 2, .tStages = PL_STAGE_COMPUTE},
                },
                .atSamplerBindings = {
                    {.uSlot = 3, .tStages = PL_STAGE_COMPUTE}
                }
            },
            {
                .atTextureBindings = {
                    {.uSlot =  0, .tStages = PL_STAGE_ALL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
                }
            }
        }
    };
    tSkinData.tShader = gptGfx->create_compute_shader(gptData->ptDevice, &tComputeShaderDesc);
    pl_temp_allocator_reset(&gptData->tTempAllocator);
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
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[2]) > 0) { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1; }
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[4]) > 0) { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_2; }
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[6]) > 0) { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_3; }

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
    for(uint32_t i = 0; i < 8; i+=2)
    {
        const uint32_t uVertexTexCount0 = pl_sb_size(ptMesh->sbtVertexTextureCoordinates[i]);
        const uint32_t uVertexTexCount1 = pl_sb_size(ptMesh->sbtVertexTextureCoordinates[i + 1]);

        if(uVertexTexCount1 > 0)
        {
            for(uint32_t j = 0; j < uVertexTexCount0; j++)
            {
                const plVec2* ptTextureCoordinates0 = &(ptMesh->sbtVertexTextureCoordinates[i])[j];
                const plVec2* ptTextureCoordinates1 = &(ptMesh->sbtVertexTextureCoordinates[i + 1])[j];
                ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + j * uStride + uOffset].x = ptTextureCoordinates0->u;
                ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + j * uStride + uOffset].y = ptTextureCoordinates0->v;
                ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + j * uStride + uOffset].z = ptTextureCoordinates1->u;
                ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + j * uStride + uOffset].w = ptTextureCoordinates1->v;
            }
        }
        else
        {
            for(uint32_t j = 0; j < uVertexTexCount0; j++)
            {
                const plVec2* ptTextureCoordinates = &(ptMesh->sbtVertexTextureCoordinates[i])[j];
                ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + j * uStride + uOffset].x = ptTextureCoordinates->u;
                ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + j * uStride + uOffset].y = ptTextureCoordinates->v;
                ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + j * uStride + uOffset].z = 0.0f;
                ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + j * uStride + uOffset].w = 0.0f;
            } 
        }

        if(uVertexTexCount0 > 0)
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
        .fOcclusionStrength = 1.0f,
        .fEmissiveStrength = 1.0f,
        .iBaseColorUVSet = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_BASE_COLOR_MAP].uUVSet,
        .iNormalUVSet = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_NORMAL_MAP].uUVSet,
        .iEmissiveUVSet = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_EMISSIVE_MAP].uUVSet,
        .iOcclusionUVSet = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_OCCLUSION_MAP].uUVSet,
        .iMetallicRoughnessUVSet = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_METAL_ROUGHNESS_MAP].uUVSet,
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
    plDevice* ptDevice = gptData->ptDevice;

    plShader* ptShader = gptGfx->get_shader(ptDevice, tHandle);

    size_t szSpecializationSize = 0;
    for(uint32_t i = 0; i < ptShader->tDesc._uConstantCount; i++)
    {
        const plSpecializationConstant* ptConstant = &ptShader->tDesc.atConstants[i];
        szSpecializationSize += pl__get_data_type_size2(ptConstant->tType);
    }

    const uint64_t ulVariantHash = pl_hm_hash(ptVariant->pTempConstantData, szSpecializationSize, ptVariant->tGraphicsState.ulValue);
    const uint64_t ulIndex = pl_hm_lookup(gptData->ptVariantHashmap, ulVariantHash);

    if(ulIndex != UINT64_MAX)
        return gptData->_sbtVariantHandles[ulIndex];

    plShaderDesc tDesc = ptShader->tDesc;
    tDesc.tGraphicsState = ptVariant->tGraphicsState;
    tDesc.pTempConstantData = ptVariant->pTempConstantData;

    plShaderHandle tShader = gptGfx->create_shader(ptDevice, &tDesc);

    pl_hm_insert(gptData->ptVariantHashmap, ulVariantHash, pl_sb_size(gptData->_sbtVariantHandles));
    pl_sb_push(gptData->_sbtVariantHandles, tShader);
    return tShader;
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
            if(i == PL_TEXTURE_SLOT_BASE_COLOR_MAP || i == PL_TEXTURE_SLOT_EMISSIVE_MAP)
            {
                size_t szResourceSize = 0;
                const char* pcFileData = gptResource->get_file_data(ptMaterial->atTextureMaps[i].tResource, &szResourceSize);
                float* rawBytes = gptImage->load_hdr((unsigned char*)pcFileData, (int)szResourceSize, &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
                gptResource->set_buffer_data(ptMaterial->atTextureMaps[i].tResource, sizeof(float) * texWidth * texHeight * 4, rawBytes);
                ptMaterial->atTextureMaps[i].uWidth = texWidth;
                ptMaterial->atTextureMaps[i].uHeight = texHeight;
            }
            else
            {
                size_t szResourceSize = 0;
                const char* pcFileData = gptResource->get_file_data(ptMaterial->atTextureMaps[i].tResource, &szResourceSize);
                unsigned char* rawBytes = gptImage->load((unsigned char*)pcFileData, (int)szResourceSize, &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
                PL_ASSERT(rawBytes);
                ptMaterial->atTextureMaps[i].uWidth = texWidth;
                ptMaterial->atTextureMaps[i].uHeight = texHeight;
                gptResource->set_buffer_data(ptMaterial->atTextureMaps[i].tResource, texWidth * texHeight * 4, rawBytes);
            }
        }
    }
}

static plTextureHandle
pl__refr_create_texture(const plTextureDesc* ptDesc, const char* pcName, uint32_t uIdentifier, plTextureUsage tInitialUsage)
{
    // for convience
    plDevice* ptDevice = gptData->ptDevice;
    plCommandPool* ptCmdPool = gptData->atCmdPools[gptGfx->get_current_frame_index()];
 
    // create texture
    plTempAllocator tTempAllocator = {0};
    plTexture* ptTexture = NULL;
    const plTextureHandle tHandle = gptGfx->create_texture(ptDevice, ptDesc, &ptTexture);
    pl_temp_allocator_reset(&tTempAllocator);

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
    gptGfx->bind_texture_to_memory(ptDevice, tHandle, &tAllocation);
    pl_temp_allocator_free(&tTempAllocator);

    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
    gptGfx->begin_command_recording(ptCommandBuffer, NULL);
    plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
    gptGfx->set_texture_usage(ptBlitEncoder, tHandle, tInitialUsage, 0);
    gptGfx->end_blit_pass(ptBlitEncoder);
    gptGfx->end_command_recording(ptCommandBuffer);
    gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);
    gptGfx->return_command_buffer(ptCommandBuffer);
    return tHandle;
}

static plTextureHandle
pl__refr_create_texture_with_data(const plTextureDesc* ptDesc, const char* pcName, uint32_t uIdentifier, const void* pData, size_t szSize)
{
    // for convience
    plDevice* ptDevice = gptData->ptDevice;
    plCommandPool* ptCmdPool = gptData->atCmdPools[gptGfx->get_current_frame_index()];
 
    // create texture
    plTempAllocator tTempAllocator = {0};
    plTexture* ptTexture = NULL;
    const plTextureHandle tHandle = gptGfx->create_texture(ptDevice, ptDesc, &ptTexture);
    pl_temp_allocator_reset(&tTempAllocator);

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
    gptGfx->bind_texture_to_memory(ptDevice, tHandle, &tAllocation);
    pl_temp_allocator_free(&tTempAllocator);

    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
    gptGfx->begin_command_recording(ptCommandBuffer, NULL);
    plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
    gptGfx->set_texture_usage(ptBlitEncoder, tHandle, PL_TEXTURE_USAGE_SAMPLED, 0);


    // if data is presented, upload using staging buffer
    if(pData)
    {
        PL_ASSERT(ptDesc->uLayers == 1); // this is for simple textures right now

        // copy data to staging buffer
        plBuffer* ptStagingBuffer = gptGfx->get_buffer(ptDevice, gptData->tStagingBufferHandle[0]);

        memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, pData, szSize);


        const plBufferImageCopy tBufferImageCopy = {
            .uImageWidth = (uint32_t)ptDesc->tDimensions.x,
            .uImageHeight = (uint32_t)ptDesc->tDimensions.y,
            .uImageDepth = 1,
            .uLayerCount = 1
        };

        gptGfx->copy_buffer_to_texture(ptBlitEncoder, gptData->tStagingBufferHandle[0], tHandle, 1, &tBufferImageCopy);
        gptGfx->generate_mipmaps(ptBlitEncoder, tHandle);
    }

    gptGfx->end_blit_pass(ptBlitEncoder);
    gptGfx->end_command_recording(ptCommandBuffer);
    gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);
    gptGfx->return_command_buffer(ptCommandBuffer);

    return tHandle;
}

static plBufferHandle
pl__refr_create_staging_buffer(const plBufferDesc* ptDesc, const char* pcName, uint32_t uIdentifier)
{
    // for convience
    plDevice* ptDevice = gptData->ptDevice;

    // create buffer
    plTempAllocator tTempAllocator = {0};
    plBuffer* ptBuffer = NULL;
    const plBufferHandle tHandle = gptGfx->create_buffer(ptDevice, ptDesc, &ptBuffer);

    // allocate memory
    const plDeviceMemoryAllocation tAllocation = gptData->ptStagingUnCachedAllocator->allocate(gptData->ptStagingUnCachedAllocator->ptInst, 
        ptBuffer->tMemoryRequirements.uMemoryTypeBits,
        ptBuffer->tMemoryRequirements.ulSize,
        ptBuffer->tMemoryRequirements.ulAlignment,
        pl_temp_allocator_sprintf(&tTempAllocator, "sbuffer alloc %s: %u", pcName, uIdentifier));

    // bind memory
    gptGfx->bind_buffer_to_memory(ptDevice, tHandle, &tAllocation);
    pl_temp_allocator_free(&tTempAllocator);
    return tHandle;
}

static plBufferHandle 
pl__refr_create_cached_staging_buffer(const plBufferDesc* ptDesc, const char* pcName, uint32_t uIdentifier)
{
    // for convience
    plDevice* ptDevice = gptData->ptDevice;

    // create buffer
    plTempAllocator tTempAllocator = {0};
    plBuffer* ptBuffer = NULL;
    const plBufferHandle tHandle = gptGfx->create_buffer(ptDevice, ptDesc, &ptBuffer);
    pl_temp_allocator_reset(&tTempAllocator);

    // allocate memory
    const plDeviceMemoryAllocation tAllocation = gptData->ptStagingCachedAllocator->allocate(gptData->ptStagingCachedAllocator->ptInst, 
        ptBuffer->tMemoryRequirements.uMemoryTypeBits,
        ptBuffer->tMemoryRequirements.ulSize,
        ptBuffer->tMemoryRequirements.ulAlignment,
        pl_temp_allocator_sprintf(&tTempAllocator, "scbuffer alloc %s: %u", pcName, uIdentifier));

    // bind memory
    gptGfx->bind_buffer_to_memory(ptDevice, tHandle, &tAllocation);
    pl_temp_allocator_free(&tTempAllocator);
    return tHandle;
}

static plCommandPool*
pl__refr_get_command_pool(void)
{
    plCommandPool* ptCmdPool = gptData->atCmdPools[gptGfx->get_current_frame_index()];
    return ptCmdPool;
}

static plBufferHandle
pl__refr_create_local_buffer(const plBufferDesc* ptDesc, const char* pcName, uint32_t uIdentifier, const void* pData)
{
    // for convience
    plDevice* ptDevice = gptData->ptDevice;
    plCommandPool* ptCmdPool = gptData->atCmdPools[gptGfx->get_current_frame_index()];
    
    // create buffer
    plTempAllocator tTempAllocator = {0};
    plBuffer* ptBuffer = NULL;
    const plBufferHandle tHandle = gptGfx->create_buffer(ptDevice, ptDesc, &ptBuffer);
    pl_temp_allocator_reset(&tTempAllocator);

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
    gptGfx->bind_buffer_to_memory(ptDevice, tHandle, &tAllocation);
    pl_temp_allocator_free(&tTempAllocator);

    // if data is presented, upload using staging buffer
    if(pData)
    {
        // copy data to staging buffer
        plBuffer* ptStagingBuffer = gptGfx->get_buffer(ptDevice, gptData->tStagingBufferHandle[0]);
        memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, pData, ptDesc->szByteSize);

        // begin recording
        plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
        gptGfx->begin_command_recording(ptCommandBuffer, NULL);
        
        // begin blit pass, copy buffer, end pass
        plBlitEncoder* ptEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
        gptGfx->copy_buffer(ptEncoder, gptData->tStagingBufferHandle[0], tHandle, 0, 0, ptDesc->szByteSize);
        gptGfx->end_blit_pass(ptEncoder);

        // finish recording
        gptGfx->end_command_recording(ptCommandBuffer);

        // submit command buffer
        gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
        gptGfx->wait_on_command_buffer(ptCommandBuffer);
        gptGfx->return_command_buffer(ptCommandBuffer);
    }
    return tHandle;
}

static size_t
pl__get_data_type_size2(plDataType tType)
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

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_renderer_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plRendererI tApi = {
        .initialize                    = pl_refr_initialize,
        .cleanup                       = pl_refr_cleanup,
        .create_scene                  = pl_refr_create_scene,
        .add_drawable_objects_to_scene = pl_add_drawable_objects_to_scene,
        .create_view                   = pl_refr_create_view,
        .run_ecs                       = pl_refr_run_ecs,
        .begin_frame                   = pl_refr_begin_frame,
        .end_frame                     = pl_refr_end_frame,
        .get_component_library         = pl_refr_get_component_library,
        .get_device                    = pl_refr_get_device,
        .get_swapchain                 = pl_refr_get_swapchain,
        .load_skybox_from_panorama     = pl_refr_load_skybox_from_panorama,
        .finalize_scene                = pl_refr_finalize_scene,
        .select_entities               = pl_refr_select_entities,
        .render_scene                  = pl_refr_render_scene,
        .get_view_color_texture        = pl_refr_get_view_color_texture,
        .resize_view                   = pl_refr_resize_view,
        .get_debug_drawlist            = pl_refr_get_debug_drawlist,
        .get_gizmo_drawlist            = pl_refr_get_gizmo_drawlist,
        .get_picked_entity             = pl_refr_get_picked_entity,
        .show_graphics_options         = pl_show_graphics_options,
        .get_command_pool              = pl__refr_get_command_pool,
        .resize                        = pl_refr_resize,
    };
    pl_set_api(ptApiRegistry, plRendererI, &tApi);

    // core apis
    gptDataRegistry  = pl_get_api_latest(ptApiRegistry, plDataRegistryI);
    gptIOI           = pl_get_api_latest(ptApiRegistry, plIOI);
    gptImage         = pl_get_api_latest(ptApiRegistry, plImageI);
    gptMemory        = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptGpuAllocators = pl_get_api_latest(ptApiRegistry, plGPUAllocatorsI);
    gptFile          = pl_get_api_latest(ptApiRegistry, plFileI);
    gptIO            = gptIOI->get_io();
    gptStats         = pl_get_api_latest(ptApiRegistry, plStatsI);
    gptImage         = pl_get_api_latest(ptApiRegistry, plImageI);
    gptJob           = pl_get_api_latest(ptApiRegistry, plJobI);

    // set contexts
    pl_set_profile_context(gptDataRegistry->get_data(PL_PROFILE_CONTEXT_NAME));
    pl_set_log_context(gptDataRegistry->get_data(PL_LOG_CONTEXT_NAME));

    
    gptECS         = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptCamera      = pl_get_api_latest(ptApiRegistry, plCameraI);
    gptDraw        = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptDrawBackend = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
    gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptUI          = pl_get_api_latest(ptApiRegistry, plUiI);
    gptResource    = pl_get_api_latest(ptApiRegistry, plResourceI);
    #ifdef PL_CORE_EXTENSION_INCLUDE_SHADER
        gptShader = pl_get_api_latest(ptApiRegistry, plShaderI);
    #endif

    if(bReload)
    {
        gptData = gptDataRegistry->get_data("ref renderer data");
    }   
}

PL_EXPORT void
pl_unload_renderer_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;
        
    const plRendererI* ptApi = pl_get_api_latest(ptApiRegistry, plRendererI);
    ptApiRegistry->remove_api(ptApi);
}

#ifndef PL_UNITY_BUILD

    #define PL_LOG_ALLOC(x) PL_ALLOC(x)
    #define PL_LOG_FREE(x) PL_FREE(x)
    #define PL_LOG_IMPLEMENTATION
    #include "pl_log.h"
    #undef PL_LOG_IMPLEMENTATION

    #define PL_PROFILE_ALLOC(x) PL_ALLOC(x)
    #define PL_PROFILE_FREE(x) PL_FREE(x)
    #define PL_PROFILE_IMPLEMENTATION
    #include "pl_profile.h"
    #undef PL_PROFILE_IMPLEMENTATION

    #define PL_MEMORY_IMPLEMENTATION
    #include "pl_memory.h"
    #undef PL_MEMORY_IMPLEMENTATION

    #ifdef PL_USE_STB_SPRINTF
        #define STB_SPRINTF_IMPLEMENTATION
        #include "stb_sprintf.h"
        #undef STB_SPRINTF_IMPLEMENTATION
    #endif

#endif