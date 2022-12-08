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
#include "pl_math.inc"

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
PL_DECLARE_STRUCT(plSwapchain);       // swapchain resources & info
PL_DECLARE_STRUCT(plDevice);          // device resources & info
PL_DECLARE_STRUCT(plGraphics);        // graphics context
PL_DECLARE_STRUCT(plFrameContext);    // per frame resource
PL_DECLARE_STRUCT(plResourceManager); // buffer/texture resource manager
PL_DECLARE_STRUCT(plBuffer);          // vulkan buffer
PL_DECLARE_STRUCT(plTexture);         // vulkan texture
PL_DECLARE_STRUCT(plTextureDesc);     // texture descriptor

// enums
typedef int plBufferUsage;
typedef int plBlendMode;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// setup
void                  pl_setup_graphics            (plGraphics* ptGraphics);
void                  pl_cleanup_graphics          (plGraphics* ptGraphics);
void                  pl_resize_graphics           (plGraphics* ptGraphics);

// per frame
bool                  pl_begin_frame               (plGraphics* ptGraphics);
void                  pl_end_frame                 (plGraphics* ptGraphics);
void                  pl_begin_recording           (plGraphics* ptGraphics);
void                  pl_end_recording             (plGraphics* ptGraphics);
void                  pl_begin_main_pass           (plGraphics* ptGraphics);
void                  pl_end_main_pass             (plGraphics* ptGraphics);

// resource manager per frame
void                  pl_process_cleanup_queue     (plResourceManager* ptResourceManager, uint32_t uFramesToProcess);

// resource manager commited resources
uint32_t              pl_create_index_buffer       (plResourceManager* ptResourceManager, size_t szSize, const void* pData);
uint32_t              pl_create_vertex_buffer      (plResourceManager* ptResourceManager, size_t szSize, size_t szStride, const void* pData);
uint32_t              pl_create_constant_buffer    (plResourceManager* ptResourceManager, size_t szItemSize, size_t szItemCount);
uint32_t              pl_create_texture            (plResourceManager* ptResourceManager, plTextureDesc tDesc, size_t szSize, const void* pData);
uint32_t              pl_create_storage_buffer     (plResourceManager* ptResourceManager, size_t szSize, const void* pData);

// resource manager misc.
void                  pl_transfer_data_to_image     (plResourceManager* ptResourceManager, plTexture* ptDest, size_t szDataSize, const void* pData);
void                  pl_transfer_data_to_buffer    (plResourceManager* ptResourceManager, VkBuffer tDest, size_t szSize, const void* pData);
void                  pl_submit_buffer_for_deletion (plResourceManager* ptResourceManager, uint32_t ulBufferIndex);
void                  pl_submit_texture_for_deletion(plResourceManager* ptResourceManager, uint32_t ulTextureIndex);

// command buffers
VkCommandBuffer       pl_begin_command_buffer      (plGraphics* ptGraphics, plDevice* ptDevice);
void                  pl_submit_command_buffer     (plGraphics* ptGraphics, plDevice* ptDevice, VkCommandBuffer tCmdBuffer);

// misc
plFrameContext*       pl_get_frame_resources       (plGraphics* ptGraphics);
uint32_t              pl_find_memory_type          (VkPhysicalDeviceMemoryProperties tMemProps, uint32_t uTypeFilter, VkMemoryPropertyFlags tProperties);
void                  pl_transition_image_layout   (VkCommandBuffer tCommandBuffer, VkImage tImage, VkImageLayout tOldLayout, VkImageLayout tNewLayout, VkImageSubresourceRange tSubresourceRange, VkPipelineStageFlags tSrcStageMask, VkPipelineStageFlags tDstStageMask);
VkFormat              pl_find_supported_format     (plDevice* ptDevice, VkFormatFeatureFlags tFlags, const VkFormat* ptFormats, uint32_t uFormatCount);
VkFormat              pl_find_depth_format         (plDevice* ptDevice);
bool                  pl_format_has_stencil        (VkFormat tFormat);
VkSampleCountFlagBits pl_get_max_sample_count      (plDevice* ptDevice);

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum plBufferUsage_
{
    PL_BUFFER_USAGE_UNSPECIFIED,
    PL_BUFFER_USAGE_INDEX,
    PL_BUFFER_USAGE_VERTEX,
    PL_BUFFER_USAGE_CONSTANT,
    PL_BUFFER_USAGE_STORAGE
};

enum plBlendMode_
{
    PL_BLEND_MODE_NONE,
    PL_BLEND_MODE_ALPHA,
    PL_BLEND_MODE_ADDITIVE,
    PL_BLEND_MODE_PREMULTIPLY,
    PL_BLEND_MODE_MULTIPLY,
    PL_BLEND_MODE_CLIP_MASK,
    
    PL_BLEND_MODE_COUNT
};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plTextureDesc
{
    VkImageViewType      tType;
    plVec3               tDimensions;
    uint32_t             uLayers;
    uint32_t             uMips;
    VkFormat             tFormat;
    VkImageUsageFlagBits tUsage;
} plTextureDesc;

typedef struct _plTexture
{
    plTextureDesc        tDesc;
    VkSampler            tSampler;
    VkImage              tImage;
    VkImageView          tImageView;
    VkDeviceMemory       tMemory;
} plTexture;

typedef struct _plBuffer
{
    plBufferUsage  tUsage;
    size_t         szRequestedSize;
    size_t         szSize;
    size_t         szStride;
    VkBuffer       tBuffer;
    VkDeviceMemory tBufferMemory;
    unsigned char* pucMapping;
} plBuffer;

typedef struct _plResourceManager
{

    plBuffer*  sbtBuffers;
    plTexture* sbtTextures;

    // [INTERNAL]
    uint32_t* _sbulBufferFreeIndices;
    uint32_t* _sbulBufferDeletionQueue;
    uint32_t* _sbulTextureFreeIndices;
    uint32_t* _sbulTextureDeletionQueue;
    uint32_t* _sbulTempQueue;

    // cached
    plGraphics* _ptGraphics;
    plDevice*   _ptDevice;
    
    // staging buffer
    size_t            _szStagingBufferSize;
    VkBuffer          _tStagingBuffer;
    VkDeviceMemory    _tStagingBufferMemory;
    unsigned char*    _pucMapping;

} plResourceManager;

typedef struct _plFrameContext
{
    VkSemaphore     tImageAvailable;
    VkSemaphore     tRenderFinish;
    VkFence         tInFlight;
    VkCommandBuffer tCmdBuf;

} plFrameContext;

typedef struct _plSwapchain
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

} plSwapchain;

typedef struct _plDevice
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

} plDevice;

typedef struct _plGraphics
{
    VkInstance               tInstance;
    VkSurfaceKHR             tSurface;
    VkDebugUtilsMessengerEXT tDbgMessenger;
    plDevice                 tDevice;
    plSwapchain              tSwapchain;
    VkDescriptorPool         tDescriptorPool;
    plResourceManager        tResourceManager;
    VkCommandPool            tCmdPool;
    VkRenderPass             tRenderPass;
    plFrameContext*          sbFrames;
    uint32_t                 uFramesInFlight;  // number of frames in flight (should be less then PL_MAX_FRAMES_IN_FLIGHT)
    size_t                   szCurrentFrameIndex; // current frame being used
} plGraphics;

#endif //PL_GRAPHICS_VULKAN_H