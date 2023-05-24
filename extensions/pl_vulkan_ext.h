/*
   pl_vulkan_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] includes
// [SECTION] defines
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] public api structs
// [SECTION] structs
// [SECTION] enums
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_VULKAN_EXT_H
#define PL_VULKAN_EXT_H

#if defined(_WIN32)
    #define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__APPLE__)
    #define VK_USE_PLATFORM_METAL_EXT
#else // linux
    #define VK_USE_PLATFORM_XCB_KHR
#endif

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define PL_API_BACKEND_VULKAN "PL_API_BACKEND_VULKAN"
typedef struct _plRenderBackendI plRenderBackendI;

#define PL_API_GRAPHICS "PL_API_GRAPHICS"
typedef struct _plGraphicsApiI plGraphicsApiI;

#define PL_API_DEVICE "PL_API_DEVICE"
typedef struct _plDeviceApiI plDeviceApiI;

#define PL_API_DESCRIPTOR_MANAGER "PL_API_DESCRIPTOR_MANAGER"
typedef struct _plDescriptorManagerApiI plDescriptorManagerApiI;

#define PL_API_DEVICE_MEMORY "PL_API_DEVICE_MEMORY"
typedef struct _plDeviceMemoryApiI plDeviceMemoryApiI;

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include "pl_graphics.inl"
#include "pl_math.h"
#include "vulkan/vulkan.h"

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_VULKAN
    #include <assert.h>
    #define PL_VULKAN(x) assert(x == VK_SUCCESS)
#endif

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// types
typedef struct _plDevice                 plDevice; // device resources & info
typedef struct _plDescriptorManager      plDescriptorManager;
typedef struct _plDeviceMemoryAllocatorI plDeviceMemoryAllocatorI;

// basic types
typedef struct _plRenderBackend     plRenderBackend;
typedef struct _plSwapchain         plSwapchain;       // swapchain resources & info
typedef struct _plDevice            plDevice;          // device resources & info
typedef struct _plGraphics          plGraphics;        // graphics context
typedef struct _plFrameContext      plFrameContext;    // per frame resource
typedef struct _plFrameGarbage      plFrameGarbage;
typedef struct _plResourceManager   plResourceManager; // buffer/texture resource manager
typedef struct _plDynamicBufferNode plDynamicBufferNode;
typedef struct _plBuffer            plBuffer;          // vulkan buffer
typedef struct _plTexture           plTexture;         // vulkan texture
typedef struct _plTextureView       plTextureView;     // vulkan texture view
typedef struct _plTextureDesc       plTextureDesc;     // texture descriptor
typedef struct _plBufferBinding     plBufferBinding;
typedef struct _plTextureBinding    plTextureBinding;
typedef struct _plShaderDesc        plShaderDesc;
typedef struct _plShader            plShader;
typedef struct _plBindGroupLayout   plBindGroupLayout;
typedef struct _plBindGroup         plBindGroup;
typedef struct _plDraw              plDraw;
typedef struct _plDrawArea          plDrawArea;
typedef struct _plFrameBuffer       plFrameBuffer;
typedef struct _plFrameBufferDesc   plFrameBufferDesc;
typedef struct _plRenderPass        plRenderPass;
typedef struct _plShaderVariant     plShaderVariant; // unique combination of graphic state, renderpass, and msaa sample count

// external types
typedef struct _plTempAllocator plTempAllocator;  // pl_memory.h

// external apis (pilotlight.h)
typedef struct _plApiRegistryApiI        plApiRegistryApiI;   // pilotlight.h
typedef struct _plMemoryApiI             plMemoryApiI;
typedef struct _plIOApiI                 plIOApiI;
typedef struct _plFileApiI               plFileApiI;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

plGraphicsApiI*          pl_load_graphics_api          (void);
plDescriptorManagerApiI* pl_load_descriptor_manager_api(void);
plDeviceApiI*            pl_load_device_api            (void);
plRenderBackendI*        pl_load_render_backend_api    (void);
plDeviceMemoryApiI*      pl_load_device_memory_api     (void);

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plDeviceApiI
{

    void (*init)    (plApiRegistryApiI* ptApiRegistry, plDevice* ptDevice, uint32_t uFramesInFlight);

    // command buffers
    VkCommandBuffer (*begin_command_buffer)  (plDevice* ptDevice, VkCommandPool tCmdPool);
    void            (*submit_command_buffer) (plDevice* ptDevice, VkCommandPool tCmdPool, VkCommandBuffer tCmdBuffer);

    bool                  (*format_has_stencil )      (plFormat tFormat);
    VkSampleCountFlagBits (*get_max_sample_count)     (plDevice* ptDevice);
    plFormat              (*find_supported_format)    (plDevice* ptDevice, VkFormatFeatureFlags tFlags, const plFormat* ptFormats, uint32_t uFormatCount);
    plFormat              (*find_depth_format)        (plDevice* ptDevice);
    plFormat              (*find_depth_stencil_format)(plDevice* ptDevice);
    VkFormat              (*vulkan_format)            (plFormat tFormat);

    void     (*set_vulkan_object_name)  (plDevice* ptDevice, uint64_t uObjectHandle, VkDebugReportObjectTypeEXT tObjectType, const char* pcName);
    uint32_t (*find_memory_type)        (VkPhysicalDeviceMemoryProperties tMemProps, uint32_t uTypeFilter, VkMemoryPropertyFlags tProperties);
    void     (*transition_image_layout) (VkCommandBuffer tCommandBuffer, VkImage tImage, VkImageLayout tOldLayout, VkImageLayout tNewLayout, VkImageSubresourceRange tSubresourceRange, VkPipelineStageFlags tSrcStageMask, VkPipelineStageFlags tDstStageMask);

    // resource management

    // per frame
    void (*process_cleanup_queue) (plDevice* ptDevice, uint32_t uCurrentFrame);

    // commited resources
    uint32_t (*create_index_buffer)    (plDevice* ptDevice, size_t szSize, const void* pData, const char* pcName);
    uint32_t (*create_vertex_buffer)   (plDevice* ptDevice, size_t szSize, size_t szStride, const void* pData, const char* pcName);
    uint32_t (*create_constant_buffer) (plDevice* ptDevice, size_t szSize, const char* pcName);
    uint32_t (*create_read_back_buffer)(plDevice* ptDevice, size_t szSize, const char* pcName);
    uint32_t (*create_staging_buffer)  (plDevice* ptDevice, size_t szSize, const char* pcName);
    uint32_t (*create_texture)         (plDevice* ptDevice, plTextureDesc tDesc, size_t szSize, const void* pData, const char* pcName);
    uint32_t (*create_storage_buffer)  (plDevice* ptDevice, size_t szSize, const void* pData, const char* pcName);
    uint32_t (*create_texture_view)    (plDevice* ptDevice, const plTextureViewDesc* ptViewDesc, const plSampler* ptSampler, uint32_t uTextureHandle, const char* pcName);
    uint32_t (*create_frame_buffer)    (plDevice* ptDevice, plFrameBufferDesc* ptDesc, const char* pcName);
    uint32_t (*create_render_pass)     (plDevice* ptDevice, plRenderPassDesc* ptDesc, const char* pcName);

    // resource manager dynamic buffers
    uint32_t (*request_dynamic_buffer) (plDevice* ptDevice);
    void     (*return_dynamic_buffer)  (plDevice* ptDevice, uint32_t uNodeIndex);

    // resource manager misc.
    void (*transfer_data_to_image)           (plDevice* ptDevice, plTexture* ptDest, size_t szDataSize, const void* pData);
    void (*transfer_data_to_buffer)          (plDevice* ptDevice, VkBuffer tDest, size_t szSize, const void* pData);
    void (*transfer_image_to_buffer)         (plDevice* ptDevice, VkBuffer tDest, size_t szSize, plTexture* ptDest);
    void (*submit_buffer_for_deletion)       (plDevice* ptDevice, uint32_t uBufferIndex);
    void (*submit_texture_for_deletion)      (plDevice* ptDevice, uint32_t uTextureIndex);
    void (*submit_texture_view_for_deletion) (plDevice* ptDevice, uint32_t uTextureViewIndex);
    void (*submit_frame_buffer_for_deletion) (plDevice* ptDevice, uint32_t uFrameBufferIndex);
    void (*submit_render_pass_for_deletion)  (plDevice* ptDevice, uint32_t uRenderPassIndex);
} plDeviceApiI;

typedef struct _plRenderBackendI
{
    // main
    void (*setup)  (plApiRegistryApiI* ptApiRegistry, plRenderBackend* ptBackend, uint32_t uVersion, bool bEnableValidation);
    void (*cleanup)(plApiRegistryApiI* ptApiRegistry, plRenderBackend* ptBackend);

    // devices
    void (*create_device) (plRenderBackend* ptBackend, VkSurfaceKHR tSurface, bool bEnableValidation, plDevice* ptDeviceOut);
    void (*cleanup_device)(plDevice* ptDevice);

    // swapchains
    void (*create_swapchain)(plRenderBackend* ptBackend, plDevice* ptDevice, VkSurfaceKHR tSurface, uint32_t uWidth, uint32_t uHeight, plSwapchain* ptSwapchainOut);
    void (*cleanup_swapchain)(plRenderBackend* ptBackend, plDevice* ptDevice, plSwapchain* ptSwapchain);
} plRenderBackendI;

typedef struct _plGraphicsApiI
{
    // setup/shutdown/resize
    void (*setup)  (plGraphics* ptGraphics, plRenderBackend* ptBackend, plApiRegistryApiI* ptApiRegistry, plTempAllocator* ptAllocator);
    void (*cleanup)(plGraphics* ptGraphics);
    void (*resize) (plGraphics* ptGraphics);

    // per frame
    bool (*begin_frame)    (plGraphics* ptGraphics);
    void (*end_frame)      (plGraphics* ptGraphics);
    void (*begin_recording)(plGraphics* ptGraphics);
    void (*end_recording)  (plGraphics* ptGraphics);

    // shaders
    uint32_t          (*create_shader)              (plGraphics* ptGraphics, const plShaderDesc* ptDesc);
    uint32_t          (*add_shader_variant)         (plGraphics* ptGraphics, uint32_t uShader, plGraphicsState tVariant, uint32_t uRenderPass, VkSampleCountFlagBits tMSAASampleCount);
    bool              (*shader_variant_exist)       (plGraphics* ptGraphics, uint32_t uShader, plGraphicsState tVariant, uint32_t uRenderPass, VkSampleCountFlagBits tMSAASampleCount);
    void              (*submit_shader_for_deletion) (plGraphics* ptGraphics, uint32_t uShaderIndex);
    plBindGroupLayout*(*get_bind_group_layout)      (plGraphics* ptGraphics, uint32_t uShaderIndex, uint32_t uBindGroupIndex);
    plShaderVariant*  (*get_shader)                 (plGraphics* ptGraphics, uint32_t uVariantIndex);

    // descriptors
    void (*update_bind_group)(plGraphics* ptGraphics, plBindGroup* ptGroup, uint32_t uBufferCount, uint32_t* auBuffers, size_t* aszBufferRanges, uint32_t uTextureViewCount, uint32_t* auTextureViews);

    // drawing
    void (*draw_areas)(plGraphics* ptGraphics, uint32_t uAreaCount, plDrawArea* atAreas, plDraw* atDraws);

    // misc
    plFrameContext* (*get_frame_resources)(plGraphics* ptGraphics);

} plGraphicsApiI;

typedef struct _plDescriptorManagerApiI
{
    VkDescriptorSetLayout (*request_layout)(plDescriptorManager* ptManager, plBindGroupLayout* ptLayout);
    void                  (*cleanup)       (plDescriptorManager* ptManager);
} plDescriptorManagerApiI;

typedef struct _plDeviceMemoryApiI
{
    plDeviceMemoryAllocatorI (*create_device_local_dedicated_allocator)(VkPhysicalDevice tPhysicalDevice, VkDevice tDevice);
    plDeviceMemoryAllocatorI (*create_device_local_buddy_allocator)    (VkPhysicalDevice tPhysicalDevice, VkDevice tDevice);
    plDeviceMemoryAllocatorI (*create_staging_uncached_allocator)      (VkPhysicalDevice tPhysicalDevice, VkDevice tDevice);
    plDeviceMemoryAllocatorI (*create_staging_cached_allocator)        (VkPhysicalDevice tPhysicalDevice, VkDevice tDevice);

    void (*cleanup_device_local_dedicated_allocator)(plDeviceMemoryAllocatorI* ptAllocator);
    void (*cleanup_device_local_buddy_allocator)    (plDeviceMemoryAllocatorI* ptAllocator);
    void (*cleanup_staging_uncached_allocator)      (plDeviceMemoryAllocatorI* ptAllocator);
    void (*cleanup_staging_cached_allocator)        (plDeviceMemoryAllocatorI* ptAllocator);
} plDeviceMemoryApiI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plDescriptorManager
{
    VkDevice               tDevice;
    VkDescriptorSetLayout* _sbtDescriptorSetLayouts;
    uint32_t*              _sbuDescriptorSetLayoutHashes;
} plDescriptorManager;

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
    plFormat             tFormat;
    VkImageUsageFlagBits tUsage;
} plTextureDesc;

typedef struct _plTexture
{
    plTextureDesc            tDesc;
    VkImage                  tImage;
    plDeviceMemoryAllocation tAllocation;
} plTexture;

typedef struct _plBuffer
{
    plBufferUsage            tUsage;
    size_t                   szRequestedSize;
    size_t                   szStride;
    size_t                   szItemCount;
    VkBuffer                 tBuffer;
    plDeviceMemoryAllocation tAllocation;
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
    uint32_t              uRenderPass;
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

typedef struct _plFrameBufferDesc
{
    uint32_t  uRenderPass;
    uint32_t  uWidth;
    uint32_t  uHeight;
    uint32_t  uAttachmentCount;
    uint32_t* puAttachments;
} plFrameBufferDesc;

typedef struct _plRenderPass
{
    plRenderPassDesc tDesc;
    VkRenderPass     _tRenderPass;
} plRenderPass;

typedef struct _plFrameBuffer
{
    VkFramebuffer _tFrameBuffer;
} plFrameBuffer;

typedef struct _plFrameContext
{
    VkSemaphore     tImageAvailable;
    VkSemaphore     tRenderFinish;
    VkFence         tInFlight;
    VkCommandPool   tCmdPool;
    VkCommandBuffer tCmdBuf;
} plFrameContext;

typedef struct _plFrameGarbage
{
    uint32_t* _sbulFrameBufferDeletionQueue;
    uint32_t* _sbulBufferDeletionQueue;
    uint32_t* _sbulTextureDeletionQueue;
    uint32_t* _sbulTextureViewDeletionQueue;
    uint32_t* _sbulRenderPassDeletionQueue;
} plFrameGarbage;

typedef struct _plSwapchain
{
    VkSwapchainKHR           tSwapChain;
    VkExtent2D               tExtent;
    uint32_t*                puFrameBuffers;
    plFormat                 tFormat;
    plFormat                 tDepthFormat;
    VkImage*                 ptImages;
    uint32_t*                puImageViews;
    uint32_t                 uColorTexture;
    uint32_t                 uColorTextureView;
    uint32_t                 uDepthTexture;
    uint32_t                 uDepthTextureView;
    uint32_t                 uImageCount;
    uint32_t                 uImageCapacity;
    uint32_t                 uCurrentImageIndex; // current image to use within the swap chain
    bool                     bVSync;
    VkSampleCountFlagBits    tMsaaSamples;
    VkSurfaceFormatKHR*      ptSurfaceFormats_;
    uint32_t                 uSurfaceFormatCapacity_;

} plSwapchain;

typedef struct _plDynamicBufferNode
{
    uint32_t    uPrev;
    uint32_t    uNext;
    uint32_t    uDynamicBuffer;
    uint32_t    uDynamicBufferOffset;
    uint32_t    uLastActiveFrame;
} plDynamicBufferNode;

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
    VkCommandPool                             tCmdPool;
    uint32_t                                  uUniformBufferBlockSize;

	PFN_vkDebugMarkerSetObjectTagEXT  vkDebugMarkerSetObjectTag;
	PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectName;
	PFN_vkCmdDebugMarkerBeginEXT      vkCmdDebugMarkerBegin;
	PFN_vkCmdDebugMarkerEndEXT        vkCmdDebugMarkerEnd;
	PFN_vkCmdDebugMarkerInsertEXT     vkCmdDebugMarkerInsert;

    // resources
    plBuffer*      sbtBuffers;
    plTexture*     sbtTextures;
    plTextureView* sbtTextureViews;
    plFrameBuffer* sbtFrameBuffers;
    plRenderPass*  sbtRenderPasses;

    // [INTERNAL]
    
    plFrameGarbage* _sbtFrameGarbage;
    uint32_t* _sbulTempQueue;
    
    // free indices
    uint32_t* _sbulBufferFreeIndices;
    uint32_t* _sbulTextureFreeIndices;
    uint32_t* _sbulTextureViewFreeIndices;
    uint32_t* _sbulFrameBufferFreeIndices;
    uint32_t* _sbulRenderPassFreeIndices;
    
    // dynamic buffers
    uint32_t*            _sbuDynamicBufferDeletionQueue;
    plDynamicBufferNode* _sbtDynamicBufferList;

    // new
    size_t   szGeneration;
    uint32_t uFramesInFlight;
    uint32_t uCurrentFrame;

    // apis
    plDeviceApiI* ptDeviceApi;

    // gpu allocators
    plDeviceMemoryAllocatorI tLocalDedicatedAllocator;
    plDeviceMemoryAllocatorI tLocalBuddyAllocator;
    plDeviceMemoryAllocatorI tStagingUnCachedAllocator;
    plDeviceMemoryAllocatorI tStagingCachedAllocator;

} plDevice;

typedef struct _plGraphics
{
    plRenderBackend*         ptBackend;
    plDevice                 tDevice;
    plSwapchain              tSwapchain;
    VkDescriptorPool         tDescriptorPool;
    uint32_t                 uRenderPass;
    plFrameContext*          sbFrames;
    uint32_t                 uFramesInFlight;
    size_t                   szCurrentFrameIndex; // current frame being used
    uint32_t                 uLogChannel;

    // apis
    plIOApiI*                ptIoInterface;
    plFileApiI*              ptFileApi;
    plDeviceApiI*            ptDeviceApi;
    plRenderBackendI*        ptBackendApi;

    // cpu allocators
    plTempAllocator* ptTempAllocator;

    // shaders

    uint32_t* _sbulTempQueue;
    plDescriptorManagerApiI* _ptDescriptorApi;
    plDescriptorManager      _tDescriptorManager;

    uint32_t* _sbulShaderHashes;
    uint32_t* _sbulShaderFreeIndices;
    uint32_t* _sbulShaderDeletionQueue; 

    plShader*        sbtShaders;
    plShaderVariant* sbtShaderVariants; // from all different shaders

} plGraphics;

typedef struct _plRenderBackend
{
    VkInstance               tInstance;
    VkDebugUtilsMessengerEXT tDbgMessenger;
    VkSurfaceKHR             tSurface;
    plDeviceApiI*            ptDeviceApi;
} plRenderBackend;

#endif // PL_VULKAN_EXT_H