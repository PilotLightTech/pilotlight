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
    uint32_t                uDependencyCount;
    uint32_t                uColorAttachmentCount;
    // VkAttachmentDescription atAttachments[PL_MAX_RENDER_TARGETS];
    // VkSubpassDependency     atSubpassDependencies[PL_MAX_SUBPASSES];
    // VkSubpassDescription    atSubpasses[PL_MAX_SUBPASSES];
    // VkAttachmentReference   atSubpassColorAttachmentReferences[PL_MAX_RENDER_TARGETS][PL_MAX_SUBPASSES];
    // VkAttachmentReference   atSubpassInputAttachmentReferences[PL_MAX_RENDER_TARGETS][PL_MAX_SUBPASSES];
    // VkAttachmentReference   tDepthAttachmentReference;
    // VkAttachmentReference   tResolveAttachmentReference;
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
    char* pcData;
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
    plBeginCommandInfo tBeginInfo;
    plCommandPool*     ptPool; // parent pool
    plCommandBuffer*   ptNext; // for linked list
} plCommandBuffer;

typedef struct _plRenderEncoder
{
    plCommandBuffer*   ptCommandBuffer;
    plRenderPassHandle tRenderPassHandle;
    uint32_t           uCurrentSubpass;
    plRenderEncoder*   ptNext; // for linked list
} plRenderEncoder;

typedef struct _plComputeEncoder
{
    plCommandBuffer*  ptCommandBuffer;
    plComputeEncoder* ptNext; // for linked list
} plComputeEncoder;

typedef struct _plBlitEncoder
{
    plCommandBuffer* ptCommandBuffer;
    plBlitEncoder*   ptNext; // for linked list
} plBlitEncoder;

typedef struct _plCommandPool
{
    plDevice*        ptDevice; // for convience
    // VkCommandBuffer* sbtReadyCommandBuffers;   // completed command buffers
    // VkCommandBuffer* sbtPendingCommandBuffers; // recently submitted command buffers
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

    // render pass layout generation pool
    plCpuRenderPassLayout* sbtRenderPassLayoutsHot;
    plRenderPassLayout*    sbtRenderPassLayoutsCold;
    uint16_t*              sbtRenderPassLayoutFreeIndices;

    // render pass generation pool
    plCpuRenderPass* sbtRenderPassesHot;
    plRenderPass*    sbtRenderPassesCold;
    uint16_t*        sbtRenderPassFreeIndices;

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
    plBuffer*       sbtBuffersCold;
    uint16_t*       sbtBufferFreeIndices;

    // texture generation pool
    // VkImageView*     sbtTextureViewsHot;
    plCpuTexture* sbtTexturesHot;
    plTexture*    sbtTexturesCold;
    uint16_t*     sbtTextureFreeIndices;

    // sampler generation pool
    // VkSampler* sbtSamplersHot;
    plSampler* sbtSamplersCold;
    uint16_t*  sbtSamplerFreeIndices;

    // bind group generation pool
    plCpuBindGroup* sbtBindGroupsHot;
    plBindGroup*       sbtBindGroupsCold;
    uint16_t*          sbtBindGroupFreeIndices;

    // bind group layout generation pool
    plCpuBindGroupLayout* sbtBindGroupLayouts;
    uint32_t*                sbtBindGroupLayoutFreeIndices;
    // VkDescriptorSetLayout    tDynamicDescriptorSetLayout;

    // vulkan specifics
    // VkDevice                         tLogicalDevice;
    // VkPhysicalDevice                 tPhysicalDevice;
    // VkPhysicalDeviceMemoryProperties tMemProps;
    int                              iGraphicsQueueFamily;
    int                              iPresentQueueFamily;
    // VkQueue                          tGraphicsQueue;
    // VkQueue                          tPresentQueue;
    // VkDescriptorPool                 tDynamicBufferDescriptorPool;

    // memory blocks
    plDeviceMemoryAllocation* sbtMemoryBlocks;
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
    plRenderEncoder*  ptRenderEncoderFreeList;
    plBlitEncoder*    ptBlitEncoderFreeList;
    plComputeEncoder* ptComputeEncoderFreeList;
    
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
pl_create_semaphore(plDevice* ptDevice, bool bHostVisible)
{
    plTimelineSemaphore* ptSemaphore = pl__get_new_semaphore(ptDevice);
    return ptSemaphore;
}

void
pl_cleanup_semaphore(plTimelineSemaphore* ptSemaphore)
{
}

void
pl_signal_semaphore(plDevice* ptDevice, plTimelineSemaphore* ptSemaphore, uint64_t ulValue)
{
}

void
pl_wait_semaphore(plDevice* ptDevice, plTimelineSemaphore* ptSemaphore, uint64_t ulValue)
{
}

uint64_t
pl_get_semaphore_value(plDevice* ptDevice, plTimelineSemaphore* ptSemaphore)
{
    uint64_t ulValue = 0;
    return ulValue;
}

plBufferHandle
pl_create_buffer(plDevice* ptDevice, const plBufferDesc* ptDesc, plBuffer **ptBufferOut)
{
    plBufferHandle tHandle = pl__get_new_buffer_handle(ptDevice);
    return tHandle;
}

void
pl_bind_buffer_to_memory(plDevice* ptDevice, plBufferHandle tHandle, const plDeviceMemoryAllocation* ptAllocation)
{
}

static plDynamicDataBlock
pl_allocate_dynamic_data_block(plDevice* ptDevice)
{
    plDynamicDataBlock tBlock = {0};
    return tBlock;
}

void
pl_copy_texture_to_buffer(plBlitEncoder* ptEncoder, plTextureHandle tTextureHandle, plBufferHandle tBufferHandle, uint32_t uRegionCount, const plBufferImageCopy* ptRegions)
{

}

void
pl_copy_texture(plBlitEncoder* ptEncoder, plTextureHandle tSrcHandle, plTextureHandle tDstHandle, uint32_t uRegionCount, const plImageCopy* ptRegions)
{
}

void
pl_generate_mipmaps(plBlitEncoder* ptEncoder, plTextureHandle tTexture)
{
}

plSamplerHandle
pl_create_sampler(plDevice* ptDevice, const plSamplerDesc* ptDesc)
{
    plSamplerHandle tHandle = pl__get_new_sampler_handle(ptDevice);
    return tHandle;
}

plBindGroupHandle
pl_create_bind_group(plDevice* ptDevice, const plBindGroupDesc* ptDesc)
{
    plBindGroupHandle tHandle = pl__get_new_bind_group_handle(ptDevice);
    return tHandle;
}

void
pl_update_bind_group(plDevice* ptDevice, plBindGroupHandle tHandle, const plBindGroupUpdateData* ptData)
{
}

plTextureHandle
pl_create_texture(plDevice* ptDevice, const plTextureDesc* ptDesc, plTexture **ptTextureOut)
{
    plTextureHandle tHandle = pl__get_new_texture_handle(ptDevice);
    return tHandle;
}

void
pl_set_texture_usage(plBlitEncoder* ptEncoder, plTextureHandle tHandle, plTextureUsage tNewUsage, plTextureUsage tOldUsage)
{
}

void
pl_bind_texture_to_memory(plDevice* ptDevice, plTextureHandle tHandle, const plDeviceMemoryAllocation* ptAllocation)
{
}

plTextureHandle
pl_create_texture_view(plDevice* ptDevice, const plTextureViewDesc* ptViewDesc)
{
    plTextureHandle tHandle = pl__get_new_texture_handle(ptDevice);
    return tHandle;
}

plComputeShaderHandle
pl_create_compute_shader(plDevice* ptDevice, const plComputeShaderDesc* ptDescription)
{

    plComputeShaderHandle tHandle = pl__get_new_compute_shader_handle(ptDevice);
    return tHandle;
}

plShaderHandle
pl_create_shader(plDevice* ptDevice, const plShaderDesc* ptDescription)
{
    plShaderHandle tHandle = pl__get_new_shader_handle(ptDevice);
    return tHandle;
}

plTextureHandle*
pl_get_swapchain_images(plSwapchain* ptSwap, uint32_t* puSizeOut)
{
    return ptSwap->sbtSwapchainTextureViews;
}

plRenderPassLayoutHandle
pl_create_render_pass_layout(plDevice* ptDevice, const plRenderPassLayoutDesc* ptDesc)
{
    const plRenderPassLayoutHandle tHandle = pl__get_new_render_pass_layout_handle(ptDevice);
    return tHandle;
}

plRenderPassHandle
pl_create_render_pass(plDevice* ptDevice, const plRenderPassDesc* ptDesc, const plRenderPassAttachments* ptAttachments)
{

    plRenderPassHandle tHandle = pl__get_new_render_pass_handle(ptDevice);
    return tHandle;
}

void
pl_update_render_pass_attachments(plDevice* ptDevice, plRenderPassHandle tHandle, plVec2 tDimensions, const plRenderPassAttachments* ptAttachments)
{
}

void
pl_begin_command_recording(plCommandBuffer* ptCommandBuffer, const plBeginCommandInfo* ptBeginInfo)
{
}

plRenderEncoder*
pl_begin_render_pass(plCommandBuffer* ptCmdBuffer, plRenderPassHandle tPass, const plPassResources* ptResource)
{
    plRenderEncoder* ptEncoder = pl__get_new_render_encoder();
    return ptEncoder;
}

void
pl_next_subpass(plRenderEncoder* ptEncoder, const plPassResources* ptResource)
{
}

void
pl_end_render_pass(plRenderEncoder* ptEncoder)
{
}

void
pl_bind_vertex_buffer(plRenderEncoder* ptEncoder, plBufferHandle tHandle)
{
}

void
pl_draw(plRenderEncoder* ptEncoder, uint32_t uCount, const plDraw *atDraws)
{
}

void
pl_draw_indexed(plRenderEncoder* ptEncoder, uint32_t uCount, const plDrawIndex *atDraws)
{
}

void
pl_bind_shader(plRenderEncoder* ptEncoder, plShaderHandle tHandle)
{
}

void
pl_bind_compute_shader(plComputeEncoder* ptEncoder, plComputeShaderHandle tHandle)
{
}

void
pl_draw_stream(plRenderEncoder* ptEncoder, uint32_t uAreaCount, plDrawArea *atAreas)
{

}

void
pl_set_depth_bias(plRenderEncoder* ptEncoder, float fDepthBiasConstantFactor, float fDepthBiasClamp, float fDepthBiasSlopeFactor)
{
}

void
pl_set_viewport(plRenderEncoder* ptEncoder, const plRenderViewport* ptViewport)
{
}

void
pl_set_scissor_region(plRenderEncoder* ptEncoder, const plScissor* ptScissor)
{
}

plDeviceMemoryAllocation
pl_allocate_memory(plDevice* ptDevice, size_t szSize, plMemoryMode tMemoryMode, uint32_t uTypeFilter, const char* pcName)
{
    plDeviceMemoryAllocation tBlock = {0};
    return tBlock;
}

void
pl_free_memory(plDevice* ptDevice, plDeviceMemoryAllocation* ptBlock)
{
}

plGraphicsBackend
pl_get_backend(void)
{
    return PL_GRAPHICS_BACKEND_CPU;
}

const char*
pl_get_backend_string(void)
{
    return "CPU Rasterizer";
}

bool
pl_initialize_graphics(const plGraphicsInit* ptDesc)
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

    gptGraphics->bValidationActive = ptDesc->tFlags & PL_GRAPHICS_INIT_FLAGS_VALIDATION_ENABLED;

    gptGraphics->bDebugMessengerActive = gptGraphics->bValidationActive;

    // set frames in flight (if zero, use a default of 2)
    gptGraphics->uFramesInFlight = pl_min(pl_max(ptDesc->uFramesInFlight, 2), PL_MAX_FRAMES_IN_FLIGHT);

    return true;
}

static void
pl_enumerate_devices(plDeviceInfo *atDeviceInfo, uint32_t* puDeviceCount)
{
    *puDeviceCount = 1;

    if(atDeviceInfo == NULL)
        return;

    plIO* ptIOCtx = gptIOI->get_io();

    strncpy(atDeviceInfo[0].acName, "PL CPU Rasterizer", 256);
    atDeviceInfo[0].tVendorId = PL_VENDOR_ID_NONE;
    atDeviceInfo[0].tType = PL_DEVICE_TYPE_CPU;
    atDeviceInfo[0].tCapabilities = PL_DEVICE_CAPABILITY_BIND_GROUP_INDEXING | PL_DEVICE_CAPABILITY_SAMPLER_ANISOTROPY | PL_DEVICE_CAPABILITY_SWAPCHAIN | PL_DEVICE_CAPABILITY_MULTIPLE_VIEWPORTS;
}

plDevice*
pl_create_device(const plDeviceInit* ptInit)
{

    plDevice* ptDevice = PL_ALLOC(sizeof(plDevice));
    memset(ptDevice, 0, sizeof(plDevice));
    return ptDevice;
}

plSurface*
pl_create_surface(plWindow* ptWindow)
{
    plSurface* ptSurface = PL_ALLOC(sizeof(plSurface));
    memset(ptSurface, 0, sizeof(plSurface));
    return ptSurface;
}

plSwapchain*
pl_create_swapchain(plDevice* ptDevice, plSurface* ptSurface, const plSwapchainInit* ptInit)
{

    plSwapchain* ptSwap = PL_ALLOC(sizeof(plSwapchain));
    memset(ptSwap, 0, sizeof(plSwapchain));
    return ptSwap;
}

void
pl_begin_frame(plDevice* ptDevice)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);
    pl__garbage_collect(ptDevice);
    pl_end_cpu_sample(gptProfile, 0);
}

bool
pl_acquire_swapchain_image(plSwapchain* ptSwap)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);
    pl_end_cpu_sample(gptProfile, 0);
    return true;
}

void
pl_end_command_recording(plCommandBuffer* ptCommandBuffer)
{
}

bool
pl_present(plCommandBuffer* ptCmdBuffer, const plSubmitInfo* ptSubmitInfo, plSwapchain **ptSwaps, uint32_t uSwapchainCount)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);
    gptGraphics->uCurrentFrameIndex = (gptGraphics->uCurrentFrameIndex + 1) % gptGraphics->uFramesInFlight;
    pl_end_cpu_sample(gptProfile, 0);
    return true;
}

void
pl_recreate_swapchain(plSwapchain* ptSwap, const plSwapchainInit* ptInit)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);
    pl_end_cpu_sample(gptProfile, 0);
}

void
pl_flush_device(plDevice* ptDevice)
{
}

void
pl_cleanup_graphics(void)
{
    pl__cleanup_common_graphics();
}

void
pl_cleanup_surface(plSurface* ptSurface)
{
    PL_FREE(ptSurface);
}

void
pl_cleanup_swapchain(plSwapchain* ptSwap)
{
    pl__cleanup_common_swapchain(ptSwap);
}

void
pl_cleanup_device(plDevice* ptDevice)
{
    pl__cleanup_common_device(ptDevice);
}

void
pl_pipeline_barrier_blit(plBlitEncoder* ptEncoder, plShaderStageFlags beforeStages, plAccessFlags beforeAccesses, plShaderStageFlags afterStages, plAccessFlags afterAccesses)
{
}

void
pl_pipeline_barrier_compute(plComputeEncoder* ptEncoder, plShaderStageFlags beforeStages, plAccessFlags beforeAccesses, plShaderStageFlags afterStages, plAccessFlags afterAccesses)
{
}

void
pl_pipeline_barrier_render(plRenderEncoder* ptEncoder,  plShaderStageFlags beforeStages, plAccessFlags beforeAccesses, plShaderStageFlags afterStages, plAccessFlags afterAccesses)
{
}

plComputeEncoder*
pl_begin_compute_pass(plCommandBuffer* ptCmdBuffer, const plPassResources* ptResources)
{
    plComputeEncoder* ptEncoder = pl__get_new_compute_encoder();
    return ptEncoder;
}

void
pl_end_compute_pass(plComputeEncoder* ptEncoder)
{
    pl__return_compute_encoder(ptEncoder);
}

plBlitEncoder*
pl_begin_blit_pass(plCommandBuffer* ptCmdBuffer)
{
    plBlitEncoder* ptEncoder = pl__get_new_blit_encoder();
    return ptEncoder;
}

void
pl_end_blit_pass(plBlitEncoder* ptEncoder)
{
    pl__return_blit_encoder(ptEncoder);
}

void
pl_dispatch(plComputeEncoder* ptEncoder, uint32_t uDispatchCount, const plDispatch *atDispatches)
{
}

void
pl_bind_compute_bind_groups(plComputeEncoder* ptEncoder, plComputeShaderHandle tHandle, uint32_t uFirst,
    uint32_t uCount, const plBindGroupHandle *atBindGroups, uint32_t uDynamicBindingCount, const plDynamicBinding* ptDynamicBinding)
{
}

void
pl_bind_graphics_bind_groups(plRenderEncoder* ptEncoder, plShaderHandle tHandle, uint32_t uFirst, uint32_t uCount, const plBindGroupHandle *atBindGroups, uint32_t uDynamicBindingCount, const plDynamicBinding* ptDynamicBinding)
{
}

void
pl_submit_command_buffer(plCommandBuffer* ptCmdBuffer, const plSubmitInfo* ptSubmitInfo)
{
}

void
pl_wait_on_command_buffer(plCommandBuffer* ptCmdBuffer)
{
}

void
pl_return_command_buffer(plCommandBuffer* ptCmdBuffer)
{
}

plBindGroupPool*
pl_create_bind_group_pool(plDevice* ptDevice, const plBindGroupPoolDesc* ptDesc)
{
    plBindGroupPool* ptPool = PL_ALLOC(sizeof(plBindGroupPool));
    memset(ptPool, 0, sizeof(plBindGroupPool));
    return ptPool;
}

void
pl_reset_bind_group_pool(plBindGroupPool* ptPool)
{
}

void
pl_cleanup_bind_group_pool(plBindGroupPool* ptPool)
{
    PL_FREE(ptPool);
}

plCommandPool *
pl_create_command_pool(plDevice* ptDevice, const plCommandPoolDesc* ptDesc)
{
    plCommandPool* ptPool = PL_ALLOC(sizeof(plCommandPool));
    memset(ptPool, 0, sizeof(plCommandPool));
    return ptPool;
}

void
pl_cleanup_command_pool(plCommandPool* ptPool)
{
    PL_FREE(ptPool);
}

void
pl_reset_command_pool(plCommandPool* ptPool, plCommandPoolResetFlags tFlags)
{
}

void
pl_reset_command_buffer(plCommandBuffer* ptCommandBuffer)
{
}

plCommandBuffer*
pl_request_command_buffer(plCommandPool* ptPool)
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
pl_copy_buffer(plBlitEncoder* ptEncoder, plBufferHandle tSource, plBufferHandle tDestination, uint32_t uSourceOffset, uint32_t uDestinationOffset, size_t szSize)
{
}

void
pl_copy_buffer_to_texture(plBlitEncoder* ptEncoder, plBufferHandle tBufferHandle, plTextureHandle tTextureHandle, uint32_t uRegionCount, const plBufferImageCopy* ptRegions)
{
}

void
pl_destroy_buffer(plDevice* ptDevice, plBufferHandle tHandle)
{
}

void
pl_destroy_texture(plDevice* ptDevice, plTextureHandle tHandle)
{
}

void
pl_destroy_sampler(plDevice* ptDevice, plSamplerHandle tHandle)
{
}

void
pl_destroy_bind_group(plDevice* ptDevice, plBindGroupHandle tHandle)
{
}

void
pl_destroy_render_pass(plDevice* ptDevice, plRenderPassHandle tHandle)
{
}

void
pl_destroy_render_pass_layout(plDevice* ptDevice, plRenderPassLayoutHandle tHandle)
{
}

void
pl_destroy_shader(plDevice* ptDevice, plShaderHandle tHandle)
{
}

void
pl_destroy_compute_shader(plDevice* ptDevice, plComputeShaderHandle tHandle)
{
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------


static void
pl__garbage_collect(plDevice* ptDevice)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);
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
    pl_end_cpu_sample(gptProfile, 0);
}
