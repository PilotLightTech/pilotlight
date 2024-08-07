/*
   pl_vulkan_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] global data
// [SECTION] internal structs
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] drawing
// [SECTION] internal api implementation
// [SECTION] device memory allocators
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pilotlight.h"
#include "pl_os.h"
#include "pl_profile.h"
#include "pl_log.h"
#include "pl_string.h"
#include "pl_memory.h"
#include "pl_graphics_ext.c"

// vulkan stuff
#if defined(_WIN32)
    #define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__ANDROID__)
    #define VK_USE_PLATFORM_ANDROID_KHR
#elif defined(__APPLE__)
    #define VK_USE_PLATFORM_METAL_EXT
#else // linux
    #define VK_USE_PLATFORM_XCB_KHR
#endif

#ifndef PL_DEVICE_LOCAL_LEVELS
    #define PL_DEVICE_LOCAL_LEVELS 8
#endif

#include "vulkan/vulkan.h"

#ifdef _WIN32
#pragma comment(lib, "vulkan-1.lib")
#endif

#ifndef PL_VULKAN
    #include <assert.h>
    #define PL_VULKAN(x) assert(x == VK_SUCCESS)
#endif

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

const plFileI*  gptFile = NULL;
const plIOI*    gptIO = NULL;
static uint32_t uLogChannel = UINT32_MAX;

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

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
} plRenderPassCommonData;

typedef struct _plVulkanDynamicBuffer
{
    uint32_t                 uByteOffset;
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
    bool        bOriginalView;
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

typedef struct _plFrameContext
{
    VkSemaphore           tImageAvailable;
    VkSemaphore           tRenderFinish;
    VkFence               tInFlight;
    VkCommandPool         tCmdPool;
    VkFramebuffer*        sbtRawFrameBuffers;
    VkDescriptorPool      tDynamicDescriptorPool;

    // dynamic buffer stuff
    uint32_t               uCurrentBufferIndex;
    plVulkanDynamicBuffer* sbtDynamicBuffers;

    VkCommandBuffer* sbtReadyCommandBuffers;
    VkCommandBuffer* sbtPendingCommandBuffers;
} plFrameContext;

typedef struct _plVulkanSwapchain
{
    VkSwapchainKHR           tSwapChain;
    VkImage*                 sbtImages;
    VkSurfaceFormatKHR*      sbtSurfaceFormats;
} plVulkanSwapchain;

typedef struct _plVulkanDevice
{
    VkDevice                                  tLogicalDevice;
    VkPhysicalDevice                          tPhysicalDevice;
    int                                       iGraphicsQueueFamily;
    int                                       iPresentQueueFamily;
    VkQueue                                   tGraphicsQueue;
    VkQueue                                   tPresentQueue;
    VkPhysicalDeviceProperties                tDeviceProps;
    VkPhysicalDeviceMemoryProperties          tMemProps;
    VkPhysicalDeviceMemoryProperties2         tMemProps2;
    VkPhysicalDeviceMemoryBudgetPropertiesEXT tMemBudgetInfo;
    VkDeviceSize                              tMaxLocalMemSize;
    VkPhysicalDeviceFeatures                  tDeviceFeatures;
    VkPhysicalDeviceFeatures2                 tDeviceFeatures2;
    VkPhysicalDeviceVulkan12Features          tDeviceFeatures12;
    bool                                      bSwapchainExtPresent;
    bool                                      bPortabilitySubsetPresent;
    VkCommandPool                             tCmdPool;
    uint32_t                                  uUniformBufferBlockSize;
    uint32_t                                  uCurrentFrame;

	PFN_vkDebugMarkerSetObjectTagEXT  vkDebugMarkerSetObjectTag;
	PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectName;
	PFN_vkCmdDebugMarkerBeginEXT      vkCmdDebugMarkerBegin;
	PFN_vkCmdDebugMarkerEndEXT        vkCmdDebugMarkerEnd;
	PFN_vkCmdDebugMarkerInsertEXT     vkCmdDebugMarkerInsert;

} plVulkanDevice;

typedef struct _plVulkanGraphics
{
    plTempAllocator          tTempAllocator;
    VkInstance               tInstance;
    VkDebugUtilsMessengerEXT tDbgMessenger;
    VkSurfaceKHR             tSurface;
    plFrameContext*          sbFrames;
    VkDescriptorPool         tDescriptorPool;

    VkSemaphore*              sbtSemaphoresHot;
    plVulkanTexture*          sbtTexturesHot;
    VkImageView*              sbtTextureViewsHot;
    VkSampler*                sbtSamplersHot;
    plVulkanBindGroup*        sbtBindGroupsHot;
    plVulkanBuffer*           sbtBuffersHot;
    plVulkanShader*           sbtShadersHot;
    plVulkanComputeShader*    sbtComputeShadersHot;
    plVulkanRenderPass*       sbtRenderPassesHot;
    plVulkanRenderPassLayout* sbtRenderPassLayoutsHot;
    plVulkanBindGroupLayout*  sbtBindGroupLayouts;
    uint32_t*                 sbtBindGroupLayoutFreeIndices;
    VkDescriptorSetLayout     tDynamicDescriptorSetLayout;
    bool                      bWithinFrameContext;
} plVulkanGraphics;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

// conversion between pilotlight & vulkan types
static VkFilter                            pl__vulkan_filter    (plFilter tFilter);
static VkSamplerAddressMode                pl__vulkan_wrap      (plWrapMode tWrap);
static VkCompareOp                         pl__vulkan_compare   (plCompareMode tCompare);
static VkFormat                            pl__vulkan_format    (plFormat tFormat);
static bool                                pl__is_depth_format  (plFormat tFormat);
static VkImageLayout                       pl__vulkan_layout    (plTextureUsage tUsage);
static VkAttachmentLoadOp                  pl__vulkan_load_op   (plLoadOp tOp);
static VkAttachmentStoreOp                 pl__vulkan_store_op  (plStoreOp tOp);
static VkCullModeFlags                     pl__vulkan_cull      (plCullMode tFlag);
static VkShaderStageFlagBits               pl__vulkan_stage_flags(plStageFlags tFlags);
static plFormat                            pl__pilotlight_format(VkFormat tFormat);
static VkStencilOp                         pl__vulkan_stencil_op(plStencilOp tStencilOp);
static VkBlendFactor                       pl__vulkan_blend_factor(plBlendFactor tFactor);
static VkBlendOp                           pl__vulkan_blend_op(plBlendOp tOp);

static plDeviceMemoryAllocation pl_allocate_memory(plDevice*, size_t ulSize, plMemoryMode tMemoryMode, uint32_t uTypeFilter, const char* pcName);
static void pl_free_memory(plDevice*, plDeviceMemoryAllocation*);

static void                  pl_set_vulkan_object_name(plDevice* ptDevice, uint64_t uObjectHandle, VkDebugReportObjectTypeEXT tObjectType, const char* pcName);
static plFrameContext*       pl__get_frame_resources(plGraphics* ptGraphics);
static plFrameContext*       pl__get_next_frame_resources(plGraphics* ptGraphics);
static VkSampleCountFlagBits pl__get_max_sample_count(plDevice* ptDevice);
static VkFormat              pl__find_supported_format(plDevice* ptDevice, VkFormatFeatureFlags tFlags, const VkFormat* ptFormats, uint32_t uFormatCount);
static VkFormat              pl__find_depth_format(plDevice* ptDevice);
static VkFormat              pl__find_depth_stencil_format(plDevice* ptDevice);
static bool                  pl__format_has_stencil(VkFormat tFormat);
static void                  pl__transition_image_layout(VkCommandBuffer tCommandBuffer, VkImage tImage, VkImageLayout tOldLayout, VkImageLayout tNewLayout, VkImageSubresourceRange tSubresourceRange, VkPipelineStageFlags tSrcStageMask, VkPipelineStageFlags tDstStageMask);
static void                  pl__create_swapchain(plGraphics* ptGraphics, uint32_t uWidth, uint32_t uHeight);
static uint32_t              pl__find_memory_type_(VkPhysicalDeviceMemoryProperties tMemProps, uint32_t uTypeFilter, VkMemoryPropertyFlags tProperties);
static void                  pl__garbage_collect(plGraphics* ptGraphics);
static void                  pl__fill_common_render_pass_data(const plRenderPassLayoutDescription* ptDesc, plRenderPassLayout* ptLayout, plRenderPassCommonData* ptDataOut);

static VKAPI_ATTR VkBool32 VKAPI_CALL pl__debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT tMsgSeverity, VkDebugUtilsMessageTypeFlagsEXT tMsgType, const VkDebugUtilsMessengerCallbackDataEXT* ptCallbackData, void* pUserData);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

static plSemaphoreHandle
pl_create_semaphore(plDevice* ptDevice, bool bHostVisible)
{
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptDevice->ptGraphics->_pInternalData;
    plGraphics* ptGraphics = ptDevice->ptGraphics;

    uint32_t uIndex = UINT32_MAX;
    if(pl_sb_size(ptGraphics->sbtSemaphoreFreeIndices) > 0)
        uIndex = pl_sb_pop(ptGraphics->sbtSemaphoreFreeIndices);
    else
    {
        uIndex = pl_sb_size(ptVulkanGraphics->sbtSemaphoresHot);
        pl_sb_push(ptGraphics->sbtSemaphoreGenerations, UINT32_MAX);
        pl_sb_add(ptVulkanGraphics->sbtSemaphoresHot);
    }

    plSemaphoreHandle tHandle = {
        .uGeneration = ++ptGraphics->sbtSemaphoreGenerations[uIndex],
        .uIndex = uIndex
    };

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

    VkSemaphore tTimelineSemaphore = VK_NULL_HANDLE;
    PL_VULKAN(vkCreateSemaphore(ptVulkanDevice->tLogicalDevice, &tCreateInfo, NULL, &tTimelineSemaphore));
    
    ptVulkanGraphics->sbtSemaphoresHot[uIndex] = tTimelineSemaphore;
    return tHandle;
}

static void
pl_signal_semaphore(plGraphics* ptGraphics, plSemaphoreHandle tHandle, uint64_t ulValue)
{
    plVulkanDevice* ptVulkanDevice = ptGraphics->tDevice._pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptGraphics->_pInternalData;
    const VkSemaphoreSignalInfo tSignalInfo = {
        .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
        .pNext     = NULL,
        .semaphore = ptVulkanGraphics->sbtSemaphoresHot[tHandle.uIndex],
        .value     = ulValue,
    };
    vkSignalSemaphore(ptVulkanDevice->tLogicalDevice, &tSignalInfo);
}

static void
pl_wait_semaphore(plGraphics* ptGraphics, plSemaphoreHandle tHandle, uint64_t ulValue)
{
    plVulkanDevice* ptVulkanDevice = ptGraphics->tDevice._pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptGraphics->_pInternalData;
    const VkSemaphoreWaitInfo tWaitInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .pNext = NULL,
        .flags = 0,
        .semaphoreCount = 1,
        .pSemaphores = &ptVulkanGraphics->sbtSemaphoresHot[tHandle.uIndex],
        .pValues = &ulValue,
    };
    vkWaitSemaphores(ptVulkanDevice->tLogicalDevice, &tWaitInfo, UINT64_MAX);
}

static uint64_t
pl_get_semaphore_value(plGraphics* ptGraphics, plSemaphoreHandle tHandle)
{
    plVulkanDevice* ptVulkanDevice = ptGraphics->tDevice._pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptGraphics->_pInternalData;
    uint64_t ulValue = 0;
    vkGetSemaphoreCounterValue(ptVulkanDevice->tLogicalDevice, ptVulkanGraphics->sbtSemaphoresHot[tHandle.uIndex], &ulValue);
    return ulValue;
}

static plBufferHandle
pl_create_buffer(plDevice* ptDevice, const plBufferDescription* ptDesc, const char* pcName)
{
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptDevice->ptGraphics->_pInternalData;
    plGraphics* ptGraphics = ptDevice->ptGraphics;

    uint32_t uBufferIndex = UINT32_MAX;
    if(pl_sb_size(ptGraphics->sbtBufferFreeIndices) > 0)
        uBufferIndex = pl_sb_pop(ptGraphics->sbtBufferFreeIndices);
    else
    {
        uBufferIndex = pl_sb_size(ptGraphics->sbtBuffersCold);
        pl_sb_add(ptGraphics->sbtBuffersCold);
        pl_sb_push(ptGraphics->sbtBufferGenerations, UINT32_MAX);
        pl_sb_add(ptVulkanGraphics->sbtBuffersHot);
    }

    plBufferHandle tHandle = {
        .uGeneration = ++ptGraphics->sbtBufferGenerations[uBufferIndex],
        .uIndex = uBufferIndex
    };

    plBuffer tBuffer = {
        .tDescription = *ptDesc
    };

    if(pcName)
    {
        pl_sprintf(tBuffer.tDescription.acDebugName, "%s", pcName);
    }

    plVulkanBuffer tVulkanBuffer = {0};

    VkBufferCreateInfo tBufferInfo = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = ptDesc->uByteSize,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VkBufferUsageFlagBits tBufferUsageFlags = 0;
    if(ptDesc->tUsage & PL_BUFFER_USAGE_VERTEX)
        tBufferInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if(ptDesc->tUsage & PL_BUFFER_USAGE_INDEX)
        tBufferInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if(ptDesc->tUsage & PL_BUFFER_USAGE_STORAGE)
        tBufferInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if(ptDesc->tUsage & PL_BUFFER_USAGE_UNIFORM)
        tBufferInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if(ptDesc->tUsage & PL_BUFFER_USAGE_STAGING)
        tBufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VkMemoryRequirements tMemRequirements = {0};

    PL_VULKAN(vkCreateBuffer(ptVulkanDevice->tLogicalDevice, &tBufferInfo, NULL, &tVulkanBuffer.tBuffer));
    if(pcName)
        pl_set_vulkan_object_name(ptDevice, (uint64_t)tVulkanBuffer.tBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, pcName);
    vkGetBufferMemoryRequirements(ptVulkanDevice->tLogicalDevice, tVulkanBuffer.tBuffer, &tMemRequirements);

    tBuffer.tMemoryRequirements.ulAlignment = tMemRequirements.alignment;
    tBuffer.tMemoryRequirements.ulSize = tMemRequirements.size;
    tBuffer.tMemoryRequirements.uMemoryTypeBits = tMemRequirements.memoryTypeBits;

    ptVulkanGraphics->sbtBuffersHot[uBufferIndex] = tVulkanBuffer;
    ptGraphics->sbtBuffersCold[uBufferIndex] = tBuffer;
    return tHandle;
}

static void
pl_bind_buffer_to_memory(plDevice* ptDevice, plBufferHandle tHandle, const plDeviceMemoryAllocation* ptAllocation)
{
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptDevice->ptGraphics->_pInternalData;
    plGraphics* ptGraphics = ptDevice->ptGraphics;

    plBuffer* ptBuffer = &ptGraphics->sbtBuffersCold[tHandle.uIndex];
    ptBuffer->tMemoryAllocation = *ptAllocation;
    plVulkanBuffer* ptVulkanBuffer = &ptVulkanGraphics->sbtBuffersHot[tHandle.uIndex];

    PL_VULKAN(vkBindBufferMemory(ptVulkanDevice->tLogicalDevice, ptVulkanBuffer->tBuffer, (VkDeviceMemory)ptAllocation->uHandle, ptAllocation->ulOffset));
    ptVulkanBuffer->pcData = ptAllocation->pHostMapped;
}

static plDynamicBinding
pl_allocate_dynamic_data(plDevice* ptDevice, size_t szSize)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptGraphics->_pInternalData;
    plFrameContext* ptFrame = pl__get_frame_resources(ptGraphics);

    PL_ASSERT(szSize <= PL_MAX_DYNAMIC_DATA_SIZE && "Dynamic data size too large");

    plVulkanDynamicBuffer* ptDynamicBuffer = NULL;

    // first call this frame
    if(ptFrame->uCurrentBufferIndex == UINT32_MAX)
    {
        ptFrame->uCurrentBufferIndex = 0;
        ptFrame->sbtDynamicBuffers[0].uByteOffset = 0;
    }
    ptDynamicBuffer = &ptFrame->sbtDynamicBuffers[ptFrame->uCurrentBufferIndex];
    
    // check if current block has room
    if(ptDynamicBuffer->uByteOffset + szSize > PL_DEVICE_ALLOCATION_BLOCK_SIZE)
    {
        ptFrame->uCurrentBufferIndex++;
        
        // check if we have available block
        if(ptFrame->uCurrentBufferIndex + 1 > pl_sb_size(ptFrame->sbtDynamicBuffers)) // create new buffer
        {
            // dynamic buffer stuff
            pl_sb_add(ptFrame->sbtDynamicBuffers);
            ptDynamicBuffer = &ptFrame->sbtDynamicBuffers[ptFrame->uCurrentBufferIndex];

            plBufferDescription tStagingBufferDescription0 = {
                .tUsage               = PL_BUFFER_USAGE_UNIFORM,
                .uByteSize            = PL_DEVICE_ALLOCATION_BLOCK_SIZE
            };
            pl_sprintf(tStagingBufferDescription0.acDebugName, "D-BUF-F%d-%d", (int)ptGraphics->uCurrentFrameIndex, (int)ptFrame->uCurrentBufferIndex);

            plBufferHandle tStagingBuffer0 = pl_create_buffer(&ptGraphics->tDevice, &tStagingBufferDescription0, "dynamic buffer");
            plBuffer* ptBuffer = &ptGraphics->sbtBuffersCold[tStagingBuffer0.uIndex];
            plDeviceMemoryAllocation tAllocation = ptDevice->ptDynamicAllocator->allocate(ptDevice->ptDynamicAllocator->ptInst, ptBuffer->tMemoryRequirements.uMemoryTypeBits, ptBuffer->tMemoryRequirements.ulSize, ptBuffer->tMemoryRequirements.ulAlignment, "dynamic buffer");
            pl_bind_buffer_to_memory(ptDevice, tStagingBuffer0, &tAllocation);

            ptDynamicBuffer->uHandle = tStagingBuffer0.uIndex;
            ptDynamicBuffer->tBuffer = ptVulkanGraphics->sbtBuffersHot[tStagingBuffer0.uIndex].tBuffer;
            ptDynamicBuffer->tMemory = ptGraphics->sbtBuffersCold[tStagingBuffer0.uIndex].tMemoryAllocation;
            ptDynamicBuffer->uByteOffset = 0;

            // allocate descriptor sets
            const VkDescriptorSetAllocateInfo tDynamicAllocInfo = {
                .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool     = ptVulkanGraphics->tDescriptorPool,
                .descriptorSetCount = 1,
                .pSetLayouts        = &ptVulkanGraphics->tDynamicDescriptorSetLayout
            };
            PL_VULKAN(vkAllocateDescriptorSets(ptVulkanDevice->tLogicalDevice, &tDynamicAllocInfo, &ptDynamicBuffer->tDescriptorSet));

            VkDescriptorBufferInfo tDescriptorInfo0 = {
                .buffer = ptDynamicBuffer->tBuffer,
                .offset = 0,
                .range  = PL_MAX_DYNAMIC_DATA_SIZE
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
            vkUpdateDescriptorSets(ptVulkanDevice->tLogicalDevice, 1, &tWrite0, 0, NULL);
        }

        ptDynamicBuffer = &ptFrame->sbtDynamicBuffers[ptFrame->uCurrentBufferIndex];
        ptDynamicBuffer->uByteOffset = 0;
    }

    plVulkanBuffer* ptBuffer = &ptVulkanGraphics->sbtBuffersHot[ptDynamicBuffer->uHandle];

    plDynamicBinding tDynamicBinding = {
        .uBufferHandle = ptFrame->uCurrentBufferIndex,
        .uByteOffset   = ptDynamicBuffer->uByteOffset,
        .pcData        = &ptBuffer->pcData[ptDynamicBuffer->uByteOffset]
    };
    ptDynamicBuffer->uByteOffset = (uint32_t)pl_align_up((size_t)ptDynamicBuffer->uByteOffset + PL_MAX_DYNAMIC_DATA_SIZE, ptVulkanDevice->tDeviceProps.limits.minUniformBufferOffsetAlignment);
    return tDynamicBinding;
}

static void
pl_copy_texture_to_buffer(plBlitEncoder* ptEncoder, plTextureHandle tTextureHandle, plBufferHandle tBufferHandle, uint32_t uRegionCount, const plBufferImageCopy* ptRegions)
{
    plDevice* ptDevice = &ptEncoder->ptGraphics->tDevice;
    plVulkanDevice*   ptVulkanDevice   = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptEncoder->ptGraphics->_pInternalData;

    VkCommandBuffer tCmdBuffer = (VkCommandBuffer)ptEncoder->tCommandBuffer._pInternal;

    plTexture* ptColdTexture = pl__get_texture(ptDevice, tTextureHandle);
    VkImageSubresourceRange* atSubResourceRanges = pl_temp_allocator_alloc(&ptVulkanGraphics->tTempAllocator, sizeof(VkImageSubresourceRange) * uRegionCount);
    VkBufferImageCopy*       atCopyRegions       = pl_temp_allocator_alloc(&ptVulkanGraphics->tTempAllocator, sizeof(VkBufferImageCopy) * uRegionCount);
    memset(atSubResourceRanges, 0, sizeof(VkImageSubresourceRange) * uRegionCount);
    memset(atCopyRegions, 0, sizeof(VkBufferImageCopy) * uRegionCount);

    for(uint32_t i = 0; i < uRegionCount; i++)
    {
        VkImageLayout tLayout = ptRegions[i].tCurrentImageUsage == 0 ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : pl__vulkan_layout(ptRegions[i].tCurrentImageUsage);
        atSubResourceRanges[i].aspectMask     = ptColdTexture->tDesc.tUsage & PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        atSubResourceRanges[i].baseMipLevel   = ptRegions[i].uMipLevel;
        atSubResourceRanges[i].levelCount     = 1;
        atSubResourceRanges[i].baseArrayLayer = ptRegions[i].uBaseArrayLayer;
        atSubResourceRanges[i].layerCount     = ptRegions[i].uLayerCount;
        pl__transition_image_layout(tCmdBuffer, ptVulkanGraphics->sbtTexturesHot[tTextureHandle.uIndex].tImage, tLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, atSubResourceRanges[i], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        atCopyRegions[i].bufferOffset                    = ptRegions[i].szBufferOffset;
        atCopyRegions[i].bufferRowLength                 = ptRegions[i].uBufferRowLength;
        atCopyRegions[i].bufferImageHeight               = ptRegions[i].uImageHeight;
        atCopyRegions[i].imageSubresource.aspectMask     = atSubResourceRanges[i].aspectMask;
        atCopyRegions[i].imageSubresource.mipLevel       = ptRegions[i].uMipLevel;
        atCopyRegions[i].imageSubresource.baseArrayLayer = ptRegions[i].uBaseArrayLayer;
        atCopyRegions[i].imageSubresource.layerCount     = ptRegions[i].uLayerCount;
        atCopyRegions[i].imageExtent.width = ptRegions[i].tImageExtent.uWidth;
        atCopyRegions[i].imageExtent.height = ptRegions[i].tImageExtent.uHeight;
        atCopyRegions[i].imageExtent.depth = ptRegions[i].tImageExtent.uDepth;
        
    }
    vkCmdCopyImageToBuffer(tCmdBuffer, ptVulkanGraphics->sbtTexturesHot[tTextureHandle.uIndex].tImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, ptVulkanGraphics->sbtBuffersHot[tBufferHandle.uIndex].tBuffer, uRegionCount, atCopyRegions);

    for(uint32_t i = 0; i < uRegionCount; i++)
    {
        VkImageLayout tLayout = ptRegions[i].tCurrentImageUsage == 0 ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : pl__vulkan_layout(ptRegions[i].tCurrentImageUsage);
        pl__transition_image_layout(tCmdBuffer, ptVulkanGraphics->sbtTexturesHot[tTextureHandle.uIndex].tImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, tLayout, atSubResourceRanges[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    }
        
    pl_temp_allocator_reset(&ptVulkanGraphics->tTempAllocator);
}

static void
pl_generate_mipmaps(plBlitEncoder* ptEncoder, plTextureHandle tTexture)
{
    plDevice* ptDevice = &ptEncoder->ptGraphics->tDevice;
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptEncoder->ptGraphics->_pInternalData;

    plTexture* ptTexture = &ptEncoder->ptGraphics->sbtTexturesCold[tTexture.uIndex];

    VkCommandBuffer tCmdBuffer = (VkCommandBuffer)ptEncoder->tCommandBuffer._pInternal;

    // generate mips
    if(ptTexture->tDesc.uMips > 1)
    {

        // check if format supports linear blitting
        VkFormatProperties tFormatProperties = {0};
        vkGetPhysicalDeviceFormatProperties(ptVulkanDevice->tPhysicalDevice, pl__vulkan_format(ptTexture->tDesc.tFormat), &tFormatProperties);

        plTexture* ptDestTexture = &ptEncoder->ptGraphics->sbtTexturesCold[tTexture.uIndex];
        const VkImageSubresourceRange tSubResourceRange = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = ptDestTexture->tDesc.uMips,
            .baseArrayLayer = 0,
            .layerCount     = ptDestTexture->tDesc.uLayers
        };

        pl__transition_image_layout(tCmdBuffer, ptVulkanGraphics->sbtTexturesHot[tTexture.uIndex].tImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, tSubResourceRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);


        if(tFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)
        {
            VkImageSubresourceRange tMipSubResourceRange = {
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseArrayLayer = 0,
                .layerCount     = ptTexture->tDesc.uLayers,
                .levelCount     = 1
            };

            int iMipWidth = (int)ptTexture->tDesc.tDimensions.x;
            int iMipHeight = (int)ptTexture->tDesc.tDimensions.y;

            for(uint32_t i = 1; i < ptTexture->tDesc.uMips; i++)
            {
                tMipSubResourceRange.baseMipLevel = i - 1;

                pl__transition_image_layout(tCmdBuffer, ptVulkanGraphics->sbtTexturesHot[tTexture.uIndex].tImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, tMipSubResourceRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

                VkImageBlit tBlit = {
                    .srcOffsets[1].x               = iMipWidth,
                    .srcOffsets[1].y               = iMipHeight,
                    .srcOffsets[1].z               = 1,
                    .srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                    .srcSubresource.mipLevel       = i - 1,
                    .srcSubresource.baseArrayLayer = 0,
                    .srcSubresource.layerCount     = 1,     
                    .dstOffsets[1].x               = 1,
                    .dstOffsets[1].y               = 1,
                    .dstOffsets[1].z               = 1,
                    .dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                    .dstSubresource.mipLevel       = i,
                    .dstSubresource.baseArrayLayer = 0,
                    .dstSubresource.layerCount     = 1,
                };

                if(iMipWidth > 1)  tBlit.dstOffsets[1].x = iMipWidth / 2;
                if(iMipHeight > 1) tBlit.dstOffsets[1].y = iMipHeight / 2;

                vkCmdBlitImage(tCmdBuffer, ptVulkanGraphics->sbtTexturesHot[tTexture.uIndex].tImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, ptVulkanGraphics->sbtTexturesHot[tTexture.uIndex].tImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &tBlit, VK_FILTER_LINEAR);

                pl__transition_image_layout(tCmdBuffer, ptVulkanGraphics->sbtTexturesHot[tTexture.uIndex].tImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, tMipSubResourceRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

                if(iMipWidth > 1)  iMipWidth /= 2;
                if(iMipHeight > 1) iMipHeight /= 2;
            }

            tMipSubResourceRange.baseMipLevel = ptTexture->tDesc.uMips - 1;
            pl__transition_image_layout(tCmdBuffer, ptVulkanGraphics->sbtTexturesHot[tTexture.uIndex].tImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, tMipSubResourceRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        }
        else
        {
            PL_ASSERT(false && "format does not support linear blitting");
        }
    }
}

static plTextureHandle
pl_create_swapchain_texture_view(plDevice* ptDevice, const plTextureViewDesc* ptViewDesc, VkImage tImage, const char* pcName)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptGraphics->_pInternalData;

    uint32_t uTextureViewIndex = UINT32_MAX;
    if(pl_sb_size(ptGraphics->sbtTextureFreeIndices) > 0)
        uTextureViewIndex = pl_sb_pop(ptGraphics->sbtTextureFreeIndices);
    else
    {
        uTextureViewIndex = pl_sb_size(ptGraphics->sbtTexturesCold);
        pl_sb_add(ptGraphics->sbtTexturesCold);
        pl_sb_push(ptGraphics->sbtTextureGenerations, UINT32_MAX);
        pl_sb_add(ptVulkanGraphics->sbtTexturesHot);
    }

    plTextureHandle tHandle = {
        .uGeneration = ++ptGraphics->sbtTextureGenerations[uTextureViewIndex],
        .uIndex = uTextureViewIndex
    };

    plTexture tTextureView = {
        .tView = *ptViewDesc,
    };

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~create view~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    VkImageViewCreateInfo tViewInfo = {
        .sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image                           = tImage,
        .viewType                        = VK_IMAGE_VIEW_TYPE_2D,
        .format                          = pl__vulkan_format(ptViewDesc->tFormat),
        .subresourceRange.baseMipLevel   = ptViewDesc->uBaseMip,
        .subresourceRange.levelCount     = tTextureView.tView.uMips,
        .subresourceRange.baseArrayLayer = ptViewDesc->uBaseLayer,
        .subresourceRange.layerCount     = ptViewDesc->uLayerCount,
        .subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
    };
    VkImageView tImageView = VK_NULL_HANDLE;
    PL_VULKAN(vkCreateImageView(ptVulkanDevice->tLogicalDevice, &tViewInfo, NULL, &tImageView));

    ptVulkanGraphics->sbtTexturesHot[uTextureViewIndex].bOriginalView = true;
    ptVulkanGraphics->sbtTexturesHot[uTextureViewIndex].tImageView = tImageView;
    ptGraphics->sbtTexturesCold[uTextureViewIndex] = tTextureView;
    return tHandle;
}

static plSamplerHandle
pl_create_sampler(plDevice* ptDevice, const plSamplerDesc* ptDesc, const char* pcName)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptGraphics->_pInternalData;

    uint32_t uResourceIndex = UINT32_MAX;
    if(pl_sb_size(ptGraphics->sbtSamplerFreeIndices) > 0)
        uResourceIndex = pl_sb_pop(ptGraphics->sbtSamplerFreeIndices);
    else
    {
        uResourceIndex = pl_sb_size(ptGraphics->sbtSamplersCold);
        pl_sb_add(ptGraphics->sbtSamplersCold);
        pl_sb_push(ptGraphics->sbtSamplerGenerations, UINT32_MAX);
        pl_sb_add(ptVulkanGraphics->sbtSamplersHot);
    }

    plSamplerHandle tHandle = {
        .uGeneration = ++ptGraphics->sbtSamplerGenerations[uResourceIndex],
        .uIndex = uResourceIndex
    };

    plSampler tSampler = {
        .tDesc = *ptDesc,
    };

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~create view~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    VkSamplerCreateInfo tSamplerInfo = {
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter               = pl__vulkan_filter(ptDesc->tFilter),
        .addressModeU            = pl__vulkan_wrap(ptDesc->tHorizontalWrap),
        .addressModeV            = pl__vulkan_wrap(ptDesc->tVerticalWrap),
        .anisotropyEnable        = (bool)ptVulkanDevice->tDeviceFeatures.samplerAnisotropy,
        .maxAnisotropy           = ptDesc->fMaxAnisotropy == 0 ? ptVulkanDevice->tDeviceProps.limits.maxSamplerAnisotropy : ptDesc->fMaxAnisotropy,
        .borderColor             = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable           = VK_FALSE,
        .compareOp               = pl__vulkan_compare(ptDesc->tCompare),
        .mipmapMode              = ptDesc->tMipmapMode == PL_MIPMAP_MODE_LINEAR ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .mipLodBias              = ptDesc->fMipBias,
        .minLod                  = ptDesc->fMinMip,
        .maxLod                  = ptDesc->fMaxMip,
    };
    tSamplerInfo.minFilter    = tSamplerInfo.magFilter;
    tSamplerInfo.addressModeW = tSamplerInfo.addressModeU;

    VkSampler tVkSampler = VK_NULL_HANDLE;
    PL_VULKAN(vkCreateSampler(ptVulkanDevice->tLogicalDevice, &tSamplerInfo, NULL, &tVkSampler));

    ptVulkanGraphics->sbtSamplersHot[uResourceIndex] = tVkSampler;
    ptGraphics->sbtSamplersCold[uResourceIndex] = tSampler;
    return tHandle;
}

static void
pl_create_bind_group_layout(plDevice* ptDevice, plBindGroupLayout* ptLayout, const char* pcName)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptGraphics->_pInternalData;

    plVulkanBindGroupLayout tVulkanBindGroupLayout = {0};

    uint32_t uBindGroupLayoutIndex = UINT32_MAX;
    if(pl_sb_size(ptVulkanGraphics->sbtBindGroupLayoutFreeIndices) > 0)
        uBindGroupLayoutIndex = pl_sb_pop(ptVulkanGraphics->sbtBindGroupLayoutFreeIndices);
    else
    {
        uBindGroupLayoutIndex = pl_sb_size(ptVulkanGraphics->sbtBindGroupLayouts);
        pl_sb_add(ptVulkanGraphics->sbtBindGroupLayouts);
    }

    ptLayout->uHandle = uBindGroupLayoutIndex;

    uint32_t uCurrentBinding = 0;
    const uint32_t uDescriptorBindingCount = ptLayout->uTextureBindingCount + ptLayout->uBufferBindingCount + ptLayout->uSamplerBindingCount;
    VkDescriptorSetLayoutBinding* atDescriptorSetLayoutBindings = pl_temp_allocator_alloc(&ptVulkanGraphics->tTempAllocator, uDescriptorBindingCount * sizeof(VkDescriptorSetLayoutBinding));
    VkDescriptorBindingFlagsEXT* atDescriptorSetLayoutFlags = pl_temp_allocator_alloc(&ptVulkanGraphics->tTempAllocator, uDescriptorBindingCount * sizeof(VkDescriptorBindingFlagsEXT));

    for(uint32_t i = 0; i < ptLayout->uBufferBindingCount; i++)
    {
        VkDescriptorSetLayoutBinding tBinding =  {
            .binding         = ptLayout->aBufferBindings[i].uSlot,
            .descriptorType  = ptLayout->aBufferBindings[i].tType == PL_BUFFER_BINDING_TYPE_STORAGE ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags      = pl__vulkan_stage_flags(ptLayout->aBufferBindings[i].tStages),
            .pImmutableSamplers = NULL,
        };
        atDescriptorSetLayoutFlags[uCurrentBinding] = 0;
        atDescriptorSetLayoutBindings[uCurrentBinding++] = tBinding;
    }



    for(uint32_t i = 0 ; i < ptLayout->uTextureBindingCount; i++)
    {
        VkDescriptorSetLayoutBinding tBinding = {
            .binding            = ptLayout->atTextureBindings[i].uSlot,
            .descriptorCount    = ptLayout->atTextureBindings[i].uDescriptorCount,
            .stageFlags         = pl__vulkan_stage_flags(ptLayout->atTextureBindings[i].tStages),
            .pImmutableSamplers = NULL
        };

        if(ptLayout->atTextureBindings[i].tType == PL_TEXTURE_BINDING_TYPE_SAMPLED)
            tBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        else if(ptLayout->atTextureBindings[i].tType == PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT)
            tBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        else if(ptLayout->atTextureBindings[i].tType == PL_TEXTURE_BINDING_TYPE_STORAGE)
            tBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

        if(tBinding.descriptorCount == 0)
            tBinding.descriptorCount = 1;
        atDescriptorSetLayoutFlags[uCurrentBinding] = 0;
        if(ptLayout->atTextureBindings[i].bVariableDescriptorCount)
            atDescriptorSetLayoutFlags[uCurrentBinding] |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT;
        atDescriptorSetLayoutBindings[uCurrentBinding++] = tBinding;
    }

    for(uint32_t i = 0 ; i < ptLayout->uSamplerBindingCount; i++)
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
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,
        .bindingCount = uDescriptorBindingCount,
        .pBindingFlags = atDescriptorSetLayoutFlags,
        .pNext = NULL
    };

    // create descriptor set layout
    const VkDescriptorSetLayoutCreateInfo tDescriptorSetLayoutInfo = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = uDescriptorBindingCount,
        .pBindings    = atDescriptorSetLayoutBindings,
        .pNext = ptDevice->bDescriptorIndexing ? &setLayoutBindingFlags : NULL,
        .flags = ptDevice->bDescriptorIndexing ?  VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT : 0
    };
    PL_VULKAN(vkCreateDescriptorSetLayout(ptVulkanDevice->tLogicalDevice, &tDescriptorSetLayoutInfo, NULL, &tVulkanBindGroupLayout.tDescriptorSetLayout));

    if(pcName)
        pl_set_vulkan_object_name(ptDevice, (uint64_t)tVulkanBindGroupLayout.tDescriptorSetLayout, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT, pcName);

    ptVulkanGraphics->sbtBindGroupLayouts[uBindGroupLayoutIndex] = tVulkanBindGroupLayout;
    pl_temp_allocator_reset(&ptVulkanGraphics->tTempAllocator);
}

static plBindGroupHandle
pl_create_bind_group(plDevice* ptDevice, const plBindGroupLayout* ptLayout, const char* pcName)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptGraphics->_pInternalData;

    uint32_t uBindGroupIndex = UINT32_MAX;
    if(pl_sb_size(ptGraphics->sbtBindGroupFreeIndices) > 0)
        uBindGroupIndex = pl_sb_pop(ptGraphics->sbtBindGroupFreeIndices);
    else
    {
        uBindGroupIndex = pl_sb_size(ptGraphics->sbtBindGroupsCold);
        pl_sb_add(ptGraphics->sbtBindGroupsCold);
        pl_sb_push(ptGraphics->sbtBindGroupGenerations, UINT32_MAX);
        pl_sb_add(ptVulkanGraphics->sbtBindGroupsHot);
    }

    plBindGroupHandle tHandle = {
        .uGeneration = ++ptGraphics->sbtBindGroupGenerations[uBindGroupIndex],
        .uIndex = uBindGroupIndex
    };

    plBindGroup tBindGroup = {
        .tLayout = *ptLayout
    };

    uint32_t uCurrentBinding = 0;
    const uint32_t uDescriptorBindingCount = ptLayout->uTextureBindingCount + ptLayout->uBufferBindingCount + ptLayout->uSamplerBindingCount;
    VkDescriptorSetLayoutBinding* atDescriptorSetLayoutBindings = pl_temp_allocator_alloc(&ptVulkanGraphics->tTempAllocator, uDescriptorBindingCount * sizeof(VkDescriptorSetLayoutBinding));
    VkDescriptorBindingFlagsEXT* atDescriptorSetLayoutFlags = pl_temp_allocator_alloc(&ptVulkanGraphics->tTempAllocator, uDescriptorBindingCount * sizeof(VkDescriptorBindingFlagsEXT));
    uint32_t tDescriptorCount = 1;
    bool bHasVariableDescriptors = false;

    for(uint32_t i = 0; i < ptLayout->uBufferBindingCount; i++)
    {
        VkDescriptorSetLayoutBinding tBinding =  {
            .binding         = ptLayout->aBufferBindings[i].uSlot,
            .descriptorType  = ptLayout->aBufferBindings[i].tType == PL_BUFFER_BINDING_TYPE_STORAGE ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags      = pl__vulkan_stage_flags(ptLayout->aBufferBindings[i].tStages),
            .pImmutableSamplers = NULL,
        };
        atDescriptorSetLayoutFlags[uCurrentBinding] = 0;
        atDescriptorSetLayoutBindings[uCurrentBinding++] = tBinding;     
    }

    for(uint32_t i = 0 ; i < ptLayout->uTextureBindingCount; i++)
    {
        VkDescriptorSetLayoutBinding tBinding = {
            .binding            = ptLayout->atTextureBindings[i].uSlot,
            .descriptorCount    = ptLayout->atTextureBindings[i].uDescriptorCount,
            .stageFlags         = pl__vulkan_stage_flags(ptLayout->atTextureBindings[i].tStages),
            .pImmutableSamplers = NULL
        };

        if(ptLayout->atTextureBindings[i].tType == PL_TEXTURE_BINDING_TYPE_SAMPLED)
            tBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        else if(ptLayout->atTextureBindings[i].tType == PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT)
            tBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        else if(ptLayout->atTextureBindings[i].tType == PL_TEXTURE_BINDING_TYPE_STORAGE)
            tBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

        if(tBinding.descriptorCount > 1)
            tDescriptorCount = tBinding.descriptorCount;
        else if(tBinding.descriptorCount == 0)
            tBinding.descriptorCount = 1;
        atDescriptorSetLayoutFlags[uCurrentBinding] = 0;
        if(ptLayout->atTextureBindings[i].bVariableDescriptorCount)
        {
            atDescriptorSetLayoutFlags[uCurrentBinding] |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT;
            bHasVariableDescriptors = true;
        }
        atDescriptorSetLayoutBindings[uCurrentBinding++] = tBinding;
    }

    for(uint32_t i = 0 ; i < ptLayout->uSamplerBindingCount; i++)
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
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,
        .bindingCount = uDescriptorBindingCount,
        .pBindingFlags = atDescriptorSetLayoutFlags,
        .pNext = NULL
    };

    // create descriptor set layout
    
    const VkDescriptorSetLayoutCreateInfo tDescriptorSetLayoutInfo = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = uDescriptorBindingCount,
        .pBindings    = atDescriptorSetLayoutBindings,
        .pNext = ptDevice->bDescriptorIndexing ? &setLayoutBindingFlags : NULL,
        .flags = ptDevice->bDescriptorIndexing ?  VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT : 0
    };
    VkDescriptorSetLayout tDescriptorSetLayout = VK_NULL_HANDLE;
    PL_VULKAN(vkCreateDescriptorSetLayout(ptVulkanDevice->tLogicalDevice, &tDescriptorSetLayoutInfo, NULL, &tDescriptorSetLayout));

    plVulkanBindGroup tVulkanBindGroup = {
        .tDescriptorSetLayout = tDescriptorSetLayout
    };

    VkDescriptorSetVariableDescriptorCountAllocateInfoEXT variableDescriptorCountAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT,
        .descriptorSetCount = 1,
        .pDescriptorCounts  = &tDescriptorCount,
    };

    // allocate descriptor sets
    const VkDescriptorSetAllocateInfo tAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = ptVulkanGraphics->tDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts        = &tDescriptorSetLayout,
        .pNext              = bHasVariableDescriptors ? &variableDescriptorCountAllocInfo : NULL
    };

    PL_VULKAN(vkAllocateDescriptorSets(ptVulkanDevice->tLogicalDevice, &tAllocInfo, &tVulkanBindGroup.tDescriptorSet));

    pl_temp_allocator_reset(&ptVulkanGraphics->tTempAllocator);

    ptVulkanGraphics->sbtBindGroupsHot[uBindGroupIndex] = tVulkanBindGroup;
    ptGraphics->sbtBindGroupsCold[uBindGroupIndex] = tBindGroup;
    return tHandle;
}

static plBindGroupHandle
pl_get_temporary_bind_group(plDevice* ptDevice, const plBindGroupLayout* ptLayout, const char* pcName)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptGraphics->_pInternalData;
    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptGraphics);

    uint32_t uBindGroupIndex = UINT32_MAX;
    if(pl_sb_size(ptGraphics->sbtBindGroupFreeIndices) > 0)
        uBindGroupIndex = pl_sb_pop(ptGraphics->sbtBindGroupFreeIndices);
    else
    {
        uBindGroupIndex = pl_sb_size(ptGraphics->sbtBindGroupsCold);
        pl_sb_add(ptGraphics->sbtBindGroupsCold);
        pl_sb_push(ptGraphics->sbtBindGroupGenerations, UINT32_MAX);
        pl_sb_add(ptVulkanGraphics->sbtBindGroupsHot);
    }

    plBindGroupHandle tHandle = {
        .uGeneration = ++ptGraphics->sbtBindGroupGenerations[uBindGroupIndex],
        .uIndex = uBindGroupIndex
    };

    plBindGroup tBindGroup = {
        .tLayout = *ptLayout
    };

    uint32_t uCurrentBinding = 0;
    const uint32_t uDescriptorBindingCount = ptLayout->uTextureBindingCount + ptLayout->uBufferBindingCount + ptLayout->uSamplerBindingCount;
    VkDescriptorSetLayoutBinding* atDescriptorSetLayoutBindings = pl_temp_allocator_alloc(&ptVulkanGraphics->tTempAllocator, uDescriptorBindingCount * sizeof(VkDescriptorSetLayoutBinding));
    VkDescriptorBindingFlagsEXT* atDescriptorSetLayoutFlags = pl_temp_allocator_alloc(&ptVulkanGraphics->tTempAllocator, uDescriptorBindingCount * sizeof(VkDescriptorBindingFlagsEXT));
    uint32_t tDescriptorCount = 1;
    bool bHasVariableDescriptors = false;

    for(uint32_t i = 0; i < ptLayout->uBufferBindingCount; i++)
    {
        VkDescriptorSetLayoutBinding tBinding =  {
            .binding         = ptLayout->aBufferBindings[i].uSlot,
            .descriptorType  = ptLayout->aBufferBindings[i].tType == PL_BUFFER_BINDING_TYPE_STORAGE ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags      = pl__vulkan_stage_flags(ptLayout->aBufferBindings[i].tStages),
            .pImmutableSamplers = NULL
        };
        atDescriptorSetLayoutFlags[uCurrentBinding] = 0;
        atDescriptorSetLayoutBindings[uCurrentBinding++] = tBinding;
    }

    for(uint32_t i = 0 ; i < ptLayout->uTextureBindingCount; i++)
    {
        VkDescriptorSetLayoutBinding tBinding = {
            .binding            = ptLayout->atTextureBindings[i].uSlot,
            .descriptorCount    = ptLayout->atTextureBindings[i].uDescriptorCount,
            .stageFlags         = pl__vulkan_stage_flags(ptLayout->atTextureBindings[i].tStages),
            .pImmutableSamplers = NULL
        };

        if(ptLayout->atTextureBindings[i].tType == PL_TEXTURE_BINDING_TYPE_SAMPLED)
            tBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        else if(ptLayout->atTextureBindings[i].tType == PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT)
            tBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        else if(ptLayout->atTextureBindings[i].tType == PL_TEXTURE_BINDING_TYPE_STORAGE)
            tBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

        if(tBinding.descriptorCount > 1)
            tDescriptorCount = tBinding.descriptorCount;
        else if(tBinding.descriptorCount == 0)
            tBinding.descriptorCount = 1;
        atDescriptorSetLayoutFlags[uCurrentBinding] = 0;
        if(ptLayout->atTextureBindings[i].bVariableDescriptorCount)
        {
            atDescriptorSetLayoutFlags[uCurrentBinding] |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT;
            bHasVariableDescriptors = true;
        }
        atDescriptorSetLayoutBindings[uCurrentBinding++] = tBinding;
    }

    for(uint32_t i = 0 ; i < ptLayout->uSamplerBindingCount; i++)
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
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,
        .bindingCount = uDescriptorBindingCount,
        .pBindingFlags = atDescriptorSetLayoutFlags,
        .pNext = NULL
    };

    // create descriptor set layout
    const VkDescriptorSetLayoutCreateInfo tDescriptorSetLayoutInfo = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = uDescriptorBindingCount,
        .pBindings    = atDescriptorSetLayoutBindings,
        .pNext = ptDevice->bDescriptorIndexing ? &setLayoutBindingFlags : NULL,
        .flags = ptDevice->bDescriptorIndexing ?  VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT : 0
    };
    VkDescriptorSetLayout tDescriptorSetLayout = VK_NULL_HANDLE;
    PL_VULKAN(vkCreateDescriptorSetLayout(ptVulkanDevice->tLogicalDevice, &tDescriptorSetLayoutInfo, NULL, &tDescriptorSetLayout));

    pl_temp_allocator_reset(&ptVulkanGraphics->tTempAllocator);

    plVulkanBindGroup tVulkanBindGroup = {
        .tDescriptorSetLayout = tDescriptorSetLayout
    };

    VkDescriptorSetVariableDescriptorCountAllocateInfoEXT variableDescriptorCountAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT,
        .descriptorSetCount = 1,
        .pDescriptorCounts  = &tDescriptorCount,
    };

    // allocate descriptor sets
    const VkDescriptorSetAllocateInfo tAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = ptCurrentFrame->tDynamicDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts        = &tDescriptorSetLayout,
        .pNext              = bHasVariableDescriptors ? &variableDescriptorCountAllocInfo : NULL
    };

    PL_VULKAN(vkAllocateDescriptorSets(ptVulkanDevice->tLogicalDevice, &tAllocInfo, &tVulkanBindGroup.tDescriptorSet));

    ptVulkanGraphics->sbtBindGroupsHot[uBindGroupIndex] = tVulkanBindGroup;
    ptGraphics->sbtBindGroupsCold[uBindGroupIndex] = tBindGroup;
    pl_queue_bind_group_for_deletion(ptDevice, tHandle);
    return tHandle;
}

static void
pl_update_bind_group(plDevice* ptDevice, plBindGroupHandle tHandle, const plBindGroupUpdateData* ptData)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptGraphics->_pInternalData;

    plBindGroup* ptBindGroup = &ptGraphics->sbtBindGroupsCold[tHandle.uIndex];
    plVulkanBindGroup* ptVulkanBindGroup = &ptVulkanGraphics->sbtBindGroupsHot[tHandle.uIndex];

    VkWriteDescriptorSet*   sbtWrites = pl_temp_allocator_alloc(&ptVulkanGraphics->tTempAllocator, (ptData->uBufferCount + ptData->uSamplerCount + ptData->uTextureCount) * sizeof(VkWriteDescriptorSet));
    
    VkDescriptorBufferInfo* sbtBufferDescInfos = ptData->uBufferCount > 0 ? pl_temp_allocator_alloc(&ptVulkanGraphics->tTempAllocator, ptData->uBufferCount * sizeof(VkDescriptorBufferInfo)) : NULL;
    VkDescriptorImageInfo*  sbtImageDescInfos = ptData->uTextureCount > 0 ? pl_temp_allocator_alloc(&ptVulkanGraphics->tTempAllocator, ptData->uTextureCount * sizeof(VkDescriptorImageInfo)) : NULL;
    VkDescriptorImageInfo*  sbtSamplerDescInfos = ptData->uSamplerCount > 0 ? pl_temp_allocator_alloc(&ptVulkanGraphics->tTempAllocator, ptData->uSamplerCount  * sizeof(VkDescriptorImageInfo)) : NULL;

    static const VkDescriptorType atDescriptorTypeLUT[] =
    {
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    };

    uint32_t uCurrentWrite = 0;
    for(uint32_t i = 0 ; i < ptData->uBufferCount; i++)
    {

        const plVulkanBuffer* ptVulkanBuffer = &ptVulkanGraphics->sbtBuffersHot[ptData->atBuffers[i].tBuffer.uIndex];

        sbtBufferDescInfos[i].buffer = ptVulkanBuffer->tBuffer;
        sbtBufferDescInfos[i].offset = ptData->atBuffers[i].szOffset;
        sbtBufferDescInfos[i].range  = ptData->atBuffers[i].szBufferRange;

        sbtWrites[uCurrentWrite].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sbtWrites[uCurrentWrite].dstBinding      = ptData->atBuffers[i].uSlot;
        sbtWrites[uCurrentWrite].dstArrayElement = 0;
        sbtWrites[uCurrentWrite].descriptorType  = atDescriptorTypeLUT[ptBindGroup->tLayout.aBufferBindings[i].tType - 1];
        sbtWrites[uCurrentWrite].descriptorCount = 1;
        sbtWrites[uCurrentWrite].dstSet          = ptVulkanBindGroup->tDescriptorSet;
        sbtWrites[uCurrentWrite].pBufferInfo     = &sbtBufferDescInfos[i];
        sbtWrites[uCurrentWrite].pNext           = NULL;
        uCurrentWrite++;
    }

    static const VkDescriptorType atTextureDescriptorTypeLUT[] =
    {
        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    };
    for(uint32_t i = 0 ; i < ptData->uTextureCount; i++)
    {

        sbtImageDescInfos[i].imageLayout         = ptData->atTextures[i].tCurrentUsage == 0 ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : pl__vulkan_layout(ptData->atTextures[i].tCurrentUsage);
        sbtImageDescInfos[i].imageView           = ptVulkanGraphics->sbtTexturesHot[ptData->atTextures[i].tTexture.uIndex].tImageView;
        sbtWrites[uCurrentWrite].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sbtWrites[uCurrentWrite].dstBinding      = ptData->atTextures[i].uSlot;
        sbtWrites[uCurrentWrite].dstArrayElement = ptData->atTextures[i].uIndex;
        sbtWrites[uCurrentWrite].descriptorType  = atTextureDescriptorTypeLUT[ptData->atTextures[i].tType - 1];
        sbtWrites[uCurrentWrite].descriptorCount = 1;
        sbtWrites[uCurrentWrite].dstSet          = ptVulkanBindGroup->tDescriptorSet;
        sbtWrites[uCurrentWrite].pImageInfo      = &sbtImageDescInfos[i];
        sbtWrites[uCurrentWrite].pNext           = NULL;
        uCurrentWrite++;
    }

    for(uint32_t i = 0 ; i < ptData->uSamplerCount; i++)
    {

        sbtSamplerDescInfos[i].sampler           = ptVulkanGraphics->sbtSamplersHot[ptData->atSamplerBindings[i].tSampler.uIndex];
        sbtWrites[uCurrentWrite].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sbtWrites[uCurrentWrite].dstBinding      = ptData->atSamplerBindings[i].uSlot;
        sbtWrites[uCurrentWrite].dstArrayElement = 0;
        sbtWrites[uCurrentWrite].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
        sbtWrites[uCurrentWrite].descriptorCount = 1;
        sbtWrites[uCurrentWrite].dstSet          = ptVulkanBindGroup->tDescriptorSet;
        sbtWrites[uCurrentWrite].pImageInfo      = &sbtSamplerDescInfos[i];
        sbtWrites[uCurrentWrite].pNext           = NULL;
        uCurrentWrite++;
    }

    vkUpdateDescriptorSets(ptVulkanDevice->tLogicalDevice, uCurrentWrite, sbtWrites, 0, NULL);
    pl_temp_allocator_reset(&ptVulkanGraphics->tTempAllocator);
}

static plTextureHandle
pl_create_texture(plDevice* ptDevice, const plTextureDesc* ptDesc, const char* pcName)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptGraphics->_pInternalData;

    if(pcName == NULL)
        pcName = "unnamed texture";

    plTextureDesc tDesc = *ptDesc;
    strncpy(tDesc.acDebugName, pcName, PL_MAX_NAME_LENGTH);

    if(tDesc.tInitialUsage == PL_TEXTURE_USAGE_UNSPECIFIED)
        tDesc.tInitialUsage = PL_TEXTURE_USAGE_SAMPLED;

    if(tDesc.uMips == 0)
        tDesc.uMips = (uint32_t)floorf(log2f((float)pl_maxi((int)tDesc.tDimensions.x, (int)tDesc.tDimensions.y))) + 1u;

    uint32_t uTextureIndex = UINT32_MAX;
    if(pl_sb_size(ptGraphics->sbtTextureFreeIndices) > 0)
        uTextureIndex = pl_sb_pop(ptGraphics->sbtTextureFreeIndices);
    else
    {
        uTextureIndex = pl_sb_size(ptGraphics->sbtTexturesCold);
        pl_sb_add(ptGraphics->sbtTexturesCold);
        pl_sb_push(ptGraphics->sbtTextureGenerations, UINT32_MAX);
        pl_sb_add(ptVulkanGraphics->sbtTexturesHot);
    }

    plTextureHandle tHandle = {
        .uGeneration = ++ptGraphics->sbtTextureGenerations[uTextureIndex],
        .uIndex = uTextureIndex
    };

    plTexture tTexture = {
        .tDesc = tDesc,
        .tView = {
            .tFormat = tDesc.tFormat,
            .uBaseMip = 0,
            .uMips = tDesc.uMips,
            .uBaseLayer = 0,
            .uLayerCount = tDesc.uLayers,
            .tTexture = tHandle
        },
        ._tDrawBindGroup = {.ulData = UINT64_MAX}
    };

    plVulkanTexture tVulkanTexture = {
        .bOriginalView = true
    };

    VkImageViewType tImageViewType = 0;
    if(tDesc.tType == PL_TEXTURE_TYPE_CUBE)
        tImageViewType = VK_IMAGE_VIEW_TYPE_CUBE;
    else if(tDesc.tType == PL_TEXTURE_TYPE_2D)
        tImageViewType = VK_IMAGE_VIEW_TYPE_2D;
    else if(tDesc.tType == PL_TEXTURE_TYPE_2D_ARRAY)
        tImageViewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    else
    {
        PL_ASSERT(false && "unsupported texture type");
    }

    VkImageUsageFlags tUsageFlags = 0;
    if(tDesc.tUsage & PL_TEXTURE_USAGE_SAMPLED)                  tUsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if(tDesc.tUsage & PL_TEXTURE_USAGE_COLOR_ATTACHMENT)         tUsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if(tDesc.tUsage & PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT) tUsageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if(tDesc.tUsage & PL_TEXTURE_USAGE_TRANSIENT_ATTACHMENT)     tUsageFlags |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
    if(tDesc.tUsage & PL_TEXTURE_USAGE_INPUT_ATTACHMENT)         tUsageFlags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    if(tDesc.tUsage & PL_TEXTURE_USAGE_STORAGE)                  tUsageFlags |= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    // create vulkan image
    VkImageCreateInfo tImageInfo = {
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
        .samples       = 1,
        .flags         = tImageViewType == VK_IMAGE_VIEW_TYPE_CUBE ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0
    };
    
    PL_VULKAN(vkCreateImage(ptVulkanDevice->tLogicalDevice, &tImageInfo, NULL, &tVulkanTexture.tImage));

    // get memory requirements
    VkMemoryRequirements tMemoryRequirements = {0};
    vkGetImageMemoryRequirements(ptVulkanDevice->tLogicalDevice, tVulkanTexture.tImage, &tMemoryRequirements);
    tTexture.tMemoryRequirements.ulSize = tMemoryRequirements.size;
    tTexture.tMemoryRequirements.ulAlignment = tMemoryRequirements.alignment;
    tTexture.tMemoryRequirements.uMemoryTypeBits = tMemoryRequirements.memoryTypeBits;

    ptVulkanGraphics->sbtTexturesHot[uTextureIndex] = tVulkanTexture;
    ptGraphics->sbtTexturesCold[uTextureIndex] = tTexture;
    return tHandle;
}

static void
pl_bind_texture_to_memory(plDevice* ptDevice, plTextureHandle tHandle, const plDeviceMemoryAllocation* ptAllocation)
{
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptDevice->ptGraphics->_pInternalData;
    plGraphics* ptGraphics = ptDevice->ptGraphics;

    plTexture* ptTexture = &ptGraphics->sbtTexturesCold[tHandle.uIndex];
    ptTexture->tMemoryAllocation = *ptAllocation;
    plVulkanTexture* ptVulkanTexture = &ptVulkanGraphics->sbtTexturesHot[tHandle.uIndex];

    PL_VULKAN(vkBindImageMemory(ptVulkanDevice->tLogicalDevice, ptVulkanTexture->tImage, (VkDeviceMemory)ptAllocation->uHandle, ptAllocation->ulOffset));

    VkImageAspectFlags tImageAspectFlags = ptTexture->tDesc.tUsage & PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    if(pl__format_has_stencil(pl__vulkan_format(ptTexture->tDesc.tFormat)))
        tImageAspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;

    VkCommandBuffer tCommandBuffer = {0};
    
    const VkCommandBufferAllocateInfo tAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool        = ptVulkanDevice->tCmdPool,
        .commandBufferCount = 1u,
    };
    vkAllocateCommandBuffers(ptVulkanDevice->tLogicalDevice, &tAllocInfo, &tCommandBuffer);

    const VkCommandBufferBeginInfo tBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    vkBeginCommandBuffer(tCommandBuffer, &tBeginInfo);

    VkImageSubresourceRange tRange = {
        .baseMipLevel   = 0,
        .levelCount     = ptTexture->tDesc.uMips,
        .baseArrayLayer = 0,
        .layerCount     = ptTexture->tDesc.uLayers,
        .aspectMask     = tImageAspectFlags
    };

    pl__transition_image_layout(tCommandBuffer, ptVulkanTexture->tImage, VK_IMAGE_LAYOUT_UNDEFINED, pl__vulkan_layout(ptTexture->tDesc.tInitialUsage), tRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    PL_VULKAN(vkEndCommandBuffer(tCommandBuffer));
    const VkSubmitInfo tSubmitInfo = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1u,
        .pCommandBuffers    = &tCommandBuffer,
    };

    PL_VULKAN(vkQueueSubmit(ptVulkanDevice->tGraphicsQueue, 1, &tSubmitInfo, VK_NULL_HANDLE));
    PL_VULKAN(vkDeviceWaitIdle(ptVulkanDevice->tLogicalDevice));
    vkFreeCommandBuffers(ptVulkanDevice->tLogicalDevice, ptVulkanDevice->tCmdPool, 1, &tCommandBuffer);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~create view~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    VkImageViewType tImageViewType = 0;
    if(ptTexture->tDesc.tType == PL_TEXTURE_TYPE_CUBE)
        tImageViewType = VK_IMAGE_VIEW_TYPE_CUBE;
    else if(ptTexture->tDesc.tType == PL_TEXTURE_TYPE_2D)
        tImageViewType = VK_IMAGE_VIEW_TYPE_2D;
    else if(ptTexture->tDesc.tType == PL_TEXTURE_TYPE_2D_ARRAY)
        tImageViewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    else
    {
        PL_ASSERT(false && "unsupported texture type");
    }

    VkImageViewCreateInfo tViewInfo = {
        .sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image                           = ptVulkanTexture->tImage,
        .viewType                        = tImageViewType,
        .format                          = pl__vulkan_format(ptTexture->tDesc.tFormat),
        .subresourceRange.baseMipLevel   = ptTexture->tView.uBaseMip,
        .subresourceRange.levelCount     = ptTexture->tDesc.uMips,
        .subresourceRange.baseArrayLayer = ptTexture->tView.uBaseLayer,
        .subresourceRange.layerCount     = ptTexture->tView.uLayerCount,
        .subresourceRange.aspectMask     = ptTexture->tDesc.tUsage & PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT,
    };
    PL_VULKAN(vkCreateImageView(ptVulkanDevice->tLogicalDevice, &tViewInfo, NULL, &ptVulkanTexture->tImageView));

    if(ptTexture->tDesc.tUsage & PL_TEXTURE_USAGE_SAMPLED)
    {
        if(pl_sb_size(ptGraphics->sbtFreeDrawBindGroups) == 0)
        {
            const plBindGroupLayout tDrawingBindGroup = {
                .uTextureBindingCount  = 1,
                .atTextureBindings = { 
                    {.uSlot = 0, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
                }
            };
            ptGraphics->sbtTexturesCold[tHandle.uIndex]._tDrawBindGroup = pl_create_bind_group(ptDevice, &tDrawingBindGroup, "draw binding");
        }
        else
        {
            ptGraphics->sbtTexturesCold[tHandle.uIndex]._tDrawBindGroup = pl_sb_pop(ptGraphics->sbtFreeDrawBindGroups);
        }

        const plBindGroupUpdateTextureData atBGTextureData[] = {
            {
                .tTexture = tHandle,
                .uSlot    = 0,
                .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED
            }
        };
        const plBindGroupUpdateData tBGData = {
            .uTextureCount = 1,
            .atTextures = atBGTextureData
        };
        pl_update_bind_group(&ptGraphics->tDevice, ptGraphics->sbtTexturesCold[tHandle.uIndex]._tDrawBindGroup, &tBGData);
    }
}

static plTextureHandle
pl_create_texture_view(plDevice* ptDevice, const plTextureViewDesc* ptViewDesc, const char* pcName)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptGraphics->_pInternalData;

    uint32_t uTextureIndex = UINT32_MAX;
    if(pl_sb_size(ptGraphics->sbtTextureFreeIndices) > 0)
        uTextureIndex = pl_sb_pop(ptGraphics->sbtTextureFreeIndices);
    else
    {
        uTextureIndex = pl_sb_size(ptGraphics->sbtTexturesCold);
        pl_sb_add(ptGraphics->sbtTexturesCold);
        pl_sb_push(ptGraphics->sbtTextureGenerations, UINT32_MAX);
        pl_sb_add(ptVulkanGraphics->sbtTexturesHot);
    }

    plTextureHandle tHandle = {
        .uGeneration = ++ptGraphics->sbtTextureGenerations[uTextureIndex],
        .uIndex = uTextureIndex
    };

    plTexture tTexture = {
        .tDesc = ptGraphics->sbtTexturesCold[ptViewDesc->tTexture.uIndex].tDesc,
        .tView = *ptViewDesc,
        ._tDrawBindGroup = {.ulData = UINT64_MAX}
    };

    plTexture* ptTexture = pl__get_texture(ptDevice, ptViewDesc->tTexture);
    plVulkanTexture* ptOldVulkanTexture = &ptVulkanGraphics->sbtTexturesHot[ptViewDesc->tTexture.uIndex];
    plVulkanTexture* ptNewVulkanTexture = &ptVulkanGraphics->sbtTexturesHot[tHandle.uIndex];

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~create view~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    VkImageViewType tImageViewType = 0;
    if(ptTexture->tDesc.tType == PL_TEXTURE_TYPE_CUBE)
        tImageViewType = VK_IMAGE_VIEW_TYPE_CUBE;
    else if(ptTexture->tDesc.tType == PL_TEXTURE_TYPE_2D)
        tImageViewType = VK_IMAGE_VIEW_TYPE_2D;
    else if(ptTexture->tDesc.tType == PL_TEXTURE_TYPE_2D_ARRAY)
        tImageViewType = VK_IMAGE_VIEW_TYPE_2D;
        // tImageViewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    else
    {
        PL_ASSERT(false && "unsupported texture type");
    }

    VkImageAspectFlags tImageAspectFlags = ptTexture->tDesc.tUsage & PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    VkImageViewCreateInfo tViewInfo = {
        .sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image                           = ptOldVulkanTexture->tImage,
        .viewType                        = tImageViewType,
        .format                          = pl__vulkan_format(ptViewDesc->tFormat),
        .subresourceRange.baseMipLevel   = ptViewDesc->uBaseMip,
        .subresourceRange.levelCount     = ptViewDesc->uMips == 0 ? ptTexture->tDesc.uMips - ptViewDesc->uBaseMip : ptViewDesc->uMips,
        .subresourceRange.baseArrayLayer = ptViewDesc->uBaseLayer,
        .subresourceRange.layerCount     = ptViewDesc->uLayerCount,
        .subresourceRange.aspectMask     = tImageAspectFlags,
    };
    PL_VULKAN(vkCreateImageView(ptVulkanDevice->tLogicalDevice, &tViewInfo, NULL, &ptNewVulkanTexture->tImageView));

    if(ptTexture->tDesc.tUsage & PL_TEXTURE_USAGE_SAMPLED)
    {
        if(pl_sb_size(ptGraphics->sbtFreeDrawBindGroups) == 0)
        {
            const plBindGroupLayout tDrawingBindGroup = {
                .uTextureBindingCount  = 1,
                .atTextureBindings = { 
                    {.uSlot = 0, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
                }
            };
            ptGraphics->sbtTexturesCold[tHandle.uIndex]._tDrawBindGroup = pl_create_bind_group(ptDevice, &tDrawingBindGroup, "draw binding");
        }
        else
        {
            ptGraphics->sbtTexturesCold[tHandle.uIndex]._tDrawBindGroup = pl_sb_pop(ptGraphics->sbtFreeDrawBindGroups);
        }

        const plBindGroupUpdateTextureData atBGTextureData[] = {
            {
                .tTexture = tHandle,
                .uSlot    = 0,
                .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED
            }
        };
        const plBindGroupUpdateData tBGData = {
            .uTextureCount = 1,
            .atTextures = atBGTextureData
        };
        pl_update_bind_group(&ptGraphics->tDevice, ptGraphics->sbtTexturesCold[tHandle.uIndex]._tDrawBindGroup, &tBGData);
    }

    ptNewVulkanTexture->bOriginalView = false;
    ptGraphics->sbtTexturesCold[uTextureIndex] = tTexture;
    return tHandle;
}

static plComputeShaderHandle
pl_create_compute_shader(plDevice* ptDevice, const plComputeShaderDescription* ptDescription)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    uint32_t uResourceIndex = UINT32_MAX;
    if(pl_sb_size(ptGraphics->sbtComputeShaderFreeIndices) > 0)
        uResourceIndex = pl_sb_pop(ptGraphics->sbtComputeShaderFreeIndices);
    else
    {
        uResourceIndex = pl_sb_size(ptGraphics->sbtComputeShadersCold);
        pl_sb_add(ptGraphics->sbtComputeShadersCold);
        pl_sb_push(ptGraphics->sbtComputeShaderGenerations, UINT32_MAX);
        pl_sb_add(ptVulkanGfx->sbtComputeShadersHot);
    }

    plComputeShaderHandle tHandle = {
        .uGeneration = ++ptGraphics->sbtComputeShaderGenerations[uResourceIndex],
        .uIndex = uResourceIndex
    };

    plComputeShader tShader = {
        .tDescription = *ptDescription
    };

    plVulkanComputeShader* ptVulkanShader = &ptVulkanGfx->sbtComputeShadersHot[uResourceIndex];

    // if(ptDescription->pcShaderEntryFunc == NULL)
        tShader.tDescription.tShader.pcEntryFunc = "main";

    ptVulkanShader->szSpecializationSize = 0;
    for(uint32_t i = 0; i < tShader.tDescription.uConstantCount; i++)
    {
        const plSpecializationConstant* ptConstant = &tShader.tDescription.atConstants[i];
        ptVulkanShader->atSpecializationEntries[i].constantID = ptConstant->uID;
        ptVulkanShader->atSpecializationEntries[i].offset = ptConstant->uOffset;
        ptVulkanShader->atSpecializationEntries[i].size = pl__get_data_type_size(ptConstant->tType);
        ptVulkanShader->szSpecializationSize += ptVulkanShader->atSpecializationEntries[i].size;
    }

    for(uint32_t i = 0; i < tShader.tDescription.uBindGroupLayoutCount; i++)
    {
        pl_create_bind_group_layout(&ptGraphics->tDevice, &tShader.tDescription.atBindGroupLayouts[i], "compute shader template bind group layout");
        ptVulkanShader->atDescriptorSetLayouts[i] = ptVulkanGfx->sbtBindGroupLayouts[tShader.tDescription.atBindGroupLayouts[i].uHandle].tDescriptorSetLayout;
    }
    ptVulkanShader->atDescriptorSetLayouts[tShader.tDescription.uBindGroupLayoutCount]  = ptVulkanGfx->tDynamicDescriptorSetLayout;

    VkShaderModuleCreateInfo tShaderCreateInfo = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = tShader.tDescription.tShader.szCodeSize,
        .pCode    = (const uint32_t*)tShader.tDescription.tShader.puCode
    };

    PL_VULKAN(vkCreateShaderModule(ptVulkanDevice->tLogicalDevice, &tShaderCreateInfo, NULL, &ptVulkanShader->tShaderModule));

    VkPipelineLayoutCreateInfo tPipelineLayoutInfo = {
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = tShader.tDescription.uBindGroupLayoutCount + 1,
        .pSetLayouts    = ptVulkanShader->atDescriptorSetLayouts
    };

    plVulkanComputeShader tVulkanShader = {0};

    const uint32_t uNewResourceIndex = uResourceIndex;

    plComputeShaderHandle tVariantHandle = {
        .uGeneration = ++ptGraphics->sbtComputeShaderGenerations[uNewResourceIndex],
        .uIndex = uNewResourceIndex
    };

    const VkSpecializationInfo tSpecializationInfo = {
        .mapEntryCount = tShader.tDescription.uConstantCount,
        .pMapEntries   = ptVulkanShader->atSpecializationEntries,
        .dataSize      = ptVulkanShader->szSpecializationSize,
        .pData         = ptDescription->pTempConstantData
    };

    VkPipelineShaderStageCreateInfo tShaderStage = {
        .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage               = VK_SHADER_STAGE_COMPUTE_BIT,
        .module              = ptVulkanShader->tShaderModule,
        .pName               = tShader.tDescription.tShader.pcEntryFunc,
        .pSpecializationInfo = tShader.tDescription.uConstantCount > 0 ? &tSpecializationInfo : NULL
    };

    PL_VULKAN(vkCreatePipelineLayout(ptVulkanDevice->tLogicalDevice, &tPipelineLayoutInfo, NULL, &tVulkanShader.tPipelineLayout));

    VkComputePipelineCreateInfo tPipelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .layout = tVulkanShader.tPipelineLayout,
        .stage  = tShaderStage
    };
    PL_VULKAN(vkCreateComputePipelines(ptVulkanDevice->tLogicalDevice, VK_NULL_HANDLE, 1, &tPipelineCreateInfo, NULL, &tVulkanShader.tPipeline));

    ptGraphics->sbtComputeShadersCold[uNewResourceIndex] = tShader;

    ptVulkanShader->tPipeline = tVulkanShader.tPipeline;
    ptVulkanShader->tPipelineLayout = tVulkanShader.tPipelineLayout;

    ptGraphics->sbtComputeShadersCold[uResourceIndex] = tShader;
    return tHandle;
}

static plShaderHandle
pl_create_shader(plDevice* ptDevice, const plShaderDescription* ptDescription)
{
    plGraphics*       ptGraphics = ptDevice->ptGraphics;
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    uint32_t uResourceIndex = UINT32_MAX;
    if(pl_sb_size(ptGraphics->sbtShaderFreeIndices) > 0)
        uResourceIndex = pl_sb_pop(ptGraphics->sbtShaderFreeIndices);
    else
    {
        uResourceIndex = pl_sb_size(ptGraphics->sbtShadersCold);
        pl_sb_add(ptGraphics->sbtShadersCold);
        pl_sb_push(ptGraphics->sbtShaderGenerations, UINT32_MAX);
        pl_sb_add(ptVulkanGfx->sbtShadersHot);
    }

    plShaderHandle tHandle = {
        .uGeneration = ++ptGraphics->sbtShaderGenerations[uResourceIndex],
        .uIndex = uResourceIndex
    };

    plShader tShader = {
        .tDescription = *ptDescription
    };

    plVulkanShader* ptVulkanShader = &ptVulkanGfx->sbtShadersHot[uResourceIndex];

    // if(ptDescription->pcPixelShaderEntryFunc == NULL)
        tShader.tDescription.tPixelShader.pcEntryFunc = "main";

    // if(ptDescription->pcVertexShaderEntryFunc == NULL)
        tShader.tDescription.tVertexShader.pcEntryFunc = "main";

    for(uint32_t i = 0; i < tShader.tDescription.uBindGroupLayoutCount; i++)
    {
        pl_create_bind_group_layout(&ptGraphics->tDevice, &tShader.tDescription.atBindGroupLayouts[i], "shader template bind group layout");
        ptVulkanShader->atDescriptorSetLayouts[i] = ptVulkanGfx->sbtBindGroupLayouts[tShader.tDescription.atBindGroupLayouts[i].uHandle].tDescriptorSetLayout;
    }
    ptVulkanShader->atDescriptorSetLayouts[tShader.tDescription.uBindGroupLayoutCount]  = ptVulkanGfx->tDynamicDescriptorSetLayout;

    uint32_t uStageCount = 1;

    VkShaderModuleCreateInfo tVertexShaderCreateInfo = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = tShader.tDescription.tVertexShader.szCodeSize,
        .pCode    = (const uint32_t*)(tShader.tDescription.tVertexShader.puCode)
    };
    PL_VULKAN(vkCreateShaderModule(ptVulkanDevice->tLogicalDevice, &tVertexShaderCreateInfo, NULL, &ptVulkanShader->tVertexShaderModule));


    if(tShader.tDescription.tPixelShader.puCode)
    {
        uStageCount++;
        VkShaderModuleCreateInfo tPixelShaderCreateInfo = {
            .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = tShader.tDescription.tPixelShader.szCodeSize,
            .pCode    = (const uint32_t*)(tShader.tDescription.tPixelShader.puCode),
        };

        PL_VULKAN(vkCreateShaderModule(ptVulkanDevice->tLogicalDevice, &tPixelShaderCreateInfo, NULL, &ptVulkanShader->tPixelShaderModule));
    }

    //---------------------------------------------------------------------
    // input assembler stage
    //---------------------------------------------------------------------

    VkPipelineInputAssemblyStateCreateInfo tInputAssembly = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    VkVertexInputAttributeDescription atAttributeDescription[PL_MAX_VERTEX_ATTRIBUTES] = {0};

    uint32_t uCurrentAttributeCount = 0;
    for(uint32_t i = 0; i < PL_MAX_VERTEX_ATTRIBUTES; i++)
    {
        if(ptDescription->tVertexBufferBinding.atAttributes[i].tFormat == PL_FORMAT_UNKNOWN)
            break;
        atAttributeDescription[i].binding = 0;
        atAttributeDescription[i].location = i;
        atAttributeDescription[i].offset = ptDescription->tVertexBufferBinding.atAttributes[i].uByteOffset;
        atAttributeDescription[i].format = pl__vulkan_format(ptDescription->tVertexBufferBinding.atAttributes[i].tFormat);
        uCurrentAttributeCount++;
    }

    VkVertexInputBindingDescription tBindingDescription = {
        .binding   = 0,
        .stride    = ptDescription->tVertexBufferBinding.uByteStride,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    VkPipelineVertexInputStateCreateInfo tVertexInputInfo = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 1,
        .vertexAttributeDescriptionCount = uCurrentAttributeCount,
        .pVertexBindingDescriptions      = &tBindingDescription,
        .pVertexAttributeDescriptions    = atAttributeDescription,
    };

    ptVulkanShader->szSpecializationSize = 0;
    for(uint32_t i = 0; i < tShader.tDescription.uConstantCount; i++)
    {
        const plSpecializationConstant* ptConstant = &tShader.tDescription.atConstants[i];
        ptVulkanShader->atSpecializationEntries[i].constantID = ptConstant->uID;
        ptVulkanShader->atSpecializationEntries[i].offset = ptConstant->uOffset;
        ptVulkanShader->atSpecializationEntries[i].size = pl__get_data_type_size(ptConstant->tType);
        ptVulkanShader->szSpecializationSize += ptVulkanShader->atSpecializationEntries[i].size;
    }

    const plRenderPassLayout* ptRenderPassLayout = &ptGraphics->sbtRenderPassLayoutsCold[ptDescription->tRenderPassLayout.uIndex];

    plVulkanShader tVulkanShader = {0};
    const uint32_t uNewResourceIndex = uResourceIndex;

    VkPipelineLayoutCreateInfo tPipelineLayoutInfo = {
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = tShader.tDescription.uBindGroupLayoutCount + 1,
        .pSetLayouts    = ptVulkanShader->atDescriptorSetLayouts
    };
    PL_VULKAN(vkCreatePipelineLayout(ptVulkanDevice->tLogicalDevice, &tPipelineLayoutInfo, NULL, &tVulkanShader.tPipelineLayout));
    
    //---------------------------------------------------------------------
    // vertex shader stage
    //---------------------------------------------------------------------

    const VkSpecializationInfo tSpecializationInfo = {
        .mapEntryCount = tShader.tDescription.uConstantCount,
        .pMapEntries   = ptVulkanShader->atSpecializationEntries,
        .dataSize      = ptVulkanShader->szSpecializationSize,
        .pData         = ptDescription->pTempConstantData
    };

    VkPipelineShaderStageCreateInfo tVertShaderStageInfo = {
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage  = VK_SHADER_STAGE_VERTEX_BIT,
        .module = ptVulkanShader->tVertexShaderModule,
        .pName  = tShader.tDescription.tVertexShader.pcEntryFunc,
        .pSpecializationInfo = tShader.tDescription.uConstantCount > 0 ? &tSpecializationInfo : NULL
    };

    //---------------------------------------------------------------------
    // tesselation stage
    //---------------------------------------------------------------------

    //---------------------------------------------------------------------
    // geometry shader stage
    //---------------------------------------------------------------------

    //---------------------------------------------------------------------
    // rasterization stage
    //---------------------------------------------------------------------

    VkViewport tViewport = {
        .x        = 0.0f,
        .y        = 0.0f,
        .width    = 100.0f,
        .height   = 100.0f,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

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
        .depthClampEnable        = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode             = ptDescription->tGraphicsState.ulWireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL,
        .lineWidth               = 1.0f,
        .cullMode                = pl__vulkan_cull((plCullMode)ptDescription->tGraphicsState.ulCullMode),
        .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable         = VK_FALSE
    };

    //---------------------------------------------------------------------
    // fragment shader stage
    //---------------------------------------------------------------------

    VkPipelineShaderStageCreateInfo tFragShaderStageInfo = {
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = ptVulkanShader->tPixelShaderModule,
        .pName  = tShader.tDescription.tPixelShader.pcEntryFunc,
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
        .back.compareOp        = pl__vulkan_compare((plCompareMode)ptDescription->tGraphicsState.ulStencilMode),
        .back.failOp           = pl__vulkan_stencil_op((plStencilOp)ptDescription->tGraphicsState.ulStencilOpFail),
        .back.depthFailOp      = pl__vulkan_stencil_op((plStencilOp)ptDescription->tGraphicsState.ulStencilOpDepthFail),
        .back.passOp           = pl__vulkan_stencil_op((plStencilOp)ptDescription->tGraphicsState.ulStencilOpPass),
        .back.compareMask      = (uint32_t)ptDescription->tGraphicsState.ulStencilMask,
        .back.writeMask        = (uint32_t)ptDescription->tGraphicsState.ulStencilMask,
        .back.reference        = (uint32_t)ptDescription->tGraphicsState.ulStencilRef
    };
    tDepthStencil.front = tDepthStencil.back;

    //---------------------------------------------------------------------
    // color blending stage
    //---------------------------------------------------------------------

    VkPipelineColorBlendAttachmentState atColorBlendAttachment[PL_MAX_RENDER_TARGETS] = {0};

    for(uint32_t i = 0; i < ptDescription->uBlendStateCount; i++)
    {
        atColorBlendAttachment[i].colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        atColorBlendAttachment[i].blendEnable         = ptDescription->atBlendStates[i].bBlendEnabled ? VK_TRUE : VK_FALSE;
        if(ptDescription->atBlendStates[i].bBlendEnabled)
        {
            atColorBlendAttachment[i].srcColorBlendFactor = pl__vulkan_blend_factor(ptDescription->atBlendStates[i].tSrcColorFactor);
            atColorBlendAttachment[i].dstColorBlendFactor = pl__vulkan_blend_factor(ptDescription->atBlendStates[i].tDstColorFactor);
            atColorBlendAttachment[i].colorBlendOp        = pl__vulkan_blend_op(ptDescription->atBlendStates[i].tColorOp);
            atColorBlendAttachment[i].srcAlphaBlendFactor = pl__vulkan_blend_factor(ptDescription->atBlendStates[i].tSrcAlphaFactor);
            atColorBlendAttachment[i].dstAlphaBlendFactor = pl__vulkan_blend_factor(ptDescription->atBlendStates[i].tDstAlphaFactor);
            atColorBlendAttachment[i].alphaBlendOp        = pl__vulkan_blend_op(ptDescription->atBlendStates[i].tAlphaOp);
        }
    }

    VkPipelineColorBlendStateCreateInfo tColorBlending = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable   = VK_FALSE,
        .logicOp         = VK_LOGIC_OP_COPY,
        .attachmentCount = ptDescription->uBlendStateCount,
        .pAttachments    = atColorBlendAttachment,
        .blendConstants  = {0}
    };

    VkPipelineMultisampleStateCreateInfo tMultisampling = {
        .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable  = (bool)ptVulkanDevice->tDeviceFeatures.sampleRateShading,
        .minSampleShading     = 0.2f,
        .rasterizationSamples = 1
    };

    //---------------------------------------------------------------------
    // Create Pipeline
    //---------------------------------------------------------------------
    VkPipelineShaderStageCreateInfo shaderStages[] = {
        tVertShaderStageInfo,
        tFragShaderStageInfo
    };

    VkDynamicState tDynamicStateEnables[3] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_DEPTH_BIAS };
    VkPipelineDynamicStateCreateInfo tDynamicState = {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 3,
        .pDynamicStates    = tDynamicStateEnables,
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
        .renderPass          = ptVulkanGfx->sbtRenderPassLayoutsHot[tShader.tDescription.tRenderPassLayout.uIndex].tRenderPass,
        .subpass             = tShader.tDescription.uSubpassIndex,
        .basePipelineHandle  = VK_NULL_HANDLE,
        .pDepthStencilState  = &tDepthStencil
    };

    PL_VULKAN(vkCreateGraphicsPipelines(ptVulkanDevice->tLogicalDevice, VK_NULL_HANDLE, 1, &tPipelineInfo, NULL, &tVulkanShader.tPipeline));
    ptGraphics->sbtShadersCold[uNewResourceIndex] = tShader;
    ptVulkanShader->tPipeline = tVulkanShader.tPipeline;
    ptVulkanShader->tPipelineLayout = tVulkanShader.tPipelineLayout;

    // no longer need these
    // vkDestroyShaderModule(ptVulkanDevice->tLogicalDevice, tVertexShaderModule, NULL);
    // vkDestroyShaderModule(ptVulkanDevice->tLogicalDevice, tPixelShaderModule, NULL);
    // tVertexShaderModule = VK_NULL_HANDLE;
    // tPixelShaderModule = VK_NULL_HANDLE;
    ptGraphics->sbtShadersCold[uResourceIndex] = tShader;
    return tHandle;
}

static void
pl_create_main_render_pass_layout(plDevice* ptDevice)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    uint32_t uResourceIndex = UINT32_MAX;
    if(pl_sb_size(ptGraphics->sbtRenderPassLayoutFreeIndices) > 0)
        uResourceIndex = pl_sb_pop(ptGraphics->sbtRenderPassLayoutFreeIndices);
    else
    {
        uResourceIndex = pl_sb_size(ptGraphics->sbtRenderPassLayoutsCold);
        pl_sb_add(ptGraphics->sbtRenderPassLayoutsCold);
        pl_sb_push(ptGraphics->sbtRenderPassLayoutGenerations, UINT32_MAX);
        pl_sb_add(ptVulkanGfx->sbtRenderPassLayoutsHot);
    }

    plRenderPassLayoutHandle tHandle = {
        .uGeneration = ++ptGraphics->sbtRenderPassLayoutGenerations[uResourceIndex],
        .uIndex = uResourceIndex
    };

    plRenderPassLayout tLayout = {
        .tDesc = {
            .atRenderTargets = {
                {
                    .tFormat = ptGraphics->tSwapchain.tFormat,
                }
            },
            .uSubpassCount = 1
        }
    };

    plVulkanRenderPassLayout tVulkanRenderPassLayout = {0};

    const VkAttachmentDescription tAttachment = {
        .flags          = 0,
        .format         = pl__vulkan_format(ptGraphics->tSwapchain.tFormat),
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    const VkAttachmentReference tColorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    const VkSubpassDescription tSubpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &tColorReference
    };

    const VkSubpassDependency tSubpassDependencies[] = {

        {
            .srcSubpass      = VK_SUBPASS_EXTERNAL,
            .dstSubpass      = 0,
            .srcStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask   = VK_ACCESS_SHADER_READ_BIT,
            .dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = 0
        },

        {
            .srcSubpass      = 0,
            .dstSubpass      = VK_SUBPASS_EXTERNAL,
            .srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask   = VK_ACCESS_SHADER_READ_BIT,
            .dependencyFlags = 0
        }
    };

    const VkRenderPassCreateInfo tRenderPassInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &tAttachment,
        .subpassCount    = 1,
        .pSubpasses      = &tSubpass,
        .pDependencies   = tSubpassDependencies,
        .dependencyCount = 2
    };

    PL_VULKAN(vkCreateRenderPass(ptVulkanDevice->tLogicalDevice, &tRenderPassInfo, NULL, &tVulkanRenderPassLayout.tRenderPass));

    ptVulkanGfx->sbtRenderPassLayoutsHot[uResourceIndex] = tVulkanRenderPassLayout;
    ptGraphics->sbtRenderPassLayoutsCold[uResourceIndex] = tLayout;
    ptGraphics->tMainRenderPassLayout = tHandle;
}

static void
pl_create_main_render_pass(plDevice* ptDevice)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    uint32_t uResourceIndex = UINT32_MAX;
    if(pl_sb_size(ptGraphics->sbtRenderPassFreeIndices) > 0)
        uResourceIndex = pl_sb_pop(ptGraphics->sbtRenderPassFreeIndices);
    else
    {
        uResourceIndex = pl_sb_size(ptGraphics->sbtRenderPassesCold);
        pl_sb_add(ptGraphics->sbtRenderPassesCold);
        pl_sb_push(ptGraphics->sbtRenderPassGenerations, UINT32_MAX);
        pl_sb_add(ptVulkanGfx->sbtRenderPassesHot);
    }

    plRenderPassHandle tHandle = {
        .uGeneration = ++ptGraphics->sbtRenderPassGenerations[uResourceIndex],
        .uIndex = uResourceIndex
    };

    plRenderPass tRenderPass = {
        .tDesc = {
            .tDimensions = {gptIO->get_io()->afMainViewportSize[0], gptIO->get_io()->afMainViewportSize[1]},
            .tLayout = ptGraphics->tMainRenderPassLayout
        },
        .bSwapchain = true
    };

    plRenderPassLayout* ptLayout = &ptGraphics->sbtRenderPassLayoutsCold[ptGraphics->tMainRenderPassLayout.uIndex];

    plVulkanRenderPass* ptVulkanRenderPass = &ptVulkanGfx->sbtRenderPassesHot[uResourceIndex];

    const VkAttachmentDescription tAttachment = {
        .flags          = 0,
        .format         = pl__vulkan_format(ptGraphics->tSwapchain.tFormat),
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    const VkAttachmentReference tColorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    const VkSubpassDescription tSubpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &tColorReference
    };

    const VkSubpassDependency tSubpassDependencies[] = {

        {
            .srcSubpass      = VK_SUBPASS_EXTERNAL,
            .dstSubpass      = 0,
            .srcStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask   = VK_ACCESS_SHADER_READ_BIT,
            .dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = 0
        },

        {
            .srcSubpass      = 0,
            .dstSubpass      = VK_SUBPASS_EXTERNAL,
            .srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask   = VK_ACCESS_SHADER_READ_BIT,
            .dependencyFlags = 0
        }
    };

    const VkRenderPassCreateInfo tRenderPassInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &tAttachment,
        .subpassCount    = 1,
        .pSubpasses      = &tSubpass,
        .pDependencies   = tSubpassDependencies,
        .dependencyCount = 2
    };

    PL_VULKAN(vkCreateRenderPass(ptVulkanDevice->tLogicalDevice, &tRenderPassInfo, NULL, &ptVulkanRenderPass->tRenderPass));

    for(uint32_t i = 0; i < ptGraphics->tSwapchain.uImageCount; i++)
    {

        VkFramebufferCreateInfo tFrameBufferInfo = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = ptVulkanRenderPass->tRenderPass,
            .attachmentCount = 1,
            .pAttachments    = &ptVulkanGfx->sbtTexturesHot[ptGraphics->tSwapchain.sbtSwapchainTextureViews[i].uIndex].tImageView,
            .width           = (uint32_t)gptIO->get_io()->afMainViewportSize[0],
            .height          = (uint32_t)gptIO->get_io()->afMainViewportSize[1],
            .layers          = 1u,
        };
        PL_VULKAN(vkCreateFramebuffer(ptVulkanDevice->tLogicalDevice, &tFrameBufferInfo, NULL, &ptVulkanRenderPass->atFrameBuffers[i]));
    }

    ptGraphics->sbtRenderPassesCold[uResourceIndex] = tRenderPass;
    ptGraphics->tMainRenderPass = tHandle;
}

static void
pl__fill_common_render_pass_data(const plRenderPassLayoutDescription* ptDesc, plRenderPassLayout* ptLayout, plRenderPassCommonData* ptDataOut)
{
    ptDataOut->uDependencyCount = 2;
    ptLayout->_uAttachmentCount = 0;
    ptDataOut->uColorAttachmentCount = 0;
    
    // find attachment count & descriptions
    for(uint32_t i = 0; i < PL_MAX_RENDER_TARGETS; i++)
    {
        if(ptDesc->atRenderTargets[i].tFormat == PL_FORMAT_UNKNOWN)
            break;
        
        ptDataOut->atAttachments[i].format = pl__vulkan_format(ptDesc->atRenderTargets[i].tFormat);
        ptDataOut->atAttachments[i].samples = 1;

        if(pl__is_depth_format(ptDesc->atRenderTargets[i].tFormat))
        {
            // overwritten by actual renderpass
            ptDataOut->atAttachments[i].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            ptDataOut->atAttachments[i].finalLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            
            ptDataOut->tDepthAttachmentReference.attachment = i;
            ptDataOut->tDepthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }
        else
        {
            // overwritten by actual renderpass
            ptDataOut->atAttachments[i].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            ptDataOut->atAttachments[i].finalLayout   = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            ptDataOut->uColorAttachmentCount++;
            
        }
        ptLayout->_uAttachmentCount++;
    }

    // fill out subpasses
    for(uint32_t i = 0; i < ptDesc->uSubpassCount; i++)
    {
        const plSubpass* ptSubpass = &ptDesc->atSubpasses[i];
        ptDataOut->atSubpasses[i].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        
        // render targets
        uint32_t uCurrentColorAttachment = 0;
        for(uint32_t j = 0; j < ptSubpass->uRenderTargetCount; j++)
        {
            if(pl__is_depth_format(ptDesc->atRenderTargets[ptSubpass->auRenderTargets[j]].tFormat))
            {
                ptDataOut->atSubpasses[i].pDepthStencilAttachment = &ptDataOut->tDepthAttachmentReference;
                ptDataOut->atSubpasses[i].colorAttachmentCount--;
                ptLayout->tDesc.atSubpasses[i]._bHasDepth = true;
            }
            else
            {
                ptDataOut->atSubpassColorAttachmentReferences[i][uCurrentColorAttachment].attachment = ptSubpass->auRenderTargets[j];
                ptDataOut->atSubpassColorAttachmentReferences[i][uCurrentColorAttachment].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                uCurrentColorAttachment++;
            }
        }
        ptDataOut->atSubpasses[i].colorAttachmentCount = uCurrentColorAttachment;
        ptDataOut->atSubpasses[i].pColorAttachments = ptDataOut->atSubpassColorAttachmentReferences[i];

        // input attachments
        for(uint32_t j = 0; j < ptSubpass->uSubpassInputCount; j++)
        {
            const uint32_t uInput = ptSubpass->auSubpassInputs[j];
            ptDataOut->atSubpassInputAttachmentReferences[i][j].attachment = uInput;
            ptDataOut->atSubpassInputAttachmentReferences[i][j].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
        ptDataOut->atSubpasses[i].inputAttachmentCount = ptSubpass->uSubpassInputCount;
        ptDataOut->atSubpasses[i].pInputAttachments = ptDataOut->atSubpassInputAttachmentReferences[i];

        // dependencies
        if(i > 0)
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
        .srcSubpass      = ptDesc->uSubpassCount - 1,
        .dstSubpass      = VK_SUBPASS_EXTERNAL,
        .srcStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .srcAccessMask   = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dstAccessMask   = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
    };
}

static plRenderPassLayoutHandle
pl_create_render_pass_layout(plDevice* ptDevice, const plRenderPassLayoutDescription* ptDesc)
{
    plGraphics*       ptGraphics     = ptDevice->ptGraphics;
    plVulkanGraphics* ptVulkanGfx    = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    // get available index
    uint32_t uResourceIndex = UINT32_MAX;
    if(pl_sb_size(ptGraphics->sbtRenderPassLayoutFreeIndices) > 0)
        uResourceIndex = pl_sb_pop(ptGraphics->sbtRenderPassLayoutFreeIndices);
    else
    {
        uResourceIndex = pl_sb_size(ptGraphics->sbtRenderPassLayoutsCold);
        pl_sb_add(ptGraphics->sbtRenderPassLayoutsCold);
        pl_sb_push(ptGraphics->sbtRenderPassLayoutGenerations, UINT32_MAX);
        pl_sb_add(ptVulkanGfx->sbtRenderPassLayoutsHot);
    }

    const plRenderPassLayoutHandle tHandle = {
        .uGeneration = ++ptGraphics->sbtRenderPassLayoutGenerations[uResourceIndex],
        .uIndex = uResourceIndex
    };

    plRenderPassLayout tLayout = {
        .tDesc = *ptDesc
    };

    plRenderPassCommonData tCommonData = {0};
    pl__fill_common_render_pass_data(ptDesc, &tLayout, &tCommonData);

    plVulkanRenderPassLayout tVulkanRenderPassLayout = {0};

    const VkRenderPassCreateInfo tRenderPassInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = tLayout._uAttachmentCount,
        .pAttachments    = tCommonData.atAttachments,
        .subpassCount    = ptDesc->uSubpassCount,
        .pSubpasses      = tCommonData.atSubpasses,
        .dependencyCount = tCommonData.uDependencyCount,
        .pDependencies   = tCommonData.atSubpassDependencies
    };

    PL_VULKAN(vkCreateRenderPass(ptVulkanDevice->tLogicalDevice, &tRenderPassInfo, NULL, &tVulkanRenderPassLayout.tRenderPass));

    ptVulkanGfx->sbtRenderPassLayoutsHot[uResourceIndex] = tVulkanRenderPassLayout;
    ptGraphics->sbtRenderPassLayoutsCold[uResourceIndex] = tLayout;
    return tHandle;
}

static plRenderPassHandle
pl_create_render_pass(plDevice* ptDevice, const plRenderPassDescription* ptDesc, const plRenderPassAttachments* ptAttachments)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    uint32_t uResourceIndex = UINT32_MAX;
    if(pl_sb_size(ptGraphics->sbtRenderPassFreeIndices) > 0)
        uResourceIndex = pl_sb_pop(ptGraphics->sbtRenderPassFreeIndices);
    else
    {
        uResourceIndex = pl_sb_size(ptGraphics->sbtRenderPassesCold);
        pl_sb_add(ptGraphics->sbtRenderPassesCold);
        pl_sb_push(ptGraphics->sbtRenderPassGenerations, UINT32_MAX);
        pl_sb_add(ptVulkanGfx->sbtRenderPassesHot);
    }

    plRenderPassHandle tHandle = {
        .uGeneration = ++ptGraphics->sbtRenderPassGenerations[uResourceIndex],
        .uIndex = uResourceIndex
    };

    plRenderPass tRenderPass = {
        .tDesc = *ptDesc
    };

    plRenderPassLayout* ptLayout = &ptGraphics->sbtRenderPassLayoutsCold[ptDesc->tLayout.uIndex];

    plRenderPassCommonData tCommonData = {0};
    pl__fill_common_render_pass_data(&ptLayout->tDesc, ptLayout, &tCommonData);

    plVulkanRenderPass* ptVulkanRenderPass = &ptVulkanGfx->sbtRenderPassesHot[uResourceIndex];

    // find attachment count & fill out descriptions
    uint32_t uColorAttachmentCount = 0;
    for(uint32_t i = 0; i < PL_MAX_RENDER_TARGETS; i++)
    {
        if(ptLayout->tDesc.atRenderTargets[i].tFormat == PL_FORMAT_UNKNOWN)
            break;
        
        if(pl__is_depth_format(ptLayout->tDesc.atRenderTargets[i].tFormat))
        {
            tCommonData.atAttachments[i].loadOp         = pl__vulkan_load_op(ptDesc->tDepthTarget.tLoadOp);
            tCommonData.atAttachments[i].storeOp        = pl__vulkan_store_op(ptDesc->tDepthTarget.tStoreOp);
            tCommonData.atAttachments[i].stencilLoadOp  = pl__vulkan_load_op(ptDesc->tDepthTarget.tStencilLoadOp);
            tCommonData.atAttachments[i].stencilStoreOp = pl__vulkan_store_op(ptDesc->tDepthTarget.tStencilStoreOp);
            tCommonData.atAttachments[i].initialLayout  = pl__vulkan_layout(ptDesc->tDepthTarget.tCurrentUsage);
            tCommonData.atAttachments[i].finalLayout    = pl__vulkan_layout(ptDesc->tDepthTarget.tNextUsage);
        }
        else
        {

            // from description
            tCommonData.atAttachments[i].loadOp         = pl__vulkan_load_op(ptDesc->atColorTargets[uColorAttachmentCount].tLoadOp);
            tCommonData.atAttachments[i].storeOp        = pl__vulkan_store_op(ptDesc->atColorTargets[uColorAttachmentCount].tStoreOp);
            tCommonData.atAttachments[i].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            tCommonData.atAttachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            tCommonData.atAttachments[i].initialLayout  = pl__vulkan_layout(ptDesc->atColorTargets[uColorAttachmentCount].tCurrentUsage);
            tCommonData.atAttachments[i].finalLayout    = pl__vulkan_layout(ptDesc->atColorTargets[uColorAttachmentCount].tNextUsage);
            uColorAttachmentCount++;

        }
    }
    
    const VkRenderPassCreateInfo tRenderPassInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = ptLayout->_uAttachmentCount,
        .pAttachments    = tCommonData.atAttachments,
        .subpassCount    = ptLayout->tDesc.uSubpassCount,
        .pSubpasses      = tCommonData.atSubpasses,
        .dependencyCount = tCommonData.uDependencyCount,
        .pDependencies   = tCommonData.atSubpassDependencies
    };

    PL_VULKAN(vkCreateRenderPass(ptVulkanDevice->tLogicalDevice, &tRenderPassInfo, NULL, &ptVulkanRenderPass->tRenderPass));


    for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
    {
        VkImageView atViewAttachments[PL_MAX_RENDER_TARGETS] = {0};

        for(uint32_t j = 0; j < ptLayout->_uAttachmentCount; j++)
        {
            atViewAttachments[j] = ptVulkanGfx->sbtTexturesHot[ptAttachments[i].atViewAttachments[j].uIndex].tImageView;
        }

        VkFramebufferCreateInfo tFrameBufferInfo = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = ptVulkanRenderPass->tRenderPass,
            .attachmentCount = ptLayout->_uAttachmentCount,
            .pAttachments    = atViewAttachments,
            .width           = (uint32_t)ptDesc->tDimensions.x,
            .height          = (uint32_t)ptDesc->tDimensions.y,
            .layers          = 1u,
        };
        PL_VULKAN(vkCreateFramebuffer(ptVulkanDevice->tLogicalDevice, &tFrameBufferInfo, NULL, &ptVulkanRenderPass->atFrameBuffers[i]));
    }

    ptGraphics->sbtRenderPassesCold[uResourceIndex] = tRenderPass;
    return tHandle;
}

static void
pl_update_render_pass_attachments(plDevice* ptDevice, plRenderPassHandle tHandle, plVec2 tDimensions, const plRenderPassAttachments* ptAttachments)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    plRenderPass* ptRenderPass = &ptGraphics->sbtRenderPassesCold[tHandle.uIndex];
    
    plVulkanRenderPass* ptVulkanRenderPass = &ptVulkanGfx->sbtRenderPassesHot[tHandle.uIndex];
    plRenderPassLayout* ptLayout = &ptGraphics->sbtRenderPassLayoutsCold[ptRenderPass->tDesc.tLayout.uIndex];
    plFrameContext* ptFrame = pl__get_frame_resources(ptGraphics);
    ptRenderPass->tDesc.tDimensions = tDimensions;

    const plRenderPassDescription* ptDesc = &ptRenderPass->tDesc;

    for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
    {

        VkImageView atViewAttachments[PL_MAX_RENDER_TARGETS] = {0};

        for(uint32_t j = 0; j < ptLayout->_uAttachmentCount; j++)
        {
            atViewAttachments[j] = ptVulkanGfx->sbtTexturesHot[ptAttachments[i].atViewAttachments[j].uIndex].tImageView;
        }

        VkFramebufferCreateInfo tFrameBufferInfo = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = ptVulkanRenderPass->tRenderPass,
            .attachmentCount = ptLayout->_uAttachmentCount,
            .pAttachments    = atViewAttachments,
            .width           = (uint32_t)ptDesc->tDimensions.x,
            .height          = (uint32_t)ptDesc->tDimensions.y,
            .layers          = 1u,
        };
        pl_sb_push(ptFrame->sbtRawFrameBuffers, ptVulkanRenderPass->atFrameBuffers[i]);
        ptVulkanRenderPass->atFrameBuffers[i] = VK_NULL_HANDLE;
        PL_VULKAN(vkCreateFramebuffer(ptVulkanDevice->tLogicalDevice, &tFrameBufferInfo, NULL, &ptVulkanRenderPass->atFrameBuffers[i]));
    }
}

static plCommandBuffer
pl_begin_command_recording(plGraphics* ptGraphics, const plBeginCommandInfo* ptBeginInfo)
{
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptGraphics);

    VkCommandBuffer tCmdBuffer = VK_NULL_HANDLE;
    if(pl_sb_size(ptCurrentFrame->sbtReadyCommandBuffers) > 0)
    {
        tCmdBuffer = pl_sb_pop(ptCurrentFrame->sbtReadyCommandBuffers);
    }
    else
    {
        const VkCommandBufferAllocateInfo tAllocInfo = {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool        = ptCurrentFrame->tCmdPool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };
        PL_VULKAN(vkAllocateCommandBuffers(ptVulkanDevice->tLogicalDevice, &tAllocInfo, &tCmdBuffer));  
    }

    const VkCommandBufferBeginInfo tBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = ptVulkanGfx->bWithinFrameContext ? 0 : VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    PL_VULKAN(vkBeginCommandBuffer(tCmdBuffer, &tBeginInfo));   
    
    plCommandBuffer tCommandBuffer = {
        ._pInternal = tCmdBuffer
    };

    if(ptBeginInfo)
        tCommandBuffer.tBeginInfo = *ptBeginInfo;
    else
        tCommandBuffer.tBeginInfo.uWaitSemaphoreCount = UINT32_MAX;

    return tCommandBuffer;
}

static plRenderEncoder
pl_begin_render_pass(plGraphics* ptGraphics, plCommandBuffer* ptCmdBuffer, plRenderPassHandle tPass)
{
    VkCommandBuffer tCmdBuffer = (VkCommandBuffer)ptCmdBuffer->_pInternal;
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    plRenderPass* ptRenderPass = &ptGraphics->sbtRenderPassesCold[tPass.uIndex];
    plVulkanRenderPass* ptVulkanRenderPass = &ptVulkanGfx->sbtRenderPassesHot[tPass.uIndex];
    plRenderPassLayout* ptLayout = &ptGraphics->sbtRenderPassLayoutsCold[ptRenderPass->tDesc.tLayout.uIndex];
    
    if(ptRenderPass->bSwapchain)
    {
        const VkClearValue atClearValues[2] = {
            { .color.float32 = {0.0f, 0.0f, 0.0f, 1.0f}},
            { .depthStencil.depth = 1.0f},
        };

        VkRenderPassBeginInfo tRenderPassInfo = {
            .sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass        = ptVulkanRenderPass->tRenderPass,
            .framebuffer       = ptVulkanRenderPass->atFrameBuffers[ptGraphics->tSwapchain.uCurrentImageIndex],
            .renderArea.extent = {
                .width  = (uint32_t)ptRenderPass->tDesc.tDimensions.x,
                .height = (uint32_t)ptRenderPass->tDesc.tDimensions.y
            },
            .clearValueCount   = 2,
            .pClearValues      = atClearValues
        };

        vkCmdBeginRenderPass(tCmdBuffer, &tRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }
    else
    {
        VkClearValue atClearValues[PL_MAX_RENDER_TARGETS] = {0};

        uint32_t uAttachmentCount = 0;

        for(uint32_t i = 0; i < PL_MAX_RENDER_TARGETS; i++)
        {

            if(ptLayout->tDesc.atRenderTargets[i].tFormat == PL_FORMAT_UNKNOWN)
                break;

            if(pl__is_depth_format(ptLayout->tDesc.atRenderTargets[i].tFormat))
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
            .sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass        = ptVulkanRenderPass->tRenderPass,
            .framebuffer       = ptVulkanRenderPass->atFrameBuffers[ptGraphics->uCurrentFrameIndex],
            .renderArea.extent = {
                .width  = (uint32_t)ptRenderPass->tDesc.tDimensions.x,
                .height = (uint32_t)ptRenderPass->tDesc.tDimensions.y
            },
            .clearValueCount   = uAttachmentCount,
            .pClearValues      = atClearValues
        };

        vkCmdBeginRenderPass(tCmdBuffer, &tRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    const VkRect2D tScissor = {
        .extent = {
            .width  = (uint32_t)ptRenderPass->tDesc.tDimensions.x,
            .height = (uint32_t)ptRenderPass->tDesc.tDimensions.y
        }
    };

    const VkViewport tViewport = {
        .width  = ptRenderPass->tDesc.tDimensions.x,
        .height = ptRenderPass->tDesc.tDimensions.y,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    vkCmdSetViewport(tCmdBuffer, 0, 1, &tViewport);
    vkCmdSetScissor(tCmdBuffer, 0, 1, &tScissor);
    
    plRenderEncoder tEncoder = {
        .ptGraphics        = ptGraphics,
        .tCommandBuffer    = *ptCmdBuffer,
        .tRenderPassHandle = tPass,
        ._uCurrentSubpass  = 0,
    };
    return tEncoder;
}

static void
pl_next_subpass(plRenderEncoder* ptEncoder)
{
    ptEncoder->_uCurrentSubpass++;
    VkCommandBuffer tCmdBuffer = (VkCommandBuffer)ptEncoder->tCommandBuffer._pInternal;
    vkCmdNextSubpass(tCmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
}

static void
pl_end_render_pass(plRenderEncoder* ptEncoder)
{
    plGraphics* ptGraphics = ptEncoder->ptGraphics;
    plRenderPass* ptRenderPass = &ptGraphics->sbtRenderPassesCold[ptEncoder->tRenderPassHandle.uIndex];
    plRenderPassLayout* ptLayout = &ptGraphics->sbtRenderPassLayoutsCold[ptRenderPass->tDesc.tLayout.uIndex];
    
    VkCommandBuffer tCmdBuffer = (VkCommandBuffer)ptEncoder->tCommandBuffer._pInternal;

    while(ptEncoder->_uCurrentSubpass < ptLayout->tDesc.uSubpassCount - 1)
    {
        vkCmdNextSubpass(tCmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
        ptEncoder->_uCurrentSubpass++;
    }
    
    
    vkCmdEndRenderPass(tCmdBuffer);
}

static void
pl_bind_vertex_buffer(plRenderEncoder* ptEncoder, plBufferHandle tHandle)
{
    plGraphics* ptGraphics = ptEncoder->ptGraphics;
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    VkCommandBuffer tCmdBuffer = (VkCommandBuffer)ptEncoder->tCommandBuffer._pInternal;
    plVulkanBuffer* ptVertexBuffer = &ptVulkanGfx->sbtBuffersHot[tHandle.uIndex];
    static VkDeviceSize offsets = { 0 };
    vkCmdBindVertexBuffers(tCmdBuffer, 0, 1, &ptVertexBuffer->tBuffer, &offsets);
}

static void
pl_draw(plRenderEncoder* ptEncoder, uint32_t uCount, const plDraw* atDraws)
{
    VkCommandBuffer tCmdBuffer = (VkCommandBuffer)ptEncoder->tCommandBuffer._pInternal;
    for(uint32_t i = 0; i < uCount; i++)
        vkCmdDraw(tCmdBuffer, atDraws[i].uVertexCount, atDraws[i].uInstanceCount, atDraws[i].uVertexStart, atDraws[i].uInstance);
}

static void
pl_draw_indexed(plRenderEncoder* ptEncoder, uint32_t uCount, const plDrawIndex* atDraws)
{
    plGraphics* ptGraphics = ptEncoder->ptGraphics;
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    VkCommandBuffer tCmdBuffer = (VkCommandBuffer)ptEncoder->tCommandBuffer._pInternal;

    uint32_t uCurrentIndexBuffer = UINT32_MAX;

    for(uint32_t i = 0; i < uCount; i++)
    {
        if(atDraws->tIndexBuffer.uIndex != uCurrentIndexBuffer)
        {
            uCurrentIndexBuffer = atDraws->tIndexBuffer.uIndex;
            plVulkanBuffer* ptIndexBuffer = &ptVulkanGfx->sbtBuffersHot[uCurrentIndexBuffer];
            vkCmdBindIndexBuffer(tCmdBuffer, ptIndexBuffer->tBuffer, 0, VK_INDEX_TYPE_UINT32);
        }
        vkCmdDrawIndexed(tCmdBuffer, atDraws[i].uIndexCount, atDraws[i].uInstanceCount, atDraws[i].uIndexStart, atDraws[i].uVertexStart, atDraws[i].uInstance);
    }
}

static void
pl_bind_shader(plRenderEncoder* ptEncoder, plShaderHandle tHandle)
{
    plGraphics* ptGraphics = ptEncoder->ptGraphics;
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanShader* ptVulkanShader = &ptVulkanGfx->sbtShadersHot[tHandle.uIndex];
    VkCommandBuffer tCmdBuffer = (VkCommandBuffer)ptEncoder->tCommandBuffer._pInternal;
    vkCmdSetDepthBias(tCmdBuffer, 0.0f, 0.0f, 0.0f);
    vkCmdBindPipeline(tCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ptVulkanShader->tPipeline);
}

static void
pl_bind_compute_shader(plComputeEncoder* ptEncoder, plComputeShaderHandle tHandle)
{
    plGraphics* ptGraphics = ptEncoder->ptGraphics;
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanComputeShader* ptVulkanShader = &ptVulkanGfx->sbtComputeShadersHot[tHandle.uIndex];
    VkCommandBuffer tCmdBuffer = (VkCommandBuffer)ptEncoder->tCommandBuffer._pInternal;
    vkCmdBindPipeline(tCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ptVulkanShader->tPipeline);
}

typedef struct _plBindGroupManagerData
{
    uint32_t uFirstSlot;
    uint32_t uCount;
    VkDescriptorSet auSlots[4];
    uint32_t auOffsets[2];
} plBindGroupManagerData;

static inline void pl__set_bind_group_count(plBindGroupManagerData* ptData, uint32_t uCount)
{
    ptData->uCount = uCount + 1;
    ptData->uFirstSlot = 0;
}

static inline void pl__set_bind_group(plBindGroupManagerData* ptData, uint32_t uIndex, VkDescriptorSet tSet)
{
    ptData->uFirstSlot = pl_min(ptData->uFirstSlot, uIndex);
    ptData->auSlots[uIndex] = tSet;
}

static inline void pl__set_dynamic_bind_group(plBindGroupManagerData* ptData, VkDescriptorSet tSet, uint32_t uOffset)
{
    ptData->auOffsets[0] = uOffset;
    ptData->uFirstSlot = pl_min(ptData->uFirstSlot, ptData->uCount - 1);
    ptData->auSlots[ptData->uCount - 1] = tSet;
}

static inline void pl__update_bindings(plBindGroupManagerData* ptData, VkCommandBuffer tCmdBuffer, VkPipelineLayout tLayout)
{
    VkDescriptorSet atSets[4] = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
    for(uint32_t i = 0; i < ptData->uCount; i++)
        atSets[i] = ptData->auSlots[i];
    vkCmdBindDescriptorSets(tCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, tLayout, ptData->uFirstSlot, ptData->uCount - ptData->uFirstSlot, &atSets[ptData->uFirstSlot], 1, ptData->auOffsets);
    ptData->uFirstSlot = ptData->uCount - ptData->uFirstSlot - 1;
}

static void
pl_draw_stream(plRenderEncoder* ptEncoder, uint32_t uAreaCount, plDrawArea* atAreas)
{
    pl_begin_profile_sample(__FUNCTION__);
    plGraphics* ptGraphics = ptEncoder->ptGraphics;
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;
    VkCommandBuffer tCmdBuffer = (VkCommandBuffer)ptEncoder->tCommandBuffer._pInternal;

    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptGraphics); 

    static VkDeviceSize offsets = { 0 };
    vkCmdSetDepthBias(tCmdBuffer, 0.0f, 0.0f, 0.0f);

    for(uint32_t i = 0; i < uAreaCount; i++)
    {
        plDrawArea* ptArea = &atAreas[i];

        const VkRect2D tScissor = {
            .offset = {
                .x = ptArea->tScissor.iOffsetX,
                .y = ptArea->tScissor.iOffsetY
            },
            .extent = {
                .width  = ptArea->tScissor.uWidth,
                .height = ptArea->tScissor.uHeight
            }
        };

        const VkViewport tViewport = {
            .x      = ptArea->tViewport.fX,
            .y      = ptArea->tViewport.fY,
            .width  = ptArea->tViewport.fWidth,
            .height = ptArea->tViewport.fHeight,
            .minDepth = ptArea->tViewport.fMinDepth,
            .maxDepth = ptArea->tViewport.fMaxDepth
        };

        vkCmdSetViewport(tCmdBuffer, 0, 1, &tViewport);
        vkCmdSetScissor(tCmdBuffer, 0, 1, &tScissor);  

        plDrawStream* ptStream = ptArea->ptDrawStream;

        const uint32_t uTokens = pl_sb_size(ptStream->sbtStream);
        uint32_t uCurrentStreamIndex = 0;
        uint32_t uTriangleCount = 0;
        uint32_t uIndexBuffer = 0;
        uint32_t uIndexBufferOffset = 0;
        uint32_t uVertexBufferOffset = 0;
        uint32_t uDynamicBufferOffset = 0;
        uint32_t uInstanceStart = 0;
        uint32_t uInstanceCount = 1;
        plVulkanShader* ptVulkanShader = NULL;
        plVulkanDynamicBuffer* ptVulkanDynamicBuffer = NULL;

        plBindGroupManagerData tBindGroupManagerData = {0};

        while(uCurrentStreamIndex < uTokens)
        {

            const uint32_t uDirtyMask = ptStream->sbtStream[uCurrentStreamIndex];
            uCurrentStreamIndex++;

            if(uDirtyMask & PL_DRAW_STREAM_BIT_SHADER)
            {
                const plShader* ptShader= &ptGraphics->sbtShadersCold[ptStream->sbtStream[uCurrentStreamIndex]];
                ptVulkanShader = &ptVulkanGfx->sbtShadersHot[ptStream->sbtStream[uCurrentStreamIndex]];
                vkCmdBindPipeline(tCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ptVulkanShader->tPipeline);
                pl__set_bind_group_count(&tBindGroupManagerData, ptShader->tDescription.uBindGroupLayoutCount);
                uCurrentStreamIndex++;
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_OFFSET)
            {
                uDynamicBufferOffset = ptStream->sbtStream[uCurrentStreamIndex];
                tBindGroupManagerData.auOffsets[0] = uDynamicBufferOffset;
                uCurrentStreamIndex++;
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_0)
            {
                plVulkanBindGroup* ptBindGroup0 = &ptVulkanGfx->sbtBindGroupsHot[ptStream->sbtStream[uCurrentStreamIndex]];
                pl__set_bind_group(&tBindGroupManagerData, 0, ptBindGroup0->tDescriptorSet);
                uCurrentStreamIndex++;
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_1)
            {
                plVulkanBindGroup* ptBindGroup1 = &ptVulkanGfx->sbtBindGroupsHot[ptStream->sbtStream[uCurrentStreamIndex]];
                pl__set_bind_group(&tBindGroupManagerData, 1, ptBindGroup1->tDescriptorSet);
                uCurrentStreamIndex++;
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_2)
            {
                plVulkanBindGroup* ptBindGroup2 = &ptVulkanGfx->sbtBindGroupsHot[ptStream->sbtStream[uCurrentStreamIndex]];
                pl__set_bind_group(&tBindGroupManagerData, 2, ptBindGroup2->tDescriptorSet);
                uCurrentStreamIndex++;
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_BUFFER)
            {
                ptVulkanDynamicBuffer = &ptCurrentFrame->sbtDynamicBuffers[ptStream->sbtStream[uCurrentStreamIndex]];
                pl__set_dynamic_bind_group(&tBindGroupManagerData, ptVulkanDynamicBuffer->tDescriptorSet, uDynamicBufferOffset);
                uCurrentStreamIndex++;
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_INDEX_OFFSET)
            {
                uIndexBufferOffset = ptStream->sbtStream[uCurrentStreamIndex];
                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_VERTEX_OFFSET)
            {
                uVertexBufferOffset = ptStream->sbtStream[uCurrentStreamIndex];
                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_INDEX_BUFFER)
            {
                uIndexBuffer = ptStream->sbtStream[uCurrentStreamIndex];
                if(uIndexBuffer != UINT32_MAX)
                {
                    plVulkanBuffer* ptIndexBuffer = &ptVulkanGfx->sbtBuffersHot[uIndexBuffer];
                    vkCmdBindIndexBuffer(tCmdBuffer, ptIndexBuffer->tBuffer, 0, VK_INDEX_TYPE_UINT32);
                }
                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_VERTEX_BUFFER)
            {
                plVulkanBuffer* ptVertexBuffer = &ptVulkanGfx->sbtBuffersHot[ptStream->sbtStream[uCurrentStreamIndex]];
                vkCmdBindVertexBuffers(tCmdBuffer, 0, 1, &ptVertexBuffer->tBuffer, &offsets);
                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_TRIANGLES)
            {
                uTriangleCount = ptStream->sbtStream[uCurrentStreamIndex];
                uCurrentStreamIndex++;
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_INSTANCE_START)
            {
                uInstanceStart = ptStream->sbtStream[uCurrentStreamIndex];
                uCurrentStreamIndex++;
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_INSTANCE_COUNT)
            {
                uInstanceCount = ptStream->sbtStream[uCurrentStreamIndex];
                uCurrentStreamIndex++;
            }

            // vkCmdBindDescriptorSets(tCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ptVulkanShader->tPipelineLayout, uDescriptorStart, 4 - uDescriptorStart, &atDescriptorSets[uDescriptorStart], 1, &uDynamicBufferOffset);
            pl__update_bindings(&tBindGroupManagerData, tCmdBuffer, ptVulkanShader->tPipelineLayout);

            if(uIndexBuffer == UINT32_MAX)
                vkCmdDraw(tCmdBuffer, uTriangleCount * 3, uInstanceCount, uVertexBufferOffset, uInstanceStart);
            else
                vkCmdDrawIndexed(tCmdBuffer, uTriangleCount * 3, uInstanceCount, uIndexBufferOffset, uVertexBufferOffset, uInstanceStart);
        }
    }
    pl_end_profile_sample();
}

static void
pl_set_viewport(plRenderEncoder* ptEncoder, const plRenderViewport* ptViewport)
{
    VkCommandBuffer tCmdBuffer = (VkCommandBuffer)ptEncoder->tCommandBuffer._pInternal;

    const VkViewport tViewport = {
        .x        = ptViewport->fX,
        .y        = ptViewport->fY,
        .width    = ptViewport->fWidth,
        .height   = ptViewport->fHeight,
        .minDepth = ptViewport->fMinDepth,
        .maxDepth = ptViewport->fMaxDepth
    };

    vkCmdSetViewport(tCmdBuffer, 0, 1, &tViewport);
}

static void
pl_set_scissor_region(plRenderEncoder* ptEncoder, const plScissor* ptScissor)
{
    VkCommandBuffer tCmdBuffer = (VkCommandBuffer)ptEncoder->tCommandBuffer._pInternal;

    const VkRect2D tScissor = {
        .offset = {
            .x = ptScissor->iOffsetX,
            .y = ptScissor->iOffsetY
        },
        .extent = {
            .width  = ptScissor->uWidth,
            .height = ptScissor->uHeight
        }
    };

    vkCmdSetScissor(tCmdBuffer, 0, 1, &tScissor);  
}

typedef struct _plInternalDeviceAllocatorData
{
    plDevice* ptDevice;
    plDeviceMemoryAllocatorI* ptAllocator;
} plInternalDeviceAllocatorData;

static plDeviceMemoryAllocation
pl_allocate_staging_dynamic(struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName)
{
    plInternalDeviceAllocatorData* ptData = (plInternalDeviceAllocatorData*)ptInst;

    plDeviceMemoryAllocation tAllocation = {
        .pHostMapped = NULL,
        .uHandle     = 0,
        .ulOffset    = 0,
        .ulSize      = ulSize,
        .ptAllocator = ptData->ptAllocator,
        .tMemoryMode = PL_MEMORY_GPU_CPU
    };


    plDeviceMemoryAllocation tBlock = pl_allocate_memory(ptData->ptDevice, ulSize, PL_MEMORY_GPU_CPU, uTypeFilter, "dynamic uncached Heap");
    tAllocation.uHandle = tBlock.uHandle;
    tAllocation.pHostMapped = tBlock.pHostMapped;
    ptData->ptDevice->ptGraphics->szHostMemoryInUse += ulSize;
    return tAllocation;
}

static void
pl_free_staging_dynamic(struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation)
{
    plInternalDeviceAllocatorData* ptData = (plInternalDeviceAllocatorData*)ptInst;
    plDeviceMemoryAllocation tBlock = {.uHandle = ptAllocation->uHandle};
    pl_free_memory(ptData->ptDevice, &tBlock);
    ptData->ptDevice->ptGraphics->szHostMemoryInUse -= ptAllocation->ulSize;
    ptAllocation->uHandle = 0;
    ptAllocation->ulSize = 0;
    ptAllocation->ulOffset = 0;
}

static void
pl_initialize_graphics(plWindow* ptWindow, const plGraphicsDesc* ptDesc, plGraphics* ptGraphics)
{
    ptGraphics->bValidationActive = ptDesc->bEnableValidation;
    ptGraphics->ptMainWindow = ptWindow;

    plIO* ptIOCtx = gptIO->get_io();

    ptGraphics->_pInternalData = PL_ALLOC(sizeof(plVulkanGraphics));
    memset(ptGraphics->_pInternalData, 0, sizeof(plVulkanGraphics));

    ptGraphics->tDevice._pInternalData = PL_ALLOC(sizeof(plVulkanDevice));
    memset(ptGraphics->tDevice._pInternalData, 0, sizeof(plVulkanDevice));

    ptGraphics->tSwapchain._pInternalData = PL_ALLOC(sizeof(plVulkanSwapchain));
    memset(ptGraphics->tSwapchain._pInternalData, 0, sizeof(plVulkanSwapchain));

    ptGraphics->tDevice.ptGraphics = ptGraphics;
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;
    plVulkanSwapchain* ptVulkanSwapchain = ptGraphics->tSwapchain._pInternalData;
    
    ptGraphics->uFramesInFlight = PL_FRAMES_IN_FLIGHT;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~create instance~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    static const char* pcKhronosValidationLayer = "VK_LAYER_KHRONOS_validation";

    const char** sbpcEnabledExtensions = NULL;
    pl_sb_push(sbpcEnabledExtensions, VK_KHR_SURFACE_EXTENSION_NAME);

    #ifdef _WIN32
        pl_sb_push(sbpcEnabledExtensions, VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
    #elif defined(__ANDROID__)
        pl_sb_push(sbpcEnabledExtensions, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
    #elif defined(__APPLE__)
        pl_sb_push(sbpcEnabledExtensions, "VK_EXT_metal_surface");
        pl_sb_push(sbpcEnabledExtensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    #else // linux
        pl_sb_push(sbpcEnabledExtensions, VK_KHR_XCB_SURFACE_EXTENSION_NAME);
    #endif

    if(ptGraphics->bValidationActive)
    {
        pl_sb_push(sbpcEnabledExtensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        pl_sb_push(sbpcEnabledExtensions, VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }

    // retrieve supported layers
    uint32_t uInstanceLayersFound = 0u;
    VkLayerProperties* ptAvailableLayers = NULL;
    PL_VULKAN(vkEnumerateInstanceLayerProperties(&uInstanceLayersFound, NULL));
    if(uInstanceLayersFound > 0)
    {
        ptAvailableLayers = (VkLayerProperties*)PL_ALLOC(sizeof(VkLayerProperties) * uInstanceLayersFound);
        PL_VULKAN(vkEnumerateInstanceLayerProperties(&uInstanceLayersFound, ptAvailableLayers));
    }

    // retrieve supported extensions
    uint32_t uInstanceExtensionsFound = 0u;
    VkExtensionProperties* ptAvailableExtensions = NULL;
    PL_VULKAN(vkEnumerateInstanceExtensionProperties(NULL, &uInstanceExtensionsFound, NULL));
    if(uInstanceExtensionsFound > 0)
    {
        ptAvailableExtensions = (VkExtensionProperties*)PL_ALLOC(sizeof(VkExtensionProperties) * uInstanceExtensionsFound);
        PL_VULKAN(vkEnumerateInstanceExtensionProperties(NULL, &uInstanceExtensionsFound, ptAvailableExtensions));
    }

    // ensure extensions are supported
    const char** sbpcMissingExtensions = NULL;
    for(uint32_t i = 0; i < pl_sb_size(sbpcEnabledExtensions); i++)
    {
        const char* requestedExtension = sbpcEnabledExtensions[i];
        bool extensionFound = false;
        for(uint32_t j = 0; j < uInstanceExtensionsFound; j++)
        {
            if(strcmp(requestedExtension, ptAvailableExtensions[j].extensionName) == 0)
            {
                pl_log_trace_to_f(uLogChannel, "extension %s found", ptAvailableExtensions[j].extensionName);
                extensionFound = true;
                break;
            }
        }

        if(!extensionFound)
        {
            pl_sb_push(sbpcMissingExtensions, requestedExtension);
        }
    }

    // report if all requested extensions aren't found
    if(pl_sb_size(sbpcMissingExtensions) > 0)
    {
        // pl_log_error_to_f(uLogChannel, "%d %s", pl_sb_size(sbpcMissingExtensions), "Missing Extensions:");
        for(uint32_t i = 0; i < pl_sb_size(sbpcMissingExtensions); i++)
        {
            pl_log_error_to_f(uLogChannel, "  * %s", sbpcMissingExtensions[i]);
        }

        PL_ASSERT(false && "Can't find all requested extensions");
    }
    pl_sb_free(sbpcMissingExtensions);

    // ensure layers are supported
    const char** sbpcMissingLayers = NULL;
    for(uint32_t i = 0; i < 1; i++)
    {
        const char* pcRequestedLayer = (&pcKhronosValidationLayer)[i];
        bool bLayerFound = false;
        for(uint32_t j = 0; j < uInstanceLayersFound; j++)
        {
            if(strcmp(pcRequestedLayer, ptAvailableLayers[j].layerName) == 0)
            {
                pl_log_trace_to_f(uLogChannel, "layer %s found", ptAvailableLayers[j].layerName);
                bLayerFound = true;
                break;
            }
        }

        if(!bLayerFound)
        {
            pl_sb_push(sbpcMissingLayers, pcRequestedLayer);
        }
    }

    // report if all requested layers aren't found
    if(pl_sb_size(sbpcMissingLayers) > 0)
    {
        pl_log_error_to_f(uLogChannel, "%d %s", pl_sb_size(sbpcMissingLayers), "Missing Layers:");
        for(uint32_t i = 0; i < pl_sb_size(sbpcMissingLayers); i++)
        {
            pl_log_error_to_f(uLogChannel, "  * %s", sbpcMissingLayers[i]);
        }
        PL_ASSERT(false && "Can't find all requested layers");
    }
    pl_sb_free(sbpcMissingLayers);

    // Setup debug messenger for vulkan tInstance
    VkDebugUtilsMessengerCreateInfoEXT tDebugCreateInfo = {
        .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = pl__debug_callback,
        .pNext           = VK_NULL_HANDLE
    };

    // create vulkan tInstance
    VkApplicationInfo tAppInfo = {
        .sType      = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = VK_API_VERSION_1_2
    };

    VkInstanceCreateInfo tCreateInfo = {
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo        = &tAppInfo,
        .pNext                   = ptGraphics->bValidationActive ? (VkDebugUtilsMessengerCreateInfoEXT*)&tDebugCreateInfo : VK_NULL_HANDLE,
        .enabledExtensionCount   = pl_sb_size(sbpcEnabledExtensions),
        .ppEnabledExtensionNames = sbpcEnabledExtensions,
        .enabledLayerCount       = ptDesc->bEnableValidation ? 1 : 0,
        .ppEnabledLayerNames     = &pcKhronosValidationLayer,

        #ifdef __APPLE__
        .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR
        #endif
    };

    PL_VULKAN(vkCreateInstance(&tCreateInfo, NULL, &ptVulkanGfx->tInstance));
    pl_log_trace_to_f(uLogChannel, "created vulkan instance");

    // cleanup
    if(ptAvailableLayers)     PL_FREE(ptAvailableLayers);
    if(ptAvailableExtensions) PL_FREE(ptAvailableExtensions);
    
    if(ptGraphics->bValidationActive)
    {
        PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ptVulkanGfx->tInstance, "vkCreateDebugUtilsMessengerEXT");
        PL_ASSERT(func != NULL && "failed to set up debug messenger!");
        PL_VULKAN(func(ptVulkanGfx->tInstance, &tDebugCreateInfo, NULL, &ptVulkanGfx->tDbgMessenger));     
        pl_log_trace_to_f(uLogChannel, "enabled Vulkan validation layers");
    }

    pl_sb_free(sbpcEnabledExtensions);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~create surface~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    #ifdef _WIN32
        const VkWin32SurfaceCreateInfoKHR tSurfaceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .pNext = NULL,
            .flags = 0,
            .hinstance = GetModuleHandle(NULL),
            .hwnd = (HWND)ptWindow->_pPlatformData
        };
        PL_VULKAN(vkCreateWin32SurfaceKHR(ptVulkanGfx->tInstance, &tSurfaceCreateInfo, NULL, &ptVulkanGfx->tSurface));
    #elif defined(__ANDROID__)
        const VkAndroidSurfaceCreateInfoKHR tSurfaceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
            .pNext = NULL,
            .flags = 0,
            .window =
        };
        PL_VULKAN(vkCreateAndroidSurfaceKHR(ptVulkanGfx->tInstance, &tSurfaceCreateInfo, NULL, &ptVulkanGfx->tSurface));
    #elif defined(__APPLE__)
        typedef struct _plWindowData
        {
            void*           ptWindow;
            void* ptViewController;
            CAMetalLayer*       ptLayer;
        } plWindowData;
        const VkMetalSurfaceCreateInfoEXT tSurfaceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
            .pLayer = ((plWindowData*)ptWindow->_pPlatformData)->ptLayer
        };
        PL_VULKAN(vkCreateMetalSurfaceEXT(ptVulkanGfx->tInstance, &tSurfaceCreateInfo, NULL, &ptVulkanGfx->tSurface));
    #else // linux
        struct tPlatformData { xcb_connection_t* ptConnection; xcb_window_t tWindow;};
        struct tPlatformData* ptPlatformData = (struct tPlatformData*)ptWindow->_pPlatformData;
        const VkXcbSurfaceCreateInfoKHR tSurfaceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
            .pNext = NULL,
            .flags = 0,
            .window = ptPlatformData->tWindow,
            .connection = ptPlatformData->ptConnection
        };
        PL_VULKAN(vkCreateXcbSurfaceKHR(ptVulkanGfx->tInstance, &tSurfaceCreateInfo, NULL, &ptVulkanGfx->tSurface));
    #endif   

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~create device~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    ptVulkanDevice->iGraphicsQueueFamily = -1;
    ptVulkanDevice->iPresentQueueFamily = -1;
    ptVulkanDevice->tMemProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    ptVulkanDevice->tMemBudgetInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
    ptVulkanDevice->tMemProps2.pNext = &ptVulkanDevice->tMemBudgetInfo;
    // int iDeviceIndex = pl__select_physical_device(ptVulkanGfx->tInstance, ptVulkanDevice);

    uint32_t uDeviceCount = 0u;
    int iBestDvcIdx = 0;
    int iDiscreteGPUIdx   = -1;
    int iIntegratedGPUIdx = -1;
    VkDeviceSize tMaxLocalMemorySize = 0u;

    PL_VULKAN(vkEnumeratePhysicalDevices(ptVulkanGfx->tInstance, &uDeviceCount, NULL));
    PL_ASSERT(uDeviceCount > 0 && "failed to find GPUs with Vulkan support!");

    // check if device is suitable
    VkPhysicalDevice atDevices[16] = {0};
    PL_VULKAN(vkEnumeratePhysicalDevices(ptVulkanGfx->tInstance, &uDeviceCount, atDevices));

    // prefer discrete, then memory size
    for(uint32_t i = 0; i < uDeviceCount; i++)
    {
        vkGetPhysicalDeviceProperties(atDevices[i], &ptVulkanDevice->tDeviceProps);
        vkGetPhysicalDeviceMemoryProperties(atDevices[i], &ptVulkanDevice->tMemProps);

        if(ptVulkanDevice->tDeviceProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            iDiscreteGPUIdx = i;
        }
        if(ptVulkanDevice->tDeviceProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
        {
            iIntegratedGPUIdx = i;
        }
    }

    if(iDiscreteGPUIdx > -1)
        iBestDvcIdx = iDiscreteGPUIdx;
    else if(iIntegratedGPUIdx > -1)
        iBestDvcIdx = iIntegratedGPUIdx;

    ptVulkanDevice->tPhysicalDevice = atDevices[iBestDvcIdx];

    PL_ASSERT(ptVulkanDevice->tPhysicalDevice != VK_NULL_HANDLE && "failed to find a suitable GPU!");
    vkGetPhysicalDeviceProperties(atDevices[iBestDvcIdx], &ptVulkanDevice->tDeviceProps);
    vkGetPhysicalDeviceMemoryProperties(atDevices[iBestDvcIdx], &ptVulkanDevice->tMemProps);
    static const char* pacDeviceTypeName[] = {"Other", "Integrated", "Discrete", "Virtual", "CPU"};

    // print info on chosen device
    pl_log_info_to_f(uLogChannel, "Device ID: %u", ptVulkanDevice->tDeviceProps.deviceID);
    pl_log_info_to_f(uLogChannel, "Vendor ID: %u", ptVulkanDevice->tDeviceProps.vendorID);
    pl_log_info_to_f(uLogChannel, "API Version: %u", ptVulkanDevice->tDeviceProps.apiVersion);
    pl_log_info_to_f(uLogChannel, "Driver Version: %u", ptVulkanDevice->tDeviceProps.driverVersion);
    pl_log_info_to_f(uLogChannel, "Device Type: %s", pacDeviceTypeName[ptVulkanDevice->tDeviceProps.deviceType]);
    pl_log_info_to_f(uLogChannel, "Device Name: %s", ptVulkanDevice->tDeviceProps.deviceName);

    uint32_t uExtensionCount = 0;
    vkEnumerateDeviceExtensionProperties(ptVulkanDevice->tPhysicalDevice, NULL, &uExtensionCount, NULL);
    VkExtensionProperties* ptExtensions = pl_temp_allocator_alloc(&ptVulkanGfx->tTempAllocator, uExtensionCount * sizeof(VkExtensionProperties));
    vkEnumerateDeviceExtensionProperties(ptVulkanDevice->tPhysicalDevice, NULL, &uExtensionCount, ptExtensions);

    for(uint32_t i = 0; i < uExtensionCount; i++)
    {
        if(pl_str_equal(ptExtensions[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) ptVulkanDevice->bSwapchainExtPresent = true; //-V522
        if(pl_str_equal(ptExtensions[i].extensionName, "VK_KHR_portability_subset"))     ptVulkanDevice->bPortabilitySubsetPresent = true; //-V522
    }
    pl_temp_allocator_reset(&ptVulkanGfx->tTempAllocator);

    ptVulkanDevice->tMaxLocalMemSize = ptVulkanDevice->tMemProps.memoryHeaps[iBestDvcIdx].size;
    ptVulkanDevice->uUniformBufferBlockSize = pl_minu(PL_DEVICE_ALLOCATION_BLOCK_SIZE, ptVulkanDevice->tDeviceProps.limits.maxUniformBufferRange);

    // find queue families
    uint32_t uQueueFamCnt = 0u;
    vkGetPhysicalDeviceQueueFamilyProperties(ptVulkanDevice->tPhysicalDevice, &uQueueFamCnt, NULL);

    VkQueueFamilyProperties auQueueFamilies[64] = {0};
    vkGetPhysicalDeviceQueueFamilyProperties(ptVulkanDevice->tPhysicalDevice, &uQueueFamCnt, auQueueFamilies);

    for(uint32_t i = 0; i < uQueueFamCnt; i++)
    {
        if (auQueueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) ptVulkanDevice->iGraphicsQueueFamily = i;

        VkBool32 tPresentSupport = false;
        PL_VULKAN(vkGetPhysicalDeviceSurfaceSupportKHR(ptVulkanDevice->tPhysicalDevice, i, ptVulkanGfx->tSurface, &tPresentSupport));

        if (tPresentSupport) ptVulkanDevice->iPresentQueueFamily  = i;

        if (ptVulkanDevice->iGraphicsQueueFamily > -1 && ptVulkanDevice->iPresentQueueFamily > -1) // complete
            break;
        i++;
    }

    VkPhysicalDeviceDescriptorIndexingFeatures tDescriptorIndexingFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
        .pNext = &ptVulkanDevice->tDeviceFeatures12
    };

    // create logical device
    ptVulkanDevice->tDeviceFeatures12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    ptVulkanDevice->tDeviceFeatures2.pNext = &tDescriptorIndexingFeatures;
    ptVulkanDevice->tDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

    vkGetPhysicalDeviceFeatures(ptVulkanDevice->tPhysicalDevice, &ptVulkanDevice->tDeviceFeatures);
    vkGetPhysicalDeviceFeatures2(ptVulkanDevice->tPhysicalDevice, &ptVulkanDevice->tDeviceFeatures2);


    // Non-uniform indexing and update after bind
    // binding flags for textures, uniforms, and buffers
    // are required for our extension
    if(tDescriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing)
    {
        ptGraphics->tDevice.bDescriptorIndexing = true;
        PL_ASSERT(tDescriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing);
        PL_ASSERT(tDescriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind);
//        PL_ASSERT(tDescriptorIndexingFeatures.shaderUniformBufferArrayNonUniformIndexing);
//        PL_ASSERT(tDescriptorIndexingFeatures.descriptorBindingUniformBufferUpdateAfterBind);
//        PL_ASSERT(tDescriptorIndexingFeatures.shaderStorageBufferArrayNonUniformIndexing);
//        PL_ASSERT(tDescriptorIndexingFeatures.descriptorBindingStorageBufferUpdateAfterBind);
    }

    const float fQueuePriority = 1.0f;
    VkDeviceQueueCreateInfo atQueueCreateInfos[] = {
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = ptVulkanDevice->iGraphicsQueueFamily,
            .queueCount = 1,
            .pQueuePriorities = &fQueuePriority
        },
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = ptVulkanDevice->iPresentQueueFamily,
            .queueCount = 1,
            .pQueuePriorities = &fQueuePriority   
        }
    };
    
    static const char* pcValidationLayers = "VK_LAYER_KHRONOS_validation";

    uint32_t uDeviceExtensionCount = 0;
    const char* apcDeviceExts[16] = {0};
    if(ptGraphics->bValidationActive)
    {
        apcDeviceExts[0] = VK_EXT_DEBUG_MARKER_EXTENSION_NAME;
        uDeviceExtensionCount++;
    }
    if(ptVulkanDevice->bSwapchainExtPresent)      apcDeviceExts[uDeviceExtensionCount++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    if(ptVulkanDevice->bPortabilitySubsetPresent) apcDeviceExts[uDeviceExtensionCount++] = "VK_KHR_portability_subset";
    VkDeviceCreateInfo tCreateDeviceInfo = {
        .sType                    = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount     = atQueueCreateInfos[0].queueFamilyIndex == atQueueCreateInfos[1].queueFamilyIndex ? 1 : 2,
        .pQueueCreateInfos        = atQueueCreateInfos,
        .pEnabledFeatures         = &ptVulkanDevice->tDeviceFeatures,
        .ppEnabledExtensionNames  = apcDeviceExts,
        .enabledLayerCount        = ptGraphics->bValidationActive ? 1 : 0,
        .ppEnabledLayerNames      = ptGraphics->bValidationActive ? &pcValidationLayers : NULL,
        .enabledExtensionCount    = uDeviceExtensionCount,
        .pNext                    = &ptVulkanDevice->tDeviceFeatures12
    };
    PL_VULKAN(vkCreateDevice(ptVulkanDevice->tPhysicalDevice, &tCreateDeviceInfo, NULL, &ptVulkanDevice->tLogicalDevice));

    // get device queues
    vkGetDeviceQueue(ptVulkanDevice->tLogicalDevice, ptVulkanDevice->iGraphicsQueueFamily, 0, &ptVulkanDevice->tGraphicsQueue);
    vkGetDeviceQueue(ptVulkanDevice->tLogicalDevice, ptVulkanDevice->iPresentQueueFamily, 0, &ptVulkanDevice->tPresentQueue);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~debug markers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    if(ptGraphics->bValidationActive)
    {
        ptVulkanDevice->vkDebugMarkerSetObjectTag  = (PFN_vkDebugMarkerSetObjectTagEXT)vkGetDeviceProcAddr(ptVulkanDevice->tLogicalDevice, "vkDebugMarkerSetObjectTagEXT");
        ptVulkanDevice->vkDebugMarkerSetObjectName = (PFN_vkDebugMarkerSetObjectNameEXT)vkGetDeviceProcAddr(ptVulkanDevice->tLogicalDevice, "vkDebugMarkerSetObjectNameEXT");
        ptVulkanDevice->vkCmdDebugMarkerBegin      = (PFN_vkCmdDebugMarkerBeginEXT)vkGetDeviceProcAddr(ptVulkanDevice->tLogicalDevice, "vkCmdDebugMarkerBeginEXT");
        ptVulkanDevice->vkCmdDebugMarkerEnd        = (PFN_vkCmdDebugMarkerEndEXT)vkGetDeviceProcAddr(ptVulkanDevice->tLogicalDevice, "vkCmdDebugMarkerEndEXT");
        ptVulkanDevice->vkCmdDebugMarkerInsert     = (PFN_vkCmdDebugMarkerInsertEXT)vkGetDeviceProcAddr(ptVulkanDevice->tLogicalDevice, "vkCmdDebugMarkerInsertEXT");
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~command pool~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const VkCommandPoolCreateInfo tCommandPoolInfo = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = ptVulkanDevice->iGraphicsQueueFamily,
        .flags            = 0
    };
    PL_VULKAN(vkCreateCommandPool(ptVulkanDevice->tLogicalDevice, &tCommandPoolInfo, NULL, &ptVulkanDevice->tCmdPool));

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~swapchain~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    ptGraphics->tSwapchain.bVSync = true;
    pl__create_swapchain(ptGraphics, (uint32_t)ptIOCtx->afMainViewportSize[0], (uint32_t)ptIOCtx->afMainViewportSize[1]);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~main descriptor pool~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    VkDescriptorPoolSize atPoolSizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER,                100000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          100000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          100000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         100000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         100000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       100000 }
    };
    VkDescriptorPoolCreateInfo tDescriptorPoolInfo = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = 100000 * 7,
        .poolSizeCount = 7,
        .pPoolSizes    = atPoolSizes,
    };
    if(ptGraphics->tDevice.bDescriptorIndexing)
    {
        tDescriptorPoolInfo.flags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
    }
    PL_VULKAN(vkCreateDescriptorPool(ptVulkanDevice->tLogicalDevice, &tDescriptorPoolInfo, NULL, &ptVulkanGfx->tDescriptorPool));

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~frame resources~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    static plInternalDeviceAllocatorData tAllocatorData = {0};
    static plDeviceMemoryAllocatorI tAllocator = {0};
    tAllocatorData.ptAllocator = &tAllocator;
    tAllocatorData.ptDevice = &ptGraphics->tDevice;
    tAllocator.allocate = pl_allocate_staging_dynamic;
    tAllocator.free = pl_free_staging_dynamic;
    tAllocator.ptInst = (struct plDeviceMemoryAllocatorO*)&tAllocatorData;
    ptGraphics->tDevice.ptDynamicAllocator = &tAllocator;
    plDeviceMemoryAllocatorI* ptDynamicAllocator = &tAllocator;

    // dynamic buffer stuff
    VkDescriptorSetLayoutBinding tBinding =  {
        .binding         = 0,
        .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        .descriptorCount = 1,
        .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
        .pImmutableSamplers = NULL
    };

    const VkDescriptorSetLayoutCreateInfo tDescriptorSetLayoutInfo = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings    = &tBinding,
    };
    PL_VULKAN(vkCreateDescriptorSetLayout(ptVulkanDevice->tLogicalDevice, &tDescriptorSetLayoutInfo, NULL, &ptVulkanGfx->tDynamicDescriptorSetLayout));

    const VkCommandPoolCreateInfo tFrameCommandPoolInfo = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = ptVulkanDevice->iGraphicsQueueFamily,
        .flags            = 0
    };
    
    const VkSemaphoreCreateInfo tSemaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    const VkFenceCreateInfo tFenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    pl_sb_resize(ptVulkanGfx->sbFrames, ptGraphics->uFramesInFlight);
    pl_sb_resize(ptGraphics->sbtGarbage, ptGraphics->uFramesInFlight);
    for(uint32_t i = 0; i < ptGraphics->uFramesInFlight; i++)
    {
        plFrameContext tFrame = {0};
        PL_VULKAN(vkCreateSemaphore(ptVulkanDevice->tLogicalDevice, &tSemaphoreInfo, NULL, &tFrame.tImageAvailable));
        PL_VULKAN(vkCreateSemaphore(ptVulkanDevice->tLogicalDevice, &tSemaphoreInfo, NULL, &tFrame.tRenderFinish));
        PL_VULKAN(vkCreateFence(ptVulkanDevice->tLogicalDevice, &tFenceInfo, NULL, &tFrame.tInFlight));
        PL_VULKAN(vkCreateCommandPool(ptVulkanDevice->tLogicalDevice, &tFrameCommandPoolInfo, NULL, &tFrame.tCmdPool));

        // dynamic buffer stuff
        pl_sb_resize(tFrame.sbtDynamicBuffers, 1);
        plBufferDescription tStagingBufferDescription0 = {
            .tUsage               = PL_BUFFER_USAGE_UNIFORM | PL_BUFFER_USAGE_STAGING,
            .uByteSize            = PL_DEVICE_ALLOCATION_BLOCK_SIZE
        };
        pl_sprintf(tStagingBufferDescription0.acDebugName, "D-BUF-F%d-0", (int)i);

        plBufferHandle tStagingBuffer0 = pl_create_buffer(&ptGraphics->tDevice, &tStagingBufferDescription0, "dynamic buffer 0");
        plBuffer* ptBuffer = &ptGraphics->sbtBuffersCold[tStagingBuffer0.uIndex];
        plDeviceMemoryAllocation tAllocation = ptDynamicAllocator->allocate(ptDynamicAllocator->ptInst, ptBuffer->tMemoryRequirements.uMemoryTypeBits, ptBuffer->tMemoryRequirements.ulSize, ptBuffer->tMemoryRequirements.ulAlignment, "dynamic buffer");
        pl_bind_buffer_to_memory(&ptGraphics->tDevice, tStagingBuffer0, &tAllocation);

        tFrame.uCurrentBufferIndex = UINT32_MAX;
        tFrame.sbtDynamicBuffers[0].uHandle = tStagingBuffer0.uIndex;
        tFrame.sbtDynamicBuffers[0].tBuffer = ptVulkanGfx->sbtBuffersHot[tStagingBuffer0.uIndex].tBuffer;
        tFrame.sbtDynamicBuffers[0].tMemory = tAllocation;
        tFrame.sbtDynamicBuffers[0].uByteOffset = 0;

          // allocate descriptor sets
        const VkDescriptorSetAllocateInfo tDynamicAllocInfo = {
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool     = ptVulkanGfx->tDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts        = &ptVulkanGfx->tDynamicDescriptorSetLayout
        };
        PL_VULKAN(vkAllocateDescriptorSets(ptVulkanDevice->tLogicalDevice, &tDynamicAllocInfo, &tFrame.sbtDynamicBuffers[0].tDescriptorSet));

        VkDescriptorBufferInfo tDescriptorInfo0 = {
            .buffer = tFrame.sbtDynamicBuffers[0].tBuffer,
            .offset = 0,
            .range  = PL_MAX_DYNAMIC_DATA_SIZE
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
        vkUpdateDescriptorSets(ptVulkanDevice->tLogicalDevice, 1, &tWrite0, 0, NULL);

        VkDescriptorPoolSize atDynamicPoolSizes[] =
        {
            { VK_DESCRIPTOR_TYPE_SAMPLER,                100000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          100000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          100000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         100000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         100000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       100000 }
        };
        VkDescriptorPoolCreateInfo tDynamicDescriptorPoolInfo = {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            .maxSets       = 100000 * 7,
            .poolSizeCount = 7,
            .pPoolSizes    = atDynamicPoolSizes,
        };
        if(ptGraphics->tDevice.bDescriptorIndexing)
        {
            tDynamicDescriptorPoolInfo.flags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
        }
        PL_VULKAN(vkCreateDescriptorPool(ptVulkanDevice->tLogicalDevice, &tDynamicDescriptorPoolInfo, NULL, &tFrame.tDynamicDescriptorPool));

        ptVulkanGfx->sbFrames[i] = tFrame;
    }
    pl_temp_allocator_reset(&ptVulkanGfx->tTempAllocator);

    pl_create_main_render_pass_layout(&ptGraphics->tDevice);
    pl_create_main_render_pass(&ptGraphics->tDevice);
}

static bool
pl_begin_frame(plGraphics* ptGraphics)
{
    pl_begin_profile_sample(__FUNCTION__);
    plIO* ptIOCtx = gptIO->get_io();

    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;
    plVulkanSwapchain* ptVulkanSwapchain = ptGraphics->tSwapchain._pInternalData;
    ptVulkanGfx->bWithinFrameContext = true;

    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptGraphics);
    ptCurrentFrame->uCurrentBufferIndex = UINT32_MAX;

    PL_VULKAN(vkWaitForFences(ptVulkanDevice->tLogicalDevice, 1, &ptCurrentFrame->tInFlight, VK_TRUE, UINT64_MAX));
    pl__garbage_collect(ptGraphics);
    
    VkResult err = vkAcquireNextImageKHR(ptVulkanDevice->tLogicalDevice, ptVulkanSwapchain->tSwapChain, UINT64_MAX, ptCurrentFrame->tImageAvailable, VK_NULL_HANDLE, &ptGraphics->tSwapchain.uCurrentImageIndex);
    if(err == VK_SUBOPTIMAL_KHR || err == VK_ERROR_OUT_OF_DATE_KHR)
    {
        if(err == VK_ERROR_OUT_OF_DATE_KHR)
        {
            pl__create_swapchain(ptGraphics, (uint32_t)ptIOCtx->afMainViewportSize[0], (uint32_t)ptIOCtx->afMainViewportSize[1]);
            pl_end_profile_sample();
            return false;
        }
    }
    else
    {
        PL_VULKAN(err);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptCurrentFrame->sbtPendingCommandBuffers); i++)
    {
        pl_sb_push(ptCurrentFrame->sbtReadyCommandBuffers, ptCurrentFrame->sbtPendingCommandBuffers[i]);
    }
    pl_sb_reset(ptCurrentFrame->sbtPendingCommandBuffers);

    PL_VULKAN(vkResetDescriptorPool(ptVulkanDevice->tLogicalDevice, ptCurrentFrame->tDynamicDescriptorPool, 0));
    PL_VULKAN(vkResetCommandPool(ptVulkanDevice->tLogicalDevice, ptCurrentFrame->tCmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT));
    PL_VULKAN(vkResetCommandPool(ptVulkanDevice->tLogicalDevice, ptVulkanDevice->tCmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT));
    
    pl_end_profile_sample();
    return true; 
}

static void
pl_end_command_recording(plGraphics* ptGraphics, plCommandBuffer* ptCmdBuffer)
{
    VkCommandBuffer tCmdBuffer = (VkCommandBuffer)ptCmdBuffer->_pInternal;
    PL_VULKAN(vkEndCommandBuffer(tCmdBuffer));  
}

static bool
pl_present(plGraphics* ptGraphics, plCommandBuffer* ptCmdBuffer, const plSubmitInfo* ptSubmitInfo)
{
    pl_begin_profile_sample(__FUNCTION__);
    plIO* ptIOCtx = gptIO->get_io();

    VkCommandBuffer tCmdBuffer = (VkCommandBuffer)ptCmdBuffer->_pInternal;

    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptGraphics);
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;
    plVulkanSwapchain* ptVulkanSwapchain = ptGraphics->tSwapchain._pInternalData;
    ptVulkanGfx->bWithinFrameContext = false;

    // submit
    VkPipelineStageFlags atWaitStages[PL_MAX_SEMAPHORES + 1] = { 0 };
    atWaitStages[0] = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    VkCommandBuffer atCmdBuffers[] = {tCmdBuffer};
    VkSubmitInfo tSubmitInfo = {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = &ptCurrentFrame->tImageAvailable,
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

    VkSemaphore atWaitSemaphores[PL_MAX_SEMAPHORES + 1] = {0};
    VkSemaphore atSignalSemaphores[PL_MAX_SEMAPHORES + 1] = {0};

    if(ptCmdBuffer->tBeginInfo.uWaitSemaphoreCount != UINT32_MAX)
    {
        for(uint32_t i = 0; i < ptCmdBuffer->tBeginInfo.uWaitSemaphoreCount; i++)
        {
            atWaitSemaphores[i]  = ptVulkanGfx->sbtSemaphoresHot[ptCmdBuffer->tBeginInfo.atWaitSempahores[i].uIndex];
            atWaitStages[i] = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        }
        atWaitSemaphores[ptCmdBuffer->tBeginInfo.uWaitSemaphoreCount] = ptCurrentFrame->tImageAvailable;
        atWaitStages[ptCmdBuffer->tBeginInfo.uWaitSemaphoreCount] = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;

        tTimelineInfo.waitSemaphoreValueCount = ptCmdBuffer->tBeginInfo.uWaitSemaphoreCount + 1;
        tTimelineInfo.pWaitSemaphoreValues = ptCmdBuffer->tBeginInfo.auWaitSemaphoreValues;

        tSubmitInfo.pNext = &tTimelineInfo;
        tSubmitInfo.pWaitSemaphores = atWaitSemaphores;
        tSubmitInfo.waitSemaphoreCount = ptCmdBuffer->tBeginInfo.uWaitSemaphoreCount + 1;
    }

    if(ptSubmitInfo)
    {
        for(uint32_t i = 0; i < ptSubmitInfo->uSignalSemaphoreCount; i++)
        {
            atSignalSemaphores[i]  = ptVulkanGfx->sbtSemaphoresHot[ptSubmitInfo->atSignalSempahores[i].uIndex];
        }
        atSignalSemaphores[ptSubmitInfo->uSignalSemaphoreCount] = ptCurrentFrame->tRenderFinish;

        tTimelineInfo.signalSemaphoreValueCount = ptSubmitInfo->uSignalSemaphoreCount + 1;
        tTimelineInfo.pSignalSemaphoreValues = ptSubmitInfo->auSignalSemaphoreValues;

        tSubmitInfo.pNext = &tTimelineInfo;
        tSubmitInfo.pSignalSemaphores = atSignalSemaphores;
        tSubmitInfo.signalSemaphoreCount = ptSubmitInfo->uSignalSemaphoreCount + 1;
    }

    PL_VULKAN(vkResetFences(ptVulkanDevice->tLogicalDevice, 1, &ptCurrentFrame->tInFlight));
    PL_VULKAN(vkQueueSubmit(ptVulkanDevice->tGraphicsQueue, 1, &tSubmitInfo, ptCurrentFrame->tInFlight)); 
                       
    const VkPresentInfoKHR tPresentInfo = {
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &ptCurrentFrame->tRenderFinish,
        .swapchainCount     = 1,
        .pSwapchains        = &ptVulkanSwapchain->tSwapChain,
        .pImageIndices      = &ptGraphics->tSwapchain.uCurrentImageIndex,
    };
    const VkResult tResult = vkQueuePresentKHR(ptVulkanDevice->tPresentQueue, &tPresentInfo);
    if(tResult == VK_SUBOPTIMAL_KHR || tResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        pl__create_swapchain(ptGraphics, (uint32_t)ptIOCtx->afMainViewportSize[0], (uint32_t)ptIOCtx->afMainViewportSize[1]);
        pl_end_profile_sample();
        return false;
    }
    else
    {
        PL_VULKAN(tResult);
    }
    ptGraphics->uCurrentFrameIndex = (ptGraphics->uCurrentFrameIndex + 1) % ptGraphics->uFramesInFlight;
    pl_sb_push(ptCurrentFrame->sbtPendingCommandBuffers, tCmdBuffer);
    pl_end_profile_sample();
    return true;
}

static void
pl_resize(plGraphics* ptGraphics)
{
    pl_begin_profile_sample(__FUNCTION__);
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;
    plIO* ptIOCtx = gptIO->get_io();

    pl__create_swapchain(ptGraphics, (uint32_t)ptIOCtx->afMainViewportSize[0], (uint32_t)ptIOCtx->afMainViewportSize[1]);

    plRenderPass* ptRenderPass = &ptGraphics->sbtRenderPassesCold[ptGraphics->tMainRenderPass.uIndex];
    plVulkanRenderPass* ptVulkanRenderPass = &ptVulkanGfx->sbtRenderPassesHot[ptGraphics->tMainRenderPass.uIndex];
    plFrameContext* ptFrame = pl__get_frame_resources(ptGraphics);
    ptRenderPass->tDesc.tDimensions.x = ptIOCtx->afMainViewportSize[0];
    ptRenderPass->tDesc.tDimensions.y = ptIOCtx->afMainViewportSize[1];

    for(uint32_t i = 0; i < ptGraphics->tSwapchain.uImageCount; i++)
    {

        VkFramebufferCreateInfo tFrameBufferInfo = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = ptVulkanRenderPass->tRenderPass,
            .attachmentCount = 1,
            .pAttachments    = &ptVulkanGfx->sbtTexturesHot[ptGraphics->tSwapchain.sbtSwapchainTextureViews[i].uIndex].tImageView,
            .width           = (uint32_t)ptIOCtx->afMainViewportSize[0],
            .height          = (uint32_t)ptIOCtx->afMainViewportSize[1],
            .layers          = 1u,
        };
        pl_sb_push(ptFrame->sbtRawFrameBuffers, ptVulkanRenderPass->atFrameBuffers[i]);
        ptVulkanRenderPass->atFrameBuffers[i] = VK_NULL_HANDLE;
        PL_VULKAN(vkCreateFramebuffer(ptVulkanDevice->tLogicalDevice, &tFrameBufferInfo, NULL, &ptVulkanRenderPass->atFrameBuffers[i]));
    }

    pl_end_profile_sample();
}

static void
pl_flush_device(plDevice* ptDevice)
{
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    vkDeviceWaitIdle(ptVulkanDevice->tLogicalDevice);
}

static void
pl_shutdown(plGraphics* ptGraphics)
{
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;
    plVulkanSwapchain* ptVulkanSwapchain = ptGraphics->tSwapchain._pInternalData;
    
    vkDeviceWaitIdle(ptVulkanDevice->tLogicalDevice);

    for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbtTexturesHot); i++)
    {
        if(ptVulkanGfx->sbtTexturesHot[i].tImage && ptVulkanGfx->sbtTexturesHot[i].bOriginalView)
        {
            vkDestroyImage(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtTexturesHot[i].tImage, NULL);
            
        }
        ptVulkanGfx->sbtTexturesHot[i].tImage = VK_NULL_HANDLE;

        if(ptVulkanGfx->sbtTexturesHot[i].tImageView)
        {
            vkDestroyImageView(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtTexturesHot[i].tImageView, NULL);
            ptVulkanGfx->sbtTexturesHot[i].tImageView = VK_NULL_HANDLE;
        }
    }

    for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbtSamplersHot); i++)
    {
        if(ptVulkanGfx->sbtSamplersHot[i])
            vkDestroySampler(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtSamplersHot[i], NULL);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbtBindGroupsHot); i++)
    {
        vkDestroyDescriptorSetLayout(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtBindGroupsHot[i].tDescriptorSetLayout, NULL);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbtBuffersHot); i++)
    {
        if(ptVulkanGfx->sbtBuffersHot[i].tBuffer)
            vkDestroyBuffer(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtBuffersHot[i].tBuffer, NULL);
    }
    for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbtShadersHot); i++)
    {
        plVulkanShader* ptVulkanShader = &ptVulkanGfx->sbtShadersHot[i];
        if(ptVulkanShader->tPipeline)
            vkDestroyPipeline(ptVulkanDevice->tLogicalDevice, ptVulkanShader->tPipeline, NULL);
        if(ptVulkanShader->tPipelineLayout)
            vkDestroyPipelineLayout(ptVulkanDevice->tLogicalDevice, ptVulkanShader->tPipelineLayout, NULL);
        if(ptVulkanShader->tVertexShaderModule)
            vkDestroyShaderModule(ptVulkanDevice->tLogicalDevice, ptVulkanShader->tVertexShaderModule, NULL);
        if(ptVulkanShader->tPixelShaderModule)
            vkDestroyShaderModule(ptVulkanDevice->tLogicalDevice, ptVulkanShader->tPixelShaderModule, NULL);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbtComputeShadersHot); i++)
    {
        plVulkanComputeShader* ptVulkanShader = &ptVulkanGfx->sbtComputeShadersHot[i];
        if(ptVulkanShader->tPipeline)
            vkDestroyPipeline(ptVulkanDevice->tLogicalDevice, ptVulkanShader->tPipeline, NULL);
        if(ptVulkanShader->tPipelineLayout)
            vkDestroyPipelineLayout(ptVulkanDevice->tLogicalDevice, ptVulkanShader->tPipelineLayout, NULL);
        if(ptVulkanShader->tShaderModule)
            vkDestroyShaderModule(ptVulkanDevice->tLogicalDevice, ptVulkanShader->tShaderModule, NULL);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbtBindGroupLayouts); i++)
    {
        if(ptVulkanGfx->sbtBindGroupLayouts[i].tDescriptorSetLayout)
            vkDestroyDescriptorSetLayout(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtBindGroupLayouts[i].tDescriptorSetLayout, NULL);    
    }

    for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbtRenderPassLayoutsHot); i++)
    {
        if(ptVulkanGfx->sbtRenderPassLayoutsHot[i].tRenderPass)
            vkDestroyRenderPass(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtRenderPassLayoutsHot[i].tRenderPass, NULL);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbtSemaphoresHot); i++)
    {
        if(ptVulkanGfx->sbtSemaphoresHot[i])
            vkDestroySemaphore(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtSemaphoresHot[i], NULL);
        ptVulkanGfx->sbtSemaphoresHot[i] = VK_NULL_HANDLE;
    }

    vkDestroyDescriptorSetLayout(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tDynamicDescriptorSetLayout, NULL);
    pl_sb_free(ptVulkanGfx->sbtTexturesHot);
    pl_sb_free(ptVulkanGfx->sbtSamplersHot);
    pl_sb_free(ptVulkanGfx->sbtBindGroupsHot);
    pl_sb_free(ptVulkanGfx->sbtBuffersHot);
    pl_sb_free(ptVulkanGfx->sbtShadersHot);
    pl_sb_free(ptVulkanGfx->sbtComputeShadersHot);
    pl_sb_free(ptVulkanGfx->sbtBindGroupLayouts);
    pl_sb_free(ptVulkanGfx->sbtSemaphoresHot);

    // cleanup per frame resources
    for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbFrames); i++)
    {
        plFrameContext* ptFrame = &ptVulkanGfx->sbFrames[i];
        vkDestroySemaphore(ptVulkanDevice->tLogicalDevice, ptFrame->tImageAvailable, NULL);
        vkDestroySemaphore(ptVulkanDevice->tLogicalDevice, ptFrame->tRenderFinish, NULL);
        vkDestroyFence(ptVulkanDevice->tLogicalDevice, ptFrame->tInFlight, NULL);
        vkDestroyCommandPool(ptVulkanDevice->tLogicalDevice, ptFrame->tCmdPool, NULL);
        vkDestroyDescriptorPool(ptVulkanDevice->tLogicalDevice, ptFrame->tDynamicDescriptorPool, NULL);

        for(uint32_t j = 0; j < pl_sb_size(ptFrame->sbtDynamicBuffers); j++)
        {
            if(ptFrame->sbtDynamicBuffers[j].tMemory.uHandle)
                ptGraphics->tDevice.ptDynamicAllocator->free(ptGraphics->tDevice.ptDynamicAllocator->ptInst, &ptFrame->sbtDynamicBuffers[j].tMemory);
        }
        
        for(uint32_t j = 0; j < pl_sb_size(ptFrame->sbtRawFrameBuffers); j++)
        {
            vkDestroyFramebuffer(ptVulkanDevice->tLogicalDevice, ptFrame->sbtRawFrameBuffers[j], NULL);
            ptFrame->sbtRawFrameBuffers[j] = VK_NULL_HANDLE;
        }

        pl_sb_free(ptFrame->sbtRawFrameBuffers);
        pl_sb_free(ptFrame->sbtDynamicBuffers);
        pl_sb_free(ptFrame->sbtPendingCommandBuffers);
        pl_sb_free(ptFrame->sbtReadyCommandBuffers);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbtRenderPassesHot); i++)
    {
        if(ptVulkanGfx->sbtRenderPassesHot[i].tRenderPass)
            vkDestroyRenderPass(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtRenderPassesHot[i].tRenderPass, NULL);

        for(uint32_t j = 0; j < 3; j++)
        {
            if(ptVulkanGfx->sbtRenderPassesHot[i].atFrameBuffers[j])
                vkDestroyFramebuffer(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtRenderPassesHot[i].atFrameBuffers[j], NULL);
            ptVulkanGfx->sbtRenderPassesHot[i].atFrameBuffers[j] = VK_NULL_HANDLE;
        }
    }

    vkDestroyDescriptorPool(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tDescriptorPool, NULL);

    // destroy command pool
    vkDestroyCommandPool(ptVulkanDevice->tLogicalDevice, ptVulkanDevice->tCmdPool, NULL);

    vkDestroySwapchainKHR(ptVulkanDevice->tLogicalDevice, ptVulkanSwapchain->tSwapChain, NULL);

    // destroy device
    vkDestroyDevice(ptVulkanDevice->tLogicalDevice, NULL);

    if(ptVulkanGfx->tDbgMessenger)
    {
        PFN_vkDestroyDebugUtilsMessengerEXT tFunc = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ptVulkanGfx->tInstance, "vkDestroyDebugUtilsMessengerEXT");
        if (tFunc != NULL)
            tFunc(ptVulkanGfx->tInstance, ptVulkanGfx->tDbgMessenger, NULL);
    }

    // destroy tSurface
    vkDestroySurfaceKHR(ptVulkanGfx->tInstance, ptVulkanGfx->tSurface, NULL);


    // destroy tInstance
    vkDestroyInstance(ptVulkanGfx->tInstance, NULL);

    pl_temp_allocator_free(&ptVulkanGfx->tTempAllocator);
    pl_sb_free(ptVulkanGfx->sbFrames);
    pl_sb_free(ptVulkanGfx->sbtRenderPassesHot);
    pl_sb_free(ptVulkanGfx->sbtBindGroupLayoutFreeIndices);
    pl_sb_free(ptVulkanSwapchain->sbtSurfaceFormats);
    pl_sb_free(ptVulkanSwapchain->sbtImages);
    pl_sb_free(ptVulkanGfx->sbtRenderPassLayoutsHot);
    pl__cleanup_common_graphics(ptGraphics);
}

static plComputeEncoder
pl_begin_compute_pass(plGraphics* ptGraphics, plCommandBuffer* ptCmdBuffer)
{
    VkCommandBuffer tCmdBuffer = (VkCommandBuffer)ptCmdBuffer->_pInternal;
    VkMemoryBarrier tMemoryBarrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT
    };
    vkCmdPipelineBarrier(tCmdBuffer, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &tMemoryBarrier, 0, NULL, 0, NULL);
    
    plComputeEncoder tEncoder = {
        .ptGraphics     = ptGraphics,
        .tCommandBuffer = *ptCmdBuffer
    };
    return tEncoder;
}

static void
pl_end_compute_pass(plComputeEncoder* ptEncoder)
{
    VkCommandBuffer tCmdBuffer = (VkCommandBuffer)ptEncoder->tCommandBuffer._pInternal;
    VkMemoryBarrier tMemoryBarrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT
    };
    vkCmdPipelineBarrier(tCmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &tMemoryBarrier, 0, NULL, 0, NULL);
}

static plBlitEncoder
pl_begin_blit_pass(plGraphics* ptGraphics, plCommandBuffer* ptCmdBuffer)
{
    VkCommandBuffer tCmdBuffer = (VkCommandBuffer)ptCmdBuffer->_pInternal;
    VkMemoryBarrier tMemoryBarrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT
    };
    vkCmdPipelineBarrier(tCmdBuffer, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &tMemoryBarrier, 0, NULL, 0, NULL);
    
    plBlitEncoder tEncoder = {
        .ptGraphics     = ptGraphics,
        .tCommandBuffer = *ptCmdBuffer
    };
    return tEncoder;
}

static void
pl_end_blit_pass(plBlitEncoder* ptEncoder)
{
    VkCommandBuffer tCmdBuffer = (VkCommandBuffer)ptEncoder->tCommandBuffer._pInternal;
    VkMemoryBarrier tMemoryBarrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT
    };
    vkCmdPipelineBarrier(tCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &tMemoryBarrier, 0, NULL, 0, NULL);
}

static void
pl_dispatch(plComputeEncoder* ptEncoder, uint32_t uDispatchCount, const plDispatch* atDispatches)
{
    VkCommandBuffer tCmdBuffer = (VkCommandBuffer)ptEncoder->tCommandBuffer._pInternal;

    for(uint32_t i = 0; i < uDispatchCount; i++)
    {
        const plDispatch* ptDispatch = &atDispatches[i];
        vkCmdDispatch(tCmdBuffer, ptDispatch->uGroupCountX, ptDispatch->uGroupCountY, ptDispatch->uGroupCountZ);
    }
}

static void
pl_bind_compute_bind_groups(plComputeEncoder* ptEncoder, plComputeShaderHandle tHandle, uint32_t uFirst, uint32_t uCount, const plBindGroupHandle* atBindGroups, plDynamicBinding* ptDynamicBinding)
{   
    plGraphics* ptGraphics = ptEncoder->ptGraphics;
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;
    VkCommandBuffer tCmdBuffer = (VkCommandBuffer)ptEncoder->tCommandBuffer._pInternal;

    plVulkanComputeShader* ptShader = &ptVulkanGfx->sbtComputeShadersHot[tHandle.uIndex];

    uint32_t uDynamicBindingCount = 0;
    uint32_t* puOffsets = NULL;
    if(ptDynamicBinding)
    {
        puOffsets = &ptDynamicBinding->uByteOffset;
        uDynamicBindingCount++;
    }

    VkDescriptorSet* atDescriptorSets = pl_temp_allocator_alloc(&ptVulkanGfx->tTempAllocator, sizeof(VkDescriptorSet) * (uCount + uDynamicBindingCount));

    for(uint32_t i = 0; i < uCount; i++)
    {
        plVulkanBindGroup* ptBindGroup = &ptVulkanGfx->sbtBindGroupsHot[atBindGroups[i].uIndex];
        atDescriptorSets[i] = ptBindGroup->tDescriptorSet;
    }

    if(ptDynamicBinding)
    {
        plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptGraphics); 
        atDescriptorSets[uCount] = ptCurrentFrame->sbtDynamicBuffers[ptDynamicBinding->uBufferHandle].tDescriptorSet;
    }

    vkCmdBindDescriptorSets(tCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ptShader->tPipelineLayout, uFirst, uCount + uDynamicBindingCount, atDescriptorSets, uDynamicBindingCount, puOffsets);
    pl_temp_allocator_reset(&ptVulkanGfx->tTempAllocator);
}

static void
pl_bind_graphics_bind_groups(plRenderEncoder* ptEncoder, plShaderHandle tHandle, uint32_t uFirst, uint32_t uCount, const plBindGroupHandle* atBindGroups, plDynamicBinding* ptDynamicBinding)
{
    plGraphics* ptGraphics = ptEncoder->ptGraphics;
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;
    VkCommandBuffer tCmdBuffer = (VkCommandBuffer)ptEncoder->tCommandBuffer._pInternal;

    plVulkanShader* ptShader = &ptVulkanGfx->sbtShadersHot[tHandle.uIndex];

    uint32_t uDynamicBindingCount = 0;
    uint32_t* puOffsets = NULL;
    if(ptDynamicBinding)
    {
        puOffsets = &ptDynamicBinding->uByteOffset;
        uDynamicBindingCount++;
    }

    VkDescriptorSet* atDescriptorSets = pl_temp_allocator_alloc(&ptVulkanGfx->tTempAllocator, sizeof(VkDescriptorSet) * (uCount + uDynamicBindingCount));

    for(uint32_t i = 0; i < uCount; i++)
    {
        plVulkanBindGroup* ptBindGroup = &ptVulkanGfx->sbtBindGroupsHot[atBindGroups[i].uIndex];
        atDescriptorSets[i] = ptBindGroup->tDescriptorSet;
    }

    if(ptDynamicBinding)
    {
        plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptGraphics); 
        atDescriptorSets[uCount] = ptCurrentFrame->sbtDynamicBuffers[ptDynamicBinding->uBufferHandle].tDescriptorSet;
    }

    vkCmdBindDescriptorSets(tCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ptShader->tPipelineLayout, uFirst, uCount + uDynamicBindingCount, atDescriptorSets, uDynamicBindingCount, puOffsets);
    pl_temp_allocator_reset(&ptVulkanGfx->tTempAllocator);
}

static void
pl_submit_command_buffer(plGraphics* ptGraphics, plCommandBuffer* ptCmdBuffer, const plSubmitInfo* ptSubmitInfo)
{
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;
    VkCommandBuffer tCmdBuffer = (VkCommandBuffer)ptCmdBuffer->_pInternal;
    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptGraphics);

    VkSemaphore atWaitSemaphores[PL_MAX_SEMAPHORES] = {0};
    VkSemaphore atSignalSemaphores[PL_MAX_SEMAPHORES] = {0};
    VkPipelineStageFlags atWaitStages[PL_MAX_SEMAPHORES] = { 0 };
    atWaitStages[0] = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;

    VkSubmitInfo tSubmitInfo = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1u,
        .pCommandBuffers    = &tCmdBuffer,
    };

    VkTimelineSemaphoreSubmitInfo tTimelineInfo = {
        .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
        .pNext = NULL,
    };

    if(ptCmdBuffer->tBeginInfo.uWaitSemaphoreCount != UINT32_MAX)
    {
        for(uint32_t i = 0; i < ptCmdBuffer->tBeginInfo.uWaitSemaphoreCount; i++)
        {
            atWaitSemaphores[i]  = ptVulkanGfx->sbtSemaphoresHot[ptCmdBuffer->tBeginInfo.atWaitSempahores[i].uIndex];
            atWaitStages[i]  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        
        tTimelineInfo.waitSemaphoreValueCount = ptCmdBuffer->tBeginInfo.uWaitSemaphoreCount;
        tTimelineInfo.pWaitSemaphoreValues = ptCmdBuffer->tBeginInfo.auWaitSemaphoreValues;

        tSubmitInfo.pNext = &tTimelineInfo;
        tSubmitInfo.pWaitSemaphores = atWaitSemaphores;
        tSubmitInfo.pWaitDstStageMask = atWaitStages;
        tSubmitInfo.waitSemaphoreCount = ptCmdBuffer->tBeginInfo.uWaitSemaphoreCount;
    }

    if(ptSubmitInfo)
    {

        for(uint32_t i = 0; i < ptSubmitInfo->uSignalSemaphoreCount; i++)
        {
            atSignalSemaphores[i]  = ptVulkanGfx->sbtSemaphoresHot[ptSubmitInfo->atSignalSempahores[i].uIndex];
        }

        tTimelineInfo.signalSemaphoreValueCount = ptSubmitInfo->uSignalSemaphoreCount;
        tTimelineInfo.pSignalSemaphoreValues = ptSubmitInfo->auSignalSemaphoreValues;

        tSubmitInfo.pNext = &tTimelineInfo;
        tSubmitInfo.pSignalSemaphores = atSignalSemaphores;
        tSubmitInfo.signalSemaphoreCount = ptSubmitInfo->uSignalSemaphoreCount;
    }

    PL_VULKAN(vkQueueSubmit(ptVulkanDevice->tGraphicsQueue, 1, &tSubmitInfo, VK_NULL_HANDLE));
    pl_sb_push(ptCurrentFrame->sbtPendingCommandBuffers, tCmdBuffer);
}

static void
pl_submit_command_buffer_blocking(plGraphics* ptGraphics, plCommandBuffer* ptCmdBuffer, const plSubmitInfo* ptSubmitInfo)
{
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;
    VkCommandBuffer tCmdBuffer = (VkCommandBuffer)ptCmdBuffer->_pInternal;
    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptGraphics);

    VkSemaphore atWaitSemaphores[PL_MAX_SEMAPHORES] = {0};
    VkSemaphore atSignalSemaphores[PL_MAX_SEMAPHORES] = {0};
    VkPipelineStageFlags atWaitStages[PL_MAX_SEMAPHORES] = { 0 };
    atWaitStages[0] = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;

    VkSubmitInfo tSubmitInfo = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1u,
        .pCommandBuffers    = &tCmdBuffer,
    };

    VkTimelineSemaphoreSubmitInfo tTimelineInfo = {
        .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
        .pNext = NULL,
    };

    if(ptCmdBuffer->tBeginInfo.uWaitSemaphoreCount != UINT32_MAX)
    {
        for(uint32_t i = 0; i < ptCmdBuffer->tBeginInfo.uWaitSemaphoreCount; i++)
        {
            atWaitSemaphores[i]  = ptVulkanGfx->sbtSemaphoresHot[ptCmdBuffer->tBeginInfo.atWaitSempahores[i].uIndex];
            atWaitStages[i]  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        
        tTimelineInfo.waitSemaphoreValueCount = ptCmdBuffer->tBeginInfo.uWaitSemaphoreCount;
        tTimelineInfo.pWaitSemaphoreValues = ptCmdBuffer->tBeginInfo.auWaitSemaphoreValues;

        tSubmitInfo.pNext = &tTimelineInfo;
        tSubmitInfo.pWaitSemaphores = atWaitSemaphores;
        tSubmitInfo.pWaitDstStageMask = atWaitStages;
        tSubmitInfo.waitSemaphoreCount = ptCmdBuffer->tBeginInfo.uWaitSemaphoreCount;
    }

    if(ptSubmitInfo)
    {

        for(uint32_t i = 0; i < ptSubmitInfo->uSignalSemaphoreCount; i++)
        {
            atSignalSemaphores[i]  = ptVulkanGfx->sbtSemaphoresHot[ptSubmitInfo->atSignalSempahores[i].uIndex];
        }

        tTimelineInfo.signalSemaphoreValueCount = ptSubmitInfo->uSignalSemaphoreCount;
        tTimelineInfo.pSignalSemaphoreValues = ptSubmitInfo->auSignalSemaphoreValues;

        tSubmitInfo.pNext = &tTimelineInfo;
        tSubmitInfo.pSignalSemaphores = atSignalSemaphores;
        tSubmitInfo.signalSemaphoreCount = ptSubmitInfo->uSignalSemaphoreCount;
    }

    PL_VULKAN(vkQueueSubmit(ptVulkanDevice->tGraphicsQueue, 1, &tSubmitInfo, VK_NULL_HANDLE));
    PL_VULKAN(vkQueueWaitIdle(ptVulkanDevice->tGraphicsQueue));
    pl_sb_push(ptCurrentFrame->sbtPendingCommandBuffers, tCmdBuffer);
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static VkSampleCountFlagBits
pl__get_max_sample_count(plDevice* ptDevice)
{
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;

    VkPhysicalDeviceProperties tPhysicalDeviceProperties = {0};
    vkGetPhysicalDeviceProperties(ptVulkanDevice->tPhysicalDevice, &tPhysicalDeviceProperties);

    VkSampleCountFlags tCounts = tPhysicalDeviceProperties.limits.framebufferColorSampleCounts & tPhysicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (tCounts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
    if (tCounts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
    if (tCounts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
    if (tCounts & VK_SAMPLE_COUNT_8_BIT)  { return VK_SAMPLE_COUNT_8_BIT; }
    if (tCounts & VK_SAMPLE_COUNT_4_BIT)  { return VK_SAMPLE_COUNT_4_BIT; }
    if (tCounts & VK_SAMPLE_COUNT_2_BIT)  { return VK_SAMPLE_COUNT_2_BIT; }
    return VK_SAMPLE_COUNT_1_BIT;    
}

static VkFormat
pl__find_supported_format(plDevice* ptDevice, VkFormatFeatureFlags tFlags, const VkFormat* ptFormats, uint32_t uFormatCount)
{
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;

    for(uint32_t i = 0u; i < uFormatCount; i++)
    {
        VkFormatProperties tProps = {0};
        vkGetPhysicalDeviceFormatProperties(ptVulkanDevice->tPhysicalDevice, ptFormats[i], &tProps);
        if(tProps.optimalTilingFeatures & tFlags)
            return ptFormats[i];
    }

    PL_ASSERT(false && "no supported format found");
    return VK_FORMAT_UNDEFINED;
}

static VkFormat
pl__find_depth_format(plDevice* ptDevice)
{
    const VkFormat atFormats[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };
    return pl__find_supported_format(ptDevice, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, atFormats, 3);
}

static VkFormat
pl__find_depth_stencil_format(plDevice* ptDevice)
{
     const VkFormat atFormats[] = {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };
    return pl__find_supported_format(ptDevice, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, atFormats, 2);   
}

static bool
pl__format_has_stencil(VkFormat tFormat)
{
    switch(tFormat)
    {
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT: return true;
        case VK_FORMAT_D32_SFLOAT:
        default: return false;
    }
}

static void
pl__transition_image_layout(VkCommandBuffer tCommandBuffer, VkImage tImage, VkImageLayout tOldLayout, VkImageLayout tNewLayout, VkImageSubresourceRange tSubresourceRange, VkPipelineStageFlags tSrcStageMask, VkPipelineStageFlags tDstStageMask)
{
    //VkCommandBuffer commandBuffer = mvBeginSingleTimeCommands();
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
    if(tMsgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        printf("error validation layer: %s\n", ptCallbackData->pMessage);
        pl_log_error_to_f(uLogChannel, "error validation layer: %s\n", ptCallbackData->pMessage);
    }

    else if(tMsgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        printf("warn validation layer: %s\n", ptCallbackData->pMessage);
        pl_log_warn_to_f(uLogChannel, "warn validation layer: %s\n", ptCallbackData->pMessage);
    }

    else if(tMsgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    {
        // pl_log_trace_to_f(uLogChannel, "info validation layer: %s\n", ptCallbackData->pMessage);
    }
    else if(tMsgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
    {
        // pl_log_trace_to_f(uLogChannel, "trace validation layer: %s\n", ptCallbackData->pMessage);
    }
    
    return VK_FALSE;
}

static void
pl__create_swapchain(plGraphics* ptGraphics, uint32_t uWidth, uint32_t uHeight)
{
    plVulkanGraphics* ptVulkanGfx    = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;
    plSwapchain* ptSwapchain = &ptGraphics->tSwapchain;
    plVulkanSwapchain* ptVulkanSwapchain = ptGraphics->tSwapchain._pInternalData;

    vkDeviceWaitIdle(ptVulkanDevice->tLogicalDevice);

    // ptSwapchain->tMsaaSamples = (plSampleCount)pl__get_max_sample_count(&ptGraphics->tDevice);

    // query swapchain support

    VkSurfaceCapabilitiesKHR tCapabilities = {0};
    PL_VULKAN(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ptVulkanDevice->tPhysicalDevice, ptVulkanGfx->tSurface, &tCapabilities));

    uint32_t uFormatCount = 0u;
    PL_VULKAN(vkGetPhysicalDeviceSurfaceFormatsKHR(ptVulkanDevice->tPhysicalDevice, ptVulkanGfx->tSurface, &uFormatCount, NULL));
    
    pl_sb_resize(ptVulkanSwapchain->sbtSurfaceFormats, uFormatCount);

    VkBool32 tPresentSupport = false;
    PL_VULKAN(vkGetPhysicalDeviceSurfaceSupportKHR(ptVulkanDevice->tPhysicalDevice, 0, ptVulkanGfx->tSurface, &tPresentSupport));
    PL_ASSERT(uFormatCount > 0);
    PL_VULKAN(vkGetPhysicalDeviceSurfaceFormatsKHR(ptVulkanDevice->tPhysicalDevice, ptVulkanGfx->tSurface, &uFormatCount, ptVulkanSwapchain->sbtSurfaceFormats));

    uint32_t uPresentModeCount = 0u;
    PL_VULKAN(vkGetPhysicalDeviceSurfacePresentModesKHR(ptVulkanDevice->tPhysicalDevice, ptVulkanGfx->tSurface, &uPresentModeCount, NULL));
    PL_ASSERT(uPresentModeCount > 0 && uPresentModeCount < 16);

    VkPresentModeKHR atPresentModes[16] = {0};
    PL_VULKAN(vkGetPhysicalDeviceSurfacePresentModesKHR(ptVulkanDevice->tPhysicalDevice, ptVulkanGfx->tSurface, &uPresentModeCount, atPresentModes));

    // choose swap tSurface Format
    static VkFormat atSurfaceFormatPreference[4] = 
    {
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_B8G8R8A8_SRGB
    };

    bool bPreferenceFound = false;
    VkSurfaceFormatKHR tSurfaceFormat = ptVulkanSwapchain->sbtSurfaceFormats[0];
    ptSwapchain->tFormat = pl__pilotlight_format(tSurfaceFormat.format);

    for(uint32_t i = 0u; i < 4; i++)
    {
        if(bPreferenceFound) break;
        
        for(uint32_t j = 0u; j < uFormatCount; j++)
        {
            if(ptVulkanSwapchain->sbtSurfaceFormats[j].format == atSurfaceFormatPreference[i] && ptVulkanSwapchain->sbtSurfaceFormats[j].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                tSurfaceFormat = ptVulkanSwapchain->sbtSurfaceFormats[j];
                ptSwapchain->tFormat = pl__pilotlight_format(tSurfaceFormat.format);
                bPreferenceFound = true;
                break;
            }
        }
    }
    PL_ASSERT(bPreferenceFound && "no preferred surface format found");

    // chose swap present mode
    VkPresentModeKHR tPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    if(!ptSwapchain->bVSync)
    {
        for(uint32_t i = 0 ; i < uPresentModeCount; i++)
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
    if(tCapabilities.currentExtent.width != UINT32_MAX)
        tExtent = tCapabilities.currentExtent;
    else
    {
        tExtent.width = pl_max(tCapabilities.minImageExtent.width, pl_min(tCapabilities.maxImageExtent.width, uWidth));
        tExtent.height = pl_max(tCapabilities.minImageExtent.height, pl_min(tCapabilities.maxImageExtent.height, uHeight));
    }
    ptSwapchain->tExtent.uWidth = tExtent.width;
    ptSwapchain->tExtent.uHeight = tExtent.height;

    // decide image count
    const uint32_t uOldImageCount = ptSwapchain->uImageCount;
    uint32_t uDesiredMinImageCount = tCapabilities.minImageCount + 1;
    if(tCapabilities.maxImageCount > 0 && uDesiredMinImageCount > tCapabilities.maxImageCount) 
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
        .surface          = ptVulkanGfx->tSurface,
        .minImageCount    = uDesiredMinImageCount,
        .imageFormat      = tSurfaceFormat.format,
        .imageColorSpace  = tSurfaceFormat.colorSpace,
        .imageExtent      = tExtent,
        .imageArrayLayers = 1,
        .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform     = (VkSurfaceTransformFlagBitsKHR)tPreTransform,
        .compositeAlpha   = tCompositeAlpha,
        .presentMode      = tPresentMode,
        .clipped          = VK_TRUE, // setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
        .oldSwapchain     = ptVulkanSwapchain->tSwapChain, // setting oldSwapChain to the saved handle of the previous swapchain aids in resource reuse and makes sure that we can still present already acquired images
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

	// enable transfer source on swap chain images if supported
	if (tCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
		tCreateSwapchainInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	// enable transfer destination on swap chain images if supported
	if (tCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		tCreateSwapchainInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    uint32_t auQueueFamilyIndices[] = { (uint32_t)ptVulkanDevice->iGraphicsQueueFamily, (uint32_t)ptVulkanDevice->iPresentQueueFamily};
    if (ptVulkanDevice->iGraphicsQueueFamily != ptVulkanDevice->iPresentQueueFamily)
    {
        tCreateSwapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        tCreateSwapchainInfo.queueFamilyIndexCount = 2;
        tCreateSwapchainInfo.pQueueFamilyIndices = auQueueFamilyIndices;
    }

    VkSwapchainKHR tOldSwapChain = ptVulkanSwapchain->tSwapChain;

    PL_VULKAN(vkCreateSwapchainKHR(ptVulkanDevice->tLogicalDevice, &tCreateSwapchainInfo, NULL, &ptVulkanSwapchain->tSwapChain));

    // plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptGraphics);
    plFrameContext* ptNextFrame = pl__get_next_frame_resources(ptGraphics);
    if(tOldSwapChain)
    {
        
        for (uint32_t i = 0u; i < uOldImageCount; i++)
        {
            pl_queue_texture_for_deletion(&ptGraphics->tDevice, ptSwapchain->sbtSwapchainTextureViews[i]);
        }
        vkDestroySwapchainKHR(ptVulkanDevice->tLogicalDevice, tOldSwapChain, NULL);
    }

    // get swapchain images

    PL_VULKAN(vkGetSwapchainImagesKHR(ptVulkanDevice->tLogicalDevice, ptVulkanSwapchain->tSwapChain, &ptSwapchain->uImageCount, NULL));
    pl_sb_resize(ptVulkanSwapchain->sbtImages, ptSwapchain->uImageCount);
    pl_sb_resize(ptSwapchain->sbtSwapchainTextureViews, ptSwapchain->uImageCount);

    PL_VULKAN(vkGetSwapchainImagesKHR(ptVulkanDevice->tLogicalDevice, ptVulkanSwapchain->tSwapChain, &ptSwapchain->uImageCount, ptVulkanSwapchain->sbtImages));

    for(uint32_t i = 0; i < ptSwapchain->uImageCount; i++)
    {
        plTextureViewDesc tTextureViewDesc = {
            .tFormat     = ptSwapchain->tFormat,
            .uBaseLayer  = 0,
            .uBaseMip    = 0,
            .uLayerCount = 1,
            .uMips       = 1
        };
        ptSwapchain->sbtSwapchainTextureViews[i] = pl_create_swapchain_texture_view(&ptGraphics->tDevice, &tTextureViewDesc, ptVulkanSwapchain->sbtImages[i], "swapchain texture view");
    }
}

static uint32_t
pl__find_memory_type_(VkPhysicalDeviceMemoryProperties tMemProps, uint32_t uTypeFilter, VkMemoryPropertyFlags tProperties)
{
    uint32_t uMemoryType = 0u;
    for (uint32_t i = 0; i < tMemProps.memoryTypeCount; i++) 
    {
        if ((uTypeFilter & (1 << i)) && (tMemProps.memoryTypes[i].propertyFlags & tProperties) == tProperties) 
        {
            uMemoryType = i;
            break;
        }
    }
    return uMemoryType;    
}

static void
pl_copy_buffer(plBlitEncoder* ptEncoder, plBufferHandle tSource, plBufferHandle tDestination, uint32_t uSourceOffset, uint32_t uDestinationOffset, size_t szSize)
{

    plVulkanGraphics* ptVulkanGraphics = ptEncoder->ptGraphics->_pInternalData;
    VkCommandBuffer tCmdBuffer = (VkCommandBuffer)ptEncoder->tCommandBuffer._pInternal;

    const VkBufferCopy tCopyRegion = {
        .size = szSize,
        .srcOffset = uSourceOffset
    };

    vkCmdCopyBuffer(tCmdBuffer, ptVulkanGraphics->sbtBuffersHot[tSource.uIndex].tBuffer, ptVulkanGraphics->sbtBuffersHot[tDestination.uIndex].tBuffer, 1, &tCopyRegion);

}

static void
pl_copy_buffer_to_texture(plBlitEncoder* ptEncoder, plBufferHandle tBufferHandle, plTextureHandle tTextureHandle, uint32_t uRegionCount, const plBufferImageCopy* ptRegions)
{
    plDevice* ptDevice = &ptEncoder->ptGraphics->tDevice;
    plVulkanDevice*   ptVulkanDevice   = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptEncoder->ptGraphics->_pInternalData;

    VkCommandBuffer tCmdBuffer = (VkCommandBuffer)ptEncoder->tCommandBuffer._pInternal;

    plTexture* ptColdTexture = pl__get_texture(ptDevice, tTextureHandle);
    VkImageSubresourceRange* atSubResourceRanges = pl_temp_allocator_alloc(&ptVulkanGraphics->tTempAllocator, sizeof(VkImageSubresourceRange) * uRegionCount);
    VkBufferImageCopy*       atCopyRegions       = pl_temp_allocator_alloc(&ptVulkanGraphics->tTempAllocator, sizeof(VkBufferImageCopy) * uRegionCount);
    memset(atSubResourceRanges, 0, sizeof(VkImageSubresourceRange) * uRegionCount);
    memset(atCopyRegions, 0, sizeof(VkBufferImageCopy) * uRegionCount);

    for(uint32_t i = 0; i < uRegionCount; i++)
    {
        VkImageLayout tLayout = ptRegions[i].tCurrentImageUsage == 0 ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : pl__vulkan_layout(ptRegions[i].tCurrentImageUsage);
        atSubResourceRanges[i].aspectMask     = ptColdTexture->tDesc.tUsage & PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        atSubResourceRanges[i].baseMipLevel   = ptRegions[i].uMipLevel;
        atSubResourceRanges[i].levelCount     = 1;
        atSubResourceRanges[i].baseArrayLayer = ptRegions[i].uBaseArrayLayer;
        atSubResourceRanges[i].layerCount     = ptRegions[i].uLayerCount;
        pl__transition_image_layout(tCmdBuffer, ptVulkanGraphics->sbtTexturesHot[tTextureHandle.uIndex].tImage, tLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, atSubResourceRanges[i], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        atCopyRegions[i].bufferOffset                    = ptRegions[i].szBufferOffset;
        atCopyRegions[i].bufferRowLength                 = ptRegions[i].uBufferRowLength;
        atCopyRegions[i].bufferImageHeight               = ptRegions[i].uImageHeight;
        atCopyRegions[i].imageSubresource.aspectMask     = atSubResourceRanges[i].aspectMask;
        atCopyRegions[i].imageSubresource.mipLevel       = ptRegions[i].uMipLevel;
        atCopyRegions[i].imageSubresource.baseArrayLayer = ptRegions[i].uBaseArrayLayer;
        atCopyRegions[i].imageSubresource.layerCount     = ptRegions[i].uLayerCount;
        atCopyRegions[i].imageExtent.width = ptRegions[i].tImageExtent.uWidth;
        atCopyRegions[i].imageExtent.height = ptRegions[i].tImageExtent.uHeight;
        atCopyRegions[i].imageExtent.depth = ptRegions[i].tImageExtent.uDepth;
        
    }
    vkCmdCopyBufferToImage(tCmdBuffer, ptVulkanGraphics->sbtBuffersHot[tBufferHandle.uIndex].tBuffer, ptVulkanGraphics->sbtTexturesHot[tTextureHandle.uIndex].tImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, uRegionCount, atCopyRegions);

    for(uint32_t i = 0; i < uRegionCount; i++)
    {
        VkImageLayout tLayout = ptRegions[i].tCurrentImageUsage == 0 ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : pl__vulkan_layout(ptRegions[i].tCurrentImageUsage);
        pl__transition_image_layout(tCmdBuffer, ptVulkanGraphics->sbtTexturesHot[tTextureHandle.uIndex].tImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, tLayout, atSubResourceRanges[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    }
        
    pl_temp_allocator_reset(&ptVulkanGraphics->tTempAllocator);
}

static plFrameContext*
pl__get_frame_resources(plGraphics* ptGraphics)
{
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    return &ptVulkanGfx->sbFrames[ptGraphics->uCurrentFrameIndex];
}

static plFrameContext*
pl__get_next_frame_resources(plGraphics* ptGraphics)
{
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    return &ptVulkanGfx->sbFrames[(ptGraphics->uCurrentFrameIndex + 1) % ptGraphics->uFramesInFlight];
}

static void
pl_set_vulkan_object_name(plDevice* ptDevice, uint64_t uObjectHandle, VkDebugReportObjectTypeEXT tObjectType, const char* pcName)
{
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    const VkDebugMarkerObjectNameInfoEXT tNameInfo = 
    {
        .sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT,
        .objectType = tObjectType,
        .object = uObjectHandle,
        .pObjectName = pcName
    };

    if(ptVulkanDevice->vkDebugMarkerSetObjectName)
        ptVulkanDevice->vkDebugMarkerSetObjectName(ptVulkanDevice->tLogicalDevice, &tNameInfo);
}

static VkFilter
pl__vulkan_filter(plFilter tFilter)
{
    switch(tFilter)
    {
        case PL_FILTER_UNSPECIFIED:
        case PL_FILTER_NEAREST: return VK_FILTER_NEAREST;
        case PL_FILTER_LINEAR:  return VK_FILTER_LINEAR;
    }

    PL_ASSERT(false && "Unsupported filter mode");
    return VK_FILTER_LINEAR;
}

static VkSamplerAddressMode
pl__vulkan_wrap(plWrapMode tWrap)
{
    switch(tWrap)
    {
        case PL_WRAP_MODE_UNSPECIFIED:
        case PL_WRAP_MODE_WRAP:   return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case PL_WRAP_MODE_CLAMP:  return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case PL_WRAP_MODE_MIRROR: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    }

    PL_ASSERT(false && "Unsupported wrap mode");
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

static VkCompareOp
pl__vulkan_compare(plCompareMode tCompare)
{
    switch(tCompare)
    {
        case PL_COMPARE_MODE_UNSPECIFIED:
        case PL_COMPARE_MODE_NEVER:            return VK_COMPARE_OP_NEVER;
        case PL_COMPARE_MODE_LESS:             return VK_COMPARE_OP_LESS;
        case PL_COMPARE_MODE_EQUAL:            return VK_COMPARE_OP_EQUAL;
        case PL_COMPARE_MODE_LESS_OR_EQUAL:    return VK_COMPARE_OP_LESS_OR_EQUAL;
        case PL_COMPARE_MODE_GREATER:          return VK_COMPARE_OP_GREATER;
        case PL_COMPARE_MODE_NOT_EQUAL:        return VK_COMPARE_OP_NOT_EQUAL;
        case PL_COMPARE_MODE_GREATER_OR_EQUAL: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case PL_COMPARE_MODE_ALWAYS:           return VK_COMPARE_OP_ALWAYS;
    }

    PL_ASSERT(false && "Unsupported compare mode");
    return VK_COMPARE_OP_NEVER;
}

static VkFormat
pl__vulkan_format(plFormat tFormat)
{
    switch(tFormat)
    {
        case PL_FORMAT_R32G32B32A32_FLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case PL_FORMAT_R32G32B32_FLOAT:    return VK_FORMAT_R32G32B32_SFLOAT;
        case PL_FORMAT_R8G8B8A8_UNORM:     return VK_FORMAT_R8G8B8A8_UNORM;
        case PL_FORMAT_R32G32_FLOAT:       return VK_FORMAT_R32G32_SFLOAT;
        case PL_FORMAT_R8G8B8A8_SRGB:      return VK_FORMAT_R8G8B8A8_SRGB;
        case PL_FORMAT_B8G8R8A8_SRGB:      return VK_FORMAT_B8G8R8A8_SRGB;
        case PL_FORMAT_B8G8R8A8_UNORM:     return VK_FORMAT_B8G8R8A8_UNORM;
        case PL_FORMAT_D32_FLOAT:          return VK_FORMAT_D32_SFLOAT;
        case PL_FORMAT_R8_UNORM:           return VK_FORMAT_R8_UNORM;
        case PL_FORMAT_R32_UINT:           return VK_FORMAT_R32_UINT;
        case PL_FORMAT_R8G8_UNORM:         return VK_FORMAT_R8G8_UNORM;
        case PL_FORMAT_D32_FLOAT_S8_UINT:  return VK_FORMAT_D32_SFLOAT_S8_UINT;
        case PL_FORMAT_D24_UNORM_S8_UINT:  return VK_FORMAT_D24_UNORM_S8_UINT;
        case PL_FORMAT_D16_UNORM_S8_UINT:  return VK_FORMAT_D16_UNORM_S8_UINT;
    }

    PL_ASSERT(false && "Unsupported format");
    return VK_FORMAT_UNDEFINED;
}

static bool
pl__is_depth_format(plFormat tFormat)
{
    switch(tFormat)
    {
        case PL_FORMAT_D32_FLOAT:
        case PL_FORMAT_D32_FLOAT_S8_UINT:
        case PL_FORMAT_D24_UNORM_S8_UINT:
        case PL_FORMAT_D16_UNORM_S8_UINT: return true;
    }
    return false;
}

static VkImageLayout
pl__vulkan_layout(plTextureUsage tUsage)
{
    switch(tUsage)
    {
        case PL_TEXTURE_USAGE_COLOR_ATTACHMENT:         return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        case PL_TEXTURE_USAGE_PRESENT:                  return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        case PL_TEXTURE_USAGE_INPUT_ATTACHMENT:
        case PL_TEXTURE_USAGE_SAMPLED:                  return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        case PL_TEXTURE_USAGE_STORAGE:                  return VK_IMAGE_LAYOUT_GENERAL;
    }

    PL_ASSERT(false && "Unsupported texture layout");
    return VK_IMAGE_LAYOUT_UNDEFINED;
}

static VkAttachmentLoadOp
pl__vulkan_load_op(plLoadOp tOp)
{
    switch(tOp)
    {
        case PL_LOAD_OP_LOAD:      return VK_ATTACHMENT_LOAD_OP_LOAD;
        case PL_LOAD_OP_CLEAR:     return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case PL_LOAD_OP_DONT_CARE: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }

    PL_ASSERT(false && "Unsupported load op");
    return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

static VkAttachmentStoreOp
pl__vulkan_store_op(plStoreOp tOp)
{
    switch(tOp)
    {
        case PL_STORE_OP_STORE:     return VK_ATTACHMENT_STORE_OP_STORE;
        case PL_STORE_OP_DONT_CARE: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        case PL_STORE_OP_NONE:      return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }

    PL_ASSERT(false && "Unsupported store op");
    return VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

static VkStencilOp
pl__vulkan_stencil_op(plStencilOp tStencilOp)
{
    switch (tStencilOp)
    {
        case PL_STENCIL_OP_KEEP:                return VK_STENCIL_OP_KEEP;
        case PL_STENCIL_OP_ZERO:                return VK_STENCIL_OP_ZERO;
        case PL_STENCIL_OP_REPLACE:             return VK_STENCIL_OP_REPLACE;
        case PL_STENCIL_OP_INCREMENT_AND_CLAMP: return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case PL_STENCIL_OP_DECREMENT_AND_CLAMP: return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case PL_STENCIL_OP_INVERT:              return VK_STENCIL_OP_INVERT;
        case PL_STENCIL_OP_INCREMENT_AND_WRAP:  return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case PL_STENCIL_OP_DECREMENT_AND_WRAP:  return VK_STENCIL_OP_DECREMENT_AND_WRAP;
    }
    PL_ASSERT(false && "Unsupported stencil op");
    return VK_STENCIL_OP_KEEP;
}

static VkBlendFactor
pl__vulkan_blend_factor(plBlendFactor tFactor)
{
    switch (tFactor)
    {
        case PL_BLEND_FACTOR_ZERO:                      return VK_BLEND_FACTOR_ZERO;
        case PL_BLEND_FACTOR_ONE:                       return VK_BLEND_FACTOR_ONE;
        case PL_BLEND_FACTOR_SRC_COLOR:                 return VK_BLEND_FACTOR_SRC_COLOR;
        case PL_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:       return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case PL_BLEND_FACTOR_DST_COLOR:                 return VK_BLEND_FACTOR_DST_COLOR;
        case PL_BLEND_FACTOR_ONE_MINUS_DST_COLOR:       return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case PL_BLEND_FACTOR_SRC_ALPHA:                 return VK_BLEND_FACTOR_SRC_ALPHA;
        case PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:       return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case PL_BLEND_FACTOR_DST_ALPHA:                 return VK_BLEND_FACTOR_DST_ALPHA;
        case PL_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:       return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case PL_BLEND_FACTOR_CONSTANT_COLOR:            return VK_BLEND_FACTOR_CONSTANT_COLOR;
        case PL_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR:  return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        case PL_BLEND_FACTOR_CONSTANT_ALPHA:            return VK_BLEND_FACTOR_CONSTANT_ALPHA;
        case PL_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:  return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
        case PL_BLEND_FACTOR_SRC_ALPHA_SATURATE:        return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
        case PL_BLEND_FACTOR_SRC1_COLOR:                return VK_BLEND_FACTOR_SRC1_COLOR;
        case PL_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR:      return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
        case PL_BLEND_FACTOR_SRC1_ALPHA:                return VK_BLEND_FACTOR_SRC1_ALPHA;
        case PL_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA:      return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
    }
    PL_ASSERT(false && "Unsupported blend factor");
    return VK_BLEND_FACTOR_ZERO;
}

static VkBlendOp
pl__vulkan_blend_op(plBlendOp tOp)
{
    switch (tOp)
    {
        case PL_BLEND_OP_ADD:              return VK_BLEND_OP_ADD;
        case PL_BLEND_OP_SUBTRACT:         return VK_BLEND_OP_SUBTRACT;
        case PL_BLEND_OP_REVERSE_SUBTRACT: return VK_BLEND_OP_REVERSE_SUBTRACT;
        case PL_BLEND_OP_MIN:              return VK_BLEND_OP_MIN;
        case PL_BLEND_OP_MAX:              return VK_BLEND_OP_MAX;
    }
    PL_ASSERT(false && "Unsupported blend op");
    return VK_BLEND_OP_ADD;
}

static VkCullModeFlags
pl__vulkan_cull(plCullMode tFlag)
{
    switch(tFlag)
    {
        case PL_CULL_MODE_CULL_FRONT: return VK_CULL_MODE_FRONT_BIT;
        case PL_CULL_MODE_CULL_BACK:  return VK_CULL_MODE_BACK_BIT;
        case PL_CULL_MODE_NONE:       return VK_CULL_MODE_NONE;
    }

    PL_ASSERT(false && "Unsupported cull mode");
    return VK_CULL_MODE_NONE;
}

static VkShaderStageFlagBits
pl__vulkan_stage_flags(plStageFlags tFlags)
{
    VkShaderStageFlagBits tResult = 0;

    if(tFlags & PL_STAGE_VERTEX)   tResult |= VK_SHADER_STAGE_VERTEX_BIT;
    if(tFlags & PL_STAGE_PIXEL)    tResult |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if(tFlags & PL_STAGE_COMPUTE)  tResult |= VK_SHADER_STAGE_COMPUTE_BIT;

    return tResult;
}

static plFormat
pl__pilotlight_format(VkFormat tFormat)
{
    switch(tFormat)
    {
        case VK_FORMAT_R32G32B32_SFLOAT:   return PL_FORMAT_R32G32B32_FLOAT;
        case VK_FORMAT_R8G8B8A8_UNORM:     return PL_FORMAT_R8G8B8A8_UNORM;
        case VK_FORMAT_R32G32_SFLOAT:      return PL_FORMAT_R32G32_FLOAT;
        case VK_FORMAT_R8G8B8A8_SRGB:      return PL_FORMAT_R8G8B8A8_SRGB;
        case VK_FORMAT_B8G8R8A8_SRGB:      return PL_FORMAT_B8G8R8A8_SRGB;
        case VK_FORMAT_B8G8R8A8_UNORM:     return PL_FORMAT_B8G8R8A8_UNORM;
        case VK_FORMAT_R8_UNORM:           return PL_FORMAT_R8_UNORM;
        case VK_FORMAT_R8G8_UNORM:         return PL_FORMAT_R8G8_UNORM;
        case VK_FORMAT_R32_UINT:           return PL_FORMAT_R32_UINT;
        case VK_FORMAT_D32_SFLOAT:         return PL_FORMAT_D32_FLOAT;
        case VK_FORMAT_D32_SFLOAT_S8_UINT: return PL_FORMAT_D32_FLOAT_S8_UINT;
        case VK_FORMAT_D24_UNORM_S8_UINT:  return PL_FORMAT_D24_UNORM_S8_UINT;
        case VK_FORMAT_D16_UNORM_S8_UINT:  return PL_FORMAT_D16_UNORM_S8_UINT;
        default:
            break;
    }

    PL_ASSERT(false && "Unsupported format");
    return PL_FORMAT_UNKNOWN;
}

//-----------------------------------------------------------------------------
// [SECTION] device memory allocators
//-----------------------------------------------------------------------------

static plDeviceMemoryAllocation
pl_allocate_memory(plDevice* ptDevice, size_t szSize, plMemoryMode tMemoryMode, uint32_t uTypeFilter, const char* pcName)
{
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;

    uint32_t uMemoryType = 0u;
    bool bFound = false;
    VkMemoryPropertyFlags tProperties = 0;
    if(tMemoryMode == PL_MEMORY_GPU_CPU)
        tProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    else if(tMemoryMode == PL_MEMORY_GPU)
        tProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    else if(tMemoryMode == PL_MEMORY_CPU)
        tProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    for (uint32_t i = 0; i < ptVulkanDevice->tMemProps.memoryTypeCount; i++) 
    {
        if ((uTypeFilter & (1 << i)) && (ptVulkanDevice->tMemProps.memoryTypes[i].propertyFlags & tProperties) == tProperties) 
        {
            uMemoryType = i;
            bFound = true;
            break;
        }
    }
    PL_ASSERT(bFound);

    if(pcName == NULL)
    {
        pcName = "unnamed memory block";
    }

    plDeviceMemoryAllocation tBlock = {
        .uHandle = 0,
        .ulSize    = (uint64_t)szSize,
        .ulMemoryType = (uint64_t)uMemoryType,
        .tMemoryMode = tMemoryMode
    };
    const VkMemoryAllocateInfo tAllocInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = szSize,
        .memoryTypeIndex = uMemoryType
    };

    VkDeviceMemory tMemory = VK_NULL_HANDLE;
    VkResult tResult = vkAllocateMemory(ptVulkanDevice->tLogicalDevice, &tAllocInfo, NULL, &tMemory);
    PL_VULKAN(tResult);
    tBlock.uHandle = (uint64_t)tMemory;

    pl_set_vulkan_object_name(ptDevice, tBlock.uHandle, VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT, pcName);

    if(tMemoryMode == PL_MEMORY_GPU)
    {
        ptDevice->ptGraphics->szLocalMemoryInUse += tBlock.ulSize;
    }
    else
    {
        PL_VULKAN(vkMapMemory(ptVulkanDevice->tLogicalDevice, (VkDeviceMemory)tBlock.uHandle, 0, tBlock.ulSize, 0, (void**)&tBlock.pHostMapped));
        ptDevice->ptGraphics->szHostMemoryInUse += tBlock.ulSize;
    }

    return tBlock;
}

static void
pl_free_memory(plDevice* ptDevice, plDeviceMemoryAllocation* ptBlock)
{
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;

    if(ptBlock->tMemoryMode == PL_MEMORY_GPU)
    {
        ptDevice->ptGraphics->szLocalMemoryInUse -= ptBlock->ulSize;
    }
    else
    {
        ptDevice->ptGraphics->szHostMemoryInUse -= ptBlock->ulSize;
    }

    vkFreeMemory(ptVulkanDevice->tLogicalDevice, (VkDeviceMemory)ptBlock->uHandle, NULL);
    ptBlock->uHandle = 0;
    ptBlock->pHostMapped = NULL;
    ptBlock->ulSize = 0;
    ptBlock->tMemoryMode = 0;
    ptBlock->ulMemoryType = 0;
}

static void
pl__garbage_collect(plGraphics* ptGraphics)
{
    pl_begin_profile_sample(__FUNCTION__);
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;
    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptGraphics);

    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptGraphics);

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtTextures); i++)
    {
        const uint32_t iResourceIndex = ptGarbage->sbtTextures[i].uIndex;
        plVulkanTexture* ptVulkanResource = &ptVulkanGfx->sbtTexturesHot[iResourceIndex];
        vkDestroyImageView(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtTexturesHot[iResourceIndex].tImageView, NULL);
        ptVulkanGfx->sbtTexturesHot[iResourceIndex].tImageView = VK_NULL_HANDLE;
        if(ptVulkanGfx->sbtTexturesHot[iResourceIndex].bOriginalView)
        {
            vkDestroyImage(ptVulkanDevice->tLogicalDevice, ptVulkanResource->tImage, NULL);
            ptVulkanResource->tImage = VK_NULL_HANDLE;   
        }
        ptVulkanGfx->sbtTexturesHot[iResourceIndex].bOriginalView = false;
        pl_sb_push(ptGraphics->sbtTextureFreeIndices, iResourceIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtSamplers); i++)
    {
        const uint32_t iResourceIndex = ptGarbage->sbtSamplers[i].uIndex;
        vkDestroySampler(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtSamplersHot[iResourceIndex], NULL);
        ptVulkanGfx->sbtSamplersHot[iResourceIndex] = VK_NULL_HANDLE;
        pl_sb_push(ptGraphics->sbtSamplerFreeIndices, iResourceIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtRenderPasses); i++)
    {
        const uint32_t iResourceIndex = ptGarbage->sbtRenderPasses[i].uIndex;
        plVulkanRenderPass* ptVulkanResource = &ptVulkanGfx->sbtRenderPassesHot[iResourceIndex];
        for(uint32_t j = 0; j < 3; j++)
        {
            if(ptVulkanResource->atFrameBuffers[j])
                vkDestroyFramebuffer(ptVulkanDevice->tLogicalDevice, ptVulkanResource->atFrameBuffers[j], NULL);
            ptVulkanResource->atFrameBuffers[j] = VK_NULL_HANDLE;
        }
        if(ptVulkanResource->tRenderPass)
            vkDestroyRenderPass(ptVulkanDevice->tLogicalDevice, ptVulkanResource->tRenderPass, NULL);
        ptVulkanResource->tRenderPass = VK_NULL_HANDLE;
        pl_sb_push(ptGraphics->sbtRenderPassFreeIndices, iResourceIndex);
        // ptVulkanResource->sbtFrameBuffers = NULL;
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtRenderPassLayouts); i++)
    {
        const uint32_t iResourceIndex = ptGarbage->sbtRenderPassLayouts[i].uIndex;
        plVulkanRenderPassLayout* ptVulkanResource = &ptVulkanGfx->sbtRenderPassLayoutsHot[iResourceIndex];
        vkDestroyRenderPass(ptVulkanDevice->tLogicalDevice, ptVulkanResource->tRenderPass, NULL);
        pl_sb_push(ptGraphics->sbtRenderPassLayoutFreeIndices, iResourceIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtShaders); i++)
    {
        const uint32_t iResourceIndex = ptGarbage->sbtShaders[i].uIndex;
        plShader* ptResource = &ptGraphics->sbtShadersCold[iResourceIndex];
        plVulkanShader* ptVulkanResource = &ptVulkanGfx->sbtShadersHot[iResourceIndex];
        if(ptVulkanResource->tVertexShaderModule)
            vkDestroyShaderModule(ptVulkanDevice->tLogicalDevice, ptVulkanResource->tVertexShaderModule, NULL);
        if(ptVulkanResource->tPixelShaderModule)
            vkDestroyShaderModule(ptVulkanDevice->tLogicalDevice, ptVulkanResource->tPixelShaderModule, NULL);

        ptVulkanResource->tVertexShaderModule = VK_NULL_HANDLE;
        ptVulkanResource->tPixelShaderModule = VK_NULL_HANDLE;

        plVulkanShader* ptVariantVulkanResource = &ptVulkanGfx->sbtShadersHot[iResourceIndex];
        vkDestroyPipelineLayout(ptVulkanDevice->tLogicalDevice, ptVariantVulkanResource->tPipelineLayout, NULL);
        vkDestroyPipeline(ptVulkanDevice->tLogicalDevice, ptVariantVulkanResource->tPipeline, NULL);
        ptVariantVulkanResource->tPipelineLayout = VK_NULL_HANDLE;
        ptVariantVulkanResource->tPipeline = VK_NULL_HANDLE;
        pl_sb_push(ptGraphics->sbtShaderFreeIndices, iResourceIndex);
        for(uint32_t k = 0; k < ptResource->tDescription.uBindGroupLayoutCount; k++)
        {
            plVulkanBindGroupLayout* ptVulkanBindGroupLayout = &ptVulkanGfx->sbtBindGroupLayouts[ptResource->tDescription.atBindGroupLayouts[k].uHandle];
            vkDestroyDescriptorSetLayout(ptVulkanDevice->tLogicalDevice, ptVulkanBindGroupLayout->tDescriptorSetLayout, NULL);   
            ptVulkanBindGroupLayout->tDescriptorSetLayout = VK_NULL_HANDLE;
            pl_sb_push(ptVulkanGfx->sbtBindGroupLayoutFreeIndices, ptResource->tDescription.atBindGroupLayouts[k].uHandle);
        }
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtComputeShaders); i++)
    {
        const uint32_t iResourceIndex = ptGarbage->sbtComputeShaders[i].uIndex;
        plComputeShader* ptResource = &ptGraphics->sbtComputeShadersCold[iResourceIndex];
        plVulkanComputeShader* ptVulkanResource = &ptVulkanGfx->sbtComputeShadersHot[iResourceIndex];
        if(ptVulkanResource->tShaderModule)
            vkDestroyShaderModule(ptVulkanDevice->tLogicalDevice, ptVulkanResource->tShaderModule, NULL);

        ptVulkanResource->tShaderModule = VK_NULL_HANDLE;

        plVulkanComputeShader* ptVariantVulkanResource = &ptVulkanGfx->sbtComputeShadersHot[iResourceIndex];
        vkDestroyPipelineLayout(ptVulkanDevice->tLogicalDevice, ptVariantVulkanResource->tPipelineLayout, NULL);
        vkDestroyPipeline(ptVulkanDevice->tLogicalDevice, ptVariantVulkanResource->tPipeline, NULL);
        ptVariantVulkanResource->tPipelineLayout = VK_NULL_HANDLE;
        ptVariantVulkanResource->tPipeline = VK_NULL_HANDLE;
        pl_sb_push(ptGraphics->sbtComputeShaderFreeIndices, iResourceIndex);

        for(uint32_t k = 0; k < ptResource->tDescription.uBindGroupLayoutCount; k++)
        {
            plVulkanBindGroupLayout* ptVulkanBindGroupLayout = &ptVulkanGfx->sbtBindGroupLayouts[ptResource->tDescription.atBindGroupLayouts[k].uHandle];
            vkDestroyDescriptorSetLayout(ptVulkanDevice->tLogicalDevice, ptVulkanBindGroupLayout->tDescriptorSetLayout, NULL);   
            ptVulkanBindGroupLayout->tDescriptorSetLayout = VK_NULL_HANDLE;
            pl_sb_push(ptVulkanGfx->sbtBindGroupLayoutFreeIndices, ptResource->tDescription.atBindGroupLayouts[k].uHandle);
        }
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtBindGroups); i++)
    {
        const uint32_t iBindGroupIndex = ptGarbage->sbtBindGroups[i].uIndex;
        plVulkanBindGroup* ptVulkanResource = &ptVulkanGfx->sbtBindGroupsHot[iBindGroupIndex];
        ptVulkanResource->tDescriptorSet = VK_NULL_HANDLE;
        vkDestroyDescriptorSetLayout(ptVulkanDevice->tLogicalDevice, ptVulkanResource->tDescriptorSetLayout, NULL);
        ptVulkanResource->tDescriptorSetLayout = VK_NULL_HANDLE;
        pl_sb_push(ptGraphics->sbtBindGroupFreeIndices, iBindGroupIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptCurrentFrame->sbtRawFrameBuffers); i++)
    {
        vkDestroyFramebuffer(ptVulkanDevice->tLogicalDevice, ptCurrentFrame->sbtRawFrameBuffers[i], NULL);
        ptCurrentFrame->sbtRawFrameBuffers[i] = VK_NULL_HANDLE;
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtBuffers); i++)
    {
        const uint32_t iResourceIndex = ptGarbage->sbtBuffers[i].uIndex;
        vkDestroyBuffer(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtBuffersHot[iResourceIndex].tBuffer, NULL);
        ptVulkanGfx->sbtBuffersHot[iResourceIndex].tBuffer = VK_NULL_HANDLE;
        pl_sb_push(ptGraphics->sbtBufferFreeIndices, iResourceIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtMemory); i++)
    {
        plDeviceMemoryAllocation tAllocation = ptGarbage->sbtMemory[i];
        plDeviceMemoryAllocatorI* ptAllocator = tAllocation.ptAllocator;
        if(ptAllocator) // swapchain doesn't have allocator since texture is provided
            ptAllocator->free(ptAllocator->ptInst, &tAllocation);
        else
            pl_free_memory(&ptGraphics->tDevice, &tAllocation);
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
    pl_end_profile_sample();
}

static void
pl_destroy_buffer(plDevice* ptDevice, plBufferHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;

    vkDestroyBuffer(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtBuffersHot[tHandle.uIndex].tBuffer, NULL);
    ptVulkanGfx->sbtBuffersHot[tHandle.uIndex].tBuffer = VK_NULL_HANDLE;
    ptGraphics->sbtBufferGenerations[tHandle.uIndex]++;
    pl_sb_push(ptGraphics->sbtBufferFreeIndices, tHandle.uIndex);

    plBuffer* ptBuffer = &ptGraphics->sbtBuffersCold[tHandle.uIndex];
    if(ptBuffer->tMemoryAllocation.ptAllocator)
        ptBuffer->tMemoryAllocation.ptAllocator->free(ptBuffer->tMemoryAllocation.ptAllocator->ptInst, &ptBuffer->tMemoryAllocation);
    else
        pl_free_memory(ptDevice, &ptBuffer->tMemoryAllocation);
}

static void
pl_destroy_texture(plDevice* ptDevice, plTextureHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;

    plVulkanTexture* ptVulkanResource = &ptVulkanGfx->sbtTexturesHot[tHandle.uIndex];
    vkDestroyImage(ptVulkanDevice->tLogicalDevice, ptVulkanResource->tImage, NULL);
    ptVulkanResource->tImage = VK_NULL_HANDLE;
    pl_sb_push(ptGraphics->sbtTextureFreeIndices, tHandle.uIndex);
    ptGraphics->sbtTextureGenerations[tHandle.uIndex]++;
    
    plTexture* ptTexture = &ptGraphics->sbtTexturesCold[tHandle.uIndex];
    if(ptTexture->_tDrawBindGroup.ulData != UINT64_MAX)
    {
        pl_sb_push(ptGraphics->sbtFreeDrawBindGroups, ptTexture->_tDrawBindGroup);
    }
    if(ptTexture->tMemoryAllocation.ptAllocator)
        ptTexture->tMemoryAllocation.ptAllocator->free(ptTexture->tMemoryAllocation.ptAllocator->ptInst, &ptTexture->tMemoryAllocation);
    else
        pl_free_memory(ptDevice, &ptTexture->tMemoryAllocation);
}

static void
pl_destroy_sampler(plDevice* ptDevice, plSamplerHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;

    vkDestroySampler(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtSamplersHot[tHandle.uIndex], NULL);
    ptVulkanGfx->sbtSamplersHot[tHandle.uIndex] = VK_NULL_HANDLE;
    ptGraphics->sbtSamplerGenerations[tHandle.uIndex]++;
    pl_sb_push(ptGraphics->sbtSamplerFreeIndices, tHandle.uIndex);
}

static void
pl_destroy_bind_group(plDevice* ptDevice, plBindGroupHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    ptGraphics->sbtBindGroupGenerations[tHandle.uIndex]++;

    plVulkanBindGroup* ptVulkanResource = &ptVulkanGfx->sbtBindGroupsHot[tHandle.uIndex];
    ptVulkanResource->tDescriptorSet = VK_NULL_HANDLE;
    vkDestroyDescriptorSetLayout(ptVulkanDevice->tLogicalDevice, ptVulkanResource->tDescriptorSetLayout, NULL);
    ptVulkanResource->tDescriptorSetLayout = VK_NULL_HANDLE;
    pl_sb_push(ptGraphics->sbtBindGroupFreeIndices, tHandle.uIndex);
}

static void
pl_destroy_render_pass(plDevice* ptDevice, plRenderPassHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    ptGraphics->sbtRenderPassGenerations[tHandle.uIndex]++;

    plVulkanRenderPass* ptVulkanResource = &ptVulkanGfx->sbtRenderPassesHot[tHandle.uIndex];
    for(uint32_t j = 0; j < PL_FRAMES_IN_FLIGHT; j++)
    {
        if(ptVulkanResource->atFrameBuffers[j])
            vkDestroyFramebuffer(ptVulkanDevice->tLogicalDevice, ptVulkanResource->atFrameBuffers[j], NULL);
        ptVulkanResource->atFrameBuffers[j] = VK_NULL_HANDLE;
    }
    if(ptVulkanResource->tRenderPass)
        vkDestroyRenderPass(ptVulkanDevice->tLogicalDevice, ptVulkanResource->tRenderPass, NULL);
    ptVulkanResource->tRenderPass = VK_NULL_HANDLE;
    pl_sb_push(ptGraphics->sbtRenderPassFreeIndices, tHandle.uIndex);
}

static void
pl_destroy_render_pass_layout(plDevice* ptDevice, plRenderPassLayoutHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    ptGraphics->sbtRenderPassLayoutGenerations[tHandle.uIndex]++;

    plVulkanRenderPassLayout* ptVulkanResource = &ptVulkanGfx->sbtRenderPassLayoutsHot[tHandle.uIndex];
    vkDestroyRenderPass(ptVulkanDevice->tLogicalDevice, ptVulkanResource->tRenderPass, NULL);
    pl_sb_push(ptGraphics->sbtRenderPassLayoutFreeIndices, tHandle.uIndex);
}

static void
pl_destroy_shader(plDevice* ptDevice, plShaderHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    ptGraphics->sbtShaderGenerations[tHandle.uIndex]++;

    plShader* ptResource = &ptGraphics->sbtShadersCold[tHandle.uIndex];

    plVulkanShader* ptVariantVulkanResource = &ptVulkanGfx->sbtShadersHot[tHandle.uIndex];
    vkDestroyPipelineLayout(ptVulkanDevice->tLogicalDevice, ptVariantVulkanResource->tPipelineLayout, NULL);
    vkDestroyPipeline(ptVulkanDevice->tLogicalDevice, ptVariantVulkanResource->tPipeline, NULL);
    ptVariantVulkanResource->tPipelineLayout = VK_NULL_HANDLE;
    ptVariantVulkanResource->tPipeline = VK_NULL_HANDLE;
    pl_sb_push(ptGraphics->sbtShaderFreeIndices, tHandle.uIndex);
    for(uint32_t k = 0; k < ptResource->tDescription.uBindGroupLayoutCount; k++)
    {
        plVulkanBindGroupLayout* ptVulkanBindGroupLayout = &ptVulkanGfx->sbtBindGroupLayouts[ptResource->tDescription.atBindGroupLayouts[k].uHandle];
        vkDestroyDescriptorSetLayout(ptVulkanDevice->tLogicalDevice, ptVulkanBindGroupLayout->tDescriptorSetLayout, NULL);   
        ptVulkanBindGroupLayout->tDescriptorSetLayout = VK_NULL_HANDLE;
        pl_sb_push(ptVulkanGfx->sbtBindGroupLayoutFreeIndices, ptResource->tDescription.atBindGroupLayouts[k].uHandle);
    }
}

static void
pl_destroy_compute_shader(plDevice* ptDevice, plComputeShaderHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    ptGraphics->sbtComputeShaderGenerations[tHandle.uIndex]++;

    plComputeShader* ptResource = &ptGraphics->sbtComputeShadersCold[tHandle.uIndex];

    plVulkanComputeShader* ptVariantVulkanResource = &ptVulkanGfx->sbtComputeShadersHot[tHandle.uIndex];
    vkDestroyPipelineLayout(ptVulkanDevice->tLogicalDevice, ptVariantVulkanResource->tPipelineLayout, NULL);
    vkDestroyPipeline(ptVulkanDevice->tLogicalDevice, ptVariantVulkanResource->tPipeline, NULL);
    ptVariantVulkanResource->tPipelineLayout = VK_NULL_HANDLE;
    ptVariantVulkanResource->tPipeline = VK_NULL_HANDLE;
    pl_sb_push(ptGraphics->sbtComputeShaderFreeIndices, tHandle.uIndex);

    for(uint32_t k = 0; k < ptResource->tDescription.uBindGroupLayoutCount + 1; k++)
    {
        plVulkanBindGroupLayout* ptVulkanBindGroupLayout = &ptVulkanGfx->sbtBindGroupLayouts[ptResource->tDescription.atBindGroupLayouts[k].uHandle];
        vkDestroyDescriptorSetLayout(ptVulkanDevice->tLogicalDevice, ptVulkanBindGroupLayout->tDescriptorSetLayout, NULL);   
        ptVulkanBindGroupLayout->tDescriptorSetLayout = VK_NULL_HANDLE;
        pl_sb_push(ptVulkanGfx->sbtBindGroupLayoutFreeIndices, ptResource->tDescription.atBindGroupLayouts[k].uHandle);
    }
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

static const plGraphicsI*
pl_load_graphics_api(void)
{
    static const plGraphicsI tApi = {
        .initialize                       = pl_initialize_graphics,
        .resize                           = pl_resize,
        .begin_frame                      = pl_begin_frame,
        .dispatch                         = pl_dispatch,
        .bind_compute_bind_groups         = pl_bind_compute_bind_groups,
        .bind_graphics_bind_groups        = pl_bind_graphics_bind_groups,
        .bind_shader                      = pl_bind_shader,
        .bind_compute_shader              = pl_bind_compute_shader,
        .set_scissor_region               = pl_set_scissor_region,
        .set_viewport                     = pl_set_viewport,
        .bind_vertex_buffer               = pl_bind_vertex_buffer,
        .cleanup                          = pl_shutdown,
        .begin_command_recording          = pl_begin_command_recording,
        .end_command_recording            = pl_end_command_recording,
        .submit_command_buffer            = pl_submit_command_buffer,
        .submit_command_buffer_blocking   = pl_submit_command_buffer_blocking,
        .begin_render_pass                = pl_begin_render_pass,
        .begin_compute_pass               = pl_begin_compute_pass,
        .begin_blit_pass                  = pl_begin_blit_pass,
        .next_subpass                     = pl_next_subpass,
        .end_render_pass                  = pl_end_render_pass,
        .end_compute_pass                 = pl_end_compute_pass,
        .end_blit_pass                    = pl_end_blit_pass,
        .draw_stream                      = pl_draw_stream,
        .draw                             = pl_draw,
        .draw_indexed                     = pl_draw_indexed,
        .present                          = pl_present,
        .copy_buffer_to_texture           = pl_copy_buffer_to_texture,
        .copy_texture_to_buffer           = pl_copy_texture_to_buffer,
        .generate_mipmaps                 = pl_generate_mipmaps,
        .copy_buffer                      = pl_copy_buffer,
        .signal_semaphore                 = pl_signal_semaphore,
        .wait_semaphore                   = pl_wait_semaphore,
        .get_semaphore_value              = pl_get_semaphore_value,
        .reset_draw_stream                = pl_drawstream_reset,
        .add_to_stream                    = pl_drawstream_draw,
        .cleanup_draw_stream              = pl_drawstream_cleanup
    };
    return &tApi;
}

static const plDeviceI*
pl_load_device_api(void)
{
    static const plDeviceI tApi = {
        .create_semaphore                       = pl_create_semaphore,
        .create_buffer                          = pl_create_buffer,
        .create_shader                          = pl_create_shader,
        .create_compute_shader                  = pl_create_compute_shader,
        .create_render_pass_layout              = pl_create_render_pass_layout,
        .create_render_pass                     = pl_create_render_pass,
        .create_texture                         = pl_create_texture,
        .create_texture_view                    = pl_create_texture_view,
        .create_bind_group                      = pl_create_bind_group,
        .create_sampler                         = pl_create_sampler,
        .get_temporary_bind_group               = pl_get_temporary_bind_group,
        .update_bind_group                      = pl_update_bind_group,
        .allocate_dynamic_data                  = pl_allocate_dynamic_data,
        .queue_buffer_for_deletion              = pl_queue_buffer_for_deletion,
        .queue_texture_for_deletion             = pl_queue_texture_for_deletion,
        .queue_bind_group_for_deletion          = pl_queue_bind_group_for_deletion,
        .queue_shader_for_deletion              = pl_queue_shader_for_deletion,
        .queue_compute_shader_for_deletion      = pl_queue_compute_shader_for_deletion,
        .queue_render_pass_for_deletion         = pl_queue_render_pass_for_deletion,
        .queue_render_pass_layout_for_deletion  = pl_queue_render_pass_layout_for_deletion,
        .queue_sampler_for_deletion             = pl_queue_sampler_for_deletion,
        .destroy_buffer                         = pl_destroy_buffer,
        .destroy_texture                        = pl_destroy_texture,
        .destroy_bind_group                     = pl_destroy_bind_group,
        .destroy_shader                         = pl_destroy_shader,
        .destroy_sampler                        = pl_destroy_sampler,
        .destroy_compute_shader                 = pl_destroy_compute_shader,
        .destroy_render_pass                    = pl_destroy_render_pass,
        .destroy_render_pass_layout             = pl_destroy_render_pass_layout,
        .update_render_pass_attachments         = pl_update_render_pass_attachments,
        .get_buffer                             = pl__get_buffer,
        .get_texture                            = pl__get_texture,
        .get_bind_group                         = pl__get_bind_group,
        .get_shader                             = pl__get_shader,
        .allocate_memory                        = pl_allocate_memory,
        .free_memory                            = pl_free_memory,
        .flush_device                           = pl_flush_device,
        .bind_buffer_to_memory                  = pl_bind_buffer_to_memory,
        .bind_texture_to_memory                 = pl_bind_texture_to_memory,
        .get_sampler                            = pl_get_sampler,
        .get_render_pass                        = pl_get_render_pass,
        .get_render_pass_layout                 = pl_get_render_pass_layout
    };
    return &tApi;
}

PL_EXPORT void
pl_load_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plDataRegistryI* ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(ptDataRegistry->get_data(PL_CONTEXT_MEMORY));
    pl_set_profile_context(ptDataRegistry->get_data("profile"));
    pl_set_log_context(ptDataRegistry->get_data("log"));
    gptFile = ptApiRegistry->first(PL_API_FILE);
    gptIO   = ptApiRegistry->first(PL_API_IO);
    if(bReload)
    {
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_GRAPHICS), pl_load_graphics_api());
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_DEVICE), pl_load_device_api());

        // find log channel
        uint32_t uChannelCount = 0;
        plLogChannel* ptChannels = pl_get_log_channels(&uChannelCount);
        for(uint32_t i = 0; i < uChannelCount; i++)
        {
            if(strcmp(ptChannels[i].pcName, "Vulkan") == 0)
            {
                uLogChannel = i;
                break;
            }
        }
    }
    else
    {
        ptApiRegistry->add(PL_API_GRAPHICS, pl_load_graphics_api());
        ptApiRegistry->add(PL_API_DEVICE, pl_load_device_api());
        uLogChannel = pl_add_log_channel("Vulkan", PL_CHANNEL_TYPE_CYCLIC_BUFFER);
    }
}

PL_EXPORT void
pl_unload_ext(plApiRegistryI* ptApiRegistry)
{
    
}