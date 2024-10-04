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

#include "pl.h"
#include "pl_os.h"
#include "pl_profile.h"
#include "pl_log.h"
#include "pl_string.h"
#include "pl_memory.h"
#include "pl_ext.inc"
#include "pl_graphics_internal.h"

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

typedef struct _plCommandBuffer
{
    plBeginCommandInfo tBeginInfo;
    plDevice*          ptDevice;
    VkCommandBuffer    tCmdBuffer;
} plCommandBuffer;

typedef struct _plRenderEncoder
{
    plCommandBufferHandle tCommandBuffer;
    plRenderPassHandle    tRenderPassHandle;
    uint32_t              _uCurrentSubpass;
} plRenderEncoder;

typedef struct _plComputeEncoder
{
    plCommandBufferHandle tCommandBuffer;
} plComputeEncoder;

typedef struct _plBlitEncoder
{
    plCommandBufferHandle tCommandBuffer;
} plBlitEncoder;

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

typedef struct _plDevice
{
    plDeviceInfo              tInfo;
    plDeviceMemoryAllocatorI* ptDynamicAllocator;
    void*                     _pInternalData;
    plFrameGarbage*           sbtGarbage;
    VkPhysicalDeviceMemoryProperties tMemProps;

    // render pass layouts
    plRenderPassLayoutHandle  tMainRenderPassLayout;
    plVulkanRenderPassLayout* sbtRenderPassLayoutsHot;
    plRenderPassLayout*       sbtRenderPassLayoutsCold;
    uint32_t*                 sbtRenderPassLayoutGenerations;
    uint32_t*                 sbtRenderPassLayoutFreeIndices;

    // render passes
    plRenderPassHandle  tMainRenderPass;
    plVulkanRenderPass* sbtRenderPassesHot;
    plRenderPass*       sbtRenderPassesCold;
    uint32_t*           sbtRenderPassGenerations;
    uint32_t*           sbtRenderPassFreeIndices;

    // shaders
    plVulkanShader* sbtShadersHot;
    plShader*       sbtShadersCold;
    uint32_t*       sbtShaderGenerations;
    uint32_t*       sbtShaderFreeIndices;

    // compute shaders
    plVulkanComputeShader* sbtComputeShadersHot;
    plComputeShader*       sbtComputeShadersCold;
    uint32_t*              sbtComputeShaderGenerations;
    uint32_t*              sbtComputeShaderFreeIndices;

    // buffers
    plVulkanBuffer* sbtBuffersHot;
    plBuffer*       sbtBuffersCold;
    uint32_t*       sbtBufferGenerations;
    uint32_t*       sbtBufferFreeIndices;

    // textures
    VkImageView*     sbtTextureViewsHot;
    plVulkanTexture* sbtTexturesHot;
    plTexture*       sbtTexturesCold;
    uint32_t*        sbtTextureGenerations;
    uint32_t*        sbtTextureFreeIndices;

    // samplers
    VkSampler* sbtSamplersHot;
    plSampler* sbtSamplersCold;
    uint32_t*  sbtSamplerGenerations;
    uint32_t*  sbtSamplerFreeIndices;

    // bind groups
    plVulkanBindGroup* sbtBindGroupsHot;
    plBindGroup*       sbtBindGroupsCold;
    uint32_t*          sbtBindGroupGenerations;
    uint32_t*          sbtBindGroupFreeIndices;
    plBindGroupHandle* sbtFreeDrawBindGroups;

    // bind group layouts
    plVulkanBindGroupLayout*  sbtBindGroupLayouts;
    uint32_t*                 sbtBindGroupLayoutFreeIndices;
    VkDescriptorSetLayout     tDynamicDescriptorSetLayout;

    // timeline semaphores
    VkSemaphore* sbtSemaphoresHot;
    uint32_t*    sbtSemaphoreGenerations;
    uint32_t*    sbtSemaphoreFreeIndices;

    // vulkan specifics
    VkDevice                                  tLogicalDevice;
    VkPhysicalDevice                          tPhysicalDevice;
    int                                       iGraphicsQueueFamily;
    int                                       iPresentQueueFamily;
    VkQueue                                   tGraphicsQueue;
    VkQueue                                   tPresentQueue;
    VkCommandPool                             tCmdPool;
    uint32_t                                  uCurrentFrame;
    plFrameContext*                           sbFrames;
    VkDescriptorPool                          tDescriptorPool;

	PFN_vkDebugMarkerSetObjectTagEXT  vkDebugMarkerSetObjectTag;
	PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectName;
	PFN_vkCmdDebugMarkerBeginEXT      vkCmdDebugMarkerBegin;
	PFN_vkCmdDebugMarkerEndEXT        vkCmdDebugMarkerEnd;
	PFN_vkCmdDebugMarkerInsertEXT     vkCmdDebugMarkerInsert;
} plDevice;

typedef struct _plGraphics
{
    uint32_t uCurrentFrameIndex;
    uint32_t uFramesInFlight;
    size_t   szLocalMemoryInUse;
    size_t   szHostMemoryInUse;
    bool     bValidationActive;
    bool     bDebugMessengerActive;

    // command buffers
    plCommandBuffer* sbtCommandBuffers;
    uint32_t*        sbuCommandBuffersFreeIndices;

    // render encoders
    plRenderEncoder* sbtRenderEncoders;
    uint32_t*        sbuRenderEncodersFreeIndices;

    // blit encoders
    plBlitEncoder* sbtBlitEncoders;
    uint32_t*      sbuBlitEncodersFreeIndices;

    // compute encoders
    plComputeEncoder* sbtComputeEncoders;
    uint32_t*         sbuComputeEncodersFreeIndices;
    
    // vulkan specifics
    plTempAllocator          tTempAllocator;
    VkInstance               tInstance;
    VkDebugUtilsMessengerEXT tDbgMessenger;
    bool                     bWithinFrameContext;
} plGraphics;

typedef struct _plSurface
{
    VkSurfaceKHR tSurface;
} plSurface;

typedef struct _plSwapchain
{
    plDevice*        ptDevice;
    plExtent         tExtent;
    plFormat         tFormat;
    uint32_t         uImageCount;
    plTextureHandle* sbtSwapchainTextureViews;
    uint32_t         uCurrentImageIndex; // current image to use within the swap chain
    bool             bVSync;

    // platform specific
    plSurface*          ptSurface;
    VkSwapchainKHR      tSwapChain;
    VkImage*            sbtImages;
    VkSurfaceFormatKHR* sbtSurfaceFormats;
} plSwapchain;

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
static plFrameContext*       pl__get_next_frame_resources(plDevice* ptDevice);
static VkSampleCountFlagBits pl__get_max_sample_count(plDevice* ptDevice);
static VkFormat              pl__find_supported_format(plDevice* ptDevice, VkFormatFeatureFlags tFlags, const VkFormat* ptFormats, uint32_t uFormatCount);
static VkFormat              pl__find_depth_format(plDevice* ptDevice);
static VkFormat              pl__find_depth_stencil_format(plDevice* ptDevice);
static bool                  pl__format_has_stencil(VkFormat tFormat);
static void                  pl__transition_image_layout(VkCommandBuffer tCommandBuffer, VkImage tImage, VkImageLayout tOldLayout, VkImageLayout tNewLayout, VkImageSubresourceRange tSubresourceRange, VkPipelineStageFlags tSrcStageMask, VkPipelineStageFlags tDstStageMask);
static void                  pl__create_swapchain(uint32_t uWidth, uint32_t uHeight, plSwapchain*);
static uint32_t              pl__find_memory_type_(VkPhysicalDeviceMemoryProperties tMemProps, uint32_t uTypeFilter, VkMemoryPropertyFlags tProperties);
static void                  pl__garbage_collect(plDevice* ptDevice);
static void                  pl__fill_common_render_pass_data(const plRenderPassLayoutDescription* ptDesc, plRenderPassLayout* ptLayout, plRenderPassCommonData* ptDataOut);

static VKAPI_ATTR VkBool32 VKAPI_CALL pl__debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT tMsgSeverity, VkDebugUtilsMessageTypeFlagsEXT tMsgType, const VkDebugUtilsMessengerCallbackDataEXT* ptCallbackData, void* pUserData);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

static plSemaphoreHandle
pl_create_semaphore(plDevice* ptDevice, bool bHostVisible)
{
    plSemaphoreHandle tHandle = pl__get_new_semaphore_handle(ptDevice);

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
    PL_VULKAN(vkCreateSemaphore(ptDevice->tLogicalDevice, &tCreateInfo, NULL, &tTimelineSemaphore));
    
    ptDevice->sbtSemaphoresHot[tHandle.uIndex] = tTimelineSemaphore;
    return tHandle;
}

static void
pl_signal_semaphore(plDevice* ptDevice, plSemaphoreHandle tHandle, uint64_t ulValue)
{
    const VkSemaphoreSignalInfo tSignalInfo = {
        .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
        .pNext     = NULL,
        .semaphore = ptDevice->sbtSemaphoresHot[tHandle.uIndex],
        .value     = ulValue,
    };
    vkSignalSemaphore(ptDevice->tLogicalDevice, &tSignalInfo);
}

static void
pl_wait_semaphore(plDevice* ptDevice, plSemaphoreHandle tHandle, uint64_t ulValue)
{
    const VkSemaphoreWaitInfo tWaitInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .pNext = NULL,
        .flags = 0,
        .semaphoreCount = 1,
        .pSemaphores = &ptDevice->sbtSemaphoresHot[tHandle.uIndex],
        .pValues = &ulValue,
    };
    vkWaitSemaphores(ptDevice->tLogicalDevice, &tWaitInfo, UINT64_MAX);
}

static uint64_t
pl_get_semaphore_value(plDevice* ptDevice, plSemaphoreHandle tHandle)
{
    uint64_t ulValue = 0;
    vkGetSemaphoreCounterValue(ptDevice->tLogicalDevice, ptDevice->sbtSemaphoresHot[tHandle.uIndex], &ulValue);
    return ulValue;
}

static plBufferHandle
pl_create_buffer(plDevice* ptDevice, const plBufferDescription* ptDesc, const char* pcName)
{
    plBufferHandle tHandle = pl__get_new_buffer_handle(ptDevice);

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

    PL_VULKAN(vkCreateBuffer(ptDevice->tLogicalDevice, &tBufferInfo, NULL, &tVulkanBuffer.tBuffer));
    if(pcName)
        pl_set_vulkan_object_name(ptDevice, (uint64_t)tVulkanBuffer.tBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, pcName);
    vkGetBufferMemoryRequirements(ptDevice->tLogicalDevice, tVulkanBuffer.tBuffer, &tMemRequirements);

    tBuffer.tMemoryRequirements.ulAlignment = tMemRequirements.alignment;
    tBuffer.tMemoryRequirements.ulSize = tMemRequirements.size;
    tBuffer.tMemoryRequirements.uMemoryTypeBits = tMemRequirements.memoryTypeBits;

    ptDevice->sbtBuffersHot[tHandle.uIndex] = tVulkanBuffer;
    ptDevice->sbtBuffersCold[tHandle.uIndex] = tBuffer;
    return tHandle;
}

static void
pl_bind_buffer_to_memory(plDevice* ptDevice, plBufferHandle tHandle, const plDeviceMemoryAllocation* ptAllocation)
{
    plBuffer* ptBuffer = &ptDevice->sbtBuffersCold[tHandle.uIndex];
    ptBuffer->tMemoryAllocation = *ptAllocation;
    plVulkanBuffer* ptVulkanBuffer = &ptDevice->sbtBuffersHot[tHandle.uIndex];

    PL_VULKAN(vkBindBufferMemory(ptDevice->tLogicalDevice, ptVulkanBuffer->tBuffer, (VkDeviceMemory)ptAllocation->uHandle, ptAllocation->ulOffset));
    ptVulkanBuffer->pcData = ptAllocation->pHostMapped;
}

static plDynamicBinding
pl_allocate_dynamic_data(plDevice* ptDevice, size_t szSize)
{
    plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);

    PL_ASSERT(szSize <= ptDevice->tInfo.szDynamicDataMaxSize && "Dynamic data size too large");

    plVulkanDynamicBuffer* ptDynamicBuffer = NULL;

    // first call this frame
    if(ptFrame->uCurrentBufferIndex == UINT32_MAX)
    {
        ptFrame->uCurrentBufferIndex = 0;
        ptFrame->sbtDynamicBuffers[0].uByteOffset = 0;
    }
    ptDynamicBuffer = &ptFrame->sbtDynamicBuffers[ptFrame->uCurrentBufferIndex];
    
    // check if current block has room
    if(ptDynamicBuffer->uByteOffset + szSize > ptDevice->tInfo.szDynamicBufferBlockSize)
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
                .uByteSize            = (uint32_t)ptDevice->tInfo.szDynamicBufferBlockSize
            };
            pl_sprintf(tStagingBufferDescription0.acDebugName, "D-BUF-F%d-%d", (int)gptGraphics->uCurrentFrameIndex, (int)ptFrame->uCurrentBufferIndex);

            plBufferHandle tStagingBuffer0 = pl_create_buffer(ptDevice, &tStagingBufferDescription0, "dynamic buffer");
            plBuffer* ptBuffer = &ptDevice->sbtBuffersCold[tStagingBuffer0.uIndex];
            plDeviceMemoryAllocation tAllocation = ptDevice->ptDynamicAllocator->allocate(ptDevice->ptDynamicAllocator->ptInst, ptBuffer->tMemoryRequirements.uMemoryTypeBits, ptBuffer->tMemoryRequirements.ulSize, ptBuffer->tMemoryRequirements.ulAlignment, "dynamic buffer");
            pl_bind_buffer_to_memory(ptDevice, tStagingBuffer0, &tAllocation);

            ptDynamicBuffer->uHandle = tStagingBuffer0.uIndex;
            ptDynamicBuffer->tBuffer = ptDevice->sbtBuffersHot[tStagingBuffer0.uIndex].tBuffer;
            ptDynamicBuffer->tMemory = ptDevice->sbtBuffersCold[tStagingBuffer0.uIndex].tMemoryAllocation;
            ptDynamicBuffer->uByteOffset = 0;

            // allocate descriptor sets
            const VkDescriptorSetAllocateInfo tDynamicAllocInfo = {
                .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool     = ptDevice->tDescriptorPool,
                .descriptorSetCount = 1,
                .pSetLayouts        = &ptDevice->tDynamicDescriptorSetLayout
            };
            PL_VULKAN(vkAllocateDescriptorSets(ptDevice->tLogicalDevice, &tDynamicAllocInfo, &ptDynamicBuffer->tDescriptorSet));

            VkDescriptorBufferInfo tDescriptorInfo0 = {
                .buffer = ptDynamicBuffer->tBuffer,
                .offset = 0,
                .range  = ptDevice->tInfo.szDynamicDataMaxSize
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

        ptDynamicBuffer = &ptFrame->sbtDynamicBuffers[ptFrame->uCurrentBufferIndex];
        ptDynamicBuffer->uByteOffset = 0;
    }

    plVulkanBuffer* ptBuffer = &ptDevice->sbtBuffersHot[ptDynamicBuffer->uHandle];

    plDynamicBinding tDynamicBinding = {
        .uBufferHandle = ptFrame->uCurrentBufferIndex,
        .uByteOffset   = ptDynamicBuffer->uByteOffset,
        .pcData        = &ptBuffer->pcData[ptDynamicBuffer->uByteOffset]
    };
    ptDynamicBuffer->uByteOffset = (uint32_t)pl_align_up((size_t)ptDynamicBuffer->uByteOffset + ptDevice->tInfo.szDynamicDataMaxSize, ptDevice->tInfo.tLimits.uMinUniformBufferOffsetAlignment);
    return tDynamicBinding;
}

static void
pl_copy_texture_to_buffer(plBlitEncoderHandle tEncoder, plTextureHandle tTextureHandle, plBufferHandle tBufferHandle, uint32_t uRegionCount, const plBufferImageCopy* ptRegions)
{
    plBlitEncoder* ptEncoder = pl__get_blit_encoder(tEncoder);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    plTexture* ptColdTexture = pl__get_texture(ptDevice, tTextureHandle);
    VkImageSubresourceRange* atSubResourceRanges = pl_temp_allocator_alloc(&gptGraphics->tTempAllocator, sizeof(VkImageSubresourceRange) * uRegionCount);
    VkBufferImageCopy*       atCopyRegions       = pl_temp_allocator_alloc(&gptGraphics->tTempAllocator, sizeof(VkBufferImageCopy) * uRegionCount);
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
        pl__transition_image_layout(ptCmdBuffer->tCmdBuffer, ptDevice->sbtTexturesHot[tTextureHandle.uIndex].tImage, tLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, atSubResourceRanges[i], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

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
    vkCmdCopyImageToBuffer(ptCmdBuffer->tCmdBuffer, ptDevice->sbtTexturesHot[tTextureHandle.uIndex].tImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, ptDevice->sbtBuffersHot[tBufferHandle.uIndex].tBuffer, uRegionCount, atCopyRegions);

    for(uint32_t i = 0; i < uRegionCount; i++)
    {
        VkImageLayout tLayout = ptRegions[i].tCurrentImageUsage == 0 ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : pl__vulkan_layout(ptRegions[i].tCurrentImageUsage);
        pl__transition_image_layout(ptCmdBuffer->tCmdBuffer, ptDevice->sbtTexturesHot[tTextureHandle.uIndex].tImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, tLayout, atSubResourceRanges[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    }
        
    pl_temp_allocator_reset(&gptGraphics->tTempAllocator);
}

static void
pl_generate_mipmaps(plBlitEncoderHandle tEncoder, plTextureHandle tTexture)
{
    plBlitEncoder* ptEncoder = pl__get_blit_encoder(tEncoder);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    plTexture* ptTexture = &ptDevice->sbtTexturesCold[tTexture.uIndex];

    // generate mips
    if(ptTexture->tDesc.uMips > 1)
    {

        // check if format supports linear blitting
        VkFormatProperties tFormatProperties = {0};
        vkGetPhysicalDeviceFormatProperties(ptDevice->tPhysicalDevice, pl__vulkan_format(ptTexture->tDesc.tFormat), &tFormatProperties);

        plTexture* ptDestTexture = &ptDevice->sbtTexturesCold[tTexture.uIndex];
        const VkImageSubresourceRange tSubResourceRange = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = ptDestTexture->tDesc.uMips,
            .baseArrayLayer = 0,
            .layerCount     = ptDestTexture->tDesc.uLayers
        };

        pl__transition_image_layout(ptCmdBuffer->tCmdBuffer, ptDevice->sbtTexturesHot[tTexture.uIndex].tImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, tSubResourceRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);


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

                pl__transition_image_layout(ptCmdBuffer->tCmdBuffer, ptDevice->sbtTexturesHot[tTexture.uIndex].tImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, tMipSubResourceRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

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

                vkCmdBlitImage(ptCmdBuffer->tCmdBuffer, ptDevice->sbtTexturesHot[tTexture.uIndex].tImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, ptDevice->sbtTexturesHot[tTexture.uIndex].tImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &tBlit, VK_FILTER_LINEAR);

                pl__transition_image_layout(ptCmdBuffer->tCmdBuffer, ptDevice->sbtTexturesHot[tTexture.uIndex].tImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, tMipSubResourceRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

                if(iMipWidth > 1)  iMipWidth /= 2;
                if(iMipHeight > 1) iMipHeight /= 2;
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

static plTextureHandle
pl_create_swapchain_texture_view(plDevice* ptDevice, const plTextureViewDesc* ptViewDesc, VkImage tImage, const char* pcName)
{
    uint32_t uTextureViewIndex = UINT32_MAX;
    if(pl_sb_size(ptDevice->sbtTextureFreeIndices) > 0)
        uTextureViewIndex = pl_sb_pop(ptDevice->sbtTextureFreeIndices);
    else
    {
        uTextureViewIndex = pl_sb_size(ptDevice->sbtTexturesCold);
        pl_sb_add(ptDevice->sbtTexturesCold);
        pl_sb_push(ptDevice->sbtTextureGenerations, UINT32_MAX);
        pl_sb_add(ptDevice->sbtTexturesHot);
    }

    plTextureHandle tHandle = {
        .uGeneration = ++ptDevice->sbtTextureGenerations[uTextureViewIndex],
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
    PL_VULKAN(vkCreateImageView(ptDevice->tLogicalDevice, &tViewInfo, NULL, &tImageView));

    ptDevice->sbtTexturesHot[uTextureViewIndex].bOriginalView = true;
    ptDevice->sbtTexturesHot[uTextureViewIndex].tImageView = tImageView;
    ptDevice->sbtTexturesCold[uTextureViewIndex] = tTextureView;
    return tHandle;
}

static plSamplerHandle
pl_create_sampler(plDevice* ptDevice, const plSamplerDesc* ptDesc, const char* pcName)
{
    plSamplerHandle tHandle = pl__get_new_sampler_handle(ptDevice);

    plSampler tSampler = {
        .tDesc = *ptDesc,
    };

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~create view~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    VkSamplerCreateInfo tSamplerInfo = {
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter               = pl__vulkan_filter(ptDesc->tFilter),
        .addressModeU            = pl__vulkan_wrap(ptDesc->tHorizontalWrap),
        .addressModeV            = pl__vulkan_wrap(ptDesc->tVerticalWrap),
        .anisotropyEnable        = (bool)(ptDevice->tInfo.tCapabilities & PL_DEVICE_CAPABILITY_SAMPLER_ANISOTROPY),
        .maxAnisotropy           = ptDesc->fMaxAnisotropy == 0 ? (ptDevice->tInfo.tCapabilities & PL_DEVICE_CAPABILITY_SAMPLER_ANISOTROPY) : ptDesc->fMaxAnisotropy,
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
    PL_VULKAN(vkCreateSampler(ptDevice->tLogicalDevice, &tSamplerInfo, NULL, &tVkSampler));

    ptDevice->sbtSamplersHot[tHandle.uIndex] = tVkSampler;
    ptDevice->sbtSamplersCold[tHandle.uIndex] = tSampler;
    return tHandle;
}

static void
pl_create_bind_group_layout(plDevice* ptDevice, plBindGroupLayout* ptLayout, const char* pcName)
{
    plVulkanBindGroupLayout tVulkanBindGroupLayout = {0};

    uint32_t uBindGroupLayoutIndex = UINT32_MAX;
    if(pl_sb_size(ptDevice->sbtBindGroupLayoutFreeIndices) > 0)
        uBindGroupLayoutIndex = pl_sb_pop(ptDevice->sbtBindGroupLayoutFreeIndices);
    else
    {
        uBindGroupLayoutIndex = pl_sb_size(ptDevice->sbtBindGroupLayouts);
        pl_sb_add(ptDevice->sbtBindGroupLayouts);
    }

    ptLayout->uHandle = uBindGroupLayoutIndex;

    uint32_t uCurrentBinding = 0;
    const uint32_t uDescriptorBindingCount = ptLayout->uTextureBindingCount + ptLayout->uBufferBindingCount + ptLayout->uSamplerBindingCount;
    VkDescriptorSetLayoutBinding* atDescriptorSetLayoutBindings = pl_temp_allocator_alloc(&gptGraphics->tTempAllocator, uDescriptorBindingCount * sizeof(VkDescriptorSetLayoutBinding));
    VkDescriptorBindingFlagsEXT* atDescriptorSetLayoutFlags = pl_temp_allocator_alloc(&gptGraphics->tTempAllocator, uDescriptorBindingCount * sizeof(VkDescriptorBindingFlagsEXT));

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
        .pNext = (ptDevice->tInfo.tCapabilities & PL_DEVICE_CAPABILITY_DESCRIPTOR_INDEXING) ? &setLayoutBindingFlags : NULL,
        .flags = (ptDevice->tInfo.tCapabilities & PL_DEVICE_CAPABILITY_DESCRIPTOR_INDEXING) ?  VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT : 0
    };
    PL_VULKAN(vkCreateDescriptorSetLayout(ptDevice->tLogicalDevice, &tDescriptorSetLayoutInfo, NULL, &tVulkanBindGroupLayout.tDescriptorSetLayout));

    if(pcName)
        pl_set_vulkan_object_name(ptDevice, (uint64_t)tVulkanBindGroupLayout.tDescriptorSetLayout, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT, pcName);

    ptDevice->sbtBindGroupLayouts[uBindGroupLayoutIndex] = tVulkanBindGroupLayout;
    pl_temp_allocator_reset(&gptGraphics->tTempAllocator);
}

static plBindGroupHandle
pl_create_bind_group(plDevice* ptDevice, const plBindGroupLayout* ptLayout, const char* pcName)
{
    plBindGroupHandle tHandle = pl__get_new_bind_group_handle(ptDevice);

    plBindGroup tBindGroup = {
        .tLayout = *ptLayout
    };

    uint32_t uCurrentBinding = 0;
    const uint32_t uDescriptorBindingCount = ptLayout->uTextureBindingCount + ptLayout->uBufferBindingCount + ptLayout->uSamplerBindingCount;
    VkDescriptorSetLayoutBinding* atDescriptorSetLayoutBindings = pl_temp_allocator_alloc(&gptGraphics->tTempAllocator, uDescriptorBindingCount * sizeof(VkDescriptorSetLayoutBinding));
    VkDescriptorBindingFlagsEXT* atDescriptorSetLayoutFlags = pl_temp_allocator_alloc(&gptGraphics->tTempAllocator, uDescriptorBindingCount * sizeof(VkDescriptorBindingFlagsEXT));
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
        .pNext = (ptDevice->tInfo.tCapabilities & PL_DEVICE_CAPABILITY_DESCRIPTOR_INDEXING) ? &setLayoutBindingFlags : NULL,
        .flags = (ptDevice->tInfo.tCapabilities & PL_DEVICE_CAPABILITY_DESCRIPTOR_INDEXING) ?  VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT : 0
    };
    VkDescriptorSetLayout tDescriptorSetLayout = VK_NULL_HANDLE;
    PL_VULKAN(vkCreateDescriptorSetLayout(ptDevice->tLogicalDevice, &tDescriptorSetLayoutInfo, NULL, &tDescriptorSetLayout));

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
        .descriptorPool     = ptDevice->tDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts        = &tDescriptorSetLayout,
        .pNext              = bHasVariableDescriptors ? &variableDescriptorCountAllocInfo : NULL
    };

    PL_VULKAN(vkAllocateDescriptorSets(ptDevice->tLogicalDevice, &tAllocInfo, &tVulkanBindGroup.tDescriptorSet));

    pl_temp_allocator_reset(&gptGraphics->tTempAllocator);

    ptDevice->sbtBindGroupsHot[tHandle.uIndex] = tVulkanBindGroup;
    ptDevice->sbtBindGroupsCold[tHandle.uIndex] = tBindGroup;
    return tHandle;
}

static plBindGroupHandle
pl_get_temporary_bind_group(plDevice* ptDevice, const plBindGroupLayout* ptLayout, const char* pcName)
{
    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptDevice);

    plBindGroupHandle tHandle = pl__get_new_bind_group_handle(ptDevice);

    plBindGroup tBindGroup = {
        .tLayout = *ptLayout
    };

    uint32_t uCurrentBinding = 0;
    const uint32_t uDescriptorBindingCount = ptLayout->uTextureBindingCount + ptLayout->uBufferBindingCount + ptLayout->uSamplerBindingCount;
    VkDescriptorSetLayoutBinding* atDescriptorSetLayoutBindings = pl_temp_allocator_alloc(&gptGraphics->tTempAllocator, uDescriptorBindingCount * sizeof(VkDescriptorSetLayoutBinding));
    VkDescriptorBindingFlagsEXT* atDescriptorSetLayoutFlags = pl_temp_allocator_alloc(&gptGraphics->tTempAllocator, uDescriptorBindingCount * sizeof(VkDescriptorBindingFlagsEXT));
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
        .pNext = (ptDevice->tInfo.tCapabilities & PL_DEVICE_CAPABILITY_DESCRIPTOR_INDEXING) ? &setLayoutBindingFlags : NULL,
        .flags = (ptDevice->tInfo.tCapabilities & PL_DEVICE_CAPABILITY_DESCRIPTOR_INDEXING) ?  VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT : 0
    };
    VkDescriptorSetLayout tDescriptorSetLayout = VK_NULL_HANDLE;
    PL_VULKAN(vkCreateDescriptorSetLayout(ptDevice->tLogicalDevice, &tDescriptorSetLayoutInfo, NULL, &tDescriptorSetLayout));

    pl_temp_allocator_reset(&gptGraphics->tTempAllocator);

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

    PL_VULKAN(vkAllocateDescriptorSets(ptDevice->tLogicalDevice, &tAllocInfo, &tVulkanBindGroup.tDescriptorSet));

    ptDevice->sbtBindGroupsHot[tHandle.uIndex] = tVulkanBindGroup;
    ptDevice->sbtBindGroupsCold[tHandle.uIndex] = tBindGroup;
    pl_queue_bind_group_for_deletion(ptDevice, tHandle);
    return tHandle;
}

static void
pl_update_bind_group(plDevice* ptDevice, plBindGroupHandle tHandle, const plBindGroupUpdateData* ptData)
{
    plBindGroup* ptBindGroup = &ptDevice->sbtBindGroupsCold[tHandle.uIndex];
    plVulkanBindGroup* ptVulkanBindGroup = &ptDevice->sbtBindGroupsHot[tHandle.uIndex];

    VkWriteDescriptorSet*   sbtWrites = pl_temp_allocator_alloc(&gptGraphics->tTempAllocator, (ptData->uBufferCount + ptData->uSamplerCount + ptData->uTextureCount) * sizeof(VkWriteDescriptorSet));
    
    VkDescriptorBufferInfo* sbtBufferDescInfos = ptData->uBufferCount > 0 ? pl_temp_allocator_alloc(&gptGraphics->tTempAllocator, ptData->uBufferCount * sizeof(VkDescriptorBufferInfo)) : NULL;
    VkDescriptorImageInfo*  sbtImageDescInfos = ptData->uTextureCount > 0 ? pl_temp_allocator_alloc(&gptGraphics->tTempAllocator, ptData->uTextureCount * sizeof(VkDescriptorImageInfo)) : NULL;
    VkDescriptorImageInfo*  sbtSamplerDescInfos = ptData->uSamplerCount > 0 ? pl_temp_allocator_alloc(&gptGraphics->tTempAllocator, ptData->uSamplerCount  * sizeof(VkDescriptorImageInfo)) : NULL;

    static const VkDescriptorType atDescriptorTypeLUT[] =
    {
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    };

    uint32_t uCurrentWrite = 0;
    for(uint32_t i = 0 ; i < ptData->uBufferCount; i++)
    {

        const plVulkanBuffer* ptVulkanBuffer = &ptDevice->sbtBuffersHot[ptData->atBuffers[i].tBuffer.uIndex];

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
        sbtImageDescInfos[i].imageView           = ptDevice->sbtTexturesHot[ptData->atTextures[i].tTexture.uIndex].tImageView;
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

        sbtSamplerDescInfos[i].sampler           = ptDevice->sbtSamplersHot[ptData->atSamplerBindings[i].tSampler.uIndex];
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

    vkUpdateDescriptorSets(ptDevice->tLogicalDevice, uCurrentWrite, sbtWrites, 0, NULL);
    pl_temp_allocator_reset(&gptGraphics->tTempAllocator);
}

static plTextureHandle
pl_create_texture(plDevice* ptDevice, const plTextureDesc* ptDesc, const char* pcName)
{
    if(pcName == NULL)
        pcName = "unnamed texture";

    plTextureDesc tDesc = *ptDesc;
    strncpy(tDesc.acDebugName, pcName, PL_MAX_NAME_LENGTH);

    if(tDesc.tInitialUsage == PL_TEXTURE_USAGE_UNSPECIFIED)
        tDesc.tInitialUsage = PL_TEXTURE_USAGE_SAMPLED;

    if(tDesc.uMips == 0)
        tDesc.uMips = (uint32_t)floorf(log2f((float)pl_maxi((int)tDesc.tDimensions.x, (int)tDesc.tDimensions.y))) + 1u;

    plTextureHandle tHandle = pl__get_new_texture_handle(ptDevice);

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
    
    PL_VULKAN(vkCreateImage(ptDevice->tLogicalDevice, &tImageInfo, NULL, &tVulkanTexture.tImage));

    // get memory requirements
    VkMemoryRequirements tMemoryRequirements = {0};
    vkGetImageMemoryRequirements(ptDevice->tLogicalDevice, tVulkanTexture.tImage, &tMemoryRequirements);
    tTexture.tMemoryRequirements.ulSize = tMemoryRequirements.size;
    tTexture.tMemoryRequirements.ulAlignment = tMemoryRequirements.alignment;
    tTexture.tMemoryRequirements.uMemoryTypeBits = tMemoryRequirements.memoryTypeBits;

    ptDevice->sbtTexturesHot[tHandle.uIndex] = tVulkanTexture;
    ptDevice->sbtTexturesCold[tHandle.uIndex] = tTexture;
    return tHandle;
}

static void
pl_bind_texture_to_memory(plDevice* ptDevice, plTextureHandle tHandle, const plDeviceMemoryAllocation* ptAllocation)
{
    plTexture* ptTexture = &ptDevice->sbtTexturesCold[tHandle.uIndex];
    ptTexture->tMemoryAllocation = *ptAllocation;
    plVulkanTexture* ptVulkanTexture = &ptDevice->sbtTexturesHot[tHandle.uIndex];

    PL_VULKAN(vkBindImageMemory(ptDevice->tLogicalDevice, ptVulkanTexture->tImage, (VkDeviceMemory)ptAllocation->uHandle, ptAllocation->ulOffset));

    VkImageAspectFlags tImageAspectFlags = ptTexture->tDesc.tUsage & PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    if(pl__format_has_stencil(pl__vulkan_format(ptTexture->tDesc.tFormat)))
        tImageAspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;

    VkCommandBuffer tCommandBuffer = {0};
    
    const VkCommandBufferAllocateInfo tAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool        = ptDevice->tCmdPool,
        .commandBufferCount = 1u,
    };
    vkAllocateCommandBuffers(ptDevice->tLogicalDevice, &tAllocInfo, &tCommandBuffer);

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

    PL_VULKAN(vkQueueSubmit(ptDevice->tGraphicsQueue, 1, &tSubmitInfo, VK_NULL_HANDLE));
    PL_VULKAN(vkDeviceWaitIdle(ptDevice->tLogicalDevice));
    vkFreeCommandBuffers(ptDevice->tLogicalDevice, ptDevice->tCmdPool, 1, &tCommandBuffer);

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
    PL_VULKAN(vkCreateImageView(ptDevice->tLogicalDevice, &tViewInfo, NULL, &ptVulkanTexture->tImageView));

    if(ptTexture->tDesc.tUsage & PL_TEXTURE_USAGE_SAMPLED)
    {
        if(pl_sb_size(ptDevice->sbtFreeDrawBindGroups) == 0)
        {
            const plBindGroupLayout tDrawingBindGroup = {
                .uTextureBindingCount  = 1,
                .atTextureBindings = { 
                    {.uSlot = 0, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
                }
            };
            ptDevice->sbtTexturesCold[tHandle.uIndex]._tDrawBindGroup = pl_create_bind_group(ptDevice, &tDrawingBindGroup, "draw binding");
        }
        else
        {
            ptDevice->sbtTexturesCold[tHandle.uIndex]._tDrawBindGroup = pl_sb_pop(ptDevice->sbtFreeDrawBindGroups);
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
        pl_update_bind_group(ptDevice, ptDevice->sbtTexturesCold[tHandle.uIndex]._tDrawBindGroup, &tBGData);
    }
}

static plTextureHandle
pl_create_texture_view(plDevice* ptDevice, const plTextureViewDesc* ptViewDesc, const char* pcName)
{
    uint32_t uTextureIndex = UINT32_MAX;
    if(pl_sb_size(ptDevice->sbtTextureFreeIndices) > 0)
        uTextureIndex = pl_sb_pop(ptDevice->sbtTextureFreeIndices);
    else
    {
        uTextureIndex = pl_sb_size(ptDevice->sbtTexturesCold);
        pl_sb_add(ptDevice->sbtTexturesCold);
        pl_sb_push(ptDevice->sbtTextureGenerations, UINT32_MAX);
        pl_sb_add(ptDevice->sbtTexturesHot);
    }

    plTextureHandle tHandle = {
        .uGeneration = ++ptDevice->sbtTextureGenerations[uTextureIndex],
        .uIndex = uTextureIndex
    };

    plTexture tTexture = {
        .tDesc = ptDevice->sbtTexturesCold[ptViewDesc->tTexture.uIndex].tDesc,
        .tView = *ptViewDesc,
        ._tDrawBindGroup = {.ulData = UINT64_MAX}
    };

    plTexture* ptTexture = pl__get_texture(ptDevice, ptViewDesc->tTexture);
    plVulkanTexture* ptOldVulkanTexture = &ptDevice->sbtTexturesHot[ptViewDesc->tTexture.uIndex];
    plVulkanTexture* ptNewVulkanTexture = &ptDevice->sbtTexturesHot[tHandle.uIndex];

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
    PL_VULKAN(vkCreateImageView(ptDevice->tLogicalDevice, &tViewInfo, NULL, &ptNewVulkanTexture->tImageView));

    if(ptTexture->tDesc.tUsage & PL_TEXTURE_USAGE_SAMPLED)
    {
        if(pl_sb_size(ptDevice->sbtFreeDrawBindGroups) == 0)
        {
            const plBindGroupLayout tDrawingBindGroup = {
                .uTextureBindingCount  = 1,
                .atTextureBindings = { 
                    {.uSlot = 0, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
                }
            };
            ptDevice->sbtTexturesCold[tHandle.uIndex]._tDrawBindGroup = pl_create_bind_group(ptDevice, &tDrawingBindGroup, "draw binding");
        }
        else
        {
            ptDevice->sbtTexturesCold[tHandle.uIndex]._tDrawBindGroup = pl_sb_pop(ptDevice->sbtFreeDrawBindGroups);
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
        pl_update_bind_group(ptDevice, ptDevice->sbtTexturesCold[tHandle.uIndex]._tDrawBindGroup, &tBGData);
    }

    ptNewVulkanTexture->bOriginalView = false;
    ptDevice->sbtTexturesCold[uTextureIndex] = tTexture;
    return tHandle;
}

static plComputeShaderHandle
pl_create_compute_shader(plDevice* ptDevice, const plComputeShaderDescription* ptDescription)
{

    plComputeShaderHandle tHandle = pl__get_new_compute_shader_handle(ptDevice);
    plComputeShader tShader = {
        .tDescription = *ptDescription
    };

    plVulkanComputeShader* ptVulkanShader = &ptDevice->sbtComputeShadersHot[tHandle.uIndex];

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
        pl_create_bind_group_layout(ptDevice, &tShader.tDescription.atBindGroupLayouts[i], "compute shader template bind group layout");
        ptVulkanShader->atDescriptorSetLayouts[i] = ptDevice->sbtBindGroupLayouts[tShader.tDescription.atBindGroupLayouts[i].uHandle].tDescriptorSetLayout;
    }
    ptVulkanShader->atDescriptorSetLayouts[tShader.tDescription.uBindGroupLayoutCount]  = ptDevice->tDynamicDescriptorSetLayout;

    VkShaderModuleCreateInfo tShaderCreateInfo = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = tShader.tDescription.tShader.szCodeSize,
        .pCode    = (const uint32_t*)tShader.tDescription.tShader.puCode
    };

    PL_VULKAN(vkCreateShaderModule(ptDevice->tLogicalDevice, &tShaderCreateInfo, NULL, &ptVulkanShader->tShaderModule));

    VkPipelineLayoutCreateInfo tPipelineLayoutInfo = {
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = tShader.tDescription.uBindGroupLayoutCount + 1,
        .pSetLayouts    = ptVulkanShader->atDescriptorSetLayouts
    };

    plVulkanComputeShader tVulkanShader = {0};

    const uint32_t uNewResourceIndex = tHandle.uIndex;

    plComputeShaderHandle tVariantHandle = {
        .uGeneration = ++ptDevice->sbtComputeShaderGenerations[uNewResourceIndex],
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

    PL_VULKAN(vkCreatePipelineLayout(ptDevice->tLogicalDevice, &tPipelineLayoutInfo, NULL, &tVulkanShader.tPipelineLayout));

    VkComputePipelineCreateInfo tPipelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .layout = tVulkanShader.tPipelineLayout,
        .stage  = tShaderStage
    };
    PL_VULKAN(vkCreateComputePipelines(ptDevice->tLogicalDevice, VK_NULL_HANDLE, 1, &tPipelineCreateInfo, NULL, &tVulkanShader.tPipeline));

    ptDevice->sbtComputeShadersCold[uNewResourceIndex] = tShader;

    ptVulkanShader->tPipeline = tVulkanShader.tPipeline;
    ptVulkanShader->tPipelineLayout = tVulkanShader.tPipelineLayout;

    ptDevice->sbtComputeShadersCold[tHandle.uIndex] = tShader;
    return tHandle;
}

static plShaderHandle
pl_create_shader(plDevice* ptDevice, const plShaderDescription* ptDescription)
{
    plShaderHandle tHandle = pl__get_new_shader_handle(ptDevice);

    plShader tShader = {
        .tDescription = *ptDescription
    };

    plVulkanShader* ptVulkanShader = &ptDevice->sbtShadersHot[tHandle.uIndex];

    // if(ptDescription->pcPixelShaderEntryFunc == NULL)
        tShader.tDescription.tPixelShader.pcEntryFunc = "main";

    // if(ptDescription->pcVertexShaderEntryFunc == NULL)
        tShader.tDescription.tVertexShader.pcEntryFunc = "main";

    for(uint32_t i = 0; i < tShader.tDescription.uBindGroupLayoutCount; i++)
    {
        pl_create_bind_group_layout(ptDevice, &tShader.tDescription.atBindGroupLayouts[i], "shader template bind group layout");
        ptVulkanShader->atDescriptorSetLayouts[i] = ptDevice->sbtBindGroupLayouts[tShader.tDescription.atBindGroupLayouts[i].uHandle].tDescriptorSetLayout;
    }
    ptVulkanShader->atDescriptorSetLayouts[tShader.tDescription.uBindGroupLayoutCount]  = ptDevice->tDynamicDescriptorSetLayout;

    uint32_t uStageCount = 1;

    VkShaderModuleCreateInfo tVertexShaderCreateInfo = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = tShader.tDescription.tVertexShader.szCodeSize,
        .pCode    = (const uint32_t*)(tShader.tDescription.tVertexShader.puCode)
    };
    PL_VULKAN(vkCreateShaderModule(ptDevice->tLogicalDevice, &tVertexShaderCreateInfo, NULL, &ptVulkanShader->tVertexShaderModule));


    if(tShader.tDescription.tPixelShader.puCode)
    {
        uStageCount++;
        VkShaderModuleCreateInfo tPixelShaderCreateInfo = {
            .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = tShader.tDescription.tPixelShader.szCodeSize,
            .pCode    = (const uint32_t*)(tShader.tDescription.tPixelShader.puCode),
        };

        PL_VULKAN(vkCreateShaderModule(ptDevice->tLogicalDevice, &tPixelShaderCreateInfo, NULL, &ptVulkanShader->tPixelShaderModule));
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

    const plRenderPassLayout* ptRenderPassLayout = &ptDevice->sbtRenderPassLayoutsCold[ptDescription->tRenderPassLayout.uIndex];

    plVulkanShader tVulkanShader = {0};
    const uint32_t uNewResourceIndex = tHandle.uIndex;

    VkPipelineLayoutCreateInfo tPipelineLayoutInfo = {
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = tShader.tDescription.uBindGroupLayoutCount + 1,
        .pSetLayouts    = ptVulkanShader->atDescriptorSetLayouts
    };
    PL_VULKAN(vkCreatePipelineLayout(ptDevice->tLogicalDevice, &tPipelineLayoutInfo, NULL, &tVulkanShader.tPipelineLayout));
    
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
        .sampleShadingEnable  = false,
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
        .renderPass          = ptDevice->sbtRenderPassLayoutsHot[tShader.tDescription.tRenderPassLayout.uIndex].tRenderPass,
        .subpass             = tShader.tDescription.uSubpassIndex,
        .basePipelineHandle  = VK_NULL_HANDLE,
        .pDepthStencilState  = &tDepthStencil
    };

    PL_VULKAN(vkCreateGraphicsPipelines(ptDevice->tLogicalDevice, VK_NULL_HANDLE, 1, &tPipelineInfo, NULL, &tVulkanShader.tPipeline));
    ptDevice->sbtShadersCold[uNewResourceIndex] = tShader;
    ptVulkanShader->tPipeline = tVulkanShader.tPipeline;
    ptVulkanShader->tPipelineLayout = tVulkanShader.tPipelineLayout;

    // no longer need these
    // vkDestroyShaderModule(ptDevice->tLogicalDevice, tVertexShaderModule, NULL);
    // vkDestroyShaderModule(ptDevice->tLogicalDevice, tPixelShaderModule, NULL);
    // tVertexShaderModule = VK_NULL_HANDLE;
    // tPixelShaderModule = VK_NULL_HANDLE;
    ptDevice->sbtShadersCold[tHandle.uIndex] = tShader;
    return tHandle;
}

static void
pl_create_main_render_pass_layout(plSwapchain* ptSwap)
{
    plDevice* ptDevice = ptSwap->ptDevice;
    uint32_t uResourceIndex = UINT32_MAX;
    if(pl_sb_size(ptDevice->sbtRenderPassLayoutFreeIndices) > 0)
        uResourceIndex = pl_sb_pop(ptDevice->sbtRenderPassLayoutFreeIndices);
    else
    {
        uResourceIndex = pl_sb_size(ptDevice->sbtRenderPassLayoutsCold);
        pl_sb_add(ptDevice->sbtRenderPassLayoutsCold);
        pl_sb_push(ptDevice->sbtRenderPassLayoutGenerations, UINT32_MAX);
        pl_sb_add(ptDevice->sbtRenderPassLayoutsHot);
    }

    plRenderPassLayoutHandle tHandle = {
        .uGeneration = ++ptDevice->sbtRenderPassLayoutGenerations[uResourceIndex],
        .uIndex = uResourceIndex
    };

    plRenderPassLayout tLayout = {
        .tDesc = {
            .atRenderTargets = {
                {
                    .tFormat = ptSwap->tFormat,
                }
            },
            .uSubpassCount = 1
        }
    };

    plVulkanRenderPassLayout tVulkanRenderPassLayout = {0};

    const VkAttachmentDescription tAttachment = {
        .flags          = 0,
        .format         = pl__vulkan_format(ptSwap->tFormat),
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

    PL_VULKAN(vkCreateRenderPass(ptDevice->tLogicalDevice, &tRenderPassInfo, NULL, &tVulkanRenderPassLayout.tRenderPass));

    ptDevice->sbtRenderPassLayoutsHot[uResourceIndex] = tVulkanRenderPassLayout;
    ptDevice->sbtRenderPassLayoutsCold[uResourceIndex] = tLayout;
    ptDevice->tMainRenderPassLayout = tHandle;
}

static void
pl_create_main_render_pass(plSwapchain* ptSwap)
{
    plDevice* ptDevice = ptSwap->ptDevice;
    uint32_t uResourceIndex = UINT32_MAX;
    if(pl_sb_size(ptDevice->sbtRenderPassFreeIndices) > 0)
        uResourceIndex = pl_sb_pop(ptDevice->sbtRenderPassFreeIndices);
    else
    {
        uResourceIndex = pl_sb_size(ptDevice->sbtRenderPassesCold);
        pl_sb_add(ptDevice->sbtRenderPassesCold);
        pl_sb_push(ptDevice->sbtRenderPassGenerations, UINT32_MAX);
        pl_sb_add(ptDevice->sbtRenderPassesHot);
    }

    plRenderPassHandle tHandle = {
        .uGeneration = ++ptDevice->sbtRenderPassGenerations[uResourceIndex],
        .uIndex = uResourceIndex
    };

    plRenderPass tRenderPass = {
        .tDesc = {
            .tDimensions = gptIOI->get_io()->tMainViewportSize,
            .tLayout = ptDevice->tMainRenderPassLayout,
            .ptSwapchain = ptSwap
        },
    };

    plRenderPassLayout* ptLayout = &ptDevice->sbtRenderPassLayoutsCold[ptDevice->tMainRenderPassLayout.uIndex];

    plVulkanRenderPass* ptVulkanRenderPass = &ptDevice->sbtRenderPassesHot[uResourceIndex];

    const VkAttachmentDescription tAttachment = {
        .flags          = 0,
        .format         = pl__vulkan_format(ptSwap->tFormat),
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

    PL_VULKAN(vkCreateRenderPass(ptDevice->tLogicalDevice, &tRenderPassInfo, NULL, &ptVulkanRenderPass->tRenderPass));

    for(uint32_t i = 0; i < ptSwap->uImageCount; i++)
    {

        VkFramebufferCreateInfo tFrameBufferInfo = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = ptVulkanRenderPass->tRenderPass,
            .attachmentCount = 1,
            .pAttachments    = &ptDevice->sbtTexturesHot[ptSwap->sbtSwapchainTextureViews[i].uIndex].tImageView,
            .width           = (uint32_t)gptIOI->get_io()->tMainViewportSize.x,
            .height          = (uint32_t)gptIOI->get_io()->tMainViewportSize.y,
            .layers          = 1u,
        };
        PL_VULKAN(vkCreateFramebuffer(ptDevice->tLogicalDevice, &tFrameBufferInfo, NULL, &ptVulkanRenderPass->atFrameBuffers[i]));
    }

    ptDevice->sbtRenderPassesCold[uResourceIndex] = tRenderPass;
    ptDevice->tMainRenderPass = tHandle;
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
    const plRenderPassLayoutHandle tHandle = pl__get_new_render_pass_layout_handle(ptDevice);

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

    PL_VULKAN(vkCreateRenderPass(ptDevice->tLogicalDevice, &tRenderPassInfo, NULL, &tVulkanRenderPassLayout.tRenderPass));

    ptDevice->sbtRenderPassLayoutsHot[tHandle.uIndex] = tVulkanRenderPassLayout;
    ptDevice->sbtRenderPassLayoutsCold[tHandle.uIndex] = tLayout;
    return tHandle;
}

static plRenderPassHandle
pl_create_render_pass(plDevice* ptDevice, const plRenderPassDescription* ptDesc, const plRenderPassAttachments* ptAttachments)
{

    plRenderPassHandle tHandle = pl__get_new_render_pass_handle(ptDevice);

    plRenderPass tRenderPass = {
        .tDesc = *ptDesc
    };

    plRenderPassLayout* ptLayout = &ptDevice->sbtRenderPassLayoutsCold[ptDesc->tLayout.uIndex];

    plRenderPassCommonData tCommonData = {0};
    pl__fill_common_render_pass_data(&ptLayout->tDesc, ptLayout, &tCommonData);

    plVulkanRenderPass* ptVulkanRenderPass = &ptDevice->sbtRenderPassesHot[tHandle.uIndex];

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

    PL_VULKAN(vkCreateRenderPass(ptDevice->tLogicalDevice, &tRenderPassInfo, NULL, &ptVulkanRenderPass->tRenderPass));


    for(uint32_t i = 0; i < gptGraphics->uFramesInFlight; i++)
    {
        VkImageView atViewAttachments[PL_MAX_RENDER_TARGETS] = {0};

        for(uint32_t j = 0; j < ptLayout->_uAttachmentCount; j++)
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
            .layers          = 1u,
        };
        PL_VULKAN(vkCreateFramebuffer(ptDevice->tLogicalDevice, &tFrameBufferInfo, NULL, &ptVulkanRenderPass->atFrameBuffers[i]));
    }

    ptDevice->sbtRenderPassesCold[tHandle.uIndex] = tRenderPass;
    return tHandle;
}

static void
pl_update_render_pass_attachments(plDevice* ptDevice, plRenderPassHandle tHandle, plVec2 tDimensions, const plRenderPassAttachments* ptAttachments)
{
    plRenderPass* ptRenderPass = &ptDevice->sbtRenderPassesCold[tHandle.uIndex];
    
    plVulkanRenderPass* ptVulkanRenderPass = &ptDevice->sbtRenderPassesHot[tHandle.uIndex];
    plRenderPassLayout* ptLayout = &ptDevice->sbtRenderPassLayoutsCold[ptRenderPass->tDesc.tLayout.uIndex];
    plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);
    ptRenderPass->tDesc.tDimensions = tDimensions;

    const plRenderPassDescription* ptDesc = &ptRenderPass->tDesc;

    for(uint32_t i = 0; i < gptGraphics->uFramesInFlight; i++)
    {

        VkImageView atViewAttachments[PL_MAX_RENDER_TARGETS] = {0};

        for(uint32_t j = 0; j < ptLayout->_uAttachmentCount; j++)
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
            .layers          = 1u,
        };
        pl_sb_push(ptFrame->sbtRawFrameBuffers, ptVulkanRenderPass->atFrameBuffers[i]);
        ptVulkanRenderPass->atFrameBuffers[i] = VK_NULL_HANDLE;
        PL_VULKAN(vkCreateFramebuffer(ptDevice->tLogicalDevice, &tFrameBufferInfo, NULL, &ptVulkanRenderPass->atFrameBuffers[i]));
    }
}

static plCommandBufferHandle
pl_begin_command_recording(plDevice* ptDevice, const plBeginCommandInfo* ptBeginInfo)
{
    plCommandBufferHandle tHandle = pl__get_new_command_buffer_handle();
    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptDevice);

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
        PL_VULKAN(vkAllocateCommandBuffers(ptDevice->tLogicalDevice, &tAllocInfo, &tCmdBuffer));  
    }

    const VkCommandBufferBeginInfo tBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = gptGraphics->bWithinFrameContext ? 0 : VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    PL_VULKAN(vkBeginCommandBuffer(tCmdBuffer, &tBeginInfo));   
    
    gptGraphics->sbtCommandBuffers[tHandle.uIndex].tCmdBuffer = tCmdBuffer;
    gptGraphics->sbtCommandBuffers[tHandle.uIndex].ptDevice = ptDevice;

    if(ptBeginInfo)
        gptGraphics->sbtCommandBuffers[tHandle.uIndex].tBeginInfo = *ptBeginInfo;
    else
        gptGraphics->sbtCommandBuffers[tHandle.uIndex].tBeginInfo.uWaitSemaphoreCount = UINT32_MAX;

    return tHandle;
}

static plRenderEncoderHandle
pl_begin_render_pass(plCommandBufferHandle tCmdBuffer, plRenderPassHandle tPass)
{
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(tCmdBuffer);
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    plRenderEncoderHandle tHandle = pl__get_new_render_encoder_handle();

    plRenderPass* ptRenderPass = &ptDevice->sbtRenderPassesCold[tPass.uIndex];
    plVulkanRenderPass* ptVulkanRenderPass = &ptDevice->sbtRenderPassesHot[tPass.uIndex];
    plRenderPassLayout* ptLayout = &ptDevice->sbtRenderPassLayoutsCold[ptRenderPass->tDesc.tLayout.uIndex];
    
    if(ptRenderPass->tDesc.ptSwapchain)
    {
        const VkClearValue atClearValues[2] = {
            { .color.float32 = {0.0f, 0.0f, 0.0f, 1.0f}},
            { .depthStencil.depth = 1.0f},
        };

        VkRenderPassBeginInfo tRenderPassInfo = {
            .sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass        = ptVulkanRenderPass->tRenderPass,
            .framebuffer       = ptVulkanRenderPass->atFrameBuffers[ptRenderPass->tDesc.ptSwapchain->uCurrentImageIndex],
            .renderArea.extent = {
                .width  = (uint32_t)ptRenderPass->tDesc.tDimensions.x,
                .height = (uint32_t)ptRenderPass->tDesc.tDimensions.y
            },
            .clearValueCount   = 2,
            .pClearValues      = atClearValues
        };

        vkCmdBeginRenderPass(gptGraphics->sbtCommandBuffers[tCmdBuffer.uIndex].tCmdBuffer, &tRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
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
            .framebuffer       = ptVulkanRenderPass->atFrameBuffers[gptGraphics->uCurrentFrameIndex],
            .renderArea.extent = {
                .width  = (uint32_t)ptRenderPass->tDesc.tDimensions.x,
                .height = (uint32_t)ptRenderPass->tDesc.tDimensions.y
            },
            .clearValueCount   = uAttachmentCount,
            .pClearValues      = atClearValues
        };

        vkCmdBeginRenderPass(gptGraphics->sbtCommandBuffers[tCmdBuffer.uIndex].tCmdBuffer, &tRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
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

    vkCmdSetViewport(gptGraphics->sbtCommandBuffers[tCmdBuffer.uIndex].tCmdBuffer, 0, 1, &tViewport);
    vkCmdSetScissor(gptGraphics->sbtCommandBuffers[tCmdBuffer.uIndex].tCmdBuffer, 0, 1, &tScissor);
    
    gptGraphics->sbtRenderEncoders[tHandle.uIndex].tCommandBuffer = tCmdBuffer;
    gptGraphics->sbtRenderEncoders[tHandle.uIndex].tRenderPassHandle = tPass;
    gptGraphics->sbtRenderEncoders[tHandle.uIndex]._uCurrentSubpass = 0;
    return tHandle;
}

static void
pl_next_subpass(plRenderEncoderHandle tHandle)
{
    plRenderEncoder* ptEncoder = pl__get_render_encoder(tHandle);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);
    ptEncoder->_uCurrentSubpass++;
    vkCmdNextSubpass(ptCmdBuffer->tCmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
}

static void
pl_end_render_pass(plRenderEncoderHandle tHandle)
{
    plRenderEncoder* ptEncoder = pl__get_render_encoder(tHandle);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    plRenderPass* ptRenderPass = &ptDevice->sbtRenderPassesCold[gptGraphics->sbtRenderEncoders[tHandle.uIndex].tRenderPassHandle.uIndex];
    plRenderPassLayout* ptLayout = &ptDevice->sbtRenderPassLayoutsCold[ptRenderPass->tDesc.tLayout.uIndex];
    
    while(gptGraphics->sbtRenderEncoders[tHandle.uIndex]._uCurrentSubpass < ptLayout->tDesc.uSubpassCount - 1)
    {
        vkCmdNextSubpass(ptCmdBuffer->tCmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
        gptGraphics->sbtRenderEncoders[tHandle.uIndex]._uCurrentSubpass++;
    }
    vkCmdEndRenderPass(ptCmdBuffer->tCmdBuffer);
    pl__return_render_encoder_handle(tHandle);
}

static void
pl_bind_vertex_buffer(plRenderEncoderHandle tEncoder, plBufferHandle tHandle)
{
    plRenderEncoder* ptEncoder = pl__get_render_encoder(tEncoder);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    plVulkanBuffer* ptVertexBuffer = &ptDevice->sbtBuffersHot[tHandle.uIndex];
    static VkDeviceSize offsets = { 0 };
    vkCmdBindVertexBuffers(ptCmdBuffer->tCmdBuffer, 0, 1, &ptVertexBuffer->tBuffer, &offsets);
}

static void
pl_draw(plRenderEncoderHandle tHandle, uint32_t uCount, const plDraw* atDraws)
{
    plRenderEncoder* ptEncoder = pl__get_render_encoder(tHandle);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);
    for(uint32_t i = 0; i < uCount; i++)
        vkCmdDraw(ptCmdBuffer->tCmdBuffer, atDraws[i].uVertexCount, atDraws[i].uInstanceCount, atDraws[i].uVertexStart, atDraws[i].uInstance);
}

static void
pl_draw_indexed(plRenderEncoderHandle tHandle, uint32_t uCount, const plDrawIndex* atDraws)
{
    plRenderEncoder* ptEncoder = pl__get_render_encoder(tHandle);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    uint32_t uCurrentIndexBuffer = UINT32_MAX;

    for(uint32_t i = 0; i < uCount; i++)
    {
        if(atDraws->tIndexBuffer.uIndex != uCurrentIndexBuffer)
        {
            uCurrentIndexBuffer = atDraws->tIndexBuffer.uIndex;
            plVulkanBuffer* ptIndexBuffer = &ptDevice->sbtBuffersHot[uCurrentIndexBuffer];
            vkCmdBindIndexBuffer(ptCmdBuffer->tCmdBuffer, ptIndexBuffer->tBuffer, 0, VK_INDEX_TYPE_UINT32);
        }
        vkCmdDrawIndexed(ptCmdBuffer->tCmdBuffer, atDraws[i].uIndexCount, atDraws[i].uInstanceCount, atDraws[i].uIndexStart, atDraws[i].uVertexStart, atDraws[i].uInstance);
    }
}

static void
pl_bind_shader(plRenderEncoderHandle tEncoder, plShaderHandle tHandle)
{
    plRenderEncoder* ptEncoder = pl__get_render_encoder(tEncoder);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    plVulkanShader* ptVulkanShader = &ptDevice->sbtShadersHot[tHandle.uIndex];
    vkCmdSetDepthBias(ptCmdBuffer->tCmdBuffer, 0.0f, 0.0f, 0.0f);
    vkCmdBindPipeline(ptCmdBuffer->tCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ptVulkanShader->tPipeline);
}

static void
pl_bind_compute_shader(plComputeEncoderHandle tEncoder, plComputeShaderHandle tHandle)
{
    plComputeEncoder* ptEncoder = pl__get_compute_encoder(tEncoder);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    plVulkanComputeShader* ptVulkanShader = &ptDevice->sbtComputeShadersHot[tHandle.uIndex];
    vkCmdBindPipeline(ptCmdBuffer->tCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ptVulkanShader->tPipeline);
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
pl_draw_stream(plRenderEncoderHandle tEncoder, uint32_t uAreaCount, plDrawArea* atAreas)
{
    pl_begin_profile_sample(0, __FUNCTION__);
    plRenderEncoder* ptEncoder = pl__get_render_encoder(tEncoder);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptDevice); 

    static VkDeviceSize offsets = { 0 };
    vkCmdSetDepthBias(ptCmdBuffer->tCmdBuffer, 0.0f, 0.0f, 0.0f);

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

        vkCmdSetViewport(ptCmdBuffer->tCmdBuffer, 0, 1, &tViewport);
        vkCmdSetScissor(ptCmdBuffer->tCmdBuffer, 0, 1, &tScissor);  

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
                const plShader* ptShader= &ptDevice->sbtShadersCold[ptStream->sbtStream[uCurrentStreamIndex]];
                ptVulkanShader = &ptDevice->sbtShadersHot[ptStream->sbtStream[uCurrentStreamIndex]];
                vkCmdBindPipeline(ptCmdBuffer->tCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ptVulkanShader->tPipeline);
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
                plVulkanBindGroup* ptBindGroup0 = &ptDevice->sbtBindGroupsHot[ptStream->sbtStream[uCurrentStreamIndex]];
                pl__set_bind_group(&tBindGroupManagerData, 0, ptBindGroup0->tDescriptorSet);
                uCurrentStreamIndex++;
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_1)
            {
                plVulkanBindGroup* ptBindGroup1 = &ptDevice->sbtBindGroupsHot[ptStream->sbtStream[uCurrentStreamIndex]];
                pl__set_bind_group(&tBindGroupManagerData, 1, ptBindGroup1->tDescriptorSet);
                uCurrentStreamIndex++;
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_2)
            {
                plVulkanBindGroup* ptBindGroup2 = &ptDevice->sbtBindGroupsHot[ptStream->sbtStream[uCurrentStreamIndex]];
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
                    plVulkanBuffer* ptIndexBuffer = &ptDevice->sbtBuffersHot[uIndexBuffer];
                    vkCmdBindIndexBuffer(ptCmdBuffer->tCmdBuffer, ptIndexBuffer->tBuffer, 0, VK_INDEX_TYPE_UINT32);
                }
                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_VERTEX_BUFFER)
            {
                plVulkanBuffer* ptVertexBuffer = &ptDevice->sbtBuffersHot[ptStream->sbtStream[uCurrentStreamIndex]];
                vkCmdBindVertexBuffers(ptCmdBuffer->tCmdBuffer, 0, 1, &ptVertexBuffer->tBuffer, &offsets);
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
            pl__update_bindings(&tBindGroupManagerData, ptCmdBuffer->tCmdBuffer, ptVulkanShader->tPipelineLayout);

            if(uIndexBuffer == UINT32_MAX)
                vkCmdDraw(ptCmdBuffer->tCmdBuffer, uTriangleCount * 3, uInstanceCount, uVertexBufferOffset, uInstanceStart);
            else
                vkCmdDrawIndexed(ptCmdBuffer->tCmdBuffer, uTriangleCount * 3, uInstanceCount, uIndexBufferOffset, uVertexBufferOffset, uInstanceStart);
        }
    }
    pl_end_profile_sample(0);
}

static void
pl_set_viewport(plRenderEncoderHandle tEncoder, const plRenderViewport* ptViewport)
{
    plRenderEncoder* ptEncoder = pl__get_render_encoder(tEncoder);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);

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

static void
pl_set_scissor_region(plRenderEncoderHandle tEncoder, const plScissor* ptScissor)
{
    plRenderEncoder* ptEncoder = pl__get_render_encoder(tEncoder);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);

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

    vkCmdSetScissor(ptCmdBuffer->tCmdBuffer, 0, 1, &tScissor);  
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
    gptGraphics->szHostMemoryInUse += ulSize;
    return tAllocation;
}

static void
pl_free_staging_dynamic(struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation)
{
    plInternalDeviceAllocatorData* ptData = (plInternalDeviceAllocatorData*)ptInst;
    plDeviceMemoryAllocation tBlock = {.uHandle = ptAllocation->uHandle};
    pl_free_memory(ptData->ptDevice, &tBlock);
    gptGraphics->szHostMemoryInUse -= ptAllocation->ulSize;
    ptAllocation->uHandle = 0;
    ptAllocation->ulSize = 0;
    ptAllocation->ulOffset = 0;
}

static bool
pl_initialize_graphics(const plGraphicsInit* ptDesc)
{
    static plGraphics gtGraphics = {0};
    gptGraphics = &gtGraphics;

    // setup logging
    plLogChannelInit tLogInit = {
        .tType       = PL_CHANNEL_TYPE_CYCLIC_BUFFER,
        .uEntryCount = 256
    };
    uLogChannelGraphics = pl_add_log_channel("Graphics", tLogInit);
    uint32_t uLogLevel = PL_LOG_LEVEL_FATAL;
    if     (ptDesc->tFlags & PL_GRAPHICS_INIT_FLAGS_LOGGING_TRACE)   uLogLevel = PL_LOG_LEVEL_TRACE;
    else if(ptDesc->tFlags & PL_GRAPHICS_INIT_FLAGS_LOGGING_DEBUG)   uLogLevel = PL_LOG_LEVEL_DEBUG;
    else if(ptDesc->tFlags & PL_GRAPHICS_INIT_FLAGS_LOGGING_INFO)    uLogLevel = PL_LOG_LEVEL_INFO;
    else if(ptDesc->tFlags & PL_GRAPHICS_INIT_FLAGS_LOGGING_WARNING) uLogLevel = PL_LOG_LEVEL_WARN;
    else if(ptDesc->tFlags & PL_GRAPHICS_INIT_FLAGS_LOGGING_ERROR)   uLogLevel = PL_LOG_LEVEL_ERROR;
    pl_set_log_level(uLogChannelGraphics, uLogLevel);

    // save context for hot-reloads
    gptDataRegistry->set_data("plGraphics", gptGraphics);
    
    gptGraphics->bValidationActive = ptDesc->tFlags & PL_GRAPHICS_INIT_FLAGS_VALIDATION_ENABLED;

    gptGraphics->bDebugMessengerActive = gptGraphics->bDebugMessengerActive || (ptDesc->tFlags & PL_GRAPHICS_INIT_FLAGS_LOGGING_TRACE);
    gptGraphics->bDebugMessengerActive = gptGraphics->bDebugMessengerActive || (ptDesc->tFlags & PL_GRAPHICS_INIT_FLAGS_LOGGING_DEBUG);
    gptGraphics->bDebugMessengerActive = gptGraphics->bDebugMessengerActive || (ptDesc->tFlags & PL_GRAPHICS_INIT_FLAGS_LOGGING_INFO);
    gptGraphics->bDebugMessengerActive = gptGraphics->bDebugMessengerActive || (ptDesc->tFlags & PL_GRAPHICS_INIT_FLAGS_LOGGING_WARNING);
    gptGraphics->bDebugMessengerActive = gptGraphics->bDebugMessengerActive || (ptDesc->tFlags & PL_GRAPHICS_INIT_FLAGS_LOGGING_ERROR);

    // set frames in flight (if zero, use a default of 2)
    gptGraphics->uFramesInFlight = pl_min(pl_max(ptDesc->uFramesInFlight, 2), PL_MAX_FRAMES_IN_FLIGHT);

    //-------------------------------extensions------------------------------------

    // required extensions
    uint32_t uExtensionCount = 0;
    const char* apcExtensions[64] = {0};
    
    // if swapchain option is enabled, add required extensions
    if(ptDesc->tFlags & PL_GRAPHICS_INIT_FLAGS_SWAPCHAIN_ENABLED)
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
    if(gptGraphics->bDebugMessengerActive)
    {
        apcExtensions[uExtensionCount++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        apcExtensions[uExtensionCount++] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
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
    uint32_t uMissingExtensionCount = 0;
    const char* apcMissingExtensions[64] = {0};
    for(uint32_t i = 0; i < uExtensionCount; i++)
    {
        bool extensionFound = false;
        for(uint32_t j = 0; j < uInstanceExtensionsFound; j++)
        {
            if(strcmp(apcExtensions[i], ptAvailableExtensions[j].extensionName) == 0)
            {
                pl_log_trace_f(uLogChannelGraphics, "extension %s found", ptAvailableExtensions[j].extensionName);
                extensionFound = true;
                break;
            }
        }

        if(!extensionFound)
        {
            apcMissingExtensions[uMissingExtensionCount++] = apcExtensions[i];
        }
    }

    // report if all requested extensions aren't found
    if(uMissingExtensionCount > 0)
    {
        for(uint32_t i = 0; i < uMissingExtensionCount; i++)
        {
            pl_log_error_f(uLogChannelGraphics, "  * %s", apcMissingExtensions[i]);
        }

        PL_ASSERT(false && "Can't find all requested extensions");
        
        if(ptAvailableExtensions)
            PL_FREE(ptAvailableExtensions);

        return false;
    }

    //---------------------------------layers--------------------------------------

    // retrieve supported layers
    uint32_t uInstanceLayersFound = 0u;
    VkLayerProperties* ptAvailableLayers = NULL;
    PL_VULKAN(vkEnumerateInstanceLayerProperties(&uInstanceLayersFound, NULL));
    if(uInstanceLayersFound > 0)
    {
        ptAvailableLayers = (VkLayerProperties*)PL_ALLOC(sizeof(VkLayerProperties) * uInstanceLayersFound);
        PL_VULKAN(vkEnumerateInstanceLayerProperties(&uInstanceLayersFound, ptAvailableLayers));
    }

    // ensure layers are supported
    static const char* pcValidationLayer = "VK_LAYER_KHRONOS_validation";
    bool bLayerFound = true;
    if(gptGraphics->bValidationActive)
    {
        bLayerFound = false;
        for(uint32_t i = 0; i < uInstanceLayersFound; i++)
        {
            if(strcmp(pcValidationLayer, ptAvailableLayers[i].layerName) == 0)
            {
                pl_log_trace_f(uLogChannelGraphics, "layer %s found", ptAvailableLayers[i].layerName);
                bLayerFound = true;
                break;
            }
        }
    }
    
    if(!bLayerFound)
    {
        PL_ASSERT("Can't find requested layers");
        if(ptAvailableLayers)
            PL_FREE(ptAvailableLayers);
        return false;
    }

    // create vulkan tInstance
    const VkApplicationInfo tAppInfo = {
        .sType      = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = VK_API_VERSION_1_2
    };

    const void* pCreateInfoNext = VK_NULL_HANDLE;

    if(gptGraphics->bDebugMessengerActive)
    {

        // Setup debug messenger for vulkan instance
        static VkDebugUtilsMessengerCreateInfoEXT tDebugCreateInfo = {
            .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = pl__debug_callback,
            .pNext           = VK_NULL_HANDLE
        };

        if(ptDesc->tFlags & PL_GRAPHICS_INIT_FLAGS_LOGGING_TRACE)
            tDebugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
        else if((ptDesc->tFlags & PL_GRAPHICS_INIT_FLAGS_LOGGING_DEBUG) || (ptDesc->tFlags & PL_GRAPHICS_INIT_FLAGS_LOGGING_INFO))
            tDebugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
        else if(ptDesc->tFlags & PL_GRAPHICS_INIT_FLAGS_LOGGING_WARNING)
            tDebugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        else if(ptDesc->tFlags & PL_GRAPHICS_INIT_FLAGS_LOGGING_ERROR)
            tDebugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

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
    pl_log_trace_f(uLogChannelGraphics, "created vulkan instance");

    // cleanup
    if(ptAvailableLayers)
        PL_FREE(ptAvailableLayers);

    if(ptAvailableExtensions)
        PL_FREE(ptAvailableExtensions);
    
    if(gptGraphics->bDebugMessengerActive)
    {
        PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(gptGraphics->tInstance, "vkCreateDebugUtilsMessengerEXT");
        PL_ASSERT(func != NULL && "failed to set up debug messenger!");
        PL_VULKAN(func(gptGraphics->tInstance, pCreateInfoNext, NULL, &gptGraphics->tDbgMessenger));     
        pl_log_trace_f(uLogChannelGraphics, "enabled Vulkan validation layers");
    }

    return true;
}

static void
pl_enumerate_devices(plDeviceInfo* atDeviceInfo, uint32_t* puDeviceCount)
{
    VkPhysicalDevice atDevices[16] = {0};
    PL_VULKAN(vkEnumeratePhysicalDevices(gptGraphics->tInstance, puDeviceCount, atDevices));

    if(atDeviceInfo == NULL)
        return;

    memset(atDeviceInfo, 0, (*puDeviceCount) * sizeof(plDeviceInfo));
    for(uint32_t i = 0; i < *puDeviceCount; i++)
    {
        atDeviceInfo[i].uDeviceIdx = i;

        VkPhysicalDeviceProperties tProps = {0};
        vkGetPhysicalDeviceProperties(atDevices[i], &tProps);

        VkPhysicalDeviceMemoryProperties tMemProps = {0};
        vkGetPhysicalDeviceMemoryProperties(atDevices[i], &tMemProps);

        strncpy(atDeviceInfo[i].acName, tProps.deviceName, 256);
        atDeviceInfo[i].tLimits.uMaxTextureSize = tProps.limits.maxImageDimension2D;
        atDeviceInfo[i].tLimits.uMinUniformBufferOffsetAlignment = (uint32_t)tProps.limits.minUniformBufferOffsetAlignment;

        switch(tProps.deviceType)
        {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   atDeviceInfo[i].tType = PL_DEVICE_TYPE_DISCRETE; break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:            atDeviceInfo[i].tType = PL_DEVICE_TYPE_CPU; break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: atDeviceInfo[i].tType = PL_DEVICE_TYPE_INTEGRATED; break;
            default:                                     atDeviceInfo[i].tType = PL_DEVICE_TYPE_NONE;
        }

        switch(tProps.vendorID)
        {
            case 0x1002: atDeviceInfo[i].tVendorId = PL_VENDOR_ID_AMD; break;
            case 0x10DE: atDeviceInfo[i].tVendorId = PL_VENDOR_ID_NVIDIA; break;
            case 0x8086: atDeviceInfo[i].tVendorId = PL_VENDOR_ID_INTEL; break;
            default:     atDeviceInfo[i].tVendorId = PL_VENDOR_ID_NONE;
        }

        for(uint32_t j = 0; j < tMemProps.memoryHeapCount; j++)
        {
            if(tMemProps.memoryHeaps[j].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                atDeviceInfo[i].szDeviceMemory += tMemProps.memoryHeaps[j].size;
            else
                atDeviceInfo[i].szHostMemory += tMemProps.memoryHeaps[j].size;
        }

        uint32_t uExtensionCount = 0;
        vkEnumerateDeviceExtensionProperties(atDevices[i], NULL, &uExtensionCount, NULL);
        VkExtensionProperties* ptExtensions = pl_temp_allocator_alloc(&gptGraphics->tTempAllocator, uExtensionCount * sizeof(VkExtensionProperties));
        vkEnumerateDeviceExtensionProperties(atDevices[i], NULL, &uExtensionCount, ptExtensions);

        for(uint32_t j = 0; j < uExtensionCount; j++)
        {
            if(pl_str_equal(ptExtensions[j].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
                atDeviceInfo[i].tCapabilities |= PL_DEVICE_CAPABILITY_SWAPCHAIN;
        }
        pl_temp_allocator_reset(&gptGraphics->tTempAllocator);

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

        vkGetPhysicalDeviceFeatures(atDevices[i], &tDeviceFeatures);
        vkGetPhysicalDeviceFeatures2(atDevices[i], &tDeviceFeatures2);

        // Non-uniform indexing and update after bind
        // binding flags for textures, uniforms, and buffers
        // are required for our extension
        if(tDescriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing &&
            tDescriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind)
        {
            atDeviceInfo[i].tCapabilities |= PL_DEVICE_CAPABILITY_DESCRIPTOR_INDEXING;
            // PL_ASSERT(tDescriptorIndexingFeatures.shaderUniformBufferArrayNonUniformIndexing);
            // PL_ASSERT(tDescriptorIndexingFeatures.descriptorBindingUniformBufferUpdateAfterBind);
            // PL_ASSERT(tDescriptorIndexingFeatures.shaderStorageBufferArrayNonUniformIndexing);
            // PL_ASSERT(tDescriptorIndexingFeatures.descriptorBindingStorageBufferUpdateAfterBind);
        }

        if(tDeviceFeatures.samplerAnisotropy)
            atDeviceInfo[i].tCapabilities |= PL_DEVICE_CAPABILITY_SAMPLER_ANISOTROPY;

    }
}

static plDevice*
pl__create_device(const plDeviceInfo* ptInfo)
{

    plDevice* ptDevice = PL_ALLOC(sizeof(plDevice));
    memset(ptDevice, 0, sizeof(plDevice));

    uint32_t uDeviceCount = 16;
    VkPhysicalDevice atDevices[16] = {0};
    PL_VULKAN(vkEnumeratePhysicalDevices(gptGraphics->tInstance, &uDeviceCount, atDevices));

    // user decided on device
    ptDevice->tPhysicalDevice = atDevices[ptInfo->uDeviceIdx];
    memcpy(&ptDevice->tInfo, ptInfo, sizeof(plDeviceInfo));

    if(ptDevice->tInfo.szDynamicBufferBlockSize == 0)        ptDevice->tInfo.szDynamicBufferBlockSize = 134217728;
    if(ptDevice->tInfo.szDynamicDataMaxSize == 0)            ptDevice->tInfo.szDynamicDataMaxSize = 256;

    if(ptDevice->tInfo.szInitSamplerBindings == 0)           ptDevice->tInfo.szInitSamplerBindings = 100000;
    if(ptDevice->tInfo.szInitUniformBufferBindings == 0)     ptDevice->tInfo.szInitUniformBufferBindings = 100000;
    if(ptDevice->tInfo.szInitStorageBufferBindings == 0)     ptDevice->tInfo.szInitStorageBufferBindings = 100000;
    if(ptDevice->tInfo.szInitSampledTextureBindings == 0)    ptDevice->tInfo.szInitSampledTextureBindings = 100000;
    if(ptDevice->tInfo.szInitStorageTextureBindings == 0)    ptDevice->tInfo.szInitStorageTextureBindings = 100000;
    if(ptDevice->tInfo.szInitAttachmentTextureBindings == 0) ptDevice->tInfo.szInitAttachmentTextureBindings = 100000;

    if(ptDevice->tInfo.szInitDynamicSamplerBindings == 0)           ptDevice->tInfo.szInitDynamicSamplerBindings = 10000;
    if(ptDevice->tInfo.szInitDynamicUniformBufferBindings == 0)     ptDevice->tInfo.szInitDynamicUniformBufferBindings = 10000;
    if(ptDevice->tInfo.szInitDynamicStorageBufferBindings == 0)     ptDevice->tInfo.szInitDynamicStorageBufferBindings = 10000;
    if(ptDevice->tInfo.szInitDynamicSampledTextureBindings == 0)    ptDevice->tInfo.szInitDynamicSampledTextureBindings = 10000;
    if(ptDevice->tInfo.szInitDynamicStorageTextureBindings == 0)    ptDevice->tInfo.szInitDynamicStorageTextureBindings = 10000;
    if(ptDevice->tInfo.szInitDynamicAttachmentTextureBindings == 0) ptDevice->tInfo.szInitDynamicAttachmentTextureBindings = 10000;

    const size_t szMaxDynamicBufferDescriptors = ptDevice->tInfo.szDynamicBufferBlockSize / ptDevice->tInfo.szDynamicDataMaxSize;

    const size_t szMaxSets = szMaxDynamicBufferDescriptors + 
        ptDevice->tInfo.szInitSamplerBindings + 
        ptDevice->tInfo.szInitUniformBufferBindings +
        ptDevice->tInfo.szInitStorageBufferBindings +
        ptDevice->tInfo.szInitSampledTextureBindings +
        ptDevice->tInfo.szInitStorageTextureBindings +
        ptDevice->tInfo.szInitAttachmentTextureBindings;

    const size_t szMaxDynamicSets =
        ptDevice->tInfo.szInitDynamicSamplerBindings + 
        ptDevice->tInfo.szInitDynamicUniformBufferBindings +
        ptDevice->tInfo.szInitDynamicStorageBufferBindings +
        ptDevice->tInfo.szInitDynamicSampledTextureBindings +
        ptDevice->tInfo.szInitDynamicStorageTextureBindings +
        ptDevice->tInfo.szInitDynamicAttachmentTextureBindings;

    // find queue families
    ptDevice->iGraphicsQueueFamily = -1;
    ptDevice->iPresentQueueFamily = -1;
    uint32_t uQueueFamCnt = 0u;
    vkGetPhysicalDeviceQueueFamilyProperties(ptDevice->tPhysicalDevice, &uQueueFamCnt, NULL);

    VkQueueFamilyProperties auQueueFamilies[64] = {0};
    vkGetPhysicalDeviceQueueFamilyProperties(ptDevice->tPhysicalDevice, &uQueueFamCnt, auQueueFamilies);

    vkGetPhysicalDeviceMemoryProperties(ptDevice->tPhysicalDevice, &ptDevice->tMemProps);

    for(uint32_t i = 0; i < uQueueFamCnt; i++)
    {
        if (auQueueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) ptDevice->iGraphicsQueueFamily = i;

        VkBool32 tPresentSupport = false;
        PL_VULKAN(vkGetPhysicalDeviceSurfaceSupportKHR(ptDevice->tPhysicalDevice, i, ptInfo->ptSurface->tSurface, &tPresentSupport));

        if (tPresentSupport) ptDevice->iPresentQueueFamily  = i;

        if (ptDevice->iGraphicsQueueFamily > -1 && ptDevice->iPresentQueueFamily > -1) // complete
            break;
        i++;
    }

    const float fQueuePriority = 1.0f;
    VkDeviceQueueCreateInfo atQueueCreateInfos[] = {
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = ptDevice->iGraphicsQueueFamily,
            .queueCount = 1,
            .pQueuePriorities = &fQueuePriority
        },
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = ptDevice->iPresentQueueFamily,
            .queueCount = 1,
            .pQueuePriorities = &fQueuePriority   
        }
    };
    
    static const char* pcValidationLayers = "VK_LAYER_KHRONOS_validation";

    uint32_t uDeviceExtensionCount = 0;
    const char* apcDeviceExts[16] = {0};
    if(gptGraphics->bDebugMessengerActive)
    {
        apcDeviceExts[0] = VK_EXT_DEBUG_MARKER_EXTENSION_NAME;
        uDeviceExtensionCount++;
    }
    if(ptDevice->tInfo.tCapabilities & PL_DEVICE_CAPABILITY_SWAPCHAIN)
        apcDeviceExts[uDeviceExtensionCount++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    #if defined(__APPLE__)
    apcDeviceExts[uDeviceExtensionCount++] = "VK_KHR_portability_subset";
    #endif

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
        .sType                    = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount     = atQueueCreateInfos[0].queueFamilyIndex == atQueueCreateInfos[1].queueFamilyIndex ? 1 : 2,
        .pQueueCreateInfos        = atQueueCreateInfos,
        .pEnabledFeatures         = &tDeviceFeatures,
        .ppEnabledExtensionNames  = apcDeviceExts,
        .enabledLayerCount        = gptGraphics->bValidationActive ? 1 : 0,
        .ppEnabledLayerNames      = gptGraphics->bValidationActive ? &pcValidationLayers : NULL,
        .enabledExtensionCount    = uDeviceExtensionCount,
        .pNext                    = &tDeviceFeatures12
    };
    PL_VULKAN(vkCreateDevice(ptDevice->tPhysicalDevice, &tCreateDeviceInfo, NULL, &ptDevice->tLogicalDevice));

    // get device queues
    vkGetDeviceQueue(ptDevice->tLogicalDevice, ptDevice->iGraphicsQueueFamily, 0, &ptDevice->tGraphicsQueue);
    vkGetDeviceQueue(ptDevice->tLogicalDevice, ptDevice->iPresentQueueFamily, 0, &ptDevice->tPresentQueue);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~debug markers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    if(gptGraphics->bValidationActive)
    {
        ptDevice->vkDebugMarkerSetObjectTag  = (PFN_vkDebugMarkerSetObjectTagEXT)vkGetDeviceProcAddr(ptDevice->tLogicalDevice, "vkDebugMarkerSetObjectTagEXT");
        ptDevice->vkDebugMarkerSetObjectName = (PFN_vkDebugMarkerSetObjectNameEXT)vkGetDeviceProcAddr(ptDevice->tLogicalDevice, "vkDebugMarkerSetObjectNameEXT");
        ptDevice->vkCmdDebugMarkerBegin      = (PFN_vkCmdDebugMarkerBeginEXT)vkGetDeviceProcAddr(ptDevice->tLogicalDevice, "vkCmdDebugMarkerBeginEXT");
        ptDevice->vkCmdDebugMarkerEnd        = (PFN_vkCmdDebugMarkerEndEXT)vkGetDeviceProcAddr(ptDevice->tLogicalDevice, "vkCmdDebugMarkerEndEXT");
        ptDevice->vkCmdDebugMarkerInsert     = (PFN_vkCmdDebugMarkerInsertEXT)vkGetDeviceProcAddr(ptDevice->tLogicalDevice, "vkCmdDebugMarkerInsertEXT");
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~command pool~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const VkCommandPoolCreateInfo tCommandPoolInfo = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = ptDevice->iGraphicsQueueFamily,
        .flags            = 0
    };
    PL_VULKAN(vkCreateCommandPool(ptDevice->tLogicalDevice, &tCommandPoolInfo, NULL, &ptDevice->tCmdPool));

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~main descriptor pool~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    VkDescriptorPoolSize atPoolSizes[] =
    {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, (uint32_t)szMaxDynamicBufferDescriptors },
        { VK_DESCRIPTOR_TYPE_SAMPLER,          (uint32_t)ptDevice->tInfo.szInitSamplerBindings },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,    (uint32_t)ptDevice->tInfo.szInitSampledTextureBindings },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,    (uint32_t)ptDevice->tInfo.szInitStorageTextureBindings },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,   (uint32_t)ptDevice->tInfo.szInitUniformBufferBindings },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,   (uint32_t)ptDevice->tInfo.szInitStorageBufferBindings },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, (uint32_t)ptDevice->tInfo.szInitAttachmentTextureBindings }
    };
    VkDescriptorPoolCreateInfo tDescriptorPoolInfo = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = (uint32_t)szMaxSets,
        .poolSizeCount = 7,
        .pPoolSizes    = atPoolSizes,
    };
    if(ptDevice->tInfo.tCapabilities & PL_DEVICE_CAPABILITY_DESCRIPTOR_INDEXING)
    {
        tDescriptorPoolInfo.flags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
    }
    PL_VULKAN(vkCreateDescriptorPool(ptDevice->tLogicalDevice, &tDescriptorPoolInfo, NULL, &ptDevice->tDescriptorPool));

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~frame resources~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    static plInternalDeviceAllocatorData tAllocatorData = {0};
    static plDeviceMemoryAllocatorI tAllocator = {0};
    tAllocatorData.ptAllocator = &tAllocator;
    tAllocatorData.ptDevice = ptDevice;
    tAllocator.allocate = pl_allocate_staging_dynamic;
    tAllocator.free = pl_free_staging_dynamic;
    tAllocator.ptInst = (struct plDeviceMemoryAllocatorO*)&tAllocatorData;
    ptDevice->ptDynamicAllocator = &tAllocator;
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
    PL_VULKAN(vkCreateDescriptorSetLayout(ptDevice->tLogicalDevice, &tDescriptorSetLayoutInfo, NULL, &ptDevice->tDynamicDescriptorSetLayout));

    const VkCommandPoolCreateInfo tFrameCommandPoolInfo = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = ptDevice->iGraphicsQueueFamily,
        .flags            = 0
    };
    
    const VkSemaphoreCreateInfo tSemaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    const VkFenceCreateInfo tFenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    pl_sb_resize(ptDevice->sbFrames, gptGraphics->uFramesInFlight);
    pl_sb_resize(ptDevice->sbtGarbage, gptGraphics->uFramesInFlight);
    for(uint32_t i = 0; i < gptGraphics->uFramesInFlight; i++)
    {
        plFrameContext tFrame = {0};
        PL_VULKAN(vkCreateSemaphore(ptDevice->tLogicalDevice, &tSemaphoreInfo, NULL, &tFrame.tImageAvailable));
        PL_VULKAN(vkCreateSemaphore(ptDevice->tLogicalDevice, &tSemaphoreInfo, NULL, &tFrame.tRenderFinish));
        PL_VULKAN(vkCreateFence(ptDevice->tLogicalDevice, &tFenceInfo, NULL, &tFrame.tInFlight));
        PL_VULKAN(vkCreateCommandPool(ptDevice->tLogicalDevice, &tFrameCommandPoolInfo, NULL, &tFrame.tCmdPool));

        // dynamic buffer stuff
        pl_sb_resize(tFrame.sbtDynamicBuffers, 1);
        plBufferDescription tStagingBufferDescription0 = {
            .tUsage               = PL_BUFFER_USAGE_UNIFORM | PL_BUFFER_USAGE_STAGING,
            .uByteSize            = (uint32_t)ptDevice->tInfo.szDynamicBufferBlockSize
        };
        pl_sprintf(tStagingBufferDescription0.acDebugName, "D-BUF-F%d-0", (int)i);

        plBufferHandle tStagingBuffer0 = pl_create_buffer(ptDevice, &tStagingBufferDescription0, "dynamic buffer 0");
        plBuffer* ptBuffer = &ptDevice->sbtBuffersCold[tStagingBuffer0.uIndex];
        plDeviceMemoryAllocation tAllocation = ptDynamicAllocator->allocate(ptDynamicAllocator->ptInst, ptBuffer->tMemoryRequirements.uMemoryTypeBits, ptBuffer->tMemoryRequirements.ulSize, ptBuffer->tMemoryRequirements.ulAlignment, "dynamic buffer");
        pl_bind_buffer_to_memory(ptDevice, tStagingBuffer0, &tAllocation);

        tFrame.uCurrentBufferIndex = UINT32_MAX;
        tFrame.sbtDynamicBuffers[0].uHandle = tStagingBuffer0.uIndex;
        tFrame.sbtDynamicBuffers[0].tBuffer = ptDevice->sbtBuffersHot[tStagingBuffer0.uIndex].tBuffer;
        tFrame.sbtDynamicBuffers[0].tMemory = tAllocation;
        tFrame.sbtDynamicBuffers[0].uByteOffset = 0;

          // allocate descriptor sets
        const VkDescriptorSetAllocateInfo tDynamicAllocInfo = {
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool     = ptDevice->tDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts        = &ptDevice->tDynamicDescriptorSetLayout
        };
        PL_VULKAN(vkAllocateDescriptorSets(ptDevice->tLogicalDevice, &tDynamicAllocInfo, &tFrame.sbtDynamicBuffers[0].tDescriptorSet));

        VkDescriptorBufferInfo tDescriptorInfo0 = {
            .buffer = tFrame.sbtDynamicBuffers[0].tBuffer,
            .offset = 0,
            .range  = ptDevice->tInfo.szDynamicDataMaxSize
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

        VkDescriptorPoolSize atDynamicPoolSizes[] =
        {
            { VK_DESCRIPTOR_TYPE_SAMPLER,          (uint32_t)ptDevice->tInfo.szInitDynamicSamplerBindings },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,    (uint32_t)ptDevice->tInfo.szInitDynamicSampledTextureBindings },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,    (uint32_t)ptDevice->tInfo.szInitDynamicStorageTextureBindings },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,   (uint32_t)ptDevice->tInfo.szInitDynamicUniformBufferBindings },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,   (uint32_t)ptDevice->tInfo.szInitDynamicStorageBufferBindings },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, (uint32_t)ptDevice->tInfo.szInitDynamicAttachmentTextureBindings }
        };
        VkDescriptorPoolCreateInfo tDynamicDescriptorPoolInfo = {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            .maxSets       = (uint32_t)szMaxDynamicSets,
            .poolSizeCount = 6,
            .pPoolSizes    = atDynamicPoolSizes,
        };
        if(ptDevice->tInfo.tCapabilities & PL_DEVICE_CAPABILITY_DESCRIPTOR_INDEXING)
        {
            tDynamicDescriptorPoolInfo.flags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
        }
        PL_VULKAN(vkCreateDescriptorPool(ptDevice->tLogicalDevice, &tDynamicDescriptorPoolInfo, NULL, &tFrame.tDynamicDescriptorPool));

        ptDevice->sbFrames[i] = tFrame;
    }
    pl_temp_allocator_reset(&gptGraphics->tTempAllocator);

    return ptDevice;
}

static plSurface*
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
            .hwnd = (HWND)ptWindow->_pPlatformData
        };
        PL_VULKAN(vkCreateWin32SurfaceKHR(gptGraphics->tInstance, &tSurfaceCreateInfo, NULL, &ptSurface->tSurface));
    #elif defined(__ANDROID__)
        const VkAndroidSurfaceCreateInfoKHR tSurfaceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
            .pNext = NULL,
            .flags = 0,
            .window =
        };
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
            .pLayer = ((plWindowData*)ptWindow->_pPlatformData)->ptLayer
        };
        PL_VULKAN(vkCreateMetalSurfaceEXT(gptGraphics->tInstance, &tSurfaceCreateInfo, NULL, &ptSurface->tSurface));
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
        PL_VULKAN(vkCreateXcbSurfaceKHR(gptGraphics->tInstance, &tSurfaceCreateInfo, NULL, &ptSurface->tSurface));
    #endif
    return ptSurface;
}

static plSwapchain*
pl_create_swapchain(plDevice* ptDevice, const plSwapchainInit* ptInit)
{

    plSwapchain* ptSwap = PL_ALLOC(sizeof(plSwapchain));
    memset(ptSwap, 0, sizeof(plSwapchain));

    ptSwap->ptSurface = ptInit->ptSurface;
    ptSwap->ptDevice = ptDevice;
    ptSwap->uImageCount = gptGraphics->uFramesInFlight;
    ptSwap->tFormat = PL_FORMAT_B8G8R8A8_UNORM;
    ptSwap->bVSync = true;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~swapchain~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    plIO* ptIOCtx = gptIOI->get_io();
    pl__create_swapchain((uint32_t)ptIOCtx->tMainViewportSize.x, (uint32_t)ptIOCtx->tMainViewportSize.y, ptSwap);

    pl_create_main_render_pass_layout(ptSwap);
    pl_create_main_render_pass(ptSwap);

    return ptSwap;
}

static bool
pl_begin_frame(plSwapchain* ptSwap)
{
    pl_begin_profile_sample(0, __FUNCTION__);

    plDevice* ptDevice = ptSwap->ptDevice;
    plIO* ptIOCtx = gptIOI->get_io();

    gptGraphics->bWithinFrameContext = true;

    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptDevice);
    ptCurrentFrame->uCurrentBufferIndex = UINT32_MAX;

    PL_VULKAN(vkWaitForFences(ptDevice->tLogicalDevice, 1, &ptCurrentFrame->tInFlight, VK_TRUE, UINT64_MAX));
    pl__garbage_collect(ptDevice);
    
    VkResult err = vkAcquireNextImageKHR(ptDevice->tLogicalDevice, ptSwap->tSwapChain, UINT64_MAX, ptCurrentFrame->tImageAvailable, VK_NULL_HANDLE, &ptSwap->uCurrentImageIndex);
    if(err == VK_SUBOPTIMAL_KHR || err == VK_ERROR_OUT_OF_DATE_KHR)
    {
        if(err == VK_ERROR_OUT_OF_DATE_KHR)
        {
            pl__create_swapchain((uint32_t)ptIOCtx->tMainViewportSize.x, (uint32_t)ptIOCtx->tMainViewportSize.y, ptSwap);
            pl_end_profile_sample(0);
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

    PL_VULKAN(vkResetDescriptorPool(ptDevice->tLogicalDevice, ptCurrentFrame->tDynamicDescriptorPool, 0));
    PL_VULKAN(vkResetCommandPool(ptDevice->tLogicalDevice, ptCurrentFrame->tCmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT));
    PL_VULKAN(vkResetCommandPool(ptDevice->tLogicalDevice, ptDevice->tCmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT));
    
    pl_end_profile_sample(0);
    return true; 
}

static void
pl_end_command_recording(plCommandBufferHandle tHandle)
{
    PL_VULKAN(vkEndCommandBuffer(pl__get_command_buffer(tHandle)->tCmdBuffer));  
}

static bool
pl_present(plCommandBufferHandle tHandle, const plSubmitInfo* ptSubmitInfo, plSwapchain* ptSwap)
{
    pl_begin_profile_sample(0, __FUNCTION__);
    plIO* ptIOCtx = gptIOI->get_io();

    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(tHandle);
    plDevice* ptDevice = ptCmdBuffer->ptDevice;


    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptDevice);
    gptGraphics->bWithinFrameContext = false;

    // submit
    VkPipelineStageFlags atWaitStages[PL_MAX_SEMAPHORES + 1] = { 0 };
    atWaitStages[0] = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    VkCommandBuffer atCmdBuffers[] = {ptCmdBuffer->tCmdBuffer};
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
            atWaitSemaphores[i]  = ptDevice->sbtSemaphoresHot[ptCmdBuffer->tBeginInfo.atWaitSempahores[i].uIndex];
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
            atSignalSemaphores[i]  = ptDevice->sbtSemaphoresHot[ptSubmitInfo->atSignalSempahores[i].uIndex];
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
                       
    const VkPresentInfoKHR tPresentInfo = {
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &ptCurrentFrame->tRenderFinish,
        .swapchainCount     = 1,
        .pSwapchains        = &ptSwap->tSwapChain,
        .pImageIndices      = &ptSwap->uCurrentImageIndex,
    };
    const VkResult tResult = vkQueuePresentKHR(ptDevice->tPresentQueue, &tPresentInfo);
    if(tResult == VK_SUBOPTIMAL_KHR || tResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        pl__create_swapchain((uint32_t)ptIOCtx->tMainViewportSize.x, (uint32_t)ptIOCtx->tMainViewportSize.y, ptSwap);
        pl_sb_push(ptCurrentFrame->sbtPendingCommandBuffers, ptCmdBuffer->tCmdBuffer);
        pl__return_command_buffer_handle(tHandle);
        pl_end_profile_sample(0);
        return false;
    }
    else
    {
        PL_VULKAN(tResult);
    }
    gptGraphics->uCurrentFrameIndex = (gptGraphics->uCurrentFrameIndex + 1) % gptGraphics->uFramesInFlight;
    pl_sb_push(ptCurrentFrame->sbtPendingCommandBuffers, ptCmdBuffer->tCmdBuffer);
    pl__return_command_buffer_handle(tHandle);
    pl_end_profile_sample(0);
    return true;
}

static void
pl_resize(plSwapchain* ptSwap)
{
    pl_begin_profile_sample(0, __FUNCTION__);
    plIO* ptIOCtx = gptIOI->get_io();
    plDevice* ptDevice = ptSwap->ptDevice;

    pl__create_swapchain((uint32_t)ptIOCtx->tMainViewportSize.x, (uint32_t)ptIOCtx->tMainViewportSize.y, ptSwap);

    plRenderPass* ptRenderPass = &ptDevice->sbtRenderPassesCold[ptDevice->tMainRenderPass.uIndex];
    plVulkanRenderPass* ptVulkanRenderPass = &ptDevice->sbtRenderPassesHot[ptDevice->tMainRenderPass.uIndex];
    plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);
    ptRenderPass->tDesc.tDimensions = ptIOCtx->tMainViewportSize;

    for(uint32_t i = 0; i < ptSwap->uImageCount; i++)
    {

        VkFramebufferCreateInfo tFrameBufferInfo = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = ptVulkanRenderPass->tRenderPass,
            .attachmentCount = 1,
            .pAttachments    = &ptDevice->sbtTexturesHot[ptSwap->sbtSwapchainTextureViews[i].uIndex].tImageView,
            .width           = (uint32_t)ptIOCtx->tMainViewportSize.x,
            .height          = (uint32_t)ptIOCtx->tMainViewportSize.y,
            .layers          = 1u,
        };
        pl_sb_push(ptFrame->sbtRawFrameBuffers, ptVulkanRenderPass->atFrameBuffers[i]);
        ptVulkanRenderPass->atFrameBuffers[i] = VK_NULL_HANDLE;
        PL_VULKAN(vkCreateFramebuffer(ptDevice->tLogicalDevice, &tFrameBufferInfo, NULL, &ptVulkanRenderPass->atFrameBuffers[i]));
    }

    pl_end_profile_sample(0);
}

static void
pl_flush_device(plDevice* ptDevice)
{
    vkDeviceWaitIdle(ptDevice->tLogicalDevice);
}

static void
pl_cleanup_graphics(void)
{
    // destroy tInstance
    vkDestroyInstance(gptGraphics->tInstance, NULL);

    pl_temp_allocator_free(&gptGraphics->tTempAllocator);
    

    pl__cleanup_common_graphics();
}

static void
pl_cleanup_surface(plSurface* ptSurface)
{
    vkDestroySurfaceKHR(gptGraphics->tInstance, ptSurface->tSurface, NULL);
    PL_FREE(ptSurface);
}

static void
pl_cleanup_swapchain(plSwapchain* ptSwap)
{
    pl_sb_free(ptSwap->sbtSurfaceFormats);
    pl_sb_free(ptSwap->sbtImages);
    vkDestroySwapchainKHR(ptSwap->ptDevice->tLogicalDevice, ptSwap->tSwapChain, NULL);
    pl__cleanup_common_swapchain(ptSwap);
}

static void
pl_cleanup_device(plDevice* ptDevice)
{

    vkDeviceWaitIdle(ptDevice->tLogicalDevice);

    for(uint32_t i = 0; i < pl_sb_size(ptDevice->sbtTexturesHot); i++)
    {
        if(ptDevice->sbtTexturesHot[i].tImage && ptDevice->sbtTexturesHot[i].bOriginalView)
        {
            vkDestroyImage(ptDevice->tLogicalDevice, ptDevice->sbtTexturesHot[i].tImage, NULL);
            
        }
        ptDevice->sbtTexturesHot[i].tImage = VK_NULL_HANDLE;

        if(ptDevice->sbtTexturesHot[i].tImageView)
        {
            vkDestroyImageView(ptDevice->tLogicalDevice, ptDevice->sbtTexturesHot[i].tImageView, NULL);
            ptDevice->sbtTexturesHot[i].tImageView = VK_NULL_HANDLE;
        }
    }

    for(uint32_t i = 0; i < pl_sb_size(ptDevice->sbtSamplersHot); i++)
    {
        if(ptDevice->sbtSamplersHot[i])
            vkDestroySampler(ptDevice->tLogicalDevice, ptDevice->sbtSamplersHot[i], NULL);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptDevice->sbtBindGroupsHot); i++)
    {
        vkDestroyDescriptorSetLayout(ptDevice->tLogicalDevice, ptDevice->sbtBindGroupsHot[i].tDescriptorSetLayout, NULL);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptDevice->sbtBuffersHot); i++)
    {
        if(ptDevice->sbtBuffersHot[i].tBuffer)
            vkDestroyBuffer(ptDevice->tLogicalDevice, ptDevice->sbtBuffersHot[i].tBuffer, NULL);
    }
    for(uint32_t i = 0; i < pl_sb_size(ptDevice->sbtShadersHot); i++)
    {
        plVulkanShader* ptVulkanShader = &ptDevice->sbtShadersHot[i];
        if(ptVulkanShader->tPipeline)
            vkDestroyPipeline(ptDevice->tLogicalDevice, ptVulkanShader->tPipeline, NULL);
        if(ptVulkanShader->tPipelineLayout)
            vkDestroyPipelineLayout(ptDevice->tLogicalDevice, ptVulkanShader->tPipelineLayout, NULL);
        if(ptVulkanShader->tVertexShaderModule)
            vkDestroyShaderModule(ptDevice->tLogicalDevice, ptVulkanShader->tVertexShaderModule, NULL);
        if(ptVulkanShader->tPixelShaderModule)
            vkDestroyShaderModule(ptDevice->tLogicalDevice, ptVulkanShader->tPixelShaderModule, NULL);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptDevice->sbtComputeShadersHot); i++)
    {
        plVulkanComputeShader* ptVulkanShader = &ptDevice->sbtComputeShadersHot[i];
        if(ptVulkanShader->tPipeline)
            vkDestroyPipeline(ptDevice->tLogicalDevice, ptVulkanShader->tPipeline, NULL);
        if(ptVulkanShader->tPipelineLayout)
            vkDestroyPipelineLayout(ptDevice->tLogicalDevice, ptVulkanShader->tPipelineLayout, NULL);
        if(ptVulkanShader->tShaderModule)
            vkDestroyShaderModule(ptDevice->tLogicalDevice, ptVulkanShader->tShaderModule, NULL);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptDevice->sbtBindGroupLayouts); i++)
    {
        if(ptDevice->sbtBindGroupLayouts[i].tDescriptorSetLayout)
            vkDestroyDescriptorSetLayout(ptDevice->tLogicalDevice, ptDevice->sbtBindGroupLayouts[i].tDescriptorSetLayout, NULL);    
    }

    for(uint32_t i = 0; i < pl_sb_size(ptDevice->sbtRenderPassLayoutsHot); i++)
    {
        if(ptDevice->sbtRenderPassLayoutsHot[i].tRenderPass)
            vkDestroyRenderPass(ptDevice->tLogicalDevice, ptDevice->sbtRenderPassLayoutsHot[i].tRenderPass, NULL);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptDevice->sbtSemaphoresHot); i++)
    {
        if(ptDevice->sbtSemaphoresHot[i])
            vkDestroySemaphore(ptDevice->tLogicalDevice, ptDevice->sbtSemaphoresHot[i], NULL);
        ptDevice->sbtSemaphoresHot[i] = VK_NULL_HANDLE;
    }

    vkDestroyDescriptorSetLayout(ptDevice->tLogicalDevice, ptDevice->tDynamicDescriptorSetLayout, NULL);
    pl_sb_free(ptDevice->sbtTexturesHot);
    pl_sb_free(ptDevice->sbtSamplersHot);
    pl_sb_free(ptDevice->sbtBindGroupsHot);
    pl_sb_free(ptDevice->sbtBuffersHot);
    pl_sb_free(ptDevice->sbtShadersHot);
    pl_sb_free(ptDevice->sbtComputeShadersHot);
    pl_sb_free(ptDevice->sbtBindGroupLayouts);
    pl_sb_free(ptDevice->sbtSemaphoresHot);

    // cleanup per frame resources
    for(uint32_t i = 0; i < pl_sb_size(ptDevice->sbFrames); i++)
    {
        plFrameContext* ptFrame = &ptDevice->sbFrames[i];
        vkDestroySemaphore(ptDevice->tLogicalDevice, ptFrame->tImageAvailable, NULL);
        vkDestroySemaphore(ptDevice->tLogicalDevice, ptFrame->tRenderFinish, NULL);
        vkDestroyFence(ptDevice->tLogicalDevice, ptFrame->tInFlight, NULL);
        vkDestroyCommandPool(ptDevice->tLogicalDevice, ptFrame->tCmdPool, NULL);
        vkDestroyDescriptorPool(ptDevice->tLogicalDevice, ptFrame->tDynamicDescriptorPool, NULL);

        for(uint32_t j = 0; j < pl_sb_size(ptFrame->sbtDynamicBuffers); j++)
        {
            if(ptFrame->sbtDynamicBuffers[j].tMemory.uHandle)
                ptDevice->ptDynamicAllocator->free(ptDevice->ptDynamicAllocator->ptInst, &ptFrame->sbtDynamicBuffers[j].tMemory);
        }
        
        for(uint32_t j = 0; j < pl_sb_size(ptFrame->sbtRawFrameBuffers); j++)
        {
            vkDestroyFramebuffer(ptDevice->tLogicalDevice, ptFrame->sbtRawFrameBuffers[j], NULL);
            ptFrame->sbtRawFrameBuffers[j] = VK_NULL_HANDLE;
        }

        pl_sb_free(ptFrame->sbtRawFrameBuffers);
        pl_sb_free(ptFrame->sbtDynamicBuffers);
        pl_sb_free(ptFrame->sbtPendingCommandBuffers);
        pl_sb_free(ptFrame->sbtReadyCommandBuffers);
    }
    pl_sb_free(ptDevice->sbFrames);

    for(uint32_t i = 0; i < pl_sb_size(ptDevice->sbtRenderPassesHot); i++)
    {
        if(ptDevice->sbtRenderPassesHot[i].tRenderPass)
            vkDestroyRenderPass(ptDevice->tLogicalDevice, ptDevice->sbtRenderPassesHot[i].tRenderPass, NULL);

        for(uint32_t j = 0; j < 3; j++)
        {
            if(ptDevice->sbtRenderPassesHot[i].atFrameBuffers[j])
                vkDestroyFramebuffer(ptDevice->tLogicalDevice, ptDevice->sbtRenderPassesHot[i].atFrameBuffers[j], NULL);
            ptDevice->sbtRenderPassesHot[i].atFrameBuffers[j] = VK_NULL_HANDLE;
        }
    }

    vkDestroyDescriptorPool(ptDevice->tLogicalDevice, ptDevice->tDescriptorPool, NULL);

    // destroy command pool
    vkDestroyCommandPool(ptDevice->tLogicalDevice, ptDevice->tCmdPool, NULL);

    // destroy device
    vkDestroyDevice(ptDevice->tLogicalDevice, NULL);

    if(gptGraphics->tDbgMessenger)
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

static plComputeEncoderHandle
pl_begin_compute_pass(plCommandBufferHandle tCmdBuffer)
{
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(tCmdBuffer);
    VkMemoryBarrier tMemoryBarrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT
    };
    vkCmdPipelineBarrier(ptCmdBuffer->tCmdBuffer, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &tMemoryBarrier, 0, NULL, 0, NULL);
    
    plComputeEncoderHandle tHandle = pl__get_new_compute_encoder_handle();
    gptGraphics->sbtComputeEncoders[tHandle.uIndex].tCommandBuffer = tCmdBuffer;

    return tHandle;
}

static void
pl_end_compute_pass(plComputeEncoderHandle tHandle)
{
    plComputeEncoder* ptEncoder = pl__get_compute_encoder(tHandle);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);
    VkMemoryBarrier tMemoryBarrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT
    };
    vkCmdPipelineBarrier(ptCmdBuffer->tCmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &tMemoryBarrier, 0, NULL, 0, NULL);
    pl__return_compute_encoder_handle(tHandle);
}

static plBlitEncoderHandle
pl_begin_blit_pass(plCommandBufferHandle tCmdBuffer)
{
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(tCmdBuffer);
    VkMemoryBarrier tMemoryBarrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT
    };
    vkCmdPipelineBarrier(ptCmdBuffer->tCmdBuffer, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &tMemoryBarrier, 0, NULL, 0, NULL);
    
    plBlitEncoderHandle tHandle = pl__get_new_blit_encoder_handle();
    gptGraphics->sbtBlitEncoders[tHandle.uIndex].tCommandBuffer = tCmdBuffer;
    return tHandle;
}

static void
pl_end_blit_pass(plBlitEncoderHandle tHandle)
{
    plBlitEncoder* ptEncoder = pl__get_blit_encoder(tHandle);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);
    VkMemoryBarrier tMemoryBarrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT
    };
    vkCmdPipelineBarrier(ptCmdBuffer->tCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &tMemoryBarrier, 0, NULL, 0, NULL);
    pl__return_blit_encoder_handle(tHandle);
}

static void
pl_dispatch(plComputeEncoderHandle tHandle, uint32_t uDispatchCount, const plDispatch* atDispatches)
{
    plComputeEncoder* ptEncoder = pl__get_compute_encoder(tHandle);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);

    for(uint32_t i = 0; i < uDispatchCount; i++)
    {
        const plDispatch* ptDispatch = &atDispatches[i];
        vkCmdDispatch(ptCmdBuffer->tCmdBuffer, ptDispatch->uGroupCountX, ptDispatch->uGroupCountY, ptDispatch->uGroupCountZ);
    }
}

static void
pl_bind_compute_bind_groups(plComputeEncoderHandle tEncoder, plComputeShaderHandle tHandle, uint32_t uFirst, uint32_t uCount, const plBindGroupHandle* atBindGroups, plDynamicBinding* ptDynamicBinding)
{   
    plComputeEncoder* ptEncoder = pl__get_compute_encoder(tEncoder);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    plVulkanComputeShader* ptShader = &ptDevice->sbtComputeShadersHot[tHandle.uIndex];

    uint32_t uDynamicBindingCount = 0;
    uint32_t* puOffsets = NULL;
    if(ptDynamicBinding)
    {
        puOffsets = &ptDynamicBinding->uByteOffset;
        uDynamicBindingCount++;
    }

    VkDescriptorSet* atDescriptorSets = pl_temp_allocator_alloc(&gptGraphics->tTempAllocator, sizeof(VkDescriptorSet) * (uCount + uDynamicBindingCount));

    for(uint32_t i = 0; i < uCount; i++)
    {
        plVulkanBindGroup* ptBindGroup = &ptDevice->sbtBindGroupsHot[atBindGroups[i].uIndex];
        atDescriptorSets[i] = ptBindGroup->tDescriptorSet;
    }

    if(ptDynamicBinding)
    {
        plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptDevice); 
        atDescriptorSets[uCount] = ptCurrentFrame->sbtDynamicBuffers[ptDynamicBinding->uBufferHandle].tDescriptorSet;
    }

    vkCmdBindDescriptorSets(ptCmdBuffer->tCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ptShader->tPipelineLayout, uFirst, uCount + uDynamicBindingCount, atDescriptorSets, uDynamicBindingCount, puOffsets);
    pl_temp_allocator_reset(&gptGraphics->tTempAllocator);
}

static void
pl_bind_graphics_bind_groups(plRenderEncoderHandle tEncoder, plShaderHandle tHandle, uint32_t uFirst, uint32_t uCount, const plBindGroupHandle* atBindGroups, plDynamicBinding* ptDynamicBinding)
{
    plRenderEncoder* ptEncoder = pl__get_render_encoder(tEncoder);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    plVulkanShader* ptShader = &ptDevice->sbtShadersHot[tHandle.uIndex];

    uint32_t uDynamicBindingCount = 0;
    uint32_t* puOffsets = NULL;
    if(ptDynamicBinding)
    {
        puOffsets = &ptDynamicBinding->uByteOffset;
        uDynamicBindingCount++;
    }

    VkDescriptorSet* atDescriptorSets = pl_temp_allocator_alloc(&gptGraphics->tTempAllocator, sizeof(VkDescriptorSet) * (uCount + uDynamicBindingCount));

    for(uint32_t i = 0; i < uCount; i++)
    {
        plVulkanBindGroup* ptBindGroup = &ptDevice->sbtBindGroupsHot[atBindGroups[i].uIndex];
        atDescriptorSets[i] = ptBindGroup->tDescriptorSet;
    }

    if(ptDynamicBinding)
    {
        plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptDevice); 
        atDescriptorSets[uCount] = ptCurrentFrame->sbtDynamicBuffers[ptDynamicBinding->uBufferHandle].tDescriptorSet;
    }

    vkCmdBindDescriptorSets(ptCmdBuffer->tCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ptShader->tPipelineLayout, uFirst, uCount + uDynamicBindingCount, atDescriptorSets, uDynamicBindingCount, puOffsets);
    pl_temp_allocator_reset(&gptGraphics->tTempAllocator);
}

static void
pl_submit_command_buffer(plCommandBufferHandle tHandle, const plSubmitInfo* ptSubmitInfo)
{
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(tHandle);
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptDevice);

    VkSemaphore atWaitSemaphores[PL_MAX_SEMAPHORES] = {0};
    VkSemaphore atSignalSemaphores[PL_MAX_SEMAPHORES] = {0};
    VkPipelineStageFlags atWaitStages[PL_MAX_SEMAPHORES] = { 0 };
    atWaitStages[0] = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;

    VkSubmitInfo tSubmitInfo = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1u,
        .pCommandBuffers    = &ptCmdBuffer->tCmdBuffer,
    };

    VkTimelineSemaphoreSubmitInfo tTimelineInfo = {
        .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
        .pNext = NULL,
    };

    if(ptCmdBuffer->tBeginInfo.uWaitSemaphoreCount != UINT32_MAX)
    {
        for(uint32_t i = 0; i < ptCmdBuffer->tBeginInfo.uWaitSemaphoreCount; i++)
        {
            atWaitSemaphores[i]  = ptDevice->sbtSemaphoresHot[ptCmdBuffer->tBeginInfo.atWaitSempahores[i].uIndex];
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
            atSignalSemaphores[i]  = ptDevice->sbtSemaphoresHot[ptSubmitInfo->atSignalSempahores[i].uIndex];
        }

        tTimelineInfo.signalSemaphoreValueCount = ptSubmitInfo->uSignalSemaphoreCount;
        tTimelineInfo.pSignalSemaphoreValues = ptSubmitInfo->auSignalSemaphoreValues;

        tSubmitInfo.pNext = &tTimelineInfo;
        tSubmitInfo.pSignalSemaphores = atSignalSemaphores;
        tSubmitInfo.signalSemaphoreCount = ptSubmitInfo->uSignalSemaphoreCount;
    }

    PL_VULKAN(vkQueueSubmit(ptDevice->tGraphicsQueue, 1, &tSubmitInfo, VK_NULL_HANDLE));
    pl_sb_push(ptCurrentFrame->sbtPendingCommandBuffers, ptCmdBuffer->tCmdBuffer);
    pl__return_command_buffer_handle(tHandle);
}

static void
pl_submit_command_buffer_blocking(plCommandBufferHandle tHandle, const plSubmitInfo* ptSubmitInfo)
{
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(tHandle);
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptDevice);

    VkSemaphore atWaitSemaphores[PL_MAX_SEMAPHORES] = {0};
    VkSemaphore atSignalSemaphores[PL_MAX_SEMAPHORES] = {0};
    VkPipelineStageFlags atWaitStages[PL_MAX_SEMAPHORES] = { 0 };
    atWaitStages[0] = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;

    VkSubmitInfo tSubmitInfo = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1u,
        .pCommandBuffers    = &ptCmdBuffer->tCmdBuffer,
    };

    VkTimelineSemaphoreSubmitInfo tTimelineInfo = {
        .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
        .pNext = NULL,
    };

    if(ptCmdBuffer->tBeginInfo.uWaitSemaphoreCount != UINT32_MAX)
    {
        for(uint32_t i = 0; i < ptCmdBuffer->tBeginInfo.uWaitSemaphoreCount; i++)
        {
            atWaitSemaphores[i]  = ptDevice->sbtSemaphoresHot[ptCmdBuffer->tBeginInfo.atWaitSempahores[i].uIndex];
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
            atSignalSemaphores[i]  = ptDevice->sbtSemaphoresHot[ptSubmitInfo->atSignalSempahores[i].uIndex];
        }

        tTimelineInfo.signalSemaphoreValueCount = ptSubmitInfo->uSignalSemaphoreCount;
        tTimelineInfo.pSignalSemaphoreValues = ptSubmitInfo->auSignalSemaphoreValues;

        tSubmitInfo.pNext = &tTimelineInfo;
        tSubmitInfo.pSignalSemaphores = atSignalSemaphores;
        tSubmitInfo.signalSemaphoreCount = ptSubmitInfo->uSignalSemaphoreCount;
    }

    PL_VULKAN(vkQueueSubmit(ptDevice->tGraphicsQueue, 1, &tSubmitInfo, VK_NULL_HANDLE));
    PL_VULKAN(vkQueueWaitIdle(ptDevice->tGraphicsQueue));
    pl_sb_push(ptCurrentFrame->sbtPendingCommandBuffers, ptCmdBuffer->tCmdBuffer);
    pl__return_command_buffer_handle(tHandle);
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static VkSampleCountFlagBits
pl__get_max_sample_count(plDevice* ptDevice)
{
    VkPhysicalDeviceProperties tPhysicalDeviceProperties = {0};
    vkGetPhysicalDeviceProperties(ptDevice->tPhysicalDevice, &tPhysicalDeviceProperties);

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
    for(uint32_t i = 0u; i < uFormatCount; i++)
    {
        VkFormatProperties tProps = {0};
        vkGetPhysicalDeviceFormatProperties(ptDevice->tPhysicalDevice, ptFormats[i], &tProps);
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
        pl_log_error_f(uLogChannelGraphics, "error validation layer: %s\n", ptCallbackData->pMessage);
    }

    else if(tMsgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        pl_log_warn_f(uLogChannelGraphics, "warn validation layer: %s\n", ptCallbackData->pMessage);
    }

    else if(tMsgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    {
        pl_log_info_f(uLogChannelGraphics, "info validation layer: %s\n", ptCallbackData->pMessage);
    }
    else if(tMsgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
    {
        pl_log_trace_f(uLogChannelGraphics, "trace validation layer: %s\n", ptCallbackData->pMessage);
    }
    
    return VK_FALSE;
}

static void
pl__create_swapchain(uint32_t uWidth, uint32_t uHeight, plSwapchain* ptSwap)
{
    plDevice* ptDevice = ptSwap->ptDevice;
    vkDeviceWaitIdle(ptDevice->tLogicalDevice);

    // ptSwapchain->tMsaaSamples = (plSampleCount)pl__get_max_sample_count(ptDevice);

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
    static VkFormat atSurfaceFormatPreference[4] = 
    {
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_B8G8R8A8_SRGB
    };

    bool bPreferenceFound = false;
    VkSurfaceFormatKHR tSurfaceFormat = ptSwap->sbtSurfaceFormats[0];
    ptSwap->tFormat = pl__pilotlight_format(tSurfaceFormat.format);

    for(uint32_t i = 0u; i < 4; i++)
    {
        if(bPreferenceFound) break;
        
        for(uint32_t j = 0u; j < uFormatCount; j++)
        {
            if(ptSwap->sbtSurfaceFormats[j].format == atSurfaceFormatPreference[i] && ptSwap->sbtSurfaceFormats[j].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                tSurfaceFormat = ptSwap->sbtSurfaceFormats[j];
                ptSwap->tFormat = pl__pilotlight_format(tSurfaceFormat.format);
                bPreferenceFound = true;
                break;
            }
        }
    }
    PL_ASSERT(bPreferenceFound && "no preferred surface format found");

    // chose swap present mode
    VkPresentModeKHR tPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    if(!ptSwap->bVSync)
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
    ptSwap->tExtent.uWidth = tExtent.width;
    ptSwap->tExtent.uHeight = tExtent.height;

    // decide image count
    const uint32_t uOldImageCount = ptSwap->uImageCount;
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
        .clipped          = VK_TRUE, // setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
        .oldSwapchain     = ptSwap->tSwapChain, // setting oldSwapChain to the saved handle of the previous swapchain aids in resource reuse and makes sure that we can still present already acquired images
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

	// enable transfer source on swap chain images if supported
	if (tCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
		tCreateSwapchainInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	// enable transfer destination on swap chain images if supported
	if (tCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		tCreateSwapchainInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    uint32_t auQueueFamilyIndices[] = { (uint32_t)ptDevice->iGraphicsQueueFamily, (uint32_t)ptDevice->iPresentQueueFamily};
    if (ptDevice->iGraphicsQueueFamily != ptDevice->iPresentQueueFamily)
    {
        tCreateSwapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        tCreateSwapchainInfo.queueFamilyIndexCount = 2;
        tCreateSwapchainInfo.pQueueFamilyIndices = auQueueFamilyIndices;
    }

    VkSwapchainKHR tOldSwapChain = ptSwap->tSwapChain;

    PL_VULKAN(vkCreateSwapchainKHR(ptDevice->tLogicalDevice, &tCreateSwapchainInfo, NULL, &ptSwap->tSwapChain));

    // plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptDevice);
    plFrameContext* ptNextFrame = pl__get_next_frame_resources(ptDevice);
    if(tOldSwapChain)
    {
        
        for (uint32_t i = 0u; i < uOldImageCount; i++)
        {
            pl_queue_texture_for_deletion(ptDevice, ptSwap->sbtSwapchainTextureViews[i]);
        }
        vkDestroySwapchainKHR(ptDevice->tLogicalDevice, tOldSwapChain, NULL);
    }

    // get swapchain images

    PL_VULKAN(vkGetSwapchainImagesKHR(ptDevice->tLogicalDevice, ptSwap->tSwapChain, &ptSwap->uImageCount, NULL));
    pl_sb_resize(ptSwap->sbtImages, ptSwap->uImageCount);
    pl_sb_resize(ptSwap->sbtSwapchainTextureViews, ptSwap->uImageCount);

    PL_VULKAN(vkGetSwapchainImagesKHR(ptDevice->tLogicalDevice, ptSwap->tSwapChain, &ptSwap->uImageCount, ptSwap->sbtImages));

    for(uint32_t i = 0; i < ptSwap->uImageCount; i++)
    {
        plTextureViewDesc tTextureViewDesc = {
            .tFormat     = ptSwap->tFormat,
            .uBaseLayer  = 0,
            .uBaseMip    = 0,
            .uLayerCount = 1,
            .uMips       = 1
        };
        ptSwap->sbtSwapchainTextureViews[i] = pl_create_swapchain_texture_view(ptDevice, &tTextureViewDesc, ptSwap->sbtImages[i], "swapchain texture view");
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
pl_copy_buffer(plBlitEncoderHandle tEncoder, plBufferHandle tSource, plBufferHandle tDestination, uint32_t uSourceOffset, uint32_t uDestinationOffset, size_t szSize)
{
    plBlitEncoder* ptEncoder = pl__get_blit_encoder(tEncoder);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    const VkBufferCopy tCopyRegion = {
        .size = szSize,
        .srcOffset = uSourceOffset
    };

    vkCmdCopyBuffer(ptCmdBuffer->tCmdBuffer, ptDevice->sbtBuffersHot[tSource.uIndex].tBuffer, ptDevice->sbtBuffersHot[tDestination.uIndex].tBuffer, 1, &tCopyRegion);

}

static void
pl_copy_buffer_to_texture(plBlitEncoderHandle tEncoder, plBufferHandle tBufferHandle, plTextureHandle tTextureHandle, uint32_t uRegionCount, const plBufferImageCopy* ptRegions)
{
    plBlitEncoder* ptEncoder = pl__get_blit_encoder(tEncoder);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    plTexture* ptColdTexture = pl__get_texture(ptDevice, tTextureHandle);
    VkImageSubresourceRange* atSubResourceRanges = pl_temp_allocator_alloc(&gptGraphics->tTempAllocator, sizeof(VkImageSubresourceRange) * uRegionCount);
    VkBufferImageCopy*       atCopyRegions       = pl_temp_allocator_alloc(&gptGraphics->tTempAllocator, sizeof(VkBufferImageCopy) * uRegionCount);
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
        pl__transition_image_layout(ptCmdBuffer->tCmdBuffer, ptDevice->sbtTexturesHot[tTextureHandle.uIndex].tImage, tLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, atSubResourceRanges[i], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

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
    vkCmdCopyBufferToImage(ptCmdBuffer->tCmdBuffer, ptDevice->sbtBuffersHot[tBufferHandle.uIndex].tBuffer, ptDevice->sbtTexturesHot[tTextureHandle.uIndex].tImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, uRegionCount, atCopyRegions);

    for(uint32_t i = 0; i < uRegionCount; i++)
    {
        VkImageLayout tLayout = ptRegions[i].tCurrentImageUsage == 0 ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : pl__vulkan_layout(ptRegions[i].tCurrentImageUsage);
        pl__transition_image_layout(ptCmdBuffer->tCmdBuffer, ptDevice->sbtTexturesHot[tTextureHandle.uIndex].tImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, tLayout, atSubResourceRanges[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    }
        
    pl_temp_allocator_reset(&gptGraphics->tTempAllocator);
}

static plFrameContext*
pl__get_next_frame_resources(plDevice* ptDevice)
{
    return &ptDevice->sbFrames[(gptGraphics->uCurrentFrameIndex + 1) % gptGraphics->uFramesInFlight];
}

static void
pl_set_vulkan_object_name(plDevice* ptDevice, uint64_t uObjectHandle, VkDebugReportObjectTypeEXT tObjectType, const char* pcName)
{
    const VkDebugMarkerObjectNameInfoEXT tNameInfo = 
    {
        .sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT,
        .objectType = tObjectType,
        .object = uObjectHandle,
        .pObjectName = pcName
    };

    if(ptDevice->vkDebugMarkerSetObjectName)
        ptDevice->vkDebugMarkerSetObjectName(ptDevice->tLogicalDevice, &tNameInfo);
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
    uint32_t uMemoryType = 0u;
    bool bFound = false;
    VkMemoryPropertyFlags tProperties = 0;
    if(tMemoryMode == PL_MEMORY_GPU_CPU)
        tProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    else if(tMemoryMode == PL_MEMORY_GPU)
        tProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    else if(tMemoryMode == PL_MEMORY_CPU)
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
    VkResult tResult = vkAllocateMemory(ptDevice->tLogicalDevice, &tAllocInfo, NULL, &tMemory);
    PL_VULKAN(tResult);
    tBlock.uHandle = (uint64_t)tMemory;

    pl_set_vulkan_object_name(ptDevice, tBlock.uHandle, VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT, pcName);

    if(tMemoryMode == PL_MEMORY_GPU)
    {
        gptGraphics->szLocalMemoryInUse += tBlock.ulSize;
    }
    else
    {
        PL_VULKAN(vkMapMemory(ptDevice->tLogicalDevice, (VkDeviceMemory)tBlock.uHandle, 0, tBlock.ulSize, 0, (void**)&tBlock.pHostMapped));
        gptGraphics->szHostMemoryInUse += tBlock.ulSize;
    }

    return tBlock;
}

static void
pl_free_memory(plDevice* ptDevice, plDeviceMemoryAllocation* ptBlock)
{
    if(ptBlock->tMemoryMode == PL_MEMORY_GPU)
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

static void
pl__garbage_collect(plDevice* ptDevice)
{
    pl_begin_profile_sample(0, __FUNCTION__);
    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptDevice);

    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptDevice);

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtTextures); i++)
    {
        const uint32_t iResourceIndex = ptGarbage->sbtTextures[i].uIndex;
        plVulkanTexture* ptVulkanResource = &ptDevice->sbtTexturesHot[iResourceIndex];
        vkDestroyImageView(ptDevice->tLogicalDevice, ptDevice->sbtTexturesHot[iResourceIndex].tImageView, NULL);
        ptDevice->sbtTexturesHot[iResourceIndex].tImageView = VK_NULL_HANDLE;
        if(ptDevice->sbtTexturesHot[iResourceIndex].bOriginalView)
        {
            vkDestroyImage(ptDevice->tLogicalDevice, ptVulkanResource->tImage, NULL);
            ptVulkanResource->tImage = VK_NULL_HANDLE;   
        }
        ptDevice->sbtTexturesHot[iResourceIndex].bOriginalView = false;
        pl_sb_push(ptDevice->sbtTextureFreeIndices, iResourceIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtSamplers); i++)
    {
        const uint32_t iResourceIndex = ptGarbage->sbtSamplers[i].uIndex;
        vkDestroySampler(ptDevice->tLogicalDevice, ptDevice->sbtSamplersHot[iResourceIndex], NULL);
        ptDevice->sbtSamplersHot[iResourceIndex] = VK_NULL_HANDLE;
        pl_sb_push(ptDevice->sbtSamplerFreeIndices, iResourceIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtRenderPasses); i++)
    {
        const uint32_t iResourceIndex = ptGarbage->sbtRenderPasses[i].uIndex;
        plVulkanRenderPass* ptVulkanResource = &ptDevice->sbtRenderPassesHot[iResourceIndex];
        for(uint32_t j = 0; j < 3; j++)
        {
            if(ptVulkanResource->atFrameBuffers[j])
                vkDestroyFramebuffer(ptDevice->tLogicalDevice, ptVulkanResource->atFrameBuffers[j], NULL);
            ptVulkanResource->atFrameBuffers[j] = VK_NULL_HANDLE;
        }
        if(ptVulkanResource->tRenderPass)
            vkDestroyRenderPass(ptDevice->tLogicalDevice, ptVulkanResource->tRenderPass, NULL);
        ptVulkanResource->tRenderPass = VK_NULL_HANDLE;
        pl_sb_push(ptDevice->sbtRenderPassFreeIndices, iResourceIndex);
        // ptVulkanResource->sbtFrameBuffers = NULL;
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtRenderPassLayouts); i++)
    {
        const uint32_t iResourceIndex = ptGarbage->sbtRenderPassLayouts[i].uIndex;
        plVulkanRenderPassLayout* ptVulkanResource = &ptDevice->sbtRenderPassLayoutsHot[iResourceIndex];
        vkDestroyRenderPass(ptDevice->tLogicalDevice, ptVulkanResource->tRenderPass, NULL);
        pl_sb_push(ptDevice->sbtRenderPassLayoutFreeIndices, iResourceIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtShaders); i++)
    {
        const uint32_t iResourceIndex = ptGarbage->sbtShaders[i].uIndex;
        plShader* ptResource = &ptDevice->sbtShadersCold[iResourceIndex];
        plVulkanShader* ptVulkanResource = &ptDevice->sbtShadersHot[iResourceIndex];
        if(ptVulkanResource->tVertexShaderModule)
            vkDestroyShaderModule(ptDevice->tLogicalDevice, ptVulkanResource->tVertexShaderModule, NULL);
        if(ptVulkanResource->tPixelShaderModule)
            vkDestroyShaderModule(ptDevice->tLogicalDevice, ptVulkanResource->tPixelShaderModule, NULL);

        ptVulkanResource->tVertexShaderModule = VK_NULL_HANDLE;
        ptVulkanResource->tPixelShaderModule = VK_NULL_HANDLE;

        plVulkanShader* ptVariantVulkanResource = &ptDevice->sbtShadersHot[iResourceIndex];
        vkDestroyPipelineLayout(ptDevice->tLogicalDevice, ptVariantVulkanResource->tPipelineLayout, NULL);
        vkDestroyPipeline(ptDevice->tLogicalDevice, ptVariantVulkanResource->tPipeline, NULL);
        ptVariantVulkanResource->tPipelineLayout = VK_NULL_HANDLE;
        ptVariantVulkanResource->tPipeline = VK_NULL_HANDLE;
        pl_sb_push(ptDevice->sbtShaderFreeIndices, iResourceIndex);
        for(uint32_t k = 0; k < ptResource->tDescription.uBindGroupLayoutCount; k++)
        {
            plVulkanBindGroupLayout* ptVulkanBindGroupLayout = &ptDevice->sbtBindGroupLayouts[ptResource->tDescription.atBindGroupLayouts[k].uHandle];
            vkDestroyDescriptorSetLayout(ptDevice->tLogicalDevice, ptVulkanBindGroupLayout->tDescriptorSetLayout, NULL);   
            ptVulkanBindGroupLayout->tDescriptorSetLayout = VK_NULL_HANDLE;
            pl_sb_push(ptDevice->sbtBindGroupLayoutFreeIndices, ptResource->tDescription.atBindGroupLayouts[k].uHandle);
        }
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtComputeShaders); i++)
    {
        const uint32_t iResourceIndex = ptGarbage->sbtComputeShaders[i].uIndex;
        plComputeShader* ptResource = &ptDevice->sbtComputeShadersCold[iResourceIndex];
        plVulkanComputeShader* ptVulkanResource = &ptDevice->sbtComputeShadersHot[iResourceIndex];
        if(ptVulkanResource->tShaderModule)
            vkDestroyShaderModule(ptDevice->tLogicalDevice, ptVulkanResource->tShaderModule, NULL);

        ptVulkanResource->tShaderModule = VK_NULL_HANDLE;

        plVulkanComputeShader* ptVariantVulkanResource = &ptDevice->sbtComputeShadersHot[iResourceIndex];
        vkDestroyPipelineLayout(ptDevice->tLogicalDevice, ptVariantVulkanResource->tPipelineLayout, NULL);
        vkDestroyPipeline(ptDevice->tLogicalDevice, ptVariantVulkanResource->tPipeline, NULL);
        ptVariantVulkanResource->tPipelineLayout = VK_NULL_HANDLE;
        ptVariantVulkanResource->tPipeline = VK_NULL_HANDLE;
        pl_sb_push(ptDevice->sbtComputeShaderFreeIndices, iResourceIndex);

        for(uint32_t k = 0; k < ptResource->tDescription.uBindGroupLayoutCount; k++)
        {
            plVulkanBindGroupLayout* ptVulkanBindGroupLayout = &ptDevice->sbtBindGroupLayouts[ptResource->tDescription.atBindGroupLayouts[k].uHandle];
            vkDestroyDescriptorSetLayout(ptDevice->tLogicalDevice, ptVulkanBindGroupLayout->tDescriptorSetLayout, NULL);   
            ptVulkanBindGroupLayout->tDescriptorSetLayout = VK_NULL_HANDLE;
            pl_sb_push(ptDevice->sbtBindGroupLayoutFreeIndices, ptResource->tDescription.atBindGroupLayouts[k].uHandle);
        }
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtBindGroups); i++)
    {
        const uint32_t iBindGroupIndex = ptGarbage->sbtBindGroups[i].uIndex;
        plVulkanBindGroup* ptVulkanResource = &ptDevice->sbtBindGroupsHot[iBindGroupIndex];
        ptVulkanResource->tDescriptorSet = VK_NULL_HANDLE;
        vkDestroyDescriptorSetLayout(ptDevice->tLogicalDevice, ptVulkanResource->tDescriptorSetLayout, NULL);
        ptVulkanResource->tDescriptorSetLayout = VK_NULL_HANDLE;
        pl_sb_push(ptDevice->sbtBindGroupFreeIndices, iBindGroupIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptCurrentFrame->sbtRawFrameBuffers); i++)
    {
        vkDestroyFramebuffer(ptDevice->tLogicalDevice, ptCurrentFrame->sbtRawFrameBuffers[i], NULL);
        ptCurrentFrame->sbtRawFrameBuffers[i] = VK_NULL_HANDLE;
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtBuffers); i++)
    {
        const uint32_t iResourceIndex = ptGarbage->sbtBuffers[i].uIndex;
        vkDestroyBuffer(ptDevice->tLogicalDevice, ptDevice->sbtBuffersHot[iResourceIndex].tBuffer, NULL);
        ptDevice->sbtBuffersHot[iResourceIndex].tBuffer = VK_NULL_HANDLE;
        pl_sb_push(ptDevice->sbtBufferFreeIndices, iResourceIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtMemory); i++)
    {
        plDeviceMemoryAllocation tAllocation = ptGarbage->sbtMemory[i];
        plDeviceMemoryAllocatorI* ptAllocator = tAllocation.ptAllocator;
        if(ptAllocator) // swapchain doesn't have allocator since texture is provided
            ptAllocator->free(ptAllocator->ptInst, &tAllocation);
        else
            pl_free_memory(ptDevice, &tAllocation);
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
    pl_end_profile_sample(0);
}

static void
pl_destroy_buffer(plDevice* ptDevice, plBufferHandle tHandle)
{
    vkDestroyBuffer(ptDevice->tLogicalDevice, ptDevice->sbtBuffersHot[tHandle.uIndex].tBuffer, NULL);
    ptDevice->sbtBuffersHot[tHandle.uIndex].tBuffer = VK_NULL_HANDLE;
    ptDevice->sbtBufferGenerations[tHandle.uIndex]++;
    pl_sb_push(ptDevice->sbtBufferFreeIndices, tHandle.uIndex);

    plBuffer* ptBuffer = &ptDevice->sbtBuffersCold[tHandle.uIndex];
    if(ptBuffer->tMemoryAllocation.ptAllocator)
        ptBuffer->tMemoryAllocation.ptAllocator->free(ptBuffer->tMemoryAllocation.ptAllocator->ptInst, &ptBuffer->tMemoryAllocation);
    else
        pl_free_memory(ptDevice, &ptBuffer->tMemoryAllocation);
}

static void
pl_destroy_texture(plDevice* ptDevice, plTextureHandle tHandle)
{
    plVulkanTexture* ptVulkanResource = &ptDevice->sbtTexturesHot[tHandle.uIndex];
    vkDestroyImage(ptDevice->tLogicalDevice, ptVulkanResource->tImage, NULL);
    ptVulkanResource->tImage = VK_NULL_HANDLE;
    pl_sb_push(ptDevice->sbtTextureFreeIndices, tHandle.uIndex);
    ptDevice->sbtTextureGenerations[tHandle.uIndex]++;
    
    plTexture* ptTexture = &ptDevice->sbtTexturesCold[tHandle.uIndex];
    if(ptTexture->_tDrawBindGroup.ulData != UINT64_MAX)
    {
        pl_sb_push(ptDevice->sbtFreeDrawBindGroups, ptTexture->_tDrawBindGroup);
    }
    if(ptTexture->tMemoryAllocation.ptAllocator)
        ptTexture->tMemoryAllocation.ptAllocator->free(ptTexture->tMemoryAllocation.ptAllocator->ptInst, &ptTexture->tMemoryAllocation);
    else
        pl_free_memory(ptDevice, &ptTexture->tMemoryAllocation);
}

static void
pl_destroy_sampler(plDevice* ptDevice, plSamplerHandle tHandle)
{
    vkDestroySampler(ptDevice->tLogicalDevice, ptDevice->sbtSamplersHot[tHandle.uIndex], NULL);
    ptDevice->sbtSamplersHot[tHandle.uIndex] = VK_NULL_HANDLE;
    ptDevice->sbtSamplerGenerations[tHandle.uIndex]++;
    pl_sb_push(ptDevice->sbtSamplerFreeIndices, tHandle.uIndex);
}

static void
pl_destroy_bind_group(plDevice* ptDevice, plBindGroupHandle tHandle)
{
    ptDevice->sbtBindGroupGenerations[tHandle.uIndex]++;

    plVulkanBindGroup* ptVulkanResource = &ptDevice->sbtBindGroupsHot[tHandle.uIndex];
    ptVulkanResource->tDescriptorSet = VK_NULL_HANDLE;
    vkDestroyDescriptorSetLayout(ptDevice->tLogicalDevice, ptVulkanResource->tDescriptorSetLayout, NULL);
    ptVulkanResource->tDescriptorSetLayout = VK_NULL_HANDLE;
    pl_sb_push(ptDevice->sbtBindGroupFreeIndices, tHandle.uIndex);
}

static void
pl_destroy_render_pass(plDevice* ptDevice, plRenderPassHandle tHandle)
{
    ptDevice->sbtRenderPassGenerations[tHandle.uIndex]++;

    plVulkanRenderPass* ptVulkanResource = &ptDevice->sbtRenderPassesHot[tHandle.uIndex];
    for(uint32_t j = 0; j < gptGraphics->uFramesInFlight; j++)
    {
        if(ptVulkanResource->atFrameBuffers[j])
            vkDestroyFramebuffer(ptDevice->tLogicalDevice, ptVulkanResource->atFrameBuffers[j], NULL);
        ptVulkanResource->atFrameBuffers[j] = VK_NULL_HANDLE;
    }
    if(ptVulkanResource->tRenderPass)
        vkDestroyRenderPass(ptDevice->tLogicalDevice, ptVulkanResource->tRenderPass, NULL);
    ptVulkanResource->tRenderPass = VK_NULL_HANDLE;
    pl_sb_push(ptDevice->sbtRenderPassFreeIndices, tHandle.uIndex);
}

static void
pl_destroy_render_pass_layout(plDevice* ptDevice, plRenderPassLayoutHandle tHandle)
{
    ptDevice->sbtRenderPassLayoutGenerations[tHandle.uIndex]++;

    plVulkanRenderPassLayout* ptVulkanResource = &ptDevice->sbtRenderPassLayoutsHot[tHandle.uIndex];
    vkDestroyRenderPass(ptDevice->tLogicalDevice, ptVulkanResource->tRenderPass, NULL);
    pl_sb_push(ptDevice->sbtRenderPassLayoutFreeIndices, tHandle.uIndex);
}

static void
pl_destroy_shader(plDevice* ptDevice, plShaderHandle tHandle)
{
    ptDevice->sbtShaderGenerations[tHandle.uIndex]++;

    plShader* ptResource = &ptDevice->sbtShadersCold[tHandle.uIndex];

    plVulkanShader* ptVariantVulkanResource = &ptDevice->sbtShadersHot[tHandle.uIndex];
    vkDestroyPipelineLayout(ptDevice->tLogicalDevice, ptVariantVulkanResource->tPipelineLayout, NULL);
    vkDestroyPipeline(ptDevice->tLogicalDevice, ptVariantVulkanResource->tPipeline, NULL);
    ptVariantVulkanResource->tPipelineLayout = VK_NULL_HANDLE;
    ptVariantVulkanResource->tPipeline = VK_NULL_HANDLE;
    pl_sb_push(ptDevice->sbtShaderFreeIndices, tHandle.uIndex);
    for(uint32_t k = 0; k < ptResource->tDescription.uBindGroupLayoutCount; k++)
    {
        plVulkanBindGroupLayout* ptVulkanBindGroupLayout = &ptDevice->sbtBindGroupLayouts[ptResource->tDescription.atBindGroupLayouts[k].uHandle];
        vkDestroyDescriptorSetLayout(ptDevice->tLogicalDevice, ptVulkanBindGroupLayout->tDescriptorSetLayout, NULL);   
        ptVulkanBindGroupLayout->tDescriptorSetLayout = VK_NULL_HANDLE;
        pl_sb_push(ptDevice->sbtBindGroupLayoutFreeIndices, ptResource->tDescription.atBindGroupLayouts[k].uHandle);
    }
}

static void
pl_destroy_compute_shader(plDevice* ptDevice, plComputeShaderHandle tHandle)
{
    ptDevice->sbtComputeShaderGenerations[tHandle.uIndex]++;

    plComputeShader* ptResource = &ptDevice->sbtComputeShadersCold[tHandle.uIndex];

    plVulkanComputeShader* ptVariantVulkanResource = &ptDevice->sbtComputeShadersHot[tHandle.uIndex];
    vkDestroyPipelineLayout(ptDevice->tLogicalDevice, ptVariantVulkanResource->tPipelineLayout, NULL);
    vkDestroyPipeline(ptDevice->tLogicalDevice, ptVariantVulkanResource->tPipeline, NULL);
    ptVariantVulkanResource->tPipelineLayout = VK_NULL_HANDLE;
    ptVariantVulkanResource->tPipeline = VK_NULL_HANDLE;
    pl_sb_push(ptDevice->sbtComputeShaderFreeIndices, tHandle.uIndex);

    for(uint32_t k = 0; k < ptResource->tDescription.uBindGroupLayoutCount + 1; k++)
    {
        plVulkanBindGroupLayout* ptVulkanBindGroupLayout = &ptDevice->sbtBindGroupLayouts[ptResource->tDescription.atBindGroupLayouts[k].uHandle];
        vkDestroyDescriptorSetLayout(ptDevice->tLogicalDevice, ptVulkanBindGroupLayout->tDescriptorSetLayout, NULL);   
        ptVulkanBindGroupLayout->tDescriptorSetLayout = VK_NULL_HANDLE;
        pl_sb_push(ptDevice->sbtBindGroupLayoutFreeIndices, ptResource->tDescription.atBindGroupLayouts[k].uHandle);
    }
}