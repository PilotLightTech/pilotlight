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
#include "pl_ui_ext.h"
#include "pl_shader_ext.h"
#include "pl_rect_pack_ext.h"
#include "pl_console_ext.h"
#include "pl_screen_log_ext.h"
#include "pl_bvh_ext.h"
#include "pl_shader_variant_ext.h"
#include "pl_vfs_ext.h"
#include "pl_starter_ext.h"
#include "pl_material_ext.h"
#include "pl_terrain_ext.h"
#include "pl_terrain_processor_ext.h"
#include "pl_stage_ext.h"
#include "pl_freelist_ext.h"
#include "pl_gjk_ext.h"

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
    static const plIOI*            gptIOI           = NULL;
    static const plShaderI*        gptShader        = NULL;
    static const plProfileI*       gptProfile       = NULL;
    static const plLogI*           gptLog           = NULL;
    static const plRectPackI*      gptRect          = NULL;
    static const plConsoleI*       gptConsole       = NULL;
    static const plVfsI*           gptVfs           = NULL;
    static const plStarterI*       gptStarter       = NULL;
    static const plScreenLogI*     gptScreenLog     = NULL;
    static const plResourceI*      gptResource      = NULL;
    static const plEcsI*           gptECS           = NULL;

    static struct _plIO* gptIO = 0;

    // experimental
    static const plCameraI*           gptCamera           = NULL;
    static const plBVHI*              gptBvh              = NULL;
    static const plAnimationI*        gptAnimation        = NULL;
    static const plMeshI*             gptMesh             = NULL;
    static const plShaderVariantI*    gptShaderVariant    = NULL;
    static const plMaterialI*         gptMaterial         = NULL;
    static const plTerrainI*          gptTerrain          = NULL;
    static const plTerrainProcessorI* gptTerrainProcessor = NULL;
    static const plStageI*            gptStage            = NULL;
    static const plFreeListI*         gptFreeList         = NULL;
    static const plGjkI*              gptGjk              = NULL;

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
typedef struct _plCullData              plCullData;
typedef struct _plMemCpyJobData         plMemCpyJobData;
typedef struct _plOBB                   plOBB;
typedef struct _plEnvironmentProbeData  plEnvironmentProbeData;

// shader variants
typedef struct _plShaderVariant plShaderVariant;
typedef struct _plComputeShaderVariant plComputeShaderVariant;

// enums & flags
typedef int plDrawableFlags;
typedef int plSceneInternalFlags;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plDrawableFlags
{
    PL_DRAWABLE_FLAG_NONE         = 0,
    PL_DRAWABLE_FLAG_FORWARD      = 1 << 0,
    PL_DRAWABLE_FLAG_DEFERRED     = 1 << 1,
    PL_DRAWABLE_FLAG_TRANSMISSION = 1 << 2,
    PL_DRAWABLE_FLAG_PROBE        = 1 << 3
};

enum _plSceneInternalFlags
{
    PL_SCENE_INTERNAL_FLAG_NONE                  = 0,
    PL_SCENE_INTERNAL_FLAG_ACTIVE                = 1 << 0,
    PL_SCENE_INTERNAL_FLAG_TRANSMISSION_REQUIRED = 1 << 1,
    PL_SCENE_INTERNAL_FLAG_SHEEN_REQUIRED        = 1 << 2,
    PL_SCENE_INTERNAL_FLAG_OBJECT_COUNT_DIRTY    = 1 << 3,
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

typedef struct _plRendererLight
{
    plEntity tEntity;
    uint32_t uShadowBufferOffset;
} plRendererLight;

typedef struct _plShadowPackData
{
    uint32_t uLightIndex;
    uint32_t uViewIndex;
    uint32_t uProbeIndex;
    bool     bAltMode; // alt mode is for probe
    plLightType tType;
} plShadowPackData;

typedef struct _plSkinData
{
    plEntity              tEntity;
    plEntity              tObjectEntity;
    plComputeShaderHandle tShader;
    uint32_t              uVertexCount;
    int                   iSourceDataOffset;
    int                   iDestDataOffset;
    int                   iDestVertexOffset;
    plFreeListNode*       ptFreeListNode;
} plSkinData;

typedef struct _plDrawable
{
    // hot (normal draw)
    uint32_t       uDataOffset;
    uint32_t       uStaticVertexOffset;
    uint32_t       uDynamicVertexOffset;
    uint32_t       uMaterialIndex;
    plBufferHandle tIndexBuffer;
    uint32_t       uIndexOffset;
    uint32_t       uTriangleCount;
    uint32_t       uTransformIndex;
    uint32_t       uInstanceCount;

    // cold
    plDrawableFlags tFlags;
    plEntity        tEntity;
    uint32_t        uVertexOffset;
    uint32_t        uVertexCount;
    uint32_t        uIndexCount;
    uint32_t        uInstanceIndex;
    uint32_t        uSkinIndex;
    bool            bCulled;
} plDrawable;

typedef struct _plDrawableResources
{
    plFreeListNode* ptIndexBufferNode;
    plFreeListNode* ptVertexBufferNode;
    plFreeListNode* ptDataBufferNode;
    plFreeListNode* ptSkinBufferNode;
} plDrawableResources;

typedef struct _plEnvironmentProbeData
{
    plEntity tEntity;
    plVec2 tTargetSize;

    plGpuDirectionLightShadow* sbtDLightShadowData;

    plRenderPassHandle atRenderPasses[6];
    plRenderPassHandle atTransparentRenderPasses[6];

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
    plBufferHandle tViewBuffer;
    plBufferHandle tView2Buffer;

    // submitted drawables
    uint32_t* sbuVisibleDeferredEntities[6];
    uint32_t* sbuVisibleForwardEntities[6];
    uint32_t* sbuVisibleTransmissionEntities[6];

    // textures
    plTextureHandle tLambertianEnvTexture;
    uint32_t        uLambertianEnvSampler;
    plTextureHandle tGGXEnvTexture;
    uint32_t        uGGXEnvSampler;
    plTextureHandle tSheenEnvTexture;
    uint32_t        uSheenEnvSampler;

    // bind groups
    plBindGroupHandle tViewBG;
    plBindGroupHandle tGBufferBG;
    plBindGroupHandle tDShadowBG;

    plBufferHandle tDShadowCameraBuffers;
    plBufferHandle tDLightShadowDataBuffer;

    // intervals
    uint32_t uCurrentFace;
    int iMips;
} plEnvironmentProbeData;

typedef struct _plView
{
    plScene* ptParentScene;
    uint32_t uIndex;
    plGpuViewData tData;

    plGpuDirectionLightShadow* sbtDLightShadowData;

    // per frame options (reset every frame)
    bool bShowSkybox;
    bool bShowGrid;

    // bind groups
    plBindGroupHandle atPickBindGroup[PL_MAX_FRAMES_IN_FLIGHT];
    plBindGroupHandle tLightingBindGroup;
    plBindGroupHandle atDeferredBG1[PL_MAX_FRAMES_IN_FLIGHT];
    plBindGroupHandle atJFABindGroups[2];
    plBindGroupHandle tTonemapBG;
    plBindGroupHandle atOutlineBG[2]; // indexed using "uLastUVIndex"
    uint32_t          uLastUVIndex;
    plBindGroupHandle atViewBG[PL_MAX_FRAMES_IN_FLIGHT];
    plBindGroupHandle tFinalTextureHandle; // handed out to draw result
    plBindGroupHandle atDShadowBG[PL_MAX_FRAMES_IN_FLIGHT];

    // renderpasses
    plRenderPassHandle tRenderPass;
    plRenderPassHandle tTransparentRenderPass;
    plRenderPassHandle tPostProcessRenderPass;
    plRenderPassHandle tFinalRenderPass;
    plRenderPassHandle tPickRenderPass;
    plRenderPassHandle tUVRenderPass;
    plVec2             tTargetSize;

    // textures
    plTextureHandle  tAlbedoTexture;           // g-buffer
    plTextureHandle  tNormalTexture;           // g-buffer
    plTextureHandle  tAOMetalRoughnessTexture; // g-buffer
    plTextureHandle  tRawOutputTexture;        // g-buffer
    plTextureHandle  tDepthTexture;            // g-buffer
    plTextureHandle  tTransmissionTexture;     // transmission texture
    plTextureHandle  tFinalTexture;            // output texture
    plTextureHandle  atUVMaskTexture0;         // outlining
    plTextureHandle  atUVMaskTexture1;         // outlining
    plTextureHandle  tPickTexture;             // picking
    plTextureHandle* sbtBloomDownChain;        // bloom textures
    plTextureHandle* sbtBloomUpChain;          // bloom textures

    // GPU buffers
    plBufferHandle atPickBuffer[PL_MAX_FRAMES_IN_FLIGHT];
    plBufferHandle atViewBuffers[PL_MAX_FRAMES_IN_FLIGHT];
    plBufferHandle atView2Buffers[PL_MAX_FRAMES_IN_FLIGHT];
    plBufferHandle atDShadowCameraBuffers[PL_MAX_FRAMES_IN_FLIGHT];
    plBufferHandle atDLightShadowDataBuffer[PL_MAX_FRAMES_IN_FLIGHT];

    // picking system
    bool     auHoverResultProcessing[PL_MAX_FRAMES_IN_FLIGHT];
    bool     auHoverResultReady[PL_MAX_FRAMES_IN_FLIGHT];
    bool     bRequestHoverCheck;
    plVec2   tHoverOffset;
    plVec2   tHoverWindowRatio;
    plEntity tHoveredEntity;
    
    // submitted drawables
    uint32_t* sbtVisibleDrawables;
    uint32_t* sbuVisibleDeferredEntities;
    uint32_t* sbuVisibleForwardEntities;
    uint32_t* sbuVisibleTransmissionEntities;

    // drawing api
    plDrawList3D* pt3DGizmoDrawList;
    plDrawList3D* pt3DDrawList;
    plDrawList3D* pt3DSelectionDrawList;

} plView;

typedef struct _plScene
{

    const char*          pcName;
    plSceneInternalFlags tFlags;
    plComponentLibrary*  ptComponentLibrary;
    plView**             sbptViews; // child views
    uint32_t             uDOffset; // directional light buffer offset (synced across views & probes)

    // shadow atlas
    uint32_t          uShadowAtlasIndex;
    uint32_t          uShadowAtlasResolution;
    plShadowPackData* sbtShadowRectData;
    plPackRect*       sbtShadowRects;

    // CPU buffers (temporary staging)
    uint32_t* sbuIndexBuffer;
    plVec3*   sbtVertexPosBuffer;
    plVec4*   sbtVertexDataBuffer;
    plVec4*   sbtSkinVertexDataBuffer;

    // hashmaps
    plHashMap64 tMaterialHashmap;
    plHashMap64 tTextureIndexHashmap;     // texture handle <-> index
    plHashMap64 tCubeTextureIndexHashmap; // texture handle <-> index
    plHashMap64 tDrawableHashmap;         // entity <-> drawable

    // textures
    plTextureHandle tBrdfLutTexture;
    plTextureHandle tSkyboxTexture;
    plTextureHandle tShadowTexture;

    // meshes (shared)
    plEntity tProbeMesh;
    plEntity tUnitSphereMesh;

    // shaders
    plShaderHandle tDirectionalLightingShader;
    plShaderHandle tSpotLightingShader;
    plShaderHandle tProbeLightingShader;
    plShaderHandle tPointLightingShader;

    // render passes
    plRenderPassHandle tFirstShadowRenderPass;
    plRenderPassHandle tShadowRenderPass;

    // bind groups
    plBindGroupHandle tSkyboxBindGroup;
    plBindGroupHandle tSkinBindGroup0;
    plBindGroupHandle atSkinBindGroup1[PL_MAX_FRAMES_IN_FLIGHT];
    plBindGroupHandle atShadowBG[PL_MAX_FRAMES_IN_FLIGHT];
    plBindGroupHandle atSceneBindGroups[PL_MAX_FRAMES_IN_FLIGHT];

    // GPU buffers
    plBufferHandle atSceneBuffer[PL_MAX_FRAMES_IN_FLIGHT];
    plBufferHandle atPointLightBuffer[PL_MAX_FRAMES_IN_FLIGHT];
    plBufferHandle atSpotLightBuffer[PL_MAX_FRAMES_IN_FLIGHT];
    plBufferHandle atDirectionLightBuffer[PL_MAX_FRAMES_IN_FLIGHT];
    plBufferHandle atTransformBuffer[PL_MAX_FRAMES_IN_FLIGHT];
    plBufferHandle atInstanceBuffer[PL_MAX_FRAMES_IN_FLIGHT];
    plBufferHandle atDynamicSkinBuffer[PL_MAX_FRAMES_IN_FLIGHT];
    plBufferHandle atShadowCameraBuffers[PL_MAX_FRAMES_IN_FLIGHT];
    plBufferHandle atPointLightShadowDataBuffer[PL_MAX_FRAMES_IN_FLIGHT];
    plBufferHandle atSpotLightShadowDataBuffer[PL_MAX_FRAMES_IN_FLIGHT];
    plBufferHandle tGPUProbeDataBuffers;
    plBufferHandle atFilterWorkingBuffers[7]; // used for runtime filtering
    plBufferHandle tIndexBuffer;
    plBufferHandle tVertexBuffer;
    plBufferHandle tStorageBuffer;
    plBufferHandle tMaterialDataBuffer;

    // freelists
    plFreeList tIndexBufferFreeList;
    plFreeList tVertexBufferFreeList;
    plFreeList tStorageBufferFreeList;
    plFreeList tSkinBufferFreeList;
    plFreeList tMaterialFreeList;
    plFreeList tShadowCameraFreeList;

    // material helpers & hashmaps (material component <-> GPU material)
    plMaterialComponent* sbtMaterials;
    plFreeListNode**     sbtMaterialNodes;
    uint64_t             uMaterialDirtyValue;

    // shadows
    plGpuPointLightShadow* sbtPointLightShadowData;
    plGpuSpotLightShadow*  sbtSpotLightShadowData;
    uint64_t               uLastSemValueForShadow;

    // bindless texture system
    uint32_t uTextureIndexCount;
    uint32_t uCubeTextureIndexCount;

    // drawables
    plDrawable tUnitSphereDrawable;
    plDrawable tSkyboxDrawable;
    uint32_t*  sbuShadowDeferredDrawables; // shadow rendering (index into regular drawables)
    uint32_t*  sbuShadowForwardDrawables;  // shadow rendering (index into regular drawables)
    uint32_t*  sbuProbeDrawables;

    // SOA drawables
    plDrawable*          sbtDrawables;
    plDrawableResources* sbtDrawableResources;
    plShaderHandle*      sbtRegularShaders;
    plShaderHandle*      sbtShadowShaders;
    plShaderHandle*      sbtProbeShaders;
    plShaderHandle*      sbtOutlineShaders;

    // bvh data
    plBVH       tBvh;
    plAABB*     sbtBvhAABBs;
    plBVHNode** sbtNodeStack;

    // outlines
    plEntity* sbtOutlinedEntities;

    // CPU-side data for GPU buffers
    plGpuSceneData       tSceneData;
    plGpuPointLight*     sbtPointLightData;
    plGpuSpotLight*      sbtSpotLightData;
    plGpuDirectionLight* sbtDirectionLightData;
    plGpuProbe*          sbtGPUProbeData;

    // misc.
    plRendererLight*        sbtPointLights;
    plRendererLight*        sbtSpotLights;
    plRendererLight*        sbtDirectionLights;
    plEnvironmentProbeData* sbtProbeData;
    plSkinData*             sbtSkinData;
    uint32_t                uNextTransformIndex;

} plScene;

typedef struct _plRefRendererData
{
    plDevice* ptDevice;
    plDeviceInfo tDeviceInfo;
    plSwapchain* ptSwap;
    plTempAllocator tTempAllocator;
    uint32_t uMaxTextureResolution;

    // bind groups
    plBindGroupPool* ptBindGroupPool;
    plBindGroupPool* aptTempGroupPools[PL_MAX_FRAMES_IN_FLIGHT];

    // main renderpass layout (used as a template for views)
    plRenderPassLayoutHandle tRenderPassLayout;
    plRenderPassLayoutHandle tTransparentRenderPassLayout;
    plRenderPassLayoutHandle tPostProcessRenderPassLayout;
    plRenderPassLayoutHandle tFinalRenderPassLayout;
    plRenderPassLayoutHandle tUVRenderPassLayout;
    plRenderPassLayoutHandle tDepthRenderPassLayout;
    plRenderPassLayoutHandle tPickRenderPassLayout;

    // bind group layouts
    plBindGroupLayoutHandle tViewBGLayout;
    plBindGroupLayoutHandle tShadowGlobalBGLayout;

    // renderer specific log channel
    uint64_t uLogChannel;

    // GPU allocators
    plDeviceMemoryAllocatorI* ptLocalDedicatedAllocator;
    plDeviceMemoryAllocatorI* ptLocalBuddyAllocator;
    plDeviceMemoryAllocatorI* ptStagingUnCachedAllocator;
    plDeviceMemoryAllocatorI* ptStagingUnCachedBuddyAllocator;
    plDeviceMemoryAllocatorI* ptStagingCachedAllocator;
    
    // samplers
    plSamplerHandle tSamplerLinearClamp;
    plSamplerHandle tSamplerNearestClamp;
    plSamplerHandle tSamplerLinearRepeat;
    plSamplerHandle tSamplerNearestRepeat;

    // default textures
    plTextureHandle tDummyTexture;
    plTextureHandle tDummyTextureCube;

    plScene** sbptScenes;

    // draw stream data
    plDrawStream tDrawStream;
    
    // sync
    plTimelineSemaphore* ptClickSemaphore;
    uint64_t ulSemClickNextValue;

    // dynamic buffer system
    plDynamicDataBlock tCurrentDynamicDataBlock;

    // graphics options
    plRendererRuntimeOptions tRuntimeOptions;

    // stats
    double* pdDrawCalls;

    // ecs key cache
    plEcsTypeKey tSkinComponentType;
    plEcsTypeKey tLightComponentType;
    plEcsTypeKey tEnvironmentProbeComponentType;
    plEcsTypeKey tObjectComponentType;
} plRefRendererData;

typedef struct _plCullData
{
    plScene*    ptScene;
    plCamera*   ptCullCamera;
    plDrawable* atDrawables;
    plVec3      atFrustumCorners[8];
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
static plBufferHandle  pl__renderer_create_local_buffer         (const plBufferDesc*, const char* pcName, uint32_t uIdentifier);

typedef struct _plCSMInfo
{
    bool bAltMode;
    plBufferHandle tDShadowCameraBuffer;
    plBufferHandle tDLightShadowDataBuffer;
    plGpuDirectionLightShadow* sbtDLightShadowData;
    plBindGroupHandle tBindGroup;

} plCSMInfo;

// scene render helpers
static void pl__renderer_perform_skinning(plCommandBuffer*, plScene*);
static bool pl__renderer_pack_shadow_atlas(plScene*);
static void pl__renderer_generate_cascaded_shadow_map(plRenderEncoder*, plCommandBuffer*, plScene*, uint32_t, uint32_t, plCamera*, plCSMInfo);
static void pl__renderer_generate_shadow_maps(plRenderEncoder*, plCommandBuffer*, plScene*);

static uint64_t pl_renderer__add_material_to_scene(plScene* ptScene, plEntity tMaterial);

// misc
static inline plDynamicBinding pl__allocate_dynamic_data(plDevice* ptDevice){ return pl_allocate_dynamic_data(gptGfx, gptData->ptDevice, &gptData->tCurrentDynamicDataBlock);}
static bool                    pl__renderer_add_drawable_data_to_global_buffer(plScene*, uint32_t uDrawableIndex);
static uint32_t                pl__renderer_get_bindless_texture_index(plScene*, plTextureHandle);
static uint32_t                pl__renderer_get_bindless_cube_texture_index(plScene*, plTextureHandle);

// drawable ops
static void pl__renderer_add_skybox_drawable(plScene*);

// environment probes
static void pl__renderer_create_probe_data(plScene*, plEntity tProbeHandle);
static void pl__renderer_update_probes(plScene*);
static void pl__renderer_create_environment_map_from_texture(plScene*, plEnvironmentProbeData* ptProbe);
bool pl_renderer_add_drawable_objects_to_scene(plScene* ptScene, uint32_t uObjectCount, const plEntity* atObjects);

plBindGroupHandle pl_renderer_get_view_texture(plView* ptView, plVec2* ptMaxUVOut);

#endif // PL_RENDERER_INTERNAL_H