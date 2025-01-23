#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "defines.glsl"
#include "material.glsl"

//-----------------------------------------------------------------------------
// [SECTION] specialication constants
//-----------------------------------------------------------------------------

layout(constant_id = 0) const int iMeshVariantFlags = 0;
layout(constant_id = 1) const int iDataStride = 0;
layout(constant_id = 2) const int iTextureMappingFlags = 0;
layout(constant_id = 3) const int iMaterialFlags = 0;
layout(constant_id = 4) const int iRenderingFlags = 0;

//-----------------------------------------------------------------------------
// [SECTION] bind group 0
//-----------------------------------------------------------------------------

layout(std140, set = 0, binding = 0) readonly buffer _tVertexBuffer
{
	vec4 atVertexData[];
} tVertexBuffer;

layout(set = 0, binding = 1) readonly buffer plMaterialInfo
{
    tMaterial atMaterials[];
} tMaterialInfo;

layout(set = 0, binding = 2)  uniform sampler tDefaultSampler;
layout(set = 0, binding = 3)  uniform sampler tEnvSampler;
layout(set = 0, binding = 4)  uniform texture2D at2DTextures[4096];
layout(set = 0, binding = 4100)  uniform textureCube atCubeTextures[4096];

//-----------------------------------------------------------------------------
// [SECTION] bind group 1
//-----------------------------------------------------------------------------

struct tGlobalData
{
    vec4 tViewportSize;
    vec4 tViewportInfo;
    vec4 tCameraPos;
    mat4 tCameraView;
    mat4 tCameraProjection;
    mat4 tCameraViewProjection;
};

layout(set = 1, binding = 0) readonly buffer _plGlobalInfo
{
    tGlobalData data[];
} tGlobalInfo;

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    int  iDataOffset;
    int  iVertexOffset;
    int  iMaterialIndex;
    mat4 tModel;

    uint uGlobalIndex;
} tObjectInfo;

//-----------------------------------------------------------------------------
// [SECTION] input & output
//-----------------------------------------------------------------------------

// input
layout(location = 0) in vec3 inPos;

// output
layout(location = 0) out struct plShaderOut {
    vec3 tWorldPosition;
    vec4 tViewPosition;
    vec2 tUV[8];
    vec4 tColor;
    vec3 tWorldNormal;
    mat3 tTBN;
} tShaderIn;

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void main() 
{

    vec4 inPosition  = vec4(inPos, 1.0);
    vec3 inNormal    = vec3(0.0, 0.0, 0.0);
    vec4 inTangent   = vec4(0.0, 0.0, 0.0, 0.0);
    vec2 inTexCoord0 = vec2(0.0, 0.0);
    vec2 inTexCoord1 = vec2(0.0, 0.0);
    vec2 inTexCoord2 = vec2(0.0, 0.0);
    vec2 inTexCoord3 = vec2(0.0, 0.0);
    vec2 inTexCoord4 = vec2(0.0, 0.0);
    vec2 inTexCoord5 = vec2(0.0, 0.0);
    vec2 inTexCoord6 = vec2(0.0, 0.0);
    vec2 inTexCoord7 = vec2(0.0, 0.0);
    vec4 inColor0    = vec4(1.0, 1.0, 1.0, 1.0);
    vec4 inColor1    = vec4(0.0, 0.0, 0.0, 0.0);
    int iCurrentAttribute = 0;
    
    // offset = offset into current mesh + offset into global buffer
    const uint iVertexDataOffset = iDataStride * (gl_VertexIndex - tObjectInfo.iVertexOffset) + tObjectInfo.iDataOffset;

    if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_POSITION))  { inPosition.xyz = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute].xyz; iCurrentAttribute++;}
    if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_NORMAL))    { inNormal       = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute].xyz; iCurrentAttribute++;}
    if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TANGENT))   { inTangent      = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}
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
    if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_COLOR_0))   { inColor0       = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}
    if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_COLOR_1))   { inColor1       = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}

    tShaderIn.tWorldNormal = mat3(tObjectInfo.tModel) * normalize(inNormal);
    if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_NORMAL))
    {

        if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TANGENT))
        {
            vec3 tangent = normalize(inTangent.xyz);
            vec3 WorldTangent = mat3(tObjectInfo.tModel) * tangent;
            vec3 WorldBitangent = cross(normalize(inNormal), tangent) * inTangent.w;
            WorldBitangent = mat3(tObjectInfo.tModel) * WorldBitangent;
            tShaderIn.tTBN = mat3(WorldTangent, WorldBitangent, tShaderIn.tWorldNormal);
        }
    }

    vec4 pos = tObjectInfo.tModel * inPosition;
    tShaderIn.tWorldPosition = pos.xyz / pos.w;
    gl_Position = tGlobalInfo.data[tObjectInfo.uGlobalIndex].tCameraViewProjection * pos;
    tShaderIn.tUV[0] = inTexCoord0;
    tShaderIn.tUV[1] = inTexCoord1;
    tShaderIn.tUV[2] = inTexCoord2;
    tShaderIn.tUV[3] = inTexCoord3;
    tShaderIn.tUV[4] = inTexCoord4;
    tShaderIn.tUV[5] = inTexCoord5;
    tShaderIn.tUV[6] = inTexCoord6;
    tShaderIn.tUV[7] = inTexCoord7;
    tShaderIn.tViewPosition = gl_Position / gl_Position.w;
    tShaderIn.tColor = inColor0;
}