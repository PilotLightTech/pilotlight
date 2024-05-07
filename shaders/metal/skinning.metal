#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

//-----------------------------------------------------------------------------
// [SECTION] specialization constants
//-----------------------------------------------------------------------------

constant int iSourceMeshVariantFlags [[ function_constant(0) ]];
constant int iSourceDataStride [[ function_constant(1) ]];
constant int iDestMeshVariantFlags [[ function_constant(2) ]];
constant int iDestDataStride [[ function_constant(3) ]];

//-----------------------------------------------------------------------------
// [SECTION] defines & structs
//-----------------------------------------------------------------------------

// iMeshVariantFlags
#define PL_MESH_FORMAT_FLAG_NONE           0
#define PL_MESH_FORMAT_FLAG_HAS_POSITION   1 << 0
#define PL_MESH_FORMAT_FLAG_HAS_NORMAL     1 << 1
#define PL_MESH_FORMAT_FLAG_HAS_TANGENT    1 << 2
#define PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0 1 << 3
#define PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1 1 << 4
#define PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_2 1 << 5
#define PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_3 1 << 6
#define PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_4 1 << 7
#define PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_5 1 << 8
#define PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_6 1 << 9
#define PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_7 1 << 10
#define PL_MESH_FORMAT_FLAG_HAS_COLOR_0    1 << 11
#define PL_MESH_FORMAT_FLAG_HAS_COLOR_1    1 << 12
#define PL_MESH_FORMAT_FLAG_HAS_JOINTS_0   1 << 13
#define PL_MESH_FORMAT_FLAG_HAS_JOINTS_1   1 << 14
#define PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0  1 << 15
#define PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1  1 << 16

//-----------------------------------------------------------------------------
// [SECTION] bind group 0
//-----------------------------------------------------------------------------

struct BindGroup_0
{
    device float4* atInputDataBuffer;
    device packed_float3* atOutputPosBuffer;
    device float4* atOutputDataBuffer;
    sampler        tSampler;
};

//-----------------------------------------------------------------------------
// [SECTION] bind group 1
//-----------------------------------------------------------------------------

struct BindGroup_1
{
    texture2d<float> tSkinningTexture;
};

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

struct DynamicData
{
    int iSourceDataOffset;
    int iDestDataOffset;
    int iDestVertexOffset;
    int iUnused;
};


//-----------------------------------------------------------------------------
// [SECTION] helpers
//-----------------------------------------------------------------------------

float4x4
get_matrix_from_texture(device const texture2d<float>& s, int index)
{
    float4x4 result = float4x4(1);
    int texSize = s.get_width();
    int pixelIndex = index * 4;
    for (int i = 0; i < 4; ++i)
    {
        int x = (pixelIndex + i) % texSize;
        //Rounding mode of integers is undefined:
        //https://www.khronos.org/registry/OpenGL/specs/es/3.0/GLSL_ES_Specification_3.00.pdf (section 12.33)
        int y = (pixelIndex + i - x) / texSize; 
        result[i] = s.read(uint2(x,y));
    }
    return result;
}

float4x4
get_skinning_matrix(device const texture2d<float>& s, float4 inJoints0, float4 inWeights0)
{
    float4x4 skin = float4x4(0);

    skin +=
        inWeights0.x * get_matrix_from_texture(s, int(inJoints0.x) * 2) +
        inWeights0.y * get_matrix_from_texture(s, int(inJoints0.y) * 2) +
        inWeights0.z * get_matrix_from_texture(s, int(inJoints0.z) * 2) +
        inWeights0.w * get_matrix_from_texture(s, int(inJoints0.w) * 2);

    // if (skin == float4x4(0)) { 
    //     return float4x4(1); 
    // }
    return skin;
}

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

kernel void kernel_main(
    device const BindGroup_0& bg0 [[ buffer(0) ]],
    device const BindGroup_1& bg1 [[ buffer(1) ]],
    device const DynamicData& tObjectInfo [[ buffer(2) ]],
    uint3 tWorkGroup [[threadgroup_position_in_grid]],
    uint3 tLocalIndex [[thread_position_in_threadgroup]]
    )
{

    const uint iVertexIndex = tWorkGroup.x;

    float4 inPosition  = float4(0.0, 0.0, 0.0, 1.0);
    float4 inNormal    = float4(0.0, 0.0, 0.0, 0.0);
    float4 inTangent   = float4(0.0, 0.0, 0.0, 0.0);
    float4 inJoints0   = float4(0.0, 0.0, 0.0, 0.0);
    float4 inWeights0  = float4(0.0, 0.0, 0.0, 0.0);

    const uint iSourceVertexDataOffset = iSourceDataStride * iVertexIndex + tObjectInfo.iSourceDataOffset;
    int iCurrentAttribute = 0;
    if(bool(iSourceMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_POSITION))  { inPosition.xyz = bg0.atInputDataBuffer[iSourceVertexDataOffset + iCurrentAttribute].xyz; iCurrentAttribute++;}
    if(bool(iSourceMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_NORMAL))    { inNormal.xyz   = bg0.atInputDataBuffer[iSourceVertexDataOffset + iCurrentAttribute].xyz; iCurrentAttribute++;}
    if(bool(iSourceMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TANGENT))   { inTangent      = bg0.atInputDataBuffer[iSourceVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}
    if(bool(iSourceMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_JOINTS_0))  { inJoints0      = bg0.atInputDataBuffer[iSourceVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}
    if(bool(iSourceMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0)) { inWeights0     = bg0.atInputDataBuffer[iSourceVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}

    float4x4 skin = get_skinning_matrix(bg1.tSkinningTexture, inJoints0, inWeights0);

    float4 outPosition = skin * inPosition;
    float3 outNormal = fast::normalize(skin * inNormal).xyz;
    float3 outTangent = fast::normalize(skin * inTangent).xyz;

    const uint iDestVertexDataOffset = iDestDataStride * iVertexIndex + tObjectInfo.iDestDataOffset;
    iCurrentAttribute = 0;
    bg0.atOutputPosBuffer[iVertexIndex + tObjectInfo.iDestVertexOffset] = outPosition.xyz;
    if(bool(iDestMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_POSITION)){ iCurrentAttribute++;}
    if(bool(iDestMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_NORMAL))  { bg0.atOutputDataBuffer[iDestVertexDataOffset + iCurrentAttribute].xyz = outNormal; iCurrentAttribute++;}
    if(bool(iDestMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TANGENT)) { bg0.atOutputDataBuffer[iDestVertexDataOffset + iCurrentAttribute].xyz = outTangent; iCurrentAttribute++;}
}