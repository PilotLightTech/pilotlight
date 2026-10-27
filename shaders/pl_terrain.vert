#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "pl_shader_interop_terrain.h"
#include "pl_bg_scene.inc"

layout(set = 1, binding = 0) readonly buffer _plGlobalInfo
{
    plGpuViewData tData;
} tViewInfo;

vec3
Decode( vec2 f )
{
    f = f * 2.0 - 1.0;
 
    // https://twitter.com/Stubbesaurus/status/937994790553227264
    vec3 n = vec3( f.x, f.y, 1.0 - abs( f.x ) - abs( f.y ) );
    float t = max( -n.z, 0.0 );
    n.x += n.x >= 0.0 ? -t : t;
    n.y += n.y >= 0.0 ? -t : t;
    return normalize( n );
}

// input
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inNormal;
layout(location = 2) in vec2 inUV;

// output
layout(location = 0) out struct plShaderOut {
    vec4 tColor;
    vec3 tWorldPosition;
    vec3 tWorldNormal;
    vec2 tUV[2];
    mat3 tTBN;
} tShaderOut;

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    plGpuDynTerrainData tData;
} tDynamicData;

void main() 
{
    gl_Position = tViewInfo.tData.tCameraViewProjection[tDynamicData.tData.uGlobalIndex] * vec4(inPos, 1.0);

    tShaderOut.tWorldPosition = inPos;
    tShaderOut.tWorldNormal = Decode(inNormal);
    float fUVFactor = float(1 << tDynamicData.tData.iLevel);
    vec2 tUVFactor = vec2(fUVFactor, fUVFactor);
    tShaderOut.tUV[0] = inUV * tUVFactor;
    tShaderOut.tUV[1] = inUV * tUVFactor;

    tShaderOut.tColor.rgb = tDynamicData.tData.tDebugColor.rgb;
    tShaderOut.tColor.a = 1.0;

    // vec3 tangent = normalize(inTangent.xyz);
    // vec3 WorldTangent = mat3(tTransform) * tangent;
    // vec3 WorldBitangent = cross(normalize(inNormal), tangent) * inTangent.w;
    // WorldBitangent = mat3(tTransform) * WorldBitangent;
    // tShaderIn.tTBN = mat3(WorldTangent, WorldBitangent, tShaderIn.tWorldNormal);

    // Tangent follows the terrain's +X direction
    vec3 T = normalize(vec3(tShaderOut.tWorldNormal.y, -tShaderOut.tWorldNormal.x, 0.0));

    // Bitangent follows the terrain's +Z direction
    vec3 B = normalize(-cross(tShaderOut.tWorldNormal, T));

    // Construct world-space TBN
    tShaderOut.tTBN = mat3(T, B, tShaderOut.tWorldNormal);
}