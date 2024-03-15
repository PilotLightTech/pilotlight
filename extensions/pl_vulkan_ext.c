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
// [SECTION] unity build
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

#include "pl_ui.h"
#include "pl_ui_vulkan.h"
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

const plFileApiI* gptFile = NULL;
static uint32_t uLogChannel = UINT32_MAX;

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _pl3DVulkanPipelineEntry
{    
    VkRenderPass          tRenderPass;
    VkSampleCountFlagBits tMSAASampleCount;
    VkPipeline            tRegularPipeline;
    VkPipeline            tSecondaryPipeline;
    pl3DDrawFlags         tFlags;
} pl3DVulkanPipelineEntry;

typedef struct _pl3DVulkanBufferInfo
{
    // vertex buffer
    VkBuffer                  tVertexBuffer;
    plDeviceMemoryAllocation  tVertexMemory;
    uint32_t                  uVertexBufferOffset;

    // index buffer
    VkBuffer                 tIndexBuffer;
    plDeviceMemoryAllocation tIndexMemory;
    uint32_t                 uIndexBufferOffset;
} pl3DVulkanBufferInfo;

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
    VkImage tImage;
} plVulkanTexture;

typedef struct _plVulkanRenderPassLayout
{
    VkRenderPass tRenderPass;
} plVulkanRenderPassLayout;

typedef struct _plVulkanRenderPass
{
    VkRenderPass   tRenderPass;
    VkFramebuffer* sbtFrameBuffers;
} plVulkanRenderPass;

typedef struct _plVulkanSampler
{
    VkSampler   tSampler;
    VkImageView tImageView;
} plVulkanSampler;

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
} plVulkanComputeShader;

typedef struct _plFrameContext
{
    VkSemaphore           tImageAvailable;
    VkSemaphore           tRenderFinish;
    VkFence               tInFlight;
    VkCommandPool         tCmdPool;
    VkCommandBuffer       tCmdBuf;
    VkBuffer*             sbtRawBuffers;
    VkFramebuffer*        sbtRawFrameBuffers;
    VkDescriptorPool      tDynamicDescriptorPool;

    // dynamic buffer stuff
    uint32_t               uCurrentBufferIndex;
    plVulkanDynamicBuffer* sbtDynamicBuffers;
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

    plVulkanTexture*          sbtTexturesHot;
    plVulkanSampler*          sbtSamplersHot;
    plVulkanBindGroup*        sbtBindGroupsHot;
    plVulkanBuffer*           sbtBuffersHot;
    plVulkanShader*           sbtShadersHot;
    plVulkanComputeShader*    sbtComputeShadersHot;
    plVulkanRenderPass*       sbtRenderPassesHot;
    plVulkanRenderPassLayout* sbtRenderPassLayoutsHot;
    plVulkanBindGroupLayout*  sbtBindGroupLayouts;
    uint32_t*                 sbtBindGroupLayoutFreeIndices;
    
    VkDescriptorSetLayout tDynamicDescriptorSetLayout;
    
    // drawing

    // vertex & index buffer
    pl3DVulkanBufferInfo* sbt3DBufferInfo;
    pl3DVulkanBufferInfo* sbtLineBufferInfo;

    // 3D drawlist pipeline caching
    VkPipelineLayout                t3DPipelineLayout;
    VkPipelineShaderStageCreateInfo t3DPxlShdrStgInfo;
    VkPipelineShaderStageCreateInfo t3DVtxShdrStgInfo;

    // 3D line drawlist pipeline caching
    VkPipelineLayout                t3DLinePipelineLayout;
    VkPipelineShaderStageCreateInfo t3DLineVtxShdrStgInfo;

    // pipelines
    pl3DVulkanPipelineEntry* sbt3DPipelines;
} plVulkanGraphics;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

// conversion between pilotlight & vulkan types
static VkFilter                            pl__vulkan_filter    (plFilter tFilter);
static VkSamplerAddressMode                pl__vulkan_wrap      (plWrapMode tWrap);
static VkCompareOp                         pl__vulkan_compare   (plCompareMode tCompare);
static VkFormat                            pl__vulkan_format    (plFormat tFormat);
static VkImageLayout                       pl__vulkan_layout    (plTextureLayout tLayout);
static VkAttachmentLoadOp                  pl__vulkan_load_op   (plLoadOp tOp);
static VkAttachmentStoreOp                 pl__vulkan_store_op  (plStoreOp tOp);
static VkCullModeFlags                     pl__vulkan_cull      (plCullMode tFlag);
static VkShaderStageFlagBits               pl__vulkan_stage_flags(plStageFlags tFlags);
static plFormat                            pl__pilotlight_format(VkFormat tFormat);
static VkPipelineColorBlendAttachmentState pl__get_blend_state(plBlendMode tBlendMode);

// 3D drawing helpers
static pl3DVulkanPipelineEntry* pl__get_3d_pipelines(plGraphics* ptGfx, VkRenderPass tRenderPass, VkSampleCountFlagBits tMSAASampleCount, pl3DDrawFlags tFlags);

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

// device memory allocators
static plDeviceMemoryAllocation pl_allocate_dedicated(struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName);
static void                     pl_free_dedicated    (struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation);

static plDeviceMemoryAllocation pl_allocate_buddy(struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName);

static plDeviceMemoryAllocation pl_allocate_staging_uncached         (struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName);
static void                     pl_free_staging_uncached             (struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation);

static plDeviceMemoryAllocation pl_allocate_staging_cached         (struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName);
static void                     pl_free_staging_cached             (struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation);

static VKAPI_ATTR VkBool32 VKAPI_CALL pl__debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT tMsgSeverity, VkDebugUtilsMessageTypeFlagsEXT tMsgType, const VkDebugUtilsMessengerCallbackDataEXT* ptCallbackData, void* pUserData);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

static void*
pl_get_ui_texture_handle(plGraphics* ptGraphics, plTextureViewHandle tHandle)
{
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanSampler* ptView = &ptVulkanGfx->sbtSamplersHot[tHandle.uIndex];
    return pl_add_texture(ptView->tImageView, ptView->tSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

static void
pl__submit_3d_drawlist(plDrawList3D* ptDrawlist, float fWidth, float fHeight, const plMat4* ptMVP, pl3DDrawFlags tFlags, plRenderPassHandle tPass, uint32_t uMSAASampleCount)
{
    plGraphics* ptGfx = ptDrawlist->ptGraphics;
    plVulkanGraphics* ptVulkanGfx = ptGfx->_pInternalData;
    plVulkanDevice* ptVulkanDevice = ptGfx->tDevice._pInternalData;

    pl3DVulkanPipelineEntry* tPipelineEntry = pl__get_3d_pipelines(ptGfx, ptVulkanGfx->sbtRenderPassesHot[tPass.uIndex].tRenderPass, uMSAASampleCount, tFlags);
    const float fAspectRatio = fWidth / fHeight;

    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptGfx);

    // regular 3D
    if(pl_sb_size(ptDrawlist->sbtSolidVertexBuffer) > 0u)
    {
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~vertex buffer prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // ensure gpu vertex buffer size is adequate
        const uint32_t uVtxBufSzNeeded = sizeof(plDrawVertex3DSolid) * pl_sb_size(ptDrawlist->sbtSolidVertexBuffer);

        pl3DVulkanBufferInfo* ptBufferInfo = &ptVulkanGfx->sbt3DBufferInfo[ptGfx->uCurrentFrameIndex];

        // space left in vertex buffer
        const uint32_t uAvailableVertexBufferSpace = (uint32_t)ptBufferInfo->tVertexMemory.ulSize - ptBufferInfo->uVertexBufferOffset;

        // grow buffer if not enough room
        if(uVtxBufSzNeeded >= uAvailableVertexBufferSpace)
        {
            if(ptBufferInfo->tVertexBuffer)
            {
                ptGfx->tDevice.tStagingUnCachedAllocator.free(ptGfx->tDevice.tStagingUnCachedAllocator.ptInst, &ptBufferInfo->tVertexMemory);
                pl_sb_push(ptCurrentFrame->sbtRawBuffers, ptBufferInfo->tVertexBuffer);
            }

            const VkBufferCreateInfo tBufferCreateInfo = {
                .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size        = pl_max(PL_DEVICE_ALLOCATION_BLOCK_SIZE, ptBufferInfo->tVertexMemory.ulSize * 2),
                .usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE
            };
            PL_VULKAN(vkCreateBuffer(ptVulkanDevice->tLogicalDevice, &tBufferCreateInfo, NULL, &ptBufferInfo->tVertexBuffer));
            

            VkMemoryRequirements tMemoryRequirements = {0};
            vkGetBufferMemoryRequirements(ptVulkanDevice->tLogicalDevice, ptBufferInfo->tVertexBuffer, &tMemoryRequirements);

            char acBuffer[256] = {0};
            pl_sprintf(acBuffer, "3D-SOLID_VTX-F%d", (int)ptGfx->uCurrentFrameIndex);
            pl_set_vulkan_object_name(&ptGfx->tDevice, (uint64_t)ptBufferInfo->tVertexBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, acBuffer);
            ptBufferInfo->tVertexMemory = ptGfx->tDevice.tStagingUnCachedAllocator.allocate(ptGfx->tDevice.tStagingUnCachedAllocator.ptInst, tMemoryRequirements.memoryTypeBits, tMemoryRequirements.size, tMemoryRequirements.alignment, acBuffer);
            PL_VULKAN(vkBindBufferMemory(ptVulkanDevice->tLogicalDevice, ptBufferInfo->tVertexBuffer, (VkDeviceMemory)ptBufferInfo->tVertexMemory.uHandle, ptBufferInfo->tVertexMemory.ulOffset));
        }

        // vertex GPU data transfer
        char* pucMappedVertexBufferLocation = ptBufferInfo->tVertexMemory.pHostMapped;
        memcpy(&pucMappedVertexBufferLocation[ptBufferInfo->uVertexBufferOffset], ptDrawlist->sbtSolidVertexBuffer, sizeof(plDrawVertex3DSolid) * pl_sb_size(ptDrawlist->sbtSolidVertexBuffer));

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~index buffer prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // ensure gpu index buffer size is adequate
        const uint32_t uIdxBufSzNeeded = sizeof(uint32_t) * pl_sb_size(ptDrawlist->sbtSolidIndexBuffer);

        // space left in index buffer
        const uint32_t uAvailableIndexBufferSpace = (uint32_t)ptBufferInfo->tIndexMemory.ulSize - ptBufferInfo->uIndexBufferOffset;

        if(uIdxBufSzNeeded >= uAvailableIndexBufferSpace)
        {
            if(ptBufferInfo->tIndexBuffer)
            {
                ptGfx->tDevice.tStagingUnCachedAllocator.free(ptGfx->tDevice.tStagingUnCachedAllocator.ptInst, &ptBufferInfo->tIndexMemory);
                pl_sb_push(ptCurrentFrame->sbtRawBuffers, ptBufferInfo->tIndexBuffer);
            }

            const VkBufferCreateInfo tBufferCreateInfo = {
                .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size        = pl_max(PL_DEVICE_ALLOCATION_BLOCK_SIZE, ptBufferInfo->tIndexMemory.ulSize * 2),
                .usage       = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE
            };
            PL_VULKAN(vkCreateBuffer(ptVulkanDevice->tLogicalDevice, &tBufferCreateInfo, NULL, &ptBufferInfo->tIndexBuffer));

            VkMemoryRequirements tMemoryRequirements = {0};
            vkGetBufferMemoryRequirements(ptVulkanDevice->tLogicalDevice, ptBufferInfo->tIndexBuffer, &tMemoryRequirements);

            char acBuffer[256] = {0};
            pl_sprintf(acBuffer, "3D-SOLID_IDX-F%d", (int)ptGfx->uCurrentFrameIndex);
            pl_set_vulkan_object_name(&ptGfx->tDevice, (uint64_t)ptBufferInfo->tIndexBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, acBuffer);
            ptBufferInfo->tIndexMemory = ptGfx->tDevice.tStagingUnCachedAllocator.allocate(ptGfx->tDevice.tStagingUnCachedAllocator.ptInst, tMemoryRequirements.memoryTypeBits, tMemoryRequirements.size, tMemoryRequirements.alignment, acBuffer);
            PL_VULKAN(vkBindBufferMemory(ptVulkanDevice->tLogicalDevice, ptBufferInfo->tIndexBuffer, (VkDeviceMemory)ptBufferInfo->tIndexMemory.uHandle, ptBufferInfo->tIndexMemory.ulOffset));
        }

        // index GPU data transfer
        char* pucMappedIndexBufferLocation = ptBufferInfo->tIndexMemory.pHostMapped;
        memcpy(&pucMappedIndexBufferLocation[ptBufferInfo->uIndexBufferOffset], ptDrawlist->sbtSolidIndexBuffer, sizeof(uint32_t) * pl_sb_size(ptDrawlist->sbtSolidIndexBuffer));
        
        const VkMappedMemoryRange aRange[2] = {
            {
                .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                .memory = (VkDeviceMemory)ptBufferInfo->tVertexMemory.uHandle,
                .size = VK_WHOLE_SIZE
            },
            {
                .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                .memory = (VkDeviceMemory)ptBufferInfo->tIndexMemory.uHandle,
                .size = VK_WHOLE_SIZE
            }
        };
        PL_VULKAN(vkFlushMappedMemoryRanges(ptVulkanDevice->tLogicalDevice, 2, aRange));

        static const VkDeviceSize tOffsets = { 0u };
        vkCmdBindIndexBuffer(ptCurrentFrame->tCmdBuf, ptBufferInfo->tIndexBuffer, 0u, VK_INDEX_TYPE_UINT32);
        vkCmdBindVertexBuffers(ptCurrentFrame->tCmdBuf, 0, 1, &ptBufferInfo->tVertexBuffer, &tOffsets);

        const int32_t iVertexOffset = ptBufferInfo->uVertexBufferOffset / sizeof(plDrawVertex3DSolid);
        const int32_t iIndexOffset = ptBufferInfo->uIndexBufferOffset / sizeof(uint32_t);

        vkCmdBindPipeline(ptCurrentFrame->tCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, tPipelineEntry->tRegularPipeline); 
        vkCmdPushConstants(ptCurrentFrame->tCmdBuf, ptVulkanGfx->t3DPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 16, ptMVP);
        vkCmdDrawIndexed(ptCurrentFrame->tCmdBuf, pl_sb_size(ptDrawlist->sbtSolidIndexBuffer), 1, iIndexOffset, iVertexOffset, 0);
        
        // bump vertex & index buffer offset
        ptBufferInfo->uVertexBufferOffset += uVtxBufSzNeeded;
        ptBufferInfo->uIndexBufferOffset += uIdxBufSzNeeded;
    }

    // 3D lines
    if(pl_sb_size(ptDrawlist->sbtLineVertexBuffer) > 0u)
    {
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~vertex buffer prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // ensure gpu vertex buffer size is adequate
        const uint32_t uVtxBufSzNeeded = sizeof(plDrawVertex3DLine) * pl_sb_size(ptDrawlist->sbtLineVertexBuffer);

        pl3DVulkanBufferInfo* ptBufferInfo = &ptVulkanGfx->sbtLineBufferInfo[ptGfx->uCurrentFrameIndex];

        // space left in vertex buffer
        const uint32_t uAvailableVertexBufferSpace = (uint32_t)ptBufferInfo->tVertexMemory.ulSize - ptBufferInfo->uVertexBufferOffset;

        // grow buffer if not enough room
        if(uVtxBufSzNeeded >= uAvailableVertexBufferSpace)
        {
            if(ptBufferInfo->tVertexBuffer)
            {
                ptGfx->tDevice.tStagingUnCachedAllocator.free(ptGfx->tDevice.tStagingUnCachedAllocator.ptInst, &ptBufferInfo->tVertexMemory);
                pl_sb_push(ptCurrentFrame->sbtRawBuffers, ptBufferInfo->tVertexBuffer);
            }

            const VkBufferCreateInfo tBufferCreateInfo = {
                .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size        = pl_max(PL_DEVICE_ALLOCATION_BLOCK_SIZE, ptBufferInfo->tVertexMemory.ulSize * 2),
                .usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE
            };
            // PL_DEVICE_ALLOCATION_BLOCK_SIZE
            PL_VULKAN(vkCreateBuffer(ptVulkanDevice->tLogicalDevice, &tBufferCreateInfo, NULL, &ptBufferInfo->tVertexBuffer));

            VkMemoryRequirements tMemoryRequirements = {0};
            vkGetBufferMemoryRequirements(ptVulkanDevice->tLogicalDevice, ptBufferInfo->tVertexBuffer, &tMemoryRequirements);

            char acBuffer[256] = {0};
            pl_sprintf(acBuffer, "3D-LINE_VTX-F%d", (int)ptGfx->uCurrentFrameIndex);
            pl_set_vulkan_object_name(&ptGfx->tDevice, (uint64_t)ptBufferInfo->tVertexBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, acBuffer);
            ptBufferInfo->tVertexMemory = ptGfx->tDevice.tStagingUnCachedAllocator.allocate(ptGfx->tDevice.tStagingUnCachedAllocator.ptInst, tMemoryRequirements.memoryTypeBits, tMemoryRequirements.size, tMemoryRequirements.alignment, acBuffer);
            PL_VULKAN(vkBindBufferMemory(ptVulkanDevice->tLogicalDevice, ptBufferInfo->tVertexBuffer, (VkDeviceMemory)ptBufferInfo->tVertexMemory.uHandle, ptBufferInfo->tVertexMemory.ulOffset));
        }

        // vertex GPU data transfer
        char* pucMappedVertexBufferLocation = ptBufferInfo->tVertexMemory.pHostMapped;
        memcpy(&pucMappedVertexBufferLocation[ptBufferInfo->uVertexBufferOffset], ptDrawlist->sbtLineVertexBuffer, sizeof(plDrawVertex3DLine) * pl_sb_size(ptDrawlist->sbtLineVertexBuffer));

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~index buffer prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // ensure gpu index buffer size is adequate
        const uint32_t uIdxBufSzNeeded = sizeof(uint32_t) * pl_sb_size(ptDrawlist->sbtLineIndexBuffer);

        // space left in index buffer
        const uint32_t uAvailableIndexBufferSpace = (uint32_t)ptBufferInfo->tIndexMemory.ulSize - ptBufferInfo->uIndexBufferOffset;

        if(uIdxBufSzNeeded >= uAvailableIndexBufferSpace)
        {
            if(ptBufferInfo->tIndexBuffer)
            {
                ptGfx->tDevice.tStagingUnCachedAllocator.free(ptGfx->tDevice.tStagingUnCachedAllocator.ptInst, &ptBufferInfo->tIndexMemory);
                pl_sb_push(ptCurrentFrame->sbtRawBuffers, ptBufferInfo->tIndexBuffer);
            }

            const VkBufferCreateInfo tBufferCreateInfo = {
                .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size        = pl_max(PL_DEVICE_ALLOCATION_BLOCK_SIZE, ptBufferInfo->tIndexMemory.ulSize * 2),
                .usage       = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE
            };
            PL_VULKAN(vkCreateBuffer(ptVulkanDevice->tLogicalDevice, &tBufferCreateInfo, NULL, &ptBufferInfo->tIndexBuffer));

            VkMemoryRequirements tMemoryRequirements = {0};
            vkGetBufferMemoryRequirements(ptVulkanDevice->tLogicalDevice, ptBufferInfo->tIndexBuffer, &tMemoryRequirements);

            char acBuffer[256] = {0};
            pl_sprintf(acBuffer, "3D-LINE_IDX-F%d", (int)ptGfx->uCurrentFrameIndex);
            pl_set_vulkan_object_name(&ptGfx->tDevice, (uint64_t)ptBufferInfo->tIndexBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, acBuffer);
            ptBufferInfo->tIndexMemory = ptGfx->tDevice.tStagingUnCachedAllocator.allocate(ptGfx->tDevice.tStagingUnCachedAllocator.ptInst, tMemoryRequirements.memoryTypeBits, tMemoryRequirements.size, tMemoryRequirements.alignment, acBuffer);
            PL_VULKAN(vkBindBufferMemory(ptVulkanDevice->tLogicalDevice, ptBufferInfo->tIndexBuffer, (VkDeviceMemory)ptBufferInfo->tIndexMemory.uHandle, ptBufferInfo->tIndexMemory.ulOffset));
        }

        // index GPU data transfer
        char* pucMappedIndexBufferLocation = ptBufferInfo->tIndexMemory.pHostMapped;
        memcpy(&pucMappedIndexBufferLocation[ptBufferInfo->uIndexBufferOffset], ptDrawlist->sbtLineIndexBuffer, sizeof(uint32_t) * pl_sb_size(ptDrawlist->sbtLineIndexBuffer));
        
        const VkMappedMemoryRange aRange[2] = {
            {
                .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                .memory = (VkDeviceMemory)ptBufferInfo->tVertexMemory.uHandle,
                .size = VK_WHOLE_SIZE
            },
            {
                .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                .memory = (VkDeviceMemory)ptBufferInfo->tIndexMemory.uHandle,
                .size = VK_WHOLE_SIZE
            }
        };
        PL_VULKAN(vkFlushMappedMemoryRanges(ptVulkanDevice->tLogicalDevice, 2, aRange));

        static const VkDeviceSize tOffsets = { 0u };
        vkCmdBindIndexBuffer(ptCurrentFrame->tCmdBuf, ptBufferInfo->tIndexBuffer, 0u, VK_INDEX_TYPE_UINT32);
        vkCmdBindVertexBuffers(ptCurrentFrame->tCmdBuf, 0, 1, &ptBufferInfo->tVertexBuffer, &tOffsets);

        const int32_t iVertexOffset = ptBufferInfo->uVertexBufferOffset / sizeof(plDrawVertex3DLine);
        const int32_t iIndexOffset = ptBufferInfo->uIndexBufferOffset / sizeof(uint32_t);

        vkCmdBindPipeline(ptCurrentFrame->tCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, tPipelineEntry->tSecondaryPipeline); 
        vkCmdPushConstants(ptCurrentFrame->tCmdBuf, ptVulkanGfx->t3DLinePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 16, ptMVP);
        vkCmdPushConstants(ptCurrentFrame->tCmdBuf, ptVulkanGfx->t3DLinePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 16, sizeof(float), &fAspectRatio);
        vkCmdDrawIndexed(ptCurrentFrame->tCmdBuf, pl_sb_size(ptDrawlist->sbtLineIndexBuffer), 1, iIndexOffset, iVertexOffset, 0);
        
        // bump vertex & index buffer offset
        ptBufferInfo->uVertexBufferOffset += uVtxBufSzNeeded;
        ptBufferInfo->uIndexBufferOffset += uIdxBufSzNeeded;
    }
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
    if(ptDesc->tUsage == PL_BUFFER_USAGE_VERTEX)
        tBufferInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if(ptDesc->tUsage == PL_BUFFER_USAGE_INDEX)
        tBufferInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if(ptDesc->tUsage == PL_BUFFER_USAGE_STORAGE)
        tBufferInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if(ptDesc->tUsage == PL_BUFFER_USAGE_UNIFORM)
        tBufferInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    VkMemoryRequirements tMemRequirements = {0};

    if(ptDesc->tMemory == PL_MEMORY_GPU_CPU)
        tBufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    else if(ptDesc->tMemory == PL_MEMORY_GPU)
        tBufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    else if(ptDesc->tMemory == PL_MEMORY_CPU)
        tBufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    PL_VULKAN(vkCreateBuffer(ptVulkanDevice->tLogicalDevice, &tBufferInfo, NULL, &tVulkanBuffer.tBuffer));
    if(pcName)
        pl_set_vulkan_object_name(ptDevice, (uint64_t)tVulkanBuffer.tBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, pcName);
    vkGetBufferMemoryRequirements(ptVulkanDevice->tLogicalDevice, tVulkanBuffer.tBuffer, &tMemRequirements);

    if(ptDesc->tMemory == PL_MEMORY_GPU_CPU)
        tBuffer.tMemoryAllocation = ptDevice->tStagingUnCachedAllocator.allocate(ptDevice->tStagingUnCachedAllocator.ptInst, tMemRequirements.memoryTypeBits, tMemRequirements.size, tMemRequirements.alignment, tBuffer.tDescription.acDebugName);
    else if(ptDesc->tMemory == PL_MEMORY_CPU)
        tBuffer.tMemoryAllocation = ptDevice->tStagingCachedAllocator.allocate(ptDevice->tStagingCachedAllocator.ptInst, tMemRequirements.memoryTypeBits, tMemRequirements.size, tMemRequirements.alignment, tBuffer.tDescription.acDebugName);
    else
    {
        plDeviceMemoryAllocatorI* ptAllocator = tMemRequirements.size > PL_DEVICE_BUDDY_BLOCK_SIZE ? &ptDevice->tLocalDedicatedAllocator : &ptDevice->tLocalBuddyAllocator;
        tBuffer.tMemoryAllocation = ptAllocator->allocate(ptAllocator->ptInst, tMemRequirements.memoryTypeBits, tMemRequirements.size, tMemRequirements.alignment, tBuffer.tDescription.acDebugName);
    }

    PL_VULKAN(vkBindBufferMemory(ptVulkanDevice->tLogicalDevice, tVulkanBuffer.tBuffer, (VkDeviceMemory)tBuffer.tMemoryAllocation.uHandle, tBuffer.tMemoryAllocation.ulOffset));
    tVulkanBuffer.pcData = tBuffer.tMemoryAllocation.pHostMapped;

    ptVulkanGraphics->sbtBuffersHot[uBufferIndex] = tVulkanBuffer;
    ptGraphics->sbtBuffersCold[uBufferIndex] = tBuffer;
    return tHandle;
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
        ptDynamicBuffer = &ptFrame->sbtDynamicBuffers[0];
        ptDynamicBuffer->uByteOffset = 0;
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
                .tMemory              = PL_MEMORY_GPU_CPU,
                .tUsage               = PL_BUFFER_USAGE_UNIFORM,
                .uByteSize            = PL_DEVICE_ALLOCATION_BLOCK_SIZE
            };
            pl_sprintf(tStagingBufferDescription0.acDebugName, "D-BUF-F%d-%d", (int)ptGraphics->uCurrentFrameIndex, (int)ptFrame->uCurrentBufferIndex);

            plBufferHandle tStagingBuffer0 = pl_create_buffer(&ptGraphics->tDevice, &tStagingBufferDescription0, NULL);

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
pl_transfer_image_to_buffer(plDevice* ptDevice, plTextureHandle tTexture, plBufferHandle tBuffer)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptGraphics->_pInternalData;

    const plTexture* ptTexture = pl__get_texture(ptDevice, tTexture);
    const plVulkanTexture* ptVulkanTexture = &ptVulkanGraphics->sbtTexturesHot[tTexture.uIndex];
    const plVulkanBuffer* ptVulkanBuffer = &ptVulkanGraphics->sbtBuffersHot[tBuffer.uIndex];

    const VkImageSubresourceLayers tSubResource = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1
    };

    const VkBufferImageCopy tCopyRegion = {
        .bufferImageHeight = (uint32_t)ptTexture->tDesc.tDimensions.y,
        .bufferOffset = 0,
        .bufferRowLength = (uint32_t)ptTexture->tDesc.tDimensions.x,
        .imageSubresource = tSubResource,
        .imageExtent = {
            .width  = (uint32_t)ptTexture->tDesc.tDimensions.x,
            .height = (uint32_t)ptTexture->tDesc.tDimensions.y,
            .depth  = (uint32_t)ptTexture->tDesc.tDimensions.z
        }
    };

    const VkImageSubresourceRange tSubResourceRange = {
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1
    };

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

    pl__transition_image_layout(tCommandBuffer, ptVulkanTexture->tImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, tSubResourceRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    vkCmdCopyImageToBuffer(tCommandBuffer, ptVulkanTexture->tImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, ptVulkanBuffer->tBuffer, 1, &tCopyRegion);
    pl__transition_image_layout(tCommandBuffer, ptVulkanTexture->tImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, tSubResourceRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    PL_VULKAN(vkEndCommandBuffer(tCommandBuffer));
    const VkSubmitInfo tSubmitInfo = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1u,
        .pCommandBuffers    = &tCommandBuffer,
    };

    PL_VULKAN(vkQueueSubmit(ptVulkanDevice->tGraphicsQueue, 1, &tSubmitInfo, VK_NULL_HANDLE));
    PL_VULKAN(vkDeviceWaitIdle(ptVulkanDevice->tLogicalDevice));
    vkFreeCommandBuffers(ptVulkanDevice->tLogicalDevice, ptVulkanDevice->tCmdPool, 1, &tCommandBuffer);
}

static void
pl_generate_mipmaps(plDevice* ptDevice, plTextureHandle tTexture)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptGraphics->_pInternalData;

    plTexture* ptTexture = &ptGraphics->sbtTexturesCold[tTexture.uIndex];

    // generate mips
    if(ptTexture->tDesc.uMips > 1)
    {

        plFrameContext* ptFrame = pl__get_frame_resources(ptGraphics);


        const VkCommandBufferAllocateInfo tAllocInfo = {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool        = ptFrame->tCmdPool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        VkCommandBuffer tCmdBuffer;
        PL_VULKAN(vkAllocateCommandBuffers(ptVulkanDevice->tLogicalDevice, &tAllocInfo, &tCmdBuffer));  

        const VkCommandBufferBeginInfo tBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
        };
        PL_VULKAN(vkBeginCommandBuffer(tCmdBuffer, &tBeginInfo));   

        // check if format supports linear blitting
        VkFormatProperties tFormatProperties = {0};
        vkGetPhysicalDeviceFormatProperties(ptVulkanDevice->tPhysicalDevice, pl__vulkan_format(ptTexture->tDesc.tFormat), &tFormatProperties);

        plTexture* ptDestTexture = &ptGraphics->sbtTexturesCold[tTexture.uIndex];
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

            PL_VULKAN(vkEndCommandBuffer(tCmdBuffer));   

            // submit
            const VkSubmitInfo tSubmitInfo = {
                .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .waitSemaphoreCount   = 0,
                .pWaitSemaphores      = NULL,
                .pWaitDstStageMask    = NULL,
                .commandBufferCount   = 1,
                .pCommandBuffers      = &tCmdBuffer,
                .signalSemaphoreCount = 0,
                .pSignalSemaphores    = NULL
            };
            PL_VULKAN(vkResetFences(ptVulkanDevice->tLogicalDevice, 1, &ptFrame->tInFlight));
            PL_VULKAN(vkQueueSubmit(ptVulkanDevice->tGraphicsQueue, 1, &tSubmitInfo, ptFrame->tInFlight));  
            PL_VULKAN(vkQueueWaitIdle(ptVulkanDevice->tGraphicsQueue));
        }
        else
        {
            PL_ASSERT(false && "format does not support linear blitting");
        }
    }
}

static plTextureHandle
pl_create_texture(plDevice* ptDevice, plTextureDesc tDesc, const char* pcName)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptGraphics->_pInternalData;

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
        .tDesc = tDesc
    };

    plVulkanTexture tVulkanTexture = {0};

    VkImageViewType tImageViewType = 0;
    if(tDesc.tType == PL_TEXTURE_TYPE_CUBE)
        tImageViewType = VK_IMAGE_VIEW_TYPE_CUBE;
    else if(tDesc.tType == PL_TEXTURE_TYPE_2D)
        tImageViewType = VK_IMAGE_VIEW_TYPE_2D;
    else
    {
        PL_ASSERT(false && "unsupported texture type");
    }
    PL_ASSERT((tDesc.uLayers == 1 || tDesc.uLayers == 6) && "unsupported layer count");

    VkImageUsageFlags tUsageFlags = 0;
    if(tDesc.tUsage & PL_TEXTURE_USAGE_SAMPLED)
        tUsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if(tDesc.tUsage & PL_TEXTURE_USAGE_COLOR_ATTACHMENT)
        tUsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if(tDesc.tUsage & PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT)
        tUsageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if(tDesc.tUsage & PL_TEXTURE_USAGE_TRANSIENT_ATTACHMENT)
        tUsageFlags |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

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

    // allocate memory
    plDeviceMemoryAllocatorI* ptAllocator = tMemoryRequirements.size > PL_DEVICE_BUDDY_BLOCK_SIZE ? &ptDevice->tLocalDedicatedAllocator : &ptDevice->tLocalBuddyAllocator;
    tTexture.tMemoryAllocation = ptAllocator->allocate(ptAllocator->ptInst, tMemoryRequirements.memoryTypeBits, tMemoryRequirements.size, tMemoryRequirements.alignment, pcName);

    PL_VULKAN(vkBindImageMemory(ptVulkanDevice->tLogicalDevice, tVulkanTexture.tImage, (VkDeviceMemory)tTexture.tMemoryAllocation.uHandle, tTexture.tMemoryAllocation.ulOffset));

    // upload data
    ptVulkanGraphics->sbtTexturesHot[uTextureIndex] = tVulkanTexture;
    ptGraphics->sbtTexturesCold[uTextureIndex] = tTexture;

    VkImageAspectFlags tImageAspectFlags = tDesc.tUsage & PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    if(pl__format_has_stencil(pl__vulkan_format(tDesc.tFormat)))
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
        .levelCount     = tDesc.uMips,
        .baseArrayLayer = 0,
        .layerCount     = tDesc.uLayers,
        .aspectMask     = tImageAspectFlags
    };

    if(tDesc.tUsage & PL_TEXTURE_USAGE_SAMPLED)
        pl__transition_image_layout(tCommandBuffer, tVulkanTexture.tImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, tRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    else if(tDesc.tUsage & PL_TEXTURE_USAGE_COLOR_ATTACHMENT)
        pl__transition_image_layout(tCommandBuffer, tVulkanTexture.tImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, tRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    else if(tDesc.tUsage & PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT)
        pl__transition_image_layout(tCommandBuffer, tVulkanTexture.tImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, tRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    PL_VULKAN(vkEndCommandBuffer(tCommandBuffer));
    const VkSubmitInfo tSubmitInfo = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1u,
        .pCommandBuffers    = &tCommandBuffer,
    };

    PL_VULKAN(vkQueueSubmit(ptVulkanDevice->tGraphicsQueue, 1, &tSubmitInfo, VK_NULL_HANDLE));
    PL_VULKAN(vkDeviceWaitIdle(ptVulkanDevice->tLogicalDevice));
    vkFreeCommandBuffers(ptVulkanDevice->tLogicalDevice, ptVulkanDevice->tCmdPool, 1, &tCommandBuffer);
    return tHandle;
}

static plTextureViewHandle
pl_create_swapchain_texture_view(plDevice* ptDevice, const plTextureViewDesc* ptViewDesc, const plSampler* ptSampler, VkImage tImage, const char* pcName)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptGraphics->_pInternalData;

    uint32_t uTextureViewIndex = UINT32_MAX;
    if(pl_sb_size(ptGraphics->sbtTextureViewFreeIndices) > 0)
        uTextureViewIndex = pl_sb_pop(ptGraphics->sbtTextureViewFreeIndices);
    else
    {
        uTextureViewIndex = pl_sb_size(ptGraphics->sbtTextureViewsCold);
        pl_sb_add(ptGraphics->sbtTextureViewsCold);
        pl_sb_push(ptGraphics->sbtTextureViewGenerations, UINT32_MAX);
        pl_sb_add(ptVulkanGraphics->sbtSamplersHot);
    }

    plTextureViewHandle tHandle = {
        .uGeneration = ++ptGraphics->sbtTextureViewGenerations[uTextureViewIndex],
        .uIndex = uTextureViewIndex
    };

    plTextureView tTextureView = {
        .tSampler         = *ptSampler,
        .tTextureViewDesc = *ptViewDesc,
        .tTexture = {
            .uIndex = UINT32_MAX,
            .uGeneration = UINT32_MAX
        }
    };

    plVulkanSampler tVulkanSampler = {0};

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~create view~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    VkImageViewCreateInfo tViewInfo = {
        .sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image                           = tImage,
        .viewType                        = VK_IMAGE_VIEW_TYPE_2D,
        .format                          = pl__vulkan_format(ptViewDesc->tFormat),
        .subresourceRange.baseMipLevel   = ptViewDesc->uBaseMip,
        .subresourceRange.levelCount     = tTextureView.tTextureViewDesc.uMips,
        .subresourceRange.baseArrayLayer = ptViewDesc->uBaseLayer,
        .subresourceRange.layerCount     = ptViewDesc->uLayerCount,
        .subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
    };
    PL_VULKAN(vkCreateImageView(ptVulkanDevice->tLogicalDevice, &tViewInfo, NULL, &tVulkanSampler.tImageView));

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~create sampler~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    VkSamplerCreateInfo tSamplerInfo = {
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter               = pl__vulkan_filter(ptSampler->tFilter),
        .addressModeU            = pl__vulkan_wrap(ptSampler->tHorizontalWrap),
        .addressModeV            = pl__vulkan_wrap(ptSampler->tVerticalWrap),
        .anisotropyEnable        = (bool)ptVulkanDevice->tDeviceFeatures.samplerAnisotropy,
        .maxAnisotropy           = ptVulkanDevice->tDeviceProps.limits.maxSamplerAnisotropy,
        .borderColor             = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable           = VK_FALSE,
        .compareOp               = pl__vulkan_compare(ptSampler->tCompare),
        .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .mipLodBias              = ptSampler->fMipBias,
        .minLod                  = ptSampler->fMinMip,
        .maxLod                  = ptSampler->fMaxMip
    };

    tSamplerInfo.minFilter    = tSamplerInfo.magFilter;
    tSamplerInfo.addressModeW = tSamplerInfo.addressModeU;

    PL_VULKAN(vkCreateSampler(ptVulkanDevice->tLogicalDevice, &tSamplerInfo, NULL, &tVulkanSampler.tSampler));

    ptVulkanGraphics->sbtSamplersHot[uTextureViewIndex] = tVulkanSampler;
    ptGraphics->sbtTextureViewsCold[uTextureViewIndex] = tTextureView;
    return tHandle;
}

static plTextureViewHandle
pl_create_texture_view(plDevice* ptDevice, const plTextureViewDesc* ptViewDesc, const plSampler* ptSampler, plTextureHandle tTextureHandle, const char* pcName)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptGraphics->_pInternalData;

    uint32_t uTextureViewIndex = UINT32_MAX;
    if(pl_sb_size(ptGraphics->sbtTextureViewFreeIndices) > 0)
        uTextureViewIndex = pl_sb_pop(ptGraphics->sbtTextureViewFreeIndices);
    else
    {
        uTextureViewIndex = pl_sb_size(ptGraphics->sbtTextureViewsCold);
        pl_sb_add(ptGraphics->sbtTextureViewsCold);
        pl_sb_push(ptGraphics->sbtTextureViewGenerations, UINT32_MAX);
        pl_sb_add(ptVulkanGraphics->sbtSamplersHot);
    }

    plTextureViewHandle tHandle = {
        .uGeneration = ++ptGraphics->sbtTextureViewGenerations[uTextureViewIndex],
        .uIndex = uTextureViewIndex
    };

    plTexture* ptTexture = pl__get_texture(ptDevice, tTextureHandle);
    plVulkanTexture* ptVulkanTexture = &ptVulkanGraphics->sbtTexturesHot[tTextureHandle.uIndex];


    plTextureView tTextureView = {
        .tSampler         = *ptSampler,
        .tTextureViewDesc = *ptViewDesc,
        .tTexture         = tTextureHandle,
    };

    plVulkanSampler tVulkanSampler = {0};

    if(ptViewDesc->uMips == 0)
        tTextureView.tTextureViewDesc.uMips = ptTexture->tDesc.uMips;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~create view~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const VkImageViewType tImageViewType = ptViewDesc->uLayerCount == 6 ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;    
    PL_ASSERT((ptViewDesc->uLayerCount == 1 || ptViewDesc->uLayerCount == 6) && "unsupported layer count");


    VkImageAspectFlags tImageAspectFlags = ptTexture->tDesc.tUsage & PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    if(pl__format_has_stencil(ptViewDesc->tFormat))
        tImageAspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;

    VkImageViewCreateInfo tViewInfo = {
        .sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image                           = ptVulkanTexture->tImage,
        .viewType                        = tImageViewType,
        .format                          = pl__vulkan_format(ptViewDesc->tFormat),
        .subresourceRange.baseMipLevel   = ptViewDesc->uBaseMip,
        .subresourceRange.levelCount     = tTextureView.tTextureViewDesc.uMips,
        .subresourceRange.baseArrayLayer = ptViewDesc->uBaseLayer,
        .subresourceRange.layerCount     = ptViewDesc->uLayerCount,
        .subresourceRange.aspectMask     = tImageAspectFlags,
    };
    PL_VULKAN(vkCreateImageView(ptVulkanDevice->tLogicalDevice, &tViewInfo, NULL, &tVulkanSampler.tImageView));

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~create sampler~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    VkSamplerCreateInfo tSamplerInfo = {
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter               = pl__vulkan_filter(ptSampler->tFilter),
        .addressModeU            = pl__vulkan_wrap(ptSampler->tHorizontalWrap),
        .addressModeV            = pl__vulkan_wrap(ptSampler->tVerticalWrap),
        .anisotropyEnable        = (bool)ptVulkanDevice->tDeviceFeatures.samplerAnisotropy,
        .maxAnisotropy           = ptVulkanDevice->tDeviceProps.limits.maxSamplerAnisotropy,
        .borderColor             = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable           = VK_FALSE,
        .compareOp               = pl__vulkan_compare(ptSampler->tCompare),
        // .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .mipLodBias              = ptSampler->fMipBias,
        .minLod                  = ptSampler->fMinMip,
        .maxLod                  = ptSampler->fMaxMip,
    };

    if(ptSampler->fMaxMip == PL_MAX_MIPS)
        tSamplerInfo.maxLod = (float)tTextureView.tTextureViewDesc.uMips;

    tSamplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    tSamplerInfo.minFilter    = tSamplerInfo.magFilter;
    tSamplerInfo.addressModeW = tSamplerInfo.addressModeU;

    PL_VULKAN(vkCreateSampler(ptVulkanDevice->tLogicalDevice, &tSamplerInfo, NULL, &tVulkanSampler.tSampler));

    ptVulkanGraphics->sbtSamplersHot[uTextureViewIndex] = tVulkanSampler;
    ptGraphics->sbtTextureViewsCold[uTextureViewIndex] = tTextureView;
    return tHandle;
}

static void
pl_create_bind_group_layout(plDevice* ptDevice, plBindGroupLayout* ptLayout)
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
    const uint32_t uDescriptorBindingCount = ptLayout->uTextureCount + ptLayout->uBufferCount;
    VkDescriptorSetLayoutBinding* atDescriptorSetLayoutBindings = pl_temp_allocator_alloc(&ptVulkanGraphics->tTempAllocator, uDescriptorBindingCount * sizeof(VkDescriptorSetLayoutBinding));
    for(uint32_t i = 0; i < ptLayout->uBufferCount; i++)
    {
        VkDescriptorSetLayoutBinding tBinding =  {
            .binding         = ptLayout->aBuffers[i].uSlot,
            .descriptorType  = ptLayout->aBuffers[i].tType == PL_BUFFER_BINDING_TYPE_STORAGE ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags      = pl__vulkan_stage_flags(ptLayout->aBuffers[i].tStages),
            .pImmutableSamplers = NULL
        };
        atDescriptorSetLayoutBindings[uCurrentBinding++] = tBinding;
    }

    for(uint32_t i = 0 ; i < ptLayout->uTextureCount; i++)
    {
        VkDescriptorSetLayoutBinding tBinding = {
            .binding            = ptLayout->aTextures[i].uSlot,
            .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount    = 1,
            .stageFlags         = pl__vulkan_stage_flags(ptLayout->aTextures[i].tStages),
            .pImmutableSamplers = NULL
        };
        atDescriptorSetLayoutBindings[uCurrentBinding++] = tBinding;
    }

    // create descriptor set layout
    const VkDescriptorSetLayoutCreateInfo tDescriptorSetLayoutInfo = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = uDescriptorBindingCount,
        .pBindings    = atDescriptorSetLayoutBindings,
    };
    PL_VULKAN(vkCreateDescriptorSetLayout(ptVulkanDevice->tLogicalDevice, &tDescriptorSetLayoutInfo, NULL, &tVulkanBindGroupLayout.tDescriptorSetLayout));

    pl_temp_allocator_reset(&ptVulkanGraphics->tTempAllocator);

    ptVulkanGraphics->sbtBindGroupLayouts[uBindGroupLayoutIndex] = tVulkanBindGroupLayout;
}

static plBindGroupHandle
pl_create_bind_group(plDevice* ptDevice, plBindGroupLayout* ptLayout)
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
    const uint32_t uDescriptorBindingCount = ptLayout->uTextureCount + ptLayout->uBufferCount;
    VkDescriptorSetLayoutBinding* atDescriptorSetLayoutBindings = pl_temp_allocator_alloc(&ptVulkanGraphics->tTempAllocator, uDescriptorBindingCount * sizeof(VkDescriptorSetLayoutBinding));

    for(uint32_t i = 0; i < ptLayout->uBufferCount; i++)
    {
        VkDescriptorSetLayoutBinding tBinding =  {
            .binding         = ptLayout->aBuffers[i].uSlot,
            .descriptorType  = ptLayout->aBuffers[i].tType == PL_BUFFER_BINDING_TYPE_STORAGE ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags      = pl__vulkan_stage_flags(ptLayout->aBuffers[i].tStages),
            .pImmutableSamplers = NULL
        };
        atDescriptorSetLayoutBindings[uCurrentBinding++] = tBinding;
    }

    for(uint32_t i = 0 ; i < ptLayout->uTextureCount; i++)
    {
        VkDescriptorSetLayoutBinding tBinding = {
            .binding            = ptLayout->aTextures[i].uSlot,
            .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount    = 1,
            .stageFlags         = pl__vulkan_stage_flags(ptLayout->aTextures[i].tStages),
            .pImmutableSamplers = NULL
        };
        atDescriptorSetLayoutBindings[uCurrentBinding++] = tBinding;
    }

    // create descriptor set layout
    VkDescriptorSetLayout tDescriptorSetLayout = VK_NULL_HANDLE;
    const VkDescriptorSetLayoutCreateInfo tDescriptorSetLayoutInfo = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = uDescriptorBindingCount,
        .pBindings    = atDescriptorSetLayoutBindings,
    };
    PL_VULKAN(vkCreateDescriptorSetLayout(ptVulkanDevice->tLogicalDevice, &tDescriptorSetLayoutInfo, NULL, &tDescriptorSetLayout));

    pl_temp_allocator_reset(&ptVulkanGraphics->tTempAllocator);

    plVulkanBindGroup tVulkanBindGroup = {
        .tDescriptorSetLayout = tDescriptorSetLayout
    };

    // allocate descriptor sets
    const VkDescriptorSetAllocateInfo tAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = ptVulkanGraphics->tDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts        = &tDescriptorSetLayout
    };

    PL_VULKAN(vkAllocateDescriptorSets(ptVulkanDevice->tLogicalDevice, &tAllocInfo, &tVulkanBindGroup.tDescriptorSet));

    ptVulkanGraphics->sbtBindGroupsHot[uBindGroupIndex] = tVulkanBindGroup;
    ptGraphics->sbtBindGroupsCold[uBindGroupIndex] = tBindGroup;
    return tHandle;
}

static plBindGroupHandle
pl_get_temporary_bind_group(plDevice* ptDevice, plBindGroupLayout* ptLayout)
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
    const uint32_t uDescriptorBindingCount = ptLayout->uTextureCount + ptLayout->uBufferCount;
    VkDescriptorSetLayoutBinding* atDescriptorSetLayoutBindings = pl_temp_allocator_alloc(&ptVulkanGraphics->tTempAllocator, uDescriptorBindingCount * sizeof(VkDescriptorSetLayoutBinding));

    for(uint32_t i = 0; i < ptLayout->uBufferCount; i++)
    {
        VkDescriptorSetLayoutBinding tBinding =  {
            .binding         = ptLayout->aBuffers[i].uSlot,
            .descriptorType  = ptLayout->aBuffers[i].tType == PL_BUFFER_BINDING_TYPE_STORAGE ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags      = pl__vulkan_stage_flags(ptLayout->aBuffers[i].tStages),
            .pImmutableSamplers = NULL
        };
        atDescriptorSetLayoutBindings[uCurrentBinding++] = tBinding;
    }

    for(uint32_t i = 0 ; i < ptLayout->uTextureCount; i++)
    {
        VkDescriptorSetLayoutBinding tBinding = {
            .binding            = ptLayout->aTextures[i].uSlot,
            .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount    = 1,
            .stageFlags         = pl__vulkan_stage_flags(ptLayout->aTextures[i].tStages),
            .pImmutableSamplers = NULL
        };
        atDescriptorSetLayoutBindings[uCurrentBinding++] = tBinding;
    }

    // create descriptor set layout
    VkDescriptorSetLayout tDescriptorSetLayout = VK_NULL_HANDLE;
    const VkDescriptorSetLayoutCreateInfo tDescriptorSetLayoutInfo = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = uDescriptorBindingCount,
        .pBindings    = atDescriptorSetLayoutBindings,
    };
    PL_VULKAN(vkCreateDescriptorSetLayout(ptVulkanDevice->tLogicalDevice, &tDescriptorSetLayoutInfo, NULL, &tDescriptorSetLayout));

    pl_temp_allocator_reset(&ptVulkanGraphics->tTempAllocator);

    plVulkanBindGroup tVulkanBindGroup = {
        .tDescriptorSetLayout = tDescriptorSetLayout
    };

    // allocate descriptor sets
    const VkDescriptorSetAllocateInfo tAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = ptCurrentFrame->tDynamicDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts        = &tDescriptorSetLayout
    };

    PL_VULKAN(vkAllocateDescriptorSets(ptVulkanDevice->tLogicalDevice, &tAllocInfo, &tVulkanBindGroup.tDescriptorSet));

    ptVulkanGraphics->sbtBindGroupsHot[uBindGroupIndex] = tVulkanBindGroup;
    ptGraphics->sbtBindGroupsCold[uBindGroupIndex] = tBindGroup;
    pl_queue_bind_group_for_deletion(ptDevice, tHandle);
    return tHandle;
}

static void
pl_update_bind_group(plDevice* ptDevice, plBindGroupHandle* ptGroup, uint32_t uBufferCount, plBufferHandle* atBuffers, size_t* aszBufferRanges, uint32_t uTextureViewCount, plTextureViewHandle* atTextureViews)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptGraphics->_pInternalData;

    plBindGroup* ptBindGroup = &ptGraphics->sbtBindGroupsCold[ptGroup->uIndex];
    plVulkanBindGroup* ptVulkanBindGroup = &ptVulkanGraphics->sbtBindGroupsHot[ptGroup->uIndex];

    VkWriteDescriptorSet*   sbtWrites = pl_temp_allocator_alloc(&ptVulkanGraphics->tTempAllocator, (uBufferCount + uTextureViewCount) * sizeof(VkWriteDescriptorSet));
    VkDescriptorBufferInfo* sbtBufferDescInfos = pl_temp_allocator_alloc(&ptVulkanGraphics->tTempAllocator, uBufferCount * sizeof(VkDescriptorBufferInfo));
    VkDescriptorImageInfo*  sbtImageDescInfos = pl_temp_allocator_alloc(&ptVulkanGraphics->tTempAllocator, uTextureViewCount * sizeof(VkDescriptorImageInfo));


    static const VkDescriptorType atDescriptorTypeLUT[] =
    {
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    };

    uint32_t uCurrentWrite = 0;
    for(uint32_t i = 0 ; i < uBufferCount; i++)
    {

        const plVulkanBuffer* ptVulkanBuffer = &ptVulkanGraphics->sbtBuffersHot[atBuffers[i].uIndex];

        sbtBufferDescInfos[i].buffer = ptVulkanBuffer->tBuffer;
        sbtBufferDescInfos[i].offset = 0;
        sbtBufferDescInfos[i].range  = aszBufferRanges[i];

        sbtWrites[uCurrentWrite].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sbtWrites[uCurrentWrite].dstBinding      = ptBindGroup->tLayout.aBuffers[i].uSlot;
        sbtWrites[uCurrentWrite].dstArrayElement = 0;
        sbtWrites[uCurrentWrite].descriptorType  = atDescriptorTypeLUT[ptBindGroup->tLayout.aBuffers[i].tType - 1];
        sbtWrites[uCurrentWrite].descriptorCount = 1;
        sbtWrites[uCurrentWrite].dstSet          = ptVulkanBindGroup->tDescriptorSet;
        sbtWrites[uCurrentWrite].pBufferInfo     = &sbtBufferDescInfos[i];
        sbtWrites[uCurrentWrite].pNext           = NULL;
        uCurrentWrite++;
    }

    for(uint32_t i = 0 ; i < uTextureViewCount; i++)
    {

        sbtImageDescInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        sbtImageDescInfos[i].imageView   = ptVulkanGraphics->sbtSamplersHot[atTextureViews[i].uIndex].tImageView;
        sbtImageDescInfos[i].sampler     = ptVulkanGraphics->sbtSamplersHot[atTextureViews[i].uIndex].tSampler;
        
        sbtWrites[uCurrentWrite].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sbtWrites[uCurrentWrite].dstBinding      = ptBindGroup->tLayout.aTextures[i].uSlot;
        sbtWrites[uCurrentWrite].dstArrayElement = 0;
        sbtWrites[uCurrentWrite].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sbtWrites[uCurrentWrite].descriptorCount = 1;
        sbtWrites[uCurrentWrite].dstSet          = ptVulkanBindGroup->tDescriptorSet;
        sbtWrites[uCurrentWrite].pImageInfo      = &sbtImageDescInfos[i];
        sbtWrites[uCurrentWrite].pNext           = NULL;
        uCurrentWrite++;
    }
    vkUpdateDescriptorSets(ptVulkanDevice->tLogicalDevice, uCurrentWrite, sbtWrites, 0, NULL);
    pl_temp_allocator_reset(&ptVulkanGraphics->tTempAllocator);
}

static plComputeShaderHandle
pl_get_compute_shader_variant(plDevice* ptDevice, plComputeShaderHandle tHandle, const plComputeShaderVariant* ptVariant)
{
    plGraphics*       ptGraphics = ptDevice->ptGraphics;
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    plComputeShader* ptShader = &ptGraphics->sbtComputeShadersCold[tHandle.uIndex];
    plVulkanComputeShader* ptParentVulkanShader = &ptVulkanGfx->sbtComputeShadersHot[tHandle.uIndex];

    const uint64_t ulVariantHash = pl_hm_hash(ptVariant->pTempConstantData, ptParentVulkanShader->szSpecializationSize, 0);
    const uint64_t ulIndex = pl_hm_lookup(&ptShader->tVariantHashmap, ulVariantHash);

    if(ulIndex != UINT64_MAX)
        return ptShader->_sbtVariantHandles[ulIndex];;

    uint32_t uNewResourceIndex = UINT32_MAX;

    if(pl_sb_size(ptGraphics->sbtComputeShaderFreeIndices) > 0)
        uNewResourceIndex = pl_sb_pop(ptGraphics->sbtComputeShaderFreeIndices);
    else
    {
        uNewResourceIndex = pl_sb_size(ptGraphics->sbtComputeShadersCold);
        pl_sb_add(ptGraphics->sbtComputeShadersCold);
        pl_sb_push(ptGraphics->sbtComputeShaderGenerations, UINT32_MAX);
        pl_sb_add(ptVulkanGfx->sbtComputeShadersHot);
        ptShader = &ptGraphics->sbtComputeShadersCold[tHandle.uIndex];
        ptParentVulkanShader = &ptVulkanGfx->sbtComputeShadersHot[tHandle.uIndex];
    }

    plVulkanComputeShader* ptVulkanShader = &ptVulkanGfx->sbtComputeShadersHot[uNewResourceIndex];

    plComputeShaderHandle tVariantHandle = {
        .uGeneration = ++ptGraphics->sbtComputeShaderGenerations[uNewResourceIndex],
        .uIndex = uNewResourceIndex
    };

    pl_hm_insert(&ptShader->tVariantHashmap, ulVariantHash, pl_sb_size(ptShader->_sbtVariantHandles));
    pl_sb_push(ptShader->_sbtVariantHandles, tVariantHandle);

    const VkSpecializationInfo tSpecializationInfo = {
        .mapEntryCount = ptShader->tDescription.uConstantCount,
        .pMapEntries   = ptParentVulkanShader->atSpecializationEntries,
        .dataSize      = ptParentVulkanShader->szSpecializationSize,
        .pData         = ptVariant->pTempConstantData
    };

    VkPipelineShaderStageCreateInfo tShaderStage = {
        .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage               = VK_SHADER_STAGE_COMPUTE_BIT,
        .module              = ptParentVulkanShader->tShaderModule,
        .pName               = ptShader->tDescription.pcShaderEntryFunc,
        .pSpecializationInfo = ptShader->tDescription.uConstantCount > 0 ? &tSpecializationInfo : NULL
    };

    VkPipelineLayoutCreateInfo tPipelineLayoutInfo = {
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts    = &ptVulkanGfx->sbtBindGroupLayouts[ptShader->tDescription.tBindGroupLayout.uHandle].tDescriptorSetLayout
    };

    PL_VULKAN(vkCreatePipelineLayout(ptVulkanDevice->tLogicalDevice, &tPipelineLayoutInfo, NULL, &ptVulkanShader->tPipelineLayout));

    VkComputePipelineCreateInfo tPipelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .layout = ptVulkanShader->tPipelineLayout,
        .stage  = tShaderStage
    };
    PL_VULKAN(vkCreateComputePipelines(ptVulkanDevice->tLogicalDevice, VK_NULL_HANDLE, 1, &tPipelineCreateInfo, NULL, &ptVulkanShader->tPipeline));

    ptGraphics->sbtComputeShadersCold[uNewResourceIndex] = *ptShader;
    ptVulkanGfx->sbtComputeShadersHot[uNewResourceIndex].tShaderModule = VK_NULL_HANDLE;
    ptGraphics->sbtComputeShadersCold[uNewResourceIndex]._sbtVariantHandles = NULL;
    memset(&ptGraphics->sbtComputeShadersCold[uNewResourceIndex].tVariantHashmap, 0, sizeof(plHashMap));
    return tVariantHandle;
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

    if(ptDescription->pcShaderEntryFunc == NULL)
        tShader.tDescription.pcShaderEntryFunc = "main";

    ptVulkanShader->szSpecializationSize = 0;
    for(uint32_t i = 0; i < tShader.tDescription.uConstantCount; i++)
    {
        const plSpecializationConstant* ptConstant = &tShader.tDescription.atConstants[i];
        ptVulkanShader->atSpecializationEntries[i].constantID = ptConstant->uID;
        ptVulkanShader->atSpecializationEntries[i].offset = ptConstant->uOffset;
        ptVulkanShader->atSpecializationEntries[i].size = pl__get_data_type_size(ptConstant->tType);
        ptVulkanShader->szSpecializationSize += ptVulkanShader->atSpecializationEntries[i].size;
    }

    pl_create_bind_group_layout(&ptGraphics->tDevice, &tShader.tDescription.tBindGroupLayout);

    uint32_t uShaderSize = 0u;
    gptFile->read(tShader.tDescription.pcShader, &uShaderSize, NULL, "rb");
    char* pcShaderCode = pl_temp_allocator_alloc(&ptVulkanGfx->tTempAllocator, uShaderSize);
    gptFile->read(tShader.tDescription.pcShader, &uShaderSize, pcShaderCode, "rb");

    VkShaderModuleCreateInfo tShaderCreateInfo = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = uShaderSize,
        .pCode    = (const uint32_t*)(pcShaderCode)
    };

    PL_VULKAN(vkCreateShaderModule(ptVulkanDevice->tLogicalDevice, &tShaderCreateInfo, NULL, &ptVulkanShader->tShaderModule));
    pl_temp_allocator_reset(&ptVulkanGfx->tTempAllocator);

    VkPipelineLayoutCreateInfo tPipelineLayoutInfo = {
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts    = &ptVulkanGfx->sbtBindGroupLayouts[tShader.tDescription.tBindGroupLayout.uHandle].tDescriptorSetLayout
    };

    const plComputeShaderVariant tMainShaderVariant = {.pTempConstantData = tShader.tDescription.pTempConstantData};

    plComputeShaderVariant *ptVariants = pl_temp_allocator_alloc(&ptVulkanGfx->tTempAllocator, sizeof(plComputeShaderVariant) * (tShader.tDescription.uVariantCount + 1));
    ptVariants[0] = tMainShaderVariant;
    for(uint32_t i = 0; i < tShader.tDescription.uVariantCount; i++)
    {
        ptVariants[i + 1] = tShader.tDescription.ptVariants[i];
    }
    for(uint32_t i = 0; i < tShader.tDescription.uVariantCount + 1; i++)
    {
        const plComputeShaderVariant *ptVariant = &ptVariants[i];

        plVulkanComputeShader tVulkanShader = {0};

        uint32_t uNewResourceIndex = UINT32_MAX;

        if(i == 0)
            uNewResourceIndex = uResourceIndex;
        else
        {
            if(pl_sb_size(ptGraphics->sbtComputeShaderFreeIndices) > 0)
                uNewResourceIndex = pl_sb_pop(ptGraphics->sbtComputeShaderFreeIndices);
            else
            {
                uNewResourceIndex = pl_sb_size(ptGraphics->sbtComputeShadersCold);
                pl_sb_add(ptGraphics->sbtComputeShadersCold);
                pl_sb_push(ptGraphics->sbtComputeShaderGenerations, UINT32_MAX);
                pl_sb_add(ptVulkanGfx->sbtComputeShadersHot);
                ptVulkanShader = &ptVulkanGfx->sbtComputeShadersHot[uResourceIndex];
            }
        }
        
        plComputeShaderHandle tVariantHandle = {
            .uGeneration = ++ptGraphics->sbtComputeShaderGenerations[uNewResourceIndex],
            .uIndex = uNewResourceIndex
        };

        const uint64_t ulVariantHash = pl_hm_hash(ptVariant->pTempConstantData, ptVulkanShader->szSpecializationSize, 0);
        pl_hm_insert(&tShader.tVariantHashmap, ulVariantHash, pl_sb_size(tShader._sbtVariantHandles));
        pl_sb_push(tShader._sbtVariantHandles, tVariantHandle);

        const VkSpecializationInfo tSpecializationInfo = {
            .mapEntryCount = tShader.tDescription.uConstantCount,
            .pMapEntries   = ptVulkanShader->atSpecializationEntries,
            .dataSize      = ptVulkanShader->szSpecializationSize,
            .pData         = ptVariant->pTempConstantData
        };

        VkPipelineShaderStageCreateInfo tShaderStage = {
            .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage               = VK_SHADER_STAGE_COMPUTE_BIT,
            .module              = ptVulkanShader->tShaderModule,
            .pName               = tShader.tDescription.pcShaderEntryFunc,
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

        if(i == 0)
        {
            ptVulkanShader->tPipeline = tVulkanShader.tPipeline;
            ptVulkanShader->tPipelineLayout = tVulkanShader.tPipelineLayout;
        }
        else
        {
            ptVulkanGfx->sbtComputeShadersHot[uNewResourceIndex] = tVulkanShader;
            ptVulkanGfx->sbtComputeShadersHot[uNewResourceIndex].tShaderModule = VK_NULL_HANDLE;
            ptGraphics->sbtComputeShadersCold[uNewResourceIndex]._sbtVariantHandles = NULL;
            memset(&ptGraphics->sbtComputeShadersCold[uNewResourceIndex].tVariantHashmap, 0, sizeof(plHashMap));
        }
    }

    ptGraphics->sbtComputeShadersCold[uResourceIndex] = tShader;
    return tHandle;
}

static plShaderHandle
pl_get_shader_variant(plDevice* ptDevice, plShaderHandle tHandle, const plShaderVariant* ptVariant)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice* ptVulkanDevice = ptGraphics->tDevice._pInternalData;
    plShader* ptShader = &ptGraphics->sbtShadersCold[tHandle.uIndex];

    plVulkanShader* ptVulkanShader = &ptVulkanGfx->sbtShadersHot[tHandle.uIndex];
    const uint64_t ulVariantHash = pl_hm_hash(ptVariant->pTempConstantData, ptVulkanShader->szSpecializationSize, ptVariant->tGraphicsState.ulValue);
    const uint64_t ulIndex = pl_hm_lookup(&ptShader->tVariantHashmap, ulVariantHash);

    if(ulIndex != UINT64_MAX)
        return ptShader->_sbtVariantHandles[ulIndex];

    plVulkanShader tVulkanShader = {0};
    uint32_t uNewResourceIndex = UINT32_MAX;

    if(pl_sb_size(ptGraphics->sbtShaderFreeIndices) > 0)
        uNewResourceIndex = pl_sb_pop(ptGraphics->sbtShaderFreeIndices);
    else
    {
        uNewResourceIndex = pl_sb_size(ptGraphics->sbtShadersCold);
        pl_sb_add(ptGraphics->sbtShadersCold);
        pl_sb_push(ptGraphics->sbtShaderGenerations, UINT32_MAX);
        pl_sb_add(ptVulkanGfx->sbtShadersHot);
        ptVulkanShader = &ptVulkanGfx->sbtShadersHot[tHandle.uIndex];
        ptShader = &ptGraphics->sbtShadersCold[tHandle.uIndex];
    }

    VkPipelineLayoutCreateInfo tPipelineLayoutInfo = {
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = ptShader->tDescription.uBindGroupLayoutCount + 1,
        .pSetLayouts    = ptVulkanShader->atDescriptorSetLayouts
    };
    PL_VULKAN(vkCreatePipelineLayout(ptVulkanDevice->tLogicalDevice, &tPipelineLayoutInfo, NULL, &tVulkanShader.tPipelineLayout));

    plShaderHandle tVariantHandle = {
        .uGeneration = ++ptGraphics->sbtShaderGenerations[uNewResourceIndex],
        .uIndex = uNewResourceIndex
    };

    pl_hm_insert(&ptShader->tVariantHashmap, ulVariantHash, pl_sb_size(ptShader->_sbtVariantHandles));
    pl_sb_push(ptShader->_sbtVariantHandles, tVariantHandle);

    //---------------------------------------------------------------------
    // vertex shader stage
    //---------------------------------------------------------------------

    const VkSpecializationInfo tSpecializationInfo = {
        .mapEntryCount = ptShader->tDescription.uConstantCount,
        .pMapEntries   = ptVulkanShader->atSpecializationEntries,
        .dataSize      = ptVulkanShader->szSpecializationSize,
        .pData         = ptVariant->pTempConstantData
    };

    VkPipelineShaderStageCreateInfo tVertShaderStageInfo = {
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage  = VK_SHADER_STAGE_VERTEX_BIT,
        .module = ptVulkanShader->tVertexShaderModule,
        .pName  = ptShader->tDescription.pcVertexShaderEntryFunc,
        .pSpecializationInfo = ptShader->tDescription.uConstantCount > 0 ? &tSpecializationInfo : NULL
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
        .polygonMode             = ptVariant->tGraphicsState.ulWireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL,
        .lineWidth               = 1.0f,
        .cullMode                = pl__vulkan_cull((plCullMode)ptVariant->tGraphicsState.ulCullMode),
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
        .pName  = ptShader->tDescription.pcPixelShaderEntryFunc,
        .pSpecializationInfo = &tSpecializationInfo
    };

    VkPipelineDepthStencilStateCreateInfo tDepthStencil = {
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable       = ptVariant->tGraphicsState.ulDepthMode == PL_COMPARE_MODE_ALWAYS ? VK_FALSE : VK_TRUE,
        .depthWriteEnable      = ptVariant->tGraphicsState.ulDepthWriteEnabled ? VK_TRUE : VK_FALSE,
        .depthCompareOp        = pl__vulkan_compare((plCompareMode)ptVariant->tGraphicsState.ulDepthMode),
        .depthBoundsTestEnable = VK_FALSE,
        .minDepthBounds        = 0.0f, // Optional,
        .maxDepthBounds        = 1.0f, // Optional,
        .stencilTestEnable     = VK_FALSE,
    };

    //---------------------------------------------------------------------
    // color blending stage
    //---------------------------------------------------------------------

    VkPipelineColorBlendAttachmentState tColorBlendAttachment = pl__get_blend_state((plBlendMode)ptVariant->tGraphicsState.ulBlendMode);

    VkPipelineColorBlendStateCreateInfo tColorBlending = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable   = VK_FALSE,
        .logicOp         = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments    = &tColorBlendAttachment,
        .blendConstants  = {0}
    };

    VkPipelineMultisampleStateCreateInfo tMultisampling = {
        .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable  = (bool)ptVulkanDevice->tDeviceFeatures.sampleRateShading,
        .minSampleShading     =  0.2f,
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

    //---------------------------------------------------------------------
    // input assembler stage
    //---------------------------------------------------------------------

    VkPipelineInputAssemblyStateCreateInfo tInputAssembly = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    VkVertexInputAttributeDescription tAttributeDescription = {
        .binding  = 0,
        .location = 0,
        .format   = VK_FORMAT_R32G32B32_SFLOAT,
        .offset   = 0,
    };

    VkVertexInputBindingDescription tBindingDescription = {
        .binding   = 0,
        .stride    = sizeof(float) * 3,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    VkPipelineVertexInputStateCreateInfo tVertexInputInfo = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 1,
        .vertexAttributeDescriptionCount = 1,
        .pVertexBindingDescriptions      = &tBindingDescription,
        .pVertexAttributeDescriptions    = &tAttributeDescription,
    };

    VkGraphicsPipelineCreateInfo tPipelineInfo = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount          = 2,
        .pStages             = shaderStages,
        .pVertexInputState   = &tVertexInputInfo,
        .pInputAssemblyState = &tInputAssembly,
        .pViewportState      = &tViewportState,
        .pRasterizationState = &tRasterizer,
        .pMultisampleState   = &tMultisampling,
        .pColorBlendState    = &tColorBlending,
        .pDynamicState       = &tDynamicState,
        .layout              = tVulkanShader.tPipelineLayout,
        .renderPass          = ptVulkanGfx->sbtRenderPassLayoutsHot[ptShader->tDescription.tRenderPassLayout.uIndex].tRenderPass,
        .subpass             = 0,
        .basePipelineHandle  = VK_NULL_HANDLE,
        .pDepthStencilState  = &tDepthStencil
    };

    PL_VULKAN(vkCreateGraphicsPipelines(ptVulkanDevice->tLogicalDevice, VK_NULL_HANDLE, 1, &tPipelineInfo, NULL, &tVulkanShader.tPipeline));
    ptVulkanGfx->sbtShadersHot[uNewResourceIndex] = tVulkanShader;
    ptGraphics->sbtShadersCold[uNewResourceIndex] = *ptShader;
    ptGraphics->sbtShadersCold[uNewResourceIndex]._sbtVariantHandles = NULL;
    memset(&ptGraphics->sbtShadersCold[uNewResourceIndex].tVariantHashmap, 0, sizeof(plHashMap));

    return tVariantHandle;
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

    if(ptDescription->pcPixelShaderEntryFunc == NULL)
        tShader.tDescription.pcPixelShaderEntryFunc = "main";

    if(ptDescription->pcVertexShaderEntryFunc == NULL)
        tShader.tDescription.pcVertexShaderEntryFunc = "main";

    VkVertexInputAttributeDescription tAttributeDescription = {
        .binding  = 0,
        .location = 0,
        .format   = VK_FORMAT_R32G32B32_SFLOAT,
        .offset   = 0,
    };

    VkVertexInputBindingDescription tBindingDescription = {
        .binding   = 0,
        .stride    = sizeof(float) * 3,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    for(uint32_t i = 0; i < tShader.tDescription.uBindGroupLayoutCount; i++)
    {
        pl_create_bind_group_layout(&ptGraphics->tDevice, &tShader.tDescription.atBindGroupLayouts[i]);
        ptVulkanShader->atDescriptorSetLayouts[i] = ptVulkanGfx->sbtBindGroupLayouts[tShader.tDescription.atBindGroupLayouts[i].uHandle].tDescriptorSetLayout;
    }
    ptVulkanShader->atDescriptorSetLayouts[tShader.tDescription.uBindGroupLayoutCount]  = ptVulkanGfx->tDynamicDescriptorSetLayout;

    uint32_t uVertShaderSize0 = 0u;
    uint32_t uPixelShaderSize0 = 0u;

    gptFile->read(tShader.tDescription.pcVertexShader, &uVertShaderSize0, NULL, "rb");
    gptFile->read(tShader.tDescription.pcPixelShader, &uPixelShaderSize0, NULL, "rb");

    char* vertexShaderCode = pl_temp_allocator_alloc(&ptVulkanGfx->tTempAllocator, uVertShaderSize0);
    char* pixelShaderCode  = pl_temp_allocator_alloc(&ptVulkanGfx->tTempAllocator, uPixelShaderSize0);

    gptFile->read(tShader.tDescription.pcVertexShader, &uVertShaderSize0, vertexShaderCode, "rb");
    gptFile->read(tShader.tDescription.pcPixelShader, &uPixelShaderSize0, pixelShaderCode, "rb");

    VkShaderModuleCreateInfo tVertexShaderCreateInfo = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = uVertShaderSize0,
        .pCode    = (const uint32_t*)(vertexShaderCode)
    };
    PL_VULKAN(vkCreateShaderModule(ptVulkanDevice->tLogicalDevice, &tVertexShaderCreateInfo, NULL, &ptVulkanShader->tVertexShaderModule));

    VkShaderModuleCreateInfo tPixelShaderCreateInfo = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = uPixelShaderSize0,
        .pCode    = (const uint32_t*)(pixelShaderCode),
    };

    PL_VULKAN(vkCreateShaderModule(ptVulkanDevice->tLogicalDevice, &tPixelShaderCreateInfo, NULL, &ptVulkanShader->tPixelShaderModule));

    pl_temp_allocator_reset(&ptVulkanGfx->tTempAllocator);

    //---------------------------------------------------------------------
    // input assembler stage
    //---------------------------------------------------------------------

    VkPipelineInputAssemblyStateCreateInfo tInputAssembly = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    VkPipelineVertexInputStateCreateInfo tVertexInputInfo = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 1,
        .vertexAttributeDescriptionCount = 1,
        .pVertexBindingDescriptions      = &tBindingDescription,
        .pVertexAttributeDescriptions    = &tAttributeDescription,
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

    const plShaderVariant tMainShaderVariant = {.pTempConstantData = tShader.tDescription.pTempConstantData, .tGraphicsState = tShader.tDescription.tGraphicsState};

    plShaderVariant *ptVariants = pl_temp_allocator_alloc(&ptVulkanGfx->tTempAllocator, sizeof(plShaderVariant) * (tShader.tDescription.uVariantCount + 1));
    ptVariants[0] = tMainShaderVariant;
    for(uint32_t i = 0; i < tShader.tDescription.uVariantCount; i++)
    {
        ptVariants[i + 1] = tShader.tDescription.ptVariants[i];
    }

    for(uint32_t i = 0; i < tShader.tDescription.uVariantCount + 1; i++)
    {
        const plShaderVariant *ptVariant = &ptVariants[i];
        
        plVulkanShader tVulkanShader = {0};
        uint32_t uNewResourceIndex = UINT32_MAX;

        if(i == 0)
            uNewResourceIndex = uResourceIndex;
        else
        {
            if(pl_sb_size(ptGraphics->sbtShaderFreeIndices) > 0)
                uNewResourceIndex = pl_sb_pop(ptGraphics->sbtShaderFreeIndices);
            else
            {
                uNewResourceIndex = pl_sb_size(ptGraphics->sbtShadersCold);
                pl_sb_add(ptGraphics->sbtShadersCold);
                pl_sb_push(ptGraphics->sbtShaderGenerations, UINT32_MAX);
                pl_sb_add(ptVulkanGfx->sbtShadersHot);
                ptVulkanShader = &ptVulkanGfx->sbtShadersHot[uResourceIndex];
            }
        }
        
        VkPipelineLayoutCreateInfo tPipelineLayoutInfo = {
            .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = tShader.tDescription.uBindGroupLayoutCount + 1,
            .pSetLayouts    = ptVulkanShader->atDescriptorSetLayouts
        };
        PL_VULKAN(vkCreatePipelineLayout(ptVulkanDevice->tLogicalDevice, &tPipelineLayoutInfo, NULL, &tVulkanShader.tPipelineLayout));

        plShaderHandle tVariantHandle = {
            .uGeneration = ++ptGraphics->sbtShaderGenerations[uNewResourceIndex],
            .uIndex = uNewResourceIndex
        };
        
        const uint64_t ulVariantHash = pl_hm_hash(ptVariant->pTempConstantData, ptVulkanShader->szSpecializationSize, ptVariant->tGraphicsState.ulValue);
        pl_hm_insert(&tShader.tVariantHashmap, ulVariantHash, pl_sb_size(tShader._sbtVariantHandles));
        pl_sb_push(tShader._sbtVariantHandles, tVariantHandle);

        //---------------------------------------------------------------------
        // vertex shader stage
        //---------------------------------------------------------------------

        const VkSpecializationInfo tSpecializationInfo = {
            .mapEntryCount = tShader.tDescription.uConstantCount,
            .pMapEntries   = ptVulkanShader->atSpecializationEntries,
            .dataSize      = ptVulkanShader->szSpecializationSize,
            .pData         = ptVariant->pTempConstantData
        };

        VkPipelineShaderStageCreateInfo tVertShaderStageInfo = {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_VERTEX_BIT,
            .module = ptVulkanShader->tVertexShaderModule,
            .pName  = tShader.tDescription.pcVertexShaderEntryFunc,
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
            .polygonMode             = ptVariant->tGraphicsState.ulWireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL,
            .lineWidth               = 1.0f,
            .cullMode                = pl__vulkan_cull((plCullMode)ptVariant->tGraphicsState.ulCullMode),
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
            .pName  = tShader.tDescription.pcPixelShaderEntryFunc,
            .pSpecializationInfo = &tSpecializationInfo
        };

        VkPipelineDepthStencilStateCreateInfo tDepthStencil = {
            .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable       = ptVariant->tGraphicsState.ulDepthMode == PL_COMPARE_MODE_ALWAYS ? VK_FALSE : VK_TRUE,
            .depthWriteEnable      = ptVariant->tGraphicsState.ulDepthWriteEnabled ? VK_TRUE : VK_FALSE,
            .depthCompareOp        = pl__vulkan_compare((plCompareMode)ptVariant->tGraphicsState.ulDepthMode),
            .depthBoundsTestEnable = VK_FALSE,
            .minDepthBounds        = 0.0f, // Optional,
            .maxDepthBounds        = 1.0f, // Optional,
            .stencilTestEnable     = VK_FALSE,
        };

        //---------------------------------------------------------------------
        // color blending stage
        //---------------------------------------------------------------------

        VkPipelineColorBlendAttachmentState tColorBlendAttachment = pl__get_blend_state((plBlendMode)ptVariant->tGraphicsState.ulBlendMode);

        VkPipelineColorBlendStateCreateInfo tColorBlending = {
            .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable   = VK_FALSE,
            .logicOp         = VK_LOGIC_OP_COPY,
            .attachmentCount = 1,
            .pAttachments    = &tColorBlendAttachment,
            .blendConstants  = {0}
        };

        VkPipelineMultisampleStateCreateInfo tMultisampling = {
            .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .sampleShadingEnable  = (bool)ptVulkanDevice->tDeviceFeatures.sampleRateShading,
            .minSampleShading     =  0.2f,
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
            .stageCount          = 2,
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
            .subpass             = 0,
            .basePipelineHandle  = VK_NULL_HANDLE,
            .pDepthStencilState  = &tDepthStencil
        };

        PL_VULKAN(vkCreateGraphicsPipelines(ptVulkanDevice->tLogicalDevice, VK_NULL_HANDLE, 1, &tPipelineInfo, NULL, &tVulkanShader.tPipeline));
        ptGraphics->sbtShadersCold[uNewResourceIndex] = tShader;
        if(i == 0)
        {
            ptVulkanShader->tPipeline = tVulkanShader.tPipeline;
            ptVulkanShader->tPipelineLayout = tVulkanShader.tPipelineLayout;
        }
        else
        {
            ptVulkanGfx->sbtShadersHot[uNewResourceIndex] = tVulkanShader;
            ptVulkanGfx->sbtShadersHot[uNewResourceIndex].tVertexShaderModule = VK_NULL_HANDLE;
            ptVulkanGfx->sbtShadersHot[uNewResourceIndex].tPixelShaderModule = VK_NULL_HANDLE;
            ptGraphics->sbtShadersCold[uNewResourceIndex]._sbtVariantHandles = NULL;
            memset(&ptGraphics->sbtShadersCold[uNewResourceIndex].tVariantHashmap, 0, sizeof(plHashMap));
        }
    }
    pl_temp_allocator_reset(&ptVulkanGfx->tTempAllocator);

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
                    .tClearColor = {0.0f, 0.0f, 0.0f, 1.0f},
                    .tFormat = ptGraphics->tSwapchain.tFormat,
                }
            }
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

    const VkRenderPassCreateInfo tRenderPassInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &tAttachment,
        .subpassCount    = 1,
        .pSubpasses      = &tSubpass
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
            .tDimensions = {pl_get_io()->afMainViewportSize[0], pl_get_io()->afMainViewportSize[1]}
        }
    };

    plRenderPassLayout* ptLayout = &ptGraphics->sbtRenderPassLayoutsCold[ptGraphics->tMainRenderPassLayout.uIndex];

    plVulkanRenderPass* ptVulkanRenderPass = &ptVulkanGfx->sbtRenderPassesHot[uResourceIndex];
    pl_sb_resize(ptVulkanRenderPass->sbtFrameBuffers, ptGraphics->tSwapchain.uImageCount);

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

    const VkRenderPassCreateInfo tRenderPassInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &tAttachment,
        .subpassCount    = 1,
        .pSubpasses      = &tSubpass
    };

    PL_VULKAN(vkCreateRenderPass(ptVulkanDevice->tLogicalDevice, &tRenderPassInfo, NULL, &ptVulkanRenderPass->tRenderPass));

    for(uint32_t i = 0; i < ptGraphics->tSwapchain.uImageCount; i++)
    {

        VkFramebufferCreateInfo tFrameBufferInfo = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = ptVulkanRenderPass->tRenderPass,
            .attachmentCount = 1,
            .pAttachments    = &ptVulkanGfx->sbtSamplersHot[ptGraphics->tSwapchain.sbtSwapchainTextureViews[i].uIndex].tImageView,
            .width           = (uint32_t)pl_get_io()->afMainViewportSize[0],
            .height          = (uint32_t)pl_get_io()->afMainViewportSize[1],
            .layers          = 1u,
        };
        PL_VULKAN(vkCreateFramebuffer(ptVulkanDevice->tLogicalDevice, &tFrameBufferInfo, NULL, &ptVulkanRenderPass->sbtFrameBuffers[i]));
    }

    ptGraphics->sbtRenderPassesCold[uResourceIndex] = tRenderPass;
    ptGraphics->tMainRenderPass = tHandle;
}

static plRenderPassLayoutHandle
pl_create_render_pass_layout(plDevice* ptDevice, const plRenderPassLayoutDescription* ptDesc)
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
        .tDesc = *ptDesc
    };

    plVulkanRenderPassLayout tVulkanRenderPassLayout = {0};

    VkSubpassDescription atSubpasses[16] = {0};
    
    VkAttachmentDescription atAttachments[16] = {0};

    VkAttachmentReference atColorAttachmentReferences[16] = {0};
    uint32_t uAttachmentCount = 0;
    for(uint32_t i = 0; i < 16; i++)
    {
        if(ptDesc->atRenderTargets[i].tFormat == PL_FORMAT_UNKNOWN)
        {
            break;
        }
        uAttachmentCount++;

        // from layout
        atAttachments[i].format = pl__vulkan_format(ptDesc->atRenderTargets[i].tFormat);
        atAttachments[i].samples = 1;

        // references
        atColorAttachmentReferences[i].attachment = i;
        atColorAttachmentReferences[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        atAttachments[i].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        atAttachments[i].finalLayout   = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    VkAttachmentReference tDepthAttachmentReference = {0};
    if(ptDesc->tDepthTarget.tFormat != PL_FORMAT_UNKNOWN)
    {
        // from layout
        atAttachments[uAttachmentCount].format = pl__vulkan_format(ptDesc->tDepthTarget.tFormat);
        atAttachments[uAttachmentCount].samples = 1;
        atAttachments[uAttachmentCount].initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        atAttachments[uAttachmentCount].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        
        tDepthAttachmentReference.attachment = uAttachmentCount;
        tDepthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        uAttachmentCount++;
    }

    // count subpasses
    uint32_t uSubpassCount = 0;
    for(uint32_t i = 0; i < 16; i++)
    {
        if(ptDesc->atSubpasses[i].uRenderTargetCount == 0)
            break;
        uSubpassCount++;

        atSubpasses[i].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        if(ptDesc->atSubpasses[i].bDepthTarget)
        {
            atSubpasses[i].pDepthStencilAttachment = &tDepthAttachmentReference;
        }

        atSubpasses[i].colorAttachmentCount = ptDesc->atSubpasses[i].uRenderTargetCount;
        atSubpasses[i].pColorAttachments = atColorAttachmentReferences;
    }

    const VkSubpassDependency tSubpassDependencies[] = {
        // color attachment
        {
            .srcSubpass      = VK_SUBPASS_EXTERNAL,
            .dstSubpass      = 0,
            .srcStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask   = VK_ACCESS_SHADER_READ_BIT,
            .dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = 0
        },

        // color attachment out
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
        .attachmentCount = uAttachmentCount,
        .pAttachments    = atAttachments,
        .subpassCount    = uSubpassCount,
        .pSubpasses      = atSubpasses,
        .dependencyCount = 2,
        .pDependencies   = tSubpassDependencies
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

    plVulkanRenderPass* ptVulkanRenderPass = &ptVulkanGfx->sbtRenderPassesHot[uResourceIndex];
    pl_sb_resize(ptVulkanRenderPass->sbtFrameBuffers, ptDesc->uAttachmentSets);

    VkSubpassDescription atSubpasses[16] = {0};
    
    VkAttachmentDescription atAttachments[16] = {0};

    VkAttachmentReference atColorAttachmentReferences[16] = {0};
    uint32_t uAttachmentCount = 0;
    for(uint32_t i = 0; i < 16; i++)
    {
        if(ptLayout->tDesc.atRenderTargets[i].tFormat == PL_FORMAT_UNKNOWN)
        {
            break;
        }
        uAttachmentCount++;

        // from layout
        atAttachments[i].format = pl__vulkan_format(ptLayout->tDesc.atRenderTargets[i].tFormat);
        atAttachments[i].samples = 1;

        // from description
        atAttachments[i].loadOp         = pl__vulkan_load_op(ptDesc->atRenderTargets[i].tLoadOp);
        atAttachments[i].storeOp        = pl__vulkan_store_op(ptDesc->atRenderTargets[i].tStoreOp);
        atAttachments[i].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        atAttachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        // layouts
        atAttachments[i].initialLayout = pl__vulkan_layout(ptDesc->atRenderTargets[i].tNextUsage);
        atAttachments[i].finalLayout   = pl__vulkan_layout(ptDesc->atRenderTargets[i].tNextUsage);

        // references
        atColorAttachmentReferences[i].attachment = i;
        atColorAttachmentReferences[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    VkAttachmentReference tDepthAttachmentReference = {0};
    if(ptLayout->tDesc.tDepthTarget.tFormat != PL_FORMAT_UNKNOWN)
    {
        atAttachments[uAttachmentCount].loadOp         = pl__vulkan_load_op(ptDesc->tDepthTarget.tLoadOp);
        atAttachments[uAttachmentCount].storeOp        = pl__vulkan_store_op(ptDesc->tDepthTarget.tStoreOp);
        atAttachments[uAttachmentCount].stencilLoadOp  = pl__vulkan_load_op(ptDesc->tDepthTarget.tStencilLoadOp);
        atAttachments[uAttachmentCount].stencilStoreOp = pl__vulkan_store_op(ptDesc->tDepthTarget.tStencilStoreOp);
        atAttachments[uAttachmentCount].initialLayout  = pl__vulkan_layout(ptDesc->tDepthTarget.tNextUsage);
        atAttachments[uAttachmentCount].finalLayout    = pl__vulkan_layout(ptDesc->tDepthTarget.tNextUsage);
        
        // frome layout
        atAttachments[uAttachmentCount].format = pl__vulkan_format(ptLayout->tDesc.tDepthTarget.tFormat);
        atAttachments[uAttachmentCount].samples = 1;

        tDepthAttachmentReference.attachment = uAttachmentCount;
        tDepthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        uAttachmentCount++;
    }

    // count subpasses
    uint32_t uSubpassCount = 0;
    for(uint32_t i = 0; i < 16; i++)
    {
        if(ptLayout->tDesc.atSubpasses[i].uRenderTargetCount == 0)
            break;
        uSubpassCount++;

        atSubpasses[i].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        if(ptLayout->tDesc.atSubpasses[i].bDepthTarget)
        {
            atSubpasses[i].pDepthStencilAttachment = &tDepthAttachmentReference;
        }

        atSubpasses[i].colorAttachmentCount = ptLayout->tDesc.atSubpasses[i].uRenderTargetCount;
        atSubpasses[i].pColorAttachments = atColorAttachmentReferences;
    }

    const VkSubpassDependency tSubpassDependencies[] = {
        // color attachment
        {
            .srcSubpass      = VK_SUBPASS_EXTERNAL,
            .dstSubpass      = 0,
            .srcStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask   = VK_ACCESS_SHADER_READ_BIT,
            .dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = 0
        },

        // color attachment out
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
        .attachmentCount = uAttachmentCount,
        .pAttachments    = atAttachments,
        .subpassCount    = uSubpassCount,
        .pSubpasses      = atSubpasses,
        .dependencyCount = 2,
        .pDependencies   = tSubpassDependencies
    };

    PL_VULKAN(vkCreateRenderPass(ptVulkanDevice->tLogicalDevice, &tRenderPassInfo, NULL, &ptVulkanRenderPass->tRenderPass));


    for(uint32_t i = 0; i < ptDesc->uAttachmentSets; i++)
    {
        VkImageView atViewAttachments[16] = {0};

        for(uint32_t j = 0; j < ptDesc->uAttachmentCount; j++)
        {
            atViewAttachments[j] = ptVulkanGfx->sbtSamplersHot[ptAttachments[i].atViewAttachments[j].uIndex].tImageView;
        }

        VkFramebufferCreateInfo tFrameBufferInfo = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = ptVulkanRenderPass->tRenderPass,
            .attachmentCount = ptDesc->uAttachmentCount,
            .pAttachments    = atViewAttachments,
            .width           = (uint32_t)ptDesc->tDimensions.x,
            .height          = (uint32_t)ptDesc->tDimensions.y,
            .layers          = 1u,
        };
        PL_VULKAN(vkCreateFramebuffer(ptVulkanDevice->tLogicalDevice, &tFrameBufferInfo, NULL, &ptVulkanRenderPass->sbtFrameBuffers[i]));
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
    plFrameContext* ptFrame = pl__get_frame_resources(ptGraphics);
    ptRenderPass->tDesc.tDimensions = tDimensions;

    const plRenderPassDescription* ptDesc = &ptRenderPass->tDesc;

    for(uint32_t i = 0; i < ptDesc->uAttachmentSets; i++)
    {
        VkImageView atViewAttachments[16] = {0};

        for(uint32_t j = 0; j < ptDesc->uAttachmentCount; j++)
        {
            atViewAttachments[j] = ptVulkanGfx->sbtSamplersHot[ptAttachments[i].atViewAttachments[j].uIndex].tImageView;
        }

        VkFramebufferCreateInfo tFrameBufferInfo = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = ptVulkanRenderPass->tRenderPass,
            .attachmentCount = ptDesc->uAttachmentCount,
            .pAttachments    = atViewAttachments,
            .width           = (uint32_t)ptDesc->tDimensions.x,
            .height          = (uint32_t)ptDesc->tDimensions.y,
            .layers          = 1u,
        };
        pl_sb_push(ptFrame->sbtRawFrameBuffers, ptVulkanRenderPass->sbtFrameBuffers[i]);
        ptVulkanRenderPass->sbtFrameBuffers[i] = VK_NULL_HANDLE;
        PL_VULKAN(vkCreateFramebuffer(ptVulkanDevice->tLogicalDevice, &tFrameBufferInfo, NULL, &ptVulkanRenderPass->sbtFrameBuffers[i]));
    }
}


static void
pl_begin_recording(plGraphics* ptGraphics)
{
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptGraphics);

    const VkCommandBufferBeginInfo tBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    };
    PL_VULKAN(vkResetCommandPool(ptVulkanDevice->tLogicalDevice, ptCurrentFrame->tCmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT));
    PL_VULKAN(vkBeginCommandBuffer(ptCurrentFrame->tCmdBuf, &tBeginInfo));

    pl_new_draw_frame_vulkan();
}

static void
pl_end_recording(plGraphics* ptGraphics)
{
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;
    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptGraphics);
    PL_VULKAN(vkEndCommandBuffer(ptCurrentFrame->tCmdBuf));
}

static void
pl_begin_pass(plGraphics* ptGraphics, plRenderPassHandle tPass)
{

    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptGraphics);

    plRenderPass* ptRenderPass = &ptGraphics->sbtRenderPassesCold[tPass.uIndex];
    plVulkanRenderPass* ptVulkanRenderPass = &ptVulkanGfx->sbtRenderPassesHot[tPass.uIndex];

    const VkClearValue atClearValues[2] = {
        { .color.float32 = {0.0f, 0.0f, 0.0f, 1.0f}},
        { .depthStencil.depth = 1.0f}
    };

    const uint32_t uFrameBufferIndex = pl_min(ptRenderPass->tDesc.uAttachmentSets - 1, ptGraphics->uCurrentFrameIndex);

    VkRenderPassBeginInfo tRenderPassInfo = {
        .sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass        = ptVulkanRenderPass->tRenderPass,
        .framebuffer       = ptVulkanRenderPass->sbtFrameBuffers[uFrameBufferIndex],
        .renderArea.extent = {
            .width  = (uint32_t)ptRenderPass->tDesc.tDimensions.x,
            .height = (uint32_t)ptRenderPass->tDesc.tDimensions.y
        },
        .clearValueCount   = 2,
        .pClearValues      = atClearValues
    };

    vkCmdBeginRenderPass(ptCurrentFrame->tCmdBuf, &tRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

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

    vkCmdSetViewport(ptCurrentFrame->tCmdBuf, 0, 1, &tViewport);
    vkCmdSetScissor(ptCurrentFrame->tCmdBuf, 0, 1, &tScissor);
}

static void
pl_end_pass(plGraphics* ptGraphics)
{
    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptGraphics);
    vkCmdEndRenderPass(ptCurrentFrame->tCmdBuf);
}

static void
pl_begin_main_pass(plGraphics* ptGraphics, plRenderPassHandle tPass)
{

    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptGraphics);

    plRenderPass* ptRenderPass = &ptGraphics->sbtRenderPassesCold[tPass.uIndex];
    plVulkanRenderPass* ptVulkanRenderPass = &ptVulkanGfx->sbtRenderPassesHot[tPass.uIndex];

    const VkClearValue atClearValues[2] = {
        { .color.float32 = {0.0f, 0.0f, 0.0f, 1.0f}},
        { .depthStencil.depth = 1.0f}
    };

    VkRenderPassBeginInfo tRenderPassInfo = {
        .sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass        = ptVulkanRenderPass->tRenderPass,
        .framebuffer       = ptVulkanRenderPass->sbtFrameBuffers[ptGraphics->tSwapchain.uCurrentImageIndex],
        .renderArea.extent = {
            .width  = (uint32_t)ptRenderPass->tDesc.tDimensions.x,
            .height = (uint32_t)ptRenderPass->tDesc.tDimensions.y
        },
        .clearValueCount   = 2,
        .pClearValues      = atClearValues
    };

    vkCmdBeginRenderPass(ptCurrentFrame->tCmdBuf, &tRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

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

    vkCmdSetViewport(ptCurrentFrame->tCmdBuf, 0, 1, &tViewport);
    vkCmdSetScissor(ptCurrentFrame->tCmdBuf, 0, 1, &tScissor);  
}

static void
pl_end_main_pass(plGraphics* ptGraphics)
{
    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptGraphics);
    vkCmdEndRenderPass(ptCurrentFrame->tCmdBuf);
}

static void
pl_draw_list(plGraphics* ptGraphics, uint32_t uListCount, plDrawList* atLists, plRenderPassHandle tPass)
{
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptGraphics);

    plIO* ptIOCtx = pl_get_io();
    for(uint32_t i = 0; i < uListCount; i++)
    {
        pl_submit_vulkan_drawlist_ex(&atLists[i],
            ptIOCtx->afMainViewportSize[0],
            ptIOCtx->afMainViewportSize[1],
            ptCurrentFrame->tCmdBuf,
            ptGraphics->uCurrentFrameIndex,
            ptVulkanGfx->sbtRenderPassesHot[tPass.uIndex].tRenderPass,
            VK_SAMPLE_COUNT_1_BIT);
    }
}

static void
pl_initialize_graphics(plGraphics* ptGraphics)
{

    plIO* ptIOCtx = pl_get_io();

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
    
    ptGraphics->uFramesInFlight = 2;

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
        .enabledLayerCount       = 1,
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
            .hwnd = *(HWND*)ptIOCtx->pBackendPlatformData
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
        const VkMetalSurfaceCreateInfoEXT tSurfaceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
            .pLayer = (CAMetalLayer*)ptIOCtx->pBackendPlatformData
        };
        PL_VULKAN(vkCreateMetalSurfaceEXT(ptVulkanGfx->tInstance, &tSurfaceCreateInfo, NULL, &ptVulkanGfx->tSurface));
    #else // linux
        struct tPlatformData { xcb_connection_t* ptConnection; xcb_window_t tWindow;};
        struct tPlatformData* ptPlatformData = (struct tPlatformData*)ptIOCtx->pBackendPlatformData;
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

    // create logical device

    vkGetPhysicalDeviceFeatures(ptVulkanDevice->tPhysicalDevice, &ptVulkanDevice->tDeviceFeatures);

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
        .enabledExtensionCount    = uDeviceExtensionCount
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

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~device memory allocators~~~~~~~~~~~~~~~~~~~~~~~~~

    // local dedicated
    static plDeviceAllocatorData tLocalDedicatedData = {0};
    tLocalDedicatedData.ptDevice = &ptGraphics->tDevice;
    ptGraphics->tDevice.tLocalDedicatedAllocator.allocate = pl_allocate_dedicated;
    ptGraphics->tDevice.tLocalDedicatedAllocator.free = pl_free_dedicated;
    ptGraphics->tDevice.tLocalDedicatedAllocator.blocks = pl_get_allocator_blocks;
    ptGraphics->tDevice.tLocalDedicatedAllocator.ranges = pl_get_allocator_ranges;
    ptGraphics->tDevice.tLocalDedicatedAllocator.ptInst = (struct plDeviceMemoryAllocatorO*)&tLocalDedicatedData;

    // local buddy
    static plDeviceAllocatorData tLocalBuddyData = {0};
    for(uint32_t i = 0; i < PL_DEVICE_LOCAL_LEVELS; i++)
        tLocalBuddyData.auFreeList[i] = UINT32_MAX;
    tLocalBuddyData.ptDevice = &ptGraphics->tDevice;
    ptGraphics->tDevice.tLocalBuddyAllocator.allocate = pl_allocate_buddy;
    ptGraphics->tDevice.tLocalBuddyAllocator.free = pl_free_buddy;
    ptGraphics->tDevice.tLocalBuddyAllocator.blocks = pl_get_allocator_blocks;
    ptGraphics->tDevice.tLocalBuddyAllocator.ranges = pl_get_allocator_ranges;
    ptGraphics->tDevice.tLocalBuddyAllocator.ptInst = (struct plDeviceMemoryAllocatorO*)&tLocalBuddyData;

    // staging uncached
    static plDeviceAllocatorData tStagingUncachedData = {0};
    tStagingUncachedData.ptDevice = &ptGraphics->tDevice;
    ptGraphics->tDevice.tStagingUnCachedAllocator.allocate = pl_allocate_staging_uncached;
    ptGraphics->tDevice.tStagingUnCachedAllocator.free = pl_free_staging_uncached;
    ptGraphics->tDevice.tStagingUnCachedAllocator.blocks = pl_get_allocator_blocks;
    ptGraphics->tDevice.tStagingUnCachedAllocator.ranges = pl_get_allocator_ranges;
    ptGraphics->tDevice.tStagingUnCachedAllocator.ptInst = (struct plDeviceMemoryAllocatorO*)&tStagingUncachedData;

    // staging cached
    static plDeviceAllocatorData tStagingCachedData = {0};
    tStagingCachedData.ptDevice = &ptGraphics->tDevice;
    ptGraphics->tDevice.tStagingCachedAllocator.allocate = pl_allocate_staging_cached;
    ptGraphics->tDevice.tStagingCachedAllocator.free = pl_free_staging_cached;
    ptGraphics->tDevice.tStagingCachedAllocator.blocks = pl_get_allocator_blocks;
    ptGraphics->tDevice.tStagingCachedAllocator.ranges = pl_get_allocator_ranges;
    ptGraphics->tDevice.tStagingCachedAllocator.ptInst = (struct plDeviceMemoryAllocatorO*)&tStagingCachedData;

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
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          100000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          100000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   100000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   100000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         100000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         100000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       100000 }
    };
    VkDescriptorPoolCreateInfo tDescriptorPoolInfo = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = 100000 * 11,
        .poolSizeCount = 11u,
        .pPoolSizes    = atPoolSizes,
    };
    PL_VULKAN(vkCreateDescriptorPool(ptVulkanDevice->tLogicalDevice, &tDescriptorPoolInfo, NULL, &ptVulkanGfx->tDescriptorPool));

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~frame resources~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // dynamic buffer stuff
    VkDescriptorSetLayoutBinding tBinding =  {
        .binding         = 0,
        .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        .descriptorCount = 1,
        .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT,
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

        const VkCommandBufferAllocateInfo tAllocInfo = {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool        = tFrame.tCmdPool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        VkCommandBuffer atCmdBuffers[1] = {0};
        PL_VULKAN(vkAllocateCommandBuffers(ptVulkanDevice->tLogicalDevice, &tAllocInfo, atCmdBuffers));  
        tFrame.tCmdBuf = atCmdBuffers[0];
        
        // dynamic buffer stuff
        pl_sb_resize(tFrame.sbtDynamicBuffers, 1);
        plBufferDescription tStagingBufferDescription0 = {
            .tMemory              = PL_MEMORY_GPU_CPU,
            .tUsage               = PL_BUFFER_USAGE_UNIFORM,
            .uByteSize            = PL_DEVICE_ALLOCATION_BLOCK_SIZE
        };
        pl_sprintf(tStagingBufferDescription0.acDebugName, "D-BUF-F%d-0", (int)i);

        plBufferHandle tStagingBuffer0 = pl_create_buffer(&ptGraphics->tDevice, &tStagingBufferDescription0, NULL);

        tFrame.sbtDynamicBuffers[0].uHandle = tStagingBuffer0.uIndex;
        tFrame.sbtDynamicBuffers[0].tBuffer = ptVulkanGfx->sbtBuffersHot[tStagingBuffer0.uIndex].tBuffer;
        tFrame.sbtDynamicBuffers[0].tMemory = ptGraphics->sbtBuffersCold[tStagingBuffer0.uIndex].tMemoryAllocation;
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
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          100000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          100000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   100000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   100000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         100000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         100000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       100000 }
        };
        VkDescriptorPoolCreateInfo tDynamicDescriptorPoolInfo = {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            .maxSets       = 100000 * 11,
            .poolSizeCount = 11u,
            .pPoolSizes    = atDynamicPoolSizes,
        };
        PL_VULKAN(vkCreateDescriptorPool(ptVulkanDevice->tLogicalDevice, &tDynamicDescriptorPoolInfo, NULL, &tFrame.tDynamicDescriptorPool));

        ptVulkanGfx->sbFrames[i] = tFrame;
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~3d setup~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // create pipeline layout
    const VkPushConstantRange t3DPushConstant = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset    = 0,
        .size      = sizeof(float) * 16
    };

    const VkPipelineLayoutCreateInfo t3DPipelineLayoutInfo = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 0u,
        .pSetLayouts            = NULL,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &t3DPushConstant,
    };

    PL_VULKAN(vkCreatePipelineLayout(ptVulkanDevice->tLogicalDevice, &t3DPipelineLayoutInfo, NULL, &ptVulkanGfx->t3DPipelineLayout));

    // vertex shader stage

    ptVulkanGfx->t3DVtxShdrStgInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    ptVulkanGfx->t3DVtxShdrStgInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    ptVulkanGfx->t3DVtxShdrStgInfo.pName = "main";

    uint32_t uVertShaderSize0 = 0u;
    uint32_t uVertShaderSize1 = 0u;
    uint32_t uPixelShaderSize0 = 0u;

    gptFile->read("draw_3d.vert.spv", &uVertShaderSize0, NULL, "rb");
    gptFile->read("draw_3d_line.vert.spv", &uVertShaderSize1, NULL, "rb");
    gptFile->read("draw_3d.frag.spv", &uPixelShaderSize0, NULL, "rb");

    char* __glsl_shader_vert_3d_spv      = pl_temp_allocator_alloc(&ptVulkanGfx->tTempAllocator, uVertShaderSize0);
    char* __glsl_shader_vert_3d_line_spv = pl_temp_allocator_alloc(&ptVulkanGfx->tTempAllocator, uVertShaderSize1);
    char* __glsl_shader_frag_3d_spv      = pl_temp_allocator_alloc(&ptVulkanGfx->tTempAllocator, uPixelShaderSize0);

    gptFile->read("draw_3d.vert.spv", &uVertShaderSize0, __glsl_shader_vert_3d_spv, "rb");
    gptFile->read("draw_3d_line.vert.spv", &uVertShaderSize1, __glsl_shader_vert_3d_line_spv, "rb");
    gptFile->read("draw_3d.frag.spv", &uPixelShaderSize0, __glsl_shader_frag_3d_spv, "rb");

    const VkShaderModuleCreateInfo t3DVtxShdrInfo = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = uVertShaderSize0,
        .pCode    = (uint32_t*)__glsl_shader_vert_3d_spv
    };
    PL_ASSERT(vkCreateShaderModule(ptVulkanDevice->tLogicalDevice, &t3DVtxShdrInfo, NULL, &ptVulkanGfx->t3DVtxShdrStgInfo.module) == VK_SUCCESS);

    // fragment shader stage
    ptVulkanGfx->t3DPxlShdrStgInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    ptVulkanGfx->t3DPxlShdrStgInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    ptVulkanGfx->t3DPxlShdrStgInfo.pName = "main";

    const VkShaderModuleCreateInfo t3DPxlShdrInfo = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = uPixelShaderSize0,
        .pCode    = (uint32_t*)__glsl_shader_frag_3d_spv
    };
    PL_ASSERT(vkCreateShaderModule(ptVulkanDevice->tLogicalDevice, &t3DPxlShdrInfo, NULL, &ptVulkanGfx->t3DPxlShdrStgInfo.module) == VK_SUCCESS);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~3d line setup~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // create pipeline layout
    const VkPushConstantRange t3DLinePushConstant = 
    {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset    = 0,
        .size      = sizeof(float) * 17
    };

    const VkPipelineLayoutCreateInfo t3DLinePipelineLayoutInfo = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 0u,
        .pSetLayouts            = NULL,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &t3DLinePushConstant,
    };

    PL_VULKAN(vkCreatePipelineLayout(ptVulkanDevice->tLogicalDevice, &t3DLinePipelineLayoutInfo, NULL, &ptVulkanGfx->t3DLinePipelineLayout));

    // vertex shader stage

    ptVulkanGfx->t3DLineVtxShdrStgInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    ptVulkanGfx->t3DLineVtxShdrStgInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    ptVulkanGfx->t3DLineVtxShdrStgInfo.pName = "main";

    const VkShaderModuleCreateInfo t3DLineVtxShdrInfo = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = uVertShaderSize1,
        .pCode    = (uint32_t*)__glsl_shader_vert_3d_line_spv
    };
    PL_ASSERT(vkCreateShaderModule(ptVulkanDevice->tLogicalDevice, &t3DLineVtxShdrInfo, NULL, &ptVulkanGfx->t3DLineVtxShdrStgInfo.module) == VK_SUCCESS);

    pl_sb_resize(ptVulkanGfx->sbt3DBufferInfo, ptGraphics->uFramesInFlight);
    pl_sb_resize(ptVulkanGfx->sbtLineBufferInfo, ptGraphics->uFramesInFlight);

    pl_temp_allocator_reset(&ptVulkanGfx->tTempAllocator);

    pl_create_main_render_pass_layout(&ptGraphics->tDevice);
    pl_create_main_render_pass(&ptGraphics->tDevice);
}

static void
pl_setup_ui(plGraphics* ptGraphics, plRenderPassHandle tPass)
{
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    // setup drawing api
    const plVulkanInit tVulkanInit = {
        .tPhysicalDevice  = ptVulkanDevice->tPhysicalDevice,
        .tLogicalDevice   = ptVulkanDevice->tLogicalDevice,
        .uImageCount      = ptGraphics->tSwapchain.uImageCount,
        .tRenderPass      = ptVulkanGfx->sbtRenderPassesHot[tPass.uIndex].tRenderPass,
        .tMSAASampleCount = VK_SAMPLE_COUNT_1_BIT,
        .uFramesInFlight  = ptGraphics->uFramesInFlight
    };
    pl_initialize_vulkan(&tVulkanInit);
}

static bool
pl_begin_frame(plGraphics* ptGraphics)
{
    pl_begin_profile_sample(__FUNCTION__);
    plIO* ptIOCtx = pl_get_io();

    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;
    plVulkanSwapchain* ptVulkanSwapchain = ptGraphics->tSwapchain._pInternalData;

    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptGraphics);

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

    if (ptCurrentFrame->tInFlight != VK_NULL_HANDLE)
        PL_VULKAN(vkWaitForFences(ptVulkanDevice->tLogicalDevice, 1, &ptCurrentFrame->tInFlight, VK_TRUE, UINT64_MAX));

    PL_VULKAN(vkResetDescriptorPool(ptVulkanDevice->tLogicalDevice, ptCurrentFrame->tDynamicDescriptorPool, 0));

    //-----------------------------------------------------------------------------
    // buffer deletion queue
    //-----------------------------------------------------------------------------

    // reset buffer offsets
    for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbt3DBufferInfo); i++)
    {
        ptVulkanGfx->sbt3DBufferInfo[i].uVertexBufferOffset = 0;
        ptVulkanGfx->sbt3DBufferInfo[i].uIndexBufferOffset = 0;
    }

    for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbtLineBufferInfo); i++)
    {
        ptVulkanGfx->sbtLineBufferInfo[i].uVertexBufferOffset = 0;
        ptVulkanGfx->sbtLineBufferInfo[i].uIndexBufferOffset = 0;
    }

    // reset 3d drawlists
    for(uint32_t i = 0u; i < pl_sb_size(ptGraphics->sbt3DDrawlists); i++)
    {
        plDrawList3D* drawlist = ptGraphics->sbt3DDrawlists[i];

        pl_sb_reset(drawlist->sbtSolidVertexBuffer);
        pl_sb_reset(drawlist->sbtLineVertexBuffer);
        pl_sb_reset(drawlist->sbtSolidIndexBuffer);    
        pl_sb_reset(drawlist->sbtLineIndexBuffer);    
    }

    pl_end_profile_sample();
    return true; 
}

static bool
pl_end_gfx_frame(plGraphics* ptGraphics)
{
    pl_begin_profile_sample(__FUNCTION__);
    plIO* ptIOCtx = pl_get_io();

    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;
    plVulkanSwapchain* ptVulkanSwapchain = ptGraphics->tSwapchain._pInternalData;

    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptGraphics);
    ptCurrentFrame->uCurrentBufferIndex = UINT32_MAX;

    // submit
    const VkPipelineStageFlags atWaitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkCommandBuffer atCmdBuffers[] = {ptCurrentFrame->tCmdBuf};
    const VkSubmitInfo tSubmitInfo = {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = &ptCurrentFrame->tImageAvailable,
        .pWaitDstStageMask    = atWaitStages,
        .commandBufferCount   = 1,
        .pCommandBuffers      = atCmdBuffers,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &ptCurrentFrame->tRenderFinish
    };
    PL_VULKAN(vkResetFences(ptVulkanDevice->tLogicalDevice, 1, &ptCurrentFrame->tInFlight));
    PL_VULKAN(vkQueueSubmit(ptVulkanDevice->tGraphicsQueue, 1, &tSubmitInfo, ptCurrentFrame->tInFlight));          
    
    // present                        
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

    pl_end_profile_sample();
    return true;
}

static void
pl_resize(plGraphics* ptGraphics)
{
    pl_begin_profile_sample(__FUNCTION__);
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;
    plIO* ptIOCtx = pl_get_io();
    ptGraphics->uCurrentFrameIndex = (ptGraphics->uCurrentFrameIndex + 1) % ptGraphics->uFramesInFlight;
    pl__garbage_collect(ptGraphics);
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
            .pAttachments    = &ptVulkanGfx->sbtSamplersHot[ptGraphics->tSwapchain.sbtSwapchainTextureViews[i].uIndex].tImageView,
            .width           = (uint32_t)ptIOCtx->afMainViewportSize[0],
            .height          = (uint32_t)ptIOCtx->afMainViewportSize[1],
            .layers          = 1u,
        };
        pl_sb_push(ptFrame->sbtRawFrameBuffers, ptVulkanRenderPass->sbtFrameBuffers[i]);
        ptVulkanRenderPass->sbtFrameBuffers[i] = VK_NULL_HANDLE;
        PL_VULKAN(vkCreateFramebuffer(ptVulkanDevice->tLogicalDevice, &tFrameBufferInfo, NULL, &ptVulkanRenderPass->sbtFrameBuffers[i]));
    }

    pl_end_profile_sample();
}

static void
pl_shutdown(plGraphics* ptGraphics)
{
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;
    plVulkanSwapchain* ptVulkanSwapchain = ptGraphics->tSwapchain._pInternalData;
    
    vkDeviceWaitIdle(ptVulkanDevice->tLogicalDevice);

    pl_cleanup_vulkan();

    for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbt3DBufferInfo); i++)
    {
        vkDestroyBuffer(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbt3DBufferInfo[i].tVertexBuffer, NULL);
        vkDestroyBuffer(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbt3DBufferInfo[i].tIndexBuffer, NULL);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbtLineBufferInfo); i++)
    {
        vkDestroyBuffer(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtLineBufferInfo[i].tVertexBuffer, NULL);
        vkDestroyBuffer(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtLineBufferInfo[i].tIndexBuffer, NULL);
    }


    for(uint32_t i = 0u; i < pl_sb_size(ptVulkanGfx->sbt3DPipelines); i++)
    {
        vkDestroyPipeline(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbt3DPipelines[i].tRegularPipeline, NULL);
        vkDestroyPipeline(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbt3DPipelines[i].tSecondaryPipeline, NULL);
    }

    vkDestroyShaderModule(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->t3DPxlShdrStgInfo.module, NULL);
    vkDestroyShaderModule(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->t3DVtxShdrStgInfo.module, NULL);
    vkDestroyShaderModule(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->t3DLineVtxShdrStgInfo.module, NULL);
    vkDestroyPipelineLayout(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->t3DPipelineLayout, NULL);
    vkDestroyPipelineLayout(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->t3DLinePipelineLayout, NULL);

    pl_sb_free(ptVulkanGfx->sbt3DBufferInfo);
    pl_sb_free(ptVulkanGfx->sbtLineBufferInfo);
    pl_sb_free(ptVulkanGfx->sbt3DPipelines);

    for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbtTexturesHot); i++)
    {
        if(ptVulkanGfx->sbtTexturesHot[i].tImage)
        {
            vkDestroyImage(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtTexturesHot[i].tImage, NULL);
            ptVulkanGfx->sbtTexturesHot[i].tImage = VK_NULL_HANDLE;
        }
    }

    for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbtSamplersHot); i++)
    {
        if(ptVulkanGfx->sbtSamplersHot[i].tSampler)
            vkDestroySampler(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtSamplersHot[i].tSampler, NULL);
        if(ptVulkanGfx->sbtSamplersHot[i].tImageView)
            vkDestroyImageView(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtSamplersHot[i].tImageView, NULL);
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

    vkDestroyDescriptorSetLayout(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tDynamicDescriptorSetLayout, NULL);
    pl_sb_free(ptVulkanGfx->sbtTexturesHot);
    pl_sb_free(ptVulkanGfx->sbtSamplersHot);
    pl_sb_free(ptVulkanGfx->sbtBindGroupsHot);
    pl_sb_free(ptVulkanGfx->sbtBuffersHot);
    pl_sb_free(ptVulkanGfx->sbtShadersHot);
    pl_sb_free(ptVulkanGfx->sbtComputeShadersHot);
    pl_sb_free(ptVulkanGfx->sbtBindGroupLayouts);

    // cleanup per frame resources
    for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbFrames); i++)
    {
        plFrameContext* ptFrame = &ptVulkanGfx->sbFrames[i];
        vkDestroySemaphore(ptVulkanDevice->tLogicalDevice, ptFrame->tImageAvailable, NULL);
        vkDestroySemaphore(ptVulkanDevice->tLogicalDevice, ptFrame->tRenderFinish, NULL);
        vkDestroyFence(ptVulkanDevice->tLogicalDevice, ptFrame->tInFlight, NULL);
        vkDestroyCommandPool(ptVulkanDevice->tLogicalDevice, ptFrame->tCmdPool, NULL);
        vkDestroyDescriptorPool(ptVulkanDevice->tLogicalDevice, ptFrame->tDynamicDescriptorPool, NULL);

        for(uint32_t j = 0; j < pl_sb_size(ptFrame->sbtRawBuffers); j++)
        {
            vkDestroyBuffer(ptVulkanDevice->tLogicalDevice, ptFrame->sbtRawBuffers[j], NULL);
            ptFrame->sbtRawBuffers[j] = VK_NULL_HANDLE;
        }

        for(uint32_t j = 0; j < pl_sb_size(ptFrame->sbtRawFrameBuffers); j++)
        {
            vkDestroyFramebuffer(ptVulkanDevice->tLogicalDevice, ptFrame->sbtRawFrameBuffers[j], NULL);
            ptFrame->sbtRawFrameBuffers[j] = VK_NULL_HANDLE;
        }

        pl_sb_free(ptFrame->sbtRawBuffers);
        pl_sb_free(ptFrame->sbtRawFrameBuffers);
        pl_sb_free(ptFrame->sbtDynamicBuffers);
    }

    plDeviceAllocatorData* ptData0 = (plDeviceAllocatorData*)ptGraphics->tDevice.tLocalDedicatedAllocator.ptInst;

    for(uint32_t i = 0; i < pl_sb_size(ptData0->sbtBlocks); i++)
    {
        if(ptData0->sbtBlocks[i].ulAddress)
            vkFreeMemory(ptVulkanDevice->tLogicalDevice, (VkDeviceMemory)ptData0->sbtBlocks[i].ulAddress, NULL);
    }

    plDeviceAllocatorData* ptData1 = (plDeviceAllocatorData*)ptGraphics->tDevice.tStagingUnCachedAllocator.ptInst;
    for(uint32_t i = 0; i < pl_sb_size(ptData1->sbtBlocks); i++)
    {
        if((VkDeviceMemory)ptData1->sbtBlocks[i].ulAddress)
            vkFreeMemory(ptVulkanDevice->tLogicalDevice, (VkDeviceMemory)ptData1->sbtBlocks[i].ulAddress, NULL);
    }

    plDeviceAllocatorData* ptData2 = (plDeviceAllocatorData*)ptGraphics->tDevice.tLocalBuddyAllocator.ptInst;
    for(uint32_t i = 0; i < pl_sb_size(ptData2->sbtBlocks); i++)
    {
        if((VkDeviceMemory)ptData2->sbtBlocks[i].ulAddress)
            vkFreeMemory(ptVulkanDevice->tLogicalDevice, (VkDeviceMemory)ptData2->sbtBlocks[i].ulAddress, NULL);
    }

    plDeviceAllocatorData* ptData3 = (plDeviceAllocatorData*)ptGraphics->tDevice.tStagingCachedAllocator.ptInst;
    for(uint32_t i = 0; i < pl_sb_size(ptData3->sbtBlocks); i++)
    {
        if((VkDeviceMemory)ptData3->sbtBlocks[i].ulAddress)
            vkFreeMemory(ptVulkanDevice->tLogicalDevice, (VkDeviceMemory)ptData3->sbtBlocks[i].ulAddress, NULL);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbtRenderPassesHot); i++)
    {
        if(ptVulkanGfx->sbtRenderPassesHot[i].tRenderPass)
            vkDestroyRenderPass(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtRenderPassesHot[i].tRenderPass, NULL);

        for(uint32_t j = 0; j < pl_sb_size(ptVulkanGfx->sbtRenderPassesHot[i].sbtFrameBuffers); j++)
        {
            if(ptVulkanGfx->sbtRenderPassesHot[i].sbtFrameBuffers[j])
                vkDestroyFramebuffer(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtRenderPassesHot[i].sbtFrameBuffers[j], NULL);
            ptVulkanGfx->sbtRenderPassesHot[i].sbtFrameBuffers[j] = VK_NULL_HANDLE;
        }
        pl_sb_free(ptVulkanGfx->sbtRenderPassesHot[i].sbtFrameBuffers);
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

static void
pl_dispatch(plGraphics* ptGraphics, uint32_t uDispatchCount, plDispatch* atDispatches)
{
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;
    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptGraphics); 

    // perform copy from staging buffer to destination buffer
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

    for(uint32_t i = 0; i < uDispatchCount; i++)
    {
        const plDispatch* ptDispatch = &atDispatches[i];
        plVulkanComputeShader* ptComputeShader = &ptVulkanGfx->sbtComputeShadersHot[ptDispatch->uShaderVariant];
        plVulkanBindGroup* ptBindGroup = &ptVulkanGfx->sbtBindGroupsHot[ptDispatch->uBindGroup0];

        vkCmdBindPipeline(tCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ptComputeShader->tPipeline);
        vkCmdBindDescriptorSets(tCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ptComputeShader->tPipelineLayout, 0, 1, &ptBindGroup->tDescriptorSet, 0, 0);
        vkCmdDispatch(tCommandBuffer, ptDispatch->uGroupCountX, ptDispatch->uGroupCountY, ptDispatch->uGroupCountZ);
    }

    PL_VULKAN(vkEndCommandBuffer(tCommandBuffer));
    const VkSubmitInfo tSubmitInfo = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1u,
        .pCommandBuffers    = &tCommandBuffer,
    };

    PL_VULKAN(vkQueueSubmit(ptVulkanDevice->tGraphicsQueue, 1, &tSubmitInfo, VK_NULL_HANDLE));
    PL_VULKAN(vkDeviceWaitIdle(ptVulkanDevice->tLogicalDevice));
    vkFreeCommandBuffers(ptVulkanDevice->tLogicalDevice, ptVulkanDevice->tCmdPool, 1, &tCommandBuffer);
}

static void
pl_draw_areas(plGraphics* ptGraphics, uint32_t uAreaCount, plDrawArea* atAreas)
{
    pl_begin_profile_sample(__FUNCTION__);
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptGraphics); 

    static VkDeviceSize offsets = { 0 };
    vkCmdSetDepthBias(ptCurrentFrame->tCmdBuf, 0.0f, 0.0f, 0.0f);

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

        vkCmdSetViewport(ptCurrentFrame->tCmdBuf, 0, 1, &tViewport);
        vkCmdSetScissor(ptCurrentFrame->tCmdBuf, 0, 1, &tScissor);  

        plDrawStream* ptStream = ptArea->ptDrawStream;

        const uint32_t uTokens = pl_sb_size(ptStream->sbtStream);
        uint32_t uCurrentStreamIndex = 0;
        uint32_t uTriangleCount = 0;
        uint32_t uIndexBuffer = 0;
        uint32_t uIndexBufferOffset = 0;
        uint32_t uVertexBufferOffset = 0;
        uint32_t uDynamicBufferOffset = 0;
        plVulkanShader* ptVulkanShader = NULL;
        plVulkanDynamicBuffer* ptVulkanDynamicBuffer = NULL;

        VkDescriptorSet atDescriptorSets[4] = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};

        while(uCurrentStreamIndex < uTokens)
        {

            uint32_t uDescriptorStart = 3;

            const uint32_t uDirtyMask = ptStream->sbtStream[uCurrentStreamIndex];
            uCurrentStreamIndex++;

            if(uDirtyMask & PL_DRAW_STREAM_BIT_SHADER)
            {
                ptVulkanShader = &ptVulkanGfx->sbtShadersHot[ptStream->sbtStream[uCurrentStreamIndex]];
                vkCmdBindPipeline(ptCurrentFrame->tCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, ptVulkanShader->tPipeline);
                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_OFFSET)
            {
                uDynamicBufferOffset = ptStream->sbtStream[uCurrentStreamIndex];
                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_BUFFER)
            {
                // uDescriptorStart = 3;
                ptVulkanDynamicBuffer = &ptCurrentFrame->sbtDynamicBuffers[ptStream->sbtStream[uCurrentStreamIndex]];
                atDescriptorSets[3] = ptVulkanDynamicBuffer->tDescriptorSet;
                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_2)
            {
                uDescriptorStart = 2;
                plVulkanBindGroup* ptBindGroup2 = &ptVulkanGfx->sbtBindGroupsHot[ptStream->sbtStream[uCurrentStreamIndex]];
                atDescriptorSets[2] = ptBindGroup2->tDescriptorSet;
                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_1)
            {
                uDescriptorStart = 1;
                plVulkanBindGroup* ptBindGroup1 = &ptVulkanGfx->sbtBindGroupsHot[ptStream->sbtStream[uCurrentStreamIndex]];
                atDescriptorSets[1] = ptBindGroup1->tDescriptorSet;
                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_0)
            {
                uDescriptorStart = 0;
                plVulkanBindGroup* ptBindGroup0 = &ptVulkanGfx->sbtBindGroupsHot[ptStream->sbtStream[uCurrentStreamIndex]];
                atDescriptorSets[0] = ptBindGroup0->tDescriptorSet;
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
                    vkCmdBindIndexBuffer(ptCurrentFrame->tCmdBuf, ptIndexBuffer->tBuffer, 0, VK_INDEX_TYPE_UINT32);
                }
                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_VERTEX_BUFFER)
            {
                plVulkanBuffer* ptVertexBuffer = &ptVulkanGfx->sbtBuffersHot[ptStream->sbtStream[uCurrentStreamIndex]];
                vkCmdBindVertexBuffers(ptCurrentFrame->tCmdBuf, 0, 1, &ptVertexBuffer->tBuffer, &offsets);
                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_TRIANGLES)
            {
                uTriangleCount = ptStream->sbtStream[uCurrentStreamIndex];
                uCurrentStreamIndex++;
            }

            vkCmdBindDescriptorSets(ptCurrentFrame->tCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, ptVulkanShader->tPipelineLayout, uDescriptorStart, 4 - uDescriptorStart, &atDescriptorSets[uDescriptorStart], 1, &uDynamicBufferOffset);

            if(uIndexBuffer == UINT32_MAX)
                vkCmdDraw(ptCurrentFrame->tCmdBuf, uTriangleCount * 3, 1, uVertexBufferOffset, 0);
            else
                vkCmdDrawIndexed(ptCurrentFrame->tCmdBuf, uTriangleCount * 3, 1, uIndexBufferOffset, uVertexBufferOffset, 0);
        }
    }
    pl_end_profile_sample();
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
            pl_queue_texture_view_for_deletion(&ptGraphics->tDevice, ptSwapchain->sbtSwapchainTextureViews[i]);
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
        plSampler tSampler = {
            .tFilter = PL_FILTER_NEAREST,
            .fMinMip = 0.0f,
            .fMaxMip = PL_MAX_MIPS,
            .tVerticalWrap = PL_WRAP_MODE_CLAMP,
            .tHorizontalWrap = PL_WRAP_MODE_CLAMP
        };
        ptSwapchain->sbtSwapchainTextureViews[i] = pl_create_swapchain_texture_view(&ptGraphics->tDevice, &tTextureViewDesc, &tSampler, ptVulkanSwapchain->sbtImages[i], "swapchain texture view");
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

static pl3DVulkanPipelineEntry*
pl__get_3d_pipelines(plGraphics* ptGfx, VkRenderPass tRenderPass, VkSampleCountFlagBits tMSAASampleCount, pl3DDrawFlags tFlags)
{
    plVulkanGraphics* ptVulkanGfx = ptGfx->_pInternalData;
    plVulkanDevice* ptVulkanDevice = ptGfx->tDevice._pInternalData;

    // return pipeline entry if it exists
    for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbt3DPipelines); i++)
    {
        if(ptVulkanGfx->sbt3DPipelines[i].tRenderPass == tRenderPass && tMSAASampleCount == ptVulkanGfx->sbt3DPipelines[i].tMSAASampleCount && ptVulkanGfx->sbt3DPipelines[i].tFlags == tFlags)
            return &ptVulkanGfx->sbt3DPipelines[i];
    }

    // create new pipeline entry
    pl3DVulkanPipelineEntry tEntry = {
        .tRenderPass      = tRenderPass,
        .tMSAASampleCount = tMSAASampleCount,
        .tFlags           = tFlags
    };

    const VkPipelineInputAssemblyStateCreateInfo tInputAssembly = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    const VkVertexInputAttributeDescription aAttributeDescriptions[] = {
        {0u, 0u, VK_FORMAT_R32G32B32_SFLOAT, 0u},
        {1u, 0u, VK_FORMAT_R8G8B8A8_UNORM,  12u}
    };
    
    const VkVertexInputBindingDescription tBindingDescription = {
        .binding   = 0u,
        .stride    = sizeof(plDrawVertex3DSolid),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    const VkPipelineVertexInputStateCreateInfo tVertexInputInfo = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 1u,
        .vertexAttributeDescriptionCount = 2u,
        .pVertexBindingDescriptions      = &tBindingDescription,
        .pVertexAttributeDescriptions    = aAttributeDescriptions
    };

    const VkVertexInputAttributeDescription aLineAttributeDescriptions[] = {
        {0u, 0u, VK_FORMAT_R32G32B32_SFLOAT, 0u},
        {1u, 0u, VK_FORMAT_R32G32B32_SFLOAT, 12u},
        {2u, 0u, VK_FORMAT_R32G32B32_SFLOAT, 24u},
        {3u, 0u, VK_FORMAT_R8G8B8A8_UNORM,  36u}
    };
    
    const VkVertexInputBindingDescription tLineBindingDescription = {
        .binding   = 0u,
        .stride    = sizeof(plDrawVertex3DLine),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    const VkPipelineVertexInputStateCreateInfo tLineVertexInputInfo = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 1u,
        .vertexAttributeDescriptionCount = 4u,
        .pVertexBindingDescriptions      = &tLineBindingDescription,
        .pVertexAttributeDescriptions    = aLineAttributeDescriptions
    };

    // dynamic, set per frame
    VkViewport tViewport = {0};
    VkRect2D tScissor = {0};

    const VkPipelineViewportStateCreateInfo tViewportState = {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports    = &tViewport,
        .scissorCount  = 1,
        .pScissors     = &tScissor
    };

    VkCullModeFlags tCullFlags = VK_CULL_MODE_NONE;
    if(tFlags & PL_PIPELINE_FLAG_CULL_FRONT)
        tCullFlags = VK_CULL_MODE_FRONT_BIT;
    else if(tFlags & PL_PIPELINE_FLAG_CULL_BACK)
        tCullFlags = VK_CULL_MODE_BACK_BIT;

    const VkPipelineRasterizationStateCreateInfo tRasterizer = {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable        = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode             = VK_POLYGON_MODE_FILL,
        .lineWidth               = 1.0f,
        .cullMode                = tCullFlags,
        .frontFace               = tFlags & PL_PIPELINE_FLAG_FRONT_FACE_CW ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable         = VK_FALSE
    };

    const VkPipelineDepthStencilStateCreateInfo tDepthStencil = {
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable       = tFlags & PL_PIPELINE_FLAG_DEPTH_TEST ? VK_TRUE : VK_FALSE,
        .depthWriteEnable      = tFlags & PL_PIPELINE_FLAG_DEPTH_WRITE ? VK_TRUE : VK_FALSE,
        .depthCompareOp        = VK_COMPARE_OP_LESS_OR_EQUAL,
        .depthBoundsTestEnable = VK_FALSE,
        .minDepthBounds        = 0.0f,
        .maxDepthBounds        = 1.0f,
        .stencilTestEnable     = VK_FALSE,
        .front                 = {0},
        .back                  = {0}
    };

    //---------------------------------------------------------------------
    // color blending stage
    //---------------------------------------------------------------------

    const VkPipelineColorBlendAttachmentState tColorBlendAttachment = {
        .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT,
        .blendEnable         = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp        = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp        = VK_BLEND_OP_ADD
    };

    const VkPipelineColorBlendStateCreateInfo tColorBlending = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable   = VK_FALSE,
        .logicOp         = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments    = &tColorBlendAttachment,
        .blendConstants  = {0}
    };

    const VkPipelineMultisampleStateCreateInfo tMultisampling = {
        .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable  = VK_FALSE,
        .rasterizationSamples = tMSAASampleCount
    };

    VkDynamicState atDynamicStateEnables[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    const VkPipelineDynamicStateCreateInfo tDynamicState = {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2u,
        .pDynamicStates    = atDynamicStateEnables
    };

    //---------------------------------------------------------------------
    // Create Regular Pipeline
    //---------------------------------------------------------------------

    VkPipelineShaderStageCreateInfo atShaderStages[] = { ptVulkanGfx->t3DVtxShdrStgInfo, ptVulkanGfx->t3DPxlShdrStgInfo };

    VkGraphicsPipelineCreateInfo pipeInfo = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount          = 2u,
        .pStages             = atShaderStages,
        .pVertexInputState   = &tVertexInputInfo,
        .pInputAssemblyState = &tInputAssembly,
        .pViewportState      = &tViewportState,
        .pRasterizationState = &tRasterizer,
        .pMultisampleState   = &tMultisampling,
        .pColorBlendState    = &tColorBlending,
        .pDynamicState       = &tDynamicState,
        .layout              = ptVulkanGfx->t3DPipelineLayout,
        .renderPass          = tRenderPass,
        .subpass             = 0u,
        .basePipelineHandle  = VK_NULL_HANDLE,
        .pDepthStencilState  = &tDepthStencil
    };
    PL_VULKAN(vkCreateGraphicsPipelines(ptVulkanDevice->tLogicalDevice, VK_NULL_HANDLE, 1, &pipeInfo, NULL, &tEntry.tRegularPipeline));

    // //---------------------------------------------------------------------
    // // Create SDF Pipeline
    // //---------------------------------------------------------------------

    atShaderStages[0] = ptVulkanGfx->t3DLineVtxShdrStgInfo;
    pipeInfo.pStages = atShaderStages;
    pipeInfo.pVertexInputState = &tLineVertexInputInfo;
    pipeInfo.layout = ptVulkanGfx->t3DLinePipelineLayout;

    PL_VULKAN(vkCreateGraphicsPipelines(ptVulkanDevice->tLogicalDevice, VK_NULL_HANDLE, 1, &pipeInfo, NULL, &tEntry.tSecondaryPipeline));

    // add to entries
    pl_sb_push(ptVulkanGfx->sbt3DPipelines, tEntry);

    return &pl_sb_back(ptVulkanGfx->sbt3DPipelines); 
}

static void
pl_copy_buffer(plDevice* ptDevice, plBufferHandle tSource, plBufferHandle tDestination, uint32_t uSourceOffset, uint32_t uDestinationOffset, size_t szSize)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptGraphics->_pInternalData;
    plFrameContext* ptFrame = pl__get_frame_resources(ptGraphics);


    const VkCommandBufferAllocateInfo tAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = ptFrame->tCmdPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VkCommandBuffer tCmdBuffer;
    PL_VULKAN(vkAllocateCommandBuffers(ptVulkanDevice->tLogicalDevice, &tAllocInfo, &tCmdBuffer));  

    const VkCommandBufferBeginInfo tBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    };
    PL_VULKAN(vkBeginCommandBuffer(tCmdBuffer, &tBeginInfo));   

    const VkBufferCopy tCopyRegion = {
        .size = szSize,
        .srcOffset = uSourceOffset
    };

    vkCmdCopyBuffer(tCmdBuffer, ptVulkanGraphics->sbtBuffersHot[tSource.uIndex].tBuffer, ptVulkanGraphics->sbtBuffersHot[tDestination.uIndex].tBuffer, 1, &tCopyRegion);

    PL_VULKAN(vkEndCommandBuffer(tCmdBuffer));   

    // submit
    const VkSubmitInfo tSubmitInfo = {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount   = 0,
        .pWaitSemaphores      = NULL,
        .pWaitDstStageMask    = NULL,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &tCmdBuffer,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores    = NULL
    };
    PL_VULKAN(vkResetFences(ptVulkanDevice->tLogicalDevice, 1, &ptFrame->tInFlight));
    PL_VULKAN(vkQueueSubmit(ptVulkanDevice->tGraphicsQueue, 1, &tSubmitInfo, ptFrame->tInFlight));  
    PL_VULKAN(vkQueueWaitIdle(ptVulkanDevice->tGraphicsQueue));
    vkFreeCommandBuffers(ptVulkanDevice->tLogicalDevice, ptFrame->tCmdPool, 1, &tCmdBuffer);
}

static void
pl_copy_buffer_to_texture(plDevice* ptDevice, plBufferHandle tBufferHandle, plTextureHandle tTextureHandle, uint32_t uRegionCount, const plBufferImageCopy* ptRegions)
{
    plGraphics*       ptGraphics       = ptDevice->ptGraphics;
    plVulkanDevice*   ptVulkanDevice   = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptGraphics->_pInternalData;
    plFrameContext* ptFrame = pl__get_frame_resources(ptGraphics);

    const VkCommandBufferAllocateInfo tAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = ptFrame->tCmdPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VkCommandBuffer tCmdBuffer;
    PL_VULKAN(vkAllocateCommandBuffers(ptVulkanDevice->tLogicalDevice, &tAllocInfo, &tCmdBuffer));  

    const VkCommandBufferBeginInfo tBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    };
    PL_VULKAN(vkBeginCommandBuffer(tCmdBuffer, &tBeginInfo));   

    plTexture* ptColdTexture = pl__get_texture(ptDevice, tTextureHandle);
    VkImageSubresourceRange* atSubResourceRanges = pl_temp_allocator_alloc(&ptVulkanGraphics->tTempAllocator, sizeof(VkImageSubresourceRange) * uRegionCount);
    VkBufferImageCopy*       atCopyRegions       = pl_temp_allocator_alloc(&ptVulkanGraphics->tTempAllocator, sizeof(VkBufferImageCopy) * uRegionCount);
    memset(atSubResourceRanges, 0, sizeof(VkImageSubresourceRange) * uRegionCount);
    memset(atCopyRegions, 0, sizeof(VkBufferImageCopy) * uRegionCount);

    for(uint32_t i = 0; i < uRegionCount; i++)
    {
        atSubResourceRanges[i].aspectMask     = ptColdTexture->tDesc.tUsage & PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        atSubResourceRanges[i].baseMipLevel   = ptRegions[i].uMipLevel;
        atSubResourceRanges[i].levelCount     = 1;
        atSubResourceRanges[i].baseArrayLayer = ptRegions[i].uBaseArrayLayer;
        atSubResourceRanges[i].layerCount     = ptRegions[i].uLayerCount;
        pl__transition_image_layout(tCmdBuffer, ptVulkanGraphics->sbtTexturesHot[tTextureHandle.uIndex].tImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, atSubResourceRanges[i], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

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
        pl__transition_image_layout(tCmdBuffer, ptVulkanGraphics->sbtTexturesHot[tTextureHandle.uIndex].tImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, atSubResourceRanges[i], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        
    pl_temp_allocator_reset(&ptVulkanGraphics->tTempAllocator);

    PL_VULKAN(vkEndCommandBuffer(tCmdBuffer));   

    // submit
    const VkSubmitInfo tSubmitInfo = {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount   = 0,
        .pWaitSemaphores      = NULL,
        .pWaitDstStageMask    = NULL,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &tCmdBuffer,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores    = NULL
    };
    PL_VULKAN(vkResetFences(ptVulkanDevice->tLogicalDevice, 1, &ptFrame->tInFlight));
    PL_VULKAN(vkQueueSubmit(ptVulkanDevice->tGraphicsQueue, 1, &tSubmitInfo, ptFrame->tInFlight));  
    PL_VULKAN(vkQueueWaitIdle(ptVulkanDevice->tGraphicsQueue));
    vkFreeCommandBuffers(ptVulkanDevice->tLogicalDevice, ptFrame->tCmdPool, 1, &tCmdBuffer);
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
        case PL_FORMAT_D32_FLOAT_S8_UINT:  return VK_FORMAT_D32_SFLOAT_S8_UINT;
        case PL_FORMAT_D24_UNORM_S8_UINT:  return VK_FORMAT_D24_UNORM_S8_UINT;
        case PL_FORMAT_D16_UNORM_S8_UINT:  return VK_FORMAT_D16_UNORM_S8_UINT;
    }

    PL_ASSERT(false && "Unsupported format");
    return VK_FORMAT_UNDEFINED;
}

static VkImageLayout
pl__vulkan_layout(plTextureLayout tLayout)
{
    switch(tLayout)
    {
        case PL_TEXTURE_LAYOUT_RENDER_TARGET: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case PL_TEXTURE_LAYOUT_DEPTH_STENCIL: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        case PL_TEXTURE_LAYOUT_PRESENT:       return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        case PL_TEXTURE_LAYOUT_SHADER_READ:   return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
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

static VkPipelineColorBlendAttachmentState
pl__get_blend_state(plBlendMode tBlendMode)
{

    static const VkPipelineColorBlendAttachmentState atStateMap[PL_BLEND_MODE_COUNT] =
    {
        // PL_BLEND_MODE_NONE
        { 
            .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            .blendEnable         = VK_FALSE,
        },

        // PL_BLEND_MODE_ALPHA
        {
            .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            .blendEnable         = VK_TRUE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .colorBlendOp        = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .alphaBlendOp        = VK_BLEND_OP_ADD
        },

        // PL_BLEND_MODE_ADDITIVE
        {
            .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            .blendEnable         = VK_TRUE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
            .colorBlendOp        = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .alphaBlendOp        = VK_BLEND_OP_ADD
        },

        // PL_BLEND_MODE_PREMULTIPLY
        {
            .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            .blendEnable         = VK_TRUE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .colorBlendOp        = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .alphaBlendOp        = VK_BLEND_OP_ADD
        },

        // PL_BLEND_MODE_MULTIPLY
        {
            .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            .blendEnable         = VK_TRUE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .colorBlendOp        = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .alphaBlendOp        = VK_BLEND_OP_ADD
        },

        // PL_BLEND_MODE_CLIP_MASK
        {
            .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            .blendEnable         = VK_TRUE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_ZERO,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .colorBlendOp        = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp        = VK_BLEND_OP_ADD
        }
    };

    PL_ASSERT(tBlendMode < PL_BLEND_MODE_COUNT && "blend mode out of range");
    return atStateMap[tBlendMode];
}

//-----------------------------------------------------------------------------
// [SECTION] device memory allocators
//-----------------------------------------------------------------------------

static plDeviceMemoryAllocation
pl_allocate_dedicated(struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;
    plVulkanDevice* ptVulkanDevice = ptData->ptDevice->_pInternalData;

    uint32_t uMemoryType = 0u;
    bool bFound = false;
    for (uint32_t i = 0; i < ptVulkanDevice->tMemProps.memoryTypeCount; i++) 
    {
        if ((uTypeFilter & (1 << i)) && (ptVulkanDevice->tMemProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) 
        {
            uMemoryType = i;
            bFound = true;
            break;
        }
    }
    PL_ASSERT(bFound);
    
    const VkMemoryAllocateInfo tAllocInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = ulSize,
        .memoryTypeIndex = uMemoryType
    };

    plDeviceMemoryAllocation tAllocation = {
        .pHostMapped = NULL,
        .uHandle     = 0,
        .ulOffset    = 0,
        .ulSize      = tAllocInfo.allocationSize,
        .ptInst      = ptInst
    };
    VkResult tResult = vkAllocateMemory(ptVulkanDevice->tLogicalDevice, &tAllocInfo, NULL, (VkDeviceMemory*)&tAllocation.uHandle);
    PL_VULKAN(tResult);

    ptData->ptDevice->ptGraphics->szLocalMemoryInUse += tAllocInfo.allocationSize;

    plDeviceAllocationBlock tBlock = {
        .ulAddress    = tAllocation.uHandle,
        .ulSize       = tAllocInfo.allocationSize,
        .ulMemoryType = (uint64_t)uMemoryType
    };

    uint32_t uBlockIndex = pl_sb_size(ptData->sbtBlocks);
    if(pl_sb_size(ptData->sbtFreeBlockIndices) > 0)
        uBlockIndex = pl_sb_pop(ptData->sbtFreeBlockIndices);
    else
        pl_sb_add(ptData->sbtBlocks);

    plDeviceAllocationRange tRange = {
        .ulOffset     = 0,
        .ulTotalSize  = tAllocInfo.allocationSize,
        .ulUsedSize   = ulSize,
        .ulBlockIndex = uBlockIndex
    };
    pl_sprintf(tRange.acName, "%s", pcName);

    if(pcName)
        pl_set_vulkan_object_name(ptData->ptDevice, tAllocation.uHandle, VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT, pcName);

    pl_sb_push(ptData->sbtNodes, tRange);
    ptData->sbtBlocks[uBlockIndex] = tBlock;
    return tAllocation;
}

static void
pl_free_dedicated(struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;
    plVulkanDevice* ptVulkanDevice = ptData->ptDevice->_pInternalData;

    uint32_t uBlockIndex = 0;
    uint32_t uNodeIndex = 0;
    for(uint32_t i = 0; i < pl_sb_size(ptData->sbtNodes); i++)
    {
        plDeviceAllocationRange* ptNode = &ptData->sbtNodes[i];
        plDeviceAllocationBlock* ptBlock = &ptData->sbtBlocks[ptNode->ulBlockIndex];

        if(ptBlock->ulAddress == ptAllocation->uHandle)
        {
            uNodeIndex = i;
            uBlockIndex = (uint32_t)ptNode->ulBlockIndex;
            ptBlock->ulSize = 0;
            ptBlock->ulAddress = 0;
            ptData->ptDevice->ptGraphics->szLocalMemoryInUse -= ptBlock->ulSize;
            break;
        }
    }
    pl_sb_del_swap(ptData->sbtNodes, uNodeIndex);
    pl_sb_push(ptData->sbtFreeBlockIndices, uBlockIndex);
    // pl_sb_del_swap(ptData->sbtBlocks, uBlockIndex);

    vkFreeMemory(ptVulkanDevice->tLogicalDevice, (VkDeviceMemory)ptAllocation->uHandle, NULL);
    ptAllocation->pHostMapped  = NULL;
    ptAllocation->uHandle      = 0;
    ptAllocation->ulOffset     = 0;
    ptAllocation->ulSize       = 0;
}

static plDeviceMemoryAllocation
pl_allocate_buddy(struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;
    plVulkanDevice* ptVulkanDevice = ptData->ptDevice->_pInternalData;

    // find what level we need
    uint32_t uMemoryType = 0u;
    bool bFound = false;
    for (uint32_t i = 0; i < ptVulkanDevice->tMemProps.memoryTypeCount; i++) 
    {
        if ((uTypeFilter & (1 << i)) && (ptVulkanDevice->tMemProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) 
        {
            uMemoryType = i;
            bFound = true;
            break;
        }
    }
    PL_ASSERT(bFound);

    plDeviceMemoryAllocation tAllocation = pl__allocate_buddy(ptInst, uTypeFilter, ulSize, ulAlignment, pcName, uMemoryType);
    
    if(tAllocation.uHandle == 0)
    {
        plDeviceAllocationBlock* ptBlock = &pl_sb_top(ptData->sbtBlocks);

        const VkMemoryAllocateInfo tAllocInfo = {
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize  = PL_DEVICE_BUDDY_BLOCK_SIZE,
            .memoryTypeIndex = uMemoryType
        };
        VkResult tResult = vkAllocateMemory(ptVulkanDevice->tLogicalDevice, &tAllocInfo, NULL, (VkDeviceMemory*)&ptBlock->ulAddress);
        PL_VULKAN(tResult);
        ptData->ptDevice->ptGraphics->szLocalMemoryInUse += PL_DEVICE_BUDDY_BLOCK_SIZE;
        tAllocation.uHandle = (uint64_t)ptBlock->ulAddress;
    }

    return tAllocation;
}

static plDeviceMemoryAllocation
pl_allocate_staging_uncached(struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;
    plVulkanDevice* ptVulkanDevice = ptData->ptDevice->_pInternalData;

    uint32_t uMemoryType = 0u;
    bool bFound = false;
    const VkMemoryPropertyFlags tProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
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

    plDeviceMemoryAllocation tAllocation = {
        .pHostMapped = NULL,
        .uHandle     = 0,
        .ulOffset    = 0,
        .ulSize      = ulSize,
        .ptInst      = ptInst
    };

    // check for existing block
    for(uint32_t i = 0; i < pl_sb_size(ptData->sbtNodes); i++)
    {
        plDeviceAllocationRange* ptNode = &ptData->sbtNodes[i];
        plDeviceAllocationBlock* ptBlock = &ptData->sbtBlocks[ptNode->ulBlockIndex];
        if(ptNode->ulUsedSize == 0 && ptNode->ulTotalSize >= ulSize && ptBlock->ulMemoryType == (uint64_t)uMemoryType && ptBlock->ulAddress != 0)
        {
            ptNode->ulUsedSize = ulSize;
            pl_sprintf(ptNode->acName, "%s", pcName);
            tAllocation.pHostMapped = ptBlock->pHostMapped;
            tAllocation.uHandle = ptBlock->ulAddress;
            tAllocation.ulOffset = 0;
            tAllocation.ulSize = ptBlock->ulSize;
            if(pcName)
                pl_set_vulkan_object_name(ptData->ptDevice, tAllocation.uHandle, VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT, pcName);
            return tAllocation;
        }
    }

    uint32_t uIndex = UINT32_MAX;
    if(pl_sb_size(ptData->sbtFreeBlockIndices) > 0)
    {
        uIndex = pl_sb_pop(ptData->sbtFreeBlockIndices);
    }
    else
    {
        uIndex = pl_sb_size(ptData->sbtBlocks);
        pl_sb_add(ptData->sbtNodes);
        pl_sb_add(ptData->sbtBlocks);
    }

    // block not found, create new block
    plDeviceAllocationBlock tBlock = {
        .ulAddress    = 0,
        .ulSize       = pl_maxu((uint32_t)ulSize, PL_DEVICE_ALLOCATION_BLOCK_SIZE),
        .ulMemoryType = uMemoryType
    };

    plDeviceAllocationRange tRange = {
        .ulOffset     = 0,
        .ulUsedSize   = ulSize,
        .ulTotalSize  = tBlock.ulSize,
        .ulBlockIndex = uIndex
    };
    pl_sprintf(tRange.acName, "%s", pcName);

    const VkMemoryAllocateInfo tAllocInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = tBlock.ulSize,
        .memoryTypeIndex = uMemoryType
    };

    PL_VULKAN(vkAllocateMemory(ptVulkanDevice->tLogicalDevice, &tAllocInfo, NULL, (VkDeviceMemory*)&tBlock.ulAddress));
    ptData->ptDevice->ptGraphics->szHostMemoryInUse += tBlock.ulSize;

    PL_VULKAN(vkMapMemory(ptVulkanDevice->tLogicalDevice, (VkDeviceMemory)tBlock.ulAddress, 0, tBlock.ulSize, 0, (void**)&tBlock.pHostMapped));

    tAllocation.pHostMapped = tBlock.pHostMapped;
    tAllocation.uHandle = tBlock.ulAddress;
    tAllocation.ulOffset = 0;

    if(pcName)
        pl_set_vulkan_object_name(ptData->ptDevice, tAllocation.uHandle, VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT, pcName);

    ptData->sbtNodes[uIndex] = tRange;
    ptData->sbtBlocks[uIndex] = tBlock;
    return tAllocation;
}

static void
pl_free_staging_uncached(struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;

    for(uint32_t i = 0; i < pl_sb_size(ptData->sbtNodes); i++)
    {
        plDeviceAllocationRange* ptRange = &ptData->sbtNodes[i];
        plDeviceAllocationBlock* ptBlock = &ptData->sbtBlocks[ptRange->ulBlockIndex];

        // find block
        if(ptBlock->ulAddress == ptAllocation->uHandle)
        {
            ptRange->ulUsedSize = 0;
            memset(ptRange->acName, 0, PL_MAX_NAME_LENGTH);
            strncpy(ptRange->acName, "not used", PL_MAX_NAME_LENGTH);
            break;
        }
    }
}

static plDeviceMemoryAllocation
pl_allocate_staging_cached(struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;
    plVulkanDevice* ptVulkanDevice = ptData->ptDevice->_pInternalData;

    uint32_t uMemoryType = 0u;
    bool bFound = false;
    const VkMemoryPropertyFlags tProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
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

    plDeviceMemoryAllocation tAllocation = {
        .pHostMapped = NULL,
        .uHandle     = 0,
        .ulOffset    = 0,
        .ulSize      = ulSize,
        .ptInst      = ptInst
    };

    // check for existing block
    for(uint32_t i = 0; i < pl_sb_size(ptData->sbtNodes); i++)
    {
        plDeviceAllocationRange* ptNode = &ptData->sbtNodes[i];
        plDeviceAllocationBlock* ptBlock = &ptData->sbtBlocks[ptNode->ulBlockIndex];
        if(ptNode->ulUsedSize == 0 && ptNode->ulTotalSize >= ulSize && ptBlock->ulMemoryType == (uint64_t)uMemoryType)
        {
            ptNode->ulUsedSize = ulSize;
            pl_sprintf(ptNode->acName, "%s", pcName);
            tAllocation.pHostMapped = ptBlock->pHostMapped;
            tAllocation.uHandle = ptBlock->ulAddress;
            tAllocation.ulOffset = 0;
            tAllocation.ulSize = ptBlock->ulSize;
            if(pcName)
                pl_set_vulkan_object_name(ptData->ptDevice, tAllocation.uHandle, VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT, pcName);
            return tAllocation;
        }
    }

    // block not found, create new block
    plDeviceAllocationBlock tBlock = {
        .ulAddress    = 0,
        .ulSize       = pl_maxu((uint32_t)ulSize, PL_DEVICE_ALLOCATION_BLOCK_SIZE),
        .ulMemoryType = uMemoryType
    };

    plDeviceAllocationRange tRange = {
        .ulOffset     = 0,
        .ulUsedSize   = ulSize,
        .ulTotalSize  = tBlock.ulSize,
        .ulBlockIndex = pl_sb_size(ptData->sbtBlocks)
    };
    pl_sprintf(tRange.acName, "%s", pcName);

    const VkMemoryAllocateInfo tAllocInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = tBlock.ulSize,
        .memoryTypeIndex = uMemoryType
    };

    PL_VULKAN(vkAllocateMemory(ptVulkanDevice->tLogicalDevice, &tAllocInfo, NULL, (VkDeviceMemory*)&tBlock.ulAddress));
    ptData->ptDevice->ptGraphics->szHostMemoryInUse += tBlock.ulSize;

    PL_VULKAN(vkMapMemory(ptVulkanDevice->tLogicalDevice, (VkDeviceMemory)tBlock.ulAddress, 0, tBlock.ulSize, 0, (void**)&tBlock.pHostMapped));

    tAllocation.pHostMapped = tBlock.pHostMapped;
    tAllocation.uHandle = tBlock.ulAddress;
    tAllocation.ulOffset = 0;

    if(pcName)
        pl_set_vulkan_object_name(ptData->ptDevice, tAllocation.uHandle, VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT, pcName);

    pl_sb_push(ptData->sbtNodes, tRange);
    pl_sb_push(ptData->sbtBlocks, tBlock);
    return tAllocation;
}

static void
pl_free_staging_cached(struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;

    for(uint32_t i = 0; i < pl_sb_size(ptData->sbtNodes); i++)
    {
        plDeviceAllocationRange* ptRange = &ptData->sbtNodes[i];
        plDeviceAllocationBlock* ptBlock = &ptData->sbtBlocks[ptRange->ulBlockIndex];

        // find block
        if(ptBlock->ulAddress == ptAllocation->uHandle)
        {
            ptRange->ulUsedSize = 0;
            memset(ptRange->acName, 0, PL_MAX_NAME_LENGTH);
            strncpy(ptRange->acName, "not used", PL_MAX_NAME_LENGTH);
            break;
        }
    }
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
        vkDestroyImage(ptVulkanDevice->tLogicalDevice, ptVulkanResource->tImage, NULL);
        ptVulkanResource->tImage = VK_NULL_HANDLE;
        pl_sb_push(ptGraphics->sbtTextureFreeIndices, iResourceIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtTextureViews); i++)
    {
        const uint32_t iResourceIndex = ptGarbage->sbtTextureViews[i].uIndex;
        plVulkanSampler* ptVulkanResource = &ptVulkanGfx->sbtSamplersHot[iResourceIndex];
        vkDestroyImageView(ptVulkanDevice->tLogicalDevice, ptVulkanResource->tImageView, NULL);
        vkDestroySampler(ptVulkanDevice->tLogicalDevice, ptVulkanResource->tSampler, NULL);
        ptVulkanResource->tImageView = VK_NULL_HANDLE;
        ptVulkanResource->tSampler = VK_NULL_HANDLE;
        pl_sb_push(ptGraphics->sbtTextureViewFreeIndices, iResourceIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtRenderPasses); i++)
    {
        const uint32_t iResourceIndex = ptGarbage->sbtRenderPasses[i].uIndex;
        plVulkanRenderPass* ptVulkanResource = &ptVulkanGfx->sbtRenderPassesHot[iResourceIndex];
        for(uint32_t j = 0; j < pl_sb_size(ptVulkanResource->sbtFrameBuffers); j++)
        {
            vkDestroyFramebuffer(ptVulkanDevice->tLogicalDevice, ptVulkanResource->sbtFrameBuffers[j], NULL);
            ptVulkanResource->sbtFrameBuffers[j] = VK_NULL_HANDLE;
        }
        if(ptVulkanResource->tRenderPass)
            vkDestroyRenderPass(ptVulkanDevice->tLogicalDevice, ptVulkanResource->tRenderPass, NULL);
        ptVulkanResource->tRenderPass = VK_NULL_HANDLE;
        pl_sb_push(ptGraphics->sbtRenderPassFreeIndices, iResourceIndex);
        pl_sb_reset(ptVulkanResource->sbtFrameBuffers);
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

        for(uint32_t j = 0; j < pl_sb_size(ptResource->_sbtVariantHandles); j++)
        {
            const uint32_t iVariantIndex = ptResource->_sbtVariantHandles[j].uIndex;
            plVulkanShader* ptVariantVulkanResource = &ptVulkanGfx->sbtShadersHot[iVariantIndex];
            vkDestroyPipelineLayout(ptVulkanDevice->tLogicalDevice, ptVariantVulkanResource->tPipelineLayout, NULL);
            vkDestroyPipeline(ptVulkanDevice->tLogicalDevice, ptVariantVulkanResource->tPipeline, NULL);
            ptVariantVulkanResource->tPipelineLayout = VK_NULL_HANDLE;
            ptVariantVulkanResource->tPipeline = VK_NULL_HANDLE;
            pl_sb_push(ptGraphics->sbtShaderFreeIndices, iVariantIndex);
            for(uint32_t k = 0; k < ptResource->tDescription.uBindGroupLayoutCount + 1; k++)
            {
                plVulkanBindGroupLayout* ptVulkanBindGroupLayout = &ptVulkanGfx->sbtBindGroupLayouts[ptResource->tDescription.atBindGroupLayouts[k].uHandle];
                vkDestroyDescriptorSetLayout(ptVulkanDevice->tLogicalDevice, ptVulkanBindGroupLayout->tDescriptorSetLayout, NULL);   
                ptVulkanBindGroupLayout->tDescriptorSetLayout = VK_NULL_HANDLE;
                pl_sb_push(ptVulkanGfx->sbtBindGroupLayoutFreeIndices, ptResource->tDescription.atBindGroupLayouts[k].uHandle);
            }
        }
        pl_sb_free(ptResource->_sbtVariantHandles);
        pl_hm_free(&ptResource->tVariantHashmap);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtComputeShaders); i++)
    {
        const uint32_t iResourceIndex = ptGarbage->sbtComputeShaders[i].uIndex;
        plComputeShader* ptResource = &ptGraphics->sbtComputeShadersCold[iResourceIndex];
        plVulkanComputeShader* ptVulkanResource = &ptVulkanGfx->sbtComputeShadersHot[iResourceIndex];
        if(ptVulkanResource->tShaderModule)
            vkDestroyShaderModule(ptVulkanDevice->tLogicalDevice, ptVulkanResource->tShaderModule, NULL);

        ptVulkanResource->tShaderModule = VK_NULL_HANDLE;

        for(uint32_t j = 0; j < pl_sb_size(ptResource->_sbtVariantHandles); j++)
        {
            const uint32_t iVariantIndex = ptResource->_sbtVariantHandles[j].uIndex;
            plVulkanComputeShader* ptVariantVulkanResource = &ptVulkanGfx->sbtComputeShadersHot[iVariantIndex];
            vkDestroyPipelineLayout(ptVulkanDevice->tLogicalDevice, ptVariantVulkanResource->tPipelineLayout, NULL);
            vkDestroyPipeline(ptVulkanDevice->tLogicalDevice, ptVariantVulkanResource->tPipeline, NULL);
            ptVariantVulkanResource->tPipelineLayout = VK_NULL_HANDLE;
            ptVariantVulkanResource->tPipeline = VK_NULL_HANDLE;
            pl_sb_push(ptGraphics->sbtComputeShaderFreeIndices, iVariantIndex);
   
            plVulkanBindGroupLayout* ptVulkanBindGroupLayout = &ptVulkanGfx->sbtBindGroupLayouts[ptResource->tDescription.tBindGroupLayout.uHandle];
            vkDestroyDescriptorSetLayout(ptVulkanDevice->tLogicalDevice, ptVulkanBindGroupLayout->tDescriptorSetLayout, NULL);   
            ptVulkanBindGroupLayout->tDescriptorSetLayout = VK_NULL_HANDLE;
            pl_sb_push(ptVulkanGfx->sbtBindGroupLayoutFreeIndices, ptResource->tDescription.tBindGroupLayout.uHandle);
        }
        pl_sb_free(ptResource->_sbtVariantHandles);
        pl_hm_free(&ptResource->tVariantHashmap);
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

    for(uint32_t i = 0; i < pl_sb_size(ptCurrentFrame->sbtRawBuffers); i++)
    {
        vkDestroyBuffer(ptVulkanDevice->tLogicalDevice, ptCurrentFrame->sbtRawBuffers[i], NULL);
        ptCurrentFrame->sbtRawBuffers[i] = VK_NULL_HANDLE;
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
        if(ptGarbage->sbtMemory[i].ptInst == ptGraphics->tDevice.tLocalBuddyAllocator.ptInst)
            ptGraphics->tDevice.tLocalBuddyAllocator.free(ptGraphics->tDevice.tLocalBuddyAllocator.ptInst, &ptGarbage->sbtMemory[i]);
        else if(ptGarbage->sbtMemory[i].ptInst == ptGraphics->tDevice.tLocalDedicatedAllocator.ptInst)
            ptGraphics->tDevice.tLocalDedicatedAllocator.free(ptGraphics->tDevice.tLocalDedicatedAllocator.ptInst, &ptGarbage->sbtMemory[i]);
        else if(ptGarbage->sbtMemory[i].ptInst == ptGraphics->tDevice.tStagingUnCachedAllocator.ptInst)
            ptGraphics->tDevice.tStagingUnCachedAllocator.free(ptGraphics->tDevice.tStagingUnCachedAllocator.ptInst, &ptGarbage->sbtMemory[i]);
        else if(ptGarbage->sbtMemory[i].ptInst == ptGraphics->tDevice.tStagingCachedAllocator.ptInst)
            ptGraphics->tDevice.tStagingCachedAllocator.free(ptGraphics->tDevice.tStagingCachedAllocator.ptInst, &ptGarbage->sbtMemory[i]);
    }

    plDeviceAllocatorData* ptUnCachedAllocatorData = (plDeviceAllocatorData*)ptGraphics->tDevice.tStagingUnCachedAllocator.ptInst;

    plIO* ptIO = pl_get_io();
    for(uint32_t i = 0; i < pl_sb_size(ptUnCachedAllocatorData->sbtNodes); i++)
    {
        plDeviceAllocationRange* ptNode = &ptUnCachedAllocatorData->sbtNodes[i];
        plDeviceAllocationBlock* ptBlock = &ptUnCachedAllocatorData->sbtBlocks[ptNode->ulBlockIndex];

        if(ptBlock->ulAddress == 0)
        {
            continue;
        }
        if(ptNode->ulUsedSize == 0 && ptIO->dTime - ptBlock->dLastTimeUsed > 1.0)
        {
            ptGraphics->szHostMemoryInUse -= ptBlock->ulSize;
            vkUnmapMemory(ptVulkanDevice->tLogicalDevice, (VkDeviceMemory)ptBlock->ulAddress);
            vkFreeMemory(ptVulkanDevice->tLogicalDevice, (VkDeviceMemory)ptBlock->ulAddress, NULL);
            ptBlock->ulAddress = 0;
            pl_sb_push(ptUnCachedAllocatorData->sbtFreeBlockIndices, (uint32_t)ptNode->ulBlockIndex);
        }
        else if(ptNode->ulUsedSize != 0)
            ptBlock->dLastTimeUsed = ptIO->dTime;
    }

    pl_sb_reset(ptGarbage->sbtTextures);
    pl_sb_reset(ptGarbage->sbtTextureViews);
    pl_sb_reset(ptGarbage->sbtShaders);
    pl_sb_reset(ptGarbage->sbtComputeShaders);
    pl_sb_reset(ptGarbage->sbtRenderPasses);
    pl_sb_reset(ptGarbage->sbtRenderPassLayouts);
    pl_sb_reset(ptGarbage->sbtMemory);
    pl_sb_reset(ptCurrentFrame->sbtRawBuffers);
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

    if(ptBuffer->tMemoryAllocation.ptInst == ptGraphics->tDevice.tLocalBuddyAllocator.ptInst)
        ptGraphics->tDevice.tLocalBuddyAllocator.free(ptGraphics->tDevice.tLocalBuddyAllocator.ptInst, &ptBuffer->tMemoryAllocation);
    else if(ptBuffer->tMemoryAllocation.ptInst == ptGraphics->tDevice.tLocalDedicatedAllocator.ptInst)
        ptGraphics->tDevice.tLocalDedicatedAllocator.free(ptGraphics->tDevice.tLocalDedicatedAllocator.ptInst, &ptBuffer->tMemoryAllocation);
    else if(ptBuffer->tMemoryAllocation.ptInst == ptGraphics->tDevice.tStagingUnCachedAllocator.ptInst)
        ptGraphics->tDevice.tStagingUnCachedAllocator.free(ptGraphics->tDevice.tStagingUnCachedAllocator.ptInst, &ptBuffer->tMemoryAllocation);
    else if(ptBuffer->tMemoryAllocation.ptInst == ptGraphics->tDevice.tStagingCachedAllocator.ptInst)
        ptGraphics->tDevice.tStagingCachedAllocator.free(ptGraphics->tDevice.tStagingCachedAllocator.ptInst, &ptBuffer->tMemoryAllocation);
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

    if(ptTexture->tMemoryAllocation.ptInst == ptGraphics->tDevice.tLocalBuddyAllocator.ptInst)
        ptGraphics->tDevice.tLocalBuddyAllocator.free(ptGraphics->tDevice.tLocalBuddyAllocator.ptInst, &ptTexture->tMemoryAllocation);
    else if(ptTexture->tMemoryAllocation.ptInst == ptGraphics->tDevice.tLocalDedicatedAllocator.ptInst)
        ptGraphics->tDevice.tLocalDedicatedAllocator.free(ptGraphics->tDevice.tLocalDedicatedAllocator.ptInst, &ptTexture->tMemoryAllocation);
    else if(ptTexture->tMemoryAllocation.ptInst == ptGraphics->tDevice.tStagingUnCachedAllocator.ptInst)
        ptGraphics->tDevice.tStagingUnCachedAllocator.free(ptGraphics->tDevice.tStagingUnCachedAllocator.ptInst, &ptTexture->tMemoryAllocation);
    else if(ptTexture->tMemoryAllocation.ptInst == ptGraphics->tDevice.tStagingCachedAllocator.ptInst)
        ptGraphics->tDevice.tStagingCachedAllocator.free(ptGraphics->tDevice.tStagingCachedAllocator.ptInst, &ptTexture->tMemoryAllocation);
}

static void
pl_destroy_texture_view(plDevice* ptDevice, plTextureViewHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;

    plVulkanSampler* ptVulkanResource = &ptVulkanGfx->sbtSamplersHot[tHandle.uIndex];
    vkDestroyImageView(ptVulkanDevice->tLogicalDevice, ptVulkanResource->tImageView, NULL);
    vkDestroySampler(ptVulkanDevice->tLogicalDevice, ptVulkanResource->tSampler, NULL);
    ptVulkanResource->tImageView = VK_NULL_HANDLE;
    ptVulkanResource->tSampler = VK_NULL_HANDLE;
    ptGraphics->sbtTextureViewGenerations[tHandle.uIndex]++;
    pl_sb_push(ptGraphics->sbtTextureViewFreeIndices, tHandle.uIndex);
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
    for(uint32_t j = 0; j < pl_sb_size(ptVulkanResource->sbtFrameBuffers); j++)
    {
        vkDestroyFramebuffer(ptVulkanDevice->tLogicalDevice, ptVulkanResource->sbtFrameBuffers[j], NULL);
        ptVulkanResource->sbtFrameBuffers[j] = VK_NULL_HANDLE;
    }
    if(ptVulkanResource->tRenderPass)
        vkDestroyRenderPass(ptVulkanDevice->tLogicalDevice, ptVulkanResource->tRenderPass, NULL);
    ptVulkanResource->tRenderPass = VK_NULL_HANDLE;
    pl_sb_push(ptGraphics->sbtRenderPassFreeIndices, tHandle.uIndex);
    pl_sb_reset(ptVulkanResource->sbtFrameBuffers);
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

    for(uint32_t j = 0; j < pl_sb_size(ptResource->_sbtVariantHandles); j++)
    {
        const uint32_t iVariantIndex = ptResource->_sbtVariantHandles[j].uIndex;
        plVulkanShader* ptVariantVulkanResource = &ptVulkanGfx->sbtShadersHot[iVariantIndex];
        vkDestroyPipelineLayout(ptVulkanDevice->tLogicalDevice, ptVariantVulkanResource->tPipelineLayout, NULL);
        vkDestroyPipeline(ptVulkanDevice->tLogicalDevice, ptVariantVulkanResource->tPipeline, NULL);
        ptVariantVulkanResource->tPipelineLayout = VK_NULL_HANDLE;
        ptVariantVulkanResource->tPipeline = VK_NULL_HANDLE;
        pl_sb_push(ptGraphics->sbtShaderFreeIndices, iVariantIndex);
        for(uint32_t k = 0; k < ptResource->tDescription.uBindGroupLayoutCount + 1; k++)
        {
            plVulkanBindGroupLayout* ptVulkanBindGroupLayout = &ptVulkanGfx->sbtBindGroupLayouts[ptResource->tDescription.atBindGroupLayouts[k].uHandle];
            vkDestroyDescriptorSetLayout(ptVulkanDevice->tLogicalDevice, ptVulkanBindGroupLayout->tDescriptorSetLayout, NULL);   
            ptVulkanBindGroupLayout->tDescriptorSetLayout = VK_NULL_HANDLE;
            pl_sb_push(ptVulkanGfx->sbtBindGroupLayoutFreeIndices, ptResource->tDescription.atBindGroupLayouts[k].uHandle);
        }
    }
    pl_sb_free(ptResource->_sbtVariantHandles);
    pl_hm_free(&ptResource->tVariantHashmap);
}

static void
pl_destroy_compute_shader(plDevice* ptDevice, plComputeShaderHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    ptGraphics->sbtComputeShaderGenerations[tHandle.uIndex]++;

    plComputeShader* ptResource = &ptGraphics->sbtComputeShadersCold[tHandle.uIndex];

    for(uint32_t j = 0; j < pl_sb_size(ptResource->_sbtVariantHandles); j++)
    {
        const uint32_t iVariantIndex = ptResource->_sbtVariantHandles[j].uIndex;
        plVulkanComputeShader* ptVariantVulkanResource = &ptVulkanGfx->sbtComputeShadersHot[iVariantIndex];
        vkDestroyPipelineLayout(ptVulkanDevice->tLogicalDevice, ptVariantVulkanResource->tPipelineLayout, NULL);
        vkDestroyPipeline(ptVulkanDevice->tLogicalDevice, ptVariantVulkanResource->tPipeline, NULL);
        ptVariantVulkanResource->tPipelineLayout = VK_NULL_HANDLE;
        ptVariantVulkanResource->tPipeline = VK_NULL_HANDLE;
        pl_sb_push(ptGraphics->sbtComputeShaderFreeIndices, iVariantIndex);

        plVulkanBindGroupLayout* ptVulkanBindGroupLayout = &ptVulkanGfx->sbtBindGroupLayouts[ptResource->tDescription.tBindGroupLayout.uHandle];
        vkDestroyDescriptorSetLayout(ptVulkanDevice->tLogicalDevice, ptVulkanBindGroupLayout->tDescriptorSetLayout, NULL);   
        ptVulkanBindGroupLayout->tDescriptorSetLayout = VK_NULL_HANDLE;
        pl_sb_push(ptVulkanGfx->sbtBindGroupLayoutFreeIndices, ptResource->tDescription.tBindGroupLayout.uHandle);
    }
    pl_sb_free(ptResource->_sbtVariantHandles);
    pl_hm_free(&ptResource->tVariantHashmap);
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
        .setup_ui                         = pl_setup_ui,
        .begin_frame                      = pl_begin_frame,
        .end_frame                        = pl_end_gfx_frame,
        .begin_recording                  = pl_begin_recording,
        .end_recording                    = pl_end_recording,
        .begin_main_pass                  = pl_begin_main_pass,
        .end_main_pass                    = pl_end_main_pass,
        .begin_pass                       = pl_begin_pass,
        .end_pass                         = pl_end_pass,
        .dispatch                         = pl_dispatch,
        .draw_areas                       = pl_draw_areas,
        .draw_lists                       = pl_draw_list,
        .cleanup                          = pl_shutdown,
        .create_font_atlas                = pl_create_vulkan_font_texture,
        .destroy_font_atlas               = pl_cleanup_vulkan_font_texture,
        .add_3d_triangle_filled           = pl__add_3d_triangle_filled,
        .add_3d_line                      = pl__add_3d_line,
        .add_3d_point                     = pl__add_3d_point,
        .add_3d_transform                 = pl__add_3d_transform,
        .add_3d_frustum                   = pl__add_3d_frustum,
        .add_3d_centered_box              = pl__add_3d_centered_box,
        .add_3d_bezier_quad               = pl__add_3d_bezier_quad,
        .add_3d_bezier_cubic              = pl__add_3d_bezier_cubic,
        .add_3d_aabb                      = pl__add_3d_aabb,
        .register_3d_drawlist             = pl__register_3d_drawlist,
        .submit_3d_drawlist               = pl__submit_3d_drawlist,
        .get_ui_texture_handle            = pl_get_ui_texture_handle
    };
    return &tApi;
}

static const plDeviceI*
pl_load_device_api(void)
{
    static const plDeviceI tApi = {
        .create_buffer                          = pl_create_buffer,
        .create_shader                          = pl_create_shader,
        .create_compute_shader                  = pl_create_compute_shader,
        .create_render_pass_layout              = pl_create_render_pass_layout,
        .create_render_pass                     = pl_create_render_pass,
        .create_texture                         = pl_create_texture,
        .create_texture_view                    = pl_create_texture_view,
        .generate_mipmaps                       = pl_generate_mipmaps,
        .create_bind_group                      = pl_create_bind_group,
        .get_temporary_bind_group               = pl_get_temporary_bind_group,
        .update_bind_group                      = pl_update_bind_group,
        .copy_buffer                            = pl_copy_buffer,
        .transfer_image_to_buffer               = pl_transfer_image_to_buffer,
        .allocate_dynamic_data                  = pl_allocate_dynamic_data,
        .queue_buffer_for_deletion              = pl_queue_buffer_for_deletion,
        .queue_texture_for_deletion             = pl_queue_texture_for_deletion,
        .queue_texture_view_for_deletion        = pl_queue_texture_view_for_deletion,
        .queue_bind_group_for_deletion          = pl_queue_bind_group_for_deletion,
        .queue_shader_for_deletion              = pl_queue_shader_for_deletion,
        .queue_compute_shader_for_deletion      = pl_queue_compute_shader_for_deletion,
        .queue_render_pass_for_deletion         = pl_queue_render_pass_for_deletion,
        .queue_render_pass_layout_for_deletion  = pl_queue_render_pass_layout_for_deletion,
        .destroy_texture_view                   = pl_queue_texture_view_for_deletion,
        .destroy_buffer                         = pl_destroy_buffer,
        .destroy_texture                        = pl_destroy_texture,
        .destroy_bind_group                     = pl_destroy_bind_group,
        .destroy_shader                         = pl_destroy_shader,
        .destroy_compute_shader                 = pl_destroy_compute_shader,
        .destroy_render_pass                    = pl_destroy_render_pass,
        .destroy_render_pass_layout             = pl_destroy_render_pass_layout,
        .update_render_pass_attachments         = pl_update_render_pass_attachments,
        .get_buffer                             = pl__get_buffer,
        .get_texture                            = pl__get_texture,
        .get_texture_view                       = pl__get_texture_view,
        .get_bind_group                         = pl__get_bind_group,
        .get_shader                             = pl__get_shader,
        .get_compute_shader_variant             = pl_get_compute_shader_variant,
        .get_shader_variant                     = pl_get_shader_variant,
        .copy_buffer_to_texture                 = pl_copy_buffer_to_texture
    };
    return &tApi;
}

PL_EXPORT void
pl_load_graphics_ext(plApiRegistryApiI* ptApiRegistry, bool bReload)
{
    const plDataRegistryApiI* ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(ptDataRegistry->get_data(PL_CONTEXT_MEMORY));
    pl_set_profile_context(ptDataRegistry->get_data("profile"));
    pl_set_log_context(ptDataRegistry->get_data("log"));
    pl_set_context(ptDataRegistry->get_data("ui"));
    gptFile = ptApiRegistry->first(PL_API_FILE);
    if(bReload)
    {
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_GRAPHICS), pl_load_graphics_api());
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_DEVICE), pl_load_device_api());
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_DRAW_STREAM), pl_load_drawstream_api());

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
        ptApiRegistry->add(PL_API_DRAW_STREAM, pl_load_drawstream_api());      
        uLogChannel = pl_add_log_channel("Vulkan", PL_CHANNEL_TYPE_CYCLIC_BUFFER);
    }
}

PL_EXPORT void
pl_unload_graphics_ext(plApiRegistryApiI* ptApiRegistry)
{
    
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#include "pl_ui_vulkan.c"
