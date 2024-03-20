/*
   pl_graphics_ext.h
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

#ifndef PL_DEVICE_BUDDY_BLOCK_SIZE
    #define PL_DEVICE_BUDDY_BLOCK_SIZE 268435456
#endif

#ifndef PL_DEVICE_ALLOCATION_BLOCK_SIZE
    #define PL_DEVICE_ALLOCATION_BLOCK_SIZE 134217728
#endif

#ifndef PL_MAX_DYNAMIC_DATA_SIZE
    #define PL_MAX_DYNAMIC_DATA_SIZE 512
#endif

#ifndef PL_DEVICE_LOCAL_LEVELS
    #define PL_DEVICE_LOCAL_LEVELS 8
#endif

#ifndef PL_MAX_BUFFERS_PER_BIND_GROUP
    #define PL_MAX_BUFFERS_PER_BIND_GROUP 32
#endif

#ifndef PL_MAX_TEXTURES_PER_BIND_GROUP
    #define PL_MAX_TEXTURES_PER_BIND_GROUP 32
#endif

#ifndef PL_MAX_SHADER_SPECIALIZATION_CONSTANTS
    #define PL_MAX_SHADER_SPECIALIZATION_CONSTANTS 64
#endif

#define PL_ALIGN_UP(num, align) (((num) + ((align)-1)) & ~((align)-1))

#ifndef PL_DEFINE_HANDLE
    #define PL_DEFINE_HANDLE(x) typedef struct x { uint32_t uIndex; uint32_t uGeneration;} x;
#endif

#define PL_MAX_MIPS 64.0f

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define PL_API_GRAPHICS "PL_API_GRAPHICS"
typedef struct _plGraphicsI plGraphicsI;

#define PL_API_DEVICE "PL_API_DEVICE"
typedef struct _plDeviceI plDeviceI;

#define PL_API_DRAW_STREAM "PL_API_DRAW_STREAM"
typedef struct _plDrawStreamI plDrawStreamI;

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
typedef struct _plDevice                   plDevice;
typedef struct _plBuffer                   plBuffer;
typedef struct _plSwapchain                plSwapchain;
typedef struct _plGraphics                 plGraphics;
typedef struct _plDraw                     plDraw;
typedef struct _plDispatch                 plDispatch;
typedef struct _plDrawArea                 plDrawArea;
typedef struct _plDrawStream               plDrawStream;
typedef struct _plGraphicsState            plGraphicsState;
typedef struct _plSpecializationConstant   plSpecializationConstant;
typedef struct _plShaderDescription        plShaderDescription;
typedef struct _plShader                   plShader;
typedef struct _plComputeShaderDescription plComputeShaderDescription;
typedef struct _plComputeShader            plComputeShader;
typedef struct _plShaderVariant            plShaderVariant;
typedef struct _plComputeShaderVariant     plComputeShaderVariant;
typedef struct _plBuffer                   plBuffer;
typedef struct _plBufferDescription        plBufferDescription;
typedef struct _plDynamicBuffer            plDynamicBuffer;
typedef struct _plSampler                  plSampler;
typedef struct _plTextureViewDesc          plTextureViewDesc;
typedef struct _plTexture                  plTexture;
typedef struct _plTextureView              plTextureView;
typedef struct _plTextureDesc              plTextureDesc;
typedef struct _plBindGroupLayout          plBindGroupLayout;
typedef struct _plBindGroup                plBindGroup;
typedef struct _plBufferBinding            plBufferBinding;
typedef struct _plTextureBinding           plTextureBinding;
typedef struct _plDynamicBinding           plDynamicBinding;
typedef struct _plRenderViewport           plRenderViewport;
typedef struct _plScissor                  plScissor;
typedef struct _plExtent                   plExtent;
typedef struct _plFrameGarbage             plFrameGarbage;
typedef struct _plBufferImageCopy          plBufferImageCopy;
typedef struct _plCommandBuffer            plCommandBuffer;
typedef struct _plPassRenderer             plPassRenderer;

// render passes
typedef struct _plRenderTarget                plRenderTarget;
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
PL_DEFINE_HANDLE(plTextureViewHandle);
PL_DEFINE_HANDLE(plBindGroupHandle);
PL_DEFINE_HANDLE(plShaderHandle);
PL_DEFINE_HANDLE(plComputeShaderHandle);
PL_DEFINE_HANDLE(plRenderPassHandle);
PL_DEFINE_HANDLE(plRenderPassLayoutHandle);

// device memory
typedef struct _plDeviceAllocationRange  plDeviceAllocationRange;
typedef struct _plDeviceAllocationBlock  plDeviceAllocationBlock;
typedef struct _plDeviceMemoryAllocation plDeviceMemoryAllocation;
typedef struct _plDeviceMemoryAllocatorI plDeviceMemoryAllocatorI;

// 3D drawing api
typedef struct _plDrawList3D        plDrawList3D;
typedef struct _plDrawVertex3DSolid plDrawVertex3DSolid; // single vertex (3D pos + uv + color)
typedef struct _plDrawVertex3DLine  plDrawVertex3DLine; // single vertex (pos + uv + color)

// enums
typedef int pl3DDrawFlags;
typedef int plDataType;               // -> enum _plDataType               // Enum:
typedef int plBufferBindingType;      // -> enum _plBufferBindingType      // Enum:
typedef int plTextureBindingType;     // -> enum _plTextureBindingType     // Enum:
typedef int plTextureType;            // -> enum _plTextureType            // Enum:
typedef int plBufferUsage;            // -> enum _plBufferUsage            // Enum:
typedef int plTextureUsage;           // -> enum _plTextureUsage           // Enum:
typedef int plMeshFormatFlags;        // -> enum _plMeshFormatFlags        // Flags:
typedef int plStageFlags;             // -> enum _plStageFlags             // Flags:
typedef int plBlendMode;              // -> enum _plBlendMode              // Enum:
typedef int plCullMode;               // -> enum _plCullMode               // Enum:
typedef int plFilter;                 // -> enum _plFilter                 // Enum:
typedef int plWrapMode;               // -> enum _plWrapMode               // Enum:
typedef int plCompareMode;            // -> enum _plCompareMode            // Enum:
typedef int plFormat;                 // -> enum _plFormat                 // Enum:
typedef int plStencilOp;              // -> enum _plStencilOp              // Enum:
typedef int plMemoryMode;             // -> enum _plMemoryMode             // Enum:
typedef int plLoadOperation;          // -> enum _plLoadOperation          // Enum:
typedef int plLoadOp;                 // -> enum _plLoadOp                 // Enum:
typedef int plStoreOp;                // -> enum _plStoreOp                // Enum:
typedef int plTextureLayout;          // -> enum _plTextureLayout          // Enum:

// external
typedef struct _plDrawList plDrawList;
typedef struct _plFontAtlas plFontAtlas;

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plDrawStreamI
{
    void (*reset)  (plDrawStream* ptStream);
    void (*cleanup)(plDrawStream* ptStream);
    void (*draw)   (plDrawStream* ptStream, plDraw tDraw);
} plDrawStreamI;

typedef struct _plDeviceI
{
    // buffers
    plBufferHandle (*create_buffer)            (plDevice* ptDevice, const plBufferDescription* ptDesc, const char* pcName);
    void           (*queue_buffer_for_deletion)(plDevice* ptDevice, plBufferHandle tHandle);
    void           (*destroy_buffer)           (plDevice* ptDevice, plBufferHandle tHandle);
    plBuffer*      (*get_buffer)               (plDevice* ptDevice, plBufferHandle ptHandle); // do not store

    // textures (if manually handling mips/levels, don't use initial data, use "copy_buffer_to_texture" instead)
    plTextureHandle     (*create_texture)                 (plDevice* ptDevice, plTextureDesc tDesc, const char* pcName);
    plTextureViewHandle (*create_texture_view)            (plDevice* ptDevice, const plTextureViewDesc* ptViewDesc, const plSampler* ptSampler, plTextureHandle tTexture, const char* pcName);
    void                (*queue_texture_for_deletion)     (plDevice* ptDevice, plTextureHandle tHandle);
    void                (*queue_texture_view_for_deletion)(plDevice* ptDevice, plTextureViewHandle tHandle);
    void                (*destroy_texture)                (plDevice* ptDevice, plTextureHandle tHandle);
    void                (*destroy_texture_view)           (plDevice* ptDevice, plTextureViewHandle tHandle);
    plTexture*          (*get_texture)                    (plDevice* ptDevice, plTextureHandle ptHandle);     // do not store
    plTextureView*      (*get_texture_view)               (plDevice* ptDevice, plTextureViewHandle ptHandle); // do not store

    // bind groups
    plBindGroupHandle (*create_bind_group)            (plDevice* ptDevice, plBindGroupLayout* ptLayout);
    plBindGroupHandle (*get_temporary_bind_group)     (plDevice* ptDevice, plBindGroupLayout* ptLayout); // don't submit for deletion
    void              (*update_bind_group)            (plDevice* ptDevice, plBindGroupHandle* ptGroup, uint32_t uBufferCount, plBufferHandle* atBuffers, size_t* aszBufferRanges, uint32_t uTextureViewCount, plTextureViewHandle* atTextureViews);
    void              (*queue_bind_group_for_deletion)(plDevice* ptDevice, plBindGroupHandle tHandle);
    void              (*destroy_bind_group)           (plDevice* ptDevice, plBindGroupHandle tHandle);
    plBindGroup*      (*get_bind_group)               (plDevice* ptDevice, plBindGroupHandle ptHandle); // do not store
    plDynamicBinding  (*allocate_dynamic_data)        (plDevice* ptDevice, size_t szSize);

    // render passes
    plRenderPassLayoutHandle (*create_render_pass_layout)            (plDevice* ptDevice, const plRenderPassLayoutDescription* ptDesc);
    plRenderPassHandle       (*create_render_pass)                   (plDevice* ptDevice, const plRenderPassDescription* ptDesc, const plRenderPassAttachments* ptAttachments);
    void                     (*update_render_pass_attachments)       (plDevice* ptDevice, plRenderPassHandle tHandle, plVec2 tDimensions, const plRenderPassAttachments* ptAttachments);
    void                     (*queue_render_pass_for_deletion)       (plDevice* ptDevice, plRenderPassHandle       tHandle);
    void                     (*queue_render_pass_layout_for_deletion)(plDevice* ptDevice, plRenderPassLayoutHandle tHandle);
    void                     (*destroy_render_pass)                  (plDevice* ptDevice, plRenderPassHandle       tHandle);
    void                     (*destroy_render_pass_layout)           (plDevice* ptDevice, plRenderPassLayoutHandle tHandle);

    // shaders
    plShaderHandle        (*create_shader)                    (plDevice* ptDevice, const plShaderDescription* ptDescription);
    plShaderHandle        (*get_shader_variant)               (plDevice* ptDevice, plShaderHandle tHandle, const plShaderVariant* ptVariant);
    plComputeShaderHandle (*create_compute_shader)            (plDevice* ptDevice, const plComputeShaderDescription* ptDescription);
    plComputeShaderHandle (*get_compute_shader_variant)       (plDevice* ptDevice, plComputeShaderHandle tHandle, const plComputeShaderVariant* ptVariant);
    void                  (*queue_shader_for_deletion)        (plDevice* ptDevice, plShaderHandle tHandle);
    void                  (*queue_compute_shader_for_deletion)(plDevice* ptDevice, plComputeShaderHandle tHandle);
    void                  (*destroy_shader)                   (plDevice* ptDevice, plShaderHandle tHandle);
    void                  (*destroy_compute_shader)           (plDevice* ptDevice, plComputeShaderHandle tHandle);
    plShader*             (*get_shader)                       (plDevice* ptDevice, plShaderHandle ptHandle); // do not store

    // texture/buffer ops (blocking)
    void (*generate_mipmaps)        (plDevice* ptDevice, plTextureHandle tTexture);
    void (*copy_buffer)             (plDevice* ptDevice, plBufferHandle tSource, plBufferHandle tDestination, uint32_t uSourceOffset, uint32_t uDestinationOffset, size_t szSize);
    void (*transfer_image_to_buffer)(plDevice* ptDevice, plTextureHandle tTexture, plBufferHandle tBuffer);          // from single layer & single mip textures
    void (*copy_buffer_to_texture)  (plDevice* ptDevice, plBufferHandle tBufferHandle, plTextureHandle tTextureHandle, uint32_t uRegionCount, const plBufferImageCopy* ptRegions);

} plDeviceI;

typedef struct _plGraphicsI
{
    void (*initialize)(plGraphics* ptGraphics);
    void (*resize)    (plGraphics* ptGraphics);
    void (*cleanup)   (plGraphics* ptGraphics);
    void (*setup_ui)  (plGraphics* ptGraphics, plRenderPassHandle tPass);

    // per frame
    bool (*begin_frame) (plGraphics* ptGraphics);

    // command buffers
    plCommandBuffer (*begin_command_recording)(plGraphics* ptGraphics);
    void            (*submit_command)(plGraphics* ptGraphics, plCommandBuffer tCommandBuffer);

    // command buffers (temporary)
    void (*submit_commands)(plGraphics* ptGraphics, plCommandBuffer tCommandBuffer);

    // pass renderer
    plPassRenderer (*begin_render_pass)(plGraphics* ptGraphics, plCommandBuffer tCommandBuffer, plRenderPassHandle tPass);
    void           (*end_render_pass)(plGraphics* ptGraphics, plCommandBuffer tCommandBuffer, plPassRenderer tPass);
    void           (*draw_subpass)(plGraphics* ptGraphics, plCommandBuffer tCommandBuffer, plPassRenderer tPass, uint32_t uAreaCount, plDrawArea* atAreas);
    bool           (*present)(plGraphics* ptGraphics);

    // compute
    void (*dispatch)(plGraphics* ptGraphics, uint32_t uDispatchCount, plDispatch* atDispatches);

    // 2D drawing api
    void (*draw_lists)(plGraphics* ptGraphics, plPassRenderer tPass, plCommandBuffer tCommandBuffer, uint32_t uListCount, plDrawList* atLists);
    void (*create_font_atlas)(plFontAtlas* ptAtlas);
    void (*destroy_font_atlas)(plFontAtlas* ptAtlas);

    // 3D drawing api
    void (*submit_3d_drawlist)    (plDrawList3D* ptDrawlist, plPassRenderer tPass, plCommandBuffer tCommandBuffer, float fWidth, float fHeight, const plMat4* ptMVP, pl3DDrawFlags tFlags, uint32_t uMSAASampleCount);
    void (*register_3d_drawlist)  (plGraphics* ptGraphics, plDrawList3D* ptDrawlist);
    void (*add_3d_triangle_filled)(plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec4 tColor);
    void (*add_3d_line)           (plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec4 tColor, float fThickness);
    void (*add_3d_point)          (plDrawList3D* ptDrawlist, plVec3 tP0, plVec4 tColor, float fLength, float fThickness);
    void (*add_3d_transform)      (plDrawList3D* ptDrawlist, const plMat4* ptTransform, float fLength, float fThickness);
    void (*add_3d_frustum)        (plDrawList3D* ptDrawlist, const plMat4* ptTransform, float fYFov, float fAspect, float fNearZ, float fFarZ, plVec4 tColor, float fThickness);
    void (*add_3d_centered_box)   (plDrawList3D* ptDrawlist, plVec3 tCenter, float fWidth, float fHeight, float fDepth, plVec4 tColor, float fThickness);
    void (*add_3d_aabb)           (plDrawList3D* ptDrawlist, plVec3 tMin, plVec3 tMax, plVec4 tColor, float fThickness);
    void (*add_3d_bezier_quad)    (plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec4 tColor, float fThickness, uint32_t uSegments);
    void (*add_3d_bezier_cubic)   (plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec3 tP3, plVec4 tColor, float fThickness, uint32_t uSegments);

    // misc
    void* (*get_ui_texture_handle)(plGraphics* ptGraphics, plTextureViewHandle tHandle);
} plGraphicsI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plCommandBuffer
{
    void* _pInternal;
} plCommandBuffer;

typedef struct _plPassRenderer
{
    plRenderPassHandle tRenderPassHandle;
    void*              _pInternal;
} plPassRenderer;

typedef struct _plGraphicsState
{
    union 
    {
        struct
        {
            uint64_t ulVertexStreamMask   : 11; // PL_MESH_FORMAT_FLAG_*
            uint64_t ulDepthMode          :  4; // PL_DEPTH_MODE_
            uint64_t ulWireframe          :  1; // bool
            uint64_t ulDepthWriteEnabled  :  1; // bool
            uint64_t ulCullMode           :  2; // VK_CULL_MODE_*
            uint64_t ulBlendMode          :  3; // PL_BLEND_MODE_*
            uint64_t ulStencilMode        :  4;
            uint64_t ulStencilRef         :  8;
            uint64_t ulStencilMask        :  8;
            uint64_t ulStencilOpFail      :  3;
            uint64_t ulStencilOpDepthFail :  3;
            uint64_t ulStencilOpPass      :  3;
            uint64_t _ulUnused            : 13;
        };
        uint64_t ulValue;
    };
    
} plGraphicsState;

typedef struct _plDrawVertex3DSolid
{
    float    pos[3];
    uint32_t uColor;
} plDrawVertex3DSolid;

typedef struct _plDrawVertex3DLine
{
    float    pos[3];
    float    fDirection;
    float    fThickness;
    float    fMultiply;
    float    posother[3];
    uint32_t uColor;
} plDrawVertex3DLine;

typedef struct _plDrawList3D
{
    plGraphics*          ptGraphics;
    plDrawVertex3DSolid* sbtSolidVertexBuffer;
    uint32_t*            sbtSolidIndexBuffer;
    plDrawVertex3DLine*  sbtLineVertexBuffer;
    uint32_t*            sbtLineIndexBuffer;
} plDrawList3D;

typedef struct _plDeviceMemoryAllocation
{
    uint64_t                         uHandle;
    uint64_t                         ulOffset;
    uint64_t                         ulSize;
    char*                            pHostMapped;
    struct plDeviceMemoryAllocatorO* ptInst;
} plDeviceMemoryAllocation;

typedef struct _plDeviceAllocationRange
{
    char     acName[PL_MAX_NAME_LENGTH];
    uint64_t ulOffset;
    uint64_t ulUsedSize;
    uint64_t ulTotalSize;
    uint64_t ulBlockIndex;
    uint32_t uNodeIndex;
    uint32_t uNextNode;
} plDeviceAllocationRange;

typedef struct _plDeviceAllocationBlock
{
    uint64_t                 ulMemoryType;
    uint64_t                 ulAddress;
    uint64_t                 ulSize;
    char*                    pHostMapped;
    uint32_t                 uCurrentIndex; // used but debug tool
    double                   dLastTimeUsed;
} plDeviceAllocationBlock;

typedef struct _plDeviceMemoryAllocatorI
{

    struct plDeviceMemoryAllocatorO* ptInst; // opaque pointer

    plDeviceMemoryAllocation (*allocate)(struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName);
    void                     (*free)    (struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation);
    plDeviceAllocationBlock* (*blocks)  (struct plDeviceMemoryAllocatorO* ptInst, uint32_t* puSizeOut);
    plDeviceAllocationRange* (*ranges)  (struct plDeviceMemoryAllocatorO* ptInst, uint32_t* puSizeOut);
} plDeviceMemoryAllocatorI;

typedef struct _plTextureViewDesc
{
    plFormat tFormat; 
    uint32_t uBaseMip;
    uint32_t uMips;
    uint32_t uBaseLayer;
    uint32_t uLayerCount;
} plTextureViewDesc;

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

typedef struct _plTextureView
{
    plTextureHandle   tTexture;
    plSampler         tSampler;
    plTextureViewDesc tTextureViewDesc;
} plTextureView;

typedef struct _plTextureDesc
{
    plVec3         tDimensions;
    uint32_t       uLayers;
    uint32_t       uMips;
    plFormat       tFormat;
    plTextureType  tType;
    plTextureUsage tUsage;
} plTextureDesc;

typedef struct _plTexture
{
    plTextureDesc            tDesc;
    plDeviceMemoryAllocation tMemoryAllocation;
} plTexture;

typedef struct _plBufferDescription
{
    char           acDebugName[PL_MAX_NAME_LENGTH];
    plBufferUsage  tUsage;
    plMemoryMode   tMemory;
    uint32_t       uByteSize;
} plBufferDescription;

typedef struct _plBuffer
{
    plBufferDescription      tDescription;
    plDeviceMemoryAllocation tMemoryAllocation;
} plBuffer;

typedef struct _plBufferBinding
{
    plBufferBindingType tType;
    uint32_t            uSlot;
    size_t              szSize;
    size_t              szOffset;
    plBufferHandle      tBuffer;
    plStageFlags        tStages;
} plBufferBinding;

typedef struct _plTextureBinding
{
    uint32_t            uSlot;
    plTextureViewHandle tTextureView;
    plStageFlags        tStages;
} plTextureBinding;

typedef struct _plDynamicBinding
{
    uint32_t uBufferHandle;
    uint32_t uByteOffset;
    char*    pcData;
} plDynamicBinding;

typedef struct _plBindGroupLayout
{
    uint32_t         uTextureCount;
    uint32_t         uBufferCount;
    plTextureBinding aTextures[PL_MAX_TEXTURES_PER_BIND_GROUP];
    plBufferBinding  aBuffers[PL_MAX_BUFFERS_PER_BIND_GROUP];
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
    size_t   szBufferOffset;
    int      iImageOffsetX;
    int      iImageOffsetY;
    int      iImageOffsetZ;
    plExtent tImageExtent;
    uint32_t uMipLevel;
    uint32_t uBaseArrayLayer;
    uint32_t uLayerCount;
    uint32_t uBufferRowLength;
    uint32_t uImageHeight;
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
    uint32_t uShaderVariant;
    uint32_t uBindGroup0;
} plDispatch;

typedef struct _plDraw
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
} plDraw;

typedef struct _plDrawStream
{
    plDraw    tCurrentDraw;
    uint32_t* sbtStream;
} plDrawStream;

typedef struct _plSpecializationConstant
{
    uint32_t   uID;
    uint32_t   uOffset;
    plDataType tType;
} plSpecializationConstant;

typedef struct _plShaderVariant
{
    plGraphicsState tGraphicsState;
    const void*     pTempConstantData;
} plShaderVariant;

typedef struct _plComputeShaderVariant
{
    const void* pTempConstantData;
} plComputeShaderVariant;

typedef struct _plShaderDescription
{
    plSpecializationConstant atConstants[PL_MAX_SHADER_SPECIALIZATION_CONSTANTS];
    uint32_t                 uConstantCount;
    plGraphicsState          tGraphicsState;
    const void*              pTempConstantData;
    const char*              pcVertexShader;
    const char*              pcPixelShader;
    const char*              pcVertexShaderEntryFunc;
    const char*              pcPixelShaderEntryFunc;
    plBindGroupLayout        atBindGroupLayouts[3];
    uint32_t                 uBindGroupLayoutCount;
    plRenderPassLayoutHandle tRenderPassLayout;
    uint32_t                 uVariantCount;
    const plShaderVariant*   ptVariants;
    
} plShaderDescription;

typedef struct _plShader
{
    plShaderDescription tDescription;
    plHashMap           tVariantHashmap;
    plShaderHandle*     _sbtVariantHandles; // needed for cleanup
} plShader;

typedef struct _plComputeShaderDescription
{
    const char*                    pcShader;
    const char*                    pcShaderEntryFunc;
    plBindGroupLayout              tBindGroupLayout;
    plSpecializationConstant       atConstants[PL_MAX_SHADER_SPECIALIZATION_CONSTANTS];
    uint32_t                       uConstantCount;
    const void*                    pTempConstantData;
    const plComputeShaderVariant*  ptVariants;
    uint32_t                       uVariantCount;
} plComputeShaderDescription;

typedef struct _plComputeShader
{
    plComputeShaderDescription tDescription;
    plHashMap                  tVariantHashmap;
    plComputeShaderHandle*     _sbtVariantHandles; // needed for cleanup
} plComputeShader;

typedef struct _plSubpass
{
    uint32_t uRenderTargetCount;
    uint32_t uSubpassInputCount;
    uint32_t auRenderTargets[16];
    uint32_t auSubpassInputs[16];
    bool     bDepthTarget;
} plSubpass;

typedef struct _plDepthTarget
{
    plFormat            tFormat;
    plLoadOp            tLoadOp;
    plStoreOp           tStoreOp;
    plLoadOp            tStencilLoadOp;
    plStoreOp           tStencilStoreOp;
    plTextureLayout     tNextUsage;
    float               fClearZ;
    uint32_t            uClearStencil;
} plDepthTarget;

typedef struct _plRenderTarget
{
    plFormat        tFormat;
    plLoadOp        tLoadOp;
    plStoreOp       tStoreOp;
    plTextureLayout tNextUsage;
    plVec4          tClearColor;
} plRenderTarget;

typedef struct _plRenderPassLayoutDescription
{
    plDepthTarget   tDepthTarget;
    plRenderTarget  atRenderTargets[16];
    plSubpass       atSubpasses[16];
} plRenderPassLayoutDescription;

typedef struct _plRenderPassLayout
{
    plRenderPassLayoutDescription tDesc;
} plRenderPassLayout;

typedef struct _plRenderPassAttachments
{
    plTextureViewHandle atViewAttachments[16];
} plRenderPassAttachments;

typedef struct _plRenderPassDescription
{
    plRenderPassLayoutHandle tLayout;
    plRenderTarget           atRenderTargets[16];
    plDepthTarget            tDepthTarget;
    plVec2                   tDimensions;
    uint32_t                 uAttachmentCount;
    uint32_t                 uAttachmentSets;
} plRenderPassDescription;

typedef struct _plRenderPass
{
    plRenderPassDescription tDesc;
    bool                    bSwapchain;
} plRenderPass;

typedef struct _plDevice
{
    plGraphics* ptGraphics;
    plDeviceMemoryAllocatorI tLocalDedicatedAllocator;
    plDeviceMemoryAllocatorI tLocalBuddyAllocator;
    plDeviceMemoryAllocatorI tStagingUnCachedAllocator;
    plDeviceMemoryAllocatorI tStagingCachedAllocator;
    void* _pInternalData;
} plDevice;

typedef struct _plSwapchain
{
    plExtent             tExtent;
    plFormat             tFormat;
    uint32_t             uImageCount;
    plTextureViewHandle* sbtSwapchainTextureViews;
    uint32_t             uCurrentImageIndex; // current image to use within the swap chain
    bool                 bVSync;

    // platform specific
    void* _pInternalData;
} plSwapchain;

typedef struct _plGraphics
{
    plDevice        tDevice;
    plSwapchain     tSwapchain;
    uint32_t        uCurrentFrameIndex;
    uint32_t        uFramesInFlight;
    plDrawList3D**  sbt3DDrawlists;
    plFrameGarbage* sbtGarbage;
    size_t          szLocalMemoryInUse;
    size_t          szHostMemoryInUse;
    bool            bValidationActive;

    // render pass layouts
    plRenderPassLayout* sbtRenderPassLayoutsCold;
    uint32_t*           sbtRenderPassLayoutGenerations;
    uint32_t*           sbtRenderPassLayoutFreeIndices;

    // render passes
    plRenderPassLayoutHandle tMainRenderPassLayout;
    plRenderPassHandle tMainRenderPass;
    plRenderPass* sbtRenderPassesCold;
    uint32_t*     sbtRenderPassGenerations;
    uint32_t*     sbtRenderPassFreeIndices;

    // shaders
    plShader* sbtShadersCold;
    uint32_t* sbtShaderGenerations;
    uint32_t* sbtShaderFreeIndices;

    // compute shaders
    plComputeShader* sbtComputeShadersCold;
    uint32_t*        sbtComputeShaderGenerations;
    uint32_t*        sbtComputeShaderFreeIndices;

    // buffers
    plBuffer* sbtBuffersCold;
    uint32_t* sbtBufferGenerations;
    uint32_t* sbtBufferFreeIndices;

    // textures
    plTexture* sbtTexturesCold;
    uint32_t*  sbtTextureGenerations;
    uint32_t*  sbtTextureFreeIndices;

    // texture views
    plTextureView* sbtTextureViewsCold;
    uint32_t*      sbtTextureViewGenerations;
    uint32_t*      sbtTextureViewFreeIndices;

    // bind groups
    plBindGroup* sbtBindGroupsCold;
    uint32_t*    sbtBindGroupGenerations;
    uint32_t*    sbtBindGroupFreeIndices;

    // platform specific
    void* _pInternalData;
} plGraphics;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _pl3DDrawFlags
{
    PL_PIPELINE_FLAG_NONE          = 0,
    PL_PIPELINE_FLAG_DEPTH_TEST    = 1 << 0,
    PL_PIPELINE_FLAG_DEPTH_WRITE   = 1 << 1,
    PL_PIPELINE_FLAG_CULL_FRONT    = 1 << 2,
    PL_PIPELINE_FLAG_CULL_BACK     = 1 << 3,
    PL_PIPELINE_FLAG_FRONT_FACE_CW = 1 << 4,
};

enum _plStageFlags
{
    PL_STAGE_NONE    = 1 << 0,
    PL_STAGE_VERTEX  = 1 << 1,
    PL_STAGE_PIXEL   = 1 << 2,
    PL_STAGE_COMPUTE = 1 << 3,
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
    PL_FORMAT_UNKNOWN,
    PL_FORMAT_R32G32B32_FLOAT,
    PL_FORMAT_R32G32B32A32_FLOAT,
    PL_FORMAT_R8G8B8A8_UNORM,
    PL_FORMAT_R32G32_FLOAT,
    PL_FORMAT_R8G8B8A8_SRGB,
    PL_FORMAT_B8G8R8A8_SRGB,
    PL_FORMAT_B8G8R8A8_UNORM,
    PL_FORMAT_D32_FLOAT,
    PL_FORMAT_D32_FLOAT_S8_UINT,
    PL_FORMAT_D24_UNORM_S8_UINT,
    PL_FORMAT_D16_UNORM_S8_UINT
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
    PL_TEXTURE_BINDING_TYPE_STORAGE
};

enum _plTextureType
{
    PL_TEXTURE_TYPE_UNSPECIFIED,
    PL_TEXTURE_TYPE_2D,
    PL_TEXTURE_TYPE_CUBE
};

enum _plBufferUsage
{
    PL_BUFFER_USAGE_UNSPECIFIED,
    PL_BUFFER_USAGE_INDEX,
    PL_BUFFER_USAGE_VERTEX,
    PL_BUFFER_USAGE_UNIFORM,
    PL_BUFFER_USAGE_STORAGE
};

enum _plTextureUsage
{
    PL_TEXTURE_USAGE_UNSPECIFIED              = 0,
    PL_TEXTURE_USAGE_SAMPLED                  = 1 << 0,
    PL_TEXTURE_USAGE_COLOR_ATTACHMENT         = 1 << 1,
    PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT = 1 << 2,
    PL_TEXTURE_USAGE_TRANSIENT_ATTACHMENT     = 1 << 3
};

enum _plTextureLayout
{
    PL_TEXTURE_LAYOUT_NONE,
    PL_TEXTURE_LAYOUT_RENDER_TARGET,
    PL_TEXTURE_LAYOUT_DEPTH_STENCIL,
    PL_TEXTURE_LAYOUT_PRESENT,
    PL_TEXTURE_LAYOUT_SHADER_READ,
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

enum _plLoadOp
{
    PL_LOAD_OP_LOAD,
    PL_LOAD_OP_CLEAR,
    PL_LOAD_OP_DONT_CARE
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