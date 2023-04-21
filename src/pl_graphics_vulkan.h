/*
   pl_graphics_vulkan.h
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
#include "pl_math.h"
#include "pl_graphics.h"
#include "pl_vulkan_ext.h"

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
typedef struct _plSwapchain         plSwapchain;       // swapchain resources & info
typedef struct _plDevice            plDevice;          // device resources & info
typedef struct _plGraphics          plGraphics;        // graphics context
typedef struct _plFrameContext      plFrameContext;    // per frame resource
typedef struct _plResourceManager   plResourceManager; // buffer/texture resource manager
typedef struct _plDynamicBufferNode plDynamicBufferNode;
typedef struct _plBuffer            plBuffer;          // vulkan buffer
typedef struct _plTexture           plTexture;         // vulkan texture
typedef struct _plTextureView       plTextureView;     // vulkan texture view
typedef struct _plTextureDesc       plTextureDesc;     // texture descriptor
typedef struct _plTextureViewDesc   plTextureViewDesc; // texture descriptor
typedef struct _plSampler           plSampler;
typedef struct _plBufferBinding     plBufferBinding;
typedef struct _plTextureBinding    plTextureBinding;
typedef struct _plShaderDesc        plShaderDesc;
typedef struct _plShader            plShader;
typedef struct _plGraphicsState     plGraphicsState;
typedef struct _plBindGroupLayout   plBindGroupLayout;
typedef struct _plBindGroup         plBindGroup;
typedef struct _plMesh              plMesh;
typedef struct _plDraw              plDraw;
typedef struct _plDrawArea          plDrawArea;
typedef struct _plShaderVariant     plShaderVariant; // unique combination of graphic state, renderpass, and msaa sample count

// enums
typedef int plBufferBindingType;  // -> enum _plBufferBindingType   // Enum:
typedef int plTextureBindingType; // -> enum _plTextureBindingType  // Enum:
typedef int plBufferUsage;        // -> enum _plBufferUsage         // Enum:
typedef int plMeshFormatFlags;    // -> enum _plMeshFormatFlags     // Flags:
typedef int plShaderTextureFlags; // -> enum _plShaderTextureFlags  // Flags:
typedef int plBlendMode;          // -> enum _plBlendMode           // Enum:
typedef int plDepthMode;          // -> enum _plDepthMode           // Enum:
typedef int plStencilMode;        // -> enum _plStencilMode         // Enum:
typedef int plFilter;             // -> enum _plFilter              // Enum:
typedef int plWrapMode;           // -> enum _plWrapMode            // Enum:
typedef int plCompareMode;        // -> enum _plCompareMode         // Enum:

// external
typedef struct _plApiRegistryApiI          plApiRegistryApiI;
typedef struct _plIOApiI                   plIOApiI;
typedef struct _plFileApiI                 plFileApiI;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// setup
void                  pl_setup_graphics               (plGraphics* ptGraphics, plApiRegistryApiI* ptApiRegistry);
void                  pl_cleanup_graphics             (plGraphics* ptGraphics);
void                  pl_resize_graphics              (plGraphics* ptGraphics);

// per frame
bool                  pl_begin_frame                  (plGraphics* ptGraphics);
void                  pl_end_frame                    (plGraphics* ptGraphics);
void                  pl_begin_recording              (plGraphics* ptGraphics);
void                  pl_end_recording                (plGraphics* ptGraphics);

// resource manager per frame
void                  pl_process_cleanup_queue        (plResourceManager* ptResourceManager, uint32_t uFramesToProcess);

// resource manager commited resources
uint32_t              pl_create_index_buffer          (plResourceManager* ptResourceManager, size_t szSize, const void* pData, const char* pcName);
uint32_t              pl_create_vertex_buffer         (plResourceManager* ptResourceManager, size_t szSize, size_t szStride, const void* pData, const char* pcName);
uint32_t              pl_create_constant_buffer       (plResourceManager* ptResourceManager, size_t szSize, const char* pcName);
uint32_t              pl_create_texture               (plResourceManager* ptResourceManager, plTextureDesc tDesc, size_t szSize, const void* pData, const char* pcName);
uint32_t              pl_create_storage_buffer        (plResourceManager* ptResourceManager, size_t szSize, const void* pData, const char* pcName);
uint32_t              pl_create_texture_view          (plResourceManager* ptResourceManager, const plTextureViewDesc* ptViewDesc, const plSampler* ptSampler, uint32_t uTextureHandle, const char* pcName);

// resource manager dynamic buffers
uint32_t              pl_request_dynamic_buffer         (plResourceManager* ptResourceManager);
void                  pl_return_dynamic_buffer          (plResourceManager* ptResourceManager, uint32_t uNodeIndex);

// resource manager misc.
void                  pl_transfer_data_to_image          (plResourceManager* ptResourceManager, plTexture* ptDest, size_t szDataSize, const void* pData);
void                  pl_transfer_data_to_buffer         (plResourceManager* ptResourceManager, VkBuffer tDest, size_t szSize, const void* pData);
void                  pl_submit_buffer_for_deletion      (plResourceManager* ptResourceManager, uint32_t uBufferIndex);
void                  pl_submit_texture_for_deletion     (plResourceManager* ptResourceManager, uint32_t uTextureIndex);
void                  pl_submit_texture_view_for_deletion(plResourceManager* ptResourceManager, uint32_t uTextureViewIndex);

// command buffers
VkCommandBuffer       pl_begin_command_buffer         (plGraphics* ptGraphics);
void                  pl_submit_command_buffer        (plGraphics* ptGraphics, VkCommandBuffer tCmdBuffer);

// shaders
uint32_t              pl_create_shader             (plResourceManager* ptResourceManager, const plShaderDesc* ptDesc);
uint32_t              pl_add_shader_variant        (plResourceManager* ptResourceManager, uint32_t uShader, plGraphicsState tVariant, VkRenderPass ptRenderPass, VkSampleCountFlagBits tMSAASampleCount);
bool                  pl_shader_variant_exist      (plResourceManager* ptResourceManager, uint32_t uShader, plGraphicsState tVariant, VkRenderPass ptRenderPass, VkSampleCountFlagBits tMSAASampleCount);
void                  pl_submit_shader_for_deletion(plResourceManager* ptResourceManager, uint32_t uShaderIndex);
plBindGroupLayout*    pl_get_bind_group_layout     (plResourceManager* ptResourceManager, uint32_t uShaderIndex, uint32_t uBindGroupIndex);
plShaderVariant*      pl_get_shader                (plResourceManager* ptResourceManager, uint32_t uVariantIndex);

// descriptors
void                  pl_update_bind_group            (plGraphics* ptGraphics, plBindGroup* ptGroup, uint32_t uBufferCount, uint32_t* auBuffers, size_t* aszBufferRanges, uint32_t uTextureViewCount, uint32_t* auTextureViews);

// drawing
void                  pl_draw_areas                   (plGraphics* ptGraphics, uint32_t uAreaCount, plDrawArea* atAreas, plDraw* atDraws);

// misc
plFrameContext*       pl_get_frame_resources          (plGraphics* ptGraphics);
uint32_t              pl_find_memory_type             (VkPhysicalDeviceMemoryProperties tMemProps, uint32_t uTypeFilter, VkMemoryPropertyFlags tProperties);
void                  pl_transition_image_layout      (VkCommandBuffer tCommandBuffer, VkImage tImage, VkImageLayout tOldLayout, VkImageLayout tNewLayout, VkImageSubresourceRange tSubresourceRange, VkPipelineStageFlags tSrcStageMask, VkPipelineStageFlags tDstStageMask);
VkFormat              pl_find_supported_format        (plDevice* ptDevice, VkFormatFeatureFlags tFlags, const VkFormat* ptFormats, uint32_t uFormatCount);
VkFormat              pl_find_depth_format            (plDevice* ptDevice);
VkFormat              pl_find_depth_stencil_format    (plDevice* ptDevice);
bool                  pl_format_has_stencil           (VkFormat tFormat);
VkSampleCountFlagBits pl_get_max_sample_count         (plDevice* ptDevice);
void                  pl_set_vulkan_object_name       (plGraphics* ptGraphics, uint64_t uObjectHandle, VkDebugReportObjectTypeEXT tObjectType, const char* pcName);

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plDrawArea
{
    VkViewport   tViewport;
    VkRect2D     tScissor;
    plBindGroup* ptBindGroup0;
    uint32_t     uDynamicBufferOffset0;
    uint32_t     uDrawOffset;
    uint32_t     uDrawCount;
} plDrawArea;

typedef struct _plDraw
{
    plMesh*      ptMesh;
    plBindGroup* ptBindGroup1;
    plBindGroup* ptBindGroup2;
    uint32_t     uShaderVariant;
    uint32_t     uDynamicBufferOffset1;
    uint32_t     uDynamicBufferOffset2;
} plDraw;

typedef struct _plTextureViewDesc
{
    VkFormat    tFormat; 
    uint32_t    uBaseMip;
    uint32_t    uMips;
    uint32_t    uBaseLayer;
    uint32_t    uLayerCount;
    uint32_t    uSlot;  
} plTextureViewDesc;

typedef struct _plTextureView
{
    uint32_t          uTextureHandle;
    plSampler         tSampler;
    plTextureViewDesc tTextureViewDesc;
    VkSampler         _tSampler;
    VkImageView       _tImageView;
} plTextureView;

typedef struct _plTextureDesc
{
    VkImageType          tType;
    plVec3               tDimensions;
    uint32_t             uLayers;
    uint32_t             uMips;
    VkFormat             tFormat;
    VkImageUsageFlagBits tUsage;
} plTextureDesc;

typedef struct _plTexture
{
    plTextureDesc  tDesc;
    VkImage        tImage;
    VkDeviceMemory tMemory;
} plTexture;

typedef struct _plBuffer
{
    plBufferUsage  tUsage;
    size_t         szRequestedSize;
    size_t         szSize;
    size_t         szStride;
    size_t         szItemCount;
    VkBuffer       tBuffer;
    VkDeviceMemory tBufferMemory;
    unsigned char* pucMapping;
} plBuffer;

typedef struct _plBufferBinding
{
    plBufferBindingType tType;
    uint32_t            uSlot;
    VkShaderStageFlags  tStageFlags;
    size_t              szSize;
    size_t              szOffset;
    plBuffer            tBuffer;
} plBufferBinding;

typedef struct _plTextureBinding
{
    plTextureBindingType tType;
    uint32_t             uSlot;
    VkShaderStageFlags   tStageFlags;
    plTexture            tTexture;
    VkSampler            tSampler;
} plTextureBinding;

typedef struct _plBindGroupLayout
{
    uint32_t              uTextureCount;
    uint32_t              uBufferCount;
    plTextureBinding      aTextures[64];
    plBufferBinding       aBuffers[64];
    VkDescriptorSetLayout _tDescriptorSetLayout;
} plBindGroupLayout;

typedef struct _plBindGroup
{
    plBindGroupLayout tLayout;
    VkDescriptorSet   _tDescriptorSet;
} plBindGroup;

typedef struct _plShaderVariant
{
    VkPipelineLayout      tPipelineLayout;
    VkRenderPass          tRenderPass;
    plGraphicsState       tGraphicsState;
    VkSampleCountFlagBits tMSAASampleCount;
    VkPipeline            tPipeline;
} plShaderVariant;

typedef struct _plShaderDesc
{
    plGraphicsState    tGraphicsState;
    const char*        pcVertexShader;
    const char*        pcPixelShader;
    plBindGroupLayout  atBindGroupLayouts[4];
    uint32_t           uBindGroupLayoutCount;
    plShaderVariant*   sbtVariants;
} plShaderDesc;

typedef struct _plShader
{
    plShaderDesc             tDesc;
    VkPipelineLayout         tPipelineLayout;
    uint32_t*                _sbuVariantPipelines;
    VkShaderModuleCreateInfo tVertexShaderInfo;
    VkShaderModuleCreateInfo tPixelShaderInfo;
} plShader;

typedef struct _plDynamicBufferNode
{
    uint32_t    uPrev;
    uint32_t    uNext;
    uint32_t    uDynamicBuffer;
    uint32_t    uDynamicBufferOffset;
    uint32_t    uLastActiveFrame;
} plDynamicBufferNode;

typedef struct _plResourceManager
{

    plBuffer*        sbtBuffers;
    plTexture*       sbtTextures;
    plTextureView*   sbtTextureViews;
    plShader*        sbtShaders;
    plShaderVariant* sbtShaderVariants; // from all different shaders

    // [INTERNAL]
    
    uint32_t* _sbulTempQueue;

    // shaders
    uint32_t* _sbulShaderHashes;
    uint32_t* _sbulShaderFreeIndices;
    uint32_t* _sbulShaderDeletionQueue; 
    
    // buffers
    uint32_t* _sbulBufferFreeIndices;
    uint32_t* _sbulBufferDeletionQueue;

    // textures
    uint32_t* _sbulTextureFreeIndices;
    uint32_t* _sbulTextureDeletionQueue;

    // texture views
    uint32_t* _sbulTextureViewFreeIndices;
    uint32_t* _sbulTextureViewDeletionQueue;

    // cached
    plGraphics*              _ptGraphics;
    plDevice*                _ptDevice;
    plDescriptorManagerApiI* _ptDescriptorApi;
    plDescriptorManager      _tDescriptorManager;
    
    // staging buffer
    size_t         _szStagingBufferSize;
    VkBuffer       _tStagingBuffer;
    VkDeviceMemory _tStagingBufferMemory;
    unsigned char* _pucMapping;

    // dynamic buffers
    uint32_t             _uDynamicBufferSize;
    uint32_t*            _sbuDynamicBufferDeletionQueue;
    plDynamicBufferNode* _sbtDynamicBufferList;

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
    VkPhysicalDeviceFeatures                  tDeviceFeatures;
    bool                                      bSwapchainExtPresent;
    bool                                      bPortabilitySubsetPresent;

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
    uint32_t                 uLogChannel;

    // apis
    plIOApiI*   ptIoInterface;
    plFileApiI* ptFileApi;
    plDeviceMemoryAllocatorI tLocalAllocator;
    plDeviceMemoryAllocatorI tStagingUnCachedAllocator;

	PFN_vkDebugMarkerSetObjectTagEXT  vkDebugMarkerSetObjectTag;
	PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectName;
	PFN_vkCmdDebugMarkerBeginEXT      vkCmdDebugMarkerBegin;
	PFN_vkCmdDebugMarkerEndEXT        vkCmdDebugMarkerEnd;
	PFN_vkCmdDebugMarkerInsertEXT     vkCmdDebugMarkerInsert;
} plGraphics;

#endif //PL_GRAPHICS_VULKAN_H