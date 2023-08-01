#include <metal_stdlib>
using namespace metal;

struct VertexIn {
    float3 tPosition [[attribute(0)]];
    float4 tColor    [[attribute(1)]];
};

struct VertexOut {
    float4 tPosition [[position]];
    float4 tColor;
};

vertex VertexOut vertex_main(VertexIn in [[stage_in]])
{
    VertexOut tOut;
    tOut.tPosition = float4(in.tPosition, 1);
    tOut.tColor = in.tColor;
    return tOut;
}

fragment half4 fragment_main(VertexOut in [[stage_in]])
{
    // return half4(in.tColor);
    return half4(float4(1.0, 1.0, 0.0, 1.0));
}