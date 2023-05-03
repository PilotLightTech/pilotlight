/*
   pl_graphics.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] structs
// [SECTION] enums
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_GRAPHICS_H
#define PL_GRAPHICS_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// types
typedef struct _plMesh          plMesh;
typedef struct _plSampler       plSampler;
typedef struct _plGraphicsState plGraphicsState;

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

#endif // PL_GRAPHICS_H