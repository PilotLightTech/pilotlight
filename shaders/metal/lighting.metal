#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

//-----------------------------------------------------------------------------
// [SECTION] specialization constants
//-----------------------------------------------------------------------------

constant int iRenderingFlags [[ function_constant(0) ]];
constant int iLightCount [[ function_constant(1) ]];

//-----------------------------------------------------------------------------
// [SECTION] defines & structs
//-----------------------------------------------------------------------------

constant float M_PI = 3.141592653589793;
constant const float GAMMA = 2.2;
constant const float INV_GAMMA = 1.0 / GAMMA;

// iRenderingFlags
#define PL_RENDERING_FLAG_USE_PUNCTUAL 1 << 0
#define PL_RENDERING_FLAG_USE_IBL      1 << 1

#define PL_LIGHT_TYPE_DIRECTIONAL 0
#define PL_LIGHT_TYPE_POINT 1

struct tMaterial
{
    // Metallic Roughness
    int   u_MipCount;
    float u_MetallicFactor;
    float u_RoughnessFactor;
    //-------------------------- ( 16 bytes )

    float4 u_BaseColorFactor;
    //-------------------------- ( 16 bytes )

    // Clearcoat
    float u_ClearcoatFactor;
    float u_ClearcoatRoughnessFactor;
    int _unused2[2];
    //-------------------------- ( 16 bytes )

    // Specular
    packed_float3 u_KHR_materials_specular_specularColorFactor;
    float u_KHR_materials_specular_specularFactor;
    //-------------------------- ( 16 bytes )

    // Iridescence
    float u_IridescenceFactor;
    float u_IridescenceIor;
    float u_IridescenceThicknessMinimum;
    float u_IridescenceThicknessMaximum;
    //-------------------------- ( 16 bytes )

    // Emissive Strength
    packed_float3 u_EmissiveFactor;
    float u_EmissiveStrength;
    //-------------------------- ( 16 bytes )
    

    // IOR
    float u_Ior;

    // Alpha mode
    float u_AlphaCutoff;
    float u_OcclusionStrength;
    float u_Unuses;
    //-------------------------- ( 16 bytes )

    int BaseColorUVSet;
    int NormalUVSet;
    int EmissiveUVSet;
    int OcclusionUVSet;
    //-------------------------- ( 16 bytes )

    int MetallicRoughnessUVSet;
    int ClearcoatUVSet;
    int ClearcoatRoughnessUVSet;
    int ClearcoatNormalUVSet;
    //-------------------------- ( 16 bytes )

    int SpecularUVSet;
    int SpecularColorUVSet;
    int IridescenceUVSet;
    int IridescenceThicknessUVSet;
    //-------------------------- ( 16 bytes )
};

//-----------------------------------------------------------------------------
// [SECTION] bind group 0
//-----------------------------------------------------------------------------

struct BindGroupData_0
{
    float4   tCameraPos;
    float4x4 tCameraView;
    float4x4 tCameraProjection;   
    float4x4 tCameraViewProjection;   
};

struct BindGroup_0
{
    device BindGroupData_0 *data;  
    device float4 *atVertexData;
    device tMaterial *atMaterials;
    sampler          tDefaultSampler;
    sampler          tEnvSampler;
    texturecube<float> u_LambertianEnvSampler;
    texturecube<float> u_GGXEnvSampler;
    texture2d<float> u_GGXLUT;
};

//-----------------------------------------------------------------------------
// [SECTION] bind group 1
//-----------------------------------------------------------------------------

struct BindGroup_1
{
    texture2d<float> tAlbedoTexture;
    texture2d<float> tNormalTexture;
    texture2d<float> tPositionTexture;
    texture2d<float> tEmissiveTexture;
    texture2d<float> tAOMetalRoughnessTexture;
    texture2d<float> tDepthTexture;
};

//-----------------------------------------------------------------------------
// [SECTION] bind group 2
//-----------------------------------------------------------------------------

struct plLightData
{
    packed_float3 tPosition;
    float         fIntensity;

    packed_float3 tDirection;
    int           iType;

    packed_float3 tColor;
    float         fRange;

    int iShadowIndex;
    int iCascadeCount;
    int padding[2];
};

struct plLightShadowData
{
	float4 cascadeSplits;
	float4x4 cascadeViewProjMat[4];
};

struct BindGroup_2
{
    device plLightData* atData;  
    device plLightShadowData* atShadowData;
    texture2d_array<float> shadowmap;
    sampler          tShadowSampler;
};

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

struct DynamicData
{
    int iDataOffset;
    int iVertexOffset;
    int iPadding[2];
};

//-----------------------------------------------------------------------------
// [SECTION] input
//-----------------------------------------------------------------------------

struct VertexIn {
    float3 tPosition [[attribute(0)]];
};

//-----------------------------------------------------------------------------
// [SECTION] helpers
//-----------------------------------------------------------------------------

float3
linearTosRGB(float3 color)
{
    return fast::pow(color, float3(INV_GAMMA));
}

static inline __attribute__((always_inline))
float clampedDot(thread const float3& x, thread const float3& y)
{
    return fast::clamp(dot(x, y), 0.0, 1.0);
}

static inline __attribute__((always_inline))
float3 F_Schlick(thread const float3& f0, thread const float3& f90, float VdotH)
{
    return f0 + (f90 - f0) * fast::pow(fast::clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

static inline __attribute__((always_inline))
float F_Schlick(float f0, float f90, float VdotH)
{
    float x = fast::clamp(1.0 - VdotH, 0.0, 1.0);
    float x2 = x * x;
    float x5 = x * x2 * x2;
    return f0 + (f90 - f0) * x5;
}

static inline __attribute__((always_inline))
float F_Schlick(float f0, float VdotH)
{
    float f90 = 1.0; //fast::clamp(50.0 * f0, 0.0, 1.0);
    return F_Schlick(f0, f90, VdotH);
}

static inline __attribute__((always_inline))
float3 F_Schlick(thread const float3& f0, float f90, float VdotH)
{
    float x = fast::clamp(1.0 - VdotH, 0.0, 1.0);
    float x2 = x * x;
    float x5 = x * x2 * x2;
    return f0 + (f90 - f0) * x5;
}

static inline __attribute__((always_inline))
float3 F_Schlick(thread const float3& f0, float VdotH)
{
    float f90 = 1.0; //fast::clamp(dot(f0, float3(50.0 * 0.33)), 0.0, 1.0);
    return F_Schlick(f0, f90, VdotH);
}

static inline __attribute__((always_inline))
float3 Schlick_to_F0(thread const float3& f, thread const float3& f90, float VdotH)
{
    float x = fast::clamp(1.0 - VdotH, 0.0, 1.0);
    float x2 = x * x;
    float x5 = fast::clamp(x * x2 * x2, 0.0, 0.9999);

    return (f - f90 * x5) / (1.0 - x5);
}

static inline __attribute__((always_inline))
float Schlick_to_F0(float f, float f90, float VdotH)
{
    float x = fast::clamp(1.0 - VdotH, 0.0, 1.0);
    float x2 = x * x;
    float x5 = fast::clamp(x * x2 * x2, 0.0, 0.9999);

    return (f - f90 * x5) / (1.0 - x5);
}

static inline __attribute__((always_inline))
float3 Schlick_to_F0(thread const float3& f, float VdotH)
{
    return Schlick_to_F0(f, float3(1.0), VdotH);
}

static inline __attribute__((always_inline))
float Schlick_to_F0(float f, float VdotH)
{
    return Schlick_to_F0(f, 1.0, VdotH);
}

static inline __attribute__((always_inline))
float V_GGX(float NdotL, float NdotV, float alphaRoughness)
{
    float alphaRoughnessSq = alphaRoughness * alphaRoughness;

    float GGXV = NdotL * fast::sqrt(NdotV * NdotV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
    float GGXL = NdotV * fast::sqrt(NdotL * NdotL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);

    float GGX = GGXV + GGXL;
    if (GGX > 0.0)
    {
        return 0.5 / GGX;
    }
    return 0.0;
}

static inline __attribute__((always_inline))
float D_GGX(float NdotH, float alphaRoughness)
{
    float alphaRoughnessSq = alphaRoughness * alphaRoughness;
    float f = (NdotH * NdotH) * (alphaRoughnessSq - 1.0) + 1.0;
    return alphaRoughnessSq / (M_PI * f * f);
}

static inline __attribute__((always_inline))
float3 BRDF_lambertian(thread const float3& f0, thread const float3& f90, thread const float3& diffuseColor, float specularWeight, float VdotH)
{
    return (1.0 - specularWeight * F_Schlick(f0, f90, VdotH)) * (diffuseColor / M_PI);
}

static inline __attribute__((always_inline))
float3 BRDF_specularGGX(thread const float3& f0, thread const float3& f90, float alphaRoughness, float specularWeight, float VdotH, float NdotL, float NdotV, float NdotH)
{
    float3 F = F_Schlick(f0, f90, VdotH);
    float Vis = V_GGX(NdotL, NdotV, alphaRoughness);
    float D = D_GGX(NdotH, alphaRoughness);
    return specularWeight * F * Vis * D;
}

static inline __attribute__((always_inline))
float3 getDiffuseLight(device const BindGroup_0& bg0, float3  n)
{
    n.z = -n.z;
    return bg0.u_LambertianEnvSampler.sample(bg0.tEnvSampler, n).rgb;
}

static inline __attribute__((always_inline))
float4 getSpecularSample(device const BindGroup_0& bg0, float3 reflection, float lod)
{
    reflection.z = -reflection.z;
    return bg0.u_GGXEnvSampler.sample(bg0.tEnvSampler, reflection, level(lod));
}

static float3
getIBLRadianceGGX(device const BindGroup_0& bg0, thread const float3& n, thread const float3& v, float roughness, thread const float3& F0, float specularWeight, int u_MipCount)
{
    float NdotV = clampedDot(n, v);
    float lod = roughness * float(u_MipCount - 1);
    float3 reflection = fast::normalize(reflect(-v, n));

    float2 brdfSamplePoint = fast::clamp(float2(NdotV, roughness), float2(0.0, 0.0), float2(1.0, 1.0));
    float2 f_ab = bg0.u_GGXLUT.sample(bg0.tEnvSampler, brdfSamplePoint).rg;
    float4 specularSample = getSpecularSample(bg0, reflection, lod);

    float3 specularLight = specularSample.rgb;

    float3 Fr = fast::max(float3(1.0 - roughness), F0) - F0;
    float3 k_S = F0 + Fr * fast::pow(1.0 - NdotV, 5.0);
    float3 FssEss = (k_S * f_ab.x) + float3(f_ab.y);

    return (specularWeight * specularLight) * FssEss;
}

static float3
getIBLRadianceLambertian(device const BindGroup_0& bg0, thread const float3& n, thread const float3& v, float roughness, thread const float3& diffuseColor, thread const float3& F0, float specularWeight)
{
    float NdotV = clampedDot(n, v);
    float2 brdfSamplePoint = fast::clamp(float2(NdotV, roughness), float2(0.0, 0.0), float2(1.0, 1.0));
    float2 f_ab = bg0.u_GGXLUT.sample(bg0.tEnvSampler, brdfSamplePoint).rg;

    float3 irradiance = getDiffuseLight(bg0, n);

    float3 Fr = fast::max(float3(1.0 - roughness), F0) - F0;
    float3 k_S = F0 + Fr * fast::pow(1.0 - NdotV, 5.0);
    float3 FssEss = specularWeight * k_S * f_ab.x + f_ab.y;

    float Ems = (1.0 - (f_ab.x + f_ab.y));
    float3 F_avg = specularWeight * (F0 + (1.0 - F0) / 21.0);
    float3 FmsEms = Ems * FssEss * F_avg / (1.0 - F_avg * Ems);
    float3 k_D = diffuseColor * (1.0 - FssEss + FmsEms);

    return (FmsEms + k_D) * irradiance;
}

static inline __attribute__((always_inline))
float getRangeAttenuation(float range, float distance)
{
    if (range <= 0.0)
    {
        // negative range means unlimited
        return 1.0 / fast::pow(distance, 2.0);
    }
    return fast::max(fast::min(1.0 - fast::pow(distance / range, 4.0), 1.0), 0.0) / fast::pow(distance, 2.0);
}


static float3 getLighIntensity(device const plLightData& light, thread const float3& pointToLight)
{
    float rangeAttenuation = 1.0;

    if (light.iType != PL_LIGHT_TYPE_DIRECTIONAL)
    {
        rangeAttenuation = getRangeAttenuation(light.fRange, fast::length(pointToLight));
    }


    return rangeAttenuation * light.fIntensity * light.tColor;
}

constant const float4x4 biasMat = float4x4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 
);

float textureProj(device const BindGroup_2& bg2, float4 shadowCoord, float2 offset, uint cascadeIndex)
{
	float shadow = 1.0;
	float bias = 0.0005;

	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 )
    {
        // float dist = bg2.shadowmap[cascadeIndex].sample(bg2.tShadowSampler, shadowCoord.xy + offset).r;
        float dist = bg2.shadowmap.sample(bg2.tShadowSampler, shadowCoord.xy + offset, cascadeIndex).r;
		if (shadowCoord.w > 0 && dist < shadowCoord.z - bias)
        {
			shadow = 0.1; // ambient
		}
	}
	return shadow;
}

float filterPCF(device const BindGroup_2& bg2, float4 sc, uint cascadeIndex)
{
	int2 texDim = int2(0, 0);
	// texDim.x = bg2.shadowmap[cascadeIndex].get_width();
	// texDim.y = bg2.shadowmap[cascadeIndex].get_height();
	texDim.x = bg2.shadowmap.get_width();
	texDim.y = bg2.shadowmap.get_height();
	float scale = 0.75;
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 1;
	
	for (int x = -range; x <= range; x++)
    {
		for (int y = -range; y <= range; y++)
        {
			shadowFactor += textureProj(bg2, sc, float2(dx*x, dy*y), cascadeIndex);
			count++;
		}
	}
	return shadowFactor / count;
}

//-----------------------------------------------------------------------------
// [SECTION] output
//-----------------------------------------------------------------------------

struct VertexOut {
    float4 tPositionOut [[position]];
    float2 tUV;
};

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

vertex VertexOut vertex_main(
    uint                vertexID [[ vertex_id ]],
    VertexIn            in [[stage_in]],
    device const BindGroup_0& bg0 [[ buffer(1) ]],
    device const BindGroup_1& bg1 [[ buffer(2) ]],
    device const BindGroup_2& bg2 [[ buffer(3) ]],
    device const DynamicData& tObjectInfo [[ buffer(4) ]]
    )
{

    VertexOut tShaderOut;
    float3 inPosition = in.tPosition;
    float2 inTexCoord0 = float2(0.0, 0.0);

    int iCurrentAttribute = 0;
    const uint iVertexDataOffset = 1 * (vertexID - tObjectInfo.iVertexOffset) + tObjectInfo.iDataOffset;
    inTexCoord0 = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].xy;  iCurrentAttribute++;
    tShaderOut.tPositionOut = float4(inPosition, 1.0);
    tShaderOut.tUV = inTexCoord0;
    tShaderOut.tPositionOut.y = tShaderOut.tPositionOut.y * -1.0;
    return tShaderOut;
}

fragment float4 fragment_main(
    VertexOut in [[stage_in]],
    device const BindGroup_0& bg0 [[ buffer(1) ]],
    device const BindGroup_1& bg1 [[ buffer(2) ]],
    device const BindGroup_2& bg2 [[ buffer(3) ]],
    device const DynamicData& tObjectInfo [[ buffer(4) ]],
    bool front_facing [[front_facing]]
    )
{
    float4 outColor = float4(0);

    float4 tBaseColor = bg1.tAlbedoTexture.sample(bg0.tEnvSampler, in.tUV);
    float4 tPosition = bg1.tPositionTexture.sample(bg0.tEnvSampler, in.tUV);
    float specularWeight = tPosition.a;

    float3 n = bg1.tNormalTexture.sample(bg0.tEnvSampler, in.tUV).xyz;

    float4 AORoughnessMetalnessData = bg1.tAOMetalRoughnessTexture.sample(bg0.tEnvSampler, in.tUV);
    const float fPerceptualRoughness = AORoughnessMetalnessData.b;
    const float fMetalness = AORoughnessMetalnessData.g;
    const float fAlphaRoughness = fPerceptualRoughness * fPerceptualRoughness;
    const float ao = AORoughnessMetalnessData.r;
    const float3 f90 = float3(1.0);

    float3 c_diff = mix(tBaseColor.rgb,  float3(0), fMetalness);
    float3 f0 = mix(float3(0.04), tBaseColor.rgb, fMetalness);

    float3 v = normalize(bg0.data->tCameraPos.xyz - tPosition.xyz);

    // LIGHTING
    float3 f_specular = float3(0.0);
    float3 f_diffuse = float3(0.0);
    float4 f_emissive = bg1.tEmissiveTexture.sample(bg0.tEnvSampler, in.tUV);
    int iMips = int(f_emissive.a);
    float3 f_clearcoat = float3(0.0);
    float3 f_sheen = float3(0.0);

    // IBL STUFF
    if(bool(iRenderingFlags & PL_RENDERING_FLAG_USE_IBL))
    {
        f_specular +=  getIBLRadianceGGX(bg0, n, v, fPerceptualRoughness, f0, specularWeight, iMips);
        f_diffuse += getIBLRadianceLambertian(bg0, n, v, fPerceptualRoughness, c_diff, f0, specularWeight);
    }

    // punctual stuff
    float3 f_diffuse_ibl = f_diffuse;
    float3 f_specular_ibl = f_specular;
    float3 f_sheen_ibl = f_sheen;
    float3 f_clearcoat_ibl = f_clearcoat;
    f_diffuse = float3(0.0);
    f_specular = float3(0.0);
    f_sheen = float3(0.0);
    f_clearcoat = float3(0.0);

    uint cascadeIndex = 0;
    if(bool(iRenderingFlags & PL_RENDERING_FLAG_USE_PUNCTUAL))
    {
        for(int i = 0; i < iLightCount; i++)
        {
            device const plLightData& tLightData = bg2.atData[i];
            float3 pointToLight;
            float shadow = 1.0;

            if(tLightData.iCascadeCount > 0)
            {
                plLightShadowData tShadowData = bg2.atShadowData[tLightData.iShadowIndex];

                // Get cascade index for the current fragment's view position
                
                float4 inViewPos = bg0.data->tCameraView * float4(tPosition.xyz, 1.0);
                for(uint j = 0; j < tLightData.iCascadeCount - 1; ++j)
                {
                    if(inViewPos.z > tShadowData.cascadeSplits[j])
                    {	
                        cascadeIndex = j + 1;
                    }
                }  

                // Depth compare for shadowing
	            float4 shadowCoord = (biasMat * tShadowData.cascadeViewProjMat[cascadeIndex]) * float4(tPosition.xyz, 1.0);	
                shadow = 0;
                shadow = textureProj(bg2, shadowCoord / shadowCoord.w, float2(0.0), cascadeIndex);
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
            float3 l = fast::normalize(pointToLight);   // Direction from surface point to light
            float3 h = fast::normalize(l + v);          // Direction of the vector between l and v, called halfway vector
            float NdotL = clampedDot(n, l);
            float NdotV = clampedDot(n, v);
            float NdotH = clampedDot(n, h);
            float VdotH = clampedDot(v, h);
            if (NdotL > 0.0 || NdotV > 0.0)
            {
                float3 intensity = getLighIntensity(tLightData, pointToLight);
                f_diffuse += shadow * intensity * NdotL *  BRDF_lambertian(f0, f90, c_diff, specularWeight, VdotH);
                f_specular += shadow * intensity * NdotL * BRDF_specularGGX(f0, f90, fAlphaRoughness, specularWeight, VdotH, NdotL, NdotV, NdotH);
            }

        }
    }

    // Layer blending

    float albedoSheenScaling = 1.0;
    float clearcoatFactor = 0.0;
    float3 clearcoatFresnel = float3(0);
    float3 diffuse;
    float3 specular;
    float3 sheen;
    float3 clearcoat;

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


    float3 color = f_emissive.rgb + diffuse + specular;
    color = sheen + color * albedoSheenScaling;
    color = color * (1.0 - clearcoatFactor * clearcoatFresnel) + clearcoat;

    outColor = float4(linearTosRGB(color.rgb), tBaseColor.a);

    // if(in.tPositionOut.x < 600.0)
    // {
    //     switch(cascadeIndex) {
    //         case 0 : 
    //             outColor.rgb *= float3(1.0f, 0.25f, 0.25f);
    //             break;
    //         case 1 : 
    //             outColor.rgb *= float3(0.25f, 1.0f, 0.25f);
    //             break;
    //         case 2 : 
    //             outColor.rgb *= float3(0.25f, 0.25f, 1.0f);
    //             break;
    //         case 3 : 
    //             outColor.rgb *= float3(1.0f, 1.0f, 0.25f);
    //             break;
    //     }
    // }

    return outColor;
}