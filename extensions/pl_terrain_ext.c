/*
   pl_terrain_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] defines
// [SECTION] forward declarations
// [SECTION] structs
// [SECTION] global data
// [SECTION] internal helpers (preprocessing)
// [SECTION] internal helpers (rendering)
// [SECTION] public api implementation
// [SECTION] internal helpers implementation (preprocessing)
// [SECTION] internal helpers implementation (rendering)
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <float.h>
#include <stdlib.h>
#include "pl.h"
#include "pl_terrain_ext.h"

// libs
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"
#undef pl_vnsprintf
#include "pl_memory.h"
#include "pl_string.h"

// stable extensions
#include "pl_image_ext.h"
#include "pl_graphics_ext.h"
#include "pl_gpu_allocators_ext.h"
#include "pl_starter_ext.h"
#include "pl_shader_ext.h"
#include "pl_screen_log_ext.h"
#include "pl_vfs_ext.h"
#include "pl_stats_ext.h"

// unstable extensions
#include "pl_collision_ext.h"
#include "pl_freelist_ext.h"
#include "pl_camera_ext.h"
#include "pl_terrain_processor_ext.h"
#include "pl_image_ops_ext.h"
#include "pl_resource_ext.h"

// shader interop
#include "pl_shader_interop_terrain.h"

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#define PL_REQUEST_QUEUE_SIZE 100

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plTerrainResidencyNode   plTerrainResidencyNode;
typedef struct _plTerrainReplacementNode plTerrainReplacementNode;
typedef struct _plTerrainContext         plTerrainContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
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

    // required APIs
    static const plImageI*            gptImage            = NULL;
    static const plGraphicsI*         gptGfx              = NULL;
    static const plFreeListI*         gptFreeList         = NULL;
    static const plIOI*               gptIOI              = NULL;
    static const plShaderI*           gptShader           = NULL;
    static const plStarterI*          gptStarter          = NULL;
    static const plCollisionI*        gptCollision        = NULL;
    static const plScreenLogI*        gptScreenLog        = NULL;
    static const plTerrainProcessorI* gptTerrainProcessor = NULL;
    static const plGPUAllocatorsI*    gptGpuAllocators    = NULL;
    static const plImageOpsI*         gptImageOps         = NULL;
    static const plVfsI*              gptVfs              = NULL;
    static const plResourceI*         gptResource         = NULL;
    static const plStatsI*            gptStats            = NULL;

#endif

#include "pl_ds.h"

// context
static plTerrainContext* gptTerrainCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plTerrainResidencyNode
{
    plTerrainResidencyNode* ptNext;
    plTerrainResidencyNode* ptPrev;
    plTerrainChunk*         ptChunk;
    uint64_t                uFrameRequested;
} plTerrainResidencyNode;

typedef struct _plOBB2
{
    plVec3 tCenter;
    plVec3 tExtents;
    plVec3 atAxes[3]; // Orthonormal basis
} plOBB2;

typedef struct _plChunkFileData
{
    plTerrainChunkFile tFile;
    char               acPakFileName[256];
    uint32_t           uTextureIndex;
} plChunkFileData;

typedef struct _plTerrain
{
    plRenderPassLayoutHandle tRenderPassLayoutHandle;
    plTerrainRuntimeOptions tRuntimeOptions;
    plChunkFileData* sbtChunkFiles;
    plTerrainProcessInfo tInfo;
    plVec2           tTopLeftGlobal;
    plVec2           tBottomRightGlobal;
    uint32_t                 uTileCount;
    plTerrainProcessTileInfo* atTiles;
    plDynamicDataBlock* ptCurrentDynamicBufferBlock;

    // shaders
    plShaderHandle tShader;
    plShaderHandle tWireframeShader;
    
    plTerrainResidencyNode tRequestQueue;
    plTerrainResidencyNode atRequests[PL_REQUEST_QUEUE_SIZE];
    uint32_t*             sbuFreeRequests;

    plTerrainChunk tReplacementQueue;

    const char* pcVertexShader;
    const char* pcFragmentShader;

    plBufferHandle tIndexBuffer;
    plFreeList tIndexBufferManager;
    
    plBufferHandle tVertexBuffer;
    plFreeList tVertexBufferManager;
} plTerrain;

typedef struct _plTerrainContext
{
    plDevice*        ptDevice;
    plBindGroupPool* ptBindGroupPool;

    // gpu allocators
    plDeviceMemoryAllocatorI* tLocalBuddyAllocator;

    // samplers
    plSamplerHandle tSampler;
    plTextureHandle tDummyTexture;
    uint32_t        uDummyIndex;

    // bindless texture system
    uint32_t          uTextureIndexCount;
    plHashMap64       tTextureIndexHashmap; // texture handle <-> index
    plBindGroupHandle atBindGroups[PL_MAX_FRAMES_IN_FLIGHT];

    plTempAllocator tTempAllocator;

    plBufferHandle tStagingBuffer;
    uint32_t       uStagingBufferSize;
    double*        pdDrawCalls;


} plTerrainContext;



//-----------------------------------------------------------------------------
// [SECTION] internal helpers (rendering)
//-----------------------------------------------------------------------------

void pl_terrain_load_shaders(plTerrain* ptTerrain);

// rendering
static void pl__handle_residency (plTerrain*, plCommandBuffer*);
static void pl__request_residency(plTerrain*, plTerrainChunk*);
static void pl__touch_chunk(plTerrain*, plTerrainChunk*);
static void pl__make_unresident  (plTerrain*, plTerrainChunk*);
static bool pl__terrain_load(plTerrain* ptTerrain, plTerrainProcessInfo* ptInfo);
void pl__remove_from_replacement_queue(plTerrain* ptTerrain, plTerrainChunk* ptChunk);

static void pl__render_chunk(plTerrain*, plCamera*, plRenderEncoder*, plTerrainChunk*, plTerrainChunkFile*, const plMat4* ptMVP);
static bool pl__sat_visibility_test(plCamera*, const plAABB*);

static void pl__free_chunk(plTerrain* ptTerrain, uint64_t);

static void pl__free_chunk_until(plTerrain* P, uint64_t idx_bytes_needed, uint64_t vtx_bytes_needed);

static plTextureHandle pl__terrain_create_texture_with_data (const plTextureDesc*, const char* pcName, uint32_t uIdentifier, const void*, size_t);
static uint32_t pl__terrain_get_bindless_texture_index(plTextureHandle tTexture);
static void pl__terrain_return_bindless_texture_index(plTextureHandle tTexture);
bool pl__chlod_update_chunk_file(plTerrain* ptTerrain, uint32_t uIndex, const char* pcTexture);


static inline bool pl__is_leaf_resident(const plTerrainChunk* c)
{
    if (!c->aptChildren[0]) return true; // no children in tree
    return !(c->aptChildren[0]->ptIndexHole ||
             c->aptChildren[1]->ptIndexHole ||
             c->aptChildren[2]->ptIndexHole ||
             c->aptChildren[3]->ptIndexHole);
}



//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_terrain_initialize(plTerrainExtInit tInit)
{
    gptTerrainCtx->ptDevice = tInit.ptDevice;
    gptTerrainCtx->pdDrawCalls = gptStats->get_counter("terrain draw calls");

    // create bind group pool
    plBindGroupPoolDesc tBindGroupPoolDesc = {
        .tFlags                   = PL_BIND_GROUP_POOL_FLAGS_NONE,
        .szSamplerBindings        = 1,
        .szSampledTextureBindings = PL_TERRAIN_MAX_BINDLESS_TEXTURES * 2,
        .szStorageTextureBindings = 1,
        .szStorageBufferBindings  = 1
    };
    gptTerrainCtx->ptBindGroupPool = gptGfx->create_bind_group_pool(tInit.ptDevice, &tBindGroupPoolDesc);

    // retrieve GPU allocators
    gptTerrainCtx->tLocalBuddyAllocator = gptGpuAllocators->get_local_buddy_allocator(tInit.ptDevice);

    const plBindGroupLayoutDesc tBindGroupLayoutDesc = {
        .atSamplerBindings = {
            { .uSlot = 0, .tStages = PL_SHADER_STAGE_FRAGMENT}
        },
        .atTextureBindings = {
            {.uSlot = 1, .tStages = PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .bNonUniformIndexing = true, .uDescriptorCount = PL_TERRAIN_MAX_BINDLESS_TEXTURES}
        }
    };
    plBindGroupLayoutHandle tBindGroupLayout = gptGfx->create_bind_group_layout(tInit.ptDevice, &tBindGroupLayoutDesc);


    const plSamplerDesc tSamplerDesc = {
        .tMagFilter    = PL_FILTER_LINEAR,
        .tMinFilter    = PL_FILTER_LINEAR,
        .fMinMip       = 0.0f,
        .fMaxMip       = 1.0f,
        .tVAddressMode = PL_ADDRESS_MODE_CLAMP_TO_BORDER,
        .tUAddressMode = PL_ADDRESS_MODE_CLAMP_TO_BORDER,
        .pcDebugName   = "sampler",
        .tBorderColor  = PL_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK
    };
    gptTerrainCtx->tSampler = gptGfx->create_sampler(tInit.ptDevice, &tSamplerDesc);

    const plBindGroupUpdateSamplerData tSamplerData = {
        .tSampler = gptTerrainCtx->tSampler,
        .uSlot    = 0
    };
    const plBindGroupUpdateData tBGSet0Data = {
        .uSamplerCount = 1,
        .atSamplerBindings = &tSamplerData
    };

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        // create global bindgroup
        const plBindGroupDesc tGlobalBindGroupDesc = {
            .ptPool      = gptTerrainCtx->ptBindGroupPool,
            .tLayout     = tBindGroupLayout,
            .pcDebugName = "global bind group"
        };
        gptTerrainCtx->atBindGroups[i] = gptGfx->create_bind_group(tInit.ptDevice, &tGlobalBindGroupDesc);

        gptGfx->update_bind_group(tInit.ptDevice, gptTerrainCtx->atBindGroups[i], &tBGSet0Data);
    }

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
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f
    };
    gptTerrainCtx->tDummyTexture = pl__terrain_create_texture_with_data(&tDummyTextureDesc, "dummy", 0, afDummyTextureData, sizeof(afDummyTextureData));
    gptTerrainCtx->uDummyIndex = pl__terrain_get_bindless_texture_index(gptTerrainCtx->tDummyTexture);

    plDevice* ptDevice = gptTerrainCtx->ptDevice;

    if(tInit.uStagingBufferSize == 0) tInit.uStagingBufferSize = 268435456;
    gptTerrainCtx->uStagingBufferSize = tInit.uStagingBufferSize;

    // create vertex buffer
    const plBufferDesc tStagingBufferDesc = {
        .tUsage      = PL_BUFFER_USAGE_TRANSFER_SOURCE,
        .szByteSize  = gptTerrainCtx->uStagingBufferSize,
        .pcDebugName = "terrain staging buffer"
    };
    gptTerrainCtx->tStagingBuffer = gptGfx->create_buffer(ptDevice, &tStagingBufferDesc, NULL);

    // retrieve buffer to get memory allocation requirements (do not store buffer pointer)
    plBuffer* ptStagingBuffer = gptGfx->get_buffer(ptDevice, gptTerrainCtx->tStagingBuffer);

    // allocate memory for the vertex buffer
    const plDeviceMemoryAllocation tStagingBufferAllocation = gptGfx->allocate_memory(ptDevice,
        ptStagingBuffer->tMemoryRequirements.ulSize,
        PL_MEMORY_FLAGS_HOST_VISIBLE | PL_MEMORY_FLAGS_HOST_COHERENT,
        ptStagingBuffer->tMemoryRequirements.uMemoryTypeBits,
        "staging buffer memory");

    // bind the buffer to the new memory allocation
    gptGfx->bind_buffer_to_memory(ptDevice, gptTerrainCtx->tStagingBuffer, &tStagingBufferAllocation);
}

void
pl_terrain_cleanup(void)
{
    plDevice* ptDevice = gptTerrainCtx->ptDevice;
    gptGfx->destroy_buffer(ptDevice, gptTerrainCtx->tStagingBuffer);
    pl_hm_free(&gptTerrainCtx->tTextureIndexHashmap);
    pl_temp_allocator_free(&gptTerrainCtx->tTempAllocator);
    gptGfx->cleanup_bind_group_pool(gptTerrainCtx->ptBindGroupPool);
    gptGpuAllocators->cleanup(gptTerrainCtx->ptDevice);
}

plTerrain*
pl_create_terrain(plCommandBuffer* ptCmdBuffer, plTerrainInit tInit, plTerrainProcessInfo* ptInfo)
{
    plTerrain* ptTerrain = PL_ALLOC(sizeof(plTerrain));
    memset(ptTerrain, 0, sizeof(plTerrain));


    ptTerrain->tRenderPassLayoutHandle = *tInit.ptRenderPassLayoutHandle;
    ptTerrain->tInfo = *ptInfo;
    ptTerrain->tRuntimeOptions.fTau = 1.0f;
    ptTerrain->tRuntimeOptions.tLightDirection = (plVec3){-1.0f, -1.0f, -1.0f};

    if(tInit.pcVertexShader == NULL) tInit.pcVertexShader = "terrain.vert";
    if(tInit.pcFragmentShader == NULL) tInit.pcFragmentShader = "terrain.frag";

    ptTerrain->pcVertexShader = tInit.pcVertexShader;
    ptTerrain->pcFragmentShader = tInit.pcFragmentShader;
    pl_terrain_load_shaders(ptTerrain);
    
    pl_sb_resize(ptTerrain->sbuFreeRequests, PL_REQUEST_QUEUE_SIZE);

    for(uint32_t i = 0; i < PL_REQUEST_QUEUE_SIZE; i++) 
    {
        ptTerrain->sbuFreeRequests[i] = i;
    }

    if(tInit.uIndexBufferSize == 0)  tInit.uIndexBufferSize = 268435456;
    if(tInit.uVertexBufferSize == 0) tInit.uVertexBufferSize = 268435456;
    
    gptFreeList->create(tInit.uVertexBufferSize, 256, &ptTerrain->tVertexBufferManager);
    gptFreeList->create(tInit.uIndexBufferSize, 256, &ptTerrain->tIndexBufferManager);

    plDevice* ptDevice = gptTerrainCtx->ptDevice;

    const plBufferDesc tVertexBufferDesc = {
        .tUsage      = PL_BUFFER_USAGE_VERTEX | PL_BUFFER_USAGE_TRANSFER_DESTINATION,
        .szByteSize  = ptTerrain->tVertexBufferManager.uSize,
        .pcDebugName = "vertex buffer"
    };
    ptTerrain->tVertexBuffer = gptGfx->create_buffer(ptDevice, &tVertexBufferDesc, NULL);

    // retrieve buffer to get memory allocation requirements (do not store buffer pointer)
    plBuffer* ptVertexBuffer = gptGfx->get_buffer(ptDevice, ptTerrain->tVertexBuffer);

    // allocate memory for the vertex buffer
    const plDeviceMemoryAllocation tVertexBufferAllocation = gptGfx->allocate_memory(ptDevice,
        ptVertexBuffer->tMemoryRequirements.ulSize,
        PL_MEMORY_FLAGS_DEVICE_LOCAL,
        ptVertexBuffer->tMemoryRequirements.uMemoryTypeBits,
        "vertex buffer memory");

    // bind the buffer to the new memory allocation
    gptGfx->bind_buffer_to_memory(ptDevice, ptTerrain->tVertexBuffer, &tVertexBufferAllocation);

    // create index buffer
    const plBufferDesc tIndexBufferDesc = {
        .tUsage      = PL_BUFFER_USAGE_INDEX | PL_BUFFER_USAGE_TRANSFER_DESTINATION,
        .szByteSize  = ptTerrain->tIndexBufferManager.uSize,
        .pcDebugName = "index buffer"
    };
    ptTerrain->tIndexBuffer = gptGfx->create_buffer(ptDevice, &tIndexBufferDesc, NULL);

    // retrieve buffer to get memory allocation requirements (do not store buffer pointer)
    plBuffer* ptIndexBuffer = gptGfx->get_buffer(ptDevice, ptTerrain->tIndexBuffer);

    // allocate memory for the index buffer
    const plDeviceMemoryAllocation tIndexBufferAllocation = gptGfx->allocate_memory(ptDevice,
        ptIndexBuffer->tMemoryRequirements.ulSize,
        PL_MEMORY_FLAGS_DEVICE_LOCAL,
        ptIndexBuffer->tMemoryRequirements.uMemoryTypeBits,
        "index buffer memory");

    // bind the buffer to the new memory allocation
    gptGfx->bind_buffer_to_memory(ptDevice, ptTerrain->tIndexBuffer, &tIndexBufferAllocation);

    pl__terrain_load(ptTerrain, ptInfo);
    ptTerrain->atTiles = PL_ALLOC(sizeof(plTerrainProcessTileInfo) * ptInfo->uTileCount);
    memset(ptTerrain->atTiles, 0, sizeof(plTerrainProcessTileInfo) * ptInfo->uTileCount);
    memcpy(ptTerrain->atTiles, ptInfo->atTiles, sizeof(plTerrainProcessTileInfo) * ptInfo->uTileCount);
    ptTerrain->uTileCount = ptInfo->uTileCount;

    for(uint32_t i = 0; i < pl_sb_size(ptTerrain->sbtChunkFiles); i++)
        pl__request_residency(ptTerrain, &ptTerrain->sbtChunkFiles[i].tFile.atChunks[0]);
    return ptTerrain;
}

void
pl_cleanup_terrain(plTerrain* ptTerrain)
{
    plDevice* ptDevice = gptTerrainCtx->ptDevice;

    for(uint32_t i = 0; i < pl_sb_size(ptTerrain->sbtChunkFiles); i++)
    {
        PL_FREE(ptTerrain->sbtChunkFiles[i].tFile.atChunks);
        ptTerrain->sbtChunkFiles[i].tFile.atChunks = NULL;
        ptTerrain->sbtChunkFiles[i].tFile.uChunkCount = 0;
        ptTerrain->sbtChunkFiles[i].tFile.fMaxBaseError = 0.0f;
        ptTerrain->sbtChunkFiles[i].tFile.iTreeDepth = 0;
    }

    PL_FREE(ptTerrain->atTiles);
    ptTerrain->atTiles = NULL;

    // cleanup our resources
    gptGfx->destroy_buffer(ptDevice, ptTerrain->tVertexBuffer);
    gptGfx->destroy_buffer(ptDevice, ptTerrain->tIndexBuffer);
    gptFreeList->cleanup(&ptTerrain->tVertexBufferManager);
    gptFreeList->cleanup(&ptTerrain->tIndexBufferManager);



    pl_sb_free(ptTerrain->sbuFreeRequests);
    pl_sb_free(ptTerrain->sbtChunkFiles);
    PL_FREE(ptTerrain);
}


//-----------------------------------------------------------------------------
// [SECTION] terrain view api
//-----------------------------------------------------------------------------

void
pl_render_terrain(plRenderEncoder* ptEncoder, plTerrain* ptTerrain, plCamera* ptCamera, plDynamicDataBlock* ptCurrentDynamicBufferBlock)
{
    ptTerrain->ptCurrentDynamicBufferBlock = ptCurrentDynamicBufferBlock;
    const plMat4 tMVP = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);
    plDevice* ptDevice = gptTerrainCtx->ptDevice;
    gptGfx->set_depth_bias(ptEncoder, 0.0f, 0.0f, 0.0f);

    plShaderHandle tShader =
        (ptTerrain->tRuntimeOptions.tFlags & PL_TERRAIN_FLAGS_WIREFRAME)
        ? ptTerrain->tWireframeShader : ptTerrain->tShader;

    gptGfx->bind_shader(ptEncoder, tShader);
    gptGfx->bind_vertex_buffer(ptEncoder, ptTerrain->tVertexBuffer);
    gptGfx->bind_graphics_bind_groups(
        ptEncoder,
        tShader,
        0, 1,
        &gptTerrainCtx->atBindGroups[gptGfx->get_current_frame_index()],
        0, NULL
    );

    for(uint32_t i = 0; i < pl_sb_size(ptTerrain->sbtChunkFiles); i++)
        pl__render_chunk(ptTerrain, ptCamera, ptEncoder, &ptTerrain->sbtChunkFiles[i].tFile.atChunks[0], &ptTerrain->sbtChunkFiles[i].tFile, &tMVP);
}

// Helper: clamp integer to a range
static inline int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

void
pl_terrain_set_texture(plTerrain* ptTerrain, plTerrainTexture* ptTexture)
{
    // ---------------------------------------------------------------------
    // 1) Evict/unbind previous textures for all chunk files
    // ---------------------------------------------------------------------
    for (uint32_t i = 0; i < pl_sb_size(ptTerrain->sbtChunkFiles); i++)
    {
        if (ptTerrain->sbtChunkFiles[i].uTextureIndex != 0)
        {
            plResourceHandle tTextureResource = gptResource->load_ex(
                ptTerrain->sbtChunkFiles[i].acPakFileName,
                PL_RESOURCE_LOAD_FLAG_NO_CACHING, NULL, 0,
                ptTerrain->sbtChunkFiles[i].acPakFileName, 0);

            plTextureHandle tTexture = gptResource->get_texture(tTextureResource);
            pl__terrain_return_bindless_texture_index(tTexture);
            ptTerrain->sbtChunkFiles[i].uTextureIndex = 0;
            gptResource->evict(tTextureResource);
            gptResource->unload(tTextureResource);
        }
    }

    // ---------------------------------------------------------------------
    // 2) Active tile mask (use tInfo counts consistently)
    // ---------------------------------------------------------------------
    const uint32_t uH          = ptTerrain->tInfo.uHorizontalTiles;
    const uint32_t uV          = ptTerrain->tInfo.uVerticalTiles;
    const uint32_t uTileCount  = ptTerrain->tInfo.uTileCount;

    bool* abActiveTextureTiles = PL_ALLOC(sizeof(bool) * uTileCount);
    memset(abActiveTextureTiles, 0, sizeof(bool) * uTileCount);

    if (ptTexture)
    {
        float fX = ptTexture->tCenter.x;
        float fY = ptTexture->tCenter.y;

        // -----------------------------------------------------------------
        // 4) Compute world-space bounds of the incoming image in meters
        // -----------------------------------------------------------------
        plImageInfo tImageInfo = (plImageInfo){0};
        gptImage->get_info_from_file(ptTexture->pcPath, &tImageInfo);

        const float imgWm = (float)tImageInfo.iWidth  * ptTexture->fMetersPerPixel;
        const float imgHm = (float)tImageInfo.iHeight * ptTexture->fMetersPerPixel;

        const plVec2 tTopLeft = {
            .x = fX - 0.5f * imgWm,
            .y = fY - 0.5f * imgHm,
        };
        const plVec2 tBottomRight = {
            .x = fX + 0.5f * imgWm,
            .y = fY + 0.5f * imgHm,
        };

        // -----------------------------------------------------------------
        // 5) Compute signed tile index range (BR index inclusive)
        // -----------------------------------------------------------------
        const float tileSizeM = (float)ptTerrain->tInfo.uSize * ptTerrain->tInfo.fMetersPerPixel;

        int tlx = (int)floorf((tTopLeft.x     - ptTerrain->tTopLeftGlobal.x) / tileSizeM);
        int tly = (int)floorf((tTopLeft.y     - ptTerrain->tTopLeftGlobal.y) / tileSizeM);
        int brx = (int)floorf ((tBottomRight.x - ptTerrain->tTopLeftGlobal.x) / tileSizeM);
        int bry = (int)floorf ((tBottomRight.y - ptTerrain->tTopLeftGlobal.y) / tileSizeM);

        // No overlap with tile grid? Early out
        if (!(tlx > (int)uH - 1 || tly > (int)uV - 1 || brx < 0 || bry < 0))
        {

            // Clamp to grid
            tlx = clampi(tlx, 0, (int)uH - 1);
            tly = clampi(tly, 0, (int)uV - 1);
            brx = clampi(brx, 0, (int)uH - 1);
            bry = clampi(bry, 0, (int)uV - 1);

            // Normalize order (defensive if rounding flipped them)
            if (brx < tlx) { int t = brx; brx = tlx; tlx = t; }
            if (bry < tly) { int t = bry; bry = tly; tly = t; }

            const uint32_t tlIndex = (uint32_t)(tlx + tly * (int)uH);
            const uint32_t brIndex = (uint32_t)(brx + bry * (int)uH);

            if (tlIndex < uTileCount && brIndex < uTileCount)
            {

                // -----------------------------------------------------------------
                // 6) Get local world coords of the clamped tile rectangle
                //     (Use tile centers +/− half a tile to form inclusive bounds)
                // -----------------------------------------------------------------
                plVec2 tTopLeftLocal     = {0};
                plVec2 tBottomRightLocal = {0};

                // --- Top-left tile local origin ---
                {
                    tTopLeftLocal.x = ptTerrain->atTiles[tlIndex].tCenter.x - 0.5f * tileSizeM;
                    tTopLeftLocal.y = ptTerrain->atTiles[tlIndex].tCenter.z - 0.5f * tileSizeM;
                }

                // --- Bottom-right tile local corner ---
                {
                    tBottomRightLocal.x = ptTerrain->atTiles[brIndex].tCenter.x + 0.5f * tileSizeM;
                    tBottomRightLocal.y = ptTerrain->atTiles[brIndex].tCenter.z + 0.5f * tileSizeM;
                }

                // -----------------------------------------------------------------
                // 7) Build a full canvas covering [tl..br] tiles, and place image
                // -----------------------------------------------------------------
                int iImageWidth  = 0;
                int iImageHeight = 0;
                int _unused      = 0;
                unsigned char* pucImageData = gptImage->load_from_file(
                    ptTexture->pcPath, &iImageWidth, &iImageHeight, &_unused, 4);

                plImageOpInit tFullInfo = {
                    .uVirtualWidth    = (uint32_t)fmaxf(1.0f, (tBottomRightLocal.x - tTopLeftLocal.x) / ptTexture->fMetersPerPixel),
                    .uVirtualHeight   = (uint32_t)fmaxf(1.0f, (tBottomRightLocal.y - tTopLeftLocal.y) / ptTexture->fMetersPerPixel),
                    .uChannels = 4,
                    .uStride   = 4
                };

                plImageOpData tFullData = (plImageOpData){0};
                gptImageOps->initialize(&tFullInfo, &tFullData);
                // gptImageOps->add_region(&tFullData, 0, 0, tFullInfo.uVirtualWidth, tFullInfo.uVirtualHeight, PL_IMAGE_OP_COLOR_WHITE);

                // If square() changes dims, we must use tFullData.uWidth/Height afterward
                gptImageOps->square(&tFullData);

                // Pixel offsets for where the image should land on the full canvas
                const float fDistanceX = tTopLeft.x - tTopLeftLocal.x;
                const float fDistanceY = tTopLeft.y - tTopLeftLocal.y;

                const uint32_t fullW = tFullData.uVirtualWidth;
                const uint32_t fullH = tFullData.uVirtualHeight;

                const float fEffectiveMetersPerPixelX =
                    (tBottomRightLocal.x - tTopLeftLocal.x) / (float)fullW;
                const float fEffectiveMetersPerPixelY =
                    (tBottomRightLocal.y - tTopLeftLocal.y) / (float)fullH;

                const float fEffectiveMetersPerPixel = pl_max(fEffectiveMetersPerPixelX, fEffectiveMetersPerPixelY);

                const uint32_t uXOffsetIndex =
                    (uint32_t)fmaxf(0.0f, floorf(fDistanceX / fEffectiveMetersPerPixel));
                const uint32_t uYOffsetIndex =
                    (uint32_t)fmaxf(0.0f, floorf(fDistanceY / fEffectiveMetersPerPixel));

                gptImageOps->add(&tFullData, uXOffsetIndex, uYOffsetIndex, (uint32_t)iImageWidth, (uint32_t)iImageHeight, pucImageData);
                gptImageOps->square(&tFullData);
                gptImage->free(pucImageData);

                // -----------------------------------------------------------------
                // 8) Slice canvas into per-tile images
                // -----------------------------------------------------------------
                const uint32_t uHorizontalExtent = (uint32_t)(brx - tlx + 1);
                const uint32_t uVerticalExtent   = (uint32_t)(bry - tly + 1);

                // Avoid zero increments if canvas is very small
                uint32_t uXInc = (uHorizontalExtent > 0) ? (fullW / uHorizontalExtent) : 0;
                uint32_t uYInc = (uVerticalExtent   > 0) ? (fullH / uVerticalExtent)   : 0;
                if (uXInc == 0) uXInc = 1;
                if (uYInc == 0) uYInc = 1;

                uint32_t uInc = pl_min(uXInc, uYInc);

                for (uint32_t ix = 0; ix < uHorizontalExtent; ix++)
                {
                    if(ix * uInc > tFullData.uActiveWidth)
                        break;

                    if(ix * uInc + uInc < tFullData.uActiveXOffset)
                        continue;

                    for (uint32_t iy = 0; iy < uVerticalExtent; iy++)
                    {

                        if(iy * uInc > tFullData.uActiveHeight)
                            break;

                        if(iy * uInc + uInc < tFullData.uActiveYOffset)
                            continue;


                        const uint32_t tileX = (uint32_t)(tlx + (int)ix);
                        const uint32_t tileY = (uint32_t)(tly + (int)iy);

                        char acNameBuffer[128] = {0};
                        sprintf(acNameBuffer, "hazard_prep_%u_%u.png", tileX, tileY);

                        // Mark tile active, with bounds check (defensive)
                        const size_t flat = (size_t)tileX + (size_t)tileY * (size_t)uH;
                        if (flat < (size_t)uTileCount)
                            abActiveTextureTiles[flat] = true;



                        int iSubXOffset = pl_max(ix * uInc, tFullData.uActiveXOffset);
                        int iSubXEnd = pl_min(ix * uInc + uInc, tFullData.uActiveXOffset + tFullData.uActiveWidth);
                        int iSubYOffset = pl_max(iy * uInc, tFullData.uActiveYOffset);
                        int iSubYEnd = pl_min(iy * uInc + uInc, tFullData.uActiveYOffset + tFullData.uActiveHeight);
                        int iFinalWidth = iSubXEnd - iSubXOffset;
                        int iFinalHeight= iSubYEnd - iSubYOffset;

                        uint8_t* puImageData = gptImageOps->extract(&tFullData, iSubXOffset, iSubYOffset, iFinalWidth, iFinalHeight, NULL);

                        plImageWriteInfo tWriteInfo = {
                            .iWidth       = (int)iFinalWidth,
                            .iHeight      = (int)iFinalHeight,
                            .iComponents  = 4,
                            .iByteStride  = (int)(iFinalWidth * 4)
                        };
                        gptImage->write(acNameBuffer, puImageData, &tWriteInfo);
                        gptImageOps->cleanup_extract(puImageData);

                        sprintf(ptTerrain->sbtChunkFiles[flat].acPakFileName, "%s", acNameBuffer);

                        plResourceHandle tTextureResource = gptResource->load_ex(
                            ptTerrain->sbtChunkFiles[flat].acPakFileName,
                            PL_RESOURCE_LOAD_FLAG_NO_CACHING, NULL, 0,
                            NULL, 0); 
                        gptResource->make_resident(tTextureResource);
                        plTextureHandle tTexture = gptResource->get_texture(tTextureResource);
                        ptTerrain->sbtChunkFiles[flat].uTextureIndex = pl__terrain_get_bindless_texture_index(tTexture);
                        // plTexture* ptTexture = gptGfx->get_texture(gptTerrainCtx->ptDevice, tTexture);

                        float fUStart = (float)iSubXOffset / (float)tFullData.uVirtualWidth;
                        float fUEnd = (float)iSubXEnd / (float)tFullData.uVirtualWidth;

                        float fVStart = (float)iSubYOffset / (float)tFullData.uVirtualHeight;
                        float fVEnd = (float)iSubYEnd / (float)tFullData.uVirtualHeight;

                        if(ix == 1 && iy == 1)
                        {
                            int a = 5;
                        }

                        // float fXAdditionalOffset = (float)(iSubXOffset - ix * uInc) / (float)iFinalWidth;
                        // float fYAdditionalOffset = (float)(iSubYOffset - iy * uInc) / (float)iFinalHeight;

                        for(uint32_t i = 0; i < ptTerrain->sbtChunkFiles[flat].tFile.uChunkCount; i++)
                        {
                            uint32_t uTopDownLevel = ptTerrain->sbtChunkFiles[flat].tFile.iTreeDepth - ptTerrain->sbtChunkFiles[flat].tFile.atChunks[i].uLevel - 1;

                            // chunk width
                            uint32_t uWidth = (uint32_t)((float)uInc / powf(2.0f, (float)uTopDownLevel));
                            uint32_t uHeight = (uint32_t)((float)uInc / powf(2.0f, (float)uTopDownLevel));

                            // final scaling factor
                            float fXScale = (float)uWidth / (float)iFinalWidth;
                            float fYScale = (float)uHeight / (float)iFinalHeight;

                            ptTerrain->sbtChunkFiles[flat].tFile.atChunks[i].tUVScale.x = fXScale;
                            ptTerrain->sbtChunkFiles[flat].tFile.atChunks[i].tUVScale.y = fYScale;

                            // UV on parent chunk
                            float fU = (float)ptTerrain->sbtChunkFiles[flat].tFile.atChunks[i].fX; // UV on original heightmap
                            float fV = (float)ptTerrain->sbtChunkFiles[flat].tFile.atChunks[i].fY; // UV on original heightmap

                            // convert to UV in final texture space
                            fU = fU * (float)uInc / (float)iFinalWidth;
                            fU = fU - (float)(iSubXOffset - ix * uInc) / (float)iFinalWidth;

                            fV = fV * (float)uInc / (float)iFinalHeight;
                            fV = fV - (float)(iSubYOffset - iy * uInc) / (float)iFinalHeight;

                            // works for root level but does too much at child levels
                            ptTerrain->sbtChunkFiles[flat].tFile.atChunks[i].tUVOffset.x = fU;
                            ptTerrain->sbtChunkFiles[flat].tFile.atChunks[i].tUVOffset.y = fV;
                        }
                    }
                }
                gptImageOps->cleanup(&tFullData);
            }
        }
    }

    // ---------------------------------------------------------------------
    // 9) Update chunk files with generated hazard textures
    // ---------------------------------------------------------------------
    for (uint32_t k = 0; k < uTileCount; k++)
    {
        if(!abActiveTextureTiles[k])
        {
            for(uint32_t i = 0; i < ptTerrain->sbtChunkFiles[k].tFile.uChunkCount; i++)
            {
                ptTerrain->sbtChunkFiles[k].tFile.atChunks[i].tUVScale.x = 1.0f;
                ptTerrain->sbtChunkFiles[k].tFile.atChunks[i].tUVScale.y = 1.0f;
            }
        }
    }

    PL_FREE(abActiveTextureTiles);
}

bool
pl_chlod_load_chunk_file(plTerrain* ptTerrain, const char* pcPath)
{
    plChunkFileData tChunkFileData = {0};
    uint32_t uChunkFileID = pl_sb_size(ptTerrain->sbtChunkFiles);
    gptTerrainProcessor->load_chunk_file(pcPath, &tChunkFileData.tFile, uChunkFileID);

    for(uint32_t i = 0; i < tChunkFileData.tFile.uChunkCount; i++)
    {

        tChunkFileData.tFile.atChunks[i].uIndex = i;

        tChunkFileData.tFile.atChunks[i].tUVScale.x = 1.0f;
        tChunkFileData.tFile.atChunks[i].tUVScale.y = 1.0f;
    }
    pl_sb_push(ptTerrain->sbtChunkFiles, tChunkFileData);
    return true;
}

void
pl_terrain_load_shaders(plTerrain* ptTerrain)
{
    if(gptGfx->is_shader_valid(gptTerrainCtx->ptDevice, ptTerrain->tShader))
    {
        gptGfx->queue_shader_for_deletion(gptTerrainCtx->ptDevice, ptTerrain->tShader);
        gptGfx->queue_shader_for_deletion(gptTerrainCtx->ptDevice, ptTerrain->tWireframeShader);
    }

    plShaderDesc tShaderDesc = {
        .tVertexShader    = gptShader->load_glsl(ptTerrain->pcVertexShader, "main", NULL, NULL),
        .tFragmentShader  = gptShader->load_glsl(ptTerrain->pcFragmentShader, "main", NULL, NULL),
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 1,
            .ulDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
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
                .atAttributes = {
                    {.tFormat = PL_VERTEX_FORMAT_FLOAT3},
                    {.tFormat = PL_VERTEX_FORMAT_FLOAT2},
                    {.tFormat = PL_VERTEX_FORMAT_FLOAT2}
                }
            }
        },
        .atBlendStates = {
            {
                .bBlendEnabled   = false,
                .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL,
                .tSrcColorFactor = PL_BLEND_FACTOR_SRC_ALPHA,
                .tDstColorFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .tColorOp        = PL_BLEND_OP_ADD,
                .tSrcAlphaFactor = PL_BLEND_FACTOR_SRC_ALPHA,
                .tDstAlphaFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .tAlphaOp        = PL_BLEND_OP_ADD
            }
        },
        .atBindGroupLayouts = {
            {
                .atSamplerBindings = {
                    { .uSlot = 0, .tStages = PL_SHADER_STAGE_FRAGMENT}
                },
                .atTextureBindings = {
                    {.uSlot = 1, .tStages = PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .bNonUniformIndexing = true, .uDescriptorCount = PL_TERRAIN_MAX_BINDLESS_TEXTURES}
                }
            }
        },
        .tRenderPassLayout = ptTerrain->tRenderPassLayoutHandle,
        .pcDebugName = "terrain shader"
    };
    plDevice* ptDevice = gptTerrainCtx->ptDevice;
    ptTerrain->tShader = gptGfx->create_shader(ptDevice, &tShaderDesc);

    tShaderDesc.tGraphicsState.ulWireframe = 1;
    tShaderDesc.tGraphicsState.ulDepthWriteEnabled = 0;
    tShaderDesc.tGraphicsState.ulDepthMode = PL_COMPARE_MODE_ALWAYS;
    ptTerrain->tWireframeShader = gptGfx->create_shader(ptDevice, &tShaderDesc);
}

void
pl_terrain_set_shaders(plTerrain* ptTerrain, const char* pcVertexShader, const char* pcFragmentShader)
{
    ptTerrain->pcVertexShader   = pcVertexShader   ? pcVertexShader   : "terrain.vert";
    ptTerrain->pcFragmentShader = pcFragmentShader ? pcFragmentShader : "terrain.frag";
    pl_terrain_load_shaders(ptTerrain);
}

void
pl_prepare_terrain(plTerrain* ptTerrain, plCommandBuffer* ptCmdBuffer)
{
    if(ptTerrain->tRequestQueue.ptNext)
    {
        gptScreenLog->add_message_ex(294, 10.0, PL_COLOR_32_YELLOW, 1.0f, "Stream Active");
        pl__handle_residency(ptTerrain, ptCmdBuffer);
    }
    else
    {
        gptScreenLog->add_message_ex(294, 10.0, PL_COLOR_32_RED, 1.0f, "Stream Inactive");
    }
}

void
pl_terrain_set_runtime_options(plTerrain* ptTerrain, plTerrainRuntimeOptions tOptions)
{
    ptTerrain->tRuntimeOptions = tOptions;
}

plTerrainRuntimeOptions
pl_terrain_get_runtime_options(plTerrain* ptTerrain)
{
    return ptTerrain->tRuntimeOptions;
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static void
pl__unload_children(plTerrain* ptTerrain, plTerrainChunk* ptChunk)
{
    if(ptChunk->aptChildren[0] == NULL)
        return;
    if(ptChunk->aptChildren[0]->ptIndexHole) pl__make_unresident(ptTerrain, ptChunk->aptChildren[0]);
    if(ptChunk->aptChildren[1]->ptIndexHole) pl__make_unresident(ptTerrain, ptChunk->aptChildren[1]);
    if(ptChunk->aptChildren[2]->ptIndexHole) pl__make_unresident(ptTerrain, ptChunk->aptChildren[2]);
    if(ptChunk->aptChildren[3]->ptIndexHole) pl__make_unresident(ptTerrain, ptChunk->aptChildren[3]);    
}

bool
pl__all_children_resident(plTerrainChunk* ptChunk)
{
    if(ptChunk->aptChildren[0] == NULL)
        return false;
    if(ptChunk->aptChildren[0]->ptIndexHole == NULL) return false;
    if(ptChunk->aptChildren[1]->ptIndexHole == NULL) return false;
    if(ptChunk->aptChildren[2]->ptIndexHole == NULL) return false;
    if(ptChunk->aptChildren[3]->ptIndexHole == NULL) return false;
    return true;
}

static void
pl__free_chunk(plTerrain* ptTerrain, uint64_t uIndexCount)
{

    const uint64_t uCurrentFrame = gptIOI->get_io()->ulFrameCount;

    // find tail (least-recently used)
    plTerrainChunk* tail = ptTerrain->tReplacementQueue.ptNext;
    if (!tail) {
        // nothing to evict -> dead end
        printf("Couldn't free chunks (replacement list empty)\n");
        return;
    }
    while (tail->ptNext) tail = tail->ptNext;

    // Pass 1: try to evict a chunk that fully satisfies the need by size

    uint64_t freed = 0;
    // move to tail
    plTerrainChunk* c = ptTerrain->tReplacementQueue.ptNext;
    if (!c) return; 
    while (c->ptNext) c = c->ptNext;

    while (c && freed < uIndexCount)
    {
        plTerrainChunk* prev = c->ptPrev;
        freed += c->uIndexCount;
        pl__make_unresident(ptTerrain, c);
        c = prev;
    }

    if (freed >= uIndexCount)
        return;


    // Pass 2: age-based eviction
    for (c = tail; c; c = c->ptPrev)
    {
        if (uCurrentFrame - c->uLastFrameUsed > 30)
        {
            pl__make_unresident(ptTerrain, c);
            return;
        }
    }

    // Pass 3 (fallback): evict strictly by LRU, even if it was recently used.
    // This prevents deadlock when memory is insufficient to hold both parent and child.
    pl__make_unresident(ptTerrain, tail);

}

// Free from LRU tail until we have at least the requested bytes for BOTH pools.
// Never evict level 0, and never evict non-leaves (chunks with resident children).
static void
pl__free_chunk_until(plTerrain* P,
                     uint64_t idx_bytes_needed,
                     uint64_t vtx_bytes_needed)
{
    // Walk to tail (oldest / least-recently-used)
    plTerrainChunk* tail = P->tReplacementQueue.ptNext;
    if (!tail) {
        printf("Couldn't free chunks (replacement list empty)\n");
        return;
    }
    while (tail->ptNext) tail = tail->ptNext;

    uint64_t freed_idx = 0;
    uint64_t freed_vtx = 0;

    // Pass 1: strictly leaf, non-root, LRU → MRU until enough bytes
    for (plTerrainChunk* c = tail; c; c = c->ptPrev) {
        if (!c->ptIndexHole) continue;            // not resident -> skip
        if (c->uLevel == 0) continue;             // keep roots
        if (!pl__is_leaf_resident(c)) continue;   // don't drop nodes with resident children

        freed_idx += (uint64_t)c->uIndexCount * sizeof(uint32_t);
        freed_vtx += (uint64_t)c->ptVertexHole->uSize; // size was requested to freelist; safe to use

        pl__make_unresident(P, c);

        if (freed_idx >= idx_bytes_needed && freed_vtx >= vtx_bytes_needed)
            return; // sufficient
    }

    // Pass 2: allow evicting non-leaf (still avoid root). Prefer aged items.
    const uint64_t now = gptIOI->get_io()->ulFrameCount;
    for (plTerrainChunk* c = tail; c; c = c->ptPrev) {
        if (!c->ptIndexHole) continue;
        if (c->uLevel == 0) continue;
        if (now - c->uLastFrameUsed <= 30) continue;

        freed_idx += (uint64_t)c->uIndexCount * sizeof(uint32_t);
        freed_vtx += (uint64_t)c->ptVertexHole->uSize;
        pl__make_unresident(P, c);

        if (freed_idx >= idx_bytes_needed && freed_vtx >= vtx_bytes_needed)
            return; // ✅ sufficient
    }

    // Pass 3: final fallback — evict oldest non-root regardless of age/leaf.
    for (plTerrainChunk* c = tail; c; c = c->ptPrev) {
        if (!c->ptIndexHole) continue;
        if (c->uLevel == 0) continue;

        pl__make_unresident(P, c);
        return; // free at least one to make progress
    }

    // If we reached here, nothing could be evicted (only root is resident)
    printf("Eviction failed: only root or no resident chunks available\n");
}


void
pl__remove_from_residency_queue(plTerrain* ptTerrain, plTerrainChunk* ptChunk)
{
    plTerrainResidencyNode* ptCurrentRequest = ptTerrain->tRequestQueue.ptNext;

    plTerrainResidencyNode* ptExistingRequest = NULL;
    plTerrainResidencyNode* ptLastRequest = NULL;

    while(ptCurrentRequest)
    {
        if(ptCurrentRequest->ptChunk == ptChunk)
        {
            ptExistingRequest = ptCurrentRequest;
            break;
        }
        ptCurrentRequest = ptCurrentRequest->ptNext;
    }

    if(ptExistingRequest)
    {

        // remove node
        if(ptExistingRequest->ptPrev)
            ptExistingRequest->ptPrev->ptNext = ptExistingRequest->ptNext;
        else
            ptTerrain->tRequestQueue.ptNext = ptExistingRequest->ptNext;

        if(ptExistingRequest->ptNext)
            ptExistingRequest->ptNext->ptPrev = ptExistingRequest->ptPrev;

        uint32_t uIndex = (uint32_t)(ptExistingRequest - &ptTerrain->atRequests[0]);
        pl_sb_push(ptTerrain->sbuFreeRequests, uIndex);
    }
}

static void
pl__make_unresident(plTerrain* ptTerrain, plTerrainChunk* ptChunk)
{
    pl__remove_from_residency_queue(ptTerrain, ptChunk);
    pl__remove_from_replacement_queue(ptTerrain, ptChunk);
    if(ptChunk->ptIndexHole && ptChunk->uIndexCount > 0)
    {
        if(ptChunk->ptIndexHole)
            gptFreeList->return_node(&ptTerrain->tIndexBufferManager, ptChunk->ptIndexHole);
        if(ptChunk->ptVertexHole)
            gptFreeList->return_node(&ptTerrain->tVertexBufferManager, ptChunk->ptVertexHole);
        ptChunk->uIndexCount = 0;
        ptChunk->uLastFrameUsed = 0;
        ptChunk->ptIndexHole = NULL;
        ptChunk->ptVertexHole = NULL;
        ptChunk->ptVertexHole = NULL;
        if(ptChunk->aptChildren[0] != NULL)
        {
            pl__make_unresident(ptTerrain, ptChunk->aptChildren[0]);
            pl__make_unresident(ptTerrain, ptChunk->aptChildren[1]);
            pl__make_unresident(ptTerrain, ptChunk->aptChildren[2]);
            pl__make_unresident(ptTerrain, ptChunk->aptChildren[3]);
        }
    }
}

static void
pl__handle_residency(plTerrain* ptTerrain, plCommandBuffer* ptCommandBuffer)
{
    const uint64_t uCurrentFrame = gptIOI->get_io()->ulFrameCount;

    plTerrainResidencyNode* ptCurrentRequest = ptTerrain->tRequestQueue.ptNext;
    // ptCurrentRequest->uFrameRequested

    if(ptCurrentRequest)
    {

        plTerrainChunk* ptChunk = ptCurrentRequest->ptChunk;

        FILE* ptDataFile = fopen(ptTerrain->sbtChunkFiles[ptChunk->uFileID].tFile.acFile, "rb");
        fseek(ptDataFile, (long)ptChunk->szFileLocation, SEEK_SET);

        fseek(ptDataFile, sizeof(plVec3) * 2 + sizeof(int) * 4, SEEK_CUR);

        uint32_t uVertexCount = 0;
        fread(&uVertexCount, 1, sizeof(uint32_t), ptDataFile);

        plTerrainVertex* ptVertices = PL_ALLOC(uVertexCount * sizeof(plTerrainVertex));
        fread(ptVertices, 1, sizeof(plTerrainVertex) * uVertexCount, ptDataFile);

        uint32_t uIndexCount = 0;
        fread(&uIndexCount, 1, sizeof(uint32_t), ptDataFile);
        
        uint32_t* ptIndices = PL_ALLOC(uIndexCount * sizeof(uint32_t));
        fread(ptIndices, 1, sizeof(uint32_t) * uIndexCount, ptDataFile);

        


        // bytes needed for this chunk
        const uint64_t idx_bytes = (uint64_t)uIndexCount * sizeof(uint32_t);
        const uint64_t vtx_bytes = (uint64_t)uVertexCount * sizeof(plTerrainVertex);

        const uint64_t uVertexStageOffset = idx_bytes;
        const uint64_t uIndexStageOffset = 0;

        // Try once
        plFreeListNode* idx_hole = gptFreeList->get_node(&ptTerrain->tIndexBufferManager, idx_bytes);
        plFreeListNode* vtx_hole = gptFreeList->get_node(&ptTerrain->tVertexBufferManager, vtx_bytes);

        if (!idx_hole || !vtx_hole)
        {
            if (idx_hole) gptFreeList->return_node(&ptTerrain->tIndexBufferManager, idx_hole);
            if (vtx_hole) gptFreeList->return_node(&ptTerrain->tVertexBufferManager, vtx_hole);

            // Free enough for BOTH pools and try again
            pl__free_chunk_until(ptTerrain, idx_bytes, vtx_bytes);

            idx_hole = gptFreeList->get_node(&ptTerrain->tIndexBufferManager, idx_bytes);
            vtx_hole = gptFreeList->get_node(&ptTerrain->tVertexBufferManager, vtx_bytes);
        }

        if (!idx_hole || !vtx_hole)
        {
            if (idx_hole) gptFreeList->return_node(&ptTerrain->tIndexBufferManager, idx_hole);
            if (vtx_hole) gptFreeList->return_node(&ptTerrain->tVertexBufferManager, vtx_hole);

            PL_FREE(ptVertices);
            PL_FREE(ptIndices);
            fclose(ptDataFile);
            printf("No Memory (post-eviction)\n");
            return;
        }

        ptChunk->ptIndexHole = idx_hole;
        ptChunk->ptVertexHole = vtx_hole;
        ptChunk->uIndexCount  = uIndexCount;

        // update buffer offsets

        plDevice* ptDevice = gptTerrainCtx->ptDevice;
        plBuffer* ptStagingBuffer = gptGfx->get_buffer(ptDevice, gptTerrainCtx->tStagingBuffer);

        void* ptIndexStageDest = &ptStagingBuffer->tMemoryAllocation.pHostMapped[uIndexStageOffset];
        void* ptVertexStageDest = &ptStagingBuffer->tMemoryAllocation.pHostMapped[uVertexStageOffset];

        // copy memory to mapped staging buffer
        memcpy(ptIndexStageDest, ptIndices, idx_bytes);
        memcpy(ptVertexStageDest, ptVertices, vtx_bytes);

        // destination offsets
        const uint64_t uIndexFinalOffset = ptChunk->ptIndexHole->uOffset;
        const uint64_t uVertexFinalOffset = ptChunk->ptVertexHole->uOffset;
        
        // begin blit pass, copy buffer, end pass
        // NOTE: we are using the starter extension to get a blit encoder, later examples we will
        //       handle this ourselves
        plBlitEncoder* ptEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
        gptGfx->pipeline_barrier_blit(ptEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);

        gptGfx->copy_buffer(ptEncoder, gptTerrainCtx->tStagingBuffer, ptTerrain->tIndexBuffer, uIndexStageOffset, uIndexFinalOffset, idx_bytes);
        gptGfx->copy_buffer(ptEncoder, gptTerrainCtx->tStagingBuffer, ptTerrain->tVertexBuffer, uVertexStageOffset, uVertexFinalOffset, vtx_bytes);
        
        gptGfx->pipeline_barrier_blit(ptEncoder, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
        gptGfx->end_blit_pass(ptEncoder);
        // gptGfx->queue_buffer_for_deletion(ptDevice, tStagingBuffer);

        PL_FREE(ptVertices);
        PL_FREE(ptIndices);

        fclose(ptDataFile);

        pl__remove_from_residency_queue(ptTerrain, ptCurrentRequest->ptChunk);
        pl__touch_chunk(ptTerrain, ptChunk);
    }
}

static inline void
pl__lru_unlink(plTerrain* P, plTerrainChunk* c)
{
    if (!c->bInReplacementList) return;
    if (c->ptPrev) c->ptPrev->ptNext = c->ptNext;
    else           P->tReplacementQueue.ptNext = c->ptNext;
    if (c->ptNext) c->ptNext->ptPrev = c->ptPrev;
    c->ptPrev = c->ptNext = NULL;
    c->bInReplacementList = false;
}

static inline void
pl__lru_push_front(plTerrain* P, plTerrainChunk* c)
{
    // Head insert
    c->ptPrev = NULL;
    c->ptNext = P->tReplacementQueue.ptNext;
    if (c->ptNext) c->ptNext->ptPrev = c;
    P->tReplacementQueue.ptNext = c;
    c->bInReplacementList = true;
}


static void
pl__touch_chunk(plTerrain* P, plTerrainChunk* c)
{
    if (!c) return;
    c->uLastFrameUsed = gptIOI->get_io()->ulFrameCount;

    pl__lru_unlink(P, c);
    pl__lru_push_front(P, c);
}


// static void
// pl__touch_chunk(plTerrain* ptTerrain, plTerrainChunk* ptChunk)
// {
//     if(ptChunk == NULL)
//         return;

  
//     ptChunk->uLastFrameUsed = gptIOI->get_io()->ulFrameCount;

//     plTerrainChunk* ptCurrentRequest = ptTerrain->tReplacementQueue.ptNext;

//     plTerrainChunk* ptExistingRequest = NULL;

//     while(ptCurrentRequest)
//     {
//         if(ptCurrentRequest == ptChunk)
//         {
//             ptExistingRequest = ptCurrentRequest;
//             break;
//         }
//         ptCurrentRequest = ptCurrentRequest->ptNext;
//     }

//     if(ptExistingRequest)
//     {

//         // remove node
//         if(ptExistingRequest->ptPrev)
//             ptExistingRequest->ptPrev->ptNext = ptExistingRequest->ptNext;

//         if(ptExistingRequest->ptNext)
//             ptExistingRequest->ptNext->ptPrev = ptExistingRequest->ptPrev;   
//     }
//     else
//     {
//         ptExistingRequest = ptChunk;
//     }

//     // place node at beginning
//     ptExistingRequest->ptPrev = NULL;
//     if(ptExistingRequest != ptTerrain->tReplacementQueue.ptNext)
//         ptExistingRequest->ptNext = ptTerrain->tReplacementQueue.ptNext;
//     ptTerrain->tReplacementQueue.ptNext = ptExistingRequest;
//     if(ptExistingRequest->ptNext)
//         ptExistingRequest->ptNext->ptPrev = ptExistingRequest;

//     ptExistingRequest->uLastFrameUsed = gptIOI->get_io()->ulFrameCount;
// }


void
pl__remove_from_replacement_queue(plTerrain* ptTerrain, plTerrainChunk* ptChunk)
{
    pl__lru_unlink(ptTerrain, ptChunk);
}

static void
pl__request_residency(plTerrain* ptTerrain, plTerrainChunk* ptChunk)
{
    if(ptChunk == NULL)
        return;

    if(ptChunk->ptIndexHole == NULL)
    {
        plTerrainResidencyNode* ptCurrentRequest = ptTerrain->tRequestQueue.ptNext;

        plTerrainResidencyNode* ptExistingRequest = NULL;
        plTerrainResidencyNode* ptLastRequest = NULL;

        while(ptCurrentRequest)
        {
            if(ptCurrentRequest->ptChunk == ptChunk)
            {
                ptExistingRequest = ptCurrentRequest;
                break;
            }
            if(ptCurrentRequest->ptNext == NULL)
                ptLastRequest = ptCurrentRequest;
            ptCurrentRequest = ptCurrentRequest->ptNext;
        }

        if(ptExistingRequest)
        {

            // remove node
            if(ptExistingRequest->ptPrev)
                ptExistingRequest->ptPrev->ptNext = ptExistingRequest->ptNext;

            if(ptExistingRequest->ptNext)
                ptExistingRequest->ptNext->ptPrev = ptExistingRequest->ptPrev;    
        }
        else
        {
            if(pl_sb_size(ptTerrain->sbuFreeRequests) > 0)
            {
                uint32_t uNewIndex = pl_sb_pop(ptTerrain->sbuFreeRequests);
                ptExistingRequest = &ptTerrain->atRequests[uNewIndex];

            }
            else if(ptLastRequest)
            {
                if(ptLastRequest->ptPrev)
                    ptLastRequest->ptPrev->ptNext = NULL;
                ptExistingRequest = ptLastRequest; 
            }
            else
            {
                PL_ASSERT(false);
            }
        }

        // place node at beginning
        ptExistingRequest->ptPrev = NULL;
        if(ptExistingRequest != ptTerrain->tRequestQueue.ptNext)
            ptExistingRequest->ptNext = ptTerrain->tRequestQueue.ptNext;
        ptTerrain->tRequestQueue.ptNext = ptExistingRequest;
        if(ptExistingRequest->ptNext)
            ptExistingRequest->ptNext->ptPrev = ptExistingRequest;

        ptExistingRequest->uFrameRequested = gptIOI->get_io()->ulFrameCount;
        ptExistingRequest->ptChunk = ptChunk;
    }
}

static void
pl__render_chunk(plTerrain* ptTerrain, plCamera* ptCamera , plRenderEncoder* ptEncoder, plTerrainChunk* ptChunk, plTerrainChunkFile* ptFile, const plMat4* ptMVP)
{
    PL_ASSERT(ptChunk != NULL);

    plAABB tAABB = {
        .tMin = ptChunk->tMinBound,
        .tMax = ptChunk->tMaxBound
    };

    if(!pl__sat_visibility_test(ptCamera, &tAABB))
        return;

    plVec3 tClosestPoint = gptCollision->point_closest_point_aabb(ptCamera->tPos, tAABB);
    float fDistance = fabsf(pl_length_vec3(pl_sub_vec3(tClosestPoint, ptCamera->tPos)));

    pl__request_residency(ptTerrain, ptChunk);

    if(ptChunk->ptIndexHole == NULL)
        return;
    
    float fViewportWidth = gptIOI->get_io()->tMainViewportSize.x;
    float fHorizontalFieldOfView = 2.0f * atanf(tanf(0.5f * ptCamera->fFieldOfView) * ptCamera->fAspectRatio);

    float fK = fViewportWidth / (2.0f * tanf(0.5f * fHorizontalFieldOfView));

    float fGeometricError = ptFile->fMaxBaseError * (float)ptChunk->uLevel;
    float fRho = fGeometricError * fK / fDistance;


    float tauSubdivide = ptTerrain->tRuntimeOptions.fTau;
    float tauMerge     = tauSubdivide * 0.5f;

    bool bChildrenResident = pl__all_children_resident(ptChunk);

    // Decide refinement using hysteresis
    if(!bChildrenResident || fRho <= tauSubdivide)
    {
        // Draw parent
        const plMat4 tMVP = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);
        plDevice* ptDevice = gptTerrainCtx->ptDevice;
        plDynamicBinding tDynamicBinding = pl_allocate_dynamic_data(gptGfx, ptDevice, ptTerrain->ptCurrentDynamicBufferBlock);
        plGpuDynTerrainData* ptDynamic = (plGpuDynTerrainData*)tDynamicBinding.pcData;

        ptDynamic->tMvp            = tMVP;
        ptDynamic->iLevel          = (int)ptChunk->uLevel;
        ptDynamic->tFlags          = ptTerrain->tRuntimeOptions.tFlags;
        ptDynamic->uTextureIndex   = ptTerrain->sbtChunkFiles[ptChunk->uFileID].uTextureIndex;
        ptDynamic->tLightDirection = ptTerrain->tRuntimeOptions.tLightDirection;
        ptDynamic->tUVInfo.xy       = ptChunk->tUVScale;
        ptDynamic->tUVInfo.zw       = ptChunk->tUVOffset;

        plShaderHandle tShader =
            (ptTerrain->tRuntimeOptions.tFlags & PL_TERRAIN_FLAGS_WIREFRAME)
            ? ptTerrain->tWireframeShader : ptTerrain->tShader;

        gptGfx->bind_graphics_bind_groups(
            ptEncoder,
            tShader,
            0, 0,
            NULL,
            1, &tDynamicBinding
        );

        const plDrawIndex tDraw = {
            .uInstanceCount = 1,
            .uIndexCount    = ptChunk->uIndexCount,
            .uVertexStart   = (uint32_t)(ptChunk->ptVertexHole->uOffset / sizeof(plTerrainVertex)),
            .uIndexStart    = (uint32_t)(ptChunk->ptIndexHole->uOffset / sizeof(uint32_t)),
            .tIndexBuffer   = ptTerrain->tIndexBuffer
        };

        gptGfx->draw_indexed(ptEncoder, 1, &tDraw);
        *gptTerrainCtx->pdDrawCalls += 1;

        pl__touch_chunk(ptTerrain, ptChunk);

        // --- Hysteresis refinement logic ---
        if(fRho > tauSubdivide)
        {
            // Need children soon – schedule them
            for(uint32_t i = 0; i < 4; i++)
            {
                if(ptChunk->aptChildren[i])
                    pl__request_residency(ptTerrain, ptChunk->aptChildren[i]);
            }
        }
        else if(fRho < tauMerge)
        {
            // Clearly low detail – unload children
            pl__unload_children(ptTerrain, ptChunk);
        }
        // else: middle band → keep current state, prevent thrash
    }
    else
    {
        // Descend into children
        for(uint32_t i = 0; i < 4; i++)
            pl__render_chunk(ptTerrain, ptCamera, ptEncoder, ptChunk->aptChildren[i], ptFile, ptMVP);
    }
}

static bool
pl__sat_visibility_test(plCamera* ptCamera, const plAABB* ptAABB)
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

    plOBB2 tObb = {
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

static bool
pl__terrain_load(plTerrain* ptTerrain, plTerrainProcessInfo* ptInfo)
{
    {
        float fX = ptInfo->atTiles[0].tCenter.x;
        float fY = ptInfo->atTiles[0].tCenter.z;
        ptTerrain->tTopLeftGlobal.x = fX - 0.5f * (float)ptInfo->uSize * ptInfo->fMetersPerPixel;
        ptTerrain->tTopLeftGlobal.y = fY - 0.5f * (float)ptInfo->uSize * ptInfo->fMetersPerPixel;
    }

    {
        float fX = ptInfo->atTiles[ptInfo->uTileCount - 1].tCenter.x;
        float fY = ptInfo->atTiles[ptInfo->uTileCount - 1].tCenter.z;
        ptTerrain->tBottomRightGlobal.x = fX - 0.5f * (float)ptInfo->uSize * ptInfo->fMetersPerPixel;
        ptTerrain->tBottomRightGlobal.y = fY - 0.5f * (float)ptInfo->uSize * ptInfo->fMetersPerPixel;
    }

    for(uint32_t k = 0; k < ptInfo->uTileCount; k++)
    {
        uint32_t i = k % ptInfo->uHorizontalTiles;
        uint32_t j = (k - i) / ptInfo->uVerticalTiles;
        pl_chlod_load_chunk_file(ptTerrain, ptInfo->atTiles[k].acOutputFile);
    }
    return true;
}

static plTextureHandle
pl__terrain_create_texture_with_data(const plTextureDesc* ptDesc, const char* pcName, uint32_t uIdentifier, const void* pData, size_t szSize)
{
    // for convience
    plDevice* ptDevice = gptTerrainCtx->ptDevice;
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
 
    // create texture
    plTexture* ptTexture = NULL;
    const plTextureHandle tHandle = gptGfx->create_texture(ptDevice, ptDesc, &ptTexture);
    pl_temp_allocator_reset(&gptTerrainCtx->tTempAllocator);

    // choose allocator
    plDeviceMemoryAllocatorI* ptAllocator = gptTerrainCtx->tLocalBuddyAllocator;

    // allocate memory
    const plDeviceMemoryAllocation tAllocation = ptAllocator->allocate(ptAllocator->ptInst, 
        ptTexture->tMemoryRequirements.uMemoryTypeBits,
        ptTexture->tMemoryRequirements.ulSize,
        ptTexture->tMemoryRequirements.ulAlignment,
        pl_temp_allocator_sprintf(&gptTerrainCtx->tTempAllocator, "texture alloc %s: %u", pcName, uIdentifier));

    // bind memory
    gptGfx->bind_texture_to_memory(ptDevice, tHandle, &tAllocation);
    pl_temp_allocator_reset(&gptTerrainCtx->tTempAllocator);

    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "create texture 2");
    gptGfx->begin_command_recording(ptCommandBuffer, NULL);
    plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
    gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);
    gptGfx->set_texture_usage(ptBlitEncoder, tHandle, PL_TEXTURE_USAGE_SAMPLED, 0);


    // if data is presented, upload using staging buffer
    if(pData)
    {
        PL_ASSERT(ptDesc->uLayers == 1); // this is for simple textures right now

        // create staging buffer
        const plBufferDesc tStagingBufferDesc = {
            .tUsage      = PL_BUFFER_USAGE_TRANSFER_SOURCE,
            .szByteSize  = szSize,
            .pcDebugName = "temp staging buffer"
        };
        plBuffer* ptBuffer = NULL;
        plBufferHandle tStagingBuffer = gptGfx->create_buffer(ptDevice, &tStagingBufferDesc, &ptBuffer);

        // allocate memory for the vertex buffer
        const plDeviceMemoryAllocation tStagingBufferAllocation = gptGfx->allocate_memory(ptDevice,
            ptBuffer->tMemoryRequirements.ulSize,
            PL_MEMORY_FLAGS_HOST_VISIBLE | PL_MEMORY_FLAGS_HOST_COHERENT,
            ptBuffer->tMemoryRequirements.uMemoryTypeBits,
            "temp staging memory");

        gptGfx->bind_buffer_to_memory(ptDevice, tStagingBuffer, &tStagingBufferAllocation);
        memcpy(ptBuffer->tMemoryAllocation.pHostMapped, pData, szSize);
        
        const plBufferImageCopy tBufferImageCopy = {
            .uImageWidth = (uint32_t)ptDesc->tDimensions.x,
            .uImageHeight = (uint32_t)ptDesc->tDimensions.y,
            .uImageDepth = 1,
            .uLayerCount = 1
        };

        gptGfx->copy_buffer_to_texture(ptBlitEncoder, tStagingBuffer, tHandle, 1, &tBufferImageCopy);
        gptGfx->generate_mipmaps(ptBlitEncoder, tHandle);
        gptGfx->queue_buffer_for_deletion(ptDevice, tStagingBuffer);
    }

    gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
    gptGfx->end_blit_pass(ptBlitEncoder);
    gptGfx->end_command_recording(ptCommandBuffer);
    gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);
    gptGfx->return_command_buffer(ptCommandBuffer);
    return tHandle;
}

static void
pl__terrain_return_bindless_texture_index(plTextureHandle tTexture)
{
    uint64_t uIndex = 0;
    if(pl_hm_has_key_ex(&gptTerrainCtx->tTextureIndexHashmap, tTexture.uData, &uIndex))
    {
        pl_hm_remove(&gptTerrainCtx->tTextureIndexHashmap, tTexture.uData);
    }
}

static uint32_t
pl__terrain_get_bindless_texture_index(plTextureHandle tTexture)
{

    uint64_t uIndex = 0;
    if(pl_hm_has_key_ex(&gptTerrainCtx->tTextureIndexHashmap, tTexture.uData, &uIndex))
        return (uint32_t)uIndex;

    uint64_t ulValue = pl_hm_get_free_index(&gptTerrainCtx->tTextureIndexHashmap);
    if(ulValue == PL_DS_HASH_INVALID)
    {
        PL_ASSERT(gptTerrainCtx->uTextureIndexCount < PL_TERRAIN_MAX_BINDLESS_TEXTURES);
        ulValue = gptTerrainCtx->uTextureIndexCount++;

        // TODO: handle when greater than 4096
    }
    pl_hm_insert(&gptTerrainCtx->tTextureIndexHashmap, tTexture.uData, ulValue);
    
    const plBindGroupUpdateTextureData tGlobalTextureData[] = {
        {
            .tTexture = tTexture,
            .uSlot    = 1,
            .uIndex   = (uint32_t)ulValue,
            .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED
        },
    };

    plBindGroupUpdateData tGlobalBindGroupData = {
        .uTextureCount = 1,
        .atTextureBindings = tGlobalTextureData
    };

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        gptGfx->update_bind_group(gptTerrainCtx->ptDevice, gptTerrainCtx->atBindGroups[i], &tGlobalBindGroupData);

    return (uint32_t)ulValue;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_terrain_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plTerrainI tApi = {
        .initialize          = pl_terrain_initialize,
        .cleanup             = pl_terrain_cleanup,
        .create_terrain      = pl_create_terrain,
        .cleanup_terrain     = pl_cleanup_terrain,
        .prepare             = pl_prepare_terrain,
        .reload_shaders      = pl_terrain_load_shaders,
        .set_runtime_options = pl_terrain_set_runtime_options,
        .get_runtime_options = pl_terrain_get_runtime_options,
        .set_shaders         = pl_terrain_set_shaders,
        .set_texture         = pl_terrain_set_texture,
        .render              = pl_render_terrain
    };
    pl_set_api(ptApiRegistry, plTerrainI, &tApi);

    #ifndef PL_UNITY_BUILD
        gptMemory           = pl_get_api_latest(ptApiRegistry, plMemoryI);
        gptImage            = pl_get_api_latest(ptApiRegistry, plImageI);
        gptGfx              = pl_get_api_latest(ptApiRegistry, plGraphicsI);
        gptFreeList         = pl_get_api_latest(ptApiRegistry, plFreeListI);
        gptIOI              = pl_get_api_latest(ptApiRegistry, plIOI);
        gptStarter          = pl_get_api_latest(ptApiRegistry, plStarterI);
        gptShader           = pl_get_api_latest(ptApiRegistry, plShaderI);
        gptCollision        = pl_get_api_latest(ptApiRegistry, plCollisionI);
        gptScreenLog        = pl_get_api_latest(ptApiRegistry, plScreenLogI);
        gptTerrainProcessor = pl_get_api_latest(ptApiRegistry, plTerrainProcessorI);
        gptGpuAllocators    = pl_get_api_latest(ptApiRegistry, plGPUAllocatorsI);
        gptImageOps         = pl_get_api_latest(ptApiRegistry, plImageOpsI);
        gptVfs              = pl_get_api_latest(ptApiRegistry, plVfsI);
        gptResource         = pl_get_api_latest(ptApiRegistry, plResourceI);
        gptStats            = pl_get_api_latest(ptApiRegistry, plStatsI);
    #endif

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptTerrainCtx = ptDataRegistry->get_data("plTerrainContext");
    }
    else
    {
        static plTerrainContext tCtx = {0};
        gptTerrainCtx = &tCtx;
        ptDataRegistry->set_data("plTerrainContext", gptTerrainCtx);
    }
}

PL_EXPORT void
pl_unload_terrain_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    if(bReload)
        return;

    const plTerrainI* ptApi = pl_get_api_latest(ptApiRegistry, plTerrainI);
    ptApiRegistry->remove_api(ptApi);
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#ifndef PL_UNITY_BUILD

    #define PL_MEMORY_IMPLEMENTATION
    #include "pl_memory.h"

    #define PL_STRING_IMPLEMENTATION
    #include "pl_string.h"

#endif