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
    plTimelineSemaphore* ptSemaphore;
    plCommandPool*       ptCmdPool;

    plStageBufferUploadRequest* sbtBufferUploadRequests;
    plStageTextureUploadRequest* sbtTextureUploadRequests;

    // blocks
    plStageBlock* sbtBlocks;
    plStageBlock* sbtStageBlocks;

    // gpu allocators
    plDeviceMemoryAllocatorI* ptAllocator;

    plCommandBuffer* ptCurrentCommandBuffer;
    plBlitEncoder* ptCurrentEncoder;
    uint64_t uNextValue;
    uint64_t uLastUsedFrame;
} plStageContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plStageContext* gptStageCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_stage_initialize(plStageInit tInit)
{
    if(gptStageCtx->ptCmdPool) // already initialized
        return;

    gptStageCtx->ptDevice = tInit.ptDevice;
    gptStageCtx->ptAllocator = gptGpuAllocators->get_staging_uncached_allocator(gptStageCtx->ptDevice);
    gptStageCtx->ptCmdPool = gptGfx->create_command_pool(gptStageCtx->ptDevice, NULL);
    gptStageCtx->ptSemaphore = gptGfx->create_semaphore(gptStageCtx->ptDevice, true);
    gptStageCtx->uNextValue = 0;
}

void
pl_stage_cleanup(void)
{
    gptGfx->cleanup_command_pool(gptStageCtx->ptCmdPool);
    gptGfx->cleanup_semaphore(gptStageCtx->ptSemaphore);
    pl_sb_free(gptStageCtx->sbtBlocks);
    pl_sb_free(gptStageCtx->sbtStageBlocks);
    pl_sb_free(gptStageCtx->sbtBufferUploadRequests);
    pl_sb_free(gptStageCtx->sbtTextureUploadRequests);
}

void
pl_stage_stage_buffer_upload(plBufferHandle tDestination, uint64_t uOffset, const void* pData, uint64_t uSize)
{

    if(pData == NULL)
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

        if(ptBlock->uSize - ptBlock->uCurrentOffset >= uSize)
        {
            tRequest.uBufferIndex = i;
            tRequest.uOffset = ptBlock->uCurrentOffset;
            bBlockFound = true;
            break;
        }
    }

    // check if first run
    if(!bBlockFound)
    {
        plBufferDesc tStagingBufferDesc = {
            .pcDebugName = "staging buffer",
            .szByteSize  = pl_max(gptGpuAllocators->get_buddy_block_size(), uSize),
            .tUsage      = PL_BUFFER_USAGE_TRANSFER_SOURCE
        };
        plBuffer* ptBuffer = NULL;
        plBufferHandle tBuffer = gptGfx->create_buffer(gptStageCtx->ptDevice, &tStagingBufferDesc, &ptBuffer);

        // allocate memory
        const plDeviceMemoryAllocation tAllocation = gptStageCtx->ptAllocator->allocate(gptStageCtx->ptAllocator->ptInst, 
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
    gptStageCtx->sbtStageBlocks[tRequest.uBufferIndex].uCurrentOffset += uSize;

    plBuffer* ptBuffer = gptGfx->get_buffer(gptStageCtx->ptDevice, gptStageCtx->sbtStageBlocks[tRequest.uBufferIndex].tBuffer);
    uint8_t* pucData = (uint8_t*)&ptBuffer->tMemoryAllocation.pHostMapped[tRequest.uOffset];
    memcpy(pucData, pData, uSize);

    pl_sb_push(gptStageCtx->sbtBufferUploadRequests, tRequest);
}

void
pl_stage_stage_texture_upload(plTextureHandle tDestination, const plBufferImageCopy* ptCopy, const void* pData, uint64_t uSize, bool bGenerateMips)
{

    if(pData == NULL)
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

        if(ptBlock->uSize - ptBlock->uCurrentOffset >= uSize)
        {
            tRequest.uBufferIndex = i;
            tRequest.uOffset = ptBlock->uCurrentOffset;
            bBlockFound = true;
            break;
        }
    }

    // check if first run
    if(!bBlockFound)
    {
        plBufferDesc tStagingBufferDesc = {
            .pcDebugName = "staging buffer",
            .szByteSize  = pl_max(gptGpuAllocators->get_buddy_block_size(), uSize),
            .tUsage      = PL_BUFFER_USAGE_TRANSFER_SOURCE
        };
        plBuffer* ptBuffer = NULL;
        plBufferHandle tBuffer = gptGfx->create_buffer(gptStageCtx->ptDevice, &tStagingBufferDesc, &ptBuffer);

        // allocate memory
        const plDeviceMemoryAllocation tAllocation = gptStageCtx->ptAllocator->allocate(gptStageCtx->ptAllocator->ptInst, 
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
    gptStageCtx->sbtStageBlocks[tRequest.uBufferIndex].uCurrentOffset += uSize;

    plBuffer* ptBuffer = gptGfx->get_buffer(gptStageCtx->ptDevice, gptStageCtx->sbtStageBlocks[tRequest.uBufferIndex].tBuffer);
    uint8_t* pucData = (uint8_t*)&ptBuffer->tMemoryAllocation.pHostMapped[tRequest.uOffset];
    memcpy(pucData, pData, uSize);

    pl_sb_push(gptStageCtx->sbtTextureUploadRequests, tRequest);
}

void
pl_stage_flush(void)
{
    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(gptStageCtx->ptCmdPool, "upload staging now");
    gptGfx->begin_command_recording(ptCommandBuffer, NULL);

    plBlitEncoder* ptEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
    gptGfx->pipeline_barrier_blit(ptEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);

    uint32_t uRequestCount = pl_sb_size(gptStageCtx->sbtBufferUploadRequests);
    for(uint32_t i = 0; i < uRequestCount; i++)
    {
        gptGfx->copy_buffer(ptEncoder,
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
        gptGfx->copy_buffer_to_texture(ptEncoder,
            gptStageCtx->sbtStageBlocks[gptStageCtx->sbtTextureUploadRequests[i].uBufferIndex].tBuffer,
            gptStageCtx->sbtTextureUploadRequests[i].uDestinationTexture,
            1,
            &gptStageCtx->sbtTextureUploadRequests[i].tBufferImageCopy);
        if(gptStageCtx->sbtTextureUploadRequests[i].bGenerateMips)
            gptGfx->generate_mipmaps(ptEncoder, gptStageCtx->sbtTextureUploadRequests[i].uDestinationTexture);
    }
    pl_sb_reset(gptStageCtx->sbtTextureUploadRequests);

    gptGfx->pipeline_barrier_blit(ptEncoder, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
    gptGfx->end_blit_pass(ptEncoder);

    // finish recording
    gptGfx->end_command_recording(ptCommandBuffer);

    // submit command buffer
    gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);
    gptGfx->return_command_buffer(ptCommandBuffer);

    // const uint32_t uBlockCount = pl_sb_size(gptStageCtx->sbtStageBlocks);
    // for(uint32_t i = 0; i < uBlockCount; i++)
    // {
    //     gptStageCtx->sbtStageBlocks[i].uCurrentOffset = 0;
    // }
    

    const uint32_t uBlockCount = pl_sb_size(gptStageCtx->sbtStageBlocks);
    for(uint32_t i = 0; i < uBlockCount; i++)
    {
        // gptGfx->destroy_buffer(gptStageCtx->ptDevice, gptStageCtx->sbtStageBlocks[i].tBuffer);
        gptStageCtx->sbtStageBlocks[i].uCurrentOffset = 0;
    }
    // pl_sb_reset(gptStageCtx->sbtStageBlocks);
}

uint64_t
pl_stage_queue_buffer_upload(plBufferHandle tDestination, uint64_t uOffset, const void* pData, uint64_t uSize)
{

    if(pData == NULL)
        return 0;

    gptStageCtx->uLastUsedFrame = gptIOI->get_io()->ulFrameCount;

    if(gptStageCtx->ptCurrentCommandBuffer == NULL)
    {
        const plBeginCommandInfo tSceneBeginInfo = {
            .uWaitSemaphoreCount   = 1,
            .atWaitSempahores      = {gptStageCtx->ptSemaphore},
            .auWaitSemaphoreValues = {gptStageCtx->uNextValue++},
        };

        gptStageCtx->ptCurrentCommandBuffer = gptGfx->request_command_buffer(gptStageCtx->ptCmdPool, "stage");
        gptGfx->begin_command_recording(gptStageCtx->ptCurrentCommandBuffer, &tSceneBeginInfo);

        gptStageCtx->ptCurrentEncoder = gptGfx->begin_blit_pass(gptStageCtx->ptCurrentCommandBuffer);
        gptGfx->pipeline_barrier_blit(gptStageCtx->ptCurrentEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);
    }

    plStageBufferUploadRequest tRequest = {
        .uSize              = uSize,
        .uDestinationOffset = uOffset,
        .uDestinationBuffer = tDestination,
    };


    const uint32_t uBlockCount = pl_sb_size(gptStageCtx->sbtBlocks);

    bool bBlockFound = false;

    for(uint32_t i = 0; i < uBlockCount; i++)
    {
        plStageBlock* ptBlock = &gptStageCtx->sbtBlocks[i];

        if(ptBlock->uSize - ptBlock->uCurrentOffset > uSize)
        {
            tRequest.uBufferIndex = i;
            tRequest.uOffset = ptBlock->uCurrentOffset;
            bBlockFound = true;
            break;
        }
    }

    // check if first run
    if(!bBlockFound)
    {
        plBufferDesc tStagingBufferDesc = {
            .pcDebugName = "staging buffer",
            .szByteSize  = pl_max(gptGpuAllocators->get_buddy_block_size(), uSize),
            .tUsage      = PL_BUFFER_USAGE_TRANSFER_SOURCE
        };
        plBuffer* ptBuffer = NULL;
        plBufferHandle tBuffer = gptGfx->create_buffer(gptStageCtx->ptDevice, &tStagingBufferDesc, &ptBuffer);

        // allocate memory
        const plDeviceMemoryAllocation tAllocation = gptStageCtx->ptAllocator->allocate(gptStageCtx->ptAllocator->ptInst, 
            ptBuffer->tMemoryRequirements.uMemoryTypeBits,
            ptBuffer->tMemoryRequirements.ulSize,
            ptBuffer->tMemoryRequirements.ulAlignment,
            "staging buffer memory");

        // bind memory
        gptGfx->bind_buffer_to_memory(gptStageCtx->ptDevice, tBuffer, &tAllocation);

        tRequest.uBufferIndex = pl_sb_size(gptStageCtx->sbtBlocks);

        plStageBlock tBlock = {
            .tBuffer = tBuffer,
            .uSize = tStagingBufferDesc.szByteSize
        };
        pl_sb_push(gptStageCtx->sbtBlocks, tBlock);
    }
    gptStageCtx->sbtBlocks[tRequest.uBufferIndex].uCurrentOffset += uSize;

    plBuffer* ptBuffer = gptGfx->get_buffer(gptStageCtx->ptDevice, gptStageCtx->sbtBlocks[tRequest.uBufferIndex].tBuffer);
    uint8_t* pucData = (uint8_t*)&ptBuffer->tMemoryAllocation.pHostMapped[tRequest.uOffset];
    memcpy(pucData, pData, uSize);

    gptGfx->copy_buffer(gptStageCtx->ptCurrentEncoder,
        gptStageCtx->sbtBlocks[tRequest.uBufferIndex].tBuffer,
        tRequest.uDestinationBuffer,
        tRequest.uOffset,
        tRequest.uDestinationOffset,
        tRequest.uSize);

    return gptStageCtx->uNextValue;
}

uint64_t
pl_stage_queue_texture_upload(plTextureHandle tDestination, const plBufferImageCopy* ptCopy, const void* pData, uint64_t uSize, bool bGenerateMips)
{

    if(pData == NULL)
        return 0;

    gptStageCtx->uLastUsedFrame = gptIOI->get_io()->ulFrameCount;

    if(gptStageCtx->ptCurrentCommandBuffer == NULL)
    {
        const plBeginCommandInfo tSceneBeginInfo = {
            .uWaitSemaphoreCount   = 1,
            .atWaitSempahores      = {gptStageCtx->ptSemaphore},
            .auWaitSemaphoreValues = {gptStageCtx->uNextValue++},
        };

        gptStageCtx->ptCurrentCommandBuffer = gptGfx->request_command_buffer(gptStageCtx->ptCmdPool, "stage");
        gptGfx->begin_command_recording(gptStageCtx->ptCurrentCommandBuffer, &tSceneBeginInfo);

        gptStageCtx->ptCurrentEncoder = gptGfx->begin_blit_pass(gptStageCtx->ptCurrentCommandBuffer);
        gptGfx->pipeline_barrier_blit(gptStageCtx->ptCurrentEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);
    }

    plStageTextureUploadRequest tRequest = {
        .uSize               = uSize,
        .tBufferImageCopy    = *ptCopy,
        .uDestinationTexture = tDestination,
        .bGenerateMips       = bGenerateMips
    };


    const uint32_t uBlockCount = pl_sb_size(gptStageCtx->sbtBlocks);

    bool bBlockFound = false;

    for(uint32_t i = 0; i < uBlockCount; i++)
    {
        plStageBlock* ptBlock = &gptStageCtx->sbtBlocks[i];

        if(ptBlock->uSize - ptBlock->uCurrentOffset >= uSize)
        {
            tRequest.uBufferIndex = i;
            tRequest.uOffset = ptBlock->uCurrentOffset;
            bBlockFound = true;
            break;
        }
    }

    // check if first run
    if(!bBlockFound)
    {
        plBufferDesc tStagingBufferDesc = {
            .pcDebugName = "staging buffer",
            .szByteSize  = pl_max(gptGpuAllocators->get_buddy_block_size(), uSize),
            .tUsage      = PL_BUFFER_USAGE_TRANSFER_SOURCE
        };
        plBuffer* ptBuffer = NULL;
        plBufferHandle tBuffer = gptGfx->create_buffer(gptStageCtx->ptDevice, &tStagingBufferDesc, &ptBuffer);

        // allocate memory
        const plDeviceMemoryAllocation tAllocation = gptStageCtx->ptAllocator->allocate(gptStageCtx->ptAllocator->ptInst, 
            ptBuffer->tMemoryRequirements.uMemoryTypeBits,
            ptBuffer->tMemoryRequirements.ulSize,
            ptBuffer->tMemoryRequirements.ulAlignment,
            "staging buffer memory");

        // bind memory
        gptGfx->bind_buffer_to_memory(gptStageCtx->ptDevice, tBuffer, &tAllocation);

        tRequest.uBufferIndex = pl_sb_size(gptStageCtx->sbtBlocks);

        plStageBlock tBlock = {
            .tBuffer = tBuffer,
            .uSize = tStagingBufferDesc.szByteSize
        };
        pl_sb_push(gptStageCtx->sbtBlocks, tBlock);
    }
    gptStageCtx->sbtBlocks[tRequest.uBufferIndex].uCurrentOffset += uSize;

    plBuffer* ptBuffer = gptGfx->get_buffer(gptStageCtx->ptDevice, gptStageCtx->sbtBlocks[tRequest.uBufferIndex].tBuffer);
    uint8_t* pucData = (uint8_t*)&ptBuffer->tMemoryAllocation.pHostMapped[tRequest.uOffset];
    memcpy(pucData, pData, uSize);

    gptGfx->copy_buffer_to_texture(gptStageCtx->ptCurrentEncoder,
        gptStageCtx->sbtBlocks[tRequest.uBufferIndex].tBuffer,
        tRequest.uDestinationTexture,
        1,
        &tRequest.tBufferImageCopy);
    if(tRequest.bGenerateMips)
        gptGfx->generate_mipmaps(gptStageCtx->ptCurrentEncoder, tRequest.uDestinationTexture);

    return gptStageCtx->uNextValue;
}

void
pl_stage_new_frame(void)
{
    if(gptStageCtx->ptCurrentCommandBuffer)
    {
        gptGfx->pipeline_barrier_blit(gptStageCtx->ptCurrentEncoder, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
        gptGfx->end_blit_pass(gptStageCtx->ptCurrentEncoder);
        gptGfx->end_command_recording(gptStageCtx->ptCurrentCommandBuffer);

        const plSubmitInfo tStageSubmitInfo = {
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {gptStageCtx->ptSemaphore},
            .auSignalSemaphoreValues = {gptStageCtx->uNextValue}
        };
        gptGfx->submit_command_buffer(gptStageCtx->ptCurrentCommandBuffer, &tStageSubmitInfo);
        gptGfx->return_command_buffer(gptStageCtx->ptCurrentCommandBuffer);

        gptStageCtx->ptCurrentCommandBuffer = NULL;
        gptStageCtx->ptCurrentEncoder = NULL;
    }

    uint64_t uSemValue = gptGfx->get_semaphore_value(gptStageCtx->ptDevice, gptStageCtx->ptSemaphore);
    if(uSemValue >= gptStageCtx->uNextValue)
    {
        const uint32_t uBlockCount = pl_sb_size(gptStageCtx->sbtBlocks);
        for(uint32_t i = 0; i < uBlockCount; i++)
        {
            gptStageCtx->sbtBlocks[i].uCurrentOffset = 0;
        }
    }

    if(gptIOI->get_io()->ulFrameCount - gptStageCtx->uLastUsedFrame > 30)
    {
        const uint32_t uBlockCount = pl_sb_size(gptStageCtx->sbtBlocks);
        for(uint32_t i = 0; i < uBlockCount; i++)
        {
            gptGfx->queue_buffer_for_deletion(gptStageCtx->ptDevice, gptStageCtx->sbtBlocks[i].tBuffer);
        }
        pl_sb_reset(gptStageCtx->sbtBlocks);
    }
}

bool
pl_stage_completed(uint64_t uValue)
{
    uint64_t uSemValue = gptGfx->get_semaphore_value(gptStageCtx->ptDevice, gptStageCtx->ptSemaphore);
    return uSemValue >= uValue;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_stage_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plStageI tApi = {
        .initialize = pl_stage_initialize,
        .cleanup    = pl_stage_cleanup,
        .new_frame = pl_stage_new_frame,
        .stage_buffer_upload = pl_stage_stage_buffer_upload,
        .stage_texture_upload = pl_stage_stage_texture_upload,
        .flush        = pl_stage_flush,
        .queue_buffer_upload = pl_stage_queue_buffer_upload,
        .queue_texture_upload = pl_stage_queue_texture_upload,
        .completed = pl_stage_completed
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

PL_EXPORT void
pl_unload_stage_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    if(bReload)
        return;

    const plStageI* ptApi = pl_get_api_latest(ptApiRegistry, plStageI);
    ptApiRegistry->remove_api(ptApi);
}
