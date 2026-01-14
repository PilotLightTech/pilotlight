#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "bg_scene.inc"
#include "bg_view.inc"
#include "brdf.glsl"
#include "math.glsl"


//-----------------------------------------------------------------------------
// [SECTION] specialication constants
//-----------------------------------------------------------------------------

layout(constant_id = 0) const int iMeshVariantFlags = 0;
layout(constant_id = 1) const int iTextureMappingFlags = 0;
layout(constant_id = 2) const int iMaterialFlags = 0;
layout(constant_id = 3) const int iRenderingFlags = 0;
layout(constant_id = 4) const int iLightCount = 0;
layout(constant_id = 5) const int iProbeCount = 0;
layout(constant_id = 6) const int tShaderDebugMode = 0;

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    plGpuDynData tData;
} tObjectInfo;

//-----------------------------------------------------------------------------
// [SECTION] input & output
//-----------------------------------------------------------------------------

layout(location = 0) out vec4 outColor;

// output
layout(location = 0) in struct plShaderIn {
    vec3 tWorldPosition;
    vec3 tViewPosition;
    vec2 tUV[8];
    vec4 tColor;
    vec3 tWorldNormal;
    mat3 tTBN;
    mat4 tModel; 
} tShaderIn;

#include "math.glsl"
#include "lighting.glsl"
#include "fog.glsl"

#define PL_INCLUDE_MATERIAL_FUNCTIONS
#include "material_info.glsl"

vec3
getPunctualRadianceClearCoat(vec3 clearcoatNormal, vec3 v, vec3 l, vec3 h, float VdotH, vec3 f0, vec3 f90, float clearcoatRoughness)
{
    float NdotL = clampedDot(clearcoatNormal, l);
    float NdotV = clampedDot(clearcoatNormal, v);
    float NdotH = clampedDot(clearcoatNormal, h);
    return NdotL * pl_brdf_specular(clearcoatRoughness * clearcoatRoughness, NdotL, NdotV, NdotH);
}

vec3
getPunctualRadianceSheen(vec3 sheenColor, float sheenRoughness, float NdotL, float NdotV, float NdotH)
{
    return NdotL * BRDF_specularSheen(sheenColor, sheenRoughness, NdotL, NdotV, NdotH);
}

float
albedoSheenScalingLUT(float NdotV, float sheenRoughnessFactor)
{
  float c = 1.0 - NdotV;
  float c3 = c * c * c;
  return 0.65584461 * c3 + 1.0 / (4.16526551 + exp(-7.97291361 * sqrt(sheenRoughnessFactor) + 6.33516894));
}

// XYZ to sRGB color space
const mat3 XYZ_TO_REC709 = mat3(
     3.2404542, -0.9692660,  0.0556434,
    -1.5371385,  1.8760108, -0.2040259,
    -0.4985314,  0.0415560,  1.0572252
);

// Assume air interface for top
// Note: We don't handle the case fresnel0 == 1
vec3 Fresnel0ToIor(vec3 fresnel0) {
    vec3 sqrtF0 = sqrt(fresnel0);
    return (vec3(1.0) + sqrtF0) / (vec3(1.0) - sqrtF0);
}

// Conversion FO/IOR
vec3 IorToFresnel0(vec3 transmittedIor, float incidentIor) {
    return sq((transmittedIor - vec3(incidentIor)) / (transmittedIor + vec3(incidentIor)));
}

// ior is a value between 1.0 and 3.0. 1.0 is air interface
float IorToFresnel0(float transmittedIor, float incidentIor) {
    return sq((transmittedIor - incidentIor) / (transmittedIor + incidentIor));
}

// Fresnel equations for dielectric/dielectric interfaces.
// Ref: https://belcour.github.io/blog/research/2017/05/01/brdf-thin-film.html
// Evaluation XYZ sensitivity curves in Fourier space
vec3 evalSensitivity(float OPD, vec3 shift) {
    float phase = 2.0 * M_PI * OPD * 1.0e-9;
    vec3 val = vec3(5.4856e-13, 4.4201e-13, 5.2481e-13);
    vec3 pos = vec3(1.6810e+06, 1.7953e+06, 2.2084e+06);
    vec3 var = vec3(4.3278e+09, 9.3046e+09, 6.6121e+09);

    vec3 xyz = val * sqrt(2.0 * M_PI * var) * cos(pos * phase + shift) * exp(-sq(phase) * var);
    xyz.x += 9.7470e-14 * sqrt(2.0 * M_PI * 4.5282e+09) * cos(2.2399e+06 * phase + shift[0]) * exp(-4.5282e+09 * sq(phase));
    xyz /= 1.0685e-7;

    vec3 srgb = XYZ_TO_REC709 * xyz;
    return srgb;
}

vec3 evalIridescence(float outsideIOR, float eta2, float cosTheta1, float thinFilmThickness, vec3 baseF0) {
    vec3 I;

    // Force iridescenceIor -> outsideIOR when thinFilmThickness -> 0.0
    float iridescenceIor = mix(outsideIOR, eta2, smoothstep(0.0, 0.03, thinFilmThickness));
    // Evaluate the cosTheta on the base layer (Snell law)
    float sinTheta2Sq = sq(outsideIOR / iridescenceIor) * (1.0 - sq(cosTheta1));

    // Handle TIR:
    float cosTheta2Sq = 1.0 - sinTheta2Sq;
    if (cosTheta2Sq < 0.0) {
        return vec3(1.0);
    }

    float cosTheta2 = sqrt(cosTheta2Sq);

    // First interface
    float R0 = IorToFresnel0(iridescenceIor, outsideIOR);
    float R12 = pl_fresnel_schlick(R0, cosTheta1);
    float R21 = R12;
    float T121 = 1.0 - R12;
    float phi12 = 0.0;
    if (iridescenceIor < outsideIOR) phi12 = M_PI;
    float phi21 = M_PI - phi12;

    // Second interface
    vec3 baseIOR = Fresnel0ToIor(clamp(baseF0, 0.0, 0.9999)); // guard against 1.0
    vec3 R1 = IorToFresnel0(baseIOR, iridescenceIor);
    vec3 R23 = pl_fresnel_schlick(R1, cosTheta2);
    vec3 phi23 = vec3(0.0);
    if (baseIOR[0] < iridescenceIor) phi23[0] = M_PI;
    if (baseIOR[1] < iridescenceIor) phi23[1] = M_PI;
    if (baseIOR[2] < iridescenceIor) phi23[2] = M_PI;

    // Phase shift
    float OPD = 2.0 * iridescenceIor * thinFilmThickness * cosTheta2;
    vec3 phi = vec3(phi21) + phi23;

    // Compound terms
    vec3 R123 = clamp(R12 * R23, 1e-5, 0.9999);
    vec3 r123 = sqrt(R123);
    vec3 Rs = sq(T121) * R23 / (vec3(1.0) - R123);

    // Reflectance term for m = 0 (DC term amplitude)
    vec3 C0 = R12 + Rs;
    I = C0;

    // Reflectance term for m > 0 (pairs of diracs)
    vec3 Cm = Rs - T121;
    for (int m = 1; m <= 2; ++m)
    {
        Cm *= r123;
        vec3 Sm = 2.0 * evalSensitivity(float(m) * OPD, float(m) * phi);
        I += Cm * Sm;
    }

    // Since out of gamut colors might be produced, negative color values are clamped to 0.
    return max(I, vec3(0.0));
}



//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void main() 
{

    plGpuMaterial material = tMaterialInfo.atMaterials[tObjectInfo.tData.iMaterialIndex];
    vec4 tBaseColor = getBaseColor(material.tBaseColorFactor, material.aiTextureUVSet[PL_TEXTURE_BASE_COLOR]);

    if(material.tAlphaMode == PL_SHADER_ALPHA_MODE_OPAQUE)
    {
        tBaseColor.a = 1.0;
    }
    vec3 color = vec3(0);

    NormalInfo tNormalInfo = pl_get_normal_info();

    vec3 n = tNormalInfo.n;
    vec3 t = tNormalInfo.t;
    // vec3 b = tNormalInfo.b;

    vec3 vraw = tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tCameraPos.xyz - tShaderIn.tWorldPosition.xyz;
    vec3 v = normalize(vraw);

    float NdotV = clampedDot(n, v);
    // float TdotV = clampedDot(t, v);
    // float BdotV = clampedDot(b, v);

    MaterialInfo materialInfo;
    materialInfo.baseColor = tBaseColor.rgb;
    materialInfo.thickness = material.fThickness;
    materialInfo.attenuationDistance = material.fAttenuationDistance;
    materialInfo.dispersion = 0.0;
    materialInfo.attenuationColor = material.tAttenuationColor;

    // The default index of refraction of 1.5 yields a dielectric normal incidence reflectance of 0.04.
    materialInfo.ior = 1.5;
    materialInfo.f0_dielectric = vec3(0.04);
    materialInfo.specularWeight = 1.0;

    materialInfo = getIorInfo(materialInfo);

    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_METALLIC_ROUGHNESS))
    {
        materialInfo = getMetallicRoughnessInfo(materialInfo, material.fMetallicFactor, material.fRoughnessFactor);
    }

    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_SHEEN))
    {
        materialInfo = getSheenInfo(materialInfo, material.tSheenColorFactor, material.fSheenRoughnessFactor);
    }

    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_CLEARCOAT))
    {
        materialInfo = getClearCoatInfo(materialInfo, tNormalInfo, material.fClearcoatFactor, material.fClearcoatRoughnessFactor);
    }

    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_TRANSMISSION))
    {
        materialInfo = getTransmissionInfo(materialInfo, material.fTransmissionFactor);
    }

    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_VOLUME))
    {
        materialInfo = getVolumeInfo(materialInfo);
    }

    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_IRIDESCENCE))
    {
        materialInfo = getIridescenceInfo(materialInfo, material.fIridescenceFactor, material.fIridescenceIor, material.fIridescenceThicknessMax, material.fIridescenceThicknessMin);
    }

    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_DIFFUSE_TRANSMISSION))
    {
        materialInfo = getDiffuseTransmissionInfo(materialInfo);
    }

    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_ANISOTROPY))
    {
        materialInfo = getAnisotropyInfo(materialInfo, tNormalInfo, material.tAnisotropy);
    }

    materialInfo.perceptualRoughness = clamp(materialInfo.perceptualRoughness, 0.0, 1.0);
    materialInfo.metallic = clamp(materialInfo.metallic, 0.0, 1.0);

    // Roughness is authored as perceptual roughness; as is convention,
    // convert to material roughness by squaring the perceptual roughness.
    materialInfo.alphaRoughness = materialInfo.perceptualRoughness * materialInfo.perceptualRoughness;

    // Anything less than 2% is physically impossible and is instead considered to be shadowing. Compare to "Real-Time-Rendering" 4th editon on page 325.
    materialInfo.f90 = vec3(1.0);
    materialInfo.f90_dielectric = materialInfo.f90;

    // LIGHTING
    vec3 f_specular_dielectric = vec3(0.0);
    vec3 f_specular_metal = vec3(0.0);
    vec3 f_diffuse = vec3(0.0);
    vec3 f_dielectric_brdf_ibl = vec3(0.0);
    vec3 f_metal_brdf_ibl = vec3(0.0);
    vec3 f_emissive = vec3(0.0);
    vec3 clearcoat_brdf = vec3(0.0);
    vec3 f_sheen = vec3(0.0);
    vec3 f_specular_transmission = vec3(0.0);
    vec3 f_diffuse_transmission = vec3(0.0);

    float clearcoatFactor = 0.0;
    vec3 clearcoatFresnel = vec3(0);

    float albedoSheenScaling = 1.0;
    float diffuseTransmissionThickness = 1.0;

    vec3 iridescenceFresnel_dielectric = vec3(0);
    vec3 iridescenceFresnel_metallic = vec3(0);

    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_IRIDESCENCE))
    {
        iridescenceFresnel_dielectric = evalIridescence(1.0, materialInfo.iridescenceIor, NdotV, materialInfo.iridescenceThickness, materialInfo.f0_dielectric);
        iridescenceFresnel_metallic = evalIridescence(1.0, materialInfo.iridescenceIor, NdotV, materialInfo.iridescenceThickness, tBaseColor.rgb);

        if (materialInfo.iridescenceThickness == 0.0)
        {
            materialInfo.iridescenceFactor = 0.0;
        }
    }

    mat4 u_ViewMatrix = tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tCameraView;
    mat4 u_ProjectionMatrix = tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tCameraProjection;
    mat4 u_ModelMatrix = tShaderIn.tModel;

    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_DIFFUSE_TRANSMISSION))
    {
        if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_VOLUME))
        {
            diffuseTransmissionThickness = materialInfo.thickness *
                (length(vec3(u_ModelMatrix[0].xyz)) + length(vec3(u_ModelMatrix[1].xyz)) + length(vec3(u_ModelMatrix[2].xyz))) / 3.0;
        }
    }

    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_CLEARCOAT))
    {
        clearcoatFactor = materialInfo.clearcoatFactor;
        clearcoatFresnel = pl_fresnel_schlick(materialInfo.clearcoatF0, materialInfo.clearcoatF90, clampedDot(materialInfo.clearcoatNormal, v));
    }



    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_TRANSMISSION))
    {
        
        f_specular_transmission = getIBLVolumeRefraction(n, v, materialInfo.perceptualRoughness,
            tBaseColor.rgb, tShaderIn.tWorldPosition, u_ModelMatrix, u_ViewMatrix, u_ProjectionMatrix,
            materialInfo.ior, materialInfo.thickness, materialInfo.attenuationColor, materialInfo.attenuationDistance, materialInfo.dispersion);
    }


    // Calculate lighting contribution from image based lighting source (IBL)
    if(bool(iRenderingFlags & PL_RENDERING_FLAG_USE_IBL) && iProbeCount > 0)
    {

        int aiActiveProbes[3];
        aiActiveProbes[0] = -1;
        aiActiveProbes[1] = -1;
        aiActiveProbes[2] = -1;

        float weights[3];
        weights[0] = 0.0;
        weights[1] = 0.0;
        weights[2] = 0.0;

        float distances[3];
        distances[0] = 10000.0;
        distances[1] = 10000.0;
        distances[2] = 10000.0;

        int K = 0;

        for(int i = 0; i < iProbeCount; i++)
        {
            vec3 tDist = tProbeData.atData[i].tPosition - tShaderIn.tWorldPosition.xyz;
            tDist = tDist * tDist;
            float fDistSqr = tDist.x + tDist.y + tDist.z;
            
            if(fDistSqr <= tProbeData.atData[i].fRangeSqr)
            {
                
                int iFurthest = 0;
                for(int j = 0; j < 3; j++)
                {
                    if(distances[j] > distances[iFurthest])
                    {
                        iFurthest = j;
                    }
                }

                if(distances[iFurthest] > fDistSqr)
                {
                    K++;
                    aiActiveProbes[iFurthest] = i;
                    distances[iFurthest] = fDistSqr;
                }
            }
        }

        int iClosestIndex = 0;
        float maxDis = distances[0];
        for(int j = 0; j < 3; j++)
        {
            if(distances[j] < distances[iClosestIndex])
            {
                iClosestIndex = j;
            }
        }

        K = min(K, 3);
        vec3 R = reflect(-v, n);
        float summing = computeProbeWeights(tShaderIn.tWorldPosition.xyz, R, 2.0, K, aiActiveProbes, weights);
        int iClosestProbeIndex = aiActiveProbes[iClosestIndex];

        int iMips2 = textureQueryLevels(samplerCube(atCubeTextures[nonuniformEXT(tProbeData.atData[iClosestProbeIndex].uGGXEnvSampler)], tSamplerNearestRepeat));
        if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_ANISOTROPY))
        {
            f_specular_metal = getIBLRadianceAnisotropy(n, v, materialInfo.perceptualRoughness, materialInfo.anisotropyStrength, materialInfo.anisotropicB, iMips2, tShaderIn.tWorldPosition.xyz, iClosestProbeIndex);
            f_specular_dielectric = f_specular_metal;
        }
        else
        {
            f_specular_metal = getIBLRadianceGGX(n, v, materialInfo.perceptualRoughness, iMips2, tShaderIn.tWorldPosition.xyz, iClosestProbeIndex);
            f_specular_dielectric = f_specular_metal;
        }

        for(int i = 0; i < K; i++)
        {
            int iProbeIndex = aiActiveProbes[i];
            

            f_diffuse = getDiffuseLight(n, iProbeIndex) * tBaseColor.rgb;

            int iMips = textureQueryLevels(samplerCube(atCubeTextures[nonuniformEXT(tProbeData.atData[iProbeIndex].uGGXEnvSampler)], tSamplerNearestRepeat));


            if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_DIFFUSE_TRANSMISSION))
            {
                vec3 diffuseTransmissionIBL = getDiffuseLight(-n, iProbeIndex) * materialInfo.diffuseTransmissionColorFactor;
                if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_VOLUME))
                {
                    diffuseTransmissionIBL = applyVolumeAttenuation(diffuseTransmissionIBL, diffuseTransmissionThickness, materialInfo.attenuationColor, materialInfo.attenuationDistance);
                }
                f_diffuse = mix(f_diffuse, diffuseTransmissionIBL, materialInfo.diffuseTransmissionFactor);
            }

            if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_TRANSMISSION))
            {
                f_diffuse = mix(f_diffuse, f_specular_transmission, materialInfo.transmissionFactor);
            }



            // Calculate fresnel mix for IBL  

            vec3 f_metal_fresnel_ibl = getIBLGGXFresnel(n, v, materialInfo.perceptualRoughness, tBaseColor.rgb, 1.0, iProbeIndex);
            f_metal_brdf_ibl = f_metal_fresnel_ibl * f_specular_metal;
        
            vec3 f_dielectric_fresnel_ibl = getIBLGGXFresnel(n, v, materialInfo.perceptualRoughness, materialInfo.f0_dielectric, materialInfo.specularWeight, iProbeIndex);
            f_dielectric_brdf_ibl = mix(f_diffuse, f_specular_dielectric,  f_dielectric_fresnel_ibl);

            if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_IRIDESCENCE))
            {
                f_metal_brdf_ibl = mix(f_metal_brdf_ibl, f_specular_metal * iridescenceFresnel_metallic, materialInfo.iridescenceFactor);
                f_dielectric_brdf_ibl = mix(f_dielectric_brdf_ibl, pl_rgb_mix(f_diffuse, f_specular_dielectric, iridescenceFresnel_dielectric), materialInfo.iridescenceFactor);
            }

            if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_CLEARCOAT))
            {
                clearcoat_brdf = getIBLRadianceGGX(materialInfo.clearcoatNormal, v, materialInfo.clearcoatRoughness, iMips, tShaderIn.tWorldPosition.xyz, iProbeIndex);
            }

            if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_SHEEN))
            {
                f_sheen = getIBLRadianceCharlie(n, v, materialInfo.sheenRoughnessFactor, materialInfo.sheenColorFactor, iMips, iProbeIndex);
                albedoSheenScaling = 1.0 - pl_max3(materialInfo.sheenColorFactor) * albedoSheenScalingLUT(NdotV, materialInfo.sheenRoughnessFactor);
            }

            vec3 icolor = mix(f_dielectric_brdf_ibl, f_metal_brdf_ibl, materialInfo.metallic);
            icolor = f_sheen + icolor * albedoSheenScaling;
            icolor = mix(icolor, clearcoat_brdf, clearcoatFactor * clearcoatFresnel);
            color += icolor * weights[i];
        }
    }

    // ambient occlusion
    
    float ao = 1.0;
    if(bool(iTextureMappingFlags & PL_HAS_OCCLUSION_MAP))
    {
        ao = texture(sampler2D(at2DTextures[nonuniformEXT(material.aiTextureIndices[PL_TEXTURE_OCCLUSION])], tSamplerLinearRepeat), pl_get_uv(PL_TEXTURE_OCCLUSION)).r;
        color = color * (1.0 + material.fOcclusionStrength * (ao - 1.0)); 
    }

    // punctual stuff
    f_diffuse = vec3(0.0);
    f_specular_dielectric = vec3(0.0);
    f_specular_metal = vec3(0.0);
    vec3 f_dielectric_brdf = vec3(0.0);
    vec3 f_metal_brdf = vec3(0.0);

    uint cascadeIndex = 0;
    const bool bShadows = bool(iRenderingFlags & PL_RENDERING_FLAG_SHADOWS);
    if(bool(iRenderingFlags & PL_RENDERING_FLAG_USE_PUNCTUAL))
    {

        // point & spot lights
        for(int i = 0; i < iLightCount; i++)
        {

            plGpuLight tLightData = tLightInfo.atData[i];

            vec3 pointToLight;
            float shadow = 1.0;

            if(tLightData.iType == PL_LIGHT_TYPE_DIRECTIONAL)
            {

                pointToLight = -tLightData.tDirection;

                if(bShadows && tLightData.iCastShadow > 0)
                {
                    plGpuLightShadow tShadowData = tDShadowData.atData[tLightData.iShadowIndex];

                    // Depth compare for shadowing
                    mat4 abiasMat = biasMat;
                    abiasMat[0][0] *= tShadowData.fFactor;
                    abiasMat[1][1] *= tShadowData.fFactor;
                    abiasMat[3][0] *= tShadowData.fFactor;
                    abiasMat[3][1] *= tShadowData.fFactor;
                    shadow = 0;

                    // Get cascade index for the current fragment's view position
                    
                    vec4 inViewPos = tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tCameraView * vec4(tShaderIn.tWorldPosition.xyz, 1.0);

                    for(int j = 0; j < tLightData.iCascadeCount; ++j)
                    {
                        vec4 rawshadowCoord = biasMat * tShadowData.viewProjMat[j] * vec4(tShaderIn.tWorldPosition.xyz, 1.0);

                        // if(rawshadowCoord.xy == pl_saturate(rawshadowCoord.xy))
                        if(abs(rawshadowCoord.x - pl_saturate(rawshadowCoord.x)) < 0.00001 && abs(rawshadowCoord.y - pl_saturate(rawshadowCoord.y)) < 0.00001)
                        {
                            vec4 shadowCoord = (abiasMat * tShadowData.viewProjMat[j]) * vec4(tShaderIn.tWorldPosition.xyz, 1.0);
                            cascadeIndex = j;
                        
                            if(bool(iRenderingFlags & PL_RENDERING_FLAG_PCF_SHADOWS))
                            {
                                shadow = filterPCF(shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset) + vec2(j * tShadowData.fFactor, 0), tShadowData.iShadowMapTexIdx);
                            }
                            else
                            {
                                shadow = textureProj(shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset) + vec2(j * tShadowData.fFactor, 0), tShadowData.iShadowMapTexIdx);
                            }
                            const vec3 shadow_box = vec3(rawshadowCoord.xy * 2.0 - 1.0, rawshadowCoord.z * 2.0 - 1.0);
                            const vec3 cascade_edgefactor = clamp(clamp(abs(shadow_box), 0.0, 1.0) - 0.8, 0.0, 1.0) * 5.0; // fade will be on edge and inwards 10%
                            const float cascade_fade = pl_max3(cascade_edgefactor);

                            if(cascade_fade > 0 && j < (tLightData.iCascadeCount - 1))
                            {

                                shadowCoord = (abiasMat * tShadowData.viewProjMat[j + 1]) * vec4(tShaderIn.tWorldPosition.xyz, 1.0);
                                float shadowfallback = 0.0;
                                if(bool(iRenderingFlags & PL_RENDERING_FLAG_PCF_SHADOWS))
                                {
                                    shadowfallback = filterPCF(shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset) + vec2((j + 1.0) * tShadowData.fFactor, 0), tShadowData.iShadowMapTexIdx);
                                }
                                else
                                {
                                    shadowfallback = textureProj(shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset) + vec2((j + 1.0) * tShadowData.fFactor, 0), tShadowData.iShadowMapTexIdx);
                                }

                                shadow = mix(shadow, shadowfallback, cascade_fade);
                            }

                            break;
                        }
                    }
                }
            }

            if(tLightData.iType == PL_LIGHT_TYPE_POINT)
            {
                pointToLight = tLightData.tPosition - tShaderIn.tWorldPosition.xyz;

                if(bShadows && tLightData.iCastShadow > 0)
                {
                    plGpuLightShadow tShadowData = tShadowData.atData[tLightData.iShadowIndex];

                    vec3 result = sampleCube(-normalize(pointToLight));
                    vec4 shadowCoord = tShadowData.viewProjMat[int(result.z)] * vec4(tShaderIn.tWorldPosition.xyz, 1.0);
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
                        if(bool(iRenderingFlags & PL_RENDERING_FLAG_PCF_SHADOWS))
                        {
                            shadow = filterPCF(shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset) + faceoffsets[int(result.z)] * tShadowData.fFactor, tShadowData.iShadowMapTexIdx);
                        }
                        else
                        {
                            shadow = textureProj(shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset) + faceoffsets[int(result.z)] * tShadowData.fFactor, tShadowData.iShadowMapTexIdx);
                        }
                    }
                }

            }

            if(tLightData.iType == PL_LIGHT_TYPE_SPOT)
            {
                pointToLight = tLightData.tPosition - tShaderIn.tWorldPosition.xyz;

                if(bShadows && tLightData.iCastShadow > 0)
                {
                    plGpuLightShadow tShadowData = tShadowData.atData[tLightData.iShadowIndex];

                    vec4 shadowCoord = tShadowData.viewProjMat[0] * vec4(tShaderIn.tWorldPosition.xyz, 1.0);
                    if(shadowCoord.z > -1.0 && shadowCoord.z < 1.0)
                    {
                        shadowCoord.xyz /= shadowCoord.w;
                        shadow = 1.0;
                        shadowCoord.x = shadowCoord.x/2 + 0.5;
                        shadowCoord.y = shadowCoord.y/2 + 0.5;
                        shadowCoord.xy *= tShadowData.fFactor;
                        // shadow = (shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset), tShadowData.iShadowMapTexIdx);

                        if(bool(iRenderingFlags & PL_RENDERING_FLAG_PCF_SHADOWS))
                        {
                            shadow = filterPCF(shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset), tShadowData.iShadowMapTexIdx);
                        }
                        else
                        {
                            shadow = textureProj(shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset), tShadowData.iShadowMapTexIdx);
                        }
                    }
                }
            }

            // BSTF
            vec3 l = normalize(pointToLight);   // Direction from surface point to light
            vec3 h = normalize(l + v);          // Direction of the vector between l and v, called halfway vector
            float NdotL = clampedDot(n, l);
            float NdotH = clampedDot(n, h);
            float LdotH = clampedDot(l, h);
            float VdotH = clampedDot(v, h);

            vec3 dielectric_fresnel = pl_fresnel_schlick(materialInfo.f0_dielectric * materialInfo.specularWeight, materialInfo.f90_dielectric, abs(VdotH));
            vec3 metal_fresnel = pl_fresnel_schlick(tBaseColor.rgb, vec3(1.0), abs(VdotH));


            if (NdotL > 0.0 || NdotV > 0.0)
            {

                vec3 intensity = getLightIntensity(tLightData, pointToLight);

                vec3 l_diffuse = shadow * intensity * NdotL * pl_brdf_diffuse(tBaseColor.rgb);
                vec3 l_specular_dielectric = vec3(0.0);
                vec3 l_specular_metal = vec3(0.0);
                vec3 l_dielectric_brdf = vec3(0.0);
                vec3 l_metal_brdf = vec3(0.0);
                vec3 l_clearcoat_brdf = vec3(0.0);
                vec3 l_sheen = vec3(0.0);
                float l_albedoSheenScaling = 1.0;

                if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_DIFFUSE_TRANSMISSION))
                {
                    l_diffuse = l_diffuse * (1.0 - materialInfo.diffuseTransmissionFactor);
                    if (dot(n, l) < 0.0)
                    {
                        float diffuseNdotL = clampedDot(-n, l);
                        vec3 diffuse_btdf = shadow * intensity * diffuseNdotL * pl_brdf_diffuse(materialInfo.diffuseTransmissionColorFactor);

                        vec3 l_mirror = normalize(l + 2.0 * n * dot(-l, n)); // Mirror light reflection vector on surface
                        float diffuseVdotH = clampedDot(v, normalize(l_mirror + v));
                        dielectric_fresnel = pl_fresnel_schlick(materialInfo.f0_dielectric * materialInfo.specularWeight, materialInfo.f90_dielectric, abs(diffuseVdotH));
                        if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_VOLUME))
                        {
                            diffuse_btdf = applyVolumeAttenuation(diffuse_btdf, diffuseTransmissionThickness, materialInfo.attenuationColor, materialInfo.attenuationDistance);
                        }
                        l_diffuse += diffuse_btdf * materialInfo.diffuseTransmissionFactor;
                    }
                }

                if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_TRANSMISSION))
                {
                        // If the light ray travels through the geometry, use the point it exits the geometry again.
                        // That will change the angle to the light source, if the material refracts the light ray.
                        vec3 transmissionRay = getVolumeTransmissionRay(n, v, materialInfo.thickness, materialInfo.ior, u_ModelMatrix);
                        pointToLight -= transmissionRay;
                        l = normalize(pointToLight);

                        vec3 transmittedLight = shadow * intensity * getPunctualRadianceTransmission(n, v, l, materialInfo.alphaRoughness, tBaseColor.rgb, materialInfo.ior);

                        if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_VOLUME))
                        {
                            transmittedLight = applyVolumeAttenuation(transmittedLight, length(transmissionRay), materialInfo.attenuationColor, materialInfo.attenuationDistance);
                        }
                        l_diffuse = mix(l_diffuse, transmittedLight, materialInfo.transmissionFactor);
                }

                if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_ANISOTROPY))
                {
                    l_specular_metal = shadow * intensity * NdotL * BRDF_specularGGXAnisotropy(materialInfo.alphaRoughness, materialInfo.anisotropyStrength, n, v, l, h, materialInfo.anisotropicT, materialInfo.anisotropicB);
                    l_specular_dielectric = l_specular_metal;
                }
                else
                {
                    l_specular_metal = shadow * intensity * NdotL * pl_brdf_specular(materialInfo.alphaRoughness, NdotL, NdotV, NdotH);
                    l_specular_dielectric = l_specular_metal;
                }

                l_metal_brdf = metal_fresnel * l_specular_metal;
                l_dielectric_brdf = mix(l_diffuse, l_specular_dielectric, dielectric_fresnel); // Do we need to handle vec3 fresnel here?
        
                if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_IRIDESCENCE))
                {
                    l_metal_brdf = mix(l_metal_brdf, l_specular_metal * iridescenceFresnel_metallic, materialInfo.iridescenceFactor);
                    l_dielectric_brdf = mix(l_dielectric_brdf, pl_rgb_mix(l_diffuse, l_specular_dielectric, iridescenceFresnel_dielectric), materialInfo.iridescenceFactor);
                }

                if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_CLEARCOAT))
                {
                    l_clearcoat_brdf = intensity * getPunctualRadianceClearCoat(materialInfo.clearcoatNormal, v, l, h, VdotH,
                        materialInfo.clearcoatF0, materialInfo.clearcoatF90, materialInfo.clearcoatRoughness);
                }

                if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_SHEEN))
                {
                    l_sheen = intensity * getPunctualRadianceSheen(materialInfo.sheenColorFactor, materialInfo.sheenRoughnessFactor, NdotL, NdotV, NdotH);
                    l_albedoSheenScaling = min(1.0 - pl_max3(materialInfo.sheenColorFactor) * albedoSheenScalingLUT(NdotV, materialInfo.sheenRoughnessFactor),
                        1.0 - pl_max3(materialInfo.sheenColorFactor) * albedoSheenScalingLUT(NdotL, materialInfo.sheenRoughnessFactor));
                }

                vec3 l_color = mix(l_dielectric_brdf, l_metal_brdf, materialInfo.metallic);
                l_color = l_sheen + l_color * l_albedoSheenScaling;
                l_color = mix(l_color, l_clearcoat_brdf, clearcoatFactor * clearcoatFresnel);
                color += l_color;
            }


        }

    }

    // emissive
    f_emissive = material.tEmissiveFactor * material.fEmissiveStrength;
    if(bool(iTextureMappingFlags & PL_HAS_EMISSIVE_MAP))
    {
        f_emissive *= pl_srgb_to_linear(texture(sampler2D(at2DTextures[nonuniformEXT(material.aiTextureIndices[PL_TEXTURE_EMISSIVE])], tSamplerLinearRepeat), pl_get_uv(PL_TEXTURE_EMISSIVE)).rgb);
    }

    if(tShaderDebugMode == PL_SHADER_DEBUG_MODE_NONE)
    {

        if(material.tAlphaMode == PL_SHADER_ALPHA_MODE_MASK)
        {
            if(tBaseColor.a <  material.fAlphaCutoff)
            {
                discard;
            }
            tBaseColor.a = 1.0;
        }

        color = f_emissive * (1.0 - clearcoatFactor * clearcoatFresnel) + color;
        outColor.rgb = color.rgb;
        outColor.a = tBaseColor.a;

        if(bool(tGpuScene.tData.iSceneFlags & PL_SCENE_FLAG_HEIGHT_FOG))
        {
            outColor = fog(outColor, vraw);
        }
        else if(bool(tGpuScene.tData.iSceneFlags & PL_SCENE_FLAG_LINEAR_FOG))
        {
            outColor = fogLinear(outColor, vraw);
        }
    }
    else
    {
        // In case of missing data for a debug view, render a checkerboard.
        outColor = vec4(1.0);
        {
            float frequency = 0.02;
            float gray = 0.9;

            vec2 v1 = step(0.5, fract(frequency * gl_FragCoord.xy));
            vec2 v2 = step(0.5, vec2(1.0) - fract(frequency * gl_FragCoord.xy));
            outColor.rgb *= gray + v1.x * v1.y + v2.x * v2.y;
        }

        if(tShaderDebugMode == PL_SHADER_DEBUG_BASE_COLOR)
        {
            outColor = tBaseColor;
        }

        if(tShaderDebugMode == PL_SHADER_DEBUG_SHADING_NORMAL)
        {
            outColor = vec4((n + 1.0) / 2.0, tBaseColor.a);
            // outColor = vec4(vec3(albedoSheenScaling), tBaseColor.a);
        }

        if(tShaderDebugMode == PL_SHADER_DEBUG_GEOMETRY_NORMAL)
        {
            outColor = vec4((tNormalInfo.ng + 1.0) / 2.0, tBaseColor.a);
        }

        if(tShaderDebugMode == PL_SHADER_DEBUG_GEOMETRY_TANGENT)
        {
            outColor = vec4((tNormalInfo.t + 1.0) / 2.0, tBaseColor.a);
        }

        if(tShaderDebugMode == PL_SHADER_DEBUG_GEOMETRY_BITANGENT)
        {
            outColor.rgb = (tNormalInfo.b + 1.0) / 2.0;
        }

        if(tShaderDebugMode == PL_SHADER_DEBUG_TEXTURE_NORMAL)
        {
            if(bool(iTextureMappingFlags & PL_HAS_NORMAL_MAP))
            {
                outColor = vec4((tNormalInfo.ntex + 1.0) / 2.0, tBaseColor.a);
            }
        }

        if(tShaderDebugMode == PL_SHADER_DEBUG_METALLIC)
        {
            outColor.rgb = vec3(materialInfo.metallic);
        }

        if(tShaderDebugMode == PL_SHADER_DEBUG_ROUGHNESS)
        {
            outColor.rgb = vec3(materialInfo.perceptualRoughness);
        }

        if(tShaderDebugMode == PL_SHADER_DEBUG_UV0)
        {
            if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0))
            {
                outColor.rgb = vec3(tShaderIn.tUV[0], 0.0);
            }
        }

        if(tShaderDebugMode == PL_SHADER_DEBUG_ALPHA)
        {
            outColor.rgb = vec3(tBaseColor.a);
        }

        if(tShaderDebugMode == PL_SHADER_DEBUG_EMMISSIVE)
        {
            outColor.rgb = pl_linear_to_srgb(f_emissive);
        }

        if(tShaderDebugMode == PL_SHADER_DEBUG_OCCLUSION)
        {
            outColor.rgb = vec3(ao);
        }

        if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_CLEARCOAT))
        {

            if(tShaderDebugMode == PL_SHADER_DEBUG_CLEARCOAT)
            {
                outColor.rgb = vec3(materialInfo.clearcoatFactor);
            }

            if(tShaderDebugMode == PL_SHADER_DEBUG_CLEARCOAT_ROUGHNESS)
            {
                outColor.rgb = vec3(materialInfo.clearcoatRoughness);
            }

            if(tShaderDebugMode == PL_SHADER_DEBUG_CLEARCOAT_NORMAL)
            {
                outColor.rgb = (materialInfo.clearcoatNormal + vec3(1)) / 2.0;
            }
        }

        if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_SHEEN))
        {
            if(tShaderDebugMode == PL_SHADER_DEBUG_SHEEN_COLOR)
            {
                outColor.rgb = materialInfo.sheenColorFactor;
            }
            if(tShaderDebugMode == PL_SHADER_DEBUG_SHEEN_ROUGHNESS)
            {
                outColor.rgb = vec3(materialInfo.sheenRoughnessFactor);
            }
        }

        if(tShaderDebugMode == PL_SHADER_DEBUG_IRIDESCENCE_FACTOR)
        {
            outColor.rgb = vec3(materialInfo.iridescenceFactor);
        }

        if(tShaderDebugMode == PL_SHADER_DEBUG_IRIDESCENCE_THICKNESS)
        {
            outColor.rgb = vec3(materialInfo.iridescenceThickness / 1200.0);
        }

        if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_ANISOTROPY))
        {
            if(tShaderDebugMode == PL_SHADER_DEBUG_ANISOTROPY_STRENGTH)
            {
                outColor.rgb = vec3(materialInfo.anisotropyStrength);
            }
            if(tShaderDebugMode == PL_SHADER_DEBUG_ANISOTROPY_DIRECTION)
            {
                vec2 direction = vec2(1.0, 0.0);
                if(bool(iTextureMappingFlags & PL_HAS_ANISOTROPY_MAP))
                {
                    direction = texture(sampler2D(at2DTextures[nonuniformEXT(material.aiTextureIndices[PL_TEXTURE_ANISOTROPY])], tSamplerLinearRepeat), pl_get_uv(PL_TEXTURE_ANISOTROPY)).xy;
                    direction = direction * 2.0 - vec2(1.0); // [0, 1] -> [-1, 1]
                }
                vec2 directionRotation = material.tAnisotropy.xy; // cos(theta), sin(theta)
                mat2 rotationMatrix = mat2(directionRotation.x, directionRotation.y, -directionRotation.y, directionRotation.x);
                direction = (direction + vec2(1.0)) * 0.5; // [-1, 1] -> [0, 1]
                outColor.rgb = vec3(direction, 0.0);
            }
        }

        if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_TRANSMISSION))
        {
            if(tShaderDebugMode == PL_SHADER_DEBUG_TRANSMISSION_STRENGTH)
            {
                outColor.rgb = vec3(materialInfo.transmissionFactor);
            }
        }

        if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_VOLUME))
        {
            if(tShaderDebugMode == PL_SHADER_DEBUG_VOLUME_THICKNESS)
            {
                outColor.rgb = vec3(materialInfo.thickness / material.fThickness);
            }
        }

        if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_DIFFUSE_TRANSMISSION))
        {
            if(tShaderDebugMode == PL_SHADER_DEBUG_DIFFUSE_TRANSMISSION_STRENGTH)
            {
                outColor.rgb = pl_linear_to_srgb(vec3(materialInfo.diffuseTransmissionFactor));
            }
            if(tShaderDebugMode == PL_SHADER_DEBUG_DIFFUSE_TRANSMISSION_COLOR)
            {
                outColor.rgb = pl_linear_to_srgb(vec3(materialInfo.diffuseTransmissionColorFactor));
            }
        }
    }

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