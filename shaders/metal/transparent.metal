#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

//-----------------------------------------------------------------------------
// [SECTION] specialization constants
//-----------------------------------------------------------------------------

constant int iMeshVariantFlags [[ function_constant(0) ]];
constant int iDataStride [[ function_constant(1) ]];
constant int iTextureMappingFlags [[ function_constant(2) ]];
constant int iMaterialFlags [[ function_constant(3) ]];
constant int iRenderingFlags [[ function_constant(4) ]];
constant int iLightCount [[ function_constant(5) ]];

//-----------------------------------------------------------------------------
// [SECTION] defines & structs
//-----------------------------------------------------------------------------

constant float M_PI = 3.141592653589793;

constant const float GAMMA = 2.2;
constant const float INV_GAMMA = 1.0 / GAMMA;

// iMeshVariantFlags
#define PL_MESH_FORMAT_FLAG_NONE           0
#define PL_MESH_FORMAT_FLAG_HAS_POSITION   1 << 0
#define PL_MESH_FORMAT_FLAG_HAS_NORMAL     1 << 1
#define PL_MESH_FORMAT_FLAG_HAS_TANGENT    1 << 2
#define PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0 1 << 3
#define PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1 1 << 4
#define PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_2 1 << 5
#define PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_3 1 << 6
#define PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_4 1 << 7
#define PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_5 1 << 8
#define PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_6 1 << 9
#define PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_7 1 << 10
#define PL_MESH_FORMAT_FLAG_HAS_COLOR_0    1 << 11
#define PL_MESH_FORMAT_FLAG_HAS_COLOR_1    1 << 12
#define PL_MESH_FORMAT_FLAG_HAS_JOINTS_0   1 << 13
#define PL_MESH_FORMAT_FLAG_HAS_JOINTS_1   1 << 14
#define PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0  1 << 15
#define PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1  1 << 16

// iTextureMappingFlags
#define PL_HAS_BASE_COLOR_MAP            1 << 0
#define PL_HAS_NORMAL_MAP                1 << 1
#define PL_HAS_EMISSIVE_MAP              1 << 2
#define PL_HAS_OCCLUSION_MAP             1 << 3
#define PL_HAS_METALLIC_ROUGHNESS_MAP    1 << 4

// iMaterialFlags
#define PL_MATERIAL_METALLICROUGHNESS  1 << 0

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

struct MaterialInfo
{
    float ior;
    float perceptualRoughness;      // roughness value, as authored by the model creator (input to shader)
    float3 f0;                        // full reflectance color (n incidence angle)

    float alphaRoughness;           // roughness mapped to a more linear change in the roughness (proposed by [2])
    float3 c_diff;

    float3 f90;                       // reflectance color at grazing angle
    float metallic;

    float3 baseColor;

    float sheenRoughnessFactor;
    float3 sheenColorFactor;

    float3 clearcoatF0;
    float3 clearcoatF90;
    float clearcoatFactor;
    float3 clearcoatNormal;
    float clearcoatRoughness;

    // KHR_materials_specular 
    float specularWeight; // product of specularFactor and specularTexture.a

    float transmissionFactor;

    float thickness;
    float3 attenuationColor;
    float attenuationDistance;

    // KHR_materials_iridescence
    float iridescenceFactor;
    float iridescenceIor;
    float iridescenceThickness;

    // KHR_materials_anisotropy
    float3 anisotropicT;
    float3 anisotropicB;
    float anisotropyStrength;
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

struct BindGroup_1
{
    device plLightData* atData;  
    device plLightShadowData* atShadowData;
    texture2d_array<float> shadowmap;
    sampler          tShadowSampler;
};

//-----------------------------------------------------------------------------
// [SECTION] bind group 2
//-----------------------------------------------------------------------------

struct BindGroup_2
{
    texture2d<float> tBaseColorTexture;
    texture2d<float> tNormalTexture;
    texture2d<float> tEmissiveTexture;
    texture2d<float> tMetallicRoughnessTexture;
    texture2d<float> tOcclusionTexture;
    texture2d<float> tClearcoatTexture;
    texture2d<float> tClearcoatRoughnessTexture;
    texture2d<float> tClearcoatNormalTexture;
    texture2d<float> tIridescenceTexture;
    texture2d<float> tIridescenceThicknessTexture;
    texture2d<float> tSpecularTexture;
    texture2d<float> tSpecularColorTexture;
};

struct VertexIn {
    float3 tPosition [[attribute(0)]];
};

struct VertexOut {
    float4 tPositionOut [[position]];
    float3 tPosition;
    float4 tWorldPosition;
    float2 tUV0;
    float2 tUV1;
    float2 tUV2;
    float2 tUV3;
    float2 tUV4;
    float2 tUV5;
    float2 tUV6;
    float2 tUV7;
    float4 tColor;
    float3 tWorldNormal;
    float3 tTBN0;
    float3 tTBN1;
    float3 tTBN2;
};

struct DynamicData
{
    int      iDataOffset;
    int      iVertexOffset;
    int      iMaterialIndex;
    int      iPadding[1];
    float4x4 tModel;
};

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
    float4 inPosition = float4(in.tPosition, 1.0);
    float4 inNormal = float4(0.0, 0.0, 0.0, 0.0);
    float4 inTangent = float4(0.0, 0.0, 0.0, 0.0);
    float2 inTexCoord0 = float2(0.0, 0.0);
    float2 inTexCoord1 = float2(0.0, 0.0);
    float2 inTexCoord2 = float2(0.0, 0.0);
    float2 inTexCoord3 = float2(0.0, 0.0);
    float2 inTexCoord4 = float2(0.0, 0.0);
    float2 inTexCoord5 = float2(0.0, 0.0);
    float2 inTexCoord6 = float2(0.0, 0.0);
    float2 inTexCoord7 = float2(0.0, 0.0);
    float4 inColor0 = float4(1.0, 1.0, 1.0, 1.0);
    float4 inColor1 = float4(0.0, 0.0, 0.0, 0.0);
    int iCurrentAttribute = 0;

    const uint iVertexDataOffset = iDataStride * (vertexID - tObjectInfo.iVertexOffset) + tObjectInfo.iDataOffset;

    
    if(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_POSITION)  { inPosition.xyz = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].xyz; iCurrentAttribute++;}
    if(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_NORMAL)    { inNormal.xyz   = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].xyz; iCurrentAttribute++;}
    if(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TANGENT)   { inTangent      = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}
    if(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0){ inTexCoord0    = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].xy;  iCurrentAttribute++;}
    if(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1){ inTexCoord1    = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].xy;  iCurrentAttribute++;}
    if(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_2){ inTexCoord2    = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].xy;  iCurrentAttribute++;}
    if(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_3){ inTexCoord3    = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].xy;  iCurrentAttribute++;}
    if(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_4){ inTexCoord4    = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].xy;  iCurrentAttribute++;}
    if(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_5){ inTexCoord5    = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].xy;  iCurrentAttribute++;}
    if(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_6){ inTexCoord6    = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].xy;  iCurrentAttribute++;}
    if(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_7){ inTexCoord7    = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].xy;  iCurrentAttribute++;}
    if(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_COLOR_0)   { inColor0       = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}
    if(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_COLOR_1)   { inColor1       = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}

    float4 tWorldNormal4 = tObjectInfo.tModel * fast::normalize(inNormal);
    tShaderOut.tWorldNormal = tWorldNormal4.xyz;
    if(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_NORMAL)
    {

        if(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TANGENT)
        {
            float4 tangent = fast::normalize(inTangent);
            float4 WorldTangent = tObjectInfo.tModel * tangent;
            float4 WorldBitangent = float4(cross(fast::normalize(inNormal).xyz, tangent.xyz) * inTangent.w, 0.0);
            WorldBitangent = tObjectInfo.tModel * WorldBitangent;
            tShaderOut.tTBN0 = WorldTangent.xyz;
            tShaderOut.tTBN1 = WorldBitangent.xyz;
            tShaderOut.tTBN2 = tShaderOut.tWorldNormal.xyz;
        }
    }

    float4 pos = tObjectInfo.tModel * inPosition;
    tShaderOut.tPosition = pos.xyz / pos.w;
    tShaderOut.tPositionOut = bg0.data->tCameraViewProjection * pos;
    tShaderOut.tUV0 = inTexCoord0;
    tShaderOut.tUV1 = inTexCoord1;
    tShaderOut.tUV2 = inTexCoord2;
    tShaderOut.tUV3 = inTexCoord3;
    tShaderOut.tUV4 = inTexCoord4;
    tShaderOut.tUV5 = inTexCoord5;
    tShaderOut.tUV6 = inTexCoord6;
    tShaderOut.tUV7 = inTexCoord7;
    tShaderOut.tWorldPosition = tShaderOut.tPositionOut / tShaderOut.tPositionOut.w;
    tShaderOut.tColor = inColor0;

    tShaderOut.tPositionOut.y = tShaderOut.tPositionOut.y * -1.0;
    return tShaderOut;
}

struct NormalInfo {
    float3 ng;   // Geometry normal
    float3 t;    // Geometry tangent
    float3 b;    // Geometry bitangent
    float3 n;    // Shading normal
    float3 ntex; // Normal from texture, scaling is accounted for.
};

NormalInfo
pl_get_normal_info(device const BindGroup_0& bg0, device const BindGroup_2& bg2, VertexOut tShaderIn, bool front_facing, float2 UV)
{
    float2 uv_dx = dfdx(UV);
    float2 uv_dy = dfdy(UV);

    if (length(uv_dx) <= 1e-2) {
      uv_dx = float2(1.0, 0.0);
    }

    if (length(uv_dy) <= 1e-2) {
      uv_dy = float2(0.0, 1.0);
    }

    float3 t_ = (uv_dy.y * dfdx(tShaderIn.tPosition) - uv_dx.y * dfdy(tShaderIn.tPosition)) /
        (uv_dx.x * uv_dy.y - uv_dy.x * uv_dx.y);

    float3 t, b, ng;

    // Compute geometrical TBN:
    if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_NORMAL))
    {

        if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TANGENT))
        {
            // Trivial TBN computation, present as vertex attribute.
            // Normalize eigenvectors as matrix is linearly interpolated.
            t = fast::normalize(tShaderIn.tTBN0);
            b = fast::normalize(tShaderIn.tTBN1);
            ng = fast::normalize(tShaderIn.tTBN2);
        }
        else
        {
            // Normals are either present as vertex attributes or approximated.
            ng = fast::normalize(tShaderIn.tWorldNormal);
            t = fast::normalize(t_ - ng * dot(ng, t_));
            b = cross(ng, t);
        }
    }
    else
    {
        ng = fast::normalize(cross(dfdx(tShaderIn.tPosition), dfdy(tShaderIn.tPosition)));
        t = fast::normalize(t_ - ng * dot(ng, t_));
        b = cross(ng, t);
    }


    // For a back-facing surface, the tangential basis vectors are negated.
    if (front_facing == false)
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
        info.ntex = bg2.tNormalTexture.sample(bg0.tDefaultSampler, UV).rgb * 2.0 - float3(1.0);
        // info.ntex *= vec3(0.2, 0.2, 1.0);
        // info.ntex *= vec3(u_NormalScale, u_NormalScale, 1.0);
        info.ntex = fast::normalize(info.ntex);
        info.n = fast::normalize(float3x3(t, b, ng) * info.ntex);
    }
    else
    {
        info.n = ng;
    }
    info.t = t;
    info.b = b;
    return info;
}

float4
getBaseColor(device const BindGroup_0& bg0, device const BindGroup_2& bg2, float4 u_ColorFactor, VertexOut tShaderIn, float2 UV)
{
    float4 baseColor = float4(1);

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
        // baseColor *= texture(u_BaseColorSampler, getBaseColorUV());
        baseColor *= bg2.tBaseColorTexture.sample(bg0.tDefaultSampler, UV);
    }
    return baseColor * tShaderIn.tColor;
}

float3
linearTosRGB(float3 color)
{
    return pow(color, float3(INV_GAMMA));
}

MaterialInfo
getMetallicRoughnessInfo(VertexOut in, device const BindGroup_0& bg0, device const BindGroup_2& bg2, MaterialInfo info, float u_MetallicFactor, float u_RoughnessFactor, float2 UV)
{
    info.metallic = u_MetallicFactor;
    info.perceptualRoughness = u_RoughnessFactor;

    if(bool(iTextureMappingFlags & PL_HAS_METALLIC_ROUGHNESS_MAP))
    {
        // Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
        // This layout intentionally reserves the 'r' channel for (optional) occlusion map data
        float4 mrSample = float4(bg2.tMetallicRoughnessTexture.sample(bg0.tDefaultSampler, UV).rgb, 1.0);
        info.perceptualRoughness *= mrSample.g;
        info.metallic *= mrSample.b;
    }

    // Achromatic f0 based on IOR.
    info.c_diff = mix(info.baseColor.rgb,  float3(0), info.metallic);
    info.f0 = mix(info.f0, info.baseColor.rgb, info.metallic);
    return info;
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

float textureProj(device const BindGroup_1& bg2, float4 shadowCoord, float2 offset, uint cascadeIndex)
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

float filterPCF(device const BindGroup_1& bg2, float4 sc, uint cascadeIndex)
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

struct plRenderTargets
{   
    float4 outColor [[ color(0) ]];
};

fragment plRenderTargets fragment_main(
    VertexOut in [[stage_in]],
    device const BindGroup_0& bg0 [[ buffer(1) ]],
    device const BindGroup_1& bg1 [[ buffer(2) ]],
    device const BindGroup_2& bg2 [[ buffer(3) ]],
    device const DynamicData& tObjectInfo [[ buffer(4) ]],
    bool front_facing [[front_facing]]
    )
{

    const float2 tUV[8] = {
        in.tUV0,
        in.tUV1,
        in.tUV2,
        in.tUV3,
        in.tUV4,
        in.tUV5,
        in.tUV6,
        in.tUV7
    };

    tMaterial material = bg0.atMaterials[tObjectInfo.iMaterialIndex];
    float4 tBaseColor = getBaseColor(bg0, bg2, material.u_BaseColorFactor, in, tUV[material.BaseColorUVSet]);

    if(tBaseColor.a < 0.1)
    {
        discard_fragment();
    }

    NormalInfo tNormalInfo = pl_get_normal_info(bg0, bg2, in, front_facing, tUV[material.NormalUVSet]);
    
    float3 n = tNormalInfo.n;

    MaterialInfo materialInfo;
    materialInfo.baseColor = tBaseColor.rgb;

    // The default index of refraction of 1.5 yields a dielectric normal incidence reflectance of 0.04.
    materialInfo.ior = 1.5;
    materialInfo.f0 = float3(0.04);
    materialInfo.specularWeight = 1.0;

    if(bool(iMaterialFlags & PL_MATERIAL_METALLICROUGHNESS))
    {
        materialInfo = getMetallicRoughnessInfo(in, bg0, bg2, materialInfo, material.u_MetallicFactor, material.u_RoughnessFactor, tUV[material.MetallicRoughnessUVSet]);
    }

    materialInfo.perceptualRoughness = fast::clamp(materialInfo.perceptualRoughness, 0.0, 1.0);
    materialInfo.metallic = fast::clamp(materialInfo.metallic, 0.0, 1.0);

    // Roughness is authored as perceptual roughness; as is convention,
    // convert to material roughness by squaring the perceptual roughness.
    materialInfo.alphaRoughness = materialInfo.perceptualRoughness * materialInfo.perceptualRoughness;

    // Compute reflectance.
    // float reflectance = fast::max(fast::max(materialInfo.f0.r, materialInfo.f0.g), materialInfo.f0.b);

    // Anything less than 2% is physically impossible and is instead considered to be shadowing. Compare to "Real-Time-Rendering" 4th editon on page 325.
    materialInfo.f90 = float3(1.0);

    // LIGHTING
    float3 f_emissive = material.u_EmissiveFactor;
    if(bool(iTextureMappingFlags & PL_HAS_EMISSIVE_MAP))
    {
        f_emissive *= bg2.tEmissiveTexture.sample(bg0.tDefaultSampler, tUV[material.EmissiveUVSet]).rgb;
    }

    // ambient occlusion
    float ao = 1.0;
    if(bool(iTextureMappingFlags & PL_HAS_OCCLUSION_MAP))
    {
        ao = bg2.tOcclusionTexture.sample(bg0.tDefaultSampler, tUV[material.OcclusionUVSet]).r;
    }

    float3 v = normalize(bg0.data->tCameraPos.xyz - in.tPosition.xyz);

    // LIGHTING
    float3 f_specular = float3(0.0);
    float3 f_diffuse = float3(0.0);
    float3 f_clearcoat = float3(0.0);
    float3 f_sheen = float3(0.0);

    // IBL STUFF
    if(bool(iRenderingFlags & PL_RENDERING_FLAG_USE_IBL))
    {
        f_specular +=  getIBLRadianceGGX(bg0, n, v, materialInfo.perceptualRoughness, materialInfo.f0, materialInfo.specularWeight, material.u_MipCount);
        f_diffuse += getIBLRadianceLambertian(bg0, n, v, materialInfo.perceptualRoughness, materialInfo.c_diff, materialInfo.f0, materialInfo.specularWeight);
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
            device const plLightData& tLightData = bg1.atData[i];
            float3 pointToLight;
            float shadow = 1.0;

            if(tLightData.iCascadeCount > 0)
            {
                plLightShadowData tShadowData = bg1.atShadowData[tLightData.iShadowIndex];

                // Get cascade index for the current fragment's view position
                
                float4 inViewPos = bg0.data->tCameraView * float4(in.tPosition.xyz, 1.0);
                for(uint j = 0; j < tLightData.iCascadeCount - 1; ++j)
                {
                    if(inViewPos.z > tShadowData.cascadeSplits[j])
                    {	
                        cascadeIndex = j + 1;
                    }
                }  

                // Depth compare for shadowing
	            float4 shadowCoord = (biasMat * tShadowData.cascadeViewProjMat[cascadeIndex]) * float4(in.tPosition.xyz, 1.0);	
                shadow = 0;
                shadow = textureProj(bg1, shadowCoord / shadowCoord.w, float2(0.0), cascadeIndex);
                // shadow = filterPCF(shadowCoord / shadowCoord.w, cascadeIndex);
            }

            if(tLightData.iType != PL_LIGHT_TYPE_DIRECTIONAL)
            {
                pointToLight = tLightData.tPosition - in.tPosition.xyz;
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
                f_diffuse += shadow * intensity * NdotL *  BRDF_lambertian(materialInfo.f0, materialInfo.f90, materialInfo.c_diff, materialInfo.specularWeight, VdotH);
                f_specular += shadow * intensity * NdotL * BRDF_specularGGX(materialInfo.f0, materialInfo.f90, materialInfo.alphaRoughness, materialInfo.specularWeight, VdotH, NdotL, NdotV, NdotH);
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

    plRenderTargets tMRT;
    tMRT.outColor.rgb = linearTosRGB(color);
    tMRT.outColor.a = tBaseColor.a;

    // if(in.tPositionOut.x < 600.0)
    // {
    //     switch(cascadeIndex) {
    //         case 0 : 
    //             tMRT.outColor.rgb *= float3(1.0f, 0.25f, 0.25f);
    //             break;
    //         case 1 : 
    //             tMRT.outColor.rgb *= float3(0.25f, 1.0f, 0.25f);
    //             break;
    //         case 2 : 
    //             tMRT.outColor.rgb *= float3(0.25f, 0.25f, 1.0f);
    //             break;
    //         case 3 : 
    //             tMRT.outColor.rgb *= float3(1.0f, 1.0f, 0.25f);
    //             break;
    //     }
    // }

    return tMRT;

}