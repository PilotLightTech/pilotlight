/*
   pl_renderer_internal.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] defines
// [SECTION] global APIs
// [SECTION] forward declarations
// [SECTION] enums
// [SECTION] structs
// [SECTION] global data
// [SECTION] internal API
// [SECTION] implementation
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_RENDERER_INTERNAL_H
#define PL_RENDERER_INTERNAL_H

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
#include "pl_animation_ext.h"
#include "pl_ecs_ext.h"
#include "pl_mesh_ext.h"
#include "pl_camera_ext.h"
#include "pl_resource_ext.h"
#include "pl_image_ext.h"
#include "pl_stats_ext.h"
#include "pl_gpu_allocators_ext.h"
#include "pl_job_ext.h"
#include "pl_draw_ext.h"
#include "pl_draw_backend_ext.h"
#include "pl_ui_ext.h"
#include "pl_shader_ext.h"
#include "pl_rect_pack_ext.h"
#include "pl_console_ext.h"
#include "pl_screen_log_ext.h"
#include "pl_bvh_ext.h"
#include "pl_shader_variant_ext.h"
#include "pl_vfs_ext.h"
#include "pl_starter_ext.h"

// shader interop
#include "pl_shader_interop_renderer.h"

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#define PL_MAX_LIGHTS 100

//-----------------------------------------------------------------------------
// [SECTION] global APIs
//-----------------------------------------------------------------------------

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
    static const plIOI*            gptIOI           = NULL;
    static const plShaderI*        gptShader        = NULL;
    static const plProfileI*       gptProfile       = NULL;
    static const plLogI*           gptLog           = NULL;
    static const plRectPackI*      gptRect          = NULL;
    static const plConsoleI*       gptConsole       = NULL;
    static const plVfsI*           gptVfs           = NULL;
    static const plStarterI*       gptStarter       = NULL;

    // experimental
    static const plScreenLogI*     gptScreenLog     = NULL;
    static const plCameraI*        gptCamera        = NULL;
    static const plResourceI*      gptResource      = NULL;
    static const plEcsI*           gptECS           = NULL;
    static const plBVHI*           gptBvh           = NULL;
    static const plAnimationI*     gptAnimation     = NULL;
    static const plMeshI*          gptMesh          = NULL;
    static const plShaderVariantI* gptShaderVariant = NULL;

    static struct _plIO* gptIO = 0;
#endif

#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plView                  plView;
typedef struct _plScene                 plScene;
typedef struct _plRefRendererData       plRefRendererData;
typedef struct _plSkinData              plSkinData;
typedef struct _plDrawable              plDrawable;
typedef struct _plRendererStagingBuffer plRendererStagingBuffer;
typedef struct _plCullData              plCullData;
typedef struct _plMemCpyJobData         plMemCpyJobData;
typedef struct _plOBB                   plOBB;
typedef struct _plEnvironmentProbeData  plEnvironmentProbeData;

// shader variants
typedef struct _plShaderVariant plShaderVariant;
typedef struct _plComputeShaderVariant plComputeShaderVariant;

// gpu buffers
typedef struct _plDirectionLightShadowData plDirectionLightShadowData;

// enums & flags
typedef int plDrawableFlags;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plDrawableFlags
{
    PL_DRAWABLE_FLAG_NONE     = 0,
    PL_DRAWABLE_FLAG_FORWARD  = 1 << 0,
    PL_DRAWABLE_FLAG_DEFERRED = 1 << 1,
    PL_DRAWABLE_FLAG_PROBE    = 1 << 2
};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plOBB
{
    plVec3 tCenter;
    plVec3 tExtents;
    plVec3 atAxes[3]; // Orthonormal basis
} plOBB;

typedef struct _plRendererStagingBuffer
{
    plBufferHandle tStagingBufferHandle;
    size_t         szSize;
    size_t         szOffset;
    double         dLastTimeActive;
} plRendererStagingBuffer;

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

typedef struct _plDirectionLightShadowData
{
    plBufferHandle     atDShadowCameraBuffers[PL_MAX_FRAMES_IN_FLIGHT];
    plBufferHandle     atDLightShadowDataBuffer[PL_MAX_FRAMES_IN_FLIGHT];
    plGpuLightShadow* sbtDLightShadowData;
    uint32_t           uOffset;
    uint32_t           uOffsetIndex;
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
    uint32_t* sbuVisibleDeferredEntities[6];
    uint32_t* sbuVisibleForwardEntities[6];

    // shadows
    plDirectionLightShadowData tDirectionLightShadowData;

    // textures
    plTextureHandle tGGXLUTTexture;
    uint32_t        uGGXLUT;
    plTextureHandle tLambertianEnvTexture;
    uint32_t        uLambertianEnvSampler;
    plTextureHandle tGGXEnvTexture;
    uint32_t        uGGXEnvSampler;

    // intervals
    uint32_t uCurrentFace;
} plEnvironmentProbeData;

typedef struct _plView
{
    plScene* ptParentScene;

    bool bShowSkybox;

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
    plVec2            tHoverOffset;
    plVec2            tHoverWindowRatio;
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
    uint32_t* sbuVisibleDeferredEntities;
    uint32_t* sbuVisibleForwardEntities;

    // drawing api
    plDrawList3D* pt3DGizmoDrawList;
    plDrawList3D* pt3DDrawList;
    plDrawList3D* pt3DSelectionDrawList;

    // shadows
    plDirectionLightShadowData tDirectionLightShadowData;
} plView;

typedef struct _plScene
{
    const char* pcName;
    bool           bActive;
    plShaderHandle tLightingShader;
    plShaderHandle tEnvLightingShader;
    uint64_t uLastSemValueForShadow;
    uint64_t aulStartTimelineValue[PL_MAX_FRAMES_IN_FLIGHT];

    // skybox resources (optional)
    plDrawable        tSkyboxDrawable;
    plTextureHandle   tSkyboxTexture;
    plBindGroupHandle tSkyboxBindGroup;
        
    // shared bind groups
    plBindGroupHandle tSkinBindGroup0;

    // CPU buffers
    plVec3*        sbtVertexPosBuffer;
    plVec4*        sbtVertexDataBuffer;
    uint32_t*      sbuIndexBuffer;
    plGpuMaterial* sbtMaterialBuffer;
    plVec4*        sbtSkinVertexDataBuffer;
    plGpuLight*   sbtLightData;

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
    plView**    sbptViews;
    plSkinData* sbtSkinData;

    // shadow atlas
    plPackRect*        sbtShadowRects;
    plRenderPassHandle tFirstShadowRenderPass;
    plRenderPassHandle tShadowRenderPass;
    uint32_t           uShadowAtlasResolution;
    plTextureHandle    tShadowTexture;
    uint32_t           atShadowTextureBindlessIndices;

    // ECS component library
    plComponentLibrary* ptComponentLibrary;

    // drawables (per scene, will be culled by views)

    plEntity* sbtStagedEntities; // unprocessed

    plDrawable* sbtDrawables; // regular rendering

    uint32_t* sbuProbeDrawables;

    uint32_t* sbuShadowDeferredDrawables; // shadow rendering (index into regular drawables)
    uint32_t* sbuShadowForwardDrawables;  // shadow rendering (index into regular drawables)

    plEntity*       sbtOutlinedEntities;
    plShaderHandle* sbtOutlineDrawablesOldShaders;
    plShaderHandle* sbtOutlineDrawablesOldEnvShaders;

    // entity to drawable hashmaps
    plHashMap64 tDrawableHashmap;

    // bindless texture system
    uint32_t          uTextureIndexCount;
    uint32_t          uCubeTextureIndexCount;
    plHashMap64       tTextureIndexHashmap; // texture handle <-> index
    plHashMap64       tCubeTextureIndexHashmap; // texture handle <-> index
    plBindGroupHandle atGlobalBindGroup[PL_MAX_FRAMES_IN_FLIGHT];

    // material hashmaps (material component <-> GPU material)
    plMaterialComponent* sbtMaterials;
    plHashMap64 tMaterialHashmap;

    // shadows
    plBufferHandle atShadowCameraBuffers[PL_MAX_FRAMES_IN_FLIGHT];
    plBufferHandle atLightShadowDataBuffer[PL_MAX_FRAMES_IN_FLIGHT];
    plGpuLightShadow* sbtLightShadowData;
    uint32_t              uShadowOffset;
    uint32_t              uShadowIndex;
    uint32_t              uDShadowOffset;
    uint32_t              uDShadowIndex;

    // environment probes
    plEntity tProbeMesh;
    plEnvironmentProbeData* sbtProbeData;
    plGpuProbe* sbtGPUProbeData;
    plBufferHandle atGPUProbeDataBuffers[PL_MAX_FRAMES_IN_FLIGHT];
    plBufferHandle atFilterWorkingBuffers[7]; // used for runtime filtering

    // transforms
    uint32_t uNextTransformIndex;
    // uint32_t* sbuFreeTransformIndices;

    // bvh
    plBVH tBvh;
    plAABB* sbtBvhAABBs;
    plBVHNode** sbtNodeStack;
} plScene;

typedef struct _plRefRendererData
{
    plDevice* ptDevice;
    plDeviceInfo tDeviceInfo;
    plSwapchain* ptSwap;
    // plSurface* ptSurface;
    plTempAllocator tTempAllocator;
    uint32_t uMaxTextureResolution;

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

    // bind group layouts
    plBindGroupLayoutHandle tSceneBGLayout;
    plBindGroupLayoutHandle tShadowGlobalBGLayout;

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

    plScene** sbptScenes;

    // draw stream data
    plDrawStream tDrawStream;

    // staging (more robust system should replace this)
    plRendererStagingBuffer atStagingBufferHandle[PL_MAX_FRAMES_IN_FLIGHT];
    // plBufferHandle tStagingBufferHandle[PL_MAX_FRAMES_IN_FLIGHT];
    // uint32_t uStagingOffset;

    // sync
    plTimelineSemaphore* ptClickSemaphore;
    uint64_t ulSemClickNextValue;

    // dynamic buffer system
    plDynamicDataBlock tCurrentDynamicDataBlock;

    // graphics options
    plRendererRuntimeOptions tRuntimeOptions;

    // stats
    double* pdDrawCalls;

    // ecs
    plEcsTypeKey tMaterialComponentType;
    plEcsTypeKey tSkinComponentType;
    plEcsTypeKey tLightComponentType;
    plEcsTypeKey tEnvironmentProbeComponentType;
    plEcsTypeKey tObjectComponentType;
} plRefRendererData;

typedef struct _plCullData
{
    plScene* ptScene;
    plCamera* ptCullCamera;
    plDrawable* atDrawables;
} plCullData;

typedef struct _plMemCpyJobData
{
    plBuffer* ptBuffer;
    void*     pDestination;
    size_t    szSize;
} plMemCpyJobData;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

// context data
static plRefRendererData* gptData = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal API
//-----------------------------------------------------------------------------

// job system tasks
static void pl__renderer_cull_job(plInvocationData, void*, void*);

// resource creation helpers
static plTextureHandle pl__renderer_create_texture              (const plTextureDesc*, const char* pcName, uint32_t uIdentifier, plTextureUsage tInitialUsage);
static plTextureHandle pl__renderer_create_local_texture        (const plTextureDesc*, const char* pcName, uint32_t uIdentifier, plTextureUsage tInitialUsage);
static plTextureHandle pl__renderer_create_texture_with_data    (const plTextureDesc*, const char* pcName, uint32_t uIdentifier, const void*, size_t);
static plBufferHandle  pl__renderer_create_staging_buffer       (const plBufferDesc*, const char* pcName, uint32_t uIdentifier);
static plBufferHandle  pl__renderer_create_cached_staging_buffer(const plBufferDesc*, const char* pcName, uint32_t uIdentifier);
static plBufferHandle  pl__renderer_create_local_buffer         (const plBufferDesc*, const char* pcName, uint32_t uIdentifier, const void*, size_t);

// culling
static bool pl__renderer_sat_visibility_test(plCamera*, const plAABB*);

// scene render helpers
static void pl__renderer_perform_skinning(plCommandBuffer*, plScene*);
static bool pl__renderer_pack_shadow_atlas(plScene*);
static void pl__renderer_generate_cascaded_shadow_map(plRenderEncoder*, plCommandBuffer*, plScene*, uint32_t, uint32_t, int, plDirectionLightShadowData*, plCamera*);
static void pl__renderer_generate_shadow_maps(plRenderEncoder*, plCommandBuffer*, plScene*);
static void pl__renderer_post_process_scene(plCommandBuffer*, plView*, const plMat4*);

// misc
static inline plDynamicBinding pl__allocate_dynamic_data(plDevice* ptDevice){ return pl_allocate_dynamic_data(gptGfx, gptData->ptDevice, &gptData->tCurrentDynamicDataBlock);}
static void                    pl__renderer_add_drawable_skin_data_to_global_buffers(plScene*, uint32_t uDrawableIndex);
static void                    pl__renderer_add_drawable_data_to_global_buffer(plScene*, uint32_t uDrawableIndex);
static plBlendState            pl__renderer_get_blend_state(plBlendMode tBlendMode);
static uint32_t                pl__renderer_get_bindless_texture_index(plScene*, plTextureHandle);
static uint32_t                pl__renderer_get_bindless_cube_texture_index(plScene*, plTextureHandle);

// drawable ops
static void pl__renderer_add_skybox_drawable(plScene*);

// accomplishes:
//   * fills CPU side vertex/index/data buffers
//   * marks probes
//   * marks deferred & forward
//   * assigns transform buffer indices & instancing stuff
//   * creates skybox drawable
//   * creates GPU buffers
//   * setups up skinning stuff
static void pl__renderer_unstage_drawables(plScene*);

// accomplishes:
//   * assigns correct shaders & shader variants
static void pl__renderer_set_drawable_shaders(plScene*);

// accomplishes:
//   * assigns to correct shadow drawables
//   * checks if index buffer is used
static void pl__renderer_sort_drawables(plScene*);

// environment probes
static void pl__renderer_create_probe_data(plScene*, plEntity tProbeHandle);
static void pl__renderer_update_probes(plScene*);
static void pl__renderer_create_environment_map_from_texture(plScene*, plEnvironmentProbeData* ptProbe);

#endif // PL_RENDERER_INTERNAL_H