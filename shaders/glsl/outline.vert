#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "common.glsl"

// input
layout(location = 0) in vec3 inPos;

void main() 
{

    vec3 inPosition  = inPos;
    vec3 inNormal    = vec3(0.0, 0.0, 0.0);

    const mat4 tMVP = tGlobalInfo.tCameraViewProj * tObjectInfo.tModel;

    int iCurrentAttribute = 0;

    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_POSITION))  { inPosition  = tVertexBuffer.atVertexData[VertexStride * (gl_VertexIndex - tObjectInfo.uVertexOffset) + tObjectInfo.uVertexDataOffset + iCurrentAttribute].xyz; iCurrentAttribute++;}
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_NORMAL))    { inNormal    = tVertexBuffer.atVertexData[VertexStride * (gl_VertexIndex - tObjectInfo.uVertexOffset) + tObjectInfo.uVertexDataOffset + iCurrentAttribute].xyz; iCurrentAttribute++;}

    vec4 tPos = tMVP * vec4(inPosition, 1.0);
    vec4 tNorm = normalize(tMVP * vec4(inNormal, 0.0));
    tPos = vec4(tPos.xyz + tNorm.xyz * 0.02, tPos.w);
    gl_Position = tPos;

}