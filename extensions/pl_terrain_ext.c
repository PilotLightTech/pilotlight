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

// stable extensions
#include "pl_platform_ext.h"
#include "pl_image_ext.h"
#include "pl_profile_ext.h"
#include "pl_graphics_ext.h"
#include "pl_starter_ext.h"
#include "pl_shader_ext.h"
#include "pl_screen_log_ext.h"
#include "pl_draw_ext.h"

// unstable extensions
#include "pl_collision_ext.h"
#include "pl_freelist_ext.h"
#include "pl_camera_ext.h"
#include "pl_terrain_processor_ext.h"

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

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plTerrainResidencyNode
{
    plTerrainResidencyNode* ptNext;
    plTerrainResidencyNode* ptPrev;
    plTerrainChunk*         ptChunk;
    uint64_t                uFrameRequested;
    float                   fDistance;
} plTerrainResidencyNode;

typedef struct _plOBB2
{
    plVec3 tCenter;
    plVec3 tExtents;
    plVec3 atAxes[3]; // Orthonormal basis
} plOBB2;

typedef struct _plTerrain
{
    plTerrainFlags      tFlags;
    plTerrainChunkFile* sbtChunkFiles;
    float               fTau;

    // shaders
    plShaderHandle tShader;
    plShaderHandle tWireframeShader;

    plBufferHandle tStagingBuffer;
    uint32_t       uStagingBufferSize;

    plBufferHandle tIndexBuffer;
    plFreeList tIndexBufferManager;
    
    plBufferHandle tVertexBuffer;
    plFreeList tVertexBufferManager;
    
    plTerrainResidencyNode tRequestQueue;
    plTerrainResidencyNode atRequests[PL_REQUEST_QUEUE_SIZE];
    uint32_t*              sbuFreeRequests;

    plTerrainChunk tReplacementQueue;
} plTerrain;

typedef struct _plTerrainContext
{
    plDevice* ptDevice;
    uint32_t  uVisibleChunks;
    uint32_t  uVisibleRootChunks;

    plTerrainFlags      tFlags;
    plTerrainChunkFile* sbtChunkFiles;
    float               fTau;

    // shaders
    plShaderHandle tShader;
    plShaderHandle tWireframeShader;

    plBufferHandle tStagingBuffer;
    uint32_t       uStagingBufferSize;

    plBufferHandle tIndexBuffer;
    plFreeList tIndexBufferManager;
    
    plBufferHandle tVertexBuffer;
    plFreeList tVertexBufferManager;
    
    plTerrainResidencyNode tRequestQueue;
    plTerrainResidencyNode atRequests[PL_REQUEST_QUEUE_SIZE];
    uint32_t*              sbuFreeRequests;

    plTerrainChunk tReplacementQueue;
} plTerrainContext;

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
    static const plFileI*             gptFile             = NULL;
    static const plProfileI*          gptProfile          = NULL;
    static const plGraphicsI*         gptGfx              = NULL;
    static const plFreeListI*         gptFreeList         = NULL;
    static const plIOI*               gptIOI              = NULL;
    static const plShaderI*           gptShader           = NULL;
    static const plStarterI*          gptStarter          = NULL;
    static const plCollisionI*        gptCollision        = NULL;
    static const plScreenLogI*        gptScreenLog        = NULL;
    static const plDrawI*             gptDraw             = NULL;
    static const plTerrainProcessorI* gptTerrainProcessor = NULL;

#endif

#include "pl_ds.h"

// context
static plTerrainContext* gptTerrainCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal helpers (rendering)
//-----------------------------------------------------------------------------

void pl_terrain_load_shaders(void);

// rendering
static void pl__handle_residency (void);
static void pl__request_residency(plTerrainChunk*, float);
static void pl__touch_chunk(plTerrainChunk*);
static void pl__make_unresident(plTerrainChunk*);

static void pl__render_chunk(plCamera*, plDynamicDataBlock*, plRenderEncoder*, plTerrainChunk*, plTerrainChunkFile*);
static bool pl__sat_visibility_test(plCamera*, const plAABB*);

void pl__draw_residency(plDrawLayer2D* ptLayer, plTerrainChunk* ptChunk, plVec2 tOrigin, float fRadius, plTerrainChunkFile* ptFile);
static void pl__free_chunk(float, uint64_t);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_initialize_cdlod(plTerrainInit tInit)
{
    gptTerrainCtx->ptDevice = tInit.ptDevice;

    gptTerrainCtx->fTau = 1.0f;
    gptTerrainCtx->uStagingBufferSize = 268435456;

    pl_sb_resize(gptTerrainCtx->sbuFreeRequests, PL_REQUEST_QUEUE_SIZE);

    for(uint32_t i = 0; i < PL_REQUEST_QUEUE_SIZE; i++) 
    {
        gptTerrainCtx->sbuFreeRequests[i] = i;
    }

    gptFreeList->create(268435456u, 256, &gptTerrainCtx->tVertexBufferManager);
    gptFreeList->create(268435456u, 256, &gptTerrainCtx->tIndexBufferManager);

}

void
pl_cleanup_cdlod(void)
{
    plDevice* ptDevice = gptTerrainCtx->ptDevice;

    for(uint32_t i = 0; i < pl_sb_size(gptTerrainCtx->sbtChunkFiles); i++)
    {
        PL_FREE(gptTerrainCtx->sbtChunkFiles[i].atChunks);
        gptTerrainCtx->sbtChunkFiles[i].atChunks = NULL;
        gptTerrainCtx->sbtChunkFiles[i].uChunkCount = 0;
        gptTerrainCtx->sbtChunkFiles[i].fMaxBaseError = 0.0f;
        gptTerrainCtx->sbtChunkFiles[i].iTreeDepth = 0;
    }

    // cleanup our resources
    gptGfx->destroy_buffer(ptDevice, gptTerrainCtx->tVertexBuffer);
    gptGfx->destroy_buffer(ptDevice, gptTerrainCtx->tIndexBuffer);
    gptGfx->destroy_buffer(ptDevice, gptTerrainCtx->tStagingBuffer);

    gptFreeList->cleanup(&gptTerrainCtx->tVertexBufferManager);
    gptFreeList->cleanup(&gptTerrainCtx->tIndexBufferManager);

    pl_sb_free(gptTerrainCtx->sbuFreeRequests);
    pl_sb_free(gptTerrainCtx->sbtChunkFiles);
}

void
pl_finalize_terrain(void)
{
    plDevice* ptDevice = gptTerrainCtx->ptDevice;

    const plBufferDesc tVertexBufferDesc = {
        .tUsage      = PL_BUFFER_USAGE_VERTEX | PL_BUFFER_USAGE_TRANSFER_DESTINATION,
        .szByteSize  = gptTerrainCtx->tVertexBufferManager.uSize,
        .pcDebugName = "vertex buffer"
    };
    gptTerrainCtx->tVertexBuffer = gptGfx->create_buffer(ptDevice, &tVertexBufferDesc, NULL);

    // retrieve buffer to get memory allocation requirements (do not store buffer pointer)
    plBuffer* ptVertexBuffer = gptGfx->get_buffer(ptDevice, gptTerrainCtx->tVertexBuffer);

    // allocate memory for the vertex buffer
    const plDeviceMemoryAllocation tVertexBufferAllocation = gptGfx->allocate_memory(ptDevice,
        ptVertexBuffer->tMemoryRequirements.ulSize,
        PL_MEMORY_FLAGS_DEVICE_LOCAL,
        ptVertexBuffer->tMemoryRequirements.uMemoryTypeBits,
        "vertex buffer memory");

    // bind the buffer to the new memory allocation
    gptGfx->bind_buffer_to_memory(ptDevice, gptTerrainCtx->tVertexBuffer, &tVertexBufferAllocation);

    // create index buffer
    const plBufferDesc tIndexBufferDesc = {
        .tUsage      = PL_BUFFER_USAGE_INDEX | PL_BUFFER_USAGE_TRANSFER_DESTINATION,
        .szByteSize  = gptTerrainCtx->tIndexBufferManager.uSize,
        .pcDebugName = "index buffer"
    };
    gptTerrainCtx->tIndexBuffer = gptGfx->create_buffer(ptDevice, &tIndexBufferDesc, NULL);

    // retrieve buffer to get memory allocation requirements (do not store buffer pointer)
    plBuffer* ptIndexBuffer = gptGfx->get_buffer(ptDevice, gptTerrainCtx->tIndexBuffer);

    // allocate memory for the index buffer
    const plDeviceMemoryAllocation tIndexBufferAllocation = gptGfx->allocate_memory(ptDevice,
        ptIndexBuffer->tMemoryRequirements.ulSize,
        PL_MEMORY_FLAGS_DEVICE_LOCAL,
        ptIndexBuffer->tMemoryRequirements.uMemoryTypeBits,
        "index buffer memory");

    // bind the buffer to the new memory allocation
    gptGfx->bind_buffer_to_memory(ptDevice, gptTerrainCtx->tIndexBuffer, &tIndexBufferAllocation);

    // create vertex buffer
    const plBufferDesc tStagingBufferDesc = {
        .tUsage      = PL_BUFFER_USAGE_TRANSFER_SOURCE,
        .szByteSize  = gptTerrainCtx->uStagingBufferSize,
        .pcDebugName = "cdlod staging buffer"
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

    for(uint32_t i = 0; i < pl_sb_size(gptTerrainCtx->sbtChunkFiles); i++)
        pl__request_residency(&gptTerrainCtx->sbtChunkFiles[i].atChunks[0], 0.0f);
    for(uint32_t i = 0; i < pl_sb_size(gptTerrainCtx->sbtChunkFiles); i++)
        pl__handle_residency();

    pl_terrain_load_shaders();
}

bool
pl_chlod_load_chunk_file(const char* pcPath)
{
    plTerrainChunkFile tFile = {0};
    gptTerrainProcessor->load_chunk_file(pcPath, &tFile, pl_sb_size(gptTerrainCtx->sbtChunkFiles));
    pl_sb_push(gptTerrainCtx->sbtChunkFiles, tFile);

    // uint32_t uMinIndexCount = UINT32_MAX;
    // uint32_t uMinVertexCount = UINT32_MAX;
    // uint32_t uMaxIndexCount = 0;
    // uint32_t uMaxVertexCount = 0;
    // uint32_t uMaxVertexChunk = 0;
    // uint32_t uMaxIndexChunk = 0;
    // for(uint32_t i = 0; i < tFile.uChunkCount; i++)
    // {

    //     FILE* ptDataFile = fopen(tFile.acFile, "rb");
    //     fseek(ptDataFile, (long)tFile.atChunks[i].szFileLocation, SEEK_SET);

    //     fseek(ptDataFile, sizeof(plVec3) * 2 + sizeof(int) + sizeof(int), SEEK_CUR);

    //     uint32_t uVertexCount = 0;
    //     fread(&uVertexCount, 1, sizeof(uint32_t), ptDataFile);
    //     fseek(ptDataFile, sizeof(plTerrainVertex) * uVertexCount, SEEK_CUR);

    //     uint32_t uIndexCount = 0;
    //     fread(&uIndexCount, 1, sizeof(uint32_t), ptDataFile);
        
    //     fclose(ptDataFile);

    //     if(uIndexCount > uMaxIndexCount)
    //     {
    //         uMaxIndexCount = uIndexCount;
    //         uMaxIndexChunk = i;
    //     }

    //     if(uIndexCount < uMinIndexCount)
    //         uMinIndexCount = uIndexCount;

    //     if(uVertexCount > uMaxVertexCount)
    //     {
    //         uMaxVertexCount = uVertexCount;
    //         uMaxVertexChunk = i;
    //     }

    //     if(uVertexCount < uMinVertexCount)
    //         uMinVertexCount = uVertexCount;
    // }
    // printf("Max Vertex Count: %u\n", uMaxVertexCount);
    // printf("Max Inex Count:   %u\n", uMaxIndexCount);
    // printf("Min Vertex Count: %u\n", uMaxVertexCount);
    // printf("Min Inex Count:   %u\n", uMaxIndexCount);
    return true;
}

void
pl_terrain_load_shaders(void)
{
    if(gptGfx->is_shader_valid(gptTerrainCtx->ptDevice, gptTerrainCtx->tShader))
    {
        gptGfx->queue_shader_for_deletion(gptTerrainCtx->ptDevice, gptTerrainCtx->tShader);
        gptGfx->queue_shader_for_deletion(gptTerrainCtx->ptDevice, gptTerrainCtx->tWireframeShader);
    }

    plShaderDesc tShaderDesc = {
        .tVertexShader    = gptShader->load_glsl("terrain.vert", "main", NULL, NULL),
        .tFragmentShader  = gptShader->load_glsl("terrain.frag", "main", NULL, NULL),
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
                    {.tFormat = PL_VERTEX_FORMAT_FLOAT2}
                }
            }
        },
        .atBlendStates = {
            {
                .bBlendEnabled   = false,
                .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL
            }
        },
        .tRenderPassLayout = gptStarter->get_render_pass_layout(),
    };
    plDevice* ptDevice = gptTerrainCtx->ptDevice;
    gptTerrainCtx->tShader = gptGfx->create_shader(ptDevice, &tShaderDesc);

    tShaderDesc.tGraphicsState.ulWireframe = 1;
    tShaderDesc.tGraphicsState.ulDepthWriteEnabled = 0;
    tShaderDesc.tGraphicsState.ulDepthMode = PL_COMPARE_MODE_ALWAYS;
    gptTerrainCtx->tWireframeShader = gptGfx->create_shader(ptDevice, &tShaderDesc);
}

void
pl_prepare_terrain(void)
{
    while(gptTerrainCtx->tRequestQueue.ptNext)
        pl__handle_residency();
}

void
pl_render_terrain(plRenderEncoder* ptEncoder, plCamera* ptCamera, plDynamicDataBlock* ptDynamicDataBlock, float fTau)
{

    gptTerrainCtx->uVisibleRootChunks = 0;
    gptTerrainCtx->uVisibleChunks = 0;
    // pl__free_unused_chunks(gptTerrainCtx);

    gptScreenLog->add_message_ex(193, 10.0, PL_COLOR_32_GREEN, 1.0f, "Index Buffer Usage:  %0.2f %%", 100.0f * (float)gptTerrainCtx->tIndexBufferManager.uUsedSpace / (float)gptTerrainCtx->tIndexBufferManager.uSize);
    gptScreenLog->add_message_ex(194, 10.0, PL_COLOR_32_GREEN, 1.0f, "Vertex Buffer Usage:  %0.2f %%", 100.0f * (float)gptTerrainCtx->tVertexBufferManager.uUsedSpace / (float)gptTerrainCtx->tVertexBufferManager.uSize);

    gptTerrainCtx->fTau = fTau;
    for(uint32_t i = 0; i < pl_sb_size(gptTerrainCtx->sbtChunkFiles); i++)
        pl__render_chunk(ptCamera, ptDynamicDataBlock, ptEncoder, &gptTerrainCtx->sbtChunkFiles[i].atChunks[0], &gptTerrainCtx->sbtChunkFiles[i]);

    gptScreenLog->add_message_ex(195, 10.0, PL_COLOR_32_GREEN, 1.0f, "Visible Root Chunks:  %u", gptTerrainCtx->uVisibleRootChunks);
    gptScreenLog->add_message_ex(196, 10.0, PL_COLOR_32_GREEN, 1.0f, "Visible Chunks:  %u", gptTerrainCtx->uVisibleChunks);

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

void
pl_terrain_draw_residency(plDrawLayer2D* ptLayer, plVec2 tOrigin, float fRadius)
{
    for(uint32_t i = 0; i < pl_sb_size(gptTerrainCtx->sbtChunkFiles); i++)
        pl__draw_residency(ptLayer, &gptTerrainCtx->sbtChunkFiles[i].atChunks[0], tOrigin, fRadius, &gptTerrainCtx->sbtChunkFiles[i]);
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static void
pl__unload_children(plTerrainChunk* ptChunk)
{
    if(ptChunk->aptChildren[0] == NULL)
        return;
    if(ptChunk->aptChildren[0]->ptIndexHole) pl__make_unresident(ptChunk->aptChildren[0]);
    if(ptChunk->aptChildren[1]->ptIndexHole) pl__make_unresident(ptChunk->aptChildren[1]);
    if(ptChunk->aptChildren[2]->ptIndexHole) pl__make_unresident(ptChunk->aptChildren[2]);
    if(ptChunk->aptChildren[3]->ptIndexHole) pl__make_unresident(ptChunk->aptChildren[3]);    
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

void
pl__remove_from_replacement_queue(plTerrainChunk* ptChunk)
{
    plTerrainChunk* ptCurrentRequest = gptTerrainCtx->tReplacementQueue.ptNext;

    plTerrainChunk* ptExistingRequest = NULL;
    plTerrainChunk* ptLastRequest = NULL;

    while(ptCurrentRequest)
    {
        if(ptCurrentRequest == ptChunk)
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
            gptTerrainCtx->tReplacementQueue.ptNext = ptExistingRequest->ptNext;

        if(ptExistingRequest->ptNext)
            ptExistingRequest->ptNext->ptPrev = ptExistingRequest->ptPrev;
    }
}

static void
pl__free_chunk(float fDistance, uint64_t uIndexCount)
{
    const uint64_t uCurrentFrame = gptIOI->get_io()->ulFrameCount;

    // find last used item
    plTerrainChunk* ptCurrentRequest = gptTerrainCtx->tReplacementQueue.ptNext;

    if(ptCurrentRequest == NULL)
        return;

    while(ptCurrentRequest->ptNext)
        ptCurrentRequest = ptCurrentRequest->ptNext;

    plTerrainChunk* ptEndRequest = ptCurrentRequest;

    bool bChunkFreed = false;
    while(ptCurrentRequest->ptPrev)
    {
        if(ptCurrentRequest->uIndexCount > uIndexCount)
        {
            pl__make_unresident(ptCurrentRequest);
            bChunkFreed = true;
            break;
        }
        ptCurrentRequest = ptCurrentRequest->ptPrev;
    }

    ptCurrentRequest = ptEndRequest;
    if(!bChunkFreed)
    {
        while(ptCurrentRequest->ptPrev)
        {
            if(uCurrentFrame - ptCurrentRequest->uLastFrameUsed > 30)
            {
                plTerrainChunk* ptChunkToRemove = ptCurrentRequest;
                ptCurrentRequest = ptCurrentRequest->ptPrev;
                pl__make_unresident(ptChunkToRemove);
                bChunkFreed = true;
                // break;
            }
            else
                ptCurrentRequest = ptCurrentRequest->ptPrev;
        }
    }

    if(!bChunkFreed)
        printf("Couldn't free chunks\n");
}

void
pl__remove_from_residency_queue(plTerrainChunk* ptChunk)
{
    plTerrainResidencyNode* ptCurrentRequest = gptTerrainCtx->tRequestQueue.ptNext;

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
            gptTerrainCtx->tRequestQueue.ptNext = ptExistingRequest->ptNext;

        if(ptExistingRequest->ptNext)
            ptExistingRequest->ptNext->ptPrev = ptExistingRequest->ptPrev;

        uint32_t uIndex = (uint32_t)(ptExistingRequest - &gptTerrainCtx->atRequests[0]);
        pl_sb_push(gptTerrainCtx->sbuFreeRequests, uIndex);
    }
}

static void
pl__make_unresident(plTerrainChunk* ptChunk)
{
    pl__remove_from_residency_queue(ptChunk);
    pl__remove_from_replacement_queue(ptChunk);
    if(ptChunk->ptIndexHole && ptChunk->uIndexCount > 0)
    {
        if(ptChunk->ptIndexHole)
            gptFreeList->return_node(&gptTerrainCtx->tIndexBufferManager, ptChunk->ptIndexHole);
        if(ptChunk->ptVertexHole)
            gptFreeList->return_node(&gptTerrainCtx->tVertexBufferManager, ptChunk->ptVertexHole);
        ptChunk->uIndexCount = 0;
        ptChunk->uLastFrameUsed = 0;
        ptChunk->ptIndexHole = NULL;
        ptChunk->ptVertexHole = NULL;
        ptChunk->ptVertexHole = NULL;
        if(ptChunk->aptChildren[0] != NULL)
        {
            pl__make_unresident(ptChunk->aptChildren[0]);
            pl__make_unresident(ptChunk->aptChildren[1]);
            pl__make_unresident(ptChunk->aptChildren[2]);
            pl__make_unresident(ptChunk->aptChildren[3]);
        }
    }
}

static void
pl__handle_residency(void)
{
    const uint64_t uCurrentFrame = gptIOI->get_io()->ulFrameCount;

    plTerrainResidencyNode* ptCurrentRequest = gptTerrainCtx->tRequestQueue.ptNext;
    // ptCurrentRequest->uFrameRequested

    if(ptCurrentRequest)
    {

        plTerrainChunk* ptChunk = ptCurrentRequest->ptChunk;

        FILE* ptDataFile = fopen(gptTerrainCtx->sbtChunkFiles[ptChunk->uFileID].acFile, "rb");
        fseek(ptDataFile, (long)ptChunk->szFileLocation, SEEK_SET);

        fseek(ptDataFile, sizeof(plVec3) * 2 + sizeof(int) + sizeof(int), SEEK_CUR);

        uint32_t uVertexCount = 0;
        fread(&uVertexCount, 1, sizeof(uint32_t), ptDataFile);

        plVec3* ptVertices = PL_ALLOC(uVertexCount * sizeof(plTerrainVertex));
        fread(ptVertices, 1, sizeof(plTerrainVertex) * uVertexCount, ptDataFile);

        uint32_t uIndexCount = 0;
        fread(&uIndexCount, 1, sizeof(uint32_t), ptDataFile);
        
        uint32_t* ptIndices = PL_ALLOC(uIndexCount * sizeof(uint32_t));
        fread(ptIndices, 1, sizeof(uint32_t) * uIndexCount, ptDataFile);

        const uint32_t uVertexBufferSizeBytes = uVertexCount * sizeof(plTerrainVertex);
        const uint32_t uIndexBufferSizeBytes = uIndexCount * sizeof(uint32_t);

        const uint32_t uIndexStageOffset = 0;
        const uint32_t uVertexStageOffset = uIndexBufferSizeBytes;

        // update chunk offsets
        plFreeListNode* ptIndexHole = gptFreeList->get_node(&gptTerrainCtx->tIndexBufferManager, uIndexCount * sizeof(uint32_t));
        plFreeListNode* ptVertexHole = gptFreeList->get_node(&gptTerrainCtx->tVertexBufferManager, uVertexCount * sizeof(plTerrainVertex));

        if(ptIndexHole == NULL || ptVertexHole == NULL)
        {
            if(ptIndexHole) gptFreeList->return_node(&gptTerrainCtx->tIndexBufferManager, ptIndexHole);
            if(ptVertexHole) gptFreeList->return_node(&gptTerrainCtx->tVertexBufferManager, ptVertexHole);
            pl__free_chunk(ptCurrentRequest->fDistance, uIndexCount);
            ptIndexHole = gptFreeList->get_node(&gptTerrainCtx->tIndexBufferManager, uIndexCount * sizeof(uint32_t));
            ptVertexHole = gptFreeList->get_node(&gptTerrainCtx->tVertexBufferManager, uVertexCount * sizeof(plTerrainVertex));
        }

        if(ptIndexHole == NULL || ptVertexHole == NULL)
        {
            if(ptIndexHole) gptFreeList->return_node(&gptTerrainCtx->tIndexBufferManager, ptIndexHole);
            if(ptVertexHole) gptFreeList->return_node(&gptTerrainCtx->tVertexBufferManager, ptVertexHole);
            pl__free_chunk(ptCurrentRequest->fDistance, uIndexCount);
            PL_FREE(ptVertices);
            PL_FREE(ptIndices);
            fclose(ptDataFile);
            printf("No Memory\n");
            return;
        }

        ptChunk->uIndexCount = uIndexCount;
        ptChunk->ptIndexHole = ptIndexHole;
        ptChunk->ptVertexHole = ptVertexHole;

        // update buffer offsets

        plDevice* ptDevice = gptTerrainCtx->ptDevice;
        plBuffer* ptStagingBuffer = gptGfx->get_buffer(ptDevice, gptTerrainCtx->tStagingBuffer);

        void* ptIndexStageDest = &ptStagingBuffer->tMemoryAllocation.pHostMapped[uIndexStageOffset];
        void* ptVertexStageDest = &ptStagingBuffer->tMemoryAllocation.pHostMapped[uVertexStageOffset];

        // copy memory to mapped staging buffer
        memcpy(ptIndexStageDest, ptIndices, uIndexBufferSizeBytes);
        memcpy(ptVertexStageDest, ptVertices, uVertexBufferSizeBytes);

        // destination offsets
        const uint64_t uIndexFinalOffset = ptChunk->ptIndexHole->uOffset;
        const uint64_t uVertexFinalOffset = ptChunk->ptVertexHole->uOffset;
        
        // begin blit pass, copy buffer, end pass
        // NOTE: we are using the starter extension to get a blit encoder, later examples we will
        //       handle this ourselves
        plBlitEncoder* ptEncoder = gptStarter->get_blit_encoder();
        gptGfx->copy_buffer(ptEncoder, gptTerrainCtx->tStagingBuffer, gptTerrainCtx->tIndexBuffer, uIndexStageOffset, uIndexFinalOffset, uIndexBufferSizeBytes);
        gptGfx->copy_buffer(ptEncoder, gptTerrainCtx->tStagingBuffer, gptTerrainCtx->tVertexBuffer, uVertexStageOffset, uVertexFinalOffset, uVertexBufferSizeBytes);
        
        gptStarter->return_blit_encoder(ptEncoder);

        // gptGfx->queue_buffer_for_deletion(ptDevice, tStagingBuffer);

        PL_FREE(ptVertices);
        PL_FREE(ptIndices);

        fclose(ptDataFile);

        pl__remove_from_residency_queue(ptCurrentRequest->ptChunk);
    }
}

static void
pl__touch_chunk(plTerrainChunk* ptChunk)
{
    if(ptChunk == NULL)
        return;

  
    ptChunk->uLastFrameUsed = gptIOI->get_io()->ulFrameCount;

    plTerrainChunk* ptCurrentRequest = gptTerrainCtx->tReplacementQueue.ptNext;

    plTerrainChunk* ptExistingRequest = NULL;

    while(ptCurrentRequest)
    {
        if(ptCurrentRequest == ptChunk)
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

        if(ptExistingRequest->ptNext)
            ptExistingRequest->ptNext->ptPrev = ptExistingRequest->ptPrev;   
    }
    else
    {
        ptExistingRequest = ptChunk;
    }

    // place node at beginning
    ptExistingRequest->ptPrev = NULL;
    if(ptExistingRequest != gptTerrainCtx->tReplacementQueue.ptNext)
        ptExistingRequest->ptNext = gptTerrainCtx->tReplacementQueue.ptNext;
    gptTerrainCtx->tReplacementQueue.ptNext = ptExistingRequest;
    if(ptExistingRequest->ptNext)
        ptExistingRequest->ptNext->ptPrev = ptExistingRequest;

    ptExistingRequest->uLastFrameUsed = gptIOI->get_io()->ulFrameCount;
    // ptExistingRequest->fDistance = fDistance;
    // ptExistingRequest->ptChunk = ptChunk;
}

static void
pl__request_residency(plTerrainChunk* ptChunk, float fDistance)
{
    if(ptChunk == NULL)
        return;

    if(ptChunk->ptIndexHole == NULL)
    {
        plTerrainResidencyNode* ptCurrentRequest = gptTerrainCtx->tRequestQueue.ptNext;

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
            if(pl_sb_size(gptTerrainCtx->sbuFreeRequests) > 0)
            {
                uint32_t uNewIndex = pl_sb_pop(gptTerrainCtx->sbuFreeRequests);
                ptExistingRequest = &gptTerrainCtx->atRequests[uNewIndex];

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
        if(ptExistingRequest != gptTerrainCtx->tRequestQueue.ptNext)
            ptExistingRequest->ptNext = gptTerrainCtx->tRequestQueue.ptNext;
        gptTerrainCtx->tRequestQueue.ptNext = ptExistingRequest;
        if(ptExistingRequest->ptNext)
            ptExistingRequest->ptNext->ptPrev = ptExistingRequest;

        ptExistingRequest->uFrameRequested = gptIOI->get_io()->ulFrameCount;
        ptExistingRequest->fDistance = fDistance;
        ptExistingRequest->ptChunk = ptChunk;
    }
}

static void
pl__render_chunk(plCamera* ptCamera , plDynamicDataBlock* ptDynamicDataBlock, plRenderEncoder* ptEncoder, plTerrainChunk* ptChunk, plTerrainChunkFile* ptFile)
{
    PL_ASSERT(ptChunk != NULL);

    plAABB tAABB = {
        .tMin = ptChunk->tMinBound,
        .tMax = ptChunk->tMaxBound
    };

    if(!pl__sat_visibility_test(ptCamera, &tAABB))
        return;

    if(ptChunk->ptParent == NULL)
        gptTerrainCtx->uVisibleRootChunks++;
    gptTerrainCtx->uVisibleChunks++;

    plVec3 tClosestPoint = gptCollision->point_closest_point_aabb(ptCamera->tPos, tAABB);
    float fDistance = fabsf(pl_length_vec3(pl_sub_vec3(tClosestPoint, ptCamera->tPos)));

    pl__request_residency(ptChunk, fDistance);

    if(ptChunk->ptIndexHole == NULL)
        return;
    
    float fViewportWidth = gptIOI->get_io()->tMainViewportSize.x;
    float fHorizontalFieldOfView = ptCamera->fFieldOfView * ptCamera->fAspectRatio;

    float fK = fViewportWidth / (2.0f * tanf(0.5f * fHorizontalFieldOfView));

    float fGeometricError = ptFile->fMaxBaseError * (float)ptChunk->uLevel;


    float fRho = fGeometricError * fK / fDistance;

    if(!pl__all_children_resident(ptChunk) || fRho <= gptTerrainCtx->fTau)
    {

        const plMat4 tMVP = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);
        plDevice* ptDevice = gptStarter->get_device();
        plDynamicBinding tDynamicBinding = pl_allocate_dynamic_data(gptGfx, ptDevice, ptDynamicDataBlock);
        plGpuDynTerrainData* ptDynamic = (plGpuDynTerrainData*)tDynamicBinding.pcData;
        ptDynamic->tMvp = tMVP;
        ptDynamic->iLevel = (int)ptChunk->uLevel;
        ptDynamic->tFlags = gptTerrainCtx->tFlags;

        // submit nonindexed draw using basic API
        plShaderHandle tShader = (gptTerrainCtx->tFlags & PL_TERRAIN_FLAGS_WIREFRAME) ? gptTerrainCtx->tWireframeShader : gptTerrainCtx->tShader;
        gptGfx->bind_shader(ptEncoder, tShader);
        gptGfx->bind_vertex_buffer(ptEncoder, gptTerrainCtx->tVertexBuffer);
        gptGfx->bind_graphics_bind_groups(ptEncoder, tShader, 0, 0, NULL, 1, &tDynamicBinding);

        const plDrawIndex tDraw = {
            .uInstanceCount = 1,
            .uIndexCount    = ptChunk->uIndexCount,
            .uVertexStart   = (uint32_t)ptChunk->ptVertexHole->uOffset / sizeof(plTerrainVertex),
            .uIndexStart    = (uint32_t)ptChunk->ptIndexHole->uOffset / sizeof(uint32_t),
            .tIndexBuffer   = gptTerrainCtx->tIndexBuffer
        };
        gptGfx->draw_indexed(ptEncoder, 1, &tDraw);

        // uint32_t atColors[] = {
        //     PL_COLOR_32_RED,
        //     PL_COLOR_32_GREEN,
        //     PL_COLOR_32_BLUE,
        //     PL_COLOR_32_YELLOW,
        //     PL_COLOR_32_MAGENTA,
        //     PL_COLOR_32_CYAN
        // };

        // gptDraw->add_3d_aabb(gptTerrainCtx->pt3dDrawlist, ptChunk->tMinBound, ptChunk->tMaxBound, (plDrawLineOptions){.fThickness = 1000.0f, .uColor = atColors[ptChunk->uLevel]});

        pl__touch_chunk(ptChunk);
        if(fRho > gptTerrainCtx->fTau) // we actually want children
        {
            for(uint32_t i = 0; i < 4; i++)
            {
                if(ptChunk->aptChildren[i] == NULL)
                    break;

                plAABB tChildAABB = {
                    .tMin = ptChunk->aptChildren[i]->tMinBound,
                    .tMax = ptChunk->aptChildren[i]->tMaxBound
                };
                plVec3 tClosestChildPoint = gptCollision->point_closest_point_aabb(ptCamera->tPos, tChildAABB);
                float fChildDistance = fabsf(pl_length_vec3(pl_sub_vec3(tClosestChildPoint, ptCamera->tPos)));
                pl__request_residency(ptChunk->aptChildren[i], fChildDistance);
            }
        }
        else // we actually want this chunk, not children
        {
            pl__unload_children(ptChunk);
        }
    }
    else
    {
        for(uint32_t i = 0; i < 4; i++)
            pl__render_chunk(ptCamera, ptDynamicDataBlock, ptEncoder, ptChunk->aptChildren[i], ptFile);
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

void
pl__draw_residency(plDrawLayer2D* ptLayer, plTerrainChunk* ptChunk, plVec2 tOrigin, float fRadius, plTerrainChunkFile* ptFile)
{
    int iTopDownLevel = (int)(ptFile->iTreeDepth - (int)ptChunk->uLevel - 1);

    // if(iBottomUpLevel > 1)
    //     return;

    uint32_t uColor = PL_COLOR_32_RED;
    if(ptChunk->ptIndexHole)
        uColor = PL_COLOR_32_GREEN;

    gptDraw->add_circle_filled(ptLayer, tOrigin, fRadius, 0, (plDrawSolidOptions){.uColor = uColor});

    if(ptChunk->ptIndexHole)
    {
        if(gptIOI->get_io()->ulFrameCount - ptChunk->uLastFrameUsed > 1 )
           gptDraw->add_circle_filled(ptLayer, tOrigin, fRadius / 2.0f, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_BLACK});
    }

    float fBaseSpacing = fRadius * 2.5f;
    float fChildSpacing = fBaseSpacing * powf(4.0f, (float)(ptFile->iTreeDepth - 2 - iTopDownLevel));
    
    float fChildWidth = fChildSpacing * 4.0f;

    for(uint32_t i = 0; i < 4; i++)
    {
        float fXOffset = ((float)(i) + 0.5f) * fChildSpacing + tOrigin.x - 0.5f * fChildWidth;
        if(ptChunk->aptChildren[i])
        {
            gptDraw->add_line(ptLayer, tOrigin, (plVec2){.x = fXOffset, .y = fRadius * 4.0f + tOrigin.y}, (plDrawLineOptions){.fThickness = 1.0f, .uColor = PL_COLOR_32_WHITE});
            pl__draw_residency(ptLayer, ptChunk->aptChildren[i], (plVec2){.x = fXOffset, .y = 4.0f * fRadius + tOrigin.y}, fRadius, ptFile);
        }
    }
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_terrain_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plTerrainI tApi = {
        .initialize        = pl_initialize_cdlod,
        .cleanup           = pl_cleanup_cdlod,
        .load_chunk_file   = pl_chlod_load_chunk_file,
        .finalize          = pl_finalize_terrain,
        .render            = pl_render_terrain,
        .prepare           = pl_prepare_terrain,
        .reload_shaders    = pl_terrain_load_shaders,
        .set_flags         = pl_terrain_set_flags,
        .get_flags         = pl_terrain_get_flags,
        .draw_residency    = pl_terrain_draw_residency,
    };
    pl_set_api(ptApiRegistry, plTerrainI, &tApi);

    #ifndef PL_UNITY_BUILD
        gptMemory           = pl_get_api_latest(ptApiRegistry, plMemoryI);
        gptImage            = pl_get_api_latest(ptApiRegistry, plImageI);
        gptFile             = pl_get_api_latest(ptApiRegistry, plFileI);
        gptProfile          = pl_get_api_latest(ptApiRegistry, plProfileI);
        gptGfx              = pl_get_api_latest(ptApiRegistry, plGraphicsI);
        gptFreeList         = pl_get_api_latest(ptApiRegistry, plFreeListI);
        gptIOI              = pl_get_api_latest(ptApiRegistry, plIOI);
        gptStarter          = pl_get_api_latest(ptApiRegistry, plStarterI);
        gptShader           = pl_get_api_latest(ptApiRegistry, plShaderI);
        gptCollision        = pl_get_api_latest(ptApiRegistry, plCollisionI);
        gptScreenLog        = pl_get_api_latest(ptApiRegistry, plScreenLogI);
        gptDraw             = pl_get_api_latest(ptApiRegistry, plDrawI);
        gptTerrainProcessor = pl_get_api_latest(ptApiRegistry, plTerrainProcessorI);
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

#endif
