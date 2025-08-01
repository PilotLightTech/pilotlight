#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_ARB_shader_viewport_layer_array : enable

#include "defines.glsl"
#include "pl_shader_interop_renderer.h"

//-----------------------------------------------------------------------------
// [SECTION] specialication constants
//-----------------------------------------------------------------------------

layout(constant_id = 0) const int iMeshVariantFlags = 0;
layout(constant_id = 1) const int iDataStride = 0;
layout(constant_id = 2) const int iTextureMappingFlags = 0;
layout(constant_id = 3) const int iMaterialFlags = 0;

//-----------------------------------------------------------------------------
// [SECTION] bind group 0
//-----------------------------------------------------------------------------

layout(std140, set = 0, binding = 0) readonly buffer _tVertexBuffer
{
	vec4 atVertexData[];
} tVertexBuffer;

layout(std140, set = 0, binding = 1) readonly buffer _tTransformBuffer
{
	mat4 atTransform[];
} tTransformBuffer;

layout(set = 0, binding = 2) readonly buffer plMaterialInfo
{
    plGpuMaterial atMaterials[];
} tMaterialInfo;

layout(set = 0, binding = 3)  uniform sampler tDefaultSampler;
layout(set = 0, binding = 4)  uniform sampler tEnvSampler;
layout(set = 0, binding = 5)  uniform texture2D at2DTextures[PL_MAX_BINDLESS_TEXTURES];
layout(set = 0, binding = PL_MAX_BINDLESS_CUBE_TEXTURE_SLOT)  uniform textureCube atCubeTextures[PL_MAX_BINDLESS_TEXTURES];

//-----------------------------------------------------------------------------
// [SECTION] bind group 1
//-----------------------------------------------------------------------------

struct plShadowInstanceBufferData
{
    uint uTransformIndex;
    int iViewportIndex;
};

layout(set = 1, binding = 0) readonly buffer _plCameraInfo
{
    mat4 atCameraProjs[];
} tCameraInfo;

layout(set = 1, binding = 1) readonly buffer _plInstanceInfo
{
    plShadowInstanceBufferData atData[];
} plInstanceInfo;

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

layout(std140, set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    plGpuDynShadow tData;
} tObjectInfo;

//-----------------------------------------------------------------------------
// [SECTION] input & output
//-----------------------------------------------------------------------------

// input
layout(location = 0) in vec3 inPos;

// output
layout(location = 0) out struct plShaderOut {
    vec2 tUV[8];
} tShaderIn;

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void main()
{

    vec4 inPosition  = vec4(inPos, 1.0);
    vec2 inTexCoord0 = vec2(0.0, 0.0);
    vec2 inTexCoord1 = vec2(0.0, 0.0);
    vec2 inTexCoord2 = vec2(0.0, 0.0);
    vec2 inTexCoord3 = vec2(0.0, 0.0);
    vec2 inTexCoord4 = vec2(0.0, 0.0);
    vec2 inTexCoord5 = vec2(0.0, 0.0);
    vec2 inTexCoord6 = vec2(0.0, 0.0);
    vec2 inTexCoord7 = vec2(0.0, 0.0);

    uint uTransformIndex = plInstanceInfo.atData[gl_InstanceIndex].uTransformIndex;
    int uViewportIndex = plInstanceInfo.atData[gl_InstanceIndex].iViewportIndex;

    const mat4 tTransform = tTransformBuffer.atTransform[uTransformIndex];

    int iCurrentAttribute = 0;
    
    // offset = offset into current mesh + offset into global buffer
    const uint iVertexDataOffset = iDataStride * (gl_VertexIndex - tObjectInfo.tData.iVertexOffset) + tObjectInfo.tData.iDataOffset;

    if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_POSITION))  { iCurrentAttribute++;}
    if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_NORMAL))    { iCurrentAttribute++;}
    if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TANGENT))   { iCurrentAttribute++;}
    if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0)){
        inTexCoord0 = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute].xy;
        inTexCoord1 = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute].zw;
        iCurrentAttribute++;
    }
    if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1)){
        inTexCoord2 = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute].xy;
        inTexCoord3 = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute].zw;
        iCurrentAttribute++;
    }
    if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_2)){
        inTexCoord4 = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute].xy;
        inTexCoord5 = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute].zw;
        iCurrentAttribute++;
    }
    if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_3)){
        inTexCoord6 = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute].xy;
        inTexCoord7 = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute].zw;
        iCurrentAttribute++;
    }

    vec4 pos = tTransform * inPosition;

    // gl_Position = tCameraInfo.atCameraProjs[tObjectInfo.tData.iIndex + gl_InstanceIndex] * pos;
    // gl_ViewportIndex = gl_InstanceIndex;

    gl_Position = tCameraInfo.atCameraProjs[tObjectInfo.tData.iIndex + uViewportIndex] * pos;
    gl_ViewportIndex = uViewportIndex;

    if(tCameraInfo.atCameraProjs[tObjectInfo.tData.iIndex + uViewportIndex][3][3] == 1.0) // orthographic
        gl_Position.z = 1.0 - gl_Position.z;

    tShaderIn.tUV[0] = inTexCoord0;
    tShaderIn.tUV[1] = inTexCoord1;
    tShaderIn.tUV[2] = inTexCoord2;
    tShaderIn.tUV[3] = inTexCoord3;
    tShaderIn.tUV[4] = inTexCoord4;
    tShaderIn.tUV[5] = inTexCoord5;
    tShaderIn.tUV[6] = inTexCoord6;
    tShaderIn.tUV[7] = inTexCoord7;
}