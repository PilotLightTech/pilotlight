/*
   pl_graphics_ext.h
    * a light wrapper over Vulkan & Metal 2.0
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] defines
// [SECTION] apis
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api structs
// [SECTION] structs
// [SECTION] enums
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_GRAPHICS_EXT_H
#define PL_GRAPHICS_EXT_H

// extension version (format XYYZZ)
#define PL_GRAPHICS_EXT_VERSION    "1.0.0"
#define PL_GRAPHICS_EXT_VERSION_NUM 10000

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

// macros
#define PL_DEFINE_HANDLE(x) \
    typedef union x { struct {uint32_t uIndex; uint32_t uGeneration;}; uint64_t ulData; } x;

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define PL_API_GRAPHICS "PL_API_GRAPHICS"
typedef struct _plGraphicsI plGraphicsI;

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
typedef struct _plRenderEncoder   plRenderEncoder;  // opaque type for command buffer encoder for render ops
typedef struct _plComputeEncoder  plComputeEncoder; // opaque type for command buffer encoder for compute ops
typedef struct _plBlitEncoder     plBlitEncoder;    // opaque type for command buffer encoder for blit ops
typedef struct _plBufferImageCopy plBufferImageCopy;// used for copying between buffers & textures with blit encoder

// bind groups
typedef struct _plBindGroup                  plBindGroup;                  // bind group resource
typedef struct _plBindGroupLayout            plBindGroupLayout;            // bind group layout decription
typedef struct _plBindGroupDesc              plBindGroupDesc;              // descriptor for creating bind groups
typedef struct _plBindGroupPool              plBindGroupPool;              // opaque type for bind group pools
typedef struct _plBindGroupPoolDesc          plBindGroupPoolDesc;          // descriptor for creating bind group pools
typedef struct _plBufferBinding              plBufferBinding;              // buffer binding for bind groups
typedef struct _plSamplerBinding             plSamplerBinding;             // sampler binding for bind groups
typedef struct _plDynamicBinding             plDynamicBinding;             // dynamic binding for bind groups
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
typedef int plMipmapMode;            // -> enum _plMipmapMode             // Enum: mipmap filter modes (PL_MIPMAP_MODE_XXXX)
typedef int plAddressMode;           // -> enum _plAddressMode            // Enum: addressing mode sampling textures outside image (PL_ADDRESS_MODE_XXXX)
typedef int plFilter;                // -> enum _plFilter                 // Enum: texture lookup filters (PL_FILTER_XXXX)
typedef int plSampleCount;           // -> enum _plSampleCount            // Enum: texture sample count (PL_SAMPLE_COUNT_XXXX)
typedef int plTextureType;           // -> enum _plTextureType            // Enum: texture type (PL_TEXTURE_TYPE_XXXX)
typedef int plTextureUsage;          // -> enum _plTextureUsage           // Flag: texture type (PL_TEXTURE_USAGE_XXXX)
typedef int plCompareMode;           // -> enum _plCompareMode            // Enum: texture sampling comparison modes (PL_COMPARE_MODE_XXXX)
typedef int plFormat;                // -> enum _plFormat                 // Enum: formats (PL_FORMAT_XXXX)
typedef int plBufferUsage;           // -> enum _plBufferUsage            // Flag: buffer usage flags (PL_BUFFER_USAGE_XXXX)
typedef int plStageFlags;            // -> enum _plStageFlags             // Flag: GPU pipeline stage (PL_STAGE_XXXX)
typedef int plCullMode;              // -> enum _plCullMode               // Flag: face culling mode (PL_CULL_MODE_XXXX)
typedef int plBufferBindingType;     // -> enum _plBufferBindingType      // Enum: buffer binding type for bind groups (PL_BUFFER_BINDING_TYPE_XXXX)
typedef int plTextureBindingType;    // -> enum _plTextureBindingType     // Enum: image binding type for bind groups (PL_TEXTURE_BINDING_TYPE_XXXX)
typedef int plBindGroupPoolFlags;    // -> enum _plBindGroupPoolFlags     // Flags: flags for creating bind group pools (PL_BIND_GROUP_POOL_FLAGS_XXXX)
typedef int plMemoryMode;            // -> enum _plMemoryMode             // Enum: memory modes for allocating memory (PL_MEMORY_XXXX)
typedef int plStencilOp;             // -> enum _plStencilOp              // Enum: stencil operations (PL_STENCIL_OP_XXXX)
typedef int plLoadOp;                // -> enum _plLoadOp                 // Enum: render pass target load operations (PL_LOAD_OP_XXXX)
typedef int plStoreOp;               // -> enum _plStoreOp                // Enum: render pass target store operations (PL_STORE_OP_XXXX)
typedef int plBlendOp;               // -> enum _plBlendOp                // Enum: blend operations (PL_BLEND_OP_XXXX)
typedef int plBlendFactor;           // -> enum _plBlendFactor            // Enum: blend operation factors (PL_BLEND_FACTOR_XXXX)
typedef int plDataType;              // -> enum _plDataType               // Enum: data types
typedef int plGraphicsInitFlags;     // -> enum _plGraphicsInitFlags      // Flags: graphics initialization flags (PL_GRAPHICS_INIT_FLAGS_XXXX)
typedef int plDeviceInitFlags;       // -> enum _plDeviceInitFlags        // Flags: device initialization flags (PL_DEVICE_INIT_FLAGS_XXXX)
typedef int plVendorId;              // -> enum _plVendorId               // Enum: device vendors (PL_VENDOR_ID_XXXX)
typedef int plDeviceType;            // -> enum _plDeviceType             // Enum: device type (PL_DEVICE_TYPE_XXXX)
typedef int plDeviceCapability;      // -> enum _plDeviceCapability       // Flags: device capabilities (PL_DEVICE_CAPABILITY_XXXX)
typedef int plCommandPoolResetFlags; // -> enum _plCommandPoolResetFlags  // Flags: device capabilities (PL_DEVICE_CAPABILITY_XXXX)

// external
typedef struct _plWindow plWindow; // pl_os.h

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plGraphicsI
{
    // context
    bool (*initialize)(const plGraphicsInit*);
    void (*cleanup)   (void);

    // devices
    void      (*enumerate_devices)(plDeviceInfo*, uint32_t* deviceCountOut);
    plDevice* (*create_device)    (const plDeviceInit*);
    void      (*cleanup_device)   (plDevice*);
    void      (*flush_device)     (plDevice*);

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
    plRenderEncoder*   (*begin_render_pass)         (plCommandBuffer*, plRenderPassHandle); // do not store
    void               (*next_subpass)              (plRenderEncoder*);
    void               (*end_render_pass)           (plRenderEncoder*);
    plRenderPassHandle (*get_encoder_render_pass)   (plRenderEncoder*);
    uint32_t           (*get_render_encoder_subpass)(plRenderEncoder*);

    // render encoder: draw stream (preferred system)
    void (*reset_draw_stream)  (plDrawStream*);
    void (*cleanup_draw_stream)(plDrawStream*);
    void (*add_to_stream)      (plDrawStream*, plDrawStreamData);
    void (*draw_stream)        (plRenderEncoder*, uint32_t areaCount, plDrawArea*);

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
    plComputeEncoder* (*begin_compute_pass)      (plCommandBuffer*); // do not store
    void              (*end_compute_pass)        (plComputeEncoder*);
    void              (*dispatch)                (plComputeEncoder*, uint32_t dispatchCount, const plDispatch*);
    void              (*bind_compute_shader)     (plComputeEncoder*, plComputeShaderHandle);
    void              (*bind_compute_bind_groups)(plComputeEncoder*, plComputeShaderHandle, uint32_t first, uint32_t count, const plBindGroupHandle*, uint32_t dynamicCount, const plDynamicBinding*);

    // blit encoder 
    plBlitEncoder* (*begin_blit_pass)          (plCommandBuffer*); // do not store
    void           (*end_blit_pass)            (plBlitEncoder*);
    void           (*set_texture_usage)        (plBlitEncoder*, plTextureHandle, plTextureUsage tNewUsage, plTextureUsage tOldUsage);
    void           (*copy_buffer_to_texture)   (plBlitEncoder*, plBufferHandle, plTextureHandle, uint32_t regionCount, const plBufferImageCopy*);
    void           (*copy_texture_to_buffer)   (plBlitEncoder*, plTextureHandle, plBufferHandle, uint32_t regionCount, const plBufferImageCopy*);
    void           (*generate_mipmaps)         (plBlitEncoder*, plTextureHandle);
    void           (*copy_buffer)              (plBlitEncoder*, plBufferHandle source, plBufferHandle destination, uint32_t sourceOffset, uint32_t destinationOffset, size_t);

    //-----------------------------------------------------------------------------

    // buffers
    plBufferHandle (*create_buffer)            (plDevice*, const plBufferDesc*, plBuffer**);
    void           (*bind_buffer_to_memory)    (plDevice*, plBufferHandle, const plDeviceMemoryAllocation*);
    void           (*queue_buffer_for_deletion)(plDevice*, plBufferHandle);
    void           (*destroy_buffer)           (plDevice*, plBufferHandle);
    plBuffer*      (*get_buffer)               (plDevice*, plBufferHandle); // do not store

    // samplers
    plSamplerHandle (*create_sampler)            (plDevice*, const plSamplerDesc*);
    void            (*destroy_sampler)           (plDevice*, plSamplerHandle);
    void            (*queue_sampler_for_deletion)(plDevice*, plSamplerHandle);
    plSampler*      (*get_sampler)               (plDevice*, plSamplerHandle); // do not store

    // textures
    plTextureHandle (*create_texture)            (plDevice*, const plTextureDesc*, plTexture**);
    plTextureHandle (*create_texture_view)       (plDevice*, const plTextureViewDesc*);
    void            (*bind_texture_to_memory)    (plDevice*, plTextureHandle, const plDeviceMemoryAllocation*);
    void            (*queue_texture_for_deletion)(plDevice*, plTextureHandle);
    void            (*destroy_texture)           (plDevice*, plTextureHandle);
    plTexture*      (*get_texture)               (plDevice*, plTextureHandle); // do not store

    // bind groups
    plBindGroupPool*  (*create_bind_group_pool)       (plDevice*, const plBindGroupPoolDesc*);
    void              (*cleanup_bind_group_pool)      (plBindGroupPool*);
    void              (*reset_bind_group_pool)        (plBindGroupPool*);
    plBindGroupHandle (*create_bind_group)            (plDevice*, const plBindGroupDesc*);
    void              (*update_bind_group)            (plDevice*, plBindGroupHandle, const plBindGroupUpdateData*);
    void              (*queue_bind_group_for_deletion)(plDevice*, plBindGroupHandle);
    void              (*destroy_bind_group)           (plDevice*, plBindGroupHandle);
    plBindGroup*      (*get_bind_group)               (plDevice*, plBindGroupHandle); // do not store
    
    // render passes
    plRenderPassHandle (*create_render_pass)            (plDevice*, const plRenderPassDesc*, const plRenderPassAttachments*);
    void               (*update_render_pass_attachments)(plDevice*, plRenderPassHandle, plVec2 dimensions, const plRenderPassAttachments*);
    void               (*queue_render_pass_for_deletion)(plDevice*, plRenderPassHandle);
    void               (*destroy_render_pass)           (plDevice*, plRenderPassHandle);
    plRenderPass*      (*get_render_pass)               (plDevice*, plRenderPassHandle); // do not store

    // render pass layouts
    plRenderPassLayoutHandle (*create_render_pass_layout)            (plDevice*, const plRenderPassLayoutDesc*);
    void                     (*queue_render_pass_layout_for_deletion)(plDevice*, plRenderPassLayoutHandle);
    void                     (*destroy_render_pass_layout)           (plDevice*, plRenderPassLayoutHandle);
    plRenderPassLayout*      (*get_render_pass_layout)               (plDevice*, plRenderPassLayoutHandle); // do not store

    // pixel & vertex shaders
    plShaderHandle (*create_shader)            (plDevice*, const plShaderDesc*);
    void           (*queue_shader_for_deletion)(plDevice*, plShaderHandle);
    void           (*destroy_shader)           (plDevice*, plShaderHandle);
    plShader*      (*get_shader)               (plDevice*, plShaderHandle); // do not store

    // compute shaders
    plComputeShaderHandle (*create_compute_shader)            (plDevice*, const plComputeShaderDesc*);
    void                  (*queue_compute_shader_for_deletion)(plDevice*, plComputeShaderHandle);
    void                  (*destroy_compute_shader)           (plDevice*, plComputeShaderHandle);

    // memory
    plDynamicBinding        (*allocate_dynamic_data)(plDevice*, size_t);
    plDeviceMemoryAllocation(*allocate_memory)      (plDevice*, size_t, plMemoryMode, uint32_t typeFilter, const char* debugName);
    void                    (*free_memory)          (plDevice*, plDeviceMemoryAllocation*);

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
    uint32_t _uGeneration;
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
    const char*     pcDebugName; // default: "unnamed texture view"
} plTextureViewDesc;

typedef struct _plTexture
{
    plTextureDesc              tDesc;
    plTextureViewDesc          tView;
    plDeviceMemoryRequirements tMemoryRequirements; // filled out when using (create_texture)
    plDeviceMemoryAllocation   tMemoryAllocation;   // filled out when using (bind_texture_to_memory)

    // [INTERNAL]
    uint32_t _uGeneration;
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
    uint32_t _uGeneration;
} plBuffer;

//-----------------------------bind groups-------------------------------------

typedef struct _plSamplerBinding
{
    uint32_t     uSlot;
    plStageFlags tStages;
} plSamplerBinding;

typedef struct _plTextureBinding
{
    plTextureBindingType tType;
    uint32_t             uSlot;
    uint32_t             uDescriptorCount; // 0 - will become 1
    plStageFlags         tStages;

    // [INTERNAL]
    bool _bVariableDescriptorCount;
} plTextureBinding;

typedef struct _plBufferBinding
{
    plBufferBindingType tType;
    uint32_t            uSlot;
    size_t              szSize;
    size_t              szOffset;
    plStageFlags        tStages;
} plBufferBinding;

typedef struct _plDynamicBinding
{
    uint32_t uBufferHandle;
    uint32_t uByteOffset;
    char*    pcData;
} plDynamicBinding;

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
    size_t         szOffset;
    size_t         szBufferRange;
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
    uint32_t _uGeneration;
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
    uint32_t _uGeneration;
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
    uint32_t uByteOffset;
    plFormat tFormat;
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
            uint64_t ulCullMode           :  2; // PL_CULL_MODE_XXXX
            uint64_t ulStencilTestEnabled :  1; // bool
            uint64_t ulStencilMode        :  4; // PL_COMPARE_MODE_XXXX
            uint64_t ulStencilRef         :  8; 
            uint64_t ulStencilMask        :  8;
            uint64_t ulStencilOpFail      :  3; // PL_STENCIL_OP_XXXX  
            uint64_t ulStencilOpDepthFail :  3; // PL_STENCIL_OP_XXXX
            uint64_t ulStencilOpPass      :  3; // PL_STENCIL_OP_XXXX
            uint64_t _ulUnused            : 26;
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
    uint32_t _uGeneration;
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

typedef struct _plRenderTarget
{
    plFormat      tFormat;
    plSampleCount tSamples;
    bool          bResolve;
    bool          bDepth;
} plRenderTarget;

typedef struct _plRenderPassLayoutDesc
{
    plRenderTarget atRenderTargets[PL_MAX_RENDER_TARGETS];
    plSubpass      atSubpasses[PL_MAX_SUBPASSES];

    // [INTERNAL]
    uint32_t _uSubpassCount;
} plRenderPassLayoutDesc;

typedef struct _plRenderPassLayout
{
    plRenderPassLayoutDesc tDesc;

    // [INTERNAL]
    uint32_t _uAttachmentCount;
    uint32_t _uGeneration;
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
    uint32_t _uGeneration;
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

typedef struct _plDrawArea
{
    plRenderViewport tViewport;
    plScissor        tScissor;
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
    uint32_t uDynamicBuffer0;
    uint32_t uVertexBuffer0;
    uint32_t uIndexBuffer;
    uint32_t uVertexOffset;
    uint32_t uIndexOffset;
    uint32_t uTriangleCount;
    uint32_t uShaderVariant;
    uint32_t uBindGroup0;
    uint32_t uBindGroup1;
    uint32_t uBindGroup2;
    uint32_t uDynamicBufferOffset0;
    uint32_t uInstanceStart;
    uint32_t uInstanceCount;
} plDrawStreamData;

typedef struct _plDrawStream
{
    // [INTERNAL]
    plDrawStreamData _tCurrentDraw;
    uint32_t*        _sbtStream;
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
    PL_DEVICE_CAPABILITY_SAMPLER_ANISOTROPY  = 1 << 2
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

enum _plStageFlags
{
    PL_STAGE_NONE    = 0,
    PL_STAGE_VERTEX  = 1 << 0,
    PL_STAGE_PIXEL   = 1 << 1,
    PL_STAGE_COMPUTE = 1 << 2,
    PL_STAGE_ALL     = PL_STAGE_VERTEX | PL_STAGE_PIXEL | PL_STAGE_COMPUTE
};

enum _plCullMode
{
    PL_CULL_MODE_NONE       = 0,
    PL_CULL_MODE_CULL_FRONT = 1 << 0,
    PL_CULL_MODE_CULL_BACK  = 1 << 1,
};

enum _plFormat
{
    PL_FORMAT_UNKNOWN = 0,
    PL_FORMAT_R32G32B32_FLOAT,
    PL_FORMAT_R32G32B32A32_FLOAT,
    PL_FORMAT_R8G8B8A8_UNORM,
    PL_FORMAT_R32G32_FLOAT,
    PL_FORMAT_R32_UINT,
    PL_FORMAT_R8_UNORM,
    PL_FORMAT_R8G8_UNORM,
    PL_FORMAT_R8G8B8A8_SRGB,
    PL_FORMAT_B8G8R8A8_SRGB,
    PL_FORMAT_B8G8R8A8_UNORM,
    PL_FORMAT_D32_FLOAT,
    PL_FORMAT_D32_FLOAT_S8_UINT,
    PL_FORMAT_D24_UNORM_S8_UINT,
    PL_FORMAT_D16_UNORM_S8_UINT,

    PL_FORMAT_COUNT
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

#endif // PL_GRAPHICS_EXT_H