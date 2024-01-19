#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "common.glsl"

// input
layout(location = 0) in vec3 inPos;

// output
layout(location = 0) out struct plShaderOut {
    vec3 tPosition;
    vec4 tWorldPosition;
    vec2 tUV[2];
    vec4 tColor;
    vec3 tWorldNormal;
    mat3 tTBN;
} tShaderOut;

vec4 get_position(vec4 inJoints0, vec4 inWeights0)
{
    vec4 pos = vec4(inPos, 1.0);
    return pos;
}

vec3 get_normal(vec3 inNormal, vec4 inJoints0, vec4 inWeights0)
{
    vec3 tNormal = inNormal;
    return normalize(tNormal);
}

vec3 get_tangent(vec4 inTangent, vec4 inJoints0, vec4 inWeights0)
{
    vec3 tTangent = inTangent.xyz;
    return normalize(tTangent);
}

void main() 
{

    vec4 inPosition  = vec4(inPos, 1.0);
    vec3 inNormal    = vec3(0.0, 0.0, 0.0);
    vec4 inTangent   = vec4(0.0, 0.0, 0.0, 0.0);
    vec2 inTexCoord0 = vec2(0.0, 0.0);
    vec2 inTexCoord1 = vec2(0.0, 0.0);
    vec4 inColor0    = vec4(1.0, 1.0, 1.0, 1.0);
    vec4 inColor1    = vec4(0.0, 0.0, 0.0, 0.0);
    vec4 inJoints0   = vec4(0.0, 0.0, 0.0, 0.0);
    vec4 inJoints1   = vec4(0.0, 0.0, 0.0, 0.0);
    vec4 inWeights0  = vec4(0.0, 0.0, 0.0, 0.0);
    vec4 inWeights1  = vec4(0.0, 0.0, 0.0, 0.0);
    int iCurrentAttribute = 0;
    
    // offset = offset into current mesh + offset into global buffer
    const uint iVertexDataOffset = PL_DATA_STRIDE * (gl_VertexIndex - tObjectInfo.iVertexOffset) + tObjectInfo.iDataOffset;

    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_POSITION))  { inPosition.xyz = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute].xyz; iCurrentAttribute++;}
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_NORMAL))    { inNormal       = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute].xyz; iCurrentAttribute++;}
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TANGENT))   { inTangent      = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0)){ inTexCoord0    = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute].xy;  iCurrentAttribute++;}
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1)){ inTexCoord1    = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute].xy;  iCurrentAttribute++;}
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_COLOR_0))   { inColor0       = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_COLOR_1))   { inColor1       = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_JOINTS_0))  { inJoints0      = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_JOINTS_1))  { inJoints1      = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0)) { inWeights0     = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1)) { inWeights1     = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}

    tShaderOut.tWorldNormal = mat3(tObjectInfo.tModel) * get_normal(inNormal, inJoints0, inWeights0);
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_NORMAL))
    {

        if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TANGENT))
        {
            vec3 tangent = get_tangent(inTangent, inJoints0, inWeights0);
            vec3 WorldTangent = mat3(tObjectInfo.tModel) * tangent;
            vec3 WorldBitangent = cross(get_normal(inNormal, inJoints0, inWeights0), tangent) * inTangent.w;
            WorldBitangent = mat3(tObjectInfo.tModel) * WorldBitangent;
            tShaderOut.tTBN = mat3(WorldTangent, WorldBitangent, tShaderOut.tWorldNormal);
        }
    }

    vec4 pos = tObjectInfo.tModel * get_position(inJoints0, inWeights0);
    tShaderOut.tPosition = pos.xyz / pos.w;
    gl_Position = tGlobalInfo.tCameraViewProjection * pos;
    tShaderOut.tUV[0] = inTexCoord0;
    tShaderOut.tWorldPosition = gl_Position / gl_Position.w;
    tShaderOut.tColor = inColor0;
}