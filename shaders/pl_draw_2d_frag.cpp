#include "pl.inc"
#include  <cstddef>
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"
#include "pl_graphics_ext.h"
#include "pl_shader_interop_cpu.h"

PL_EXPORT plVec4
main_frag(plPixelShaderBuiltIns tBuiltIns, plDescriptorSet* atDescriptorSets, const plVaryingData* ptVaryingDataIn)
{
    const plVec4* ptColor = (plVec4*)pl_shading_get_varying(0, ptVaryingDataIn);
    const plVec2* ptUV = (plVec2*)pl_shading_get_varying(1, ptVaryingDataIn);

    plDescriptor tTexture = atDescriptorSets[1].atDescriptors[0];

    plVec4 tSampledColor = pl_sample_texture_bilinear(&tTexture, *ptUV);
    // plVec4 tSampledColor = pl_sample_texture(&tTexture, *ptUV);
    plVec4 tOutColor = pl_mul_vec4(*ptColor, tSampledColor);
    return {tOutColor.b, tOutColor.g, tOutColor.r, tOutColor.a};
}