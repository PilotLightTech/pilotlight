#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "defines.glsl"
#include "pl_shader_interop_renderer.h"
#include "lights.glsl"
#include "math.glsl"

//-----------------------------------------------------------------------------
// [SECTION] specialication constants
//-----------------------------------------------------------------------------

layout(constant_id = 0) const int iRenderingFlags = 0;
layout(constant_id = 1) const int iLightCount = 0;
layout(constant_id = 2) const int iProbeCount = 0;

//-----------------------------------------------------------------------------
// [SECTION] bind group 0
//-----------------------------------------------------------------------------

layout(std140, set = 0, binding = 0) readonly buffer _tVertexBuffer
{
	vec4 atVertexData[];
} tVertexBuffer;

layout(std140, set = 0, binding = 1) readonly buffer _tTransformBuffer
{
	mat4 atTransform[];
} tTransformBuffer;

layout(set = 0, binding = 2) readonly buffer plMaterialInfo
{
    plGpuMaterial atMaterials[];
} tMaterialInfo;

layout(set = 0, binding = 3)  uniform sampler tDefaultSampler;
layout(set = 0, binding = 4)  uniform sampler tEnvSampler;
layout(set = 0, binding = 5)  uniform texture2D at2DTextures[PL_MAX_BINDLESS_TEXTURES];
layout(set = 0, binding = PL_MAX_BINDLESS_CUBE_TEXTURE_SLOT)  uniform textureCube atCubeTextures[PL_MAX_BINDLESS_TEXTURES];
//-----------------------------------------------------------------------------
// [SECTION] bind group 1
//-----------------------------------------------------------------------------

layout(input_attachment_index = 1, set = 1, binding = 0)  uniform subpassInput tAlbedoSampler;
layout(input_attachment_index = 2, set = 1, binding = 1)  uniform subpassInput tNormalTexture;
layout(input_attachment_index = 3, set = 1, binding = 2)  uniform subpassInput tAOMetalRoughnessTexture;
layout(input_attachment_index = 0, set = 1, binding = 3)  uniform subpassInput tDepthSampler;

//-----------------------------------------------------------------------------
// [SECTION] bind group 2
//-----------------------------------------------------------------------------

layout(set = 2, binding = 0) readonly buffer _plGlobalInfo
{
    plGpuGlobalData data[];
} tGlobalInfo;

layout(set = 2, binding = 1) uniform _plLightInfo
{
    plGpuLight atData[1];
} tLightInfo;

layout(set = 2, binding = 2) readonly buffer plDShadowData
{
    plGpuLightShadow atData[];
} tDShadowData;

layout(set = 2, binding = 3) readonly buffer plShadowData
{
    plGpuLightShadow atData[];
} tShadowData;

layout(set = 2, binding = 4) readonly buffer plProbeData
{
    plGpuProbe atData[];
} tProbeData;

layout(set = 2, binding = 5)  uniform sampler tShadowSampler;

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    plGpuDynDeferredLighting tData;
} tObjectInfo;

//-----------------------------------------------------------------------------
// [SECTION] input & output
//-----------------------------------------------------------------------------

layout(location = 0) out vec4 outColor;

// output
layout(location = 0) in struct plShaderIn {
    vec2 tUV;
} tShaderIn;

vec3
getDiffuseLight(vec3 n, int iProbeIndex)
{
    // n.z = -n.z; // uncomment if not reverse z
    // return texture(samplerCube(u_LambertianEnvSampler, tEnvSampler), n).rgb;
    return texture(samplerCube(atCubeTextures[nonuniformEXT(tProbeData.atData[iProbeIndex].uLambertianEnvSampler)], tEnvSampler), n).rgb;
}

vec4
getSpecularSample(vec3 reflection, float lod, int iProbeIndex)
{
    // reflection.z = -reflection.z; // uncomment if not reverse z
    // reflection.x = -reflection.x; // uncomment if not reverse z
    // return textureLod(samplerCube(u_GGXEnvSampler, tEnvSampler), reflection, lod);
    return textureLod(samplerCube(atCubeTextures[nonuniformEXT(tProbeData.atData[iProbeIndex].uGGXEnvSampler)], tEnvSampler), reflection, lod);
}

vec3
getIBLRadianceGGX(vec3 n, vec3 v, float roughness, vec3 F0, float specularWeight, int u_MipCount, int iProbeIndex, vec3 tWorldPos)
{
    
    float lod = roughness * float(u_MipCount - 1);
    vec3 reflection = normalize(reflect(-v, n));

    if(bool(tProbeData.atData[iProbeIndex].iParallaxCorrection))
    {

        // Find the ray intersection with box plane
        vec3 FirstPlaneIntersect = (tProbeData.atData[iProbeIndex].tMax.xyz - tWorldPos) / reflection;
        vec3 SecondPlaneIntersect = (tProbeData.atData[iProbeIndex].tMin.xyz - tWorldPos) / reflection;
        
        // Get the furthest of these intersections along the ray
        // (Ok because x/0 give +inf and -x/0 give –inf )
        vec3 FurthestPlane = max(FirstPlaneIntersect, SecondPlaneIntersect);

        // Find the closest far intersection
        float Distance = min(min(FurthestPlane.x, FurthestPlane.y), FurthestPlane.z);

        // Get the intersection position
        vec3 IntersectPositionWS = tWorldPos + reflection * Distance;
        
        // Get corrected reflection
        reflection = IntersectPositionWS - tProbeData.atData[iProbeIndex].tPosition;
    }
    
    // End parallax-correction code

    vec4 specularSample = getSpecularSample(reflection, lod, iProbeIndex);

    float NdotV = clampedDot(n, v);
    vec2 brdfSamplePoint = clamp(vec2(NdotV, roughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
    // vec2 f_ab = texture(sampler2D(u_GGXLUT, tEnvSampler), brdfSamplePoint).rg;
    vec2 f_ab = texture(sampler2D(at2DTextures[nonuniformEXT(tProbeData.atData[iProbeIndex].uGGXLUT)], tEnvSampler), brdfSamplePoint).rg;

    vec3 specularLight = specularSample.rgb;

    vec3 Fr = max(vec3(1.0 - roughness), F0) - F0;
    vec3 k_S = F0 + Fr * pow(1.0 - NdotV, 5.0);
    vec3 FssEss = k_S * f_ab.x + f_ab.y;

    return specularWeight * specularLight * FssEss;
}

// specularWeight is introduced with KHR_materials_specular
vec3
getIBLRadianceLambertian(vec3 n, vec3 v, float roughness, vec3 diffuseColor, vec3 F0, float specularWeight, int iProbeIndex, vec3 tWorldPos)
{

    // if(bool(tProbeData.atData[iProbeIndex].iParallaxCorrection))
    // {

    //     // Find the ray intersection with box plane
    //     vec3 FirstPlaneIntersect = (tProbeData.atData[iProbeIndex].tMax.xyz - tWorldPos) / n;
    //     vec3 SecondPlaneIntersect = (tProbeData.atData[iProbeIndex].tMin.xyz - tWorldPos) / n;
        
    //     // Get the furthest of these intersections along the ray
    //     // (Ok because x/0 give +inf and -x/0 give –inf )
    //     vec3 FurthestPlane = max(FirstPlaneIntersect, SecondPlaneIntersect);

    //     // Find the closest far intersection
    //     float Distance = min(min(FurthestPlane.x, FurthestPlane.y), FurthestPlane.z);

    //     // Get the intersection position
    //     vec3 IntersectPositionWS = tWorldPos + n * Distance;

    //     // Get corrected reflection
    //     n = IntersectPositionWS - tProbeData.atData[iProbeIndex].tPosition;
    // }

    // End parallax-correction code

    vec3 irradiance = getDiffuseLight(n, iProbeIndex);

    float NdotV = clampedDot(n, v);
    vec2 brdfSamplePoint = clamp(vec2(NdotV, roughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
    // vec2 f_ab = texture(sampler2D(u_GGXLUT, tEnvSampler), brdfSamplePoint).rg;
    vec2 f_ab = texture(sampler2D(at2DTextures[nonuniformEXT(tProbeData.atData[iProbeIndex].uGGXLUT)], tEnvSampler), brdfSamplePoint).rg;

    // see https://bruop.github.io/ibl/#single_scattering_results at Single Scattering Results
    // Roughness dependent fresnel, from Fdez-Aguera

    vec3 Fr = max(vec3(1.0 - roughness), F0) - F0;
    vec3 k_S = F0 + Fr * pow(1.0 - NdotV, 5.0);
    vec3 FssEss = specularWeight * k_S * f_ab.x + f_ab.y; // <--- GGX / specular light contribution (scale it down if the specularWeight is low)

    // Multiple scattering, from Fdez-Aguera
    float Ems = (1.0 - (f_ab.x + f_ab.y));
    vec3 F_avg = specularWeight * (F0 + (1.0 - F0) / 21.0);
    vec3 FmsEms = Ems * FssEss * F_avg / (1.0 - F_avg * Ems);
    vec3 k_D = diffuseColor * (1.0 - FssEss + FmsEms); // we use +FmsEms as indicated by the formula in the blog post (might be a typo in the implementation)

    return (FmsEms + k_D) * irradiance;
}

float
textureProj(vec4 shadowCoord, vec2 offset, int textureIndex)
{
	float shadow = 1.0;
    vec2 comp2 = shadowCoord.st + offset;

	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 )
    {
		float dist = 1.0 - texture(sampler2D(at2DTextures[nonuniformEXT(textureIndex)], tShadowSampler), comp2).r * shadowCoord.w;
		if (shadowCoord.w > 0 && dist < shadowCoord.z)
        {
			shadow = 0.0; // ambient
		}
	}
	return shadow;
}

float
textureProj2(vec4 shadowCoord, vec2 offset, int textureIndex)
{
	float shadow = 1.0;
    vec2 comp2 = shadowCoord.st + offset;

	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 )
    {
		float dist = texture(sampler2D(at2DTextures[nonuniformEXT(textureIndex)], tShadowSampler), comp2).r;
		if (shadowCoord.w > 0 && dist > shadowCoord.z)
        {
			shadow = 0.0; // ambient
		}
	}
	return shadow;
}

float
filterPCF(vec4 sc, vec2 offset, int textureIndex)
{
	ivec2 texDim = textureSize(sampler2D(at2DTextures[nonuniformEXT(textureIndex)], tShadowSampler), 0).xy;
	float scale = 1.0;
	float dx = scale * 1.0 / (float(texDim.x));
	float dy = scale * 1.0 / (float(texDim.y));

	float shadowFactor = 0.0;
	int count = 0;
	int range = 1;
	
	for (int x = -range; x <= range; x++) {
		for (int y = -range; y <= range; y++) {
			shadowFactor += textureProj(sc, vec2(dx*x, dy*y) + offset, textureIndex);
			count++;
		}
	}
	return shadowFactor / count;
}

float
filterPCF2(vec4 sc, vec2 offset, int textureIndex)
{
	ivec2 texDim = textureSize(sampler2D(at2DTextures[nonuniformEXT(textureIndex)], tShadowSampler), 0).xy;
	float scale = 1.0;
	float dx = scale * 1.0 / (float(texDim.x));
	float dy = scale * 1.0 / (float(texDim.y));

	float shadowFactor = 0.0;
	int count = 0;
	int range = 1;
	
	for (int x = -range; x <= range; x++) {
		for (int y = -range; y <= range; y++) {
			shadowFactor += textureProj2(sc, vec2(dx*x, dy*y) + offset, textureIndex);
			count++;
		}
	}
	return shadowFactor / count;
}

void main() 
{
    vec4 AORoughnessMetalnessData = subpassLoad(tAOMetalRoughnessTexture);
    vec4 tBaseColor = subpassLoad(tAlbedoSampler);
    
    float depth = subpassLoad(tDepthSampler).r;
    vec3 ndcSpace = vec3((gl_FragCoord.x - tGlobalInfo.data[tObjectInfo.tData.uGlobalIndex].tViewportInfo.x) / tGlobalInfo.data[tObjectInfo.tData.uGlobalIndex].tViewportSize.x, (gl_FragCoord.y - tGlobalInfo.data[tObjectInfo.tData.uGlobalIndex].tViewportInfo.y) / tGlobalInfo.data[tObjectInfo.tData.uGlobalIndex].tViewportSize.y, depth);


    vec3 clipSpace = ndcSpace;
    clipSpace.xy = clipSpace.xy * 2.0 - 1.0;
    // clipSpace.x *= tGlobalInfo.tViewportSize.z;
    // clipSpace.y *= tGlobalInfo.tViewportSize.w;
    // clipSpace.x += tGlobalInfo.tViewportInfo.x;
    // clipSpace.y += tGlobalInfo.tViewportInfo.y;
    vec4 homoLocation = inverse(tGlobalInfo.data[tObjectInfo.tData.uGlobalIndex].tCameraProjection) * vec4(clipSpace, 1.0);

    // vec4 tWorldPosition0 = subpassLoad(tPositionSampler);
    vec4 tViewPosition = homoLocation;
    tViewPosition.xyz = tViewPosition.xyz / tViewPosition.w;
    tViewPosition.x = tViewPosition.x;
    tViewPosition.y = tViewPosition.y;
    tViewPosition.z = tViewPosition.z;
    tViewPosition.w = 1.0;
    vec4 tWorldPosition = inverse(tGlobalInfo.data[tObjectInfo.tData.uGlobalIndex].tCameraView) * tViewPosition;
    vec3 n = Decode(subpassLoad(tNormalTexture).xy);

    const vec3 f90 = vec3(1.0);
    
    // LIGHTING
    vec3 f_specular = vec3(0.0);
    vec3 f_diffuse = vec3(0.0);
   
    const float fMetalness = AORoughnessMetalnessData.g;
    vec3 c_diff = mix(tBaseColor.rgb,  vec3(0), fMetalness);
    vec3 f0 = mix(vec3(0.04), tBaseColor.rgb, fMetalness);

    const float fPerceptualRoughness = AORoughnessMetalnessData.b;
    float specularWeight = 1.0;
    vec3 v = normalize(tGlobalInfo.data[tObjectInfo.tData.uGlobalIndex].tCameraPos.xyz - tWorldPosition.xyz);

    // Calculate lighting contribution from image based lighting source (IBL)
    if(bool(iRenderingFlags & PL_RENDERING_FLAG_USE_IBL) && iProbeCount > 0)
    {
        int iProbeIndex = 0;
        float fCurrentDistance = 10000.0;
        for(int i = iProbeCount - 1; i > -1; i--)
        {
            vec3 tDist = tProbeData.atData[i].tPosition - tWorldPosition.xyz;
            tDist = tDist * tDist;
            float fDistSqr = tDist.x + tDist.y + tDist.z;
            if(fDistSqr <= tProbeData.atData[i].fRangeSqr && fDistSqr < fCurrentDistance)
            {
                iProbeIndex = i;
                fCurrentDistance = fDistSqr;
            }
        }

        if(iProbeIndex > -1)
        {
            int iMips = textureQueryLevels(samplerCube(atCubeTextures[nonuniformEXT(tProbeData.atData[iProbeIndex].uGGXEnvSampler)], tEnvSampler));
            f_specular +=  getIBLRadianceGGX(n, v, fPerceptualRoughness, f0, specularWeight, iMips, iProbeIndex, tWorldPosition.xyz);
            f_diffuse += getIBLRadianceLambertian(n, v, fPerceptualRoughness, c_diff, f0, specularWeight, iProbeIndex, tWorldPosition.xyz);
        }
    }

    // punctual stuff
    vec3 f_diffuse_ibl = f_diffuse;
    vec3 f_specular_ibl = f_specular;
    f_diffuse = vec3(0.0);
    f_specular = vec3(0.0);

    uint cascadeIndex = 0;
    const bool bShadows = bool(iRenderingFlags & PL_RENDERING_FLAG_SHADOWS);
    if(bool(iRenderingFlags & PL_RENDERING_FLAG_USE_PUNCTUAL))
    {
        const float fAlphaRoughness = fPerceptualRoughness * fPerceptualRoughness;

        for(int i = 0; i < iLightCount; i++)
        {

            plGpuLight tLightData = tLightInfo.atData[i];

            float shadow = 1.0;

            if(tLightData.iType == PL_LIGHT_TYPE_DIRECTIONAL)
            {
                vec3 pointToLight = -tLightData.tDirection;

                if(bShadows && tLightData.iCastShadow > 0)
                {
                    plGpuLightShadow tShadowData = tDShadowData.atData[tLightData.iShadowIndex];

                    // Get cascade index for the current fragment's view position
                    
                    
                    for(int j = 0; j < tLightData.iCascadeCount - 1; ++j)
                    {
                        if(tViewPosition.z > tShadowData.cascadeSplits[j])
                        {	
                            cascadeIndex = j + 1;
                        }
                    }  

                    // Depth compare for shadowing
                    mat4 abiasMat = biasMat;
                    abiasMat[0][0] *= tShadowData.fFactor;
                    abiasMat[1][1] *= tShadowData.fFactor;
                    abiasMat[3][0] *= tShadowData.fFactor;
                    abiasMat[3][1] *= tShadowData.fFactor;
                    vec4 shadowCoord = (abiasMat * tShadowData.viewProjMat[cascadeIndex]) * vec4(tWorldPosition.xyz, 1.0);
                    shadow = 0;
                    // shadow = textureProj(shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset) + vec2(cascadeIndex * tShadowData.fFactor, 0), tShadowData.iShadowMapTexIdx);
                    shadow = filterPCF(shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset) + vec2(cascadeIndex * tShadowData.fFactor, 0), tShadowData.iShadowMapTexIdx);

                    // for(int j = 0; j < 4; j++)
                    // {
                    //     // int index = int(16.0*random(gl_FragCoord.xyy, j))%16;
                    //     int index = int(16.0*random(tWorldPosition.xxx, j))%16;
                    //     shadow += 0.25 * textureProj(vec4(( poissonDisk[index] / 4000.0 + shadowCoord.xy), shadowCoord.z, shadowCoord.w), vec2(tShadowData.fXOffset, tShadowData.fYOffset) + vec2(cascadeIndex * tShadowData.fFactor, 0), tShadowData.iShadowMapTexIdx);
                    // }
                    // shadow = clamp(shadow, 0.02, 1);
                }

                // BSTF
                vec3 l = normalize(pointToLight);   // Direction from surface point to light
                vec3 h = normalize(l + v);          // Direction of the vector between l and v, called halfway vector
                float NdotL = clampedDot(n, l);
                float NdotV = clampedDot(n, v);
                float NdotH = clampedDot(n, h);
                float LdotH = clampedDot(l, h);
                float VdotH = clampedDot(v, h);
                if (NdotL > 0.0 || NdotV > 0.0)
                {
                    vec3 intensity = getLightIntensity(tLightData, pointToLight);
                    f_diffuse += shadow * intensity * NdotL *  BRDF_lambertian(f0, f90, c_diff, specularWeight, VdotH);
                    f_specular += shadow * intensity * NdotL * BRDF_specularGGX(f0, f90, fAlphaRoughness, specularWeight, VdotH, NdotL, NdotV, NdotH);
                }
            }

            else if(tLightData.iType == PL_LIGHT_TYPE_POINT)
            {
                vec3 pointToLight = tLightData.tPosition - tWorldPosition.xyz;

                if(bShadows && tLightData.iCastShadow > 0)
                {
                    plGpuLightShadow tShadowData = tShadowData.atData[tLightData.iShadowIndex];

                    vec3 result = sampleCube(-normalize(pointToLight));
                    vec4 shadowCoord = tShadowData.viewProjMat[int(result.z)] * vec4(tWorldPosition.xyz, 1.0);
                    if(shadowCoord.z > -1.0 && shadowCoord.z < 1.0)
                    {
                        shadow = 1.0;
                        const vec2 faceoffsets[6] = {
                            vec2(0, 0),
                            vec2(1, 0),
                            vec2(0, 1),
                            vec2(1, 1),
                            vec2(0, 2),
                            vec2(1, 2),
                        };

                        shadowCoord.xyz /= shadowCoord.w;
                        result.xy *= tShadowData.fFactor;
                        shadowCoord.xy = result.xy;
                        shadow = textureProj2(shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset) + faceoffsets[int(result.z)] * tShadowData.fFactor, tShadowData.iShadowMapTexIdx);
                        // shadow = filterPCF2(shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset) + faceoffsets[int(result.z)] * tShadowData.fFactor, tShadowData.iShadowMapTexIdx);
                    }
                }

                // BSTF
                vec3 l = normalize(pointToLight);   // Direction from surface point to light
                vec3 h = normalize(l + v);          // Direction of the vector between l and v, called halfway vector
                float NdotL = clampedDot(n, l);
                float NdotV = clampedDot(n, v);
                float NdotH = clampedDot(n, h);
                float LdotH = clampedDot(l, h);
                float VdotH = clampedDot(v, h);
                if (NdotL > 0.0 || NdotV > 0.0)
                {
                    vec3 intensity = getLightIntensity(tLightData, pointToLight);
                    f_diffuse += shadow * intensity * NdotL *  BRDF_lambertian(f0, f90, c_diff, specularWeight, VdotH);
                    f_specular += shadow * intensity * NdotL * BRDF_specularGGX(f0, f90, fAlphaRoughness, specularWeight, VdotH, NdotL, NdotV, NdotH);
                }
            }

            else if(tLightData.iType == PL_LIGHT_TYPE_SPOT)
            {
                vec3 pointToLight = tLightData.tPosition - tWorldPosition.xyz;
                if(bShadows && tLightData.iCastShadow > 0)
                {
                    plGpuLightShadow tShadowData = tShadowData.atData[tLightData.iShadowIndex];

                    vec4 shadowCoord = tShadowData.viewProjMat[0] * vec4(tWorldPosition.xyz, 1.0);
                    if(shadowCoord.z > -1.0 && shadowCoord.z < 1.0)
                    {
                        shadowCoord.xyz /= shadowCoord.w;
                        shadow = 0.0;
                        shadowCoord.x = shadowCoord.x/2 + 0.5;
                        shadowCoord.y = shadowCoord.y/2 + 0.5;
                        shadowCoord.xy *= tShadowData.fFactor;

                        // shadow = textureProj2(shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset), tShadowData.iShadowMapTexIdx);
                        shadow = filterPCF2(shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset), tShadowData.iShadowMapTexIdx);

                        // for(int j = 0; j < 4; j++)
                        // {
                        //     int index = int(16.0*random(gl_FragCoord.xyy, j))%16;
                        //     shadow += 0.2 * textureProj2(vec4(( poissonDisk[index] / 2000.0 + shadowCoord.xy), shadowCoord.z, shadowCoord.w), vec2(tShadowData.fXOffset, tShadowData.fYOffset), tShadowData.iShadowMapTexIdx);
                        // }
                        // shadow = clamp(shadow, 0.02, 1);
                    }
                }

                // BSTF
                vec3 l = normalize(pointToLight);   // Direction from surface point to light
                vec3 h = normalize(l + v);          // Direction of the vector between l and v, called halfway vector
                float NdotL = clampedDot(n, l);
                float NdotV = clampedDot(n, v);
                float NdotH = clampedDot(n, h);
                float LdotH = clampedDot(l, h);
                float VdotH = clampedDot(v, h);
                if (NdotL > 0.0 || NdotV > 0.0)
                {
                    vec3 intensity = getLightIntensity(tLightData, pointToLight);
                    f_diffuse += shadow * intensity * NdotL *  BRDF_lambertian(f0, f90, c_diff, specularWeight, VdotH);
                    f_specular += shadow * intensity * NdotL * BRDF_specularGGX(f0, f90, fAlphaRoughness, specularWeight, VdotH, NdotL, NdotV, NdotH);
                }
            }

        }
        
    }

    // Layer blending
    vec3 diffuse;
    vec3 specular;

    const float ao = AORoughnessMetalnessData.r;
    if(ao != 1.0)
    {
        float u_OcclusionStrength = 1.0;
        diffuse = f_diffuse + mix(f_diffuse_ibl, f_diffuse_ibl * ao, u_OcclusionStrength);
        // apply ambient occlusion to all lighting that is not punctual
        specular = f_specular + mix(f_specular_ibl, f_specular_ibl * ao, u_OcclusionStrength);
    }
    else
    {
        diffuse = f_diffuse_ibl + f_diffuse;
        specular = f_specular_ibl + f_specular;
    }

    vec3 color = diffuse + specular;

    outColor = vec4(color.rgb, tBaseColor.a);
    
    // outColor = vec4(ndcSpace.rgb, tBaseColor.a);
    // outColor = vec4(gl_FragCoord.x, 0, 0, tBaseColor.a);
    // outColor = vec4(v.r, v.g, v.b, tBaseColor.a);
    // outColor = vec4(v.r, v.g, v.b, tBaseColor.a);
    // outColor = vec4(tWorldPosition.rgb, tBaseColor.a);
    // outColor = vec4(tViewPosition.rgb, tBaseColor.a);
    // outColor = vec4(n, tBaseColor.a);

    // if(gl_FragCoord.x < 1400.0)
    // {
    //     switch(cascadeIndex) {
    //         case 0 : 
    //             outColor.rgb *= vec3(1.0f, 0.25f, 0.25f);
    //             break;
    //         case 1 : 
    //             outColor.rgb *= vec3(0.25f, 1.0f, 0.25f);
    //             break;
    //         case 2 : 
    //             outColor.rgb *= vec3(0.25f, 0.25f, 1.0f);
    //             break;
    //         case 3 : 
    //             outColor.rgb *= vec3(1.0f, 1.0f, 0.25f);
    //             break;
    //     }
    // }
}