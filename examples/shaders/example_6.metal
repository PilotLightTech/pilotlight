#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

//-----------------------------------------------------------------------------
// [SECTION] bind group 0
//-----------------------------------------------------------------------------

struct BindGroup_0
{
    sampler tSampler;
    texture2d<float> tTexture;
};

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

struct DynamicData
{
    float4 tTintColor;
};

//-----------------------------------------------------------------------------
// [SECTION] input
//-----------------------------------------------------------------------------

struct VertexIn {
    float2 tPosition [[attribute(0)]];
    float2 tUV [[attribute(1)]];
};

//-----------------------------------------------------------------------------
// [SECTION] output
//-----------------------------------------------------------------------------

struct VertexOut {
    float4 tPositionOut [[position]];
    float2 tUV;
};

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

vertex VertexOut
vertex_main(VertexIn in [[stage_in]])
{

    VertexOut tShaderOut;
    tShaderOut.tPositionOut = float4(in.tPosition, 0.0, 1.0);
    tShaderOut.tUV = in.tUV;
    tShaderOut.tPositionOut.y = tShaderOut.tPositionOut.y * -1.0;
    return tShaderOut;
}

fragment float4
fragment_main(
    VertexOut in [[stage_in]],
    device const BindGroup_0& bg0 [[ buffer(1) ]],
    device const DynamicData& tDynamicData [[ buffer(2) ]])
{
    float4 tTextureColor = bg0.tTexture.sample(bg0.tSampler, in.tUV);
    return tTextureColor * tDynamicData.tTintColor;
}