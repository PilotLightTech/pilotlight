#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "pl_bg_scene.inc"
#include "pl_bg_view.inc"
#include "pl_water.glsl"

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
layout(location = 0) out plShaderOut {
    vec3 tWorldPosition;
    vec3 tViewPosition;
    vec2 tUV[2];
    vec4 tColor;
    vec3 tWorldNormal;
    mat3 tTBN;
    mat4 tModel; 
    flat uvec4 tSelectedProbes;
    flat uint uProbeSelection;
    vec2 tWaterPosition; // undisplaced world XZ for wave normals
} tShaderIn;

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void main()
{

    tShaderIn.uProbeSelection = tTransformBuffer.atTransform[gl_InstanceIndex].uProbeSelection;
    plGpuProbeSelection probes = tProbeSelections.atData[tShaderIn.uProbeSelection & ~PL_PROBE_SELECTION_FALLBACK_FLAG];
    tShaderIn.tSelectedProbes = uvec4(probes.iProbe0, probes.iProbe1, probes.iProbe2, probes.iCount);
    const int iDataStride = 4;

    vec4 inPosition  = vec4(inPos, 1.0);
    const mat4 tTransform = tTransformBuffer.atTransform[gl_InstanceIndex].tTransform;
    
    // offset = offset into current mesh + offset into global buffer
    const uint iVertexDataOffset = iDataStride * (gl_VertexIndex - tObjectInfo.tData.iVertexOffset) + tObjectInfo.tData.iDataOffset;

    vec3 inNormal = tVertexBuffer.atVertexData[iVertexDataOffset + 0].xyz;
    vec4 inTangent = tVertexBuffer.atVertexData[iVertexDataOffset + 1];
    vec2 inTexCoord0 = tVertexBuffer.atVertexData[iVertexDataOffset + 2].xy;
    vec2 inTexCoord1 = tVertexBuffer.atVertexData[iVertexDataOffset + 2].zw;
    vec4 inColor = tVertexBuffer.atVertexData[iVertexDataOffset + 3];

    tShaderIn.tWorldNormal = normalize((tTransform * vec4(normalize(inNormal), 0.0)).xyz);

    vec3 tangent = normalize(inTangent.xyz);
    vec3 WorldTangent = mat3(tTransform) * tangent;
    vec3 WorldBitangent = cross(normalize(inNormal), tangent) * inTangent.w;
    WorldBitangent = mat3(tTransform) * WorldBitangent;
    tShaderIn.tTBN = mat3(WorldTangent, WorldBitangent, tShaderIn.tWorldNormal);


    vec4 pos = tTransform * inPosition;
    tShaderIn.tWaterPosition = pos.xz / pos.w;
    plGpuMaterial material = tMaterialInfo.atMaterials[tObjectInfo.tData.iMaterialIndex];
    if(bool(material.iMaterialFlags & PL_MATERIAL_SHADER_FLAG_WATER))
        pos.xyz += pl_water_displacement(tShaderIn.tWaterPosition, tGpuScene.tData.g_time, material.tWater) * pos.w;
    tShaderIn.tWorldPosition = pos.xyz / pos.w;
    gl_Position = tViewInfo.tData.tCameraViewProjection[tObjectInfo.tData.uGlobalIndex] * pos;
    tShaderIn.tUV[0] = inTexCoord0;
    tShaderIn.tUV[1] = inTexCoord1;
    tShaderIn.tColor = inColor;
    tShaderIn.tModel = tTransform;
    tShaderIn.tViewPosition = (pos * tViewInfo.tData.tCameraView[tObjectInfo.tData.uGlobalIndex]).xyz;
}
