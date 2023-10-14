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

    device float *buffer_0;
};

struct BindGroup_1
{

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
    float3 inPosition = in.tPosition;
    float3 inNormal = float3(0.0, 0.0, 0.0);
    float4 inTangent = float4(0.0, 0.0, 0.0, 0.0);
    float2 inTexCoord0 = float2(0.0, 0.0);
    float2 inTexCoord1 = float2(0.0, 0.0);
    float4 inColor0 = float4(1.0, 1.0, 1.0, 1.0);
    float4 inColor1 = float4(0.0, 0.0, 0.0, 0.0);
    float4 inJoints0 = float4(0.0, 0.0, 0.0, 0.0);
    float4 inJoints1 = float4(0.0, 0.0, 0.0, 0.0);
    float4 inWeights0 = float4(0.0, 0.0, 0.0, 0.0);
    float4 inWeights1 = float4(0.0, 0.0, 0.0, 0.0);
    const float4x4 tMvp = bg0.data->tCameraViewProjection * bg3.tModel;

    tOut.tPosition = tMvp * float4(inPosition, 1);
    tOut.tPosition.y = tOut.tPosition.y * -1;

    uint uDataOffset = 0;

    const uint iVertexDataOffset = PL_DATA_STRIDE * (vertexID - bg3.iVertexOffset) + bg3.iDataOffset * 4;

    #if PL_MESH_FORMAT_FLAG_HAS_POSITION > 0
    inPosition.x = bg0.buffer_0[iVertexDataOffset + 0 + uDataOffset];
    inPosition.y = bg0.buffer_0[iVertexDataOffset + 1 + uDataOffset];
    inPosition.z = bg0.buffer_0[iVertexDataOffset + 2 + uDataOffset];
    uDataOffset += 4;
    #endif

    #if PL_MESH_FORMAT_FLAG_HAS_NORMAL > 0
    inNormal.x = bg0.buffer_0[iVertexDataOffset + 0 + uDataOffset];
    inNormal.y = bg0.buffer_0[iVertexDataOffset + 1 + uDataOffset];
    inNormal.z = bg0.buffer_0[iVertexDataOffset + 2 + uDataOffset];
    uDataOffset += 4;
    #endif

    #if PL_MESH_FORMAT_FLAG_HAS_TANGENT > 0
    inTangent.x = bg0.buffer_0[iVertexDataOffset + 0 + uDataOffset];
    inTangent.y = bg0.buffer_0[iVertexDataOffset + 1 + uDataOffset];
    inTangent.z = bg0.buffer_0[iVertexDataOffset + 2 + uDataOffset];
    inTangent.w = bg0.buffer_0[iVertexDataOffset + 3 + uDataOffset];
    uDataOffset += 4;
    #endif

    #if PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0 > 0
    inTexCoord0.x = bg0.buffer_0[iVertexDataOffset + 0 + uDataOffset];
    inTexCoord0.y = bg0.buffer_0[iVertexDataOffset + 1 + uDataOffset];
    uDataOffset += 4;
    #endif

    #if PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1 > 0
    inTexCoord1.x = bg0.buffer_0[iVertexDataOffset + 0 + uDataOffset];
    inTexCoord1.y = bg0.buffer_0[iVertexDataOffset + 1 + uDataOffset];
    uDataOffset += 4;
    #endif

    #if PL_MESH_FORMAT_FLAG_HAS_COLOR_0 > 0
    inColor0.r = bg0.buffer_0[iVertexDataOffset + 0 + uDataOffset];
    inColor0.g = bg0.buffer_0[iVertexDataOffset + 1 + uDataOffset];
    inColor0.b = bg0.buffer_0[iVertexDataOffset + 2 + uDataOffset];
    inColor0.a = bg0.buffer_0[iVertexDataOffset + 3 + uDataOffset];
    uDataOffset += 4;
    #endif

    #if PL_MESH_FORMAT_FLAG_HAS_COLOR_1 > 0
    inColor1.r = bg0.buffer_0[iVertexDataOffset + 0 + uDataOffset];
    inColor1.g = bg0.buffer_0[iVertexDataOffset + 1 + uDataOffset];
    inColor1.b = bg0.buffer_0[iVertexDataOffset + 2 + uDataOffset];
    inColor1.a = bg0.buffer_0[iVertexDataOffset + 3 + uDataOffset];
    uDataOffset += 4;
    #endif

    #if PL_MESH_FORMAT_FLAG_HAS_JOINTS_0 > 0
    inJoints0.r = bg0.buffer_0[iVertexDataOffset + 0 + uDataOffset];
    inJoints0.g = bg0.buffer_0[iVertexDataOffset + 1 + uDataOffset];
    inJoints0.b = bg0.buffer_0[iVertexDataOffset + 2 + uDataOffset];
    inJoints0.a = bg0.buffer_0[iVertexDataOffset + 3 + uDataOffset];
    uDataOffset += 4;
    #endif

    #if PL_MESH_FORMAT_FLAG_HAS_JOINTS_1 > 0
    inJoints1.r = bg0.buffer_0[iVertexDataOffset + 0 + uDataOffset];
    inJoints1.g = bg0.buffer_0[iVertexDataOffset + 1 + uDataOffset];
    inJoints1.b = bg0.buffer_0[iVertexDataOffset + 2 + uDataOffset];
    inJoints1.a = bg0.buffer_0[iVertexDataOffset + 3 + uDataOffset];
    uDataOffset += 4;
    #endif

    #if PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0 > 0
    inWeights0.r = bg1.buffer_0[iVertexDataOffset + 0 + uDataOffset];
    inWeights0.g = bg1.buffer_0[iVertexDataOffset + 1 + uDataOffset];
    inWeights0.b = bg1.buffer_0[iVertexDataOffset + 2 + uDataOffset];
    inWeights0.a = bg1.buffer_0[iVertexDataOffset + 3 + uDataOffset];
    uDataOffset += 4;
    #endif

    #if PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1 > 0
    inWeights1.r = bg0.buffer_0[iVertexDataOffset + 0 + uDataOffset];
    inWeights1.g = bg0.buffer_0[iVertexDataOffset + 1 + uDataOffset];
    inWeights1.b = bg0.buffer_0[iVertexDataOffset + 2 + uDataOffset];
    inWeights1.a = bg0.buffer_0[iVertexDataOffset + 3 + uDataOffset];
    uDataOffset += 4;
    #endif

    tOut.tUV = inTexCoord0;
    tOut.tColor = inColor0;

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