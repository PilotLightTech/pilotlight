#ifndef PL_GRAPHICS_EXT_CPU_H
#define PL_GRAPHICS_EXT_CPU_H

#include <assert.h>
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"
// #include "pl_graphics_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plFrameBufferData plFrameBufferData;

// cpu shader types
typedef struct _plVaryingData          plVaryingData;
typedef struct _plPixelShaderBuiltIns  plPixelShaderBuiltIns;
typedef struct _plVertexShaderBuiltIns plVertexShaderBuiltIns;
typedef struct _plDescriptorSet        plDescriptorSet;
typedef struct _plDescriptor           plDescriptor;

// enums/flags
typedef int plVaryingType;
typedef int plDescriptorType;

// function pointers
typedef plVec4 (*plPixelShader) (plPixelShaderBuiltIns, plDescriptorSet*, const plVaryingData*);
typedef plVec2 (*plVertexShader)(plVertexShaderBuiltIns, plDescriptorSet*, const void*, plVaryingData*);

typedef struct _plVertexBufferLayout     plVertexBufferLayout;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plVaryingData
{
    plVaryingType atTypes[16];
    char          acVaryingData[512];

    // internal
    uint32_t _uCurrentVarying;
    uint32_t _uCurrentOffset;
    uint32_t _auOffset[16];
} plVaryingData;

typedef struct _plFrameBufferData
{
    uint32_t       uWidth;
    uint32_t       uHeight;
    unsigned char* pucData;
} plFrameBufferData;

typedef struct _plPixelShaderBuiltIns
{
    plVec4 gl_FragCoord;
} plPixelShaderBuiltIns;

typedef struct _plVertexShaderBuiltIns
{
    uint32_t                    uVertexID;
    const plVertexBufferLayout* atLayouts;
} plVertexShaderBuiltIns;

typedef struct _plDescriptor
{
    plDescriptorType eType;

    // texture
    uint32_t uWidth;
    uint32_t uHeight;
    uint32_t uComponents;

    // buffer
    uint8_t* puData;
} plDescriptor;

typedef struct _plDescriptorSet
{
    plDescriptor* atDescriptors;
} plDescriptorSet;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plVaryingType
{
    PL_VARYING_TYPE_NONE = 0,
    PL_VARYING_TYPE_VEC2,
    PL_VARYING_TYPE_VEC3,
    PL_VARYING_TYPE_VEC4,
    PL_VARYING_TYPE_FLOAT,
    AY_MAX_VARYINGS
};

enum _plDescriptorType
{
    PL_DESCRIPTOR_TYPE_NONE = 0,
    PL_DESCRIPTOR_TYPE_TEXTURE,
    PL_DESCRIPTOR_TYPE_SAMPLER,
    PL_DESCRIPTOR_TYPE_BUFFER,
};

//-----------------------------------------------------------------------------
// [SECTION] shader api
//-----------------------------------------------------------------------------

#ifdef PL_SHADER_CODE

static inline void*
pl_shading_set_varying(plVaryingType tType, plVaryingData* ptVaryingDataOut)
{
    void* ptResult = (void*)&ptVaryingDataOut->acVaryingData[ptVaryingDataOut->_uCurrentOffset];
    ptVaryingDataOut->_auOffset[ptVaryingDataOut->_uCurrentVarying] = ptVaryingDataOut->_uCurrentOffset;

    if(tType == PL_VARYING_TYPE_FLOAT)     ptVaryingDataOut->_uCurrentOffset += sizeof(float);
    else if(tType == PL_VARYING_TYPE_VEC2) ptVaryingDataOut->_uCurrentOffset += sizeof(float) * 2;
    else if(tType == PL_VARYING_TYPE_VEC3) ptVaryingDataOut->_uCurrentOffset += sizeof(float) * 3;
    else if(tType == PL_VARYING_TYPE_VEC4) ptVaryingDataOut->_uCurrentOffset += sizeof(float) * 4;

    ptVaryingDataOut->atTypes[ptVaryingDataOut->_uCurrentVarying] = tType;
    ptVaryingDataOut->_uCurrentVarying++;

    return ptResult;
}

static inline const void*
pl_shading_get_varying(uint32_t uVaryingIndex, const plVaryingData* ptVaryingDataOut)
{
    uint32_t uOffset = ptVaryingDataOut->_auOffset[uVaryingIndex];
    return (void*)&ptVaryingDataOut->acVaryingData[uOffset];
}

static inline const void*
pl_shading_get_vertex_attrib(const void* pcVertexDataIn, const plVertexBufferLayout* atLayout, uint32_t uBufferIndex, uint32_t tAttribLocation)
{
    const void* ptResult = pcVertexDataIn;
    uint32_t uOffset = (uint32_t)atLayout[uBufferIndex].atAttributes[tAttribLocation].uByteOffset;
    ptResult = (char*)ptResult + uOffset;
    return (void*)ptResult;
};

static inline plVec4
pl_sample_texture(plDescriptor* ptTexture, plVec2 tUV)
{
    int iWidth = (int)ptTexture->uWidth;
    int iHeight = (int)ptTexture->uHeight;

    // convert UV to pixel coords & clamp to texture bounds 
    int iPixelX = (int)floorf(tUV.x * iWidth);
    int iPixelY = (int)floorf(tUV.y * iHeight);
    iPixelX = iPixelX < 0 ? 0 : (iPixelX >= iWidth ? iWidth - 1 : iPixelX);
    iPixelY = iPixelY < 0 ? 0 : (iPixelY >= iHeight ? iHeight - 1 : iPixelY);

    assert(ptTexture->uComponents == 4);
    
    // compute offset
    int iPixelStart = (iPixelY * iWidth + iPixelX) * ptTexture->uComponents;

    return {
        (float)ptTexture->puData[iPixelStart] / 255.0f, 
        (float)ptTexture->puData[iPixelStart + 1] / 255.0f, 
        (float)ptTexture->puData[iPixelStart + 2] / 255.0f,
        (float)ptTexture->puData[iPixelStart + 3] / 255.0f};
}

static inline plVec4
pl_sample_texture_bilinear(plDescriptor* ptTexture, plVec2 uv)
{
    const int w = (int)ptTexture->uWidth;
    const int h = (int)ptTexture->uHeight;

    float x = uv.x * (float)w - 0.5f;
    float y = uv.y * (float)h - 0.5f;

    int x0 = (int)floorf(x);
    int y0 = (int)floorf(y);
    int x1 = x0 + 1;
    int y1 = y0 + 1;

    float fx = x - floorf(x);
    float fy = y - floorf(y);

    x0 = pl_clampi(0, x0, w - 1);
    y0 = pl_clampi(0, y0, h - 1);
    x1 = pl_clampi(0, x1, w - 1);
    y1 = pl_clampi(0, y1, h - 1);

    const uint8_t* p = ptTexture->puData;

    int i00 = (y0 * w + x0) * 4;
    int i10 = (y0 * w + x1) * 4;
    int i01 = (y1 * w + x0) * 4;
    int i11 = (y1 * w + x1) * 4;

    plVec4 c00 = { (float)p[i00+0], (float)p[i00+1], (float)p[i00+2], (float)p[i00+3] };
    plVec4 c10 = { (float)p[i10+0], (float)p[i10+1], (float)p[i10+2], (float)p[i10+3] };
    plVec4 c01 = { (float)p[i01+0], (float)p[i01+1], (float)p[i01+2], (float)p[i01+3] };
    plVec4 c11 = { (float)p[i11+0], (float)p[i11+1], (float)p[i11+2], (float)p[i11+3] };

    plVec4 cx0 = {
        c00.x + (c10.x - c00.x) * fx,
        c00.y + (c10.y - c00.y) * fx,
        c00.z + (c10.z - c00.z) * fx,
        c00.w + (c10.w - c00.w) * fx
    };

    plVec4 cx1 = {
        c01.x + (c11.x - c01.x) * fx,
        c01.y + (c11.y - c01.y) * fx,
        c01.z + (c11.z - c01.z) * fx,
        c01.w + (c11.w - c01.w) * fx
    };

    return {
        (cx0.x + (cx1.x - cx0.x) * fy) / 255.0f,
        (cx0.y + (cx1.y - cx0.y) * fy) / 255.0f,
        (cx0.z + (cx1.z - cx0.z) * fy) / 255.0f,
        (cx0.w + (cx1.w - cx0.w) * fy) / 255.0f
    };
}

#endif

#endif // PL_GRAPHICS_EXT_CPU_H