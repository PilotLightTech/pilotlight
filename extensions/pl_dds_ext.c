/*
   pl_dds_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] enums
// [SECTION] internal structs
// [SECTION] internal API
// [SECTION] public api implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <string.h>
#include "pl.h"
#include "pl_dds_ext.h"

// extensions
#include "pl_graphics_ext.h"

// libs
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum DDS_PIXELFORMAT_FLAGS
{
    DDPF_ALPHAPIXELS = 0x1,		// Texture contains alpha data; dwRGBAlphaBitMask contains valid data.
    DDPF_ALPHA       = 0x2,     // Used in some older DDS files for alpha channel only uncompressed data (dwRGBBitCount contains the alpha channel bitcount; dwABitMask contains valid data)
    DDPF_FOURCC      = 0x4,		// Texture contains compressed RGB data; dwFourCC contains valid data.
    DDPF_RGB         = 0x40,	// Texture contains uncompressed RGB data; dwRGBBitCount and the RGB masks (dwRBitMask, dwGBitMask, dwBBitMask) contain valid data.
    DDPF_YUV         = 0x200,	// Used in some older DDS files for YUV uncompressed data (dwRGBBitCount contains the YUV bit count; dwRBitMask contains the Y mask, dwGBitMask contains the U mask, dwBBitMask contains the V mask)
    DDPF_LUMINANCE   = 0x20000	// Used in some older DDS files for single channel color uncompressed data (dwRGBBitCount contains the luminance channel bit count; dwRBitMask contains the channel mask). Can be combined with DDPF_ALPHAPIXELS for a two channel DDS file.
};

enum DDSD_CAPS
{
    DDSD_CAPS        = 0x1,		// Required in every .dds file.
    DDSD_HEIGHT      = 0x2,		// Required in every .dds file.
    DDSD_WIDTH       = 0x4,		// Required in every .dds file.
    DDSD_PITCH       = 0x8,		// Required when pitch is provided for an uncompressed texture.
    DDSD_PIXELFORMAT = 0x1000,	// Required in every .dds file.
    DDSD_MIPMAPCOUNT = 0x20000,	// Required in a mipmapped texture.
    DDSD_LINEARSIZE  = 0x80000,	// Required when pitch is provided for a compressed texture.
    DDSD_DEPTH       = 0x800000 // Required in a depth texture.
};

enum DDSCAPS
{
    DDSCAPS_COMPLEX = 0x8,		// Optional; must be used on any file that contains more than one surface (a mipmap, a cubic environment map, or mipmapped volume texture).
    DDSCAPS_MIPMAP  = 0x400000,	// Optional; should be used for a mipmap.
    DDSCAPS_TEXTURE = 0x1000,	// Required
};

enum DDSCAPS2
{
    DDSCAPS2_CUBEMAP           = 0x200,    // Required for a cube map.
    DDSCAPS2_CUBEMAP_POSITIVEX = 0x400,	   // Required when these surfaces are stored in a cube map.
    DDSCAPS2_CUBEMAP_NEGATIVEX = 0x800,	   // Required when these surfaces are stored in a cube map.
    DDSCAPS2_CUBEMAP_POSITIVEY = 0x1000,   // Required when these surfaces are stored in a cube map.
    DDSCAPS2_CUBEMAP_NEGATIVEY = 0x2000,   // Required when these surfaces are stored in a cube map.
    DDSCAPS2_CUBEMAP_POSITIVEZ = 0x4000,   // Required when these surfaces are stored in a cube map.
    DDSCAPS2_CUBEMAP_NEGATIVEZ = 0x8000,   // Required when these surfaces are stored in a cube map.
    DDSCAPS2_VOLUME            = 0x200000, // Required for a volume texture.
};

enum D3D10_RESOURCE_DIMENSION
{
    D3D10_RESOURCE_DIMENSION_UNKNOWN   = 0,
    D3D10_RESOURCE_DIMENSION_BUFFER    = 1,
    D3D10_RESOURCE_DIMENSION_TEXTURE1D = 2,
    D3D10_RESOURCE_DIMENSION_TEXTURE2D = 3,
    D3D10_RESOURCE_DIMENSION_TEXTURE3D = 4
};

enum DDS_RESOURCE_MISC_TEXTURECUBE
{
    DDS_RESOURCE_MISC_TEXTURECUBE = 0x4, // Indicates a 2D texture is a cube-map texture.
};

enum DDS_ALPHA_MODE
{
    DDS_ALPHA_MODE_UNKNOWN       = 0x0,	// Alpha channel content is unknown. This is the value for legacy files, which typically is assumed to be 'straight' alpha.
    DDS_ALPHA_MODE_STRAIGHT      = 0x1,	// Any alpha channel content is presumed to use straight alpha.
    DDS_ALPHA_MODE_PREMULTIPLIED = 0x2,	// Any alpha channel content is using premultiplied alpha. The only legacy file formats that indicate this information are 'DX2' and 'DX4'.
    DDS_ALPHA_MODE_OPAQUE        = 0x3, // Any alpha channel content is all set to fully opaque.
    DDS_ALPHA_MODE_CUSTOM        = 0x4, // Any alpha channel content is being used as a 4th channel and is not intended to represent transparency (straight or premultiplied).
};

enum DXGI_FORMAT {
    DXGI_FORMAT_UNKNOWN = 0,
    DXGI_FORMAT_R32G32B32A32_TYPELESS = 1,
    DXGI_FORMAT_R32G32B32A32_FLOAT = 2,
    DXGI_FORMAT_R32G32B32A32_UINT = 3,
    DXGI_FORMAT_R32G32B32A32_SINT = 4,
    DXGI_FORMAT_R32G32B32_TYPELESS = 5,
    DXGI_FORMAT_R32G32B32_FLOAT = 6,
    DXGI_FORMAT_R32G32B32_UINT = 7,
    DXGI_FORMAT_R32G32B32_SINT = 8,
    DXGI_FORMAT_R16G16B16A16_TYPELESS = 9,
    DXGI_FORMAT_R16G16B16A16_FLOAT = 10,
    DXGI_FORMAT_R16G16B16A16_UNORM = 11,
    DXGI_FORMAT_R16G16B16A16_UINT = 12,
    DXGI_FORMAT_R16G16B16A16_SNORM = 13,
    DXGI_FORMAT_R16G16B16A16_SINT = 14,
    DXGI_FORMAT_R32G32_TYPELESS = 15,
    DXGI_FORMAT_R32G32_FLOAT = 16,
    DXGI_FORMAT_R32G32_UINT = 17,
    DXGI_FORMAT_R32G32_SINT = 18,
    DXGI_FORMAT_R32G8X24_TYPELESS = 19,
    DXGI_FORMAT_D32_FLOAT_S8X24_UINT = 20,
    DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS = 21,
    DXGI_FORMAT_X32_TYPELESS_G8X24_UINT = 22,
    DXGI_FORMAT_R10G10B10A2_TYPELESS = 23,
    DXGI_FORMAT_R10G10B10A2_UNORM = 24,
    DXGI_FORMAT_R10G10B10A2_UINT = 25,
    DXGI_FORMAT_R11G11B10_FLOAT = 26,
    DXGI_FORMAT_R8G8B8A8_TYPELESS = 27,
    DXGI_FORMAT_R8G8B8A8_UNORM = 28,
    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB = 29,
    DXGI_FORMAT_R8G8B8A8_UINT = 30,
    DXGI_FORMAT_R8G8B8A8_SNORM = 31,
    DXGI_FORMAT_R8G8B8A8_SINT = 32,
    DXGI_FORMAT_R16G16_TYPELESS = 33,
    DXGI_FORMAT_R16G16_FLOAT = 34,
    DXGI_FORMAT_R16G16_UNORM = 35,
    DXGI_FORMAT_R16G16_UINT = 36,
    DXGI_FORMAT_R16G16_SNORM = 37,
    DXGI_FORMAT_R16G16_SINT = 38,
    DXGI_FORMAT_R32_TYPELESS = 39,
    DXGI_FORMAT_D32_FLOAT = 40,
    DXGI_FORMAT_R32_FLOAT = 41,
    DXGI_FORMAT_R32_UINT = 42,
    DXGI_FORMAT_R32_SINT = 43,
    DXGI_FORMAT_R24G8_TYPELESS = 44,
    DXGI_FORMAT_D24_UNORM_S8_UINT = 45,
    DXGI_FORMAT_R24_UNORM_X8_TYPELESS = 46,
    DXGI_FORMAT_X24_TYPELESS_G8_UINT = 47,
    DXGI_FORMAT_R8G8_TYPELESS = 48,
    DXGI_FORMAT_R8G8_UNORM = 49,
    DXGI_FORMAT_R8G8_UINT = 50,
    DXGI_FORMAT_R8G8_SNORM = 51,
    DXGI_FORMAT_R8G8_SINT = 52,
    DXGI_FORMAT_R16_TYPELESS = 53,
    DXGI_FORMAT_R16_FLOAT = 54,
    DXGI_FORMAT_D16_UNORM = 55,
    DXGI_FORMAT_R16_UNORM = 56,
    DXGI_FORMAT_R16_UINT = 57,
    DXGI_FORMAT_R16_SNORM = 58,
    DXGI_FORMAT_R16_SINT = 59,
    DXGI_FORMAT_R8_TYPELESS = 60,
    DXGI_FORMAT_R8_UNORM = 61,
    DXGI_FORMAT_R8_UINT = 62,
    DXGI_FORMAT_R8_SNORM = 63,
    DXGI_FORMAT_R8_SINT = 64,
    DXGI_FORMAT_A8_UNORM = 65,
    DXGI_FORMAT_R1_UNORM = 66,
    DXGI_FORMAT_R9G9B9E5_SHAREDEXP = 67,
    DXGI_FORMAT_R8G8_B8G8_UNORM = 68,
    DXGI_FORMAT_G8R8_G8B8_UNORM = 69,
    DXGI_FORMAT_BC1_TYPELESS = 70,
    DXGI_FORMAT_BC1_UNORM = 71,
    DXGI_FORMAT_BC1_UNORM_SRGB = 72,
    DXGI_FORMAT_BC2_TYPELESS = 73,
    DXGI_FORMAT_BC2_UNORM = 74,
    DXGI_FORMAT_BC2_UNORM_SRGB = 75,
    DXGI_FORMAT_BC3_TYPELESS = 76,
    DXGI_FORMAT_BC3_UNORM = 77,
    DXGI_FORMAT_BC3_UNORM_SRGB = 78,
    DXGI_FORMAT_BC4_TYPELESS = 79,
    DXGI_FORMAT_BC4_UNORM = 80,
    DXGI_FORMAT_BC4_SNORM = 81,
    DXGI_FORMAT_BC5_TYPELESS = 82,
    DXGI_FORMAT_BC5_UNORM = 83,
    DXGI_FORMAT_BC5_SNORM = 84,
    DXGI_FORMAT_B5G6R5_UNORM = 85,
    DXGI_FORMAT_B5G5R5A1_UNORM = 86,
    DXGI_FORMAT_B8G8R8A8_UNORM = 87,
    DXGI_FORMAT_B8G8R8X8_UNORM = 88,
    DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM = 89,
    DXGI_FORMAT_B8G8R8A8_TYPELESS = 90,
    DXGI_FORMAT_B8G8R8A8_UNORM_SRGB = 91,
    DXGI_FORMAT_B8G8R8X8_TYPELESS = 92,
    DXGI_FORMAT_B8G8R8X8_UNORM_SRGB = 93,
    DXGI_FORMAT_BC6H_TYPELESS = 94,
    DXGI_FORMAT_BC6H_UF16 = 95,
    DXGI_FORMAT_BC6H_SF16 = 96,
    DXGI_FORMAT_BC7_TYPELESS = 97,
    DXGI_FORMAT_BC7_UNORM = 98,
    DXGI_FORMAT_BC7_UNORM_SRGB = 99,
    DXGI_FORMAT_AYUV = 100,
    DXGI_FORMAT_Y410 = 101,
    DXGI_FORMAT_Y416 = 102,
    DXGI_FORMAT_NV12 = 103,
    DXGI_FORMAT_P010 = 104,
    DXGI_FORMAT_P016 = 105,
    DXGI_FORMAT_420_OPAQUE = 106,
    DXGI_FORMAT_YUY2 = 107,
    DXGI_FORMAT_Y210 = 108,
    DXGI_FORMAT_Y216 = 109,
    DXGI_FORMAT_NV11 = 110,
    DXGI_FORMAT_AI44 = 111,
    DXGI_FORMAT_IA44 = 112,
    DXGI_FORMAT_P8 = 113,
    DXGI_FORMAT_A8P8 = 114,
    DXGI_FORMAT_B4G4R4A4_UNORM = 115,
    DXGI_FORMAT_P208 = 130,
    DXGI_FORMAT_V208 = 131,
    DXGI_FORMAT_V408 = 132,
    DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE,
    DXGI_FORMAT_SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE,
    D3DFMT_R8G8B8, // Note: you will need to handle conversion of this legacy format yourself
    DXGI_FORMAT_FORCE_DWORD = 0xffffffff
};

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef int DXGI_FORMAT;
typedef int DDS_ALPHA_MODE;
typedef int D3D10_RESOURCE_DIMENSION;

typedef struct _plDssPixelFormat
{
    uint32_t uSize;
    uint32_t uFlags;
    uint32_t uFourCC;
    uint32_t uRGBBitCount;
    uint32_t uRBitMask;
    uint32_t uGBitMask;
    uint32_t uBBitMask;
    uint32_t uABitMask;
} plDssPixelFormat;

typedef struct _plDdsSubHeader
{
    uint32_t         uSize;  // size of this struct
    uint32_t         uFlags; // flags for members included
    uint32_t         uHeight;
    uint32_t         uWidth;
    uint32_t         uPitchOrLinearSize;
    uint32_t         uDepth;
    uint32_t         uMips;
    uint32_t         auReserved1[11];
    plDssPixelFormat ddspf;
    uint32_t         uCaps;
    uint32_t         uCaps2;
    uint32_t         uCaps3;
    uint32_t         uCaps4;
    uint32_t         uReserved2;
} plDdsSubHeader;

typedef struct _plDdsHeaderDxt10
{
    DXGI_FORMAT              tDxgiFormat;
    D3D10_RESOURCE_DIMENSION tResourceDimension;
    uint32_t                 uMiscFlag;
    uint32_t                 uLayers;
    uint32_t                 uMiscFlags2;
} plDdsHeaderDxt10;

typedef struct _plDdsHeader
{
    uint32_t         uMagic;
    plDdsSubHeader   tHeader;
    plDdsHeaderDxt10 tHeader10;
} plDdsHeader;

//-----------------------------------------------------------------------------
// [SECTION] internal API
//-----------------------------------------------------------------------------

static inline uint32_t
pl__dds_fourcc(char a, char b, char c, char d)
{
    return (((unsigned)(d) << 24) | ((unsigned)(c) << 16) | ((unsigned)(b) << 8) | (unsigned)(a));
}

static inline uint32_t
pl__dds_block_size(plFormat tFormat)
{
    switch(tFormat)
    {
        case PL_FORMAT_BC1_RGBA_UNORM:
        case PL_FORMAT_BC1_RGBA_SRGB:
        case PL_FORMAT_BC4_UNORM:
        case PL_FORMAT_BC4_SNORM:
        case PL_FORMAT_BC2_UNORM:
        case PL_FORMAT_BC2_SRGB:
        case PL_FORMAT_BC3_UNORM:
        case PL_FORMAT_BC3_SRGB:
        case PL_FORMAT_BC5_UNORM:
        case PL_FORMAT_BC5_SNORM:
        case PL_FORMAT_BC6H_UFLOAT:
        case PL_FORMAT_BC6H_FLOAT:
        case PL_FORMAT_BC7_UNORM:
        case PL_FORMAT_BC7_SRGB:
            return 4;
        default:
            return 1;
    }
}

static inline uint32_t
pl__dds_bits_per_element(plFormat tFormat)
{
    switch (tFormat)
    {
        case PL_FORMAT_R32G32B32A32_FLOAT:
        case PL_FORMAT_R32G32B32A32_UINT:
        case PL_FORMAT_R32G32B32A32_SINT:
            return 128;

        case PL_FORMAT_R16G16B16A16_FLOAT:
        case PL_FORMAT_R16G16B16A16_UNORM:
        case PL_FORMAT_R16G16B16A16_UINT:
        case PL_FORMAT_R16G16B16A16_SNORM:
        case PL_FORMAT_R16G16B16A16_SINT:
        case PL_FORMAT_R32G32_FLOAT:
        case PL_FORMAT_R32G32_UINT:
        case PL_FORMAT_R32G32_SINT:
            return 64;

        case PL_FORMAT_R10G10B10A2_UNORM:
        case PL_FORMAT_R10G10B10A2_UINT:
        case PL_FORMAT_R11G11B10_FLOAT:
        case PL_FORMAT_R8G8B8A8_UNORM:
        case PL_FORMAT_R8G8B8A8_SRGB:
        case PL_FORMAT_R8G8B8A8_UINT:
        case PL_FORMAT_R8G8B8A8_SNORM:
        case PL_FORMAT_R8G8B8A8_SINT:
        case PL_FORMAT_R16G16_FLOAT:
        case PL_FORMAT_R16G16_UNORM:
        case PL_FORMAT_R16G16_UINT:
        case PL_FORMAT_R16G16_SNORM:
        case PL_FORMAT_R16G16_SINT:
        case PL_FORMAT_D32_FLOAT:
        case PL_FORMAT_R32_FLOAT:
        case PL_FORMAT_R32_UINT:
        case PL_FORMAT_R32_SINT:
        case PL_FORMAT_D24_UNORM_S8_UINT:
        case PL_FORMAT_B8G8R8A8_UNORM:
        case PL_FORMAT_B8G8R8A8_SRGB:
            return 32;

        case PL_FORMAT_R8G8_UNORM:
        case PL_FORMAT_R8G8_UINT:
        case PL_FORMAT_R8G8_SNORM:
        case PL_FORMAT_R8G8_SINT:
        case PL_FORMAT_R16_FLOAT:
        case PL_FORMAT_D16_UNORM:
        case PL_FORMAT_R16_UNORM:
        case PL_FORMAT_R16_UINT:
        case PL_FORMAT_R16_SNORM:
        case PL_FORMAT_R16_SINT:
        case PL_FORMAT_B5G6R5_UNORM:
        case PL_FORMAT_B5G5R5A1_UNORM:
            return 16;

        case PL_FORMAT_R8_UNORM:
        case PL_FORMAT_R8_UINT:
        case PL_FORMAT_R8_SNORM:
        case PL_FORMAT_R8_SINT:
            return 8;

        case PL_FORMAT_BC1_RGBA_UNORM:
        case PL_FORMAT_BC1_RGBA_SRGB:
        case PL_FORMAT_BC4_UNORM:
        case PL_FORMAT_BC4_SNORM:
            return 64;

        case PL_FORMAT_BC2_UNORM:
        case PL_FORMAT_BC2_SRGB:
        case PL_FORMAT_BC3_UNORM:
        case PL_FORMAT_BC3_SRGB:
        case PL_FORMAT_BC5_UNORM:
        case PL_FORMAT_BC5_SNORM:
        case PL_FORMAT_BC6H_UFLOAT:
        case PL_FORMAT_BC6H_FLOAT:
        case PL_FORMAT_BC7_UNORM:
        case PL_FORMAT_BC7_SRGB:
            return 128;
        default:
            return 0;
    }
}

static inline DXGI_FORMAT
pl__pl_to_dxgi_format(plFormat tFormat)
{
    switch(tFormat)
    {
        case PL_FORMAT_R32G32B32A32_FLOAT: return DXGI_FORMAT_R32G32B32A32_FLOAT;  
        case PL_FORMAT_R32G32B32A32_UINT:  return DXGI_FORMAT_R32G32B32A32_UINT;   
        case PL_FORMAT_R32G32B32A32_SINT:  return DXGI_FORMAT_R32G32B32A32_SINT;   
        case PL_FORMAT_R16G16B16A16_FLOAT: return DXGI_FORMAT_R16G16B16A16_FLOAT;  
        case PL_FORMAT_R16G16B16A16_UNORM: return DXGI_FORMAT_R16G16B16A16_UNORM;  
        case PL_FORMAT_R16G16B16A16_UINT:  return DXGI_FORMAT_R16G16B16A16_UINT;   
        case PL_FORMAT_R16G16B16A16_SNORM: return DXGI_FORMAT_R16G16B16A16_SNORM;  
        case PL_FORMAT_R16G16B16A16_SINT:  return DXGI_FORMAT_R16G16B16A16_SINT;   
        case PL_FORMAT_R32G32_FLOAT:       return DXGI_FORMAT_R32G32_FLOAT;        
        case PL_FORMAT_R32G32_UINT:        return DXGI_FORMAT_R32G32_UINT;         
        case PL_FORMAT_R32G32_SINT:        return DXGI_FORMAT_R32G32_SINT;         
        case PL_FORMAT_R10G10B10A2_UNORM:  return DXGI_FORMAT_R10G10B10A2_UNORM;   
        case PL_FORMAT_R10G10B10A2_UINT:   return DXGI_FORMAT_R10G10B10A2_UINT;    
        case PL_FORMAT_R11G11B10_FLOAT:    return DXGI_FORMAT_R11G11B10_FLOAT;     
        case PL_FORMAT_R8G8B8A8_UNORM:     return DXGI_FORMAT_R8G8B8A8_UNORM;      
        case PL_FORMAT_R8G8B8A8_UINT:      return DXGI_FORMAT_R8G8B8A8_UINT;       
        case PL_FORMAT_R8G8B8A8_SNORM:     return DXGI_FORMAT_R8G8B8A8_SNORM;      
        case PL_FORMAT_R8G8B8A8_SINT:      return DXGI_FORMAT_R8G8B8A8_SINT;       
        case PL_FORMAT_R16G16_FLOAT:       return DXGI_FORMAT_R16G16_FLOAT;        
        case PL_FORMAT_R16G16_UNORM:       return DXGI_FORMAT_R16G16_UNORM;        
        case PL_FORMAT_R16G16_UINT:        return DXGI_FORMAT_R16G16_UINT;         
        case PL_FORMAT_R16G16_SNORM:       return DXGI_FORMAT_R16G16_SNORM;        
        case PL_FORMAT_R16G16_SINT:        return DXGI_FORMAT_R16G16_SINT;         
        case PL_FORMAT_D32_FLOAT:          return DXGI_FORMAT_D32_FLOAT;           
        case PL_FORMAT_R32_FLOAT:          return DXGI_FORMAT_R32_FLOAT;           
        case PL_FORMAT_R32_UINT:           return DXGI_FORMAT_R32_UINT;            
        case PL_FORMAT_R32_SINT:           return DXGI_FORMAT_R32_SINT;            
        case PL_FORMAT_D24_UNORM_S8_UINT:  return DXGI_FORMAT_D24_UNORM_S8_UINT;   
        case PL_FORMAT_R8G8_UNORM:         return DXGI_FORMAT_R8G8_UNORM;          
        case PL_FORMAT_R8G8_UINT:          return DXGI_FORMAT_R8G8_UINT;           
        case PL_FORMAT_R8G8_SNORM:         return DXGI_FORMAT_R8G8_SNORM;          
        case PL_FORMAT_R8G8_SINT:          return DXGI_FORMAT_R8G8_SINT;           
        case PL_FORMAT_R16_FLOAT:          return DXGI_FORMAT_R16_FLOAT;           
        case PL_FORMAT_D16_UNORM:          return DXGI_FORMAT_D16_UNORM;           
        case PL_FORMAT_R16_UNORM:          return DXGI_FORMAT_R16_UNORM;           
        case PL_FORMAT_R16_UINT:           return DXGI_FORMAT_R16_UINT;            
        case PL_FORMAT_R16_SNORM:          return DXGI_FORMAT_R16_SNORM;           
        case PL_FORMAT_R16_SINT:           return DXGI_FORMAT_R16_SINT;            
        case PL_FORMAT_R8_UINT:            return DXGI_FORMAT_R8_UINT;             
        case PL_FORMAT_R8_SNORM:           return DXGI_FORMAT_R8_SNORM;            
        case PL_FORMAT_R8_SINT:            return DXGI_FORMAT_R8_SINT;             
        case PL_FORMAT_BC1_RGBA_UNORM:     return DXGI_FORMAT_BC1_UNORM;           
        case PL_FORMAT_BC1_RGBA_SRGB:      return DXGI_FORMAT_BC1_UNORM_SRGB;      
        case PL_FORMAT_BC2_SRGB:           return DXGI_FORMAT_BC2_UNORM_SRGB;      
        case PL_FORMAT_BC2_UNORM:          return DXGI_FORMAT_BC2_UNORM;           
        case PL_FORMAT_BC3_UNORM:          return DXGI_FORMAT_BC3_UNORM;           
        case PL_FORMAT_BC3_SRGB:           return DXGI_FORMAT_BC3_UNORM_SRGB;      
        case PL_FORMAT_BC4_UNORM:          return DXGI_FORMAT_BC4_UNORM;           
        case PL_FORMAT_BC4_SNORM:          return DXGI_FORMAT_BC4_SNORM;           
        case PL_FORMAT_BC5_UNORM:          return DXGI_FORMAT_BC5_UNORM;           
        case PL_FORMAT_BC5_SNORM:          return DXGI_FORMAT_BC5_SNORM;           
        case PL_FORMAT_B5G6R5_UNORM:       return DXGI_FORMAT_B5G6R5_UNORM;        
        case PL_FORMAT_B5G5R5A1_UNORM:     return DXGI_FORMAT_B5G5R5A1_UNORM;      
        case PL_FORMAT_B8G8R8A8_UNORM:     return DXGI_FORMAT_B8G8R8A8_UNORM;      
        case PL_FORMAT_B8G8R8A8_SRGB:      return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB; 
        case PL_FORMAT_BC6H_UFLOAT:        return DXGI_FORMAT_BC6H_UF16;           
        case PL_FORMAT_BC6H_FLOAT:         return DXGI_FORMAT_BC6H_SF16;           
        case PL_FORMAT_BC7_UNORM:          return DXGI_FORMAT_BC7_UNORM;           
        case PL_FORMAT_BC7_SRGB:           return DXGI_FORMAT_BC7_UNORM_SRGB;      
        case PL_FORMAT_R8G8B8A8_SRGB:      return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; 
        case PL_FORMAT_R8_UNORM:           return DXGI_FORMAT_R8_UNORM;
        default:
            PL_ASSERT(false);
            return DXGI_FORMAT_UNKNOWN;
    }
}

static inline plFormat
pl__dxgi_to_pl_format(DXGI_FORMAT tFormat)
{
    switch(tFormat)
    {
        case DXGI_FORMAT_R32G32B32A32_FLOAT:  return PL_FORMAT_R32G32B32A32_FLOAT;
        case DXGI_FORMAT_R32G32B32A32_UINT:   return PL_FORMAT_R32G32B32A32_UINT;
        case DXGI_FORMAT_R32G32B32A32_SINT:   return PL_FORMAT_R32G32B32A32_SINT;
        case DXGI_FORMAT_R16G16B16A16_FLOAT:  return PL_FORMAT_R16G16B16A16_FLOAT;
        case DXGI_FORMAT_R16G16B16A16_UNORM:  return PL_FORMAT_R16G16B16A16_UNORM;
        case DXGI_FORMAT_R16G16B16A16_UINT:   return PL_FORMAT_R16G16B16A16_UINT;
        case DXGI_FORMAT_R16G16B16A16_SNORM:  return PL_FORMAT_R16G16B16A16_SNORM;
        case DXGI_FORMAT_R16G16B16A16_SINT:   return PL_FORMAT_R16G16B16A16_SINT;
        case DXGI_FORMAT_R32G32_FLOAT:        return PL_FORMAT_R32G32_FLOAT;
        case DXGI_FORMAT_R32G32_UINT:         return PL_FORMAT_R32G32_UINT;
        case DXGI_FORMAT_R32G32_SINT:         return PL_FORMAT_R32G32_SINT;
        case DXGI_FORMAT_R10G10B10A2_UNORM:   return PL_FORMAT_R10G10B10A2_UNORM;
        case DXGI_FORMAT_R10G10B10A2_UINT:    return PL_FORMAT_R10G10B10A2_UINT;
        case DXGI_FORMAT_R11G11B10_FLOAT:     return PL_FORMAT_R11G11B10_FLOAT;
        case DXGI_FORMAT_R8G8B8A8_UNORM:      return PL_FORMAT_R8G8B8A8_UNORM;
        case DXGI_FORMAT_R8G8B8A8_UINT:       return PL_FORMAT_R8G8B8A8_UINT;
        case DXGI_FORMAT_R8G8B8A8_SNORM:      return PL_FORMAT_R8G8B8A8_SNORM;
        case DXGI_FORMAT_R8G8B8A8_SINT:       return PL_FORMAT_R8G8B8A8_SINT;
        case DXGI_FORMAT_R16G16_FLOAT:        return PL_FORMAT_R16G16_FLOAT;
        case DXGI_FORMAT_R16G16_UNORM:        return PL_FORMAT_R16G16_UNORM;
        case DXGI_FORMAT_R16G16_UINT:         return PL_FORMAT_R16G16_UINT;
        case DXGI_FORMAT_R16G16_SNORM:        return PL_FORMAT_R16G16_SNORM;
        case DXGI_FORMAT_R16G16_SINT:         return PL_FORMAT_R16G16_SINT;
        case DXGI_FORMAT_D32_FLOAT:           return PL_FORMAT_D32_FLOAT;
        case DXGI_FORMAT_R32_FLOAT:           return PL_FORMAT_R32_FLOAT;
        case DXGI_FORMAT_R32_UINT:            return PL_FORMAT_R32_UINT;
        case DXGI_FORMAT_R32_SINT:            return PL_FORMAT_R32_SINT;
        case DXGI_FORMAT_D24_UNORM_S8_UINT:   return PL_FORMAT_D24_UNORM_S8_UINT;
        case DXGI_FORMAT_R8G8_UNORM:          return PL_FORMAT_R8G8_UNORM;
        case DXGI_FORMAT_R8G8_UINT:           return PL_FORMAT_R8G8_UINT;
        case DXGI_FORMAT_R8G8_SNORM:          return PL_FORMAT_R8G8_SNORM;
        case DXGI_FORMAT_R8G8_SINT:           return PL_FORMAT_R8G8_SINT;
        case DXGI_FORMAT_R16_FLOAT:           return PL_FORMAT_R16_FLOAT;
        case DXGI_FORMAT_D16_UNORM:           return PL_FORMAT_D16_UNORM;
        case DXGI_FORMAT_R16_UNORM:           return PL_FORMAT_R16_UNORM;
        case DXGI_FORMAT_R16_UINT:            return PL_FORMAT_R16_UINT;
        case DXGI_FORMAT_R16_SNORM:           return PL_FORMAT_R16_SNORM;
        case DXGI_FORMAT_R16_SINT:            return PL_FORMAT_R16_SINT;
        case DXGI_FORMAT_R8_UINT:             return PL_FORMAT_R8_UINT;
        case DXGI_FORMAT_R8_SNORM:            return PL_FORMAT_R8_SNORM;
        case DXGI_FORMAT_R8_SINT:             return PL_FORMAT_R8_SINT;
        case DXGI_FORMAT_BC1_UNORM:           return PL_FORMAT_BC1_RGBA_UNORM;
        case DXGI_FORMAT_BC1_UNORM_SRGB:      return PL_FORMAT_BC1_RGBA_SRGB;
        case DXGI_FORMAT_BC2_UNORM_SRGB:      return PL_FORMAT_BC2_SRGB;
        case DXGI_FORMAT_BC2_UNORM:           return PL_FORMAT_BC2_UNORM;
        case DXGI_FORMAT_BC3_UNORM:           return PL_FORMAT_BC3_UNORM;
        case DXGI_FORMAT_BC3_UNORM_SRGB:      return PL_FORMAT_BC3_SRGB;
        case DXGI_FORMAT_BC4_UNORM:           return PL_FORMAT_BC4_UNORM;
        case DXGI_FORMAT_BC4_SNORM:           return PL_FORMAT_BC4_SNORM;
        case DXGI_FORMAT_BC5_UNORM:           return PL_FORMAT_BC5_UNORM;
        case DXGI_FORMAT_BC5_SNORM:           return PL_FORMAT_BC5_SNORM;
        case DXGI_FORMAT_B5G6R5_UNORM:        return PL_FORMAT_B5G6R5_UNORM;
        case DXGI_FORMAT_B5G5R5A1_UNORM:      return PL_FORMAT_B5G5R5A1_UNORM;
        case DXGI_FORMAT_B8G8R8A8_UNORM:      return PL_FORMAT_B8G8R8A8_UNORM;
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return PL_FORMAT_B8G8R8A8_SRGB;
        case DXGI_FORMAT_BC6H_UF16:           return PL_FORMAT_BC6H_UFLOAT;
        case DXGI_FORMAT_BC6H_SF16:           return PL_FORMAT_BC6H_FLOAT;
        case DXGI_FORMAT_BC7_UNORM:           return PL_FORMAT_BC7_UNORM;
        case DXGI_FORMAT_BC7_UNORM_SRGB:      return PL_FORMAT_BC7_SRGB;
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return PL_FORMAT_R8G8B8A8_SRGB;

        case DXGI_FORMAT_A8_UNORM:
        case DXGI_FORMAT_R8_UNORM: return PL_FORMAT_R8_UNORM;

        case DXGI_FORMAT_UNKNOWN:
        case DXGI_FORMAT_BC5_TYPELESS:
        case DXGI_FORMAT_BC2_TYPELESS:
        case DXGI_FORMAT_BC3_TYPELESS:
        case DXGI_FORMAT_BC4_TYPELESS:
        case DXGI_FORMAT_R24G8_TYPELESS:
        case DXGI_FORMAT_R32_TYPELESS:
        case DXGI_FORMAT_R16G16_TYPELESS:
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        case DXGI_FORMAT_R32G32B32A32_TYPELESS:
        case DXGI_FORMAT_R32G32B32_TYPELESS:
        case DXGI_FORMAT_R32G32B32_FLOAT:
        case DXGI_FORMAT_R32G32B32_UINT:
        case DXGI_FORMAT_R32G32B32_SINT:
        case DXGI_FORMAT_R16G16B16A16_TYPELESS:
        case DXGI_FORMAT_R32G32_TYPELESS:
        case DXGI_FORMAT_R32G8X24_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
        case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        case DXGI_FORMAT_R10G10B10A2_TYPELESS:
        case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
        case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        case DXGI_FORMAT_R8G8_TYPELESS:
        case DXGI_FORMAT_R16_TYPELESS:
        case DXGI_FORMAT_R8_TYPELESS:
        case DXGI_FORMAT_R1_UNORM:
        case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
        case DXGI_FORMAT_R8G8_B8G8_UNORM:
        case DXGI_FORMAT_G8R8_G8B8_UNORM:
        case DXGI_FORMAT_BC1_TYPELESS:
        case DXGI_FORMAT_B8G8R8X8_UNORM:
        case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
        case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        case DXGI_FORMAT_B8G8R8X8_TYPELESS:
        case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
        case DXGI_FORMAT_BC6H_TYPELESS:
        case DXGI_FORMAT_BC7_TYPELESS:
        case DXGI_FORMAT_AYUV:
        case DXGI_FORMAT_Y410:
        case DXGI_FORMAT_Y416:
        case DXGI_FORMAT_NV12:
        case DXGI_FORMAT_P010:
        case DXGI_FORMAT_P016:
        case DXGI_FORMAT_420_OPAQUE:
        case DXGI_FORMAT_YUY2:
        case DXGI_FORMAT_Y210:
        case DXGI_FORMAT_Y216:
        case DXGI_FORMAT_NV11:
        case DXGI_FORMAT_AI44:
        case DXGI_FORMAT_IA44:
        case DXGI_FORMAT_P8:
        case DXGI_FORMAT_A8P8:
        case DXGI_FORMAT_B4G4R4A4_UNORM:
        case DXGI_FORMAT_P208:
        case DXGI_FORMAT_V208:
        case DXGI_FORMAT_V408:
        case DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE:
        case DXGI_FORMAT_SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE:
        case D3DFMT_R8G8B8:
        default:
            PL_ASSERT(false);
            return PL_FORMAT_UNKNOWN;
    }
}

//-----------------------------------------------------------------------------
// [SECTION] public implementation
//-----------------------------------------------------------------------------

bool
pl_dds_read_info(uint8_t* puBuffer, plDdsReadInfo* ptInfoOut)
{
    
    plDdsHeader* ptHeader = (plDdsHeader*)puBuffer;

    // check validity
    const bool bValid = ptHeader->uMagic == pl__dds_fourcc('D', 'D', 'S', ' ') && ptHeader->tHeader.ddspf.uSize == sizeof(plDssPixelFormat);
    if(!bValid)
        return false;

    const bool bDx10 = (ptHeader->tHeader.ddspf.uFlags & DDPF_FOURCC) && ptHeader->tHeader.ddspf.uFourCC == pl__dds_fourcc('D', 'X', '1', '0');

    ptInfoOut->uWidth = ptHeader->tHeader.uWidth < 1 ? 1 : ptHeader->tHeader.uWidth;
    ptInfoOut->uHeight = ptHeader->tHeader.uHeight < 1 ? 1 : ptHeader->tHeader.uHeight;
    ptInfoOut->uDepth = ptHeader->tHeader.uDepth < 1 ? 1 : ptHeader->tHeader.uDepth;
    ptInfoOut->uMips = ptHeader->tHeader.uMips > 0 ? ptHeader->tHeader.uMips : 1;

    // check if cubemap
    bool bCubeMap = false;
    if(bDx10)
        bCubeMap = ptHeader->tHeader10.uMiscFlag & DDS_RESOURCE_MISC_TEXTURECUBE;
    else
    {
        bCubeMap = 
            (ptHeader->tHeader.uCaps2 & DDSCAPS2_CUBEMAP) &&
            (ptHeader->tHeader.uCaps2 & DDSCAPS2_CUBEMAP_POSITIVEX) &&
            (ptHeader->tHeader.uCaps2 & DDSCAPS2_CUBEMAP_NEGATIVEX) &&
            (ptHeader->tHeader.uCaps2 & DDSCAPS2_CUBEMAP_POSITIVEY) &&
            (ptHeader->tHeader.uCaps2 & DDSCAPS2_CUBEMAP_NEGATIVEY) &&
            (ptHeader->tHeader.uCaps2 & DDSCAPS2_CUBEMAP_POSITIVEZ) &&
            (ptHeader->tHeader.uCaps2 & DDSCAPS2_CUBEMAP_NEGATIVEZ);
    }

    // get array size
    if(!bDx10)
    {
        if(bCubeMap)
            ptInfoOut->uLayers = 6;
        else
            ptInfoOut->uLayers = 1;
    }
    else
    {
        uint32_t uCount = 0;
        if (bCubeMap)
            uCount = ptHeader->tHeader10.uLayers * 6;
        else
            uCount = ptHeader->tHeader10.uLayers;
        uCount = uCount < 1 ? 1 : uCount;
        ptInfoOut->uLayers = uCount;
    }

    // check 1D
    bool b1D = false;
    if(!bDx10)
        b1D = false;
    else
        b1D = ptHeader->tHeader10.tResourceDimension == D3D10_RESOURCE_DIMENSION_TEXTURE1D;

    // check 3d
    bool b3D = false;
    if(!bDx10)
        b3D = false;
    else
        b3D = (ptHeader->tHeader.uCaps2 & DDSCAPS2_VOLUME) && ptHeader->tHeader10.tResourceDimension == D3D10_RESOURCE_DIMENSION_TEXTURE3D;

    ptInfoOut->uOffset = sizeof(plDdsHeader);
    if(!bDx10)
        ptInfoOut->uOffset -= sizeof(plDdsHeaderDxt10);

    // figure out format
    ptInfoOut->tFormat = PL_FORMAT_UNKNOWN;
    if(!bDx10)
    {
        if(ptHeader->tHeader.ddspf.uFlags & DDPF_RGB)
        {
            switch(ptHeader->tHeader.ddspf.uRGBBitCount)
            {
                case 32:
                {
                    if (ptHeader->tHeader.ddspf.uRBitMask == 0x000000ff &&
                        ptHeader->tHeader.ddspf.uGBitMask == 0x0000ff00 &&
                        ptHeader->tHeader.ddspf.uBBitMask == 0x00ff0000 &&
                        ptHeader->tHeader.ddspf.uABitMask == 0xff000000)
                        ptInfoOut->tFormat = PL_FORMAT_R8G8B8A8_UNORM;
                    else if (ptHeader->tHeader.ddspf.uRBitMask == 0x00ff0000 &&
                        ptHeader->tHeader.ddspf.uGBitMask == 0x0000ff00 &&
                        ptHeader->tHeader.ddspf.uBBitMask == 0x000000ff &&
                        ptHeader->tHeader.ddspf.uABitMask == 0xff000000)
                        ptInfoOut->tFormat = PL_FORMAT_B8G8R8A8_UNORM;
                    else if (ptHeader->tHeader.ddspf.uRBitMask == 0x00ff0000 &&
                        ptHeader->tHeader.ddspf.uGBitMask == 0x0000ff00 &&
                        ptHeader->tHeader.ddspf.uBBitMask == 0x000000ff &&
                        ptHeader->tHeader.ddspf.uABitMask == 0x00000000)
                    {
                        PL_ASSERT(false);
                        // ptInfoOut->tFormat = DXGI_FORMAT_B8G8R8X8_UNORM;
                        ptInfoOut->tFormat = PL_FORMAT_UNKNOWN;
                    }

                    else if (ptHeader->tHeader.ddspf.uRBitMask == 0x0000ffff &&
                        ptHeader->tHeader.ddspf.uGBitMask == 0xffff0000 &&
                        ptHeader->tHeader.ddspf.uBBitMask == 0x00000000 &&
                        ptHeader->tHeader.ddspf.uABitMask == 0x00000000)
                        ptInfoOut->tFormat = PL_FORMAT_R16G16_UNORM;

                    else if (ptHeader->tHeader.ddspf.uRBitMask == 0xffffffff &&
                        ptHeader->tHeader.ddspf.uGBitMask == 0x00000000 &&
                        ptHeader->tHeader.ddspf.uBBitMask == 0x00000000 &&
                        ptHeader->tHeader.ddspf.uABitMask == 0x00000000)
                        ptInfoOut->tFormat = PL_FORMAT_R32_FLOAT;
                    break;
                }

                case 24:
                {
                    ptInfoOut->tFormat = PL_FORMAT_UNKNOWN;
                    PL_ASSERT(false);
                    break;
                }

                case 16:
                {
                    if (ptHeader->tHeader.ddspf.uRBitMask == 0x7c00 && ptHeader->tHeader.ddspf.uGBitMask == 0x03e0 &&
                        ptHeader->tHeader.ddspf.uBBitMask == 0x001f && ptHeader->tHeader.ddspf.uABitMask == 0x8000)
                        ptInfoOut->tFormat = PL_FORMAT_B5G5R5A1_UNORM;
                    else if (ptHeader->tHeader.ddspf.uRBitMask == 0xf800 && ptHeader->tHeader.ddspf.uGBitMask == 0x07e0 &&
                        ptHeader->tHeader.ddspf.uBBitMask == 0x001f && ptHeader->tHeader.ddspf.uABitMask == 0x0000)
                        ptInfoOut->tFormat = PL_FORMAT_B5G6R5_UNORM;

                    else if (ptHeader->tHeader.ddspf.uRBitMask == 0x0f00 && ptHeader->tHeader.ddspf.uGBitMask == 0x00f0 &&
                        ptHeader->tHeader.ddspf.uBBitMask == 0x000f && ptHeader->tHeader.ddspf.uABitMask == 0xf000)
                    {
                        // ptInfoOut->tFormat = DXGI_FORMAT_B4G4R4A4_UNORM;
                        ptInfoOut->tFormat = PL_FORMAT_UNKNOWN;
                        PL_ASSERT(false);
                    }
                    break;
                }

                default:
                    break;
            }
        }
        else if (ptHeader->tHeader.ddspf.uFlags & DDPF_LUMINANCE)
        {
            if (8 == ptHeader->tHeader.ddspf.uRGBBitCount)
            {
                if (ptHeader->tHeader.ddspf.uRBitMask == 0x000000ff && ptHeader->tHeader.ddspf.uGBitMask == 0x00000000 &&
                    ptHeader->tHeader.ddspf.uBBitMask == 0x00000000 && ptHeader->tHeader.ddspf.uABitMask == 0x00000000)
                    ptInfoOut->tFormat = PL_FORMAT_R8_UNORM;
                else if (ptHeader->tHeader.ddspf.uRBitMask == 0x000000ff && ptHeader->tHeader.ddspf.uGBitMask == 0x0000ff00 &&
                    ptHeader->tHeader.ddspf.uBBitMask == 0x00000000 && ptHeader->tHeader.ddspf.uABitMask == 0x00000000)
                    ptInfoOut->tFormat = PL_FORMAT_R8G8_UNORM;
            }
            else if (16 == ptHeader->tHeader.ddspf.uRGBBitCount)
            {
                if (ptHeader->tHeader.ddspf.uRBitMask == 0x0000ffff && ptHeader->tHeader.ddspf.uGBitMask == 0x00000000 &&
                    ptHeader->tHeader.ddspf.uBBitMask == 0x00000000 && ptHeader->tHeader.ddspf.uABitMask == 0x00000000)
                    ptInfoOut->tFormat = PL_FORMAT_R16_UNORM;

                else if (ptHeader->tHeader.ddspf.uRBitMask == 0x000000ff && ptHeader->tHeader.ddspf.uGBitMask == 0x0000ff00 &&
                    ptHeader->tHeader.ddspf.uBBitMask == 0x00000000 && ptHeader->tHeader.ddspf.uABitMask == 0x00000000)
                    ptInfoOut->tFormat = PL_FORMAT_R8G8_UNORM;
            }
        }
        else if (ptHeader->tHeader.ddspf.uFlags & DDPF_ALPHA)
        {
            if (ptHeader->tHeader.ddspf.uRGBBitCount == 8)
                ptInfoOut->tFormat = PL_FORMAT_R8_UNORM;
        }
        else if (ptHeader->tHeader.ddspf.uFlags & DDPF_FOURCC)
        {
            if (pl__dds_fourcc('D', 'X', 'T', '1') == ptHeader->tHeader.ddspf.uFourCC)
                ptInfoOut->tFormat = PL_FORMAT_BC1_RGBA_UNORM;
            else if (pl__dds_fourcc('D', 'X', 'T', '3') == ptHeader->tHeader.ddspf.uFourCC)
                ptInfoOut->tFormat = PL_FORMAT_BC2_UNORM;
            else if (pl__dds_fourcc('D', 'X', 'T', '5') == ptHeader->tHeader.ddspf.uFourCC)
                ptInfoOut->tFormat = PL_FORMAT_BC3_UNORM;
            else if (pl__dds_fourcc('D', 'X', 'T', '4') == ptHeader->tHeader.ddspf.uFourCC)
                ptInfoOut->tFormat = PL_FORMAT_BC2_UNORM;
            else if (pl__dds_fourcc('A', 'T', 'I', '1') == ptHeader->tHeader.ddspf.uFourCC)
                ptInfoOut->tFormat = PL_FORMAT_BC4_UNORM;
            else if (pl__dds_fourcc('B', 'C', '4', 'U') == ptHeader->tHeader.ddspf.uFourCC)
                ptInfoOut->tFormat = PL_FORMAT_BC4_UNORM;
            else if (pl__dds_fourcc('B', 'C', '4', 'S') == ptHeader->tHeader.ddspf.uFourCC)
                ptInfoOut->tFormat = PL_FORMAT_BC4_SNORM;
            else if (pl__dds_fourcc('A', 'T', 'I', '2') == ptHeader->tHeader.ddspf.uFourCC)
                ptInfoOut->tFormat = PL_FORMAT_BC5_UNORM;
            else if (pl__dds_fourcc('B', 'C', '5', 'U') == ptHeader->tHeader.ddspf.uFourCC)
                ptInfoOut->tFormat = PL_FORMAT_BC5_UNORM;
            else if (pl__dds_fourcc('B', 'C', '5', 'S') == ptHeader->tHeader.ddspf.uFourCC)
                ptInfoOut->tFormat = PL_FORMAT_BC5_SNORM;
            else if (pl__dds_fourcc('R', 'G', 'B', 'G') == ptHeader->tHeader.ddspf.uFourCC)
            {
                // ptInfoOut->tFormat = GI_FORMAT_R8G8_B8G8_UNORM;
                ptInfoOut->tFormat = PL_FORMAT_UNKNOWN;
                PL_ASSERT(false);
            }
            else if (pl__dds_fourcc('G', 'R', 'G', 'B') == ptHeader->tHeader.ddspf.uFourCC)
            {
                // ptInfoOut->tFormat = GI_FORMAT_G8R8_G8B8_UNORM;
                ptInfoOut->tFormat = PL_FORMAT_UNKNOWN;
                PL_ASSERT(false);
            }

            else if (pl__dds_fourcc('Y', 'U', 'Y', '2') == ptHeader->tHeader.ddspf.uFourCC)
            {
                // ptInfoOut->tFormat = GI_FORMAT_YUY2;
                ptInfoOut->tFormat = PL_FORMAT_UNKNOWN;
                PL_ASSERT(false);
            }
            else
            {
                switch (ptHeader->tHeader.ddspf.uFourCC)
                {
                    case 36:  ptInfoOut->tFormat = PL_FORMAT_R16G16B16A16_UNORM; break;
                    case 110: ptInfoOut->tFormat = PL_FORMAT_R16G16B16A16_SNORM; break;
                    case 111: ptInfoOut->tFormat = PL_FORMAT_R16_FLOAT; break;
                    case 112: ptInfoOut->tFormat = PL_FORMAT_R16G16_FLOAT; break;
                    case 113: ptInfoOut->tFormat = PL_FORMAT_R16G16B16A16_FLOAT; break;
                    case 114: ptInfoOut->tFormat = PL_FORMAT_R32_FLOAT; break;
                    case 115: ptInfoOut->tFormat = PL_FORMAT_R32G32_FLOAT; break;
                    case 116: ptInfoOut->tFormat = PL_FORMAT_R32G32B32A32_FLOAT; break;
                }
            }
        }
    }
    else
        ptInfoOut->tFormat = pl__dxgi_to_pl_format(ptHeader->tHeader10.tDxgiFormat);

    if(bCubeMap)
        ptInfoOut->tType = PL_TEXTURE_TYPE_CUBE;
    else if(ptInfoOut->uLayers > 1)
        ptInfoOut->tType = PL_TEXTURE_TYPE_2D_ARRAY;
    else
        ptInfoOut->tType = PL_TEXTURE_TYPE_2D;

    // not supported yet
    PL_ASSERT(b1D == false);
    PL_ASSERT(b3D == false);

    const uint32_t uBlockSize = pl__dds_block_size(ptInfoOut->tFormat);
    const uint32_t uBitsPerElement = pl__dds_bits_per_element(ptInfoOut->tFormat);

    if(uBlockSize > 1)
    {
        for(uint32_t i = 0; i < ptInfoOut->uMips; i++)
        {
            uint32_t uNumElementsX = ptInfoOut->uWidth;
            uint32_t uNumElementsY = ptInfoOut->uHeight;
            uint32_t uNumElementsZ = ptInfoOut->uDepth;

            uNumElementsX >>= i;
            uNumElementsY >>= i;
            uNumElementsZ >>= i;
            uNumElementsX = uNumElementsX < 1 ? 1 : uNumElementsX;
            uNumElementsY = uNumElementsY < 1 ? 1 : uNumElementsY;
            uNumElementsZ = uNumElementsZ < 1 ? 1 : uNumElementsZ;

            ptInfoOut->atMipInfo[i].uWidth = uNumElementsX;
            ptInfoOut->atMipInfo[i].uHeight = uNumElementsY;

            uint32_t uNumBlocksX = (uNumElementsX + uBlockSize - 1) / uBlockSize;
            uint32_t uNumBlocksY = (uNumElementsY + uBlockSize - 1) / uBlockSize;
            ptInfoOut->atMipInfo[i].uSize = uNumBlocksX * uNumBlocksY * uNumElementsZ *  uBitsPerElement / 8;
            ptInfoOut->atMipInfo[i].uRowPitch = ((ptInfoOut->atMipInfo[i].uWidth + 3) / 4 ) * uBlockSize;
            ptInfoOut->uSize += ptInfoOut->atMipInfo[i].uSize;
        }
    }
    else
    {
        for(uint32_t i = 0; i < ptInfoOut->uMips; i++)
        {
            uint32_t uNumElementsX = ptInfoOut->uWidth;
            uint32_t uNumElementsY = ptInfoOut->uHeight;
            uint32_t uNumElementsZ = ptInfoOut->uDepth;

            uNumElementsX >>= i;
            uNumElementsY >>= i;
            uNumElementsZ >>= i;
            uNumElementsX = uNumElementsX < 1 ? 1 : uNumElementsX;
            uNumElementsY = uNumElementsY < 1 ? 1 : uNumElementsY;
            uNumElementsZ = uNumElementsZ < 1 ? 1 : uNumElementsZ;

            ptInfoOut->atMipInfo[i].uWidth = uNumElementsX;
            ptInfoOut->atMipInfo[i].uHeight = uNumElementsY;
            uint32_t uNumBlocksX = (uNumElementsX + uBlockSize - 1) / uBlockSize;
            uint32_t uNumBlocksY = (uNumElementsY + uBlockSize - 1) / uBlockSize;
            ptInfoOut->atMipInfo[i].uSize = uNumBlocksX * uNumBlocksY * uNumElementsZ *  uBitsPerElement / 8;
            ptInfoOut->uSize += ptInfoOut->atMipInfo[i].uSize;
            ptInfoOut->atMipInfo[i].uRowPitch = (ptInfoOut->atMipInfo[i].uWidth * uBitsPerElement + 7) / 8;
        }
    }

    ptInfoOut->uSize *= ptInfoOut->uLayers;

    ptInfoOut->atMipInfo[0].uOffset = 0;
    for(uint32_t i = 1; i < ptInfoOut->uMips; i++)
        ptInfoOut->atMipInfo[i].uOffset = ptInfoOut->atMipInfo[i - 1].uOffset + ptInfoOut->atMipInfo[i - 1].uSize;

    return ptInfoOut->tFormat != PL_FORMAT_UNKNOWN;
}

void
pl_dds_write_info(uint8_t* puBuffer, const plDdsWriteInfo* ptInfo)
{
    plDdsHeader tHeader = {0};

    tHeader.uMagic = pl__dds_fourcc('D', 'D', 'S', ' ');
    tHeader.tHeader.uSize = sizeof(plDdsHeader);
    tHeader.tHeader.uFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT | DDSD_MIPMAPCOUNT;

    tHeader.tHeader.uWidth = ptInfo->uWidth;
    tHeader.tHeader.uHeight = ptInfo->uHeight;
    tHeader.tHeader.uDepth = ptInfo->uDepth;
    tHeader.tHeader.uMips = ptInfo->uMips;
    tHeader.tHeader.ddspf.uSize = sizeof(plDssPixelFormat);
    tHeader.tHeader.ddspf.uFlags = DDPF_FOURCC;
    tHeader.tHeader.ddspf.uFourCC = pl__dds_fourcc('D', 'X', '1', '0');
    tHeader.tHeader.uCaps = DDSCAPS_TEXTURE;

    tHeader.tHeader10.tDxgiFormat = pl__pl_to_dxgi_format(ptInfo->tFormat);
    tHeader.tHeader10.tResourceDimension = D3D10_RESOURCE_DIMENSION_TEXTURE2D;
    tHeader.tHeader10.uMiscFlags2 = DDS_ALPHA_MODE_UNKNOWN;

    if (ptInfo->tType == PL_TEXTURE_TYPE_CUBE)
    {
        tHeader.tHeader10.uLayers = ptInfo->uLayers / 6;
        tHeader.tHeader.uCaps |= DDSCAPS_COMPLEX;
        tHeader.tHeader.uCaps2 =
            DDSCAPS2_CUBEMAP |
            DDSCAPS2_CUBEMAP_POSITIVEX |
            DDSCAPS2_CUBEMAP_NEGATIVEX |
            DDSCAPS2_CUBEMAP_POSITIVEY |
            DDSCAPS2_CUBEMAP_NEGATIVEY |
            DDSCAPS2_CUBEMAP_POSITIVEZ |
            DDSCAPS2_CUBEMAP_NEGATIVEZ
            ;
        tHeader.tHeader10.uMiscFlag = DDS_RESOURCE_MISC_TEXTURECUBE;
    }
    else if (ptInfo->uDepth > 0)
    {
        tHeader.tHeader10.uLayers = 1;
        tHeader.tHeader10.tResourceDimension = D3D10_RESOURCE_DIMENSION_TEXTURE3D;
        tHeader.tHeader.uCaps2 = DDSCAPS2_VOLUME;
    }
    else
        tHeader.tHeader10.uLayers = ptInfo->uLayers;

    if (ptInfo->uHeight == 0)
    {
        tHeader.tHeader10.tResourceDimension = D3D10_RESOURCE_DIMENSION_TEXTURE1D;
        tHeader.tHeader.uHeight = 1;
    }
    if (ptInfo->uMips > 1)
        tHeader.tHeader.uCaps |= DDSCAPS_COMPLEX;

    memcpy(puBuffer, &tHeader, sizeof(plDdsHeader));
}

uint32_t
pl_dds_get_header_size(void)
{
    return sizeof(plDdsHeader);
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_dds_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plDdsI tApi = {
        .get_header_size = pl_dds_get_header_size,
        .read_info       = pl_dds_read_info,
        .write_info      = pl_dds_write_info,
    };
    pl_set_api(ptApiRegistry, plDdsI, &tApi);
}

PL_EXPORT void
pl_unload_dds_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;
        
    const plDdsI* ptApi = pl_get_api_latest(ptApiRegistry, plDdsI);
    ptApiRegistry->remove_api(ptApi);
}
