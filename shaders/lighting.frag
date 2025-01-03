#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "defines.glsl"
#include "material.glsl"
#include "lights.glsl"
#include "math.glsl"

//-----------------------------------------------------------------------------
// [SECTION] specialication constants
//-----------------------------------------------------------------------------

layout(constant_id = 0) const int iRenderingFlags = 0;
layout(constant_id = 1) const int iDirectionLightCount = 0;
layout(constant_id = 2) const int iPointLightCount = 0;
layout(constant_id = 3) const int iSpotLightCount = 0;

//-----------------------------------------------------------------------------
// [SECTION] bind group 0
//-----------------------------------------------------------------------------

layout(std140, set = 0, binding = 0) readonly buffer _tVertexBuffer
{
	vec4 atVertexData[];
} tVertexBuffer;

layout(set = 0, binding = 1) readonly buffer plMaterialInfo
{
    tMaterial atMaterials[];
} tMaterialInfo;
layout(set = 0, binding = 2)  uniform sampler tDefaultSampler;
layout(set = 0, binding = 3)  uniform sampler tEnvSampler;
layout(set = 0, binding = 4)  uniform texture2D at2DTextures[4096];
layout(set = 0, binding = 4100)  uniform textureCube atCubeTextures[4096];

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

layout(set = 2, binding = 0) uniform _plGlobalInfo
{
    vec4 tViewportSize;
    vec4 tCameraPos;
    mat4 tCameraView;
    mat4 tCameraProjection;
    mat4 tCameraViewProjection;
    uint uLambertianEnvSampler;
    uint uGGXEnvSampler;
    uint uGGXLUT;
    uint _uUnUsed;
} tGlobalInfo;

layout(set = 2, binding = 1) uniform _plDLightInfo
{
    plLightData atData[1];
} tDirectionLightInfo;

layout(set = 2, binding = 2) uniform _plPLightInfo
{
    plLightData atData[1];
} tPointLightInfo;

layout(set = 2, binding = 3) uniform _plSLightInfo
{
    plLightData atData[1];
} tSpotLightInfo;

layout(set = 2, binding = 4) readonly buffer plDShadowData
{
    plLightShadowData atData[];
} tDShadowData;

layout(set = 2, binding = 5) readonly buffer plPShadowData
{
    plLightShadowData atData[];
} tPShadowData;

layout(set = 2, binding = 6) readonly buffer plSShadowData
{
    plLightShadowData atData[];
} tSShadowData;

layout(set = 2, binding = 7) uniform sampler tShadowSampler;

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    int iDataOffset;
    int iVertexOffset;
} tObjectInfo;

//-----------------------------------------------------------------------------
// [SECTION] input & output
//-----------------------------------------------------------------------------

layout(location = 0) out vec4 outColor;

// output
layout(location = 0) in struct plShaderIn {
    vec2 tUV;
} tShaderIn;

//-----------------------------------------------------------------------------
// [SECTION] BRDF
//-----------------------------------------------------------------------------

//
// Fresnel
//
// http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html
// https://github.com/wdas/brdf/tree/master/src/brdfs
// https://google.github.io/filament/Filament.md.html
//

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from [4], Equation 15
vec3 F_Schlick(vec3 f0, vec3 f90, float VdotH)
{
    return f0 + (f90 - f0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

// Smith Joint GGX
// Note: Vis = G / (4 * NdotL * NdotV)
// see Eric Heitz. 2014. Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs. Journal of Computer Graphics Techniques, 3
// see Real-Time Rendering. Page 331 to 336.
// see https://google.github.io/filament/Filament.md.html#materialsystem/specularbrdf/geometricshadowing(specularg)
float V_GGX(float NdotL, float NdotV, float alphaRoughness)
{
    float alphaRoughnessSq = alphaRoughness * alphaRoughness;

    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);

    float GGX = GGXV + GGXL;
    if (GGX > 0.0)
    {
        return 0.5 / GGX;
    }
    return 0.0;
}


// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float D_GGX(float NdotH, float alphaRoughness)
{
    float alphaRoughnessSq = alphaRoughness * alphaRoughness;
    float f = (NdotH * NdotH) * (alphaRoughnessSq - 1.0) + 1.0;
    return alphaRoughnessSq / (M_PI * f * f);
}


//https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#acknowledgments AppendixB
vec3 BRDF_lambertian(vec3 f0, vec3 f90, vec3 diffuseColor, float specularWeight, float VdotH)
{
    // see https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
    return (1.0 - specularWeight * F_Schlick(f0, f90, VdotH)) * (diffuseColor / M_PI);
}

//  https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#acknowledgments AppendixB
vec3 BRDF_specularGGX(vec3 f0, vec3 f90, float alphaRoughness, float specularWeight, float VdotH, float NdotL, float NdotV, float NdotH)
{
    vec3 F = F_Schlick(f0, f90, VdotH);
    float Vis = V_GGX(NdotL, NdotV, alphaRoughness);
    float D = D_GGX(NdotH, alphaRoughness);

    return specularWeight * F * Vis * D;
}

vec3 getDiffuseLight(vec3 n)
{
    // n.z = -n.z; uncomment if not reverse z
    // return texture(samplerCube(u_LambertianEnvSampler, tEnvSampler), n).rgb;
    return texture(samplerCube(atCubeTextures[nonuniformEXT(tGlobalInfo.uLambertianEnvSampler)], tEnvSampler), n).rgb;
}


vec4 getSpecularSample(vec3 reflection, float lod)
{
    // reflection.z = -reflection.z; // uncomment if not reverse z
    // return textureLod(samplerCube(u_GGXEnvSampler, tEnvSampler), reflection, lod);
    return textureLod(samplerCube(atCubeTextures[nonuniformEXT(tGlobalInfo.uGGXEnvSampler)], tEnvSampler), reflection, lod);
}

vec3 getIBLRadianceGGX(vec3 n, vec3 v, float roughness, vec3 F0, float specularWeight, int u_MipCount)
{
    
    float lod = roughness * float(u_MipCount - 1);
    vec3 reflection = normalize(reflect(-v, n));
    vec4 specularSample = getSpecularSample(reflection, lod);

    float NdotV = clampedDot(n, v);
    vec2 brdfSamplePoint = clamp(vec2(NdotV, roughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
    // vec2 f_ab = texture(sampler2D(u_GGXLUT, tEnvSampler), brdfSamplePoint).rg;
    vec2 f_ab = texture(sampler2D(at2DTextures[nonuniformEXT(tGlobalInfo.uGGXLUT)], tEnvSampler), brdfSamplePoint).rg;

    vec3 specularLight = specularSample.rgb;

    vec3 Fr = max(vec3(1.0 - roughness), F0) - F0;
    vec3 k_S = F0 + Fr * pow(1.0 - NdotV, 5.0);
    vec3 FssEss = k_S * f_ab.x + f_ab.y;

    return specularWeight * specularLight * FssEss;
}

// specularWeight is introduced with KHR_materials_specular
vec3 getIBLRadianceLambertian(vec3 n, vec3 v, float roughness, vec3 diffuseColor, vec3 F0, float specularWeight)
{
    vec3 irradiance = getDiffuseLight(n);

    float NdotV = clampedDot(n, v);
    vec2 brdfSamplePoint = clamp(vec2(NdotV, roughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
    // vec2 f_ab = texture(sampler2D(u_GGXLUT, tEnvSampler), brdfSamplePoint).rg;
    vec2 f_ab = texture(sampler2D(at2DTextures[nonuniformEXT(tGlobalInfo.uGGXLUT)], tEnvSampler), brdfSamplePoint).rg;

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

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 
);

float textureProj(vec4 shadowCoord, vec2 offset, int textureIndex)
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

float textureProj2(vec4 shadowCoord, vec2 offset, int textureIndex)
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

float filterPCF(vec4 sc, vec2 offset, int textureIndex)
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

vec3 Decode( vec2 f )
{
    f = f * 2.0 - 1.0;
 
    // https://twitter.com/Stubbesaurus/status/937994790553227264
    vec3 n = vec3( f.x, f.y, 1.0 - abs( f.x ) - abs( f.y ) );
    float t = max( -n.z, 0.0 );
    n.x += n.x >= 0.0 ? -t : t;
    n.y += n.y >= 0.0 ? -t : t;
    return normalize( n );
}

vec3 sampleCube(vec3 v)
{
	vec3 vAbs = abs(v);
	float ma;
	vec2 uv;
    float faceIndex = 0.0;
	if(vAbs.z >= vAbs.x && vAbs.z >= vAbs.y)
	{
		faceIndex = v.z < 0.0 ? 1.0 : 0.0;
		ma = 0.5 / vAbs.z;
		uv = vec2(v.z < 0.0 ? v.x : -v.x, -v.y);
	}
	else if(vAbs.y >= vAbs.x)
	{
		faceIndex = v.y < 0.0 ? 5.0 : 4.0;
		ma = 0.5 / vAbs.y;
		uv = vec2(-v.x, v.y < 0.0 ? -v.z : v.z);
	}
	else
	{
		faceIndex = v.x < 0.0 ? 3.0 : 2.0;
		ma = 0.5 / vAbs.x;
		uv = vec2(v.x < 0.0 ? -v.z : v.z, -v.y);
	}
	vec2 result = uv * ma + vec2(0.5, 0.5);
    return vec3(result, faceIndex);
}

void main() 
{
    vec4 AORoughnessMetalnessData = subpassLoad(tAOMetalRoughnessTexture);
    vec4 tBaseColor = subpassLoad(tAlbedoSampler);
    
    float depth = subpassLoad(tDepthSampler).r;
    vec3 ndcSpace = vec3(gl_FragCoord.x / tGlobalInfo.tViewportSize.x, gl_FragCoord.y / tGlobalInfo.tViewportSize.y, depth);
    vec3 clipSpace = ndcSpace;
    clipSpace.xy = clipSpace.xy * 2.0 - 1.0;
    vec4 homoLocation = inverse(tGlobalInfo.tCameraProjection) * vec4(clipSpace, 1.0);


    // vec4 tWorldPosition0 = subpassLoad(tPositionSampler);
    vec4 tViewPosition = homoLocation;
    tViewPosition.xyz = tViewPosition.xyz / tViewPosition.w;
    tViewPosition.x = tViewPosition.x;
    tViewPosition.y = tViewPosition.y;
    tViewPosition.z = tViewPosition.z;
    tViewPosition.w = 1.0;
    vec4 tWorldPosition = inverse(tGlobalInfo.tCameraView) * tViewPosition;
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
    vec3 v = normalize(tGlobalInfo.tCameraPos.xyz - tWorldPosition.xyz);
    int iMips = int(AORoughnessMetalnessData.a);

    // Calculate lighting contribution from image based lighting source (IBL)
    if(bool(iRenderingFlags & PL_RENDERING_FLAG_USE_IBL))
    {
        f_specular +=  getIBLRadianceGGX(n, v, fPerceptualRoughness, f0, specularWeight, iMips);
        f_diffuse += getIBLRadianceLambertian(n, v, fPerceptualRoughness, c_diff, f0, specularWeight);
    }

    // punctual stuff
    vec3 f_diffuse_ibl = f_diffuse;
    vec3 f_specular_ibl = f_specular;
    f_diffuse = vec3(0.0);
    f_specular = vec3(0.0);

    uint cascadeIndex = 0;
    if(bool(iRenderingFlags & PL_RENDERING_FLAG_USE_PUNCTUAL))
    {
        const float fAlphaRoughness = fPerceptualRoughness * fPerceptualRoughness;

        for(int i = 0; i < iDirectionLightCount; i++)
        {
            plLightData tLightData = tDirectionLightInfo.atData[i];

            vec3 pointToLight = -tLightData.tDirection;
            float shadow = 1.0;

            if(tLightData.iCastShadow > 0)
            {
                plLightShadowData tShadowData = tDShadowData.atData[tLightData.iShadowIndex];

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

        // spot light
        for(int i = 0; i < iSpotLightCount; i++)
        {

            plLightData tLightData = tSpotLightInfo.atData[i];

            vec3 pointToLight = tLightData.tPosition - tWorldPosition.xyz;

            float shadow = 1.0;

            if(tLightData.iCastShadow > 0)
            {
                plLightShadowData tShadowData = tSShadowData.atData[tLightData.iShadowIndex];

                vec4 shadowCoord = tShadowData.viewProjMat[0] * vec4(tWorldPosition.xyz, 1.0);
                if(shadowCoord.z > -1.0 && shadowCoord.z < 1.0)
                {
                    shadowCoord.xyz /= shadowCoord.w;
                    shadow = 1.0;
                    shadowCoord.x = shadowCoord.x/2 + 0.5;
                    shadowCoord.y = shadowCoord.y/2 + 0.5;

                    vec2 sampleLocation = vec2(tShadowData.fXOffset, tShadowData.fYOffset) + (shadowCoord.xy * tShadowData.fFactor);
                    float dist = texture(sampler2D(at2DTextures[nonuniformEXT(tShadowData.iShadowMapTexIdx)], tShadowSampler), sampleLocation).r;
                    float fDist = abs(shadowCoord.z);

                    if(dist > fDist)
                    {
                        shadow = 0.0;
                    }
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
        

        for(int i = 0; i < iPointLightCount; i++)
        {
            plLightData tLightData = tPointLightInfo.atData[i];

            vec3 pointToLight = tLightData.tPosition - tWorldPosition.xyz;

            float shadow = 1.0;

            if(tLightData.iCastShadow > 0)
            {
                plLightShadowData tShadowData = tPShadowData.atData[tLightData.iShadowIndex];

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
                    vec2 sampleLocation = vec2(tShadowData.fXOffset, tShadowData.fYOffset) + (result.xy * tShadowData.fFactor) + (faceoffsets[int(result.z)] * tShadowData.fFactor);
                    float dist = texture(sampler2D(at2DTextures[nonuniformEXT(tShadowData.iShadowMapTexIdx)], tShadowSampler), sampleLocation).r;
                    float fDist = shadowCoord.z;
                    dist = dist * shadowCoord.w;

                    if(shadowCoord.w > 0 && dist > fDist)
                    {
                        shadow = 0.0;
                    }
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