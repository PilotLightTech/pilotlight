/*
   pl_graphics_cpu.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] defines
// [SECTION] internal structs
// [SECTION] opaque structs
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] drawing
// [SECTION] internal api implementation
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.h"
#include "pl_string.h"
#include "pl_memory.h"
#include "pl_graphics_internal.h"

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plInternalDeviceAllocatorData
{
    plDevice*                 ptDevice;
    plDeviceMemoryAllocatorI* ptAllocator;
} plInternalDeviceAllocatorData;

typedef struct _plRenderPassCommonData
{
    uint32_t uDependencyCount;
    uint32_t uColorAttachmentCount;
} plRenderPassCommonData;

typedef struct _plCpuDynamicBuffer
{
    uint32_t                 uHandle;
    // VkBuffer                 tBuffer;
    plDeviceMemoryAllocation tMemory;
    // VkDescriptorSet          tDescriptorSet;
} plCputDynamicBuffer;

typedef struct _plCpuBuffer
{
    char*         pcData;
} plCpuBuffer;

typedef struct _plCpuTexture
{
    // VkImage     tImage;
    // VkImageView tImageView;
    bool        bOriginalView; // so we only free if original
} plCpuTexture;

typedef struct _plCpuRenderPassLayout
{
    int a;
} plCpuRenderPassLayout;

typedef struct _plCpuRenderPass
{
    // VkFramebuffer atFrameBuffers[6];
    int a;
} plCpuRenderPass;

typedef struct _plCpuBindGroupLayout
{
    int a;
} plCpuBindGroupLayout;

typedef struct _plCpuBindGroup
{
    // VkDescriptorPool      tPool; // owning pool
    bool                  bResetable;
    // VkDescriptorSet       tDescriptorSet;
    // VkDescriptorSetLayout tDescriptorSetLayout;
} plCpuBindGroup;

typedef struct _plCpuShader
{
    // VkPipelineLayout         tPipelineLayout;
    // VkPipeline               tPipeline;
    // VkSampleCountFlagBits    tMSAASampleCount;
    // VkShaderModule           tPixelShaderModule;
    // VkShaderModule           tVertexShaderModule;
    // VkDescriptorSetLayout    atDescriptorSetLayouts[4];
    // VkSpecializationMapEntry atSpecializationEntries[PL_MAX_SHADER_SPECIALIZATION_CONSTANTS];
    size_t                   szSpecializationSize;
} plCpuShader;

typedef struct _plCpuComputeShader
{
    // VkPipelineLayout         tPipelineLayout;
    // VkPipeline               tPipeline;
    // VkShaderModule           tShaderModule;
    // VkSpecializationMapEntry atSpecializationEntries[PL_MAX_SHADER_SPECIALIZATION_CONSTANTS];
    size_t                   szSpecializationSize;
    // VkDescriptorSetLayout    atDescriptorSetLayouts[4];
} plCpuComputeShader;

//-----------------------------------------------------------------------------
// [SECTION] opaque structs
//-----------------------------------------------------------------------------

typedef struct _plCommandBuffer
{
    plDevice*          ptDevice; // for convience
    plCommandPool*     ptPool; // parent pool
    plCommandBuffer*   ptNext; // for linked list
} plCommandBuffer;

typedef struct _plCommandPool
{
    plDevice*        ptDevice; // for convience
    plCommandBuffer* ptCommandBufferFreeList;  // free list of command buffers
} plCommandPool;

typedef struct _plBindGroupPool
{
    plDevice*           ptDevice; // for convience
    plBindGroupPoolDesc tDesc;
} plBindGroupPool;

typedef struct _plTimelineSemaphore
{
    plDevice*            ptDevice; // for convience
    plTimelineSemaphore* ptNext; // for linked list
} plTimelineSemaphore;

typedef struct _plFrameContext
{
    // VkSemaphore    tRenderFinish;
    // VkFence        tInFlight;
    // VkFramebuffer* sbtRawFrameBuffers;

    // dynamic buffer stuff
    uint16_t               uCurrentBufferIndex;
    uint64_t               uNextValue;
    // plCpuDynamicBuffer* sbtDynamicBuffers;
} plFrameContext;

typedef struct _plDevice
{
    // general
    plDeviceInit              tInit;
    plDeviceInfo              tInfo;
    plFrameGarbage*           sbtGarbage;
    plFrameContext*           sbtFrames;
    plDeviceMemoryAllocatorI* ptDynamicAllocator;
    
    // timeline semaphore free list
    plTimelineSemaphore* ptSemaphoreFreeList;

    // shader generation pool
    plCpuShader* sbtShadersHot;
    plShader*    sbtShadersCold;
    uint16_t*    sbtShaderFreeIndices;

    // compute shader generation pool
    plCpuComputeShader* sbtComputeShadersHot;
    plComputeShader*    sbtComputeShadersCold;
    uint16_t*           sbtComputeShaderFreeIndices;

    // buffer generation pool
    plCpuBuffer* sbtBuffersHot;
    plBuffer*    sbtBuffersCold;
    uint16_t*    sbtBufferFreeIndices;

    // texture generation pool
    // VkImageView*     sbtTextureViewsHot;
    plCpuTexture* sbtTexturesHot;
    plTexture*    sbtTexturesCold;
    uint16_t*     sbtTextureFreeIndices;

    // sampler generation pool
    int*       sbtSamplersHot;
    plSampler* sbtSamplersCold;
    uint16_t*  sbtSamplerFreeIndices;

    // bind group generation pool
    plCpuBindGroup* sbtBindGroupsHot;
    plBindGroup*    sbtBindGroupsCold;
    uint16_t*       sbtBindGroupFreeIndices;

    // bind group layout generation pool
    plCpuBindGroupLayout* sbtBindGroupLayoutsHot;
    plBindGroupLayout*    sbtBindGroupLayoutsCold;
    uint16_t*             sbtBindGroupLayoutFreeIndices;

    // memory blocks
    plDeviceMemoryAllocation* sbtMemoryBlocks;

    plStackedBarrier* sbtBarrierStack;
} plDevice;

typedef struct _plGraphics
{
    // general
    uint32_t uCurrentFrameIndex;
    uint32_t uFramesInFlight;
    size_t   szLocalMemoryInUse;
    size_t   szHostMemoryInUse;
    bool     bValidationActive;
    bool     bDebugMessengerActive;

    // free lists
    plCommandBuffer*  ptCommandBufferFreeList;
    
    // specifics
    plTempAllocator tTempAllocator;
} plGraphics;

typedef struct _plSurface
{
    int a;
} plSurface;

typedef struct _plSwapchain
{
    plDevice*        ptDevice; // for convience
    plSwapchainInfo  tInfo;
    // VkSemaphore      atImageAvailable[PL_MAX_FRAMES_IN_FLIGHT];
    uint32_t         uImageCount;
    plTextureHandle* sbtSwapchainTextureViews;
    uint32_t         uCurrentImageIndex; // current image to use within the swap chain

    // platform specific
    plSurface* ptSurface;
    // VkImage             atImages[8];
    // VkSurfaceFormatKHR* sbtSurfaceFormats;
} plSwapchain;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

plTimelineSemaphore*
pl_graphics_create_semaphore(plDevice* ptDevice, bool bHostVisible)
{
    plTimelineSemaphore* ptSemaphore = pl__get_new_semaphore(ptDevice);
    return ptSemaphore;
}

void
pl_graphics_cleanup_semaphore(plTimelineSemaphore* ptSemaphore)
{
}

void
pl_graphics_signal_semaphore(plDevice* ptDevice, plTimelineSemaphore* ptSemaphore, uint64_t ulValue)
{
}

void
pl_graphics_wait_semaphore(plDevice* ptDevice, plTimelineSemaphore* ptSemaphore, uint64_t ulValue)
{
}

uint64_t
pl_graphics_get_semaphore_value(plDevice* ptDevice, plTimelineSemaphore* ptSemaphore)
{
    uint64_t ulValue = 0;
    return ulValue;
}

plBufferHandle
pl_graphics_create_buffer(plDevice* ptDevice, const plBufferDesc* ptDesc, plBuffer **ptBufferOut)
{
    plBufferHandle tHandle = pl__get_new_buffer_handle(ptDevice);
    plBuffer* ptBuffer = pl_graphics_get_buffer(ptDevice, tHandle);
    ptBuffer->tDesc = *ptDesc;

    if(ptDesc->pcDebugName == NULL)
    {
        ptBuffer->tDesc.pcDebugName = "unnamed buffer";
    }

    plCpuBuffer tCPUBuffer = {0};

    ptBuffer->tMemoryRequirements.ulSize = ptDesc->szByteSize;
    ptBuffer->tMemoryRequirements.ulAlignment = 0;
    ptBuffer->tMemoryRequirements.uMemoryTypeBits = 0;

    ptDevice->sbtBuffersHot[tHandle.uIndex] = tCPUBuffer;

    if(ptBufferOut)
    {
        *ptBufferOut = &ptDevice->sbtBuffersCold[tHandle.uIndex];
    }

    return tHandle;
}

void
pl_graphics_bind_buffer_to_memory(plDevice* ptDevice, plBufferHandle tHandle, const plDeviceMemoryAllocation* ptAllocation)
{   
    plBuffer* ptBuffer = &ptDevice->sbtBuffersCold[tHandle.uIndex];
    ptBuffer->tMemoryAllocation = *ptAllocation;
    plCpuBuffer* ptCPUBuffer = &ptDevice->sbtBuffersHot[tHandle.uIndex];

    ptDevice->sbtBuffersHot[tHandle.uIndex].pcData = PL_ALLOC(sizeof(ptDevice->sbtBuffersCold[tHandle.uIndex].tMemoryRequirements.ulSize));
    memset(ptDevice->sbtBuffersHot[tHandle.uIndex].pcData, 0, sizeof(ptDevice->sbtBuffersCold[tHandle.uIndex].tMemoryRequirements.ulSize));

    ptCPUBuffer->pcData = ptAllocation->pHostMapped;

}

void
pl_graphics_reset_dynamic_data_blocks(plDevice* ptDevice)
{
}

plDynamicDataBlock
pl_graphics_allocate_dynamic_data_block(plDevice* ptDevice)
{
    plDynamicDataBlock tBlock = {0};
    return tBlock;
}

void
pl_graphics_copy_texture_to_buffer(plCommandBuffer* ptEncoder, plTextureHandle tTextureHandle, plBufferHandle tBufferHandle, uint32_t uRegionCount, const plBufferImageCopy* ptRegions)
{

}

void
pl_graphics_copy_texture(plCommandBuffer* ptEncoder, plTextureHandle tSrcHandle, plTextureHandle tDstHandle, uint32_t uRegionCount, const plImageCopy* ptRegions)
{
}

void
pl_graphics_generate_mipmaps(plCommandBuffer* ptEncoder, plTextureHandle tTexture)
{
}

plSamplerHandle
pl_graphics_create_sampler(plDevice* ptDevice, const plSamplerDesc* ptDesc)
{
    plSamplerHandle tHandle = pl__get_new_sampler_handle(ptDevice);
    return tHandle;
}

plBindGroupHandle
pl_graphics_create_bind_group(plDevice* ptDevice, const plBindGroupDesc* ptDesc)
{
    plBindGroupHandle tHandle = pl__get_new_bind_group_handle(ptDevice);
    return tHandle;
}

plBindGroupLayoutHandle
pl_graphics_create_bind_group_layout(plDevice* ptDevice, const plBindGroupLayoutDesc* ptDesc)
{
    plBindGroupLayoutHandle tHandle = pl__get_new_bind_group_layout_handle(ptDevice);
    return tHandle;
}

void
pl_graphics_update_bind_group(plDevice* ptDevice, plBindGroupHandle tHandle, const plBindGroupUpdateData* ptData)
{
}

plTextureHandle
pl_graphics_create_texture(plDevice* ptDevice, const plTextureDesc* ptDesc, plTexture **ptTextureOut)
{
    plTextureHandle tHandle = pl__get_new_texture_handle(ptDevice);
    return tHandle;
}

void
pl_graphics_bind_texture_to_memory(plDevice* ptDevice, plTextureHandle tHandle, const plDeviceMemoryAllocation* ptAllocation)
{
}

plTextureHandle
pl_graphics_create_texture_view(plDevice* ptDevice, const plTextureViewDesc* ptViewDesc)
{
    plTextureHandle tHandle = pl__get_new_texture_handle(ptDevice);
    return tHandle;
}

plComputeShaderHandle
pl_graphics_create_compute_shader(plDevice* ptDevice, const plComputeShaderDesc* ptDescription)
{

    plComputeShaderHandle tHandle = pl__get_new_compute_shader_handle(ptDevice);
    return tHandle;
}

plShaderHandle
pl_graphics_create_shader(plDevice* ptDevice, const plShaderDesc* ptDescription)
{
    plShaderHandle tHandle = pl__get_new_shader_handle(ptDevice);
    return tHandle;
}

plTextureHandle*
pl_graphics_get_swapchain_images(plSwapchain* ptSwap, uint32_t* puSizeOut)
{
    return ptSwap->sbtSwapchainTextureViews;
}

void
pl_graphics_begin_command_recording(plCommandBuffer* ptCommandBuffer)
{
}

void
pl_graphics_begin_render_pass(plCommandBuffer* ptCmdBuffer, const plRenderInfo* ptInfo, const plPassResources* ptResource)
{
}

void
pl_graphics_end_render_pass(plCommandBuffer* ptEncoder)
{
}

void
pl_graphics_bind_vertex_buffers(plCommandBuffer* ptEncoder, uint32_t uFirst, uint32_t uCount, const plBufferHandle* ptHandles, const size_t* pszOffsets)
{
}

void
pl_graphics_bind_vertex_buffer(plCommandBuffer* ptEncoder, plBufferHandle tHandle)
{
}

void
pl_graphics_draw(plCommandBuffer* ptEncoder, uint32_t uCount, const plDraw *atDraws)
{
}

void
pl_graphics_draw_indexed(plCommandBuffer* ptEncoder, uint32_t uCount, const plDrawIndex *atDraws)
{
}

void
pl_graphics_bind_shader(plCommandBuffer* ptEncoder, plShaderHandle tHandle)
{
}

void
pl_graphics_bind_compute_shader(plCommandBuffer* ptEncoder, plComputeShaderHandle tHandle)
{
}

void
pl_graphics_draw_stream(plCommandBuffer* ptEncoder, uint32_t uAreaCount, plDrawArea *atAreas)
{

}

void
pl_graphics_set_depth_bias(plCommandBuffer* ptEncoder, float fDepthBiasConstantFactor, float fDepthBiasClamp, float fDepthBiasSlopeFactor)
{
}

void
pl_graphics_set_viewport(plCommandBuffer* ptEncoder, const plRenderViewport* ptViewport)
{
}

void
pl_graphics_set_scissor_region(plCommandBuffer* ptEncoder, const plScissor* ptScissor)
{
}

plDeviceMemoryAllocation
pl_graphics_allocate_memory(plDevice* ptDevice, size_t szSize, plMemoryFlags tMemoryFlags, uint32_t uTypeFilter, const char* pcName)
{
    plDeviceMemoryAllocation tBlock = {0};
    return tBlock;
}

void
pl_graphics_free_memory(plDevice* ptDevice, plDeviceMemoryAllocation* ptBlock)
{
}

bool
pl_graphics_flush_memory(plDevice* ptDevice, uint32_t uRangeCount, const plDeviceMemoryRange* atRanges)
{
    return true;
}

bool
pl_graphics_invalidate_memory(plDevice* ptDevice, uint32_t uRangeCount, const plDeviceMemoryRange* atRanges)
{
    return true;
}

plGraphicsBackend
pl_graphics_get_backend(void)
{
    return PL_GRAPHICS_BACKEND_CPU;
}

const char*
pl_graphics_get_backend_string(void)
{
    return "CPU Rasterizer";
}

bool
pl_graphics_initialize(const plGraphicsInit* ptDesc)
{
    static plGraphics gtGraphics = {0};
    gptGraphics = &gtGraphics;

    // setup logging
    plLogExtChannelInit tLogInit = {
        .tType       = PL_LOG_CHANNEL_TYPE_BUFFER | PL_LOG_CHANNEL_TYPE_CONSOLE,
        .uEntryCount = 1024
    };
    uLogChannelGraphics = gptLog->add_channel("Graphics", tLogInit);
    uint32_t uLogLevel = PL_LOG_LEVEL_INFO;
    #ifdef PL_CONFIG_DEBUG
        uLogLevel = PL_LOG_LEVEL_DEBUG;
    #endif
    gptLog->set_level(uLogChannelGraphics, uLogLevel);

    // save context for hot-reloads
    gptDataRegistry->set_data("plGraphics", gptGraphics);

    gptGraphics->bValidationActive = ptDesc->eFlags & PL_GRAPHICS_INIT_FLAGS_VALIDATION_ENABLED;

    gptGraphics->bDebugMessengerActive = gptGraphics->bValidationActive;

    // set frames in flight (if zero, use a default of 2)
    gptGraphics->uFramesInFlight = pl_min(pl_max(ptDesc->uFramesInFlight, 2), PL_MAX_FRAMES_IN_FLIGHT);

    return true;
}

void
pl_graphics_enumerate_devices(plDeviceInfo *atDeviceInfo, uint32_t* puDeviceCount)
{
    *puDeviceCount = 1;

    if(atDeviceInfo == NULL)
        return;

    plIO* ptIOCtx = gptIOI->get_io();

    strncpy(atDeviceInfo[0].acName, "PL CPU Rasterizer", 256);
    atDeviceInfo[0].eVendorId = PL_VENDOR_ID_NONE;
    atDeviceInfo[0].eType = PL_DEVICE_TYPE_CPU;
    atDeviceInfo[0].eCapabilities = PL_DEVICE_CAPABILITY_BIND_GROUP_INDEXING | PL_DEVICE_CAPABILITY_SAMPLER_ANISOTROPY | PL_DEVICE_CAPABILITY_SWAPCHAIN | PL_DEVICE_CAPABILITY_MULTIPLE_VIEWPORTS;
}

plDevice*
pl_graphics_create_device(const plDeviceInit* ptInit)
{

    plDevice* ptDevice = PL_ALLOC(sizeof(plDevice));
    memset(ptDevice, 0, sizeof(plDevice));
    ptDevice->tInit = *ptInit;

    pl_sb_add(ptDevice->sbtShadersHot);
    pl_sb_add(ptDevice->sbtComputeShadersHot);
    pl_sb_add(ptDevice->sbtBuffersHot);
    // pl_sb_add(ptDevice->sbtTextureViewsHot);
    pl_sb_add(ptDevice->sbtTexturesHot);
    pl_sb_add(ptDevice->sbtSamplersHot);
    pl_sb_add(ptDevice->sbtBindGroupsHot);
    pl_sb_add(ptDevice->sbtBindGroupLayoutsHot);
    
    pl_sb_add(ptDevice->sbtShadersCold);
    pl_sb_add(ptDevice->sbtComputeShadersCold);
    pl_sb_add(ptDevice->sbtBuffersCold);
    pl_sb_add(ptDevice->sbtTexturesCold);
    pl_sb_add(ptDevice->sbtSamplersCold);
    pl_sb_add(ptDevice->sbtBindGroupsCold);
    pl_sb_add(ptDevice->sbtBindGroupLayoutsCold);

    pl_sb_back(ptDevice->sbtShadersCold)._uGeneration = 1;
    pl_sb_back(ptDevice->sbtComputeShadersCold)._uGeneration = 1;
    pl_sb_back(ptDevice->sbtBuffersCold)._uGeneration = 1;
    pl_sb_back(ptDevice->sbtTexturesCold)._uGeneration = 1;
    pl_sb_back(ptDevice->sbtSamplersCold)._uGeneration = 1;
    pl_sb_back(ptDevice->sbtBindGroupsCold)._uGeneration = 1;
    pl_sb_back(ptDevice->sbtBindGroupLayoutsCold)._uGeneration = 1;

    
    return ptDevice;
}

plSurface*
pl_graphics_create_surface(plWindow* ptWindow)
{
    plSurface* ptSurface = PL_ALLOC(sizeof(plSurface));
    memset(ptSurface, 0, sizeof(plSurface));
    return ptSurface;
}

plSwapchain*
pl_graphics_create_swapchain(plDevice* ptDevice, plSurface* ptSurface, const plSwapchainInit* ptInit)
{

    plSwapchain* ptSwap = PL_ALLOC(sizeof(plSwapchain));
    memset(ptSwap, 0, sizeof(plSwapchain));
    return ptSwap;
}

void
pl_graphics_begin_frame(plDevice* ptDevice)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);
    pl__garbage_collect(ptDevice);
    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

bool
pl_graphics_acquire_swapchain_image(plSwapchain* ptSwap)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);
    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
    return true;
}

void
pl_graphics_end_command_recording(plCommandBuffer* ptCommandBuffer)
{
}

bool
pl_graphics_present(plCommandBuffer* ptCmdBuffer, const plSubmitInfo* ptSubmitInfo, plSwapchain **ptSwaps, uint32_t uSwapchainCount)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);
    gptGraphics->uCurrentFrameIndex = (gptGraphics->uCurrentFrameIndex + 1) % gptGraphics->uFramesInFlight;
    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
    return true;
}

void
pl_graphics_recreate_swapchain(plSwapchain* ptSwap, const plSwapchainInit* ptInit)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);
    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

void
pl_graphics_flush_device(plDevice* ptDevice)
{
}

void
pl_graphics_cleanup(void)
{
    pl__cleanup_common_graphics();
}

void
pl_graphics_cleanup_surface(plSurface* ptSurface)
{
    PL_FREE(ptSurface);
}

void
pl_graphics_cleanup_swapchain(plSwapchain* ptSwap)
{
    pl__cleanup_common_swapchain(ptSwap);
}

void
pl_graphics_cleanup_device(plDevice* ptDevice)
{
    pl__cleanup_common_device(ptDevice);
}

void
pl_graphics_begin_compute_pass(plCommandBuffer* ptCmdBuffer, const plPassResources* ptResources)
{
}

void
pl_graphics_end_compute_pass(plCommandBuffer* ptEncoder)
{
}

void
pl_graphics_dispatch(plCommandBuffer* ptEncoder, uint32_t uDispatchCount, const plDispatch *atDispatches)
{
}

void
pl_graphics_bind_compute_bind_groups(plCommandBuffer* ptEncoder, plComputeShaderHandle tHandle, uint32_t uFirst,
    uint32_t uCount, const plBindGroupHandle *atBindGroups, uint32_t uDynamicBindingCount, const plDynamicBinding* ptDynamicBinding)
{
}

void
pl_graphics_bind_graphics_bind_groups(plCommandBuffer* ptEncoder, plShaderHandle tHandle, uint32_t uFirst, uint32_t uCount, const plBindGroupHandle *atBindGroups, uint32_t uDynamicBindingCount, const plDynamicBinding* ptDynamicBinding)
{
}

void
pl_graphics_submit_command_buffer(plCommandBuffer* ptCmdBuffer, const plSubmitInfo* ptSubmitInfo)
{
}

void
pl_graphics_wait_on_command_buffer(plCommandBuffer* ptCmdBuffer)
{
}

void
pl_graphics_return_command_buffer(plCommandBuffer* ptCmdBuffer)
{
}

plBindGroupPool*
pl_graphics_create_bind_group_pool(plDevice* ptDevice, const plBindGroupPoolDesc* ptDesc)
{
    plBindGroupPool* ptPool = PL_ALLOC(sizeof(plBindGroupPool));
    memset(ptPool, 0, sizeof(plBindGroupPool));
    return ptPool;
}

void
pl_graphics_reset_bind_group_pool(plBindGroupPool* ptPool)
{
}

void
pl_graphics_cleanup_bind_group_pool(plBindGroupPool* ptPool)
{
    PL_FREE(ptPool);
}

plCommandPool *
pl_graphics_create_command_pool(plDevice* ptDevice, const plCommandPoolDesc* ptDesc)
{
    plCommandPool* ptPool = PL_ALLOC(sizeof(plCommandPool));
    memset(ptPool, 0, sizeof(plCommandPool));
    return ptPool;
}

void
pl_graphics_cleanup_command_pool(plCommandPool* ptPool)
{
    PL_FREE(ptPool);
}

void
pl_graphics_reset_command_pool(plCommandPool* ptPool, plCommandPoolResetFlags tFlags)
{
}

void
pl_graphics_reset_command_buffer(plCommandBuffer* ptCommandBuffer)
{
}

plCommandBuffer*
pl_graphics_request_command_buffer(plCommandPool* ptPool, const char* pcDebugName)
{
    plCommandBuffer* ptCommandBuffer = ptPool->ptCommandBufferFreeList;
    if (ptCommandBuffer)
    {
        ptPool->ptCommandBufferFreeList = ptCommandBuffer->ptNext;
    }
    else
    {
        ptCommandBuffer = PL_ALLOC(sizeof(plCommandBuffer));
        memset(ptCommandBuffer, 0, sizeof(plCommandBuffer));
    }
    return ptCommandBuffer;
}

void
pl_graphics_copy_buffer(plCommandBuffer* ptEncoder, plBufferHandle tSource, plBufferHandle tDestination, uint64_t uSourceOffset, uint64_t uDestinationOffset, size_t szSize)
{
}

void
pl_graphics_copy_buffer_to_texture(plCommandBuffer* ptEncoder, plBufferHandle tBufferHandle, plTextureHandle tTextureHandle, uint32_t uRegionCount, const plBufferImageCopy* ptRegions)
{
}

void
pl_graphics_destroy_buffer(plDevice* ptDevice, plBufferHandle tHandle)
{
    PL_LOG_TRACE_API_F(gptLog, uLogChannelGraphics, "destroy buffer %u immediately", tHandle.uIndex);
    ptDevice->sbtBuffersCold[tHandle.uIndex]._uGeneration++;
    pl_sb_push(ptDevice->sbtBufferFreeIndices, tHandle.uIndex);

    PL_FREE(ptDevice->sbtBuffersHot[tHandle.uIndex].pcData);
}

void
pl_graphics_destroy_texture(plDevice* ptDevice, plTextureHandle tHandle)
{
}

void
pl_graphics_destroy_sampler(plDevice* ptDevice, plSamplerHandle tHandle)
{
}

void
pl_graphics_destroy_bind_group(plDevice* ptDevice, plBindGroupHandle tHandle)
{
}

void
pl_graphics_destroy_bind_group_layout(plDevice* ptDevice, plBindGroupLayoutHandle tHandle)
{
}

void
pl_graphics_destroy_render_pass(plDevice* ptDevice, plRenderPassHandle tHandle)
{
}

void
pl_graphics_destroy_render_pass_layout(plDevice* ptDevice, plRenderPassLayoutHandle tHandle)
{
}

void
pl_graphics_destroy_shader(plDevice* ptDevice, plShaderHandle tHandle)
{
}

void
pl_graphics_destroy_compute_shader(plDevice* ptDevice, plComputeShaderHandle tHandle)
{
}

void
pl_graphics_insert_debug_label(plCommandBuffer* ptCmdBuffer, const char* pcLabel, plVec4 tColor)
{

}

void
pl_graphics_push_debug_group(plCommandBuffer* ptCmdBuffer, const char* pcLabel, plVec4 tColor)
{
}

void
pl_graphics_pop_debug_group(plCommandBuffer* ptCmdBuffer)
{
}

void
pl_graphics_intra_pass_barrier(plCommandBuffer* ptCmdBuffer, plPipelineStageFlags tSrcStages, plPipelineStageFlags tDstStages, plBarrierScope tScope, const plPassResources* ptResources)
{
}

void
pl_graphics_consumer_barrier(plCommandBuffer* ptCmdBuffer, plPipelineStageFlags tSrcStages, plPipelineStageFlags tDstStages, plBarrierScope tScope)
{
}

void
pl_graphics_producer_barrier(plCommandBuffer* ptCmdBuffer, plPipelineStageFlags tSrcStages, plPipelineStageFlags tDstStages, plBarrierScope tScope)
{
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------


static void
pl__garbage_collect(plDevice* ptDevice)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);
    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptDevice);

    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptDevice);
    pl_sb_reset(ptGarbage->sbtTextures);
    pl_sb_reset(ptGarbage->sbtShaders);
    pl_sb_reset(ptGarbage->sbtComputeShaders);
    pl_sb_reset(ptGarbage->sbtRenderPasses);
    pl_sb_reset(ptGarbage->sbtRenderPassLayouts);
    pl_sb_reset(ptGarbage->sbtMemory);
    pl_sb_reset(ptGarbage->sbtBuffers);
    pl_sb_reset(ptGarbage->sbtBindGroups);
    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}
