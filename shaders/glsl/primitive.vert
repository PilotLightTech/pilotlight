#version 450
#extension GL_ARB_separate_shader_objects : enable

// input
layout(location = 0) in vec3 inPos;

// output
layout(location = 0) out struct plShaderOut {
    vec3 tPosition;
    vec4 tWorldPosition;
    vec2 tUV[8];
    vec4 tColor;
    vec3 tWorldNormal;
    mat3 tTBN;
} tShaderIn;

#include "gbuffer_common.glsl"

void
main() 
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
    if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0)){ inTexCoord0    = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute].xy;  iCurrentAttribute++;}
    if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1)){ inTexCoord1    = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute].xy;  iCurrentAttribute++;}
    if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_2)){ inTexCoord2    = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute].xy;  iCurrentAttribute++;}
    if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_3)){ inTexCoord3    = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute].xy;  iCurrentAttribute++;}
    if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_4)){ inTexCoord4    = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute].xy;  iCurrentAttribute++;}
    if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_5)){ inTexCoord5    = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute].xy;  iCurrentAttribute++;}
    if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_6)){ inTexCoord6    = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute].xy;  iCurrentAttribute++;}
    if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_7)){ inTexCoord7    = tVertexBuffer.atVertexData[iVertexDataOffset + iCurrentAttribute].xy;  iCurrentAttribute++;}
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
    tShaderIn.tPosition = pos.xyz / pos.w;
    gl_Position = tGlobalInfo.tCameraViewProjection * pos;
    tShaderIn.tUV[0] = inTexCoord0;
    tShaderIn.tUV[1] = inTexCoord1;
    tShaderIn.tUV[2] = inTexCoord2;
    tShaderIn.tUV[3] = inTexCoord3;
    tShaderIn.tUV[4] = inTexCoord4;
    tShaderIn.tUV[5] = inTexCoord5;
    tShaderIn.tUV[6] = inTexCoord6;
    tShaderIn.tUV[7] = inTexCoord7;
    tShaderIn.tWorldPosition = gl_Position / gl_Position.w;
    tShaderIn.tColor = inColor0;
}