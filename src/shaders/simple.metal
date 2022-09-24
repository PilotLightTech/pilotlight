#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

typedef struct
{
    vector_float2 position; // Positions in pixel space (i.e. a value of 100 indicates 100 pixels from the origin/center)
    vector_float3 color; // 2D texture coordinate
} plVertex;

typedef struct
{
    float scale;
    vector_uint2 viewportSize;
} AAPLUniforms;

// Vertex shader outputs and per-fragment inputs
struct RasterizerData
{
    float4 clipSpacePosition [[position]];
    float3 color;
};

vertex RasterizerData vertexShader(uint vertexID [[ vertex_id ]], constant plVertex *vertexArray [[ buffer(0) ]], constant AAPLUniforms &uniforms  [[ buffer(1) ]])
{
    RasterizerData out;

    float2 pixelSpacePosition = vertexArray[vertexID].position.xy;

    // scale the vertex by scale factor of the current frame
    pixelSpacePosition *= uniforms.scale;

    float2 viewportSize = float2(uniforms.viewportSize);

    // divide the pixel coordinates by half the size of the viewport to convert from positions in
    // pixel space to positions in clip space
    out.clipSpacePosition.xy = pixelSpacePosition / (viewportSize / 2.0);
    out.clipSpacePosition.z = 0.0;
    out.clipSpacePosition.w = 1.0;

    out.color = vertexArray[vertexID].color;

    return out;
}

fragment float4
fragmentShader(RasterizerData in [[stage_in]])
{
    return float4(in.color, 1.0);
}

