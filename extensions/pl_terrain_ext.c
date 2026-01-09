/*
   pl_terrain_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] structs
// [SECTION] enums
// [SECTION] global data
// [SECTION] defines & macros
// [SECTION] internal helpers
// [SECTION] job system threads
// [SECTION] public api implementation
// [SECTION] internal helpers implementation
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <float.h>
#include "pl.h"
#include "pl_terrain_ext.h"

// libs
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"
#undef pl_vnsprintf
#include "pl_memory.h"
#include "pl_string.h"

// stable extensions
#include "pl_graphics_ext.h"
#include "pl_vfs_ext.h"
#include "pl_platform_ext.h"
#include "pl_shader_ext.h"
#include "pl_image_ext.h"
#include "pl_gpu_allocators_ext.h"
#include "pl_draw_ext.h"
#include "pl_ui_ext.h"
#include "pl_job_ext.h"

// unstable extensions
#include "pl_mesh_ext.h"
#include "pl_camera_ext.h"

// shader interop
#include "pl_shader_interop_terrain.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plTerrainBufferChunk plTerrainBufferChunk;
typedef struct _plTerrainContext     plTerrainContext;

// enums/flags
typedef int plTerrainDirection;
typedef int plTerrainTileFlags;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plTileCacheFileInfo
{
    uint32_t uVersion;
    uint32_t uXAlignmentOffset;
    uint32_t uYAlignmentOffset;
} plTileCacheFileInfo;

typedef struct _plHeightMapTile
{
    plTerrainTileFlags tFlags;

    plVec2   tWorldPos; // world position of top-left corner
    uint32_t uXOffset; // original texture offset
    uint32_t uYOffset; // original texture offset
    char     acFile[256];
    float    fMinHeight;
    float    fMaxHeight;

    // internal
    int32_t  _iXCoord;        // tile position in grid space
    int32_t  _iYCoord;        // tile position in grid space
    uint32_t _uXOffsetActual; // offset in terrain texture
    uint32_t _uYOffsetActual; // offset in terrain texture
    uint32_t _uChunkIndex;
    bool     _bEmpty;
} plHeightMapTile;

typedef struct _plTerrainBufferChunk
{
    size_t   szOffset;
    uint32_t uOwnerTileIndex;
} plTerrainBufferChunk;

typedef struct _plTerrainContext
{

    // new
    plRenderPassLayoutHandle tRenderPassLayoutHandle;
    uint32_t uSubpassIndex;

    bool bLoadedTextures;

    // graphics
    plDevice*        ptDevice;
    plBindGroupPool* ptBindGroupPool;
    plSamplerHandle  tSampler;
    plSamplerHandle  tMipSampler;
    plSamplerHandle  tMirrorSampler;
    plSamplerHandle  tClampSampler;
    uint32_t         uMeshLevels;

    // bind group layouts
    plBindGroupLayoutHandle tBindGroupLayout0;
    plBindGroupLayoutHandle tPreprocessBGLayout0;
    plBindGroupLayoutHandle tMipmapBGLayout0;

    // gpu allocators
    plDeviceMemoryAllocatorI* tLocalDedicatedAllocator;
    plDeviceMemoryAllocatorI* tLocalBuddyAllocator;
    plDeviceMemoryAllocatorI* tStagingDedicatedAllocator;
    plDeviceMemoryAllocatorI* tStagingBuddyAllocator;

    // CPU side data
    uint32_t  uIndexCount;
    uint32_t  uVertexCount;
    plVec3*   ptVertexBuffer;
    uint32_t* puIndexBuffer;

    // GPU side data
    plBufferHandle tIndexBuffer;
    plBufferHandle tVertexBuffer;
    plTextureHandle tDiffuseTexture;
    plTextureHandle tNoiseTexture;

    plDynamicDataBlock tCurrentDynamicDataBlock;

    // UI
    plUiTextFilter tFilter;

    plDrawList3D* pt3dDrawlist;
    size_t szHeightTexelStride;

    plTerrainFlags tFlags;
    bool bShowAtlas;
    bool bShowWorld;
    bool bShowLocal;

    // visualization options
    plVec3 tSunDirection;

    // bind groups
    plBindGroupHandle atBindGroup0[PL_MAX_FRAMES_IN_FLIGHT];
    plBindGroupHandle tMipmapBG0;
    plBindGroupHandle tPreprocessBG0;
    plBindGroupHandle tFullBG0;

    // height map
    uint32_t          uHeightMapResolution;
    uint32_t          uTileSize;
    float             fMetersPerTexel;
    float             fMaxElevation;
    float             fMinElevation;
    plVec2            tMinWorldPosition;
    plVec2            tMaxWorldPosition;
    uint32_t          uHorizontalTiles;
    uint32_t          uVerticalTiles;

    plBufferHandle tStagingBuffer;
    plBufferHandle tStagingBuffer2;
    
    plVec2 tCurrentExtent;
    plTerrainDirection tCurrentDirectionUpdate;
    bool               bNeedsUpdate;
    uint32_t uCurrentXOffset;
    int32_t iCurrentXCoordMin;
    int32_t iCurrentXCoordMax;

    uint32_t uCurrentYOffset;
    int32_t iCurrentYCoordMin;
    int32_t iCurrentYCoordMax;

    uint32_t         uTileCount;
    plHeightMapTile* atTiles;
    uint32_t*        sbuActiveTileIndices;
    uint32_t*        auFetchTileIndices;
    uint32_t         uNewActiveTileCount;
    uint32_t         uMaxActiveTiles;

    // tile prefetching
    uint32_t* auPrefetchTileIndices;
    uint32_t  uMaxPrefetchedTiles;
    uint32_t  uPrefetchRadius;

    plAtomicCounter* ptPrefetchDirty;
    plAtomicCounter* ptPrefetchCounter;
    plThread* ptPrefetchThread;
    bool      bPrefetchThreadRunning;
    plMutex*  ptPrefetchMutex;
    plConditionVariable* ptConditionVariable;
    plCriticalSection*   ptCriticalSection;
    bool bProcessAll;
    uint32_t* sbuPrefetchOverflow;

    uint32_t* sbuFreeChunks;
    plTerrainBufferChunk* atChunks;
    uint32_t  uChunkCapacity;

    // shaders
    plShaderHandle tFullShader;
    plShaderHandle tFullWireFrameShader;
    plShaderHandle tRegularShader;
    plShaderHandle tWireframeShader;

    // compute shaders
    plComputeShaderHandle tPreProcessHeightShader;
    plComputeShaderHandle tMipMapShader;

    // textures
    plTextureHandle tRawTexture;                                     // raw heightmap
    plTextureHandle tDummyTexture;                                   // raw heightmap
    plTextureHandle tProcessedTexture;                               // processed heightmap
    plTextureHandle tFullTexture;                                    // full heightmap
    plTextureHandle atActiveTexture[PL_MAX_FRAMES_IN_FLIGHT];        // actual heightmap in use (double buffered ideally)

    uint32_t uFullIndexCount;
    plBufferHandle tFullIndexBuffer;
    plBufferHandle tFullVertexBuffer;

    // low res stuff
    float fLowResMetersPerTexel;
} plTerrainContext;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plTerrainTileFlags
{
    PL_TERRAIN_TILE_FLAGS_NONE                   = 0,
    PL_TERRAIN_TILE_FLAGS_ACTIVE                 = 1 << 0,
    PL_TERRAIN_TILE_FLAGS_QUEUED                 = 1 << 1,
    PL_TERRAIN_TILE_FLAGS_UPLOADED               = 1 << 2,
    PL_TERRAIN_TILE_FLAGS_PROCESSED              = 1 << 3,
    PL_TERRAIN_TILE_FLAGS_PROCESSED_INTERMEDIATE = 1 << 4,
};

enum _plTerrainDirection
{
    PL_TERRAIN_DIRECTION_NONE  = 0,
    PL_TERRAIN_DIRECTION_EAST  = 1 << 0,
    PL_TERRAIN_DIRECTION_WEST  = 1 << 1,
    PL_TERRAIN_DIRECTION_NORTH = 1 << 2,
    PL_TERRAIN_DIRECTION_SOUTH = 1 << 3,
    PL_TERRAIN_DIRECTION_ALL   = PL_TERRAIN_DIRECTION_EAST | PL_TERRAIN_DIRECTION_WEST | PL_TERRAIN_DIRECTION_NORTH | PL_TERRAIN_DIRECTION_SOUTH,
};

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
    static const plGraphicsI*      gptGfx           = NULL;
    static const plVfsI*           gptVfs           = NULL;
    static const plMeshBuilderI*   gptMeshBuilder   = NULL;
    static const plShaderI*        gptShader        = NULL;
    static const plImageI*         gptImage         = NULL;
    static const plGPUAllocatorsI* gptGpuAllocators = NULL;
    static const plFileI*          gptFile          = NULL;
    static const plAtomicsI*       gptAtomics       = NULL;
    static const plCameraI*        gptCamera        = NULL;
    static const plDrawI*          gptDraw          = NULL;
    static const plUiI*            gptUI            = NULL;
    static const plIOI*            gptIOI           = NULL;
    static const plThreadsI*       gptThreads       = NULL;
    static const plJobI*           gptJob           = NULL;

#endif

// context
static plTerrainContext* gptTerrainCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] defines & macros
//-----------------------------------------------------------------------------

#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] internal helpers
//-----------------------------------------------------------------------------

static void            pl__terrain_create_shaders          (void);
static plTextureHandle pl__terrain_load_texture            (plCommandBuffer*, const char* pcFile);
static uint32_t        pl__terrain_tile_activation_distance(uint32_t uIndex);
static void            pl__terrain_return_free_chunk       (uint32_t uOwnerTileIndex);
static void            pl__terrain_clear_cache             (uint32_t uCount, uint32_t uRadius);
static void            pl__terrain_clear_cache_direction   (plTerrainDirection);
static void            pl__terrain_get_free_chunk          (uint32_t uOwnerTileIndex);
static bool            pl__terrain_process_height_map_tiles(plCamera*);
static plTextureHandle pl__terrain_create_texture          (plCommandBuffer* ptCmdBuffer, const plTextureDesc* ptDesc, const char* pcName, plTextureUsage);
static void            pl__terrain_prepare                 (plCommandBuffer*, plCamera*);
static void            pl__process_full_heightmap          (plCommandBuffer*);

static inline int
pl__terrain_get_tile_index_by_pos(int iX, int iY)
{
    if(iX < 0 || iY < 0 || iX >= (int)gptTerrainCtx->uHorizontalTiles || iY >= (int)gptTerrainCtx->uVerticalTiles)
        return -1;
    return iX + iY * (int)gptTerrainCtx->uHorizontalTiles;
}

static inline plHeightMapTile*
pl__terrain_get_tile_by_pos(int iX, int iY)
{
    int iIndex = pl__terrain_get_tile_index_by_pos(iX, iY);
    if(iIndex > -1)
        return &gptTerrainCtx->atTiles[iIndex];
    return NULL;
}

void pl__terrain_tile_height_map(plTerrainTilingInfo tInfo);

//-----------------------------------------------------------------------------
// [SECTION] job system threads
//-----------------------------------------------------------------------------

static bool
pl__push_prefetch_tile(uint32_t uTileIndex)
{

    plHeightMapTile* ptTile = &gptTerrainCtx->atTiles[uTileIndex];

    if(ptTile->tFlags & PL_TERRAIN_TILE_FLAGS_UPLOADED)
        return true;

    if(ptTile->tFlags & PL_TERRAIN_TILE_FLAGS_QUEUED)
        return true;

    // ptTile->tFlags &= ~PL_TERRAIN_TILE_FLAGS_PROCESSED;
    gptThreads->wake_condition_variable(gptTerrainCtx->ptConditionVariable);

    if(gptAtomics->atomic_load(gptTerrainCtx->ptPrefetchCounter) > gptTerrainCtx->uMaxPrefetchedTiles - 1)
    {
        pl_sb_push(gptTerrainCtx->sbuPrefetchOverflow, uTileIndex);
        return false;
    }
    
    // wait if max is set
    // while(gptAtomics->atomic_load(gptTerrainCtx->ptPrefetchCounter) > gptTerrainCtx->uMaxPrefetchedTiles - 1)
    // {
    //     gptThreads->wake_condition_variable(gptTerrainCtx->ptConditionVariable);
    // }
    gptThreads->lock_mutex(gptTerrainCtx->ptPrefetchMutex);

    pl__terrain_get_free_chunk(uTileIndex);
    ptTile->tFlags |= PL_TERRAIN_TILE_FLAGS_QUEUED;
    gptTerrainCtx->auPrefetchTileIndices[gptAtomics->atomic_increment(gptTerrainCtx->ptPrefetchCounter) - 1] = uTileIndex;   
    gptThreads->unlock_mutex(gptTerrainCtx->ptPrefetchMutex);
    return true;
}

static void*
pl__prefetch_thread(void* pData)
{
    size_t szTileFileSize = gptTerrainCtx->uTileSize * gptTerrainCtx->uTileSize * gptTerrainCtx->szHeightTexelStride;

    while(gptTerrainCtx->bPrefetchThreadRunning)
    {

        int64_t uPrefetchCount = gptAtomics->atomic_load(gptTerrainCtx->ptPrefetchCounter);
        if(uPrefetchCount > 0)
        {
            // gptThreads->sleep_thread(300);
            gptThreads->lock_mutex(gptTerrainCtx->ptPrefetchMutex);
            uint32_t uTileIndex = gptTerrainCtx->auPrefetchTileIndices[gptAtomics->atomic_load(gptTerrainCtx->ptPrefetchCounter) - 1];
            gptAtomics->atomic_decrement(gptTerrainCtx->ptPrefetchCounter); 
            gptThreads->unlock_mutex(gptTerrainCtx->ptPrefetchMutex);

            if(!gptTerrainCtx->bPrefetchThreadRunning)
                return NULL;

            plHeightMapTile* ptTile = &gptTerrainCtx->atTiles[uTileIndex];

            if(ptTile->_uChunkIndex != UINT32_MAX)
            {
                plBuffer* ptStagingBuffer = gptGfx->get_buffer(gptTerrainCtx->ptDevice, gptTerrainCtx->tStagingBuffer);

                const plTerrainBufferChunk* ptChunk = &gptTerrainCtx->atChunks[ptTile->_uChunkIndex];

                if(ptTile->_bEmpty)
                    memset((uint8_t*)&ptStagingBuffer->tMemoryAllocation.pHostMapped[ptChunk->szOffset], 0, szTileFileSize);
                else
                    gptFile->binary_read(ptTile->acFile, &szTileFileSize, (uint8_t*)&ptStagingBuffer->tMemoryAllocation.pHostMapped[ptChunk->szOffset]);

                ptTile->tFlags |= PL_TERRAIN_TILE_FLAGS_UPLOADED;
                gptAtomics->atomic_store(gptTerrainCtx->ptPrefetchDirty, 1);
                if(ptTile->tFlags & PL_TERRAIN_TILE_FLAGS_ACTIVE)
                {
                    ptTile->tFlags &= ~PL_TERRAIN_TILE_FLAGS_PROCESSED_INTERMEDIATE;
                    ptTile->tFlags &= ~PL_TERRAIN_TILE_FLAGS_PROCESSED;
                    plHeightMapTile* ptNeighborTile = NULL;
                    ptNeighborTile = pl__terrain_get_tile_by_pos(ptTile->_iXCoord + 1, ptTile->_iYCoord);
                    if(ptNeighborTile)
                    {
                        ptNeighborTile->tFlags &= ~PL_TERRAIN_TILE_FLAGS_PROCESSED;
                        ptNeighborTile->tFlags &= ~PL_TERRAIN_TILE_FLAGS_PROCESSED_INTERMEDIATE;
                    }
                    ptNeighborTile = pl__terrain_get_tile_by_pos(ptTile->_iXCoord - 1, ptTile->_iYCoord);
                    if(ptNeighborTile)
                    {
                        ptNeighborTile->tFlags &= ~PL_TERRAIN_TILE_FLAGS_PROCESSED;
                        ptNeighborTile->tFlags &= ~PL_TERRAIN_TILE_FLAGS_PROCESSED_INTERMEDIATE;
                    }
                    ptNeighborTile = pl__terrain_get_tile_by_pos(ptTile->_iXCoord, ptTile->_iYCoord + 1);
                     if(ptNeighborTile)
                    {
                        ptNeighborTile->tFlags &= ~PL_TERRAIN_TILE_FLAGS_PROCESSED;
                        ptNeighborTile->tFlags &= ~PL_TERRAIN_TILE_FLAGS_PROCESSED_INTERMEDIATE;
                    }
                    ptNeighborTile = pl__terrain_get_tile_by_pos(ptTile->_iXCoord, ptTile->_iYCoord - 1);
                    if(ptNeighborTile)
                    {
                        ptNeighborTile->tFlags &= ~PL_TERRAIN_TILE_FLAGS_PROCESSED;
                        ptNeighborTile->tFlags &= ~PL_TERRAIN_TILE_FLAGS_PROCESSED_INTERMEDIATE;
                    }
                }
                
            }
            ptTile->tFlags &= ~PL_TERRAIN_TILE_FLAGS_QUEUED;
        }
        else
        {
            gptTerrainCtx->bProcessAll = true;
            gptThreads->enter_critical_section(gptTerrainCtx->ptCriticalSection);
            gptThreads->sleep_condition_variable(gptTerrainCtx->ptConditionVariable, gptTerrainCtx->ptCriticalSection);
            gptThreads->leave_critical_section(gptTerrainCtx->ptCriticalSection);
        }
    }

    return NULL;
}

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_initialize_terrain(plCommandBuffer* ptCmdBuffer, plTerrainInit tInit)
{

    gptJob->initialize((plJobSystemInit){0});
    plDevice* ptDevice = tInit.ptDevice;
    gptTerrainCtx->ptDevice = tInit.ptDevice;
    gptTerrainCtx->uSubpassIndex = tInit.uSubpassIndex;
    gptTerrainCtx->tRenderPassLayoutHandle = *tInit.ptRenderPassLayoutHandle;

    // retrieve GPU allocators
    gptTerrainCtx->tLocalDedicatedAllocator   = gptGpuAllocators->get_local_dedicated_allocator(ptDevice);
    gptTerrainCtx->tLocalBuddyAllocator       = gptGpuAllocators->get_local_buddy_allocator(ptDevice);
    gptTerrainCtx->tStagingDedicatedAllocator = gptGpuAllocators->get_staging_uncached_allocator(ptDevice);
    gptTerrainCtx->tStagingBuddyAllocator     = gptGpuAllocators->get_staging_uncached_buddy_allocator(ptDevice);

    // create bind group pool
    plBindGroupPoolDesc tBindGroupPoolDesc = {
        .tFlags                   = PL_BIND_GROUP_POOL_FLAGS_NONE,
        .szSamplerBindings        = 10,
        .szSampledTextureBindings = 10,
        .szStorageTextureBindings = 10,
        .szStorageBufferBindings  = 10
    };
    gptTerrainCtx->ptBindGroupPool = gptGfx->create_bind_group_pool(ptDevice, &tBindGroupPoolDesc);

    // create samplers
    plSamplerDesc tSamplerDesc = {
        .tMagFilter     = PL_FILTER_LINEAR,
        .tMinFilter     = PL_FILTER_LINEAR,
        .tMipmapMode    = PL_MIPMAP_MODE_NEAREST,
        .fMaxAnisotropy = 0.0f,
        .fMinMip        = 0.0f,
        .fMaxMip        = 64.0f,
        .tVAddressMode  = PL_ADDRESS_MODE_CLAMP,
        .tUAddressMode  = PL_ADDRESS_MODE_CLAMP,
        .pcDebugName    = "linear clamp sampler",
    };
    gptTerrainCtx->tSampler = gptGfx->create_sampler(ptDevice, &tSamplerDesc);

    tSamplerDesc.tMagFilter = PL_FILTER_NEAREST;
    tSamplerDesc.tMinFilter = PL_FILTER_NEAREST;
    tSamplerDesc.pcDebugName    = "nearest clamp sampler",
    gptTerrainCtx->tMipSampler  = gptGfx->create_sampler(ptDevice, &tSamplerDesc);

    plSamplerDesc tSamplerDesc2 = {
        .tMagFilter     = PL_FILTER_LINEAR,
        .tMinFilter     = PL_FILTER_LINEAR,
        .tMipmapMode    = PL_MIPMAP_MODE_LINEAR,
        .fMaxAnisotropy = 0.0f,
        .fMinMip        = 0.0f,
        .fMaxMip        = 64.0f,
        .tVAddressMode  = PL_ADDRESS_MODE_WRAP,
        .tUAddressMode  = PL_ADDRESS_MODE_WRAP,
        .pcDebugName    = "linear mirror sampler",
    };
    gptTerrainCtx->tMirrorSampler = gptGfx->create_sampler(ptDevice, &tSamplerDesc2);

    plSamplerDesc tSamplerDesc3 = {
        .tMagFilter     = PL_FILTER_LINEAR,
        .tMinFilter     = PL_FILTER_LINEAR,
        .tMipmapMode    = PL_MIPMAP_MODE_LINEAR,
        .fMaxAnisotropy = 0.0f,
        .fMinMip        = 0.0f,
        .fMaxMip        = 64.0f,
        .tVAddressMode  = PL_ADDRESS_MODE_WRAP,
        .tUAddressMode  = PL_ADDRESS_MODE_WRAP,
        // .fMaxMip        = 1.0f,
        // .tVAddressMode  = PL_ADDRESS_MODE_CLAMP_TO_BORDER,
        // .tUAddressMode  = PL_ADDRESS_MODE_CLAMP_TO_BORDER,
        // .tBorderColor   = PL_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        .pcDebugName    = "linear mirror sampler",
    };
    gptTerrainCtx->tClampSampler = gptGfx->create_sampler(ptDevice, &tSamplerDesc3);

    // bind group layouts
    plBindGroupLayoutDesc tBindGroupLayout0 = {
        .atSamplerBindings = {
            {.uSlot = 0, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT },
            {.uSlot = 4, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT }
        },
        .atTextureBindings = {
            {.uSlot = 1, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED },
            {.uSlot = 2, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED },
            {.uSlot = 3, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED },
            {.uSlot = 5, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED },
        }
    };
    gptTerrainCtx->tBindGroupLayout0 = gptGfx->create_bind_group_layout(ptDevice, &tBindGroupLayout0);

    plBindGroupLayoutDesc tPreprocessBGLayout0Desc = {
        .atTextureBindings = {
            {.uSlot = 0, .tStages = PL_SHADER_STAGE_COMPUTE, .tType = PL_TEXTURE_BINDING_TYPE_STORAGE },
            {.uSlot = 1, .tStages = PL_SHADER_STAGE_COMPUTE, .tType = PL_TEXTURE_BINDING_TYPE_STORAGE },
        }
    };
    gptTerrainCtx->tPreprocessBGLayout0 = gptGfx->create_bind_group_layout(ptDevice, &tPreprocessBGLayout0Desc);

    plBindGroupLayoutDesc tMipmapBGLayout0Desc = {
        .atSamplerBindings = {
            {.uSlot = 0, .tStages = PL_SHADER_STAGE_COMPUTE }
        },
        .atTextureBindings = {
            {.uSlot = 1, .tStages = PL_SHADER_STAGE_COMPUTE, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED },
            {.uSlot = 2, .tStages = PL_SHADER_STAGE_COMPUTE, .tType = PL_TEXTURE_BINDING_TYPE_STORAGE },
        }
    };
    gptTerrainCtx->tMipmapBGLayout0 = gptGfx->create_bind_group_layout(ptDevice, &tMipmapBGLayout0Desc);

    if(!gptTerrainCtx->bLoadedTextures)
    {
        gptTerrainCtx->tNoiseTexture   = pl__terrain_load_texture(ptCmdBuffer, "/assets/rock.jpg");
        gptTerrainCtx->tDiffuseTexture = pl__terrain_load_texture(ptCmdBuffer, "/assets/grass.png");
        gptTerrainCtx->bLoadedTextures = true;
    }

    gptTerrainCtx->pt3dDrawlist = gptDraw->request_3d_drawlist();

    gptTerrainCtx->tFlags                  = PL_TERRAIN_FLAGS_SHOW_BOUNDARY | PL_TERRAIN_FLAGS_CACHE_TILES | PL_TERRAIN_FLAGS_TILE_STREAMING | PL_TERRAIN_FLAGS_HIGH_RES | PL_TERRAIN_FLAGS_LOW_RES;
    gptTerrainCtx->uHeightMapResolution    = 4096 / 2;
    gptTerrainCtx->uTileSize               = 512 / 2;
    gptTerrainCtx->fMetersPerTexel         = tInit.fMetersPerTexel;
    gptTerrainCtx->fMaxElevation           = tInit.fMaxElevation;
    gptTerrainCtx->fMinElevation           = tInit.fMinElevation;
    gptTerrainCtx->tMaxWorldPosition       = tInit.tMaxPosition;
    gptTerrainCtx->tMinWorldPosition       = tInit.tMinPosition;

    gptTerrainCtx->tCurrentDirectionUpdate = PL_TERRAIN_DIRECTION_ALL;
    gptTerrainCtx->bNeedsUpdate = true;
    gptTerrainCtx->tSunDirection           = (plVec3){-1.0f, -1.0f, -1.0f};
    gptTerrainCtx->szHeightTexelStride     = sizeof(uint16_t);
    pl__terrain_create_shaders();

    const plTextureDesc tTextureDesc = {
        .tDimensions = { (float)gptTerrainCtx->uHeightMapResolution, (float)gptTerrainCtx->uHeightMapResolution, 1},
        .tFormat     = PL_FORMAT_R16_UINT,
        .uLayers     = 1,
        .uMips       = 1,
        .tType       = PL_TEXTURE_TYPE_2D,
        .tUsage      = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_STORAGE,
        .pcDebugName = "raw height map texture",
    };
    gptTerrainCtx->tRawTexture = gptGfx->create_texture(ptDevice, &tTextureDesc, NULL);

    plTextureDesc tProcessedTextureDesc = {
        .tDimensions = { (float)gptTerrainCtx->uHeightMapResolution, (float)gptTerrainCtx->uHeightMapResolution, 1},
        .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers     = 1,
        // .uMips       = 0,
        .tType       = PL_TEXTURE_TYPE_2D,
        .tUsage      = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_STORAGE,
        .pcDebugName = "process height map texture",
    };

    tProcessedTextureDesc.uMips = (uint32_t)floorf(log2f((float)pl_maxi((int)tProcessedTextureDesc.tDimensions.x, (int)tProcessedTextureDesc.tDimensions.y))) + 1u;
    tProcessedTextureDesc.uMips = pl_minu(tProcessedTextureDesc.uMips, gptTerrainCtx->uMeshLevels + 1);
    gptTerrainCtx->tProcessedTexture = gptGfx->create_texture(ptDevice, &tProcessedTextureDesc, NULL);

    tProcessedTextureDesc.pcDebugName = "active height map texture";
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        gptTerrainCtx->atActiveTexture[i] = gptGfx->create_texture(ptDevice, &tProcessedTextureDesc, NULL);

    tProcessedTextureDesc.pcDebugName = "dummy texture";
    tProcessedTextureDesc.uMips = 1;
    gptTerrainCtx->tDummyTexture = gptGfx->create_texture(ptDevice, &tProcessedTextureDesc, NULL);

    tProcessedTextureDesc.pcDebugName = "full texture";
    gptTerrainCtx->tFullTexture = gptGfx->create_texture(ptDevice, &tProcessedTextureDesc, NULL);

    // retrieve new texture (also could have used out param from create_texture above)
    plTexture* ptRawTexture   = gptGfx->get_texture(ptDevice, gptTerrainCtx->tRawTexture);
    plTexture* ptDummyTexture = gptGfx->get_texture(ptDevice, gptTerrainCtx->tDummyTexture);
    plTexture* ptTexture      = gptGfx->get_texture(ptDevice, gptTerrainCtx->tProcessedTexture);
    plTexture* ptFullTexture  = gptGfx->get_texture(ptDevice, gptTerrainCtx->tFullTexture);

    size_t szBuddyBlockSize = gptGpuAllocators->get_buddy_block_size();

    plDeviceMemoryAllocatorI* ptAllocator = gptTerrainCtx->tLocalDedicatedAllocator;

    if(ptTexture->tMemoryRequirements.ulSize < szBuddyBlockSize)
        ptAllocator = gptTerrainCtx->tLocalBuddyAllocator;

    // allocate memory blocks
    const plDeviceMemoryAllocation tRawTextureAllocation = ptAllocator->allocate(ptAllocator->ptInst,
        ptRawTexture->tMemoryRequirements.uMemoryTypeBits,
        ptRawTexture->tMemoryRequirements.ulSize,
        ptRawTexture->tMemoryRequirements.ulAlignment,
        "raw heightmap memory");

    const plDeviceMemoryAllocation tProcessedTextureAllocation = ptAllocator->allocate(ptAllocator->ptInst,
        ptTexture->tMemoryRequirements.uMemoryTypeBits,
        ptTexture->tMemoryRequirements.ulSize,
        ptTexture->tMemoryRequirements.ulAlignment,
        "processed heightmap memory");

    const plDeviceMemoryAllocation tDummyTextureAllocation = ptAllocator->allocate(ptAllocator->ptInst,
        ptDummyTexture->tMemoryRequirements.uMemoryTypeBits,
        ptDummyTexture->tMemoryRequirements.ulSize,
        ptDummyTexture->tMemoryRequirements.ulAlignment,
        "dummy heightmap memory");

    const plDeviceMemoryAllocation tFullTextureAllocation = ptAllocator->allocate(ptAllocator->ptInst,
        ptFullTexture->tMemoryRequirements.uMemoryTypeBits,
        ptFullTexture->tMemoryRequirements.ulSize,
        ptFullTexture->tMemoryRequirements.ulAlignment,
        "full heightmap memory");

    // bind memory blocks
    gptGfx->bind_texture_to_memory(ptDevice, gptTerrainCtx->tRawTexture, &tRawTextureAllocation);
    gptGfx->bind_texture_to_memory(ptDevice, gptTerrainCtx->tProcessedTexture, &tProcessedTextureAllocation);
    gptGfx->bind_texture_to_memory(ptDevice, gptTerrainCtx->tDummyTexture, &tDummyTextureAllocation);
    gptGfx->bind_texture_to_memory(ptDevice, gptTerrainCtx->tFullTexture, &tFullTextureAllocation);

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        const plDeviceMemoryAllocation tActiveTextureAllocation = ptAllocator->allocate(ptAllocator->ptInst,
            ptTexture->tMemoryRequirements.uMemoryTypeBits,
            ptTexture->tMemoryRequirements.ulSize,
            ptTexture->tMemoryRequirements.ulAlignment,
            "active heightmap memory");
        gptGfx->bind_texture_to_memory(ptDevice, gptTerrainCtx->atActiveTexture[i], &tActiveTextureAllocation);
    }

    gptTerrainCtx->uPrefetchRadius = 2; // lets try to keep this small until we have priority prefetching

    const uint32_t uTilesAcross = gptTerrainCtx->uHeightMapResolution / gptTerrainCtx->uTileSize;
    gptTerrainCtx->uMaxPrefetchedTiles = uTilesAcross * gptTerrainCtx->uPrefetchRadius * 4 + gptTerrainCtx->uPrefetchRadius * gptTerrainCtx->uPrefetchRadius * 4;
    gptTerrainCtx->uMaxPrefetchedTiles *= 2;
    gptTerrainCtx->uMaxActiveTiles = uTilesAcross * uTilesAcross;

    // create staging buffer
    plBufferDesc tStagingBufferDesc = {
        .tUsage      = PL_BUFFER_USAGE_TRANSFER_SOURCE | PL_BUFFER_USAGE_TRANSFER_DESTINATION,
        .szByteSize  = (gptTerrainCtx->uMaxPrefetchedTiles + gptTerrainCtx->uMaxActiveTiles) * (gptTerrainCtx->uTileSize * gptTerrainCtx->uTileSize * gptTerrainCtx->szHeightTexelStride),
        .pcDebugName = "staging buffer"
    };

    plBuffer* ptStagingBuffer = NULL;
    gptTerrainCtx->tStagingBuffer = gptGfx->create_buffer(ptDevice, &tStagingBufferDesc, &ptStagingBuffer);
    

    // allocate memory for the vertex buffer
    ptAllocator = gptTerrainCtx->tStagingDedicatedAllocator;
    const plDeviceMemoryAllocation tStagingBufferAllocation = ptAllocator->allocate(ptAllocator->ptInst,
        ptStagingBuffer->tMemoryRequirements.uMemoryTypeBits,
        ptStagingBuffer->tMemoryRequirements.ulSize,
        0,
        "staging buffer memory");

    gptGfx->bind_buffer_to_memory(ptDevice, gptTerrainCtx->tStagingBuffer, &tStagingBufferAllocation);
    memset(ptStagingBuffer->tMemoryAllocation.pHostMapped, 0, ptStagingBuffer->tMemoryRequirements.ulSize);

    plBufferDesc tStagingBufferDesc2 = {
        .tUsage      = PL_BUFFER_USAGE_TRANSFER_DESTINATION | PL_BUFFER_USAGE_TRANSFER_SOURCE,
        .szByteSize  = gptTerrainCtx->uHeightMapResolution * gptTerrainCtx->uHeightMapResolution * gptTerrainCtx->szHeightTexelStride * 2,
        .pcDebugName = "staging buffer2"
    };
    plBuffer* ptStagingBuffer2 = NULL;
    gptTerrainCtx->tStagingBuffer2 = gptGfx->create_buffer(ptDevice, &tStagingBufferDesc2, &ptStagingBuffer2);

    const plDeviceMemoryAllocation tStagingBufferAllocation2 = ptAllocator->allocate(ptAllocator->ptInst,
        ptStagingBuffer2->tMemoryRequirements.uMemoryTypeBits,
        ptStagingBuffer2->tMemoryRequirements.ulSize,
        0,
        "staging buffer memory2");

    gptGfx->bind_buffer_to_memory(ptDevice, gptTerrainCtx->tStagingBuffer2, &tStagingBufferAllocation2);
    memset(ptStagingBuffer2->tMemoryAllocation.pHostMapped, 0, ptStagingBuffer2->tMemoryRequirements.ulSize);
    
    gptTerrainCtx->auPrefetchTileIndices = PL_ALLOC(sizeof(uint32_t) * gptTerrainCtx->uMaxPrefetchedTiles);
    memset(gptTerrainCtx->auPrefetchTileIndices, 255, sizeof(uint32_t) * gptTerrainCtx->uMaxPrefetchedTiles);
    gptTerrainCtx->auFetchTileIndices = PL_ALLOC(sizeof(uint32_t) * gptTerrainCtx->uMaxActiveTiles);
    memset(gptTerrainCtx->auFetchTileIndices, 255, sizeof(uint32_t) * gptTerrainCtx->uMaxActiveTiles);

    gptTerrainCtx->uChunkCapacity = gptTerrainCtx->uMaxPrefetchedTiles + gptTerrainCtx->uMaxActiveTiles;
    gptTerrainCtx->atChunks = PL_ALLOC(gptTerrainCtx->uChunkCapacity * sizeof(plTerrainBufferChunk));
    pl_sb_resize(gptTerrainCtx->sbuFreeChunks, gptTerrainCtx->uChunkCapacity);
    for(uint32_t i = 0; i < gptTerrainCtx->uChunkCapacity; i++)
    {
        gptTerrainCtx->atChunks[i].szOffset = gptTerrainCtx->uTileSize * gptTerrainCtx->uTileSize * gptTerrainCtx->szHeightTexelStride * i;
        gptTerrainCtx->atChunks[i].uOwnerTileIndex = UINT32_MAX;
        gptTerrainCtx->sbuFreeChunks[i] = i;
    }

    // create bindgroup
    plBindGroupDesc tBindGroupDesc = {
        .tLayout = gptTerrainCtx->tBindGroupLayout0,
        .ptPool  = gptTerrainCtx->ptBindGroupPool,
        .pcDebugName = "bind group 0"
    };

    plBindGroupUpdateSamplerData atSamplerData[] = {
        {
            .tSampler = gptTerrainCtx->tSampler,
            .uSlot    = 0
        },
        {
            .tSampler = gptTerrainCtx->tMirrorSampler,
            .uSlot    = 4
        },
    };
    

    // set initial texture usages
    plBlitEncoder* ptBlit = gptGfx->begin_blit_pass(ptCmdBuffer);
    gptGfx->pipeline_barrier_blit(ptBlit, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);
    gptGfx->set_texture_usage(ptBlit, gptTerrainCtx->tDummyTexture, PL_TEXTURE_USAGE_STORAGE, 0);
    gptGfx->set_texture_usage(ptBlit, gptTerrainCtx->tRawTexture, PL_TEXTURE_USAGE_STORAGE, 0);
    gptGfx->set_texture_usage(ptBlit, gptTerrainCtx->tProcessedTexture, PL_TEXTURE_USAGE_SAMPLED, 0);
    gptGfx->set_texture_usage(ptBlit, gptTerrainCtx->tFullTexture, PL_TEXTURE_USAGE_SAMPLED, 0);
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        gptGfx->set_texture_usage(ptBlit, gptTerrainCtx->atActiveTexture[i], PL_TEXTURE_USAGE_SAMPLED, 0);
    }
    gptGfx->pipeline_barrier_blit(ptBlit, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
    gptGfx->end_blit_pass(ptBlit);

    gptTerrainCtx->tFullBG0 = gptGfx->create_bind_group(ptDevice, &tBindGroupDesc);

    // create & update static bind groups
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        gptTerrainCtx->atBindGroup0[i] = gptGfx->create_bind_group(ptDevice, &tBindGroupDesc);
        
        plBindGroupUpdateTextureData atTextureData[] = 
        {
            {
                .tTexture = gptTerrainCtx->atActiveTexture[i],
                .uSlot    = 1,
                .tType    = PL_TEXTURE_BINDING_TYPE_SAMPLED
            },
            {
                .tTexture = gptTerrainCtx->tNoiseTexture,
                .uSlot    = 2,
                .tType    = PL_TEXTURE_BINDING_TYPE_SAMPLED
            },
            {
                .tTexture = gptTerrainCtx->tDiffuseTexture,
                .uSlot    = 3,
                .tType    = PL_TEXTURE_BINDING_TYPE_SAMPLED
            },
            {
                .tTexture = gptTerrainCtx->tFullTexture,
                .uSlot    = 5,
                .tType    = PL_TEXTURE_BINDING_TYPE_SAMPLED
            },
        };

        plBindGroupUpdateData tBGData = {
            .uSamplerCount     = 2,
            .atSamplerBindings = atSamplerData,
            .uTextureCount     = 4,
            .atTextureBindings = atTextureData
        };
        gptGfx->update_bind_group(ptDevice, gptTerrainCtx->atBindGroup0[i], &tBGData);
    }

    {
        plBindGroupUpdateTextureData atTextureData[] = 
        {
            {
                .tTexture = gptTerrainCtx->tFullTexture,
                .uSlot    = 1,
                .tType    = PL_TEXTURE_BINDING_TYPE_SAMPLED
            },
            {
                .tTexture = gptTerrainCtx->tNoiseTexture,
                .uSlot    = 2,
                .tType    = PL_TEXTURE_BINDING_TYPE_SAMPLED
            },
            {
                .tTexture = gptTerrainCtx->tDiffuseTexture,
                .uSlot    = 3,
                .tType    = PL_TEXTURE_BINDING_TYPE_SAMPLED
            },
            {
                .tTexture = gptTerrainCtx->tFullTexture,
                .uSlot    = 5,
                .tType    = PL_TEXTURE_BINDING_TYPE_SAMPLED
            },
        };

        atSamplerData[1].tSampler = gptTerrainCtx->tClampSampler;
        plBindGroupUpdateData tBGData = {
            .uSamplerCount     = 2,
            .atSamplerBindings = atSamplerData,
            .uTextureCount     = 4,
            .atTextureBindings = atTextureData
        };
        gptGfx->update_bind_group(ptDevice, gptTerrainCtx->tFullBG0, &tBGData);
    }

    plBindGroupDesc tPreprocessBG0Desc = {
        .tLayout = gptTerrainCtx->tPreprocessBGLayout0,
        .ptPool = gptTerrainCtx->ptBindGroupPool,
        .pcDebugName = "compute bind group 0"
    };
    gptTerrainCtx->tPreprocessBG0 = gptGfx->create_bind_group(ptDevice, &tPreprocessBG0Desc);

    plBindGroupDesc tMipmapBG0Desc = {
        .tLayout = gptTerrainCtx->tMipmapBGLayout0,
        .ptPool = gptTerrainCtx->ptBindGroupPool,
        .pcDebugName = "compute bind group 0"
    };
    gptTerrainCtx->tMipmapBG0 = gptGfx->create_bind_group(ptDevice, &tMipmapBG0Desc);

    plBindGroupUpdateTextureData atComputeTextureData0[2] = {
        {.uSlot = 0, .tTexture = gptTerrainCtx->tRawTexture, .tType = PL_TEXTURE_BINDING_TYPE_STORAGE, .tCurrentUsage = PL_TEXTURE_USAGE_STORAGE},
        {.uSlot = 1, .tTexture = gptTerrainCtx->tProcessedTexture, .tType = PL_TEXTURE_BINDING_TYPE_STORAGE, .tCurrentUsage = PL_TEXTURE_USAGE_STORAGE},
    };

    plBindGroupUpdateData tPreprocessBG0Data = {
        .uTextureCount = 2,
        .atTextureBindings = atComputeTextureData0
    };
    gptGfx->update_bind_group(ptDevice, gptTerrainCtx->tPreprocessBG0, &tPreprocessBG0Data);
    
    plBindGroupUpdateSamplerData tSamplerData = {
        .tSampler = gptTerrainCtx->tMipSampler,
        .uSlot    = 0
    };

    plBindGroupUpdateTextureData atComputeTextureData1[2] = {
        {.uSlot = 1, .tTexture = gptTerrainCtx->tProcessedTexture, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tCurrentUsage = PL_TEXTURE_USAGE_SAMPLED},
        {.uSlot = 2, .tTexture = gptTerrainCtx->tDummyTexture, .tType = PL_TEXTURE_BINDING_TYPE_STORAGE, .tCurrentUsage = PL_TEXTURE_USAGE_STORAGE},
    };

    plBindGroupUpdateData tMipmapBG0Data = {
        .uSamplerCount     = 1,
        .atSamplerBindings = &tSamplerData,
        .uTextureCount     = 2,
        .atTextureBindings = atComputeTextureData1
    };
    gptGfx->update_bind_group(ptDevice, gptTerrainCtx->tMipmapBG0, &tMipmapBG0Data);

    // calculate tile field dimensions
    gptTerrainCtx->uHorizontalTiles = (uint32_t)ceilf(((gptTerrainCtx->tMaxWorldPosition.x - gptTerrainCtx->tMinWorldPosition.x) / ((float)gptTerrainCtx->uTileSize * gptTerrainCtx->fMetersPerTexel)));
    gptTerrainCtx->uVerticalTiles   = (uint32_t)ceilf(((gptTerrainCtx->tMaxWorldPosition.y - gptTerrainCtx->tMinWorldPosition.y) / ((float)gptTerrainCtx->uTileSize * gptTerrainCtx->fMetersPerTexel)));

    // handle case where there isn't enough tiles to fill terrain texture
    if(gptTerrainCtx->uHeightMapResolution / gptTerrainCtx->uTileSize > gptTerrainCtx->uHorizontalTiles)
    {
        gptTerrainCtx->uHorizontalTiles = gptTerrainCtx->uHeightMapResolution / gptTerrainCtx->uTileSize;
        gptTerrainCtx->tMaxWorldPosition.x = gptTerrainCtx->uHorizontalTiles * (float)gptTerrainCtx->uTileSize * gptTerrainCtx->fMetersPerTexel + gptTerrainCtx->tMinWorldPosition.x;
    }

    // handle case where there isn't enough tiles to fill terrain texture
    if(gptTerrainCtx->uHeightMapResolution / gptTerrainCtx->uTileSize > gptTerrainCtx->uVerticalTiles)
    {
        gptTerrainCtx->uVerticalTiles = gptTerrainCtx->uHeightMapResolution / gptTerrainCtx->uTileSize;
        gptTerrainCtx->tMaxWorldPosition.y = gptTerrainCtx->uVerticalTiles * (float)gptTerrainCtx->uTileSize * gptTerrainCtx->fMetersPerTexel + gptTerrainCtx->tMinWorldPosition.y;
    }

    // allocate tiles
    gptTerrainCtx->uTileCount = gptTerrainCtx->uHorizontalTiles * gptTerrainCtx->uVerticalTiles;
    gptTerrainCtx->atTiles = PL_ALLOC(gptTerrainCtx->uTileCount * sizeof(plHeightMapTile));
    memset(gptTerrainCtx->atTiles, 0, gptTerrainCtx->uTileCount * sizeof(plHeightMapTile));

    // load empty tiles
    uint32_t uYCoord = UINT32_MAX;
    for(uint32_t i = 0; i < gptTerrainCtx->uTileCount; i++)
    {
        plHeightMapTile* ptTile = &gptTerrainCtx->atTiles[i];

        uint32_t uXCoord = (int)(i % gptTerrainCtx->uHorizontalTiles);
        if(uXCoord == 0)
            uYCoord++;

        ptTile->_uChunkIndex = UINT32_MAX;
        ptTile->fMaxHeight   = 0.0f;
        ptTile->fMinHeight   = 0.0f;
        ptTile->uXOffset     = uXCoord * gptTerrainCtx->uTileSize;
        ptTile->uYOffset     = uYCoord * gptTerrainCtx->uTileSize;
        ptTile->_iXCoord     = (int)uXCoord * (int)gptTerrainCtx->uTileSize;
        ptTile->_iYCoord     = (int)uYCoord * (int)gptTerrainCtx->uTileSize;
        ptTile->tWorldPos.x  = (float)(ptTile->_iXCoord) * gptTerrainCtx->fMetersPerTexel + gptTerrainCtx->tMinWorldPosition.x;
        ptTile->tWorldPos.y  = (float)(ptTile->_iYCoord) * gptTerrainCtx->fMetersPerTexel + gptTerrainCtx->tMinWorldPosition.y;
        ptTile->_iXCoord     = (int32_t)((ptTile->tWorldPos.x - gptTerrainCtx->tMinWorldPosition.x) / ((float)gptTerrainCtx->uTileSize * gptTerrainCtx->fMetersPerTexel));
        ptTile->_iYCoord     = (int32_t)((ptTile->tWorldPos.y - gptTerrainCtx->tMinWorldPosition.y) / ((float)gptTerrainCtx->uTileSize * gptTerrainCtx->fMetersPerTexel));
    }

    gptTerrainCtx->bPrefetchThreadRunning = true;
    gptAtomics->create_atomic_counter(0, &gptTerrainCtx->ptPrefetchCounter);
    gptAtomics->create_atomic_counter(0, &gptTerrainCtx->ptPrefetchDirty);
    gptThreads->create_condition_variable(&gptTerrainCtx->ptConditionVariable);
    gptThreads->create_critical_section(&gptTerrainCtx->ptCriticalSection);
    gptThreads->create_mutex(&gptTerrainCtx->ptPrefetchMutex);
    

    float* sbtVertexBuffer = NULL;
    uint32_t* sbuIndices = NULL;


    // TODO: ellipsoid

    uint32_t uResolution = 64;
    plVec2 tLowResMinPosition = gptTerrainCtx->tMinWorldPosition;
    plVec2 tLowResMaxPosition = gptTerrainCtx->tMaxWorldPosition;
    plVec2 tLowResExtent = pl_sub_vec2(tLowResMaxPosition, tLowResMinPosition);
    float fXRange = tLowResExtent.x;
    float fYRange = tLowResExtent.y;
    float fXIncrement = fXRange / (float)uResolution;
    float fYIncrement = fYRange / (float)uResolution;
    for(uint32_t i = 0; i < uResolution; i++)
    {
        float fCurrentX = (float)i * fXIncrement + tLowResMinPosition.x;
        for(uint32_t j = 0; j < uResolution; j++)
        {
            float fCurrentY = (float)j * fYIncrement + tLowResMinPosition.y;

            uint32_t uStartVtxBuffer = pl_sb_size(sbtVertexBuffer) / 5;

            pl_sb_push(sbtVertexBuffer, fCurrentX);
            pl_sb_push(sbtVertexBuffer, 0.0f);
            pl_sb_push(sbtVertexBuffer, fCurrentY);
            pl_sb_push(sbtVertexBuffer, (fCurrentX - tLowResMinPosition.x) / fXRange);
            pl_sb_push(sbtVertexBuffer, (fCurrentY - tLowResMinPosition.y) / fYRange);

            pl_sb_push(sbtVertexBuffer, fCurrentX);
            pl_sb_push(sbtVertexBuffer, 0.0f);
            pl_sb_push(sbtVertexBuffer, fCurrentY + fYIncrement);
            pl_sb_push(sbtVertexBuffer, (fCurrentX - tLowResMinPosition.x) / fXRange);
            pl_sb_push(sbtVertexBuffer, (fCurrentY + fYIncrement - tLowResMinPosition.y) / fYRange);

            pl_sb_push(sbtVertexBuffer, fCurrentX + fXIncrement);
            pl_sb_push(sbtVertexBuffer, 0.0f);
            pl_sb_push(sbtVertexBuffer, fCurrentY + fYIncrement);
            pl_sb_push(sbtVertexBuffer, (fCurrentX + fXIncrement - tLowResMinPosition.x) / fXRange);
            pl_sb_push(sbtVertexBuffer, (fCurrentY + fYIncrement - tLowResMinPosition.y) / fYRange);

            pl_sb_push(sbtVertexBuffer, fCurrentX + fXIncrement);
            pl_sb_push(sbtVertexBuffer, 0.0f);
            pl_sb_push(sbtVertexBuffer, fCurrentY);
            pl_sb_push(sbtVertexBuffer, (fCurrentX + fXIncrement - tLowResMinPosition.x) / fXRange);
            pl_sb_push(sbtVertexBuffer, (fCurrentY - tLowResMinPosition.y) / fYRange);

            pl_sb_push(sbuIndices, uStartVtxBuffer + 0);
            pl_sb_push(sbuIndices, uStartVtxBuffer + 1);
            pl_sb_push(sbuIndices, uStartVtxBuffer + 2);
            pl_sb_push(sbuIndices, uStartVtxBuffer + 0);
            pl_sb_push(sbuIndices, uStartVtxBuffer + 2);
            pl_sb_push(sbuIndices, uStartVtxBuffer + 3);
        }

    }
    {
        plBufferDesc tVertexBufferDesc = {
            .tUsage      = PL_BUFFER_USAGE_VERTEX | PL_BUFFER_USAGE_TRANSFER_DESTINATION,
            .szByteSize  = pl_sb_size(sbtVertexBuffer) * sizeof(float),
            .pcDebugName = "full vertex buffer"
        };

        plBuffer* ptVertexBuffer = NULL;
        gptTerrainCtx->tFullVertexBuffer = gptGfx->create_buffer(ptDevice, &tVertexBufferDesc, &ptVertexBuffer);

        plDeviceMemoryAllocatorI* ptAllocator3 = gptTerrainCtx->tStagingBuddyAllocator;

        const plDeviceMemoryAllocation tFullVertexBufferMemory = ptAllocator3->allocate(ptAllocator3->ptInst,
            ptVertexBuffer->tMemoryRequirements.uMemoryTypeBits,
            ptVertexBuffer->tMemoryRequirements.ulSize,
            ptVertexBuffer->tMemoryRequirements.ulAlignment,
            "full vertex memory");

        gptGfx->bind_buffer_to_memory(ptDevice, gptTerrainCtx->tFullVertexBuffer, &tFullVertexBufferMemory);

        memcpy(ptVertexBuffer->tMemoryAllocation.pHostMapped, sbtVertexBuffer, tVertexBufferDesc.szByteSize);
    }

    {
        plBufferDesc tVertexBufferDesc = {
            .tUsage      = PL_BUFFER_USAGE_INDEX | PL_BUFFER_USAGE_TRANSFER_DESTINATION,
            .szByteSize  = pl_sb_size(sbuIndices) * sizeof(uint32_t),
            .pcDebugName = "full index buffer"
        };

        plBuffer* ptVertexBuffer = NULL;
        gptTerrainCtx->tFullIndexBuffer = gptGfx->create_buffer(ptDevice, &tVertexBufferDesc, &ptVertexBuffer);

        plDeviceMemoryAllocatorI* ptAllocator3 = gptTerrainCtx->tStagingBuddyAllocator;

        const plDeviceMemoryAllocation tFullVertexBufferMemory = ptAllocator3->allocate(ptAllocator3->ptInst,
            ptVertexBuffer->tMemoryRequirements.uMemoryTypeBits,
            ptVertexBuffer->tMemoryRequirements.ulSize,
            ptVertexBuffer->tMemoryRequirements.ulAlignment,
            "full vertex memory");

        gptGfx->bind_buffer_to_memory(ptDevice, gptTerrainCtx->tFullIndexBuffer, &tFullVertexBufferMemory);

        memcpy(ptVertexBuffer->tMemoryAllocation.pHostMapped, sbuIndices, tVertexBufferDesc.szByteSize);
        gptTerrainCtx->uFullIndexCount = pl_sb_size(sbuIndices);
    }

    pl_sb_free(sbtVertexBuffer);
    pl_sb_free(sbuIndices);

    gptTerrainCtx->fLowResMetersPerTexel = (tInit.tMaxPosition.x - tInit.tMinPosition.x) / (float)gptTerrainCtx->uHeightMapResolution;
}

void
pl_terrain_cleanup(void)
{
    plDevice* ptDevice = gptTerrainCtx->ptDevice;
    gptGfx->flush_device(ptDevice);

    gptTerrainCtx->bPrefetchThreadRunning = false;
    gptAtomics->atomic_store(gptTerrainCtx->ptPrefetchCounter, 0);
    gptThreads->wake_condition_variable(gptTerrainCtx->ptConditionVariable);
    if(!(gptTerrainCtx->tFlags & PL_TERRAIN_FLAGS_CACHE_TILES))
    {
        for(uint32_t j = 0; j < gptTerrainCtx->uTileCount; j++)
        {
            plHeightMapTile* ptTile = &gptTerrainCtx->atTiles[j];
            if(!ptTile->_bEmpty)
            {
                gptFile->remove(ptTile->acFile);
            }
        }
    }

    gptThreads->destroy_thread(&gptTerrainCtx->ptPrefetchThread);
    gptThreads->destroy_mutex(&gptTerrainCtx->ptPrefetchMutex);
    gptAtomics->destroy_atomic_counter(&gptTerrainCtx->ptPrefetchCounter);
    gptAtomics->destroy_atomic_counter(&gptTerrainCtx->ptPrefetchDirty);
    gptThreads->destroy_condition_variable(&gptTerrainCtx->ptConditionVariable);
    gptThreads->destroy_critical_section(&gptTerrainCtx->ptCriticalSection);
    
    gptGfx->destroy_shader(ptDevice, gptTerrainCtx->tRegularShader);
    gptGfx->destroy_shader(ptDevice, gptTerrainCtx->tWireframeShader);
    gptGfx->destroy_compute_shader(ptDevice, gptTerrainCtx->tPreProcessHeightShader);
    gptGfx->destroy_compute_shader(ptDevice, gptTerrainCtx->tMipMapShader);
    gptGfx->destroy_texture(ptDevice, gptTerrainCtx->tProcessedTexture);
    for(uint32_t j = 0; j < gptGfx->get_frames_in_flight(); j++)
    {
        gptGfx->destroy_bind_group(ptDevice, gptTerrainCtx->atBindGroup0[j]);
        gptGfx->destroy_texture(ptDevice, gptTerrainCtx->atActiveTexture[j]);
    }
    gptDraw->return_3d_drawlist(gptTerrainCtx->pt3dDrawlist);
    gptGfx->destroy_texture(ptDevice, gptTerrainCtx->tDummyTexture);
    gptGfx->destroy_texture(ptDevice, gptTerrainCtx->tRawTexture);
    gptGfx->destroy_bind_group(ptDevice, gptTerrainCtx->tMipmapBG0);
    pl_sb_free(gptTerrainCtx->sbuActiveTileIndices);
    pl_sb_free(gptTerrainCtx->sbuFreeChunks);
    pl_sb_free(gptTerrainCtx->sbuPrefetchOverflow);
    PL_FREE(gptTerrainCtx->auPrefetchTileIndices);
    PL_FREE(gptTerrainCtx->auFetchTileIndices);
    PL_FREE(gptTerrainCtx->atChunks);
    PL_FREE(gptTerrainCtx->atTiles);

    gptGfx->destroy_buffer(ptDevice, gptTerrainCtx->tIndexBuffer);
    gptGfx->destroy_buffer(ptDevice, gptTerrainCtx->tVertexBuffer);

    gptGfx->destroy_texture(ptDevice, gptTerrainCtx->tNoiseTexture);
    gptGfx->destroy_texture(ptDevice, gptTerrainCtx->tDiffuseTexture);
    gptGfx->destroy_sampler(ptDevice, gptTerrainCtx->tSampler);
    gptGfx->destroy_sampler(ptDevice, gptTerrainCtx->tMirrorSampler);
    
    gptGfx->destroy_bind_group_layout(ptDevice, gptTerrainCtx->tBindGroupLayout0);
    gptGfx->destroy_bind_group_layout(ptDevice, gptTerrainCtx->tPreprocessBGLayout0);
    gptGfx->destroy_bind_group_layout(ptDevice, gptTerrainCtx->tMipmapBGLayout0);
    gptGfx->cleanup_bind_group_pool(gptTerrainCtx->ptBindGroupPool);

    gptGpuAllocators->cleanup(ptDevice);
}

static void
pl__tile_job(plInvocationData tInvoData, void* pData, void* pGroupSharedMemory)
{
    plTerrainTilingInfo* atInfos = (plTerrainTilingInfo*)pData;
    pl__terrain_tile_height_map(atInfos[tInvoData.uGlobalIndex]);
}

void
pl_terrain_tile_height_map(uint32_t uCount, plTerrainTilingInfo* atInfos)
{
    for(uint32_t i = 0; i < uCount; i++)
    {
        // preregister with vfs since vfs isn't thread safe
        plVfsFileHandle tFileHandle = gptVfs->open_file(atInfos[i].pcFile, PL_VFS_FILE_MODE_READ);
        gptVfs->close_file(tFileHandle);
    }

    #if 0 // single threaded
        for(uint32_t i = 0; i < uCount; i++)
        {
            pl__terrain_tile_height_map(atInfos[i]);
        }
    #else
        plJobDesc tJobDesc = {
            .task  = pl__tile_job,
            .pData = atInfos
        };
        plAtomicCounter* ptCounter = NULL;
        gptJob->dispatch_batch(uCount, 0, tJobDesc, &ptCounter);
        gptJob->wait_for_counter(ptCounter);
    #endif
}

void
pl__terrain_tile_height_map(plTerrainTilingInfo tInfo)
{

    // load raw heightmap image
    size_t szImageFileSize = gptVfs->get_file_size_str(tInfo.pcFile);
    plVfsFileHandle tRawHeightMapFile = gptVfs->open_file(tInfo.pcFile, PL_VFS_FILE_MODE_READ);
    gptVfs->read_file(tRawHeightMapFile, NULL, &szImageFileSize);
    unsigned char* pucBuffer = (unsigned char*)PL_ALLOC(szImageFileSize);
    gptVfs->read_file(tRawHeightMapFile, pucBuffer, &szImageFileSize);
    gptVfs->close_file(tRawHeightMapFile);

    // load image info
    plImageInfo tImageInfo = {0};
    gptImage->get_info(pucBuffer, (int)szImageFileSize, &tImageInfo);
    int iImageWidth = tImageInfo.iWidth;
    int iImageHeight = tImageInfo.iHeight;

    const uint32_t uXAlignmentOffset = (uint32_t)(tInfo.tOrigin.x - gptTerrainCtx->tMinWorldPosition.x) % gptTerrainCtx->uTileSize;
    const uint32_t uYAlignmentOffset = (uint32_t)(tInfo.tOrigin.y - gptTerrainCtx->tMinWorldPosition.y) % gptTerrainCtx->uTileSize;

    uint32_t uHorizontalTileCount = (int)ceilf((float)((uint32_t)tImageInfo.iWidth + uXAlignmentOffset) / (float)gptTerrainCtx->uTileSize);
    uint32_t uVerticalTileCount   = (int)ceilf((float)((uint32_t)tImageInfo.iHeight + uYAlignmentOffset) / (float)gptTerrainCtx->uTileSize);

    // check for cache file
    plTileCacheFileInfo tCacheFileInfo = {0};
    char pcFileNameOnlyBuffer[256] = {0};
    pl_str_get_file_name_only(tInfo.pcFile, pcFileNameOnlyBuffer, 256);

    plTempAllocator tAllocator = {0};
    char* pcFileName = pl_temp_allocator_sprintf(&tAllocator, "../cache/cache_%s_%u.txt", pcFileNameOnlyBuffer, gptTerrainCtx->uTileSize);

    bool bValidCache = true;

    uint8_t*       puTileBuffer   = NULL;
    void*          pImageData     = NULL;
    void*          pConvertedData = NULL; // if not loaded as 16 bit
    unsigned char* pucImageData   = NULL; // could be converted or not (aliased)

    // if cache exists, ensure cache is valid
    if(gptFile->exists(pcFileName))
    {
        size_t szCacheFileSize = 0;
        gptFile->binary_read(pcFileName, &szCacheFileSize, (uint8_t*)&tCacheFileInfo);

        if(tCacheFileInfo.uVersion != 1)
            bValidCache = false;
        else if(tCacheFileInfo.uXAlignmentOffset != uXAlignmentOffset)
            bValidCache = false;
        else if(tCacheFileInfo.uYAlignmentOffset != uYAlignmentOffset)
            bValidCache = false;
    }
    else
        bValidCache = false;

    if(!(gptTerrainCtx->tFlags & PL_TERRAIN_FLAGS_CACHE_TILES))
        bValidCache = false;
    
    // update cache file
    if(!bValidCache)
    {

        tCacheFileInfo.uVersion = 1;
        tCacheFileInfo.uXAlignmentOffset = uXAlignmentOffset;
        tCacheFileInfo.uYAlignmentOffset = uYAlignmentOffset;

        int _unused = 0;
        uint32_t uPixelCount = (uint32_t)(iImageWidth * iImageHeight);
        if(tImageInfo.b16Bit)
        {
            uint16_t* puImageData = gptImage->load_16bit(pucBuffer, (int)szImageFileSize, &iImageWidth, &iImageHeight, &_unused, 1);
            pucImageData = (unsigned char*)puImageData;
            pImageData = puImageData;
        }
        else if(tImageInfo.bHDR)
        {
            float* pufImageData = gptImage->load_hdr(pucBuffer, (int)szImageFileSize, &iImageWidth, &iImageHeight, &_unused, 1);
            uint16_t* puConvertedData = PL_ALLOC(sizeof(uint32_t) * uPixelCount);

            // scale for 16 bit
            for(uint32_t i = 0; i < uPixelCount; i++)
                puConvertedData[i] = (uint16_t)(pufImageData[i] * 65535.0f);

            gptImage->free(pufImageData);
            pucImageData = (unsigned char*)puConvertedData;
            pConvertedData = puConvertedData;

        }
        else
        {
            uint8_t* puImageData = gptImage->load(pucBuffer, (int)szImageFileSize, &iImageWidth, &iImageHeight, &_unused, 1);
            uint16_t* puConvertedData = PL_ALLOC(sizeof(uint32_t) * uPixelCount);

            // scale for 16 bit
            for(uint32_t i = 0; i < uPixelCount; i++)
                puConvertedData[i] = (uint16_t)(((float)puImageData[i] / 255.0f) * 65535.0f);

            gptImage->free(puImageData);
            pucImageData = (unsigned char*)puConvertedData;
            pConvertedData = puConvertedData;
        }


        // write out cache file
        if(gptTerrainCtx->tFlags & PL_TERRAIN_FLAGS_CACHE_TILES)
            gptFile->binary_write(pcFileName, sizeof(plTileCacheFileInfo), (uint8_t*)&tCacheFileInfo);

        puTileBuffer = PL_ALLOC(gptTerrainCtx->uTileSize * gptTerrainCtx->uTileSize * gptTerrainCtx->szHeightTexelStride);
        memset(puTileBuffer, 0, gptTerrainCtx->uTileSize * gptTerrainCtx->uTileSize * gptTerrainCtx->szHeightTexelStride);
    }
    pl_temp_allocator_reset(&tAllocator);
    PL_FREE(pucBuffer);

    const uint32_t uFileSize = gptTerrainCtx->uTileSize * gptTerrainCtx->uTileSize * (uint32_t)gptTerrainCtx->szHeightTexelStride;

    for(uint32_t j = 0; j < uVerticalTileCount; j++)
    {
        uint32_t uActiveTileHeight = gptTerrainCtx->uTileSize;
        uint32_t uYStart = 0;
        uint32_t uYSrcStart = 0;

        // handling all the odd vertical alignment cases
        if(j == 0)
            uYStart = uYAlignmentOffset;
        else if(j == uVerticalTileCount - 1)
        {
            uActiveTileHeight = (uint32_t)iImageHeight - gptTerrainCtx->uTileSize * j + uYAlignmentOffset;
            uYSrcStart = uYAlignmentOffset;
        }
        else
            uYSrcStart = uYAlignmentOffset;

        for(uint32_t i = 0; i < uHorizontalTileCount; i++)
        {
            uint32_t uTileIndex = i + j * uHorizontalTileCount;
            uint32_t uStartX = 0;
            char* pcCacheFileName = pl_temp_allocator_sprintf(&tAllocator, "../cache/tile_%s_%u_%u_%u.tile", pcFileNameOnlyBuffer, gptTerrainCtx->uTileSize, i, j);

            if(!bValidCache)
            {

                memset(puTileBuffer, 0, uFileSize);

                uint32_t uActiveTileWidth = gptTerrainCtx->uTileSize;
                uint32_t uXSrcStart = 0;

                // handling all the odd horizontal alignment cases
                if(i == 0)
                {
                    uActiveTileWidth = gptTerrainCtx->uTileSize - uXAlignmentOffset;
                    uStartX = uXAlignmentOffset;
                }
                else if(i == uHorizontalTileCount - 1)
                {
                    uActiveTileWidth = (uint32_t)iImageWidth - gptTerrainCtx->uTileSize * i + uXAlignmentOffset;
                    uXSrcStart = uXAlignmentOffset;
                }
                else
                    uXSrcStart = uXAlignmentOffset;

                uint32_t uFileOffset = (gptTerrainCtx->uTileSize * j - uYSrcStart) * iImageWidth * (uint32_t)gptTerrainCtx->szHeightTexelStride;
                uFileOffset += (gptTerrainCtx->uTileSize * i - uXSrcStart) * (uint32_t)gptTerrainCtx->szHeightTexelStride;

                unsigned char* pcStart = &pucImageData[uFileOffset];

                for(uint32_t k = uYStart; k < uActiveTileHeight; k++)
                {
                    uint32_t uDest = k * gptTerrainCtx->uTileSize * (uint32_t)gptTerrainCtx->szHeightTexelStride + uStartX * (uint32_t)gptTerrainCtx->szHeightTexelStride;
                    memcpy(&puTileBuffer[uDest], &pcStart[(k - uYStart) * iImageWidth * gptTerrainCtx->szHeightTexelStride], uActiveTileWidth * gptTerrainCtx->szHeightTexelStride);
                }

                // write out cached tile
                gptFile->binary_write(pcCacheFileName, gptTerrainCtx->uTileSize * gptTerrainCtx->uTileSize * gptTerrainCtx->szHeightTexelStride, puTileBuffer);
            }

            plHeightMapTile tTile = {
                ._uChunkIndex = UINT32_MAX,
                .fMinHeight   = tInfo.fMinHeight,
                .fMaxHeight   = tInfo.fMaxHeight,
                ._bEmpty      = false,
                .uXOffset     = i * gptTerrainCtx->uTileSize,
                .uYOffset     = j * gptTerrainCtx->uTileSize,
                .tWorldPos    = tInfo.tOrigin
            };
            tTile.tWorldPos.x -= uXAlignmentOffset;
            tTile.tWorldPos.y -= uYAlignmentOffset;
            tTile.tWorldPos.x += tTile.uXOffset * gptTerrainCtx->fMetersPerTexel;
            tTile.tWorldPos.y += tTile.uYOffset * gptTerrainCtx->fMetersPerTexel;
            strncpy(tTile.acFile, pcFileName, 256);

            tTile._iXCoord = (int32_t)floorf(((tTile.tWorldPos.x - gptTerrainCtx->tMinWorldPosition.x) / ((float)gptTerrainCtx->uTileSize * gptTerrainCtx->fMetersPerTexel)));
            tTile._iYCoord = (int32_t)floorf(((tTile.tWorldPos.y - gptTerrainCtx->tMinWorldPosition.y) / ((float)gptTerrainCtx->uTileSize * gptTerrainCtx->fMetersPerTexel)));

            int iIndex = pl__terrain_get_tile_index_by_pos(tTile._iXCoord, tTile._iYCoord);
            PL_ASSERT(iIndex > -1);
            gptTerrainCtx->atTiles[iIndex] = tTile;
            pl_temp_allocator_reset(&tAllocator);
        }
    }
    pl_temp_allocator_free(&tAllocator);
    
    if(pImageData)
        gptImage->free(pImageData);
    if(puTileBuffer)
    {
        PL_FREE(puTileBuffer);
    }

    if(pConvertedData)
    {
        PL_FREE(pConvertedData);
    }
}

void
pl_finalize_terrain(plCommandBuffer* ptCmdBuffer)
{
    plDevice* ptDevice = gptTerrainCtx->ptDevice;
    gptTerrainCtx->tCurrentDynamicDataBlock = gptGfx->allocate_dynamic_data_block(ptDevice);

    plBuffer* ptStagingBuffer = gptGfx->get_buffer(gptTerrainCtx->ptDevice, gptTerrainCtx->tStagingBuffer2);
    size_t szTileFileSize = gptTerrainCtx->uTileSize * gptTerrainCtx->uTileSize * gptTerrainCtx->szHeightTexelStride;
    

    uint16_t* puOriginalData = PL_ALLOC(szTileFileSize);
    uint16_t* puDestData = (uint16_t*)ptStagingBuffer->tMemoryAllocation.pHostMapped;

    for(uint32_t i = 0; i < gptTerrainCtx->uTileCount; i++)
    {
        const plHeightMapTile* ptTile = &gptTerrainCtx->atTiles[i];
        if(!ptTile->_bEmpty)
        {
            memset(puOriginalData, 0, szTileFileSize);
            gptFile->binary_read(ptTile->acFile, &szTileFileSize, (uint8_t*)puOriginalData);

            uint32_t uDstRowStart = (uint32_t)((ptTile->tWorldPos.y - gptTerrainCtx->tMinWorldPosition.y) / gptTerrainCtx->fLowResMetersPerTexel);
            uint32_t uDstColStart = (uint32_t)((ptTile->tWorldPos.x - gptTerrainCtx->tMinWorldPosition.x) / gptTerrainCtx->fLowResMetersPerTexel);

            uint32_t uDstRowCount = (uint32_t)gptTerrainCtx->uTileSize + uDstRowStart;
            uint32_t uDstColCount = (uint32_t)gptTerrainCtx->uTileSize + uDstColStart;

            int iHorizontalPixels = (int)(gptTerrainCtx->fLowResMetersPerTexel / gptTerrainCtx->fMetersPerTexel);
            int iVerticalPixels = (int)(gptTerrainCtx->fLowResMetersPerTexel / gptTerrainCtx->fMetersPerTexel);
            float fUvxInc = (float)iHorizontalPixels / (float)gptTerrainCtx->uTileSize;
            float fUvyInc = (float)iVerticalPixels / (float)gptTerrainCtx->uTileSize;
            for(uint32_t uDstRow = uDstRowStart; uDstRow < uDstRowCount; uDstRow++)
            {
                float fUvy = (float)(uDstRow - uDstRowStart) / (float)gptTerrainCtx->uTileSize;
                for(uint32_t uDstCol = uDstColStart; uDstCol < uDstColCount; uDstCol++)
                {

                    float fUvx = (float)(uDstCol - uDstColStart) / (float)gptTerrainCtx->uTileSize;

                    uint32_t uResult = 0;
                    uint32_t uSumCount = 0;
                    for(int x = 0; x < iHorizontalPixels; x++)
                    {
                        for(int y = 0; y < iVerticalPixels; y++)
                        {
                            float fUvxTap = fUvx + x * fUvxInc - fUvxInc * (iHorizontalPixels - 1);
                            float fUvyTap = fUvy + y * fUvyInc - fUvyInc * (iVerticalPixels - 1);


                            if(fUvxTap < 0.0f || fUvxTap > 1.0f || fUvyTap < 0.0f || fUvyTap > 1.0f)
                            {
                            }
                            else
                            {
                                uint32_t uSrcRow = (uint32_t)(fUvyTap * (float)gptTerrainCtx->uTileSize);
                                uint32_t uSrcCol = (uint32_t)(fUvxTap * (float)gptTerrainCtx->uTileSize);
                                // uResult += (uint32_t)puOriginalData[uSrcRow * gptTerrainCtx->uTileSize + uSrcCol];
                                uResult = pl_max((uint32_t)puOriginalData[uSrcRow * gptTerrainCtx->uTileSize + uSrcCol], uResult);
                                uSumCount++;
                            }
                        }
                    }
                    // TODO: make sample technique option
                    // if(uSumCount > 0)
                    //     uResult /= uSumCount;

                    puDestData[uDstRow * gptTerrainCtx->uHeightMapResolution + uDstCol] = (uint16_t)uResult;
                }
            }
        }
    }


    plBufferImageCopy tBufferImageCopy = {
        .uImageWidth        = gptTerrainCtx->uHeightMapResolution,
        .uImageHeight       = gptTerrainCtx->uHeightMapResolution,
        .uImageDepth        = 1,
        .uLayerCount        = 1,
        .tCurrentImageUsage = PL_TEXTURE_USAGE_STORAGE,
    };
    
    plBlitEncoder* ptBlit = gptGfx->begin_blit_pass(ptCmdBuffer);
    gptGfx->pipeline_barrier_blit(ptBlit,
        PL_PIPELINE_STAGE_VERTEX_SHADER,
        PL_ACCESS_SHADER_READ,
        PL_PIPELINE_STAGE_TRANSFER,
        PL_ACCESS_TRANSFER_WRITE);

    gptGfx->copy_buffer_to_texture(ptBlit, gptTerrainCtx->tStagingBuffer2, gptTerrainCtx->tRawTexture, 1, &tBufferImageCopy);
    gptGfx->set_texture_usage(ptBlit, gptTerrainCtx->tProcessedTexture, PL_TEXTURE_USAGE_STORAGE, PL_TEXTURE_USAGE_SAMPLED);

    gptGfx->pipeline_barrier_blit(ptBlit,
        PL_PIPELINE_STAGE_TRANSFER,
        PL_ACCESS_TRANSFER_WRITE | PL_ACCESS_TRANSFER_READ,
        PL_PIPELINE_STAGE_COMPUTE_SHADER,
        PL_ACCESS_SHADER_WRITE | PL_ACCESS_SHADER_READ);
    gptGfx->end_blit_pass(ptBlit);

    plComputeEncoder* ptComputeEncoder = gptGfx->begin_compute_pass(ptCmdBuffer, NULL);
    gptGfx->bind_compute_shader(ptComputeEncoder, gptTerrainCtx->tPreProcessHeightShader);

    plDynamicBinding tDynamicBinding = pl_allocate_dynamic_data(gptGfx, gptTerrainCtx->ptDevice, &gptTerrainCtx->tCurrentDynamicDataBlock);
    plTerrainPrepDynamicData* ptDynamicData = (plTerrainPrepDynamicData*)tDynamicBinding.pcData;
    memset(ptDynamicData, 0, sizeof(plTerrainPrepDynamicData));
    ptDynamicData->fMetersPerHeightFieldTexel = gptTerrainCtx->fLowResMetersPerTexel;
    ptDynamicData->fMaxHeight = gptTerrainCtx->fMaxElevation;
    ptDynamicData->fMinHeight = gptTerrainCtx->fMinElevation;
    ptDynamicData->fGlobalMaxHeight = gptTerrainCtx->fMaxElevation;
    ptDynamicData->fGlobalMinHeight = gptTerrainCtx->fMinElevation;
    ptDynamicData->iXOffset = 0;
    ptDynamicData->iYOffset = 0;
    ptDynamicData->iNormalCalcReach = 10;

    gptGfx->bind_compute_bind_groups(ptComputeEncoder, gptTerrainCtx->tPreProcessHeightShader, 0, 1, &gptTerrainCtx->tPreprocessBG0, 1, &tDynamicBinding);

    plDispatch tDispatch = {
        .uGroupCountX     = gptTerrainCtx->uHeightMapResolution / 8,
        .uGroupCountY     = gptTerrainCtx->uHeightMapResolution / 8,
        .uGroupCountZ     = 1,
        .uThreadPerGroupX = 8,
        .uThreadPerGroupY = 8,
        .uThreadPerGroupZ = 1,
    };

    gptGfx->dispatch(ptComputeEncoder, 1, &tDispatch);

    gptGfx->end_compute_pass(ptComputeEncoder);

    // set proper texture usages

    plImageCopy tImageCopy = {
        .uSourceExtentX             = gptTerrainCtx->uHeightMapResolution,
        .uSourceExtentY             = gptTerrainCtx->uHeightMapResolution,
        .uSourceExtentZ             = 1,
        .uSourceMipLevel            = 0,
        .uSourceBaseArrayLayer      = 0,
        .uSourceLayerCount          = 1,
        .tSourceImageUsage          = PL_TEXTURE_USAGE_SAMPLED,
        .uDestinationMipLevel       = 0,
        .uDestinationBaseArrayLayer = 0,
        .uDestinationLayerCount     = 1,
        .tDestinationImageUsage     = PL_TEXTURE_USAGE_SAMPLED,
    };

    ptBlit = gptGfx->begin_blit_pass(ptCmdBuffer);
    gptGfx->pipeline_barrier_blit(ptBlit,
        PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER,
        PL_ACCESS_SHADER_WRITE | PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ,
        PL_PIPELINE_STAGE_TRANSFER,
        PL_ACCESS_TRANSFER_WRITE);

    
    gptGfx->set_texture_usage(ptBlit, gptTerrainCtx->tProcessedTexture, PL_TEXTURE_USAGE_SAMPLED, PL_TEXTURE_USAGE_STORAGE);
    gptGfx->copy_texture(ptBlit, gptTerrainCtx->tProcessedTexture, gptTerrainCtx->tFullTexture, 1, &tImageCopy);
    

    gptGfx->pipeline_barrier_blit(ptBlit,
        PL_PIPELINE_STAGE_TRANSFER,
        PL_ACCESS_TRANSFER_WRITE | PL_ACCESS_TRANSFER_READ,
        PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER,
        PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ | PL_ACCESS_SHADER_WRITE | PL_ACCESS_TRANSFER_WRITE);

    gptGfx->end_blit_pass(ptBlit);

    gptThreads->create_thread(pl__prefetch_thread, NULL, &gptTerrainCtx->ptPrefetchThread);
}

void
pl_terrain_prepare(plCamera* ptCamera, plCommandBuffer* ptCmdBuffer)
{
    plDevice* ptDevice = gptTerrainCtx->ptDevice;
    gptTerrainCtx->tCurrentDynamicDataBlock = gptGfx->allocate_dynamic_data_block(ptDevice);

    const uint32_t uOverflowCount = pl_sb_size(gptTerrainCtx->sbuPrefetchOverflow);
    
    if(uOverflowCount > 0)
    {
        for(uint32_t i = 0; i < uOverflowCount; i++)
        {
            bool bResult = pl__push_prefetch_tile(pl_sb_pop(gptTerrainCtx->sbuPrefetchOverflow));
            if(!bResult)
                break;
        }
        gptThreads->wake_condition_variable(gptTerrainCtx->ptConditionVariable);
    }

    // try to recycle 10 cached tiles per frame
    static uint32_t uChunkCheck = 0;
    for(uint32_t i = 0; i < 10; i++)
    {
        uint32_t uTileIndex = gptTerrainCtx->atChunks[uChunkCheck].uOwnerTileIndex;
        if(uTileIndex != UINT32_MAX)
        {
            if(!(gptTerrainCtx->atTiles[uTileIndex].tFlags & PL_TERRAIN_TILE_FLAGS_ACTIVE))
            {
                if(!(gptTerrainCtx->atTiles[uTileIndex].tFlags & PL_TERRAIN_TILE_FLAGS_QUEUED))
                {
                    uint32_t uActivationDistance = pl__terrain_tile_activation_distance(uTileIndex);
                    if(uActivationDistance > gptTerrainCtx->uPrefetchRadius)
                    {
                        pl__terrain_return_free_chunk(uTileIndex);
                    }
                }
            }
        }
        uChunkCheck = (uChunkCheck + 1) % gptTerrainCtx->uChunkCapacity;
    }
    
    
    plVec3 tPos = ptCamera->tPos;

    // check how many tile boundaries we've cross (needed for streaming new tiles)
    float fAddressOriginX = (float)gptTerrainCtx->uTileSize * floorf(tPos.x / ((float)gptTerrainCtx->uTileSize * gptTerrainCtx->fMetersPerTexel)) / (float)gptTerrainCtx->uHeightMapResolution;
    float fAddressOriginY = (float)gptTerrainCtx->uTileSize * floorf(tPos.z / ((float)gptTerrainCtx->uTileSize * gptTerrainCtx->fMetersPerTexel)) / (float)gptTerrainCtx->uHeightMapResolution;

    int iDeltaXDirectionUpdateTileCount = 0;
    int iDeltaYDirectionUpdateTileCount = 0;

    gptTerrainCtx->tCurrentDirectionUpdate = PL_TERRAIN_DIRECTION_NONE;
    if(gptTerrainCtx->tCurrentExtent.x > fAddressOriginX)
    {
        gptTerrainCtx->tCurrentDirectionUpdate |= PL_TERRAIN_DIRECTION_EAST;
        iDeltaXDirectionUpdateTileCount = (int32_t)((float)gptTerrainCtx->uHeightMapResolution * (fAddressOriginX - gptTerrainCtx->tCurrentExtent.x) / (float)gptTerrainCtx->uTileSize);  
    }
    else if(gptTerrainCtx->tCurrentExtent.x < fAddressOriginX)
    {
        gptTerrainCtx->tCurrentDirectionUpdate |= PL_TERRAIN_DIRECTION_WEST;
        iDeltaXDirectionUpdateTileCount = (int32_t)((float)gptTerrainCtx->uHeightMapResolution * (fAddressOriginX - gptTerrainCtx->tCurrentExtent.x) / (float)gptTerrainCtx->uTileSize);  
    }
    else
        iDeltaXDirectionUpdateTileCount = 0;

    if(gptTerrainCtx->tCurrentExtent.y > fAddressOriginY)
    {
        gptTerrainCtx->tCurrentDirectionUpdate |= PL_TERRAIN_DIRECTION_SOUTH;
        iDeltaYDirectionUpdateTileCount = (int32_t)((float)gptTerrainCtx->uHeightMapResolution * (fAddressOriginY - gptTerrainCtx->tCurrentExtent.y) / (float)gptTerrainCtx->uTileSize);   
    }
    else if(gptTerrainCtx->tCurrentExtent.y < fAddressOriginY)
    {
        gptTerrainCtx->tCurrentDirectionUpdate |= PL_TERRAIN_DIRECTION_NORTH;
        iDeltaYDirectionUpdateTileCount = (int32_t)((float)gptTerrainCtx->uHeightMapResolution * (fAddressOriginY - gptTerrainCtx->tCurrentExtent.y) / (float)gptTerrainCtx->uTileSize);   
    }
    else
        iDeltaYDirectionUpdateTileCount = 0;

    if(gptTerrainCtx->tCurrentDirectionUpdate != PL_TERRAIN_DIRECTION_NONE)
    {
        // gptTerrainCtx->bProcessAll = true;
        gptTerrainCtx->bNeedsUpdate = true;
        const uint32_t uTilesAcross = gptTerrainCtx->uHeightMapResolution / gptTerrainCtx->uTileSize;
        gptTerrainCtx->uCurrentXOffset = (gptTerrainCtx->uCurrentXOffset + iDeltaXDirectionUpdateTileCount) % uTilesAcross;
        gptTerrainCtx->uCurrentYOffset = (gptTerrainCtx->uCurrentYOffset + iDeltaYDirectionUpdateTileCount) % uTilesAcross;
    }
    
    gptTerrainCtx->tCurrentExtent.x = fAddressOriginX;
    gptTerrainCtx->tCurrentExtent.y = fAddressOriginY;

    pl__terrain_prepare(ptCmdBuffer, ptCamera);
}

void
pl_terrain_render(plCamera* ptCamera, plRenderEncoder* ptEncoder)
{
    plDevice* ptDevice = gptTerrainCtx->ptDevice;
    plVec3 tPos = ptCamera->tPos;

    const plMat4 tMVP = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);

    if(gptTerrainCtx->tFlags & PL_TERRAIN_FLAGS_LOW_RES)
    {
        // full but low resolution terrain draw call
        plDrawIndex tFullDraw = {
            .uInstanceCount = 1,
            .tIndexBuffer = gptTerrainCtx->tFullIndexBuffer,
            .uIndexCount = gptTerrainCtx->uFullIndexCount,
        };

        gptGfx->bind_shader(ptEncoder, gptTerrainCtx->tFlags & PL_TERRAIN_FLAGS_WIREFRAME ? gptTerrainCtx->tFullWireFrameShader : gptTerrainCtx->tFullShader);
        gptGfx->bind_vertex_buffer(ptEncoder, gptTerrainCtx->tFullVertexBuffer);
        {
            plDynamicBinding tDynamicBinding = pl_allocate_dynamic_data(gptGfx, ptDevice, &gptTerrainCtx->tCurrentDynamicDataBlock);
            plTerrainDynamicData* ptDynamicData = (plTerrainDynamicData*)tDynamicBinding.pcData;
            memset(ptDynamicData, 0, sizeof(plTerrainDynamicData));
            ptDynamicData->tPos.xyz                   = tPos;
            ptDynamicData->tCameraViewProjection      = tMVP;
            ptDynamicData->fMetersPerHeightFieldTexel = gptTerrainCtx->fLowResMetersPerTexel;
            ptDynamicData->fGlobalMaxHeight           = gptTerrainCtx->fMaxElevation;
            ptDynamicData->fGlobalMinHeight           = gptTerrainCtx->fMinElevation;
            ptDynamicData->tSunDirection.xyz          = gptTerrainCtx->tSunDirection;
            ptDynamicData->fXUVOffset                 = gptTerrainCtx->tCurrentExtent.x;
            ptDynamicData->fYUVOffset                 = gptTerrainCtx->tCurrentExtent.y;
            ptDynamicData->fStencilRadius             = gptTerrainCtx->fMetersPerTexel * 0.5f * (float)gptTerrainCtx->uHeightMapResolution;
            ptDynamicData->fBlurRadius                = gptTerrainCtx->fMetersPerTexel * (float)gptTerrainCtx->uHeightMapResolution;
            ptDynamicData->tMinMax.xy                 = gptTerrainCtx->tMinWorldPosition;
            ptDynamicData->tMinMax.zw                 = gptTerrainCtx->tMaxWorldPosition;

            gptGfx->bind_graphics_bind_groups(ptEncoder, gptTerrainCtx->tFullShader, 0, 1, &gptTerrainCtx->tFullBG0, 1, &tDynamicBinding);
        }
        gptGfx->draw_indexed(ptEncoder, 1, &tFullDraw);
    }

    if(gptTerrainCtx->tFlags & PL_TERRAIN_FLAGS_HIGH_RES)
    {

        // clip map drawing
        gptGfx->bind_shader(ptEncoder, gptTerrainCtx->tFlags & PL_TERRAIN_FLAGS_WIREFRAME ? gptTerrainCtx->tWireframeShader : gptTerrainCtx->tRegularShader);
        gptGfx->bind_vertex_buffer(ptEncoder, gptTerrainCtx->tVertexBuffer);

        plDynamicBinding tDynamicBinding = pl_allocate_dynamic_data(gptGfx, ptDevice, &gptTerrainCtx->tCurrentDynamicDataBlock);
        plTerrainDynamicData* ptDynamicData = (plTerrainDynamicData*)tDynamicBinding.pcData;
        memset(ptDynamicData, 0, sizeof(plTerrainDynamicData));
        ptDynamicData->tPos.xyz                   = tPos;
        ptDynamicData->tCameraViewProjection      = tMVP;
        ptDynamicData->fMetersPerHeightFieldTexel = gptTerrainCtx->fMetersPerTexel;
        ptDynamicData->fGlobalMaxHeight           = gptTerrainCtx->fMaxElevation;
        ptDynamicData->fGlobalMinHeight           = gptTerrainCtx->fMinElevation;
        ptDynamicData->tSunDirection.xyz          = gptTerrainCtx->tSunDirection;
        ptDynamicData->fXUVOffset                 = gptTerrainCtx->tCurrentExtent.x;
        ptDynamicData->fYUVOffset                 = gptTerrainCtx->tCurrentExtent.y;
        ptDynamicData->fStencilRadius             = gptTerrainCtx->fMetersPerTexel * 0.5f * (float)gptTerrainCtx->uHeightMapResolution;
        ptDynamicData->fBlurRadius                = gptTerrainCtx->fMetersPerTexel * (float)gptTerrainCtx->uHeightMapResolution;
        ptDynamicData->tMinMax.xy                 = gptTerrainCtx->tMinWorldPosition;
        ptDynamicData->tMinMax.zw                 = gptTerrainCtx->tMaxWorldPosition;


        gptGfx->bind_graphics_bind_groups(ptEncoder, gptTerrainCtx->tRegularShader, 0, 1, &gptTerrainCtx->atBindGroup0[gptGfx->get_current_frame_index()], 1, &tDynamicBinding);

        plDrawIndex tDraw = {
            .uInstanceCount = 1,
            .uIndexCount    = gptTerrainCtx->uIndexCount,
            .tIndexBuffer   = gptTerrainCtx->tIndexBuffer,
        };

        gptGfx->draw_indexed(ptEncoder, 1, &tDraw);

    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~debug drawing~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    if(gptTerrainCtx->tFlags & PL_TERRAIN_FLAGS_SHOW_ORIGIN)
    {
        plMat4 tOrigin = pl_identity_mat4();
        gptDraw->add_3d_transform(gptTerrainCtx->pt3dDrawlist, &tOrigin, 100000.0f, (plDrawLineOptions){.fThickness = 5.0f});
    }

    if(gptTerrainCtx->tFlags & PL_TERRAIN_FLAGS_SHOW_BOUNDARY)
    {
        plVec3 tOriginOrigin = {
            0.5f * (gptTerrainCtx->tMaxWorldPosition.x + gptTerrainCtx->tMinWorldPosition.x),
            0.5f * (gptTerrainCtx->fMaxElevation + gptTerrainCtx->fMinElevation),
            0.5f * (gptTerrainCtx->tMaxWorldPosition.y + gptTerrainCtx->tMinWorldPosition.y)
        };

        plVec3 tOrigin = tOriginOrigin;
        tOrigin.z = gptTerrainCtx->tMaxWorldPosition.y;
        gptDraw->add_3d_plane_xy_filled(gptTerrainCtx->pt3dDrawlist, tOrigin, 
            gptTerrainCtx->tMaxWorldPosition.x - gptTerrainCtx->tMinWorldPosition.x,
            gptTerrainCtx->fMaxElevation - gptTerrainCtx->fMinElevation,
            (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(0.5f, 0.0f, 0.0f, 0.95f)}
        );

        tOrigin.z = gptTerrainCtx->tMinWorldPosition.y;
        gptDraw->add_3d_plane_xy_filled(gptTerrainCtx->pt3dDrawlist, tOrigin, 
            gptTerrainCtx->tMaxWorldPosition.x - gptTerrainCtx->tMinWorldPosition.x,
            gptTerrainCtx->fMaxElevation - gptTerrainCtx->fMinElevation,
            (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(0.5f, 0.0f, 0.0f, 0.95f)}
        );

        tOrigin = tOriginOrigin;
        tOrigin.x = gptTerrainCtx->tMaxWorldPosition.x;
        gptDraw->add_3d_plane_yz_filled(gptTerrainCtx->pt3dDrawlist, tOrigin, 
            gptTerrainCtx->tMaxWorldPosition.y - gptTerrainCtx->tMinWorldPosition.y,
            gptTerrainCtx->fMaxElevation - gptTerrainCtx->fMinElevation,            
            (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(0.5f, 0.0f, 0.0f, 0.95f)}
        );

        tOrigin.x = gptTerrainCtx->tMinWorldPosition.x;
        gptDraw->add_3d_plane_yz_filled(gptTerrainCtx->pt3dDrawlist, tOrigin, 
            gptTerrainCtx->tMaxWorldPosition.y - gptTerrainCtx->tMinWorldPosition.y,
            gptTerrainCtx->fMaxElevation - gptTerrainCtx->fMinElevation,
            (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(0.5f, 0.0f, 0.0f, 0.95f)}
        );
    }

    if(gptTerrainCtx->tFlags & PL_TERRAIN_FLAGS_SHOW_GRID)
    {
        plVec3 tP0 = {gptTerrainCtx->tMinWorldPosition.x, gptTerrainCtx->fMaxElevation, gptTerrainCtx->tMinWorldPosition.y};
        plVec3 tP1 = {gptTerrainCtx->tMaxWorldPosition.x, gptTerrainCtx->fMaxElevation, gptTerrainCtx->tMinWorldPosition.y};
        const float fInc = (float)gptTerrainCtx->uTileSize * gptTerrainCtx->fMetersPerTexel;
        for(uint32_t i = 0; i < gptTerrainCtx->uHorizontalTiles; i++)
        {
            tP0.z += fInc;
            tP1.z += fInc;
            gptDraw->add_3d_line(gptTerrainCtx->pt3dDrawlist, tP0, tP1,   
                (plDrawLineOptions){.uColor = PL_COLOR_32_GREEN, .fThickness = 20.0f});
        }

        tP0 = (plVec3){gptTerrainCtx->tMinWorldPosition.x, gptTerrainCtx->fMaxElevation, gptTerrainCtx->tMinWorldPosition.y};
        tP1 = (plVec3){gptTerrainCtx->tMinWorldPosition.x, gptTerrainCtx->fMaxElevation, gptTerrainCtx->tMaxWorldPosition.y};
        for(uint32_t i = 0; i < gptTerrainCtx->uHorizontalTiles; i++)
        {
            tP0.x += fInc;
            tP1.x += fInc;
            gptDraw->add_3d_line(gptTerrainCtx->pt3dDrawlist, tP0, tP1,   
                (plDrawLineOptions){.uColor = PL_COLOR_32_GREEN, .fThickness = 20.0f});
        }
    }


    if(gptTerrainCtx->tFlags & PL_TERRAIN_FLAGS_DEBUG_TOOLS)
    {
        static uint32_t uSelectedTile = UINT32_MAX;
        if(gptUI->begin_window("Terrain", NULL, 0))
        {
            gptUI->layout_dynamic(0.0f, 1);
            gptUI->text("Free Prefetch Chunks: %u", pl_sb_size(gptTerrainCtx->sbuFreeChunks));
            gptUI->text("Free Prefetch Overflow: %u", pl_sb_size(gptTerrainCtx->sbuPrefetchOverflow));
            gptUI->text("Prefetch Count: %d", gptAtomics->atomic_load(gptTerrainCtx->ptPrefetchCounter));
            gptUI->text("Tiles: %u", gptTerrainCtx->uTileCount);
            gptUI->text("Active Tiles: %u", pl_sb_size(gptTerrainCtx->sbuActiveTileIndices));
            gptUI->checkbox("Local Terrain", &gptTerrainCtx->bShowLocal);
            gptUI->checkbox("World Terrain", &gptTerrainCtx->bShowWorld);
            gptUI->checkbox("Atlas", &gptTerrainCtx->bShowAtlas);
            bool bSunChange = false;
            if(gptUI->slider_float("Sun X", &gptTerrainCtx->tSunDirection.x, -1.0f, 1.0f, 0)) bSunChange = true;
            if(gptUI->slider_float("Sun Y", &gptTerrainCtx->tSunDirection.y, -1.0f, 1.0f, 0)) bSunChange = true;
            if(gptUI->slider_float("Sun Z", &gptTerrainCtx->tSunDirection.z, -1.0f, 1.0f, 0)) bSunChange = true;

            if(bSunChange) gptTerrainCtx->tSunDirection = pl_norm_vec3(gptTerrainCtx->tSunDirection);
            if(gptUI->button("Process All")) gptTerrainCtx->bProcessAll = true;
            if(gptUI->begin_collapsing_header("Tiles", 0))
            {
                if(gptUI->input_text_hint("Tiles", "Filter (inc,-exc)", gptTerrainCtx->tFilter.acInputBuffer, 256, 0))
                {
                    gptUI->text_filter_build(&gptTerrainCtx->tFilter);
                }
            
                plVec2 tWindowSize = gptUI->get_window_size();
                plVec2 tCursorPos = gptUI->get_cursor_pos();
                // gptUI->set_next_window_size(pl_sub_vec2(tWindowSize, pl_sub_vec2(tCursorPos, gptUI->get_window_pos())), 0);
                gptUI->layout_dynamic(pl_sub_vec2(tWindowSize, pl_sub_vec2(tCursorPos, gptUI->get_window_pos())).y - 15.0f, 1);
                if(gptUI->begin_child("Tile List", 0, 0))
                {

                    if(gptUI->text_filter_active(&gptTerrainCtx->tFilter))
                    {
                        for(uint32_t i = 0; i < gptTerrainCtx->uTileCount; i++)
                        {
                            bool bSelected = uSelectedTile == i;
                            if(gptUI->selectable(gptTerrainCtx->atTiles[i].acFile, &bSelected, 0))
                            {
                                if(bSelected)
                                    uSelectedTile = i;
                                else
                                    uSelectedTile = UINT32_MAX;
                            }
                        }
                    }
                    else
                    {
                        plUiClipper tClipper = {gptTerrainCtx->uTileCount};
                        while(gptUI->step_clipper(&tClipper))
                        {
                            for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
                            {
                                bool bSelected = uSelectedTile == i;
                                if(gptUI->selectable(gptTerrainCtx->atTiles[i].acFile, &bSelected, 0))
                                {
                                    if(bSelected)
                                        uSelectedTile = i;
                                    else
                                        uSelectedTile = UINT32_MAX;
                                }
                            }
                        }
                    }
                    gptUI->end_child();
                }
                gptUI->end_collapsing_header();
            }
            gptUI->layout_dynamic(0.0f, 1);

            if(gptUI->begin_collapsing_header("Active Tiles", 0))
            {
                if(gptUI->input_text_hint("Tiles", "Filter (inc,-exc)", gptTerrainCtx->tFilter.acInputBuffer, 256, 0))
                {
                    gptUI->text_filter_build(&gptTerrainCtx->tFilter);
                }
            
                plVec2 tWindowSize = gptUI->get_window_size();
                plVec2 tCursorPos = gptUI->get_cursor_pos();
                gptUI->layout_dynamic(pl_sub_vec2(tWindowSize, pl_sub_vec2(tCursorPos, gptUI->get_window_pos())).y - 15.0f, 1);
                if(gptUI->begin_child("Active Tile List", 0, 0))
                {

                    const uint32_t uActiveTileCount = pl_sb_size(gptTerrainCtx->sbuActiveTileIndices);

                    if(gptUI->text_filter_active(&gptTerrainCtx->tFilter))
                    {
                        for(uint32_t i = 0; i < uActiveTileCount; i++)
                        {
                            const uint32_t uIndex = gptTerrainCtx->sbuActiveTileIndices[i];
                            bool bSelected = uSelectedTile == uIndex;
                            if(gptUI->selectable(gptTerrainCtx->atTiles[uIndex].acFile, &bSelected, 0))
                            {
                                if(bSelected)
                                    uSelectedTile = uIndex;
                                else
                                    uSelectedTile = UINT32_MAX;
                            }
                        }
                    }
                    else
                    {
                        plUiClipper tClipper = {uActiveTileCount};
                        while(gptUI->step_clipper(&tClipper))
                        {
                            for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
                            {
                                const uint32_t uIndex = gptTerrainCtx->sbuActiveTileIndices[i];
                                bool bSelected = uSelectedTile == uIndex;
                                if(gptUI->selectable(gptTerrainCtx->atTiles[uIndex].acFile, &bSelected, 0))
                                {
                                    if(bSelected)
                                        uSelectedTile = uIndex;
                                    else
                                        uSelectedTile = UINT32_MAX;
                                }
                            }
                        }
                    }
                    gptUI->end_child();
                }
                gptUI->end_collapsing_header();
            }

            gptUI->layout_dynamic(0.0f, 1);

            gptUI->end_window();
        }

        if(uSelectedTile != UINT32_MAX)
        {
            plHeightMapTile* ptTile = &gptTerrainCtx->atTiles[uSelectedTile];
            if(gptUI->begin_window("Selected Tile", NULL, 0))
            {
                
                gptUI->text("File: %s", ptTile->acFile);
                gptUI->text("Chunk Index: %u", ptTile->_uChunkIndex);
                gptUI->text("Source Texture Offset: %u, %u", ptTile->uXOffset, ptTile->uYOffset);
                gptUI->text("Dest Texture Offset: %u, %u", ptTile->_uXOffsetActual, ptTile->_uYOffsetActual);
                gptUI->text("Grid Coord: %d, %d", ptTile->_iXCoord, ptTile->_iYCoord);
                gptUI->text("World Pos: %g, 0.0, %g", ptTile->tWorldPos.x, ptTile->tWorldPos.y);
                gptUI->end_window();
            }

            plVec3 tOrigin = {
                (ptTile->tWorldPos.x + (float)gptTerrainCtx->uTileSize * gptTerrainCtx->fMetersPerTexel * 0.5f), 
                gptTerrainCtx->fMaxElevation,
                (ptTile->tWorldPos.y + (float)gptTerrainCtx->uTileSize * gptTerrainCtx->fMetersPerTexel * 0.5f),
            };

            gptDraw->add_3d_plane_xz_filled(gptTerrainCtx->pt3dDrawlist, tOrigin, 
                (float)gptTerrainCtx->uTileSize * gptTerrainCtx->fMetersPerTexel,
                (float)gptTerrainCtx->uTileSize * gptTerrainCtx->fMetersPerTexel,   
                (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(0.0f, 0.5f, 0.0f, 1.0f)});

            gptDraw->add_3d_line(gptTerrainCtx->pt3dDrawlist,
                tOrigin,  
                (plVec3){tOrigin.x, gptTerrainCtx->fMinElevation, tOrigin.z},  
                (plDrawLineOptions){.uColor = PL_COLOR_32_RGBA(0.0f, 0.5f, 0.0f, 1.0f), .fThickness = 10.0f});
        }

        if(gptTerrainCtx->bShowAtlas && gptUI->begin_window("Terrain Atlas", &gptTerrainCtx->bShowAtlas, 0))
        {
            const uint32_t uTilesAcross = gptTerrainCtx->uHeightMapResolution / gptTerrainCtx->uTileSize;
            const uint32_t uActiveTileCount = pl_sb_size(gptTerrainCtx->sbuActiveTileIndices);
            plVec2 tWindowSize = gptUI->get_window_size();


            static bool bShowNumbers = false;
            // gptUI->layout_dynamic(0.0f, 3);
            gptUI->checkbox("Show Numbers", &bShowNumbers);
            // gptUI->dummy((plVec2){1.0f, 1.0f});
            gptUI->layout_dynamic(0.0f, 1);

            plVec2 tCursorPos = gptUI->get_cursor_pos();
            plDrawLayer2D* ptLayer = gptUI->get_window_fg_drawlayer();

            const float fTileSize = ((float)gptTerrainCtx->uTileSize / (float)gptTerrainCtx->uHeightMapResolution) * (tWindowSize.x - 15.0f);

            char acTileNumber[16] = {0};
            for(uint32_t i = 0; i < uActiveTileCount; i++)
            {
                const uint32_t uIndex = gptTerrainCtx->sbuActiveTileIndices[i];
                const plHeightMapTile* ptTile = &gptTerrainCtx->atTiles[uIndex];
                plVec2 tTilePos = {
                    tCursorPos.x + fTileSize * (float)uTilesAcross * (float)ptTile->_uXOffsetActual / (float)gptTerrainCtx->uHeightMapResolution,
                    tCursorPos.y + fTileSize * (float)uTilesAcross * (float)ptTile->_uYOffsetActual / (float)gptTerrainCtx->uHeightMapResolution,
                };

                plVec4 tColor = {0.0f, 0.5f, 0.2f, 1.0f};

                if(ptTile->_iXCoord % 2 == ptTile->_iYCoord % 2)
                    tColor.a *= 0.75f;

                gptDraw->add_rect_filled(ptLayer,
                    tTilePos,
                    pl_add_vec2(tTilePos, (plVec2){fTileSize, fTileSize}),
                    (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(tColor.r, tColor.g, tColor.b, tColor.a)}
                );

                if(bShowNumbers)
                {
                    sprintf(acTileNumber, "%u", uIndex);
                    gptDraw->add_text(ptLayer,
                        tTilePos,
                        acTileNumber,
                        (plDrawTextOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 1.0f, 1.0f, 1.0f), .ptFont = gptUI->get_default_font()}
                    );
                }
            }

            plVec2 tXOffsetTop = {
                tCursorPos.x + (float)gptTerrainCtx->uCurrentXOffset * fTileSize,
                tCursorPos.y,
            };

            plVec2 tXOffsetBottom = {
                tCursorPos.x + (float)gptTerrainCtx->uCurrentXOffset * fTileSize,
                tCursorPos.y + tWindowSize.y,
            };

            plVec2 tYOffsetLeft = {
                tCursorPos.x,
                tCursorPos.y + (float)gptTerrainCtx->uCurrentYOffset * fTileSize,
            };

            plVec2 tYOffsetRight = {
                tCursorPos.x + tWindowSize.x,
                tCursorPos.y + (float)gptTerrainCtx->uCurrentYOffset * fTileSize,
            };

            gptDraw->add_line(ptLayer,
                tXOffsetTop,
                tXOffsetBottom,
                (plDrawLineOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 1.0f, 1.0f, 1.0f), .fThickness = 2.0f});

            gptDraw->add_line(ptLayer,
                tYOffsetLeft,
                tYOffsetRight,
                (plDrawLineOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 1.0f, 1.0f, 1.0f), .fThickness = 2.0f});

            gptUI->dummy((plVec2){fTileSize * (float)uTilesAcross, fTileSize * (float)uTilesAcross});
            gptUI->end_window();
        }

        if(gptTerrainCtx->bShowWorld && gptUI->begin_window("Terrain World", &gptTerrainCtx->bShowWorld, 0))
        {
            const uint32_t uTilesAcross = gptTerrainCtx->uHeightMapResolution / gptTerrainCtx->uTileSize;
            const uint32_t uActiveTileCount = pl_sb_size(gptTerrainCtx->sbuActiveTileIndices);
            plVec2 tWindowSize = gptUI->get_window_size();
            plVec2 tCursorPos = gptUI->get_cursor_pos();
            plDrawLayer2D* ptLayer = gptUI->get_window_fg_drawlayer();

            const float fTileSize = (tWindowSize.x - 15.0f) / (float)gptTerrainCtx->uHorizontalTiles;

            char acTileNumber[16] = {0};
            for(uint32_t i = 0; i < gptTerrainCtx->uTileCount; i++)
            {
                const plHeightMapTile* ptTile = &gptTerrainCtx->atTiles[i];


                uint32_t uColor = PL_COLOR_32_RGBA(0.0f, 0.0f, 0.0f, 0.0f);
                if     (ptTile->tFlags & PL_TERRAIN_TILE_FLAGS_ACTIVE) uColor = PL_COLOR_32_RGBA(0.0f, 0.5f, 0.0f, 1.0f);
                else if(ptTile->tFlags & PL_TERRAIN_TILE_FLAGS_UPLOADED) uColor = PL_COLOR_32_RGBA(0.5f, 0.5f, 0.0f, 1.0f);
                else if(ptTile->tFlags & PL_TERRAIN_TILE_FLAGS_PROCESSED) uColor = PL_COLOR_32_RGBA(0.5f, 0.0f, 0.0f, 1.0f);

                plVec2 tTilePos = {
                    tCursorPos.x + fTileSize * (float)ptTile->_iXCoord,
                    tCursorPos.y + fTileSize * (float)ptTile->_iYCoord,
                };
                gptDraw->add_rect_filled(ptLayer,
                    tTilePos,
                    pl_add_vec2(tTilePos, (plVec2){fTileSize, fTileSize}),
                    (plDrawSolidOptions){.uColor = uColor}
                );
            }

            gptUI->dummy((plVec2){fTileSize * (float)gptTerrainCtx->uHorizontalTiles, fTileSize * (float)gptTerrainCtx->uVerticalTiles});
            gptUI->end_window();
        }

        if(gptTerrainCtx->bShowLocal && gptUI->begin_window("Local Terrain", &gptTerrainCtx->bShowLocal, 0))
        {
            const uint32_t uTilesAcross = gptTerrainCtx->uHeightMapResolution / gptTerrainCtx->uTileSize;
            const uint32_t uActiveTileCount = pl_sb_size(gptTerrainCtx->sbuActiveTileIndices);
            plVec2 tWindowSize = gptUI->get_window_size();


            static int iFiltering = 0;
            static int iBuffer = 2;

            gptUI->slider_int("Perimeter Buffer", &iBuffer, 0, 10, 0);

            gptUI->layout_dynamic(0.0f, 3);
            gptUI->radio_button("None", &iFiltering, 0);
            gptUI->radio_button("Active", &iFiltering, PL_TERRAIN_TILE_FLAGS_ACTIVE);
            gptUI->radio_button("Queued", &iFiltering, PL_TERRAIN_TILE_FLAGS_QUEUED);
            gptUI->radio_button("Uploaded", &iFiltering, PL_TERRAIN_TILE_FLAGS_UPLOADED);
            gptUI->radio_button("Processed", &iFiltering, PL_TERRAIN_TILE_FLAGS_PROCESSED);
            gptUI->dummy((plVec2){1.0f, 1.0f});
            gptUI->layout_dynamic(0.0f, 1);

            plVec2 tCursorPos = gptUI->get_cursor_pos();
            plDrawLayer2D* ptLayer = gptUI->get_window_fg_drawlayer();

            char acTileNumber[16] = {0};

            int uStartOffset = gptTerrainCtx->uPrefetchRadius + iBuffer;
            int uVerticalCount = (int)(gptTerrainCtx->iCurrentYCoordMax - gptTerrainCtx->iCurrentYCoordMin) + gptTerrainCtx->uPrefetchRadius*2 + iBuffer * 2;
            int uHorizontalCount = (int)(gptTerrainCtx->iCurrentXCoordMax - gptTerrainCtx->iCurrentXCoordMin) + gptTerrainCtx->uPrefetchRadius*2 + iBuffer * 2;

            const float fTileSize = (tWindowSize.x - 15.0f) / (float)uHorizontalCount;
            for(int j = 0; j < uVerticalCount; j++)
            {
                for(int i = 0; i < uHorizontalCount; i++)
                {
                    int uXIndex = gptTerrainCtx->iCurrentXCoordMin + i - uStartOffset;
                    int uYIndex = gptTerrainCtx->iCurrentYCoordMin + j - uStartOffset;
                    plHeightMapTile* ptTile = pl__terrain_get_tile_by_pos(uXIndex, uYIndex);
                    int iTileIndex = pl__terrain_get_tile_index_by_pos(uXIndex, uYIndex);
                    if(ptTile)
                    {
                        plVec4 tColor = {0.2f, 0.2f, 0.2f, 1.0f};

                        if(iFiltering == 0)
                        {
                            if     (ptTile->tFlags & PL_TERRAIN_TILE_FLAGS_ACTIVE)    tColor = (plVec4){0.0f, 0.5f, 0.0f, 1.0f};
                            else if(ptTile->tFlags & PL_TERRAIN_TILE_FLAGS_UPLOADED)  tColor = (plVec4){0.5f, 0.5f, 0.0f, 1.0f};
                            else if(ptTile->tFlags & PL_TERRAIN_TILE_FLAGS_PROCESSED) tColor = (plVec4){0.5f, 0.0f, 0.0f, 1.0f};
                        }
                        else
                        {
                            if (ptTile->tFlags & iFiltering) tColor = (plVec4){0.0f, 0.3f, 0.9f, 1.0f};
                        }

                        if(i % 2 == j % 2)
                            tColor.a *= 0.75f;


                        plVec2 tTilePos0 = {
                            tCursorPos.x + fTileSize * (float)i,
                            tCursorPos.y + fTileSize * (float)j,
                        };

                        plVec2 tTilePos1 = pl_add_vec2(tTilePos0, (plVec2){fTileSize, fTileSize});

                        if(gptIOI->is_mouse_hovering_rect(tTilePos0, tTilePos1))
                        {
                            gptUI->begin_tooltip();
                            gptUI->text("Index: %d", iTileIndex);
                            gptUI->text("File: %s", ptTile->acFile);
                            gptUI->text("Chunk Index: %u", ptTile->_uChunkIndex);
                            gptUI->text("Source Texture Offset: %u, %u", ptTile->uXOffset, ptTile->uYOffset);
                            gptUI->text("Dest Texture Offset: %u, %u", ptTile->_uXOffsetActual, ptTile->_uYOffsetActual);
                            gptUI->text("Grid Coord: %d, %d", ptTile->_iXCoord, ptTile->_iYCoord);
                            gptUI->text("World Pos: %g, 0.0, %g", ptTile->tWorldPos.x, ptTile->tWorldPos.y);
                            gptUI->end_tooltip();
                        }

                        gptDraw->add_rect_filled(ptLayer,
                            tTilePos0,
                            tTilePos1,
                            (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(tColor.r, tColor.g, tColor.b, tColor.a)}
                        );
                    }
                }
            }

            plVec2 tTilePos0 = {
                tCursorPos.x + fTileSize * (float)uStartOffset,
                tCursorPos.y + fTileSize * (float)uStartOffset,
            };

            plVec2 tTilePos1 = {
                tCursorPos.x + fTileSize * (float)(gptTerrainCtx->iCurrentXCoordMax - gptTerrainCtx->iCurrentXCoordMin + uStartOffset + 1),
                tCursorPos.y + fTileSize * (float)(gptTerrainCtx->iCurrentYCoordMax - gptTerrainCtx->iCurrentYCoordMin + uStartOffset + 1),
            };

            gptDraw->add_rect(ptLayer,
                tTilePos0,
                tTilePos1,
                (plDrawLineOptions){.uColor = PL_COLOR_32_WHITE, .fThickness = 1.0f}
            );

            // gptDraw->add_rect(ptLayer,
            //     tCursorPos,
            //     pl_add_vec2(tCursorPos, (plVec2){fTileSize * (float)uHorizontalCount, fTileSize * (float)uVerticalCount}),
            //     (plDrawLineOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 1.0f, 1.0f, 1.0f), .fThickness = 1.0f}
            // );
            gptUI->dummy((plVec2){fTileSize * (float)uHorizontalCount, fTileSize * (float)uVerticalCount});
            gptUI->end_window();
        }
    }

    // submit 3d drawlist
    plRenderPassHandle tRenderEncoderHandle = gptGfx->get_encoder_render_pass(ptEncoder);
    float fOutputWidth = gptGfx->get_render_pass(ptDevice, tRenderEncoderHandle)->tDesc.tDimensions.x;
    float fOutputHeight = gptGfx->get_render_pass(ptDevice, tRenderEncoderHandle)->tDesc.tDimensions.y;
    gptDraw->submit_3d_drawlist(gptTerrainCtx->pt3dDrawlist,
        ptEncoder,
        fOutputWidth,
        fOutputHeight,
        &tMVP,
        PL_DRAW_FLAG_DEPTH_TEST | PL_DRAW_FLAG_DEPTH_WRITE | PL_DRAW_FLAG_REVERSE_Z_DEPTH,
        PL_SAMPLE_COUNT_1);
}



void
pl_terrain_load_mesh(plCommandBuffer* ptCmdBuffer, const char* pcFile, uint32_t uMeshLevels, uint32_t uMeshBaseLodExtentTexels)
{
    plDevice* ptDevice = gptTerrainCtx->ptDevice;
    gptTerrainCtx->uMeshLevels = uMeshLevels;

    if(gptVfs->does_file_exist(pcFile))
    {
        plVfsFileHandle tFileHandle = gptVfs->open_file(pcFile, PL_VFS_FILE_MODE_READ);
        size_t szFileSize = 0;
        gptVfs->read_file(tFileHandle, NULL, &szFileSize);
        void* pBuffer = PL_ALLOC(szFileSize);
        memset(pBuffer, 0, szFileSize);
        gptVfs->read_file(tFileHandle, pBuffer, &szFileSize);

        char* pcBuffer = (char*)pBuffer;

        gptTerrainCtx->uIndexCount = *(uint32_t*)pcBuffer;
        gptTerrainCtx->uVertexCount = *(uint32_t*)&pcBuffer[sizeof(uint32_t)];
        gptTerrainCtx->puIndexBuffer = (uint32_t*)PL_ALLOC(sizeof(uint32_t) * gptTerrainCtx->uIndexCount);
        gptTerrainCtx->ptVertexBuffer = (plVec3*)PL_ALLOC(sizeof(plVec3) * gptTerrainCtx->uVertexCount);

        memcpy(gptTerrainCtx->puIndexBuffer, (uint32_t*)&pcBuffer[sizeof(uint32_t) * 2], sizeof(uint32_t) * gptTerrainCtx->uIndexCount);
        memcpy(gptTerrainCtx->ptVertexBuffer, &pcBuffer[sizeof(uint32_t) * (gptTerrainCtx->uIndexCount + 2)], sizeof(plVec3) * gptTerrainCtx->uVertexCount);
        gptVfs->close_file(tFileHandle);
        PL_FREE(pBuffer);
    }
    else
    {
        plMeshBuilderOptions tOptions = {0};
        plMeshBuilder* ptBuilder = gptMeshBuilder->create(tOptions);

        int numMeshLODLevels = (int)uMeshLevels;
        int meshBaseLODExtentHeightfieldTexels = (int)uMeshBaseLodExtentTexels;

        // Store LOD in the Y coordinate, store XZ on the texel grid resolution
        for (int level = 0; level < numMeshLODLevels; ++level)
        {
            printf("Level: %d\n", level);
            const int step = (1 << level);
            const int prevStep = pl_max(0, (1 << (level - 1)));
            const int halfStep = prevStep;

            const int g = meshBaseLODExtentHeightfieldTexels / 2;
            const float L = (float)level;

            // Move up one grid level; used when stitching
            const plVec3 nextLevel = {0, 1, 0};

            // Pad by one element to hide the gap to the next level
            const int pad = 1;
            const int radius = step * (g + pad);
            for (int z = -radius; z < radius; z += step)
            {
                for (int x = -radius; x < radius; x += step)
                {
                    if (pl_max(abs(x + halfStep), abs(z + halfStep)) >= g * prevStep)
                    {
                        // Cleared the cutout from the previous level. Tessellate the
                        // square.

                        //   A-----B-----C
                        //   | \   |   / |
                        //   |   \ | /   |
                        //   D-----E-----F
                        //   |   / | \   |
                        //   | /   |   \ |
                        //   G-----H-----I

                        const plVec3 A = {(float)x, L, (float)z};
                        const plVec3 C = {(float)(x + step), L, A.z};
                        const plVec3 G = {A.x, L, (float)(z + step)};
                        const plVec3 I = {C.x, L, G.z};

                        const plVec3 B = pl_mul_vec3_scalarf(pl_add_vec3(A, C), 0.5f);
                        const plVec3 D = pl_mul_vec3_scalarf(pl_add_vec3(A, G), 0.5f);
                        const plVec3 F = pl_mul_vec3_scalarf(pl_add_vec3(C, I), 0.5f);
                        const plVec3 H = pl_mul_vec3_scalarf(pl_add_vec3(G, I), 0.5f);

                        const plVec3 E = pl_mul_vec3_scalarf(pl_add_vec3(A, I), 0.5f);

                        // Stitch the border into the next level

                        if (x == -radius)
                        {
                            //   A-----B-----C
                            //   | \   |   / |
                            //   |   \ | /   |
                            //   |     E-----F
                            //   |   / | \   |
                            //   | /   |   \ |
                            //   G-----H-----I
                            gptMeshBuilder->add_triangle(ptBuilder, E, A, G);
                        }
                        else
                        {
                            gptMeshBuilder->add_triangle(ptBuilder, E, A, D);
                            gptMeshBuilder->add_triangle(ptBuilder, E, D, G);
                        }

                        if (z == radius - 1)
                        {
                            gptMeshBuilder->add_triangle(ptBuilder, E, G, I);
                        }
                        else
                        {
                            gptMeshBuilder->add_triangle(ptBuilder, E, G, H);
                            gptMeshBuilder->add_triangle(ptBuilder, E, H, I);
                        }

                        if (x == radius - 1)
                        {
                            gptMeshBuilder->add_triangle(ptBuilder, E, I, C);
                        }
                        else
                        {
                            gptMeshBuilder->add_triangle(ptBuilder, E, I, F);
                            gptMeshBuilder->add_triangle(ptBuilder, E, F, C);
                        }

                        if(z == -radius)
                        {
                            gptMeshBuilder->add_triangle(ptBuilder, E, C, A);
                        }
                        else
                        {
                            gptMeshBuilder->add_triangle(ptBuilder, E, C, B);
                            gptMeshBuilder->add_triangle(ptBuilder, E, B, A);
                        }
                    }
                }
            }
        }

        gptMeshBuilder->commit(ptBuilder, NULL, NULL, &gptTerrainCtx->uIndexCount, &gptTerrainCtx->uVertexCount);
        gptTerrainCtx->puIndexBuffer = (uint32_t*)PL_ALLOC(sizeof(uint32_t) * gptTerrainCtx->uIndexCount);
        gptTerrainCtx->ptVertexBuffer = (plVec3*)PL_ALLOC(sizeof(plVec3) * gptTerrainCtx->uVertexCount);
        gptMeshBuilder->commit(ptBuilder, gptTerrainCtx->puIndexBuffer, gptTerrainCtx->ptVertexBuffer, &gptTerrainCtx->uIndexCount, &gptTerrainCtx->uVertexCount);
        gptMeshBuilder->cleanup(ptBuilder);

        plVfsFileHandle tFileHandle = gptVfs->open_file(pcFile, PL_VFS_FILE_MODE_WRITE);
        gptVfs->write_file(tFileHandle, &gptTerrainCtx->uIndexCount, sizeof(uint32_t));
        gptVfs->write_file(tFileHandle, &gptTerrainCtx->uVertexCount, sizeof(uint32_t));
        gptVfs->write_file(tFileHandle, gptTerrainCtx->puIndexBuffer, sizeof(uint32_t) * gptTerrainCtx->uIndexCount);
        gptVfs->write_file(tFileHandle, gptTerrainCtx->ptVertexBuffer, sizeof(plVec3) * gptTerrainCtx->uVertexCount);
        gptVfs->close_file(tFileHandle);
    }

    plBufferDesc tVertexBufferDesc = {
        .tUsage      = PL_BUFFER_USAGE_VERTEX | PL_BUFFER_USAGE_TRANSFER_DESTINATION,
        .szByteSize  = sizeof(plVec3) * gptTerrainCtx->uVertexCount,
        .pcDebugName = "clipmap vertex buffer",
    };

    plBufferDesc tIndexBufferDesc = {
        .tUsage      = PL_BUFFER_USAGE_INDEX | PL_BUFFER_USAGE_TRANSFER_DESTINATION,
        .szByteSize  = sizeof(uint32_t) * gptTerrainCtx->uIndexCount,
        .pcDebugName = "clipmap index buffer",
    };

    gptTerrainCtx->tVertexBuffer = gptGfx->create_buffer(ptDevice, &tVertexBufferDesc, NULL);
    gptTerrainCtx->tIndexBuffer = gptGfx->create_buffer(ptDevice, &tIndexBufferDesc, NULL);

    plBuffer* ptIndexBuffer = gptGfx->get_buffer(ptDevice, gptTerrainCtx->tIndexBuffer);

    size_t szBuddyBlockSize = gptGpuAllocators->get_buddy_block_size();

    plDeviceMemoryAllocatorI* ptAllocator = gptTerrainCtx->tLocalDedicatedAllocator;

    if(ptIndexBuffer->tMemoryRequirements.ulSize >= szBuddyBlockSize)
        ptAllocator = gptTerrainCtx->tLocalBuddyAllocator;

    const plDeviceMemoryAllocation tIndexMemory = ptAllocator->allocate(ptAllocator->ptInst,
        ptIndexBuffer->tMemoryRequirements.uMemoryTypeBits,
        ptIndexBuffer->tMemoryRequirements.ulSize,
        ptIndexBuffer->tMemoryRequirements.ulAlignment,
        "clipmap index memory");

    plBuffer* ptVertexBuffer = gptGfx->get_buffer(ptDevice, gptTerrainCtx->tVertexBuffer);

    ptAllocator = gptTerrainCtx->tLocalDedicatedAllocator;

    if(ptVertexBuffer->tMemoryRequirements.ulSize >= szBuddyBlockSize)
        ptAllocator = gptTerrainCtx->tLocalBuddyAllocator;

    const plDeviceMemoryAllocation tVertexMemory = ptAllocator->allocate(ptAllocator->ptInst,
        ptVertexBuffer->tMemoryRequirements.uMemoryTypeBits,
        ptVertexBuffer->tMemoryRequirements.ulSize,
        ptVertexBuffer->tMemoryRequirements.ulAlignment,
        "clipmap vertex memory");

    gptGfx->bind_buffer_to_memory(ptDevice, gptTerrainCtx->tIndexBuffer, &tIndexMemory);
    gptGfx->bind_buffer_to_memory(ptDevice, gptTerrainCtx->tVertexBuffer, &tVertexMemory);

    size_t szMaxBufferSize = tVertexBufferDesc.szByteSize + tIndexBufferDesc.szByteSize;

    // create staging buffer
    plBufferDesc tStagingBufferDesc = {
        .tUsage      = PL_BUFFER_USAGE_TRANSFER_DESTINATION | PL_BUFFER_USAGE_TRANSFER_SOURCE,
        .szByteSize  = szMaxBufferSize,
        .pcDebugName = "staging buffer"
    };

    plBuffer* ptStagingBuffer = NULL;
    plBufferHandle tStagingBuffer = gptGfx->create_buffer(ptDevice, &tStagingBufferDesc, &ptStagingBuffer);

    // allocate memory for the vertex buffer
    ptAllocator = gptTerrainCtx->tStagingDedicatedAllocator;
    const plDeviceMemoryAllocation tStagingBufferAllocation = ptAllocator->allocate(ptAllocator->ptInst,
        ptStagingBuffer->tMemoryRequirements.uMemoryTypeBits,
        ptStagingBuffer->tMemoryRequirements.ulSize,
        0,
        "staging buffer memory");

    // bind the buffer to the new memory allocation
    gptGfx->bind_buffer_to_memory(ptDevice, tStagingBuffer, &tStagingBufferAllocation);

    memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, gptTerrainCtx->puIndexBuffer, tIndexBufferDesc.szByteSize);
    PL_FREE(gptTerrainCtx->puIndexBuffer);
    gptTerrainCtx->puIndexBuffer = NULL;
    memcpy(&ptStagingBuffer->tMemoryAllocation.pHostMapped[tIndexBufferDesc.szByteSize], gptTerrainCtx->ptVertexBuffer, tVertexBufferDesc.szByteSize);
    PL_FREE(gptTerrainCtx->ptVertexBuffer);
    gptTerrainCtx->ptVertexBuffer = NULL;

    plBlitEncoder* ptBlit = gptGfx->begin_blit_pass(ptCmdBuffer);
    gptGfx->pipeline_barrier_blit(ptBlit, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);
    gptGfx->copy_buffer(ptBlit, tStagingBuffer, gptTerrainCtx->tIndexBuffer, 0, 0, tIndexBufferDesc.szByteSize);
    gptGfx->copy_buffer(ptBlit, tStagingBuffer, gptTerrainCtx->tVertexBuffer, (uint32_t)tIndexBufferDesc.szByteSize, 0, tVertexBufferDesc.szByteSize);
    gptGfx->pipeline_barrier_blit(ptBlit, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
    gptGfx->end_blit_pass(ptBlit);

    gptGfx->queue_buffer_for_deletion(gptTerrainCtx->ptDevice, tStagingBuffer);
}

//-----------------------------------------------------------------------------
// [SECTION] internal helpers implementation
//-----------------------------------------------------------------------------

static void
pl__terrain_prepare(plCommandBuffer* ptCmdBuffer, plCamera* ptCamera)
{

    plDevice* ptDevice = gptTerrainCtx->ptDevice;

    // determine required tiles
    if(gptTerrainCtx->bNeedsUpdate)
    {
        pl__terrain_process_height_map_tiles(ptCamera);
    }

    if(gptAtomics->atomic_load(gptTerrainCtx->ptPrefetchDirty) > 0 || gptTerrainCtx->bProcessAll)
    {

        // sets active tiles & decides final tile location
        // PL_ASSERT(gptTerrainCtx->uNewActiveTileCount == 0);
        // pl__terrain_process_height_map_tiles(ptTerrain, ptCmdBuffer);

        // load new tiles & process them
        const uint32_t uActiveTileCount = pl_sb_size(gptTerrainCtx->sbuActiveTileIndices);

        // add new tile data to raw texture
        plBuffer* ptStagingBuffer = gptGfx->get_buffer(ptDevice, gptTerrainCtx->tStagingBuffer);

        // find active tiles that require updates (newly visible tiles)
    
        plBlitEncoder* ptBlit = gptGfx->begin_blit_pass(ptCmdBuffer);
        gptGfx->pipeline_barrier_blit(ptBlit, PL_PIPELINE_STAGE_VERTEX_SHADER, PL_ACCESS_SHADER_READ, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);

        // copy unprocessed tiles into raw texture
        for(uint32_t i = 0; i < uActiveTileCount; i++)
        {
            plHeightMapTile* ptTile = &gptTerrainCtx->atTiles[gptTerrainCtx->sbuActiveTileIndices[i]];

            if(!(ptTile->tFlags & PL_TERRAIN_TILE_FLAGS_UPLOADED))
            {
                continue;
            }

            // if(ptTile->tFlags & PL_TERRAIN_TILE_FLAGS_PROCESSED_INTERMEDIATE)
            // {
            //     continue;
            // }

            if(!(ptTile->tFlags & PL_TERRAIN_TILE_FLAGS_PROCESSED) || gptTerrainCtx->bProcessAll) // otherwise, its already in the texture
            {

                ptTile->tFlags |= PL_TERRAIN_TILE_FLAGS_PROCESSED_INTERMEDIATE;

                const plTerrainBufferChunk* ptChunk = &gptTerrainCtx->atChunks[ptTile->_uChunkIndex];

                // copy staging buffer contents to raw texture
                plBufferImageCopy tBufferImageCopy = {
                    .uImageWidth        = gptTerrainCtx->uTileSize,
                    .uImageHeight       = gptTerrainCtx->uTileSize,
                    .uImageDepth        = 1,
                    .iImageOffsetX      = ptTile->_uXOffsetActual,
                    .iImageOffsetY      = ptTile->_uYOffsetActual,
                    .uLayerCount        = 1,
                    .szBufferOffset     = ptChunk->szOffset,
                    .tCurrentImageUsage = PL_TEXTURE_USAGE_STORAGE,
                };
                gptGfx->copy_buffer_to_texture(ptBlit, gptTerrainCtx->tStagingBuffer, gptTerrainCtx->tRawTexture, 1, &tBufferImageCopy);
            }
        }
        
        gptGfx->pipeline_barrier_blit(ptBlit, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
        gptGfx->end_blit_pass(ptBlit);
        
        // let individual frames know they need to update
        // active texture (but we must preprocess first in the
        // next step)
        // if(bEndOfAmortization)
        {

            // uUpdateLimit = 1;

            // set proper texture usages
            ptBlit = gptGfx->begin_blit_pass(ptCmdBuffer);
            gptGfx->pipeline_barrier_blit(ptBlit, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_READ | PL_ACCESS_SHADER_WRITE, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_READ | PL_ACCESS_TRANSFER_WRITE);
            gptGfx->set_texture_usage(ptBlit, gptTerrainCtx->tProcessedTexture, PL_TEXTURE_USAGE_STORAGE, PL_TEXTURE_USAGE_SAMPLED);
            gptGfx->pipeline_barrier_blit(ptBlit, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_READ | PL_ACCESS_TRANSFER_WRITE, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_READ | PL_ACCESS_SHADER_WRITE);
            gptGfx->end_blit_pass(ptBlit);

            // preprocess heightmap (i.e. calculate normals)

            plComputeEncoder* ptComputeEncoder = gptGfx->begin_compute_pass(ptCmdBuffer, NULL);
            gptGfx->bind_compute_shader(ptComputeEncoder, gptTerrainCtx->tPreProcessHeightShader);
            
            // process new tiles
            
            for(uint32_t i = 0; i < uActiveTileCount; i++)
            {
                plHeightMapTile* ptTile = &gptTerrainCtx->atTiles[gptTerrainCtx->sbuActiveTileIndices[i]];

                if(!(ptTile->tFlags & PL_TERRAIN_TILE_FLAGS_UPLOADED))
                {
                    continue;
                }

                if(ptTile->tFlags & PL_TERRAIN_TILE_FLAGS_PROCESSED_INTERMEDIATE)
                {

                    ptTile->tFlags |= PL_TERRAIN_TILE_FLAGS_PROCESSED;
                    ptTile->tFlags &= ~PL_TERRAIN_TILE_FLAGS_PROCESSED_INTERMEDIATE;

                    plDynamicBinding tDynamicBinding = pl_allocate_dynamic_data(gptGfx, ptDevice, &gptTerrainCtx->tCurrentDynamicDataBlock);
                    plTerrainPrepDynamicData* ptDynamicData = (plTerrainPrepDynamicData*)tDynamicBinding.pcData;
                    memset(ptDynamicData, 0, sizeof(plTerrainPrepDynamicData));
                    ptDynamicData->fMetersPerHeightFieldTexel = gptTerrainCtx->fMetersPerTexel;
                    ptDynamicData->fMaxHeight = ptTile->fMaxHeight;
                    ptDynamicData->fMinHeight = ptTile->fMinHeight;
                    ptDynamicData->fGlobalMaxHeight = gptTerrainCtx->fMaxElevation;
                    ptDynamicData->fGlobalMinHeight = gptTerrainCtx->fMinElevation;
                    ptDynamicData->iXOffset = (int)ptTile->_uXOffsetActual;
                    ptDynamicData->iYOffset = (int)ptTile->_uYOffsetActual;

                    // TODO: handle this
                    ptDynamicData->iNormalCalcReach = 10;

                    gptGfx->bind_compute_bind_groups(ptComputeEncoder, gptTerrainCtx->tPreProcessHeightShader, 0, 1, &gptTerrainCtx->tPreprocessBG0, 1, &tDynamicBinding);

                    plDispatch tDispatch = {
                        .uGroupCountX     = gptTerrainCtx->uTileSize / 8,
                        .uGroupCountY     = gptTerrainCtx->uTileSize / 8,
                        .uGroupCountZ     = 1,
                        .uThreadPerGroupX = 8,
                        .uThreadPerGroupY = 8,
                        .uThreadPerGroupZ = 1,
                    };

                    gptGfx->dispatch(ptComputeEncoder, 1, &tDispatch);

                    // uUpdateLimit--;

                    // if(uUpdateLimit == 0)
                    // {
                    //     bEndOfAmortization = false;
                    //     break;
                    // }
                }
            }
            gptGfx->end_compute_pass(ptComputeEncoder);

            // set proper texture usages
            ptBlit = gptGfx->begin_blit_pass(ptCmdBuffer);
            gptGfx->pipeline_barrier_blit(ptBlit,
                PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER,
                PL_ACCESS_SHADER_WRITE | PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ,
                PL_PIPELINE_STAGE_TRANSFER,
                PL_ACCESS_TRANSFER_WRITE);
            gptGfx->set_texture_usage(ptBlit, gptTerrainCtx->tProcessedTexture, PL_TEXTURE_USAGE_SAMPLED, PL_TEXTURE_USAGE_STORAGE);
            gptGfx->pipeline_barrier_blit(ptBlit,
                PL_PIPELINE_STAGE_TRANSFER,
                PL_ACCESS_TRANSFER_WRITE,
                PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER,
                PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
            gptGfx->end_blit_pass(ptBlit);
        }

        gptAtomics->atomic_store(gptTerrainCtx->ptPrefetchDirty, 0);
    // }

    // generate mip maps


        plTexture* ptProcessedTexture = gptGfx->get_texture(ptDevice, gptTerrainCtx->tProcessedTexture);
        for(uint32_t i = 1; i < ptProcessedTexture->tDesc.uMips; i++)
        {
            plDynamicBinding tDynamicBinding = pl_allocate_dynamic_data(gptGfx, ptDevice, &gptTerrainCtx->tCurrentDynamicDataBlock);
            int* piSourceLevel = (int*)tDynamicBinding.pcData;
            *piSourceLevel = (int)i - 1;

            // process single mip level
            plComputeEncoder* ptComputeEncoder = gptGfx->begin_compute_pass(ptCmdBuffer, NULL);
            gptGfx->bind_compute_shader(ptComputeEncoder, gptTerrainCtx->tMipMapShader);
            gptGfx->bind_compute_bind_groups(ptComputeEncoder, gptTerrainCtx->tMipMapShader, 0, 1, &gptTerrainCtx->tMipmapBG0, 1, &tDynamicBinding);
            plDispatch tDispatch = {
                .uGroupCountX     = (int)gptTerrainCtx->uHeightMapResolution / (8 * (1 << (int)i)),
                .uGroupCountY     = (int)gptTerrainCtx->uHeightMapResolution / (8 * (1 << (int)i)),
                .uGroupCountZ     = 1,
                .uThreadPerGroupX = 8,
                .uThreadPerGroupY = 8,
                .uThreadPerGroupZ = 1,
            };

            if(tDispatch.uGroupCountX > 0 && tDispatch.uGroupCountY > 0)
                gptGfx->dispatch(ptComputeEncoder, 1, &tDispatch);
            gptGfx->end_compute_pass(ptComputeEncoder);

            // copy processed mip level 
            plImageCopy tImageCopy = {
                .uSourceExtentX             = (int)gptTerrainCtx->uHeightMapResolution / (1 << (int)i),
                .uSourceExtentY             = (int)gptTerrainCtx->uHeightMapResolution / (1 << (int)i),
                .uSourceExtentZ             = 1,
                .uSourceMipLevel            = 0,
                .uSourceBaseArrayLayer      = 0,
                .uSourceLayerCount          = 1,
                .tSourceImageUsage          = PL_TEXTURE_USAGE_STORAGE,
                .uDestinationMipLevel       = i,
                .uDestinationBaseArrayLayer = 0,
                .uDestinationLayerCount     = 1,
                .tDestinationImageUsage     = PL_TEXTURE_USAGE_SAMPLED,
            };

            ptBlit = gptGfx->begin_blit_pass(ptCmdBuffer);
            gptGfx->pipeline_barrier_blit(ptBlit, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_READ | PL_ACCESS_SHADER_WRITE, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_READ | PL_ACCESS_TRANSFER_WRITE);
            gptGfx->copy_texture(ptBlit, gptTerrainCtx->tDummyTexture, gptTerrainCtx->tProcessedTexture, 1, &tImageCopy);
            gptGfx->pipeline_barrier_blit(ptBlit, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_READ | PL_ACCESS_TRANSFER_WRITE, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_READ | PL_ACCESS_SHADER_WRITE);
            gptGfx->end_blit_pass(ptBlit);
        }
    }

    // update active texture with new tile data
    // if(gptTerrainCtx->abPendingTextureUpdate[gptGfx->get_current_frame_index()])
    {
        gptGfx->pipeline_barrier(ptCmdBuffer,
            PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE | PL_ACCESS_TRANSFER_READ,
            PL_PIPELINE_STAGE_TRANSFER,  PL_ACCESS_TRANSFER_WRITE | PL_ACCESS_TRANSFER_READ);

        plBlitEncoder* ptBlit = gptGfx->begin_blit_pass(ptCmdBuffer);

        plTexture* ptActiveTexture = gptGfx->get_texture(ptDevice, gptTerrainCtx->atActiveTexture[gptGfx->get_current_frame_index()]);
        for(uint32_t i = 0; i < ptActiveTexture->tDesc.uMips; i++)
        {
            plImageCopy tImageCopy = {
                .uSourceExtentX             = (int)gptTerrainCtx->uHeightMapResolution / (1 << (int)i),
                .uSourceExtentY             = (int)gptTerrainCtx->uHeightMapResolution / (1 << (int)i),
                .uSourceExtentZ             = 1,
                .uSourceMipLevel            = i,
                .uSourceBaseArrayLayer      = 0,
                .uSourceLayerCount          = 1,
                .tSourceImageUsage          = PL_TEXTURE_USAGE_SAMPLED,
                .uDestinationMipLevel       = i,
                .uDestinationBaseArrayLayer = 0,
                .uDestinationLayerCount     = 1,
                .tDestinationImageUsage     = PL_TEXTURE_USAGE_SAMPLED,
            };

            gptGfx->pipeline_barrier_blit(ptBlit, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_READ | PL_ACCESS_SHADER_WRITE, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_READ | PL_ACCESS_TRANSFER_WRITE);
            gptGfx->copy_texture(ptBlit, gptTerrainCtx->tProcessedTexture, gptTerrainCtx->atActiveTexture[gptGfx->get_current_frame_index()], 1, &tImageCopy);
            gptGfx->pipeline_barrier_blit(ptBlit, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_READ | PL_ACCESS_TRANSFER_WRITE, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_READ | PL_ACCESS_SHADER_WRITE);
            
        }
        gptGfx->end_blit_pass(ptBlit);

        gptGfx->pipeline_barrier(ptCmdBuffer,
            PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER,
            PL_ACCESS_SHADER_WRITE | PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_WRITE | PL_ACCESS_TRANSFER_READ,
            PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_FRAGMENT_SHADER,
            PL_ACCESS_SHADER_READ
        );
    }

    gptTerrainCtx->bProcessAll = false;
}

static void
pl__terrain_create_shaders(void)
{
    plShaderDesc tTerrainShaderDesc = {
        .tVertexShader     = gptShader->load_glsl("terrain.vert", "main", NULL, NULL),
        .tPixelShader      = gptShader->load_glsl("terrain.frag", "main", NULL, NULL),
        .tRenderPassLayout = gptTerrainCtx->tRenderPassLayoutHandle,
        .uSubpassIndex = gptTerrainCtx->uSubpassIndex,
        .tMSAASampleCount  = PL_SAMPLE_COUNT_1,
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 1,
            .ulDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
            .ulCullMode           = PL_CULL_MODE_NONE,
            .ulWireframe          = 0,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP,
        },
        .atVertexBufferLayouts = {
            {
                .uByteStride = sizeof(float) * 3,
                .atAttributes = {
                    {.tFormat = PL_VERTEX_FORMAT_FLOAT3 }
                }
            }
        },
        .atBlendStates = {
            {
                .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL,
                .bBlendEnabled = true,
                .tSrcColorFactor = PL_BLEND_FACTOR_SRC_ALPHA,
                .tDstColorFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .tColorOp =        PL_BLEND_OP_ADD,
                .tSrcAlphaFactor = PL_BLEND_FACTOR_SRC_ALPHA,
                .tDstAlphaFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .tAlphaOp =        PL_BLEND_OP_ADD
            }
        },
        .atBindGroupLayouts = {
            {
                .atSamplerBindings = {
                    {.uSlot = 0, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT },
                    {.uSlot = 4, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT },
                },
                .atTextureBindings = {
                    {.uSlot = 1, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED },
                    {.uSlot = 2, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED },
                    {.uSlot = 3, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED },
                    {.uSlot = 5, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED },
                }
            }
        }
    };

    gptTerrainCtx->tRegularShader = gptGfx->create_shader(gptTerrainCtx->ptDevice, &tTerrainShaderDesc);
    tTerrainShaderDesc.tGraphicsState.ulWireframe = 1;
    gptTerrainCtx->tWireframeShader = gptGfx->create_shader(gptTerrainCtx->ptDevice, &tTerrainShaderDesc);

    plShaderDesc tFullTerrainShaderDesc = {
        .tVertexShader     = gptShader->load_glsl("terrain_full.vert", "main", NULL, NULL),
        .tPixelShader      = gptShader->load_glsl("terrain_full.frag", "main", NULL, NULL),
        .tRenderPassLayout = gptTerrainCtx->tRenderPassLayoutHandle,
        .uSubpassIndex = gptTerrainCtx->uSubpassIndex,
        .tMSAASampleCount  = PL_SAMPLE_COUNT_1,
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 1,
            .ulDepthMode          = PL_COMPARE_MODE_GREATER,
            .ulCullMode           = PL_CULL_MODE_NONE,
            .ulWireframe          = 0,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP,
        },
        .atVertexBufferLayouts = {
            {
                .uByteStride = sizeof(float) * 5,
                .atAttributes = {
                    {.tFormat = PL_VERTEX_FORMAT_FLOAT3 },
                    {.tFormat = PL_VERTEX_FORMAT_FLOAT2 },
                }
            }
        },
        .atBlendStates =
            {
                {
                    .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL,
                    .bBlendEnabled = false,
                    .tSrcColorFactor = PL_BLEND_FACTOR_SRC_ALPHA,
                    .tDstColorFactor = PL_BLEND_FACTOR_ONE,
                    .tColorOp        = PL_BLEND_OP_ADD,
                    .tSrcAlphaFactor = PL_BLEND_FACTOR_SRC_ALPHA,
                    .tDstAlphaFactor = PL_BLEND_FACTOR_ONE,
                    .tAlphaOp        = PL_BLEND_OP_ADD
                }

            },
        .atBindGroupLayouts = {
            {
                .atSamplerBindings = {
                    {.uSlot = 0, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT },
                    {.uSlot = 4, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT },
                },
                .atTextureBindings = {
                    {.uSlot = 1, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED },
                    {.uSlot = 2, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED },
                    {.uSlot = 3, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED },
                    {.uSlot = 5, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED },
                }
            }
        }
    };

    gptTerrainCtx->tFullShader = gptGfx->create_shader(gptTerrainCtx->ptDevice, &tFullTerrainShaderDesc);
    tFullTerrainShaderDesc.tGraphicsState.ulWireframe = 1;
    gptTerrainCtx->tFullWireFrameShader = gptGfx->create_shader(gptTerrainCtx->ptDevice, &tFullTerrainShaderDesc);

    const plComputeShaderDesc tPreProcessHeightShaderDesc = {
        .tShader = gptShader->load_glsl("heightfield.comp", "main", NULL, NULL),
        .atBindGroupLayouts = {
            {
                .atTextureBindings = {
                    {.uSlot = 0, .tStages = PL_SHADER_STAGE_COMPUTE, .tType = PL_TEXTURE_BINDING_TYPE_STORAGE},
                    {.uSlot = 1, .tStages = PL_SHADER_STAGE_COMPUTE, .tType = PL_TEXTURE_BINDING_TYPE_STORAGE},
                }
            }
        }
    };
    
    const plComputeShaderDesc tMipMapShaderDesc = {
        .tShader = gptShader->load_glsl("mipmap.comp", "main", NULL, NULL),
        .atBindGroupLayouts = {
            {
                .atSamplerBindings = {
                    {.uSlot = 0, .tStages = PL_SHADER_STAGE_COMPUTE}
                },
                .atTextureBindings = {
                    {.uSlot = 1, .tStages = PL_SHADER_STAGE_COMPUTE, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
                    {.uSlot = 2, .tStages = PL_SHADER_STAGE_COMPUTE, .tType = PL_TEXTURE_BINDING_TYPE_STORAGE},
                }
            }
        }
    };

    gptTerrainCtx->tPreProcessHeightShader = gptGfx->create_compute_shader(gptTerrainCtx->ptDevice, &tPreProcessHeightShaderDesc);
    gptTerrainCtx->tMipMapShader = gptGfx->create_compute_shader(gptTerrainCtx->ptDevice, &tMipMapShaderDesc);
}

static plTextureHandle
pl__terrain_load_texture(plCommandBuffer* ptCmdBuffer, const char* pcFile)
{
    plDevice* ptDevice = gptTerrainCtx->ptDevice;

    size_t szImageFileSize = gptVfs->get_file_size_str(pcFile);
    plVfsFileHandle tSpriteSheet = gptVfs->open_file(pcFile, PL_VFS_FILE_MODE_READ);
    gptVfs->read_file(tSpriteSheet, NULL, &szImageFileSize);
    unsigned char* pucBuffer = (unsigned char*)PL_ALLOC(szImageFileSize);
    gptVfs->read_file(tSpriteSheet, pucBuffer, &szImageFileSize);
    gptVfs->close_file(tSpriteSheet);

    // load actual data from file data
    int iImageWidth = 0;
    int iImageHeight = 0;
    int _unused;
    unsigned char* pucImageData = gptImage->load(pucBuffer, (int)szImageFileSize, &iImageWidth, &iImageHeight, &_unused, 4);
    PL_FREE(pucBuffer);

    // create texture
    plTextureDesc tTextureDesc = {
        .tDimensions = { (float)iImageWidth, (float)iImageHeight, 1},
        .tFormat     = PL_FORMAT_R8G8B8A8_UNORM,
        .uLayers     = 1,
        .uMips       = 0,
        .tType       = PL_TEXTURE_TYPE_2D,
        .tUsage      = PL_TEXTURE_USAGE_SAMPLED,
        .pcDebugName = "noise texture",
    };

    plTextureHandle tTexture = gptGfx->create_texture(ptDevice, &tTextureDesc, NULL);
    
    // retrieve new texture (also could have used out param from create_texture above)
    plTexture* ptTexture = gptGfx->get_texture(ptDevice, tTexture);

    size_t szBuddyBlockSize = gptGpuAllocators->get_buddy_block_size();

    plDeviceMemoryAllocatorI* ptAllocator = gptTerrainCtx->tLocalDedicatedAllocator;

    if(ptTexture->tMemoryRequirements.ulSize >= szBuddyBlockSize)
        ptAllocator = gptTerrainCtx->tLocalBuddyAllocator;

    // allocate memory
    const plDeviceMemoryAllocation tRawTextureAllocation = ptAllocator->allocate(ptAllocator->ptInst,
        ptTexture->tMemoryRequirements.uMemoryTypeBits,
        ptTexture->tMemoryRequirements.ulSize,
        ptTexture->tMemoryRequirements.ulAlignment,
        "noise texture memory");

    // bind memory
    gptGfx->bind_texture_to_memory(ptDevice, tTexture, &tRawTextureAllocation);

    // create staging buffer
    plBufferDesc tStagingBufferDesc = {
        .tUsage      = PL_BUFFER_USAGE_TRANSFER_DESTINATION | PL_BUFFER_USAGE_TRANSFER_SOURCE,
        .szByteSize  = iImageWidth * iImageHeight * 4,
        .pcDebugName = "staging buffer"
    };

    plBuffer* ptStagingBuffer = NULL;
    plBufferHandle tStagingBuffer = gptGfx->create_buffer(ptDevice, &tStagingBufferDesc, &ptStagingBuffer);

    // allocate memory for the vertex buffer
    ptAllocator = gptTerrainCtx->tStagingDedicatedAllocator;
    const plDeviceMemoryAllocation tStagingBufferAllocation = ptAllocator->allocate(ptAllocator->ptInst,
        ptStagingBuffer->tMemoryRequirements.uMemoryTypeBits,
        ptStagingBuffer->tMemoryRequirements.ulSize,
        0,
        "staging buffer memory");

    // bind the buffer to the new memory allocation
    gptGfx->bind_buffer_to_memory(ptDevice, tStagingBuffer, &tStagingBufferAllocation);

    // set the initial texture usage (this is a no-op in metal but does layout transition for vulkan)
    plBlitEncoder* ptBlit = gptGfx->begin_blit_pass(ptCmdBuffer);
    gptGfx->pipeline_barrier_blit(ptBlit, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);
    gptGfx->set_texture_usage(ptBlit, tTexture, PL_TEXTURE_USAGE_SAMPLED, 0);

    // copy memory to mapped staging buffer
    memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, pucImageData, iImageWidth * iImageHeight * 4);
    gptImage->free(pucImageData);

    plBufferImageCopy tBufferImageCopy = {
        .uImageWidth        = (uint32_t)iImageWidth,
        .uImageHeight       = (uint32_t)iImageHeight,
        .uImageDepth        = 1,
        .uLayerCount        = 1,
        .szBufferOffset     = 0,
        .tCurrentImageUsage = PL_TEXTURE_USAGE_SAMPLED,
    };

    gptGfx->copy_buffer_to_texture(ptBlit, tStagingBuffer, tTexture, 1, &tBufferImageCopy);
    gptGfx->generate_mipmaps(ptBlit, tTexture);
    gptGfx->pipeline_barrier_blit(ptBlit, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
    gptGfx->end_blit_pass(ptBlit);
    gptGfx->queue_buffer_for_deletion(ptDevice, tStagingBuffer);
    return tTexture;
}

static plTextureHandle
pl__terrain_create_texture(plCommandBuffer* ptCmdBuffer, const plTextureDesc* ptDesc, const char* pcName, plTextureUsage tInitialUsage)
{
    // for convience
   plDevice* ptDevice = gptTerrainCtx->ptDevice;
 
    // create texture
    plTempAllocator tTempAllocator = {0};
    plTexture* ptTexture = NULL;
    const plTextureHandle tHandle = gptGfx->create_texture(ptDevice, ptDesc, &ptTexture);
    pl_temp_allocator_reset(&tTempAllocator);

    // choose allocator
    plDeviceMemoryAllocatorI* ptAllocator = gptTerrainCtx->tLocalBuddyAllocator;
    if(ptTexture->tMemoryRequirements.ulSize > gptGpuAllocators->get_buddy_block_size())
        ptAllocator = gptTerrainCtx->tLocalDedicatedAllocator;

    // allocate memory
    const plDeviceMemoryAllocation tAllocation = ptAllocator->allocate(ptAllocator->ptInst, 
        ptTexture->tMemoryRequirements.uMemoryTypeBits,
        ptTexture->tMemoryRequirements.ulSize,
        ptTexture->tMemoryRequirements.ulAlignment,
        pl_temp_allocator_sprintf(&tTempAllocator, "texture alloc %s", pcName));

    // bind memory
    gptGfx->bind_texture_to_memory(ptDevice, tHandle, &tAllocation);
    pl_temp_allocator_free(&tTempAllocator);


    // set the initial texture usage (this is a no-op in metal but does layout transition for vulkan)
    plBlitEncoder* ptBlit = gptGfx->begin_blit_pass(ptCmdBuffer);
    gptGfx->set_texture_usage(ptBlit, tHandle, tInitialUsage, 0);
    gptGfx->end_blit_pass(ptBlit);
    return tHandle;
}

static uint32_t
pl__terrain_tile_activation_distance(uint32_t uIndex)
{
    plHeightMapTile* ptTile = &gptTerrainCtx->atTiles[uIndex];

    bool bWithinX = ptTile->_iXCoord >= gptTerrainCtx->iCurrentXCoordMin && ptTile->_iXCoord <= gptTerrainCtx->iCurrentXCoordMax;
    bool bWithinY = ptTile->_iYCoord >= gptTerrainCtx->iCurrentYCoordMin && ptTile->_iYCoord <= gptTerrainCtx->iCurrentYCoordMax;

    if(bWithinX && bWithinY)
        return 0;

    uint32_t uMinDistanceX = 0;
    if(ptTile->_iXCoord > gptTerrainCtx->iCurrentXCoordMax)
        uMinDistanceX = ptTile->_iXCoord - gptTerrainCtx->iCurrentXCoordMax;
    else if(ptTile->_iXCoord < gptTerrainCtx->iCurrentXCoordMin)
        uMinDistanceX = gptTerrainCtx->iCurrentXCoordMin - ptTile->_iXCoord;

    uint32_t uMinDistanceY = 0;
    if(ptTile->_iYCoord > gptTerrainCtx->iCurrentYCoordMax)
        uMinDistanceY = ptTile->_iYCoord - gptTerrainCtx->iCurrentYCoordMax;
    else if(ptTile->_iYCoord < gptTerrainCtx->iCurrentYCoordMin)
        uMinDistanceY = gptTerrainCtx->iCurrentYCoordMin - ptTile->_iYCoord;

    return pl_maxu(uMinDistanceX, uMinDistanceY);
}

static void
pl__terrain_return_free_chunk(uint32_t uOwnerTileIndex)
{
    plHeightMapTile* ptTile = &gptTerrainCtx->atTiles[uOwnerTileIndex];
    uint32_t uChunkIndex = ptTile->_uChunkIndex;
    ptTile->tFlags = PL_TERRAIN_TILE_FLAGS_NONE;

    if(uChunkIndex != UINT32_MAX)
    {
        ptTile->_uChunkIndex = UINT32_MAX;
        gptTerrainCtx->atChunks[uChunkIndex].uOwnerTileIndex = UINT32_MAX;
        pl_sb_push(gptTerrainCtx->sbuFreeChunks, uChunkIndex);
    }
}

static void
pl__terrain_clear_cache_direction(plTerrainDirection tDirection)
{

    uint32_t uXRange = (uint32_t)(gptTerrainCtx->iCurrentXCoordMax - gptTerrainCtx->iCurrentXCoordMin);
    uint32_t uYRange = (uint32_t)(gptTerrainCtx->iCurrentYCoordMax - gptTerrainCtx->iCurrentYCoordMin);

    if(!(tDirection & PL_TERRAIN_DIRECTION_EAST))
    {
        
        for(uint32_t j = 0; j <= uYRange; j++)
        {
            for(uint32_t i = 0; i <= gptTerrainCtx->uPrefetchRadius; i++)
            {
                uint32_t uXIndex = gptTerrainCtx->iCurrentXCoordMax + i;
                uint32_t uYIndex = gptTerrainCtx->iCurrentYCoordMin + j;
                plHeightMapTile* ptTile = pl__terrain_get_tile_by_pos(uXIndex, uYIndex);
                int iTileIndex = pl__terrain_get_tile_index_by_pos(uXIndex, uYIndex);
                if(ptTile)
                {
                    if(!(ptTile->tFlags & PL_TERRAIN_TILE_FLAGS_ACTIVE))
                    {
                        if(!(ptTile->tFlags & PL_TERRAIN_TILE_FLAGS_QUEUED))
                        {
                            pl__terrain_return_free_chunk(iTileIndex);
                        }
                    }
                }
            }
        }
    }

    if(!(tDirection & PL_TERRAIN_DIRECTION_WEST))
    {
        
        for(uint32_t j = 0; j <= uYRange; j++)
        {
            for(uint32_t i = 0; i <= gptTerrainCtx->uPrefetchRadius; i++)
            {
                int uXIndex = gptTerrainCtx->iCurrentXCoordMin - i;
                int uYIndex = gptTerrainCtx->iCurrentYCoordMin + j;
                plHeightMapTile* ptTile = pl__terrain_get_tile_by_pos(uXIndex, uYIndex);
                int iTileIndex = pl__terrain_get_tile_index_by_pos(uXIndex, uYIndex);
                if(ptTile)
                {
                    if(!(ptTile->tFlags & PL_TERRAIN_TILE_FLAGS_ACTIVE))
                    {
                        if(!(ptTile->tFlags & PL_TERRAIN_TILE_FLAGS_QUEUED))
                        {
                            pl__terrain_return_free_chunk(iTileIndex);
                        }
                    }
                }
            }
        }
    }

    if(!(tDirection & PL_TERRAIN_DIRECTION_NORTH))
    {
        
        for(uint32_t j = 0; j <= gptTerrainCtx->uPrefetchRadius; j++)
        {
            for(uint32_t i = 0; i <= uXRange; i++)
            {
                int uXIndex = gptTerrainCtx->iCurrentXCoordMin + i;
                int uYIndex = gptTerrainCtx->iCurrentYCoordMax + j;
                plHeightMapTile* ptTile = pl__terrain_get_tile_by_pos(uXIndex, uYIndex);
                int iTileIndex = pl__terrain_get_tile_index_by_pos(uXIndex, uYIndex);
                if(ptTile)
                {
                    if(!(ptTile->tFlags & PL_TERRAIN_TILE_FLAGS_ACTIVE))
                    {
                        if(!(ptTile->tFlags & PL_TERRAIN_TILE_FLAGS_QUEUED))
                        {
                            pl__terrain_return_free_chunk(iTileIndex);
                        }
                    }
                }
            }
        }
    }

    if(!(tDirection & PL_TERRAIN_DIRECTION_SOUTH))
    {
        
        for(uint32_t j = 0; j <= gptTerrainCtx->uPrefetchRadius; j++)
        {
            for(uint32_t i = 0; i <= uXRange; i++)
            {
                int uXIndex = gptTerrainCtx->iCurrentXCoordMin + i;
                int uYIndex = gptTerrainCtx->iCurrentYCoordMin - j;
                plHeightMapTile* ptTile = pl__terrain_get_tile_by_pos(uXIndex, uYIndex);
                int iTileIndex = pl__terrain_get_tile_index_by_pos(uXIndex, uYIndex);
                if(ptTile)
                {
                    if(!(ptTile->tFlags & PL_TERRAIN_TILE_FLAGS_ACTIVE))
                    {
                        if(!(ptTile->tFlags & PL_TERRAIN_TILE_FLAGS_QUEUED))
                        {
                            pl__terrain_return_free_chunk(iTileIndex);
                        }
                    }
                }
            }
        }
    }

}

static void
pl__terrain_clear_cache(uint32_t uCount, uint32_t uRadius)
{
    for(uint32_t i = 0; i < gptTerrainCtx->uChunkCapacity; i++)
    {
        uint32_t uTileIndex = gptTerrainCtx->atChunks[i].uOwnerTileIndex;
        if(uTileIndex != UINT32_MAX)
        {
            if(!(gptTerrainCtx->atTiles[uTileIndex].tFlags & PL_TERRAIN_TILE_FLAGS_ACTIVE))
            {
                if(!(gptTerrainCtx->atTiles[uTileIndex].tFlags & PL_TERRAIN_TILE_FLAGS_QUEUED))
                {
                    uint32_t uActivationDistance = pl__terrain_tile_activation_distance(uTileIndex) + 1;
                    if(uActivationDistance >= uRadius)
                    {
                        pl__terrain_return_free_chunk(uTileIndex);
                        uCount--;
                        if(uCount == 0)
                            return;
                    }
                }
            }
        }
    }
}

static void
pl__terrain_get_free_chunk(uint32_t uOwnerTileIndex)
{
    // check if already assigned chunk
    if(gptTerrainCtx->atTiles[uOwnerTileIndex]._uChunkIndex != UINT32_MAX)
        return;

    if(pl_sb_size(gptTerrainCtx->sbuFreeChunks) > 0)
    {
        uint32_t uChunkIndex = pl_sb_pop(gptTerrainCtx->sbuFreeChunks);
        gptTerrainCtx->atChunks[uChunkIndex].uOwnerTileIndex = uOwnerTileIndex;
        gptTerrainCtx->atTiles[uOwnerTileIndex]._uChunkIndex = uChunkIndex;
        return;
    }

    pl__terrain_clear_cache(UINT32_MAX, gptTerrainCtx->uPrefetchRadius);

    if(pl_sb_size(gptTerrainCtx->sbuFreeChunks) > 0)
    {
        uint32_t uChunkIndex = pl_sb_pop(gptTerrainCtx->sbuFreeChunks);
        gptTerrainCtx->atChunks[uChunkIndex].uOwnerTileIndex = uOwnerTileIndex;
        gptTerrainCtx->atTiles[uOwnerTileIndex]._uChunkIndex = uChunkIndex;
        return;
    }
    
    gptTerrainCtx->atTiles[uOwnerTileIndex]._uChunkIndex = UINT32_MAX;
}

static bool
pl__terrain_process_height_map_tiles(plCamera* ptCamera)
{
    
    plDevice* ptDevice = gptTerrainCtx->ptDevice;
    
    const uint32_t uTilesAcross = gptTerrainCtx->uHeightMapResolution / gptTerrainCtx->uTileSize;
    bool bTextureNeedUpdate = false;

    float fRadius = (float)(gptTerrainCtx->uHeightMapResolution / 2) * gptTerrainCtx->fMetersPerTexel;
    const int iRadiusInTiles = (int)((gptTerrainCtx->uHeightMapResolution / 2) / gptTerrainCtx->uTileSize);
    plVec2 tCameraPos = {ptCamera->tPos.x, ptCamera->tPos.z};
    float fIncrement = (float)gptTerrainCtx->uTileSize * gptTerrainCtx->fMetersPerTexel;
    tCameraPos.x = floorf(tCameraPos.x * (1.0f / fIncrement)) * fIncrement;
    tCameraPos.y = floorf(tCameraPos.y * (1.0f / fIncrement)) * fIncrement;

    int iCameraXTile = (int)(((float)tCameraPos.x - gptTerrainCtx->tMinWorldPosition.x) / fIncrement);
    int iCameraYTile = (int)(((float)tCameraPos.y - gptTerrainCtx->tMinWorldPosition.y) / fIncrement);
    
    gptTerrainCtx->iCurrentXCoordMin = iCameraXTile - iRadiusInTiles;
    gptTerrainCtx->iCurrentXCoordMax = iCameraXTile + iRadiusInTiles - 1;
    gptTerrainCtx->iCurrentYCoordMin = iCameraYTile - iRadiusInTiles;
    gptTerrainCtx->iCurrentYCoordMax = iCameraYTile + iRadiusInTiles - 1;

    if(pl_sb_size(gptTerrainCtx->sbuPrefetchOverflow) > 0)
        return false;

    gptTerrainCtx->bNeedsUpdate = false;

    // unmark old active tiles
    const uint32_t uActiveTileCount = pl_sb_size(gptTerrainCtx->sbuActiveTileIndices);
    for(uint32_t i = 0; i < uActiveTileCount; i++)
    {
        plHeightMapTile* ptTile = &gptTerrainCtx->atTiles[gptTerrainCtx->sbuActiveTileIndices[i]];
        if(ptTile->_iXCoord < gptTerrainCtx->iCurrentXCoordMin || ptTile->_iXCoord > gptTerrainCtx->iCurrentXCoordMax || ptTile->_iYCoord < gptTerrainCtx->iCurrentYCoordMin || ptTile->_iYCoord > gptTerrainCtx->iCurrentYCoordMax)
        {
            ptTile->tFlags &= ~PL_TERRAIN_TILE_FLAGS_ACTIVE;
            ptTile->tFlags &= ~PL_TERRAIN_TILE_FLAGS_PROCESSED;
            ptTile->tFlags &= ~PL_TERRAIN_TILE_FLAGS_PROCESSED_INTERMEDIATE;
        }  
    }
    pl_sb_reset(gptTerrainCtx->sbuActiveTileIndices);

    // mark new tiles & pack into atlas
    for(uint32_t j = 0; j <= (uint32_t)(gptTerrainCtx->iCurrentYCoordMax - gptTerrainCtx->iCurrentYCoordMin); j++)
    {
        uint32_t uCurrentYOffset = (gptTerrainCtx->uCurrentYOffset + j) % uTilesAcross;

        for(uint32_t i = 0; i <= (uint32_t)(gptTerrainCtx->iCurrentXCoordMax - gptTerrainCtx->iCurrentXCoordMin); i++)
        {
            uint32_t uXIndex = gptTerrainCtx->iCurrentXCoordMin + i;
            uint32_t uYIndex = gptTerrainCtx->iCurrentYCoordMin + j;
            plHeightMapTile* ptTile = pl__terrain_get_tile_by_pos(uXIndex, uYIndex);
            
            if(ptTile)
            {
                int iTileIndex = pl__terrain_get_tile_index_by_pos(uXIndex, uYIndex);
                uint32_t uCurrentXOffset = (gptTerrainCtx->uCurrentXOffset + i) % uTilesAcross;

                pl__push_prefetch_tile(iTileIndex);

                // if not processed, pack into atlas
                if(!(ptTile->tFlags & PL_TERRAIN_TILE_FLAGS_PROCESSED))
                {
                    ptTile->_uYOffsetActual = uCurrentYOffset;
                    ptTile->_uYOffsetActual *= gptTerrainCtx->uTileSize;
                    ptTile->_uXOffsetActual = uCurrentXOffset;
                    ptTile->_uXOffsetActual *= gptTerrainCtx->uTileSize;
                }

                bTextureNeedUpdate = true;
                if(!(ptTile->tFlags & PL_TERRAIN_TILE_FLAGS_ACTIVE))
                {
                    plHeightMapTile* ptNeighborTile = NULL;
                    if(gptTerrainCtx->tCurrentDirectionUpdate & PL_TERRAIN_DIRECTION_EAST)
                    {
                        ptNeighborTile = pl__terrain_get_tile_by_pos(ptTile->_iXCoord + 1, ptTile->_iYCoord);
                        if(ptNeighborTile)
                        {
                            ptNeighborTile->tFlags &= ~PL_TERRAIN_TILE_FLAGS_PROCESSED;
                            ptNeighborTile->tFlags &= ~PL_TERRAIN_TILE_FLAGS_PROCESSED_INTERMEDIATE;
                        }
                    }
                    if(gptTerrainCtx->tCurrentDirectionUpdate & PL_TERRAIN_DIRECTION_WEST)
                    {
                        ptNeighborTile = pl__terrain_get_tile_by_pos(ptTile->_iXCoord - 1, ptTile->_iYCoord);
                        if(ptNeighborTile)
                        {
                            ptNeighborTile->tFlags &= ~PL_TERRAIN_TILE_FLAGS_PROCESSED;
                            ptNeighborTile->tFlags &= ~PL_TERRAIN_TILE_FLAGS_PROCESSED_INTERMEDIATE;
                        }
                    }

                    if(gptTerrainCtx->tCurrentDirectionUpdate & PL_TERRAIN_DIRECTION_SOUTH)
                    {
                        ptNeighborTile = pl__terrain_get_tile_by_pos(ptTile->_iXCoord, ptTile->_iYCoord + 1);
                        if(ptNeighborTile)
                        {
                            ptNeighborTile->tFlags &= ~PL_TERRAIN_TILE_FLAGS_PROCESSED;
                            ptNeighborTile->tFlags &= ~PL_TERRAIN_TILE_FLAGS_PROCESSED_INTERMEDIATE;
                        }
                    }

                    if(gptTerrainCtx->tCurrentDirectionUpdate & PL_TERRAIN_DIRECTION_NORTH)
                    {
                        ptNeighborTile = pl__terrain_get_tile_by_pos(ptTile->_iXCoord, ptTile->_iYCoord - 1);
                        if(ptNeighborTile)
                        {
                            ptNeighborTile->tFlags &= ~PL_TERRAIN_TILE_FLAGS_PROCESSED;
                            ptNeighborTile->tFlags &= ~PL_TERRAIN_TILE_FLAGS_PROCESSED_INTERMEDIATE;
                        }
                    }
                    ptTile->tFlags |= PL_TERRAIN_TILE_FLAGS_ACTIVE;
                }
                pl_sb_push(gptTerrainCtx->sbuActiveTileIndices, iTileIndex);
            }
        }
    }

    // if streaming is active & pretching is complete, begin prefetch
    if(gptTerrainCtx->tFlags & PL_TERRAIN_FLAGS_TILE_STREAMING)
    {

        pl__terrain_clear_cache_direction(gptTerrainCtx->tCurrentDirectionUpdate);

        // gptTerrainCtx->uNewPrefetchTileCount = 0;
        int iLeftX = gptTerrainCtx->iCurrentXCoordMin - (int)gptTerrainCtx->uPrefetchRadius;
        int iTopY = gptTerrainCtx->iCurrentYCoordMin - (int)gptTerrainCtx->uPrefetchRadius;
        int iFullXLength = gptTerrainCtx->iCurrentXCoordMax - gptTerrainCtx->iCurrentXCoordMin + 2 * (int)gptTerrainCtx->uPrefetchRadius;
        int iFullYLength = gptTerrainCtx->iCurrentYCoordMax - gptTerrainCtx->iCurrentYCoordMin + 2 * (int)gptTerrainCtx->uPrefetchRadius;
        int iBottomY = iTopY + iFullYLength;
        int iRightX = iLeftX + iFullXLength;

        if(gptTerrainCtx->tCurrentDirectionUpdate & PL_TERRAIN_DIRECTION_EAST)
        {
            for(int y = iTopY; y <= iBottomY; y++)
            {
                // left
                for(int x = iLeftX; x < iLeftX + (int)gptTerrainCtx->uPrefetchRadius; x++)
                {
                    plHeightMapTile* ptTile = pl__terrain_get_tile_by_pos(x, y);
                    if(ptTile)
                    {

                        int iIndex = pl__terrain_get_tile_index_by_pos(x, y); 
                        pl__push_prefetch_tile(iIndex);

                    }
                }
            }
        }


        if(gptTerrainCtx->tCurrentDirectionUpdate & PL_TERRAIN_DIRECTION_WEST)
        {
            for(int y = iTopY; y <= iBottomY; y++)
            {
                // right
                for(int x = iRightX - (int)gptTerrainCtx->uPrefetchRadius + 1; x <= iRightX; x++)
                {
                    plHeightMapTile* ptTile = pl__terrain_get_tile_by_pos(x, y);
                    if(ptTile)
                    {

                        int iIndex = pl__terrain_get_tile_index_by_pos(x, y); 
                        pl__push_prefetch_tile(iIndex);

                    }
                }
            }
        }

        if(gptTerrainCtx->tCurrentDirectionUpdate & PL_TERRAIN_DIRECTION_SOUTH)
        {
            for(int x = iLeftX; x <= iLeftX + iFullXLength; x++)
            {
                // top
                for(int y = iTopY; y <= iTopY + (int)gptTerrainCtx->uPrefetchRadius; y++)
                {
                    plHeightMapTile* ptTile = pl__terrain_get_tile_by_pos(x, y);
                    if(ptTile)
                    {

                        int iIndex = pl__terrain_get_tile_index_by_pos(x, y); 
                        pl__push_prefetch_tile(iIndex);

                    }
                }
            }
        }

        if(gptTerrainCtx->tCurrentDirectionUpdate & PL_TERRAIN_DIRECTION_NORTH)
        {
            for(int x = iLeftX; x <= iLeftX + iFullXLength; x++)
            {
                // bottom
                for(int y = iBottomY - (int)gptTerrainCtx->uPrefetchRadius + 1; y <= iBottomY; y++)
                {
                    plHeightMapTile* ptTile = pl__terrain_get_tile_by_pos(x, y);
                    if(ptTile)
                    {

                        int iIndex = pl__terrain_get_tile_index_by_pos(x, y); 
                        pl__push_prefetch_tile(iIndex);

                    }
                }
            }
        }
    }

    gptTerrainCtx->tCurrentDirectionUpdate = PL_TERRAIN_DIRECTION_NONE;

    return bTextureNeedUpdate;
}

void
pl_terrain_reload_shaders(void)
{
    plDevice* ptDevice = gptTerrainCtx->ptDevice;
    gptGfx->queue_shader_for_deletion(ptDevice, gptTerrainCtx->tRegularShader);
    gptGfx->queue_shader_for_deletion(ptDevice, gptTerrainCtx->tWireframeShader);  
    gptGfx->queue_compute_shader_for_deletion(ptDevice, gptTerrainCtx->tPreProcessHeightShader);  
    gptGfx->queue_compute_shader_for_deletion(ptDevice, gptTerrainCtx->tMipMapShader);
    pl__terrain_create_shaders();
}

void
pl_terrain_set_flags(plTerrainFlags tFlags)
{
    gptTerrainCtx->tFlags = tFlags;
}

plTerrainFlags
pl_terrain_get_flags(void)
{
    return gptTerrainCtx->tFlags;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_terrain_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plTerrainI tApi = {
        .cleanup                  = pl_terrain_cleanup,
        .load_mesh                = pl_terrain_load_mesh,
        .prepare                  = pl_terrain_prepare,
        .render                   = pl_terrain_render,
        .tile_height_map          = pl_terrain_tile_height_map,
        .initialize               = pl_initialize_terrain,
        .finalize                 = pl_finalize_terrain,
        .reload_shaders           = pl_terrain_reload_shaders,
        .set_flags                = pl_terrain_set_flags,
        .get_flags                = pl_terrain_get_flags,
    };
    pl_set_api(ptApiRegistry, plTerrainI, &tApi);

    #ifndef PL_UNITY_BUILD
        gptMemory        = pl_get_api_latest(ptApiRegistry, plMemoryI);
        gptGfx           = pl_get_api_latest(ptApiRegistry, plGraphicsI);
        gptVfs           = pl_get_api_latest(ptApiRegistry, plVfsI);
        gptMeshBuilder   = pl_get_api_latest(ptApiRegistry, plMeshBuilderI);
        gptShader        = pl_get_api_latest(ptApiRegistry, plShaderI);
        gptImage         = pl_get_api_latest(ptApiRegistry, plImageI);
        gptGpuAllocators = pl_get_api_latest(ptApiRegistry, plGPUAllocatorsI);
        gptFile          = pl_get_api_latest(ptApiRegistry, plFileI);
        gptAtomics       = pl_get_api_latest(ptApiRegistry, plAtomicsI);
        gptCamera        = pl_get_api_latest(ptApiRegistry, plCameraI);
        gptDraw          = pl_get_api_latest(ptApiRegistry, plDrawI);
        gptUI            = pl_get_api_latest(ptApiRegistry, plUiI);
        gptIOI           = pl_get_api_latest(ptApiRegistry, plIOI);
        gptThreads       = pl_get_api_latest(ptApiRegistry, plThreadsI);
        gptJob           = pl_get_api_latest(ptApiRegistry, plJobI);
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