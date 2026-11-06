#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "pl_bg_scene.inc"
#include "pl_bg_view.inc"
#include "pl_cluster_lighting.glsl"
#include "pl_brdf.glsl"
#include "pl_math.glsl"

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    plGpuDynForwardData tData;
} tObjectInfo;

//-----------------------------------------------------------------------------
// [SECTION] input & output
//-----------------------------------------------------------------------------


layout(location = 0) out vec4 outColor;

// output
layout(location = 0) in plShaderOut {
    vec3 tWorldPosition;
    vec3 tViewPosition;
    vec2 tUV[2];
    vec4 tColor;
    vec3 tWorldNormal;
    mat3 tTBN;
    mat4 tModel; 
    flat uvec4 tSelectedProbes;
    flat uint uProbeSelection;
} tShaderIn;

#include "pl_math.glsl"
#include "pl_lighting.glsl"
#include "pl_fog.glsl"

#define PL_INCLUDE_MATERIAL_FUNCTIONS
#include "pl_material_info.glsl"

// Conversion FO/IOR
vec3 IorToFresnel0(vec3 transmittedIor, float incidentIor) {
    return sq((transmittedIor - vec3(incidentIor)) / (transmittedIor + vec3(incidentIor)));
}

// ior is a value between 1.0 and 3.0. 1.0 is air interface
float IorToFresnel0(float transmittedIor, float incidentIor) {
    return sq((transmittedIor - incidentIor) / (transmittedIor + incidentIor));
}

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void main() 
{
    int iClusterBase = pl_cluster_base(tShaderIn.tWorldPosition, tObjectInfo.tData.uGlobalIndex);
    int iPointCount = pl_cluster_count(iClusterBase, 0, tObjectInfo.tData.iPointLightCount);
    int iSpotCount = pl_cluster_count(iClusterBase, 1, tObjectInfo.tData.iSpotLightCount);


    plGpuMaterial material = tMaterialInfo.atMaterials[tObjectInfo.tData.iMaterialIndex];
    vec4 tBaseColor = getBaseColor(material.tBaseColorFactor, material.aiTextureUVSet[PL_TEXTURE_BASE_COLOR], tObjectInfo.tData.iMaterialIndex);


    if(tBaseColor.a <  material.fAlphaCutoff)
    {
        discard;
    }
    tBaseColor.a = 1.0;

    vec3 color = vec3(0);

    NormalInfo tNormalInfo = pl_get_normal_info(tObjectInfo.tData.iMaterialIndex);

    vec3 n = tNormalInfo.n;
    vec3 t = tNormalInfo.t;
    // vec3 b = tNormalInfo.b;

    vec3 vraw = tViewInfo.tData.tCameraPos.xyz - tShaderIn.tWorldPosition.xyz;
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

    materialInfo = getIorInfo(materialInfo, tObjectInfo.tData.iMaterialIndex);

    materialInfo = getMetallicRoughnessInfo(materialInfo, material.fMetallicFactor, material.fRoughnessFactor, tObjectInfo.tData.iMaterialIndex);

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

    // Calculate lighting contribution from image based lighting source (IBL)
    if(bool(tGpuScene.tData.iSceneFlags & PL_SCENE_FLAG_USE_IBL) && tShaderIn.tSelectedProbes.w > 0u)
    {

        // The CPU supplies up to three probes, sorted nearest first.
        int aiActiveProbes[3] = int[3](int(tShaderIn.tSelectedProbes.x), int(tShaderIn.tSelectedProbes.y), int(tShaderIn.tSelectedProbes.z));
        int K = int(tShaderIn.tSelectedProbes.w);
        vec3 R = reflect(-v, n);

        float weights[3];
        weights[0] = 0.0;
        weights[1] = 0.0;
        weights[2] = 0.0;
        float summing = computeProbeWeights(tShaderIn.tWorldPosition.xyz, R, 2.0, K, aiActiveProbes, weights);
        int iClosestProbeIndex = aiActiveProbes[0];

        int iMips2 = tProbeData.atData[iClosestProbeIndex].iMips;
        if(bool(material.iMaterialFlags & PL_MATERIAL_SHADER_FLAG_ANISOTROPY))
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

            int iMips = tProbeData.atData[iClosestProbeIndex].iMips;

            // Calculate fresnel mix for IBL  

            vec3 f_metal_fresnel_ibl = getIBLGGXFresnel(n, v, materialInfo.perceptualRoughness, tBaseColor.rgb, 1.0, iProbeIndex);
            f_metal_brdf_ibl = f_metal_fresnel_ibl * f_specular_metal;
        
            vec3 f_dielectric_fresnel_ibl = getIBLGGXFresnel(n, v, materialInfo.perceptualRoughness, materialInfo.f0_dielectric, materialInfo.specularWeight, iProbeIndex);
            f_dielectric_brdf_ibl = mix(f_diffuse, f_specular_dielectric,  f_dielectric_fresnel_ibl);

            vec3 icolor = mix(f_dielectric_brdf_ibl, f_metal_brdf_ibl, materialInfo.metallic);
            color += icolor * weights[i];
        }
    }

    // punctual stuff
    f_diffuse = vec3(0.0);
    f_specular_dielectric = vec3(0.0);
    f_specular_metal = vec3(0.0);
    vec3 f_dielectric_brdf = vec3(0.0);
    vec3 f_metal_brdf = vec3(0.0);

    uint cascadeIndex = tGpuScene.tData.iCascadeCount - 1;
    const bool bShadows = bool(tObjectInfo.tData.iObjectShaderFlags & PL_OBJECT_SHADER_FLAG_SHADOWS) && bool(tGpuScene.tData.iSceneFlags & PL_SCENE_FLAG_SHADOWS);
    const bool bPunctual = bool(tGpuScene.tData.iSceneFlags & PL_SCENE_FLAG_PUNCTUAL);
    if(bPunctual)
    {

        // main sun light
        {

            float shadow = 1.0;

            vec3 pointToLight = -tGpuScene.tData.tDirection;

            int iCascadeCount = tGpuScene.tData.iCascadeCount;
            if(bool(tObjectInfo.tData.iObjectShaderFlags & PL_OBJECT_SHADER_FLAG_PROBE))
            {
                iCascadeCount = 1;
            }

            if(bShadows && tGpuScene.tData.iCastShadow > 0)
            {
                // Depth compare for shadowing
                mat4 abiasMat = biasMat;
                abiasMat[0][0] *= tGpuScene.tData.fFactor;
                abiasMat[1][1] *= tGpuScene.tData.fFactor;
                abiasMat[3][0] *= tGpuScene.tData.fFactor;
                abiasMat[3][1] *= tGpuScene.tData.fFactor;
                shadow = 0;

                vec4 inViewPos = tViewInfo.tData.tCameraView[tObjectInfo.tData.uGlobalIndex] * vec4(tShaderIn.tWorldPosition.xyz, 1.0);

                // Get cascade index for the current fragment's view position
                float viewDepth = (inViewPos.z - tViewInfo.tData.fCameraNearZ) / tViewInfo.tData.fCameraRange;

                vec4 tWorldPos2 = vec4(tShaderIn.tWorldPosition.xyz, 1.0);
                if(!bool(tObjectInfo.tData.iObjectShaderFlags & PL_OBJECT_SHADER_FLAG_PROBE))
                {
                    for(int j = 0; j < iCascadeCount - 1; j++)
                    {
                        if(viewDepth <= tViewInfo.tData.afCascadeSplits[j])
                        {
                            cascadeIndex = j;
                            break;
                        }
                    }
                }
                else
                {
                    cascadeIndex = 0;
                }

                // if(cascadeIndex < 4)
                {
                    vec4 shadowCoord = (abiasMat * tViewInfo.tData.viewProjMat[cascadeIndex]) * tWorldPos2;
                    // cascadeIndex = j;
                
                    if(bool(tGpuScene.tData.iSceneFlags & PL_SCENE_FLAG_PCF_SHADOWS))
                    {
                        shadow = filterPCFSimple(
                            shadowCoord,
                            vec2(tGpuScene.tData.fXOffset, tGpuScene.tData.fYOffset) + vec2(cascadeIndex * tGpuScene.tData.fFactor, 0),
                            tGpuScene.tData.iShadowMapTexIdx);
                            // cascadeIndex);
                    }
                    else
                    {
                        shadow = textureProj(shadowCoord, vec2(tGpuScene.tData.fXOffset, tGpuScene.tData.fYOffset) + vec2(cascadeIndex * tGpuScene.tData.fFactor, 0), tGpuScene.tData.iShadowMapTexIdx);
                    }

                    
                    // if(abs(rawshadowCoord.x - pl_saturate(rawshadowCoord.x)) < 0.00001 && abs(rawshadowCoord.y - pl_saturate(rawshadowCoord.y)) < 0.00001 && abs(rawshadowCoord.z - pl_saturate(rawshadowCoord.z)) < 0.00001)
                    if(!bool(tObjectInfo.tData.iObjectShaderFlags & PL_OBJECT_SHADER_FLAG_PROBE) && cascadeIndex < (iCascadeCount - 1))
                    {
                        float splitStart = (cascadeIndex == 0) ? 0.0 : tViewInfo.tData.afCascadeSplits[cascadeIndex - 1];
                        float splitEnd   = tViewInfo.tData.afCascadeSplits[cascadeIndex];

                        // width of fade region (10% of cascade)
                        float fadeRange = (splitEnd - splitStart) * 0.1;

                        // distance to end of cascade
                        float distToEnd = splitEnd - viewDepth;

                        // normalize fade
                        float cascade_fade = clamp(1.0 - distToEnd / fadeRange, 0.0, 1.0);

                        if(cascade_fade > 0)
                        {

                            shadowCoord = (abiasMat * tViewInfo.tData.viewProjMat[cascadeIndex + 1]) * tWorldPos2;
                            float shadowfallback = filterPCF(
                                shadowCoord,
                                vec2(tGpuScene.tData.fXOffset, tGpuScene.tData.fYOffset) + vec2((cascadeIndex + 1.0) * tGpuScene.tData.fFactor, 0),
                                tGpuScene.tData.iShadowMapTexIdx, cascadeIndex + 1);

                            shadow = mix(shadow, shadowfallback, cascade_fade);
                            // shadow = 100.0;
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


            if (NdotL > 0.0)
            {
                
                vec3 intensity = tGpuScene.tData.fIntensity * tGpuScene.tData.tColor;


                vec3 l_diffuse = shadow * intensity * NdotL * pl_brdf_diffuse(tBaseColor.rgb);
                vec3 l_specular_dielectric = vec3(0.0);
                vec3 l_specular_metal = vec3(0.0);
                vec3 l_dielectric_brdf = vec3(0.0);
                vec3 l_metal_brdf = vec3(0.0);
                vec3 l_sheen = vec3(0.0);

                if(NdotV > 0.0)
                {
                    l_specular_metal = shadow * intensity * NdotL * pl_brdf_specular(materialInfo.alphaRoughness, NdotL, NdotV, NdotH);
                    l_specular_dielectric = l_specular_metal;
                }

                l_metal_brdf = metal_fresnel * l_specular_metal;
                l_dielectric_brdf = mix(l_diffuse, l_specular_dielectric, dielectric_fresnel); // Do we need to handle vec3 fresnel here?

                vec3 l_color = mix(l_dielectric_brdf, l_metal_brdf, materialInfo.metallic);
                color += l_color;
            }
        }

        // point lights
        for(int i = 0; i < iPointCount; i++)
        {

            plGpuPointLight tLightData = tPointLightInfo.atData[pl_cluster_light(iClusterBase, 0, i)];

            float shadow = 1.0;

            vec3 pointToLight = tLightData.tPosition - tShaderIn.tWorldPosition.xyz;

            if(bShadows && tLightData.iCastShadow > 0)
            {
                plGpuPointLightShadow tShadowData = tPointShadowData.atData[tLightData.iShadowIndex];

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
                    if(bool(tGpuScene.tData.iSceneFlags & PL_SCENE_FLAG_PCF_SHADOWS))
                    {
                        shadow = filterPCFSimple(shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset) + faceoffsets[int(result.z)] * tShadowData.fFactor, tShadowData.iShadowMapTexIdx);
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
            float NdotH = clampedDot(n, h);
            float LdotH = clampedDot(l, h);
            float VdotH = clampedDot(v, h);

            vec3 dielectric_fresnel = pl_fresnel_schlick(materialInfo.f0_dielectric * materialInfo.specularWeight, materialInfo.f90_dielectric, abs(VdotH));
            vec3 metal_fresnel = pl_fresnel_schlick(tBaseColor.rgb, vec3(1.0), abs(VdotH));


            if (NdotL > 0.0 && NdotV > 0.0)
            {
                
                vec3 intensity = getLightIntensity(tLightData, pointToLight);


                vec3 l_diffuse = shadow * intensity * NdotL * pl_brdf_diffuse(tBaseColor.rgb);
                vec3 l_specular_dielectric = vec3(0.0);
                vec3 l_specular_metal = vec3(0.0);
                vec3 l_dielectric_brdf = vec3(0.0);
                vec3 l_metal_brdf = vec3(0.0);
                vec3 l_sheen = vec3(0.0);


                l_specular_metal = shadow * intensity * NdotL * pl_brdf_specular(materialInfo.alphaRoughness, NdotL, NdotV, NdotH);
                l_specular_dielectric = l_specular_metal;

                l_metal_brdf = metal_fresnel * l_specular_metal;
                l_dielectric_brdf = mix(l_diffuse, l_specular_dielectric, dielectric_fresnel); // Do we need to handle vec3 fresnel here?

                vec3 l_color = mix(l_dielectric_brdf, l_metal_brdf, materialInfo.metallic);
                color += l_color;
            }
        }

        // spot lights
        for(int i = 0; i < iSpotCount; i++)
        {

            plGpuSpotLight tLightData = tSpotLightInfo.atData[pl_cluster_light(iClusterBase, 1, i)];

            float shadow = 1.0;

            vec3 pointToLight = tLightData.tPosition - tShaderIn.tWorldPosition.xyz;

            if(bShadows && tLightData.iCastShadow > 0)
            {
                plGpuSpotLightShadow tShadowData = tSpotShadowData.atData[tLightData.iShadowIndex];

                vec4 shadowCoord = tShadowData.viewProjMat * vec4(tShaderIn.tWorldPosition.xyz, 1.0);
                if(shadowCoord.z > -1.0 && shadowCoord.z < 1.0)
                {
                    shadowCoord.xyz /= shadowCoord.w;
                    shadow = 1.0;
                    shadowCoord.x = shadowCoord.x/2 + 0.5;
                    shadowCoord.y = shadowCoord.y/2 + 0.5;
                    shadowCoord.xy *= tShadowData.fFactor;
                    // shadow = (shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset), tShadowData.iShadowMapTexIdx);

                    if(bool(tGpuScene.tData.iSceneFlags & PL_SCENE_FLAG_PCF_SHADOWS))
                    {
                        shadow = filterPCFSimple(shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset), tShadowData.iShadowMapTexIdx);
                    }
                    else
                    {
                        shadow = textureProj(shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset), tShadowData.iShadowMapTexIdx);
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


            if (NdotL > 0.0 && NdotV > 0.0)
            {
                
                vec3 intensity = getLightIntensity(tLightData, pointToLight);


                vec3 l_diffuse = shadow * intensity * NdotL * pl_brdf_diffuse(tBaseColor.rgb);
                vec3 l_specular_dielectric = vec3(0.0);
                vec3 l_specular_metal = vec3(0.0);
                vec3 l_dielectric_brdf = vec3(0.0);
                vec3 l_metal_brdf = vec3(0.0);
                vec3 l_sheen = vec3(0.0);


                l_specular_metal = shadow * intensity * NdotL * pl_brdf_specular(materialInfo.alphaRoughness, NdotL, NdotV, NdotH);
                l_specular_dielectric = l_specular_metal;

                l_metal_brdf = metal_fresnel * l_specular_metal;
                l_dielectric_brdf = mix(l_diffuse, l_specular_dielectric, dielectric_fresnel); // Do we need to handle vec3 fresnel here?

                vec3 l_color = mix(l_dielectric_brdf, l_metal_brdf, materialInfo.metallic);
                color += l_color;
            }
        }

        // direction lights
        for(int i = 0; i < tObjectInfo.tData.iDirectionLightCount; i++)
        {

            plGpuDirectionLight tLightData = tDirectionLightInfo.atData[i];

            float shadow = 1.0;

            vec3 pointToLight = -tLightData.tDirection;

            if(bShadows && tLightData.iCastShadow > 0)
            {
                int iShadowIndex = tLightData.iShadowIndex;
                plGpuDirectionLightShadow tShadowData = tDirectionShadowData.atData[iShadowIndex];

                // Depth compare for shadowing
                mat4 abiasMat = biasMat;
                abiasMat[0][0] *= tShadowData.fFactor;
                abiasMat[1][1] *= tShadowData.fFactor;
                abiasMat[3][0] *= tShadowData.fFactor;
                abiasMat[3][1] *= tShadowData.fFactor;
                shadow = 0;

                vec4 inViewPos = tViewInfo.tData.tCameraView[tObjectInfo.tData.uGlobalIndex] * vec4(tShaderIn.tWorldPosition.xyz, 1.0);

                // Get cascade index for the current fragment's view position
                float viewDepth = (inViewPos.z - tViewInfo.tData.fCameraNearZ) / tViewInfo.tData.fCameraRange;

                vec4 tWorldPos2 = vec4(tShaderIn.tWorldPosition.xyz, 1.0);

                vec4 shadowCoord = (abiasMat * tDirectionShadowData.atData[iShadowIndex].viewProjMat) * tWorldPos2;
            
                if(bool(tGpuScene.tData.iSceneFlags & PL_SCENE_FLAG_PCF_SHADOWS))
                {
                    shadow = filterPCF(
                        shadowCoord,
                        vec2(tDirectionShadowData.atData[iShadowIndex].fXOffset, tDirectionShadowData.atData[iShadowIndex].fYOffset),
                        tDirectionShadowData.atData[iShadowIndex].iShadowMapTexIdx,
                        cascadeIndex);
                }
                else
                {
                    shadow = textureProj(shadowCoord, vec2(tDirectionShadowData.atData[iShadowIndex].fXOffset, tDirectionShadowData.atData[iShadowIndex].fYOffset), tDirectionShadowData.atData[iShadowIndex].iShadowMapTexIdx);
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


            if (NdotL > 0.0 && NdotV > 0.0)
            {
                
                vec3 intensity = getLightIntensity(tLightData, pointToLight);


                vec3 l_diffuse = shadow * intensity * NdotL * pl_brdf_diffuse(tBaseColor.rgb);
                vec3 l_specular_dielectric = vec3(0.0);
                vec3 l_specular_metal = vec3(0.0);
                vec3 l_dielectric_brdf = vec3(0.0);
                vec3 l_metal_brdf = vec3(0.0);
                vec3 l_sheen = vec3(0.0);


                l_specular_metal = shadow * intensity * NdotL * pl_brdf_specular(materialInfo.alphaRoughness, NdotL, NdotV, NdotH);
                l_specular_dielectric = l_specular_metal;

                l_metal_brdf = metal_fresnel * l_specular_metal;
                l_dielectric_brdf = mix(l_diffuse, l_specular_dielectric, dielectric_fresnel); // Do we need to handle vec3 fresnel here?

                vec3 l_color = mix(l_dielectric_brdf, l_metal_brdf, materialInfo.metallic);
                color += l_color;
            }
        }

    }

    outColor.rgb = color.rgb;
    outColor.a = tBaseColor.a;

    if(bool(tGpuScene.tData.iSceneFlags & PL_SCENE_FLAG_HEIGHT_FOG))
    {
        outColor = fog(outColor, vraw, tViewInfo.tData.tCameraPos.y);
    }
    else if(bool(tGpuScene.tData.iSceneFlags & PL_SCENE_FLAG_LINEAR_FOG))
    {
        outColor = fogLinear(outColor, vraw);
    }

    if(tGpuScene.tData.bCascadeDebug == 1)
    {
        switch(cascadeIndex) {
            case 0 : 
                outColor.rgb *= vec3(1.0f, 0.25f, 0.25f);
                break;
            case 1 : 
                outColor.rgb *= vec3(0.25f, 1.0f, 0.25f);
                break;
            case 2 : 
                outColor.rgb *= vec3(0.25f, 0.25f, 1.0f);
                break;
            case 3 : 
                outColor.rgb *= vec3(1.0f, 1.0f, 0.25f);
                break;
        }
    }
}