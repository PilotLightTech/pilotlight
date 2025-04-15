/*
   pl_graphics_ext.h
    * a light wrapper over Vulkan & Metal 3.0
*/

/*
Index of this file:
// [SECTION] quick notes
// [SECTION] header mess
// [SECTION] defines
// [SECTION] apis
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api struct
// [SECTION] structs
// [SECTION] enums
// [SECTION] internal enums
// [SECTION] inline API implementations
*/

//-----------------------------------------------------------------------------
// [SECTION] quick notes
//-----------------------------------------------------------------------------

/*

    BACKGROUND:

    The graphics extension is meant to be an extremely lightweight abstraction
    over the "modern" explicit graphics APIs (Vulkan/DirectX 12/Metal 3.0).
    Ideally it should be 1 to 1 when possible. The explicit control provided
    by these APIs are their power, so the extension tries to preserve that
    as much as possible while also allowing a graphics programmer the ability
    to write cross platform graphics code without too much consideration of
    the differences between the APIs. This is accomplished by careful
    consideration of the APIs and their common concepts and features. In some
    cases, the API is required to be stricter than the underlying API. For
    example, Vulkan makes it easy to issue draw calls and compute dispatches
    directly to command buffers while Metal requires these to be submitted
    to different "encoders" which can't be recording simutaneously. So the
    graphics extension introduces the concept of encoder to match the stricter
    API at the cost of some freedom Vulkan would normally allow. There is only
    a few cases like this but you should be aware of them.

    Provided Implementation:
        The provided implementation of this extension depends on the following
        APIs being available:

        * plLogI
        * plThreadsI
        * plLogI
        * plProfileI

    WARNING:

    The purpose of the graphics extension is NOT to make low level graphics
    programming easier.
    
    The purpose of the graphics extension is NOT to be an abstraction for the
    sake of abstraction.

    The graphics extension does not hold your hand. You are expected to be
    familar with either Vulkan or Metal 3.0 concepts.

    This extension mostly assume you understand low level graphics and will
    not attempt to explain those concepts (i.e. what is a vertex buffer?)

*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_GRAPHICS_EXT_H
#define PL_GRAPHICS_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

// misc config options
#define PL_MAX_BUFFERS_PER_BIND_GROUP 32
#define PL_MAX_TEXTURES_PER_BIND_GROUP 32
#define PL_MAX_SAMPLERS_PER_BIND_GROUP 8
#define PL_MAX_SHADER_SPECIALIZATION_CONSTANTS 64
#define PL_MAX_RENDER_TARGETS 16
#define PL_MAX_FRAMES_IN_FLIGHT 2
#define PL_MAX_SUBPASSES 8
#define PL_MAX_TIMELINE_SEMAPHORES 8
#define PL_MAX_VERTEX_ATTRIBUTES 8
#define PL_MAX_VIEWPORTS 16

// macros
#define PL_DEFINE_HANDLE(x) \
    typedef union x { struct {uint16_t uIndex; uint16_t uGeneration;}; uint32_t uData; } x;

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plGraphicsI_version {1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdlib.h>  // size_t
#include <stdint.h>  // uint*_t
#include <stdbool.h> // bool
#include "pl_math.h" // plVec*

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// devices
typedef struct _plGraphicsInit plGraphicsInit; // graphics extension intialization options
typedef struct _plDevice       plDevice;       // represents a single GPU
typedef struct _plDeviceInfo   plDeviceInfo;   // device information
typedef struct _plDeviceInit   plDeviceInit;   // device creation options

// draw stream
typedef struct _plDrawStream     plDrawStream;     // draw command for drawing stream api
typedef struct _plDrawStreamData plDrawStreamData; // data for draw stream api
typedef struct _plDrawArea       plDrawArea;       // data for draw stream api

// basic resources
typedef struct _plSamplerDesc       plSamplerDesc;       // descriptor for creating samplers
typedef struct _plTextureDesc       plTextureDesc;       // descriptor for creating textures
typedef struct _plTextureViewDesc   plTextureViewDesc;   // descriptor for creating texture views
typedef struct _plBufferDesc        plBufferDesc;        // descriptor for creating buffers
typedef struct _plSampler           plSampler;           // sampler resource
typedef struct _plTexture           plTexture;           // texture resource
typedef struct _plBuffer            plBuffer;            // buffer resource
typedef struct _plTimelineSemaphore plTimelineSemaphore; // timeline semaphore

// swapchains
typedef struct _plSwapchain     plSwapchain;     // swapchain
typedef struct _plSurface       plSurface;       // used for creating swapchains
typedef struct _plSwapchainInfo plSwapchainInfo; // swapchain information
typedef struct _plSwapchainInit plSwapchainInit; // swapchain creation options

// encoders
typedef struct _plRenderEncoder       plRenderEncoder;       // opaque type for command buffer encoder for render ops
typedef struct _plComputeEncoder      plComputeEncoder;      // opaque type for command buffer encoder for compute ops
typedef struct _plBlitEncoder         plBlitEncoder;         // opaque type for command buffer encoder for blit ops
typedef struct _plBufferImageCopy     plBufferImageCopy;     // used for copying between buffers & textures with blit encoder
typedef struct _plImageCopy           plImageCopy;           // used for copying between textures with blit encoder
typedef struct _plPassTextureResource plPassTextureResource;
typedef struct _plPassBufferResource  plPassBufferResource;
typedef struct _plPassResources       plPassResources;

// bind groups
typedef struct _plBindGroup                  plBindGroup;                  // bind group resource
typedef struct _plBindGroupLayout            plBindGroupLayout;            // bind group layout decription
typedef struct _plBindGroupDesc              plBindGroupDesc;              // descriptor for creating bind groups
typedef struct _plBindGroupPool              plBindGroupPool;              // opaque type for bind group pools
typedef struct _plBindGroupPoolDesc          plBindGroupPoolDesc;          // descriptor for creating bind group pools
typedef struct _plBufferBinding              plBufferBinding;              // buffer binding for bind groups
typedef struct _plSamplerBinding             plSamplerBinding;             // sampler binding for bind groups
typedef struct _plDynamicBinding             plDynamicBinding;             // dynamic binding for bind groups
typedef struct _plDynamicDataBlock           plDynamicDataBlock;           // dynamic data block
typedef struct _plTextureBinding             plTextureBinding;             // texture binding for bind groups
typedef struct _plBindGroupUpdateBufferData  plBindGroupUpdateBufferData;  // buffer update data for bind groups
typedef struct _plBindGroupUpdateSamplerData plBindGroupUpdateSamplerData; // sampler update data for bind groups
typedef struct _plBindGroupUpdateTextureData plBindGroupUpdateTextureData; // texture update data for bind groups
typedef struct _plBindGroupUpdateData        plBindGroupUpdateData;        // data for updating bind groups

// shaders
typedef struct _plComputeShaderDesc plComputeShaderDesc; // descriptor for creating compute shaders
typedef struct _plShaderDesc        plShaderDesc;        // descriptor for creating shaders
typedef struct _plShader            plShader;            // shader resource
typedef struct _plComputeShader     plComputeShader;     // compute shader resource
typedef struct _plShaderModule      plShaderModule;      // shader code

// command buffers
typedef struct _plCommandBuffer    plCommandBuffer;    // opaque type for command buffer
typedef struct _plCommandPool      plCommandPool;      // opaque type for command buffer pools
typedef struct _plCommandPoolDesc  plCommandPoolDesc;  // descriptor for creating command pools (future use)
typedef struct _plBeginCommandInfo plBeginCommandInfo; // options when beginning command recording
typedef struct _plSubmitInfo       plSubmitInfo;       // options when submitting commands to queues

// render passes
typedef struct _plRenderTarget          plRenderTarget;
typedef struct _plColorTarget           plColorTarget;
typedef struct _plDepthTarget           plDepthTarget;
typedef struct _plSubpass               plSubpass;
typedef struct _plSubpassDependency     plSubpassDependency;
typedef struct _plRenderPassLayout      plRenderPassLayout;
typedef struct _plRenderPassLayoutDesc  plRenderPassLayoutDesc;
typedef struct _plRenderPassDesc        plRenderPassDesc;
typedef struct _plRenderPass            plRenderPass;
typedef struct _plRenderPassAttachments plRenderPassAttachments;

// device memory
typedef struct _plDeviceMemoryRequirements plDeviceMemoryRequirements; // requirements for a new memory allocations
typedef struct _plDeviceMemoryAllocation   plDeviceMemoryAllocation;   // single memory allocation or suballocations
typedef struct _plDeviceMemoryAllocatorI   plDeviceMemoryAllocatorI;   // GPU allocator interface

// misc.
typedef struct _plDraw                   plDraw;                   // data for submitting individual draw command
typedef struct _plDrawIndex              plDrawIndex;              // data for submitting individual indexed draw command
typedef struct _plDispatch               plDispatch;               // data for submitting individual compute command
typedef struct _plVertexBufferLayout     plVertexBufferLayout;     // vertex buffer layout description
typedef struct _plVertexAttribute        plVertexAttribute;        // vertex buffer layout attribute
typedef struct _plSpecializationConstant plSpecializationConstant; // shader specialization constant
typedef struct _plBlendState             plBlendState;             // blend state
typedef struct _plGraphicsState          plGraphicsState;
typedef struct _plRenderViewport         plRenderViewport;
typedef struct _plScissor                plScissor;

// handles
PL_DEFINE_HANDLE(plBufferHandle);
PL_DEFINE_HANDLE(plTextureHandle);
PL_DEFINE_HANDLE(plSamplerHandle);
PL_DEFINE_HANDLE(plBindGroupHandle);
PL_DEFINE_HANDLE(plShaderHandle);
PL_DEFINE_HANDLE(plComputeShaderHandle);
PL_DEFINE_HANDLE(plRenderPassHandle);
PL_DEFINE_HANDLE(plRenderPassLayoutHandle);

// enums
typedef int plMipmapMode;             // -> enum _plMipmapMode             // Enum: mipmap filter modes (PL_MIPMAP_MODE_XXXX)
typedef int plAddressMode;            // -> enum _plAddressMode            // Enum: addressing mode sampling textures outside image (PL_ADDRESS_MODE_XXXX)
typedef int plFilter;                 // -> enum _plFilter                 // Enum: texture lookup filters (PL_FILTER_XXXX)
typedef int plSampleCount;            // -> enum _plSampleCount            // Enum: texture sample count (PL_SAMPLE_COUNT_XXXX)
typedef int plTextureType;            // -> enum _plTextureType            // Enum: texture type (PL_TEXTURE_TYPE_XXXX)
typedef int plTextureUsage;           // -> enum _plTextureUsage           // Flag: texture type (PL_TEXTURE_USAGE_XXXX)
typedef int plCompareMode;            // -> enum _plCompareMode            // Enum: texture sampling comparison modes (PL_COMPARE_MODE_XXXX)
typedef int plFormat;                 // -> enum _plFormat                 // Enum: formats (PL_FORMAT_XXXX)
typedef int plVertexFormat;           // -> enum _plVertexFormat           // Enum: formats (PL_FORMAT_VERTEX_XXXX)
typedef int plBufferUsage;            // -> enum _plBufferUsage            // Flag: buffer usage flags (PL_BUFFER_USAGE_XXXX)
typedef int plShaderStageFlags;       // -> enum _plShaderStageFlags       // Flag: GPU pipeline stage (PL_SHADER_STAGE_XXXX)
typedef int plPipelineStageFlags;     // -> enum _plPipelineStageFlags     // Flag: GPU pipeline stage (PL_PIPELINE_STAGE_XXXX)
typedef int plAccessFlags;            // -> enum _plAccessFlags            // Flag: GPU pipeline stage (PL_ACCESS_XXXX)
typedef int plCullMode;               // -> enum _plCullMode               // Flag: face culling mode (PL_CULL_MODE_XXXX)
typedef int plBufferBindingType;      // -> enum _plBufferBindingType      // Enum: buffer binding type for bind groups (PL_BUFFER_BINDING_TYPE_XXXX)
typedef int plTextureBindingType;     // -> enum _plTextureBindingType     // Enum: image binding type for bind groups (PL_TEXTURE_BINDING_TYPE_XXXX)
typedef int plBindGroupPoolFlags;     // -> enum _plBindGroupPoolFlags     // Flags: flags for creating bind group pools (PL_BIND_GROUP_POOL_FLAGS_XXXX)
typedef int plMemoryMode;             // -> enum _plMemoryMode             // Enum: memory modes for allocating memory (PL_MEMORY_XXXX)
typedef int plStencilOp;              // -> enum _plStencilOp              // Enum: stencil operations (PL_STENCIL_OP_XXXX)
typedef int plLoadOp;                 // -> enum _plLoadOp                 // Enum: render pass target load operations (PL_LOAD_OP_XXXX)
typedef int plStoreOp;                // -> enum _plStoreOp                // Enum: render pass target store operations (PL_STORE_OP_XXXX)
typedef int plBlendOp;                // -> enum _plBlendOp                // Enum: blend operations (PL_BLEND_OP_XXXX)
typedef int plBlendFactor;            // -> enum _plBlendFactor            // Enum: blend operation factors (PL_BLEND_FACTOR_XXXX)
typedef int plDataType;               // -> enum _plDataType               // Enum: data types
typedef int plGraphicsInitFlags;      // -> enum _plGraphicsInitFlags      // Flags: graphics initialization flags (PL_GRAPHICS_INIT_FLAGS_XXXX)
typedef int plDeviceInitFlags;        // -> enum _plDeviceInitFlags        // Flags: device initialization flags (PL_DEVICE_INIT_FLAGS_XXXX)
typedef int plVendorId;               // -> enum _plVendorId               // Enum: device vendors (PL_VENDOR_ID_XXXX)
typedef int plDeviceType;             // -> enum _plDeviceType             // Enum: device type (PL_DEVICE_TYPE_XXXX)
typedef int plDeviceCapability;       // -> enum _plDeviceCapability       // Flags: device capabilities (PL_DEVICE_CAPABILITY_XXXX)
typedef int plCommandPoolResetFlags;  // -> enum _plCommandPoolResetFlags  // Flags: device capabilities (PL_DEVICE_CAPABILITY_XXXX)
typedef int plPassResourceUsageFlags; // -> enum _plPassResourceUsageFlags // Flags: resource usage (PL_PASS_RESOURCE_USAGE_XXXX)
typedef int plGraphicsBackend;        // -> enum _plGraphicsBackend        // Enum: graphics backend (PL_GRAPHICS_BACKEND_XXXX)

// external
typedef struct _plWindow plWindow; // pl_os.h

//------------------------------NOT STABLE-------------------------------------

#ifdef PL_GRAPHICS_EXPOSE_VULKAN
typedef struct VkInstance_T*       VkInstance;
typedef struct VkPhysicalDevice_T* VkPhysicalDevice;
typedef struct VkDevice_T*         VkDevice;
typedef struct VkQueue_T*          VkQueue;
typedef struct VkCommandBuffer_T*  VkCommandBuffer;

typedef struct VkRenderPass_T*     VkRenderPass;
typedef struct VkDescriptorPool_T* VkDescriptorPool;
typedef struct VkImageView_T*      VkImageView;
typedef struct VkSampler_T*        VkSampler;
typedef struct VkDescriptorSet_T*  VkDescriptorSet;
#endif

#ifdef PL_GRAPHICS_EXPOSE_METAL
@class MTLRenderPassDescriptor;
@protocol MTLDevice, MTLCommandBuffer, MTLRenderCommandEncoder, MTLTexture;
#endif

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plGraphicsI
{
    // context
    bool              (*initialize)(const plGraphicsInit*);
    void              (*cleanup)   (void);
    plGraphicsBackend (*get_backend)(void);
    const char*       (*get_backend_string)(void);

    // devices
    void                (*enumerate_devices)(plDeviceInfo*, uint32_t* deviceCountOut);
    plDevice*           (*create_device)    (const plDeviceInit*);
    void                (*cleanup_device)   (plDevice*);
    void                (*flush_device)     (plDevice*);
    const plDeviceInfo* (*get_device_info)  (plDevice*);

    // surface
    plSurface* (*create_surface) (plWindow*);
    void       (*cleanup_surface)(plSurface*);

    // swapchain
    plSwapchain*     (*create_swapchain)       (plDevice*, plSurface*, const plSwapchainInit*);
    void             (*cleanup_swapchain)      (plSwapchain*);
    bool             (*acquire_swapchain_image)(plSwapchain*);
    void             (*recreate_swapchain)     (plSwapchain*, const plSwapchainInit*);
    plTextureHandle* (*get_swapchain_images)   (plSwapchain*, uint32_t* puSizeOut);
    plSwapchainInfo  (*get_swapchain_info)     (plSwapchain*);

    // query
    uint32_t (*get_frames_in_flight)   (void);
    uint32_t (*get_current_frame_index)(void);
    size_t   (*get_host_memory_in_use) (void);
    size_t   (*get_local_memory_in_use)(void);

    // per frame
    void (*begin_frame)(plDevice*);
    bool (*present)    (plCommandBuffer*, const plSubmitInfo*, plSwapchain**, uint32_t uSwapchainCount);

    // timeline semaphore ops
    plTimelineSemaphore* (*create_semaphore)   (plDevice*, bool hostVisible);
    void                 (*cleanup_semaphore)  (plTimelineSemaphore*);
    void                 (*signal_semaphore)   (plDevice*, plTimelineSemaphore*, uint64_t);
    void                 (*wait_semaphore)     (plDevice*, plTimelineSemaphore*, uint64_t);
    uint64_t             (*get_semaphore_value)(plDevice*, plTimelineSemaphore*);

    // command pools & buffers
    plCommandPool*   (*create_command_pool)    (plDevice*, const plCommandPoolDesc*);
    void             (*cleanup_command_pool)   (plCommandPool*);
    void             (*reset_command_pool)     (plCommandPool*, plCommandPoolResetFlags); // call at beginning of frame
    plCommandBuffer* (*request_command_buffer) (plCommandPool*);   // retrieve command buffer from the pool
    void             (*return_command_buffer)  (plCommandBuffer*); // return command buffer to pool
    void             (*reset_command_buffer)   (plCommandBuffer*); // call if reusing after submit/present
    void             (*wait_on_command_buffer) (plCommandBuffer*); // call after submit to block/wait
    void             (*begin_command_recording)(plCommandBuffer*, const plBeginCommandInfo*);
    void             (*end_command_recording)  (plCommandBuffer*);
    void             (*submit_command_buffer)  (plCommandBuffer*, const plSubmitInfo*);

    // render encoder
    plRenderEncoder*   (*begin_render_pass)         (plCommandBuffer*, plRenderPassHandle, const plPassResources*); // do not store
    void               (*next_subpass)              (plRenderEncoder*, const plPassResources*);
    void               (*end_render_pass)           (plRenderEncoder*);
    plRenderPassHandle (*get_encoder_render_pass)   (plRenderEncoder*);
    uint32_t           (*get_render_encoder_subpass)(plRenderEncoder*);
    plCommandBuffer*   (*get_encoder_command_buffer)(plRenderEncoder*);

    // render encoder: draw stream (preferred system)
    //   Notes:
    //     - call reset_draw_stream(...) with the maximum number of possible calls to
    //       pl_add_to_draw_stream before the next call to draw_stream so any memory
    //       allocations can happen before a hot loop where pl_add_to_draw_stream is called.
    void (*reset_draw_stream)   (plDrawStream*, uint32_t drawCount);
    void (*cleanup_draw_stream) (plDrawStream*);
    void (*draw_stream)         (plRenderEncoder*, uint32_t areaCount, plDrawArea*); // decodes drawstream (does not reset draw stream)
    // INLINED -> void pl_add_to_draw_stream(plDrawStream*, plDrawStreamData);

    // render encoder: direct (prefer draw stream system, this will be used for bindless mostly)
    void (*bind_graphics_bind_groups)(plRenderEncoder*, plShaderHandle, uint32_t first, uint32_t count, const plBindGroupHandle*, uint32_t dynamicCount, const plDynamicBinding*);
    void (*set_depth_bias)           (plRenderEncoder*, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor);
    void (*set_viewport)             (plRenderEncoder*, const plRenderViewport*);
    void (*set_scissor_region)       (plRenderEncoder*, const plScissor*);
    void (*bind_vertex_buffer)       (plRenderEncoder*, plBufferHandle);
    void (*draw)                     (plRenderEncoder*, uint32_t count, const plDraw*);
    void (*draw_indexed)             (plRenderEncoder*, uint32_t count, const plDrawIndex*);
    void (*bind_shader)              (plRenderEncoder*, plShaderHandle);

    // compute encoder
    plComputeEncoder* (*begin_compute_pass)                (plCommandBuffer*, const plPassResources*); // do not store
    void              (*end_compute_pass)                  (plComputeEncoder*);
    void              (*dispatch)                          (plComputeEncoder*, uint32_t dispatchCount, const plDispatch*);
    void              (*bind_compute_shader)               (plComputeEncoder*, plComputeShaderHandle);
    void              (*bind_compute_bind_groups)          (plComputeEncoder*, plComputeShaderHandle, uint32_t first, uint32_t count, const plBindGroupHandle*, uint32_t dynamicCount, const plDynamicBinding*);
    plCommandBuffer*  (*get_compute_encoder_command_buffer)(plComputeEncoder*);

    // blit encoder 
    plBlitEncoder*   (*begin_blit_pass)                (plCommandBuffer*); // do not store
    void             (*end_blit_pass)                  (plBlitEncoder*);
    void             (*set_texture_usage)              (plBlitEncoder*, plTextureHandle, plTextureUsage tNewUsage, plTextureUsage tOldUsage);
    void             (*copy_buffer_to_texture)         (plBlitEncoder*, plBufferHandle, plTextureHandle, uint32_t regionCount, const plBufferImageCopy*);
    void             (*copy_texture_to_buffer)         (plBlitEncoder*, plTextureHandle, plBufferHandle, uint32_t regionCount, const plBufferImageCopy*);
    void             (*copy_texture)                   (plBlitEncoder*, plTextureHandle, plTextureHandle, uint32_t regionCount, const plImageCopy*);
    void             (*generate_mipmaps)               (plBlitEncoder*, plTextureHandle);
    void             (*copy_buffer)                    (plBlitEncoder*, plBufferHandle source, plBufferHandle destination, uint32_t sourceOffset, uint32_t destinationOffset, size_t);
    plCommandBuffer* (*get_blit_encoder_command_buffer)(plBlitEncoder*);

    // global barriers
    void (*pipeline_barrier_blit)   (plBlitEncoder*,    plShaderStageFlags beforeStages, plAccessFlags beforeAccesses, plShaderStageFlags afterStages, plAccessFlags afterAccesses);
    void (*pipeline_barrier_compute)(plComputeEncoder*, plShaderStageFlags beforeStages, plAccessFlags beforeAccesses, plShaderStageFlags afterStages, plAccessFlags afterAccesses);
    void (*pipeline_barrier_render) (plRenderEncoder*,  plShaderStageFlags beforeStages, plAccessFlags beforeAccesses, plShaderStageFlags afterStages, plAccessFlags afterAccesses);

    //-----------------------------------------------------------------------------

    // buffers
    plBufferHandle (*create_buffer)            (plDevice*, const plBufferDesc*, plBuffer**);
    void           (*bind_buffer_to_memory)    (plDevice*, plBufferHandle, const plDeviceMemoryAllocation*);
    void           (*queue_buffer_for_deletion)(plDevice*, plBufferHandle);
    void           (*destroy_buffer)           (plDevice*, plBufferHandle);
    plBuffer*      (*get_buffer)               (plDevice*, plBufferHandle); // do not store
    bool           (*is_buffer_valid)          (plDevice*, plBufferHandle);

    // samplers
    plSamplerHandle (*create_sampler)            (plDevice*, const plSamplerDesc*);
    void            (*destroy_sampler)           (plDevice*, plSamplerHandle);
    void            (*queue_sampler_for_deletion)(plDevice*, plSamplerHandle);
    plSampler*      (*get_sampler)               (plDevice*, plSamplerHandle); // do not store
    bool            (*is_sampler_valid)          (plDevice*, plSamplerHandle);

    // textures
    plTextureHandle (*create_texture)            (plDevice*, const plTextureDesc*, plTexture**);
    plTextureHandle (*create_texture_view)       (plDevice*, const plTextureViewDesc*);
    void            (*bind_texture_to_memory)    (plDevice*, plTextureHandle, const plDeviceMemoryAllocation*);
    void            (*queue_texture_for_deletion)(plDevice*, plTextureHandle);
    void            (*destroy_texture)           (plDevice*, plTextureHandle);
    plTexture*      (*get_texture)               (plDevice*, plTextureHandle); // do not store
    bool            (*is_texture_valid)          (plDevice*, plTextureHandle);

    // bind groups
    plBindGroupPool*  (*create_bind_group_pool)       (plDevice*, const plBindGroupPoolDesc*);
    void              (*cleanup_bind_group_pool)      (plBindGroupPool*);
    void              (*reset_bind_group_pool)        (plBindGroupPool*);
    plBindGroupHandle (*create_bind_group)            (plDevice*, const plBindGroupDesc*);
    void              (*update_bind_group)            (plDevice*, plBindGroupHandle, const plBindGroupUpdateData*);
    void              (*queue_bind_group_for_deletion)(plDevice*, plBindGroupHandle);
    void              (*destroy_bind_group)           (plDevice*, plBindGroupHandle);
    plBindGroup*      (*get_bind_group)               (plDevice*, plBindGroupHandle); // do not store
    bool              (*is_bind_group_valid)          (plDevice*, plBindGroupHandle);
    
    // render passes
    plRenderPassHandle (*create_render_pass)            (plDevice*, const plRenderPassDesc*, const plRenderPassAttachments*);
    void               (*update_render_pass_attachments)(plDevice*, plRenderPassHandle, plVec2 dimensions, const plRenderPassAttachments*);
    void               (*queue_render_pass_for_deletion)(plDevice*, plRenderPassHandle);
    void               (*destroy_render_pass)           (plDevice*, plRenderPassHandle);
    plRenderPass*      (*get_render_pass)               (plDevice*, plRenderPassHandle); // do not store
    bool               (*is_render_pass_valid)          (plDevice*, plRenderPassHandle);

    // render pass layouts
    plRenderPassLayoutHandle (*create_render_pass_layout)            (plDevice*, const plRenderPassLayoutDesc*);
    void                     (*queue_render_pass_layout_for_deletion)(plDevice*, plRenderPassLayoutHandle);
    void                     (*destroy_render_pass_layout)           (plDevice*, plRenderPassLayoutHandle);
    plRenderPassLayout*      (*get_render_pass_layout)               (plDevice*, plRenderPassLayoutHandle); // do not store
    bool                     (*is_render_pass_layout_valid)          (plDevice*, plRenderPassLayoutHandle);

    // pixel & vertex shaders
    plShaderHandle (*create_shader)            (plDevice*, const plShaderDesc*);
    void           (*queue_shader_for_deletion)(plDevice*, plShaderHandle);
    void           (*destroy_shader)           (plDevice*, plShaderHandle);
    plShader*      (*get_shader)               (plDevice*, plShaderHandle); // do not store
    bool           (*is_shader_valid)          (plDevice*, plShaderHandle);

    // compute shaders
    plComputeShaderHandle (*create_compute_shader)            (plDevice*, const plComputeShaderDesc*);
    void                  (*queue_compute_shader_for_deletion)(plDevice*, plComputeShaderHandle);
    void                  (*destroy_compute_shader)           (plDevice*, plComputeShaderHandle);
    bool                  (*is_compute_shader_valid)          (plDevice*, plComputeShaderHandle);

    // dynamic data system
    //   Notes:
    //     - call "allocate_dynamic_data_block" once at the beginning of the frame
    //       then use "pl_allocate_dynamic_data" throughout the frame (it will allocate new
    //       blocks as needed)
    //     - do not store the plDynamicDataBlock returned from "allocate_dynamic_data_block"
    //       longer than a single frame (they are reset every frame)
    plDynamicDataBlock (*allocate_dynamic_data_block)(plDevice*); // do not store longer than a single frame (this ar)
    // INLINED -> plDynamicBinding pl_allocate_dynamic_data(const plGraphicsI*,  plDevice*, plDynamicDataBlock*)

    // memory
    plDeviceMemoryAllocation        (*allocate_memory)(plDevice*, size_t, plMemoryMode, uint32_t typeFilter, const char* debugName);
    void                            (*free_memory)    (plDevice*, plDeviceMemoryAllocation*);
    const plDeviceMemoryAllocation* (*get_allocations)(plDevice*, uint32_t* sizeOut);

    //------------------------------NOT STABLE-------------------------------------

    #ifdef PL_GRAPHICS_EXPOSE_VULKAN
    VkInstance       (*get_vulkan_instance)       (void);
    uint32_t         (*get_vulkan_api_version)    (void);
    VkDevice         (*get_vulkan_device)         (plDevice*);
    VkPhysicalDevice (*get_vulkan_physical_device)(plDevice*);
    VkQueue          (*get_vulkan_queue)          (plDevice*);
    uint32_t         (*get_vulkan_queue_family)   (plDevice*);
    VkRenderPass     (*get_vulkan_render_pass)    (plDevice*, plRenderPassHandle);
    VkDescriptorPool (*get_vulkan_descriptor_pool)(plBindGroupPool*);
    int              (*get_vulkan_sample_count)   (plSwapchain*);
    VkCommandBuffer  (*get_vulkan_command_buffer) (plCommandBuffer*);
    VkImageView      (*get_vulkan_image_view)     (plDevice*, plTextureHandle);
    VkSampler        (*get_vulkan_sampler)        (plDevice*, plSamplerHandle);
    VkDescriptorSet  (*get_vulkan_descriptor_set) (plDevice*, plBindGroupHandle);
    #endif

    #ifdef PL_GRAPHICS_EXPOSE_METAL
    id<MTLDevice>               (*get_metal_device)(plDevice*);
    plTextureHandle             (*get_metal_bind_group_texture)(plDevice*, plBindGroupHandle);
    id<MTLTexture>              (*get_metal_texture)(plDevice*, plTextureHandle);
    MTLRenderPassDescriptor*    (*get_metal_render_pass_descriptor)(plDevice*, plRenderPassHandle);
    id<MTLCommandBuffer>        (*get_metal_command_buffer)(plCommandBuffer*);
    id<MTLRenderCommandEncoder> (*get_metal_command_encoder)(plRenderEncoder*);
    #endif

} plGraphicsI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

//-----------------------------gpu memory--------------------------------------

typedef struct _plDeviceMemoryRequirements
{
    uint64_t ulSize;
    uint64_t ulAlignment;
    uint32_t uMemoryTypeBits;
} plDeviceMemoryRequirements;

typedef struct _plDeviceMemoryAllocation
{
    plMemoryMode              tMemoryMode;
    uint64_t                  ulMemoryType;
    uint64_t                  uHandle;
    uint64_t                  ulOffset;
    uint64_t                  ulSize;
    char*                     pHostMapped; // if host visible
    plDeviceMemoryAllocatorI* ptAllocator; // if allocated from user allocator
} plDeviceMemoryAllocation;

typedef struct _plDeviceMemoryAllocatorI
{
    struct plDeviceMemoryAllocatorO* ptInst; // opaque pointer
    plDeviceMemoryAllocation (*allocate)(struct plDeviceMemoryAllocatorO* instPtr, uint32_t typeFilter, uint64_t size, uint64_t alignment, const char* name);
    void                     (*free)    (struct plDeviceMemoryAllocatorO* instPtr, plDeviceMemoryAllocation* allocation);
} plDeviceMemoryAllocatorI;

//------------------------------resources--------------------------------------

typedef struct _plSamplerDesc
{
    float         fMinMip;
    float         fMaxMip;
    float         fMaxAnisotropy;
    bool          bUnnormalizedCoordinates;
    plFilter      tMagFilter;
    plFilter      tMinFilter;
    plMipmapMode  tMipmapMode;
    plCompareMode tCompare;
    plAddressMode tUAddressMode;
    plAddressMode tVAddressMode;
    plAddressMode tWAddressMode;
    const char*   pcDebugName; // default: "unnamed sampler"
} plSamplerDesc;

typedef struct _plSampler
{
    plSamplerDesc tDesc;

    // [INTERNAL]
    uint16_t _uGeneration;
} plSampler;

typedef struct _plTextureDesc
{
    plVec3         tDimensions;
    uint32_t       uLayers;
    uint32_t       uMips;
    plSampleCount  tSampleCount;
    plFormat       tFormat;
    plTextureType  tType;
    plTextureUsage tUsage;
    const char*    pcDebugName; // default: "unnamed texture"
} plTextureDesc;

typedef struct _plTextureViewDesc
{
    plFormat        tFormat; 
    uint32_t        uBaseMip;
    uint32_t        uMips;
    uint32_t        uBaseLayer;
    uint32_t        uLayerCount;
    plTextureHandle tTexture;
    plTextureType   tType;
    const char*     pcDebugName; // default: "unnamed texture view"
} plTextureViewDesc;

typedef struct _plTexture
{
    plTextureDesc              tDesc;
    plTextureViewDesc          tView;
    plDeviceMemoryRequirements tMemoryRequirements; // filled out when using (create_texture)
    plDeviceMemoryAllocation   tMemoryAllocation;   // filled out when using (bind_texture_to_memory)

    // [INTERNAL]
    uint16_t _uGeneration;
} plTexture;

typedef struct _plBufferDesc
{
    plBufferUsage tUsage;
    size_t        szByteSize;
    const char*   pcDebugName; // default: "unnamed buffer"
} plBufferDesc;

typedef struct _plBuffer
{
    plBufferDesc               tDesc;
    plDeviceMemoryRequirements tMemoryRequirements; // filled out when using (create_buffer)
    plDeviceMemoryAllocation   tMemoryAllocation;   // filled out when using (bind_buffer_to_memory)

    // [INTERNAL]
    uint16_t _uGeneration;
} plBuffer;

//-----------------------------bind groups-------------------------------------

typedef struct _plSamplerBinding
{
    uint32_t           uSlot;
    plShaderStageFlags tStages;
} plSamplerBinding;

typedef struct _plTextureBinding
{
    plTextureBindingType tType;
    uint32_t             uSlot;
    uint32_t             uDescriptorCount; // 0 - will become 1
    plShaderStageFlags   tStages;
    bool                 bNonUniformIndexing; // only available if device capability has PL_DEVICE_CAPABILITY_BIND_GROUP_INDEXING
} plTextureBinding;

typedef struct _plBufferBinding
{
    plBufferBindingType tType;
    uint32_t            uSlot;
    size_t              szSize;
    size_t              szOffset;
    plShaderStageFlags  tStages;
} plBufferBinding;

typedef struct _plDynamicBinding
{
    uint16_t uBufferHandle;
    uint32_t uByteOffset;
    char*    pcData;
} plDynamicBinding;

typedef struct _plDynamicDataBlock
{
    // [INTERNAL]
    uint16_t _uBufferHandle;
    uint32_t _uByteSize;
    uint32_t _uAlignment;
    uint32_t _uBumpAmount;
    char*    _pcData;
    uint32_t _uCurrentOffset;
} plDynamicDataBlock;

typedef struct _plBindGroupUpdateSamplerData
{
    plSamplerHandle tSampler;
    uint32_t        uSlot;
} plBindGroupUpdateSamplerData;

typedef struct _plBindGroupUpdateTextureData
{
    plTextureHandle      tTexture;
    plTextureBindingType tType;
    uint32_t             uSlot;
    uint32_t             uIndex;
    plTextureUsage       tCurrentUsage;
} plBindGroupUpdateTextureData;

typedef struct _plBindGroupUpdateBufferData
{
    plBufferHandle tBuffer;
    uint32_t       uSlot;
    size_t         szBufferRange;

    // [INTERNAL]
    size_t _szOffset; // vulkan only :(
} plBindGroupUpdateBufferData;

typedef struct _plBindGroupUpdateData
{
    uint32_t                            uBufferCount;
    uint32_t                            uTextureCount;
    uint32_t                            uSamplerCount;
    const plBindGroupUpdateBufferData*  atBufferBindings;
    const plBindGroupUpdateTextureData* atTextureBindings;
    const plBindGroupUpdateSamplerData* atSamplerBindings;
} plBindGroupUpdateData;

typedef struct _plPassTextureResource
{
    plTextureHandle          tHandle;
    plShaderStageFlags       tStages;
    plPassResourceUsageFlags tUsage;
} plPassTextureResource;

typedef struct _plPassBufferResource
{
    plBufferHandle           tHandle;
    plShaderStageFlags       tStages;
    plPassResourceUsageFlags tUsage;
} plPassBufferResource;

typedef struct _plPassResources
{
    uint32_t                     uBufferCount;
    uint32_t                     uTextureCount;
    const plPassBufferResource*  atBuffers;
    const plPassTextureResource* atTextures;
} plPassResources;

typedef struct _plBindGroupLayout
{

    plTextureBinding atTextureBindings[PL_MAX_TEXTURES_PER_BIND_GROUP];
    plBufferBinding  atBufferBindings[PL_MAX_BUFFERS_PER_BIND_GROUP];
    plSamplerBinding atSamplerBindings[PL_MAX_SAMPLERS_PER_BIND_GROUP];

    // [INTERNAL]
    uint32_t _uHandle;
    uint32_t _uTextureBindingCount;
    uint32_t _uBufferBindingCount;
    uint32_t _uSamplerBindingCount;
} plBindGroupLayout;

typedef struct _plBindGroupPoolDesc
{
    plBindGroupPoolFlags tFlags;
    size_t               szSamplerBindings;
    size_t               szUniformBufferBindings;
    size_t               szStorageBufferBindings;
    size_t               szSampledTextureBindings;
    size_t               szStorageTextureBindings;
    size_t               szAttachmentTextureBindings;
} plBindGroupPoolDesc;

typedef struct _plBindGroupDesc
{
    const plBindGroupLayout* ptLayout;
    plBindGroupPool*         ptPool;     // pool to allocator from
    const char*              pcDebugName; // default: "unnamed bind group"
} plBindGroupDesc;

typedef struct _plBindGroup
{
    plBindGroupLayout tLayout;

    // [INTERNAL]
    uint16_t _uGeneration;
    plTextureHandle* _sbtTextures;
} plBindGroup;

//-------------------------------shaders---------------------------------------

typedef struct _plShaderModule
{
    size_t      szCodeSize;
    uint8_t*    puCode;
    const char* pcEntryFunc;
} plShaderModule;

typedef struct _plSpecializationConstant
{
    uint32_t   uID;
    uint32_t   uOffset;
    plDataType tType;
} plSpecializationConstant;

typedef struct _plComputeShaderDesc
{
    plShaderModule           tShader;
    plBindGroupLayout        atBindGroupLayouts[3];
    plSpecializationConstant atConstants[PL_MAX_SHADER_SPECIALIZATION_CONSTANTS];
    const void*              pTempConstantData;

    // [INTERNAL]
    uint32_t _uConstantCount;
    uint32_t _uBindGroupLayoutCount;
} plComputeShaderDesc;

typedef struct _plComputeShader
{
    plComputeShaderDesc tDesc;

    // [INTERNAL]
    uint16_t _uGeneration;
} plComputeShader;

typedef struct _plBlendState
{
    bool          bBlendEnabled;
    plBlendOp     tColorOp;
    plBlendOp     tAlphaOp;
    plBlendFactor tSrcColorFactor;
    plBlendFactor tDstColorFactor;
    plBlendFactor tSrcAlphaFactor;
    plBlendFactor tDstAlphaFactor;
} plBlendState;

typedef struct _plVertexAttribute
{
    uint32_t       uByteOffset;
    plVertexFormat tFormat;
} plVertexAttribute;

typedef struct _plVertexBufferLayout
{
    uint32_t          uByteStride;
    plVertexAttribute atAttributes[PL_MAX_VERTEX_ATTRIBUTES];
} plVertexBufferLayout;

typedef struct _plGraphicsState
{
    union 
    {
        struct
        {
            uint64_t ulDepthMode          :  4; // PL_COMPARE_MODE_XXXX
            uint64_t ulWireframe          :  1; // bool
            uint64_t ulDepthWriteEnabled  :  1; // bool
            uint64_t ulDepthClampEnabled  :  1; // bool
            uint64_t ulCullMode           :  2; // PL_CULL_MODE_XXXX
            uint64_t ulStencilTestEnabled :  1; // bool
            uint64_t ulStencilMode        :  4; // PL_COMPARE_MODE_XXXX
            uint64_t ulStencilRef         :  8; 
            uint64_t ulStencilMask        :  8;
            uint64_t ulStencilOpFail      :  3; // PL_STENCIL_OP_XXXX  
            uint64_t ulStencilOpDepthFail :  3; // PL_STENCIL_OP_XXXX
            uint64_t ulStencilOpPass      :  3; // PL_STENCIL_OP_XXXX
            uint64_t _ulUnused            : 25;
        };
        uint64_t ulValue;
    };
    
} plGraphicsState;

typedef struct _plShaderDesc
{
    plSpecializationConstant atConstants[PL_MAX_SHADER_SPECIALIZATION_CONSTANTS];
    uint32_t                 uSubpassIndex;
    plGraphicsState          tGraphicsState;
    plBlendState             atBlendStates[PL_MAX_RENDER_TARGETS];
    plVertexBufferLayout     atVertexBufferLayouts[1];
    plShaderModule           tVertexShader;
    plShaderModule           tPixelShader;
    const void*              pTempConstantData;
    plBindGroupLayout        atBindGroupLayouts[3];
    plRenderPassLayoutHandle tRenderPassLayout;    
    plSampleCount            tMSAASampleCount;

    // [INTERNAL]
    uint32_t _uConstantCount;
    uint32_t _uBindGroupLayoutCount;
} plShaderDesc;

typedef struct _plShader
{
    plShaderDesc tDesc;

    // [INTERNAL]
    uint16_t _uGeneration;
} plShader;

//----------------------------render passes------------------------------------

typedef struct _plSubpass
{
    uint32_t uRenderTargetCount;
    uint32_t uSubpassInputCount;
    uint32_t auRenderTargets[PL_MAX_RENDER_TARGETS];
    uint32_t auSubpassInputs[PL_MAX_RENDER_TARGETS];

    // [INTERNAL]
    uint32_t _uColorAttachmentCount;
} plSubpass;

typedef struct _plSubpassDependency
{
    uint32_t             uSourceSubpass;      // set to UINT32_MAX for external
    uint32_t             uDestinationSubpass; // set to UINT32_MAX for external
    plPipelineStageFlags tSourceStageMask;
    plPipelineStageFlags tDestinationStageMask;
    plAccessFlags        tSourceAccessMask;
    plAccessFlags        tDestinationAccessMask;
} plSubpassDependency;

typedef struct _plRenderTarget
{
    plFormat      tFormat;
    plSampleCount tSamples;
    bool          bResolve;
    bool          bDepth;
} plRenderTarget;

typedef struct _plRenderPassLayoutDesc
{
    plRenderTarget      atRenderTargets[PL_MAX_RENDER_TARGETS];
    plSubpass           atSubpasses[PL_MAX_SUBPASSES];
    plSubpassDependency atSubpassDependencies[PL_MAX_SUBPASSES];

    // [INTERNAL]
    uint32_t _uSubpassCount;
} plRenderPassLayoutDesc;

typedef struct _plRenderPassLayout
{
    plRenderPassLayoutDesc tDesc;

    // [INTERNAL]
    uint32_t _uAttachmentCount;
    uint16_t _uGeneration;
} plRenderPassLayout;

typedef struct _plRenderPassAttachments
{
    plTextureHandle atViewAttachments[PL_MAX_RENDER_TARGETS];
} plRenderPassAttachments;

typedef struct _plDepthTarget
{
    plLoadOp       tLoadOp;
    plStoreOp      tStoreOp;
    plLoadOp       tStencilLoadOp;
    plStoreOp      tStencilStoreOp;
    plTextureUsage tCurrentUsage;
    plTextureUsage tNextUsage;
    float          fClearZ;
    uint32_t       uClearStencil;
} plDepthTarget;

typedef struct _plColorTarget
{
    plLoadOp       tLoadOp;
    plStoreOp      tStoreOp;
    plTextureUsage tCurrentUsage;
    plTextureUsage tNextUsage;
    plVec4         tClearColor;
} plColorTarget;

typedef struct _plRenderPassDesc
{
    plRenderPassLayoutHandle tLayout;
    plColorTarget            atColorTargets[PL_MAX_RENDER_TARGETS];
    plColorTarget            tResolveTarget;
    plDepthTarget            tDepthTarget;
    plVec2                   tDimensions;
    plSwapchain*             ptSwapchain;
} plRenderPassDesc;

typedef struct _plRenderPass
{
    plRenderPassDesc tDesc;

    // [INTERNAL]
    uint16_t _uGeneration;
} plRenderPass;

//---------------------------command buffers-----------------------------------

typedef struct _plCommandPoolDesc
{
    // [INTERNAL]
    int _iUnused;
} plCommandPoolDesc;

typedef struct _plBeginCommandInfo
{
    uint32_t             uWaitSemaphoreCount;
    plTimelineSemaphore* atWaitSempahores[PL_MAX_TIMELINE_SEMAPHORES];
    uint64_t             auWaitSemaphoreValues[PL_MAX_TIMELINE_SEMAPHORES + 1];
} plBeginCommandInfo;

typedef struct _plSubmitInfo
{
    uint32_t             uSignalSemaphoreCount;
    plTimelineSemaphore* atSignalSempahores[PL_MAX_TIMELINE_SEMAPHORES];
    uint64_t             auSignalSemaphoreValues[PL_MAX_TIMELINE_SEMAPHORES + 1];
} plSubmitInfo;

//--------------------------------misc-----------------------------------------

typedef struct _plRenderViewport
{
    float fX;
    float fY;
    float fWidth;
    float fHeight;
    float fMinDepth;
    float fMaxDepth;
} plRenderViewport;

typedef struct _plScissor
{
    int      iOffsetX;
    int      iOffsetY;
    uint32_t uWidth;
    uint32_t uHeight;
} plScissor;

typedef struct _plBufferImageCopy
{
    size_t         szBufferOffset;
    int            iImageOffsetX;
    int            iImageOffsetY;
    int            iImageOffsetZ;
    uint32_t       uImageWidth;
    uint32_t       uImageHeight;
    uint32_t       uImageDepth;
    uint32_t       uMipLevel;
    uint32_t       uBaseArrayLayer;
    uint32_t       uLayerCount;
    uint32_t       uBufferRowLength;
    uint32_t       uBufferImageHeight;
    plTextureUsage tCurrentImageUsage;
} plBufferImageCopy;

typedef struct _plImageCopy
{
    // source info
    int            iSourceOffsetX;
    int            iSourceOffsetY;
    int            iSourceOffsetZ;
    uint32_t       uSourceExtentX;
    uint32_t       uSourceExtentY;
    uint32_t       uSourceExtentZ;
    uint32_t       uSourceMipLevel;
    uint32_t       uSourceBaseArrayLayer;
    uint32_t       uSourceLayerCount;
    plTextureUsage tSourceImageUsage;

    // destination offset
    int            iDestinationOffsetX;
    int            iDestinationOffsetY;
    int            iDestinationOffsetZ;
    uint32_t       uDestinationMipLevel;
    uint32_t       uDestinationBaseArrayLayer;
    uint32_t       uDestinationLayerCount;
    plTextureUsage tDestinationImageUsage;
} plImageCopy; 

typedef struct _plDrawArea
{
    plRenderViewport atViewports[PL_MAX_VIEWPORTS];
    plScissor        atScissors[PL_MAX_VIEWPORTS];
    plDrawStream*    ptDrawStream;
} plDrawArea;

typedef struct _plDispatch
{
    uint32_t uThreadPerGroupX;
    uint32_t uThreadPerGroupY;
    uint32_t uThreadPerGroupZ;
    uint32_t uGroupCountX;
    uint32_t uGroupCountY;
    uint32_t uGroupCountZ;
} plDispatch;

typedef struct _plDrawStreamData
{
    plShaderHandle    tShader;
    plBindGroupHandle atBindGroups[3];
    uint16_t          auDynamicBuffers[1];
    plBufferHandle    tIndexBuffer;
    plBufferHandle    atVertexBuffers[1];
    uint32_t          uIndexOffset;
    uint32_t          uVertexOffset;
    uint32_t          uInstanceOffset;
    uint32_t          uInstanceCount;
    uint32_t          auDynamicBufferOffsets[1];
    uint32_t          uTriangleCount;
} plDrawStreamData;

typedef struct _plDrawStream
{
    // [INTERNAL]
    plDrawStreamData _tCurrentDraw;
    uint32_t         _uStreamCount;
    uint32_t         _uStreamCapacity;
    uint32_t*        _auStream;
} plDrawStream;

typedef struct _plDraw
{
    uint32_t uVertexStart;
    uint32_t uVertexCount;
    uint32_t uInstance;
    uint32_t uInstanceCount;
} plDraw;

typedef struct _plDrawIndex
{
    uint32_t       uIndexStart;
    uint32_t       uIndexCount;
    uint32_t       uVertexStart;
    uint32_t       uInstance;
    uint32_t       uInstanceCount;
    plBufferHandle tIndexBuffer;
} plDrawIndex;

typedef struct _plGraphicsInit
{
    uint32_t            uFramesInFlight; // default: 2
    plGraphicsInitFlags tFlags;
} plGraphicsInit;

typedef struct _plSwapchainInfo
{
    bool          bVSync;
    uint32_t      uWidth;
    uint32_t      uHeight;
    plFormat      tFormat;
    plSampleCount tSampleCount;
} plSwapchainInfo;

typedef struct _plSwapchainInit
{
    bool          bVSync;
    uint32_t      uWidth;
    uint32_t      uHeight;
    plSampleCount tSampleCount;
} plSwapchainInit;

typedef struct _plDeviceLimits
{
    uint32_t uMaxTextureSize; // width or height
    uint32_t uMinUniformBufferOffsetAlignment;
} plDeviceLimits;

typedef struct _plDeviceInfo
{
    char               acName[256];
    plVendorId         tVendorId;
    plDeviceType       tType;
    plDeviceLimits     tLimits;
    size_t             szDeviceMemory;
    size_t             szHostMemory;
    plDeviceCapability tCapabilities;
    plSampleCount      tMaxSampleCount;
} plDeviceInfo;

typedef struct _plDeviceInit
{
    plDeviceInitFlags tFlags;
    uint32_t          uDeviceIdx;
    size_t            szDynamicBufferBlockSize;
    size_t            szDynamicDataMaxSize;
    plSurface*        ptSurface;
} plDeviceInit;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plGraphicsBackend
{
    PL_GRAPHICS_BACKEND_NONE = 0,
    PL_GRAPHICS_BACKEND_CPU,
    PL_GRAPHICS_BACKEND_VULKAN,
    PL_GRAPHICS_BACKEND_METAL
};

enum _plPassResourceUsageFlags
{
    PL_PASS_RESOURCE_USAGE_NONE  = 0,
    PL_PASS_RESOURCE_USAGE_READ  = 1 << 0,
    PL_PASS_RESOURCE_USAGE_WRITE = 1 << 1,
};

enum _plBindGroupPoolFlags
{
    PL_BIND_GROUP_POOL_FLAGS_NONE             = 0,
    PL_BIND_GROUP_POOL_FLAGS_INDIVIDUAL_RESET = 1 << 0,
};

enum _plMipmapMode
{
    PL_MIPMAP_MODE_LINEAR,
    PL_MIPMAP_MODE_NEAREST
};

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

enum _plAddressMode
{
    PL_ADDRESS_MODE_UNSPECIFIED,
    PL_ADDRESS_MODE_WRAP,
    PL_ADDRESS_MODE_CLAMP,
    PL_ADDRESS_MODE_MIRROR
};

enum _plSampleCount
{
    PL_SAMPLE_COUNT_UNSPECIFIED,
    PL_SAMPLE_COUNT_1  = 1 << 0,
    PL_SAMPLE_COUNT_2  = 1 << 1,
    PL_SAMPLE_COUNT_4  = 1 << 2,
    PL_SAMPLE_COUNT_8  = 1 << 3,
    PL_SAMPLE_COUNT_16 = 1 << 4,
    PL_SAMPLE_COUNT_32 = 1 << 5,
    PL_SAMPLE_COUNT_64 = 1 << 6
};

enum _plTextureType
{
    PL_TEXTURE_TYPE_UNSPECIFIED,
    PL_TEXTURE_TYPE_2D,
    PL_TEXTURE_TYPE_CUBE,
    PL_TEXTURE_TYPE_2D_ARRAY
};

enum _plTextureUsage
{
    PL_TEXTURE_USAGE_UNSPECIFIED              = 0,
    PL_TEXTURE_USAGE_SAMPLED                  = 1 << 0,
    PL_TEXTURE_USAGE_COLOR_ATTACHMENT         = 1 << 1,
    PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT = 1 << 2,
    PL_TEXTURE_USAGE_TRANSIENT_ATTACHMENT     = 1 << 3,
    PL_TEXTURE_USAGE_PRESENT                  = 1 << 4,
    PL_TEXTURE_USAGE_INPUT_ATTACHMENT         = 1 << 5,
    PL_TEXTURE_USAGE_STORAGE                  = 1 << 6
};

enum _plBufferUsage
{
    PL_BUFFER_USAGE_UNSPECIFIED,
    PL_BUFFER_USAGE_INDEX,
    PL_BUFFER_USAGE_VERTEX,
    PL_BUFFER_USAGE_UNIFORM,
    PL_BUFFER_USAGE_STORAGE,
    PL_BUFFER_USAGE_STAGING,
};

enum _plVendorId
{
    PL_VENDOR_ID_NONE = 0,
    PL_VENDOR_ID_SOFTWARE_RASTERIZER,
    PL_VENDOR_ID_AMD,
    PL_VENDOR_ID_APPLE,
    PL_VENDOR_ID_INTEL,
    PL_VENDOR_ID_NVIDIA
};

enum _plDeviceType
{
    PL_DEVICE_TYPE_NONE = 0,
    PL_DEVICE_TYPE_INTEGRATED,
    PL_DEVICE_TYPE_DISCRETE,
    PL_DEVICE_TYPE_CPU
};

enum _plDeviceCapability
{
    PL_DEVICE_CAPABILITY_NONE                = 0,
    PL_DEVICE_CAPABILITY_SWAPCHAIN           = 1 << 0,
    PL_DEVICE_CAPABILITY_BIND_GROUP_INDEXING = 1 << 1,
    PL_DEVICE_CAPABILITY_SAMPLER_ANISOTROPY  = 1 << 2,
    PL_DEVICE_CAPABILITY_MULTIPLE_VIEWPORTS  = 1 << 3
};

enum _plCommandPoolResetFlags
{
    PL_COMMAND_POOL_RESET_FLAG_NONE           = 0,
    PL_COMMAND_POOL_RESET_FLAG_FREE_RESOURCES = 1 << 0,
};

enum _plDeviceInitFlags
{
    PL_DEVICE_INIT_FLAGS_NONE = 0
};

enum _plGraphicsInitFlags
{
    PL_GRAPHICS_INIT_FLAGS_NONE                = 0,
    PL_GRAPHICS_INIT_FLAGS_VALIDATION_ENABLED  = 1 << 0,
    PL_GRAPHICS_INIT_FLAGS_SWAPCHAIN_ENABLED   = 1 << 1
};

enum _plShaderStageFlags
{
    PL_SHADER_STAGE_NONE     = 0,
    PL_SHADER_STAGE_VERTEX   = 1 << 0,
    PL_SHADER_STAGE_FRAGMENT = 1 << 1,
    PL_SHADER_STAGE_COMPUTE  = 1 << 2,
    PL_SHADER_STAGE_TRANSFER = 1 << 3,
    PL_SHADER_STAGE_ALL      = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_COMPUTE | PL_SHADER_STAGE_TRANSFER
};

enum _plPipelineStageFlags
{
    PL_PIPELINE_STAGE_NONE                    = 0,
    PL_PIPELINE_STAGE_VERTEX_INPUT            = 1 << 0,
    PL_PIPELINE_STAGE_VERTEX_SHADER           = 1 << 1,
    PL_PIPELINE_STAGE_FRAGMENT_SHADER         = 1 << 2,
    PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS    = 1 << 3,
    PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS     = 1 << 4,
    PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT = 1 << 5,
    PL_PIPELINE_STAGE_COMPUTE_SHADER          = 1 << 6,
    PL_PIPELINE_STAGE_TRANSFER                = 1 << 7,
};

enum _plAccessFlags
{
    PL_ACCESS_NONE                           = 0,
    PL_ACCESS_SHADER_READ                    = 1 << 0,
    PL_ACCESS_SHADER_WRITE                   = 1 << 1,
    PL_ACCESS_TRANSFER_WRITE                 = 1 << 2,
    PL_ACCESS_TRANSFER_READ                  = 1 << 3,
    PL_ACCESS_INPUT_ATTACHMENT_READ          = 1 << 4,
    PL_ACCESS_COLOR_ATTACHMENT_READ          = 1 << 5,
    PL_ACCESS_COLOR_ATTACHMENT_WRITE         = 1 << 6,
    PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ  = 1 << 7,
    PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE = 1 << 8,
};

enum _plCullMode
{
    PL_CULL_MODE_NONE       = 0,
    PL_CULL_MODE_CULL_FRONT = 1 << 0,
    PL_CULL_MODE_CULL_BACK  = 1 << 1,
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
    PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT,
    PL_TEXTURE_BINDING_TYPE_STORAGE
};

enum _plStencilOp
{
    PL_STENCIL_OP_KEEP,
    PL_STENCIL_OP_ZERO,
    PL_STENCIL_OP_REPLACE,
    PL_STENCIL_OP_INCREMENT_AND_CLAMP,
    PL_STENCIL_OP_DECREMENT_AND_CLAMP,
    PL_STENCIL_OP_INVERT,
    PL_STENCIL_OP_INCREMENT_AND_WRAP,
    PL_STENCIL_OP_DECREMENT_AND_WRAP
};

enum _plStoreOp
{
    PL_STORE_OP_STORE,
    PL_STORE_OP_STORE_MULTISAMPLE_RESOLVE,
    PL_STORE_OP_DONT_CARE,
    PL_STORE_OP_NONE
};

enum _plBlendOp
{
    PL_BLEND_OP_ADD,
    PL_BLEND_OP_SUBTRACT,
    PL_BLEND_OP_REVERSE_SUBTRACT,
    PL_BLEND_OP_MIN,
    PL_BLEND_OP_MAX
};

enum _plBlendFactor
{
    PL_BLEND_FACTOR_ZERO,
    PL_BLEND_FACTOR_ONE,
    PL_BLEND_FACTOR_SRC_COLOR,
    PL_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
    PL_BLEND_FACTOR_DST_COLOR,
    PL_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
    PL_BLEND_FACTOR_SRC_ALPHA,
    PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    PL_BLEND_FACTOR_DST_ALPHA,
    PL_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
    PL_BLEND_FACTOR_CONSTANT_COLOR,
    PL_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
    PL_BLEND_FACTOR_CONSTANT_ALPHA,
    PL_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,
    PL_BLEND_FACTOR_SRC_ALPHA_SATURATE,
    PL_BLEND_FACTOR_SRC1_COLOR,
    PL_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR,
    PL_BLEND_FACTOR_SRC1_ALPHA,
    PL_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA,
};

enum _plLoadOp
{
    PL_LOAD_OP_LOAD,
    PL_LOAD_OP_CLEAR,
    PL_LOAD_OP_DONT_CARE
};

enum _plMemoryMode
{
    PL_MEMORY_GPU,
    PL_MEMORY_GPU_CPU,
    PL_MEMORY_CPU
};

enum _plDataType
{
    PL_DATA_TYPE_UNSPECIFIED,
    PL_DATA_TYPE_BOOL,
    PL_DATA_TYPE_BOOL2,
    PL_DATA_TYPE_BOOL3,
    PL_DATA_TYPE_BOOL4,
    PL_DATA_TYPE_FLOAT,
    PL_DATA_TYPE_FLOAT2,
    PL_DATA_TYPE_FLOAT3,
    PL_DATA_TYPE_FLOAT4,
    PL_DATA_TYPE_BYTE,
    PL_DATA_TYPE_BYTE2,
    PL_DATA_TYPE_BYTE3,
    PL_DATA_TYPE_BYTE4,
    PL_DATA_TYPE_UNSIGNED_BYTE,
    PL_DATA_TYPE_UNSIGNED_BYTE2,
    PL_DATA_TYPE_UNSIGNED_BYTE3,
    PL_DATA_TYPE_UNSIGNED_BYTE4,
    PL_DATA_TYPE_SHORT,
    PL_DATA_TYPE_SHORT2,
    PL_DATA_TYPE_SHORT3,
    PL_DATA_TYPE_SHORT4,
    PL_DATA_TYPE_UNSIGNED_SHORT,
    PL_DATA_TYPE_UNSIGNED_SHORT2,
    PL_DATA_TYPE_UNSIGNED_SHORT3,
    PL_DATA_TYPE_UNSIGNED_SHORT4,
    PL_DATA_TYPE_INT,
    PL_DATA_TYPE_INT2,
    PL_DATA_TYPE_INT3,
    PL_DATA_TYPE_INT4,
    PL_DATA_TYPE_UNSIGNED_INT,
    PL_DATA_TYPE_UNSIGNED_INT2,
    PL_DATA_TYPE_UNSIGNED_INT3,
    PL_DATA_TYPE_UNSIGNED_INT4,
    PL_DATA_TYPE_LONG,
    PL_DATA_TYPE_LONG2,
    PL_DATA_TYPE_LONG3,
    PL_DATA_TYPE_LONG4,
    PL_DATA_TYPE_UNSIGNED_LONG,
    PL_DATA_TYPE_UNSIGNED_LONG2,
    PL_DATA_TYPE_UNSIGNED_LONG3,
    PL_DATA_TYPE_UNSIGNED_LONG4,
};

enum _plVertexFormat
{
    PL_VERTEX_FORMAT_UNKNOWN = 0,

    PL_VERTEX_FORMAT_HALF,
    PL_VERTEX_FORMAT_HALF2,
    PL_VERTEX_FORMAT_HALF3,
    PL_VERTEX_FORMAT_HALF4,

    PL_VERTEX_FORMAT_FLOAT,
    PL_VERTEX_FORMAT_FLOAT2,
    PL_VERTEX_FORMAT_FLOAT3,
    PL_VERTEX_FORMAT_FLOAT4,

    PL_VERTEX_FORMAT_UCHAR,
    PL_VERTEX_FORMAT_UCHAR2,
    PL_VERTEX_FORMAT_UCHAR3,
    PL_VERTEX_FORMAT_UCHAR4,

    PL_VERTEX_FORMAT_CHAR,
    PL_VERTEX_FORMAT_CHAR2,
    PL_VERTEX_FORMAT_CHAR3,
    PL_VERTEX_FORMAT_CHAR4,

    PL_VERTEX_FORMAT_USHORT,
    PL_VERTEX_FORMAT_USHORT2,
    PL_VERTEX_FORMAT_USHORT3,
    PL_VERTEX_FORMAT_USHORT4,

    PL_VERTEX_FORMAT_SHORT,
    PL_VERTEX_FORMAT_SHORT2,
    PL_VERTEX_FORMAT_SHORT3,
    PL_VERTEX_FORMAT_SHORT4,   
    
    PL_VERTEX_FORMAT_UINT,
    PL_VERTEX_FORMAT_UINT2,
    PL_VERTEX_FORMAT_UINT3,
    PL_VERTEX_FORMAT_UINT4,

    PL_VERTEX_FORMAT_INT,
    PL_VERTEX_FORMAT_INT2,
    PL_VERTEX_FORMAT_INT3,
    PL_VERTEX_FORMAT_INT4,    

    PL_VERTEX_FORMAT_COUNT
};

enum _plFormat
{
    PL_FORMAT_UNKNOWN = 0,

    // 8-bit pixel formats
    PL_FORMAT_R8_UNORM,
    PL_FORMAT_R8_SNORM,
    PL_FORMAT_R8_UINT,
    PL_FORMAT_R8_SINT,
    PL_FORMAT_R8_SRGB,

    // 16-bit pixel formats
    PL_FORMAT_R8G8_UNORM,
    PL_FORMAT_R16_UNORM,
    PL_FORMAT_R16_SNORM,
    PL_FORMAT_R16_UINT,
    PL_FORMAT_R16_SINT,
    PL_FORMAT_R16_FLOAT,
    PL_FORMAT_R8G8_SNORM,
    PL_FORMAT_R8G8_UINT,
    PL_FORMAT_R8G8_SINT,
    PL_FORMAT_R8G8_SRGB,

    // packed 16-bit pixel formats
    PL_FORMAT_B5G6R5_UNORM,
    PL_FORMAT_A1R5G5B5_UNORM,
    PL_FORMAT_B5G5R5A1_UNORM,

    // 32-bit pixel formats
    PL_FORMAT_R8G8B8A8_SRGB,
    PL_FORMAT_B8G8R8A8_SRGB,
    PL_FORMAT_B8G8R8A8_UNORM,
    PL_FORMAT_R8G8B8A8_UNORM,
    PL_FORMAT_R32_UINT,
    PL_FORMAT_R32_SINT,
    PL_FORMAT_R32_FLOAT,
    PL_FORMAT_R16G16_UNORM,
    PL_FORMAT_R16G16_SNORM,
    PL_FORMAT_R16G16_UINT,
    PL_FORMAT_R16G16_SINT,
    PL_FORMAT_R16G16_FLOAT,
    PL_FORMAT_R8G8B8A8_SNORM,
    PL_FORMAT_R8G8B8A8_UINT,
    PL_FORMAT_R8G8B8A8_SINT,

    // packed 32-bit pixel formats
    PL_FORMAT_B10G10R10A2_UNORM,
    PL_FORMAT_R10G10B10A2_UNORM,
    PL_FORMAT_R10G10B10A2_UINT,
    PL_FORMAT_R11G11B10_FLOAT,
    PL_FORMAT_R9G9B9E5_FLOAT,

    // 64-bit pixel formats
    PL_FORMAT_R32G32_FLOAT,
    PL_FORMAT_R32G32_UINT,
    PL_FORMAT_R32G32_SINT,
    PL_FORMAT_R16G16B16A16_UNORM,
    PL_FORMAT_R16G16B16A16_SNORM,
    PL_FORMAT_R16G16B16A16_UINT,
    PL_FORMAT_R16G16B16A16_SINT,
    PL_FORMAT_R16G16B16A16_FLOAT,

    // 128-bit pixel formats
    PL_FORMAT_R32G32B32A32_FLOAT,
    PL_FORMAT_R32G32B32A32_UINT,
    PL_FORMAT_R32G32B32A32_SINT,

    // compressed bc pixel formats
    PL_FORMAT_BC1_RGBA_UNORM,
    PL_FORMAT_BC1_RGBA_SRGB,
    PL_FORMAT_BC2_UNORM,
    PL_FORMAT_BC2_SRGB,
    PL_FORMAT_BC3_UNORM,
    PL_FORMAT_BC3_SRGB,
    PL_FORMAT_BC4_UNORM,
    PL_FORMAT_BC4_SNORM,
    PL_FORMAT_BC5_UNORM,
    PL_FORMAT_BC5_SNORM,
    PL_FORMAT_BC6H_UFLOAT, 
    PL_FORMAT_BC6H_FLOAT,
    PL_FORMAT_BC7_UNORM,
    PL_FORMAT_BC7_SRGB,

    // compressed eac/etc pixel formats
    PL_FORMAT_ETC2_R8G8B8_UNORM,
    PL_FORMAT_ETC2_R8G8B8_SRGB,
    PL_FORMAT_ETC2_R8G8B8A1_UNORM,
    PL_FORMAT_ETC2_R8G8B8A1_SRGB,
    PL_FORMAT_EAC_R11_UNORM,
    PL_FORMAT_EAC_R11_SNORM,
    PL_FORMAT_EAC_R11G11_UNORM,
    PL_FORMAT_EAC_R11G11_SNORM,

    // compressed astc pixel formats
    PL_FORMAT_ASTC_4x4_UNORM,
    PL_FORMAT_ASTC_4x4_SRGB,
    PL_FORMAT_ASTC_5x4_UNORM,
    PL_FORMAT_ASTC_5x4_SRGB,
    PL_FORMAT_ASTC_5x5_UNORM,
    PL_FORMAT_ASTC_5x5_SRGB,
    PL_FORMAT_ASTC_6x5_UNORM,
    PL_FORMAT_ASTC_6x5_SRGB,
    PL_FORMAT_ASTC_6x6_UNORM,
    PL_FORMAT_ASTC_6x6_SRGB,
    PL_FORMAT_ASTC_8x5_UNORM,
    PL_FORMAT_ASTC_8x5_SRGB,
    PL_FORMAT_ASTC_8x6_UNORM,
    PL_FORMAT_ASTC_8x6_SRGB,
    PL_FORMAT_ASTC_8x8_UNORM,
    PL_FORMAT_ASTC_8x8_SRGB,
    PL_FORMAT_ASTC_10x5_UNORM, 
    PL_FORMAT_ASTC_10x5_SRGB,
    PL_FORMAT_ASTC_10x6_UNORM,
    PL_FORMAT_ASTC_10x6_SRGB,
    PL_FORMAT_ASTC_10x8_UNORM,
    PL_FORMAT_ASTC_10x8_SRGB,
    PL_FORMAT_ASTC_10x10_UNORM,
    PL_FORMAT_ASTC_10x10_SRGB,
    PL_FORMAT_ASTC_12x10_UNORM,
    PL_FORMAT_ASTC_12x10_SRGB,
    PL_FORMAT_ASTC_12x12_UNORM,
    PL_FORMAT_ASTC_12x12_SRGB,

    // depth
    PL_FORMAT_D32_FLOAT,
    PL_FORMAT_D32_FLOAT_S8_UINT,
    PL_FORMAT_D24_UNORM_S8_UINT,
    PL_FORMAT_D16_UNORM_S8_UINT,
    PL_FORMAT_D16_UNORM,
    PL_FORMAT_S8_UINT,
    
    PL_FORMAT_COUNT
};

//-----------------------------------------------------------------------------
// [SECTION] internal enums (NOT FOR PUBLIC CONSUMPTION)
//-----------------------------------------------------------------------------

enum plDrawStreamBits
{
    PL_DRAW_STREAM_BIT_NONE             = 0,
    PL_DRAW_STREAM_BIT_SHADER           = 1 << 0,
    PL_DRAW_STREAM_BIT_DYNAMIC_OFFSET_0 = 1 << 1,
    PL_DRAW_STREAM_BIT_DYNAMIC_BUFFER_0 = 1 << 2,
    PL_DRAW_STREAM_BIT_BINDGROUP_2      = 1 << 3,
    PL_DRAW_STREAM_BIT_BINDGROUP_1      = 1 << 4,
    PL_DRAW_STREAM_BIT_BINDGROUP_0      = 1 << 5,
    PL_DRAW_STREAM_BIT_INDEX_OFFSET     = 1 << 6,
    PL_DRAW_STREAM_BIT_VERTEX_OFFSET    = 1 << 7,
    PL_DRAW_STREAM_BIT_INDEX_BUFFER     = 1 << 8,
    PL_DRAW_STREAM_BIT_VERTEX_BUFFER_0  = 1 << 9,
    PL_DRAW_STREAM_BIT_TRIANGLES        = 1 << 10,
    PL_DRAW_STREAM_BIT_INSTANCE_OFFSET  = 1 << 11,
    PL_DRAW_STREAM_BIT_INSTANCE_COUNT   = 1 << 12
};

//-----------------------------------------------------------------------------
// [SECTION] inline API implementations
//-----------------------------------------------------------------------------

static inline plDynamicBinding
pl_allocate_dynamic_data(const plGraphicsI* ptGfx, plDevice* ptDevice, plDynamicDataBlock* ptCurrentBlock)
{
    if(ptCurrentBlock->_uCurrentOffset + ptCurrentBlock->_uBumpAmount > ptCurrentBlock->_uByteSize)
    {
        *ptCurrentBlock = ptGfx->allocate_dynamic_data_block(ptDevice);
    }

    plDynamicBinding tBinding = PL_ZERO_INIT;
    tBinding.uBufferHandle = ptCurrentBlock->_uBufferHandle;
    tBinding.uByteOffset   = ptCurrentBlock->_uCurrentOffset;
    tBinding.pcData        = &ptCurrentBlock->_pcData[ptCurrentBlock->_uCurrentOffset];

    ptCurrentBlock->_uCurrentOffset = ((ptCurrentBlock->_uCurrentOffset + ptCurrentBlock->_uBumpAmount + (ptCurrentBlock->_uAlignment - 1)) & ~(ptCurrentBlock->_uAlignment - 1));
    return tBinding;
}

static inline void
pl_add_to_draw_stream(plDrawStream* ptStream, plDrawStreamData tDraw)
{

    uint32_t uDirtyMask = PL_DRAW_STREAM_BIT_NONE;

    if(ptStream->_tCurrentDraw.tShader.uData != tDraw.tShader.uData)
    {
        ptStream->_tCurrentDraw.tShader = tDraw.tShader;
        uDirtyMask |= PL_DRAW_STREAM_BIT_SHADER;
    }

    if(ptStream->_tCurrentDraw.auDynamicBufferOffsets[0] != tDraw.auDynamicBufferOffsets[0])
    {   
        ptStream->_tCurrentDraw.auDynamicBufferOffsets[0] = tDraw.auDynamicBufferOffsets[0];
        uDirtyMask |= PL_DRAW_STREAM_BIT_DYNAMIC_OFFSET_0;
    }

    if(ptStream->_tCurrentDraw.atBindGroups[0].uData != tDraw.atBindGroups[0].uData)
    {
        ptStream->_tCurrentDraw.atBindGroups[0] = tDraw.atBindGroups[0];
        uDirtyMask |= PL_DRAW_STREAM_BIT_BINDGROUP_0;
    }

    if(ptStream->_tCurrentDraw.atBindGroups[1].uData != tDraw.atBindGroups[1].uData)
    {
        ptStream->_tCurrentDraw.atBindGroups[1] = tDraw.atBindGroups[1];
        uDirtyMask |= PL_DRAW_STREAM_BIT_BINDGROUP_1;
    }

    if(ptStream->_tCurrentDraw.atBindGroups[2].uData != tDraw.atBindGroups[2].uData)
    {
        ptStream->_tCurrentDraw.atBindGroups[2] = tDraw.atBindGroups[2];
        uDirtyMask |= PL_DRAW_STREAM_BIT_BINDGROUP_2;
    }

    if(ptStream->_tCurrentDraw.auDynamicBuffers[0] != tDraw.auDynamicBuffers[0])
    {
        ptStream->_tCurrentDraw.auDynamicBuffers[0] = tDraw.auDynamicBuffers[0];
        uDirtyMask |= PL_DRAW_STREAM_BIT_DYNAMIC_BUFFER_0;
    }

    if(ptStream->_tCurrentDraw.uIndexOffset != tDraw.uIndexOffset)
    {   
        ptStream->_tCurrentDraw.uIndexOffset = tDraw.uIndexOffset;
        uDirtyMask |= PL_DRAW_STREAM_BIT_INDEX_OFFSET;
    }

    if(ptStream->_tCurrentDraw.uVertexOffset != tDraw.uVertexOffset)
    {   
        ptStream->_tCurrentDraw.uVertexOffset = tDraw.uVertexOffset;
        uDirtyMask |= PL_DRAW_STREAM_BIT_VERTEX_OFFSET;
    }

    if(ptStream->_tCurrentDraw.tIndexBuffer.uData != tDraw.tIndexBuffer.uData)
    {
        ptStream->_tCurrentDraw.tIndexBuffer = tDraw.tIndexBuffer;
        uDirtyMask |= PL_DRAW_STREAM_BIT_INDEX_BUFFER;
    }

    if(ptStream->_tCurrentDraw.atVertexBuffers[0].uData != tDraw.atVertexBuffers[0].uData)
    {
        ptStream->_tCurrentDraw.atVertexBuffers[0] = tDraw.atVertexBuffers[0];
        uDirtyMask |= PL_DRAW_STREAM_BIT_VERTEX_BUFFER_0;
    }

    if(ptStream->_tCurrentDraw.uTriangleCount != tDraw.uTriangleCount)
    {
        ptStream->_tCurrentDraw.uTriangleCount = tDraw.uTriangleCount;
        uDirtyMask |= PL_DRAW_STREAM_BIT_TRIANGLES;
    }

    if(ptStream->_tCurrentDraw.uInstanceOffset != tDraw.uInstanceOffset)
    {
        ptStream->_tCurrentDraw.uInstanceOffset = tDraw.uInstanceOffset;
        uDirtyMask |= PL_DRAW_STREAM_BIT_INSTANCE_OFFSET;
    }

    if(ptStream->_tCurrentDraw.uInstanceCount != tDraw.uInstanceCount)
    {
        ptStream->_tCurrentDraw.uInstanceCount = tDraw.uInstanceCount;
        uDirtyMask |= PL_DRAW_STREAM_BIT_INSTANCE_COUNT;
    }

    ptStream->_auStream[ptStream->_uStreamCount++] = uDirtyMask;
    if(uDirtyMask & PL_DRAW_STREAM_BIT_SHADER)
        ptStream->_auStream[ptStream->_uStreamCount++] = ptStream->_tCurrentDraw.tShader.uData;
    if(uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_OFFSET_0)
        ptStream->_auStream[ptStream->_uStreamCount++] = ptStream->_tCurrentDraw.auDynamicBufferOffsets[0];
    if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_0)
        ptStream->_auStream[ptStream->_uStreamCount++] = ptStream->_tCurrentDraw.atBindGroups[0].uData;
    if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_1)
        ptStream->_auStream[ptStream->_uStreamCount++] = ptStream->_tCurrentDraw.atBindGroups[1].uData;
    if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_2)
        ptStream->_auStream[ptStream->_uStreamCount++] = ptStream->_tCurrentDraw.atBindGroups[2].uData;
    if(uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_BUFFER_0)
        ptStream->_auStream[ptStream->_uStreamCount++] = ptStream->_tCurrentDraw.auDynamicBuffers[0];
    if(uDirtyMask & PL_DRAW_STREAM_BIT_INDEX_OFFSET)
        ptStream->_auStream[ptStream->_uStreamCount++] = ptStream->_tCurrentDraw.uIndexOffset;
    if(uDirtyMask & PL_DRAW_STREAM_BIT_VERTEX_OFFSET)
        ptStream->_auStream[ptStream->_uStreamCount++] = ptStream->_tCurrentDraw.uVertexOffset;
    if(uDirtyMask & PL_DRAW_STREAM_BIT_INDEX_BUFFER)
        ptStream->_auStream[ptStream->_uStreamCount++] = ptStream->_tCurrentDraw.tIndexBuffer.uData;
    if(uDirtyMask & PL_DRAW_STREAM_BIT_VERTEX_BUFFER_0)
        ptStream->_auStream[ptStream->_uStreamCount++] = ptStream->_tCurrentDraw.atVertexBuffers[0].uData;
    if(uDirtyMask & PL_DRAW_STREAM_BIT_TRIANGLES)
        ptStream->_auStream[ptStream->_uStreamCount++] = ptStream->_tCurrentDraw.uTriangleCount;
    if(uDirtyMask & PL_DRAW_STREAM_BIT_INSTANCE_OFFSET)
        ptStream->_auStream[ptStream->_uStreamCount++] = ptStream->_tCurrentDraw.uInstanceOffset;
    if(uDirtyMask & PL_DRAW_STREAM_BIT_INSTANCE_COUNT)
        ptStream->_auStream[ptStream->_uStreamCount++] = ptStream->_tCurrentDraw.uInstanceCount;
}

#endif // PL_GRAPHICS_EXT_H