#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#include "pl_math.glsl"
#include "pl_shader_interop_terrain.h"
#include "pl_bg_scene.inc"
#include "pl_bg_view.inc"

//-----------------------------------------------------------------------------
// [SECTION] specialication constants
//-----------------------------------------------------------------------------

layout(constant_id = 0) const int tShaderDebugMode = 0;

//-----------------------------------------------------------------------------
// [SECTION] input & output
//-----------------------------------------------------------------------------

// input
layout(location = 0) in struct plShaderIn {
    vec4 tColor;
    vec3 tWorldPosition;
    vec3 tWorldNormal;
    vec2 tUV[2];
    mat3 tTBN;
} tShaderIn;

// output
layout(location = 0) out vec4 outFinal;
layout(location = 1) out vec4 outAlbedo;
layout(location = 2) out vec2 outNormal;
layout(location = 3) out vec4 outAOMetalnessRoughness;

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    plGpuDynTerrainData tData;
} tObjectInfo;

float
terrain_zone_weight(float height, float minH, float maxH, float blendSize)
{
    float fadeIn  = smoothstep(minH - blendSize, minH + blendSize, height);
    float fadeOut = 1.0 - smoothstep(maxH - blendSize, maxH + blendSize, height);
    return fadeIn * fadeOut;
}

vec4 terrain_sample_triplanar(
    sampler2D tex,
    vec3 position,
    vec3 normal,
    float scale)
{
    vec3 blend = pow(abs(normal), vec3(4.0));
    blend /= max(blend.x + blend.y + blend.z, 0.0001);

    vec4 x = texture(tex, position.yz * scale);
    vec4 y = texture(tex, position.xz * scale);
    vec4 z = texture(tex, position.xy * scale);

    return x * blend.x +
           y * blend.y +
           z * blend.z;
}

#define PL_INCLUDE_MATERIAL_FUNCTIONS
#include "pl_material_info.glsl"

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void main() 
{

    // vec3 dx = dFdx(tShaderIn.tWorldPosition);
    // vec3 dy = dFdy(tShaderIn.tWorldPosition);
    // vec3 normal = normalize(cross(dy, dx));

    // vec2 tUVActual = tShaderIn.tUV[0];
    // tUVActual = tUVActual * tObjectInfo.tData.tUVInfo.xy;
    // tUVActual = tUVActual + tObjectInfo.tData.tUVInfo.zw;

    vec3 normal = normalize(tShaderIn.tWorldNormal);
    // vec4 diffuse = pl_srgb_to_linear(texture(sampler2D(at2DTextures[tObjectInfo.tData.uTextureIndex], tSamplerLinearRepeat), tUVActual));

    float height = tShaderIn.tWorldPosition.y;

    // float noise = terrain_noise(
    //     tShaderIn.tWorldPosition.xz / 200.0
    // );

    // height += (noise - 0.5) * 100.0;

    float upness = clamp(dot(normal, vec3(0.0, 1.0, 0.0)), 0.0, 1.0);
    float steepness = 1.0 - upness;
    float slopeBlend = smoothstep(tObjectInfo.tData.fSlopeStart, tObjectInfo.tData.fSlopeEnd, steepness);

    float weights[5] = float[5](0.0, 0.0, 0.0, 0.0, 0.0);

    float wsum = 0.0;
    for(int i = 0; i < tObjectInfo.tData.iZoneCount; i++)
    {
        weights[i] = terrain_zone_weight(height,
            tObjectInfo.tData.atElevationZones[i].tElevation.x,
            tObjectInfo.tData.atElevationZones[i].tElevation.y,
            tObjectInfo.tData.atElevationZones[i].tElevation.z);
        wsum += weights[i];
    }

    wsum = max(wsum, 0.0001);

    for(int i = 0; i < tObjectInfo.tData.iZoneCount; i++)
    {
        weights[i] /= wsum;
    }


    normal = vec3(0);
    outAlbedo = vec4(0);
    vec4 tBaseColor = vec4(0);
    outAOMetalnessRoughness = vec4(0);
    for(int i = 0; i < tObjectInfo.tData.iZoneCount; i++)
    {
        plGpuMaterial tFlatMaterial = tMaterialInfo.atMaterials[tObjectInfo.tData.atElevationZones[i].iFlatMaterialIndex];
        plGpuMaterial tSteepMaterial = tMaterialInfo.atMaterials[tObjectInfo.tData.atElevationZones[i].iSteepMaterialIndex];

        vec4 tFlatBaseColor = getBaseColor(tFlatMaterial.tBaseColorFactor, tFlatMaterial.aiTextureUVSet[PL_TEXTURE_BASE_COLOR], tObjectInfo.tData.atElevationZones[i].iFlatMaterialIndex);
        vec4 tSteepBaseColor = getBaseColor(tSteepMaterial.tBaseColorFactor, tSteepMaterial.aiTextureUVSet[PL_TEXTURE_BASE_COLOR], tObjectInfo.tData.atElevationZones[i].iSteepMaterialIndex);

        NormalInfo tFlatNormalInfo = pl_get_normal_info(tObjectInfo.tData.atElevationZones[i].iFlatMaterialIndex);
        NormalInfo tStepNormalInfo = pl_get_normal_info(tObjectInfo.tData.atElevationZones[i].iSteepMaterialIndex);

        MaterialInfo flatmaterialInfo;
        flatmaterialInfo.f0_dielectric = vec3(0.04);
        flatmaterialInfo.baseColor = tFlatBaseColor.rgb;
        flatmaterialInfo = getMetallicRoughnessInfo(flatmaterialInfo, tFlatMaterial.fMetallicFactor, tFlatMaterial.fRoughnessFactor, tObjectInfo.tData.atElevationZones[i].iFlatMaterialIndex);
        flatmaterialInfo.perceptualRoughness = clamp(flatmaterialInfo.perceptualRoughness, 0.0, 1.0);
        flatmaterialInfo.metallic = clamp(flatmaterialInfo.metallic, 0.0, 1.0);

        MaterialInfo steepmaterialInfo;
        steepmaterialInfo.f0_dielectric = vec3(0.04);
        steepmaterialInfo.baseColor = tSteepBaseColor.rgb;
        steepmaterialInfo = getMetallicRoughnessInfo(steepmaterialInfo, tSteepMaterial.fMetallicFactor, tSteepMaterial.fRoughnessFactor, tObjectInfo.tData.atElevationZones[i].iSteepMaterialIndex);
        steepmaterialInfo.perceptualRoughness = clamp(steepmaterialInfo.perceptualRoughness, 0.0, 1.0);
        steepmaterialInfo.metallic = clamp(steepmaterialInfo.metallic, 0.0, 1.0);

        vec4 zoneColor = mix(tFlatBaseColor, tSteepBaseColor, slopeBlend);
        vec3 zoneNormal = normalize(mix(tFlatNormalInfo.n, tStepNormalInfo.n, slopeBlend));
        vec4 AoMetalnessRoughness = mix(
            vec4(1.0, flatmaterialInfo.metallic, flatmaterialInfo.perceptualRoughness, 1.0),
            vec4(1.0, steepmaterialInfo.metallic, steepmaterialInfo.perceptualRoughness, 1.0),
            slopeBlend);

        tBaseColor += zoneColor * weights[i];
        normal += zoneNormal * weights[i];
        outAOMetalnessRoughness += AoMetalnessRoughness * weights[i];
    }

    normal = normalize(normal);
    outAlbedo = tBaseColor;
    outNormal = vec2(0);
    outFinal = vec4(0);
    outAOMetalnessRoughness.r = clamp(outAOMetalnessRoughness.r, 0.0, 1.0);
    outAOMetalnessRoughness.g = clamp(outAOMetalnessRoughness.g, 0.0, 1.0);
    outAOMetalnessRoughness.b = clamp(outAOMetalnessRoughness.b, 0.0, 1.0);
    outAOMetalnessRoughness.a = clamp(outAOMetalnessRoughness.a, 0.0, 1.0);

    // In case of missing data for a debug view, render a checkerboard.
    if(tShaderDebugMode != PL_SHADER_DEBUG_BASE_COLOR)
    {
        outAlbedo = vec4(1.0);
        {
            float frequency = 0.02;
            float gray = 0.9;

            vec2 v1 = step(0.5, fract(frequency * gl_FragCoord.xy));
            vec2 v2 = step(0.5, vec2(1.0) - fract(frequency * gl_FragCoord.xy));
            outAlbedo.rgb *= gray + v1.x * v1.y + v2.x * v2.y;
        }
    }
    outAlbedo.a = 1.0;

    if(tShaderDebugMode == PL_SHADER_DEBUG_BASE_COLOR)
    {
        outAlbedo = tBaseColor;
    }

    if(tShaderDebugMode == PL_SHADER_DEBUG_SHADING_NORMAL)
    {
        outAlbedo.rgb = vec3((1.0 + normal) / 2.0);
    }

    if(tShaderDebugMode == PL_SHADER_DEBUG_METALLIC)
    {
        outAlbedo.rgb = vec3(outAOMetalnessRoughness.g);
    }

    if(tShaderDebugMode == PL_SHADER_DEBUG_ROUGHNESS)
    {
        outAlbedo.rgb = vec3(outAOMetalnessRoughness.b);
    }

    if(tShaderDebugMode == PL_SHADER_DEBUG_ALPHA)
    {
        outAlbedo.rgb = vec3(tBaseColor.a);
    }

    if(tShaderDebugMode == PL_SHADER_DEBUG_OCCLUSION)
    {
        outAlbedo.rgb = vec3(outAOMetalnessRoughness.r);
    }

    if(tShaderDebugMode == PL_SHADER_DEBUG_UV0)
    {
        outAlbedo.rgb = vec3(tShaderIn.tUV[0], 0.0);
    }

    if(tShaderDebugMode == PL_SHADER_DEBUG_GEOMETRY_NORMAL)
    {
        outAlbedo.rgb = vec3((1.0 + normal) / 2.0);
    }

    // if(tShaderDebugMode == PL_SHADER_DEBUG_GEOMETRY_TANGENT)
    // {
    //     outAlbedo.rgb = vec3((1.0 + tNormalInfo.t) / 2.0);
    // }

    // if(tShaderDebugMode == PL_SHADER_DEBUG_GEOMETRY_BITANGENT)
    // {
    //     outAlbedo.rgb = vec3((1.0 + tNormalInfo.b) / 2.0);
    // }

    // if(tShaderDebugMode == PL_SHADER_DEBUG_TEXTURE_NORMAL)
    // {
    //     if(bool(material.iTextureMappingFlags & PL_HAS_NORMAL_MAP))
    //     {
    //         outAlbedo.rgb = (tNormalInfo.ntex + 1.0) / 2.0;
    //     }
    // }
}