#include <metal_stdlib>
using namespace metal;

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

struct DynamicData
{
    float4x4 tMVP;
};

//-----------------------------------------------------------------------------
// [SECTION] input
//-----------------------------------------------------------------------------

struct VertexIn
{
    float3 position  [[attribute(0)]];
    uchar4 color     [[attribute(1)]];
};

//-----------------------------------------------------------------------------
// [SECTION] output
//-----------------------------------------------------------------------------

struct VertexOut
{
    float4 position [[position]];
    float4 color;
};


vertex VertexOut
vertex_main(VertexIn in [[stage_in]],
            device DynamicData& bg0 [[ buffer(1)]])
{
    VertexOut out;
    out.color = float4(in.color) / float4(255.0);
    out.position = bg0.tMVP * float4(in.position, 1);
    out.position.y *= -1;
    return out;
}

fragment float4
fragment_main(VertexOut in [[stage_in]],
            device DynamicData& bg0 [[ buffer(1)]])
{
    return in.color;
}