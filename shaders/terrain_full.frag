#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "pl_shader_interop_terrain.h"
#include "terrain.glsl"

//-----------------------------------------------------------------------------
// [SECTION] input & output
//-----------------------------------------------------------------------------

layout(set = 0, binding = 0)  uniform sampler tSampler;
layout(set = 0, binding = 1)  uniform texture2D tHeightMap;
layout(set = 0, binding = 2)  uniform texture2D tNoiseTexture;
layout(set = 0, binding = 3)  uniform texture2D tDiffuseTexture;
layout(set = 0, binding = 4)  uniform sampler tMirrorSampler;
layout(set = 0, binding = 5)  uniform texture2D tHeightMap2;

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    plTerrainDynamicData tInfo;
} tObjectInfo;

layout(location = 0) out vec4 outColor;

// output
layout(location = 0) in struct plShaderIn {
    vec3 tPosition;
    vec2 tUV;
} tShaderIn;

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void main() 
{

    vec2 tTextureSize = textureSize(sampler2D(tHeightMap, tSampler), 0);
    vec2 heightfield_invSize = vec2(1.0 / tTextureSize.x, 1.0 / tTextureSize.y);
    float metersPerHeightFieldTexel = tObjectInfo.tInfo.fMetersPerHeightFieldTexel;
    float fMaxHeight = tObjectInfo.tInfo.fGlobalMaxHeight;
    float fMinHeight = tObjectInfo.tInfo.fGlobalMinHeight;
    float heightfieldTexelsPerMeter = 1.0 / metersPerHeightFieldTexel;

    float materialTilesPerMeter = 1.0;

    vec2 lowFreqTexCoord = tShaderIn.tPosition.xz * materialTilesPerMeter;
    float lowFreqNoiseValue1 = texture(sampler2D(tNoiseTexture, tMirrorSampler), lowFreqTexCoord * (1.0 / 32.0)).r;
    float lowFreqNoiseValue2 = texture(sampler2D(tNoiseTexture, tMirrorSampler), lowFreqTexCoord * (1.0 / 500.0)).r;

    vec4 heightfield_readMultiplyFirst = vec4(2.0, 2.0, 2.0, fMaxHeight - fMinHeight);
    vec4 heightfield_readAddSecond = vec4(-1.0, -1.0, -1.0, fMinHeight);

    vec2 UV = tShaderIn.tUV;

    vec4 temp0 = textureLod(sampler2D(tHeightMap, tMirrorSampler), UV, 0.0);
    vec4 temp = temp0 * heightfield_readMultiplyFirst + heightfield_readAddSecond;
    
    vec3 shadingPosition = vec3(tShaderIn.tPosition.x, temp.a, tShaderIn.tPosition.z);
    vec3 shadingNormal = normalize(temp.xyz);

    // Weights are stored in the z channel of the texture coordinates
    vec3 sunlightColor = vec3(1.0, 1.0, 1.0);

    vec3 cameraPos = tObjectInfo.tInfo.tPos.xyz;
    vec3 w_o = normalize(cameraPos - shadingPosition); 
    vec3 w_i = normalize(-tObjectInfo.tInfo.tSunDirection.xyz);
    vec3 w_h = normalize(w_i + w_o);

    // vec3 diffuse = vec3(0.9);
    // vec3 diffuse = vec3(shadingPosition.y / (fMaxHeight - fMinHeight), 0.5, 0.5);
    vec3 diffuse = texture(sampler2D(tDiffuseTexture, tMirrorSampler), UV * 1000.0).rgb * 1.5;

    // vec3 ambient = vec3(0.1, 0.1, 0.1);
    vec3 ambient = vec3(0);

    // diffuse *= min(1.2,
    //             (noise(lowFreqTexCoord * 0.2, 2) * 0.5 + 0.8) *
    //             (texture(sampler2D(tNoiseTexture, tMirrorSampler), lowFreqTexCoord * (1.0 / 4.5)).r   * 0.9 + 0.6) *
    //             (lowFreqNoiseValue1 * 0.8 + 0.6) *
    //             (lowFreqNoiseValue2 * 0.8 + 0.6));

    outColor.xyz = diffuse * (max(0.0, dot(shadingNormal, w_i)) * sunlightColor + ambient) +
            sunlightColor * (0.1 * pow(max(0.0, dot(shadingNormal, w_h)), 6));


    // outColor.xyz = diffuse;
    outColor.a = 1.0;

    // if(length(shadingPosition.xz - tObjectInfo.tInfo.tPos.xz) < tObjectInfo.tInfo.fStencilRadius * 0.5)
    //     discard;
    // outColor = vec4(UV, 0, 1.0); 
}