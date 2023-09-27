#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct BindGroupData_0
{
    float4x4 tCameraView;
    float4x4 tCameraProjection;   
    float4x4 tCameraViewProjection;   
};

struct BindGroup_0
{
    device BindGroupData_0 *data;  
};

struct BindGroup_1
{

    #if PL_BUFFER_COUNT > 0
    device float    *buffer_0;
    #endif

    #if PL_TEXTURE_COUNT > 0
    texture2d<half>  texture_0;
    sampler          sampler_0;
    #endif

    #if PL_TEXTURE_COUNT > 1
    texture2d<half>  texture_1;
    sampler          sampler_1;
    #endif
};

struct BindGroupData_2
{
    float4 shaderSpecific;
};

struct BindGroup_2
{
    device BindGroupData_2 *data;  
};

struct VertexIn {
    float3 tPosition [[attribute(0)]];
};

struct VertexOut {
    float4 tPosition [[position]];
    float2 tUV;
    float4 tColor;
};

struct DynamicData
{
    int      iDataOffset;
    int      iVertexOffset;
    int      iPadding[2];
    float4x4 tModel;
};

vertex VertexOut vertex_main(
    uint                vertexID [[ vertex_id ]],
    VertexIn            in [[stage_in]],
    device BindGroup_0& bg0 [[ buffer(1) ]],
    device BindGroup_1& bg1 [[ buffer(2) ]],
    device BindGroup_2& bg2 [[ buffer(3) ]],
    device DynamicData& bg3 [[ buffer(4) ]]
    )
{
    VertexOut tOut;
    const float4x4 tMvp = bg0.data->tCameraViewProjection * bg3.tModel;
    tOut.tPosition = tMvp * float4(in.tPosition, 1);
    tOut.tPosition.y = tOut.tPosition.y * -1;

    uint uDataOffset = 0;

    const uint iVertexDataOffset = PL_DATA_STRIDE * (vertexID - bg3.iVertexOffset) + bg3.iDataOffset;

    #if PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0 > 0
    tOut.tUV.x = bg1.buffer_0[iVertexDataOffset + 0 + uDataOffset];
    tOut.tUV.y = bg1.buffer_0[iVertexDataOffset + 1 + uDataOffset];
    uDataOffset += 4;
    #endif

    #if PL_MESH_FORMAT_FLAG_HAS_COLOR_0 > 0
    tOut.tColor.r = bg1.buffer_0[iVertexDataOffset + 0 + uDataOffset];
    tOut.tColor.g = bg1.buffer_0[iVertexDataOffset + 1 + uDataOffset];
    tOut.tColor.b = bg1.buffer_0[iVertexDataOffset + 2 + uDataOffset];
    tOut.tColor.a = bg1.buffer_0[iVertexDataOffset + 3 + uDataOffset];
    uDataOffset += 4;
    #endif


    return tOut;
}

fragment float4 fragment_main(
    VertexOut in [[stage_in]],
    device BindGroup_0& bg0 [[ buffer(1) ]],
    device BindGroup_1& bg1 [[ buffer(2) ]],
    device BindGroup_2& bg2 [[ buffer(3) ]],
    device DynamicData& bg3 [[ buffer(4) ]])
{

    #if PL_TEXTURE_COUNT > 0
    half4 textureSample = bg1.texture_0.sample(bg1.sampler_0, in.tUV);

    // Add the sample and color values together and return the result.
    return float4(textureSample) + bg2.data->shaderSpecific;
    #endif
    return in.tColor + bg2.data->shaderSpecific;
}