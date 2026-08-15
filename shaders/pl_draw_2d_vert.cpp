#include "pl.inc"
#include  <cstddef>
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"
#include "pl_graphics_ext.h"
#include "pl_shader_interop_cpu.h"

void unpackUnorm4x8(uint32_t packed, float out[4]) {
    // Extract each byte and normalize to [0.0, 1.0]
    out[0] = ((packed >> 0)  & 0xFF) / 255.0f;  // R
    out[1] = ((packed >> 8)  & 0xFF) / 255.0f;  // G
    out[2] = ((packed >> 16) & 0xFF) / 255.0f;  // B
    out[3] = ((packed >> 24) & 0xFF) / 255.0f;  // A
}

struct PL_DYNAMIC_DATA{
    plVec2 uScale;
    plVec2 uTranslate;
};

PL_EXPORT plVec2
main_vert(plVertexShaderBuiltIns tBuiltIns, plDescriptorSet* atDescriptorSets, const void* pVertexDataIn, plVaryingData* ptVaryingDataOut) 
{
    const char* pcVertexDataIn = (const char*)pVertexDataIn;
    plVec2 tPos = *(plVec2*)pl_shading_get_vertex_attrib(pVertexDataIn, tBuiltIns.atLayouts, 0, 0);
    plVec2 tUV = *(plVec2*)pl_shading_get_vertex_attrib(pVertexDataIn, tBuiltIns.atLayouts, 0, 1);
    uint32_t uColor = *(uint32_t*)pl_shading_get_vertex_attrib(pVertexDataIn, tBuiltIns.atLayouts, 0, 2);

    plVec4 tColor;
    unpackUnorm4x8(uColor, tColor.d);

    // set varyings (outputs)
    plVec4* ptColor = (plVec4*)pl_shading_set_varying(PL_VARYING_TYPE_VEC4, ptVaryingDataOut);  
    *ptColor = tColor;

    plVec2* ptUV = (plVec2*)pl_shading_set_varying(PL_VARYING_TYPE_VEC2, ptVaryingDataOut);  
    *ptUV = tUV;

    plDescriptor tDynamicBuffer = atDescriptorSets[3].atDescriptors[0];
    PL_DYNAMIC_DATA* tObjectInfo = (PL_DYNAMIC_DATA*)tDynamicBuffer.puData;

    return {
        tPos.x * tObjectInfo->uScale.x + tObjectInfo->uTranslate.x,
        tPos.y * tObjectInfo->uScale.y + tObjectInfo->uTranslate.y};
}