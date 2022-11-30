/*
   vulkan_pl_graphics.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] enums
// [SECTION] structs
*/

#ifndef PL_GRAPHICS_VULKAN_H
#define PL_GRAPHICS_VULKAN_H

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#include "pilotlight.h"

#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__APPLE__)
#define VK_USE_PLATFORM_METAL_EXT
#else // linux
#define VK_USE_PLATFORM_XCB_KHR
#endif
#include "vulkan/vulkan.h"

#ifndef PL_VULKAN
#include <assert.h>
#define PL_VULKAN(x) assert(x == VK_SUCCESS)
#endif

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
PL_DECLARE_STRUCT(plVulkanSwapchain);       // swapchain resources & info
PL_DECLARE_STRUCT(plVulkanDevice);          // device resources & info
PL_DECLARE_STRUCT(plVulkanGraphics);        // graphics context
PL_DECLARE_STRUCT(plVulkanFrameContext);    // per frame resource
PL_DECLARE_STRUCT(plVulkanResourceManager); // buffer/texture resource manager
PL_DECLARE_STRUCT(plVulkanBuffer);          // vulkan buffer

// enums
typedef int plBufferUsage;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// setup
void                  pl_setup_graphics            (plVulkanGraphics* ptGraphics);
void                  pl_cleanup_graphics          (plVulkanGraphics* ptGraphics);
void                  pl_resize_graphics           (plVulkanGraphics* ptGraphics);

// per frame
bool                  pl_begin_frame               (plVulkanGraphics* ptGraphics);
void                  pl_end_frame                 (plVulkanGraphics* ptGraphics);
void                  pl_begin_recording           (plVulkanGraphics* ptGraphics);
void                  pl_end_recording             (plVulkanGraphics* ptGraphics);
void                  pl_begin_main_pass           (plVulkanGraphics* ptGraphics);
void                  pl_end_main_pass             (plVulkanGraphics* ptGraphics);

// resource manager per frame
void                  pl_process_cleanup_queue     (plVulkanResourceManager* ptResourceManager, uint32_t uFramesToProcess);

// resource manager commited resources
uint64_t              pl_create_index_buffer       (plVulkanResourceManager* ptResourceManager, size_t szSize, const void* pData);
uint64_t              pl_create_vertex_buffer      (plVulkanResourceManager* ptResourceManager, size_t szSize, size_t szStride, const void* pData);
uint64_t              pl_create_constant_buffer    (plVulkanResourceManager* ptResourceManager, size_t szItemSize, size_t szItemCount);

// resource manager misc.
void                  pl_transfer_data_to_buffer   (plVulkanResourceManager* ptResourceManager, VkBuffer tDest, size_t szSize, const void* pData);
void                  pl_submit_buffer_for_deletion(plVulkanResourceManager* ptResourceManager, uint64_t ulBufferIndex);

// command buffers
VkCommandBuffer       pl_begin_command_buffer      (plVulkanGraphics* ptGraphics, plVulkanDevice* ptDevice);
void                  pl_submit_command_buffer     (plVulkanGraphics* ptGraphics, plVulkanDevice* ptDevice, VkCommandBuffer tCmdBuffer);

// misc
plVulkanFrameContext* pl_get_frame_resources       (plVulkanGraphics* ptGraphics);
uint32_t              pl_find_memory_type          (VkPhysicalDeviceMemoryProperties tMemProps, uint32_t uTypeFilter, VkMemoryPropertyFlags tProperties);
void                  pl_transition_image_layout   (VkCommandBuffer tCommandBuffer, VkImage tImage, VkImageLayout tOldLayout, VkImageLayout tNewLayout, VkImageSubresourceRange tSubresourceRange, VkPipelineStageFlags tSrcStageMask, VkPipelineStageFlags tDstStageMask);
VkFormat              pl_find_supported_format     (plVulkanDevice* ptDevice, VkFormatFeatureFlags tFlags, const VkFormat* ptFormats, uint32_t uFormatCount);
VkFormat              pl_find_depth_format         (plVulkanDevice* ptDevice);
bool                  pl_format_has_stencil        (VkFormat tFormat);
VkSampleCountFlagBits pl_get_max_sample_count      (plVulkanDevice* ptDevice);

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum plBufferUsage_
{
    PL_BUFFER_USAGE_UNSPECIFIED,
    PL_BUFFER_USAGE_INDEX,
    PL_BUFFER_USAGE_VERTEX,
    PL_BUFFER_USAGE_CONSTANT
};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plVulkanBuffer
{
    plBufferUsage  tUsage;
    size_t         szRequestedSize;
    size_t         szSize;
    size_t         szStride;
    VkBuffer       tBuffer;
    VkDeviceMemory tBufferMemory;
    unsigned char* pucMapping;
} plVulkanBuffer;

typedef struct _plVulkanResourceManager
{

    plVulkanBuffer*   sbtBuffers;

    // [INTERNAL]
    uint64_t*         _sbulFreeIndices;
    uint64_t*         _sbulDeletionQueue;
    uint64_t*         _sbulTempQueue;

    // cached
    plVulkanGraphics* _ptGraphics;
    plVulkanDevice*   _ptDevice;
    
    // staging buffer
    size_t            _szStagingBufferSize;
    VkBuffer          _tStagingBuffer;
    VkDeviceMemory    _tStagingBufferMemory;
    unsigned char*    _pucMapping;

} plVulkanResourceManager;

typedef struct _plVulkanFrameContext
{
    VkSemaphore     tImageAvailable;
    VkSemaphore     tRenderFinish;
    VkFence         tInFlight;
    VkCommandBuffer tCmdBuf;

} plVulkanFrameContext;

typedef struct _plVulkanSwapchain
{
    VkSwapchainKHR        tSwapChain;
    VkExtent2D            tExtent;
    VkFramebuffer*        ptFrameBuffers;
    VkFormat              tFormat;
    VkFormat              tDepthFormat;
    VkImage*              ptImages;
    VkImageView*          ptImageViews;
    VkImage               tColorImage;
    VkImageView           tColorImageView;
    VkDeviceMemory        tColorMemory;
    VkImage               tDepthImage;
    VkImageView           tDepthImageView;
    VkDeviceMemory        tDepthMemory;
    uint32_t              uImageCount;
    uint32_t              uImageCapacity;
    uint32_t              uCurrentImageIndex; // current image to use within the swap chain
    bool                  bVSync;
    VkSampleCountFlagBits tMsaaSamples;
    VkSurfaceFormatKHR*   ptSurfaceFormats_;
    uint32_t              uSurfaceFormatCapacity_;

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

} plVulkanDevice;

typedef struct _plVulkanGraphics
{
    VkInstance               tInstance;
    VkSurfaceKHR             tSurface;
    VkDebugUtilsMessengerEXT tDbgMessenger;
    plVulkanDevice           tDevice;
    plVulkanSwapchain        tSwapchain;
    VkDescriptorPool         tDescriptorPool;
    plVulkanResourceManager  tResourceManager;
    VkCommandPool            tCmdPool;
    VkRenderPass             tRenderPass;
    plVulkanFrameContext*    sbFrames;
    uint32_t                 uFramesInFlight;  // number of frames in flight (should be less then PL_MAX_FRAMES_IN_FLIGHT)
    size_t                   szCurrentFrameIndex; // current frame being used
} plVulkanGraphics;

#endif //PL_GRAPHICS_VULKAN_H