#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#include "pl_math.glsl"
#include "pl_shader_interop_terrain.h"
#include "pl_bg_scene.inc"
#include "pl_bg_view.inc"


//-----------------------------------------------------------------------------
// [SECTION] input & output
//-----------------------------------------------------------------------------

// input
layout(location = 0) in struct plShaderIn {
    vec4 tColor;
    vec3 tWorldPosition;
    vec3 tWorldNormal;
    vec2 tUV;
} tShaderIn;

// output
layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec2 outNormal;
layout(location = 2) out vec4 outAOMetalnessRoughness;

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    plGpuDynTerrainData tData;
} tDynamicData;

float
terrain_zone_weight(float height, float minH, float maxH, float blendSize)
{
    float fadeIn  = smoothstep(minH - blendSize, minH + blendSize, height);
    float fadeOut = 1.0 - smoothstep(maxH - blendSize, maxH + blendSize, height);
    return fadeIn * fadeOut;
}

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void main() 
{

    // vec3 dx = dFdx(tShaderIn.tWorldPosition);
    // vec3 dy = dFdy(tShaderIn.tWorldPosition);
    // vec3 normal = normalize(cross(dy, dx));

    vec2 tUVActual = tShaderIn.tUV;
    tUVActual = tUVActual * tDynamicData.tData.tUVInfo.xy;
    tUVActual = tUVActual + tDynamicData.tData.tUVInfo.zw;

    vec3 normal = normalize(tShaderIn.tWorldNormal);
    // vec4 diffuse = pl_srgb_to_linear(texture(sampler2D(at2DTextures[tDynamicData.tData.uTextureIndex], tSamplerLinearRepeat), tUVActual));

    float height = tShaderIn.tWorldPosition.y;

    float upness = clamp(dot(normal, vec3(0.0, 1.0, 0.0)), 0.0, 1.0);
    float steepness = 1.0 - upness;
    // float slopeBlend = smoothstep(0.0, 0.45, steepness);
    float slopeBlend = smoothstep(tDynamicData.tData.fSlopeStart, tDynamicData.tData.fSlopeEnd, steepness);

    // float w0 = terrain_zone_weight(height, -1000.0, 20.0, 30.0);
    float w0 = terrain_zone_weight(height, tDynamicData.tData.atElevationZones[0].tElevation.x, tDynamicData.tData.atElevationZones[0].tElevation.y, tDynamicData.tData.atElevationZones[0].tElevation.z);
    float w1 = terrain_zone_weight(height, tDynamicData.tData.atElevationZones[1].tElevation.x, tDynamicData.tData.atElevationZones[1].tElevation.y, tDynamicData.tData.atElevationZones[1].tElevation.z);
    float w2 = terrain_zone_weight(height, tDynamicData.tData.atElevationZones[2].tElevation.x, tDynamicData.tData.atElevationZones[2].tElevation.y, tDynamicData.tData.atElevationZones[2].tElevation.z);
    // float w1 = terrain_zone_weight(height,    20.0, 1000.0, 40.0);
    // float w2 = terrain_zone_weight(height,   950.0, 2000.0, 60.0);

    float wsum = max(w0 + w1 + w2, 0.0001);
    w0 /= wsum;
    w1 /= wsum;
    w2 /= wsum;

    vec4 zone0 = mix(
        tDynamicData.tData.atElevationZones[0].tFlatMaterial.tBaseColor,
        tDynamicData.tData.atElevationZones[0].tSteepMaterial.tBaseColor,
        slopeBlend);

    vec4 zone1 = mix(
        tDynamicData.tData.atElevationZones[1].tFlatMaterial.tBaseColor,
        tDynamicData.tData.atElevationZones[1].tSteepMaterial.tBaseColor,
        slopeBlend);

    vec4 zone2 = mix(
        tDynamicData.tData.atElevationZones[2].tFlatMaterial.tBaseColor,
        tDynamicData.tData.atElevationZones[2].tSteepMaterial.tBaseColor,
        slopeBlend);

    // vec4 zone0 = mix(
    //     vec4(0.66, 0.598, 0.402, 1.0),
    //     vec4(0.20, 0.18, 0.16, 1.0),
    //     slopeBlend);

    // vec4 zone1 = mix(
    //     vec4(0.05, 0.30, 0.05, 1.0),
    //     vec4(0.08, 0.07, 0.06, 1.0),
    //     slopeBlend);

    // vec4 zone2 = mix(
    //     vec4(0.85, 0.85, 0.82, 1.0),
    //     vec4(0.45, 0.45, 0.43, 1.0),
    //     slopeBlend);

    outAlbedo = zone0 * w0 + zone1 * w1 + zone2 * w2;

    outNormal = Encode(normal);
    outAOMetalnessRoughness = vec4(1.0, 1.0, 1.0, 1.0);


    if(bool(tDynamicData.tData.tFlags & PL_TERRAIN_SHADER_FLAGS_SHOW_LEVELS))
    {
        // outColor = tShaderIn.tColor;
        outAlbedo = tShaderIn.tColor;
        // outAlbedo += tShaderIn.tColor;
    }

    if(bool(tDynamicData.tData.tFlags & PL_TERRAIN_SHADER_FLAGS_WIREFRAME))
    {
        outAlbedo = tShaderIn.tColor;
    }

}