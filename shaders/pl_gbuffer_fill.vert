#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

/*
   gbuffer_fill.vert
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] specialization constants
// [SECTION] bind group 0
// [SECTION] bind group 1
// [SECTION] dynamic bind group
// [SECTION] input & output
// [SECTION] entry
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_bg_scene.inc"

//-----------------------------------------------------------------------------
// [SECTION] bind group 1
//-----------------------------------------------------------------------------

layout(set = 1, binding = 0) readonly buffer _plGlobalInfo
{
    plGpuViewData tData;
} tViewInfo;

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    plGpuDynData tData;
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
    vec2 tUV[2];
    vec4 tColor;
    vec3 tWorldNormal;
    mat3 tTBN;
} tShaderIn;

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void main() 
{

    const int iDataStride = 4;

    vec4 inPosition  = vec4(inPos, 1.0);

    const mat4 tTransform = tTransformBuffer.atTransform[gl_InstanceIndex];
    
    // offset = offset into current mesh + offset into global buffer
    const uint iVertexDataOffset = iDataStride * (gl_VertexIndex - tObjectInfo.tData.iVertexOffset) + tObjectInfo.tData.iDataOffset;

    vec3 inNormal = tVertexBuffer.atVertexData[iVertexDataOffset + 0].xyz;
    vec4 inTangent = tVertexBuffer.atVertexData[iVertexDataOffset + 1];
    vec2 inTexCoord0 = tVertexBuffer.atVertexData[iVertexDataOffset + 2].xy;
    vec2 inTexCoord1 = tVertexBuffer.atVertexData[iVertexDataOffset + 2].zw;
    vec4 inColor = tVertexBuffer.atVertexData[iVertexDataOffset + 3];

    tShaderIn.tWorldNormal = mat3(tTransform) * normalize(inNormal);

    vec3 tangent = normalize(inTangent.xyz);
    vec3 WorldTangent = mat3(tTransform) * tangent;
    vec3 WorldBitangent = cross(normalize(inNormal), tangent) * inTangent.w;
    WorldBitangent = mat3(tTransform) * WorldBitangent;
    tShaderIn.tTBN = mat3(WorldTangent, WorldBitangent, tShaderIn.tWorldNormal);

    vec4 pos = tTransform * inPosition;
    tShaderIn.tWorldPosition = pos.xyz / pos.w;
    gl_Position = tViewInfo.tData.tCameraViewProjection[tObjectInfo.tData.uGlobalIndex] * pos;
    tShaderIn.tUV[0] = inTexCoord0;
    tShaderIn.tUV[1] = inTexCoord1;
    tShaderIn.tViewPosition = gl_Position / gl_Position.w;
    tShaderIn.tColor = inColor;
}