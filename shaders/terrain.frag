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
    plGeoClipMapDynamicData tInfo;
} tObjectInfo;

layout(location = 0) out vec4 outColor;

// output
layout(location = 0) in struct plShaderIn {
    vec3 tPosition;
    float fGridLevel;
} tShaderIn;


void computeTriplanar(in vec3 pos, in vec3 normal, out vec3 texCoordNorthSouth, out vec3 texCoordFlat, out vec3 texCoordEastWest) {
	// Calculate weights for blending
	vec3 weights = vec3(abs(normal.x), max(0.0, abs(normal.y) - 0.55), abs(normal.z));
	weights.x = pow(weights.x, 64);
	weights.y = pow(weights.y, 64);
	weights.z = pow(weights.z, 64);
	weights *= 1.0 / (weights.x + weights.y + weights.z);				
		
	// Perform the uv projection on to each plane: these are the texture coordinates
	texCoordNorthSouth  = vec3(pos.z, sign(normal.x) * 2.0 * pos.x, weights.x);
	texCoordFlat        = vec3(pos.x, pos.z, weights.y);
	texCoordEastWest    = vec3(pos.x, sign(normal.z) * 2.0 * pos.z, weights.z);
}

vec3 readTextures(vec3 texCoord)
{
    // We have to compute gradients outside of the branches, since within
    // a pixel-quad any given branch may be ignored.
    vec2 gradX = dFdx(texCoord.xy);
    vec2 gradY = dFdy(texCoord.xy);

    vec3 value =  textureGrad(sampler2D(tDiffuseTexture, tMirrorSampler), texCoord.xy, gradX, gradY).rgb;

    return value;
}

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

    float materialTilesPerMeter = 10.0;

    vec2 lowFreqTexCoord = tShaderIn.tPosition.xz * materialTilesPerMeter;
    float lowFreqNoiseValue1 = texture(sampler2D(tNoiseTexture, tMirrorSampler), lowFreqTexCoord * (1.0 / 32.0)).r;
    float lowFreqNoiseValue2 = texture(sampler2D(tNoiseTexture, tMirrorSampler), lowFreqTexCoord * (1.0 / 500.0)).r;

    vec4 heightfield_readMultiplyFirst = vec4(2.0, 2.0, 2.0, fMaxHeight - fMinHeight);
    vec4 heightfield_readAddSecond = vec4(-1.0, -1.0, -1.0, fMinHeight);

    vec2 UV = (tShaderIn.tPosition.xz * heightfieldTexelsPerMeter + 0.5) * heightfield_invSize.xy;

    UV.x += 0.5;
    UV.y += 0.5; // 225 / 4127
    // UV.y += 0.455; // 225 / 4127

    // vec2 UV = (tShaderIn.tPosition.xz * heightfieldTexelsPerMeter + 0.5) * heightfield_invSize.xy;

    vec4 temp0 = textureLod(sampler2D(tHeightMap, tMirrorSampler), UV, 0.0);
    vec4 temp = temp0 * heightfield_readMultiplyFirst + heightfield_readAddSecond;



    // TODO: handle properly

    // if(tObjectInfo.tInfo.tPos.y > fMaxHeight * 3)
    // {
    //     vec2 inUV = tShaderIn.tPosition.xz - tObjectInfo.tInfo.tMinMax.xy;
    //     inUV.x /= (tObjectInfo.tInfo.tMinMax.z - tObjectInfo.tInfo.tMinMax.x);
    //     inUV.y /= (tObjectInfo.tInfo.tMinMax.w - tObjectInfo.tInfo.tMinMax.y);

    //     temp0 = textureLod(sampler2D(tHeightMap2, tMirrorSampler), inUV, 0.0);
    //     vec4 temp2 = temp0 * heightfield_readMultiplyFirst + heightfield_readAddSecond;

    //     float fRatio0 = (tObjectInfo.tInfo.tPos.y - tObjectInfo.tInfo.fGlobalMaxHeight) / 10000.0;
    //     // float fRatio0 = 0.0;
    //     float fRatio1 = 0.5 * abs(length(tShaderIn.tPosition.xz - tObjectInfo.tInfo.tPos.xz)) / tObjectInfo.tInfo.fBlurRadius;
    //     float fRatio = clamp(fRatio0 + fRatio1, 0.0, 1.0);
    //     temp = temp * (1.0 - fRatio) + temp2 * fRatio;
    // }
    
    vec3 shadingPosition = vec3(tShaderIn.tPosition.x, temp.a, tShaderIn.tPosition.z);
    vec3 shadingNormal = normalize(temp.xyz);

    // if(tShaderIn.tPosition.y < tObjectInfo.tInfo.fGlobalMinHeight)
    //     discard;

    // Weights are stored in the z channel of the texture coordinates
    // vec3 texCoordNorthSouth, texCoordFlat, texCoordEastWest;
    // computeTriplanar(shadingPosition * materialTilesPerMeter, shadingNormal + vec3(0,(lowFreqNoiseValue1 + lowFreqNoiseValue2 - 0.2) * 0.5,0), texCoordNorthSouth, texCoordFlat, texCoordEastWest);

    vec3 sunlightColor = vec3(1.0, 1.0, 1.0);

    vec3 cameraPos = tObjectInfo.tInfo.tPos.xyz;
    vec3 w_o = normalize(cameraPos - shadingPosition); 
    vec3 w_i = normalize(-tObjectInfo.tInfo.tSunDirection.xyz);
    vec3 w_h = normalize(w_i + w_o);

    // vec3 diffuse = vec3(0.9);

    vec3 diffuse = texture(sampler2D(tDiffuseTexture, tMirrorSampler), UV * 1000.0).rgb * 1.5;

    // vec3 diffuse = vec3(shadingPosition.y / (fMaxHeight - fMinHeight), 0.5, 0.5);
    // vec3 diffuse = readTextures(texCoordFlat) * 1.5;
    // 
    // vec3 diffuse = texture(sampler2D(tDiffuseTexture, tMirrorSampler), texCoordFlat.xy).rgb;
    // vec3 diffuse = readTextures(texCoordFlat);
    // if(tShaderIn.fGridLevel < 3.0)
    // {
    //     diffuse = readTextures(texCoordFlat);
    // }
    
    // vec3 ambient = vec3(0.01, 0.01, 0.01);
    vec3 ambient = vec3(0);

    diffuse *= min(1.2,
                (noise(lowFreqTexCoord * 0.2, 2) * 0.5 + 0.8) *
                (texture(sampler2D(tNoiseTexture, tMirrorSampler), lowFreqTexCoord * (1.0 / 4.5)).r   * 0.9 + 0.6) *
                (lowFreqNoiseValue1 * 0.8 + 0.6) *
                (lowFreqNoiseValue2 * 0.8 + 0.6));

    outColor.xyz = diffuse * (max(0.0, dot(shadingNormal, w_i)) * sunlightColor + ambient) +
            sunlightColor * (0.1 * pow(max(0.0, dot(shadingNormal, w_h)), 6));

    outColor.a = 1.0;
    // outColor.a = 1.0 * clamp((cameraPos.y - tObjectInfo.tInfo.fGlobalMaxHeight) / (20000.0), 0.0, 1.0);
    // outColor.a = 1.0 - 1.0 * clamp((cameraPos.y - tObjectInfo.tInfo.fGlobalMaxHeight) / (20000.0), 0.0, 1.0);
    // if(outColor.a < 0.25)
    //     discard;

    // TODO: handle properly
    // if(length(shadingPosition.xz - tObjectInfo.tInfo.tPos.xz) > tObjectInfo.tInfo.fStencilRadius * 0.6)
    //     discard;

    // outColor.rgb *= outColor.a;
    // outColor.a = 0.5;
    // outColor = vec4(shadingNormal, 1.0); 
}