#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "defines.glsl"
#include "material.glsl"
#include "lights.glsl"
#include "math.glsl"

//-----------------------------------------------------------------------------
// [SECTION] specialication constants
//-----------------------------------------------------------------------------

layout(constant_id = 0) const int iRenderingFlags = 0;
layout(constant_id = 1) const int iLightCount = 1;

//-----------------------------------------------------------------------------
// [SECTION] bind group 0
//-----------------------------------------------------------------------------

layout(set = 0, binding = 0) uniform _plGlobalInfo
{
    vec4 tCameraPos;
    mat4 tCameraView;
    mat4 tCameraProjection;
    mat4 tCameraViewProjection;
} tGlobalInfo;

layout(std140, set = 0, binding = 1) readonly buffer _tVertexBuffer
{
	vec4 atVertexData[];
} tVertexBuffer;

layout(set = 0, binding = 2) readonly buffer plMaterialInfo
{
    tMaterial atMaterials[];
} tMaterialInfo;
layout(set = 0, binding = 3)  uniform sampler tDefaultSampler;
layout(set = 0, binding = 4)  uniform sampler tEnvSampler;
layout (set = 0, binding = 5) uniform textureCube u_LambertianEnvSampler;
layout (set = 0, binding = 6) uniform textureCube u_GGXEnvSampler;
layout (set = 0, binding = 7) uniform texture2D u_GGXLUT;

//-----------------------------------------------------------------------------
// [SECTION] bind group 1
//-----------------------------------------------------------------------------

layout(input_attachment_index = 1, set = 1, binding = 0)  uniform subpassInput tAlbedoSampler;
layout(input_attachment_index = 2, set = 1, binding = 1)  uniform subpassInput tNormalTexture;
layout(input_attachment_index = 3, set = 1, binding = 2)  uniform subpassInput tPositionSampler;
layout(input_attachment_index = 4, set = 1, binding = 3)  uniform subpassInput tAOMetalRoughnessTexture;
layout(input_attachment_index = 0, set = 1, binding = 4)  uniform subpassInput tDepthSampler;

//-----------------------------------------------------------------------------
// [SECTION] bind group 2
//-----------------------------------------------------------------------------

layout(set = 2, binding = 0) uniform _plLightInfo
{
    plLightData atData[iLightCount];
} tLightInfo;

layout(set = 2, binding = 1) readonly buffer plShadowData
{
    plLightShadowData atData[];
} tShadowData;

layout (set = 2, binding = 2) uniform texture2D shadowmap[4];
layout(set = 2, binding = 3)  uniform sampler tShadowSampler;

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

layout(set = 3, binding = 0) uniform _plObjectInfo
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

float F_Schlick(float f0, float f90, float VdotH)
{
    float x = clamp(1.0 - VdotH, 0.0, 1.0);
    float x2 = x * x;
    float x5 = x * x2 * x2;
    return f0 + (f90 - f0) * x5;
}

float F_Schlick(float f0, float VdotH)
{
    float f90 = 1.0; //clamp(50.0 * f0, 0.0, 1.0);
    return F_Schlick(f0, f90, VdotH);
}

vec3 F_Schlick(vec3 f0, float f90, float VdotH)
{
    float x = clamp(1.0 - VdotH, 0.0, 1.0);
    float x2 = x * x;
    float x5 = x * x2 * x2;
    return f0 + (f90 - f0) * x5;
}

vec3 F_Schlick(vec3 f0, float VdotH)
{
    float f90 = 1.0; //clamp(dot(f0, vec3(50.0 * 0.33)), 0.0, 1.0);
    return F_Schlick(f0, f90, VdotH);
}

vec3 Schlick_to_F0(vec3 f, vec3 f90, float VdotH) {
    float x = clamp(1.0 - VdotH, 0.0, 1.0);
    float x2 = x * x;
    float x5 = clamp(x * x2 * x2, 0.0, 0.9999);

    return (f - f90 * x5) / (1.0 - x5);
}

float Schlick_to_F0(float f, float f90, float VdotH) {
    float x = clamp(1.0 - VdotH, 0.0, 1.0);
    float x2 = x * x;
    float x5 = clamp(x * x2 * x2, 0.0, 0.9999);

    return (f - f90 * x5) / (1.0 - x5);
}

vec3 Schlick_to_F0(vec3 f, float VdotH) {
    return Schlick_to_F0(f, vec3(1.0), VdotH);
}

float Schlick_to_F0(float f, float VdotH) {
    return Schlick_to_F0(f, 1.0, VdotH);
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

// Estevez and Kulla http://www.aconty.com/pdf/s2017_pbs_imageworks_sheen.pdf
float D_Charlie(float sheenRoughness, float NdotH)
{
    sheenRoughness = max(sheenRoughness, 0.000001); //clamp (0,1]
    float alphaG = sheenRoughness * sheenRoughness;
    float invR = 1.0 / alphaG;
    float cos2h = NdotH * NdotH;
    float sin2h = 1.0 - cos2h;
    return (2.0 + invR) * pow(sin2h, invR * 0.5) / (2.0 * M_PI);
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
    n.z = -n.z;
    return texture(samplerCube(u_LambertianEnvSampler, tEnvSampler), n).rgb;
}


vec4 getSpecularSample(vec3 reflection, float lod)
{
    reflection.z = -reflection.z;
    return textureLod(samplerCube(u_GGXEnvSampler, tEnvSampler), reflection, lod);
}

vec3 getIBLRadianceGGX(vec3 n, vec3 v, float roughness, vec3 F0, float specularWeight, int u_MipCount)
{
    
    float lod = roughness * float(u_MipCount - 1);
    vec3 reflection = normalize(reflect(-v, n));
    vec4 specularSample = getSpecularSample(reflection, lod);

    float NdotV = clampedDot(n, v);
    vec2 brdfSamplePoint = clamp(vec2(NdotV, roughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
    vec2 f_ab = texture(sampler2D(u_GGXLUT, tEnvSampler), brdfSamplePoint).rg;

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
    vec2 f_ab = texture(sampler2D(u_GGXLUT, tEnvSampler), brdfSamplePoint).rg;

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

float textureProj(vec4 shadowCoord, vec2 offset, uint cascadeIndex)
{
	float shadow = 1.0;
	float bias = 0.0005;
    float comp = shadowCoord.z - bias;
    vec2 comp2 = shadowCoord.st + offset;

	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 )
    {
		float dist = texture(sampler2D(shadowmap[cascadeIndex], tShadowSampler), comp2).r;
		if (shadowCoord.w > 0 && dist < comp)
        {
			shadow = 0.1; // ambient
		}
	}
	return shadow;
}

float filterPCF(vec4 sc, uint cascadeIndex)
{
	ivec2 texDim = textureSize(sampler2D(shadowmap[cascadeIndex], tShadowSampler), 0).xy;
	float scale = 0.75;
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 1;
	
	for (int x = -range; x <= range; x++) {
		for (int y = -range; y <= range; y++) {
			shadowFactor += textureProj(sc, vec2(dx*x, dy*y), cascadeIndex);
			count++;
		}
	}
	return shadowFactor / count;
}

void main() 
{
    vec4 AORoughnessMetalnessData = subpassLoad(tAOMetalRoughnessTexture);
    vec4 tBaseColor = subpassLoad(tAlbedoSampler);
    vec4 tPosition = subpassLoad(tPositionSampler);
    vec3 n = subpassLoad(tNormalTexture).xyz;

    const vec3 f90 = vec3(1.0);
    

    // LIGHTING
    vec3 f_specular = vec3(0.0);
    vec3 f_diffuse = vec3(0.0);
   
    vec3 f_clearcoat = vec3(0.0);
    vec3 f_sheen = vec3(0.0);
    vec3 f_transmission = vec3(0.0);

    const float fMetalness = AORoughnessMetalnessData.g;
    vec3 c_diff = mix(tBaseColor.rgb,  vec3(0), fMetalness);
    vec3 f0 = mix(vec3(0.04), tBaseColor.rgb, fMetalness);

    const float fPerceptualRoughness = AORoughnessMetalnessData.b;
    float specularWeight = tPosition.a;
    vec3 v = normalize(tGlobalInfo.tCameraPos.xyz - tPosition.xyz);
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
    vec3 f_sheen_ibl = f_sheen;
    vec3 f_clearcoat_ibl = f_clearcoat;
    f_diffuse = vec3(0.0);
    f_specular = vec3(0.0);
    f_sheen = vec3(0.0);
    f_clearcoat = vec3(0.0);

    uint cascadeIndex = 0;
    if(bool(iRenderingFlags & PL_RENDERING_FLAG_USE_PUNCTUAL))
    {
        const float fAlphaRoughness = fPerceptualRoughness * fPerceptualRoughness;

        for(int i = 0; i < iLightCount; i++)
        {
            plLightData tLightData = tLightInfo.atData[i];

            vec3 pointToLight;
            float shadow = 1.0;

            if(tLightData.iCascadeCount > 0)
            {
                plLightShadowData tShadowData = tShadowData.atData[tLightData.iShadowIndex];

                // Get cascade index for the current fragment's view position
                
                vec4 inViewPos = tGlobalInfo.tCameraView * vec4(tPosition.xyz, 1.0);
                for(uint j = 0; j < tLightData.iCascadeCount - 1; ++j)
                {
                    if(inViewPos.z > tShadowData.cascadeSplits[j])
                    {	
                        cascadeIndex = j + 1;
                    }
                }  

                // Depth compare for shadowing
	            vec4 shadowCoord = (biasMat * tShadowData.cascadeViewProjMat[cascadeIndex]) * vec4(tPosition.xyz, 1.0);	
                shadow = 0;
                shadow = textureProj(shadowCoord / shadowCoord.w, vec2(0.0), cascadeIndex);
                // shadow = filterPCF(shadowCoord / shadowCoord.w, cascadeIndex);
            }

            if(tLightData.iType != PL_LIGHT_TYPE_DIRECTIONAL)
            {
                pointToLight = tLightData.tPosition - tPosition.xyz;
            }
            else
            {
                pointToLight = -tLightData.tDirection;
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
                vec3 intensity = getLighIntensity(tLightData, pointToLight);
                f_diffuse += shadow * intensity * NdotL *  BRDF_lambertian(f0, f90, c_diff, specularWeight, VdotH);
                f_specular += shadow * intensity * NdotL * BRDF_specularGGX(f0, f90, fAlphaRoughness, specularWeight, VdotH, NdotL, NdotV, NdotH);
            }

        }

    }

    // Layer blending

    float albedoSheenScaling = 1.0;
    float clearcoatFactor = 0.0;
    vec3 clearcoatFresnel = vec3(0);
    vec3 diffuse;
    vec3 specular;
    vec3 sheen;
    vec3 clearcoat;

    const float ao = AORoughnessMetalnessData.r;
    if(ao != 1.0)
    {
        float u_OcclusionStrength = 1.0;
        diffuse = f_diffuse + mix(f_diffuse_ibl, f_diffuse_ibl * ao, u_OcclusionStrength);
        // apply ambient occlusion to all lighting that is not punctual
        specular = f_specular + mix(f_specular_ibl, f_specular_ibl * ao, u_OcclusionStrength);
        sheen = f_sheen + mix(f_sheen_ibl, f_sheen_ibl * ao, u_OcclusionStrength);
        clearcoat = f_clearcoat + mix(f_clearcoat_ibl, f_clearcoat_ibl * ao, u_OcclusionStrength);
    }
    else
    {
        diffuse = f_diffuse_ibl + f_diffuse;
        specular = f_specular_ibl + f_specular;
        sheen = f_sheen_ibl + f_sheen;
        clearcoat = f_clearcoat_ibl + f_clearcoat;
    }

    vec3 color = diffuse + specular;
    color = sheen + color * albedoSheenScaling;
    color = color * (1.0 - clearcoatFactor * clearcoatFresnel) + clearcoat;

    // outColor = vec4(linearTosRGB(color.rgb), tBaseColor.a);
    outColor = vec4(color.rgb, tBaseColor.a);

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