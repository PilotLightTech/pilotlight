/*
   vulkan_pl_graphics.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] forward declarations
// [SECTION] public api
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

PL_DECLARE_STRUCT(plVulkanSwapchain);    // swapchain resources & info
PL_DECLARE_STRUCT(plVulkanDevice);       // device resources & info
PL_DECLARE_STRUCT(plVulkanGraphics);     // graphics context
PL_DECLARE_STRUCT(plVulkanFrameContext); // per frame resource

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// setup
void                  pl_create_instance       (plVulkanGraphics* ptGraphics, uint32_t uVersion, bool bEnableValidation);
void                  pl_create_instance_ex    (plVulkanGraphics* ptGraphics, uint32_t uVersion, uint32_t uLayerCount, const char** ppcEnabledLayers, uint32_t uExtensioncount, const char** ppcEnabledExtensions);
void                  pl_create_frame_resources(plVulkanGraphics* ptGraphics, plVulkanDevice* ptDevice);
void                  pl_create_device         (VkInstance tInstance, VkSurfaceKHR tSurface, plVulkanDevice* ptDeviceOut, bool bEnableValidation);

// swapchain ops
void                  pl_create_swapchain      (plVulkanDevice* ptDevice, VkSurfaceKHR tSurface, uint32_t uWidth, uint32_t uHeight, plVulkanSwapchain* ptSwapchainOut);
void                  pl_create_framebuffers   (plVulkanDevice* ptDevice, VkRenderPass tRenderPass, plVulkanSwapchain* ptSwapchain);

// cleanup
void                  pl_cleanup_graphics      (plVulkanGraphics* ptGraphics, plVulkanDevice* ptDevice);

// misc
plVulkanFrameContext* pl_get_frame_resources    (plVulkanGraphics* ptGraphics);
uint32_t              pl_find_memory_type       (VkPhysicalDeviceMemoryProperties tMemProps, uint32_t uTypeFilter, VkMemoryPropertyFlags tProperties);
void                  pl_transition_image_layout(VkCommandBuffer tCommandBuffer, VkImage tImage, VkImageLayout tOldLayout, VkImageLayout tNewLayout, VkImageSubresourceRange tSubresourceRange, VkPipelineStageFlags tSrcStageMask, VkPipelineStageFlags tDstStageMask);

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plVulkanFrameContext
{
    VkSemaphore     tImageAvailable;
    VkSemaphore     tRenderFinish;
    VkFence         tInFlight;
    VkCommandBuffer tCmdBuf;

} plVulkanFrameContext;

typedef struct _plVulkanSwapchain
{
    VkSwapchainKHR tSwapChain;
    VkExtent2D     tExtent;
    VkFramebuffer* ptFrameBuffers;
    VkFormat       tFormat;
    VkImage*       ptImages;
    VkImageView*   ptImageViews;
    uint32_t       uImageCount;
    uint32_t       uImageCapacity;
    uint32_t       uCurrentImageIndex; // current image to use within the swap chain
    bool           bVSync;

    VkSurfaceFormatKHR* ptSurfaceFormats_;
    uint32_t            uSurfaceFormatCapacity_;

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
    VkCommandPool            tCmdPool;
    VkRenderPass             tRenderPass;
    plVulkanFrameContext*    sbFrames;
    uint32_t                 uFramesInFlight;  // number of frames in flight (should be less then PL_MAX_FRAMES_IN_FLIGHT)
    size_t                   szCurrentFrameIndex; // current frame being used
} plVulkanGraphics;

#endif //PL_GRAPHICS_VULKAN_H