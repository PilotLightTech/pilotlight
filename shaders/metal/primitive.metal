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

//-----------------------------------------------------------------------------
// [SECTION] defines & structs
//-----------------------------------------------------------------------------

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
#define PL_MESH_FORMAT_FLAG_HAS_COLOR_0    1 << 7
#define PL_MESH_FORMAT_FLAG_HAS_COLOR_1    1 << 8
#define PL_MESH_FORMAT_FLAG_HAS_JOINTS_0   1 << 9
#define PL_MESH_FORMAT_FLAG_HAS_JOINTS_1   1 << 10
#define PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0  1 << 11
#define PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1  1 << 12

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

struct NormalInfo {
    float3 ng;   // Geometry normal
    float3 t;    // Geometry tangent
    float3 b;    // Geometry bitangent
    float3 n;    // Shading normal
    float3 ntex; // Normal from texture, scaling is accounted for.
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
    float4   tCameraPosition;
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

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

struct DynamicData
{
    int      iDataOffset;
    int      iVertexOffset;
    int      iMaterialIndex;
    int      iPadding[1];
    float4x4 tModel;
};

//-----------------------------------------------------------------------------
// [SECTION] input
//-----------------------------------------------------------------------------

struct VertexIn {
    float3 tPosition [[attribute(0)]];
};

//-----------------------------------------------------------------------------
// [SECTION] output
//-----------------------------------------------------------------------------

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

struct plMultipleRenderTargets
{   
    float4 outAlbedo [[ color(0) ]];
    float4 outNormal [[ color(1) ]];
    float4 outPosition [[ color(2) ]];
    float4 outEmissive [[ color(3) ]];
    float4 outAOMetalnessRoughness [[ color(4) ]];
};

//-----------------------------------------------------------------------------
// [SECTION] helpers
//-----------------------------------------------------------------------------

NormalInfo
pl_get_normal_info(device const BindGroup_0& bg0, device const BindGroup_1& bg1, VertexOut tShaderIn, bool front_facing, float2 UV)
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
        info.ntex = bg1.tNormalTexture.sample(bg0.tDefaultSampler, UV).rgb * 2.0 - float3(1.0);
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
getBaseColor(device const BindGroup_0& bg0, device const BindGroup_1& bg1, float4 u_ColorFactor, VertexOut tShaderIn, float2 UV)
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
        baseColor *= bg1.tBaseColorTexture.sample(bg0.tDefaultSampler, UV);
    }
    return baseColor * tShaderIn.tColor;
}

float3
linearTosRGB(float3 color)
{
    return pow(color, float3(INV_GAMMA));
}

MaterialInfo
getMetallicRoughnessInfo(VertexOut in, device const BindGroup_0& bg0, device const BindGroup_1& bg1, MaterialInfo info, float u_MetallicFactor, float u_RoughnessFactor, float2 UV)
{
    info.metallic = u_MetallicFactor;
    info.perceptualRoughness = u_RoughnessFactor;

    if(bool(iTextureMappingFlags & PL_HAS_METALLIC_ROUGHNESS_MAP))
    {
        // Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
        // This layout intentionally reserves the 'r' channel for (optional) occlusion map data
        float4 mrSample = float4(bg1.tMetallicRoughnessTexture.sample(bg0.tDefaultSampler, UV).rgb, 1.0);
        info.perceptualRoughness *= mrSample.g;
        info.metallic *= mrSample.b;
    }

    // Achromatic f0 based on IOR.
    info.c_diff = mix(info.baseColor.rgb,  float3(0), info.metallic);
    info.f0 = mix(info.f0, info.baseColor.rgb, info.metallic);
    return info;
}

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

vertex VertexOut vertex_main(
    uint                vertexID [[ vertex_id ]],
    VertexIn            in [[stage_in]],
    device const BindGroup_0& bg0 [[ buffer(1) ]],
    device const BindGroup_1& bg1 [[ buffer(2) ]],
    device const DynamicData& tObjectInfo [[ buffer(3) ]]
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
    if(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0){
        inTexCoord0 = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].xy; 
        inTexCoord1 = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].zw; 
        iCurrentAttribute++;
    }
    if(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1){
        inTexCoord2 = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].xy; 
        inTexCoord3 = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].zw; 
        iCurrentAttribute++;
    }
    if(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_2){
        inTexCoord4 = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].xy; 
        inTexCoord5 = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].zw; 
        iCurrentAttribute++;
    }
    if(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_3){
        inTexCoord6 = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].xy; 
        inTexCoord7 = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].zw; 
        iCurrentAttribute++;
    }
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

fragment plMultipleRenderTargets fragment_main(
    VertexOut in [[stage_in]],
    device const BindGroup_0& bg0 [[ buffer(1) ]],
    device const BindGroup_1& bg1 [[ buffer(2) ]],
    device const DynamicData& tObjectInfo [[ buffer(3) ]],
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
    float4 tBaseColor = getBaseColor(bg0, bg1, material.u_BaseColorFactor, in, tUV[material.BaseColorUVSet]);
    NormalInfo tNormalInfo = pl_get_normal_info(bg0, bg1, in, front_facing, tUV[material.NormalUVSet]);
    
    float3 n = tNormalInfo.n;
    float3 t = tNormalInfo.t;
    float3 b = tNormalInfo.b;

    MaterialInfo materialInfo;
    materialInfo.baseColor = tBaseColor.rgb;

    // The default index of refraction of 1.5 yields a dielectric normal incidence reflectance of 0.04.
    materialInfo.ior = 1.5;
    materialInfo.f0 = float3(0.04);
    materialInfo.specularWeight = 1.0;

    if(bool(iMaterialFlags & PL_MATERIAL_METALLICROUGHNESS))
    {
        materialInfo = getMetallicRoughnessInfo(in, bg0, bg1, materialInfo, material.u_MetallicFactor, material.u_RoughnessFactor, tUV[material.MetallicRoughnessUVSet]);
    }

    materialInfo.perceptualRoughness = fast::clamp(materialInfo.perceptualRoughness, 0.0, 1.0);
    materialInfo.metallic = fast::clamp(materialInfo.metallic, 0.0, 1.0);

    // Roughness is authored as perceptual roughness; as is convention,
    // convert to material roughness by squaring the perceptual roughness.
    materialInfo.alphaRoughness = materialInfo.perceptualRoughness * materialInfo.perceptualRoughness;

    // Compute reflectance.
    float reflectance = fast::max(fast::max(materialInfo.f0.r, materialInfo.f0.g), materialInfo.f0.b);

    // Anything less than 2% is physically impossible and is instead considered to be shadowing. Compare to "Real-Time-Rendering" 4th editon on page 325.
    materialInfo.f90 = float3(1.0);

    // LIGHTING
    float3 f_emissive = material.u_EmissiveFactor;
    if(bool(iTextureMappingFlags & PL_HAS_EMISSIVE_MAP))
    {
        f_emissive *= bg1.tEmissiveTexture.sample(bg0.tDefaultSampler, tUV[material.EmissiveUVSet]).rgb;
    }

    // ambient occlusion
    float ao = 1.0;
    if(bool(iTextureMappingFlags & PL_HAS_OCCLUSION_MAP))
    {
        ao = bg1.tOcclusionTexture.sample(bg0.tDefaultSampler, tUV[material.OcclusionUVSet]).r;
    }
    
    plMultipleRenderTargets tMRT;
    tMRT.outAlbedo = tBaseColor;
    tMRT.outNormal = float4(tNormalInfo.n, 1.0);
    tMRT.outPosition = float4(in.tPosition, materialInfo.specularWeight);
    tMRT.outEmissive = float4(f_emissive, material.u_MipCount);
    tMRT.outAOMetalnessRoughness = float4(ao, materialInfo.metallic, materialInfo.perceptualRoughness, 1.0);

    return tMRT;
}