#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

#define PL_MESH_FORMAT_FLAG_NONE            0
#define PL_MESH_FORMAT_FLAG_HAS_POSITION    1 << 0
#define PL_MESH_FORMAT_FLAG_HAS_NORMAL      1 << 1
#define PL_MESH_FORMAT_FLAG_HAS_TANGENT     1 << 2
#define PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0  1 << 3
#define PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1  1 << 4
#define PL_MESH_FORMAT_FLAG_HAS_COLOR_0     1 << 5
#define PL_MESH_FORMAT_FLAG_HAS_COLOR_1     1 << 6
#define PL_MESH_FORMAT_FLAG_HAS_JOINTS_0    1 << 7
#define PL_MESH_FORMAT_FLAG_HAS_JOINTS_1    1 << 8
#define PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0   1 << 9
#define PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1   1 << 10
#define PL_TEXTURE_HAS_BASE_COLOR           1 << 0
#define PL_TEXTURE_HAS_NORMAL               1 << 1

struct BindGroupData_0
{
    float4   tCameraPosition;
    float4x4 tCameraView;
    float4x4 tCameraProjection;   
    float4x4 tCameraViewProjection;   
};

struct BindGroup_0
{
    device BindGroupData_0 *data;  

    device float4 *buffer_0;
};

struct BindGroup_1
{

    texture2d<half>  texture_0;
    sampler          sampler_0;

    texture2d<half>  texture_1;
    sampler          sampler_1;

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

constant int MeshVariantFlags [[ function_constant(0) ]];
constant int PL_DATA_STRIDE [[ function_constant(1) ]];
constant int ShaderTextureFlags [[ function_constant(2) ]];

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

    const uint iVertexDataOffset = PL_DATA_STRIDE * (vertexID - bg3.iVertexOffset) + bg3.iDataOffset;

    int iCurrentAttribute = 0;
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_POSITION))  { inPosition.xyz = bg0.buffer_0[iVertexDataOffset + iCurrentAttribute].xyz; iCurrentAttribute++;}
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_NORMAL))    { inNormal       = bg0.buffer_0[iVertexDataOffset + iCurrentAttribute].xyz; iCurrentAttribute++;}
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TANGENT))   { inTangent      = bg0.buffer_0[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0)){ inTexCoord0    = bg0.buffer_0[iVertexDataOffset + iCurrentAttribute].xy;  iCurrentAttribute++;}
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1)){ inTexCoord1    = bg0.buffer_0[iVertexDataOffset + iCurrentAttribute].xy;  iCurrentAttribute++;}
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_COLOR_0))   { inColor0       = bg0.buffer_0[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_COLOR_1))   { inColor1       = bg0.buffer_0[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_JOINTS_0))  { inJoints0      = bg0.buffer_0[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_JOINTS_1))  { inJoints1      = bg0.buffer_0[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0)) { inWeights0     = bg0.buffer_0[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1)) { inWeights1     = bg0.buffer_0[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}

    tOut.tUV = inTexCoord0;
    tOut.tColor = inColor0;

    return tOut;
}

fragment float4 fragment_main(
    VertexOut in [[stage_in]],
    device BindGroup_0& bg0 [[ buffer(1) ]],
    device BindGroup_1& bg1 [[ buffer(2) ]],
    device BindGroup_2& bg2 [[ buffer(3) ]],
    device DynamicData& bg3 [[ buffer(4) ]]
    )
{

    float4 tColor = in.tColor;
    if(ShaderTextureFlags & PL_TEXTURE_HAS_BASE_COLOR)
    {
        half4 textureSample = bg1.texture_0.sample(bg1.sampler_0, in.tUV);
        tColor = float4(textureSample);
    }
    return tColor;
}