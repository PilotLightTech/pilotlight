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
#include "pl_graphics_ext.c"

// vulkan stuff
#if defined(_WIN32)
    #define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__APPLE__)
    #define VK_USE_PLATFORM_METAL_EXT
#else // linux
    #define VK_USE_PLATFORM_XCB_KHR
#endif

#define PL_DEVICE_ALLOCATION_BLOCK_SIZE 268435456
#define PL_DEVICE_LOCAL_LEVELS 8

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

typedef struct _pl3DBufferReturn
{
    VkBuffer       tBuffer;
    VkDeviceMemory tDeviceMemory;
    int64_t        slFreedFrame;
} pl3DBufferReturn;

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
    VkBuffer        tVertexBuffer;
    VkDeviceMemory  tVertexMemory;
    unsigned char*  ucVertexBufferMap;
    uint32_t        uVertexByteSize;
    uint32_t        uVertexBufferOffset;

    // index buffer
    VkBuffer       tIndexBuffer;
    VkDeviceMemory tIndexMemory;
    unsigned char* ucIndexBufferMap;
    uint32_t       uIndexByteSize;
    uint32_t       uIndexBufferOffset;
} pl3DVulkanBufferInfo;

typedef struct _plVulkanDynamicBuffer
{
    uint32_t              uHandle;
    VkBuffer              tBuffer;
    VkDeviceMemory        tMemory;
    VkDescriptorSet       tDescriptorSet;
} plVulkanDynamicBuffer;

typedef struct _plVulkanBuffer
{
    char*          pcData;
    VkBuffer       tBuffer;
    VkDeviceMemory tMemory;
} plVulkanBuffer;

typedef struct _plVulkanTexture
{
    VkImage        tImage;
    VkDeviceMemory tMemory;
} plVulkanTexture;

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
    VkPipelineLayout      tPipelineLayout;
    VkPipeline            tPipeline;
    VkSampleCountFlagBits tMSAASampleCount;
    VkShaderModule        tVertexShaderModule;
    VkShaderModule        tPixelShaderModule;
} plVulkanShader;

typedef struct _plFrameGarbage
{
    VkImage*        sbtTextures;
    VkImageView*    sbtTextureViews;
    VkFramebuffer*  sbtFrameBuffers;
    VkDeviceMemory* sbtMemory;
} plFrameGarbage;

typedef struct _plFrameContext
{
    VkSemaphore     tImageAvailable;
    VkSemaphore     tRenderFinish;
    VkFence         tInFlight;
    VkCommandPool   tCmdPool;
    VkCommandBuffer tCmdBuf;
} plFrameContext;

typedef struct _plVulkanSwapchain
{
    VkSwapchainKHR        tSwapChain;
    VkExtent2D            tExtent;
    VkFramebuffer*        sbtFrameBuffers;
    VkFormat              tFormat;
    VkFormat              tDepthFormat;
    uint32_t              uImageCount;
    VkImage*              sbtImages;
    VkImageView*          sbtImageViews;
    VkImage               tColorTexture;
    VkDeviceMemory        tColorTextureMemory;
    VkImageView           tColorTextureView;
    VkImage               tDepthTexture;
    VkDeviceMemory        tDepthTextureMemory;
    VkImageView           tDepthTextureView;
    uint32_t              uCurrentImageIndex; // current image to use within the swap chain
    bool                  bVSync;
    VkSampleCountFlagBits tMsaaSamples;
    VkSurfaceFormatKHR*   sbtSurfaceFormats;

} plVulkanSwapchain;

typedef struct _plVulkanDevice
{
    plGraphics*                               ptGraphics;
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

    // [INTERNAL]
    plFrameGarbage* _sbtFrameGarbage;

} plVulkanDevice;

typedef struct _plVulkanGraphics
{
    VkInstance               tInstance;
    VkDebugUtilsMessengerEXT tDbgMessenger;
    VkSurfaceKHR             tSurface;
    plFrameContext*          sbFrames;
    VkRenderPass             tRenderPass;
    VkDescriptorPool         tDescriptorPool;
    plVulkanSwapchain        tSwapchain;

    plVulkanTexture*         sbtTextures;
    plVulkanSampler*         sbtSamplers;
    plVulkanBindGroup*       sbtBindGroups;
    plVulkanBuffer*          sbtBuffers;
    plVulkanShader*          sbtShaders;
    plVulkanBindGroupLayout* sbtBindGroupLayouts;

    VkDescriptorSetLayout tDynamicDescriptorSetLayout;
    plVulkanDynamicBuffer tDynamicBuffers[2];
    uint32_t              uDynamicByteOffset;

    // staging buffer
    size_t         szStageByteSize;
    VkBuffer       tStagingBuffer;
    VkDeviceMemory tStagingMemory;
    void*          pStageMapping; // persistent mapping for staging buffer

    // drawing

    // committed buffers
    pl3DBufferReturn* sbReturnedBuffers;
    pl3DBufferReturn* sbReturnedBuffersTemp;
    uint32_t          uBufferDeletionQueueSize;

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
static VkFilter             pl__vulkan_filter (plFilter tFilter);
static VkSamplerAddressMode pl__vulkan_wrap   (plWrapMode tWrap);
static VkCompareOp          pl__vulkan_compare(plCompareMode tCompare);
static VkFormat             pl__vulkan_format (plFormat tFormat);
static plFormat             pl__pilotlight_format(VkFormat tFormat);

// 3D drawing helpers
static void                     pl__grow_vulkan_3d_vertex_buffer(plGraphics* ptGraphics, uint32_t uVtxBufSzNeeded, pl3DVulkanBufferInfo* ptBufferInfo);
static void                     pl__grow_vulkan_3d_index_buffer(plGraphics* ptGraphics, uint32_t uIdxBufSzNeeded, pl3DVulkanBufferInfo* ptBufferInfo);
static pl3DVulkanPipelineEntry* pl__get_3d_pipelines(plGraphics* ptGfx, VkRenderPass tRenderPass, VkSampleCountFlagBits tMSAASampleCount, pl3DDrawFlags tFlags);
static void                     pl__grow_vulkan_3d_vertex_buffer(plGraphics* ptGfx, uint32_t uVtxBufSzNeeded, pl3DVulkanBufferInfo* ptBufferInfo);
static void                     pl__grow_vulkan_3d_index_buffer(plGraphics* ptGfx, uint32_t uIdxBufSzNeeded, pl3DVulkanBufferInfo* ptBufferInfo);

static plFrameContext*       pl__get_frame_resources(plGraphics* ptGraphics);
static VkDeviceMemory        pl__allocate_dedicated(plDevice* ptDevice, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName);
static VkSampleCountFlagBits pl__get_max_sample_count(plDevice* ptDevice);
static VkFormat              pl__find_supported_format(plDevice* ptDevice, VkFormatFeatureFlags tFlags, const VkFormat* ptFormats, uint32_t uFormatCount);
static VkFormat              pl__find_depth_format(plDevice* ptDevice);
static VkFormat              pl__find_depth_stencil_format(plDevice* ptDevice);
static bool                  pl__format_has_stencil(VkFormat tFormat);
static void                  pl__transition_image_layout(VkCommandBuffer tCommandBuffer, VkImage tImage, VkImageLayout tOldLayout, VkImageLayout tNewLayout, VkImageSubresourceRange tSubresourceRange, VkPipelineStageFlags tSrcStageMask, VkPipelineStageFlags tDstStageMask);
static void                  pl__create_swapchain(plGraphics* ptGraphics, uint32_t uWidth, uint32_t uHeight, plVulkanSwapchain* ptSwapchainOut);
static uint32_t              pl__find_memory_type_(VkPhysicalDeviceMemoryProperties tMemProps, uint32_t uTypeFilter, VkMemoryPropertyFlags tProperties);
static void                  pl__transfer_data_to_buffer(plDevice* ptDevice, VkBuffer tDest, size_t szSize, const void* pData);
static void                  pl__transfer_data_to_image(plDevice* ptDevice, plTexture* ptDest, size_t szDataSize, const void* pData);

static VKAPI_ATTR VkBool32 VKAPI_CALL pl__debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT tMsgSeverity, VkDebugUtilsMessageTypeFlagsEXT tMsgType, const VkDebugUtilsMessengerCallbackDataEXT* ptCallbackData, void* pUserData);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

static void
pl__submit_3d_drawlist(plDrawList3D* ptDrawlist, float fWidth, float fHeight, const plMat4* ptMVP, pl3DDrawFlags tFlags)
{
    plGraphics* ptGfx = ptDrawlist->ptGraphics;
    plVulkanGraphics* ptVulkanGfx = ptGfx->_pInternalData;
    plVulkanDevice* ptVulkanDevice = ptGfx->tDevice._pInternalData;

    pl3DVulkanPipelineEntry* tPipelineEntry = pl__get_3d_pipelines(ptGfx, ptVulkanGfx->tRenderPass, ptVulkanGfx->tSwapchain.tMsaaSamples, tFlags);
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
        const uint32_t uAvailableVertexBufferSpace = ptBufferInfo->uVertexByteSize - ptBufferInfo->uVertexBufferOffset;

        // grow buffer if not enough room
        if(uVtxBufSzNeeded >= uAvailableVertexBufferSpace)
            pl__grow_vulkan_3d_vertex_buffer(ptGfx, uVtxBufSzNeeded * 2, ptBufferInfo);

        // vertex GPU data transfer
        unsigned char* pucMappedVertexBufferLocation = ptBufferInfo->ucVertexBufferMap;
        memcpy(&pucMappedVertexBufferLocation[ptBufferInfo->uVertexBufferOffset], ptDrawlist->sbtSolidVertexBuffer, sizeof(plDrawVertex3DSolid) * pl_sb_size(ptDrawlist->sbtSolidVertexBuffer));

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~index buffer prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // ensure gpu index buffer size is adequate
        const uint32_t uIdxBufSzNeeded = sizeof(uint32_t) * pl_sb_size(ptDrawlist->sbtSolidIndexBuffer);

        // space left in index buffer
        const uint32_t uAvailableIndexBufferSpace = ptBufferInfo->uIndexByteSize - ptBufferInfo->uIndexBufferOffset;

        if(uIdxBufSzNeeded >= uAvailableIndexBufferSpace)
            pl__grow_vulkan_3d_index_buffer(ptGfx, uIdxBufSzNeeded * 2, ptBufferInfo);

        // index GPU data transfer
        unsigned char* pucMappedIndexBufferLocation = ptBufferInfo->ucIndexBufferMap;
        memcpy(&pucMappedIndexBufferLocation[ptBufferInfo->uIndexBufferOffset], ptDrawlist->sbtSolidIndexBuffer, sizeof(uint32_t) * pl_sb_size(ptDrawlist->sbtSolidIndexBuffer));
        
        const VkMappedMemoryRange aRange[2] = {
            {
                .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                .memory = ptBufferInfo->tVertexMemory,
                .size = VK_WHOLE_SIZE
            },
            {
                .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                .memory = ptBufferInfo->tIndexMemory,
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
        const uint32_t uAvailableVertexBufferSpace = ptBufferInfo->uVertexByteSize - ptBufferInfo->uVertexBufferOffset;

        // grow buffer if not enough room
        if(uVtxBufSzNeeded >= uAvailableVertexBufferSpace)
            pl__grow_vulkan_3d_vertex_buffer(ptGfx, uVtxBufSzNeeded * 2, ptBufferInfo);

        // vertex GPU data transfer
        unsigned char* pucMappedVertexBufferLocation = ptBufferInfo->ucVertexBufferMap;
        memcpy(&pucMappedVertexBufferLocation[ptBufferInfo->uVertexBufferOffset], ptDrawlist->sbtLineVertexBuffer, sizeof(plDrawVertex3DLine) * pl_sb_size(ptDrawlist->sbtLineVertexBuffer));

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~index buffer prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // ensure gpu index buffer size is adequate
        const uint32_t uIdxBufSzNeeded = sizeof(uint32_t) * pl_sb_size(ptDrawlist->sbtLineIndexBuffer);

        // space left in index buffer
        const uint32_t uAvailableIndexBufferSpace = ptBufferInfo->uIndexByteSize - ptBufferInfo->uIndexBufferOffset;

        if(uIdxBufSzNeeded >= uAvailableIndexBufferSpace)
            pl__grow_vulkan_3d_index_buffer(ptGfx, uIdxBufSzNeeded * 2, ptBufferInfo);

        // index GPU data transfer
        unsigned char* pucMappedIndexBufferLocation = ptBufferInfo->ucIndexBufferMap;
        memcpy(&pucMappedIndexBufferLocation[ptBufferInfo->uIndexBufferOffset], ptDrawlist->sbtLineIndexBuffer, sizeof(uint32_t) * pl_sb_size(ptDrawlist->sbtLineIndexBuffer));
        
        const VkMappedMemoryRange aRange[2] = {
            {
                .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                .memory = ptBufferInfo->tVertexMemory,
                .size = VK_WHOLE_SIZE
            },
            {
                .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                .memory = ptBufferInfo->tIndexMemory,
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

static plBuffer
pl_create_buffer(plDevice* ptDevice, const plBufferDescription* ptDesc)
{
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptVulkanDevice->ptGraphics->_pInternalData;

    plBuffer tBuffer = {
        .tDescription = *ptDesc,
        .uHandle      = pl_sb_size(ptVulkanGraphics->sbtBuffers)
    };

    if(ptDesc->tMemory == PL_MEMORY_GPU_CPU)
    {
        plVulkanBuffer tVulkanBuffer = {0};
        VkBufferUsageFlagBits tBufferUsageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        if(ptDesc->tUsage == PL_BUFFER_USAGE_VERTEX)
            tBufferUsageFlags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        if(ptDesc->tUsage == PL_BUFFER_USAGE_INDEX)
            tBufferUsageFlags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if(ptDesc->tUsage == PL_BUFFER_USAGE_STORAGE)
            tBufferUsageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        if(ptDesc->tUsage == PL_BUFFER_USAGE_UNIFORM)
            tBufferUsageFlags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

        const VkBufferCreateInfo tBufferInfo = {
            .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size        = ptDesc->uByteSize,
            .usage       = tBufferUsageFlags,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        PL_VULKAN(vkCreateBuffer(ptVulkanDevice->tLogicalDevice, &tBufferInfo, NULL, &tVulkanBuffer.tBuffer));

        VkMemoryRequirements tMemRequirements = {0};
        vkGetBufferMemoryRequirements(ptVulkanDevice->tLogicalDevice, tVulkanBuffer.tBuffer, &tMemRequirements);

        const VkMemoryAllocateInfo tAllocInfo = {
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize  = tMemRequirements.size,
            .memoryTypeIndex = pl__find_memory_type_(ptVulkanDevice->tMemProps, tMemRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        };

        PL_VULKAN(vkAllocateMemory(ptVulkanDevice->tLogicalDevice, &tAllocInfo, NULL, &tVulkanBuffer.tMemory));
        PL_VULKAN(vkBindBufferMemory(ptVulkanDevice->tLogicalDevice, tVulkanBuffer.tBuffer, tVulkanBuffer.tMemory, 0));

        PL_VULKAN(vkMapMemory(ptVulkanDevice->tLogicalDevice, tVulkanBuffer.tMemory, 0, ptDesc->uByteSize, 0, (void**)&tBuffer.pcData));
        memset(tBuffer.pcData, 0, tMemRequirements.size);
        if(ptDesc->puInitialData)
            memcpy(tBuffer.pcData, ptDesc->puInitialData, ptDesc->uInitialDataByteSize);
        tVulkanBuffer.pcData = tBuffer.pcData;
        pl_sb_push(ptVulkanGraphics->sbtBuffers, tVulkanBuffer);
    }
    else if(ptDesc->tMemory == PL_MEMORY_GPU)
    {
        // todo: finish

        // pl__transfer_data_to_buffer

        plVulkanBuffer tVulkanBuffer = {0};
        VkBufferUsageFlagBits tBufferUsageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        if(ptDesc->tUsage == PL_BUFFER_USAGE_VERTEX)
            tBufferUsageFlags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        if(ptDesc->tUsage == PL_BUFFER_USAGE_INDEX)
            tBufferUsageFlags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if(ptDesc->tUsage == PL_BUFFER_USAGE_STORAGE)
            tBufferUsageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        if(ptDesc->tUsage == PL_BUFFER_USAGE_UNIFORM)
            tBufferUsageFlags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

        const VkBufferCreateInfo tBufferInfo = {
            .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size        = ptDesc->uByteSize,
            .usage       = tBufferUsageFlags,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        PL_VULKAN(vkCreateBuffer(ptVulkanDevice->tLogicalDevice, &tBufferInfo, NULL, &tVulkanBuffer.tBuffer));

        VkMemoryRequirements tMemRequirements = {0};
        vkGetBufferMemoryRequirements(ptVulkanDevice->tLogicalDevice, tVulkanBuffer.tBuffer, &tMemRequirements);

        const VkMemoryAllocateInfo tAllocInfo = {
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize  = tMemRequirements.size,
            .memoryTypeIndex = pl__find_memory_type_(ptVulkanDevice->tMemProps, tMemRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        };

        PL_VULKAN(vkAllocateMemory(ptVulkanDevice->tLogicalDevice, &tAllocInfo, NULL, &tVulkanBuffer.tMemory));
        PL_VULKAN(vkBindBufferMemory(ptVulkanDevice->tLogicalDevice, tVulkanBuffer.tBuffer, tVulkanBuffer.tMemory, 0));

        if(ptDesc->puInitialData)
            pl__transfer_data_to_buffer(ptDevice, tVulkanBuffer.tBuffer, ptDesc->uInitialDataByteSize, ptDesc->puInitialData);

        pl_sb_push(ptVulkanGraphics->sbtBuffers, tVulkanBuffer);
    }

    return tBuffer;
}

static plDynamicBinding
pl_allocate_dynamic_data(plDevice* ptDevice, size_t szSize)
{
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptVulkanDevice->ptGraphics->_pInternalData;

    plVulkanBuffer* ptBuffer = &ptVulkanGraphics->sbtBuffers[ptVulkanGraphics->tDynamicBuffers[ptVulkanDevice->ptGraphics->uCurrentFrameIndex].uHandle];

    plDynamicBinding tDynamicBinding = {
        .uBufferHandle = ptVulkanDevice->ptGraphics->uCurrentFrameIndex,
        .uByteOffset   = ptVulkanGraphics->uDynamicByteOffset,
        .pcData        = &ptBuffer->pcData[ptVulkanGraphics->uDynamicByteOffset]
    };
    ptVulkanGraphics->uDynamicByteOffset = (uint32_t)pl_align_up((size_t)ptVulkanGraphics->uDynamicByteOffset + szSize, ptVulkanDevice->tDeviceProps.limits.minUniformBufferOffsetAlignment);
    return tDynamicBinding;
}

static plTexture
pl_create_texture(plDevice* ptDevice, plTextureDesc tDesc, size_t szSize, const void* pData, const char* pcName)
{
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptVulkanDevice->ptGraphics->_pInternalData;

    if(tDesc.uMips == 0)
    {
        tDesc.uMips = (uint32_t)floorf(log2f((float)pl_maxi((int)tDesc.tDimensions.x, (int)tDesc.tDimensions.y))) + 1u;
    }

    plTexture tTexture = {
        .tDesc = tDesc,
        .uHandle = pl_sb_size(ptVulkanGraphics->sbtTextures)
    };

    plVulkanTexture tVulkanTexture = {0};

    const VkImageViewType tImageViewType = tDesc.uLayers == 6 ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
    PL_ASSERT((tDesc.uLayers == 1 || tDesc.uLayers == 6) && "unsupported layer count");

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
        .usage         = VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .flags         = tImageViewType == VK_IMAGE_VIEW_TYPE_CUBE ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0
    };

    if(pData)
        tImageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    PL_VULKAN(vkCreateImage(ptVulkanDevice->tLogicalDevice, &tImageInfo, NULL, &tVulkanTexture.tImage));

    // get memory requirements
    VkMemoryRequirements tMemoryRequirements = {0};
    vkGetImageMemoryRequirements(ptVulkanDevice->tLogicalDevice, tVulkanTexture.tImage, &tMemoryRequirements);

    // allocate memory
    tVulkanTexture.tMemory = pl__allocate_dedicated(ptDevice, tMemoryRequirements.memoryTypeBits, tMemoryRequirements.size, tMemoryRequirements.alignment, "text");

    PL_VULKAN(vkBindImageMemory(ptVulkanDevice->tLogicalDevice, tVulkanTexture.tImage, tVulkanTexture.tMemory, 0));

    pl_sb_push(ptVulkanGraphics->sbtTextures, tVulkanTexture);

    // upload data
    if(pData)
        pl__transfer_data_to_image(ptDevice, &tTexture, szSize, pData);

    // VkImageAspectFlags tImageAspectFlags = tDesc.tUsage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    VkImageAspectFlags tImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

    if(pl__format_has_stencil(tDesc.tFormat))
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
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = tDesc.uLayers,
        .aspectMask     = tImageAspectFlags
    };

    if(pData)
        pl__transition_image_layout(tCommandBuffer, tVulkanTexture.tImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, tRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    // else if (tDesc.tUsage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
    //     ptDeviceApi->pl__transition_image_layout(tCommandBuffer, tTexture.tImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, tRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    // else if(tDesc.tUsage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
    //     ptDeviceApi->pl__transition_image_layout(tCommandBuffer, tTexture.tImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, tRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    // else if(tDesc.tUsage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
    //     ptDeviceApi->pl__transition_image_layout(tCommandBuffer, tTexture.tImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, tRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    // ptDeviceApi->submit_command_buffer(ptDevice, ptDevice->tCmdPool, tCommandBuffer);

    PL_VULKAN(vkEndCommandBuffer(tCommandBuffer));
    const VkSubmitInfo tSubmitInfo = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1u,
        .pCommandBuffers    = &tCommandBuffer,
    };

    PL_VULKAN(vkQueueSubmit(ptVulkanDevice->tGraphicsQueue, 1, &tSubmitInfo, VK_NULL_HANDLE));
    PL_VULKAN(vkDeviceWaitIdle(ptVulkanDevice->tLogicalDevice));
    vkFreeCommandBuffers(ptVulkanDevice->tLogicalDevice, ptVulkanDevice->tCmdPool, 1, &tCommandBuffer);

    return tTexture;
}

static plTextureView
pl_create_texture_view(plDevice* ptDevice, const plTextureViewDesc* ptViewDesc, const plSampler* ptSampler, uint32_t uTextureHandle, const char* pcName)
{
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptVulkanDevice->ptGraphics->_pInternalData;

    plVulkanTexture* ptVulkanTexture = &ptVulkanGraphics->sbtTextures[uTextureHandle];

    plTextureView tTextureView = {
        .tSampler         = *ptSampler,
        .tTextureViewDesc = *ptViewDesc,
        .uTextureHandle   = uTextureHandle,
        ._uSamplerHandle  = pl_sb_size(ptVulkanGraphics->sbtSamplers)
    };

    plVulkanSampler tVulkanSampler = {0};

    if(ptViewDesc->uMips == 0)
        tTextureView.tTextureViewDesc.uMips = ptViewDesc->uMips;
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~create view~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const VkImageViewType tImageViewType = ptViewDesc->uLayerCount == 6 ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;    
    PL_ASSERT((ptViewDesc->uLayerCount == 1 || ptViewDesc->uLayerCount == 6) && "unsupported layer count");

    // VkImageAspectFlags tImageAspectFlags = ptTexture->tDesc.tUsage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    VkImageAspectFlags tImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

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
        .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .mipLodBias              = ptSampler->fMipBias,
        .minLod                  = ptSampler->fMinMip,
        .maxLod                  = ptSampler->fMaxMip,
    };

    // .compareOp               = VK_COMPARE_OP_ALWAYS,
    tSamplerInfo.minFilter    = tSamplerInfo.magFilter;
    tSamplerInfo.addressModeW = tSamplerInfo.addressModeU;

    PL_VULKAN(vkCreateSampler(ptVulkanDevice->tLogicalDevice, &tSamplerInfo, NULL, &tVulkanSampler.tSampler));

    pl_sb_push(ptVulkanGraphics->sbtSamplers, tVulkanSampler);

    return tTextureView;
}

static void
pl_create_bind_group_layout(plDevice* ptDevice, plBindGroupLayout* ptLayout)
{
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptVulkanDevice->ptGraphics->_pInternalData;

    plVulkanBindGroupLayout tVulkanBindGroupLayout = {0};

    ptLayout->uHandle = pl_sb_size(ptVulkanGraphics->sbtBindGroupLayouts);

    VkDescriptorSetLayoutBinding* sbtDescriptorSetLayoutBindings = NULL;
    for(uint32_t i = 0; i < ptLayout->uBufferCount; i++)
    {
        VkDescriptorSetLayoutBinding tBinding =  {
            .binding         = ptLayout->aBuffers[i].uSlot,
            .descriptorType  = ptLayout->aBuffers[i].tType == PL_BUFFER_BINDING_TYPE_STORAGE ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = NULL
        };
        pl_sb_push(sbtDescriptorSetLayoutBindings, tBinding);
    }

    for(uint32_t i = 0 ; i < ptLayout->uTextureCount; i++)
    {
        VkDescriptorSetLayoutBinding tBinding = {
            .binding            = ptLayout->aTextures[i].uSlot,
            .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount    = 1,
            .stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL
        };
        pl_sb_push(sbtDescriptorSetLayoutBindings, tBinding);
    }

    // create descriptor set layout
    const VkDescriptorSetLayoutCreateInfo tDescriptorSetLayoutInfo = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = pl_sb_size(sbtDescriptorSetLayoutBindings),
        .pBindings    = sbtDescriptorSetLayoutBindings,
    };
    PL_VULKAN(vkCreateDescriptorSetLayout(ptVulkanDevice->tLogicalDevice, &tDescriptorSetLayoutInfo, NULL, &tVulkanBindGroupLayout.tDescriptorSetLayout));

    pl_sb_free(sbtDescriptorSetLayoutBindings);

    pl_sb_push(ptVulkanGraphics->sbtBindGroupLayouts, tVulkanBindGroupLayout);
}

static plBindGroup
pl_create_bind_group(plDevice* ptDevice, plBindGroupLayout* ptLayout)
{
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptVulkanDevice->ptGraphics->_pInternalData;

    plBindGroup tBindGroup = {
        .tLayout = *ptLayout,
        .uHandle = pl_sb_size(ptVulkanGraphics->sbtBindGroups)
    };

    VkDescriptorSetLayoutBinding* sbtDescriptorSetLayoutBindings = NULL;
    for(uint32_t i = 0; i < ptLayout->uBufferCount; i++)
    {
        VkDescriptorSetLayoutBinding tBinding =  {
            .binding         = ptLayout->aBuffers[i].uSlot,
            .descriptorType  = ptLayout->aBuffers[i].tType == PL_BUFFER_BINDING_TYPE_STORAGE ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = NULL
        };
        pl_sb_push(sbtDescriptorSetLayoutBindings, tBinding);
    }

    for(uint32_t i = 0 ; i < ptLayout->uTextureCount; i++)
    {
        VkDescriptorSetLayoutBinding tBinding = {
            .binding            = ptLayout->aTextures[i].uSlot,
            .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount    = 1,
            .stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL
        };
        pl_sb_push(sbtDescriptorSetLayoutBindings, tBinding);
    }

    // create descriptor set layout
    VkDescriptorSetLayout tDescriptorSetLayout = VK_NULL_HANDLE;
    const VkDescriptorSetLayoutCreateInfo tDescriptorSetLayoutInfo = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = pl_sb_size(sbtDescriptorSetLayoutBindings),
        .pBindings    = sbtDescriptorSetLayoutBindings,
    };
    PL_VULKAN(vkCreateDescriptorSetLayout(ptVulkanDevice->tLogicalDevice, &tDescriptorSetLayoutInfo, NULL, &tDescriptorSetLayout));

    pl_sb_free(sbtDescriptorSetLayoutBindings);

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

    pl_sb_push(ptVulkanGraphics->sbtBindGroups, tVulkanBindGroup);
    return tBindGroup;
}

static void
pl_update_bind_group(plDevice* ptDevice, plBindGroup* ptGroup, uint32_t uBufferCount, plBuffer* atBuffers, size_t* aszBufferRanges, uint32_t uTextureViewCount, plTextureView* atTextureViews)
{
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptVulkanDevice->ptGraphics->_pInternalData;

    plVulkanBindGroup* ptVulkanBindGroup = &ptVulkanGraphics->sbtBindGroups[ptGroup->uHandle];

    VkWriteDescriptorSet* sbtWrites = NULL;
    VkDescriptorBufferInfo* sbtBufferDescInfos = NULL;
    VkDescriptorImageInfo* sbtImageDescInfos = NULL;
    pl_sb_resize(sbtWrites, uBufferCount + uTextureViewCount);
    pl_sb_resize(sbtBufferDescInfos, uBufferCount);
    pl_sb_resize(sbtImageDescInfos, uTextureViewCount);

    static const VkDescriptorType atDescriptorTypeLUT[] =
    {
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    };

    uint32_t uCurrentWrite = 0;
    for(uint32_t i = 0 ; i < uBufferCount; i++)
    {

        const plVulkanBuffer* ptVulkanBuffer = &ptVulkanGraphics->sbtBuffers[atBuffers[i].uHandle];

        sbtBufferDescInfos[i].buffer = ptVulkanBuffer->tBuffer;
        sbtBufferDescInfos[i].offset = 0;
        sbtBufferDescInfos[i].range  = aszBufferRanges[i];

        sbtWrites[uCurrentWrite].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sbtWrites[uCurrentWrite].dstBinding      = ptGroup->tLayout.aBuffers[i].uSlot;
        sbtWrites[uCurrentWrite].dstArrayElement = 0;
        sbtWrites[uCurrentWrite].descriptorType  = atDescriptorTypeLUT[ptGroup->tLayout.aBuffers[i].tType - 1];
        sbtWrites[uCurrentWrite].descriptorCount = 1;
        sbtWrites[uCurrentWrite].dstSet          = ptVulkanBindGroup->tDescriptorSet;
        sbtWrites[uCurrentWrite].pBufferInfo     = &sbtBufferDescInfos[i];
        sbtWrites[uCurrentWrite].pNext           = NULL;
        uCurrentWrite++;
    }

    for(uint32_t i = 0 ; i < uTextureViewCount; i++)
    {

        sbtImageDescInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        sbtImageDescInfos[i].imageView   = ptVulkanGraphics->sbtSamplers[atTextureViews[i]._uSamplerHandle].tImageView;
        sbtImageDescInfos[i].sampler     = ptVulkanGraphics->sbtSamplers[atTextureViews[i]._uSamplerHandle].tSampler;
        
        sbtWrites[uCurrentWrite].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sbtWrites[uCurrentWrite].dstBinding      = ptGroup->tLayout.aTextures[i].uSlot;
        sbtWrites[uCurrentWrite].dstArrayElement = 0;
        sbtWrites[uCurrentWrite].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sbtWrites[uCurrentWrite].descriptorCount = 1;
        sbtWrites[uCurrentWrite].dstSet          = ptVulkanBindGroup->tDescriptorSet;
        sbtWrites[uCurrentWrite].pImageInfo      = &sbtImageDescInfos[i];
        sbtWrites[uCurrentWrite].pNext           = NULL;
        uCurrentWrite++;
    }
    vkUpdateDescriptorSets(ptVulkanDevice->tLogicalDevice, uCurrentWrite, sbtWrites, 0, NULL);
    pl_sb_free(sbtWrites);
    pl_sb_free(sbtBufferDescInfos);
    pl_sb_free(sbtImageDescInfos);

}

static plShader
pl_create_shader(plGraphics* ptGraphics, plShaderDescription* ptDescription)
{
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    plShader tShader = {
        .uHandle = pl_sb_size(ptVulkanGfx->sbtShaders),
        .tDescription = *ptDescription
    };

    plVulkanShader tVulkanShader = {0};

    const VkSpecializationMapEntry tSpecializationEntries[] = 
    {
        {
            .constantID = 0,
            .offset     = 0,
            .size       = sizeof(int)
        },
        {
            .constantID = 1,
            .offset     = sizeof(int),
            .size       = sizeof(int)
        },
        {
            .constantID = 2,
            .offset     = sizeof(int) * 2,
            .size       = sizeof(int)
        },

    };

        int aiData[3] = {
            (int)ptDescription->tGraphicsState.ulVertexStreamMask,
            0,
            (int)ptDescription->tGraphicsState.ulShaderTextureFlags
        };

        int iFlagCopy = (int)ptDescription->tGraphicsState.ulVertexStreamMask;
        while(iFlagCopy)
        {
            aiData[1] += iFlagCopy & 1;
            iFlagCopy >>= 1;
        }

    VkSpecializationInfo tSpecializationInfo = {
        .mapEntryCount = 3,
        .pMapEntries   = tSpecializationEntries,
        .dataSize      = sizeof(int) * 3,
        .pData         = aiData
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

    VkDescriptorSetLayout atDescriptorSetLayouts[4] = {0};
    for(uint32_t i = 0; i < ptDescription->uBindGroupLayoutCount; i++)
    {
        pl_create_bind_group_layout(&ptGraphics->tDevice, &ptDescription->atBindGroupLayouts[i]);
        atDescriptorSetLayouts[i] = ptVulkanGfx->sbtBindGroupLayouts[ptDescription->atBindGroupLayouts[i].uHandle].tDescriptorSetLayout;
    }
    atDescriptorSetLayouts[ptDescription->uBindGroupLayoutCount]  = ptVulkanGfx->tDynamicDescriptorSetLayout;

    VkPipelineLayoutCreateInfo tPipelineLayoutInfo = {
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = ptDescription->uBindGroupLayoutCount + 1,
        .pSetLayouts    = atDescriptorSetLayouts
    };
    PL_VULKAN(vkCreatePipelineLayout(ptVulkanDevice->tLogicalDevice, &tPipelineLayoutInfo, NULL, &tVulkanShader.tPipelineLayout));

    uint32_t uVertShaderSize0 = 0u;
    uint32_t uPixelShaderSize0 = 0u;

    gptFile->read(ptDescription->pcVertexShader, &uVertShaderSize0, NULL, "rb");
    gptFile->read(ptDescription->pcPixelShader, &uPixelShaderSize0, NULL, "rb");

    char* vertexShaderCode = PL_ALLOC(uVertShaderSize0);
    char* pixelShaderCode  = PL_ALLOC(uPixelShaderSize0);

    gptFile->read(ptDescription->pcVertexShader, &uVertShaderSize0, vertexShaderCode, "rb");
    gptFile->read(ptDescription->pcPixelShader, &uPixelShaderSize0, pixelShaderCode, "rb");

    VkShaderModuleCreateInfo tVertexShaderCreateInfo = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = uVertShaderSize0,
        .pCode    = (const uint32_t*)(vertexShaderCode)
    };

    PL_VULKAN(vkCreateShaderModule(ptVulkanDevice->tLogicalDevice, &tVertexShaderCreateInfo, NULL, &tVulkanShader.tVertexShaderModule));

    VkShaderModuleCreateInfo tPixelShaderCreateInfo = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = uPixelShaderSize0,
        .pCode    = (const uint32_t*)(pixelShaderCode),
    };
    PL_VULKAN(vkCreateShaderModule(ptVulkanDevice->tLogicalDevice, &tPixelShaderCreateInfo, NULL, &tVulkanShader.tPixelShaderModule));

    PL_FREE(vertexShaderCode);
    PL_FREE(pixelShaderCode);

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


    //---------------------------------------------------------------------
    // vertex shader stage
    //---------------------------------------------------------------------

    VkPipelineShaderStageCreateInfo tVertShaderStageInfo = {
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage  = VK_SHADER_STAGE_VERTEX_BIT,
        .module = tVulkanShader.tVertexShaderModule,
        .pName  = "main",
        .pSpecializationInfo = &tSpecializationInfo
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
        .polygonMode             = VK_POLYGON_MODE_FILL,
        .lineWidth               = 1.0f,
        .cullMode                = VK_CULL_MODE_NONE,
        .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable         = VK_FALSE
    };

    //---------------------------------------------------------------------
    // fragment shader stage
    //---------------------------------------------------------------------

    VkPipelineShaderStageCreateInfo tFragShaderStageInfo = {
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = tVulkanShader.tPixelShaderModule,
        .pName  = "main",
        .pSpecializationInfo = &tSpecializationInfo
    };

    VkPipelineDepthStencilStateCreateInfo tDepthStencil = {
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable       = VK_TRUE,
        .depthWriteEnable      = VK_TRUE,
        .depthCompareOp        = VK_COMPARE_OP_LESS_OR_EQUAL,
        .depthBoundsTestEnable = VK_FALSE,
        .minDepthBounds        = 0.0f, // Optional,
        .maxDepthBounds        = 1.0f, // Optional,
        .stencilTestEnable     = VK_FALSE,
    };

    //---------------------------------------------------------------------
    // color blending stage
    //---------------------------------------------------------------------

    VkPipelineColorBlendAttachmentState tColorBlendAttachment = {
        .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable         = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp        = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp        = VK_BLEND_OP_ADD,
    };

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
        .sampleShadingEnable  = VK_FALSE,
        .rasterizationSamples = ptVulkanGfx->tSwapchain.tMsaaSamples,
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
        .renderPass          = ptVulkanGfx->tRenderPass,
        .subpass             = 0,
        .basePipelineHandle  = VK_NULL_HANDLE,
        .pDepthStencilState  = &tDepthStencil
    };


    PL_VULKAN(vkCreateGraphicsPipelines(ptVulkanDevice->tLogicalDevice, VK_NULL_HANDLE, 1, &tPipelineInfo, NULL, &tVulkanShader.tPipeline));

    // no longer need these
    vkDestroyShaderModule(ptVulkanDevice->tLogicalDevice, tVulkanShader.tVertexShaderModule, NULL);
    vkDestroyShaderModule(ptVulkanDevice->tLogicalDevice, tVulkanShader.tPixelShaderModule, NULL);
    tVulkanShader.tVertexShaderModule = VK_NULL_HANDLE;
    tVulkanShader.tPixelShaderModule = VK_NULL_HANDLE;

    pl_sb_push(ptVulkanGfx->sbtShaders, tVulkanShader);

    return tShader;
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

    VkRenderPassBeginInfo renderPassInfo = {0};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = ptVulkanGfx->tRenderPass;
    renderPassInfo.framebuffer = ptVulkanGfx->tSwapchain.sbtFrameBuffers[ptVulkanGfx->tSwapchain.uCurrentImageIndex];
    renderPassInfo.renderArea.extent = ptVulkanGfx->tSwapchain.tExtent;

    VkClearValue clearValues[2];
    clearValues[0].color.float32[0] = 0.0f;
    clearValues[0].color.float32[1] = 0.0f;
    clearValues[0].color.float32[2] = 0.0f;
    clearValues[0].color.float32[3] = 1.0f;
    clearValues[1].depthStencil.depth = 0.0f;
    renderPassInfo.clearValueCount = 2;
    renderPassInfo.pClearValues = clearValues;

    VkRect2D scissor = {0};
    scissor.extent = ptVulkanGfx->tSwapchain.tExtent;

    VkViewport tViewport = {0};
    tViewport.x = 0.0f;
    tViewport.y = 0.0f;
    tViewport.width = (float)ptVulkanGfx->tSwapchain.tExtent.width;
    tViewport.height = (float)ptVulkanGfx->tSwapchain.tExtent.height;

    vkCmdSetViewport(ptCurrentFrame->tCmdBuf, 0, 1, &tViewport);
    vkCmdSetScissor(ptCurrentFrame->tCmdBuf, 0, 1, &scissor);  

    vkCmdBeginRenderPass(ptCurrentFrame->tCmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    pl_new_draw_frame_vulkan();

}

static void
pl_end_recording(plGraphics* ptGraphics)
{
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptGraphics);

    vkCmdEndRenderPass(ptCurrentFrame->tCmdBuf);

    PL_VULKAN(vkEndCommandBuffer(ptCurrentFrame->tCmdBuf));
}

static void
pl_draw_list(plGraphics* ptGraphics, uint32_t uListCount, plDrawList* atLists)
{
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptGraphics);

    plIO* ptIOCtx = pl_get_io();
    for(uint32_t i = 0; i < uListCount; i++)
    {
        pl_submit_vulkan_drawlist(&atLists[i], ptIOCtx->afMainViewportSize[0], ptIOCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, ptGraphics->uCurrentFrameIndex);
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

    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;
    ptVulkanDevice->ptGraphics = ptGraphics;
    
    ptGraphics->uFramesInFlight = 2;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~create instance~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const bool bEnableValidation = true;

    static const char* pcKhronosValidationLayer = "VK_LAYER_KHRONOS_validation";

    const char** sbpcEnabledExtensions = NULL;
    pl_sb_push(sbpcEnabledExtensions, VK_KHR_SURFACE_EXTENSION_NAME);

    #ifdef _WIN32
        pl_sb_push(sbpcEnabledExtensions, VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
    #elif defined(__APPLE__)
        pl_sb_push(sbpcEnabledExtensions, "VK_EXT_metal_surface");
        pl_sb_push(sbpcEnabledExtensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    #else // linux
        pl_sb_push(sbpcEnabledExtensions, VK_KHR_XCB_SURFACE_EXTENSION_NAME);
    #endif

    if(bEnableValidation)
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
        .pNext                   = bEnableValidation ? (VkDebugUtilsMessengerCreateInfoEXT*)&tDebugCreateInfo : VK_NULL_HANDLE,
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
    pl_sb_free(sbpcMissingLayers);
    pl_sb_free(sbpcMissingExtensions);

    if(bEnableValidation)
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
    bool bDiscreteGPUFound = false;
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

        for(uint32_t j = 0; j < ptVulkanDevice->tMemProps.memoryHeapCount; j++)
        {
            if(ptVulkanDevice->tMemProps.memoryHeaps[j].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT && ptVulkanDevice->tMemProps.memoryHeaps[j].size > tMaxLocalMemorySize && !bDiscreteGPUFound)
            {
                    tMaxLocalMemorySize = ptVulkanDevice->tMemProps.memoryHeaps[j].size;
                iBestDvcIdx = i;
            }
        }

        if(ptVulkanDevice->tDeviceProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && !bDiscreteGPUFound)
        {
            iBestDvcIdx = i;
            bDiscreteGPUFound = true;
        }
    }

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
    VkExtensionProperties* ptExtensions = PL_ALLOC(uExtensionCount * sizeof(VkExtensionProperties));
    vkEnumerateDeviceExtensionProperties(ptVulkanDevice->tPhysicalDevice, NULL, &uExtensionCount, ptExtensions);

    for(uint32_t i = 0; i < uExtensionCount; i++)
    {
        if(pl_str_equal(ptExtensions[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) ptVulkanDevice->bSwapchainExtPresent = true; //-V522
        if(pl_str_equal(ptExtensions[i].extensionName, "VK_KHR_portability_subset"))     ptVulkanDevice->bPortabilitySubsetPresent = true; //-V522
    }

    PL_FREE(ptExtensions);

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

    const char** sbpcDeviceExts = NULL;
    if(ptVulkanDevice->bSwapchainExtPresent)      pl_sb_push(sbpcDeviceExts, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    if(ptVulkanDevice->bPortabilitySubsetPresent) pl_sb_push(sbpcDeviceExts, "VK_KHR_portability_subset");
    pl_sb_push(sbpcDeviceExts, VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
    VkDeviceCreateInfo tCreateDeviceInfo = {
        .sType                    = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount     = atQueueCreateInfos[0].queueFamilyIndex == atQueueCreateInfos[1].queueFamilyIndex ? 1 : 2,
        .pQueueCreateInfos        = atQueueCreateInfos,
        .pEnabledFeatures         = &ptVulkanDevice->tDeviceFeatures,
        .ppEnabledExtensionNames  = sbpcDeviceExts,
        .enabledLayerCount        = bEnableValidation ? 1 : 0,
        .ppEnabledLayerNames      = bEnableValidation ? &pcValidationLayers : NULL,
        .enabledExtensionCount    = pl_sb_size(sbpcDeviceExts)
    };
    PL_VULKAN(vkCreateDevice(ptVulkanDevice->tPhysicalDevice, &tCreateDeviceInfo, NULL, &ptVulkanDevice->tLogicalDevice));

    pl_sb_free(sbpcDeviceExts);

    // get device queues
    vkGetDeviceQueue(ptVulkanDevice->tLogicalDevice, ptVulkanDevice->iGraphicsQueueFamily, 0, &ptVulkanDevice->tGraphicsQueue);
    vkGetDeviceQueue(ptVulkanDevice->tLogicalDevice, ptVulkanDevice->iPresentQueueFamily, 0, &ptVulkanDevice->tPresentQueue);


    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~debug markers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	ptVulkanDevice->vkDebugMarkerSetObjectTag  = (PFN_vkDebugMarkerSetObjectTagEXT)vkGetDeviceProcAddr(ptVulkanDevice->tLogicalDevice, "vkDebugMarkerSetObjectTagEXT");
	ptVulkanDevice->vkDebugMarkerSetObjectName = (PFN_vkDebugMarkerSetObjectNameEXT)vkGetDeviceProcAddr(ptVulkanDevice->tLogicalDevice, "vkDebugMarkerSetObjectNameEXT");
	ptVulkanDevice->vkCmdDebugMarkerBegin      = (PFN_vkCmdDebugMarkerBeginEXT)vkGetDeviceProcAddr(ptVulkanDevice->tLogicalDevice, "vkCmdDebugMarkerBeginEXT");
	ptVulkanDevice->vkCmdDebugMarkerEnd        = (PFN_vkCmdDebugMarkerEndEXT)vkGetDeviceProcAddr(ptVulkanDevice->tLogicalDevice, "vkCmdDebugMarkerEndEXT");
	ptVulkanDevice->vkCmdDebugMarkerInsert     = (PFN_vkCmdDebugMarkerInsertEXT)vkGetDeviceProcAddr(ptVulkanDevice->tLogicalDevice, "vkCmdDebugMarkerInsertEXT");

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~command pool~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const VkCommandPoolCreateInfo tCommandPoolInfo = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = ptVulkanDevice->iGraphicsQueueFamily,
        .flags            = 0
    };
    PL_VULKAN(vkCreateCommandPool(ptVulkanDevice->tLogicalDevice, &tCommandPoolInfo, NULL, &ptVulkanDevice->tCmdPool));

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~swapchain~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    ptVulkanGfx->tSwapchain.bVSync = true;
    pl__create_swapchain(ptGraphics, (uint32_t)ptIOCtx->afMainViewportSize[0], (uint32_t)ptIOCtx->afMainViewportSize[1], &ptVulkanGfx->tSwapchain);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~main renderpass~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const VkAttachmentDescription atAttachments[] = {

        // multisampled color attachment (render to this)
        {
            .format         = ptVulkanGfx->tSwapchain.tFormat,
            .samples        = ptVulkanGfx->tSwapchain.tMsaaSamples,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        },

        // depth attachment
        {
            .format         = pl__find_depth_stencil_format(&ptGraphics->tDevice),
            .samples        = ptVulkanGfx->tSwapchain.tMsaaSamples,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        },

        // color resolve attachment
        {
            .format         = ptVulkanGfx->tSwapchain.tFormat,
            .samples        = VK_SAMPLE_COUNT_1_BIT,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        },
    };

    const VkSubpassDependency tSubpassDependencies[] = {

        // color attachment
        { 
            .srcSubpass      = VK_SUBPASS_EXTERNAL,
            .dstSubpass      = 0,
            .srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask   = 0,
            .dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
            .dependencyFlags = 0
        },
        // depth attachment
        {
            .srcSubpass      = VK_SUBPASS_EXTERNAL,
            .dstSubpass      = 0,
            .srcStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .srcAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
            .dependencyFlags = 0
        }
    };

    const VkAttachmentReference atAttachmentReferences[] = {
        {
            .attachment = 0,
            .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        },
        {
            .attachment = 1,
            .layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        },
        {
            .attachment = 2,
            .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL     
        }
    };

    const VkSubpassDescription tSubpass = {
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount    = 1,
        .pColorAttachments       = &atAttachmentReferences[0],
        .pDepthStencilAttachment = &atAttachmentReferences[1],
        .pResolveAttachments     = &atAttachmentReferences[2]
    };

    const VkRenderPassCreateInfo tRenderPassInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 3,
        .pAttachments    = atAttachments,
        .subpassCount    = 1,
        .pSubpasses      = &tSubpass,
        .dependencyCount = 2,
        .pDependencies   = tSubpassDependencies
    };

    PL_VULKAN(vkCreateRenderPass(ptVulkanDevice->tLogicalDevice, &tRenderPassInfo, NULL, &ptVulkanGfx->tRenderPass));

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~frame buffer~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    pl_sb_resize(ptVulkanGfx->tSwapchain.sbtFrameBuffers, ptVulkanGfx->tSwapchain.uImageCount);
    for(uint32_t i = 0; i < ptVulkanGfx->tSwapchain.uImageCount; i++)
    {
        ptVulkanGfx->tSwapchain.sbtFrameBuffers[i] = VK_NULL_HANDLE;

        VkImageView atViewAttachments[] = {
            ptVulkanGfx->tSwapchain.tColorTextureView,
            ptVulkanGfx->tSwapchain.tDepthTextureView,
            ptVulkanGfx->tSwapchain.sbtImageViews[i]
        };

        VkFramebufferCreateInfo tFrameBufferInfo = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = ptVulkanGfx->tRenderPass,
            .attachmentCount = 3,
            .pAttachments    = atViewAttachments,
            .width           = ptVulkanGfx->tSwapchain.tExtent.width,
            .height          = ptVulkanGfx->tSwapchain.tExtent.height,
            .layers          = 1u,
        };
        PL_VULKAN(vkCreateFramebuffer(ptVulkanDevice->tLogicalDevice, &tFrameBufferInfo, NULL, &ptVulkanGfx->tSwapchain.sbtFrameBuffers[i]));
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~frame resources~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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

        PL_VULKAN(vkAllocateCommandBuffers(ptVulkanDevice->tLogicalDevice, &tAllocInfo, &tFrame.tCmdBuf));  
        ptVulkanGfx->sbFrames[i] = tFrame;
    }

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
    

    // setup drawing api
    const plVulkanInit tVulkanInit = {
        .tPhysicalDevice  = ptVulkanDevice->tPhysicalDevice,
        .tLogicalDevice   = ptVulkanDevice->tLogicalDevice,
        .uImageCount      = ptVulkanGfx->tSwapchain.uImageCount,
        .tRenderPass      = ptVulkanGfx->tRenderPass,
        .tMSAASampleCount = ptVulkanGfx->tSwapchain.tMsaaSamples,
        .uFramesInFlight  = ptGraphics->uFramesInFlight
    };
    pl_initialize_vulkan(&tVulkanInit);


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

    char* __glsl_shader_vert_3d_spv      = PL_ALLOC(uVertShaderSize0);
    char* __glsl_shader_vert_3d_line_spv = PL_ALLOC(uVertShaderSize1);
    char* __glsl_shader_frag_3d_spv      = PL_ALLOC(uPixelShaderSize0);

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

    PL_FREE(__glsl_shader_vert_3d_spv);
    PL_FREE(__glsl_shader_vert_3d_line_spv);
    PL_FREE(__glsl_shader_frag_3d_spv);

    {
        const plBufferDescription tStagingBufferDescription0 = {
            .pcDebugName          = "dynamic buffer 0",
            .tMemory              = PL_MEMORY_GPU_CPU,
            .tUsage               = PL_BUFFER_USAGE_UNIFORM,
            .uByteSize            = 134217728,
            .uInitialDataByteSize = 0,
            .puInitialData        = NULL
        };

        const plBufferDescription tStagingBufferDescription1 = {
            .pcDebugName          = "dynamic buffer 1",
            .tMemory              = PL_MEMORY_GPU_CPU,
            .tUsage               = PL_BUFFER_USAGE_UNIFORM,
            .uByteSize            = 134217728,
            .uInitialDataByteSize = 0,
            .puInitialData        = NULL
        };
        plBuffer tStagingBuffer0 = pl_create_buffer(&ptGraphics->tDevice, &tStagingBufferDescription0);
        plBuffer tStagingBuffer1 = pl_create_buffer(&ptGraphics->tDevice, &tStagingBufferDescription1);

        ptVulkanGfx->tDynamicBuffers[0].uHandle = tStagingBuffer0.uHandle;
        ptVulkanGfx->tDynamicBuffers[0].tBuffer = ptVulkanGfx->sbtBuffers[tStagingBuffer0.uHandle].tBuffer;
        ptVulkanGfx->tDynamicBuffers[0].tMemory = ptVulkanGfx->sbtBuffers[tStagingBuffer0.uHandle].tMemory;

        ptVulkanGfx->tDynamicBuffers[1].uHandle = tStagingBuffer1.uHandle;
        ptVulkanGfx->tDynamicBuffers[1].tBuffer = ptVulkanGfx->sbtBuffers[tStagingBuffer1.uHandle].tBuffer;
        ptVulkanGfx->tDynamicBuffers[1].tMemory = ptVulkanGfx->sbtBuffers[tStagingBuffer1.uHandle].tMemory;

        // allocate descriptor sets
        const VkDescriptorSetAllocateInfo tAllocInfo = {
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool     = ptVulkanGfx->tDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts        = &ptVulkanGfx->tDynamicDescriptorSetLayout
        };

        PL_VULKAN(vkAllocateDescriptorSets(ptVulkanDevice->tLogicalDevice, &tAllocInfo, &ptVulkanGfx->tDynamicBuffers[0].tDescriptorSet));
        PL_VULKAN(vkAllocateDescriptorSets(ptVulkanDevice->tLogicalDevice, &tAllocInfo, &ptVulkanGfx->tDynamicBuffers[1].tDescriptorSet));


        VkDescriptorBufferInfo tDescriptorInfo0 = {
            .buffer = ptVulkanGfx->tDynamicBuffers[0].tBuffer,
            .offset = 0,
            .range  = 1024
        };

        VkDescriptorBufferInfo tDescriptorInfo1 = {
            .buffer = ptVulkanGfx->tDynamicBuffers[1].tBuffer,
            .offset = 0,
            .range  = 1024
        };

        VkWriteDescriptorSet tWrite0 = {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding      = 0,
            .dstArrayElement = 0,
            .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .descriptorCount = 1,
            .dstSet          = ptVulkanGfx->tDynamicBuffers[0].tDescriptorSet,
            .pBufferInfo     = &tDescriptorInfo0,
            .pNext           = NULL,
        };

        VkWriteDescriptorSet tWrite1 = {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding      = 0,
            .dstArrayElement = 0,
            .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .descriptorCount = 1,
            .dstSet          = ptVulkanGfx->tDynamicBuffers[1].tDescriptorSet,
            .pBufferInfo     = &tDescriptorInfo1,
            .pNext           = NULL,
        };

        vkUpdateDescriptorSets(ptVulkanDevice->tLogicalDevice, 1, &tWrite0, 0, NULL);
        vkUpdateDescriptorSets(ptVulkanDevice->tLogicalDevice, 1, &tWrite1, 0, NULL);
    }

    // staging buffer
    {
        const VkBufferCreateInfo tBufferCreateInfo = {
            .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size        = PL_DEVICE_ALLOCATION_BLOCK_SIZE,
            .usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };
        ptVulkanGfx->szStageByteSize = PL_DEVICE_ALLOCATION_BLOCK_SIZE;
        PL_VULKAN(vkCreateBuffer(ptVulkanDevice->tLogicalDevice, &tBufferCreateInfo, NULL, &ptVulkanGfx->tStagingBuffer));

        VkMemoryRequirements tMemoryRequirements = {0};
        vkGetBufferMemoryRequirements(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tStagingBuffer, &tMemoryRequirements);
        ptVulkanGfx->szStageByteSize = tMemoryRequirements.size;

        uint32_t uMemoryType = 0u;
        bool bFound = false;
        const VkMemoryPropertyFlags tProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        for (uint32_t i = 0; i < ptVulkanDevice->tMemProps.memoryTypeCount; i++) 
        {
            if ((tMemoryRequirements.memoryTypeBits & (1 << i)) && (ptVulkanDevice->tMemProps.memoryTypes[i].propertyFlags & tProperties) == tProperties) 
            {
                uMemoryType = i;
                bFound = true;
                break;
            }
        }

        const VkMemoryAllocateInfo tAllocInfo = {
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize  = ptVulkanGfx->szStageByteSize,
            .memoryTypeIndex = uMemoryType
        };

        PL_VULKAN(vkAllocateMemory(ptVulkanDevice->tLogicalDevice, &tAllocInfo, NULL, &ptVulkanGfx->tStagingMemory));

        PL_VULKAN(vkMapMemory(ptVulkanDevice->tLogicalDevice, (VkDeviceMemory)ptVulkanGfx->tStagingMemory, 0, ptVulkanGfx->szStageByteSize, 0, &ptVulkanGfx->pStageMapping));
        PL_VULKAN(vkBindBufferMemory(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tStagingBuffer, ptVulkanGfx->tStagingMemory, 0));
    }
}

static bool
pl_begin_frame(plGraphics* ptGraphics)
{
    pl_begin_profile_sample(__FUNCTION__);
    plIO* ptIOCtx = pl_get_io();

    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;
    ptVulkanGfx->uDynamicByteOffset = 0;

    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptGraphics);

    // cleanup queue
    plFrameGarbage* ptGarbage = &ptVulkanDevice->_sbtFrameGarbage[ptVulkanDevice->uCurrentFrame];

    if(ptGarbage)
    {
        for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtTextures); i++)
            vkDestroyImage(ptVulkanDevice->tLogicalDevice, ptGarbage->sbtTextures[i], NULL);

        for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtTextureViews); i++)
            vkDestroyImageView(ptVulkanDevice->tLogicalDevice, ptGarbage->sbtTextureViews[i], NULL);

        for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtFrameBuffers); i++)
            vkDestroyFramebuffer(ptVulkanDevice->tLogicalDevice, ptGarbage->sbtFrameBuffers[i], NULL);

        for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtMemory); i++)
            vkFreeMemory(ptVulkanDevice->tLogicalDevice, ptGarbage->sbtMemory[i], NULL);

        pl_sb_reset(ptGarbage->sbtTextures);
        pl_sb_reset(ptGarbage->sbtTextureViews);
        pl_sb_reset(ptGarbage->sbtFrameBuffers);
        pl_sb_reset(ptGarbage->sbtMemory);
    }

    PL_VULKAN(vkWaitForFences(ptVulkanDevice->tLogicalDevice, 1, &ptCurrentFrame->tInFlight, VK_TRUE, UINT64_MAX));
    VkResult err = vkAcquireNextImageKHR(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tSwapchain.tSwapChain, UINT64_MAX, ptCurrentFrame->tImageAvailable, VK_NULL_HANDLE, &ptVulkanGfx->tSwapchain.uCurrentImageIndex);
    if(err == VK_SUBOPTIMAL_KHR || err == VK_ERROR_OUT_OF_DATE_KHR)
    {
        if(err == VK_ERROR_OUT_OF_DATE_KHR)
        {
            pl__create_swapchain(ptGraphics, (uint32_t)ptIOCtx->afMainViewportSize[0], (uint32_t)ptIOCtx->afMainViewportSize[1], &ptVulkanGfx->tSwapchain);

            // recreate frame buffers
            pl_sb_resize(ptVulkanGfx->tSwapchain.sbtFrameBuffers, ptVulkanGfx->tSwapchain.uImageCount);
            for(uint32_t i = 0; i < ptVulkanGfx->tSwapchain.uImageCount; i++)
            {
                ptVulkanGfx->tSwapchain.sbtFrameBuffers[i] = VK_NULL_HANDLE;

                VkImageView atViewAttachments[] = {
                    ptVulkanGfx->tSwapchain.tColorTextureView,
                    ptVulkanGfx->tSwapchain.tDepthTextureView,
                    ptVulkanGfx->tSwapchain.sbtImageViews[i]
                };

                VkFramebufferCreateInfo tFrameBufferInfo = {
                    .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                    .renderPass      = ptVulkanGfx->tRenderPass,
                    .attachmentCount = 3,
                    .pAttachments    = atViewAttachments,
                    .width           = ptVulkanGfx->tSwapchain.tExtent.width,
                    .height          = ptVulkanGfx->tSwapchain.tExtent.height,
                    .layers          = 1u,
                };
                PL_VULKAN(vkCreateFramebuffer(ptVulkanDevice->tLogicalDevice, &tFrameBufferInfo, NULL, &ptVulkanGfx->tSwapchain.sbtFrameBuffers[i]));
            }

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

    //-----------------------------------------------------------------------------
    // buffer deletion queue
    //-----------------------------------------------------------------------------

    pl_sb_reset(ptVulkanGfx->sbReturnedBuffersTemp);
    if(ptVulkanGfx->uBufferDeletionQueueSize > 0u)
    {
        for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbReturnedBuffers); i++)
        {
            if(ptVulkanGfx->sbReturnedBuffers[i].slFreedFrame < (int64_t)ptGraphics->uCurrentFrameIndex)
            {
                ptVulkanGfx->uBufferDeletionQueueSize--;
                vkDestroyBuffer(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbReturnedBuffers[i].tBuffer, NULL);
                vkFreeMemory(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbReturnedBuffers[i].tDeviceMemory, NULL);
            }
            else
            {
                pl_sb_push(ptVulkanGfx->sbReturnedBuffersTemp, ptVulkanGfx->sbReturnedBuffers[i]);
            }
        }     
    }

    pl_sb_reset(ptVulkanGfx->sbReturnedBuffers);
    for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbReturnedBuffersTemp); i++)
        pl_sb_push(ptVulkanGfx->sbReturnedBuffers, ptVulkanGfx->sbReturnedBuffersTemp[i]);

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

static void
pl_end_gfx_frame(plGraphics* ptGraphics)
{
    pl_begin_profile_sample(__FUNCTION__);
    plIO* ptIOCtx = pl_get_io();

    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptGraphics);

    // submit
    const VkPipelineStageFlags atWaitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    const VkSubmitInfo tSubmitInfo = {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = &ptCurrentFrame->tImageAvailable,
        .pWaitDstStageMask    = atWaitStages,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &ptCurrentFrame->tCmdBuf,
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
        .pSwapchains        = &ptVulkanGfx->tSwapchain.tSwapChain,
        .pImageIndices      = &ptVulkanGfx->tSwapchain.uCurrentImageIndex,
    };
    const VkResult tResult = vkQueuePresentKHR(ptVulkanDevice->tPresentQueue, &tPresentInfo);
    if(tResult == VK_SUBOPTIMAL_KHR || tResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        pl__create_swapchain(ptGraphics, (uint32_t)ptIOCtx->afMainViewportSize[0], (uint32_t)ptIOCtx->afMainViewportSize[1], &ptVulkanGfx->tSwapchain);

        // recreate frame buffers
        pl_sb_resize(ptVulkanGfx->tSwapchain.sbtFrameBuffers, ptVulkanGfx->tSwapchain.uImageCount);
        for(uint32_t i = 0; i < ptVulkanGfx->tSwapchain.uImageCount; i++)
        {
            ptVulkanGfx->tSwapchain.sbtFrameBuffers[i] = VK_NULL_HANDLE;

            VkImageView atViewAttachments[] = {
                ptVulkanGfx->tSwapchain.tColorTextureView,
                ptVulkanGfx->tSwapchain.tDepthTextureView,
                ptVulkanGfx->tSwapchain.sbtImageViews[i]
            };

            VkFramebufferCreateInfo tFrameBufferInfo = {
                .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass      = ptVulkanGfx->tRenderPass,
                .attachmentCount = 3,
                .pAttachments    = atViewAttachments,
                .width           = ptVulkanGfx->tSwapchain.tExtent.width,
                .height          = ptVulkanGfx->tSwapchain.tExtent.height,
                .layers          = 1u,
            };
            PL_VULKAN(vkCreateFramebuffer(ptVulkanDevice->tLogicalDevice, &tFrameBufferInfo, NULL, &ptVulkanGfx->tSwapchain.sbtFrameBuffers[i]));
        }
    }
    else
    {
        PL_VULKAN(tResult);
    }

    ptGraphics->uCurrentFrameIndex = (ptGraphics->uCurrentFrameIndex + 1) % ptGraphics->uFramesInFlight;  

    pl_end_profile_sample();
}

static void
pl_resize(plGraphics* ptGraphics)
{
    pl_begin_profile_sample(__FUNCTION__);
    plIO* ptIOCtx = pl_get_io();

    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptGraphics);

    pl__create_swapchain(ptGraphics, (uint32_t)ptIOCtx->afMainViewportSize[0], (uint32_t)ptIOCtx->afMainViewportSize[1], &ptVulkanGfx->tSwapchain);

    // recreate frame buffers
    pl_sb_resize(ptVulkanGfx->tSwapchain.sbtFrameBuffers, ptVulkanGfx->tSwapchain.uImageCount);
    for(uint32_t i = 0; i < ptVulkanGfx->tSwapchain.uImageCount; i++)
    {
        ptVulkanGfx->tSwapchain.sbtFrameBuffers[i] = VK_NULL_HANDLE;

        VkImageView atViewAttachments[] = {
            ptVulkanGfx->tSwapchain.tColorTextureView,
            ptVulkanGfx->tSwapchain.tDepthTextureView,
            ptVulkanGfx->tSwapchain.sbtImageViews[i]
        };

        VkFramebufferCreateInfo tFrameBufferInfo = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = ptVulkanGfx->tRenderPass,
            .attachmentCount = 3,
            .pAttachments    = atViewAttachments,
            .width           = ptVulkanGfx->tSwapchain.tExtent.width,
            .height          = ptVulkanGfx->tSwapchain.tExtent.height,
            .layers          = 1u,
        };
        PL_VULKAN(vkCreateFramebuffer(ptVulkanDevice->tLogicalDevice, &tFrameBufferInfo, NULL, &ptVulkanGfx->tSwapchain.sbtFrameBuffers[i]));
    }

    ptGraphics->uCurrentFrameIndex = 0;

    pl_end_profile_sample();
}

static void
pl_shutdown(plGraphics* ptGraphics)
{
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    vkDeviceWaitIdle(ptVulkanDevice->tLogicalDevice);

    pl_cleanup_vulkan();

    // cleanup 3d
    {
        for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbt3DBufferInfo); i++)
        {
            vkDestroyBuffer(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbt3DBufferInfo[i].tVertexBuffer, NULL);
            vkFreeMemory(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbt3DBufferInfo[i].tVertexMemory, NULL);
            vkDestroyBuffer(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbt3DBufferInfo[i].tIndexBuffer, NULL);
            vkFreeMemory(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbt3DBufferInfo[i].tIndexMemory, NULL);
        }

        for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbtLineBufferInfo); i++)
        {
            vkDestroyBuffer(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtLineBufferInfo[i].tVertexBuffer, NULL);
            vkFreeMemory(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtLineBufferInfo[i].tVertexMemory, NULL);
            vkDestroyBuffer(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtLineBufferInfo[i].tIndexBuffer, NULL);
            vkFreeMemory(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtLineBufferInfo[i].tIndexMemory, NULL);
        }

        for(uint32_t i = 0u; i < pl_sb_size(ptVulkanGfx->sbt3DPipelines); i++)
        {
            vkDestroyPipeline(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbt3DPipelines[i].tRegularPipeline, NULL);
            vkDestroyPipeline(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbt3DPipelines[i].tSecondaryPipeline, NULL);
        }

        if(ptVulkanGfx->uBufferDeletionQueueSize > 0u)
        {
            for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbReturnedBuffers); i++)
            {
                vkDestroyBuffer(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbReturnedBuffers[i].tBuffer, NULL);
                vkFreeMemory(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbReturnedBuffers[i].tDeviceMemory, NULL);
            }     
        }

        vkDestroyShaderModule(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->t3DPxlShdrStgInfo.module, NULL);
        vkDestroyShaderModule(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->t3DVtxShdrStgInfo.module, NULL);
        vkDestroyShaderModule(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->t3DLineVtxShdrStgInfo.module, NULL);
        vkDestroyBuffer(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tStagingBuffer, NULL);
        vkFreeMemory(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tStagingMemory, NULL);
        vkDestroyPipelineLayout(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->t3DPipelineLayout, NULL);
        vkDestroyPipelineLayout(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->t3DLinePipelineLayout, NULL);

        pl_sb_free(ptVulkanGfx->sbReturnedBuffers);
        pl_sb_free(ptVulkanGfx->sbReturnedBuffersTemp);
        pl_sb_free(ptVulkanGfx->sbt3DBufferInfo);
        pl_sb_free(ptVulkanGfx->sbtLineBufferInfo);
        pl_sb_free(ptVulkanGfx->sbt3DPipelines);

        for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbtTextures); i++)
        {
            vkDestroyImage(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtTextures[i].tImage, NULL);
            vkFreeMemory(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtTextures[i].tMemory, NULL);
        }

        for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbtSamplers); i++)
        {
            vkDestroySampler(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtSamplers[i].tSampler, NULL);
            vkDestroyImageView(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtSamplers[i].tImageView, NULL);
        }

        for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbtBindGroups); i++)
        {
            vkDestroyDescriptorSetLayout(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtBindGroups[i].tDescriptorSetLayout, NULL);
        }

        for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbtBuffers); i++)
        {
            vkDestroyBuffer(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtBuffers[i].tBuffer, NULL);
            vkFreeMemory(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtBuffers[i].tMemory, NULL);
        }

        for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbtShaders); i++)
        {
            vkDestroyPipeline(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtShaders[i].tPipeline, NULL);
            vkDestroyPipelineLayout(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtShaders[i].tPipelineLayout, NULL);
        }

        for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbtBindGroupLayouts); i++)
        {
            vkDestroyDescriptorSetLayout(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtBindGroupLayouts[i].tDescriptorSetLayout, NULL);    
        }

        vkDestroyDescriptorSetLayout(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tDynamicDescriptorSetLayout, NULL);
        pl_sb_free(ptVulkanGfx->sbtTextures);
        pl_sb_free(ptVulkanGfx->sbtSamplers);
        pl_sb_free(ptVulkanGfx->sbtBindGroups);
        pl_sb_free(ptVulkanGfx->sbtBuffers);
        pl_sb_free(ptVulkanGfx->sbtShaders);
        pl_sb_free(ptVulkanGfx->sbtBindGroupLayouts);
        
        for(uint32_t i = 0u; i < pl_sb_size(ptGraphics->sbt3DDrawlists); i++)
        {
            plDrawList3D* drawlist = ptGraphics->sbt3DDrawlists[i];
            pl_sb_free(drawlist->sbtSolidIndexBuffer);
            pl_sb_free(drawlist->sbtSolidVertexBuffer);
            pl_sb_free(drawlist->sbtLineVertexBuffer);
            pl_sb_free(drawlist->sbtLineIndexBuffer);  
        }
        pl_sb_free(ptGraphics->sbt3DDrawlists);
    }

    // cleanup buffers
    // for(uint32_t i = 0; i < pl_sb_size(ptGraphics->tDevice.sbtBuffers); i++)
    // {
    //     plVulkanBuffer* ptBuffer = ptGraphics->tDevice.sbtBuffers[i].pBuffer;
    //     vkDestroyBuffer(ptVulkanDevice->tLogicalDevice, ptBuffer->tBuffer, NULL);
    //     vkFreeMemory(ptVulkanDevice->tLogicalDevice, ptBuffer->tMemory, NULL);
    //     PL_FREE(ptGraphics->tDevice.sbtBuffers[i].pBuffer);
    // }

    // cleanup per frame resources
    for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbFrames); i++)
    {
        plFrameContext* ptFrame = &ptVulkanGfx->sbFrames[i];
        vkDestroySemaphore(ptVulkanDevice->tLogicalDevice, ptFrame->tImageAvailable, NULL);
        vkDestroySemaphore(ptVulkanDevice->tLogicalDevice, ptFrame->tRenderFinish, NULL);
        vkDestroyFence(ptVulkanDevice->tLogicalDevice, ptFrame->tInFlight, NULL);
        vkDestroyCommandPool(ptVulkanDevice->tLogicalDevice, ptFrame->tCmdPool, NULL);
    }

    // swapchain stuff
    vkDestroyImageView(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tSwapchain.tColorTextureView, NULL);
    vkDestroyImageView(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tSwapchain.tDepthTextureView, NULL);
    vkDestroyImage(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tSwapchain.tColorTexture, NULL);
    vkDestroyImage(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tSwapchain.tDepthTexture, NULL);
    vkFreeMemory(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tSwapchain.tColorTextureMemory, NULL);
    vkFreeMemory(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tSwapchain.tDepthTextureMemory, NULL);

    for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->tSwapchain.sbtImageViews); i++)
        vkDestroyImageView(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tSwapchain.sbtImageViews[i], NULL);

    for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->tSwapchain.sbtFrameBuffers); i++)
        vkDestroyFramebuffer(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tSwapchain.sbtFrameBuffers[i], NULL);
    

    // vkDestroyPipelineLayout(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->g_pipelineLayout, NULL);
    // vkDestroyPipeline(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->g_pipeline, NULL);

    vkDestroyRenderPass(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tRenderPass, NULL);

    vkDestroyDescriptorPool(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tDescriptorPool, NULL);

    // destroy command pool
    vkDestroyCommandPool(ptVulkanDevice->tLogicalDevice, ptVulkanDevice->tCmdPool, NULL);

    vkDestroySwapchainKHR(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tSwapchain.tSwapChain, NULL);


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

    for(uint32_t i = 0; i < pl_sb_size(ptVulkanDevice->_sbtFrameGarbage); i++)
    {
        pl_sb_free(ptVulkanDevice->_sbtFrameGarbage[i].sbtFrameBuffers);
        pl_sb_free(ptVulkanDevice->_sbtFrameGarbage[i].sbtMemory);
        pl_sb_free(ptVulkanDevice->_sbtFrameGarbage[i].sbtTextures);
        pl_sb_free(ptVulkanDevice->_sbtFrameGarbage[i].sbtTextureViews);
    }

    pl_sb_free(ptVulkanDevice->_sbtFrameGarbage);
    pl_sb_free(ptVulkanGfx->sbFrames);
    pl_sb_free(ptVulkanGfx->tSwapchain.sbtSurfaceFormats);
    pl_sb_free(ptVulkanGfx->tSwapchain.sbtImages);
    pl_sb_free(ptVulkanGfx->tSwapchain.sbtFrameBuffers);
    pl_sb_free(ptVulkanGfx->tSwapchain.sbtImageViews);
    // pl_sb_free(ptGraphics->tDevice.sbtBuffers);
    PL_FREE(ptGraphics->_pInternalData);
    PL_FREE(ptGraphics->tDevice._pInternalData);
}

static void
pl_draw_areas(plGraphics* ptGraphics, uint32_t uAreaCount, plDrawArea* atAreas, plDraw* atDraws)
{
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptGraphics); 

    static VkDeviceSize offsets = { 0 };
    vkCmdSetDepthBias(ptCurrentFrame->tCmdBuf, 0.0f, 0.0f, 0.0f);

    for(uint32_t i = 0; i < uAreaCount; i++)
    {
        plDrawArea* ptArea = &atAreas[i];

        for(uint32_t j = 0; j < ptArea->uDrawCount; j++)
        {
            plDraw* ptDraw = &atDraws[ptArea->uDrawOffset + j];
            plVulkanShader* ptVulkanShader = &ptVulkanGfx->sbtShaders[ptDraw->uShaderVariant];
            vkCmdBindPipeline(ptCurrentFrame->tCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, ptVulkanShader->tPipeline);

            plVulkanBindGroup* ptBindGroup0 = &ptVulkanGfx->sbtBindGroups[ptDraw->aptBindGroups[0]->uHandle];
            plVulkanBindGroup* ptBindGroup1 = &ptVulkanGfx->sbtBindGroups[ptDraw->aptBindGroups[1]->uHandle];
            plVulkanBindGroup* ptBindGroup2 = &ptVulkanGfx->sbtBindGroups[ptDraw->aptBindGroups[2]->uHandle];
            plVulkanDynamicBuffer* ptVulkanDynamicBuffer = &ptVulkanGfx->tDynamicBuffers[ptDraw->uDynamicBuffer];

            vkCmdBindDescriptorSets(ptCurrentFrame->tCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, ptVulkanShader->tPipelineLayout, 0, 1, &ptBindGroup0->tDescriptorSet, 0, NULL);
            vkCmdBindDescriptorSets(ptCurrentFrame->tCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, ptVulkanShader->tPipelineLayout, 1, 1, &ptBindGroup1->tDescriptorSet, 0, NULL);
            vkCmdBindDescriptorSets(ptCurrentFrame->tCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, ptVulkanShader->tPipelineLayout, 2, 1, &ptBindGroup2->tDescriptorSet, 0, NULL);
            vkCmdBindDescriptorSets(ptCurrentFrame->tCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, ptVulkanShader->tPipelineLayout, 3, 1, &ptVulkanDynamicBuffer->tDescriptorSet, 1, &ptDraw->auDynamicBufferOffset[0]);

            plVulkanBuffer* ptVertexBuffer = &ptVulkanGfx->sbtBuffers[ptDraw->uVertexBuffer];
            plVulkanBuffer* ptIndexBuffer = &ptVulkanGfx->sbtBuffers[ptDraw->uIndexBuffer];
            vkCmdBindIndexBuffer(ptCurrentFrame->tCmdBuf, ptIndexBuffer->tBuffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindVertexBuffers(ptCurrentFrame->tCmdBuf, 0, 1, &ptVertexBuffer->tBuffer, &offsets);

            vkCmdDrawIndexed(ptCurrentFrame->tCmdBuf, ptDraw->uIndexCount, 1, ptDraw->uIndexOffset, 0, 0);

        }
    }
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static VkDeviceMemory
pl__allocate_dedicated(plDevice* ptDevice, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName)
{
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;

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
        .allocationSize  = (uint32_t)ulSize,
        .memoryTypeIndex = uMemoryType
    };

    VkDeviceMemory tMemory = VK_NULL_HANDLE;
    VkResult tResult = vkAllocateMemory(ptVulkanDevice->tLogicalDevice, &tAllocInfo, NULL, &tMemory);
    PL_VULKAN(tResult);

    return tMemory;
}

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
pl__create_swapchain(plGraphics* ptGraphics, uint32_t uWidth, uint32_t uHeight, plVulkanSwapchain* ptSwapchainOut)
{
    plVulkanGraphics* ptVulkanGfx    = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    vkDeviceWaitIdle(ptVulkanDevice->tLogicalDevice);

    ptSwapchainOut->tMsaaSamples = pl__get_max_sample_count(&ptGraphics->tDevice);

    // query swapchain support

    VkSurfaceCapabilitiesKHR tCapabilities = {0};
    PL_VULKAN(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ptVulkanDevice->tPhysicalDevice, ptVulkanGfx->tSurface, &tCapabilities));

    uint32_t uFormatCount = 0u;
    PL_VULKAN(vkGetPhysicalDeviceSurfaceFormatsKHR(ptVulkanDevice->tPhysicalDevice, ptVulkanGfx->tSurface, &uFormatCount, NULL));
    
    pl_sb_resize(ptSwapchainOut->sbtSurfaceFormats, uFormatCount);

    VkBool32 tPresentSupport = false;
    PL_VULKAN(vkGetPhysicalDeviceSurfaceSupportKHR(ptVulkanDevice->tPhysicalDevice, 0, ptVulkanGfx->tSurface, &tPresentSupport));
    PL_ASSERT(uFormatCount > 0);
    PL_VULKAN(vkGetPhysicalDeviceSurfaceFormatsKHR(ptVulkanDevice->tPhysicalDevice, ptVulkanGfx->tSurface, &uFormatCount, ptSwapchainOut->sbtSurfaceFormats));

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
    VkSurfaceFormatKHR tSurfaceFormat = ptSwapchainOut->sbtSurfaceFormats[0];
    ptSwapchainOut->tFormat = tSurfaceFormat.format;

    for(uint32_t i = 0u; i < 4; i++)
    {
        if(bPreferenceFound) break;
        
        for(uint32_t j = 0u; j < uFormatCount; j++)
        {
            if(ptSwapchainOut->sbtSurfaceFormats[j].format == atSurfaceFormatPreference[i] && ptSwapchainOut->sbtSurfaceFormats[j].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                tSurfaceFormat = ptSwapchainOut->sbtSurfaceFormats[j];
                ptSwapchainOut->tFormat = tSurfaceFormat.format;
                bPreferenceFound = true;
                break;
            }
        }
    }
    PL_ASSERT(bPreferenceFound && "no preferred surface format found");

    // chose swap present mode
    VkPresentModeKHR tPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    if(!ptSwapchainOut->bVSync)
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
    ptSwapchainOut->tExtent = tExtent;

    // decide image count
    const uint32_t uOldImageCount = ptSwapchainOut->uImageCount;
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
        .oldSwapchain     = ptSwapchainOut->tSwapChain, // setting oldSwapChain to the saved handle of the previous swapchain aids in resource reuse and makes sure that we can still present already acquired images
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

    VkSwapchainKHR tOldSwapChain = ptSwapchainOut->tSwapChain;

    PL_VULKAN(vkCreateSwapchainKHR(ptVulkanDevice->tLogicalDevice, &tCreateSwapchainInfo, NULL, &ptSwapchainOut->tSwapChain));

    if(tOldSwapChain)
    {
        if(pl_sb_size(ptVulkanDevice->_sbtFrameGarbage) == 0)
        {
            pl_sb_resize(ptVulkanDevice->_sbtFrameGarbage, ptSwapchainOut->uImageCount);
        }
        for (uint32_t i = 0u; i < uOldImageCount; i++)
        {
            pl_sb_push(ptVulkanDevice->_sbtFrameGarbage[ptVulkanDevice->uCurrentFrame].sbtTextureViews, ptSwapchainOut->sbtImageViews[i]);
            pl_sb_push(ptVulkanDevice->_sbtFrameGarbage[ptVulkanDevice->uCurrentFrame].sbtFrameBuffers, ptSwapchainOut->sbtFrameBuffers[i]);
        }
        vkDestroySwapchainKHR(ptVulkanDevice->tLogicalDevice, tOldSwapChain, NULL);
    }

    // get swapchain images

    PL_VULKAN(vkGetSwapchainImagesKHR(ptVulkanDevice->tLogicalDevice, ptSwapchainOut->tSwapChain, &ptSwapchainOut->uImageCount, NULL));
    pl_sb_resize(ptSwapchainOut->sbtImages, ptSwapchainOut->uImageCount);
    pl_sb_resize(ptSwapchainOut->sbtImageViews, ptSwapchainOut->uImageCount);

    PL_VULKAN(vkGetSwapchainImagesKHR(ptVulkanDevice->tLogicalDevice, ptSwapchainOut->tSwapChain, &ptSwapchainOut->uImageCount, ptSwapchainOut->sbtImages));

    for(uint32_t i = 0; i < ptSwapchainOut->uImageCount; i++)
    {

        ptSwapchainOut->sbtImageViews[i] = VK_NULL_HANDLE;

        VkImageViewCreateInfo tViewInfo = {
            .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image            = ptSwapchainOut->sbtImages[i],
            .viewType         = VK_IMAGE_VIEW_TYPE_2D,
            .format           = ptSwapchainOut->tFormat,
            .subresourceRange = {
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT
            },
            .components = {
                        VK_COMPONENT_SWIZZLE_R,
                        VK_COMPONENT_SWIZZLE_G,
                        VK_COMPONENT_SWIZZLE_B,
                        VK_COMPONENT_SWIZZLE_A
                    }
        };

        PL_VULKAN(vkCreateImageView(ptVulkanDevice->tLogicalDevice, &tViewInfo, NULL, &ptSwapchainOut->sbtImageViews[i]));
    }  //-V1020

    // color & depth
    if(ptSwapchainOut->tColorTextureView)  pl_sb_push(ptVulkanDevice->_sbtFrameGarbage[ptVulkanDevice->uCurrentFrame].sbtTextureViews, ptSwapchainOut->tColorTextureView);
    if(ptSwapchainOut->tDepthTextureView)  pl_sb_push(ptVulkanDevice->_sbtFrameGarbage[ptVulkanDevice->uCurrentFrame].sbtTextureViews, ptSwapchainOut->tDepthTextureView);
    if(ptSwapchainOut->tColorTexture)      pl_sb_push(ptVulkanDevice->_sbtFrameGarbage[ptVulkanDevice->uCurrentFrame].sbtTextures, ptSwapchainOut->tColorTexture);
    if(ptSwapchainOut->tDepthTexture)      pl_sb_push(ptVulkanDevice->_sbtFrameGarbage[ptVulkanDevice->uCurrentFrame].sbtTextures, ptSwapchainOut->tDepthTexture);
    if(ptSwapchainOut->tColorTextureMemory)      pl_sb_push(ptVulkanDevice->_sbtFrameGarbage[ptVulkanDevice->uCurrentFrame].sbtMemory, ptSwapchainOut->tColorTextureMemory);
    if(ptSwapchainOut->tDepthTextureMemory)      pl_sb_push(ptVulkanDevice->_sbtFrameGarbage[ptVulkanDevice->uCurrentFrame].sbtMemory, ptSwapchainOut->tDepthTextureMemory);

    ptSwapchainOut->tColorTextureView = VK_NULL_HANDLE;
    ptSwapchainOut->tColorTexture     = VK_NULL_HANDLE;
    ptSwapchainOut->tDepthTextureView = VK_NULL_HANDLE;
    ptSwapchainOut->tDepthTexture     = VK_NULL_HANDLE;

    VkImageCreateInfo tDepthImageInfo = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .extent        = 
        {
            .width  = ptSwapchainOut->tExtent.width,
            .height = ptSwapchainOut->tExtent.height,
            .depth  = 1
        },
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .format        = pl__find_depth_stencil_format(&ptGraphics->tDevice),
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage         = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .samples       = ptSwapchainOut->tMsaaSamples,
        .flags         = 0
    };

    VkImageCreateInfo tColorImageInfo = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .extent        = 
        {
            .width  = ptSwapchainOut->tExtent.width,
            .height = ptSwapchainOut->tExtent.height,
            .depth  = 1
        },
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .format        = ptSwapchainOut->tFormat,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .samples       = ptSwapchainOut->tMsaaSamples,
        .flags         = 0
    };

    PL_VULKAN(vkCreateImage(ptVulkanDevice->tLogicalDevice, &tDepthImageInfo, NULL, &ptSwapchainOut->tDepthTexture));
    PL_VULKAN(vkCreateImage(ptVulkanDevice->tLogicalDevice, &tColorImageInfo, NULL, &ptSwapchainOut->tColorTexture));

    VkMemoryRequirements tDepthMemReqs = {0};
    VkMemoryRequirements tColorMemReqs = {0};
    vkGetImageMemoryRequirements(ptVulkanDevice->tLogicalDevice, ptSwapchainOut->tDepthTexture, &tDepthMemReqs);
    vkGetImageMemoryRequirements(ptVulkanDevice->tLogicalDevice, ptSwapchainOut->tColorTexture, &tColorMemReqs);


    ptSwapchainOut->tColorTextureMemory = pl__allocate_dedicated(&ptGraphics->tDevice, tColorMemReqs.memoryTypeBits, tColorMemReqs.size, tColorMemReqs.alignment, "swapchain color");
    ptSwapchainOut->tDepthTextureMemory = pl__allocate_dedicated(&ptGraphics->tDevice, tDepthMemReqs.memoryTypeBits, tDepthMemReqs.size, tDepthMemReqs.alignment, "swapchain depth");

    PL_VULKAN(vkBindImageMemory(ptVulkanDevice->tLogicalDevice, ptSwapchainOut->tDepthTexture, ptSwapchainOut->tDepthTextureMemory, 0));
    PL_VULKAN(vkBindImageMemory(ptVulkanDevice->tLogicalDevice, ptSwapchainOut->tColorTexture, ptSwapchainOut->tColorTextureMemory, 0));

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
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1,
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT
    };

    pl__transition_image_layout(tCommandBuffer, ptSwapchainOut->tColorTexture, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, tRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    tRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    pl__transition_image_layout(tCommandBuffer, ptSwapchainOut->tDepthTexture, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, tRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    PL_VULKAN(vkEndCommandBuffer(tCommandBuffer));
    const VkSubmitInfo tSubmitInfo = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1u,
        .pCommandBuffers    = &tCommandBuffer,
    };

    PL_VULKAN(vkQueueSubmit(ptVulkanDevice->tGraphicsQueue, 1, &tSubmitInfo, VK_NULL_HANDLE));
    PL_VULKAN(vkDeviceWaitIdle(ptVulkanDevice->tLogicalDevice));
    vkFreeCommandBuffers(ptVulkanDevice->tLogicalDevice, ptVulkanDevice->tCmdPool, 1, &tCommandBuffer);

    VkImageViewCreateInfo tDepthViewInfo = {
        .sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image                           = ptSwapchainOut->tDepthTexture,
        .viewType                        = VK_IMAGE_VIEW_TYPE_2D,
        .format                          = tDepthImageInfo.format,
        .subresourceRange.baseMipLevel   = 0,
        .subresourceRange.levelCount     = 1,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount     = 1,
        .subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT,
    };

    if(pl__format_has_stencil(tDepthViewInfo.format))
        tDepthViewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

    VkImageViewCreateInfo tColorViewInfo = {
        .sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image                           = ptSwapchainOut->tColorTexture,
        .viewType                        = VK_IMAGE_VIEW_TYPE_2D,
        .format                          = tColorImageInfo.format,
        .subresourceRange.baseMipLevel   = 0,
        .subresourceRange.levelCount     = 1,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount     = 1,
        .subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
    };

    PL_VULKAN(vkCreateImageView(ptVulkanDevice->tLogicalDevice, &tDepthViewInfo, NULL, &ptSwapchainOut->tDepthTextureView));
    PL_VULKAN(vkCreateImageView(ptVulkanDevice->tLogicalDevice, &tColorViewInfo, NULL, &ptSwapchainOut->tColorTextureView));

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
pl__grow_vulkan_3d_vertex_buffer(plGraphics* ptGfx, uint32_t uVtxBufSzNeeded, pl3DVulkanBufferInfo* ptBufferInfo)
{
    plVulkanGraphics* ptVulkanGfx = ptGfx->_pInternalData;
    plVulkanDevice* ptVulkanDevice = ptGfx->tDevice._pInternalData;

    // buffer currently exists & mapped, submit for cleanup
    if(ptBufferInfo->ucVertexBufferMap)
    {
        const pl3DBufferReturn tReturnBuffer = {
            .tBuffer       = ptBufferInfo->tVertexBuffer,
            .tDeviceMemory = ptBufferInfo->tVertexMemory,
            .slFreedFrame  = (int64_t)(pl_get_io()->ulFrameCount + ptGfx->uFramesInFlight * 2)
        };
        pl_sb_push(ptVulkanGfx->sbReturnedBuffers, tReturnBuffer);
        ptVulkanGfx->uBufferDeletionQueueSize++;
        vkUnmapMemory(ptVulkanDevice->tLogicalDevice, ptBufferInfo->tVertexMemory);
    }

    // create new buffer
    const VkBufferCreateInfo tBufferCreateInfo = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = uVtxBufSzNeeded,
        .usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    PL_VULKAN(vkCreateBuffer(ptVulkanDevice->tLogicalDevice, &tBufferCreateInfo, NULL, &ptBufferInfo->tVertexBuffer));

    // check memory requirements
    VkMemoryRequirements tMemReqs = {0};
    vkGetBufferMemoryRequirements(ptVulkanDevice->tLogicalDevice, ptBufferInfo->tVertexBuffer, &tMemReqs);

    // allocate memory & bind buffer
    const VkMemoryAllocateInfo tAllocInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = tMemReqs.size,
        .memoryTypeIndex = pl__find_memory_type_(ptVulkanDevice->tMemProps, tMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    ptBufferInfo->uVertexByteSize = (uint32_t)tMemReqs.size;
    PL_VULKAN(vkAllocateMemory(ptVulkanDevice->tLogicalDevice, &tAllocInfo, NULL, &ptBufferInfo->tVertexMemory));
    PL_VULKAN(vkBindBufferMemory(ptVulkanDevice->tLogicalDevice, ptBufferInfo->tVertexBuffer, ptBufferInfo->tVertexMemory, 0));

    // map memory persistently
    PL_VULKAN(vkMapMemory(ptVulkanDevice->tLogicalDevice, ptBufferInfo->tVertexMemory, 0, tMemReqs.size, 0, (void**)&ptBufferInfo->ucVertexBufferMap));

    ptBufferInfo->uVertexBufferOffset = 0;
}

static void
pl__grow_vulkan_3d_index_buffer(plGraphics* ptGfx, uint32_t uIdxBufSzNeeded, pl3DVulkanBufferInfo* ptBufferInfo)
{
    plVulkanGraphics* ptVulkanGfx = ptGfx->_pInternalData;
    plVulkanDevice* ptVulkanDevice = ptGfx->tDevice._pInternalData;

    // buffer currently exists & mapped, submit for cleanup
    if(ptBufferInfo->ucIndexBufferMap)
    {
        const pl3DBufferReturn tReturnBuffer = {
            .tBuffer       = ptBufferInfo->tIndexBuffer,
            .tDeviceMemory = ptBufferInfo->tIndexMemory,
            .slFreedFrame  = (int64_t)(pl_get_io()->ulFrameCount + ptGfx->uFramesInFlight * 2)
        };
        pl_sb_push(ptVulkanGfx->sbReturnedBuffers, tReturnBuffer);
        ptVulkanGfx->uBufferDeletionQueueSize++;
        vkUnmapMemory(ptVulkanDevice->tLogicalDevice, ptBufferInfo->tIndexMemory);
    }

    // create new buffer
    const VkBufferCreateInfo tBufferCreateInfo = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = uIdxBufSzNeeded,
        .usage       = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    PL_VULKAN(vkCreateBuffer(ptVulkanDevice->tLogicalDevice, &tBufferCreateInfo, NULL, &ptBufferInfo->tIndexBuffer));

    // check memory requirements
    VkMemoryRequirements tMemReqs = {0};
    vkGetBufferMemoryRequirements(ptVulkanDevice->tLogicalDevice, ptBufferInfo->tIndexBuffer, &tMemReqs);

    // alllocate memory & bind buffer
    const VkMemoryAllocateInfo tAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = tMemReqs.size,
        .memoryTypeIndex = pl__find_memory_type_(ptVulkanDevice->tMemProps, tMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    ptBufferInfo->uIndexByteSize = (uint32_t)tMemReqs.size;
    PL_VULKAN(vkAllocateMemory(ptVulkanDevice->tLogicalDevice, &tAllocInfo, NULL, &ptBufferInfo->tIndexMemory));
    PL_VULKAN(vkBindBufferMemory(ptVulkanDevice->tLogicalDevice, ptBufferInfo->tIndexBuffer, ptBufferInfo->tIndexMemory, 0));

    // map memory persistently
    PL_VULKAN(vkMapMemory(ptVulkanDevice->tLogicalDevice, ptBufferInfo->tIndexMemory, 0, tMemReqs.size, 0, (void**)&ptBufferInfo->ucIndexBufferMap));

    ptBufferInfo->uIndexBufferOffset = 0;
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
pl__transfer_data_to_buffer(plDevice* ptDevice, VkBuffer tDest, size_t szSize, const void* pData)
{

    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptVulkanDevice->ptGraphics->_pInternalData;

    // copy data
    memcpy(ptVulkanGraphics->pStageMapping, pData, szSize);

    // flush memory (incase we are using non-coherent memory)
    const VkMappedMemoryRange tMemoryRange = {
        .sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        .memory = ptVulkanGraphics->tStagingMemory,
        .size   = VK_WHOLE_SIZE
    };
    PL_VULKAN(vkFlushMappedMemoryRanges(ptVulkanDevice->tLogicalDevice, 1, &tMemoryRange));

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

    const VkBufferCopy tCopyRegion = {
        .size = szSize
    };
    vkCmdCopyBuffer(tCommandBuffer, ptVulkanGraphics->tStagingBuffer, tDest, 1, &tCopyRegion);

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
pl__transfer_data_to_image(plDevice* ptDevice, plTexture* ptDest, size_t szDataSize, const void* pData)
{
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;
    plVulkanGraphics* ptVulkanGraphics = ptVulkanDevice->ptGraphics->_pInternalData;

    const plBufferDescription tStagingBufferDescription = {
        .pcDebugName          = "image staging",
        .tMemory              = PL_MEMORY_GPU_CPU,
        .tUsage               = PL_BUFFER_USAGE_UNSPECIFIED,
        .uByteSize            = (uint32_t)szDataSize,
        .uInitialDataByteSize = (uint32_t)szDataSize,
        .puInitialData        = pData
    };
    plBuffer tStagingBuffer = pl_create_buffer(ptDevice, &tStagingBufferDescription);
    plVulkanBuffer* ptVulkanStagingBuffer = &ptVulkanGraphics->sbtBuffers[tStagingBuffer.uHandle];

    // flush memory (incase we are using non-coherent memory)
    const VkMappedMemoryRange tMemoryRange = {
        .sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        .memory = (VkDeviceMemory)ptVulkanStagingBuffer->tMemory,
        .size   = VK_WHOLE_SIZE
    };
    PL_VULKAN(vkFlushMappedMemoryRanges(ptVulkanDevice->tLogicalDevice, 1, &tMemoryRange));

    const VkImageSubresourceRange tSubResourceRange = {
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = ptDest->tDesc.uMips,
        .baseArrayLayer = 0,
        .layerCount     = ptDest->tDesc.uLayers
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

    // transition destination image layout to transfer destination
    pl__transition_image_layout(tCommandBuffer, ptVulkanGraphics->sbtTextures[ptDest->uHandle].tImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, tSubResourceRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    // copy regions
    const VkBufferImageCopy tCopyRegion = {
        .bufferOffset                    = 0u,
        .bufferRowLength                 = 0u,
        .bufferImageHeight               = 0u,
        .imageSubresource.aspectMask     = tSubResourceRange.aspectMask,
        .imageSubresource.mipLevel       = 0,
        .imageSubresource.baseArrayLayer = 0,
        .imageSubresource.layerCount     = ptDest->tDesc.uLayers,
        .imageExtent                     = {.width = (uint32_t)ptDest->tDesc.tDimensions.x, .height = (uint32_t)ptDest->tDesc.tDimensions.y, .depth = (uint32_t)ptDest->tDesc.tDimensions.z},
    };
    vkCmdCopyBufferToImage(tCommandBuffer, ptVulkanStagingBuffer->tBuffer, ptVulkanGraphics->sbtTextures[ptDest->uHandle].tImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &tCopyRegion);

    // generate mips
    if(ptDest->tDesc.uMips > 1)
    {
        // check if format supports linear blitting
        VkFormatProperties tFormatProperties = {0};
        vkGetPhysicalDeviceFormatProperties(ptVulkanDevice->tPhysicalDevice, pl__vulkan_format(ptDest->tDesc.tFormat), &tFormatProperties);

        if(tFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)
        {
            VkImageSubresourceRange tMipSubResourceRange = {
                .aspectMask     = tSubResourceRange.aspectMask,
                .baseArrayLayer = 0,
                .layerCount     = ptDest->tDesc.uLayers,
                .levelCount     = 1
            };

            int iMipWidth = (int)ptDest->tDesc.tDimensions.x;
            int iMipHeight = (int)ptDest->tDesc.tDimensions.y;

            for(uint32_t i = 1; i < ptDest->tDesc.uMips; i++)
            {
                tMipSubResourceRange.baseMipLevel = i - 1;

                pl__transition_image_layout(tCommandBuffer, ptVulkanGraphics->sbtTextures[ptDest->uHandle].tImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, tMipSubResourceRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

                VkImageBlit tBlit = {
                    .srcOffsets[1].x               = iMipWidth,
                    .srcOffsets[1].y               = iMipHeight,
                    .srcOffsets[1].z               = 1,
                    .srcSubresource.aspectMask     = tSubResourceRange.aspectMask,
                    .srcSubresource.mipLevel       = i - 1,
                    .srcSubresource.baseArrayLayer = 0,
                    .srcSubresource.layerCount     = 1,     
                    .dstOffsets[1].x               = 1,
                    .dstOffsets[1].y               = 1,
                    .dstOffsets[1].z               = 1,
                    .dstSubresource.aspectMask     = tSubResourceRange.aspectMask,
                    .dstSubresource.mipLevel       = i,
                    .dstSubresource.baseArrayLayer = 0,
                    .dstSubresource.layerCount     = 1,
                };

                if(iMipWidth > 1)  tBlit.dstOffsets[1].x = iMipWidth / 2;
                if(iMipHeight > 1) tBlit.dstOffsets[1].y = iMipHeight / 2;

                vkCmdBlitImage(tCommandBuffer, ptVulkanGraphics->sbtTextures[ptDest->uHandle].tImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, ptVulkanGraphics->sbtTextures[ptDest->uHandle].tImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &tBlit, VK_FILTER_LINEAR);

                pl__transition_image_layout(tCommandBuffer, ptVulkanGraphics->sbtTextures[ptDest->uHandle].tImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, tMipSubResourceRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);


                if(iMipWidth > 1)  iMipWidth /= 2;
                if(iMipHeight > 1) iMipHeight /= 2;
            }

            tMipSubResourceRange.baseMipLevel = ptDest->tDesc.uMips - 1;
            pl__transition_image_layout(tCommandBuffer, ptVulkanGraphics->sbtTextures[ptDest->uHandle].tImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, tMipSubResourceRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        }
        else
        {
            PL_ASSERT(false && "format does not support linear blitting");
        }
    }
    else
    {
        // transition destination image layout to shader usage
        pl__transition_image_layout(tCommandBuffer, ptVulkanGraphics->sbtTextures[ptDest->uHandle].tImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, tSubResourceRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
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
    // pl_delete_buffer(ptDevice, uStagingBuffer);
}

static plFrameContext*
pl__get_frame_resources(plGraphics* ptGraphics)
{
    plVulkanGraphics* ptVulkanGfx    = ptGraphics->_pInternalData;
    return &ptVulkanGfx->sbFrames[ptGraphics->uCurrentFrameIndex];
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

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

static const plGraphicsI*
pl_load_graphics_api(void)
{
    static const plGraphicsI tApi = {
        .initialize               = pl_initialize_graphics,
        .resize                   = pl_resize,
        .begin_frame              = pl_begin_frame,
        .end_frame                = pl_end_gfx_frame,
        .begin_recording          = pl_begin_recording,
        .end_recording            = pl_end_recording,
        .draw_areas               = pl_draw_areas,
        .draw_lists               = pl_draw_list,
        .cleanup                  = pl_shutdown,
        .create_font_atlas        = pl_create_vulkan_font_texture,
        .destroy_font_atlas       = pl_cleanup_vulkan_font_texture,
        .add_3d_triangle_filled   = pl__add_3d_triangle_filled,
        .add_3d_line              = pl__add_3d_line,
        .add_3d_point             = pl__add_3d_point,
        .add_3d_transform         = pl__add_3d_transform,
        .add_3d_frustum           = pl__add_3d_frustum,
        .add_3d_centered_box      = pl__add_3d_centered_box,
        .add_3d_bezier_quad       = pl__add_3d_bezier_quad,
        .add_3d_bezier_cubic      = pl__add_3d_bezier_cubic,
        .register_3d_drawlist     = pl__register_3d_drawlist,
        .submit_3d_drawlist       = pl__submit_3d_drawlist,
        .create_shader            = pl_create_shader,
    };
    return &tApi;
}

static const plDeviceI*
pl_load_device_api(void)
{
    static const plDeviceI tApi = {
        .create_buffer         = pl_create_buffer,
        .create_texture        = pl_create_texture,
        .create_texture_view   = pl_create_texture_view,
        .create_bind_group     = pl_create_bind_group,
        .update_bind_group     = pl_update_bind_group,
        .allocate_dynamic_data = pl_allocate_dynamic_data
    };
    return &tApi;
}

PL_EXPORT void
pl_load_ext(plApiRegistryApiI* ptApiRegistry, bool bReload)
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
pl_unload_ext(plApiRegistryApiI* ptApiRegistry)
{
    
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#include "pl_ui_vulkan.c"