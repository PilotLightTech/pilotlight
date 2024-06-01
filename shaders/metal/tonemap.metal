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
    sampler          tSampler;
    texture2d<float> tColorTexture;
    texture2d<float> tMaskTexture;
};

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

struct DynamicData
{
    float fTargetWidth;
    int   iPadding[3];
    float4 tOutlineColor;
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
    device const BindGroup_0& bg0 [[ buffer(1) ]],
    device const DynamicData& tObjectInfo [[ buffer(2) ]]
    )
{
    float4 color = bg0.tColorTexture.sample(bg0.tSampler, in.tUV);
    float2 closestSeed = bg0.tMaskTexture.sample(bg0.tSampler, in.tUV).xy;
    float2 h = closestSeed - in.tUV;
    float xdist = h.x * float(bg0.tColorTexture.get_width());
    float ydist = h.y * float(bg0.tColorTexture.get_height());
    float tdist2 = xdist * xdist + ydist * ydist;
    float dist = distance(closestSeed, in.tUV);
    if (closestSeed.x > 0 && closestSeed.y > 0 && dist > 0 && tdist2 < tObjectInfo.fTargetWidth * tObjectInfo.fTargetWidth)
    {
        color = tObjectInfo.tOutlineColor;
    }
    color.rgb = linearTosRGB(color.rgb);
    return color;
}