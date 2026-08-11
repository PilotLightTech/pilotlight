#ifndef PL_GRAPHICS_EXT_CPU_H
#define PL_GRAPHICS_EXT_CPU_H

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

// enums/flags
typedef int plVaryingType;

// function pointers
typedef plVec4 (*plPixelShader) (plPixelShaderBuiltIns, const plVaryingData*);
typedef plVec2 (*plVertexShader)(plVertexShaderBuiltIns, const void*, plVaryingData*);

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

//-----------------------------------------------------------------------------
// [SECTION] shader api
//-----------------------------------------------------------------------------

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

//-----------------------------------------------------------------------------
// [SECTION] internal
//-----------------------------------------------------------------------------

#endif // PL_GRAPHICS_EXT_CPU_H