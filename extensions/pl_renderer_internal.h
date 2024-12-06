/*
   pl_renderer_internal_ext.h
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal structs
// [SECTION] internal enums
// [SECTION] global data
// [SECTION] internal API
// [SECTION] implementation
// [SECTION] internal API implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_RENDERER_INTERNAL_EXT_H
#define PL_RENDERER_INTERNAL_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <float.h> // FLT_MAX
#include "pl.h"
#include "pl_renderer_ext.h"
#include "pl_ds.h"
#include "pl_string.h"
#include "pl_memory.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extensions
#include "pl_graphics_ext.h"
#include "pl_profile_ext.h"
#include "pl_log_ext.h"
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
    static const plProfileI*       gptProfile       = NULL;
    static const plLogI*           gptLog           = NULL;
    
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
    plVec4 tViewportSize;
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
    plDrawable* sbtDeferredDrawables;
    plDrawable* sbtForwardDrawables;
    plDrawable* sbtOutlineDrawables;
    plShaderHandle* sbtOutlineDrawablesOldShaders;

    // entity to drawable hashmaps
    plHashMap* ptDeferredHashmap;
    plHashMap* ptForwardHashmap;

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
    plBindGroupPool* aptTempGroupPools[PL_MAX_FRAMES_IN_FLIGHT];

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
    plShaderHandle tDeferredShader;
    plShaderHandle tForwardShader;
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

typedef struct _plCullData
{
    plRefScene* ptScene;
    plCameraComponent* ptCullCamera;
    plDrawable* atDrawables;
} plCullData;

typedef struct _plMemCpyJobData
{
    plBuffer* ptBuffer;
    void*     pDestination;
    size_t    szSize;
} plMemCpyJobData;

//-----------------------------------------------------------------------------
// [SECTION] internal enums
//-----------------------------------------------------------------------------

enum _plTextureMappingFlags
{
    PL_HAS_BASE_COLOR_MAP         = 1 << 0,
    PL_HAS_NORMAL_MAP             = 1 << 1,
    PL_HAS_EMISSIVE_MAP           = 1 << 2,
    PL_HAS_OCCLUSION_MAP          = 1 << 3,
    PL_HAS_METALLIC_ROUGHNESS_MAP = 1 << 4
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
// [SECTION] global data
//-----------------------------------------------------------------------------

// context data
static plRefRendererData* gptData = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal API
//-----------------------------------------------------------------------------

// job system tasks
static void pl__refr_job     (plInvocationData, void*);
static void pl__refr_cull_job(plInvocationData, void*);

// resource creation helpers
static plTextureHandle pl__create_texture_helper            (plMaterialComponent*, plTextureSlot, bool bHdr, int iMips);
static plTextureHandle pl__refr_create_texture              (const plTextureDesc* ptDesc, const char* pcName, uint32_t uIdentifier, plTextureUsage tInitialUsage);
static plTextureHandle pl__refr_create_texture_with_data    (const plTextureDesc* ptDesc, const char* pcName, uint32_t uIdentifier, const void* pData, size_t szSize);
static plBufferHandle  pl__refr_create_staging_buffer       (const plBufferDesc* ptDesc, const char* pcName, uint32_t uIdentifier);
static plBufferHandle  pl__refr_create_cached_staging_buffer(const plBufferDesc* ptDesc, const char* pcName, uint32_t uIdentifier);
static plBufferHandle  pl__refr_create_local_buffer         (const plBufferDesc* ptDesc, const char* pcName, uint32_t uIdentifier, const void* pData);

// culling
static bool pl__sat_visibility_test(plCameraComponent*, const plAABB*);

// scene render helpers
static void pl_refr_update_skin_textures(plCommandBuffer*, uint32_t);
static void pl_refr_perform_skinning(plCommandBuffer*, uint32_t);
static void pl_refr_generate_cascaded_shadow_map(plCommandBuffer*, uint32_t, uint32_t, plEntity tCamera, plEntity tLight, float fCascadeSplitLambda);
static void pl_refr_post_process_scene(plCommandBuffer*, uint32_t, uint32_t, const plMat4*);

// shader variant system
static plShaderHandle pl__get_shader_variant(uint32_t, plShaderHandle, const plShaderVariant*);

// misc
static inline plDynamicBinding pl__allocate_dynamic_data(plDevice* ptDevice){ return pl_allocate_dynamic_data(gptGfx, gptData->ptDevice, &gptData->tCurrentDynamicDataBlock);}
static void                    pl__add_drawable_skin_data_to_global_buffer(plRefScene*, uint32_t uDrawableIndex, plDrawable* atDrawables);
static void                    pl__add_drawable_data_to_global_buffer(plRefScene*, uint32_t uDrawableIndex, plDrawable* atDrawables);
static void                    pl_refr_create_global_shaders(void);
static size_t                  pl__get_data_type_size2(plDataType tType);
static plBlendState            pl__get_blend_state(plBlendMode tBlendMode);


#endif // PL_RENDERER_INTERNAL_EXT_H