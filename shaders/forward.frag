#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "bg_scene.inc"
#include "bg_view.inc"
#include "brdf.glsl"

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
    vec2 tUV[8];
    vec4 tColor;
    vec3 tWorldNormal;
    mat3 tTBN;
} tShaderIn;

#include "math.glsl"
#include "lighting.glsl"
#include "material_info.glsl"

struct NormalInfo {
    vec3 ng;   // Geometry normal
    vec3 t;    // Geometry tangent
    vec3 b;    // Geometry bitangent
    vec3 n;    // Shading normal
    vec3 ntex; // Normal from texture, scaling is accounted for.
};

NormalInfo
pl_get_normal_info(int iUVSet)
{
    vec2 UV = tShaderIn.tUV[iUVSet];
    vec2 uv_dx = dFdx(UV);
    vec2 uv_dy = dFdy(UV);

    // if (length(uv_dx) <= 0.0001) {
    //   uv_dx = vec2(1.0, 0.0);
    // }

    // if (length(uv_dy) <= 0.0001) {
    //   uv_dy = vec2(0.0, 1.0);
    // }

    vec3 t_ = (uv_dy.t * dFdx(tShaderIn.tWorldPosition) - uv_dx.t * dFdy(tShaderIn.tWorldPosition)) /
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
        ng = normalize(cross(dFdx(tShaderIn.tWorldPosition), dFdy(tShaderIn.tWorldPosition)));
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
    if(bool(iTextureMappingFlags & PL_HAS_NORMAL_MAP) && bool(iRenderingFlags & PL_RENDERING_FLAG_USE_NORMAL_MAPS)) 
    {
        plGpuMaterial material = tMaterialInfo.atMaterials[tObjectInfo.tData.iMaterialIndex];
        info.ntex = texture(sampler2D(at2DTextures[nonuniformEXT(material.iNormalTexIdx)], tSamplerLinearRepeat), UV).rgb * 2.0 - vec3(1.0);
        // info.ntex *= vec3(0.2, 0.2, 1.0);
        // info.ntex *= vec3(u_NormalScale, u_NormalScale, 1.0);
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

vec4
getBaseColor(vec4 u_ColorFactor, int iUVSet)
{
    vec4 baseColor = vec4(1);

    // if(bool(MATERIAL_SPECULARGLOSSINESS))
    // {
    //     baseColor = u_DiffuseFactor;
    // }
    // else if(bool(MATERIAL_METALLICROUGHNESS))
    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_METALLIC_ROUGHNESS))
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
    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_METALLIC_ROUGHNESS) && bool(iTextureMappingFlags & PL_HAS_BASE_COLOR_MAP))
    {
        plGpuMaterial material = tMaterialInfo.atMaterials[tObjectInfo.tData.iMaterialIndex];
        baseColor *= pl_srgb_to_linear(texture(sampler2D(at2DTextures[nonuniformEXT(material.iBaseColorTexIdx)], tSamplerLinearRepeat), tShaderIn.tUV[iUVSet]));
    }
    return baseColor * tShaderIn.tColor;
}

MaterialInfo
getMetallicRoughnessInfo(MaterialInfo info, float u_MetallicFactor, float u_RoughnessFactor, int UVSet)
{
    info.metallic = u_MetallicFactor;
    info.perceptualRoughness = u_RoughnessFactor;

    if(bool(iTextureMappingFlags & PL_HAS_METALLIC_ROUGHNESS_MAP))
    {
        // Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
        // This layout intentionally reserves the 'r' channel for (optional) occlusion map data
        plGpuMaterial material = tMaterialInfo.atMaterials[tObjectInfo.tData.iMaterialIndex];
        vec4 mrSample = texture(sampler2D(at2DTextures[nonuniformEXT(material.iMetallicRoughnessTexIdx)], tSamplerLinearRepeat), tShaderIn.tUV[UVSet]);
        info.perceptualRoughness *= mrSample.g;
        info.metallic *= mrSample.b;
    }

    // Achromatic f0 based on IOR.
    info.c_diff = mix(info.baseColor.rgb,  vec3(0), info.metallic);
    info.f0_dielectric = mix(info.f0_dielectric, info.baseColor.rgb, info.metallic);
    return info;
}

vec3
getPunctualRadianceClearCoat(vec3 clearcoatNormal, vec3 v, vec3 l, vec3 h, float VdotH, vec3 f0, vec3 f90, float clearcoatRoughness)
{
    float NdotL = clampedDot(clearcoatNormal, l);
    float NdotV = clampedDot(clearcoatNormal, v);
    float NdotH = clampedDot(clearcoatNormal, h);
    return NdotL * pl_brdf_specular(clearcoatRoughness * clearcoatRoughness, NdotL, NdotV, NdotH);
}

vec3
getClearcoatNormal(NormalInfo normalInfo, int iClearcoatNormalTexIdx, int UVSet)
{
    if(bool(iTextureMappingFlags & PL_HAS_CLEARCOAT_NORMAL_MAP))
    {
        vec3 n = texture(sampler2D(at2DTextures[nonuniformEXT(iClearcoatNormalTexIdx)], tSamplerLinearRepeat), tShaderIn.tUV[UVSet]).rgb * 2.0 - vec3(1.0);
        // n *= vec3(u_ClearcoatNormalScale, u_ClearcoatNormalScale, 1.0);
        n = mat3(normalInfo.t, normalInfo.b, normalInfo.ng) * normalize(n);
        return n;
    }
    else
    {
        return normalInfo.ng;
    }
}

MaterialInfo
getClearCoatInfo(MaterialInfo info, NormalInfo tNormalInfo, float u_ClearcoatFactor, float u_ClearcoatRoughnessFactor, int UVSet0, int UVSet1, int UVSet2)
{
    info.clearcoatFactor = u_ClearcoatFactor;
    info.clearcoatRoughness = u_ClearcoatRoughnessFactor;
    info.clearcoatF0 = vec3(pow((info.ior - 1.0) / (info.ior + 1.0), 2.0));
    info.clearcoatF90 = vec3(1.0);

    plGpuMaterial material = tMaterialInfo.atMaterials[tObjectInfo.tData.iMaterialIndex];

    if(bool(iTextureMappingFlags & PL_HAS_CLEARCOAT_MAP))
    {
        vec4 clearcoatSample = texture(sampler2D(at2DTextures[nonuniformEXT(material.iClearcoatTexIdx)], tSamplerLinearRepeat), tShaderIn.tUV[UVSet0]);
        info.clearcoatFactor *= clearcoatSample.r;
    }

    if(bool(iTextureMappingFlags & PL_HAS_CLEARCOAT_ROUGHNESS_MAP))
    {
        vec4 clearcoatSampleRoughness = texture(sampler2D(at2DTextures[nonuniformEXT(material.iClearcoatRoughnessTexIdx)], tSamplerLinearRepeat), tShaderIn.tUV[UVSet1]);
        info.clearcoatRoughness *= clearcoatSampleRoughness.g;
    }

    info.clearcoatNormal = getClearcoatNormal(tNormalInfo, material.iClearcoatNormalTexIdx, UVSet2);
    info.clearcoatRoughness = clamp(info.clearcoatRoughness, 0.0, 1.0);
    return info;
}

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void main() 
{

    plGpuMaterial material = tMaterialInfo.atMaterials[tObjectInfo.tData.iMaterialIndex];
    vec4 tBaseColor = getBaseColor(material.tBaseColorFactor, material.iBaseColorUVSet);
    vec3 color = vec3(0);

    if(tShaderDebugMode == PL_SHADER_DEBUG_MODE_NONE)
    {
        if(tBaseColor.a <  material.fAlphaCutoff)
        {
            discard;
        }
    }

    NormalInfo tNormalInfo = pl_get_normal_info(material.iNormalUVSet);

    vec3 n = tNormalInfo.n;
    vec3 t = tNormalInfo.t;
    // vec3 b = tNormalInfo.b;

    MaterialInfo materialInfo;
    materialInfo.baseColor = tBaseColor.rgb;

    // The default index of refraction of 1.5 yields a dielectric normal incidence reflectance of 0.04.
    materialInfo.ior = 1.5;
    materialInfo.f0_dielectric = vec3(0.04);
    materialInfo.specularWeight = 1.0;

    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_METALLIC_ROUGHNESS))
    {
        materialInfo = getMetallicRoughnessInfo(materialInfo, material.fMetallicFactor, material.fRoughnessFactor, material.iMetallicRoughnessUVSet);
    }

    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_CLEARCOAT))
    {
        materialInfo = getClearCoatInfo(materialInfo, tNormalInfo, material.fClearcoatFactor,
            material.fClearcoatRoughnessFactor, material.iClearcoatUVSet, material.iClearcoatRoughnessUVSet,
            material.iClearcoatNormalUVSet);
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

    float clearcoatFactor = 0.0;
    vec3 clearcoatFresnel = vec3(0);

    vec3 v = normalize(tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tCameraPos.xyz - tShaderIn.tWorldPosition.xyz);

    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_CLEARCOAT))
    {
        clearcoatFactor = materialInfo.clearcoatFactor;
        clearcoatFresnel = pl_fresnel_schlick(materialInfo.clearcoatF0, materialInfo.clearcoatF90, clampedDot(materialInfo.clearcoatNormal, v));
    }
    
    // Calculate lighting contribution from image based lighting source (IBL)
    if(bool(iRenderingFlags & PL_RENDERING_FLAG_USE_IBL) && iProbeCount > 0)
    {
        int iProbeIndex = 0;
        float fCurrentDistance = 10000.0;
        for(int i = iProbeCount - 1; i > -1; i--)
        {
            vec3 tDist = tProbeData.atData[i].tPosition - tShaderIn.tWorldPosition.xyz;
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
            f_diffuse = getDiffuseLight(n, iProbeIndex) * tBaseColor.rgb;

            int iMips = textureQueryLevels(samplerCube(atCubeTextures[nonuniformEXT(tProbeData.atData[iProbeIndex].uGGXEnvSampler)], tSamplerNearestRepeat));
            f_specular_metal = getIBLRadianceGGX(n, v, materialInfo.perceptualRoughness, iMips, tShaderIn.tWorldPosition.xyz, iProbeIndex);
            f_specular_dielectric = f_specular_metal;

            // Calculate fresnel mix for IBL  

            vec3 f_metal_fresnel_ibl = getIBLGGXFresnel(n, v, materialInfo.perceptualRoughness, tBaseColor.rgb, 1.0, iProbeIndex);
            f_metal_brdf_ibl = f_metal_fresnel_ibl * f_specular_metal;
        
            vec3 f_dielectric_fresnel_ibl = getIBLGGXFresnel(n, v, materialInfo.perceptualRoughness, materialInfo.f0_dielectric, materialInfo.specularWeight, iProbeIndex);
            f_dielectric_brdf_ibl = mix(f_diffuse, f_specular_dielectric,  f_dielectric_fresnel_ibl);

            if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_CLEARCOAT))
            {
                clearcoat_brdf = getIBLRadianceGGX(materialInfo.clearcoatNormal, v, materialInfo.clearcoatRoughness, iMips, tShaderIn.tWorldPosition.xyz, iProbeIndex);
            }

            color = mix(f_dielectric_brdf_ibl, f_metal_brdf_ibl, materialInfo.metallic);
            color = mix(color, clearcoat_brdf, clearcoatFactor * clearcoatFresnel);
        }
    }

    // ambient occlusion
    
    float ao = 1.0;
    if(bool(iTextureMappingFlags & PL_HAS_OCCLUSION_MAP))
    {
        float u_OcclusionStrength = 1.0;
        ao = texture(sampler2D(at2DTextures[nonuniformEXT(material.iOcclusionTexIdx)], tSamplerLinearRepeat), tShaderIn.tUV[material.iOcclusionUVSet]).r;
        color = color * (1.0 + u_OcclusionStrength * (ao - 1.0)); 
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

            float shadow = 1.0;

            if(tLightData.iType == PL_LIGHT_TYPE_DIRECTIONAL)
            {

                vec3 pointToLight = -tLightData.tDirection;



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
                        
                            shadow = filterPCF(shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset) + vec2(j * tShadowData.fFactor, 0), tShadowData.iShadowMapTexIdx);
                            // shadow = textureProj(shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset) + vec2(j * tShadowData.fFactor, 0), tShadowData.iShadowMapTexIdx);

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
                                // shadow = 100.0;
                            }

                            break;
                        }
                    }
                    // for(int j = 0; j < 4; j++)
                    // {
                    //     // int index = int(16.0*random(gl_FragCoord.xyy, j))%16;
                    //     int index = int(16.0*random(tShaderIn.tWorldPosition.xxx, j))%16;
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

                    l_specular_metal = shadow * intensity * NdotL * pl_brdf_specular(materialInfo.alphaRoughness, NdotL, NdotV, NdotH);
                    l_specular_dielectric = l_specular_metal;

                    l_metal_brdf = metal_fresnel * l_specular_metal;
                    l_dielectric_brdf = mix(l_diffuse, l_specular_dielectric, dielectric_fresnel); // Do we need to handle vec3 fresnel here?
            
                    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_CLEARCOAT))
                    {
                        l_clearcoat_brdf = intensity * getPunctualRadianceClearCoat(materialInfo.clearcoatNormal, v, l, h, VdotH,
                            materialInfo.clearcoatF0, materialInfo.clearcoatF90, materialInfo.clearcoatRoughness);
                    }

                    vec3 l_color = mix(l_dielectric_brdf, l_metal_brdf, materialInfo.metallic);
                    l_color = mix(l_color, l_clearcoat_brdf, clearcoatFactor * clearcoatFresnel);
                    color += l_color;
                }
            }

            else if(tLightData.iType == PL_LIGHT_TYPE_POINT)
            {
                vec3 pointToLight = tLightData.tPosition - tShaderIn.tWorldPosition.xyz;

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

                // BSTF
                vec3 l = normalize(pointToLight);   // Direction from surface point to light
                vec3 h = normalize(l + v);          // Direction of the vector between l and v, called halfway vector
                float NdotL = clampedDot(n, l);
                float NdotV = clampedDot(n, v);
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

                    l_specular_metal = shadow * intensity * NdotL * pl_brdf_specular(materialInfo.alphaRoughness, NdotL, NdotV, NdotH);
                    l_specular_dielectric = l_specular_metal;

                    l_metal_brdf = metal_fresnel * l_specular_metal;
                    l_dielectric_brdf = mix(l_diffuse, l_specular_dielectric, dielectric_fresnel); // Do we need to handle vec3 fresnel here?
            
                    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_CLEARCOAT))
                    {
                        l_clearcoat_brdf = intensity * getPunctualRadianceClearCoat(materialInfo.clearcoatNormal, v, l, h, VdotH,
                            materialInfo.clearcoatF0, materialInfo.clearcoatF90, materialInfo.clearcoatRoughness);
                    }

                    vec3 l_color = mix(l_dielectric_brdf, l_metal_brdf, materialInfo.metallic);
                    l_color = mix(l_color, l_clearcoat_brdf, clearcoatFactor * clearcoatFresnel);
                    color += l_color;
                }
            }

            else if(tLightData.iType == PL_LIGHT_TYPE_SPOT)
            {
                vec3 pointToLight = tLightData.tPosition - tShaderIn.tWorldPosition.xyz;

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

                    l_specular_metal = shadow * intensity * NdotL * pl_brdf_specular(materialInfo.alphaRoughness, NdotL, NdotV, NdotH);
                    l_specular_dielectric = l_specular_metal;

                    l_metal_brdf = metal_fresnel * l_specular_metal;
                    l_dielectric_brdf = mix(l_diffuse, l_specular_dielectric, dielectric_fresnel); // Do we need to handle vec3 fresnel here?
            
                    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_CLEARCOAT))
                    {
                        l_clearcoat_brdf = intensity * getPunctualRadianceClearCoat(materialInfo.clearcoatNormal, v, l, h, VdotH,
                            materialInfo.clearcoatF0, materialInfo.clearcoatF90, materialInfo.clearcoatRoughness);
                    }

                    vec3 l_color = mix(l_dielectric_brdf, l_metal_brdf, materialInfo.metallic);
                    l_color = mix(l_color, l_clearcoat_brdf, clearcoatFactor * clearcoatFresnel);
                    color += l_color;
                }
            }


        }

    }

    // emissive
    f_emissive = material.tEmissiveFactor;
    if(bool(iTextureMappingFlags & PL_HAS_EMISSIVE_MAP))
    {
        f_emissive *= pl_srgb_to_linear(texture(sampler2D(at2DTextures[nonuniformEXT(material.iEmissiveTexIdx)], tSamplerLinearRepeat), tShaderIn.tUV[material.iEmissiveUVSet]).rgb);
    }

    if(tShaderDebugMode == PL_SHADER_DEBUG_MODE_NONE)
    {
        color = f_emissive + color;
        outColor.rgb = color.rgb;
        outColor.a = tBaseColor.a;
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