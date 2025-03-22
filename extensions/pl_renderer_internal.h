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
#include "pl_rect_pack_ext.h"
#include "pl_console_ext.h"
#include "pl_screen_log_ext.h"
#include "pl_physics_ext.h"

// stb
#include "stb_image_resize2.h"

#define PL_MAX_VIEWS_PER_SCENE 4
#define PL_MAX_LIGHTS 100

#ifndef PL_DEVICE_BUDDY_BLOCK_SIZE
    #define PL_DEVICE_BUDDY_BLOCK_SIZE 268435456
#endif

#define PL_MAX_BINDLESS_TEXTURES 4096

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
    static const plRectPackI*      gptRect          = NULL;
    static const plConsoleI*       gptConsole       = NULL;
    static const plPhysicsI *      gptPhysics       = NULL;
    
    // experimental
    static const plScreenLogI*  gptScreenLog  = NULL;
    static const plCameraI*     gptCamera   = NULL;
    static const plResourceI*   gptResource = NULL;
    static const plEcsI*        gptECS      = NULL;

    static struct _plIO* gptIO = 0;
#endif

#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef int plDrawableFlags;

enum _plDrawableFlags
{
    PL_DRAWABLE_FLAG_NONE     = 0,
    PL_DRAWABLE_FLAG_FORWARD  = 1 << 0,
    PL_DRAWABLE_FLAG_DEFERRED = 1 << 1,
    PL_DRAWABLE_FLAG_PROBE    = 1 << 2
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

typedef struct _plSkinData
{
    plEntity              tEntity;
    plEntity              tObjectEntity;
    plBufferHandle        atDynamicSkinBuffer[PL_MAX_FRAMES_IN_FLIGHT];
    plBindGroupHandle     atBindGroup[PL_MAX_FRAMES_IN_FLIGHT];
    plComputeShaderHandle tShader;
    uint32_t              uVertexCount;
    int                   iSourceDataOffset;
    int                   iDestDataOffset;
    int                   iDestVertexOffset;
} plSkinData;

typedef struct _plDrawable
{
    plDrawableFlags tFlags;
    plEntity        tEntity;
    uint32_t        uDataOffset;
    uint32_t        uVertexOffset;
    uint32_t        uVertexCount;
    uint32_t        uIndexOffset;
    uint32_t        uIndexCount;
    uint32_t        uMaterialIndex;
    uint32_t        uTransformIndex;
    uint32_t        uInstanceIndex;
    plShaderHandle  tShader;
    plShaderHandle  tEnvShader;
    plShaderHandle  tShadowShader;
    uint32_t        uSkinIndex;
    uint32_t        uTriangleCount;
    plBufferHandle  tIndexBuffer;
    uint32_t        uStaticVertexOffset;
    uint32_t        uDynamicVertexOffset;
    uint32_t        uInstanceCount;
    bool            bCulled;
} plDrawable;

typedef struct _plGPUProbeData
{
    plVec3 tPosition;
    float  fRangeSqr;

    uint32_t  uLambertianEnvSampler;
    uint32_t  uGGXEnvSampler;
    uint32_t  uGGXLUT;
    int       iParallaxCorrection;

    plVec4 tMin;
    plVec4 tMax;
} plGPUProbeData;

typedef struct _plGPUMaterial
{
    // Metallic Roughness
    float fMetallicFactor;
    float fRoughnessFactor;
    int _unused0[2];
    plVec4 tBaseColorFactor;

    // Emissive Strength
    plVec3 tEmissiveFactor;
    float  fEmissiveStrength;
    
    // Alpha mode
    float fAlphaCutoff;
    float fOcclusionStrength;
    int iBaseColorUVSet;
    int iNormalUVSet;

    int iEmissiveUVSet;
    int iOcclusionUVSet;
    int iMetallicRoughnessUVSet;
    int iBaseColorTexIdx;
    
    int iNormalTexIdx;
    int iEmissiveTexIdx;
    int iMetallicRoughnessTexIdx;
    int iOcclusionTexIdx;
} plGPUMaterial;

typedef struct _plGPULight
{
    plVec3 tPosition;
    float  fIntensity;

    plVec3 tDirection;
    float fInnerConeCos;

    plVec3 tColor;
    float  fRange;

    int iShadowIndex;
    int iCascadeCount;
    int iCastShadow;
    float fOuterConeCos;

    int iType;
    int _unused[3];
} plGPULight;

typedef struct _plGPULightShadowData
{
    plVec4 tCascadeSplits;
	plMat4 viewProjMat[6];
    int iShadowMapTexIdx;
    float fFactor;
    float fXOffset;
    float fYOffset;
} plGPULightShadowData;

typedef struct _BindGroup_0
{
    plVec4 tViewportSize;
    plVec4 tViewportInfo;
    plVec4 tCameraPos;
    plMat4 tCameraView;
    plMat4 tCameraProjection;   
    plMat4 tCameraViewProjection;
} BindGroup_0;

typedef struct _DynamicData
{
    int      iDataOffset;
    int      iVertexOffset;
    int      iMaterialOffset;
    uint32_t uGlobalIndex;
} DynamicData;

typedef struct _plShadowInstanceBufferData
{
    uint32_t uTransformIndex;
    int32_t  iViewportIndex;
} plShadowInstanceBufferData;

typedef struct _plShadowDynamicData
{
    int    iIndex;
    int    iDataOffset;
    int    iVertexOffset;
    int    iMaterialIndex;
} plShadowDynamicData;

typedef struct _plSkyboxDynamicData
{
    uint32_t uGlobalIndex;
    uint32_t _auUnused[3];
    plMat4 tModel;
} plSkyboxDynamicData;

typedef struct _plDirectionLightShadowData
{
    plBufferHandle        atDShadowCameraBuffers[PL_MAX_FRAMES_IN_FLIGHT];
    plBufferHandle        atDLightShadowDataBuffer[PL_MAX_FRAMES_IN_FLIGHT];
    plGPULightShadowData* sbtDLightShadowData;
    uint32_t              uOffset;
    uint32_t              uOffsetIndex;
} plDirectionLightShadowData;

typedef struct _plEnvironmentProbeData
{
    plEntity tEntity;
    plVec2 tTargetSize;

    plRenderPassHandle atRenderPasses[6];

    // g-buffer textures
    plTextureHandle tAlbedoTexture;
    plTextureHandle tNormalTexture;
    plTextureHandle tAOMetalRoughnessTexture;
    plTextureHandle tRawOutputTexture;
    plTextureHandle tDepthTexture;

    // views
    plTextureHandle atAlbedoTextureViews[6];
    plTextureHandle atNormalTextureViews[6];
    plTextureHandle atAOMetalRoughnessTextureViews[6];
    plTextureHandle atRawOutputTextureViews[6];
    plTextureHandle atDepthTextureViews[6];

    // lighting
    plBindGroupHandle atLightingBindGroup[6];

    // GPU buffers
    plBufferHandle atGlobalBuffers[PL_MAX_FRAMES_IN_FLIGHT];

    // submitted drawables
    plDrawable* sbtVisibleOpaqueDrawables[6];
    plDrawable* sbtVisibleTransparentDrawables[6];

    // shadows
    plDirectionLightShadowData tDirectionLightShadowData;

    // textures
    plTextureHandle   tGGXLUTTexture;
    uint32_t          uGGXLUT;
    plTextureHandle   tLambertianEnvTexture;
    uint32_t          uLambertianEnvSampler;
    plTextureHandle   tGGXEnvTexture;
    uint32_t          uGGXEnvSampler;

    // intervals
    uint32_t uCurrentFace;
} plEnvironmentProbeData;

typedef struct _plRefView
{
    // renderpasses
    plRenderPassHandle tRenderPass;
    plRenderPassHandle tPostProcessRenderPass;
    plRenderPassHandle tPickRenderPass;
    plRenderPassHandle tUVRenderPass;
    plVec2             tTargetSize;

    // g-buffer textures
    plTextureHandle tAlbedoTexture;
    plTextureHandle tNormalTexture;
    plTextureHandle tAOMetalRoughnessTexture;
    plTextureHandle tRawOutputTexture;
    plTextureHandle tDepthTexture;

    // picking
    bool              auHoverResultProcessing[PL_MAX_FRAMES_IN_FLIGHT];
    bool              auHoverResultReady[PL_MAX_FRAMES_IN_FLIGHT];
    bool              bRequestHoverCheck;
    plEntity          tHoveredEntity;
    plTextureHandle   tPickTexture;
    plBufferHandle    atPickBuffer[PL_MAX_FRAMES_IN_FLIGHT];
    plBindGroupHandle atPickBindGroup[PL_MAX_FRAMES_IN_FLIGHT];

    // outlining
    plTextureHandle atUVMaskTexture0;
    plTextureHandle atUVMaskTexture1;
    plTextureHandle tLastUVMask;
    
    // output texture
    plTextureHandle   tFinalTexture;
    plBindGroupHandle tFinalTextureHandle;

    // lighting
    plBindGroupHandle tLightingBindGroup;

    // GPU buffers
    plBufferHandle atGlobalBuffers[PL_MAX_FRAMES_IN_FLIGHT];

    // submitted drawables
    uint32_t* sbtVisibleDrawables;
    uint32_t* sbtVisibleOpaqueDrawables;
    uint32_t* sbtVisibleTransparentDrawables;

    // drawing api
    plDrawList3D* pt3DGizmoDrawList;
    plDrawList3D* pt3DDrawList;
    plDrawList3D* pt3DSelectionDrawList;

    // shadows
    plDirectionLightShadowData tDirectionLightShadowData;
} plRefView;

typedef struct _plRefScene
{
    bool           bActive;
    plShaderHandle tLightingShader;
    plShaderHandle tEnvLightingShader;
    plShaderHandle tTonemapShader;

    // skybox resources (optional)
    plDrawable        tSkyboxDrawable;
    plTextureHandle   tSkyboxTexture;
    plBindGroupHandle tSkyboxBindGroup;
    bool              bShowSkybox;
        
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
    plBufferHandle tSkinStorageBuffer;
    plBufferHandle atLightBuffer[PL_MAX_FRAMES_IN_FLIGHT];
    plBufferHandle atTransformBuffer[PL_MAX_FRAMES_IN_FLIGHT];
    plBufferHandle atInstanceBuffer[PL_MAX_FRAMES_IN_FLIGHT];

    // GPU materials
    uint32_t       uGPUMaterialDirty;
    uint32_t       uGPUMaterialBufferCapacity;
    plBufferHandle atMaterialDataBuffer[PL_MAX_FRAMES_IN_FLIGHT];

    // views
    uint32_t    uViewCount;
    plRefView   atViews[PL_MAX_VIEWS_PER_SCENE];
    plSkinData* sbtSkinData;

    // shadow atlas
    plPackRect*        sbtShadowRects;
    plRenderPassHandle tFirstShadowRenderPass;
    plRenderPassHandle tShadowRenderPass;
    uint32_t           uShadowAtlasResolution;
    plTextureHandle    tShadowTexture;
    uint32_t           atShadowTextureBindlessIndices;

    // ECS component library
    plComponentLibrary tComponentLibrary;

    // drawables (per scene, will be culled by views)

    plDrawable* sbtStagedDrawables; // unprocessed

    plDrawable* sbtDrawables; // regular rendering

    uint32_t* sbuProbeDrawables;

    uint32_t* sbuShadowDeferredDrawables; // shadow rendering (index into regular drawables)
    uint32_t* sbuShadowForwardDrawables;  // shadow rendering (index into regular drawables)

    plDrawable* sbtOutlineDrawables;
    plShaderHandle* sbtOutlineDrawablesOldShaders;
    plShaderHandle* sbtOutlineDrawablesOldEnvShaders;

    // entity to drawable hashmaps
    plHashMap* ptDrawableHashmap;

    // bindless texture system
    uint32_t          uTextureIndexCount;
    uint32_t          uCubeTextureIndexCount;
    plHashMap*        ptTextureIndexHashmap; // texture handle <-> index
    plHashMap*        ptCubeTextureIndexHashmap; // texture handle <-> index
    plBindGroupHandle atGlobalBindGroup[PL_MAX_FRAMES_IN_FLIGHT];

    // material hashmaps (material component <-> GPU material)
    plMaterialComponent* sbtMaterials;
    plHashMap* ptMaterialHashmap;

    // shadows
    plBufferHandle atShadowCameraBuffers[PL_MAX_FRAMES_IN_FLIGHT];
    plBufferHandle atLightShadowDataBuffer[PL_MAX_FRAMES_IN_FLIGHT];
    plGPULightShadowData* sbtLightShadowData;
    uint32_t              uShadowOffset;
    uint32_t              uShadowIndex;
    uint32_t              uDShadowOffset;
    uint32_t              uDShadowIndex;

    // environment probes
    plEntity tProbeMesh;
    plEnvironmentProbeData* sbtProbeData;
    plGPUProbeData* sbtGPUProbeData;
    plBufferHandle atGPUProbeDataBuffers[PL_MAX_FRAMES_IN_FLIGHT];
    plBufferHandle atFilterWorkingBuffers[7];

    // transforms
    uint32_t uNextTransformIndex;
    // uint32_t* sbuFreeTransformIndices;
} plRefScene;

typedef struct _plRefRendererData
{
    plDevice* ptDevice;
    plDeviceInfo tDeviceInfo;
    plSwapchain* ptSwap;
    plSurface* ptSurface;
    plTempAllocator tTempAllocator;
    uint32_t uMaxTextureResolution;

    // main render pass stuff
    plRenderPassHandle       tCurrentMainRenderPass;
    plRenderPassHandle       tMainRenderPass;
    plRenderPassLayoutHandle tMainRenderPassLayout;
    plRenderPassHandle       tMainMSAARenderPass;
    plRenderPassLayoutHandle tMainMSAARenderPassLayout;
    plTextureHandle          tMSAATexture;

    // bind groups
    plBindGroupPool* ptBindGroupPool;
    plBindGroupPool* aptTempGroupPools[PL_MAX_FRAMES_IN_FLIGHT];

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
    plShaderHandle        tShadowShader;
    plShaderHandle        tAlphaShadowShader;
    plShaderHandle        tDeferredShader;
    plShaderHandle        tForwardShader;
    plShaderHandle        tSkyboxShader;
    plShaderHandle        tPickShader;
    plComputeShaderHandle tEnvFilterShader;

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
    plDeviceMemoryAllocatorI* ptStagingUnCachedBuddyAllocator;
    plDeviceMemoryAllocatorI* ptStagingCachedAllocator;

    // default textures & samplers & bindgroups
    plSamplerHandle tDefaultSampler;
    plSamplerHandle tSkyboxSampler;
    plSamplerHandle tShadowSampler;
    plSamplerHandle tEnvSampler;
    plTextureHandle tDummyTexture;
    plTextureHandle tDummyTextureCube;

    // scenes
    plRefScene* sbtScenes;
    uint32_t*   sbuSceneFreeIndices;

    // draw stream data
    plDrawStream tDrawStream;

    // staging (more robust system should replace this)
    plBufferHandle tStagingBufferHandle[PL_MAX_FRAMES_IN_FLIGHT];
    uint32_t uStagingOffset;

    // sync
    plTimelineSemaphore* aptSemaphores[PL_MAX_FRAMES_IN_FLIGHT];
    uint64_t aulNextTimelineValue[PL_MAX_FRAMES_IN_FLIGHT];
    plTimelineSemaphore* ptClickSemaphore;
    uint64_t ulSemClickNextValue;

    // command pools
    plCommandPool* atCmdPools[PL_MAX_FRAMES_IN_FLIGHT];

    // dynamic buffer system
    plDynamicDataBlock tCurrentDynamicDataBlock;

    // texture lookup (resource handle <-> texture handle)
    plTextureHandle*  sbtTextureHandles;
    plHashMap*        ptTextureHashmap;

    // graphics options
    bool     bMSAA;
    bool     bShowProbes;
    bool     bWireframe;
    bool     bReloadSwapchain;
    bool     bReloadMSAA;
    bool     bVSync;
    bool     bShowOrigin;
    bool     bFrustumCulling;
    bool     bDrawAllBoundingBoxes;
    bool     bDrawVisibleBoundingBoxes;
    bool     bShowSelectedBoundingBox;
    bool     bMultiViewportShadows;
    bool     bImageBasedLighting;
    bool     bPunctualLighting;
    float    fShadowConstantDepthBias;
    float    fShadowSlopeDepthBias;
    uint32_t uOutlineWidth;

    // stats
    double* pdDrawCalls;
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
    PL_RENDERING_FLAG_USE_IBL      = 1 << 1,
    PL_RENDERING_FLAG_SHADOWS      = 1 << 2
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
static void pl__refr_cull_job(plInvocationData, void*);

// resource creation helpers
static plTextureHandle pl__create_texture_helper            (plMaterialComponent*, plTextureSlot, bool bHdr, int iMips);
static plTextureHandle pl__refr_create_texture              (const plTextureDesc* ptDesc, const char* pcName, uint32_t uIdentifier, plTextureUsage tInitialUsage);
static plTextureHandle pl__refr_create_local_texture        (const plTextureDesc* ptDesc, const char* pcName, uint32_t uIdentifier, plTextureUsage tInitialUsage);
static plTextureHandle pl__refr_create_texture_with_data    (const plTextureDesc* ptDesc, const char* pcName, uint32_t uIdentifier, const void* pData, size_t szSize);
static plBufferHandle  pl__refr_create_staging_buffer       (const plBufferDesc* ptDesc, const char* pcName, uint32_t uIdentifier);
static plBufferHandle  pl__refr_create_cached_staging_buffer(const plBufferDesc* ptDesc, const char* pcName, uint32_t uIdentifier);
static plBufferHandle  pl__refr_create_local_buffer         (const plBufferDesc* ptDesc, const char* pcName, uint32_t uIdentifier, const void* pData, size_t szSize);

// culling
static bool pl__sat_visibility_test(plCameraComponent*, const plAABB*);

// scene render helpers
static void pl_refr_perform_skinning(plCommandBuffer*, uint32_t);
static bool pl_refr_pack_shadow_atlas(uint32_t uSceneHandle, const uint32_t* auViewHandles, uint32_t uViewCount);
static void pl_refr_generate_cascaded_shadow_map(plRenderEncoder*, plCommandBuffer*, uint32_t, uint32_t, uint32_t, int, plDirectionLightShadowData*, plCameraComponent*);
static void pl_refr_generate_shadow_maps(plRenderEncoder*, plCommandBuffer*, uint32_t);
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
static uint32_t                pl__get_bindless_texture_index(uint32_t uSceneHandle, plTextureHandle);
static uint32_t                pl__get_bindless_cube_texture_index(uint32_t uSceneHandle, plTextureHandle);

// drawable ops
static void pl__refr_add_skybox_drawable (uint32_t uSceneHandle);
static void pl__refr_unstage_drawables   (uint32_t uSceneHandle);
static void pl__refr_set_drawable_shaders(uint32_t uSceneHandle);
static void pl__refr_sort_drawables      (uint32_t uSceneHandle);

// environment probes
static void pl__create_probe_data(uint32_t uSceneHandle, plEntity tProbeHandle);
static uint64_t pl__update_environment_probes(uint32_t uSceneHandle, uint64_t ulValue);
static uint64_t pl_create_environment_map_from_texture(uint32_t uSceneHandle, plEnvironmentProbeData* ptProbe, uint64_t ulValue);


#endif // PL_RENDERER_INTERNAL_EXT_H