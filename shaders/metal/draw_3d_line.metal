#include <metal_stdlib>
using namespace metal;

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

struct DynamicData
{
    float4x4 tMVP;
    float    fAspect;
};

//-----------------------------------------------------------------------------
// [SECTION] input
//-----------------------------------------------------------------------------

struct VertexIn
{
    float3 aPos      [[attribute(0)]];
    float4 aInfo     [[attribute(1)]];
    float3 aPosOther [[attribute(2)]];
    float4 color     [[attribute(3)]];
};

//-----------------------------------------------------------------------------
// [SECTION] output
//-----------------------------------------------------------------------------

struct VertexOut
{
    float4 position [[position]];
    float4 color;
};

//-----------------------------------------------------------------------------
// [SECTION] entries
//-----------------------------------------------------------------------------

vertex VertexOut
vertex_main(VertexIn in [[stage_in]],
            device DynamicData& bg0 [[ buffer(1)]])
{
    // clip space
    float4 tCurrentProj = bg0.tMVP * float4(in.aPos.xyz, 1.0f);
    float4 tOtherProj = bg0.tMVP * float4(in.aPosOther.xyz, 1.0f);
    // NDC Space
    float2 tCurrentNDC =  tCurrentProj.xy / tCurrentProj.w;
    float2 tOtherNDC =  tOtherProj.xy / tOtherProj.w;
    // correct for aspect
    tCurrentNDC.x *= bg0.fAspect;
    tOtherNDC.x *= bg0.fAspect;
    // normal of line (B - A)
    float2 dir = in.aInfo.z * normalize(tOtherNDC - tCurrentNDC);
    float2 normal = float2(-dir.y, dir.x);
    // extrude from center & correct aspect ratio
    normal *= in.aInfo.y / 2.0;
    normal.x /= bg0.fAspect;
    // offset by the direction of this point in the pair (-1 or 1)
    float4 offset = float4(normal * in.aInfo.x, 0.0, 0.0);
    VertexOut out;
    out.position = tCurrentProj + offset;
    out.position.y *= -1;
    out.color = in.color;
    return out;
}

fragment half4
fragment_main(VertexOut in [[stage_in]],
            device DynamicData& bg0 [[ buffer(1)]])
{
    return half4(in.color);
}