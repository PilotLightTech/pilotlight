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

layout(constant_id = 0) const int iMeshVariantFlags = 0;
layout(constant_id = 1) const int iDataStride = 0;
layout(constant_id = 2) const int iTextureMappingFlags = 0;
layout(constant_id = 3) const int iMaterialFlags = 0;
layout(constant_id = 4) const int iRenderingFlags = 0;
layout(constant_id = 5) const int iLightCount = 0;
layout(constant_id = 6) const int iProbeCount = 0;

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

struct tGlobalData
{
    vec4 tViewportSize;
    vec4 tViewportInfo;
    vec4 tCameraPos;
    mat4 tCameraView;
    mat4 tCameraProjection;
    mat4 tCameraViewProjection;
};

layout(set = 1, binding = 0) readonly buffer _plGlobalInfo
{
    tGlobalData data[];
} tGlobalInfo;

layout(set = 1, binding = 1) uniform _plLightInfo
{
    plLightData atData[1];
} tLightInfo;

layout(set = 1, binding = 2) readonly buffer plDShadowData
{
    plLightShadowData atData[];
} tDShadowData;

layout(set = 1, binding = 3) readonly buffer plShadowData
{
    plLightShadowData atData[];
} tShadowData;

layout(set = 1, binding = 4) readonly buffer plProbeData
{
    plEnvironmentProbeData atData[];
} tProbeData;

layout(set = 1, binding = 5)  uniform sampler tShadowSampler;

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    int  iDataOffset;
    int  iVertexOffset;
    int  iMaterialIndex;
    int  iPadding;
    mat4 tModel;

    uint uGlobalIndex;
} tObjectInfo;

//-----------------------------------------------------------------------------
// [SECTION] input & output
//-----------------------------------------------------------------------------

layout(location = 0) out vec4 outColor;

// output
layout(location = 0) in struct plShaderIn {
    vec3 tPosition;
    vec2 tUV[8];
    vec4 tColor;
    vec3 tWorldNormal;
    mat3 tTBN;
} tShaderIn;

//-----------------------------------------------------------------------------
// [SECTION] helpers
//-----------------------------------------------------------------------------

vec2 poissonDisk[16] = vec2[]( 
   vec2( -0.94201624, -0.39906216 ), 
   vec2( 0.94558609, -0.76890725 ), 
   vec2( -0.094184101, -0.92938870 ), 
   vec2( 0.34495938, 0.29387760 ), 
   vec2( -0.91588581, 0.45771432 ), 
   vec2( -0.81544232, -0.87912464 ), 
   vec2( -0.38277543, 0.27676845 ), 
   vec2( 0.97484398, 0.75648379 ), 
   vec2( 0.44323325, -0.97511554 ), 
   vec2( 0.53742981, -0.47373420 ), 
   vec2( -0.26496911, -0.41893023 ), 
   vec2( 0.79197514, 0.19090188 ), 
   vec2( -0.24188840, 0.99706507 ), 
   vec2( -0.81409955, 0.91437590 ), 
   vec2( 0.19984126, 0.78641367 ), 
   vec2( 0.14383161, -0.14100790 ) 
);

float random(vec3 seed, int i)
{
    vec4 seed4 = vec4(seed, float(i));
    float dot_product = dot(seed4, vec4(12.9898,78.233,45.164,94.673));
    return fract(sin(dot_product) * 43758.5453);
}

struct NormalInfo {
    vec3 ng;   // Geometry normal
    vec3 t;    // Geometry tangent
    vec3 b;    // Geometry bitangent
    vec3 n;    // Shading normal
    vec3 ntex; // Normal from texture, scaling is accounted for.
};

NormalInfo pl_get_normal_info(int iUVSet)
{
    vec2 UV = tShaderIn.tUV[iUVSet];
    vec2 uv_dx = dFdx(UV);
    vec2 uv_dy = dFdy(UV);

    // if (length(uv_dx) <= 1e-2) {
    //   uv_dx = vec2(1.0, 0.0);
    // }

    // if (length(uv_dy) <= 1e-2) {
    //   uv_dy = vec2(0.0, 1.0);
    // }

    vec3 t_ = (uv_dy.t * dFdx(tShaderIn.tPosition) - uv_dx.t * dFdy(tShaderIn.tPosition)) /
        (uv_dx.s * uv_dy.t - uv_dy.s * uv_dx.t);

    vec3 n, t, b, ng;

    // Compute geometrical TBN:
    if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_NORMAL))
    {

        if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TANGENT))
        {
            // Trivial TBN computation, present as vertex attribute.
            // Normalize eigenvectors as matrix is linearly interpolated.
            t = normalize(tShaderIn.tTBN[0]);
            b = normalize(tShaderIn.tTBN[1]);
            ng = normalize(tShaderIn.tTBN[2]);
        }
        else
        {
            // Normals are either present as vertex attributes or approximated.
            ng = normalize(tShaderIn.tWorldNormal);
            t = normalize(t_ - ng * dot(ng, t_));
            b = cross(ng, t);
        }
    }
    else
    {
        ng = normalize(cross(dFdx(tShaderIn.tPosition), dFdy(tShaderIn.tPosition)));
        t = normalize(t_ - ng * dot(ng, t_));
        b = cross(ng, t);
    }


    // For a back-facing surface, the tangential basis vectors are negated.
    if (gl_FrontFacing == false)
    {
        t *= -1.0;
        b *= -1.0;
        ng *= -1.0;
    }

    // Compute normals:
    NormalInfo info;
    info.ng = ng;
    if(bool(iTextureMappingFlags & PL_HAS_NORMAL_MAP)) 
    {
        tMaterial material = tMaterialInfo.atMaterials[tObjectInfo.iMaterialIndex];
        info.ntex = texture(sampler2D(at2DTextures[nonuniformEXT(material.iNormalTexIdx)], tDefaultSampler), UV).rgb * 2.0 - vec3(1.0);
        // if (gl_FrontFacing == false)
        // {
        //     info.ntex.x *= -1.0;
        //     info.ntex.z *= -1.0;
        // }
        // else
        // {
        //     info.ntex.y *= -1.0;
        // }
        info.ntex = normalize(info.ntex);
        info.n = normalize(mat3(t, b, ng) * info.ntex);
    }
    else
    {
        info.n = ng;
    }
    info.t = t;
    info.b = b;
    return info;
}

vec4 getBaseColor(vec4 u_ColorFactor, int iUVSet)
{
    vec4 baseColor = vec4(1);

    // if(bool(MATERIAL_SPECULARGLOSSINESS))
    // {
    //     baseColor = u_DiffuseFactor;
    // }
    // else if(bool(MATERIAL_METALLICROUGHNESS))
    if(bool(iMaterialFlags & PL_MATERIAL_METALLICROUGHNESS))
    {
        // baseColor = u_BaseColorFactor;
        baseColor = u_ColorFactor;
    }

    // if(bool(MATERIAL_SPECULARGLOSSINESS) && bool(HAS_DIFFUSE_MAP))
    // {
    //     // baseColor *= texture(u_DiffuseSampler, getDiffuseUV());
    //     baseColor *= texture(u_DiffuseSampler, tShaderIn.tUV);
    // }
    // else if(bool(MATERIAL_METALLICROUGHNESS) && bool(HAS_BASE_COLOR_MAP))
    if(bool(iMaterialFlags & PL_MATERIAL_METALLICROUGHNESS) && bool(iTextureMappingFlags & PL_HAS_BASE_COLOR_MAP))
    {
        tMaterial material = tMaterialInfo.atMaterials[tObjectInfo.iMaterialIndex];
        baseColor *= texture(sampler2D(at2DTextures[nonuniformEXT(material.iBaseColorTexIdx)], tDefaultSampler), tShaderIn.tUV[iUVSet]);
    }
    return baseColor * tShaderIn.tColor;
}

MaterialInfo getMetallicRoughnessInfo(MaterialInfo info, float u_MetallicFactor, float u_RoughnessFactor, int UVSet)
{
    info.metallic = u_MetallicFactor;
    info.perceptualRoughness = u_RoughnessFactor;

    if(bool(iTextureMappingFlags & PL_HAS_METALLIC_ROUGHNESS_MAP))
    {
        // Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
        // This layout intentionally reserves the 'r' channel for (optional) occlusion map data
        tMaterial material = tMaterialInfo.atMaterials[tObjectInfo.iMaterialIndex];
        vec4 mrSample = texture(sampler2D(at2DTextures[nonuniformEXT(material.iMetallicRoughnessTexIdx)], tDefaultSampler), tShaderIn.tUV[UVSet]);
        info.perceptualRoughness *= mrSample.g;
        info.metallic *= mrSample.b;
    }

    // Achromatic f0 based on IOR.
    info.c_diff = mix(info.baseColor.rgb,  vec3(0), info.metallic);
    info.f0 = mix(info.f0, info.baseColor.rgb, info.metallic);
    return info;
}

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

vec3 getDiffuseLight(vec3 n, int iProbeIndex)
{


    return texture(samplerCube(atCubeTextures[nonuniformEXT(tProbeData.atData[iProbeIndex].uLambertianEnvSampler)], tEnvSampler), n).rgb;
}


vec4 getSpecularSample(vec3 reflection, float lod, int iProbeIndex)
{
    return textureLod(samplerCube(atCubeTextures[nonuniformEXT(tProbeData.atData[iProbeIndex].uGGXEnvSampler)], tEnvSampler), reflection, lod);
}

vec3 getIBLRadianceGGX(vec3 n, vec3 v, float roughness, vec3 F0, float specularWeight, int u_MipCount, int iProbeIndex)
{
    float NdotV = clampedDot(n, v);
    float lod = roughness * float(u_MipCount - 1);
    vec3 reflection = normalize(reflect(-v, n));

    vec2 brdfSamplePoint = clamp(vec2(NdotV, roughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
    vec2 f_ab = texture(sampler2D(at2DTextures[nonuniformEXT(tProbeData.atData[iProbeIndex].uGGXLUT)], tEnvSampler), brdfSamplePoint).rg;

    if(bool(tProbeData.atData[iProbeIndex].iParallaxCorrection))
    {

        // Find the ray intersection with box plane
        vec3 FirstPlaneIntersect = (tProbeData.atData[iProbeIndex].tMax.xyz - tShaderIn.tPosition.xyz) / reflection;
        vec3 SecondPlaneIntersect = (tProbeData.atData[iProbeIndex].tMin.xyz - tShaderIn.tPosition.xyz) / reflection;

        // Get the furthest of these intersections along the ray
        // (Ok because x/0 give +inf and -x/0 give –inf )
        vec3 FurthestPlane = max(FirstPlaneIntersect, SecondPlaneIntersect);

        // Find the closest far intersection
        float Distance = min(min(FurthestPlane.x, FurthestPlane.y), FurthestPlane.z);

        // Get the intersection position
        vec3 IntersectPositionWS = tShaderIn.tPosition.xyz + reflection * Distance;
        // Get corrected reflection
        reflection = IntersectPositionWS - tProbeData.atData[iProbeIndex].tPosition;
    }

    vec4 specularSample = getSpecularSample(reflection, lod, iProbeIndex);

    vec3 specularLight = specularSample.rgb;

    // see https://bruop.github.io/ibl/#single_scattering_results at Single Scattering Results
    // Roughness dependent fresnel, from Fdez-Aguera
    vec3 Fr = max(vec3(1.0 - roughness), F0) - F0;
    vec3 k_S = F0 + Fr * pow(1.0 - NdotV, 5.0);
    vec3 FssEss = k_S * f_ab.x + f_ab.y;

    return specularWeight * specularLight * FssEss;
}


// specularWeight is introduced with KHR_materials_specular
vec3 getIBLRadianceLambertian(vec3 n, vec3 v, float roughness, vec3 diffuseColor, vec3 F0, float specularWeight, int iProbeIndex)
{
    float NdotV = clampedDot(n, v);
    vec2 brdfSamplePoint = clamp(vec2(NdotV, roughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
    vec2 f_ab = texture(sampler2D(at2DTextures[nonuniformEXT(tProbeData.atData[iProbeIndex].uGGXLUT)], tEnvSampler), brdfSamplePoint).rg;

    // if(bool(tProbeData.atData[iProbeIndex].iParallaxCorrection))
    // {

    //     // Find the ray intersection with box plane
    //     vec3 FirstPlaneIntersect = (tProbeData.atData[iProbeIndex].tMax.xyz - tShaderIn.tPosition.xyz) / n;
    //     vec3 SecondPlaneIntersect = (tProbeData.atData[iProbeIndex].tMin.xyz - tShaderIn.tPosition.xyz) / n;

    //     // Get the furthest of these intersections along the ray
    //     // (Ok because x/0 give +inf and -x/0 give –inf )
    //     vec3 FurthestPlane = max(FirstPlaneIntersect, SecondPlaneIntersect);

    //     // Find the closest far intersection
    //     float Distance = min(min(FurthestPlane.x, FurthestPlane.y), FurthestPlane.z);

    //     // Get the intersection position
    //     vec3 IntersectPositionWS = tShaderIn.tPosition.xyz + n * Distance;
    //     // Get corrected reflection
    //     n = IntersectPositionWS - tProbeData.atData[iProbeIndex].tPosition;

    // }

    vec3 irradiance = getDiffuseLight(n, iProbeIndex);

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

float filterPCF2(vec4 sc, vec2 offset, int textureIndex)
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

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void main() 
{

    tMaterial material = tMaterialInfo.atMaterials[tObjectInfo.iMaterialIndex];
    vec4 tBaseColor = getBaseColor(material.u_BaseColorFactor, material.BaseColorUVSet);

    if(tBaseColor.a <  material.u_AlphaCutoff)
    {
        discard;
    }

    NormalInfo tNormalInfo = pl_get_normal_info(material.NormalUVSet);

    vec3 n = tNormalInfo.n;
    vec3 t = tNormalInfo.t;
    vec3 b = tNormalInfo.b;

    MaterialInfo materialInfo;
    materialInfo.baseColor = tBaseColor.rgb;

    // The default index of refraction of 1.5 yields a dielectric normal incidence reflectance of 0.04.
    materialInfo.f0 = vec3(0.04);
    float specularWeight = 1.0;

    if(bool(iMaterialFlags & PL_MATERIAL_METALLICROUGHNESS))
    {
        materialInfo = getMetallicRoughnessInfo(materialInfo, material.u_MetallicFactor, material.u_RoughnessFactor, material.MetallicRoughnessUVSet);
    }

    materialInfo.perceptualRoughness = clamp(materialInfo.perceptualRoughness, 0.0, 1.0);
    materialInfo.metallic = clamp(materialInfo.metallic, 0.0, 1.0);

    // Roughness is authored as perceptual roughness; as is convention,
    // convert to material roughness by squaring the perceptual roughness.
    materialInfo.alphaRoughness = materialInfo.perceptualRoughness * materialInfo.perceptualRoughness;

    // Anything less than 2% is physically impossible and is instead considered to be shadowing. Compare to "Real-Time-Rendering" 4th editon on page 325.
    materialInfo.f90 = vec3(1.0);

    // emissive
    vec3 f_emissive = material.u_EmissiveFactor;
    if(bool(iTextureMappingFlags & PL_HAS_EMISSIVE_MAP))
    {
        f_emissive *= texture(sampler2D(at2DTextures[nonuniformEXT(material.iEmissiveTexIdx)], tDefaultSampler), tShaderIn.tUV[material.EmissiveUVSet]).rgb;
    }
    
    // ambient occlusion
    float ao = 1.0;
    if(bool(iTextureMappingFlags & PL_HAS_OCCLUSION_MAP))
    {
        ao = texture(sampler2D(at2DTextures[nonuniformEXT(material.iOcclusionTexIdx)], tDefaultSampler), tShaderIn.tUV[material.OcclusionUVSet]).r;
    }

    // fill g-buffer
    // outEmissive = vec4(f_emissive, material.u_MipCount);
    // outAlbedo = tBaseColor;
    // outNormal = vec4(tNormalInfo.n, 1.0);
    // outPosition = vec4(tShaderIn.tPosition, materialInfo.specularWeight);
    // outAOMetalnessRoughness = vec4(ao, materialInfo.metallic, materialInfo.perceptualRoughness, 1.0);

    vec3 v = normalize(tGlobalInfo.data[tObjectInfo.uGlobalIndex].tCameraPos.xyz - tShaderIn.tPosition.xyz);

    // LIGHTING
    vec3 f_specular = vec3(0.0);
    vec3 f_diffuse = vec3(0.0);

    // Calculate lighting contribution from image based lighting source (IBL)
    if(bool(iRenderingFlags & PL_RENDERING_FLAG_USE_IBL) && iProbeCount > 0)
    {
        int iProbeIndex = 0;
        float fCurrentDistance = 10000.0;
        for(int i = iProbeCount - 1; i > -1; i--)
        {
            vec3 tDist = tProbeData.atData[i].tPosition - tShaderIn.tPosition.xyz;
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
            f_specular +=  getIBLRadianceGGX(n, v, materialInfo.perceptualRoughness, materialInfo.f0, specularWeight, iMips, iProbeIndex);
            f_diffuse += getIBLRadianceLambertian(n, v, materialInfo.perceptualRoughness, materialInfo.c_diff, materialInfo.f0, specularWeight, iProbeIndex);
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

        // point & spot lights
        for(int i = 0; i < iLightCount; i++)
        {

            plLightData tLightData = tLightInfo.atData[i];

            float shadow = 1.0;

            if(tLightData.iType == 0) // directional lights
            {

                vec3 pointToLight = -tLightData.tDirection;

                if(bShadows && tLightData.iCastShadow > 0)
                {
                    plLightShadowData tShadowData = tDShadowData.atData[tLightData.iShadowIndex];

                    // Get cascade index for the current fragment's view position
                    
                    vec4 inViewPos = tGlobalInfo.data[tObjectInfo.uGlobalIndex].tCameraView * vec4(tShaderIn.tPosition.xyz, 1.0);
                    for(uint j = 0; j < tLightData.iCascadeCount - 1; ++j)
                    {
                        if(inViewPos.z > tShadowData.cascadeSplits[j])
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
                    vec4 shadowCoord = (abiasMat * tShadowData.viewProjMat[cascadeIndex]) * vec4(tShaderIn.tPosition.xyz, 1.0);	
                    shadow = 0;
                    // shadow = textureProj(shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset) + vec2(cascadeIndex * tShadowData.fFactor, 0), tShadowData.iShadowMapTexIdx);
                    shadow = filterPCF(shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset) + vec2(cascadeIndex * tShadowData.fFactor, 0), tShadowData.iShadowMapTexIdx);

                    // for(int j = 0; j < 4; j++)
                    // {
                    //     // int index = int(16.0*random(gl_FragCoord.xyy, j))%16;
                    //     int index = int(16.0*random(tShaderIn.tPosition.xxx, j))%16;
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
                    f_diffuse += shadow * intensity * NdotL *  BRDF_lambertian(materialInfo.f0, materialInfo.f90, materialInfo.c_diff, specularWeight, VdotH);
                    f_specular += shadow * intensity * NdotL * BRDF_specularGGX(materialInfo.f0, materialInfo.f90, materialInfo.alphaRoughness, specularWeight, VdotH, NdotL, NdotV, NdotH);
                }
            }

            else if(tLightData.iType == 1) // point lights
            {
                vec3 pointToLight = tLightData.tPosition - tShaderIn.tPosition.xyz;

                if(bShadows && tLightData.iCastShadow > 0)
                {
                    plLightShadowData tShadowData = tShadowData.atData[tLightData.iShadowIndex];

                    vec3 result = sampleCube(-normalize(pointToLight));
                    vec4 shadowCoord = tShadowData.viewProjMat[int(result.z)] * vec4(tShaderIn.tPosition.xyz, 1.0);
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
                        // dist = 1 - dist * shadowCoord.w;

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
                    f_diffuse += shadow * intensity * NdotL *  BRDF_lambertian(materialInfo.f0, materialInfo.f90, materialInfo.c_diff, specularWeight, VdotH);
                    f_specular += shadow * intensity * NdotL * BRDF_specularGGX(materialInfo.f0, materialInfo.f90, materialInfo.alphaRoughness, specularWeight, VdotH, NdotL, NdotV, NdotH);
                }
            }

            else if(tLightData.iType == 2) // spot lights
            {
                vec3 pointToLight = tLightData.tPosition - tShaderIn.tPosition.xyz;

                if(bShadows && tLightData.iCastShadow > 0)
                {
                    plLightShadowData tShadowData = tShadowData.atData[tLightData.iShadowIndex];

                    vec4 shadowCoord = tShadowData.viewProjMat[0] * vec4(tShaderIn.tPosition.xyz, 1.0);
                    if(shadowCoord.z > -1.0 && shadowCoord.z < 1.0)
                    {
                        shadowCoord.xyz /= shadowCoord.w;
                        shadow = 1.0;
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
                    f_diffuse += shadow * intensity * NdotL *  BRDF_lambertian(materialInfo.f0, materialInfo.f90, materialInfo.c_diff, specularWeight, VdotH);
                    f_specular += shadow * intensity * NdotL * BRDF_specularGGX(materialInfo.f0, materialInfo.f90, materialInfo.alphaRoughness, specularWeight, VdotH, NdotL, NdotV, NdotH);
                }
            }


        }

    }

    // Layer blending
    vec3 diffuse;
    vec3 specular;

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

    vec3 color = f_emissive.rgb + diffuse + specular;

    // outColor = vec4(linearTosRGB(color.rgb), tBaseColor.a);
    outColor = vec4(color.rgb, tBaseColor.a);
    // outColor = vec4(n, 1.0);
    // outColor = vec4(tNormalInfo.ng, tBaseColor.a);
    // outColor = vec4(tNormalInfo.ntex, tBaseColor.a);

    // if(gl_FragCoord.x < 600.0)
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