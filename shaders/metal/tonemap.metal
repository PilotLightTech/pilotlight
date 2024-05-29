#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

//-----------------------------------------------------------------------------
// [SECTION] defines & structs
//-----------------------------------------------------------------------------

constant const float GAMMA = 2.2;
constant const float INV_GAMMA = 1.0 / GAMMA;

//-----------------------------------------------------------------------------
// [SECTION] bind group 0
//-----------------------------------------------------------------------------

struct BindGroup_0
{
    sampler          tDefaultSampler;
    texture2d<float> tTexture;
};

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

struct DynamicData
{
    int      iIndex;
    int      iDataOffset;
    int      iVertexOffset;
    int      iMaterialIndex;
    float4x4 tModel;
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
// [SECTION] helpers
//-----------------------------------------------------------------------------

float3
linearTosRGB(float3 color)
{
    return pow(color, float3(INV_GAMMA));
}

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

vertex VertexOut vertex_main(
    uint     vertexID [[ vertex_id ]],
    VertexIn in [[stage_in]]
    )
{

    VertexOut tShaderOut;
    tShaderOut.tPositionOut = float4(in.tPosition, 0.0, 1.0);
    tShaderOut.tUV = in.tUV;
    tShaderOut.tPositionOut.y = tShaderOut.tPositionOut.y * -1.0;
    return tShaderOut;
}

fragment float4 fragment_main(
    VertexOut in [[stage_in]],
    device const BindGroup_0& bg0 [[ buffer(1) ]]
    )
{

    float4 tBaseColor = bg0.tTexture.sample(bg0.tDefaultSampler, in.tUV);
    return float4(linearTosRGB(tBaseColor.rgb), tBaseColor.a);
}