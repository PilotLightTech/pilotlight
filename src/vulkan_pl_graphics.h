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

#include "pl.h"

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
void                  pl_create_instance       (plVulkanGraphics* graphics, uint32_t version, bool enableValidation);
void                  pl_create_instance_ex    (plVulkanGraphics* graphics, uint32_t version, uint32_t layerCount, const char** enabledLayers, uint32_t extensioncount, const char** enabledExtensions);
void                  pl_create_frame_resources(plVulkanGraphics* graphics, plVulkanDevice* device);
void                  pl_create_device         (VkInstance instance, VkSurfaceKHR surface, plVulkanDevice* deviceOut, bool enableValidation);

// swapchain ops
void                  pl_create_swapchain      (plVulkanDevice* device, VkSurfaceKHR surface, uint32_t width, uint32_t height, plVulkanSwapchain* swapchainOut);
void                  pl_create_framebuffers   (plVulkanDevice* device, VkRenderPass renderPass, plVulkanSwapchain* swapchain);

// cleanup
void                  pl_cleanup_graphics      (plVulkanGraphics* graphics, plVulkanDevice* device);

// misc
plVulkanFrameContext* pl_get_frame_resources    (plVulkanGraphics* graphics);
uint32_t              pl_find_memory_type       (VkPhysicalDeviceMemoryProperties memProps, uint32_t typeFilter, VkMemoryPropertyFlags properties);
void                  pl_transition_image_layout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageSubresourceRange subresourceRange, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask);

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct plVulkanFrameContext_t
{
    VkSemaphore     imageAvailable;
    VkSemaphore     renderFinish;
    VkFence         inFlight;
    VkCommandBuffer cmdBuf;

} plVulkanFrameContext;

typedef struct plVulkanSwapchain_t
{
    VkSwapchainKHR swapChain;
    VkExtent2D     extent;
    VkFramebuffer* frameBuffers;
    VkFormat       format;
    VkImage*       images;
    VkImageView*   imageViews;
    uint32_t       imageCount;
    uint32_t       imageCapacity;
    uint32_t       currentImageIndex; // current image to use within the swap chain
    bool           vsync;

    VkSurfaceFormatKHR* surfaceFormats_;
    uint32_t            surfaceFormatCapacity_;

} plVulkanSwapchain;

typedef struct plVulkanDevice_t
{
    VkDevice                                  logicalDevice;
    VkPhysicalDevice                          physicalDevice;
    int                                       graphicsQueueFamily;
    int                                       presentQueueFamily;
    VkQueue                                   graphicsQueue;
    VkQueue                                   presentQueue;
    VkPhysicalDeviceProperties                deviceProps;
    VkPhysicalDeviceMemoryProperties          memProps;
    VkPhysicalDeviceMemoryProperties2         memProps2;
    VkPhysicalDeviceMemoryBudgetPropertiesEXT memBudgetInfo;
    VkDeviceSize                              maxLocalMemSize;

} plVulkanDevice;

typedef struct plVulkanGraphics_t
{
    VkInstance               instance;
    VkSurfaceKHR             surface;
    VkDebugUtilsMessengerEXT dbgMessenger;
    VkDescriptorPool         descPool;
    VkCommandPool            cmdPool;
    VkRenderPass             renderPass;
    plVulkanFrameContext*    sbFrames;
    uint32_t                 framesInFlight;  // number of frames in flight (should be less then PL_MAX_FRAMES_IN_FLIGHT)
    size_t                   currentFrameIndex; // current frame being used
} plVulkanGraphics;

#endif //PL_GRAPHICS_VULKAN_H