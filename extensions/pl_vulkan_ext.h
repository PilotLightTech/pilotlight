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

#define PL_API_GRAPHICS "PL_API_GRAPHICS"
typedef struct _plGraphicsApiI plGraphicsApiI;

#define PL_API_DEVICE "PL_API_DEVICE"
typedef struct _plDeviceApiI plDeviceApiI;

#define PL_API_DEVICE_MEMORY "PL_API_DEVICE_MEMORY"
typedef struct _plDeviceMemoryApiI plDeviceMemoryApiI;

#define PL_API_DESCRIPTOR_MANAGER "PL_API_DESCRIPTOR_MANAGER"
typedef struct _plDescriptorManagerApiI plDescriptorManagerApiI;

#define PL_API_RESOURCE_MANAGER_0 "PL_API_RESOURCE_MANAGER_0"
typedef struct _plResourceManager0ApiI plResourceManager0ApiI;

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
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
typedef struct _plDescriptorManager      plDescriptorManager;
typedef struct _plDeviceMemoryAllocatorI plDeviceMemoryAllocatorI;
typedef struct _plMesh          plMesh;
typedef struct _plSampler       plSampler;
typedef struct _plGraphicsState plGraphicsState;

// external api (pilotlight.h)
typedef struct _plApiRegistryApiI plApiRegistryApiI;

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
typedef struct _plMemoryApiI               plMemoryApiI;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

plGraphicsApiI*          pl_load_graphics_api          (void);
plDeviceApiI*            pl_load_device_api            (void);
plDeviceMemoryApiI*      pl_load_device_memory_api     (void);
plDescriptorManagerApiI* pl_load_descriptor_manager_api(void);
plResourceManager0ApiI*  pl_load_resource_manager_api  (void);

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plDeviceMemoryApiI
{
    plDeviceMemoryAllocatorI (*create_device_local_allocator)(VkPhysicalDevice tPhysicalDevice, VkDevice tDevice);
    plDeviceMemoryAllocatorI (*create_staging_uncached_allocator)(VkPhysicalDevice tPhysicalDevice, VkDevice tDevice);
} plDeviceMemoryApiI;

typedef struct _plDescriptorManagerApiI
{
    VkDescriptorSetLayout (*request_layout)(plDescriptorManager* ptManager, plBindGroupLayout* ptLayout);
    void                  (*cleanup)       (plDescriptorManager* ptManager);
} plDescriptorManagerApiI;

typedef struct _plDeviceApiI
{

    bool                  (*format_has_stencil )      (VkFormat tFormat);
    VkSampleCountFlagBits (*get_max_sample_count)     (plDevice* ptDevice);
    VkFormat              (*find_supported_format)    (plDevice* ptDevice, VkFormatFeatureFlags tFlags, const VkFormat* ptFormats, uint32_t uFormatCount);
    VkFormat              (*find_depth_format)        (plDevice* ptDevice);
    VkFormat              (*find_depth_stencil_format)(plDevice* ptDevice);

} plDeviceApiI;

typedef struct _plResourceManager0ApiI
{
    // per frame
    void (*process_cleanup_queue) (plResourceManager* ptResourceManager, uint32_t uFramesToProcess);

    // commited resources
    uint32_t (*create_index_buffer)   (plResourceManager* ptResourceManager, size_t szSize, const void* pData, const char* pcName);
    uint32_t (*create_vertex_buffer)  (plResourceManager* ptResourceManager, size_t szSize, size_t szStride, const void* pData, const char* pcName);
    uint32_t (*create_constant_buffer)(plResourceManager* ptResourceManager, size_t szSize, const char* pcName);
    uint32_t (*create_texture)        (plResourceManager* ptResourceManager, plTextureDesc tDesc, size_t szSize, const void* pData, const char* pcName);
    uint32_t (*create_storage_buffer) (plResourceManager* ptResourceManager, size_t szSize, const void* pData, const char* pcName);
    uint32_t (*create_texture_view)   (plResourceManager* ptResourceManager, const plTextureViewDesc* ptViewDesc, const plSampler* ptSampler, uint32_t uTextureHandle, const char* pcName);

    // resource manager dynamic buffers
    uint32_t (*request_dynamic_buffer) (plResourceManager* ptResourceManager);
    void     (*return_dynamic_buffer)  (plResourceManager* ptResourceManager, uint32_t uNodeIndex);

    // resource manager misc.
    void (*transfer_data_to_image)           (plResourceManager* ptResourceManager, plTexture* ptDest, size_t szDataSize, const void* pData);
    void (*transfer_data_to_buffer)          (plResourceManager* ptResourceManager, VkBuffer tDest, size_t szSize, const void* pData);
    void (*submit_buffer_for_deletion)       (plResourceManager* ptResourceManager, uint32_t uBufferIndex);
    void (*submit_texture_for_deletion)      (plResourceManager* ptResourceManager, uint32_t uTextureIndex);
    void (*submit_texture_view_for_deletion) (plResourceManager* ptResourceManager, uint32_t uTextureViewIndex);

} plResourceManager0ApiI;

typedef struct _plGraphicsApiI
{
    // setup/shutdown/resize
    void (*setup_graphics)  (plGraphics* ptGraphics, plApiRegistryApiI* ptApiRegistry);
    void (*cleanup_graphics)(plGraphics* ptGraphics);
    void (*resize_graphics) (plGraphics* ptGraphics);

    // per frame
    bool (*begin_frame)    (plGraphics* ptGraphics);
    void (*end_frame)      (plGraphics* ptGraphics);
    void (*begin_recording)(plGraphics* ptGraphics);
    void (*end_recording)  (plGraphics* ptGraphics);

    // command buffers
    VkCommandBuffer (*begin_command_buffer)  (plGraphics* ptGraphics);
    void            (*submit_command_buffer) (plGraphics* ptGraphics, VkCommandBuffer tCmdBuffer);

    // shaders
    uint32_t          (*create_shader)              (plResourceManager* ptResourceManager, const plShaderDesc* ptDesc);
    uint32_t          (*add_shader_variant)         (plResourceManager* ptResourceManager, uint32_t uShader, plGraphicsState tVariant, VkRenderPass ptRenderPass, VkSampleCountFlagBits tMSAASampleCount);
    bool              (*shader_variant_exist)       (plResourceManager* ptResourceManager, uint32_t uShader, plGraphicsState tVariant, VkRenderPass ptRenderPass, VkSampleCountFlagBits tMSAASampleCount);
    void              (*submit_shader_for_deletion) (plResourceManager* ptResourceManager, uint32_t uShaderIndex);
    plBindGroupLayout*(*get_bind_group_layout)      (plResourceManager* ptResourceManager, uint32_t uShaderIndex, uint32_t uBindGroupIndex);
    plShaderVariant*  (*get_shader)                 (plResourceManager* ptResourceManager, uint32_t uVariantIndex);

    // descriptors
    void (*update_bind_group)(plGraphics* ptGraphics, plBindGroup* ptGroup, uint32_t uBufferCount, uint32_t* auBuffers, size_t* aszBufferRanges, uint32_t uTextureViewCount, uint32_t* auTextureViews);

    // drawing
    void (*draw_areas)(plGraphics* ptGraphics, uint32_t uAreaCount, plDrawArea* atAreas, plDraw* atDraws);

    // misc
    plFrameContext* (*get_frame_resources)     (plGraphics* ptGraphics);
    uint32_t        (*find_memory_type)        (VkPhysicalDeviceMemoryProperties tMemProps, uint32_t uTypeFilter, VkMemoryPropertyFlags tProperties);
    void            (*transition_image_layout) (VkCommandBuffer tCommandBuffer, VkImage tImage, VkImageLayout tOldLayout, VkImageLayout tNewLayout, VkImageSubresourceRange tSubresourceRange, VkPipelineStageFlags tSrcStageMask, VkPipelineStageFlags tDstStageMask);
    void            (*set_vulkan_object_name)  (plGraphics* ptGraphics, uint64_t uObjectHandle, VkDebugReportObjectTypeEXT tObjectType, const char* pcName);
} plGraphicsApiI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plMesh
{
    uint32_t uVertexBuffer;
    uint32_t uIndexBuffer;
    uint32_t uVertexOffset;
    uint32_t uVertexCount;
    uint32_t uIndexOffset;
    uint32_t uIndexCount;
    uint64_t ulVertexStreamMask; // PL_MESH_FORMAT_FLAG_*
} plMesh;

typedef struct _plSampler
{
    plFilter      tFilter;
    plCompareMode tCompare;
    plWrapMode    tHorizontalWrap;
    plWrapMode    tVerticalWrap;
    float         fMipBias;
    float         fMinMip;
    float         fMaxMip; 
} plSampler;

typedef struct _plGraphicsState
{
    union 
    {
        struct
        {
            uint64_t ulVertexStreamMask   : 11; // PL_MESH_FORMAT_FLAG_*
            uint64_t ulDepthMode          :  3; // PL_DEPTH_MODE_
            uint64_t ulDepthWriteEnabled  :  1; // bool
            uint64_t ulCullMode           :  2; // VK_CULL_MODE_*
            uint64_t ulBlendMode          :  3; // PL_BLEND_MODE_*
            uint64_t ulShaderTextureFlags :  4; // PL_SHADER_TEXTURE_FLAG_* 
            uint64_t ulStencilMode        :  3;
            uint64_t ulStencilRef         :  8;
            uint64_t ulStencilMask        :  8;
            uint64_t ulStencilOpFail      :  3;
            uint64_t ulStencilOpDepthFail :  3;
            uint64_t ulStencilOpPass      :  3;
            uint64_t _ulUnused            : 12;
        };
        uint64_t ulValue;
    };
    
} plGraphicsState;

typedef struct _plDeviceMemoryAllocation
{
    VkDeviceMemory tMemory;
    uint64_t       ulOffset;
    uint64_t       ulSize;
    void*          pHostMapped;
} plDeviceMemoryAllocation;

typedef struct _plDescriptorManager
{
    VkDevice               tDevice;
    VkDescriptorSetLayout* _sbtDescriptorSetLayouts;
    uint32_t*              _sbuDescriptorSetLayoutHashes;
} plDescriptorManager;

typedef struct _plDeviceMemoryAllocatorI
{

    struct plDeviceMemoryAllocatorO* ptInst; // opaque pointer

    plDeviceMemoryAllocation (*allocate)(struct plDeviceMemoryAllocatorO* ptInst, uint64_t ulSize, uint64_t ulAlignment, const char* pcName);
    void                     (*free)    (struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation);

} plDeviceMemoryAllocatorI;

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
    uint32_t                 uFramesInFlight;
    size_t                   szCurrentFrameIndex; // current frame being used
    uint32_t                 uLogChannel;

    // apis
    plMemoryApiI*            ptMemoryApi;
    plIOApiI*                ptIoInterface;
    plFileApiI*              ptFileApi;
    plDeviceApiI*            ptDeviceApi;
    plResourceManager0ApiI*  ptResourceApi;
    plDeviceMemoryAllocatorI tLocalAllocator;
    plDeviceMemoryAllocatorI tStagingUnCachedAllocator;

	PFN_vkDebugMarkerSetObjectTagEXT  vkDebugMarkerSetObjectTag;
	PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectName;
	PFN_vkCmdDebugMarkerBeginEXT      vkCmdDebugMarkerBegin;
	PFN_vkCmdDebugMarkerEndEXT        vkCmdDebugMarkerEnd;
	PFN_vkCmdDebugMarkerInsertEXT     vkCmdDebugMarkerInsert;
} plGraphics;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plCompareMode
{
    PL_COMPARE_MODE_UNSPECIFIED,
    PL_COMPARE_MODE_NEVER,
    PL_COMPARE_MODE_LESS,
    PL_COMPARE_MODE_EQUAL,
    PL_COMPARE_MODE_LESS_OR_EQUAL,
    PL_COMPARE_MODE_GREATER,
    PL_COMPARE_MODE_NOT_EQUAL,
    PL_COMPARE_MODE_GREATER_OR_EQUAL,
    PL_COMPARE_MODE_ALWAYS
};

enum _plFilter
{
    PL_FILTER_UNSPECIFIED,
    PL_FILTER_NEAREST,
    PL_FILTER_LINEAR
};

enum _plWrapMode
{
    PL_WRAP_MODE_UNSPECIFIED,
    PL_WRAP_MODE_WRAP,
    PL_WRAP_MODE_CLAMP,
    PL_WRAP_MODE_MIRROR
};

enum _plBufferBindingType
{
    PL_BUFFER_BINDING_TYPE_UNSPECIFIED,
    PL_BUFFER_BINDING_TYPE_UNIFORM,
    PL_BUFFER_BINDING_TYPE_STORAGE
};

enum _plTextureBindingType
{
    PL_TEXTURE_BINDING_TYPE_UNSPECIFIED,
    PL_TEXTURE_BINDING_TYPE_SAMPLED,
    PL_TEXTURE_BINDING_TYPE_STORAGE
};

enum _plBufferUsage
{
    PL_BUFFER_USAGE_UNSPECIFIED,
    PL_BUFFER_USAGE_INDEX,
    PL_BUFFER_USAGE_VERTEX,
    PL_BUFFER_USAGE_CONSTANT,
    PL_BUFFER_USAGE_STORAGE
};

enum _plBlendMode
{
    PL_BLEND_MODE_NONE,
    PL_BLEND_MODE_ALPHA,
    PL_BLEND_MODE_ADDITIVE,
    PL_BLEND_MODE_PREMULTIPLY,
    PL_BLEND_MODE_MULTIPLY,
    PL_BLEND_MODE_CLIP_MASK,
    
    PL_BLEND_MODE_COUNT
};

enum _plDepthMode
{
    PL_DEPTH_MODE_NEVER,
    PL_DEPTH_MODE_LESS,
    PL_DEPTH_MODE_EQUAL,
    PL_DEPTH_MODE_LESS_OR_EQUAL,
    PL_DEPTH_MODE_GREATER,
    PL_DEPTH_MODE_NOT_EQUAL,
    PL_DEPTH_MODE_GREATER_OR_EQUAL,
    PL_DEPTH_MODE_ALWAYS,
};

enum _plStencilMode
{
    PL_STENCIL_MODE_NEVER,
    PL_STENCIL_MODE_LESS,
    PL_STENCIL_MODE_EQUAL,
    PL_STENCIL_MODE_LESS_OR_EQUAL,
    PL_STENCIL_MODE_GREATER,
    PL_STENCIL_MODE_NOT_EQUAL,
    PL_STENCIL_MODE_GREATER_OR_EQUAL,
    PL_STENCIL_MODE_ALWAYS,
};

enum _plMeshFormatFlags
{
    PL_MESH_FORMAT_FLAG_NONE           = 0,
    PL_MESH_FORMAT_FLAG_HAS_POSITION   = 1 << 0,
    PL_MESH_FORMAT_FLAG_HAS_NORMAL     = 1 << 1,
    PL_MESH_FORMAT_FLAG_HAS_TANGENT    = 1 << 2,
    PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0 = 1 << 3,
    PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1 = 1 << 4,
    PL_MESH_FORMAT_FLAG_HAS_COLOR_0    = 1 << 5,
    PL_MESH_FORMAT_FLAG_HAS_COLOR_1    = 1 << 6,
    PL_MESH_FORMAT_FLAG_HAS_JOINTS_0   = 1 << 7,
    PL_MESH_FORMAT_FLAG_HAS_JOINTS_1   = 1 << 8,
    PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0  = 1 << 9,
    PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1  = 1 << 10
};

enum _plShaderTextureFlags
{
    PL_SHADER_TEXTURE_FLAG_BINDING_NONE       = 0,
    PL_SHADER_TEXTURE_FLAG_BINDING_0          = 1 << 0,
    PL_SHADER_TEXTURE_FLAG_BINDING_1          = 1 << 1,
    PL_SHADER_TEXTURE_FLAG_BINDING_2          = 1 << 2,
    PL_SHADER_TEXTURE_FLAG_BINDING_3          = 1 << 3,
    // PL_SHADER_TEXTURE_FLAG_BINDING_4          = 1 << 4,
    // PL_SHADER_TEXTURE_FLAG_BINDING_5          = 1 << 5,
    // PL_SHADER_TEXTURE_FLAG_BINDING_6          = 1 << 6,
    // PL_SHADER_TEXTURE_FLAG_BINDING_7          = 1 << 7,
    // PL_SHADER_TEXTURE_FLAG_BINDING_8          = 1 << 8,
    // PL_SHADER_TEXTURE_FLAG_BINDING_9          = 1 << 9,
    // PL_SHADER_TEXTURE_FLAG_BINDING_10         = 1 << 10,
    // PL_SHADER_TEXTURE_FLAG_BINDING_11         = 1 << 11,
    // PL_SHADER_TEXTURE_FLAG_BINDING_12         = 1 << 12,
    // PL_SHADER_TEXTURE_FLAG_BINDING_13         = 1 << 13,
    // PL_SHADER_TEXTURE_FLAG_BINDING_14         = 1 << 14
};

#endif // PL_VULKAN_EXT_H