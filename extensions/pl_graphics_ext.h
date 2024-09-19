/*
   pl_graphics_ext.h
    * currently under very active development trying to finalize for 1.0
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
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_GRAPHICS_EXT_H
#define PL_GRAPHICS_EXT_H

#ifndef PL_MAX_NAME_LENGTH
    #define PL_MAX_NAME_LENGTH 1024
#endif

#ifndef PL_DEVICE_ALLOCATION_BLOCK_SIZE
    #define PL_DEVICE_ALLOCATION_BLOCK_SIZE 134217728
#endif

#ifndef PL_MAX_DYNAMIC_DATA_SIZE
    #define PL_MAX_DYNAMIC_DATA_SIZE 256
#endif

#ifndef PL_MAX_BUFFERS_PER_BIND_GROUP
    #define PL_MAX_BUFFERS_PER_BIND_GROUP 32
#endif

#ifndef PL_MAX_TEXTURES_PER_BIND_GROUP
    #define PL_MAX_TEXTURES_PER_BIND_GROUP 32
#endif

#ifndef PL_MAX_SAMPLERS_PER_BIND_GROUP
    #define PL_MAX_SAMPLERS_PER_BIND_GROUP 8
#endif

#ifndef PL_MAX_SHADER_SPECIALIZATION_CONSTANTS
    #define PL_MAX_SHADER_SPECIALIZATION_CONSTANTS 64
#endif

#ifndef PL_MAX_RENDER_TARGETS
    #define PL_MAX_RENDER_TARGETS 16
#endif

#ifndef PL_MAX_FRAMES_IN_FLIGHT
    #define PL_MAX_FRAMES_IN_FLIGHT 2
#endif

#ifndef PL_MAX_SUBPASSES
    #define PL_MAX_SUBPASSES 8
#endif

#ifndef PL_MAX_SEMAPHORES
    #define PL_MAX_SEMAPHORES 4
#endif

#ifndef PL_MAX_VERTEX_ATTRIBUTES
    #define PL_MAX_VERTEX_ATTRIBUTES 8
#endif

#ifndef PL_DEFINE_HANDLE
    #define PL_DEFINE_HANDLE(x) typedef union x { struct {uint32_t uIndex; uint32_t uGeneration;}; uint64_t ulData; } x;
#endif

#define PL_MAX_MIPS 64.0f

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define PL_API_GRAPHICS "PL_API_GRAPHICS"
typedef struct _plGraphicsI plGraphicsI;

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "pl_math.h"
#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plDevice                     plDevice;
typedef struct _plDeviceInfo                 plDeviceInfo;
typedef struct _plBuffer                     plBuffer;
typedef struct _plSwapchain                  plSwapchain;
typedef struct _plSurface                    plSurface;
typedef struct _plGraphicsInit               plGraphicsInit;
typedef struct _plDeviceInit                 plDeviceInit;
typedef struct _plSwapchainInit              plSwapchainInit;
typedef struct _plStreamDraw                 plStreamDraw;
typedef struct _plDispatch                   plDispatch;
typedef struct _plDrawArea                   plDrawArea;
typedef struct _plDrawStream                 plDrawStream;
typedef struct _plDraw                       plDraw;
typedef struct _plDrawIndex                  plDrawIndex;
typedef struct _plGraphicsState              plGraphicsState;
typedef struct _plBlendState                 plBlendState;
typedef struct _plSpecializationConstant     plSpecializationConstant;
typedef struct _plShaderModule               plShaderModule;
typedef struct _plShaderDescription          plShaderDescription;
typedef struct _plShader                     plShader;
typedef struct _plComputeShaderDescription   plComputeShaderDescription;
typedef struct _plComputeShader              plComputeShader;
typedef struct _plBuffer                     plBuffer;
typedef struct _plBufferDescription          plBufferDescription;
typedef struct _plDynamicBuffer              plDynamicBuffer;
typedef struct _plSamplerDesc                plSamplerDesc;
typedef struct _plSampler                    plSampler;
typedef struct _plTextureViewDesc            plTextureViewDesc;
typedef struct _plTexture                    plTexture;
typedef struct _plTextureDesc                plTextureDesc;
typedef struct _plBindGroupLayout            plBindGroupLayout;
typedef struct _plBindGroup                  plBindGroup;
typedef struct _plVertexAttributes           plVertexAttributes;
typedef struct _plVertexBufferBinding        plVertexBufferBinding;
typedef struct _plBufferBinding              plBufferBinding;
typedef struct _plTextureBinding             plTextureBinding;
typedef struct _plSamplerBinding             plSamplerBinding;
typedef struct _plDynamicBinding             plDynamicBinding;
typedef struct _plRenderViewport             plRenderViewport;
typedef struct _plScissor                    plScissor;
typedef struct _plExtent                     plExtent;
typedef struct _plFrameGarbage               plFrameGarbage;
typedef struct _plBufferImageCopy            plBufferImageCopy;
typedef struct _plCommandBuffer              plCommandBuffer;
typedef struct _plRenderEncoder              plRenderEncoder;
typedef struct _plComputeEncoder             plComputeEncoder;
typedef struct _plBlitEncoder                plBlitEncoder;
typedef struct _plBeginCommandInfo           plBeginCommandInfo;
typedef struct _plSubmitInfo                 plSubmitInfo;
typedef struct _plBindGroupUpdateData        plBindGroupUpdateData;
typedef struct _plBindGroupUpdateTextureData plBindGroupUpdateTextureData;
typedef struct _plBindGroupUpdateBufferData  plBindGroupUpdateBufferData;
typedef struct _plBindGroupUpdateSamplerData plBindGroupUpdateSamplerData;

// render passes
typedef struct _plRenderTarget                plRenderTarget;
typedef struct _plColorTarget                 plColorTarget;
typedef struct _plDepthTarget                 plDepthTarget;
typedef struct _plSubpass                     plSubpass;
typedef struct _plRenderPassLayout            plRenderPassLayout;
typedef struct _plRenderPassLayoutDescription plRenderPassLayoutDescription;
typedef struct _plRenderPassDescription       plRenderPassDescription;
typedef struct _plRenderPass                  plRenderPass;
typedef struct _plRenderPassAttachments       plRenderPassAttachments;

// handles
PL_DEFINE_HANDLE(plBufferHandle);
PL_DEFINE_HANDLE(plTextureHandle);
PL_DEFINE_HANDLE(plSamplerHandle);
PL_DEFINE_HANDLE(plBindGroupHandle);
PL_DEFINE_HANDLE(plShaderHandle);
PL_DEFINE_HANDLE(plComputeShaderHandle);
PL_DEFINE_HANDLE(plRenderPassHandle);
PL_DEFINE_HANDLE(plRenderPassLayoutHandle);
PL_DEFINE_HANDLE(plSemaphoreHandle);

PL_DEFINE_HANDLE(plCommandBufferHandle);
PL_DEFINE_HANDLE(plRenderEncoderHandle);
PL_DEFINE_HANDLE(plComputeEncoderHandle);
PL_DEFINE_HANDLE(plBlitEncoderHandle);

// device memory
typedef struct _plDeviceMemoryRequirements plDeviceMemoryRequirements;
typedef struct _plDeviceMemoryAllocation   plDeviceMemoryAllocation;
typedef struct _plDeviceMemoryAllocatorI   plDeviceMemoryAllocatorI;

// enums
typedef int plGraphicsInitFlags;  // -> enum _plGraphicsInitFlags      // Flags:
typedef int plDeviceInitFlags;    // -> enum _plDeviceInitFlags        // Flags:
typedef int plSwapchainInitFlags; // -> enum _plSwapchainInitFlags     // Flags:
typedef int plDataType;           // -> enum _plDataType               // Enum:
typedef int plBufferBindingType;  // -> enum _plBufferBindingType      // Enum:
typedef int plTextureBindingType; // -> enum _plTextureBindingType     // Enum:
typedef int plTextureType;        // -> enum _plTextureType            // Enum:
typedef int plBufferUsage;        // -> enum _plBufferUsage            // Enum:
typedef int plTextureUsage;       // -> enum _plTextureUsage           // Enum:
typedef int plMeshFormatFlags;    // -> enum _plMeshFormatFlags        // Flags:
typedef int plStageFlags;         // -> enum _plStageFlags             // Flags:
typedef int plCullMode;           // -> enum _plCullMode               // Enum:
typedef int plFilter;             // -> enum _plFilter                 // Enum:
typedef int plWrapMode;           // -> enum _plWrapMode               // Enum:
typedef int plCompareMode;        // -> enum _plCompareMode            // Enum:
typedef int plFormat;             // -> enum _plFormat                 // Enum:
typedef int plStencilOp;          // -> enum _plStencilOp              // Enum:
typedef int plMemoryMode;         // -> enum _plMemoryMode             // Enum:
typedef int plLoadOperation;      // -> enum _plLoadOperation          // Enum:
typedef int plLoadOp;             // -> enum _plLoadOp                 // Enum:
typedef int plStoreOp;            // -> enum _plStoreOp                // Enum:
typedef int plBlendOp;            // -> enum _plBlendOp                // Enum:
typedef int plBlendFactor;        // -> enum _plBlendFactor            // Enum:
typedef int plMipmapMode;         // -> enum _plMipmapMode             // Enum:
typedef int plVendorId;           // -> enum _plVendorId               // Enum:
typedef int plDeviceType;         // -> enum _plDeviceType             // Enum:
typedef int plDeviceCapability;   // -> enum _plDeviceCapability       // Flags:

// external
typedef struct _plWindow plWindow;

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plGraphicsI
{
    // context
    bool (*initialize)(const plGraphicsInit*);
    void (*resize)    (plSwapchain*);
    void (*cleanup)   (void);

    // devices
    void      (*enumerate_devices)(plDeviceInfo*, uint32_t* puDeviceCount);
    plDevice* (*create_device)(const plDeviceInit*);
    void      (*cleanup_device)(plDevice*);

    // surface
    plSurface* (*create_surface)(plWindow*);
    void       (*cleanup_surface)(plSurface*);

    // swapchain
    plSwapchain* (*create_swapchain)(plDevice*, const plSwapchainInit*);
    void         (*cleanup_swapchain)(plSwapchain*);

    // query
    uint32_t (*get_frames_in_flight)(void);
    uint32_t (*get_current_frame_index)(void);
    size_t   (*get_host_memory_in_use)(void);
    size_t   (*get_local_memory_in_use)(void);

    // per frame
    bool (*begin_frame)(plSwapchain*);

    // timeline semaphore ops
    void     (*signal_semaphore)   (plDevice*, plSemaphoreHandle, uint64_t);
    void     (*wait_semaphore)     (plDevice*, plSemaphoreHandle, uint64_t);
    uint64_t (*get_semaphore_value)(plDevice*, plSemaphoreHandle);

    // command buffers
    plCommandBufferHandle (*begin_command_recording)       (plDevice*, const plBeginCommandInfo*);
    void                  (*end_command_recording)         (plCommandBufferHandle);
    void                  (*submit_command_buffer)         (plCommandBufferHandle, const plSubmitInfo*);
    void                  (*submit_command_buffer_blocking)(plCommandBufferHandle, const plSubmitInfo*);
    bool                  (*present)                       (plCommandBufferHandle, const plSubmitInfo*, plSwapchain*);

    // render encoder
    plRenderEncoderHandle (*begin_render_pass)         (plCommandBufferHandle, plRenderPassHandle);
    void                  (*next_subpass)              (plRenderEncoderHandle);
    void                  (*end_render_pass)           (plRenderEncoderHandle);
    plRenderPassHandle    (*get_encoder_render_pass)   (plRenderEncoderHandle);
    uint32_t              (*get_render_encoder_subpass)(plRenderEncoderHandle);

    // render encoder: draw stream (preferred system)
    void (*reset_draw_stream)  (plDrawStream*);
    void (*cleanup_draw_stream)(plDrawStream*);
    void (*add_to_stream)      (plDrawStream*, plStreamDraw);
    void (*draw_stream)        (plRenderEncoderHandle, uint32_t uAreaCount, plDrawArea*);

    // render encoder: direct (prefer draw stream, this should be used for bindless mostly)
    void (*bind_graphics_bind_groups)(plRenderEncoderHandle, plShaderHandle, uint32_t uFirst, uint32_t uCount, const plBindGroupHandle*, plDynamicBinding*);
    void (*set_viewport)             (plRenderEncoderHandle, const plRenderViewport*);
    void (*set_scissor_region)       (plRenderEncoderHandle, const plScissor*);
    void (*bind_vertex_buffer)       (plRenderEncoderHandle, plBufferHandle);
    void (*draw)                     (plRenderEncoderHandle, uint32_t uCount, const plDraw*);
    void (*draw_indexed)             (plRenderEncoderHandle, uint32_t uCount, const plDrawIndex*);
    void (*bind_shader)              (plRenderEncoderHandle, plShaderHandle);

    // compute encoder
    plComputeEncoderHandle (*begin_compute_pass)      (plCommandBufferHandle);
    void                   (*end_compute_pass)        (plComputeEncoderHandle);
    void                   (*dispatch)                (plComputeEncoderHandle, uint32_t uDispatchCount, const plDispatch*);
    void                   (*bind_compute_shader)     (plComputeEncoderHandle, plComputeShaderHandle);
    void                   (*bind_compute_bind_groups)(plComputeEncoderHandle, plComputeShaderHandle, uint32_t uFirst, uint32_t uCount, const plBindGroupHandle*, plDynamicBinding*);

    // blit encoder
    plBlitEncoderHandle (*begin_blit_pass)         (plCommandBufferHandle);
    void                (*end_blit_pass)           (plBlitEncoderHandle);
    void                (*copy_buffer_to_texture)  (plBlitEncoderHandle, plBufferHandle, plTextureHandle, uint32_t uRegionCount, const plBufferImageCopy*);
    void                (*copy_texture_to_buffer)  (plBlitEncoderHandle, plTextureHandle, plBufferHandle, uint32_t uRegionCount, const plBufferImageCopy*);
    void                (*generate_mipmaps)        (plBlitEncoderHandle, plTextureHandle);
    void                (*copy_buffer)             (plBlitEncoderHandle, plBufferHandle tSource, plBufferHandle tDestination, uint32_t uSourceOffset, uint32_t uDestinationOffset, size_t);

    //-----------------------------------------------------------------------------

    // buffers
    plBufferHandle (*create_buffer)            (plDevice*, const plBufferDescription*, const char* pcDebugName);
    void           (*bind_buffer_to_memory)    (plDevice*, plBufferHandle, const plDeviceMemoryAllocation*);
    void           (*queue_buffer_for_deletion)(plDevice*, plBufferHandle);
    void           (*destroy_buffer)           (plDevice*, plBufferHandle);
    plBuffer*      (*get_buffer)               (plDevice*, plBufferHandle); // do not store

    // samplers
    plSamplerHandle (*create_sampler)            (plDevice*, const plSamplerDesc*, const char* pcDebugName);
    void            (*destroy_sampler)           (plDevice*, plSamplerHandle);
    void            (*queue_sampler_for_deletion)(plDevice*, plSamplerHandle);
    plSampler*      (*get_sampler)               (plDevice*, plSamplerHandle); // do not store

    // textures (if manually handling mips/levels, don't use initial data, use "copy_buffer_to_texture" instead)
    plTextureHandle (*create_texture)            (plDevice*, const plTextureDesc*, const char* pcDebugName);
    plTextureHandle (*create_texture_view)       (plDevice*, const plTextureViewDesc*, const char* pcDebugName);
    void            (*bind_texture_to_memory)    (plDevice*, plTextureHandle, const plDeviceMemoryAllocation*);
    void            (*queue_texture_for_deletion)(plDevice*, plTextureHandle);
    void            (*destroy_texture)           (plDevice*, plTextureHandle);
    plTexture*      (*get_texture)               (plDevice*, plTextureHandle);     // do not store

    // bind groups
    plBindGroupHandle (*create_bind_group)            (plDevice*, const plBindGroupLayout*, const char* pcDebugName);
    plBindGroupHandle (*get_temporary_bind_group)     (plDevice*, const plBindGroupLayout*, const char* pcDebugName); // don't submit for deletion
    void              (*update_bind_group)            (plDevice*, plBindGroupHandle, const plBindGroupUpdateData*);
    void              (*queue_bind_group_for_deletion)(plDevice*, plBindGroupHandle);
    void              (*destroy_bind_group)           (plDevice*, plBindGroupHandle);
    plBindGroup*      (*get_bind_group)               (plDevice*, plBindGroupHandle); // do not store
    
    // render passes
    plRenderPassLayoutHandle (*create_render_pass_layout)            (plDevice*, const plRenderPassLayoutDescription*);
    plRenderPassHandle       (*create_render_pass)                   (plDevice*, const plRenderPassDescription*, const plRenderPassAttachments*);
    void                     (*update_render_pass_attachments)       (plDevice*, plRenderPassHandle, plVec2 tDimensions, const plRenderPassAttachments*);
    void                     (*queue_render_pass_for_deletion)       (plDevice*, plRenderPassHandle);
    void                     (*queue_render_pass_layout_for_deletion)(plDevice*, plRenderPassLayoutHandle);
    void                     (*destroy_render_pass)                  (plDevice*, plRenderPassHandle);
    void                     (*destroy_render_pass_layout)           (plDevice*, plRenderPassLayoutHandle);
    plRenderPassLayout*      (*get_render_pass_layout)               (plDevice*, plRenderPassLayoutHandle); // do not store
    plRenderPass*            (*get_render_pass)                      (plDevice*, plRenderPassHandle); // do not store

    // render passes (temporary)
    plRenderPassHandle (*get_main_render_pass)(plDevice*);

    // shaders
    plShaderHandle        (*create_shader)                    (plDevice*, const plShaderDescription*);
    plComputeShaderHandle (*create_compute_shader)            (plDevice*, const plComputeShaderDescription*);
    void                  (*queue_shader_for_deletion)        (plDevice*, plShaderHandle);
    void                  (*queue_compute_shader_for_deletion)(plDevice*, plComputeShaderHandle);
    void                  (*destroy_shader)                   (plDevice*, plShaderHandle);
    void                  (*destroy_compute_shader)           (plDevice*, plComputeShaderHandle);
    plShader*             (*get_shader)                       (plDevice*, plShaderHandle); // do not store

    // syncronization
    plSemaphoreHandle (*create_semaphore)(plDevice*, bool bHostVisible);

    // memory
    plDynamicBinding        (*allocate_dynamic_data)(plDevice*, size_t);
    plDeviceMemoryAllocation(*allocate_memory)      (plDevice*, size_t, plMemoryMode, uint32_t uTypeFilter, const char* pcDebugName);
    void                    (*free_memory)          (plDevice*, plDeviceMemoryAllocation*);

    // misc
    void (*flush_device)(plDevice*);

} plGraphicsI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plBeginCommandInfo
{
    uint32_t          uWaitSemaphoreCount;
    plSemaphoreHandle atWaitSempahores[PL_MAX_SEMAPHORES];
    uint64_t          auWaitSemaphoreValues[PL_MAX_SEMAPHORES + 1];
} plBeginCommandInfo;

typedef struct _plSubmitInfo
{
    uint32_t          uSignalSemaphoreCount;
    plSemaphoreHandle atSignalSempahores[PL_MAX_SEMAPHORES];
    uint64_t          auSignalSemaphoreValues[PL_MAX_SEMAPHORES + 1];
} plSubmitInfo;

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

typedef struct _plBindGroupUpdateSamplerData
{
    plSamplerHandle tSampler;
    uint32_t        uSlot;
} plBindGroupUpdateSamplerData;

typedef struct _plBindGroupUpdateData
{
    uint32_t                            uBufferCount;
    uint32_t                            uTextureCount;
    uint32_t                            uSamplerCount;
    const plBindGroupUpdateBufferData*  atBuffers;
    const plBindGroupUpdateTextureData* atTextures;
    const plBindGroupUpdateSamplerData* atSamplerBindings;
} plBindGroupUpdateData;

typedef struct _plGraphicsState
{
    union 
    {
        struct
        {
            uint64_t ulDepthMode          :  4; // PL_DEPTH_MODE_
            uint64_t ulWireframe          :  1; // bool
            uint64_t ulDepthWriteEnabled  :  1; // bool
            uint64_t ulCullMode           :  2; // PL_CULL_MODE_*
            uint64_t ulStencilTestEnabled :  1; // bool
            uint64_t ulStencilMode        :  4;
            uint64_t ulStencilRef         :  8;
            uint64_t ulStencilMask        :  8;
            uint64_t ulStencilOpFail      :  3;
            uint64_t ulStencilOpDepthFail :  3;
            uint64_t ulStencilOpPass      :  3;
            uint64_t _ulUnused            : 26;
        };
        uint64_t ulValue;
    };
    
} plGraphicsState;

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
    char*                     pHostMapped;
    uint32_t                  uCurrentIndex; // used but debug tool
    plDeviceMemoryAllocatorI* ptAllocator;
} plDeviceMemoryAllocation;

typedef struct _plDeviceMemoryAllocatorI
{
    struct plDeviceMemoryAllocatorO* ptInst; // opaque pointer
    plDeviceMemoryAllocation (*allocate)(struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName);
    void                     (*free)    (struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation);
} plDeviceMemoryAllocatorI;

typedef struct _plTextureViewDesc
{
    plFormat        tFormat; 
    uint32_t        uBaseMip;
    uint32_t        uMips;
    uint32_t        uBaseLayer;
    uint32_t        uLayerCount;
    plTextureHandle tTexture;
} plTextureViewDesc;

typedef struct _plSamplerDesc
{
    plFilter      tFilter;
    plMipmapMode  tMipmapMode;
    plCompareMode tCompare;
    plWrapMode    tHorizontalWrap;
    plWrapMode    tVerticalWrap;
    float         fMipBias;
    float         fMinMip;
    float         fMaxMip;
    float         fMaxAnisotropy;
} plSamplerDesc;

typedef struct _plSampler
{
    plSamplerDesc tDesc;
} plSampler;

typedef struct _plTextureDesc
{
    char           acDebugName[PL_MAX_NAME_LENGTH];
    plVec3         tDimensions;
    uint32_t       uLayers;
    uint32_t       uMips;
    plFormat       tFormat;
    plTextureType  tType;
    plTextureUsage tUsage;
    plTextureUsage tInitialUsage;
} plTextureDesc;

typedef struct _plTexture
{
    plTextureDesc              tDesc;
    plTextureViewDesc          tView;
    plDeviceMemoryRequirements tMemoryRequirements;
    plDeviceMemoryAllocation   tMemoryAllocation;
    plBindGroupHandle          _tDrawBindGroup;
} plTexture;

typedef struct _plBufferDescription
{
    char          acDebugName[PL_MAX_NAME_LENGTH];
    plBufferUsage tUsage;
    uint32_t      uByteSize;
} plBufferDescription;

typedef struct _plBuffer
{
    plBufferDescription        tDescription;
    plDeviceMemoryRequirements tMemoryRequirements;
    plDeviceMemoryAllocation   tMemoryAllocation;
} plBuffer;

typedef struct _plBufferBinding
{
    plBufferBindingType tType;
    uint32_t            uSlot;
    size_t              szSize;
    size_t              szOffset;
    plStageFlags        tStages;
} plBufferBinding;

typedef struct _plTextureBinding
{
    plTextureBindingType tType;
    uint32_t             uSlot;
    uint32_t             uDescriptorCount; // 0 - will become 1
    plStageFlags         tStages;
    bool                 bVariableDescriptorCount;
} plTextureBinding;

typedef struct _plSamplerBinding
{
    uint32_t     uSlot;
    plStageFlags tStages;
} plSamplerBinding;

typedef struct _plDynamicBinding
{
    uint32_t uBufferHandle;
    uint32_t uByteOffset;
    char*    pcData;
} plDynamicBinding;

typedef struct _plBindGroupLayout
{
    uint32_t         uTextureBindingCount;
    uint32_t         uBufferBindingCount;
    uint32_t         uSamplerBindingCount;
    plTextureBinding atTextureBindings[PL_MAX_TEXTURES_PER_BIND_GROUP];
    plBufferBinding  aBufferBindings[PL_MAX_BUFFERS_PER_BIND_GROUP];
    plSamplerBinding atSamplerBindings[PL_MAX_SAMPLERS_PER_BIND_GROUP];
    uint32_t         uHandle;
} plBindGroupLayout;

typedef struct _plBindGroup
{
    plBindGroupLayout tLayout;
} plBindGroup;

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

typedef struct _plExtent
{
    uint32_t uWidth;
    uint32_t uHeight;
    uint32_t uDepth;
} plExtent;

typedef struct _plBufferImageCopy
{
    size_t         szBufferOffset;
    int            iImageOffsetX;
    int            iImageOffsetY;
    int            iImageOffsetZ;
    plExtent       tImageExtent;
    uint32_t       uMipLevel;
    uint32_t       uBaseArrayLayer;
    uint32_t       uLayerCount;
    uint32_t       uBufferRowLength;
    uint32_t       uImageHeight;
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

typedef struct _plStreamDraw
{
    uint32_t uDynamicBuffer;
    uint32_t uVertexBuffer;
    uint32_t uIndexBuffer;
    uint32_t uVertexOffset;
    uint32_t uIndexOffset;
    uint32_t uTriangleCount;
    uint32_t uShaderVariant;
    uint32_t uBindGroup0;
    uint32_t uBindGroup1;
    uint32_t uBindGroup2;
    uint32_t uDynamicBufferOffset;
    uint32_t uInstanceStart;
    uint32_t uInstanceCount;
} plStreamDraw;

typedef struct _plDrawStream
{
    plStreamDraw tCurrentDraw;
    uint32_t*    sbtStream;
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

typedef struct _plSpecializationConstant
{
    uint32_t   uID;
    uint32_t   uOffset;
    plDataType tType;
} plSpecializationConstant;

typedef struct _plVertexAttributes
{
    uint32_t uByteOffset;
    plFormat tFormat;
} plVertexAttributes;

typedef struct _plVertexBufferBinding
{
    uint32_t           uByteStride;
    plVertexAttributes atAttributes[PL_MAX_VERTEX_ATTRIBUTES];
} plVertexBufferBinding;

typedef struct _plShaderModule
{
    size_t      szCodeSize;
    uint8_t*    puCode;
    const char* pcEntryFunc;
} plShaderModule;

typedef struct _plShaderDescription
{
    plSpecializationConstant atConstants[PL_MAX_SHADER_SPECIALIZATION_CONSTANTS];
    uint32_t                 uConstantCount;
    uint32_t                 uSubpassIndex;
    plGraphicsState          tGraphicsState;
    plBlendState             atBlendStates[PL_MAX_RENDER_TARGETS];
    uint32_t                 uBlendStateCount;
    plVertexBufferBinding    tVertexBufferBinding;
    plShaderModule           tVertexShader;
    plShaderModule           tPixelShader;
    const void*              pTempConstantData;
    plBindGroupLayout        atBindGroupLayouts[3];
    uint32_t                 uBindGroupLayoutCount;
    plRenderPassLayoutHandle tRenderPassLayout;    
} plShaderDescription;

typedef struct _plShader
{
    plShaderDescription tDescription;
} plShader;

typedef struct _plComputeShaderDescription
{
    plShaderModule           tShader;
    plBindGroupLayout        atBindGroupLayouts[3];
    uint32_t                 uBindGroupLayoutCount;
    plSpecializationConstant atConstants[PL_MAX_SHADER_SPECIALIZATION_CONSTANTS];
    uint32_t                 uConstantCount;
    const void*              pTempConstantData;
} plComputeShaderDescription;

typedef struct _plComputeShader
{
    plComputeShaderDescription tDescription;
} plComputeShader;

typedef struct _plSubpass
{
    uint32_t uRenderTargetCount;
    uint32_t uSubpassInputCount;
    uint32_t auRenderTargets[PL_MAX_RENDER_TARGETS];
    uint32_t auSubpassInputs[PL_MAX_RENDER_TARGETS];
    bool     _bHasDepth;
} plSubpass;

typedef struct _plRenderTarget
{
    plFormat tFormat;
} plRenderTarget;

typedef struct _plRenderPassLayoutDescription
{
    uint32_t       uSubpassCount;
    plRenderTarget atRenderTargets[PL_MAX_RENDER_TARGETS];
    plSubpass      atSubpasses[PL_MAX_SUBPASSES];
} plRenderPassLayoutDescription;

typedef struct _plRenderPassLayout
{
    plRenderPassLayoutDescription tDesc;

    // internal
    uint32_t _uAttachmentCount;
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

typedef struct _plRenderPassDescription
{
    plRenderPassLayoutHandle tLayout;
    plColorTarget            atColorTargets[PL_MAX_RENDER_TARGETS];
    plDepthTarget            tDepthTarget;
    plVec2                   tDimensions;
    plSwapchain*             ptSwapchain;
} plRenderPassDescription;

typedef struct _plRenderPass
{
    plRenderPassDescription tDesc;
} plRenderPass;

typedef struct _plGraphicsInit
{
    uint32_t            uFramesInFlight; // default: 2
    plGraphicsInitFlags tFlags;
} plGraphicsInit;

typedef struct _plDeviceInit
{
    plDeviceInitFlags tFlags;
    plSurface*        ptSurface;
    plDeviceInfo*     ptInfo; // pick default device if NULL
} plDeviceInit;

typedef struct _plSwapchainInit
{
    plSwapchainInitFlags tFlags;
    plSurface*           ptSurface;
} plSwapchainInit;

typedef struct _plDeviceLimits
{
    uint32_t uMaxTextureSize; // width or height
    uint32_t uMinUniformBufferOffsetAlignment;
} plDeviceLimits;

typedef struct _plDeviceInfo
{
    char               acName[256];
    uint32_t           uDeviceIdx;
    plVendorId         tVendorId;
    plDeviceType       tType;
    plDeviceLimits     tLimits;
    size_t             szDeviceMemory;
    size_t             szHostMemory;
    plDeviceCapability tCapabilities;

    // TODO:
    //   * formats
} plDeviceInfo;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

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
    PL_DEVICE_CAPABILITY_DESCRIPTOR_INDEXING = 1 << 1,
    PL_DEVICE_CAPABILITY_SAMPLER_ANISOTROPY  = 1 << 2
};

enum _plDeviceInitFlags
{
    PL_DEVICE_INIT_FLAGS_NONE = 0
};

enum _plSwapchainInitFlags
{
    PL_SWAPCHAIN_INIT_FLAGS_NONE = 0
};

enum _plGraphicsInitFlags
{
    PL_GRAPHICS_INIT_FLAGS_NONE                = 0,
    PL_GRAPHICS_INIT_FLAGS_VALIDATION_ENABLED  = 1 << 0,
    PL_GRAPHICS_INIT_FLAGS_SWAPCHAIN_ENABLED   = 1 << 1,
    PL_GRAPHICS_INIT_FLAGS_LOGGING_TRACE       = 1 << 2,
    PL_GRAPHICS_INIT_FLAGS_LOGGING_DEBUG       = 1 << 3,
    PL_GRAPHICS_INIT_FLAGS_LOGGING_INFO        = 1 << 4,
    PL_GRAPHICS_INIT_FLAGS_LOGGING_WARNING     = 1 << 5,
    PL_GRAPHICS_INIT_FLAGS_LOGGING_ERROR       = 1 << 6
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
    PL_CULL_MODE_NONE          = 0,
    PL_CULL_MODE_CULL_FRONT    = 1 << 0,
    PL_CULL_MODE_CULL_BACK     = 1 << 1,
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
    PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT,
    PL_TEXTURE_BINDING_TYPE_STORAGE
};

enum _plTextureType
{
    PL_TEXTURE_TYPE_UNSPECIFIED,
    PL_TEXTURE_TYPE_2D,
    PL_TEXTURE_TYPE_CUBE,
    PL_TEXTURE_TYPE_2D_ARRAY
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

enum _plLoadOperation
{
    PL_LOAD_OPERATION_DONT_CARE,
    PL_LOAD_OPERATION_LOAD,
    PL_LOAD_OPERATION_CLEAR
};

enum _plMipmapMode
{
    PL_MIPMAP_MODE_LINEAR,
    PL_MIPMAP_MODE_NEAREST
};

enum _plDataType
{
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