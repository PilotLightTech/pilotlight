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

#ifndef PL_DEVICE_ALLOCATION_BLOCK_SIZE
    #define PL_DEVICE_ALLOCATION_BLOCK_SIZE 268435456
#endif

#ifndef PL_DEVICE_LOCAL_LEVELS
    #define PL_DEVICE_LOCAL_LEVELS 8
#endif

#define PL_ALIGN_UP(num, align) (((num) + ((align)-1)) & ~((align)-1))

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define PL_API_GRAPHICS "PL_API_GRAPHICS"
typedef struct _plGraphicsI plGraphicsI;

#define PL_API_DEVICE "PL_API_DEVICE"
typedef struct _plDeviceI plDeviceI;

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plDevice            plDevice;
typedef struct _plBuffer            plBuffer;
typedef struct _plGraphics          plGraphics;
typedef struct _plDraw              plDraw;
typedef struct _plDrawArea          plDrawArea;
typedef struct _plGraphicsState     plGraphicsState;
typedef struct _plShaderDescription plShaderDescription;
typedef struct _plShader            plShader;
typedef struct _plBuffer            plBuffer;
typedef struct _plBufferDescription plBufferDescription;
typedef struct _plDynamicBuffer     plDynamicBuffer;
typedef struct _plSampler           plSampler;
typedef struct _plTextureViewDesc   plTextureViewDesc;
typedef struct _plTexture           plTexture;
typedef struct _plTextureView       plTextureView;
typedef struct _plTextureDesc       plTextureDesc;
typedef struct _plBindGroupLayout   plBindGroupLayout;
typedef struct _plBindGroup         plBindGroup;
typedef struct _plBufferBinding     plBufferBinding;
typedef struct _plTextureBinding    plTextureBinding;
typedef struct _plDynamicBinding    plDynamicBinding;

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
typedef int plBufferBindingType;      // -> enum _plBufferBindingType      // Enum:
typedef int plTextureBindingType;     // -> enum _plTextureBindingType     // Enum:
typedef int plTextureType;            // -> enum _plTextureType            // Enum:
typedef int plBufferUsage;            // -> enum _plBufferUsage            // Enum:
typedef int plTextureUsage;           // -> enum _plTextureUsage           // Enum:
typedef int plMeshFormatFlags;        // -> enum _plMeshFormatFlags        // Flags:
typedef int plShaderTextureFlags;     // -> enum _plShaderTextureFlags     // Flags:
typedef int plBlendMode;              // -> enum _plBlendMode              // Enum:
typedef int plDepthMode;              // -> enum _plDepthMode              // Enum:
typedef int plCullMode;               // -> enum _plCullMode               // Enum:
typedef int plStencilMode;            // -> enum _plStencilMode            // Enum:
typedef int plFilter;                 // -> enum _plFilter                 // Enum:
typedef int plWrapMode;               // -> enum _plWrapMode               // Enum:
typedef int plCompareMode;            // -> enum _plCompareMode            // Enum:
typedef int plFormat;                 // -> enum _plFormat                 // Enum:
typedef int plStencilOp;              // -> enum _plStencilOp              // Enum:
typedef int plMemoryMode;             // -> enum _plMemoryMode             // Enum:
typedef int plDeviceAllocationStatus; // -> enum _plDeviceAllocationStatus // Enum:

// external
typedef struct _plDrawList plDrawList;
typedef struct _plFontAtlas plFontAtlas;

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plDeviceI
{
    // commited resources
    plBuffer      (*create_buffer)            (plDevice* ptDevice, const plBufferDescription* ptDesc);
    plTexture     (*create_texture)           (plDevice* ptDevice, plTextureDesc tDesc, size_t szSize, const void* pData, const char* pcName);
    plTextureView (*create_texture_view)      (plDevice* ptDevice, const plTextureViewDesc* ptViewDesc, const plSampler* ptSampler, plTexture* ptTexture, const char* pcName);
    plBindGroup   (*create_bind_group)        (plDevice* ptDevice, plBindGroupLayout* ptLayout);
    void          (*update_bind_group)        (plDevice* ptDevice, plBindGroup* ptGroup, uint32_t uBufferCount, plBuffer* atBuffers, size_t* aszBufferRanges, uint32_t uTextureViewCount, plTextureView* atTextureViews);

    plDynamicBinding (*allocate_dynamic_data)(plDevice* ptDevice, size_t szSize);

    // cleanup
    void (*submit_buffer_for_deletion)      (plDevice* ptDevice, plBuffer* ptBuffer);
    void (*submit_texture_for_deletion)     (plDevice* ptDevice, plTexture* ptTexture);
    void (*submit_texture_view_for_deletion)(plDevice* ptDevice, plTextureView* ptView);
} plDeviceI;

typedef struct _plGraphicsI
{
    void (*initialize)(plGraphics* ptGraphics);
    void (*resize)    (plGraphics* ptGraphics);
    void (*cleanup)   (plGraphics* ptGraphics);

    // buffers

    // per frame
    bool (*begin_frame)    (plGraphics* ptGraphics);
    void (*end_frame)      (plGraphics* ptGraphics);
    void (*begin_recording)(plGraphics* ptGraphics);
    void (*end_recording)  (plGraphics* ptGraphics);

    // shaders
    plShader (*create_shader)(plGraphics* ptGraphics, plShaderDescription* ptDescription);

    // drawing
    void (*draw_areas)(plGraphics* ptGraphics, uint32_t uAreaCount, plDrawArea* atAreas, plDraw* atDraws);

    // 2D drawing api
    void (*draw_lists)(plGraphics* ptGraphics, uint32_t uListCount, plDrawList* atLists);
    void (*create_font_atlas)(plFontAtlas* ptAtlas);
    void (*destroy_font_atlas)(plFontAtlas* ptAtlas);

    // 3D drawing api
    void (*submit_3d_drawlist)    (plDrawList3D* ptDrawlist, float fWidth, float fHeight, const plMat4* ptMVP, pl3DDrawFlags tFlags);
    void (*register_3d_drawlist)  (plGraphics* ptGraphics, plDrawList3D* ptDrawlist);
    void (*add_3d_triangle_filled)(plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec4 tColor);
    void (*add_3d_line)           (plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec4 tColor, float fThickness);
    void (*add_3d_point)          (plDrawList3D* ptDrawlist, plVec3 tP0, plVec4 tColor, float fLength, float fThickness);
    void (*add_3d_transform)      (plDrawList3D* ptDrawlist, const plMat4* ptTransform, float fLength, float fThickness);
    void (*add_3d_frustum)        (plDrawList3D* ptDrawlist, const plMat4* ptTransform, float fYFov, float fAspect, float fNearZ, float fFarZ, plVec4 tColor, float fThickness);
    void (*add_3d_centered_box)   (plDrawList3D* ptDrawlist, plVec3 tCenter, float fWidth, float fHeight, float fDepth, plVec4 tColor, float fThickness);
    void (*add_3d_bezier_quad)    (plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec4 tColor, float fThickness, uint32_t uSegments);
    void (*add_3d_bezier_cubic)   (plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec3 tP3, plVec4 tColor, float fThickness, uint32_t uSegments);

    // misc
    void* (*get_ui_texture_handle)(plGraphics* ptGraphics, plTextureView* ptTextureView);
} plGraphicsI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

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
    const char*              pcName;
    plDeviceAllocationStatus tStatus;
    plDeviceMemoryAllocation tAllocation;
} plDeviceAllocationRange;

typedef struct _plDeviceAllocationBlock
{
    uint64_t                 ulAddress;
    uint64_t                 ulSize;
    char*                    pHostMapped;
    plDeviceAllocationRange  tRange;
} plDeviceAllocationBlock;

typedef struct _plDeviceMemoryAllocatorI
{

    struct plDeviceMemoryAllocatorO* ptInst; // opaque pointer

    plDeviceMemoryAllocation (*allocate)(struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName);
    void                     (*free)    (struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation);
    plDeviceAllocationBlock* (*blocks)  (struct plDeviceMemoryAllocatorO* ptInst, uint32_t* puSizeOut);
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
    uint32_t          uTextureHandle;
    plSampler         tSampler;
    plTextureViewDesc tTextureViewDesc;
    uint32_t          _uSamplerHandle;
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
    uint32_t                 uHandle;
    plDeviceMemoryAllocation tMemoryAllocation;
} plTexture;

typedef struct _plBufferDescription
{
    const char*    pcDebugName;
    plBufferUsage  tUsage;
    plMemoryMode   tMemory;
    uint32_t       uByteSize;
    uint32_t       uInitialDataByteSize;
    const uint8_t* puInitialData;
} plBufferDescription;

typedef struct _plBuffer
{
    plBufferDescription      tDescription;
    uint32_t                 uHandle;
    plDeviceMemoryAllocation tMemoryAllocation;
} plBuffer;

typedef struct _plBufferBinding
{
    plBufferBindingType tType;
    uint32_t            uSlot;
    size_t              szSize;
    size_t              szOffset;
    plBuffer            tBuffer;
} plBufferBinding;

typedef struct _plTextureBinding
{
    uint32_t      uSlot;
    plTextureView tTextureView;
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
    plTextureBinding aTextures[8];
    plBufferBinding  aBuffers[8];
    uint32_t         uHandle;
} plBindGroupLayout;

typedef struct _plBindGroup
{
    plBindGroupLayout tLayout;
    uint32_t          uHandle;
} plBindGroup;

typedef struct _plDrawArea
{
    uint32_t     uDrawOffset;
    uint32_t     uDrawCount;
} plDrawArea;

typedef struct _plDraw
{
    uint32_t         uDynamicBuffer;
    uint32_t         uVertexBuffer;
    uint32_t         uIndexBuffer;
    uint32_t         uVertexOffset;
    uint32_t         uIndexOffset;
    uint32_t         uVertexCount;
    uint32_t         uIndexCount;
    uint32_t         uShaderVariant;
    plBindGroup*     aptBindGroups[3];
    uint32_t         auDynamicBufferOffset[1];
} plDraw;

typedef struct _plShaderDescription
{
    plGraphicsState   tGraphicsState;
    const char*       pcVertexShader;
    const char*       pcPixelShader;
    plBindGroupLayout atBindGroupLayouts[3];
    uint32_t          uBindGroupLayoutCount;
} plShaderDescription;

typedef struct _plShader
{
    plShaderDescription tDescription;
    uint32_t            uHandle;
} plShader;

typedef struct _plDevice
{
    plDeviceMemoryAllocatorI tLocalDedicatedAllocator;
    plDeviceMemoryAllocatorI tStagingUnCachedAllocator;
    void* _pInternalData;
} plDevice;

typedef struct _plGraphics
{
    plDevice       tDevice;
    uint32_t       uCurrentFrameIndex;
    uint32_t       uFramesInFlight;
    plDrawList3D** sbt3DDrawlists;
    void*          _pInternalData;
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
    PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT = 1 << 2
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
    PL_SHADER_TEXTURE_FLAG_BINDING_4          = 1 << 4,
    PL_SHADER_TEXTURE_FLAG_BINDING_5          = 1 << 5,
    PL_SHADER_TEXTURE_FLAG_BINDING_6          = 1 << 6,
    PL_SHADER_TEXTURE_FLAG_BINDING_7          = 1 << 7,
    PL_SHADER_TEXTURE_FLAG_BINDING_8          = 1 << 8,
    PL_SHADER_TEXTURE_FLAG_BINDING_9          = 1 << 9,
    PL_SHADER_TEXTURE_FLAG_BINDING_10         = 1 << 10,
    PL_SHADER_TEXTURE_FLAG_BINDING_11         = 1 << 11,
    PL_SHADER_TEXTURE_FLAG_BINDING_12         = 1 << 12,
    PL_SHADER_TEXTURE_FLAG_BINDING_13         = 1 << 13,
    PL_SHADER_TEXTURE_FLAG_BINDING_14         = 1 << 14
};

enum _plMemoryMode
{
    PL_MEMORY_GPU,
    PL_MEMORY_GPU_CPU,
    PL_MEMORY_CPU
};

enum _plDeviceAllocationStatus
{
    PL_DEVICE_ALLOCATION_STATUS_FREE,
    PL_DEVICE_ALLOCATION_STATUS_USED,
    PL_DEVICE_ALLOCATION_STATUS_WASTE
};

#endif // PL_GRAPHICS_EXT_H