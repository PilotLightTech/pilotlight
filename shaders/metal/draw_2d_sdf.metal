#include <metal_stdlib>
using namespace metal;

//-----------------------------------------------------------------------------
// [SECTION] bind group 0
//-----------------------------------------------------------------------------

struct BindGroup_0
{
    sampler tDefaultSampler;
};

//-----------------------------------------------------------------------------
// [SECTION] bind group 1
//-----------------------------------------------------------------------------

struct BindGroup_1
{
    texture2d<float> tFontAtlas;
};

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

struct DynamicData
{
    float4 data;
};
    
struct VertexIn {
    float2 aPos  [[attribute(0)]];
    float2 aUV [[attribute(1)]];
    uchar4 aColor [[attribute(2)]];
};

struct VertexOut {
    float4 position [[position]];
    float2 UV;
    float4 Color;
};

vertex VertexOut vertex_main(
    VertexIn in [[stage_in]],
    device const DynamicData& tObjectInfo [[ buffer(3) ]])
{
    VertexOut out;
    out.Color = float4(in.aColor) / float4(255.0);
    out.UV = in.aUV;
    out.position = float4(in.aPos * tObjectInfo.data.xy + tObjectInfo.data.zw, 0, 1);
    out.position.y = out.position.y * -1.0;
    return out;
}

fragment float4 fragment_main(
    VertexOut in [[stage_in]],
    device const BindGroup_0& bg0 [[ buffer(1) ]],
    device const BindGroup_1& bg1 [[ buffer(2) ]])
{
    float distance = bg1.tFontAtlas.sample(bg0.tDefaultSampler, in.UV).a;
    float smoothWidth = fwidth(distance);
    float alpha = smoothstep(0.5 - smoothWidth, 0.5 + smoothWidth, distance);
    float3 texColor = bg1.tFontAtlas.sample(bg0.tDefaultSampler, in.UV).rgb * float3(in.Color.rgb);
    return float4(texColor, alpha);
}