#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

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
fragment_main(VertexOut in [[stage_in]])
{
    return float4(in.tUV, 0, 1);
}