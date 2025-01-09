/*
   pl_graphics_vulkan.c
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
#include "pl_ds.h"
#include "pl_string.h"
#include "pl_memory.h"
#include "pl_graphics_internal.h"

// vulkan surface stuff
#if defined(_WIN32)
    #define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__ANDROID__)
    #define VK_USE_PLATFORM_ANDROID_KHR
#elif defined(__APPLE__)
    #define VK_USE_PLATFORM_METAL_EXT
#else // linux
    #define VK_USE_PLATFORM_XCB_KHR
#endif

#include "vulkan/vulkan.h"

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_VULKAN
    #ifdef NDEBUG
        #define PL_VULKAN(x) x
    #else
    #include <assert.h>
        #define PL_VULKAN(x) assert(x == VK_SUCCESS)
    #endif
#endif

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
    VkAttachmentDescription atAttachments[PL_MAX_RENDER_TARGETS];
    VkSubpassDependency     atSubpassDependencies[PL_MAX_RENDER_TARGETS * PL_MAX_RENDER_TARGETS + 2];
    VkSubpassDescription    atSubpasses[PL_MAX_SUBPASSES];
    VkAttachmentReference   atSubpassColorAttachmentReferences[PL_MAX_RENDER_TARGETS][PL_MAX_SUBPASSES];
    VkAttachmentReference   atSubpassInputAttachmentReferences[PL_MAX_RENDER_TARGETS][PL_MAX_SUBPASSES];
    VkAttachmentReference   tDepthAttachmentReference;
    VkAttachmentReference   tResolveAttachmentReference;
} plRenderPassCommonData;

typedef struct _plVulkanDynamicBuffer
{
    uint32_t                 uHandle;
    VkBuffer                 tBuffer;
    plDeviceMemoryAllocation tMemory;
    VkDescriptorSet          tDescriptorSet;
} plVulkanDynamicBuffer;

typedef struct _plVulkanBuffer
{
    char*    pcData;
    VkBuffer tBuffer;
} plVulkanBuffer;

typedef struct _plVulkanTexture
{
    VkImage     tImage;
    VkImageView tImageView;
    bool        bOriginalView; // so we only free if original
} plVulkanTexture;

typedef struct _plVulkanRenderPassLayout
{
    VkRenderPass tRenderPass;
} plVulkanRenderPassLayout;

typedef struct _plVulkanRenderPass
{
    VkRenderPass  tRenderPass;
    VkFramebuffer atFrameBuffers[6];
} plVulkanRenderPass;

typedef struct _plVulkanBindGroupLayout
{
    VkDescriptorSetLayout tDescriptorSetLayout;
} plVulkanBindGroupLayout;

typedef struct _plVulkanBindGroup
{
    VkDescriptorPool      tPool; // owning pool
    bool                  bResetable;
    VkDescriptorSet       tDescriptorSet;
    VkDescriptorSetLayout tDescriptorSetLayout;
} plVulkanBindGroup;

typedef struct _plVulkanShader
{
    VkPipelineLayout         tPipelineLayout;
    VkPipeline               tPipeline;
    VkSampleCountFlagBits    tMSAASampleCount;
    VkShaderModule           tPixelShaderModule;
    VkShaderModule           tVertexShaderModule;
    VkDescriptorSetLayout    atDescriptorSetLayouts[4];
    VkSpecializationMapEntry atSpecializationEntries[PL_MAX_SHADER_SPECIALIZATION_CONSTANTS];
    size_t                   szSpecializationSize;
} plVulkanShader;

typedef struct _plVulkanComputeShader
{
    VkPipelineLayout         tPipelineLayout;
    VkPipeline               tPipeline;
    VkShaderModule           tShaderModule;
    VkSpecializationMapEntry atSpecializationEntries[PL_MAX_SHADER_SPECIALIZATION_CONSTANTS];
    size_t                   szSpecializationSize;
    VkDescriptorSetLayout    atDescriptorSetLayouts[4];
} plVulkanComputeShader;

//-----------------------------------------------------------------------------
// [SECTION] opaque structs
//-----------------------------------------------------------------------------

typedef struct _plCommandBuffer
{
    plDevice*          ptDevice; // for convience
    plBeginCommandInfo tBeginInfo;
    VkCommandBuffer    tCmdBuffer;
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
    VkCommandPool    tCmdPool;
    VkCommandBuffer* sbtReadyCommandBuffers;   // completed command buffers
    VkCommandBuffer* sbtPendingCommandBuffers; // recently submitted command buffers
    plCommandBuffer* ptCommandBufferFreeList;  // free list of command buffers
} plCommandPool;

typedef struct _plBindGroupPool
{
    plDevice*           ptDevice; // for convience
    VkDescriptorPool    tDescriptorPool;
    plBindGroupPoolDesc tDesc;
} plBindGroupPool;

typedef struct _plTimelineSemaphore
{
    plDevice*            ptDevice; // for convience
    VkSemaphore          tSemaphore;
    plTimelineSemaphore* ptNext; // for linked list
} plTimelineSemaphore;

typedef struct _plFrameContext
{
    VkSemaphore    tRenderFinish;
    VkFence        tInFlight;
    VkFramebuffer* sbtRawFrameBuffers;

    // dynamic buffer stuff
    uint16_t               uCurrentBufferIndex;
    plVulkanDynamicBuffer* sbtDynamicBuffers;
} plFrameContext;

typedef struct _plDevice
{
    // general
    plDeviceInit              tInit;
    plDeviceInfo              tInfo;
    plFrameGarbage*           sbtGarbage;
    plFrameContext*           sbtFrames;
    plDeviceMemoryAllocatorI* ptDynamicAllocator;

    // null descriptor
    VkDescriptorSet       tNullDecriptorSet;
    VkDescriptorSet       tNullDynamicDecriptorSet;
    VkDescriptorSetLayout tNullDescriptorSetLayout;
    VkDeviceMemory        tDummyMemory;
    
    // timeline semaphore free list
    plTimelineSemaphore* ptSemaphoreFreeList;

    // render pass layout generation pool
    plVulkanRenderPassLayout* sbtRenderPassLayoutsHot;
    plRenderPassLayout*       sbtRenderPassLayoutsCold;
    uint16_t*                 sbtRenderPassLayoutFreeIndices;

    // render pass generation pool
    plVulkanRenderPass* sbtRenderPassesHot;
    plRenderPass*       sbtRenderPassesCold;
    uint16_t*           sbtRenderPassFreeIndices;

    // shader generation pool
    plVulkanShader* sbtShadersHot;
    plShader*       sbtShadersCold;
    uint16_t*       sbtShaderFreeIndices;

    // compute shader generation pool
    plVulkanComputeShader* sbtComputeShadersHot;
    plComputeShader*       sbtComputeShadersCold;
    uint16_t*              sbtComputeShaderFreeIndices;

    // buffer generation pool
    plVulkanBuffer* sbtBuffersHot;
    plBuffer*       sbtBuffersCold;
    uint16_t*       sbtBufferFreeIndices;

    // texture generation pool
    VkImageView*     sbtTextureViewsHot;
    plVulkanTexture* sbtTexturesHot;
    plTexture*       sbtTexturesCold;
    uint16_t*        sbtTextureFreeIndices;

    // sampler generation pool
    VkSampler* sbtSamplersHot;
    plSampler* sbtSamplersCold;
    uint16_t*  sbtSamplerFreeIndices;

    // bind group generation pool
    plVulkanBindGroup* sbtBindGroupsHot;
    plBindGroup*       sbtBindGroupsCold;
    uint16_t*          sbtBindGroupFreeIndices;

    // bind group layout generation pool
    plVulkanBindGroupLayout* sbtBindGroupLayouts;
    uint32_t*                sbtBindGroupLayoutFreeIndices;
    VkDescriptorSetLayout    tDynamicDescriptorSetLayout;

    // vulkan specifics
    VkDevice                         tLogicalDevice;
    VkPhysicalDevice                 tPhysicalDevice;
    VkPhysicalDeviceMemoryProperties tMemProps;
    int                              iGraphicsQueueFamily;
    int                              iPresentQueueFamily;
    VkQueue                          tGraphicsQueue;
    VkQueue                          tPresentQueue;
    VkDescriptorPool                 tDynamicBufferDescriptorPool;

    // debug marker function pointers
    PFN_vkDebugMarkerSetObjectTagEXT  vkDebugMarkerSetObjectTag;
    PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectName;
    PFN_vkCmdDebugMarkerBeginEXT      vkCmdDebugMarkerBegin;
    PFN_vkCmdDebugMarkerEndEXT        vkCmdDebugMarkerEnd;
    PFN_vkCmdDebugMarkerInsertEXT     vkCmdDebugMarkerInsert;
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
    
    // vulkan specifics
    plTempAllocator          tTempAllocator;
    VkInstance               tInstance;
    VkDebugUtilsMessengerEXT tDbgMessenger;
} plGraphics;

typedef struct _plSurface
{
    VkSurfaceKHR tSurface;
} plSurface;

typedef struct _plSwapchain
{
    plDevice*        ptDevice; // for convience
    plSwapchainInfo  tInfo;
    VkSemaphore      atImageAvailable[PL_MAX_FRAMES_IN_FLIGHT];
    uint32_t         uImageCount;
    plTextureHandle* sbtSwapchainTextureViews;
    uint32_t         uCurrentImageIndex; // current image to use within the swap chain

    // platform specific
    plSurface*          ptSurface;
    VkSwapchainKHR      tSwapChain;
    VkImage             atImages[8];
    VkSurfaceFormatKHR* sbtSurfaceFormats;
} plSwapchain;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

// conversion between pilotlight & vulkan types
static VkFilter              pl__vulkan_filter       (plFilter);
static VkSamplerAddressMode  pl__vulkan_wrap         (plAddressMode);
static VkCompareOp           pl__vulkan_compare      (plCompareMode);
static VkFormat              pl__vulkan_format       (plFormat);
static VkFormat              pl__vulkan_vertex_format(plVertexFormat);
static VkImageLayout         pl__vulkan_layout       (plTextureUsage);
static VkAttachmentLoadOp    pl__vulkan_load_op      (plLoadOp);
static VkAttachmentStoreOp   pl__vulkan_store_op     (plStoreOp);
static VkCullModeFlags       pl__vulkan_cull         (plCullMode);
static VkShaderStageFlagBits pl__vulkan_stage_flags  (plStageFlags);
static plFormat              pl__pilotlight_format   (VkFormat);
static VkStencilOp           pl__vulkan_stencil_op   (plStencilOp);
static VkBlendFactor         pl__vulkan_blend_factor (plBlendFactor);
static VkBlendOp             pl__vulkan_blend_op     (plBlendOp);
static VkAccessFlags         pl__vulkan_access_flags (plAccessFlags);

// misc
static plDeviceMemoryAllocation pl__allocate_staging_dynamic(struct plDeviceMemoryAllocatorO*, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName);
static void                     pl__free_staging_dynamic    (struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation);

// misc helpers
static VkFormat pl__find_supported_format       (plDevice*, VkFormatFeatureFlags, const VkFormat*, uint32_t uFormatCount);
static bool     pl__format_has_stencil          (VkFormat);
static void     pl__transition_image_layout     (VkCommandBuffer, VkImage, VkImageLayout tOldLayout, VkImageLayout tNewLayout, VkImageSubresourceRange, VkPipelineStageFlags tSrcStageMask, VkPipelineStageFlags tDstStageMask);
static void     pl__create_swapchain            (uint32_t uWidth, uint32_t uHeight, plSwapchain *);
static void     pl__fill_common_render_pass_data(plRenderPassLayoutDesc*, plRenderPassLayout*, plRenderPassCommonData* ptDataOut);
static void     pl__create_bind_group_layout    (plDevice*, plBindGroupLayout*, const char* pcName);

// debug stuff
static VKAPI_ATTR VkBool32 VKAPI_CALL pl__debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT*, void*);
static void pl__set_vulkan_object_name(plDevice*, uint64_t uObjectHandle, VkDebugReportObjectTypeEXT, const char* pcName);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

plTimelineSemaphore*
pl_create_semaphore(plDevice* ptDevice, bool bHostVisible)
{
    plTimelineSemaphore* ptSemaphore = pl__get_new_semaphore(ptDevice);

    VkSemaphoreTypeCreateInfo tTimelineCreateInfo = {
        .sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .pNext         = NULL,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue  = 0,
    };

    VkSemaphoreCreateInfo tCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &tTimelineCreateInfo,
        .flags = 0,
    };
    PL_VULKAN(vkCreateSemaphore(ptDevice->tLogicalDevice, &tCreateInfo, NULL, &ptSemaphore->tSemaphore));
    return ptSemaphore;
}

void
pl_cleanup_semaphore(plTimelineSemaphore* ptSemaphore)
{
    pl__return_semaphore(ptSemaphore->ptDevice, ptSemaphore);
    vkDestroySemaphore(ptSemaphore->ptDevice->tLogicalDevice, ptSemaphore->tSemaphore, NULL);
    ptSemaphore->tSemaphore = VK_NULL_HANDLE;
}

void
pl_signal_semaphore(plDevice* ptDevice, plTimelineSemaphore* ptSemaphore, uint64_t ulValue)
{
    const VkSemaphoreSignalInfo tSignalInfo = {
        .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
        .semaphore = ptSemaphore->tSemaphore,
        .value     = ulValue,
        .pNext     = NULL,
    };
    vkSignalSemaphore(ptDevice->tLogicalDevice, &tSignalInfo);
}

void
pl_wait_semaphore(plDevice* ptDevice, plTimelineSemaphore* ptSemaphore, uint64_t ulValue)
{
    const VkSemaphoreWaitInfo tWaitInfo = {
        .sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .semaphoreCount = 1,
        .pSemaphores    = &ptSemaphore->tSemaphore,
        .pValues        = &ulValue,
        .flags          = 0,
        .pNext          = NULL,
    };
    vkWaitSemaphores(ptDevice->tLogicalDevice, &tWaitInfo, UINT64_MAX);
}

uint64_t
pl_get_semaphore_value(plDevice* ptDevice, plTimelineSemaphore* ptSemaphore)
{
    uint64_t ulValue = 0;
    vkGetSemaphoreCounterValue(ptDevice->tLogicalDevice, ptSemaphore->tSemaphore, &ulValue);
    return ulValue;
}

plBufferHandle
pl_create_buffer(plDevice* ptDevice, const plBufferDesc* ptDesc, plBuffer **ptBufferOut)
{
    plBufferHandle tHandle = pl__get_new_buffer_handle(ptDevice);
    plBuffer* ptBuffer = pl__get_buffer(ptDevice, tHandle);
    ptBuffer->tDesc = *ptDesc;

    if (ptDesc->pcDebugName == NULL)
    {
        ptBuffer->tDesc.pcDebugName = "unnamed buffer";
    }

    VkBufferCreateInfo tBufferInfo = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = ptDesc->szByteSize,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VkBufferUsageFlagBits tBufferUsageFlags = 0;
    if (ptDesc->tUsage & PL_BUFFER_USAGE_VERTEX)
        tBufferInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (ptDesc->tUsage & PL_BUFFER_USAGE_INDEX)
        tBufferInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (ptDesc->tUsage & PL_BUFFER_USAGE_STORAGE)
        tBufferInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (ptDesc->tUsage & PL_BUFFER_USAGE_UNIFORM)
        tBufferInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (ptDesc->tUsage & PL_BUFFER_USAGE_STAGING)
        tBufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VkMemoryRequirements tMemRequirements = {0};

    plVulkanBuffer tVulkanBuffer = {0};
    PL_VULKAN(vkCreateBuffer(ptDevice->tLogicalDevice, &tBufferInfo, NULL, &tVulkanBuffer.tBuffer));
    pl__set_vulkan_object_name(ptDevice, (uint64_t)tVulkanBuffer.tBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, ptBuffer->tDesc.pcDebugName);
    vkGetBufferMemoryRequirements(ptDevice->tLogicalDevice, tVulkanBuffer.tBuffer, &tMemRequirements);

    ptBuffer->tMemoryRequirements.ulAlignment     = tMemRequirements.alignment;
    ptBuffer->tMemoryRequirements.ulSize          = tMemRequirements.size;
    ptBuffer->tMemoryRequirements.uMemoryTypeBits = tMemRequirements.memoryTypeBits;

    ptDevice->sbtBuffersHot[tHandle.uIndex] = tVulkanBuffer;

    if (ptBufferOut)
       *ptBufferOut = &ptDevice->sbtBuffersCold[tHandle.uIndex];

    return tHandle;
}

void
pl_bind_buffer_to_memory(plDevice* ptDevice, plBufferHandle tHandle, const plDeviceMemoryAllocation* ptAllocation)
{
    plBuffer* ptBuffer = &ptDevice->sbtBuffersCold[tHandle.uIndex];
    ptBuffer->tMemoryAllocation = *ptAllocation;
    plVulkanBuffer* ptVulkanBuffer = &ptDevice->sbtBuffersHot[tHandle.uIndex];

    PL_VULKAN(vkBindBufferMemory(ptDevice->tLogicalDevice, ptVulkanBuffer->tBuffer, (VkDeviceMemory)ptAllocation->uHandle, ptAllocation->ulOffset));
    ptVulkanBuffer->pcData = ptAllocation->pHostMapped;
}

static plDynamicDataBlock
pl_allocate_dynamic_data_block(plDevice* ptDevice)
{
    plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);

    plVulkanDynamicBuffer* ptDynamicBuffer = NULL;

    if(ptFrame->uCurrentBufferIndex != 0)
    {
        if(pl_sb_size(ptFrame->sbtDynamicBuffers) <= ptFrame->uCurrentBufferIndex)
        {
            pl_sb_add(ptFrame->sbtDynamicBuffers);
            ptDynamicBuffer = &ptFrame->sbtDynamicBuffers[ptFrame->uCurrentBufferIndex];

            const plBufferDesc tStagingBufferDescription0 = {
                .tUsage      = PL_BUFFER_USAGE_UNIFORM,
                .szByteSize  = ptDevice->tInit.szDynamicBufferBlockSize,
                .pcDebugName = "dynamic buffer"};

            plBuffer* ptStagingBuffer = NULL;
            plBufferHandle tStagingBuffer0 = pl_create_buffer(ptDevice, &tStagingBufferDescription0, &ptStagingBuffer);
            plDeviceMemoryAllocation tAllocation = ptDevice->ptDynamicAllocator->allocate(
                ptDevice->ptDynamicAllocator->ptInst,
                ptStagingBuffer->tMemoryRequirements.uMemoryTypeBits,
                ptStagingBuffer->tMemoryRequirements.ulSize,
                ptStagingBuffer->tMemoryRequirements.ulAlignment,
                "dynamic buffer");

            pl_bind_buffer_to_memory(ptDevice, tStagingBuffer0, &tAllocation);

            ptDynamicBuffer->uHandle = tStagingBuffer0.uIndex;
            ptDynamicBuffer->tBuffer = ptDevice->sbtBuffersHot[tStagingBuffer0.uIndex].tBuffer;
            ptDynamicBuffer->tMemory = ptStagingBuffer->tMemoryAllocation;

            // allocate descriptor set
            const VkDescriptorSetAllocateInfo tDynamicAllocInfo = {
                .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool     = ptDevice->tDynamicBufferDescriptorPool,
                .descriptorSetCount = 1,
                .pSetLayouts        = &ptDevice->tDynamicDescriptorSetLayout
            };
            PL_VULKAN(vkAllocateDescriptorSets(ptDevice->tLogicalDevice, &tDynamicAllocInfo, &ptDynamicBuffer->tDescriptorSet));

            // update descriptor
            VkDescriptorBufferInfo tDescriptorInfo0 = {
                .buffer = ptDynamicBuffer->tBuffer,
                .offset = 0,
                .range  = ptDevice->tInit.szDynamicDataMaxSize
            };

            VkWriteDescriptorSet tWrite0 = {
                .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstBinding      = 0,
                .dstArrayElement = 0,
                .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                .descriptorCount = 1,
                .dstSet          = ptDynamicBuffer->tDescriptorSet,
                .pBufferInfo     = &tDescriptorInfo0,
                .pNext           = NULL,
            };
            vkUpdateDescriptorSets(ptDevice->tLogicalDevice, 1, &tWrite0, 0, NULL);
        }
    }
    
    if(ptDynamicBuffer == NULL)
        ptDynamicBuffer = &ptFrame->sbtDynamicBuffers[ptFrame->uCurrentBufferIndex];

    plVulkanBuffer* ptBuffer = &ptDevice->sbtBuffersHot[ptDynamicBuffer->uHandle];

    plDynamicDataBlock tBlock = {
        ._uBufferHandle  = ptFrame->uCurrentBufferIndex,
        ._uByteSize      = (uint32_t)ptDevice->tInit.szDynamicBufferBlockSize,
        ._pcData         = ptBuffer->pcData,
        ._uAlignment     = ptDevice->tInfo.tLimits.uMinUniformBufferOffsetAlignment,
        ._uBumpAmount    = (uint32_t)ptDevice->tInit.szDynamicDataMaxSize,
        ._uCurrentOffset = 0
    };
    ptFrame->uCurrentBufferIndex++;
    return tBlock;
}

void
pl_copy_texture_to_buffer(plBlitEncoder* ptEncoder, plTextureHandle tTextureHandle, plBufferHandle tBufferHandle, uint32_t uRegionCount, const plBufferImageCopy* ptRegions)
{
    plCommandBuffer* ptCmdBuffer = ptEncoder->ptCommandBuffer;
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    // allocate temporary memory
    VkImageSubresourceRange* atSubResourceRanges = pl_temp_allocator_alloc(&gptGraphics->tTempAllocator, sizeof(VkImageSubresourceRange) * uRegionCount);
    VkBufferImageCopy* atCopyRegions = pl_temp_allocator_alloc(&gptGraphics->tTempAllocator, sizeof(VkBufferImageCopy) * uRegionCount);
    memset(atSubResourceRanges, 0, sizeof(VkImageSubresourceRange) * uRegionCount);
    memset(atCopyRegions, 0, sizeof(VkBufferImageCopy) * uRegionCount);

    // setup copy regions
    plTexture* ptColdTexture = pl__get_texture(ptDevice, tTextureHandle);
    for (uint32_t i = 0; i < uRegionCount; i++)
    {

        // transition textures to acceptable layouts
        VkImageLayout tLayout = ptRegions[i].tCurrentImageUsage == 0 ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : pl__vulkan_layout(ptRegions[i].tCurrentImageUsage);
        atSubResourceRanges[i].aspectMask = ptColdTexture->tDesc.tUsage & PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        atSubResourceRanges[i].baseMipLevel = ptRegions[i].uMipLevel;
        atSubResourceRanges[i].levelCount = 1;
        atSubResourceRanges[i].baseArrayLayer = ptRegions[i].uBaseArrayLayer;
        atSubResourceRanges[i].layerCount = ptRegions[i].uLayerCount;
        pl__transition_image_layout(ptCmdBuffer->tCmdBuffer, ptDevice->sbtTexturesHot[tTextureHandle.uIndex].tImage, tLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, atSubResourceRanges[i], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        atCopyRegions[i].bufferOffset = ptRegions[i].szBufferOffset;
        atCopyRegions[i].bufferRowLength = ptRegions[i].uBufferRowLength;
        atCopyRegions[i].bufferImageHeight = ptRegions[i].uBufferImageHeight;
        atCopyRegions[i].imageSubresource.aspectMask = atSubResourceRanges[i].aspectMask;
        atCopyRegions[i].imageSubresource.mipLevel = ptRegions[i].uMipLevel;
        atCopyRegions[i].imageSubresource.baseArrayLayer = ptRegions[i].uBaseArrayLayer;
        atCopyRegions[i].imageSubresource.layerCount = ptRegions[i].uLayerCount;
        atCopyRegions[i].imageExtent.width = ptRegions[i].uImageWidth;
        atCopyRegions[i].imageExtent.height = ptRegions[i].uImageHeight;
        atCopyRegions[i].imageExtent.depth = ptRegions[i].uImageDepth;
    }

    vkCmdCopyImageToBuffer(ptCmdBuffer->tCmdBuffer, ptDevice->sbtTexturesHot[tTextureHandle.uIndex].tImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, ptDevice->sbtBuffersHot[tBufferHandle.uIndex].tBuffer, uRegionCount, atCopyRegions);

    // return textures to original layouts
    for (uint32_t i = 0; i < uRegionCount; i++)
    {
        VkImageLayout tLayout = ptRegions[i].tCurrentImageUsage == 0 ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : pl__vulkan_layout(ptRegions[i].tCurrentImageUsage);
        pl__transition_image_layout(ptCmdBuffer->tCmdBuffer, ptDevice->sbtTexturesHot[tTextureHandle.uIndex].tImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, tLayout, atSubResourceRanges[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    }

    pl_temp_allocator_reset(&gptGraphics->tTempAllocator);
}

void
pl_generate_mipmaps(plBlitEncoder* ptEncoder, plTextureHandle tTexture)
{
    plCommandBuffer* ptCmdBuffer = ptEncoder->ptCommandBuffer;
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    plTexture* ptTexture = pl__get_texture(ptDevice, tTexture);

    // generate mips
    if (ptTexture->tDesc.uMips > 1)
    {

        // check if format supports linear blitting
        VkFormatProperties tFormatProperties = {0};
        vkGetPhysicalDeviceFormatProperties(ptDevice->tPhysicalDevice, pl__vulkan_format(ptTexture->tDesc.tFormat), &tFormatProperties);

        // transition texture to proper layout
        const VkImageSubresourceRange tSubResourceRange = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = ptTexture->tDesc.uMips,
            .baseArrayLayer = 0,
            .layerCount     = ptTexture->tDesc.uLayers
        };
        pl__transition_image_layout(ptCmdBuffer->tCmdBuffer, ptDevice->sbtTexturesHot[tTexture.uIndex].tImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, tSubResourceRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

        // perform blits
        if (tFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)
        {
            VkImageSubresourceRange tMipSubResourceRange = {
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseArrayLayer = 0,
                .layerCount     = ptTexture->tDesc.uLayers,
                .levelCount     = 1
            };

            int iMipWidth  = (int)ptTexture->tDesc.tDimensions.x;
            int iMipHeight = (int)ptTexture->tDesc.tDimensions.y;

            for (uint32_t i = 1; i < ptTexture->tDesc.uMips; i++)
            {
                tMipSubResourceRange.baseMipLevel = i - 1;

                pl__transition_image_layout(ptCmdBuffer->tCmdBuffer, ptDevice->sbtTexturesHot[tTexture.uIndex].tImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, tMipSubResourceRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

                VkImageBlit tBlit = {
                    .srcOffsets[1] = {
                        .x = iMipWidth,
                        .y = iMipHeight,
                        .z = 1
                    },
                    .srcSubresource = {
                        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                        .mipLevel       = i - 1,
                        .baseArrayLayer = 0,
                        .layerCount     = 1
                    },
                    .dstOffsets[1] = {
                        .x = 1,
                        .y = 1,
                        .z = 1
                    },
                    .dstSubresource = {
                        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                        .mipLevel       = i,
                        .baseArrayLayer = 0,
                        .layerCount     = 1
                    }
                };

                if (iMipWidth > 1)
                    tBlit.dstOffsets[1].x = iMipWidth / 2;

                if (iMipHeight > 1)
                    tBlit.dstOffsets[1].y = iMipHeight / 2;

                vkCmdBlitImage(ptCmdBuffer->tCmdBuffer, 
                    ptDevice->sbtTexturesHot[tTexture.uIndex].tImage,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    ptDevice->sbtTexturesHot[tTexture.uIndex].tImage,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1, &tBlit, VK_FILTER_LINEAR);

                pl__transition_image_layout(ptCmdBuffer->tCmdBuffer,
                    ptDevice->sbtTexturesHot[tTexture.uIndex].tImage,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    tMipSubResourceRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

                if (iMipWidth > 1)
                    iMipWidth /= 2;

                if (iMipHeight > 1)
                    iMipHeight /= 2;
            }

            tMipSubResourceRange.baseMipLevel = ptTexture->tDesc.uMips - 1;
            pl__transition_image_layout(ptCmdBuffer->tCmdBuffer, ptDevice->sbtTexturesHot[tTexture.uIndex].tImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, tMipSubResourceRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        }
        else
        {
            PL_ASSERT(false && "format does not support linear blitting");
        }
    }
}

plSamplerHandle
pl_create_sampler(plDevice* ptDevice, const plSamplerDesc* ptDesc)
{
    plSamplerHandle tHandle = pl__get_new_sampler_handle(ptDevice);
    plSampler* ptSampler = pl_get_sampler(ptDevice, tHandle);
    ptSampler->tDesc = *ptDesc;

    const VkSamplerCreateInfo tSamplerInfo = {
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .minFilter               = pl__vulkan_filter(ptDesc->tMinFilter),
        .magFilter               = pl__vulkan_filter(ptDesc->tMagFilter),
        .addressModeU            = pl__vulkan_wrap(ptDesc->tUAddressMode),
        .addressModeV            = pl__vulkan_wrap(ptDesc->tVAddressMode),
        .addressModeW            = pl__vulkan_wrap(ptDesc->tWAddressMode),
        .anisotropyEnable        = (bool)(ptDevice->tInfo.tCapabilities & PL_DEVICE_CAPABILITY_SAMPLER_ANISOTROPY),
        .maxAnisotropy           = ptDesc->fMaxAnisotropy == 0 ? (ptDevice->tInfo.tCapabilities & PL_DEVICE_CAPABILITY_SAMPLER_ANISOTROPY) : ptDesc->fMaxAnisotropy,
        .borderColor             = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        .unnormalizedCoordinates = ptDesc->bUnnormalizedCoordinates ? VK_TRUE : VK_FALSE,
        .compareEnable           = ptDesc->tCompare == 0 ? VK_FALSE : VK_TRUE,
        .compareOp               = pl__vulkan_compare(ptDesc->tCompare),
        .mipmapMode              = ptDesc->tMipmapMode == PL_MIPMAP_MODE_LINEAR ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .mipLodBias              = 0.0f,
        .minLod                  = ptDesc->fMinMip,
        .maxLod                  = ptDesc->fMaxMip,
    };

    VkSampler tVkSampler = VK_NULL_HANDLE;
    PL_VULKAN(vkCreateSampler(ptDevice->tLogicalDevice, &tSamplerInfo, NULL, &tVkSampler));

    ptDevice->sbtSamplersHot[tHandle.uIndex] = tVkSampler;
    return tHandle;
}

plBindGroupHandle
pl_create_bind_group(plDevice* ptDevice, const plBindGroupDesc* ptDesc)
{
    plBindGroupHandle tHandle = pl__get_new_bind_group_handle(ptDevice);
    plBindGroup* ptBindGroup = pl__get_bind_group(ptDevice, tHandle);
    ptBindGroup->tLayout = *ptDesc->ptLayout;

    plBindGroupLayout* ptLayout = &ptBindGroup->tLayout;

    // count bindings
    ptLayout->_uBufferBindingCount = 0;
    ptLayout->_uTextureBindingCount = 0;
    ptLayout->_uSamplerBindingCount = 0;
    for(uint32_t i = 0; i < PL_MAX_TEXTURES_PER_BIND_GROUP; i++)
    {
        if(ptLayout->atTextureBindings[i].tStages == PL_STAGE_NONE)
            break;
        ptLayout->_uTextureBindingCount++;
    }

    for(uint32_t i = 0; i < PL_MAX_BUFFERS_PER_BIND_GROUP; i++)
    {
        if(ptLayout->atBufferBindings[i].tStages == PL_STAGE_NONE)
            break;
        ptLayout->_uBufferBindingCount++;
    }

    for(uint32_t i = 0; i < PL_MAX_SAMPLERS_PER_BIND_GROUP; i++)
    {
        if(ptLayout->atSamplerBindings[i].tStages == PL_STAGE_NONE)
            break;
        ptLayout->_uSamplerBindingCount++;
    }

    
    const uint32_t uDescriptorBindingCount = ptLayout->_uTextureBindingCount + ptLayout->_uBufferBindingCount + ptLayout->_uSamplerBindingCount;
    VkDescriptorSetLayoutBinding *atDescriptorSetLayoutBindings = pl_temp_allocator_alloc(&gptGraphics->tTempAllocator, uDescriptorBindingCount * sizeof(VkDescriptorSetLayoutBinding));
    VkDescriptorBindingFlagsEXT *atDescriptorSetLayoutFlags = pl_temp_allocator_alloc(&gptGraphics->tTempAllocator, uDescriptorBindingCount * sizeof(VkDescriptorBindingFlagsEXT));
    uint32_t tDescriptorCount = 1;
    bool bHasVariableDescriptors = false;

    // buffer bindings
    uint32_t uCurrentBinding = 0;
    for (uint32_t i = 0; i < ptLayout->_uBufferBindingCount; i++)
    {
        VkDescriptorSetLayoutBinding tBinding = {
            .binding            = ptLayout->atBufferBindings[i].uSlot,
            .descriptorType     = ptLayout->atBufferBindings[i].tType == PL_BUFFER_BINDING_TYPE_STORAGE ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount    = 1,
            .stageFlags         = pl__vulkan_stage_flags(ptLayout->atBufferBindings[i].tStages),
            .pImmutableSamplers = NULL
        };
        atDescriptorSetLayoutFlags[uCurrentBinding] = 0;
        atDescriptorSetLayoutBindings[uCurrentBinding++] = tBinding;
    }

    // texture bindings
    for (uint32_t i = 0; i < ptLayout->_uTextureBindingCount; i++)
    {
        VkDescriptorSetLayoutBinding tBinding = {
            .binding            = ptLayout->atTextureBindings[i].uSlot,
            .descriptorCount    = ptLayout->atTextureBindings[i].uDescriptorCount,
            .stageFlags         = pl__vulkan_stage_flags(ptLayout->atTextureBindings[i].tStages),
            .pImmutableSamplers = NULL
        };

        if (ptLayout->atTextureBindings[i].tType == PL_TEXTURE_BINDING_TYPE_SAMPLED)
            tBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        else if (ptLayout->atTextureBindings[i].tType == PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT)
            tBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        else if (ptLayout->atTextureBindings[i].tType == PL_TEXTURE_BINDING_TYPE_STORAGE)
            tBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

        if (tBinding.descriptorCount > 1)
            tDescriptorCount = tBinding.descriptorCount;
        else if (tBinding.descriptorCount == 0)
            tBinding.descriptorCount = 1;

        atDescriptorSetLayoutFlags[uCurrentBinding] = 0;
        if (ptLayout->atTextureBindings[i].bNonUniformIndexing)
        {
            atDescriptorSetLayoutFlags[uCurrentBinding] |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT;
            bHasVariableDescriptors = true;
        }
        atDescriptorSetLayoutBindings[uCurrentBinding++] = tBinding;
    }

    // sampler bindings
    for (uint32_t i = 0; i < ptLayout->_uSamplerBindingCount; i++)
    {
        VkDescriptorSetLayoutBinding tBinding = {
            .binding            = ptLayout->atSamplerBindings[i].uSlot,
            .descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount    = 1,
            .stageFlags         = pl__vulkan_stage_flags(ptLayout->atSamplerBindings[i].tStages),
            .pImmutableSamplers = NULL
        };

        atDescriptorSetLayoutFlags[uCurrentBinding] = 0;
        atDescriptorSetLayoutBindings[uCurrentBinding++] = tBinding;
    }

    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT tSetLayoutBindingFlags = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,
        .bindingCount  = uDescriptorBindingCount,
        .pBindingFlags = atDescriptorSetLayoutFlags,
        .pNext         = NULL
    };

    // create descriptor set layout
    const VkDescriptorSetLayoutCreateInfo tDescriptorSetLayoutInfo = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = uDescriptorBindingCount,
        .pBindings    = atDescriptorSetLayoutBindings,
        .pNext        = (ptDevice->tInfo.tCapabilities & PL_DEVICE_CAPABILITY_BIND_GROUP_INDEXING) ? &tSetLayoutBindingFlags : NULL,
        .flags        = (ptDevice->tInfo.tCapabilities & PL_DEVICE_CAPABILITY_BIND_GROUP_INDEXING) ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT : 0
    };

    VkDescriptorSetLayout tDescriptorSetLayout = VK_NULL_HANDLE;
    PL_VULKAN(vkCreateDescriptorSetLayout(ptDevice->tLogicalDevice, &tDescriptorSetLayoutInfo, NULL, &tDescriptorSetLayout));

    plVulkanBindGroup tVulkanBindGroup = {
        .bResetable           = ptDesc->ptPool->tDesc.tFlags & PL_BIND_GROUP_POOL_FLAGS_INDIVIDUAL_RESET,
        .tDescriptorSetLayout = tDescriptorSetLayout,
        .tPool                = ptDesc->ptPool->tDescriptorPool
    };

    VkDescriptorSetVariableDescriptorCountAllocateInfoEXT tVariableDescriptorCountAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT,
        .descriptorSetCount = 1,
        .pDescriptorCounts  = &tDescriptorCount,
    };

    // allocate descriptor sets
    const VkDescriptorSetAllocateInfo tAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = ptDesc->ptPool->tDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts        = &tDescriptorSetLayout,
        // .pNext              = bHasVariableDescriptors ? &tVariableDescriptorCountAllocInfo : NULL
        .pNext              = NULL
    };
    PL_VULKAN(vkAllocateDescriptorSets(ptDevice->tLogicalDevice, &tAllocInfo, &tVulkanBindGroup.tDescriptorSet));

    pl_temp_allocator_reset(&gptGraphics->tTempAllocator);

    ptDevice->sbtBindGroupsHot[tHandle.uIndex] = tVulkanBindGroup;
    return tHandle;
}

void
pl_update_bind_group(plDevice* ptDevice, plBindGroupHandle tHandle, const plBindGroupUpdateData* ptData)
{
    plBindGroup* ptBindGroup = pl__get_bind_group(ptDevice, tHandle);
    plVulkanBindGroup* ptVulkanBindGroup = &ptDevice->sbtBindGroupsHot[tHandle.uIndex];

    VkWriteDescriptorSet* sbtWrites = pl_temp_allocator_alloc(&gptGraphics->tTempAllocator, (ptData->uBufferCount + ptData->uSamplerCount + ptData->uTextureCount) * sizeof(VkWriteDescriptorSet));

    VkDescriptorBufferInfo* sbtBufferDescInfos = ptData->uBufferCount > 0 ? pl_temp_allocator_alloc(&gptGraphics->tTempAllocator, ptData->uBufferCount * sizeof(VkDescriptorBufferInfo)) : NULL;
    VkDescriptorImageInfo* sbtImageDescInfos = ptData->uTextureCount > 0 ? pl_temp_allocator_alloc(&gptGraphics->tTempAllocator, ptData->uTextureCount * sizeof(VkDescriptorImageInfo)) : NULL;
    VkDescriptorImageInfo* sbtSamplerDescInfos = ptData->uSamplerCount > 0 ? pl_temp_allocator_alloc(&gptGraphics->tTempAllocator, ptData->uSamplerCount * sizeof(VkDescriptorImageInfo)) : NULL;

    // fill out buffer writes

    static const VkDescriptorType atDescriptorTypeLUT[] = {
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    };

    uint32_t uCurrentWrite = 0;
    for (uint32_t i = 0; i < ptData->uBufferCount; i++)
    {

        const plVulkanBuffer* ptVulkanBuffer = &ptDevice->sbtBuffersHot[ptData->atBufferBindings[i].tBuffer.uIndex];

        sbtBufferDescInfos[i].buffer = ptVulkanBuffer->tBuffer;
        sbtBufferDescInfos[i].offset = ptData->atBufferBindings[i].szOffset;
        sbtBufferDescInfos[i].range = ptData->atBufferBindings[i].szBufferRange == 0 ? VK_WHOLE_SIZE : ptData->atBufferBindings[i].szBufferRange;

        sbtWrites[uCurrentWrite].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sbtWrites[uCurrentWrite].dstBinding = ptData->atBufferBindings[i].uSlot;
        sbtWrites[uCurrentWrite].dstArrayElement = 0;
        sbtWrites[uCurrentWrite].descriptorType = atDescriptorTypeLUT[ptBindGroup->tLayout.atBufferBindings[i].tType - 1];
        sbtWrites[uCurrentWrite].descriptorCount = 1;
        sbtWrites[uCurrentWrite].dstSet = ptVulkanBindGroup->tDescriptorSet;
        sbtWrites[uCurrentWrite].pBufferInfo = &sbtBufferDescInfos[i];
        sbtWrites[uCurrentWrite].pNext = NULL;
        uCurrentWrite++;
    }

    // fill out texture writes

    static const VkDescriptorType atTextureDescriptorTypeLUT[] ={
        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    };

    for (uint32_t i = 0; i < ptData->uTextureCount; i++)
    {

        sbtImageDescInfos[i].imageLayout = ptData->atTextureBindings[i].tCurrentUsage == 0 ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : pl__vulkan_layout(ptData->atTextureBindings[i].tCurrentUsage);
        sbtImageDescInfos[i].imageView = ptDevice->sbtTexturesHot[ptData->atTextureBindings[i].tTexture.uIndex].tImageView;
        sbtWrites[uCurrentWrite].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sbtWrites[uCurrentWrite].dstBinding = ptData->atTextureBindings[i].uSlot;
        sbtWrites[uCurrentWrite].dstArrayElement = ptData->atTextureBindings[i].uIndex;
        sbtWrites[uCurrentWrite].descriptorType = atTextureDescriptorTypeLUT[ptData->atTextureBindings[i].tType - 1];
        sbtWrites[uCurrentWrite].descriptorCount = 1;
        sbtWrites[uCurrentWrite].dstSet = ptVulkanBindGroup->tDescriptorSet;
        sbtWrites[uCurrentWrite].pImageInfo = &sbtImageDescInfos[i];
        sbtWrites[uCurrentWrite].pNext = NULL;
        uCurrentWrite++;
    }

    // fill out sampler writes
    for (uint32_t i = 0; i < ptData->uSamplerCount; i++)
    {

        sbtSamplerDescInfos[i].sampler = ptDevice->sbtSamplersHot[ptData->atSamplerBindings[i].tSampler.uIndex];
        sbtWrites[uCurrentWrite].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sbtWrites[uCurrentWrite].dstBinding = ptData->atSamplerBindings[i].uSlot;
        sbtWrites[uCurrentWrite].dstArrayElement = 0;
        sbtWrites[uCurrentWrite].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        sbtWrites[uCurrentWrite].descriptorCount = 1;
        sbtWrites[uCurrentWrite].dstSet = ptVulkanBindGroup->tDescriptorSet;
        sbtWrites[uCurrentWrite].pImageInfo = &sbtSamplerDescInfos[i];
        sbtWrites[uCurrentWrite].pNext = NULL;
        uCurrentWrite++;
    }

    vkUpdateDescriptorSets(ptDevice->tLogicalDevice, uCurrentWrite, sbtWrites, 0, NULL);
    pl_temp_allocator_reset(&gptGraphics->tTempAllocator);
}

plTextureHandle
pl_create_texture(plDevice* ptDevice, const plTextureDesc* ptDesc, plTexture **ptTextureOut)
{
    plTextureDesc tDesc = *ptDesc;

    if (tDesc.pcDebugName == NULL)
        tDesc.pcDebugName = "unnamed texture";

    if (tDesc.uMips == 0)
        tDesc.uMips = (uint32_t)floorf(log2f((float)pl_maxi((int)tDesc.tDimensions.x, (int)tDesc.tDimensions.y))) + 1u;

    plTextureHandle tHandle = pl__get_new_texture_handle(ptDevice);
    plTexture* ptTexture = pl__get_texture(ptDevice, tHandle);
    ptTexture->tDesc = tDesc;
    ptTexture->tView = (plTextureViewDesc){
        .tFormat     = tDesc.tFormat,
        .uBaseMip    = 0,
        .uMips       = tDesc.uMips,
        .uBaseLayer  = 0,
        .uLayerCount = tDesc.uLayers,
        .tTexture    = tHandle
    };

    plVulkanTexture tVulkanTexture = {
        .bOriginalView = true
    };

    VkImageViewType tImageViewType = 0;
    if (tDesc.tType == PL_TEXTURE_TYPE_CUBE)
        tImageViewType = VK_IMAGE_VIEW_TYPE_CUBE;
    else if (tDesc.tType == PL_TEXTURE_TYPE_2D)
        tImageViewType = VK_IMAGE_VIEW_TYPE_2D;
    else if (tDesc.tType == PL_TEXTURE_TYPE_2D_ARRAY)
        tImageViewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    else
    {
        PL_ASSERT(false && "unsupported texture type");
    }

    VkImageUsageFlags tUsageFlags = 0;
    if (tDesc.tUsage & PL_TEXTURE_USAGE_SAMPLED)
        tUsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (tDesc.tUsage & PL_TEXTURE_USAGE_COLOR_ATTACHMENT)
        tUsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (tDesc.tUsage & PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT)
        tUsageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (tDesc.tUsage & PL_TEXTURE_USAGE_TRANSIENT_ATTACHMENT)
        tUsageFlags |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
    if (tDesc.tUsage & PL_TEXTURE_USAGE_INPUT_ATTACHMENT)
        tUsageFlags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    if (tDesc.tUsage & PL_TEXTURE_USAGE_STORAGE)
        tUsageFlags |= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    const VkImageCreateInfo tImageInfo = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .extent.width  = (uint32_t)tDesc.tDimensions.x,
        .extent.height = (uint32_t)tDesc.tDimensions.y,
        .extent.depth  = (uint32_t)tDesc.tDimensions.z,
        .mipLevels     = tDesc.uMips,
        .arrayLayers   = tDesc.uLayers,
        .format        = pl__vulkan_format(tDesc.tFormat),
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage         = tUsageFlags,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .samples       = ptDesc->tSampleCount == 0 ? VK_SAMPLE_COUNT_1_BIT : ptDesc->tSampleCount,
        .flags         = tImageViewType == VK_IMAGE_VIEW_TYPE_CUBE ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0
    };
    PL_VULKAN(vkCreateImage(ptDevice->tLogicalDevice, &tImageInfo, NULL, &tVulkanTexture.tImage));

    // get memory requirements
    VkMemoryRequirements tMemoryRequirements = {0};
    vkGetImageMemoryRequirements(ptDevice->tLogicalDevice, tVulkanTexture.tImage, &tMemoryRequirements);
    ptTexture->tMemoryRequirements.ulSize = tMemoryRequirements.size;
    ptTexture->tMemoryRequirements.ulAlignment = tMemoryRequirements.alignment;
    ptTexture->tMemoryRequirements.uMemoryTypeBits = tMemoryRequirements.memoryTypeBits;

    ptDevice->sbtTexturesHot[tHandle.uIndex] = tVulkanTexture;
    if (ptTextureOut)
       * ptTextureOut = &ptDevice->sbtTexturesCold[tHandle.uIndex];
    return tHandle;
}

void
pl_set_texture_usage(plBlitEncoder* ptEncoder, plTextureHandle tHandle, plTextureUsage tNewUsage, plTextureUsage tOldUsage)
{
    plCommandBuffer* ptCmdBuffer = ptEncoder->ptCommandBuffer;
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    plTexture* ptTexture = pl__get_texture(ptDevice, tHandle);
    plVulkanTexture* ptVulkanTexture = &ptDevice->sbtTexturesHot[tHandle.uIndex];

    VkImageAspectFlags tImageAspectFlags = ptTexture->tDesc.tUsage & PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    if (pl__format_has_stencil(pl__vulkan_format(ptTexture->tDesc.tFormat)))
        tImageAspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;

    VkImageSubresourceRange tRange = {
        .baseMipLevel   = 0,
        .levelCount     = ptTexture->tDesc.uMips,
        .baseArrayLayer = 0,
        .layerCount     = ptTexture->tDesc.uLayers,
        .aspectMask     = tImageAspectFlags
    };
    pl__transition_image_layout(ptCmdBuffer->tCmdBuffer, ptVulkanTexture->tImage, pl__vulkan_layout(tOldUsage), pl__vulkan_layout(tNewUsage), tRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
}

void
pl_bind_texture_to_memory(plDevice* ptDevice, plTextureHandle tHandle, const plDeviceMemoryAllocation* ptAllocation)
{
    plTexture* ptTexture = pl__get_texture(ptDevice, tHandle);
    ptTexture->tMemoryAllocation = *ptAllocation;
    plVulkanTexture* ptVulkanTexture = &ptDevice->sbtTexturesHot[tHandle.uIndex];

    PL_VULKAN(vkBindImageMemory(ptDevice->tLogicalDevice, ptVulkanTexture->tImage, (VkDeviceMemory)ptTexture->tMemoryAllocation.uHandle, ptTexture->tMemoryAllocation.ulOffset));

    VkImageViewType tImageViewType = 0;
    if (ptTexture->tDesc.tType == PL_TEXTURE_TYPE_CUBE)
        tImageViewType = VK_IMAGE_VIEW_TYPE_CUBE;
    else if (ptTexture->tDesc.tType == PL_TEXTURE_TYPE_2D)
        tImageViewType = VK_IMAGE_VIEW_TYPE_2D;
    else if (ptTexture->tDesc.tType == PL_TEXTURE_TYPE_2D_ARRAY)
        tImageViewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    else
    {
        PL_ASSERT(false && "unsupported texture type");
    }

    VkImageViewCreateInfo tViewInfo = {
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image            = ptVulkanTexture->tImage,
        .viewType         = tImageViewType,
        .format           = pl__vulkan_format(ptTexture->tDesc.tFormat),
        .subresourceRange = {
            .baseMipLevel   = ptTexture->tView.uBaseMip,
            .levelCount     = ptTexture->tDesc.uMips,
            .baseArrayLayer = ptTexture->tView.uBaseLayer,
            .layerCount     = ptTexture->tView.uLayerCount,
            .aspectMask     = ptTexture->tDesc.tUsage & PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT
    }

    };
    PL_VULKAN(vkCreateImageView(ptDevice->tLogicalDevice, &tViewInfo, NULL, &ptVulkanTexture->tImageView));
}

plTextureHandle
pl_create_texture_view(plDevice* ptDevice, const plTextureViewDesc* ptViewDesc)
{
    plTextureHandle tHandle = pl__get_new_texture_handle(ptDevice);
    plTexture* ptOriginalTexture = pl__get_texture(ptDevice, ptViewDesc->tTexture);
    plTexture* ptNewTexture = pl__get_texture(ptDevice, ptViewDesc->tTexture);
    ptNewTexture->tDesc = ptOriginalTexture->tDesc;
    ptNewTexture->tView = *ptViewDesc;
    plVulkanTexture* ptOldVulkanTexture = &ptDevice->sbtTexturesHot[ptViewDesc->tTexture.uIndex];
    plVulkanTexture* ptNewVulkanTexture = &ptDevice->sbtTexturesHot[tHandle.uIndex];

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~create view~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    VkImageViewType tImageViewType = 0;
    if (ptOriginalTexture->tDesc.tType == PL_TEXTURE_TYPE_CUBE)
        tImageViewType = VK_IMAGE_VIEW_TYPE_CUBE;
    else if (ptOriginalTexture->tDesc.tType == PL_TEXTURE_TYPE_2D)
        tImageViewType = VK_IMAGE_VIEW_TYPE_2D;
    else if (ptOriginalTexture->tDesc.tType == PL_TEXTURE_TYPE_2D_ARRAY)
        tImageViewType = VK_IMAGE_VIEW_TYPE_2D; // VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    else
    {
        PL_ASSERT(false && "unsupported texture type");
    }

    VkImageAspectFlags tImageAspectFlags = ptOriginalTexture->tDesc.tUsage & PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    const VkImageViewCreateInfo tViewInfo = {
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image            = ptOldVulkanTexture->tImage,
        .viewType         = tImageViewType,
        .format           = pl__vulkan_format(ptViewDesc->tFormat),
        .subresourceRange = {
            .baseMipLevel   = ptViewDesc->uBaseMip,
            .levelCount     = ptViewDesc->uMips == 0 ? ptOriginalTexture->tDesc.uMips - ptViewDesc->uBaseMip : ptViewDesc->uMips,
            .baseArrayLayer = ptViewDesc->uBaseLayer,
            .layerCount     = ptViewDesc->uLayerCount,
            .aspectMask     = tImageAspectFlags
        }
    };
    PL_VULKAN(vkCreateImageView(ptDevice->tLogicalDevice, &tViewInfo, NULL, &ptNewVulkanTexture->tImageView));
    ptNewVulkanTexture->bOriginalView = false;
    return tHandle;
}

plComputeShaderHandle
pl_create_compute_shader(plDevice* ptDevice, const plComputeShaderDesc* ptDescription)
{

    plComputeShaderHandle tHandle = pl__get_new_compute_shader_handle(ptDevice);
    plComputeShader* ptShader = pl__get_compute_shader(ptDevice, tHandle);
    ptShader->tDesc = *ptDescription;
    
    plVulkanComputeShader* ptVulkanShader = &ptDevice->sbtComputeShadersHot[tHandle.uIndex];

    // TODO: handle these properly
    // if(ptDescription->pcShaderEntryFunc == NULL)
    ptShader->tDesc.tShader.pcEntryFunc = "main";

    // setup & count specilization constants
    ptShader->tDesc._uConstantCount = 0;
    ptVulkanShader->szSpecializationSize = 0;
    for (uint32_t i = 0; i < PL_MAX_SHADER_SPECIALIZATION_CONSTANTS; i++)
    {
        const plSpecializationConstant* ptConstant = &ptShader->tDesc.atConstants[i];
        if(ptConstant->tType == PL_DATA_TYPE_UNSPECIFIED)
            break;
        ptVulkanShader->atSpecializationEntries[i].constantID = ptConstant->uID;
        ptVulkanShader->atSpecializationEntries[i].offset = ptConstant->uOffset;
        ptVulkanShader->atSpecializationEntries[i].size = pl__get_data_type_size(ptConstant->tType);
        ptVulkanShader->szSpecializationSize += ptVulkanShader->atSpecializationEntries[i].size;
        ptShader->tDesc._uConstantCount++;
    }

    // setup & count bind groups
    ptShader->tDesc._uBindGroupLayoutCount = 0;
    for (uint32_t i = 0; i < 3; i++)
    {
        if(ptShader->tDesc.atBindGroupLayouts[i].atTextureBindings[0].tStages == PL_STAGE_NONE &&
            ptShader->tDesc.atBindGroupLayouts[i].atBufferBindings[0].tStages == PL_STAGE_NONE &&
            ptShader->tDesc.atBindGroupLayouts[i].atSamplerBindings[0].tStages == PL_STAGE_NONE)
        {
            ptVulkanShader->atDescriptorSetLayouts[i] = ptDevice->tNullDescriptorSetLayout;
        }
        else
        {
            pl__create_bind_group_layout(ptDevice, &ptShader->tDesc.atBindGroupLayouts[i], "compute shader template bind group layout");
            ptVulkanShader->atDescriptorSetLayouts[i] = ptDevice->sbtBindGroupLayouts[ptShader->tDesc.atBindGroupLayouts[i]._uHandle].tDescriptorSetLayout;
            ptShader->tDesc._uBindGroupLayoutCount++;
        }
    }
    ptVulkanShader->atDescriptorSetLayouts[3] = ptDevice->tDynamicDescriptorSetLayout;

    // create shader modules
    VkShaderModuleCreateInfo tShaderCreateInfo = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = ptShader->tDesc.tShader.szCodeSize,
        .pCode    = (const uint32_t *)ptShader->tDesc.tShader.puCode
    };

    PL_VULKAN(vkCreateShaderModule(ptDevice->tLogicalDevice, &tShaderCreateInfo, NULL, &ptVulkanShader->tShaderModule));

    // create pipeline layouts
    VkPipelineLayoutCreateInfo tPipelineLayoutInfo = {
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 4,
        .pSetLayouts    = ptVulkanShader->atDescriptorSetLayouts
        };

    const VkSpecializationInfo tSpecializationInfo = {
        .mapEntryCount = ptShader->tDesc._uConstantCount,
        .pMapEntries   = ptVulkanShader->atSpecializationEntries,
        .dataSize      = ptVulkanShader->szSpecializationSize,
        .pData         = ptDescription->pTempConstantData
    };

    VkPipelineShaderStageCreateInfo tShaderStage = {
        .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage               = VK_SHADER_STAGE_COMPUTE_BIT,
        .module              = ptVulkanShader->tShaderModule,
        .pName               = ptShader->tDesc.tShader.pcEntryFunc,
        .pSpecializationInfo = ptShader->tDesc._uConstantCount > 0 ? &tSpecializationInfo : NULL
    };

    PL_VULKAN(vkCreatePipelineLayout(ptDevice->tLogicalDevice, &tPipelineLayoutInfo, NULL, &ptVulkanShader->tPipelineLayout));

    // create pipeline
    VkComputePipelineCreateInfo tPipelineCreateInfo = {
        .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .layout = ptVulkanShader->tPipelineLayout,
        .stage  = tShaderStage
    };
    PL_VULKAN(vkCreateComputePipelines(ptDevice->tLogicalDevice, VK_NULL_HANDLE, 1, &tPipelineCreateInfo, NULL, &ptVulkanShader->tPipeline));

    return tHandle;
}

plShaderHandle
pl_create_shader(plDevice* ptDevice, const plShaderDesc* ptDescription)
{
    plShaderHandle tHandle = pl__get_new_shader_handle(ptDevice);
    plShader* ptShader = pl__get_shader(ptDevice, tHandle);
    ptShader->tDesc = *ptDescription;
    
    uint32_t uStageCount = 1;

    plVulkanShader* ptVulkanShader = &ptDevice->sbtShadersHot[tHandle.uIndex];

    // TODO: handle these properly

    // if(ptDescription->pcPixelShaderEntryFunc == NULL)
    ptShader->tDesc.tPixelShader.pcEntryFunc = "main";

    // if(ptDescription->pcVertexShaderEntryFunc == NULL)
    ptShader->tDesc.tVertexShader.pcEntryFunc = "main";

    // setup & count bind groups
    ptShader->tDesc._uBindGroupLayoutCount = 0;
    for (uint32_t i = 0; i < 3; i++)
    {
        if(ptShader->tDesc.atBindGroupLayouts[i].atTextureBindings[0].tStages == PL_STAGE_NONE &&
            ptShader->tDesc.atBindGroupLayouts[i].atBufferBindings[0].tStages == PL_STAGE_NONE &&
            ptShader->tDesc.atBindGroupLayouts[i].atSamplerBindings[0].tStages == PL_STAGE_NONE)
        {
            ptVulkanShader->atDescriptorSetLayouts[i] = ptDevice->tNullDescriptorSetLayout;
        }
        else
        {
            pl__create_bind_group_layout(ptDevice, &ptShader->tDesc.atBindGroupLayouts[i], "shader template bind group layout");
            ptVulkanShader->atDescriptorSetLayouts[i] = ptDevice->sbtBindGroupLayouts[ptShader->tDesc.atBindGroupLayouts[i]._uHandle].tDescriptorSetLayout;
            ptShader->tDesc._uBindGroupLayoutCount++;
        }
    }
    ptVulkanShader->atDescriptorSetLayouts[3] = ptDevice->tDynamicDescriptorSetLayout;

    // create shader modules
    VkShaderModuleCreateInfo tVertexShaderCreateInfo = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = ptShader->tDesc.tVertexShader.szCodeSize,
        .pCode    = (const uint32_t *)(ptShader->tDesc.tVertexShader.puCode)
    };
    PL_VULKAN(vkCreateShaderModule(ptDevice->tLogicalDevice, &tVertexShaderCreateInfo, NULL, &ptVulkanShader->tVertexShaderModule));

    if (ptShader->tDesc.tPixelShader.puCode)
    {
        uStageCount++;
        VkShaderModuleCreateInfo tPixelShaderCreateInfo = {
            .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = ptShader->tDesc.tPixelShader.szCodeSize,
            .pCode    = (const uint32_t *)(ptShader->tDesc.tPixelShader.puCode)
        };

        PL_VULKAN(vkCreateShaderModule(ptDevice->tLogicalDevice, &tPixelShaderCreateInfo, NULL, &ptVulkanShader->tPixelShaderModule));
    }

    // setup & count vertex attributes
    VkVertexInputAttributeDescription atAttributeDescription[PL_MAX_VERTEX_ATTRIBUTES] = {0};
    uint32_t uCurrentAttributeCount = 0;
    for (uint32_t i = 0; i < PL_MAX_VERTEX_ATTRIBUTES; i++)
    {
        if (ptDescription->atVertexBufferLayouts[0].atAttributes[i].tFormat == PL_VERTEX_FORMAT_UNKNOWN)
            break;
        atAttributeDescription[i].binding = 0;
        atAttributeDescription[i].location = i;
        atAttributeDescription[i].offset = ptDescription->atVertexBufferLayouts[0].atAttributes[i].uByteOffset;
        atAttributeDescription[i].format = pl__vulkan_vertex_format(ptDescription->atVertexBufferLayouts[0].atAttributes[i].tFormat);
        uCurrentAttributeCount++;
    }

    VkVertexInputBindingDescription tBindingDescription = {
        .binding   = 0,
        .stride    = ptDescription->atVertexBufferLayouts[0].uByteStride,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    VkPipelineVertexInputStateCreateInfo tVertexInputInfo = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 1,
        .vertexAttributeDescriptionCount = uCurrentAttributeCount,
        .pVertexBindingDescriptions      = &tBindingDescription,
        .pVertexAttributeDescriptions    = atAttributeDescription
    };

    // setup & count specilization constants
    ptShader->tDesc._uConstantCount = 0;
    ptVulkanShader->szSpecializationSize = 0;
    for (uint32_t i = 0; i < PL_MAX_SHADER_SPECIALIZATION_CONSTANTS; i++)
    {
        const plSpecializationConstant* ptConstant = &ptShader->tDesc.atConstants[i];
        if(ptConstant->tType == PL_DATA_TYPE_UNSPECIFIED)
            break;
        ptVulkanShader->atSpecializationEntries[i].constantID = ptConstant->uID;
        ptVulkanShader->atSpecializationEntries[i].offset = ptConstant->uOffset;
        ptVulkanShader->atSpecializationEntries[i].size = pl__get_data_type_size(ptConstant->tType);
        ptVulkanShader->szSpecializationSize += ptVulkanShader->atSpecializationEntries[i].size;
        ptShader->tDesc._uConstantCount++;
    }

    const VkSpecializationInfo tSpecializationInfo = {
        .mapEntryCount = ptShader->tDesc._uConstantCount,
        .pMapEntries   = ptVulkanShader->atSpecializationEntries,
        .dataSize      = ptVulkanShader->szSpecializationSize,
        .pData         = ptDescription->pTempConstantData
    };

    // create pipeline layout
    plVulkanShader tVulkanShader = {0};
    VkPipelineLayoutCreateInfo tPipelineLayoutInfo = {
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 4,
        .pSetLayouts    = ptVulkanShader->atDescriptorSetLayouts
    };
    PL_VULKAN(vkCreatePipelineLayout(ptDevice->tLogicalDevice, &tPipelineLayoutInfo, NULL, &tVulkanShader.tPipelineLayout));

    VkPipelineShaderStageCreateInfo tVertShaderStageInfo = {
        .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage               = VK_SHADER_STAGE_VERTEX_BIT,
        .module              = ptVulkanShader->tVertexShaderModule,
        .pName               = ptShader->tDesc.tVertexShader.pcEntryFunc,
        .pSpecializationInfo = ptShader->tDesc._uConstantCount > 0 ? &tSpecializationInfo : NULL
    };

    // doesn't matter since dynamic
    VkViewport tViewport = {
        .x        = 0.0f,
        .y        = 0.0f,
        .width    = 100.0f,
        .height   = 100.0f,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    // doesn't matter since dynamic
    VkRect2D tScissor = {
        .extent = {
            .width  = 100,
            .height = 100
        }
        };

    VkPipelineViewportStateCreateInfo tViewportState = {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports    = &tViewport,
        .scissorCount  = 1,
        .pScissors     = &tScissor
    };

    VkPipelineRasterizationStateCreateInfo tRasterizer = {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable        = ptDescription->tGraphicsState.ulDepthClampEnabled ? VK_TRUE : VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode             = ptDescription->tGraphicsState.ulWireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL,
        .lineWidth               = 1.0f,
        .cullMode                = pl__vulkan_cull((plCullMode)ptDescription->tGraphicsState.ulCullMode),
        .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable         = VK_TRUE
    };

    VkPipelineShaderStageCreateInfo tFragShaderStageInfo = {
        .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage               = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module              = ptVulkanShader->tPixelShaderModule,
        .pName               = ptShader->tDesc.tPixelShader.pcEntryFunc,
        .pSpecializationInfo = &tSpecializationInfo
    };

    VkPipelineDepthStencilStateCreateInfo tDepthStencil = {
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable       = ptDescription->tGraphicsState.ulDepthMode == PL_COMPARE_MODE_ALWAYS ? VK_FALSE : VK_TRUE,
        .depthWriteEnable      = ptDescription->tGraphicsState.ulDepthWriteEnabled ? VK_TRUE : VK_FALSE,
        .depthCompareOp        = pl__vulkan_compare((plCompareMode)ptDescription->tGraphicsState.ulDepthMode),
        .depthBoundsTestEnable = VK_FALSE,
        .minDepthBounds        = 0.0f, // Optional,
        .maxDepthBounds        = 1.0f, // Optional,
        .stencilTestEnable     = ptDescription->tGraphicsState.ulStencilTestEnabled ? VK_TRUE : VK_FALSE,
        .back                  = {
            .compareOp   = pl__vulkan_compare((plCompareMode)ptDescription->tGraphicsState.ulStencilMode),
            .failOp      = pl__vulkan_stencil_op((plStencilOp)ptDescription->tGraphicsState.ulStencilOpFail),
            .depthFailOp = pl__vulkan_stencil_op((plStencilOp)ptDescription->tGraphicsState.ulStencilOpDepthFail),
            .passOp      = pl__vulkan_stencil_op((plStencilOp)ptDescription->tGraphicsState.ulStencilOpPass),
            .compareMask = (uint32_t)ptDescription->tGraphicsState.ulStencilMask,
            .writeMask   = (uint32_t)ptDescription->tGraphicsState.ulStencilMask,
            .reference   = (uint32_t)ptDescription->tGraphicsState.ulStencilRef
        }
    };
    tDepthStencil.front = tDepthStencil.back;

    // color blending
    VkPipelineColorBlendAttachmentState atColorBlendAttachment[PL_MAX_RENDER_TARGETS] = {0};
    for (uint32_t i = 0; i < PL_MAX_RENDER_TARGETS; i++)
    {
        atColorBlendAttachment[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        atColorBlendAttachment[i].blendEnable = ptDescription->atBlendStates[i].bBlendEnabled ? VK_TRUE : VK_FALSE;
        if (ptDescription->atBlendStates[i].bBlendEnabled)
        {
            atColorBlendAttachment[i].srcColorBlendFactor = pl__vulkan_blend_factor(ptDescription->atBlendStates[i].tSrcColorFactor);
            atColorBlendAttachment[i].dstColorBlendFactor = pl__vulkan_blend_factor(ptDescription->atBlendStates[i].tDstColorFactor);
            atColorBlendAttachment[i].colorBlendOp = pl__vulkan_blend_op(ptDescription->atBlendStates[i].tColorOp);
            atColorBlendAttachment[i].srcAlphaBlendFactor = pl__vulkan_blend_factor(ptDescription->atBlendStates[i].tSrcAlphaFactor);
            atColorBlendAttachment[i].dstAlphaBlendFactor = pl__vulkan_blend_factor(ptDescription->atBlendStates[i].tDstAlphaFactor);
            atColorBlendAttachment[i].alphaBlendOp = pl__vulkan_blend_op(ptDescription->atBlendStates[i].tAlphaOp);
        }
    }

    const plRenderPassLayout* ptLayout = pl_get_render_pass_layout(ptDevice, ptDescription->tRenderPassLayout);

    VkPipelineColorBlendStateCreateInfo tColorBlending = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable   = VK_FALSE,
        .logicOp         = VK_LOGIC_OP_COPY,
        .attachmentCount = ptLayout->tDesc.atSubpasses[ptDescription->uSubpassIndex]._uColorAttachmentCount,
        .pAttachments    = atColorBlendAttachment,
        .blendConstants  = {0}
    };

    // multisampling
    VkPipelineMultisampleStateCreateInfo tMultisampling = {
        .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable  = false,
        .minSampleShading     = 0.2f,
        .rasterizationSamples = ptDescription->tMSAASampleCount == 0 ? 1 : ptDescription->tMSAASampleCount
    };

    // create pipelines
    VkPipelineInputAssemblyStateCreateInfo tInputAssembly = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    VkPipelineShaderStageCreateInfo shaderStages[] = {
        tVertShaderStageInfo,
        tFragShaderStageInfo
    };

    VkDynamicState tDynamicStateEnables[3] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_DEPTH_BIAS};
    VkPipelineDynamicStateCreateInfo tDynamicState = {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 3,
        .pDynamicStates    = tDynamicStateEnables
    };

    VkGraphicsPipelineCreateInfo tPipelineInfo = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount          = uStageCount,
        .pStages             = shaderStages,
        .pVertexInputState   = &tVertexInputInfo,
        .pInputAssemblyState = &tInputAssembly,
        .pViewportState      = &tViewportState,
        .pRasterizationState = &tRasterizer,
        .pMultisampleState   = &tMultisampling,
        .pColorBlendState    = &tColorBlending,
        .pDynamicState       = &tDynamicState,
        .layout              = tVulkanShader.tPipelineLayout,
        .renderPass          = ptDevice->sbtRenderPassLayoutsHot[ptShader->tDesc.tRenderPassLayout.uIndex].tRenderPass,
        .subpass             = ptShader->tDesc.uSubpassIndex,
        .basePipelineHandle  = VK_NULL_HANDLE,
        .pDepthStencilState  = &tDepthStencil
    };

    PL_VULKAN(vkCreateGraphicsPipelines(ptDevice->tLogicalDevice, VK_NULL_HANDLE, 1, &tPipelineInfo, NULL, &tVulkanShader.tPipeline));
    ptVulkanShader->tPipeline       = tVulkanShader.tPipeline;
    ptVulkanShader->tPipelineLayout = tVulkanShader.tPipelineLayout;
    return tHandle;
}

plTextureHandle*
pl_get_swapchain_images(plSwapchain* ptSwap, uint32_t* puSizeOut)
{
    if (puSizeOut)
       *puSizeOut = ptSwap->uImageCount;
    return ptSwap->sbtSwapchainTextureViews;
}

plRenderPassLayoutHandle
pl_create_render_pass_layout(plDevice* ptDevice, const plRenderPassLayoutDesc* ptDesc)
{
    const plRenderPassLayoutHandle tHandle = pl__get_new_render_pass_layout_handle(ptDevice);
    plRenderPassLayout* ptLayout = pl_get_render_pass_layout(ptDevice, tHandle);
    ptLayout->tDesc = *ptDesc;

    plRenderPassCommonData tCommonData = {0};
    pl__fill_common_render_pass_data(&ptLayout->tDesc, ptLayout, &tCommonData);

    plVulkanRenderPassLayout tVulkanRenderPassLayout = {0};

    const VkRenderPassCreateInfo tRenderPassInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = ptLayout->_uAttachmentCount,
        .pAttachments    = tCommonData.atAttachments,
        .subpassCount    = ptLayout->tDesc._uSubpassCount,
        .pSubpasses      = tCommonData.atSubpasses,
        .dependencyCount = tCommonData.uDependencyCount,
        .pDependencies   = tCommonData.atSubpassDependencies
    };
    PL_VULKAN(vkCreateRenderPass(ptDevice->tLogicalDevice, &tRenderPassInfo, NULL, &tVulkanRenderPassLayout.tRenderPass));

    ptDevice->sbtRenderPassLayoutsHot[tHandle.uIndex] = tVulkanRenderPassLayout;
    return tHandle;
}

plRenderPassHandle
pl_create_render_pass(plDevice* ptDevice, const plRenderPassDesc* ptDesc, const plRenderPassAttachments* ptAttachments)
{

    plRenderPassHandle tHandle = pl__get_new_render_pass_handle(ptDevice);
    plRenderPass* ptPass = pl_get_render_pass(ptDevice, tHandle);
    ptPass->tDesc = *ptDesc;

    plRenderPassLayout* ptLayout = pl_get_render_pass_layout(ptDevice, ptDesc->tLayout);

    plRenderPassCommonData tCommonData = {0};
    pl__fill_common_render_pass_data(&ptLayout->tDesc, ptLayout, &tCommonData);

    plVulkanRenderPass* ptVulkanRenderPass = &ptDevice->sbtRenderPassesHot[tHandle.uIndex];

    // find attachment count & fill out descriptions
    uint32_t uColorAttachmentCount = 0;
    for (uint32_t i = 0; i < PL_MAX_RENDER_TARGETS; i++)
    {
        if (ptLayout->tDesc.atRenderTargets[i].tFormat == PL_FORMAT_UNKNOWN)
            break;

        tCommonData.atAttachments[i].samples = ptLayout->tDesc.atRenderTargets[i].tSamples == 0 ? 1 : ptLayout->tDesc.atRenderTargets[i].tSamples;
        if (ptLayout->tDesc.atRenderTargets[i].bDepth)
        {
            tCommonData.atAttachments[i].loadOp = pl__vulkan_load_op(ptDesc->tDepthTarget.tLoadOp);
            tCommonData.atAttachments[i].storeOp = pl__vulkan_store_op(ptDesc->tDepthTarget.tStoreOp);
            tCommonData.atAttachments[i].stencilLoadOp = pl__vulkan_load_op(ptDesc->tDepthTarget.tStencilLoadOp);
            tCommonData.atAttachments[i].stencilStoreOp = pl__vulkan_store_op(ptDesc->tDepthTarget.tStencilStoreOp);
            tCommonData.atAttachments[i].initialLayout = pl__vulkan_layout(ptDesc->tDepthTarget.tCurrentUsage);
            tCommonData.atAttachments[i].finalLayout = pl__vulkan_layout(ptDesc->tDepthTarget.tNextUsage);
        }
        else if (ptLayout->tDesc.atRenderTargets[i].bResolve)
        {
            tCommonData.atAttachments[i].loadOp = pl__vulkan_load_op(ptDesc->tResolveTarget.tLoadOp);
            tCommonData.atAttachments[i].storeOp = pl__vulkan_store_op(ptDesc->tResolveTarget.tStoreOp);
            tCommonData.atAttachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            tCommonData.atAttachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            tCommonData.atAttachments[i].initialLayout = pl__vulkan_layout(ptDesc->tResolveTarget.tCurrentUsage);
            tCommonData.atAttachments[i].finalLayout = pl__vulkan_layout(ptDesc->tResolveTarget.tNextUsage);
        }
        else
        {
            // from description
            tCommonData.atAttachments[i].loadOp = pl__vulkan_load_op(ptDesc->atColorTargets[uColorAttachmentCount].tLoadOp);
            tCommonData.atAttachments[i].storeOp = pl__vulkan_store_op(ptDesc->atColorTargets[uColorAttachmentCount].tStoreOp);
            tCommonData.atAttachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            tCommonData.atAttachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            tCommonData.atAttachments[i].initialLayout = pl__vulkan_layout(ptDesc->atColorTargets[uColorAttachmentCount].tCurrentUsage);
            tCommonData.atAttachments[i].finalLayout = pl__vulkan_layout(ptDesc->atColorTargets[uColorAttachmentCount].tNextUsage);
            uColorAttachmentCount++;
        }
    }

    const VkRenderPassCreateInfo tRenderPassInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = ptLayout->_uAttachmentCount,
        .pAttachments    = tCommonData.atAttachments,
        .subpassCount    = ptLayout->tDesc._uSubpassCount,
        .pSubpasses      = tCommonData.atSubpasses,
        .dependencyCount = tCommonData.uDependencyCount,
        .pDependencies   = tCommonData.atSubpassDependencies
    };
    PL_VULKAN(vkCreateRenderPass(ptDevice->tLogicalDevice, &tRenderPassInfo, NULL, &ptVulkanRenderPass->tRenderPass));

    uint32_t uCount = gptGraphics->uFramesInFlight;
    if (ptDesc->ptSwapchain)
        uCount = ptDesc->ptSwapchain->uImageCount;

    for (uint32_t i = 0; i < uCount; i++)
    {
        VkImageView atViewAttachments[PL_MAX_RENDER_TARGETS] = {0};

        for (uint32_t j = 0; j < ptLayout->_uAttachmentCount; j++)
        {
            atViewAttachments[j] = ptDevice->sbtTexturesHot[ptAttachments[i].atViewAttachments[j].uIndex].tImageView;
        }

        VkFramebufferCreateInfo tFrameBufferInfo = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = ptVulkanRenderPass->tRenderPass,
            .attachmentCount = ptLayout->_uAttachmentCount,
            .pAttachments    = atViewAttachments,
            .width           = (uint32_t)ptDesc->tDimensions.x,
            .height          = (uint32_t)ptDesc->tDimensions.y,
            .layers          = 1
        };
        PL_VULKAN(vkCreateFramebuffer(ptDevice->tLogicalDevice, &tFrameBufferInfo, NULL, &ptVulkanRenderPass->atFrameBuffers[i]));
    }
    return tHandle;
}

void
pl_update_render_pass_attachments(plDevice* ptDevice, plRenderPassHandle tHandle, plVec2 tDimensions, const plRenderPassAttachments* ptAttachments)
{
    plRenderPass* ptRenderPass = pl_get_render_pass(ptDevice, tHandle);

    plVulkanRenderPass* ptVulkanRenderPass = &ptDevice->sbtRenderPassesHot[tHandle.uIndex];
    plRenderPassLayout* ptLayout = pl_get_render_pass_layout(ptDevice, ptRenderPass->tDesc.tLayout);
    plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);
    ptRenderPass->tDesc.tDimensions = tDimensions;

    const plRenderPassDesc* ptDesc = &ptRenderPass->tDesc;

    uint32_t uCount = gptGraphics->uFramesInFlight;
    if (ptDesc->ptSwapchain)
        uCount = ptDesc->ptSwapchain->uImageCount;

    for (uint32_t i = 0; i < uCount; i++)
    {

        VkImageView atViewAttachments[PL_MAX_RENDER_TARGETS] = {0};

        for (uint32_t j = 0; j < ptLayout->_uAttachmentCount; j++)
        {
            atViewAttachments[j] = ptDevice->sbtTexturesHot[ptAttachments[i].atViewAttachments[j].uIndex].tImageView;
        }

        VkFramebufferCreateInfo tFrameBufferInfo = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = ptVulkanRenderPass->tRenderPass,
            .attachmentCount = ptLayout->_uAttachmentCount,
            .pAttachments    = atViewAttachments,
            .width           = (uint32_t)ptDesc->tDimensions.x,
            .height          = (uint32_t)ptDesc->tDimensions.y,
            .layers          = 1
        };
        pl_sb_push(ptFrame->sbtRawFrameBuffers, ptVulkanRenderPass->atFrameBuffers[i]);
        ptVulkanRenderPass->atFrameBuffers[i] = VK_NULL_HANDLE;
        PL_VULKAN(vkCreateFramebuffer(ptDevice->tLogicalDevice, &tFrameBufferInfo, NULL, &ptVulkanRenderPass->atFrameBuffers[i]));
    }
}

void
pl_begin_command_recording(plCommandBuffer* ptCommandBuffer, const plBeginCommandInfo* ptBeginInfo)
{
    const VkCommandBufferBeginInfo tBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = 0
    };
    PL_VULKAN(vkBeginCommandBuffer(ptCommandBuffer->tCmdBuffer, &tBeginInfo));

    if (ptBeginInfo)
        ptCommandBuffer->tBeginInfo = *ptBeginInfo;
    else
        ptCommandBuffer->tBeginInfo.uWaitSemaphoreCount = UINT32_MAX;
}

plRenderEncoder*
pl_begin_render_pass(plCommandBuffer* ptCmdBuffer, plRenderPassHandle tPass, const plPassResources* ptResource)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    plRenderPass* ptRenderPass = &ptDevice->sbtRenderPassesCold[tPass.uIndex];
    plVulkanRenderPass* ptVulkanRenderPass = &ptDevice->sbtRenderPassesHot[tPass.uIndex];
    plRenderPassLayout* ptLayout = &ptDevice->sbtRenderPassLayoutsCold[ptRenderPass->tDesc.tLayout.uIndex];

    if (ptRenderPass->tDesc.ptSwapchain)
    {
        VkClearValue atClearValues[PL_MAX_RENDER_TARGETS] = {0};

        uint32_t uAttachmentCount = 0;

        for (uint32_t i = 0; i < PL_MAX_RENDER_TARGETS; i++)
        {

            if (ptLayout->tDesc.atRenderTargets[i].tFormat == PL_FORMAT_UNKNOWN)
                break;

            if (ptLayout->tDesc.atRenderTargets[i].bDepth)
            {
                atClearValues[i].depthStencil.depth = ptRenderPass->tDesc.tDepthTarget.fClearZ;
                atClearValues[i].depthStencil.stencil = ptRenderPass->tDesc.tDepthTarget.uClearStencil;
            }
            else
            {
                atClearValues[i].color.float32[0] = ptRenderPass->tDesc.atColorTargets[i].tClearColor.r;
                atClearValues[i].color.float32[1] = ptRenderPass->tDesc.atColorTargets[i].tClearColor.g;
                atClearValues[i].color.float32[2] = ptRenderPass->tDesc.atColorTargets[i].tClearColor.b;
                atClearValues[i].color.float32[3] = ptRenderPass->tDesc.atColorTargets[i].tClearColor.a;
            }
            uAttachmentCount++;
        }

        VkRenderPassBeginInfo tRenderPassInfo = {
            .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass      = ptVulkanRenderPass->tRenderPass,
            .framebuffer     = ptVulkanRenderPass->atFrameBuffers[ptRenderPass->tDesc.ptSwapchain->uCurrentImageIndex],
            .clearValueCount = uAttachmentCount,
            .pClearValues    = atClearValues,
            .renderArea = {
                .extent = {
                    .width = (uint32_t)ptRenderPass->tDesc.tDimensions.x,
                    .height = (uint32_t)ptRenderPass->tDesc.tDimensions.y,
                }
            }
        };

        vkCmdBeginRenderPass(ptCmdBuffer->tCmdBuffer, &tRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }
    else
    {
        VkClearValue atClearValues[PL_MAX_RENDER_TARGETS] = {0};

        uint32_t uAttachmentCount = 0;

        for (uint32_t i = 0; i < PL_MAX_RENDER_TARGETS; i++)
        {

            if (ptLayout->tDesc.atRenderTargets[i].tFormat == PL_FORMAT_UNKNOWN)
                break;

            if (ptLayout->tDesc.atRenderTargets[i].bDepth)
            {
                atClearValues[i].depthStencil.depth = ptRenderPass->tDesc.tDepthTarget.fClearZ;
                atClearValues[i].depthStencil.stencil = ptRenderPass->tDesc.tDepthTarget.uClearStencil;
            }
            else
            {
                atClearValues[i].color.float32[0] = ptRenderPass->tDesc.atColorTargets[i].tClearColor.r;
                atClearValues[i].color.float32[1] = ptRenderPass->tDesc.atColorTargets[i].tClearColor.g;
                atClearValues[i].color.float32[2] = ptRenderPass->tDesc.atColorTargets[i].tClearColor.b;
                atClearValues[i].color.float32[3] = ptRenderPass->tDesc.atColorTargets[i].tClearColor.a;
            }
            uAttachmentCount++;
        }

        VkRenderPassBeginInfo tRenderPassInfo = {
            .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass      = ptVulkanRenderPass->tRenderPass,
            .framebuffer     = ptVulkanRenderPass->atFrameBuffers[gptGraphics->uCurrentFrameIndex],
            .clearValueCount = uAttachmentCount,
            .pClearValues    = atClearValues,
            .renderArea = {
                .extent = {
                    .width = (uint32_t)ptRenderPass->tDesc.tDimensions.x,
                    .height = (uint32_t)ptRenderPass->tDesc.tDimensions.y
                }
            }
        };

        vkCmdBeginRenderPass(ptCmdBuffer->tCmdBuffer, &tRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    const VkRect2D tScissor = {
        .extent = {
            .width = (uint32_t)ptRenderPass->tDesc.tDimensions.x,
            .height = (uint32_t)ptRenderPass->tDesc.tDimensions.y
        }
    };

    const VkViewport tViewport = {
        .width    = ptRenderPass->tDesc.tDimensions.x,
        .height   = ptRenderPass->tDesc.tDimensions.y,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    vkCmdSetViewport(ptCmdBuffer->tCmdBuffer, 0, 1, &tViewport);
    vkCmdSetScissor(ptCmdBuffer->tCmdBuffer, 0, 1, &tScissor);
    vkCmdSetDepthBias(ptCmdBuffer->tCmdBuffer, 0.0f, 0.0f, 1.0f);

    plRenderEncoder* ptEncoder = pl__get_new_render_encoder();
    ptEncoder->ptCommandBuffer = ptCmdBuffer;
    ptEncoder->tRenderPassHandle = tPass;
    ptEncoder->uCurrentSubpass = 0;
    return ptEncoder;
}

void
pl_next_subpass(plRenderEncoder* ptEncoder, const plPassResources* ptResource)
{
    plCommandBuffer* ptCmdBuffer = ptEncoder->ptCommandBuffer;
    ptEncoder->uCurrentSubpass++;
    vkCmdNextSubpass(ptCmdBuffer->tCmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
}

void
pl_end_render_pass(plRenderEncoder* ptEncoder)
{
    plCommandBuffer* ptCmdBuffer = ptEncoder->ptCommandBuffer;
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    plRenderPass* ptRenderPass = &ptDevice->sbtRenderPassesCold[ptEncoder->tRenderPassHandle.uIndex];
    plRenderPassLayout* ptLayout = &ptDevice->sbtRenderPassLayoutsCold[ptRenderPass->tDesc.tLayout.uIndex];

    while(ptEncoder->uCurrentSubpass < ptLayout->tDesc._uSubpassCount - 1)
    {
        vkCmdNextSubpass(ptCmdBuffer->tCmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
        ptEncoder->uCurrentSubpass++;
    }
    vkCmdEndRenderPass(ptCmdBuffer->tCmdBuffer);

    pl__return_render_encoder(ptEncoder);
}

void
pl_bind_vertex_buffer(plRenderEncoder* ptEncoder, plBufferHandle tHandle)
{
    plCommandBuffer* ptCmdBuffer = ptEncoder->ptCommandBuffer;
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    plVulkanBuffer* ptVertexBuffer = &ptDevice->sbtBuffersHot[tHandle.uIndex];
    static VkDeviceSize offsets = {0};
    vkCmdBindVertexBuffers(ptCmdBuffer->tCmdBuffer, 0, 1, &ptVertexBuffer->tBuffer, &offsets);
}

void
pl_draw(plRenderEncoder* ptEncoder, uint32_t uCount, const plDraw *atDraws)
{
    plCommandBuffer* ptCmdBuffer = ptEncoder->ptCommandBuffer;
    for (uint32_t i = 0; i < uCount; i++)
    {
        vkCmdDraw(ptCmdBuffer->tCmdBuffer, atDraws[i].uVertexCount, atDraws[i].uInstanceCount, atDraws[i].uVertexStart, atDraws[i].uInstance);
    }
}

void
pl_draw_indexed(plRenderEncoder* ptEncoder, uint32_t uCount, const plDrawIndex *atDraws)
{
    plCommandBuffer* ptCmdBuffer = ptEncoder->ptCommandBuffer;
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    uint32_t uCurrentIndexBuffer = UINT32_MAX;

    for (uint32_t i = 0; i < uCount; i++)
    {
        if (atDraws->tIndexBuffer.uIndex != uCurrentIndexBuffer)
        {
            uCurrentIndexBuffer = atDraws->tIndexBuffer.uIndex;
            plVulkanBuffer* ptIndexBuffer = &ptDevice->sbtBuffersHot[uCurrentIndexBuffer];
            vkCmdBindIndexBuffer(ptCmdBuffer->tCmdBuffer, ptIndexBuffer->tBuffer, 0, VK_INDEX_TYPE_UINT32);
        }
        vkCmdDrawIndexed(ptCmdBuffer->tCmdBuffer, atDraws[i].uIndexCount, atDraws[i].uInstanceCount, atDraws[i].uIndexStart, atDraws[i].uVertexStart, atDraws[i].uInstance);
    }
}

void
pl_bind_shader(plRenderEncoder* ptEncoder, plShaderHandle tHandle)
{
    plCommandBuffer* ptCmdBuffer = ptEncoder->ptCommandBuffer;
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    plVulkanShader* ptVulkanShader = &ptDevice->sbtShadersHot[tHandle.uIndex];
    vkCmdBindPipeline(ptCmdBuffer->tCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ptVulkanShader->tPipeline);
}

void
pl_bind_compute_shader(plComputeEncoder* ptEncoder, plComputeShaderHandle tHandle)
{
    plCommandBuffer* ptCmdBuffer = ptEncoder->ptCommandBuffer;
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    plVulkanComputeShader* ptVulkanShader = &ptDevice->sbtComputeShadersHot[tHandle.uIndex];
    vkCmdBindPipeline(ptCmdBuffer->tCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ptVulkanShader->tPipeline);
}

void
pl_draw_stream(plRenderEncoder* ptEncoder, uint32_t uAreaCount, plDrawArea *atAreas)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);
    plCommandBuffer* ptCmdBuffer = ptEncoder->ptCommandBuffer;
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptDevice);

    static VkDeviceSize offsets = {0};

    for (uint32_t i = 0; i < uAreaCount; i++)
    {
        plDrawArea* ptArea = &atAreas[i];

        VkRect2D atScissors[PL_MAX_VIEWPORTS] = {0};
        VkViewport atViewports[PL_MAX_VIEWPORTS] = {0};

        uint32_t uViewportCount = 0;

        for(uint32_t j = 0; j < PL_MAX_VIEWPORTS; j++)
        {

            if(ptArea->atViewports[j].fWidth == 0.0f)
            {
                break;
            }

            atScissors[j] = (VkRect2D){
                .offset = {
                    .x = ptArea->atScissors[j].iOffsetX,
                    .y = ptArea->atScissors[j].iOffsetY
                },
                .extent = {
                    .width = ptArea->atScissors[j].uWidth,
                    .height = ptArea->atScissors[j].uHeight
                }
            };

            atViewports[j] = (VkViewport){
                .x        = ptArea->atViewports[j].fX,
                .y        = ptArea->atViewports[j].fY,
                .width    = ptArea->atViewports[j].fWidth,
                .height   = ptArea->atViewports[j].fHeight,
                .minDepth = ptArea->atViewports[j].fMinDepth,
                .maxDepth = ptArea->atViewports[j].fMaxDepth
            };

            uViewportCount++;
        }

        vkCmdSetViewport(ptCmdBuffer->tCmdBuffer, 0, uViewportCount, atViewports);
        vkCmdSetScissor(ptCmdBuffer->tCmdBuffer, 0, uViewportCount, atScissors);

        plDrawStream* ptStream = ptArea->ptDrawStream;

        const uint32_t uTokens = ptStream->_uStreamCount;
        uint32_t uCurrentStreamIndex = 0;
        uint32_t uTriangleCount = 0;
        plBufferHandle tIndexBuffer = {0};
        uint32_t uIndexBufferOffset = 0;
        uint32_t uVertexBufferOffset = 0;
        uint32_t uDynamicBufferOffset0 = 0;
        uint32_t uInstanceStart = 0;
        uint32_t uInstanceCount = 1;
        plVulkanShader* ptVulkanShader = NULL;
        plVulkanDynamicBuffer* ptVulkanDynamicBuffer = NULL;

        VkDescriptorSet auSlots[4] = {
            ptDevice->tNullDecriptorSet,
            ptDevice->tNullDecriptorSet,
            ptDevice->tNullDecriptorSet,
            ptDevice->tNullDynamicDecriptorSet
        };

        uint32_t uUpdateCount = 4;

        while (uCurrentStreamIndex < uTokens)
        {

            const uint32_t uDirtyMask = ptStream->_auStream[uCurrentStreamIndex];
            uCurrentStreamIndex++;

            if (uDirtyMask & PL_DRAW_STREAM_BIT_SHADER)
            {
                const plShaderHandle tShaderHandle = {.uData = ptStream->_auStream[uCurrentStreamIndex] };
                const plShader* ptShader = &ptDevice->sbtShadersCold[tShaderHandle.uIndex];
                ptVulkanShader = &ptDevice->sbtShadersHot[tShaderHandle.uIndex];
                vkCmdBindPipeline(ptCmdBuffer->tCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ptVulkanShader->tPipeline);
                uUpdateCount = 4;
                uCurrentStreamIndex++;
            }

            if (uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_OFFSET_0)
            {
                uDynamicBufferOffset0 = ptStream->_auStream[uCurrentStreamIndex];
                uCurrentStreamIndex++;
            }

            if (uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_0)
            {
                const plBindGroupHandle tBindGroupHandle = {.uData = ptStream->_auStream[uCurrentStreamIndex] };
                plVulkanBindGroup* ptBindGroup0 = &ptDevice->sbtBindGroupsHot[tBindGroupHandle.uIndex];
                auSlots[0] = ptBindGroup0->tDescriptorSet;
                uUpdateCount = pl_max(3, uUpdateCount);
                uCurrentStreamIndex++;
            }

            if (uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_1)
            {
                const plBindGroupHandle tBindGroupHandle = {.uData = ptStream->_auStream[uCurrentStreamIndex] };
                plVulkanBindGroup* ptBindGroup1 = &ptDevice->sbtBindGroupsHot[tBindGroupHandle.uIndex];
                uUpdateCount = pl_max(2, uUpdateCount);
                auSlots[1] = ptBindGroup1->tDescriptorSet;
                uCurrentStreamIndex++;
            }

            if (uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_2)
            {
                const plBindGroupHandle tBindGroupHandle = {.uData = ptStream->_auStream[uCurrentStreamIndex] };
                plVulkanBindGroup* ptBindGroup2 = &ptDevice->sbtBindGroupsHot[tBindGroupHandle.uIndex];
                uUpdateCount = pl_max(1, uUpdateCount);
                auSlots[2] = ptBindGroup2->tDescriptorSet;
                uCurrentStreamIndex++;
            }

            if (uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_BUFFER_0)
            {
                ptVulkanDynamicBuffer = &ptCurrentFrame->sbtDynamicBuffers[ptStream->_auStream[uCurrentStreamIndex]];
                auSlots[3] = ptVulkanDynamicBuffer->tDescriptorSet;
                uUpdateCount = pl_max(1, uUpdateCount);
                uCurrentStreamIndex++;
            }

            if (uDirtyMask & PL_DRAW_STREAM_BIT_INDEX_OFFSET)
            {
                uIndexBufferOffset = ptStream->_auStream[uCurrentStreamIndex];
                uCurrentStreamIndex++;
            }
            if (uDirtyMask & PL_DRAW_STREAM_BIT_VERTEX_OFFSET)
            {
                uVertexBufferOffset = ptStream->_auStream[uCurrentStreamIndex];
                uCurrentStreamIndex++;
            }
            if (uDirtyMask & PL_DRAW_STREAM_BIT_INDEX_BUFFER)
            {

                tIndexBuffer = (plBufferHandle){.uData = ptStream->_auStream[uCurrentStreamIndex] };
                if (tIndexBuffer.uData != 0)
                {
                    plVulkanBuffer* ptIndexBuffer = &ptDevice->sbtBuffersHot[tIndexBuffer.uIndex];
                    vkCmdBindIndexBuffer(ptCmdBuffer->tCmdBuffer, ptIndexBuffer->tBuffer, 0, VK_INDEX_TYPE_UINT32);
                }
                uCurrentStreamIndex++;
            }
            if (uDirtyMask & PL_DRAW_STREAM_BIT_VERTEX_BUFFER_0)
            {
                const plBufferHandle tBufferHandle = {.uData = ptStream->_auStream[uCurrentStreamIndex] };
                plVulkanBuffer* ptVertexBuffer = &ptDevice->sbtBuffersHot[tBufferHandle.uIndex];
                vkCmdBindVertexBuffers(ptCmdBuffer->tCmdBuffer, 0, 1, &ptVertexBuffer->tBuffer, &offsets);
                uCurrentStreamIndex++;
            }
            if (uDirtyMask & PL_DRAW_STREAM_BIT_TRIANGLES)
            {
                uTriangleCount = ptStream->_auStream[uCurrentStreamIndex];
                uCurrentStreamIndex++;
            }

            if (uDirtyMask & PL_DRAW_STREAM_BIT_INSTANCE_OFFSET)
            {
                uInstanceStart = ptStream->_auStream[uCurrentStreamIndex];
                uCurrentStreamIndex++;
            }

            if (uDirtyMask & PL_DRAW_STREAM_BIT_INSTANCE_COUNT)
            {
                uInstanceCount = ptStream->_auStream[uCurrentStreamIndex];
                uCurrentStreamIndex++;
            }

            vkCmdBindDescriptorSets(ptCmdBuffer->tCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                ptVulkanShader->tPipelineLayout, 4 - uUpdateCount, uUpdateCount, auSlots, 1, &uDynamicBufferOffset0);

            if (tIndexBuffer.uData == 0)
                vkCmdDraw(ptCmdBuffer->tCmdBuffer, uTriangleCount * 3, uInstanceCount, uVertexBufferOffset, uInstanceStart);
            else
                vkCmdDrawIndexed(ptCmdBuffer->tCmdBuffer, uTriangleCount * 3, uInstanceCount, uIndexBufferOffset, uVertexBufferOffset, uInstanceStart);
        }
    }
    pl_end_cpu_sample(gptProfile, 0);
}

void
pl_set_depth_bias(plRenderEncoder* ptEncoder, float fDepthBiasConstantFactor, float fDepthBiasClamp, float fDepthBiasSlopeFactor)
{
    plCommandBuffer* ptCmdBuffer = ptEncoder->ptCommandBuffer;
    vkCmdSetDepthBias(ptCmdBuffer->tCmdBuffer, fDepthBiasConstantFactor, fDepthBiasClamp, fDepthBiasSlopeFactor);
}

void
pl_set_viewport(plRenderEncoder* ptEncoder, const plRenderViewport* ptViewport)
{
    plCommandBuffer* ptCmdBuffer = ptEncoder->ptCommandBuffer;

    const VkViewport tViewport = {
        .x        = ptViewport->fX,
        .y        = ptViewport->fY,
        .width    = ptViewport->fWidth,
        .height   = ptViewport->fHeight,
        .minDepth = ptViewport->fMinDepth,
        .maxDepth = ptViewport->fMaxDepth
    };
    vkCmdSetViewport(ptCmdBuffer->tCmdBuffer, 0, 1, &tViewport);
}

void
pl_set_scissor_region(plRenderEncoder* ptEncoder, const plScissor* ptScissor)
{
    plCommandBuffer* ptCmdBuffer = ptEncoder->ptCommandBuffer;

    const VkRect2D tScissor = {
        .offset = {
            .x = ptScissor->iOffsetX,
            .y = ptScissor->iOffsetY
        },
        .extent = {
            .width = ptScissor->uWidth,
            .height = ptScissor->uHeight
        }
    };
    vkCmdSetScissor(ptCmdBuffer->tCmdBuffer, 0, 1, &tScissor);
}

plDeviceMemoryAllocation
pl_allocate_memory(plDevice* ptDevice, size_t szSize, plMemoryMode tMemoryMode, uint32_t uTypeFilter, const char* pcName)
{
    uint32_t uMemoryType = 0u;
    bool bFound = false;
    VkMemoryPropertyFlags tProperties = 0;
    if (tMemoryMode == PL_MEMORY_GPU_CPU)
        tProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    else if (tMemoryMode == PL_MEMORY_GPU)
        tProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    else if (tMemoryMode == PL_MEMORY_CPU)
        tProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    for (uint32_t i = 0; i < ptDevice->tMemProps.memoryTypeCount; i++)
    {
        if ((uTypeFilter & (1 << i)) && (ptDevice->tMemProps.memoryTypes[i].propertyFlags & tProperties) == tProperties)
        {
            uMemoryType = i;
            bFound = true;
            break;
        }
    }
    PL_ASSERT(bFound);

    if (pcName == NULL)
    {
        pcName = "unnamed memory block";
    }

    plDeviceMemoryAllocation tBlock = {
        .uHandle      = 0,
        .ulSize       = (uint64_t)szSize,
        .ulMemoryType = (uint64_t)uMemoryType,
        .tMemoryMode  = tMemoryMode
    };

    const VkMemoryAllocateInfo tAllocInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = szSize,
        .memoryTypeIndex = uMemoryType
    };

    VkDeviceMemory tMemory = VK_NULL_HANDLE;
    VkResult tResult = vkAllocateMemory(ptDevice->tLogicalDevice, &tAllocInfo, NULL, &tMemory);
    PL_VULKAN(tResult);
    tBlock.uHandle = (uint64_t)tMemory;

    pl__set_vulkan_object_name(ptDevice, tBlock.uHandle, VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT, pcName);

    if (tMemoryMode == PL_MEMORY_GPU)
    {
        gptGraphics->szLocalMemoryInUse += tBlock.ulSize;
    }
    else
    {
        PL_VULKAN(vkMapMemory(ptDevice->tLogicalDevice, (VkDeviceMemory)tBlock.uHandle, 0, tBlock.ulSize, 0, (void **)&tBlock.pHostMapped));
        gptGraphics->szHostMemoryInUse += tBlock.ulSize;
    }

    return tBlock;
}

void
pl_free_memory(plDevice* ptDevice, plDeviceMemoryAllocation* ptBlock)
{
    if (ptBlock->tMemoryMode == PL_MEMORY_GPU)
    {
        gptGraphics->szLocalMemoryInUse -= ptBlock->ulSize;
    }
    else
    {
        gptGraphics->szHostMemoryInUse -= ptBlock->ulSize;
    }

    vkFreeMemory(ptDevice->tLogicalDevice, (VkDeviceMemory)ptBlock->uHandle, NULL);
    ptBlock->uHandle = 0;
    ptBlock->pHostMapped = NULL;
    ptBlock->ulSize = 0;
    ptBlock->tMemoryMode = 0;
    ptBlock->ulMemoryType = 0;
}

bool
pl_initialize_graphics(const plGraphicsInit* ptDesc)
{
    static plGraphics gtGraphics = {0};
    gptGraphics = &gtGraphics;

    // setup logging
    plLogExtChannelInit tLogInit = {
        .tType       = PL_LOG_CHANNEL_TYPE_CYCLIC_BUFFER,
        .uEntryCount = 256
    };
    uLogChannelGraphics = gptLog->add_channel("Graphics", tLogInit);
    uint32_t uLogLevel = PL_LOG_LEVEL_INFO;
    gptLog->set_level(uLogChannelGraphics, uLogLevel);

    // save context for hot-reloads
    gptDataRegistry->set_data("plGraphics", gptGraphics);

    gptGraphics->bValidationActive = ptDesc->tFlags & PL_GRAPHICS_INIT_FLAGS_VALIDATION_ENABLED;

    gptGraphics->bDebugMessengerActive = gptGraphics->bValidationActive;

    // set frames in flight (if zero, use a default of 2)
    gptGraphics->uFramesInFlight = pl_min(pl_max(ptDesc->uFramesInFlight, 2), PL_MAX_FRAMES_IN_FLIGHT);

    //-------------------------------extensions------------------------------------

    // required extensions
    uint32_t uExtensionCount = 0;
    const char *apcExtensions[64] = {0};

    // if swapchain option is enabled, add required extensions
    if (ptDesc->tFlags & PL_GRAPHICS_INIT_FLAGS_SWAPCHAIN_ENABLED)
    {
        apcExtensions[uExtensionCount++] = VK_KHR_SURFACE_EXTENSION_NAME;
        
    #ifdef _WIN32
            apcExtensions[uExtensionCount++] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
    #elif defined(__ANDROID__)
            apcExtensions[uExtensionCount++] = VK_KHR_ANDROID_SURFACE_EXTENSION_NAME;
    #elif defined(__APPLE__)
            apcExtensions[uExtensionCount++] = "VK_EXT_metal_surface";
            apcExtensions[uExtensionCount++] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
    #else // linux
            apcExtensions[uExtensionCount++] = VK_KHR_XCB_SURFACE_EXTENSION_NAME;
    #endif
    }

    // if debug messenger options is enabled, add required extensions
    if (gptGraphics->bDebugMessengerActive)
    {
        apcExtensions[uExtensionCount++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        apcExtensions[uExtensionCount++] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
    }

    // retrieve supported extensions
    uint32_t uInstanceExtensionsFound = 0u;
    VkExtensionProperties* ptAvailableExtensions = NULL;
    PL_VULKAN(vkEnumerateInstanceExtensionProperties(NULL, &uInstanceExtensionsFound, NULL));
    if (uInstanceExtensionsFound > 0)
    {
        ptAvailableExtensions = (VkExtensionProperties *)PL_ALLOC(sizeof(VkExtensionProperties) * uInstanceExtensionsFound);
        PL_VULKAN(vkEnumerateInstanceExtensionProperties(NULL, &uInstanceExtensionsFound, ptAvailableExtensions));
    }

    // ensure extensions are supported
    uint32_t uMissingExtensionCount = 0;
    const char *apcMissingExtensions[64] = {0};
    for (uint32_t i = 0; i < uExtensionCount; i++)
    {
        bool extensionFound = false;
        for (uint32_t j = 0; j < uInstanceExtensionsFound; j++)
        {
            if (strcmp(apcExtensions[i], ptAvailableExtensions[j].extensionName) == 0)
            {
                pl_log_trace_f(gptLog, uLogChannelGraphics, "extension %s found", ptAvailableExtensions[j].extensionName);
                extensionFound = true;
                break;
            }
        }

        if (!extensionFound)
        {
            apcMissingExtensions[uMissingExtensionCount++] = apcExtensions[i];
        }
    }

    // report if all requested extensions aren't found
    if (uMissingExtensionCount > 0)
    {
        for (uint32_t i = 0; i < uMissingExtensionCount; i++)
        {
            pl_log_error_f(gptLog, uLogChannelGraphics, "  * %s", apcMissingExtensions[i]);
        }

        PL_ASSERT(false && "Can't find all requested extensions");

        if (ptAvailableExtensions)
            PL_FREE(ptAvailableExtensions);

        return false;
    }

    //---------------------------------layers--------------------------------------

    // retrieve supported layers
    uint32_t uInstanceLayersFound = 0u;
    VkLayerProperties* ptAvailableLayers = NULL;
    PL_VULKAN(vkEnumerateInstanceLayerProperties(&uInstanceLayersFound, NULL));
    if (uInstanceLayersFound > 0)
    {
        ptAvailableLayers = (VkLayerProperties *)PL_ALLOC(sizeof(VkLayerProperties) * uInstanceLayersFound);
        PL_VULKAN(vkEnumerateInstanceLayerProperties(&uInstanceLayersFound, ptAvailableLayers));
    }

    // ensure layers are supported
    static const char* pcValidationLayer = "VK_LAYER_KHRONOS_validation";
    bool bLayerFound = true;
    if (gptGraphics->bValidationActive)
    {
        bLayerFound = false;
        for (uint32_t i = 0; i < uInstanceLayersFound; i++)
        {
            if (strcmp(pcValidationLayer, ptAvailableLayers[i].layerName) == 0)
            {
                pl_log_trace_f(gptLog, uLogChannelGraphics, "layer %s found", ptAvailableLayers[i].layerName);
                bLayerFound = true;
                break;
            }
        }
    }

    if (!bLayerFound)
    {
        PL_ASSERT("Can't find requested layers");
        if (ptAvailableLayers)
            PL_FREE(ptAvailableLayers);
        return false;
    }

    // create vulkan tInstance
    const VkApplicationInfo tAppInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = VK_API_VERSION_1_2};

    const void* pCreateInfoNext = VK_NULL_HANDLE;

    if (gptGraphics->bDebugMessengerActive)
    {

        // Setup debug messenger for vulkan instance
        static VkDebugUtilsMessengerCreateInfoEXT tDebugCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = pl__debug_callback,
            .pNext = VK_NULL_HANDLE};

        // if (ptDesc->tFlags & PL_GRAPHICS_INIT_FLAGS_LOGGING_TRACE)
        //     tDebugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
        // else if ((ptDesc->tFlags & PL_GRAPHICS_INIT_FLAGS_LOGGING_DEBUG) || (ptDesc->tFlags & PL_GRAPHICS_INIT_FLAGS_LOGGING_INFO))
        tDebugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        // else if (ptDesc->tFlags & PL_GRAPHICS_INIT_FLAGS_LOGGING_WARNING)
        //     tDebugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        // else if (ptDesc->tFlags & PL_GRAPHICS_INIT_FLAGS_LOGGING_ERROR)
        //     tDebugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

        pCreateInfoNext = &tDebugCreateInfo;
    }

    //--------------------------------instance-------------------------------------

    const VkInstanceCreateInfo tCreateInfo = {
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo        = &tAppInfo,
        .pNext                   = pCreateInfoNext,
        .enabledExtensionCount   = uExtensionCount,
        .ppEnabledExtensionNames = apcExtensions,
        .enabledLayerCount       = gptGraphics->bValidationActive ? 1 : 0,
        .ppEnabledLayerNames     = &pcValidationLayer,

        #ifdef __APPLE__
                .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR
        #endif
    };

    PL_VULKAN(vkCreateInstance(&tCreateInfo, NULL, &gptGraphics->tInstance));
    pl_log_trace_f(gptLog, uLogChannelGraphics, "created vulkan instance");

    // cleanup
    if (ptAvailableLayers)
    {
        PL_FREE(ptAvailableLayers);
    }

    if (ptAvailableExtensions)
    {
        PL_FREE(ptAvailableExtensions);
    }

    if (gptGraphics->bDebugMessengerActive)
    {
        PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(gptGraphics->tInstance, "vkCreateDebugUtilsMessengerEXT");
        PL_ASSERT(func != NULL && "failed to set up debug messenger!");
        PL_VULKAN(func(gptGraphics->tInstance, pCreateInfoNext, NULL, &gptGraphics->tDbgMessenger));
        pl_log_trace_f(gptLog, uLogChannelGraphics, "enabled Vulkan validation layers");
    }

    return true;
}

static void
pl_enumerate_devices(plDeviceInfo *atDeviceInfo, uint32_t* puDeviceCount)
{
    VkPhysicalDevice atDevices[16] = {0};
    PL_VULKAN(vkEnumeratePhysicalDevices(gptGraphics->tInstance, puDeviceCount, atDevices));

    if (atDeviceInfo == NULL)
        return;

    memset(atDeviceInfo, 0, (*puDeviceCount) * sizeof(plDeviceInfo));
    for (uint32_t i = 0; i < *puDeviceCount; i++)
    {

        VkPhysicalDeviceProperties tProps = {0};
        vkGetPhysicalDeviceProperties(atDevices[i], &tProps);

        VkPhysicalDeviceMemoryProperties tMemProps = {0};
        vkGetPhysicalDeviceMemoryProperties(atDevices[i], &tMemProps);

        atDeviceInfo[i].tMaxSampleCount = VK_SAMPLE_COUNT_1_BIT;

        VkSampleCountFlags tCounts = tProps.limits.framebufferColorSampleCounts & tProps.limits.framebufferDepthSampleCounts;
        if (tCounts & VK_SAMPLE_COUNT_64_BIT)
        {
            atDeviceInfo[i].tMaxSampleCount = VK_SAMPLE_COUNT_64_BIT;
        }
        else if (tCounts & VK_SAMPLE_COUNT_32_BIT)
        {
            atDeviceInfo[i].tMaxSampleCount = VK_SAMPLE_COUNT_32_BIT;
        }
        else if (tCounts & VK_SAMPLE_COUNT_16_BIT)
        {
            atDeviceInfo[i].tMaxSampleCount = VK_SAMPLE_COUNT_16_BIT;
        }
        else if (tCounts & VK_SAMPLE_COUNT_8_BIT)
        {
            atDeviceInfo[i].tMaxSampleCount = VK_SAMPLE_COUNT_8_BIT;
        }
        else if (tCounts & VK_SAMPLE_COUNT_4_BIT)
        {
            atDeviceInfo[i].tMaxSampleCount = VK_SAMPLE_COUNT_4_BIT;
        }
        else if (tCounts & VK_SAMPLE_COUNT_2_BIT)
        {
            atDeviceInfo[i].tMaxSampleCount = VK_SAMPLE_COUNT_2_BIT;
        }

        strncpy(atDeviceInfo[i].acName, tProps.deviceName, 256);
        atDeviceInfo[i].tLimits.uMaxTextureSize = tProps.limits.maxImageDimension2D;
        atDeviceInfo[i].tLimits.uMinUniformBufferOffsetAlignment = (uint32_t)tProps.limits.minUniformBufferOffsetAlignment;

        switch (tProps.deviceType)
        {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            atDeviceInfo[i].tType = PL_DEVICE_TYPE_DISCRETE;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            atDeviceInfo[i].tType = PL_DEVICE_TYPE_CPU;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            atDeviceInfo[i].tType = PL_DEVICE_TYPE_INTEGRATED;
            break;
        default:
            atDeviceInfo[i].tType = PL_DEVICE_TYPE_NONE;
        }

        switch (tProps.vendorID)
        {
        case 0x1002:
            atDeviceInfo[i].tVendorId = PL_VENDOR_ID_AMD;
            break;
        case 0x10DE:
            atDeviceInfo[i].tVendorId = PL_VENDOR_ID_NVIDIA;
            break;
        case 0x8086:
            atDeviceInfo[i].tVendorId = PL_VENDOR_ID_INTEL;
            break;
        default:
            atDeviceInfo[i].tVendorId = PL_VENDOR_ID_NONE;
        }

        for (uint32_t j = 0; j < tMemProps.memoryHeapCount; j++)
        {
            if (tMemProps.memoryHeaps[j].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                atDeviceInfo[i].szDeviceMemory += tMemProps.memoryHeaps[j].size;
            else
                atDeviceInfo[i].szHostMemory += tMemProps.memoryHeaps[j].size;
        }

        uint32_t uExtensionCount = 0;
        vkEnumerateDeviceExtensionProperties(atDevices[i], NULL, &uExtensionCount, NULL);
        VkExtensionProperties* ptExtensions = pl_temp_allocator_alloc(&gptGraphics->tTempAllocator, uExtensionCount * sizeof(VkExtensionProperties));
        vkEnumerateDeviceExtensionProperties(atDevices[i], NULL, &uExtensionCount, ptExtensions);

        for (uint32_t j = 0; j < uExtensionCount; j++)
        {
            if (pl_str_equal(ptExtensions[j].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
                atDeviceInfo[i].tCapabilities |= PL_DEVICE_CAPABILITY_SWAPCHAIN;
        }
        pl_temp_allocator_reset(&gptGraphics->tTempAllocator);

        // get device features
        VkPhysicalDeviceFeatures tDeviceFeatures = {0};
        VkPhysicalDeviceFeatures2 tDeviceFeatures2 = {0};
        VkPhysicalDeviceVulkan12Features tDeviceFeatures12 = {0};
        VkPhysicalDeviceDescriptorIndexingFeatures tDescriptorIndexingFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
            .pNext = &tDeviceFeatures12};

        // create logical device
        tDeviceFeatures12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        tDeviceFeatures2.pNext = &tDescriptorIndexingFeatures;
        tDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

        vkGetPhysicalDeviceFeatures(atDevices[i], &tDeviceFeatures);
        vkGetPhysicalDeviceFeatures2(atDevices[i], &tDeviceFeatures2);

        // Non-uniform indexing and update after bind
        // binding flags for textures, uniforms, and buffers
        // are required for our extension
        if (tDescriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing &&
            tDescriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind)
        {
            atDeviceInfo[i].tCapabilities |= PL_DEVICE_CAPABILITY_BIND_GROUP_INDEXING;
            // PL_ASSERT(tDescriptorIndexingFeatures.shaderUniformBufferArrayNonUniformIndexing);
            // PL_ASSERT(tDescriptorIndexingFeatures.descriptorBindingUniformBufferUpdateAfterBind);
            // PL_ASSERT(tDescriptorIndexingFeatures.shaderStorageBufferArrayNonUniformIndexing);
            // PL_ASSERT(tDescriptorIndexingFeatures.descriptorBindingStorageBufferUpdateAfterBind);
        }

        if (tDeviceFeatures.samplerAnisotropy)
            atDeviceInfo[i].tCapabilities |= PL_DEVICE_CAPABILITY_SAMPLER_ANISOTROPY;

        if (tDeviceFeatures.multiViewport)
            atDeviceInfo[i].tCapabilities |= PL_DEVICE_CAPABILITY_MULTIPLE_VIEWPORTS;
    }
}

plDevice*
pl_create_device(const plDeviceInit* ptInit)
{

    plDevice* ptDevice = PL_ALLOC(sizeof(plDevice));
    memset(ptDevice, 0, sizeof(plDevice));
    ptDevice->tInit = *ptInit;

    pl_sb_add(ptDevice->sbtRenderPassLayoutsHot);
    pl_sb_add(ptDevice->sbtRenderPassesHot);
    pl_sb_add(ptDevice->sbtShadersHot);
    pl_sb_add(ptDevice->sbtComputeShadersHot);
    pl_sb_add(ptDevice->sbtBuffersHot);
    pl_sb_add(ptDevice->sbtTextureViewsHot);
    pl_sb_add(ptDevice->sbtTexturesHot);
    pl_sb_add(ptDevice->sbtSamplersHot);
    pl_sb_add(ptDevice->sbtBindGroupsHot);
    
    pl_sb_add(ptDevice->sbtRenderPassLayoutsCold);
    pl_sb_add(ptDevice->sbtShadersCold);
    pl_sb_add(ptDevice->sbtComputeShadersCold);
    pl_sb_add(ptDevice->sbtBuffersCold);
    pl_sb_add(ptDevice->sbtTexturesCold);
    pl_sb_add(ptDevice->sbtSamplersCold);
    pl_sb_add(ptDevice->sbtBindGroupsCold);

    pl_sb_back(ptDevice->sbtRenderPassLayoutsCold)._uGeneration = 1;
    pl_sb_back(ptDevice->sbtShadersCold)._uGeneration = 1;
    pl_sb_back(ptDevice->sbtComputeShadersCold)._uGeneration = 1;
    pl_sb_back(ptDevice->sbtBuffersCold)._uGeneration = 1;
    pl_sb_back(ptDevice->sbtTexturesCold)._uGeneration = 1;
    pl_sb_back(ptDevice->sbtSamplersCold)._uGeneration = 1;
    pl_sb_back(ptDevice->sbtBindGroupsCold)._uGeneration = 1;

    uint32_t uDeviceCount = 16;
    VkPhysicalDevice atDevices[16] = {0};
    PL_VULKAN(vkEnumeratePhysicalDevices(gptGraphics->tInstance, &uDeviceCount, atDevices));

    plDeviceInfo atDeviceInfos[16] = {0};
    pl_enumerate_devices(atDeviceInfos, &uDeviceCount);

    // user decided on device
    ptDevice->tPhysicalDevice = atDevices[ptInit->uDeviceIdx];
    memcpy(&ptDevice->tInfo, &atDeviceInfos[ptInit->uDeviceIdx], sizeof(plDeviceInfo));

    if (ptDevice->tInit.szDynamicBufferBlockSize == 0)
        ptDevice->tInit.szDynamicBufferBlockSize = 134217728;
    if (ptDevice->tInit.szDynamicDataMaxSize == 0)
        ptDevice->tInit.szDynamicDataMaxSize = 256;

    const size_t szMaxDynamicBufferDescriptors = ptDevice->tInit.szDynamicBufferBlockSize / ptDevice->tInit.szDynamicDataMaxSize;

    // find queue families
    ptDevice->iGraphicsQueueFamily = -1;
    ptDevice->iPresentQueueFamily = -1;
    uint32_t uQueueFamCnt = 0u;
    vkGetPhysicalDeviceQueueFamilyProperties(ptDevice->tPhysicalDevice, &uQueueFamCnt, NULL);

    VkQueueFamilyProperties auQueueFamilies[64] = {0};
    vkGetPhysicalDeviceQueueFamilyProperties(ptDevice->tPhysicalDevice, &uQueueFamCnt, auQueueFamilies);

    vkGetPhysicalDeviceMemoryProperties(ptDevice->tPhysicalDevice, &ptDevice->tMemProps);

    for (uint32_t i = 0; i < uQueueFamCnt; i++)
    {
        if (auQueueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            ptDevice->iGraphicsQueueFamily = i;

        VkBool32 tPresentSupport = false;
        PL_VULKAN(vkGetPhysicalDeviceSurfaceSupportKHR(ptDevice->tPhysicalDevice, i, ptInit->ptSurface->tSurface, &tPresentSupport));

        if (tPresentSupport)
            ptDevice->iPresentQueueFamily = i;

        if (ptDevice->iGraphicsQueueFamily > -1 && ptDevice->iPresentQueueFamily > -1) // complete
            break;
        i++;
    }

    const float fQueuePriority = 1.0f;
    VkDeviceQueueCreateInfo atQueueCreateInfos[] = {
        {
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = ptDevice->iGraphicsQueueFamily,
            .queueCount       = 1,
            .pQueuePriorities = &fQueuePriority
         },
        {
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = ptDevice->iPresentQueueFamily,
            .queueCount       = 1,
            .pQueuePriorities = &fQueuePriority
         }
    };

    static const char* pcValidationLayers = "VK_LAYER_KHRONOS_validation";

    uint32_t uDeviceExtensionCount = 0;
    const char *apcDeviceExts[16] = {0};
    if (gptGraphics->bDebugMessengerActive)
    {
        apcDeviceExts[0] = VK_EXT_DEBUG_MARKER_EXTENSION_NAME;
        uDeviceExtensionCount++;
    }
    if (ptDevice->tInfo.tCapabilities & PL_DEVICE_CAPABILITY_SWAPCHAIN)
        apcDeviceExts[uDeviceExtensionCount++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    #if defined(__APPLE__)
        apcDeviceExts[uDeviceExtensionCount++] = "VK_KHR_portability_subset";
    #endif

    apcDeviceExts[uDeviceExtensionCount++] = VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME;

    // get device features
    VkPhysicalDeviceFeatures tDeviceFeatures = {0};
    VkPhysicalDeviceFeatures2 tDeviceFeatures2 = {0};
    VkPhysicalDeviceVulkan12Features tDeviceFeatures12 = {0};
    VkPhysicalDeviceDescriptorIndexingFeatures tDescriptorIndexingFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
        .pNext = &tDeviceFeatures12
    };

    // create logical device
    tDeviceFeatures12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    tDeviceFeatures2.pNext = &tDescriptorIndexingFeatures;
    tDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

    vkGetPhysicalDeviceFeatures(ptDevice->tPhysicalDevice, &tDeviceFeatures);
    vkGetPhysicalDeviceFeatures2(ptDevice->tPhysicalDevice, &tDeviceFeatures2);

    const VkDeviceCreateInfo tCreateDeviceInfo = {
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount    = atQueueCreateInfos[0].queueFamilyIndex == atQueueCreateInfos[1].queueFamilyIndex ? 1 : 2,
        .pQueueCreateInfos       = atQueueCreateInfos,
        .pEnabledFeatures        = &tDeviceFeatures,
        .ppEnabledExtensionNames = apcDeviceExts,
        .enabledLayerCount       = gptGraphics->bValidationActive ? 1 : 0,
        .ppEnabledLayerNames     = gptGraphics->bValidationActive ? &pcValidationLayers : NULL,
        .enabledExtensionCount   = uDeviceExtensionCount,
        .pNext                   = &tDeviceFeatures12
    };
    PL_VULKAN(vkCreateDevice(ptDevice->tPhysicalDevice, &tCreateDeviceInfo, NULL, &ptDevice->tLogicalDevice));

    // get device queues
    vkGetDeviceQueue(ptDevice->tLogicalDevice, ptDevice->iGraphicsQueueFamily, 0, &ptDevice->tGraphicsQueue);
    vkGetDeviceQueue(ptDevice->tLogicalDevice, ptDevice->iPresentQueueFamily, 0, &ptDevice->tPresentQueue);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~debug markers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    if (gptGraphics->bValidationActive)
    {
        ptDevice->vkDebugMarkerSetObjectTag = (PFN_vkDebugMarkerSetObjectTagEXT)vkGetDeviceProcAddr(ptDevice->tLogicalDevice, "vkDebugMarkerSetObjectTagEXT");
        ptDevice->vkDebugMarkerSetObjectName = (PFN_vkDebugMarkerSetObjectNameEXT)vkGetDeviceProcAddr(ptDevice->tLogicalDevice, "vkDebugMarkerSetObjectNameEXT");
        ptDevice->vkCmdDebugMarkerBegin = (PFN_vkCmdDebugMarkerBeginEXT)vkGetDeviceProcAddr(ptDevice->tLogicalDevice, "vkCmdDebugMarkerBeginEXT");
        ptDevice->vkCmdDebugMarkerEnd = (PFN_vkCmdDebugMarkerEndEXT)vkGetDeviceProcAddr(ptDevice->tLogicalDevice, "vkCmdDebugMarkerEndEXT");
        ptDevice->vkCmdDebugMarkerInsert = (PFN_vkCmdDebugMarkerInsertEXT)vkGetDeviceProcAddr(ptDevice->tLogicalDevice, "vkCmdDebugMarkerInsertEXT");
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~main descriptor pool~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    VkDescriptorPoolSize atPoolSizes[] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 256},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
    };
    VkDescriptorPoolCreateInfo tDescriptorPoolInfo = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = 257,
        .poolSizeCount = 2,
        .pPoolSizes    = atPoolSizes
    };
    if (ptDevice->tInfo.tCapabilities & PL_DEVICE_CAPABILITY_BIND_GROUP_INDEXING)
    {
        tDescriptorPoolInfo.flags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
    }
    PL_VULKAN(vkCreateDescriptorPool(ptDevice->tLogicalDevice, &tDescriptorPoolInfo, NULL, &ptDevice->tDynamicBufferDescriptorPool));

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~frame resources~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    static plInternalDeviceAllocatorData tAllocatorData = {0};
    static plDeviceMemoryAllocatorI tAllocator = {0};
    tAllocatorData.ptAllocator = &tAllocator;
    tAllocatorData.ptDevice = ptDevice;
    tAllocator.allocate = pl__allocate_staging_dynamic;
    tAllocator.free = pl__free_staging_dynamic;
    tAllocator.ptInst = (struct plDeviceMemoryAllocatorO *)&tAllocatorData;
    ptDevice->ptDynamicAllocator = &tAllocator;
    plDeviceMemoryAllocatorI* ptDynamicAllocator = &tAllocator;

    // dynamic buffer stuff
    VkDescriptorSetLayoutBinding tBinding = {
        .binding            = 0,
        .descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        .descriptorCount    = 1,
        .stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
        .pImmutableSamplers = NULL
    };

    const VkDescriptorSetLayoutCreateInfo tDescriptorSetLayoutInfo = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings    = &tBinding
    };
    PL_VULKAN(vkCreateDescriptorSetLayout(ptDevice->tLogicalDevice, &tDescriptorSetLayoutInfo, NULL, &ptDevice->tDynamicDescriptorSetLayout));

    {
        // null descriptor stuff
        VkDescriptorSetLayoutBinding tNullBinding = {
            .binding            = 0,
            .descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount    = 1,
            .stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = NULL
        };

        const VkDescriptorSetLayoutCreateInfo tNullDescriptorSetLayoutInfo = {
            .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings    = &tNullBinding
        };
        PL_VULKAN(vkCreateDescriptorSetLayout(ptDevice->tLogicalDevice, &tNullDescriptorSetLayoutInfo, NULL, &ptDevice->tNullDescriptorSetLayout));

        // allocate null descriptor set
        const VkDescriptorSetAllocateInfo tNullAllocInfo = {
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool     = ptDevice->tDynamicBufferDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts        = &ptDevice->tNullDescriptorSetLayout
        };
        PL_VULKAN(vkAllocateDescriptorSets(ptDevice->tLogicalDevice, &tNullAllocInfo, &ptDevice->tNullDecriptorSet));

        plBufferDesc tDummyBufferDescription = {
            .tUsage      = PL_BUFFER_USAGE_UNIFORM,
            .szByteSize  = 64,
            .pcDebugName = "dummy buffer"
        };
        plBuffer* ptDummyBuffer = NULL;
        plBufferHandle tDummyBuffer = pl_create_buffer(ptDevice, &tDummyBufferDescription, &ptDummyBuffer);
        plDeviceMemoryAllocation tDummyAllocation = ptDynamicAllocator->allocate(ptDynamicAllocator->ptInst,
            ptDummyBuffer->tMemoryRequirements.uMemoryTypeBits,
            ptDummyBuffer->tMemoryRequirements.ulSize,
            ptDummyBuffer->tMemoryRequirements.ulAlignment,
            "dummy buffer");
        pl_bind_buffer_to_memory(ptDevice, tDummyBuffer, &tDummyAllocation);

        ptDevice->tDummyMemory = (VkDeviceMemory)tDummyAllocation.uHandle;

        VkDescriptorBufferInfo tNullDescriptorInfo0 = {
            .buffer = ptDevice->sbtBuffersHot[tDummyBuffer.uIndex].tBuffer,
            .offset = 0,
            .range  = 64
        };

        VkWriteDescriptorSet tNullWrite = {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding      = 0,
            .dstArrayElement = 0,
            .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .dstSet          = ptDevice->tNullDecriptorSet,
            .pBufferInfo     = &tNullDescriptorInfo0,
            .pNext           = NULL,
        };
        vkUpdateDescriptorSets(ptDevice->tLogicalDevice, 1, &tNullWrite, 0, NULL);
    }

    const VkSemaphoreCreateInfo tSemaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    const VkFenceCreateInfo tFenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    pl_sb_resize(ptDevice->sbtFrames, gptGraphics->uFramesInFlight);
    pl_sb_resize(ptDevice->sbtGarbage, gptGraphics->uFramesInFlight);
    for (uint32_t i = 0; i < gptGraphics->uFramesInFlight; i++)
    {
        plFrameContext tFrame = {0};
        PL_VULKAN(vkCreateSemaphore(ptDevice->tLogicalDevice, &tSemaphoreInfo, NULL, &tFrame.tRenderFinish));
        PL_VULKAN(vkCreateFence(ptDevice->tLogicalDevice, &tFenceInfo, NULL, &tFrame.tInFlight));

        // dynamic buffer stuff
        pl_sb_resize(tFrame.sbtDynamicBuffers, 1);
        plBufferDesc tStagingBufferDescription0 = {
            .tUsage      = PL_BUFFER_USAGE_UNIFORM | PL_BUFFER_USAGE_STAGING,
            .szByteSize  = ptDevice->tInit.szDynamicBufferBlockSize,
            .pcDebugName = "dynamic buffer"
        };
        plBuffer* ptBuffer = NULL;
        plBufferHandle tStagingBuffer0 = pl_create_buffer(ptDevice, &tStagingBufferDescription0, &ptBuffer);
        plDeviceMemoryAllocation tAllocation = ptDynamicAllocator->allocate(ptDynamicAllocator->ptInst, ptBuffer->tMemoryRequirements.uMemoryTypeBits, ptBuffer->tMemoryRequirements.ulSize, ptBuffer->tMemoryRequirements.ulAlignment, "dynamic buffer");
        pl_bind_buffer_to_memory(ptDevice, tStagingBuffer0, &tAllocation);

        tFrame.uCurrentBufferIndex = 0;
        tFrame.sbtDynamicBuffers[0].uHandle = tStagingBuffer0.uIndex;
        tFrame.sbtDynamicBuffers[0].tBuffer = ptDevice->sbtBuffersHot[tStagingBuffer0.uIndex].tBuffer;
        tFrame.sbtDynamicBuffers[0].tMemory = tAllocation;

        // allocate descriptor sets
        const VkDescriptorSetAllocateInfo tDynamicAllocInfo = {
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool     = ptDevice->tDynamicBufferDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts        = &ptDevice->tDynamicDescriptorSetLayout
        };
        PL_VULKAN(vkAllocateDescriptorSets(ptDevice->tLogicalDevice, &tDynamicAllocInfo, &tFrame.sbtDynamicBuffers[0].tDescriptorSet));

        VkDescriptorBufferInfo tDescriptorInfo0 = {
            .buffer = tFrame.sbtDynamicBuffers[0].tBuffer,
            .offset = 0,
            .range  = ptDevice->tInit.szDynamicDataMaxSize
        };

        VkWriteDescriptorSet tWrite0 = {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding      = 0,
            .dstArrayElement = 0,
            .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .descriptorCount = 1,
            .dstSet          = tFrame.sbtDynamicBuffers[0].tDescriptorSet,
            .pBufferInfo     = &tDescriptorInfo0,
            .pNext           = NULL,
        };
        vkUpdateDescriptorSets(ptDevice->tLogicalDevice, 1, &tWrite0, 0, NULL);

        ptDevice->sbtFrames[i] = tFrame;

        if(i == 0)
            ptDevice->tNullDynamicDecriptorSet = tFrame.sbtDynamicBuffers[0].tDescriptorSet;
    }
    pl_temp_allocator_reset(&gptGraphics->tTempAllocator);
    return ptDevice;
}

plSurface*
pl_create_surface(plWindow* ptWindow)
{
    plSurface* ptSurface = PL_ALLOC(sizeof(plSurface));
    memset(ptSurface, 0, sizeof(plSurface));

    #ifdef _WIN32
        const VkWin32SurfaceCreateInfoKHR tSurfaceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .pNext = NULL,
            .flags = 0,
            .hinstance = GetModuleHandle(NULL),
            .hwnd = (HWND)ptWindow->_pPlatformData};
        PL_VULKAN(vkCreateWin32SurfaceKHR(gptGraphics->tInstance, &tSurfaceCreateInfo, NULL, &ptSurface->tSurface));
    #elif defined(__ANDROID__)
        const VkAndroidSurfaceCreateInfoKHR tSurfaceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
            .pNext = NULL,
            .flags = 0,
            .window = };
        PL_VULKAN(vkCreateAndroidSurfaceKHR(gptGraphics->tInstance, &tSurfaceCreateInfo, NULL, &ptSurface->tSurface));
    #elif defined(__APPLE__)
        typedef struct _plWindowData
        {
            void* ptWindow;
            void* ptViewController;
            CAMetalLayer* ptLayer;
        } plWindowData;
        const VkMetalSurfaceCreateInfoEXT tSurfaceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
            .pLayer = ((plWindowData *)ptWindow->_pPlatformData)->ptLayer};
        PL_VULKAN(vkCreateMetalSurfaceEXT(gptGraphics->tInstance, &tSurfaceCreateInfo, NULL, &ptSurface->tSurface));
    #else // linux
        struct tPlatformData
        {
            xcb_connection_t* ptConnection;
            xcb_window_t tWindow;
        };
        struct tPlatformData* ptPlatformData = (struct tPlatformData *)ptWindow->_pPlatformData;
        const VkXcbSurfaceCreateInfoKHR tSurfaceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
            .pNext = NULL,
            .flags = 0,
            .window = ptPlatformData->tWindow,
            .connection = ptPlatformData->ptConnection};
        PL_VULKAN(vkCreateXcbSurfaceKHR(gptGraphics->tInstance, &tSurfaceCreateInfo, NULL, &ptSurface->tSurface));
    #endif
    return ptSurface;
}

plSwapchain*
pl_create_swapchain(plDevice* ptDevice, plSurface* ptSurface, const plSwapchainInit* ptInit)
{

    plSwapchain* ptSwap = PL_ALLOC(sizeof(plSwapchain));
    memset(ptSwap, 0, sizeof(plSwapchain));

    ptSwap->tInfo.bVSync = ptInit->bVSync;
    ptSwap->tInfo.uWidth = ptInit->uWidth;
    ptSwap->tInfo.uHeight = ptInit->uHeight;
    ptSwap->ptSurface = ptSurface;
    ptSwap->ptDevice = ptDevice;
    ptSwap->uImageCount = gptGraphics->uFramesInFlight;
    ptSwap->tInfo.tFormat = PL_FORMAT_B8G8R8A8_UNORM;
    ptSwap->tInfo.tSampleCount = pl_min(ptInit->tSampleCount, ptSwap->ptDevice->tInfo.tMaxSampleCount);
    if(ptSwap->tInfo.tSampleCount == 0)
        ptSwap->tInfo.tSampleCount = 1;

    plIO* ptIOCtx = gptIOI->get_io();
    pl__create_swapchain((uint32_t)ptIOCtx->tMainViewportSize.x, (uint32_t)ptIOCtx->tMainViewportSize.y, ptSwap);

    const VkSemaphoreCreateInfo tSemaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };
    for (uint32_t i = 0; i < gptGraphics->uFramesInFlight; i++)
        PL_VULKAN(vkCreateSemaphore(ptDevice->tLogicalDevice, &tSemaphoreInfo, NULL, &ptSwap->atImageAvailable[i]));
    return ptSwap;
}

void
pl_begin_frame(plDevice* ptDevice)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptDevice);
    ptCurrentFrame->uCurrentBufferIndex = 0;

    PL_VULKAN(vkWaitForFences(ptDevice->tLogicalDevice, 1, &ptCurrentFrame->tInFlight, VK_TRUE, UINT64_MAX));
    pl__garbage_collect(ptDevice);

    pl_end_cpu_sample(gptProfile, 0);
}

bool
pl_acquire_swapchain_image(plSwapchain* ptSwap)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    plDevice* ptDevice = ptSwap->ptDevice;

    VkResult err = vkAcquireNextImageKHR(ptDevice->tLogicalDevice, ptSwap->tSwapChain, UINT64_MAX, ptSwap->atImageAvailable[gptGraphics->uCurrentFrameIndex], VK_NULL_HANDLE, &ptSwap->uCurrentImageIndex);
    if (err == VK_SUBOPTIMAL_KHR || err == VK_ERROR_OUT_OF_DATE_KHR)
    {
        if (err == VK_ERROR_OUT_OF_DATE_KHR)
        {
            pl_end_cpu_sample(gptProfile, 0);
            return false;
        }
    }
    else
    {
        PL_VULKAN(err);
    }
    pl_end_cpu_sample(gptProfile, 0);
    return true;
}

void
pl_end_command_recording(plCommandBuffer* ptCommandBuffer)
{
    PL_VULKAN(vkEndCommandBuffer(ptCommandBuffer->tCmdBuffer));
}

bool
pl_present(plCommandBuffer* ptCmdBuffer, const plSubmitInfo* ptSubmitInfo, plSwapchain **ptSwaps, uint32_t uSwapchainCount)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);
    plIO* ptIOCtx = gptIOI->get_io();

    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptDevice);

    // submit
    VkPipelineStageFlags atWaitStages[64] = {0};
    VkSemaphore atWaitSemaphores[64] = {0};
    atWaitStages[0] = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    VkCommandBuffer atCmdBuffers[] = {ptCmdBuffer->tCmdBuffer};
    VkSubmitInfo tSubmitInfo = {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount   = uSwapchainCount,
        .pWaitSemaphores      = atWaitSemaphores,
        .pWaitDstStageMask    = atWaitStages,
        .commandBufferCount   = 1,
        .pCommandBuffers      = atCmdBuffers,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &ptCurrentFrame->tRenderFinish
    };

    VkTimelineSemaphoreSubmitInfo tTimelineInfo = {
        .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
        .pNext = NULL,
    };

    VkSemaphore atSignalSemaphores[PL_MAX_TIMELINE_SEMAPHORES + 1] = {0};

    if(ptCmdBuffer->tBeginInfo.uWaitSemaphoreCount != UINT32_MAX)
    {
        for (uint32_t i = 0; i < ptCmdBuffer->tBeginInfo.uWaitSemaphoreCount; i++)
        {
            atWaitSemaphores[i] = ptCmdBuffer->tBeginInfo.atWaitSempahores[i]->tSemaphore;
            atWaitStages[i] = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        }
        for(uint32_t i = 0; i < uSwapchainCount; i++)
        {
            atWaitSemaphores[ptCmdBuffer->tBeginInfo.uWaitSemaphoreCount + i] = ptSwaps[i]->atImageAvailable[gptGraphics->uCurrentFrameIndex];
            atWaitStages[ptCmdBuffer->tBeginInfo.uWaitSemaphoreCount + i] = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        }

        tTimelineInfo.waitSemaphoreValueCount = ptCmdBuffer->tBeginInfo.uWaitSemaphoreCount + uSwapchainCount;
        tTimelineInfo.pWaitSemaphoreValues = ptCmdBuffer->tBeginInfo.auWaitSemaphoreValues;

        tSubmitInfo.pNext = &tTimelineInfo;
        tSubmitInfo.pWaitSemaphores = atWaitSemaphores;
        tSubmitInfo.waitSemaphoreCount = ptCmdBuffer->tBeginInfo.uWaitSemaphoreCount + uSwapchainCount;
    }

    if (ptSubmitInfo)
    {
        for (uint32_t i = 0; i < ptSubmitInfo->uSignalSemaphoreCount; i++)
        {
            atSignalSemaphores[i] = ptSubmitInfo->atSignalSempahores[i]->tSemaphore;
        }
        
        atSignalSemaphores[ptSubmitInfo->uSignalSemaphoreCount] = ptCurrentFrame->tRenderFinish;
        tTimelineInfo.signalSemaphoreValueCount = ptSubmitInfo->uSignalSemaphoreCount + 1;
        tTimelineInfo.pSignalSemaphoreValues = ptSubmitInfo->auSignalSemaphoreValues;

        tSubmitInfo.pNext = &tTimelineInfo;
        tSubmitInfo.pSignalSemaphores = atSignalSemaphores;
        tSubmitInfo.signalSemaphoreCount = ptSubmitInfo->uSignalSemaphoreCount + 1;
    }

    PL_VULKAN(vkResetFences(ptDevice->tLogicalDevice, 1, &ptCurrentFrame->tInFlight));
    PL_VULKAN(vkQueueSubmit(ptDevice->tGraphicsQueue, 1, &tSubmitInfo, ptCurrentFrame->tInFlight));

    VkSwapchainKHR atSwapchains[64] = {0};
    uint32_t auImageIndices[64] = {0};
    for(uint32_t i = 0; i < uSwapchainCount; i++)
    {
        atSwapchains[i] = ptSwaps[i]->tSwapChain;
        auImageIndices[i] = ptSwaps[i]->uCurrentImageIndex;
    }

    const VkPresentInfoKHR tPresentInfo = {
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &ptCurrentFrame->tRenderFinish,
        .swapchainCount     = uSwapchainCount,
        .pSwapchains        = atSwapchains,
        .pImageIndices      = auImageIndices
    };
    const VkResult tResult = vkQueuePresentKHR(ptDevice->tPresentQueue, &tPresentInfo);
    if (tResult == VK_SUBOPTIMAL_KHR || tResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        pl_sb_push(ptCmdBuffer->ptPool->sbtPendingCommandBuffers, ptCmdBuffer->tCmdBuffer);
        pl_end_cpu_sample(gptProfile, 0);
        return false;
    }
    else
    {
        PL_VULKAN(tResult);
    }
    gptGraphics->uCurrentFrameIndex = (gptGraphics->uCurrentFrameIndex + 1) % gptGraphics->uFramesInFlight;
    pl_sb_push(ptCmdBuffer->ptPool->sbtPendingCommandBuffers, ptCmdBuffer->tCmdBuffer);
    pl_end_cpu_sample(gptProfile, 0);
    return true;
}

void
pl_recreate_swapchain(plSwapchain* ptSwap, const plSwapchainInit* ptInit)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);
    ptSwap->tInfo.bVSync = ptInit->bVSync;
    ptSwap->tInfo.uWidth = ptInit->uWidth;
    ptSwap->tInfo.uHeight = ptInit->uHeight;
    ptSwap->tInfo.tSampleCount = pl_min(ptInit->tSampleCount, ptSwap->ptDevice->tInfo.tMaxSampleCount);
    if(ptSwap->tInfo.tSampleCount == 0)
        ptSwap->tInfo.tSampleCount = 1;

    pl__create_swapchain(ptInit->uWidth, ptInit->uHeight, ptSwap);
    pl_end_cpu_sample(gptProfile, 0);
}

void
pl_flush_device(plDevice* ptDevice)
{
    vkDeviceWaitIdle(ptDevice->tLogicalDevice);
}

void
pl_cleanup_graphics(void)
{
    vkDestroyInstance(gptGraphics->tInstance, NULL);
    pl_temp_allocator_free(&gptGraphics->tTempAllocator);
    pl__cleanup_common_graphics();
}

void
pl_cleanup_surface(plSurface* ptSurface)
{
    vkDestroySurfaceKHR(gptGraphics->tInstance, ptSurface->tSurface, NULL);
    PL_FREE(ptSurface);
}

void
pl_cleanup_swapchain(plSwapchain* ptSwap)
{
    pl_sb_free(ptSwap->sbtSurfaceFormats);
    vkDestroySwapchainKHR(ptSwap->ptDevice->tLogicalDevice, ptSwap->tSwapChain, NULL);
    for (uint32_t i = 0; i < gptGraphics->uFramesInFlight; i++)
    {
        vkDestroySemaphore(ptSwap->ptDevice->tLogicalDevice, ptSwap->atImageAvailable[i], NULL);
    }
    pl__cleanup_common_swapchain(ptSwap);
}

void
pl_cleanup_device(plDevice* ptDevice)
{

    vkDeviceWaitIdle(ptDevice->tLogicalDevice);

    for (uint32_t i = 0; i < pl_sb_size(ptDevice->sbtTexturesHot); i++)
    {
        if (ptDevice->sbtTexturesHot[i].tImage && ptDevice->sbtTexturesHot[i].bOriginalView)
        {
            vkDestroyImage(ptDevice->tLogicalDevice, ptDevice->sbtTexturesHot[i].tImage, NULL);
        }
        ptDevice->sbtTexturesHot[i].tImage = VK_NULL_HANDLE;

        if (ptDevice->sbtTexturesHot[i].tImageView)
        {
            vkDestroyImageView(ptDevice->tLogicalDevice, ptDevice->sbtTexturesHot[i].tImageView, NULL);
            ptDevice->sbtTexturesHot[i].tImageView = VK_NULL_HANDLE;
        }
    }

    for (uint32_t i = 0; i < pl_sb_size(ptDevice->sbtSamplersHot); i++)
    {
        if (ptDevice->sbtSamplersHot[i])
            vkDestroySampler(ptDevice->tLogicalDevice, ptDevice->sbtSamplersHot[i], NULL);
    }

    for (uint32_t i = 0; i < pl_sb_size(ptDevice->sbtBindGroupsHot); i++)
    {
        vkDestroyDescriptorSetLayout(ptDevice->tLogicalDevice, ptDevice->sbtBindGroupsHot[i].tDescriptorSetLayout, NULL);
    }

    // vkDestroyBuffer(ptDevice->tLogicalDevice, ptDevice->tDummyBuffer, NULL);
    // ptDevice->tDummyBuffer = VK_NULL_HANDLE;

    for (uint32_t i = 0; i < pl_sb_size(ptDevice->sbtBuffersHot); i++)
    {
        if (ptDevice->sbtBuffersHot[i].tBuffer)
            vkDestroyBuffer(ptDevice->tLogicalDevice, ptDevice->sbtBuffersHot[i].tBuffer, NULL);
    }
    for (uint32_t i = 0; i < pl_sb_size(ptDevice->sbtShadersHot); i++)
    {
        plVulkanShader* ptVulkanShader = &ptDevice->sbtShadersHot[i];
        if (ptVulkanShader->tPipeline)
            vkDestroyPipeline(ptDevice->tLogicalDevice, ptVulkanShader->tPipeline, NULL);
        if (ptVulkanShader->tPipelineLayout)
            vkDestroyPipelineLayout(ptDevice->tLogicalDevice, ptVulkanShader->tPipelineLayout, NULL);
        if (ptVulkanShader->tVertexShaderModule)
            vkDestroyShaderModule(ptDevice->tLogicalDevice, ptVulkanShader->tVertexShaderModule, NULL);
        if (ptVulkanShader->tPixelShaderModule)
            vkDestroyShaderModule(ptDevice->tLogicalDevice, ptVulkanShader->tPixelShaderModule, NULL);
    }

    for (uint32_t i = 0; i < pl_sb_size(ptDevice->sbtComputeShadersHot); i++)
    {
        plVulkanComputeShader* ptVulkanShader = &ptDevice->sbtComputeShadersHot[i];
        if (ptVulkanShader->tPipeline)
            vkDestroyPipeline(ptDevice->tLogicalDevice, ptVulkanShader->tPipeline, NULL);
        if (ptVulkanShader->tPipelineLayout)
            vkDestroyPipelineLayout(ptDevice->tLogicalDevice, ptVulkanShader->tPipelineLayout, NULL);
        if (ptVulkanShader->tShaderModule)
            vkDestroyShaderModule(ptDevice->tLogicalDevice, ptVulkanShader->tShaderModule, NULL);
    }

    vkFreeMemory(ptDevice->tLogicalDevice, ptDevice->tDummyMemory, NULL);
    vkDestroyDescriptorSetLayout(ptDevice->tLogicalDevice, ptDevice->tNullDescriptorSetLayout, NULL);

    for (uint32_t i = 0; i < pl_sb_size(ptDevice->sbtBindGroupLayouts); i++)
    {
        if (ptDevice->sbtBindGroupLayouts[i].tDescriptorSetLayout)
            vkDestroyDescriptorSetLayout(ptDevice->tLogicalDevice, ptDevice->sbtBindGroupLayouts[i].tDescriptorSetLayout, NULL);
    }

    for (uint32_t i = 0; i < pl_sb_size(ptDevice->sbtRenderPassLayoutsHot); i++)
    {
        if (ptDevice->sbtRenderPassLayoutsHot[i].tRenderPass)
            vkDestroyRenderPass(ptDevice->tLogicalDevice, ptDevice->sbtRenderPassLayoutsHot[i].tRenderPass, NULL);
    }

    vkDestroyDescriptorSetLayout(ptDevice->tLogicalDevice, ptDevice->tDynamicDescriptorSetLayout, NULL);
    pl_sb_free(ptDevice->sbtTexturesHot);
    pl_sb_free(ptDevice->sbtSamplersHot);
    pl_sb_free(ptDevice->sbtBindGroupsHot);
    pl_sb_free(ptDevice->sbtBuffersHot);
    pl_sb_free(ptDevice->sbtShadersHot);
    pl_sb_free(ptDevice->sbtComputeShadersHot);
    pl_sb_free(ptDevice->sbtBindGroupLayouts);
    pl_sb_free(ptDevice->sbtTextureViewsHot);

    // cleanup per frame resources
    for (uint32_t i = 0; i < pl_sb_size(ptDevice->sbtFrames); i++)
    {
        plFrameContext* ptFrame = &ptDevice->sbtFrames[i];
        vkDestroySemaphore(ptDevice->tLogicalDevice, ptFrame->tRenderFinish, NULL);
        vkDestroyFence(ptDevice->tLogicalDevice, ptFrame->tInFlight, NULL);

        for (uint32_t j = 0; j < pl_sb_size(ptFrame->sbtDynamicBuffers); j++)
        {
            if (ptFrame->sbtDynamicBuffers[j].tMemory.uHandle)
                ptDevice->ptDynamicAllocator->free(ptDevice->ptDynamicAllocator->ptInst, &ptFrame->sbtDynamicBuffers[j].tMemory);
        }

        for (uint32_t j = 0; j < pl_sb_size(ptFrame->sbtRawFrameBuffers); j++)
        {
            vkDestroyFramebuffer(ptDevice->tLogicalDevice, ptFrame->sbtRawFrameBuffers[j], NULL);
            ptFrame->sbtRawFrameBuffers[j] = VK_NULL_HANDLE;
        }

        pl_sb_free(ptFrame->sbtRawFrameBuffers);
        pl_sb_free(ptFrame->sbtDynamicBuffers);
    }
    pl_sb_free(ptDevice->sbtFrames);

    for (uint32_t i = 0; i < pl_sb_size(ptDevice->sbtRenderPassesHot); i++)
    {
        if (ptDevice->sbtRenderPassesHot[i].tRenderPass)
            vkDestroyRenderPass(ptDevice->tLogicalDevice, ptDevice->sbtRenderPassesHot[i].tRenderPass, NULL);

        for (uint32_t j = 0; j < 6; j++)
        {
            if (ptDevice->sbtRenderPassesHot[i].atFrameBuffers[j])
                vkDestroyFramebuffer(ptDevice->tLogicalDevice, ptDevice->sbtRenderPassesHot[i].atFrameBuffers[j], NULL);
            ptDevice->sbtRenderPassesHot[i].atFrameBuffers[j] = VK_NULL_HANDLE;
        }
    }

    vkDestroyDescriptorPool(ptDevice->tLogicalDevice, ptDevice->tDynamicBufferDescriptorPool, NULL);

    vkDestroyDevice(ptDevice->tLogicalDevice, NULL);

    if (gptGraphics->tDbgMessenger)
    {
        PFN_vkDestroyDebugUtilsMessengerEXT tFunc = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(gptGraphics->tInstance, "vkDestroyDebugUtilsMessengerEXT");
        if (tFunc != NULL)
            tFunc(gptGraphics->tInstance, gptGraphics->tDbgMessenger, NULL);
    }
    pl_sb_free(ptDevice->sbtRenderPassesHot);
    pl_sb_free(ptDevice->sbtBindGroupLayoutFreeIndices);
    pl_sb_free(ptDevice->sbtRenderPassLayoutsHot);

    pl__cleanup_common_device(ptDevice);
}

void
pl_pipeline_barrier_blit(plBlitEncoder* ptEncoder, plStageFlags beforeStages, plAccessFlags beforeAccesses, plStageFlags afterStages, plAccessFlags afterAccesses)
{
    VkMemoryBarrier tMemoryBarrier = {
        .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = pl__vulkan_access_flags(beforeAccesses),
        .dstAccessMask = pl__vulkan_access_flags(afterAccesses)
    };

    vkCmdPipelineBarrier(ptEncoder->ptCommandBuffer->tCmdBuffer, pl__vulkan_stage_flags(beforeStages), pl__vulkan_stage_flags(afterStages), 0, 1, &tMemoryBarrier, 0, NULL, 0, NULL);
}

void
pl_pipeline_barrier_compute(plComputeEncoder* ptEncoder, plStageFlags beforeStages, plAccessFlags beforeAccesses, plStageFlags afterStages, plAccessFlags afterAccesses)
{
    VkMemoryBarrier tMemoryBarrier = {
        .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = pl__vulkan_access_flags(beforeAccesses),
        .dstAccessMask = pl__vulkan_access_flags(afterAccesses)
    };

    vkCmdPipelineBarrier(ptEncoder->ptCommandBuffer->tCmdBuffer, pl__vulkan_stage_flags(beforeStages), pl__vulkan_stage_flags(afterStages), 0, 1, &tMemoryBarrier, 0, NULL, 0, NULL);
}

void
pl_pipeline_barrier_render(plRenderEncoder* ptEncoder,  plStageFlags beforeStages, plAccessFlags beforeAccesses, plStageFlags afterStages, plAccessFlags afterAccesses)
{
    VkMemoryBarrier tMemoryBarrier = {
        .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = pl__vulkan_access_flags(beforeAccesses),
        .dstAccessMask = pl__vulkan_access_flags(afterAccesses)
    };

    vkCmdPipelineBarrier(ptEncoder->ptCommandBuffer->tCmdBuffer, pl__vulkan_stage_flags(beforeStages), pl__vulkan_stage_flags(afterStages), 0, 1, &tMemoryBarrier, 0, NULL, 0, NULL);
}

plComputeEncoder*
pl_begin_compute_pass(plCommandBuffer* ptCmdBuffer, const plPassResources* ptResources)
{
    plComputeEncoder* ptEncoder = pl__get_new_compute_encoder();
    ptEncoder->ptCommandBuffer = ptCmdBuffer;
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
    ptEncoder->ptCommandBuffer = ptCmdBuffer;
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
    plCommandBuffer* ptCmdBuffer = ptEncoder->ptCommandBuffer;

    for (uint32_t i = 0; i < uDispatchCount; i++)
    {
        const plDispatch* ptDispatch = &atDispatches[i];
        vkCmdDispatch(ptCmdBuffer->tCmdBuffer, ptDispatch->uGroupCountX, ptDispatch->uGroupCountY, ptDispatch->uGroupCountZ);
    }
}

void
pl_bind_compute_bind_groups(
    plComputeEncoder* ptEncoder, plComputeShaderHandle tHandle, uint32_t uFirst,
    uint32_t uCount, const plBindGroupHandle *atBindGroups, uint32_t uDynamicBindingCount, const plDynamicBinding* ptDynamicBinding)
{
    plCommandBuffer* ptCmdBuffer = ptEncoder->ptCommandBuffer;
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    plVulkanComputeShader* ptShader = &ptDevice->sbtComputeShadersHot[tHandle.uIndex];

    uint32_t uDummyByteOffset = 0;
    const uint32_t* puOffsets = &uDummyByteOffset;
    if (uDynamicBindingCount > 0)
    {
        puOffsets = &ptDynamicBinding->uByteOffset;
    }

    VkDescriptorSet atDescriptorSets[4] = {
        ptDevice->tNullDecriptorSet,
        ptDevice->tNullDecriptorSet,
        ptDevice->tNullDecriptorSet,
        ptDevice->tNullDynamicDecriptorSet
    };
        
    for (uint32_t i = 0; i < uCount; i++)
    {
        plVulkanBindGroup* ptBindGroup = &ptDevice->sbtBindGroupsHot[atBindGroups[i].uIndex];
        atDescriptorSets[uFirst + i] = ptBindGroup->tDescriptorSet;
    }

    if (ptDynamicBinding)
    {
        plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptDevice);
        atDescriptorSets[3] = ptCurrentFrame->sbtDynamicBuffers[ptDynamicBinding->uBufferHandle].tDescriptorSet;
    }

    if(uCount == 0)
    {
        if(uDynamicBindingCount == 0)
            return;

        vkCmdBindDescriptorSets(ptCmdBuffer->tCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ptShader->tPipelineLayout, 3, 1, &atDescriptorSets[3], 1, puOffsets);
    }
    else
        vkCmdBindDescriptorSets(ptCmdBuffer->tCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ptShader->tPipelineLayout, uFirst, 4 - uFirst, &atDescriptorSets[uFirst], 1, puOffsets);
}

void
pl_bind_graphics_bind_groups(plRenderEncoder* ptEncoder, plShaderHandle tHandle, uint32_t uFirst, uint32_t uCount, const plBindGroupHandle *atBindGroups, uint32_t uDynamicBindingCount, const plDynamicBinding* ptDynamicBinding)
{
    plCommandBuffer* ptCmdBuffer = ptEncoder->ptCommandBuffer;
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    plVulkanShader* ptShader = &ptDevice->sbtShadersHot[tHandle.uIndex];


    uint32_t uDummyByteOffset = 0;
    const uint32_t* puOffsets = &uDummyByteOffset;
    if (uDynamicBindingCount > 0)
    {
        puOffsets = &ptDynamicBinding->uByteOffset;
    }

    VkDescriptorSet atDescriptorSets[4] = {
        ptDevice->tNullDecriptorSet,
        ptDevice->tNullDecriptorSet,
        ptDevice->tNullDecriptorSet,
        ptDevice->tNullDynamicDecriptorSet
    };
        
    for (uint32_t i = 0; i < uCount; i++)
    {
        plVulkanBindGroup* ptBindGroup = &ptDevice->sbtBindGroupsHot[atBindGroups[i].uIndex];
        atDescriptorSets[uFirst + i] = ptBindGroup->tDescriptorSet;
    }

    if (ptDynamicBinding)
    {
        plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptDevice);
        atDescriptorSets[3] = ptCurrentFrame->sbtDynamicBuffers[ptDynamicBinding->uBufferHandle].tDescriptorSet;
    }

    if(uCount == 0)
    {
        if(uDynamicBindingCount == 0)
            return;

        vkCmdBindDescriptorSets(ptCmdBuffer->tCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ptShader->tPipelineLayout, 3, 1, &atDescriptorSets[3], 1, puOffsets);
    }
    else
        vkCmdBindDescriptorSets(ptCmdBuffer->tCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ptShader->tPipelineLayout, uFirst, 4 - uFirst, &atDescriptorSets[uFirst], 1, puOffsets);
}

void
pl_submit_command_buffer(plCommandBuffer* ptCmdBuffer, const plSubmitInfo* ptSubmitInfo)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptDevice);

    VkSemaphore atWaitSemaphores[PL_MAX_TIMELINE_SEMAPHORES] = {0};
    VkSemaphore atSignalSemaphores[PL_MAX_TIMELINE_SEMAPHORES] = {0};
    VkPipelineStageFlags atWaitStages[PL_MAX_TIMELINE_SEMAPHORES] = {0};
    atWaitStages[0] = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;

    VkSubmitInfo tSubmitInfo = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &ptCmdBuffer->tCmdBuffer,
    };

    VkTimelineSemaphoreSubmitInfo tTimelineInfo = {
        .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
        .pNext = NULL,
    };

    if (ptCmdBuffer->tBeginInfo.uWaitSemaphoreCount != UINT32_MAX)
    {
        for (uint32_t i = 0; i < ptCmdBuffer->tBeginInfo.uWaitSemaphoreCount; i++)
        {
            atWaitSemaphores[i] = ptCmdBuffer->tBeginInfo.atWaitSempahores[i]->tSemaphore;
            atWaitStages[i] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
        }

        tTimelineInfo.waitSemaphoreValueCount = ptCmdBuffer->tBeginInfo.uWaitSemaphoreCount;
        tTimelineInfo.pWaitSemaphoreValues = ptCmdBuffer->tBeginInfo.auWaitSemaphoreValues;

        tSubmitInfo.pNext = &tTimelineInfo;
        tSubmitInfo.pWaitSemaphores = atWaitSemaphores;
        tSubmitInfo.pWaitDstStageMask = atWaitStages;
        tSubmitInfo.waitSemaphoreCount = ptCmdBuffer->tBeginInfo.uWaitSemaphoreCount;
    }

    if (ptSubmitInfo)
    {

        for (uint32_t i = 0; i < ptSubmitInfo->uSignalSemaphoreCount; i++)
        {
            atSignalSemaphores[i] = ptSubmitInfo->atSignalSempahores[i]->tSemaphore;
        }

        tTimelineInfo.signalSemaphoreValueCount = ptSubmitInfo->uSignalSemaphoreCount;
        tTimelineInfo.pSignalSemaphoreValues = ptSubmitInfo->auSignalSemaphoreValues;

        tSubmitInfo.pNext = &tTimelineInfo;
        tSubmitInfo.pSignalSemaphores = atSignalSemaphores;
        tSubmitInfo.signalSemaphoreCount = ptSubmitInfo->uSignalSemaphoreCount;
    }

    PL_VULKAN(vkQueueSubmit(ptDevice->tGraphicsQueue, 1, &tSubmitInfo, VK_NULL_HANDLE));
    pl_sb_push(ptCmdBuffer->ptPool->sbtPendingCommandBuffers, ptCmdBuffer->tCmdBuffer);
}

void
pl_wait_on_command_buffer(plCommandBuffer* ptCmdBuffer)
{
    PL_VULKAN(vkQueueWaitIdle(ptCmdBuffer->ptDevice->tGraphicsQueue));
}

void
pl_return_command_buffer(plCommandBuffer* ptCmdBuffer)
{
    ptCmdBuffer->ptNext = ptCmdBuffer->ptPool->ptCommandBufferFreeList;
    ptCmdBuffer->ptPool->ptCommandBufferFreeList = ptCmdBuffer;
}

plBindGroupPool*
pl_create_bind_group_pool(plDevice* ptDevice, const plBindGroupPoolDesc* ptDesc)
{
    plBindGroupPool* ptPool = PL_ALLOC(sizeof(plBindGroupPool));
    memset(ptPool, 0, sizeof(plBindGroupPool));
    ptPool->tDesc = *ptDesc;

    const size_t szMaxSets =
        ptPool->tDesc.szSamplerBindings +
        ptPool->tDesc.szUniformBufferBindings +
        ptPool->tDesc.szStorageBufferBindings +
        ptPool->tDesc.szSampledTextureBindings +
        ptPool->tDesc.szStorageTextureBindings +
        ptPool->tDesc.szAttachmentTextureBindings;

    VkDescriptorPoolSize atPoolSizes[6] = {0};
    uint32_t uPoolSizeCount = 0;

    if (ptPool->tDesc.szSamplerBindings > 0)
    {
        atPoolSizes[uPoolSizeCount].descriptorCount = (uint32_t)ptPool->tDesc.szSamplerBindings;
        atPoolSizes[uPoolSizeCount].type = VK_DESCRIPTOR_TYPE_SAMPLER;
        uPoolSizeCount++;
    }

    if (ptPool->tDesc.szSampledTextureBindings > 0)
    {
        atPoolSizes[uPoolSizeCount].descriptorCount = (uint32_t)ptPool->tDesc.szSampledTextureBindings;
        atPoolSizes[uPoolSizeCount].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        uPoolSizeCount++;
    }

    if (ptPool->tDesc.szStorageTextureBindings > 0)
    {
        atPoolSizes[uPoolSizeCount].descriptorCount = (uint32_t)ptPool->tDesc.szStorageTextureBindings;
        atPoolSizes[uPoolSizeCount].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        uPoolSizeCount++;
    }

    if (ptPool->tDesc.szUniformBufferBindings > 0)
    {
        atPoolSizes[uPoolSizeCount].descriptorCount = (uint32_t)ptPool->tDesc.szUniformBufferBindings;
        atPoolSizes[uPoolSizeCount].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uPoolSizeCount++;
    }

    if (ptPool->tDesc.szStorageBufferBindings > 0)
    {
        atPoolSizes[uPoolSizeCount].descriptorCount = (uint32_t)ptPool->tDesc.szStorageBufferBindings;
        atPoolSizes[uPoolSizeCount].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        uPoolSizeCount++;
    }

    if (ptPool->tDesc.szAttachmentTextureBindings > 0)
    {
        atPoolSizes[uPoolSizeCount].descriptorCount = (uint32_t)ptPool->tDesc.szAttachmentTextureBindings;
        atPoolSizes[uPoolSizeCount].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        uPoolSizeCount++;
    }

    VkDescriptorPoolCreateInfo tDescriptorPoolInfo = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = (uint32_t)szMaxSets,
        .poolSizeCount = uPoolSizeCount,
        .pPoolSizes    = atPoolSizes
    };
    if (ptDevice->tInfo.tCapabilities & PL_DEVICE_CAPABILITY_BIND_GROUP_INDEXING)
    {
        tDescriptorPoolInfo.flags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
    }
    PL_VULKAN(vkCreateDescriptorPool(ptDevice->tLogicalDevice, &tDescriptorPoolInfo, NULL, &ptPool->tDescriptorPool));
    ptPool->ptDevice = ptDevice;
    return ptPool;
}

void
pl_reset_bind_group_pool(plBindGroupPool* ptPool)
{
    vkResetDescriptorPool(ptPool->ptDevice->tLogicalDevice, ptPool->tDescriptorPool, 0);
}

void
pl_cleanup_bind_group_pool(plBindGroupPool* ptPool)
{
    vkDestroyDescriptorPool(ptPool->ptDevice->tLogicalDevice, ptPool->tDescriptorPool, NULL);
    PL_FREE(ptPool);
}

plCommandPool *
pl_create_command_pool(plDevice* ptDevice, const plCommandPoolDesc* ptDesc)
{
    plCommandPool* ptPool = PL_ALLOC(sizeof(plCommandPool));
    memset(ptPool, 0, sizeof(plCommandPool));

    ptPool->ptDevice = ptDevice;
    const VkCommandPoolCreateInfo tCommandPoolInfo = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = ptDevice->iGraphicsQueueFamily,
        .flags            = 0
    };
    PL_VULKAN(vkCreateCommandPool(ptDevice->tLogicalDevice, &tCommandPoolInfo, NULL, &ptPool->tCmdPool));
    return ptPool;
}

void
pl_cleanup_command_pool(plCommandPool* ptPool)
{
    plCommandBuffer* ptCurrentCommandBuffer = ptPool->ptCommandBufferFreeList;
    while (ptCurrentCommandBuffer)
    {
        plCommandBuffer* ptNextCommandBuffer = ptCurrentCommandBuffer->ptNext;
        PL_FREE(ptCurrentCommandBuffer);
        ptCurrentCommandBuffer = ptNextCommandBuffer;
    }
    vkDestroyCommandPool(ptPool->ptDevice->tLogicalDevice, ptPool->tCmdPool, NULL);
    pl_sb_free(ptPool->sbtPendingCommandBuffers);
    pl_sb_free(ptPool->sbtReadyCommandBuffers);
    PL_FREE(ptPool);
}

void
pl_reset_command_pool(plCommandPool* ptPool, plCommandPoolResetFlags tFlags)
{
    for (uint32_t i = 0; i < pl_sb_size(ptPool->sbtPendingCommandBuffers); i++)
    {
        pl_sb_push(ptPool->sbtReadyCommandBuffers, ptPool->sbtPendingCommandBuffers[i]);
    }
    pl_sb_reset(ptPool->sbtPendingCommandBuffers);
    if(tFlags & PL_COMMAND_POOL_RESET_FLAG_FREE_RESOURCES)
    {
        PL_VULKAN(vkResetCommandPool(ptPool->ptDevice->tLogicalDevice, ptPool->tCmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT));
    }
    else
    {
        PL_VULKAN(vkResetCommandPool(ptPool->ptDevice->tLogicalDevice, ptPool->tCmdPool, 0));
    }
}

void
pl_reset_command_buffer(plCommandBuffer* ptCommandBuffer)
{
    VkCommandBuffer tCmdBuffer = VK_NULL_HANDLE;
    if (pl_sb_size(ptCommandBuffer->ptPool->sbtReadyCommandBuffers) > 0)
    {
        tCmdBuffer = pl_sb_pop(ptCommandBuffer->ptPool->sbtReadyCommandBuffers);
    }
    else
    {
        const VkCommandBufferAllocateInfo tAllocInfo = {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool        = ptCommandBuffer->ptPool->tCmdPool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };
        PL_VULKAN(vkAllocateCommandBuffers(ptCommandBuffer->ptPool->ptDevice->tLogicalDevice, &tAllocInfo, &tCmdBuffer));
    }
    ptCommandBuffer->tCmdBuffer = tCmdBuffer;
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

    VkCommandBuffer tCmdBuffer = VK_NULL_HANDLE;
    if (pl_sb_size(ptPool->sbtReadyCommandBuffers) > 0)
    {
        tCmdBuffer = pl_sb_pop(ptPool->sbtReadyCommandBuffers);
    }
    else
    {
        const VkCommandBufferAllocateInfo tAllocInfo = {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool        = ptPool->tCmdPool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };
        PL_VULKAN(vkAllocateCommandBuffers(ptPool->ptDevice->tLogicalDevice, &tAllocInfo, &tCmdBuffer));
    }
    ptCommandBuffer->tCmdBuffer = tCmdBuffer;
    ptCommandBuffer->ptDevice = ptPool->ptDevice;
    ptCommandBuffer->ptPool = ptPool;
    return ptCommandBuffer;
}

void
pl_copy_buffer(plBlitEncoder* ptEncoder, plBufferHandle tSource, plBufferHandle tDestination, uint32_t uSourceOffset, uint32_t uDestinationOffset, size_t szSize)
{
    plCommandBuffer* ptCmdBuffer = ptEncoder->ptCommandBuffer;
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    const VkBufferCopy tCopyRegion = {
        .size      = szSize,
        .srcOffset = uSourceOffset
    };

    vkCmdCopyBuffer(ptCmdBuffer->tCmdBuffer, ptDevice->sbtBuffersHot[tSource.uIndex].tBuffer, ptDevice->sbtBuffersHot[tDestination.uIndex].tBuffer, 1, &tCopyRegion);
}

void
pl_copy_buffer_to_texture(plBlitEncoder* ptEncoder, plBufferHandle tBufferHandle, plTextureHandle tTextureHandle, uint32_t uRegionCount, const plBufferImageCopy* ptRegions)
{
    plCommandBuffer* ptCmdBuffer = ptEncoder->ptCommandBuffer;
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    plTexture* ptColdTexture = pl__get_texture(ptDevice, tTextureHandle);
    VkImageSubresourceRange *atSubResourceRanges = pl_temp_allocator_alloc(&gptGraphics->tTempAllocator, sizeof(VkImageSubresourceRange) * uRegionCount);
    VkBufferImageCopy *atCopyRegions = pl_temp_allocator_alloc(&gptGraphics->tTempAllocator, sizeof(VkBufferImageCopy) * uRegionCount);
    memset(atSubResourceRanges, 0, sizeof(VkImageSubresourceRange) * uRegionCount);
    memset(atCopyRegions, 0, sizeof(VkBufferImageCopy) * uRegionCount);

    for (uint32_t i = 0; i < uRegionCount; i++)
    {
        VkImageLayout tLayout = ptRegions[i].tCurrentImageUsage == 0 ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : pl__vulkan_layout(ptRegions[i].tCurrentImageUsage);
        atSubResourceRanges[i].aspectMask = ptColdTexture->tDesc.tUsage & PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        atSubResourceRanges[i].baseMipLevel = ptRegions[i].uMipLevel;
        atSubResourceRanges[i].levelCount = 1;
        atSubResourceRanges[i].baseArrayLayer = ptRegions[i].uBaseArrayLayer;
        atSubResourceRanges[i].layerCount = ptRegions[i].uLayerCount;
        pl__transition_image_layout(ptCmdBuffer->tCmdBuffer, ptDevice->sbtTexturesHot[tTextureHandle.uIndex].tImage, tLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, atSubResourceRanges[i], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        atCopyRegions[i].bufferOffset = ptRegions[i].szBufferOffset;
        atCopyRegions[i].bufferRowLength = ptRegions[i].uBufferRowLength;
        atCopyRegions[i].bufferImageHeight = ptRegions[i].uImageHeight;
        atCopyRegions[i].imageSubresource.aspectMask = atSubResourceRanges[i].aspectMask;
        atCopyRegions[i].imageSubresource.mipLevel = ptRegions[i].uMipLevel;
        atCopyRegions[i].imageSubresource.baseArrayLayer = ptRegions[i].uBaseArrayLayer;
        atCopyRegions[i].imageSubresource.layerCount = ptRegions[i].uLayerCount;
        atCopyRegions[i].imageOffset.x = ptRegions[i].iImageOffsetX;
        atCopyRegions[i].imageOffset.y = ptRegions[i].iImageOffsetY;
        atCopyRegions[i].imageOffset.z = ptRegions[i].iImageOffsetZ;
        atCopyRegions[i].imageExtent.width = ptRegions[i].uImageWidth;
        atCopyRegions[i].imageExtent.height = ptRegions[i].uImageHeight;
        atCopyRegions[i].imageExtent.depth = ptRegions[i].uImageDepth;
    }
    vkCmdCopyBufferToImage(ptCmdBuffer->tCmdBuffer, ptDevice->sbtBuffersHot[tBufferHandle.uIndex].tBuffer, ptDevice->sbtTexturesHot[tTextureHandle.uIndex].tImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, uRegionCount, atCopyRegions);

    for (uint32_t i = 0; i < uRegionCount; i++)
    {
        VkImageLayout tLayout = ptRegions[i].tCurrentImageUsage == 0 ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : pl__vulkan_layout(ptRegions[i].tCurrentImageUsage);
        pl__transition_image_layout(ptCmdBuffer->tCmdBuffer, ptDevice->sbtTexturesHot[tTextureHandle.uIndex].tImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, tLayout, atSubResourceRanges[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    }

    pl_temp_allocator_reset(&gptGraphics->tTempAllocator);
}

void
pl_destroy_buffer(plDevice* ptDevice, plBufferHandle tHandle)
{
    vkDestroyBuffer(ptDevice->tLogicalDevice, ptDevice->sbtBuffersHot[tHandle.uIndex].tBuffer, NULL);
    ptDevice->sbtBuffersHot[tHandle.uIndex].tBuffer = VK_NULL_HANDLE;
    ptDevice->sbtBuffersCold[tHandle.uIndex]._uGeneration++;
    pl_sb_push(ptDevice->sbtBufferFreeIndices, tHandle.uIndex);

    plBuffer* ptBuffer = &ptDevice->sbtBuffersCold[tHandle.uIndex];
    if (ptBuffer->tMemoryAllocation.ptAllocator)
        ptBuffer->tMemoryAllocation.ptAllocator->free(ptBuffer->tMemoryAllocation.ptAllocator->ptInst, &ptBuffer->tMemoryAllocation);
    else
        pl_free_memory(ptDevice, &ptBuffer->tMemoryAllocation);
}

void
pl_destroy_texture(plDevice* ptDevice, plTextureHandle tHandle)
{
    plVulkanTexture* ptVulkanResource = &ptDevice->sbtTexturesHot[tHandle.uIndex];
    vkDestroyImage(ptDevice->tLogicalDevice, ptVulkanResource->tImage, NULL);
    ptVulkanResource->tImage = VK_NULL_HANDLE;
    pl_sb_push(ptDevice->sbtTextureFreeIndices, tHandle.uIndex);
    ptDevice->sbtTexturesCold[tHandle.uIndex]._uGeneration++;

    plTexture* ptTexture = &ptDevice->sbtTexturesCold[tHandle.uIndex];
    if (ptTexture->tMemoryAllocation.ptAllocator)
        ptTexture->tMemoryAllocation.ptAllocator->free(ptTexture->tMemoryAllocation.ptAllocator->ptInst, &ptTexture->tMemoryAllocation);
    else
        pl_free_memory(ptDevice, &ptTexture->tMemoryAllocation);
}

void
pl_destroy_sampler(plDevice* ptDevice, plSamplerHandle tHandle)
{
    vkDestroySampler(ptDevice->tLogicalDevice, ptDevice->sbtSamplersHot[tHandle.uIndex], NULL);
    ptDevice->sbtSamplersHot[tHandle.uIndex] = VK_NULL_HANDLE;
    ptDevice->sbtSamplersCold[tHandle.uIndex]._uGeneration++;
    pl_sb_push(ptDevice->sbtSamplerFreeIndices, tHandle.uIndex);
}

void
pl_destroy_bind_group(plDevice* ptDevice, plBindGroupHandle tHandle)
{
    ptDevice->sbtBindGroupsCold[tHandle.uIndex]._uGeneration++;

    plVulkanBindGroup* ptVulkanResource = &ptDevice->sbtBindGroupsHot[tHandle.uIndex];
    ptVulkanResource->tDescriptorSet = VK_NULL_HANDLE;
    vkDestroyDescriptorSetLayout(ptDevice->tLogicalDevice, ptVulkanResource->tDescriptorSetLayout, NULL);
    ptVulkanResource->tDescriptorSetLayout = VK_NULL_HANDLE;
    pl_sb_push(ptDevice->sbtBindGroupFreeIndices, tHandle.uIndex);
}

void
pl_destroy_render_pass(plDevice* ptDevice, plRenderPassHandle tHandle)
{
    ptDevice->sbtRenderPassesCold[tHandle.uIndex]._uGeneration++;

    plVulkanRenderPass* ptVulkanResource = &ptDevice->sbtRenderPassesHot[tHandle.uIndex];
    for (uint32_t j = 0; j < gptGraphics->uFramesInFlight; j++)
    {
        if (ptVulkanResource->atFrameBuffers[j])
            vkDestroyFramebuffer(ptDevice->tLogicalDevice, ptVulkanResource->atFrameBuffers[j], NULL);
        ptVulkanResource->atFrameBuffers[j] = VK_NULL_HANDLE;
    }
    if (ptVulkanResource->tRenderPass)
        vkDestroyRenderPass(ptDevice->tLogicalDevice, ptVulkanResource->tRenderPass, NULL);
    ptVulkanResource->tRenderPass = VK_NULL_HANDLE;
    pl_sb_push(ptDevice->sbtRenderPassFreeIndices, tHandle.uIndex);
}

void
pl_destroy_render_pass_layout(plDevice* ptDevice, plRenderPassLayoutHandle tHandle)
{
    ptDevice->sbtRenderPassLayoutsCold[tHandle.uIndex]._uGeneration++;

    plVulkanRenderPassLayout* ptVulkanResource = &ptDevice->sbtRenderPassLayoutsHot[tHandle.uIndex];
    vkDestroyRenderPass(ptDevice->tLogicalDevice, ptVulkanResource->tRenderPass, NULL);
    pl_sb_push(ptDevice->sbtRenderPassLayoutFreeIndices, tHandle.uIndex);
}

void
pl_destroy_shader(plDevice* ptDevice, plShaderHandle tHandle)
{
    ptDevice->sbtShadersCold[tHandle.uIndex]._uGeneration++;

    plShader* ptResource = &ptDevice->sbtShadersCold[tHandle.uIndex];

    plVulkanShader* ptVariantVulkanResource = &ptDevice->sbtShadersHot[tHandle.uIndex];
    vkDestroyPipelineLayout(ptDevice->tLogicalDevice, ptVariantVulkanResource->tPipelineLayout, NULL);
    vkDestroyPipeline(ptDevice->tLogicalDevice, ptVariantVulkanResource->tPipeline, NULL);
    ptVariantVulkanResource->tPipelineLayout = VK_NULL_HANDLE;
    ptVariantVulkanResource->tPipeline = VK_NULL_HANDLE;
    pl_sb_push(ptDevice->sbtShaderFreeIndices, tHandle.uIndex);
    for (uint32_t k = 0; k < ptResource->tDesc._uBindGroupLayoutCount; k++)
    {
        plVulkanBindGroupLayout* ptVulkanBindGroupLayout = &ptDevice->sbtBindGroupLayouts[ptResource->tDesc.atBindGroupLayouts[k]._uHandle];
        vkDestroyDescriptorSetLayout(ptDevice->tLogicalDevice, ptVulkanBindGroupLayout->tDescriptorSetLayout, NULL);
        ptVulkanBindGroupLayout->tDescriptorSetLayout = VK_NULL_HANDLE;
        pl_sb_push(ptDevice->sbtBindGroupLayoutFreeIndices, ptResource->tDesc.atBindGroupLayouts[k]._uHandle);
    }
}

void
pl_destroy_compute_shader(plDevice* ptDevice, plComputeShaderHandle tHandle)
{
    ptDevice->sbtComputeShadersCold[tHandle.uIndex]._uGeneration++;

    plComputeShader* ptResource = &ptDevice->sbtComputeShadersCold[tHandle.uIndex];

    plVulkanComputeShader* ptVariantVulkanResource = &ptDevice->sbtComputeShadersHot[tHandle.uIndex];
    vkDestroyPipelineLayout(ptDevice->tLogicalDevice, ptVariantVulkanResource->tPipelineLayout, NULL);
    vkDestroyPipeline(ptDevice->tLogicalDevice, ptVariantVulkanResource->tPipeline, NULL);
    ptVariantVulkanResource->tPipelineLayout = VK_NULL_HANDLE;
    ptVariantVulkanResource->tPipeline = VK_NULL_HANDLE;
    pl_sb_push(ptDevice->sbtComputeShaderFreeIndices, tHandle.uIndex);

    for (uint32_t k = 0; k < ptResource->tDesc._uBindGroupLayoutCount + 1; k++)
    {
        plVulkanBindGroupLayout* ptVulkanBindGroupLayout = &ptDevice->sbtBindGroupLayouts[ptResource->tDesc.atBindGroupLayouts[k]._uHandle];
        vkDestroyDescriptorSetLayout(ptDevice->tLogicalDevice, ptVulkanBindGroupLayout->tDescriptorSetLayout, NULL);
        ptVulkanBindGroupLayout->tDescriptorSetLayout = VK_NULL_HANDLE;
        pl_sb_push(ptDevice->sbtBindGroupLayoutFreeIndices, ptResource->tDesc.atBindGroupLayouts[k]._uHandle);
    }
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static void
pl__create_bind_group_layout(plDevice* ptDevice, plBindGroupLayout* ptLayout, const char* pcName)
{
    plVulkanBindGroupLayout tVulkanBindGroupLayout = {0};

    uint32_t uBindGroupLayoutIndex = UINT32_MAX;
    if (pl_sb_size(ptDevice->sbtBindGroupLayoutFreeIndices) > 0)
        uBindGroupLayoutIndex = pl_sb_pop(ptDevice->sbtBindGroupLayoutFreeIndices);
    else
    {
        uBindGroupLayoutIndex = pl_sb_size(ptDevice->sbtBindGroupLayouts);
        pl_sb_add(ptDevice->sbtBindGroupLayouts);
    }
    ptLayout->_uHandle = uBindGroupLayoutIndex;
    ptLayout->_uBufferBindingCount = 0;
    ptLayout->_uTextureBindingCount = 0;
    ptLayout->_uSamplerBindingCount = 0;

    // count bindings
    for(uint32_t i = 0; i < PL_MAX_TEXTURES_PER_BIND_GROUP; i++)
    {
        if(ptLayout->atTextureBindings[i].tStages == PL_STAGE_NONE)
            break;
        ptLayout->_uTextureBindingCount++;
    }

    for(uint32_t i = 0; i < PL_MAX_BUFFERS_PER_BIND_GROUP; i++)
    {
        if(ptLayout->atBufferBindings[i].tStages == PL_STAGE_NONE)
            break;
        ptLayout->_uBufferBindingCount++;
    }

    for(uint32_t i = 0; i < PL_MAX_SAMPLERS_PER_BIND_GROUP; i++)
    {
        if(ptLayout->atSamplerBindings[i].tStages == PL_STAGE_NONE)
            break;
        ptLayout->_uSamplerBindingCount++;
    }

    uint32_t uCurrentBinding = 0;
    const uint32_t uDescriptorBindingCount = ptLayout->_uTextureBindingCount + ptLayout->_uBufferBindingCount + ptLayout->_uSamplerBindingCount;
    VkDescriptorSetLayoutBinding *atDescriptorSetLayoutBindings = pl_temp_allocator_alloc(&gptGraphics->tTempAllocator, uDescriptorBindingCount * sizeof(VkDescriptorSetLayoutBinding));
    VkDescriptorBindingFlagsEXT *atDescriptorSetLayoutFlags = pl_temp_allocator_alloc(&gptGraphics->tTempAllocator, uDescriptorBindingCount * sizeof(VkDescriptorBindingFlagsEXT));

    for (uint32_t i = 0; i < ptLayout->_uBufferBindingCount; i++)
    {
        VkDescriptorSetLayoutBinding tBinding = {
            .binding            = ptLayout->atBufferBindings[i].uSlot,
            .descriptorType     = ptLayout->atBufferBindings[i].tType == PL_BUFFER_BINDING_TYPE_STORAGE ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount    = 1,
            .stageFlags         = pl__vulkan_stage_flags(ptLayout->atBufferBindings[i].tStages),
            .pImmutableSamplers = NULL
        };
        atDescriptorSetLayoutFlags[uCurrentBinding] = 0;
        atDescriptorSetLayoutBindings[uCurrentBinding++] = tBinding;
    }

    for (uint32_t i = 0; i < ptLayout->_uTextureBindingCount; i++)
    {
        VkDescriptorSetLayoutBinding tBinding = {
            .binding            = ptLayout->atTextureBindings[i].uSlot,
            .descriptorCount    = ptLayout->atTextureBindings[i].uDescriptorCount,
            .stageFlags         = pl__vulkan_stage_flags(ptLayout->atTextureBindings[i].tStages),
            .pImmutableSamplers = NULL
        };

        if (ptLayout->atTextureBindings[i].tType == PL_TEXTURE_BINDING_TYPE_SAMPLED)
            tBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        else if (ptLayout->atTextureBindings[i].tType == PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT)
            tBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        else if (ptLayout->atTextureBindings[i].tType == PL_TEXTURE_BINDING_TYPE_STORAGE)
            tBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

        if (tBinding.descriptorCount == 0)
            tBinding.descriptorCount = 1;
        atDescriptorSetLayoutFlags[uCurrentBinding] = 0;
        if (ptLayout->atTextureBindings[i].bNonUniformIndexing)
            atDescriptorSetLayoutFlags[uCurrentBinding] |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT;
        atDescriptorSetLayoutBindings[uCurrentBinding++] = tBinding;
    }

    for (uint32_t i = 0; i < ptLayout->_uSamplerBindingCount; i++)
    {
        VkDescriptorSetLayoutBinding tBinding = {
            .binding            = ptLayout->atSamplerBindings[i].uSlot,
            .descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount    = 1,
            .stageFlags         = pl__vulkan_stage_flags(ptLayout->atSamplerBindings[i].tStages),
            .pImmutableSamplers = NULL
        };
        atDescriptorSetLayoutFlags[uCurrentBinding] = 0;
        atDescriptorSetLayoutBindings[uCurrentBinding++] = tBinding;
    }

    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT setLayoutBindingFlags = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,
        .bindingCount  = uDescriptorBindingCount,
        .pBindingFlags = atDescriptorSetLayoutFlags,
        .pNext         = NULL
    };

    // create descriptor set layout
    const VkDescriptorSetLayoutCreateInfo tDescriptorSetLayoutInfo = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = uDescriptorBindingCount,
        .pBindings    = atDescriptorSetLayoutBindings,
        .pNext        = (ptDevice->tInfo.tCapabilities & PL_DEVICE_CAPABILITY_BIND_GROUP_INDEXING) ? &setLayoutBindingFlags : NULL,
        .flags        = (ptDevice->tInfo.tCapabilities & PL_DEVICE_CAPABILITY_BIND_GROUP_INDEXING) ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT : 0
    };
    PL_VULKAN(vkCreateDescriptorSetLayout(ptDevice->tLogicalDevice, &tDescriptorSetLayoutInfo, NULL, &tVulkanBindGroupLayout.tDescriptorSetLayout));

    if (pcName)
        pl__set_vulkan_object_name(ptDevice, (uint64_t)tVulkanBindGroupLayout.tDescriptorSetLayout, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT, pcName);

    ptDevice->sbtBindGroupLayouts[uBindGroupLayoutIndex] = tVulkanBindGroupLayout;
    pl_temp_allocator_reset(&gptGraphics->tTempAllocator);
}

static void
pl__fill_common_render_pass_data(plRenderPassLayoutDesc* ptDesc, plRenderPassLayout* ptLayout, plRenderPassCommonData* ptDataOut)
{
    ptDesc->_uSubpassCount = 0;
    ptDataOut->uDependencyCount = 2;
    ptLayout->_uAttachmentCount = 0;
    ptDataOut->uColorAttachmentCount = 0;

    // find attachment count & descriptions
    for (uint32_t i = 0; i < PL_MAX_RENDER_TARGETS; i++)
    {
        if (ptDesc->atRenderTargets[i].tFormat == PL_FORMAT_UNKNOWN)
            break;

        ptDataOut->atAttachments[i].format = pl__vulkan_format(ptDesc->atRenderTargets[i].tFormat);
        ptDataOut->atAttachments[i].samples = (ptDesc->atRenderTargets[i].tSamples == 0) ? 1 : ptDesc->atRenderTargets[i].tSamples;

        if (ptDesc->atRenderTargets[i].bDepth)
        {
            // overwritten by actual renderpass
            ptDataOut->atAttachments[i].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            ptDataOut->atAttachments[i].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            ptDataOut->tDepthAttachmentReference.attachment = i;
            ptDataOut->tDepthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }
        else if (ptDesc->atRenderTargets[i].bResolve)
        {
            // overwritten by actual renderpass
            ptDataOut->atAttachments[i].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            ptDataOut->atAttachments[i].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            ptDataOut->tResolveAttachmentReference.attachment = i;
            ptDataOut->tResolveAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }
        else
        {
            // overwritten by actual renderpass
            ptDataOut->atAttachments[i].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            ptDataOut->atAttachments[i].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            ptDataOut->uColorAttachmentCount++;
        }
        ptLayout->_uAttachmentCount++;
    }

    // fill out subpasses
    for (uint32_t i = 0; i < PL_MAX_SUBPASSES; i++)
    {
        plSubpass* ptSubpass = &ptDesc->atSubpasses[i];

        if(ptSubpass->uRenderTargetCount == 0 && ptSubpass->uSubpassInputCount == 0)
            break;

        ptDesc->_uSubpassCount++;

        ptSubpass->_uColorAttachmentCount = 0;
        ptDataOut->atSubpasses[i].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        // render targets
        uint32_t uCurrentColorAttachment = 0;
        for (uint32_t j = 0; j < ptSubpass->uRenderTargetCount; j++)
        {
            if (ptDesc->atRenderTargets[ptSubpass->auRenderTargets[j]].bDepth)
            {
                ptDataOut->atSubpasses[i].pDepthStencilAttachment = &ptDataOut->tDepthAttachmentReference;
            }
            else if (ptDesc->atRenderTargets[ptSubpass->auRenderTargets[j]].bResolve)
            {
                ptDataOut->atSubpasses[i].pResolveAttachments = &ptDataOut->tResolveAttachmentReference;
            }
            else
            {
                ptDataOut->atSubpassColorAttachmentReferences[i][uCurrentColorAttachment].attachment = ptSubpass->auRenderTargets[j];
                ptDataOut->atSubpassColorAttachmentReferences[i][uCurrentColorAttachment].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                uCurrentColorAttachment++;
                ptSubpass->_uColorAttachmentCount++;
            }
        }
        ptDataOut->atSubpasses[i].colorAttachmentCount = uCurrentColorAttachment;
        ptDataOut->atSubpasses[i].pColorAttachments = ptDataOut->atSubpassColorAttachmentReferences[i];

        // input attachments
        for (uint32_t j = 0; j < ptSubpass->uSubpassInputCount; j++)
        {
            const uint32_t uInput = ptSubpass->auSubpassInputs[j];
            ptDataOut->atSubpassInputAttachmentReferences[i][j].attachment = uInput;
            ptDataOut->atSubpassInputAttachmentReferences[i][j].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
        ptDataOut->atSubpasses[i].inputAttachmentCount = ptSubpass->uSubpassInputCount;
        ptDataOut->atSubpasses[i].pInputAttachments = ptDataOut->atSubpassInputAttachmentReferences[i];

        // dependencies
        if (i > 0)
        {
            ptDataOut->atSubpassDependencies[ptDataOut->uDependencyCount] = (VkSubpassDependency){
                .srcSubpass      = i - 1,
                .dstSubpass      = i,
                .srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                .dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                .srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .dstAccessMask   = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
            };
            ptDataOut->uDependencyCount++;
        }
    }
    // ensure everything outside render pass is finished
    ptDataOut->atSubpassDependencies[0] = (VkSubpassDependency){
        .srcSubpass      = VK_SUBPASS_EXTERNAL,
        .dstSubpass      = 0,
        .srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = 0
    };

    ptDataOut->atSubpassDependencies[1] = (VkSubpassDependency){
        .srcSubpass      = ptDesc->_uSubpassCount - 1,
        .dstSubpass      = VK_SUBPASS_EXTERNAL,
        .srcStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .srcAccessMask   = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dstAccessMask   = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
    };
}

static plDeviceMemoryAllocation
pl__allocate_staging_dynamic(struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName)
{
    plInternalDeviceAllocatorData* ptData = (plInternalDeviceAllocatorData *)ptInst;

    plDeviceMemoryAllocation tAllocation = {
        .pHostMapped = NULL,
        .uHandle     = 0,
        .ulOffset    = 0,
        .ulSize      = ulSize,
        .ptAllocator = ptData->ptAllocator,
        .tMemoryMode = PL_MEMORY_GPU_CPU
    };

    plDeviceMemoryAllocation tBlock = pl_allocate_memory(ptData->ptDevice, ulSize, PL_MEMORY_GPU_CPU, uTypeFilter, pcName);
    tAllocation.uHandle = tBlock.uHandle;
    tAllocation.pHostMapped = tBlock.pHostMapped;
    gptGraphics->szHostMemoryInUse += ulSize;
    return tAllocation;
}

static void
pl__free_staging_dynamic(struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation)
{
    plInternalDeviceAllocatorData* ptData = (plInternalDeviceAllocatorData *)ptInst;
    plDeviceMemoryAllocation tBlock = {
        .uHandle = ptAllocation->uHandle
    };
    pl_free_memory(ptData->ptDevice, &tBlock);
    gptGraphics->szHostMemoryInUse -= ptAllocation->ulSize;
    ptAllocation->uHandle = 0;
    ptAllocation->ulSize = 0;
    ptAllocation->ulOffset = 0;
}

static VkFormat
pl__find_supported_format(plDevice* ptDevice, VkFormatFeatureFlags tFlags, const VkFormat* ptFormats, uint32_t uFormatCount)
{
    for (uint32_t i = 0u; i < uFormatCount; i++)
    {
        VkFormatProperties tProps = {0};
        vkGetPhysicalDeviceFormatProperties(ptDevice->tPhysicalDevice, ptFormats[i], &tProps);
        if (tProps.optimalTilingFeatures & tFlags)
            return ptFormats[i];
    }

    PL_ASSERT(false && "no supported format found");
    return VK_FORMAT_UNDEFINED;
}

static bool
pl__format_has_stencil(VkFormat tFormat)
{
    switch (tFormat)
    {
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return true;
        case VK_FORMAT_D32_SFLOAT:
        default:
            return false;
    }
}

static void
pl__transition_image_layout(VkCommandBuffer tCommandBuffer, VkImage tImage, VkImageLayout tOldLayout, VkImageLayout tNewLayout, VkImageSubresourceRange tSubresourceRange, VkPipelineStageFlags tSrcStageMask, VkPipelineStageFlags tDstStageMask)
{
    // VkCommandBuffer commandBuffer = mvBeginSingleTimeCommands();
    VkImageMemoryBarrier tBarrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout           = tOldLayout,
        .newLayout           = tNewLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = tImage,
        .subresourceRange    = tSubresourceRange,
    };

    // Source layouts (old)
    // Source access mask controls actions that have to be finished on the old layout
    // before it will be transitioned to the new layout
    switch (tOldLayout)
    {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        // Image layout is undefined (or does not matter)
        // Only valid as initial layout
        // No flags required, listed only for completeness
        tBarrier.srcAccessMask = 0;
        break;

    case VK_IMAGE_LAYOUT_PREINITIALIZED:
        // Image is preinitialized
        // Only valid as initial layout for linear images, preserves memory contents
        // Make sure host writes have been finished
        tBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        // Image is a color attachment
        // Make sure any writes to the color buffer have been finished
        tBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        // Image is a depth/stencil attachment
        // Make sure any writes to the depth/stencil buffer have been finished
        tBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        // Image is a transfer source
        // Make sure any reads from the image have been finished
        tBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        // Image is a transfer destination
        // Make sure any writes to the image have been finished
        tBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        // Image is read by a shader
        // Make sure any shader reads from the image have been finished
        tBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        break;
    default:
        // Other source layouts aren't handled (yet)
        break;
    }

    // Target layouts (new)
    // Destination access mask controls the dependency for the new image layout
    switch (tNewLayout)
    {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        // Image will be used as a transfer destination
        // Make sure any writes to the image have been finished
        tBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        // Image will be used as a transfer source
        // Make sure any reads from the image have been finished
        tBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        // Image will be used as a color attachment
        // Make sure any writes to the color buffer have been finished
        tBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        // Image layout will be used as a depth/stencil attachment
        // Make sure any writes to depth/stencil buffer have been finished
        tBarrier.dstAccessMask = tBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        // Image will be read in a shader (sampler, input attachment)
        // Make sure any writes to the image have been finished
        if (tBarrier.srcAccessMask == 0)
            tBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        tBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        break;
    default:
        // Other source layouts aren't handled (yet)
        break;
    }
    vkCmdPipelineBarrier(tCommandBuffer, tSrcStageMask, tDstStageMask, 0, 0, NULL, 0, NULL, 1, &tBarrier);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
pl__debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT tMsgSeverity, VkDebugUtilsMessageTypeFlagsEXT tMsgType, const VkDebugUtilsMessengerCallbackDataEXT* ptCallbackData, void* pUserData)
{
    if (tMsgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        printf("error validation layer: %s\n", ptCallbackData->pMessage);
        pl_log_error_f(gptLog, uLogChannelGraphics, "error validation layer: %s\n", ptCallbackData->pMessage);
    }

    else if (tMsgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        pl_log_warn_f(gptLog, uLogChannelGraphics, "warn validation layer: %s\n", ptCallbackData->pMessage);
    }

    else if (tMsgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    {
        pl_log_info_f(gptLog, uLogChannelGraphics, "info validation layer: %s\n", ptCallbackData->pMessage);
    }
    else if (tMsgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
    {
        pl_log_trace_f(gptLog, uLogChannelGraphics, "trace validation layer: %s\n", ptCallbackData->pMessage);
    }

    return VK_FALSE;
}

static void
pl__create_swapchain(uint32_t uWidth, uint32_t uHeight, plSwapchain* ptSwap)
{
    plDevice* ptDevice = ptSwap->ptDevice;
    vkDeviceWaitIdle(ptDevice->tLogicalDevice);

    // query swapchain support

    VkSurfaceCapabilitiesKHR tCapabilities = {0};
    PL_VULKAN(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ptDevice->tPhysicalDevice, ptSwap->ptSurface->tSurface, &tCapabilities));

    uint32_t uFormatCount = 0u;
    PL_VULKAN(vkGetPhysicalDeviceSurfaceFormatsKHR(ptDevice->tPhysicalDevice, ptSwap->ptSurface->tSurface, &uFormatCount, NULL));

    pl_sb_resize(ptSwap->sbtSurfaceFormats, uFormatCount);

    VkBool32 tPresentSupport = false;
    PL_VULKAN(vkGetPhysicalDeviceSurfaceSupportKHR(ptDevice->tPhysicalDevice, 0, ptSwap->ptSurface->tSurface, &tPresentSupport));
    PL_ASSERT(uFormatCount > 0);
    PL_VULKAN(vkGetPhysicalDeviceSurfaceFormatsKHR(ptDevice->tPhysicalDevice, ptSwap->ptSurface->tSurface, &uFormatCount, ptSwap->sbtSurfaceFormats));

    uint32_t uPresentModeCount = 0u;
    PL_VULKAN(vkGetPhysicalDeviceSurfacePresentModesKHR(ptDevice->tPhysicalDevice, ptSwap->ptSurface->tSurface, &uPresentModeCount, NULL));
    PL_ASSERT(uPresentModeCount > 0 && uPresentModeCount < 16);

    VkPresentModeKHR atPresentModes[16] = {0};
    PL_VULKAN(vkGetPhysicalDeviceSurfacePresentModesKHR(ptDevice->tPhysicalDevice, ptSwap->ptSurface->tSurface, &uPresentModeCount, atPresentModes));

    // choose swap tSurface Format
    static VkFormat atSurfaceFormatPreference[4] = {
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_B8G8R8A8_SRGB
    };

    bool bPreferenceFound = false;
    VkSurfaceFormatKHR tSurfaceFormat = ptSwap->sbtSurfaceFormats[0];
    ptSwap->tInfo.tFormat = pl__pilotlight_format(tSurfaceFormat.format);

    for (uint32_t i = 0; i < 4; i++)
    {
        if (bPreferenceFound)
            break;

        for (uint32_t j = 0u; j < uFormatCount; j++)
        {
            if (ptSwap->sbtSurfaceFormats[j].format == atSurfaceFormatPreference[i] && ptSwap->sbtSurfaceFormats[j].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                tSurfaceFormat = ptSwap->sbtSurfaceFormats[j];
                ptSwap->tInfo.tFormat = pl__pilotlight_format(tSurfaceFormat.format);
                bPreferenceFound = true;
                break;
            }
        }
    }
    PL_ASSERT(bPreferenceFound && "no preferred surface format found");

    // chose swap present mode
    VkPresentModeKHR tPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    if (!ptSwap->tInfo.bVSync)
    {
        for (uint32_t i = 0; i < uPresentModeCount; i++)
        {
            if (atPresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                tPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                break;
            }
            if (atPresentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
                tPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
    }

    // chose swap extent
    VkExtent2D tExtent = {0};
    if (tCapabilities.currentExtent.width != UINT32_MAX)
        tExtent = tCapabilities.currentExtent;
    else
    {
        tExtent.width = pl_max(tCapabilities.minImageExtent.width, pl_min(tCapabilities.maxImageExtent.width, uWidth));
        tExtent.height = pl_max(tCapabilities.minImageExtent.height, pl_min(tCapabilities.maxImageExtent.height, uHeight));
    }
    ptSwap->tInfo.uWidth = tExtent.width;
    ptSwap->tInfo.uHeight = tExtent.height;

    // decide image count
    const uint32_t uOldImageCount = ptSwap->uImageCount;
    uint32_t uDesiredMinImageCount = tCapabilities.minImageCount + 1;
    if (tCapabilities.maxImageCount > 0 && uDesiredMinImageCount > tCapabilities.maxImageCount)
        uDesiredMinImageCount = tCapabilities.maxImageCount;

    // find the transformation of the surface
    VkSurfaceTransformFlagsKHR tPreTransform;
    if (tCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
    {
        // We prefer a non-rotated transform
        tPreTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }
    else
    {
        tPreTransform = tCapabilities.currentTransform;
    }

    // find a supported composite alpha format (not all devices support alpha opaque)
    VkCompositeAlphaFlagBitsKHR tCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    // simply select the first composite alpha format available
    static const VkCompositeAlphaFlagBitsKHR atCompositeAlphaFlags[] = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
    };

    for (int i = 0; i < 4; i++)
    {
        if (tCapabilities.supportedCompositeAlpha & atCompositeAlphaFlags[i])
        {
            tCompositeAlpha = atCompositeAlphaFlags[i];
            break;
        };
    }

    VkSwapchainCreateInfoKHR tCreateSwapchainInfo = {
        .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface          = ptSwap->ptSurface->tSurface,
        .minImageCount    = uDesiredMinImageCount,
        .imageFormat      = tSurfaceFormat.format,
        .imageColorSpace  = tSurfaceFormat.colorSpace,
        .imageExtent      = tExtent,
        .imageArrayLayers = 1,
        .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform     = (VkSurfaceTransformFlagBitsKHR)tPreTransform,
        .compositeAlpha   = tCompositeAlpha,
        .presentMode      = tPresentMode,
        .clipped          = VK_TRUE,                 // setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
        .oldSwapchain     = ptSwap->tSwapChain, // setting oldSwapChain to the saved handle of the previous swapchain aids in resource reuse and makes sure that we can still present already acquired images
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    // enable transfer source on swap chain images if supported
    if (tCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
        tCreateSwapchainInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    // enable transfer destination on swap chain images if supported
    if (tCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        tCreateSwapchainInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    uint32_t auQueueFamilyIndices[] = {(uint32_t)ptDevice->iGraphicsQueueFamily, (uint32_t)ptDevice->iPresentQueueFamily};
    if (ptDevice->iGraphicsQueueFamily != ptDevice->iPresentQueueFamily)
    {
        tCreateSwapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        tCreateSwapchainInfo.queueFamilyIndexCount = 2;
        tCreateSwapchainInfo.pQueueFamilyIndices = auQueueFamilyIndices;
    }

    VkSwapchainKHR tOldSwapChain = ptSwap->tSwapChain;

    PL_VULKAN(vkCreateSwapchainKHR(ptDevice->tLogicalDevice, &tCreateSwapchainInfo, NULL, &ptSwap->tSwapChain));

    if (tOldSwapChain)
    {
        plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptDevice);

        for (uint32_t i = 0; i < uOldImageCount; i++)
        {
            // pl_queue_texture_for_deletion(ptDevice, ptSwap->sbtSwapchainTextureViews[i]);
            pl_sb_push(ptGarbage->sbtTextures, ptSwap->sbtSwapchainTextureViews[i]);
        }
        vkDestroySwapchainKHR(ptDevice->tLogicalDevice, tOldSwapChain, NULL);
    }

    // get swapchain images

    PL_VULKAN(vkGetSwapchainImagesKHR(ptDevice->tLogicalDevice, ptSwap->tSwapChain, &ptSwap->uImageCount, NULL));
    pl_sb_resize(ptSwap->sbtSwapchainTextureViews, ptSwap->uImageCount);

    PL_VULKAN(vkGetSwapchainImagesKHR(ptDevice->tLogicalDevice, ptSwap->tSwapChain, &ptSwap->uImageCount, ptSwap->atImages));

    for (uint32_t i = 0; i < ptSwap->uImageCount; i++)
    {
        plTextureViewDesc tTextureViewDesc = {
            .tFormat     = ptSwap->tInfo.tFormat,
            .uBaseLayer  = 0,
            .uBaseMip    = 0,
            .uLayerCount = 1,
            .uMips       = 1,
            .pcDebugName = "swapchain dummy image"
        };

        plTextureHandle tHandle = pl__get_new_texture_handle(ptDevice);
        plTexture* ptTexture = pl__get_texture(ptDevice, tHandle);
        ptTexture->tView = tTextureViewDesc;

        ptTexture->tDesc.tDimensions = (plVec3){gptIO->tMainViewportSize.x, gptIO->tMainViewportSize.y, 1.0f};
        ptTexture->tDesc.uLayers = 1;
        ptTexture->tDesc.uMips = 1;
        ptTexture->tDesc.tSampleCount = ptSwap->tInfo.tSampleCount;
        ptTexture->tDesc.tFormat = tTextureViewDesc.tFormat;
        ptTexture->tDesc.tType = PL_TEXTURE_TYPE_2D;
        ptTexture->tDesc.tUsage = PL_TEXTURE_USAGE_PRESENT;
        ptTexture->tDesc.pcDebugName = "swapchain dummy image";

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~create view~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        VkImageViewCreateInfo tViewInfo = {
            .sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image                           = ptSwap->atImages[i],
            .viewType                        = VK_IMAGE_VIEW_TYPE_2D,
            .format                          = pl__vulkan_format(tTextureViewDesc.tFormat),
            .subresourceRange.baseMipLevel   = tTextureViewDesc.uBaseMip,
            .subresourceRange.levelCount     = ptTexture->tView.uMips,
            .subresourceRange.baseArrayLayer = tTextureViewDesc.uBaseLayer,
            .subresourceRange.layerCount     = tTextureViewDesc.uLayerCount,
            .subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        };
        VkImageView tImageView = VK_NULL_HANDLE;
        PL_VULKAN(vkCreateImageView(ptDevice->tLogicalDevice, &tViewInfo, NULL, &tImageView));

        ptDevice->sbtTexturesHot[tHandle.uIndex].bOriginalView = true;
        ptDevice->sbtTexturesHot[tHandle.uIndex].tImageView = tImageView;
        ptSwap->sbtSwapchainTextureViews[i] = tHandle;
    }
}

static void
pl__set_vulkan_object_name(plDevice* ptDevice, uint64_t uObjectHandle, VkDebugReportObjectTypeEXT tObjectType, const char* pcName)
{
    const VkDebugMarkerObjectNameInfoEXT tNameInfo ={
        .sType       = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT,
        .objectType  = tObjectType,
        .object      = uObjectHandle,
        .pObjectName = pcName
    };

    if (ptDevice->vkDebugMarkerSetObjectName)
        ptDevice->vkDebugMarkerSetObjectName(ptDevice->tLogicalDevice, &tNameInfo);
}

static VkFilter
pl__vulkan_filter(plFilter tFilter)
{
    switch (tFilter)
    {
    case PL_FILTER_UNSPECIFIED:
    case PL_FILTER_NEAREST:
        return VK_FILTER_NEAREST;
    case PL_FILTER_LINEAR:
        return VK_FILTER_LINEAR;
    }

    PL_ASSERT(false && "Unsupported filter mode");
    return VK_FILTER_LINEAR;
}

static VkSamplerAddressMode
pl__vulkan_wrap(plAddressMode tWrap)
{
    switch (tWrap)
    {
        case PL_ADDRESS_MODE_UNSPECIFIED:
        case PL_ADDRESS_MODE_WRAP:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case PL_ADDRESS_MODE_CLAMP:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case PL_ADDRESS_MODE_MIRROR:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    }

    PL_ASSERT(false && "Unsupported wrap mode");
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

static VkCompareOp
pl__vulkan_compare(plCompareMode tCompare)
{
    switch (tCompare)
    {
        case PL_COMPARE_MODE_UNSPECIFIED:
        case PL_COMPARE_MODE_NEVER:
            return VK_COMPARE_OP_NEVER;
        case PL_COMPARE_MODE_LESS:
            return VK_COMPARE_OP_LESS;
        case PL_COMPARE_MODE_EQUAL:
            return VK_COMPARE_OP_EQUAL;
        case PL_COMPARE_MODE_LESS_OR_EQUAL:
            return VK_COMPARE_OP_LESS_OR_EQUAL;
        case PL_COMPARE_MODE_GREATER:
            return VK_COMPARE_OP_GREATER;
        case PL_COMPARE_MODE_NOT_EQUAL:
            return VK_COMPARE_OP_NOT_EQUAL;
        case PL_COMPARE_MODE_GREATER_OR_EQUAL:
            return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case PL_COMPARE_MODE_ALWAYS:
            return VK_COMPARE_OP_ALWAYS;
    }

    PL_ASSERT(false && "Unsupported compare mode");
    return VK_COMPARE_OP_NEVER;
}

static VkFormat
pl__vulkan_vertex_format(plVertexFormat tFormat)
{
    switch(tFormat)
    {
        case PL_VERTEX_FORMAT_HALF:    return VK_FORMAT_R16_SFLOAT;
        case PL_VERTEX_FORMAT_HALF2:   return VK_FORMAT_R16G16_SFLOAT;
        case PL_VERTEX_FORMAT_HALF3:   return VK_FORMAT_R16G16B16_SFLOAT;
        case PL_VERTEX_FORMAT_HALF4:   return VK_FORMAT_R16G16B16A16_SFLOAT;
        case PL_VERTEX_FORMAT_FLOAT:   return VK_FORMAT_R32_SFLOAT;
        case PL_VERTEX_FORMAT_FLOAT2:  return VK_FORMAT_R32G32_SFLOAT;
        case PL_VERTEX_FORMAT_FLOAT3:  return VK_FORMAT_R32G32B32_SFLOAT;
        case PL_VERTEX_FORMAT_FLOAT4:  return VK_FORMAT_R32G32B32A32_SFLOAT;
        case PL_VERTEX_FORMAT_UCHAR:   return VK_FORMAT_R8_UNORM;
        case PL_VERTEX_FORMAT_UCHAR2:  return VK_FORMAT_R8G8_UNORM;
        case PL_VERTEX_FORMAT_UCHAR3:  return VK_FORMAT_R8G8B8_UNORM;
        case PL_VERTEX_FORMAT_UCHAR4:  return VK_FORMAT_R8G8B8A8_UNORM;
        case PL_VERTEX_FORMAT_CHAR:    return VK_FORMAT_R8_SNORM;
        case PL_VERTEX_FORMAT_CHAR2:   return VK_FORMAT_R8G8_SNORM;
        case PL_VERTEX_FORMAT_CHAR3:   return VK_FORMAT_R8G8B8_SNORM;
        case PL_VERTEX_FORMAT_CHAR4:   return VK_FORMAT_R8G8B8A8_SNORM;
        case PL_VERTEX_FORMAT_USHORT:  return VK_FORMAT_R16_UINT;
        case PL_VERTEX_FORMAT_USHORT2: return VK_FORMAT_R16G16_UINT;
        case PL_VERTEX_FORMAT_USHORT3: return VK_FORMAT_R16G16B16_UINT;
        case PL_VERTEX_FORMAT_USHORT4: return VK_FORMAT_R16G16B16A16_UINT;
        case PL_VERTEX_FORMAT_SHORT:   return VK_FORMAT_R16_SINT;
        case PL_VERTEX_FORMAT_SHORT2:  return VK_FORMAT_R16G16_SINT;
        case PL_VERTEX_FORMAT_SHORT3:  return VK_FORMAT_R16G16B16_SINT;
        case PL_VERTEX_FORMAT_SHORT4:  return VK_FORMAT_R16G16B16A16_SINT;
        case PL_VERTEX_FORMAT_UINT:    return VK_FORMAT_R32_UINT;
        case PL_VERTEX_FORMAT_UINT2:   return VK_FORMAT_R32G32_UINT;
        case PL_VERTEX_FORMAT_UINT3:   return VK_FORMAT_R32G32B32_UINT;
        case PL_VERTEX_FORMAT_UINT4:   return VK_FORMAT_R32G32B32A32_UINT;
        case PL_VERTEX_FORMAT_INT:     return VK_FORMAT_R32_SINT;
        case PL_VERTEX_FORMAT_INT2:    return VK_FORMAT_R32G32_SINT;
        case PL_VERTEX_FORMAT_INT3:    return VK_FORMAT_R32G32B32_SINT;
        case PL_VERTEX_FORMAT_INT4:    return VK_FORMAT_R32G32B32A32_SINT;
    }

    PL_ASSERT(false && "Unsupported vertex format");
    return VK_FORMAT_UNDEFINED;
}

static VkFormat
pl__vulkan_format(plFormat tFormat)
{
    switch (tFormat)
    {
        case PL_FORMAT_R32G32B32A32_FLOAT:  return VK_FORMAT_R32G32B32A32_SFLOAT;
        case PL_FORMAT_R8G8B8A8_UNORM:      return VK_FORMAT_R8G8B8A8_UNORM;
        case PL_FORMAT_R32G32_FLOAT:        return VK_FORMAT_R32G32_SFLOAT;
        case PL_FORMAT_R8G8B8A8_SRGB:       return VK_FORMAT_R8G8B8A8_SRGB;
        case PL_FORMAT_B8G8R8A8_SRGB:       return VK_FORMAT_B8G8R8A8_SRGB;
        case PL_FORMAT_B8G8R8A8_UNORM:      return VK_FORMAT_B8G8R8A8_UNORM;
        case PL_FORMAT_D32_FLOAT:           return VK_FORMAT_D32_SFLOAT;
        case PL_FORMAT_R8_UNORM:            return VK_FORMAT_R8_UNORM;
        case PL_FORMAT_R32_UINT:            return VK_FORMAT_R32_UINT;
        case PL_FORMAT_R8G8_UNORM:          return VK_FORMAT_R8G8_UNORM;
        case PL_FORMAT_D32_FLOAT_S8_UINT:   return VK_FORMAT_D32_SFLOAT_S8_UINT;
        case PL_FORMAT_D24_UNORM_S8_UINT:   return VK_FORMAT_D24_UNORM_S8_UINT;
        case PL_FORMAT_D16_UNORM_S8_UINT:   return VK_FORMAT_D16_UNORM_S8_UINT;
        case PL_FORMAT_R8_SNORM:            return VK_FORMAT_R8_SNORM;
        case PL_FORMAT_R8_UINT:             return VK_FORMAT_R8_UINT; 
        case PL_FORMAT_R8_SINT:             return VK_FORMAT_R8_SINT;
        case PL_FORMAT_R8_SRGB:             return VK_FORMAT_R8_SRGB;
        case PL_FORMAT_R16_UNORM:           return VK_FORMAT_R16_UNORM; 
        case PL_FORMAT_R16_SNORM:           return VK_FORMAT_R16_SNORM;
        case PL_FORMAT_R16_UINT:            return VK_FORMAT_R16_UINT;
        case PL_FORMAT_R16_SINT:            return VK_FORMAT_R16_SINT;
        case PL_FORMAT_R16_FLOAT:           return VK_FORMAT_R16_SFLOAT;
        case PL_FORMAT_R8G8_SNORM:          return VK_FORMAT_R8G8_SNORM;
        case PL_FORMAT_R8G8_UINT:           return VK_FORMAT_R8G8_UINT;
        case PL_FORMAT_R8G8_SINT:           return VK_FORMAT_R8G8_SINT;
        case PL_FORMAT_R8G8_SRGB:           return VK_FORMAT_R8G8_SRGB;
        case PL_FORMAT_B5G6R5_UNORM:        return VK_FORMAT_B5G6R5_UNORM_PACK16;
        case PL_FORMAT_A1R5G5B5_UNORM:      return VK_FORMAT_A1R5G5B5_UNORM_PACK16;
        case PL_FORMAT_B5G5R5A1_UNORM:      return VK_FORMAT_B5G5R5A1_UNORM_PACK16;
        case PL_FORMAT_R32_SINT:            return VK_FORMAT_R32_SINT;
        case PL_FORMAT_R32_FLOAT:           return VK_FORMAT_R32_SFLOAT;
        case PL_FORMAT_R16G16_UNORM:        return VK_FORMAT_R16G16_UNORM;
        case PL_FORMAT_R16G16_SNORM:        return VK_FORMAT_R16G16_SNORM;
        case PL_FORMAT_R16G16_UINT:         return VK_FORMAT_R16G16_UINT;
        case PL_FORMAT_R16G16_SINT:         return VK_FORMAT_R16G16_SINT;
        case PL_FORMAT_R16G16_FLOAT:        return VK_FORMAT_R16G16_SFLOAT;
        case PL_FORMAT_R8G8B8A8_SNORM:      return VK_FORMAT_R8G8B8A8_SNORM;
        case PL_FORMAT_R8G8B8A8_UINT:       return VK_FORMAT_R8G8B8A8_UINT;
        case PL_FORMAT_R8G8B8A8_SINT:       return VK_FORMAT_R8G8B8A8_SINT;
        case PL_FORMAT_B10G10R10A2_UNORM:   return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
        case PL_FORMAT_R10G10B10A2_UNORM:   return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        case PL_FORMAT_R10G10B10A2_UINT:    return VK_FORMAT_A2B10G10R10_UINT_PACK32;
        case PL_FORMAT_R11G11B10_FLOAT:     return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
        case PL_FORMAT_R9G9B9E5_FLOAT:      return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
        case PL_FORMAT_R32G32_UINT:         return VK_FORMAT_R32G32_UINT;
        case PL_FORMAT_R32G32_SINT:         return VK_FORMAT_R32G32_SINT;
        case PL_FORMAT_R16G16B16A16_UNORM:  return VK_FORMAT_R16G16B16A16_UNORM;
        case PL_FORMAT_R16G16B16A16_SNORM:  return VK_FORMAT_R16G16B16A16_SNORM;
        case PL_FORMAT_R16G16B16A16_UINT:   return VK_FORMAT_R16G16B16A16_UINT;
        case PL_FORMAT_R16G16B16A16_SINT:   return VK_FORMAT_R16G16B16A16_SINT;
        case PL_FORMAT_R16G16B16A16_FLOAT:  return VK_FORMAT_R16G16B16A16_SFLOAT;
        case PL_FORMAT_R32G32B32A32_UINT:   return VK_FORMAT_R32G32B32A32_UINT;
        case PL_FORMAT_R32G32B32A32_SINT:   return VK_FORMAT_R32G32B32A32_SINT;
        case PL_FORMAT_BC1_RGBA_UNORM:      return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case PL_FORMAT_BC1_RGBA_SRGB:       return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
        case PL_FORMAT_BC2_UNORM:           return VK_FORMAT_BC2_UNORM_BLOCK;
        case PL_FORMAT_BC2_SRGB:            return VK_FORMAT_BC2_SRGB_BLOCK;
        case PL_FORMAT_BC3_UNORM:           return VK_FORMAT_BC3_UNORM_BLOCK;
        case PL_FORMAT_BC3_SRGB:            return VK_FORMAT_BC3_SRGB_BLOCK;
        case PL_FORMAT_BC4_UNORM:           return VK_FORMAT_BC4_UNORM_BLOCK;
        case PL_FORMAT_BC4_SNORM:           return VK_FORMAT_BC4_SNORM_BLOCK;
        case PL_FORMAT_BC5_UNORM:           return VK_FORMAT_BC5_UNORM_BLOCK;
        case PL_FORMAT_BC5_SNORM:           return VK_FORMAT_BC5_SNORM_BLOCK;
        case PL_FORMAT_BC6H_UFLOAT:         return VK_FORMAT_BC6H_UFLOAT_BLOCK;
        case PL_FORMAT_BC6H_FLOAT:          return VK_FORMAT_BC6H_SFLOAT_BLOCK;
        case PL_FORMAT_BC7_UNORM:           return VK_FORMAT_BC7_UNORM_BLOCK;
        case PL_FORMAT_BC7_SRGB:            return VK_FORMAT_BC7_SRGB_BLOCK;
        case PL_FORMAT_ETC2_R8G8B8_UNORM:   return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
        case PL_FORMAT_ETC2_R8G8B8_SRGB:    return VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK;
        case PL_FORMAT_ETC2_R8G8B8A1_UNORM: return VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK;
        case PL_FORMAT_ETC2_R8G8B8A1_SRGB:  return VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK;
        case PL_FORMAT_EAC_R11_UNORM:       return VK_FORMAT_EAC_R11_UNORM_BLOCK;
        case PL_FORMAT_EAC_R11_SNORM:       return VK_FORMAT_EAC_R11_SNORM_BLOCK;
        case PL_FORMAT_EAC_R11G11_UNORM:    return VK_FORMAT_EAC_R11G11_UNORM_BLOCK;
        case PL_FORMAT_EAC_R11G11_SNORM:    return VK_FORMAT_EAC_R11G11_SNORM_BLOCK;
        case PL_FORMAT_ASTC_4x4_UNORM:      return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
        case PL_FORMAT_ASTC_4x4_SRGB:       return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
        case PL_FORMAT_ASTC_5x4_UNORM:      return VK_FORMAT_ASTC_5x4_UNORM_BLOCK;
        case PL_FORMAT_ASTC_5x4_SRGB:       return VK_FORMAT_ASTC_5x4_SRGB_BLOCK;
        case PL_FORMAT_ASTC_5x5_UNORM:      return VK_FORMAT_ASTC_5x5_UNORM_BLOCK;
        case PL_FORMAT_ASTC_5x5_SRGB:       return VK_FORMAT_ASTC_5x5_SRGB_BLOCK;
        case PL_FORMAT_ASTC_6x5_UNORM:      return VK_FORMAT_ASTC_6x5_UNORM_BLOCK;
        case PL_FORMAT_ASTC_6x5_SRGB:       return VK_FORMAT_ASTC_6x5_SRGB_BLOCK;
        case PL_FORMAT_ASTC_6x6_UNORM:      return VK_FORMAT_ASTC_6x6_UNORM_BLOCK;
        case PL_FORMAT_ASTC_6x6_SRGB:       return VK_FORMAT_ASTC_6x6_SRGB_BLOCK;
        case PL_FORMAT_ASTC_8x5_UNORM:      return VK_FORMAT_ASTC_8x5_UNORM_BLOCK;
        case PL_FORMAT_ASTC_8x5_SRGB:       return VK_FORMAT_ASTC_8x5_SRGB_BLOCK;
        case PL_FORMAT_ASTC_8x6_UNORM:      return VK_FORMAT_ASTC_8x6_UNORM_BLOCK;
        case PL_FORMAT_ASTC_8x6_SRGB:       return VK_FORMAT_ASTC_8x6_SRGB_BLOCK;
        case PL_FORMAT_ASTC_8x8_UNORM:      return VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
        case PL_FORMAT_ASTC_8x8_SRGB:       return VK_FORMAT_ASTC_8x8_SRGB_BLOCK;
        case PL_FORMAT_ASTC_10x5_UNORM:     return VK_FORMAT_ASTC_10x5_UNORM_BLOCK; 
        case PL_FORMAT_ASTC_10x5_SRGB:      return VK_FORMAT_ASTC_10x5_SRGB_BLOCK;
        case PL_FORMAT_ASTC_10x6_UNORM:     return VK_FORMAT_ASTC_10x6_UNORM_BLOCK;
        case PL_FORMAT_ASTC_10x6_SRGB:      return VK_FORMAT_ASTC_10x6_SRGB_BLOCK;
        case PL_FORMAT_ASTC_10x8_UNORM:     return VK_FORMAT_ASTC_10x8_UNORM_BLOCK;
        case PL_FORMAT_ASTC_10x8_SRGB:      return VK_FORMAT_ASTC_10x8_SRGB_BLOCK;
        case PL_FORMAT_ASTC_10x10_UNORM:    return VK_FORMAT_ASTC_10x10_UNORM_BLOCK;
        case PL_FORMAT_ASTC_10x10_SRGB:     return VK_FORMAT_ASTC_10x10_SRGB_BLOCK;
        case PL_FORMAT_ASTC_12x10_UNORM:    return VK_FORMAT_ASTC_12x10_UNORM_BLOCK;
        case PL_FORMAT_ASTC_12x10_SRGB:     return VK_FORMAT_ASTC_12x10_SRGB_BLOCK;
        case PL_FORMAT_ASTC_12x12_UNORM:    return VK_FORMAT_ASTC_12x12_UNORM_BLOCK;
        case PL_FORMAT_ASTC_12x12_SRGB:     return VK_FORMAT_ASTC_12x12_SRGB_BLOCK;
        case PL_FORMAT_D16_UNORM:           return VK_FORMAT_D16_UNORM;
        case PL_FORMAT_S8_UINT:             return VK_FORMAT_S8_UINT;
    }

    PL_ASSERT(false && "Unsupported format");
    return VK_FORMAT_UNDEFINED;
}

static VkImageLayout
pl__vulkan_layout(plTextureUsage tUsage)
{
    switch (tUsage)
    {
        case PL_TEXTURE_USAGE_UNSPECIFIED:
            return VK_IMAGE_LAYOUT_UNDEFINED;
        case PL_TEXTURE_USAGE_COLOR_ATTACHMENT:
            return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT:
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        case PL_TEXTURE_USAGE_PRESENT:
            return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        case PL_TEXTURE_USAGE_INPUT_ATTACHMENT:
        case PL_TEXTURE_USAGE_SAMPLED:
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        case PL_TEXTURE_USAGE_STORAGE:
            return VK_IMAGE_LAYOUT_GENERAL;
    }

    PL_ASSERT(false && "Unsupported texture layout");
    return VK_IMAGE_LAYOUT_UNDEFINED;
}

static VkAttachmentLoadOp
pl__vulkan_load_op(plLoadOp tOp)
{
    switch (tOp)
    {
        case PL_LOAD_OP_LOAD:
            return VK_ATTACHMENT_LOAD_OP_LOAD;
        case PL_LOAD_OP_CLEAR:
            return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case PL_LOAD_OP_DONT_CARE:
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }

    PL_ASSERT(false && "Unsupported load op");
    return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

static VkAttachmentStoreOp
pl__vulkan_store_op(plStoreOp tOp)
{
    switch (tOp)
    {
        case PL_STORE_OP_STORE_MULTISAMPLE_RESOLVE:
        case PL_STORE_OP_STORE:
            return VK_ATTACHMENT_STORE_OP_STORE;
        case PL_STORE_OP_DONT_CARE:
            return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        case PL_STORE_OP_NONE:
            return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }

    PL_ASSERT(false && "Unsupported store op");
    return VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

static VkStencilOp
pl__vulkan_stencil_op(plStencilOp tStencilOp)
{
    switch (tStencilOp)
    {
        case PL_STENCIL_OP_KEEP:
            return VK_STENCIL_OP_KEEP;
        case PL_STENCIL_OP_ZERO:
            return VK_STENCIL_OP_ZERO;
        case PL_STENCIL_OP_REPLACE:
            return VK_STENCIL_OP_REPLACE;
        case PL_STENCIL_OP_INCREMENT_AND_CLAMP:
            return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case PL_STENCIL_OP_DECREMENT_AND_CLAMP:
            return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case PL_STENCIL_OP_INVERT:
            return VK_STENCIL_OP_INVERT;
        case PL_STENCIL_OP_INCREMENT_AND_WRAP:
            return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case PL_STENCIL_OP_DECREMENT_AND_WRAP:
            return VK_STENCIL_OP_DECREMENT_AND_WRAP;
    }
    PL_ASSERT(false && "Unsupported stencil op");
    return VK_STENCIL_OP_KEEP;
}

static VkBlendFactor
pl__vulkan_blend_factor(plBlendFactor tFactor)
{
    switch (tFactor)
    {
        case PL_BLEND_FACTOR_ZERO:
            return VK_BLEND_FACTOR_ZERO;
        case PL_BLEND_FACTOR_ONE:
            return VK_BLEND_FACTOR_ONE;
        case PL_BLEND_FACTOR_SRC_COLOR:
            return VK_BLEND_FACTOR_SRC_COLOR;
        case PL_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case PL_BLEND_FACTOR_DST_COLOR:
            return VK_BLEND_FACTOR_DST_COLOR;
        case PL_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
            return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case PL_BLEND_FACTOR_SRC_ALPHA:
            return VK_BLEND_FACTOR_SRC_ALPHA;
        case PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case PL_BLEND_FACTOR_DST_ALPHA:
            return VK_BLEND_FACTOR_DST_ALPHA;
        case PL_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
            return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case PL_BLEND_FACTOR_CONSTANT_COLOR:
            return VK_BLEND_FACTOR_CONSTANT_COLOR;
        case PL_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR:
            return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        case PL_BLEND_FACTOR_CONSTANT_ALPHA:
            return VK_BLEND_FACTOR_CONSTANT_ALPHA;
        case PL_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:
            return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
        case PL_BLEND_FACTOR_SRC_ALPHA_SATURATE:
            return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
        case PL_BLEND_FACTOR_SRC1_COLOR:
            return VK_BLEND_FACTOR_SRC1_COLOR;
        case PL_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
        case PL_BLEND_FACTOR_SRC1_ALPHA:
            return VK_BLEND_FACTOR_SRC1_ALPHA;
        case PL_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
    }
    PL_ASSERT(false && "Unsupported blend factor");
    return VK_BLEND_FACTOR_ZERO;
}

static VkBlendOp
pl__vulkan_blend_op(plBlendOp tOp)
{
    switch (tOp)
    {
        case PL_BLEND_OP_ADD:
            return VK_BLEND_OP_ADD;
        case PL_BLEND_OP_SUBTRACT:
            return VK_BLEND_OP_SUBTRACT;
        case PL_BLEND_OP_REVERSE_SUBTRACT:
            return VK_BLEND_OP_REVERSE_SUBTRACT;
        case PL_BLEND_OP_MIN:
            return VK_BLEND_OP_MIN;
        case PL_BLEND_OP_MAX:
            return VK_BLEND_OP_MAX;
    }
    PL_ASSERT(false && "Unsupported blend op");
    return VK_BLEND_OP_ADD;
}

static VkCullModeFlags
pl__vulkan_cull(plCullMode tFlag)
{
    switch (tFlag)
    {
        case PL_CULL_MODE_CULL_FRONT:
            return VK_CULL_MODE_FRONT_BIT;
        case PL_CULL_MODE_CULL_BACK:
            return VK_CULL_MODE_BACK_BIT;
        case PL_CULL_MODE_NONE:
            return VK_CULL_MODE_NONE;
    }

    PL_ASSERT(false && "Unsupported cull mode");
    return VK_CULL_MODE_NONE;
}

static VkShaderStageFlagBits
pl__vulkan_stage_flags(plStageFlags tFlags)
{
    VkShaderStageFlagBits tResult = 0;

    if (tFlags & PL_STAGE_VERTEX)
        tResult |= VK_SHADER_STAGE_VERTEX_BIT;
    if (tFlags & PL_STAGE_PIXEL)
        tResult |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (tFlags & PL_STAGE_COMPUTE)
        tResult |= VK_SHADER_STAGE_COMPUTE_BIT;
    if(tFlags & PL_STAGE_TRANSFER)
        tResult |= VK_PIPELINE_STAGE_TRANSFER_BIT;

    return tResult;
}

static VkAccessFlags
pl__vulkan_access_flags(plAccessFlags tFlags)
{
    VkAccessFlags tResult = 0;

    if (tFlags & PL_ACCESS_SHADER_READ)
        tResult |= VK_ACCESS_SHADER_READ_BIT;
    if (tFlags & PL_ACCESS_SHADER_WRITE)
        tResult |= VK_ACCESS_SHADER_WRITE_BIT;
    if (tFlags & PL_ACCESS_TRANSFER_WRITE)
        tResult |= VK_ACCESS_TRANSFER_WRITE_BIT;
    if(tFlags & PL_ACCESS_TRANSFER_READ)
        tResult |= VK_ACCESS_TRANSFER_READ_BIT;

    return tResult;
}

static plFormat
pl__pilotlight_format(VkFormat tFormat)
{
    switch (tFormat)
    {
            case VK_FORMAT_R32G32B32A32_SFLOAT:       return PL_FORMAT_R32G32B32A32_FLOAT;
            case VK_FORMAT_R8G8B8A8_UNORM:            return PL_FORMAT_R8G8B8A8_UNORM;
            case VK_FORMAT_R32G32_SFLOAT:             return PL_FORMAT_R32G32_FLOAT;
            case VK_FORMAT_R8G8B8A8_SRGB:             return PL_FORMAT_R8G8B8A8_SRGB;
            case VK_FORMAT_B8G8R8A8_SRGB:             return PL_FORMAT_B8G8R8A8_SRGB;
            case VK_FORMAT_B8G8R8A8_UNORM:            return PL_FORMAT_B8G8R8A8_UNORM;
            case VK_FORMAT_D32_SFLOAT:                return PL_FORMAT_D32_FLOAT;
            case VK_FORMAT_R8_UNORM:                  return PL_FORMAT_R8_UNORM;
            case VK_FORMAT_R32_UINT:                  return PL_FORMAT_R32_UINT;
            case VK_FORMAT_R8G8_UNORM:                return PL_FORMAT_R8G8_UNORM;
            case VK_FORMAT_D32_SFLOAT_S8_UINT:        return PL_FORMAT_D32_FLOAT_S8_UINT;
            case VK_FORMAT_D24_UNORM_S8_UINT:         return PL_FORMAT_D24_UNORM_S8_UINT;
            case VK_FORMAT_D16_UNORM_S8_UINT:         return PL_FORMAT_D16_UNORM_S8_UINT;
            case VK_FORMAT_R8_SNORM:                  return PL_FORMAT_R8_SNORM;
            case VK_FORMAT_R8_UINT:                   return PL_FORMAT_R8_UINT; 
            case VK_FORMAT_R8_SINT:                   return PL_FORMAT_R8_SINT;
            case VK_FORMAT_R8_SRGB:                   return PL_FORMAT_R8_SRGB;
            case VK_FORMAT_R16_UNORM:                 return PL_FORMAT_R16_UNORM; 
            case VK_FORMAT_R16_SNORM:                 return PL_FORMAT_R16_SNORM;
            case VK_FORMAT_R16_UINT:                  return PL_FORMAT_R16_UINT;
            case VK_FORMAT_R16_SINT:                  return PL_FORMAT_R16_SINT;
            case VK_FORMAT_R16_SFLOAT:                return PL_FORMAT_R16_FLOAT;
            case VK_FORMAT_R8G8_SNORM:                return PL_FORMAT_R8G8_SNORM;
            case VK_FORMAT_R8G8_UINT:                 return PL_FORMAT_R8G8_UINT;
            case VK_FORMAT_R8G8_SINT:                 return PL_FORMAT_R8G8_SINT;
            case VK_FORMAT_R8G8_SRGB:                 return PL_FORMAT_R8G8_SRGB;
            case VK_FORMAT_B5G6R5_UNORM_PACK16:       return PL_FORMAT_B5G6R5_UNORM;
            case VK_FORMAT_A1R5G5B5_UNORM_PACK16:     return PL_FORMAT_A1R5G5B5_UNORM;
            case VK_FORMAT_B5G5R5A1_UNORM_PACK16:     return PL_FORMAT_B5G5R5A1_UNORM;
            case VK_FORMAT_R32_SINT:                  return PL_FORMAT_R32_SINT;
            case VK_FORMAT_R32_SFLOAT:                return PL_FORMAT_R32_FLOAT;
            case VK_FORMAT_R16G16_UNORM:              return PL_FORMAT_R16G16_UNORM;
            case VK_FORMAT_R16G16_SNORM:              return PL_FORMAT_R16G16_SNORM;
            case VK_FORMAT_R16G16_UINT:               return PL_FORMAT_R16G16_UINT;
            case VK_FORMAT_R16G16_SINT:               return PL_FORMAT_R16G16_SINT;
            case VK_FORMAT_R16G16_SFLOAT:             return PL_FORMAT_R16G16_FLOAT;
            case VK_FORMAT_R8G8B8A8_SNORM:            return PL_FORMAT_R8G8B8A8_SNORM;
            case VK_FORMAT_R8G8B8A8_UINT:             return PL_FORMAT_R8G8B8A8_UINT;
            case VK_FORMAT_R8G8B8A8_SINT:             return PL_FORMAT_R8G8B8A8_SINT;
            case VK_FORMAT_A2R10G10B10_UNORM_PACK32:  return PL_FORMAT_B10G10R10A2_UNORM;
            case VK_FORMAT_A2B10G10R10_UNORM_PACK32:  return PL_FORMAT_R10G10B10A2_UNORM;
            case VK_FORMAT_A2B10G10R10_UINT_PACK32:   return PL_FORMAT_R10G10B10A2_UINT;
            case VK_FORMAT_B10G11R11_UFLOAT_PACK32:   return PL_FORMAT_R11G11B10_FLOAT;
            case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:    return PL_FORMAT_R9G9B9E5_FLOAT;
            case VK_FORMAT_R32G32_UINT:               return PL_FORMAT_R32G32_UINT;
            case VK_FORMAT_R32G32_SINT:               return PL_FORMAT_R32G32_SINT;
            case VK_FORMAT_R16G16B16A16_UNORM:        return PL_FORMAT_R16G16B16A16_UNORM;
            case VK_FORMAT_R16G16B16A16_SNORM:        return PL_FORMAT_R16G16B16A16_SNORM;
            case VK_FORMAT_R16G16B16A16_UINT:         return PL_FORMAT_R16G16B16A16_UINT;
            case VK_FORMAT_R16G16B16A16_SINT:         return PL_FORMAT_R16G16B16A16_SINT;
            case VK_FORMAT_R16G16B16A16_SFLOAT:       return PL_FORMAT_R16G16B16A16_FLOAT;
            case VK_FORMAT_R32G32B32A32_UINT:         return PL_FORMAT_R32G32B32A32_UINT;
            case VK_FORMAT_R32G32B32A32_SINT:         return PL_FORMAT_R32G32B32A32_SINT;
            case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:      return PL_FORMAT_BC1_RGBA_UNORM;
            case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:       return PL_FORMAT_BC1_RGBA_SRGB;
            case VK_FORMAT_BC2_UNORM_BLOCK:           return PL_FORMAT_BC2_UNORM;
            case VK_FORMAT_BC2_SRGB_BLOCK:            return PL_FORMAT_BC2_SRGB;
            case VK_FORMAT_BC3_UNORM_BLOCK:           return PL_FORMAT_BC3_UNORM;
            case VK_FORMAT_BC3_SRGB_BLOCK:            return PL_FORMAT_BC3_SRGB;
            case VK_FORMAT_BC4_UNORM_BLOCK:           return PL_FORMAT_BC4_UNORM;
            case VK_FORMAT_BC4_SNORM_BLOCK:           return PL_FORMAT_BC4_SNORM;
            case VK_FORMAT_BC5_UNORM_BLOCK:           return PL_FORMAT_BC5_UNORM;
            case VK_FORMAT_BC5_SNORM_BLOCK:           return PL_FORMAT_BC5_SNORM;
            case VK_FORMAT_BC6H_UFLOAT_BLOCK:         return PL_FORMAT_BC6H_UFLOAT;
            case VK_FORMAT_BC6H_SFLOAT_BLOCK:         return PL_FORMAT_BC6H_FLOAT;
            case VK_FORMAT_BC7_UNORM_BLOCK:           return PL_FORMAT_BC7_UNORM;
            case VK_FORMAT_BC7_SRGB_BLOCK:            return PL_FORMAT_BC7_SRGB;
            case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:   return PL_FORMAT_ETC2_R8G8B8_UNORM;
            case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:    return PL_FORMAT_ETC2_R8G8B8_SRGB;
            case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK: return PL_FORMAT_ETC2_R8G8B8A1_UNORM;
            case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:  return PL_FORMAT_ETC2_R8G8B8A1_SRGB;
            case VK_FORMAT_EAC_R11_UNORM_BLOCK:       return PL_FORMAT_EAC_R11_UNORM;
            case VK_FORMAT_EAC_R11_SNORM_BLOCK:       return PL_FORMAT_EAC_R11_SNORM;
            case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:    return PL_FORMAT_EAC_R11G11_UNORM;
            case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:    return PL_FORMAT_EAC_R11G11_SNORM;
            case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:      return PL_FORMAT_ASTC_4x4_UNORM;
            case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:       return PL_FORMAT_ASTC_4x4_SRGB;
            case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:      return PL_FORMAT_ASTC_5x4_UNORM;
            case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:       return PL_FORMAT_ASTC_5x4_SRGB;
            case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:      return PL_FORMAT_ASTC_5x5_UNORM;
            case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:       return PL_FORMAT_ASTC_5x5_SRGB;
            case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:      return PL_FORMAT_ASTC_6x5_UNORM;
            case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:       return PL_FORMAT_ASTC_6x5_SRGB;
            case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:      return PL_FORMAT_ASTC_6x6_UNORM;
            case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:       return PL_FORMAT_ASTC_6x6_SRGB;
            case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:      return PL_FORMAT_ASTC_8x5_UNORM;
            case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:       return PL_FORMAT_ASTC_8x5_SRGB;
            case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:      return PL_FORMAT_ASTC_8x6_UNORM;
            case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:       return PL_FORMAT_ASTC_8x6_SRGB;
            case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:      return PL_FORMAT_ASTC_8x8_UNORM;
            case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:       return PL_FORMAT_ASTC_8x8_SRGB;
            case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:     return PL_FORMAT_ASTC_10x5_UNORM; 
            case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:      return PL_FORMAT_ASTC_10x5_SRGB;
            case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:     return PL_FORMAT_ASTC_10x6_UNORM;
            case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:      return PL_FORMAT_ASTC_10x6_SRGB;
            case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:     return PL_FORMAT_ASTC_10x8_UNORM;
            case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:      return PL_FORMAT_ASTC_10x8_SRGB;
            case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:    return PL_FORMAT_ASTC_10x10_UNORM;
            case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:     return PL_FORMAT_ASTC_10x10_SRGB;
            case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:    return PL_FORMAT_ASTC_12x10_UNORM;
            case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:     return PL_FORMAT_ASTC_12x10_SRGB;
            case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:    return PL_FORMAT_ASTC_12x12_UNORM;
            case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:     return PL_FORMAT_ASTC_12x12_SRGB;
            case VK_FORMAT_D16_UNORM:                 return PL_FORMAT_D16_UNORM;
            case VK_FORMAT_S8_UINT:                   return PL_FORMAT_S8_UINT;
        default:
            break;
    }

    PL_ASSERT(false && "Unsupported format");
    return PL_FORMAT_UNKNOWN;
}

static void
pl__garbage_collect(plDevice* ptDevice)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);
    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptDevice);

    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptDevice);

    for (uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtTextures); i++)
    {
        const uint16_t iResourceIndex = ptGarbage->sbtTextures[i].uIndex;
        plVulkanTexture* ptVulkanResource = &ptDevice->sbtTexturesHot[iResourceIndex];
        vkDestroyImageView(ptDevice->tLogicalDevice, ptDevice->sbtTexturesHot[iResourceIndex].tImageView, NULL);
        ptDevice->sbtTexturesHot[iResourceIndex].tImageView = VK_NULL_HANDLE;
        if (ptDevice->sbtTexturesHot[iResourceIndex].bOriginalView)
        {
            vkDestroyImage(ptDevice->tLogicalDevice, ptVulkanResource->tImage, NULL);
            ptVulkanResource->tImage = VK_NULL_HANDLE;
        }
        ptDevice->sbtTexturesHot[iResourceIndex].bOriginalView = false;
        pl_sb_push(ptDevice->sbtTextureFreeIndices, iResourceIndex);
    }

    for (uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtSamplers); i++)
    {
        const uint16_t iResourceIndex = ptGarbage->sbtSamplers[i].uIndex;
        vkDestroySampler(ptDevice->tLogicalDevice, ptDevice->sbtSamplersHot[iResourceIndex], NULL);
        ptDevice->sbtSamplersHot[iResourceIndex] = VK_NULL_HANDLE;
        pl_sb_push(ptDevice->sbtSamplerFreeIndices, iResourceIndex);
    }

    for (uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtRenderPasses); i++)
    {
        const uint16_t iResourceIndex = ptGarbage->sbtRenderPasses[i].uIndex;
        plVulkanRenderPass* ptVulkanResource = &ptDevice->sbtRenderPassesHot[iResourceIndex];
        for (uint32_t j = 0; j < 3; j++)
        {
            if (ptVulkanResource->atFrameBuffers[j])
                vkDestroyFramebuffer(ptDevice->tLogicalDevice, ptVulkanResource->atFrameBuffers[j], NULL);
            ptVulkanResource->atFrameBuffers[j] = VK_NULL_HANDLE;
        }
        if (ptVulkanResource->tRenderPass)
            vkDestroyRenderPass(ptDevice->tLogicalDevice, ptVulkanResource->tRenderPass, NULL);
        ptVulkanResource->tRenderPass = VK_NULL_HANDLE;
        pl_sb_push(ptDevice->sbtRenderPassFreeIndices, iResourceIndex);
        // ptVulkanResource->sbtFrameBuffers = NULL;
    }

    for (uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtRenderPassLayouts); i++)
    {
        const uint16_t iResourceIndex = ptGarbage->sbtRenderPassLayouts[i].uIndex;
        plVulkanRenderPassLayout* ptVulkanResource = &ptDevice->sbtRenderPassLayoutsHot[iResourceIndex];
        vkDestroyRenderPass(ptDevice->tLogicalDevice, ptVulkanResource->tRenderPass, NULL);
        pl_sb_push(ptDevice->sbtRenderPassLayoutFreeIndices, iResourceIndex);
    }

    for (uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtShaders); i++)
    {
        const uint16_t iResourceIndex = ptGarbage->sbtShaders[i].uIndex;
        plShader* ptResource = &ptDevice->sbtShadersCold[iResourceIndex];
        plVulkanShader* ptVulkanResource = &ptDevice->sbtShadersHot[iResourceIndex];
        if (ptVulkanResource->tVertexShaderModule)
            vkDestroyShaderModule(ptDevice->tLogicalDevice, ptVulkanResource->tVertexShaderModule, NULL);
        if (ptVulkanResource->tPixelShaderModule)
            vkDestroyShaderModule(ptDevice->tLogicalDevice, ptVulkanResource->tPixelShaderModule, NULL);

        ptVulkanResource->tVertexShaderModule = VK_NULL_HANDLE;
        ptVulkanResource->tPixelShaderModule = VK_NULL_HANDLE;

        plVulkanShader* ptVariantVulkanResource = &ptDevice->sbtShadersHot[iResourceIndex];
        vkDestroyPipelineLayout(ptDevice->tLogicalDevice, ptVariantVulkanResource->tPipelineLayout, NULL);
        vkDestroyPipeline(ptDevice->tLogicalDevice, ptVariantVulkanResource->tPipeline, NULL);
        ptVariantVulkanResource->tPipelineLayout = VK_NULL_HANDLE;
        ptVariantVulkanResource->tPipeline = VK_NULL_HANDLE;
        pl_sb_push(ptDevice->sbtShaderFreeIndices, iResourceIndex);
        for (uint32_t k = 0; k < ptResource->tDesc._uBindGroupLayoutCount; k++)
        {
            plVulkanBindGroupLayout* ptVulkanBindGroupLayout = &ptDevice->sbtBindGroupLayouts[ptResource->tDesc.atBindGroupLayouts[k]._uHandle];
            vkDestroyDescriptorSetLayout(ptDevice->tLogicalDevice, ptVulkanBindGroupLayout->tDescriptorSetLayout, NULL);
            ptVulkanBindGroupLayout->tDescriptorSetLayout = VK_NULL_HANDLE;
            pl_sb_push(ptDevice->sbtBindGroupLayoutFreeIndices, ptResource->tDesc.atBindGroupLayouts[k]._uHandle);
        }
    }

    for (uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtComputeShaders); i++)
    {
        const uint16_t iResourceIndex = ptGarbage->sbtComputeShaders[i].uIndex;
        plComputeShader* ptResource = &ptDevice->sbtComputeShadersCold[iResourceIndex];
        plVulkanComputeShader* ptVulkanResource = &ptDevice->sbtComputeShadersHot[iResourceIndex];
        if (ptVulkanResource->tShaderModule)
            vkDestroyShaderModule(ptDevice->tLogicalDevice, ptVulkanResource->tShaderModule, NULL);

        ptVulkanResource->tShaderModule = VK_NULL_HANDLE;

        plVulkanComputeShader* ptVariantVulkanResource = &ptDevice->sbtComputeShadersHot[iResourceIndex];
        vkDestroyPipelineLayout(ptDevice->tLogicalDevice, ptVariantVulkanResource->tPipelineLayout, NULL);
        vkDestroyPipeline(ptDevice->tLogicalDevice, ptVariantVulkanResource->tPipeline, NULL);
        ptVariantVulkanResource->tPipelineLayout = VK_NULL_HANDLE;
        ptVariantVulkanResource->tPipeline = VK_NULL_HANDLE;
        pl_sb_push(ptDevice->sbtComputeShaderFreeIndices, iResourceIndex);

        for (uint32_t k = 0; k < ptResource->tDesc._uBindGroupLayoutCount; k++)
        {
            plVulkanBindGroupLayout* ptVulkanBindGroupLayout = &ptDevice->sbtBindGroupLayouts[ptResource->tDesc.atBindGroupLayouts[k]._uHandle];
            vkDestroyDescriptorSetLayout(ptDevice->tLogicalDevice, ptVulkanBindGroupLayout->tDescriptorSetLayout, NULL);
            ptVulkanBindGroupLayout->tDescriptorSetLayout = VK_NULL_HANDLE;
            pl_sb_push(ptDevice->sbtBindGroupLayoutFreeIndices, ptResource->tDesc.atBindGroupLayouts[k]._uHandle);
        }
    }

    for (uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtBindGroups); i++)
    {
        const uint16_t iBindGroupIndex = ptGarbage->sbtBindGroups[i].uIndex;
        plVulkanBindGroup* ptVulkanResource = &ptDevice->sbtBindGroupsHot[iBindGroupIndex];
        if (ptVulkanResource->bResetable)
            vkFreeDescriptorSets(ptDevice->tLogicalDevice, ptVulkanResource->tPool, 1, &ptVulkanResource->tDescriptorSet);
        ptVulkanResource->tPool = VK_NULL_HANDLE;
        ptVulkanResource->tDescriptorSet = VK_NULL_HANDLE;
        vkDestroyDescriptorSetLayout(ptDevice->tLogicalDevice, ptVulkanResource->tDescriptorSetLayout, NULL);
        ptVulkanResource->tDescriptorSetLayout = VK_NULL_HANDLE;
        pl_sb_push(ptDevice->sbtBindGroupFreeIndices, iBindGroupIndex);
    }

    for (uint32_t i = 0; i < pl_sb_size(ptCurrentFrame->sbtRawFrameBuffers); i++)
    {
        vkDestroyFramebuffer(ptDevice->tLogicalDevice, ptCurrentFrame->sbtRawFrameBuffers[i], NULL);
        ptCurrentFrame->sbtRawFrameBuffers[i] = VK_NULL_HANDLE;
    }

    for (uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtBuffers); i++)
    {
        const uint16_t iResourceIndex = ptGarbage->sbtBuffers[i].uIndex;
        vkDestroyBuffer(ptDevice->tLogicalDevice, ptDevice->sbtBuffersHot[iResourceIndex].tBuffer, NULL);
        ptDevice->sbtBuffersHot[iResourceIndex].tBuffer = VK_NULL_HANDLE;
        pl_sb_push(ptDevice->sbtBufferFreeIndices, iResourceIndex);
    }

    const uint32_t uMemoryBlocks = pl_sb_size(ptGarbage->sbtMemory);

    for (uint32_t i = 0; i < uMemoryBlocks; i++)
    {
        plDeviceMemoryAllocation* ptAllocation = &ptGarbage->sbtMemory[i];
        plDeviceMemoryAllocatorI* ptAllocator = ptAllocation->ptAllocator;
        if (ptAllocator) // swapchain doesn't have allocator since texture is provided
            ptAllocator->free(ptAllocator->ptInst, ptAllocation);
        else
            pl_free_memory(ptDevice, ptAllocation);
    }

    pl_sb_reset(ptGarbage->sbtTextures);
    pl_sb_reset(ptGarbage->sbtShaders);
    pl_sb_reset(ptGarbage->sbtComputeShaders);
    pl_sb_reset(ptGarbage->sbtRenderPasses);
    pl_sb_reset(ptGarbage->sbtRenderPassLayouts);
    pl_sb_reset(ptGarbage->sbtMemory);
    pl_sb_reset(ptCurrentFrame->sbtRawFrameBuffers);
    pl_sb_reset(ptGarbage->sbtBuffers);
    pl_sb_reset(ptGarbage->sbtBindGroups);
    pl_end_cpu_sample(gptProfile, 0);
}
