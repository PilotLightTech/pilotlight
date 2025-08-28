#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "pl_shader_interop_renderer.h"
#include "bg_scene.inc"
#include "bg_view.inc"
#include "math.glsl"
#include "brdf.glsl"

//-----------------------------------------------------------------------------
// [SECTION] specialication constants
//-----------------------------------------------------------------------------

layout(constant_id = 0) const int iRenderingFlags = 0;
layout(constant_id = 1) const int tShaderDebugMode = 0;
layout(constant_id = 2) const int iProbeCount = 0;

//-----------------------------------------------------------------------------
// [SECTION] bind group 2
//-----------------------------------------------------------------------------

layout(input_attachment_index = 1, set = 2, binding = 0)  uniform subpassInput tAlbedoSampler;
layout(input_attachment_index = 2, set = 2, binding = 1)  uniform subpassInput tNormalTexture;
layout(input_attachment_index = 3, set = 2, binding = 2)  uniform subpassInput tAOMetalRoughnessTexture;
layout(input_attachment_index = 0, set = 2, binding = 3)  uniform subpassInput tDepthSampler;

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

// layout(location = 0) in vec2 tUV;

const int iMaterialFlags = 0;
#include "lighting.glsl"
#include "material_info.glsl"

void main() 
{
    vec4 AORoughnessMetalnessData = subpassLoad(tAOMetalRoughnessTexture);
    vec4 tBaseColor = subpassLoad(tAlbedoSampler);
    vec3 color = vec3(0);
    
    float depth = subpassLoad(tDepthSampler).r;
    vec3 ndcSpace = vec3(gl_FragCoord.x / tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tViewportSize.x, gl_FragCoord.y / tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tViewportSize.y, depth);

    vec3 clipSpace = ndcSpace;
    clipSpace.xy = clipSpace.xy * 2.0 - 1.0;
    vec4 homoLocation = inverse(tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tCameraProjection) * vec4(clipSpace, 1.0);

    vec4 tViewPosition = homoLocation;
    tViewPosition.xyz = tViewPosition.xyz / tViewPosition.w;
    tViewPosition.x = tViewPosition.x;
    tViewPosition.y = tViewPosition.y;
    tViewPosition.z = tViewPosition.z;
    tViewPosition.w = 1.0;
    vec4 tWorldPosition = inverse(tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tCameraView) * tViewPosition;
    vec3 n = Decode(subpassLoad(tNormalTexture).xy);

    MaterialInfo materialInfo;
    materialInfo.baseColor = tBaseColor.rgb;

    // The default index of refraction of 1.5 yields a dielectric normal incidence reflectance of 0.04.
    materialInfo.f0_dielectric = vec3(0.04);
    materialInfo.specularWeight = 1.0;

    // Anything less than 2% is physically impossible and is instead considered to be shadowing. Compare to "Real-Time-Rendering" 4th editon on page 325.
    materialInfo.f90 = vec3(1.0);
    materialInfo.f90_dielectric = materialInfo.f90;

    materialInfo.perceptualRoughness = AORoughnessMetalnessData.b;
    materialInfo.metallic = AORoughnessMetalnessData.g;

    // Roughness is authored as perceptual roughness; as is convention,
    // convert to material roughness by squaring the perceptual roughness.
    materialInfo.alphaRoughness = materialInfo.perceptualRoughness * materialInfo.perceptualRoughness;
    
    // LIGHTING
    vec3 f_specular_dielectric = vec3(0.0);
    vec3 f_specular_metal = vec3(0.0);
    vec3 f_diffuse = vec3(0.0);
    vec3 f_dielectric_brdf_ibl = vec3(0.0);
    vec3 f_metal_brdf_ibl = vec3(0.0);
    vec3 f_emissive = vec3(0.0);
   
    vec3 v = normalize(tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tCameraPos.xyz - tWorldPosition.xyz);

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

            f_diffuse = getDiffuseLight(n, iProbeIndex) * tBaseColor.rgb;

            int iMips = textureQueryLevels(samplerCube(atCubeTextures[nonuniformEXT(tProbeData.atData[iProbeIndex].uGGXEnvSampler)], tSamplerNearestRepeat));
            f_specular_metal = getIBLRadianceGGX(n, v, materialInfo.perceptualRoughness, iMips, tWorldPosition.xyz, iProbeIndex);
            f_specular_dielectric = f_specular_metal;

            // Calculate fresnel mix for IBL  

            vec3 f_metal_fresnel_ibl = getIBLGGXFresnel(n, v, materialInfo.perceptualRoughness, tBaseColor.rgb, 1.0, iProbeIndex);
            f_metal_brdf_ibl = f_metal_fresnel_ibl * f_specular_metal;
        
            vec3 f_dielectric_fresnel_ibl = getIBLGGXFresnel(n, v, materialInfo.perceptualRoughness, materialInfo.f0_dielectric, materialInfo.specularWeight, iProbeIndex);
            f_dielectric_brdf_ibl = mix(f_diffuse, f_specular_dielectric,  f_dielectric_fresnel_ibl);

            color = mix(f_dielectric_brdf_ibl, f_metal_brdf_ibl, materialInfo.metallic);
        }
    }

    const float ao = AORoughnessMetalnessData.r;
    if(ao != 1.0)
    {
        float u_OcclusionStrength = 1.0;
        color = color * (1.0 + u_OcclusionStrength * (ao - 1.0)); 
    }

    f_diffuse = vec3(0.0);
    f_specular_dielectric = vec3(0.0);
    f_specular_metal = vec3(0.0);
    vec3 f_dielectric_brdf = vec3(0.0);
    vec3 f_metal_brdf = vec3(0.0);

    // punctual stuff
    uint cascadeIndex = 4;
    const bool bShadows = bool(iRenderingFlags & PL_RENDERING_FLAG_SHADOWS);
    if(bool(iRenderingFlags & PL_RENDERING_FLAG_USE_PUNCTUAL) && tObjectInfo.tData.iLightIndex != -1)
    {
        plGpuLight tLightData = tLightInfo.atData[tObjectInfo.tData.iLightIndex];
        float shadow = 1.0;

        vec3 pointToLight;
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
                
                
                for(int j = 0; j < tLightData.iCascadeCount; j++)
                {
                    

                    vec4 rawshadowCoord = biasMat * tShadowData.viewProjMat[j] * vec4(tWorldPosition.xyz, 1.0);

                    if(abs(rawshadowCoord.x - pl_saturate(rawshadowCoord.x)) < 0.00001 && abs(rawshadowCoord.y - pl_saturate(rawshadowCoord.y)) < 0.00001)
                    {
                        vec4 shadowCoord = (abiasMat * tShadowData.viewProjMat[j]) * vec4(tWorldPosition.xyz, 1.0);
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

                            shadowCoord = (abiasMat * tShadowData.viewProjMat[j + 1]) * vec4(tWorldPosition.xyz, 1.0);
                            float shadowfallback = filterPCF(shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset) + vec2((j + 1.0) * tShadowData.fFactor, 0), tShadowData.iShadowMapTexIdx);

                            shadow = mix(shadow, shadowfallback, cascade_fade);
                            // shadow = 100.0;
                        }

                        break;
                    }
                }  

            }
        }
        if(tLightData.iType == PL_LIGHT_TYPE_POINT)
        {
            pointToLight = tLightData.tPosition - tWorldPosition.xyz;

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
            pointToLight = tLightData.tPosition - tWorldPosition.xyz;
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

            l_specular_metal = shadow * intensity * NdotL * pl_brdf_specular(materialInfo.alphaRoughness, NdotL, NdotV, NdotH);
            l_specular_dielectric = l_specular_metal;

            l_metal_brdf = metal_fresnel * l_specular_metal;
            l_dielectric_brdf = mix(l_diffuse, l_specular_dielectric, dielectric_fresnel); // Do we need to handle vec3 fresnel here?
    
            vec3 l_color = mix(l_dielectric_brdf, l_metal_brdf, materialInfo.metallic);
            color += l_color;
        }    
    }

    // Layer blending

    outColor.a = tBaseColor.a;

    if(tShaderDebugMode == PL_SHADER_DEBUG_MODE_NONE)
    {
        outColor.rgb = color.rgb;
    }
    else
    {

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

        if(tShaderDebugMode == PL_SHADER_DEBUG_METALLIC)
        {
            outColor.rgb = vec3(materialInfo.metallic);
        }

        if(tShaderDebugMode == PL_SHADER_DEBUG_ROUGHNESS)
        {
            outColor.rgb = vec3(materialInfo.perceptualRoughness);
        }

        if(tShaderDebugMode == PL_SHADER_DEBUG_ALPHA)
        {
            outColor.rgb = vec3(tBaseColor.a);
        }

        if(tShaderDebugMode == PL_SHADER_DEBUG_OCCLUSION)
        {
            outColor.rgb = vec3(ao);
        }

        if(tShaderDebugMode > PL_SHADER_DEBUG_SHADING_NORMAL)
        {
            outColor.rgb = vec3(tBaseColor.rgb);
        }
    }

    // if(bool(iRenderingFlags & PL_RENDERING_FLAG_USE_PUNCTUAL) && tObjectInfo.tData.iLightIndex != -1)
    // {
    //     plGpuLight tLightData = tLightInfo.atData[tObjectInfo.tData.iLightIndex];

    //     if(tLightData.iType == PL_LIGHT_TYPE_DIRECTIONAL)
    //     {
    //         if(gl_FragCoord.x < 1400.0)
    //         {
    //             switch(cascadeIndex) {
    //                 case 0 : 
    //                     outColor.rgb *= vec3(1.0f, 0.25f, 0.25f);
    //                     break;
    //                 case 1 : 
    //                     outColor.rgb *= vec3(0.25f, 1.0f, 0.25f);
    //                     break;
    //                 case 2 : 
    //                     outColor.rgb *= vec3(0.25f, 0.25f, 1.0f);
    //                     break;
    //                 case 3 : 
    //                     outColor.rgb *= vec3(1.0f, 1.0f, 0.25f);
    //                     break;
    //             }
    //         }
    //     }
    // }

}