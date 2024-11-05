#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "defines.glsl"
#include "material.glsl"

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

struct plCameraInfo
{
    vec4 tCameraPos;
    mat4 tCameraView;
    mat4 tCameraProjection;
    mat4 tCameraViewProjection;
};

layout(set = 0, binding = 0) readonly buffer _plGlobalInfo
{
    plCameraInfo atInfo[];
} tGlobalInfo;

layout(std140, set = 0, binding = 1) readonly buffer _tVertexBuffer
{
	vec4 atVertexData[];
} tVertexBuffer;

layout(set = 0, binding = 2) readonly buffer plMaterialInfo
{
    tMaterial atMaterials[];
} tMaterialInfo;

layout(set = 0, binding = 3)  uniform sampler tDefaultSampler;

//-----------------------------------------------------------------------------
// [SECTION] bind group 0
//-----------------------------------------------------------------------------

layout(set = 1, binding = 0) uniform texture2D tBaseColorTexture;

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

layout(std140, set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    int  iIndex;
    int  iDataOffset;
    int  iVertexOffset;
    int  iMaterialIndex;
    mat4 tModel;
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

    int iCurrentAttribute = 0;
    
    // offset = offset into current mesh + offset into global buffer
    const uint iVertexDataOffset = iDataStride * (gl_VertexIndex - tObjectInfo.iVertexOffset) + tObjectInfo.iDataOffset;

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

    vec4 pos = tObjectInfo.tModel * inPosition;
    gl_Position = tGlobalInfo.atInfo[tObjectInfo.iIndex].tCameraViewProjection * pos;
    tShaderIn.tUV[0] = inTexCoord0;
    tShaderIn.tUV[1] = inTexCoord1;
    tShaderIn.tUV[2] = inTexCoord2;
    tShaderIn.tUV[3] = inTexCoord3;
    tShaderIn.tUV[4] = inTexCoord4;
    tShaderIn.tUV[5] = inTexCoord5;
    tShaderIn.tUV[6] = inTexCoord6;
    tShaderIn.tUV[7] = inTexCoord7;
}