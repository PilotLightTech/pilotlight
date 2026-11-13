/*
   pl_stage_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal forward declarations
// [SECTION] internal structs
// [SECTION] global data
// [SECTION] public api implementation
// [SECTION] helper implementations
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <math.h>
#include "pl.h"
#include "pl_stage_ext.h"

// libraries
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extensions
#include "pl_graphics_ext.h"
#include "pl_gpu_allocators_ext.h"

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

    static const plGraphicsI*      gptGfx           = NULL;
    static const plIOI*            gptIOI           = NULL;
    static const plGPUAllocatorsI* gptGpuAllocators = NULL;
#endif

#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plStageBufferUploadRequest
{
    uint32_t       uBufferIndex;
    uint64_t       uOffset;
    uint64_t       uSize;
    uint64_t       uDestinationOffset;
    plBufferHandle uDestinationBuffer;
} plStageBufferUploadRequest;

typedef struct _plStageTextureUploadRequest
{
    uint32_t          uBufferIndex;
    uint64_t          uOffset;
    uint64_t          uSize;
    plBufferImageCopy tBufferImageCopy;
    plTextureHandle   uDestinationTexture;
    bool              bGenerateMips;
} plStageTextureUploadRequest;

typedef struct _plStageBlock
{
    plBufferHandle tBuffer;
    uint64_t       uSize;
    uint64_t       uCurrentOffset;
} plStageBlock;

typedef struct _plStageContext
{
    plDevice*            ptDevice;
    plCommandPool*       ptCmdPool;

    plStageBufferUploadRequest* sbtBufferUploadRequests;
    plStageTextureUploadRequest* sbtTextureUploadRequests;

    // blocks
    plStageBlock* sbtBlocks;
    plStageBlock* sbtStageBlocks;

    // gpu allocators
    
    plDeviceMemoryAllocatorI* ptLocalDedicatedAllocator;
    plDeviceMemoryAllocatorI* ptLocalBuddyAllocator;
    plDeviceMemoryAllocatorI* ptStagingUnCachedAllocator;
    plDeviceMemoryAllocatorI* ptStagingUnCachedBuddyAllocator;
    plDeviceMemoryAllocatorI* ptStagingCachedAllocator;

    uint64_t uNextValue;
    uint64_t uLastUsedFrame;
    uint64_t uInitialBlockSize;
} plStageContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plStageContext* gptStageCtx = NULL;
void pl_stage_flush(void);

static uint64_t
pl__stage_next_block_size(uint64_t uRequired)
{
    uint64_t uCapacity = gptStageCtx->uInitialBlockSize;
    const uint64_t uLimit = gptGpuAllocators->get_buddy_block_size();
    if(pl_sb_size(gptStageCtx->sbtStageBlocks))
    {
        const uint64_t uLast = pl_sb_last(gptStageCtx->sbtStageBlocks).uSize;
        uCapacity = uLast > uLimit / 2 ? uLimit : uLast * 2;
    }
    return pl_max(uRequired, uCapacity);
}

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_stage_initialize(plStageInit tInit)
{
    if(gptStageCtx->ptCmdPool) // already initialized
        return;

    gptStageCtx->ptDevice = tInit.ptDevice;
    gptStageCtx->uInitialBlockSize = tInit.uInitialBlockSize ? tInit.uInitialBlockSize : 1024 * 1024;
    gptStageCtx->uInitialBlockSize = pl_max((uint64_t)256, pl_min(gptStageCtx->uInitialBlockSize, (uint64_t)SIZE_MAX - 255));
    gptStageCtx->ptLocalBuddyAllocator           = gptGpuAllocators->get_local_buddy_allocator(gptStageCtx->ptDevice);
    gptStageCtx->ptLocalDedicatedAllocator       = gptGpuAllocators->get_local_dedicated_allocator(gptStageCtx->ptDevice);
    gptStageCtx->ptStagingUnCachedAllocator      = gptGpuAllocators->get_staging_uncached_allocator(gptStageCtx->ptDevice);
    gptStageCtx->ptStagingUnCachedBuddyAllocator = gptGpuAllocators->get_staging_uncached_buddy_allocator(gptStageCtx->ptDevice);
    gptStageCtx->ptStagingCachedAllocator        = gptGpuAllocators->get_staging_cached_allocator(gptStageCtx->ptDevice);

    gptStageCtx->ptCmdPool = gptGfx->create_command_pool(gptStageCtx->ptDevice, NULL);
    gptStageCtx->uNextValue = 0;
}

void
pl_stage_cleanup(void)
{
    if(gptStageCtx->ptCmdPool == NULL) // already initialized
        return;
    pl_stage_flush();
    for(uint32_t i = 0; i < pl_sb_size(gptStageCtx->sbtStageBlocks); i++)
        gptGfx->destroy_buffer(gptStageCtx->ptDevice, gptStageCtx->sbtStageBlocks[i].tBuffer);
    gptGfx->cleanup_command_pool(gptStageCtx->ptCmdPool);
    pl_sb_free(gptStageCtx->sbtBlocks);
    pl_sb_free(gptStageCtx->sbtStageBlocks);
    pl_sb_free(gptStageCtx->sbtBufferUploadRequests);
    pl_sb_free(gptStageCtx->sbtTextureUploadRequests);
    gptStageCtx->ptCmdPool = NULL;
}

void
pl_stage_stage_buffer_upload(plBufferHandle tDestination, uint64_t uOffset, const void* pData, uint64_t uSize)
{

    if(pData == NULL || uSize == 0 || uSize > SIZE_MAX - 255)
        return;

    plStageBufferUploadRequest tRequest = {
        .uSize              = uSize,
        .uDestinationOffset = uOffset,
        .uDestinationBuffer = tDestination,
    };

    gptStageCtx->uLastUsedFrame = gptIOI->get_io()->ulFrameCount;


    const uint32_t uBlockCount = pl_sb_size(gptStageCtx->sbtStageBlocks);

    bool bBlockFound = false;

    for(uint32_t i = 0; i < uBlockCount; i++)
    {
        plStageBlock* ptBlock = &gptStageCtx->sbtStageBlocks[i];

        const uint64_t uAlignedOffset = (ptBlock->uCurrentOffset + 255) & ~(uint64_t)255;
        if(uAlignedOffset <= ptBlock->uSize && ptBlock->uSize - uAlignedOffset >= uSize)
        {
            tRequest.uBufferIndex = i;
            tRequest.uOffset = uAlignedOffset;
            bBlockFound = true;
            break;
        }
    }

    // check if first run
    if(!bBlockFound)
    {
        plBufferDesc tStagingBufferDesc = {
            .pcDebugName = "staging buffer",
            .szByteSize  = (size_t)pl__stage_next_block_size(uSize),
            .eUsage      = PL_BUFFER_USAGE_TRANSFER
        };
        plBuffer* ptBuffer = NULL;
        plBufferHandle tBuffer = gptGfx->create_buffer(gptStageCtx->ptDevice, &tStagingBufferDesc, &ptBuffer);

        // allocate memory
        plDeviceMemoryAllocatorI* ptAllocator = ptBuffer->tMemoryRequirements.ulSize > gptGpuAllocators->get_buddy_block_size() ?
            gptStageCtx->ptStagingUnCachedAllocator : gptStageCtx->ptStagingUnCachedBuddyAllocator;
        const plDeviceMemoryAllocation tAllocation = ptAllocator->allocate(ptAllocator->ptInst,
            ptBuffer->tMemoryRequirements.uMemoryTypeBits,
            ptBuffer->tMemoryRequirements.ulSize,
            ptBuffer->tMemoryRequirements.ulAlignment,
            "staging buffer memory");

        // bind memory
        gptGfx->bind_buffer_to_memory(gptStageCtx->ptDevice, tBuffer, &tAllocation);

        tRequest.uBufferIndex = pl_sb_size(gptStageCtx->sbtStageBlocks);

        plStageBlock tBlock = {
            .tBuffer = tBuffer,
            .uSize = tStagingBufferDesc.szByteSize
        };
        pl_sb_push(gptStageCtx->sbtStageBlocks, tBlock);
    }
    gptStageCtx->sbtStageBlocks[tRequest.uBufferIndex].uCurrentOffset = tRequest.uOffset + uSize;

    plBuffer* ptBuffer = gptGfx->get_buffer(gptStageCtx->ptDevice, gptStageCtx->sbtStageBlocks[tRequest.uBufferIndex].tBuffer);
    uint8_t* pucData = (uint8_t*)&ptBuffer->tMemoryAllocation.pHostMapped[tRequest.uOffset];
    memcpy(pucData, pData, uSize);

    pl_sb_push(gptStageCtx->sbtBufferUploadRequests, tRequest);
}

void
pl_stage_stage_texture_upload(plTextureHandle tDestination, const plBufferImageCopy* ptCopy, const void* pData, uint64_t uSize, bool bGenerateMips)
{

    if(pData == NULL || ptCopy == NULL || uSize == 0 || uSize > SIZE_MAX - 255 || ptCopy->szBufferOffset >= uSize)
        return;

    gptStageCtx->uLastUsedFrame = gptIOI->get_io()->ulFrameCount;

    plStageTextureUploadRequest tRequest = {
        .uSize               = uSize,
        .tBufferImageCopy    = *ptCopy,
        .uDestinationTexture = tDestination,
        .bGenerateMips       = bGenerateMips
    };


    const uint32_t uBlockCount = pl_sb_size(gptStageCtx->sbtStageBlocks);

    bool bBlockFound = false;

    for(uint32_t i = 0; i < uBlockCount; i++)
    {
        plStageBlock* ptBlock = &gptStageCtx->sbtStageBlocks[i];

        const uint64_t uAlignedOffset = (ptBlock->uCurrentOffset + 255) & ~(uint64_t)255;
        if(uAlignedOffset <= ptBlock->uSize && ptBlock->uSize - uAlignedOffset >= uSize)
        {
            tRequest.uBufferIndex = i;
            tRequest.uOffset = uAlignedOffset;
            bBlockFound = true;
            break;
        }
    }

    // check if first run
    if(!bBlockFound)
    {
        plBufferDesc tStagingBufferDesc = {
            .pcDebugName = "staging buffer",
            .szByteSize  = (size_t)pl__stage_next_block_size(uSize),
            .eUsage      = PL_BUFFER_USAGE_TRANSFER
        };
        plBuffer* ptBuffer = NULL;
        plBufferHandle tBuffer = gptGfx->create_buffer(gptStageCtx->ptDevice, &tStagingBufferDesc, &ptBuffer);

        // allocate memory
        plDeviceMemoryAllocatorI* ptAllocator = ptBuffer->tMemoryRequirements.ulSize > gptGpuAllocators->get_buddy_block_size() ?
            gptStageCtx->ptStagingUnCachedAllocator : gptStageCtx->ptStagingUnCachedBuddyAllocator;
        const plDeviceMemoryAllocation tAllocation = ptAllocator->allocate(ptAllocator->ptInst,
            ptBuffer->tMemoryRequirements.uMemoryTypeBits,
            ptBuffer->tMemoryRequirements.ulSize,
            ptBuffer->tMemoryRequirements.ulAlignment,
            "staging buffer memory");

        // bind memory
        gptGfx->bind_buffer_to_memory(gptStageCtx->ptDevice, tBuffer, &tAllocation);

        tRequest.uBufferIndex = pl_sb_size(gptStageCtx->sbtStageBlocks);

        plStageBlock tBlock = {
            .tBuffer = tBuffer,
            .uSize = tStagingBufferDesc.szByteSize
        };
        pl_sb_push(gptStageCtx->sbtStageBlocks, tBlock);
    }
    gptStageCtx->sbtStageBlocks[tRequest.uBufferIndex].uCurrentOffset = tRequest.uOffset + uSize;
    tRequest.tBufferImageCopy.szBufferOffset += (size_t)tRequest.uOffset;

    plBuffer* ptBuffer = gptGfx->get_buffer(gptStageCtx->ptDevice, gptStageCtx->sbtStageBlocks[tRequest.uBufferIndex].tBuffer);
    uint8_t* pucData = (uint8_t*)&ptBuffer->tMemoryAllocation.pHostMapped[tRequest.uOffset];
    memcpy(pucData, pData, uSize);

    pl_sb_push(gptStageCtx->sbtTextureUploadRequests, tRequest);
}

void
pl_stage_flush(void)
{
    if(pl_sb_size(gptStageCtx->sbtTextureUploadRequests) == 0 && pl_sb_size(gptStageCtx->sbtBufferUploadRequests) == 0)
        return;

    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(gptStageCtx->ptCmdPool, "upload staging now");
    gptGfx->begin_command_recording(ptCommandBuffer);
    gptGfx->consumer_barrier(ptCommandBuffer, PL_PIPELINE_STAGE_ALL, PL_PIPELINE_STAGE_BLIT, PL_BARRIER_SCOPE_ALL);

    gptGfx->begin_compute_pass(ptCommandBuffer, NULL);

    uint32_t uRequestCount = pl_sb_size(gptStageCtx->sbtBufferUploadRequests);
    for(uint32_t i = 0; i < uRequestCount; i++)
    {
        gptGfx->copy_buffer(ptCommandBuffer,
            gptStageCtx->sbtStageBlocks[gptStageCtx->sbtBufferUploadRequests[i].uBufferIndex].tBuffer,
            gptStageCtx->sbtBufferUploadRequests[i].uDestinationBuffer,
            gptStageCtx->sbtBufferUploadRequests[i].uOffset,
            gptStageCtx->sbtBufferUploadRequests[i].uDestinationOffset,
            gptStageCtx->sbtBufferUploadRequests[i].uSize);
    }
    pl_sb_reset(gptStageCtx->sbtBufferUploadRequests);

    uRequestCount = pl_sb_size(gptStageCtx->sbtTextureUploadRequests);
    for(uint32_t i = 0; i < uRequestCount; i++)
    {
        gptGfx->copy_buffer_to_texture(ptCommandBuffer,
            gptStageCtx->sbtStageBlocks[gptStageCtx->sbtTextureUploadRequests[i].uBufferIndex].tBuffer,
            gptStageCtx->sbtTextureUploadRequests[i].uDestinationTexture,
            1,
            &gptStageCtx->sbtTextureUploadRequests[i].tBufferImageCopy);
        if(gptStageCtx->sbtTextureUploadRequests[i].bGenerateMips)
            gptGfx->generate_mipmaps(ptCommandBuffer, gptStageCtx->sbtTextureUploadRequests[i].uDestinationTexture);
    }
    pl_sb_reset(gptStageCtx->sbtTextureUploadRequests);
    gptGfx->producer_barrier(ptCommandBuffer, PL_PIPELINE_STAGE_BLIT, PL_PIPELINE_STAGE_ALL, PL_BARRIER_SCOPE_ALL);
    gptGfx->end_compute_pass(ptCommandBuffer);

    // finish recording
    gptGfx->end_command_recording(ptCommandBuffer);

    // submit command buffer
    gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);
    gptGfx->return_command_buffer(ptCommandBuffer);

    const uint32_t uBlockCount = pl_sb_size(gptStageCtx->sbtStageBlocks);
    for(uint32_t i = 0; i < uBlockCount; i++)
    {
        gptStageCtx->sbtStageBlocks[i].uCurrentOffset = 0;
    }
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

bool
pl_stage_grow_buffer(plBufferHandle* ptHandle, uint64_t uCapacity)
{
    plDevice* ptDevice = gptStageCtx->ptDevice;
    if(!ptHandle || !gptGfx->is_buffer_valid(ptDevice, *ptHandle) || uCapacity > SIZE_MAX)
        return false;

    const plBufferHandle tOldHandle = *ptHandle;
    const plBuffer* ptOld = gptGfx->get_buffer(ptDevice, tOldHandle);
    plBufferDesc tDesc = ptOld->tDesc;
    const size_t szOldSize = tDesc.szByteSize;
    if(uCapacity <= szOldSize)
        return true;
    if(!(tDesc.eUsage & PL_BUFFER_USAGE_TRANSFER))
        return false;

    // Pending uploads still target the old handle. Complete them before copying.
    // The copy's completion fence also covers prior submissions on the shared
    // graphics queue, making descriptor replacement safe across frames in flight.
    const bool bMapped = ptOld->tMemoryAllocation.pHostMapped != NULL;
    pl_stage_flush();
    tDesc.szByteSize = (size_t)uCapacity;
    plBuffer* ptNew = NULL;
    const plBufferHandle tNewHandle = gptGfx->create_buffer(ptDevice, &tDesc, &ptNew);
    if(!gptGfx->is_buffer_valid(ptDevice, tNewHandle))
        return false;
    plDeviceMemoryAllocatorI* ptAllocator = bMapped ? gptStageCtx->ptStagingUnCachedAllocator : gptStageCtx->ptLocalDedicatedAllocator;
    if(ptNew->tMemoryRequirements.ulSize <= gptGpuAllocators->get_buddy_block_size())
        ptAllocator = bMapped ? gptStageCtx->ptStagingUnCachedBuddyAllocator : gptStageCtx->ptLocalBuddyAllocator;
    const plDeviceMemoryAllocation tAllocation = ptAllocator->allocate(ptAllocator->ptInst, ptNew->tMemoryRequirements.uMemoryTypeBits, ptNew->tMemoryRequirements.ulSize, ptNew->tMemoryRequirements.ulAlignment, tDesc.pcDebugName);
    gptGfx->bind_buffer_to_memory(ptDevice, tNewHandle, &tAllocation);

    plCommandBuffer* ptCmd = gptGfx->request_command_buffer(gptStageCtx->ptCmdPool, "grow buffer");
    gptGfx->begin_command_recording(ptCmd);
    gptGfx->consumer_barrier(ptCmd, PL_PIPELINE_STAGE_ALL, PL_PIPELINE_STAGE_BLIT, PL_BARRIER_SCOPE_BUFFERS);
    gptGfx->begin_compute_pass(ptCmd, NULL);
    gptGfx->copy_buffer(ptCmd, tOldHandle, tNewHandle, 0, 0, szOldSize);
    gptGfx->producer_barrier(ptCmd, PL_PIPELINE_STAGE_BLIT, PL_PIPELINE_STAGE_ALL, PL_BARRIER_SCOPE_BUFFERS);
    gptGfx->end_compute_pass(ptCmd);
    gptGfx->end_command_recording(ptCmd);
    gptGfx->submit_command_buffer(ptCmd, NULL);
    gptGfx->wait_on_command_buffer(ptCmd);
    gptGfx->return_command_buffer(ptCmd);
    gptGfx->queue_buffer_for_deletion(ptDevice, tOldHandle);
    *ptHandle = tNewHandle;
    return true;
}

void
pl_load_stage_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plStageI tApi = {
        .initialize             = pl_stage_initialize,
        .cleanup                = pl_stage_cleanup,
        .stage_buffer_upload    = pl_stage_stage_buffer_upload,
        .stage_texture_upload   = pl_stage_stage_texture_upload,
        .flush                  = pl_stage_flush,
        .grow_buffer            = pl_stage_grow_buffer
    };
    pl_set_api(ptApiRegistry, plStageI, &tApi);

    gptMemory        = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptGfx           = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptIOI           = pl_get_api_latest(ptApiRegistry, plIOI);
    gptGpuAllocators = pl_get_api_latest(ptApiRegistry, plGPUAllocatorsI);

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);
    if(bReload)
    {
        gptStageCtx = ptDataRegistry->get_data("plStageContext");
    }
    else
    {
        static plStageContext tStageContext = {0};
        gptStageCtx = &tStageContext;
        ptDataRegistry->set_data("plStageContext", gptStageCtx);
    }

}

void
pl_unload_stage_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    if(bReload)
        return;

    const plStageI* ptApi = pl_get_api_latest(ptApiRegistry, plStageI);
    ptApiRegistry->remove_api(ptApi);
}
