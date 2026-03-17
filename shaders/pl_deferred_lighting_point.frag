#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "pl_shader_interop_renderer.h"
#include "pl_bg_scene.inc"
#include "pl_bg_view.inc"
#include "pl_math.glsl"
#include "pl_brdf.glsl"

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
#include "pl_lighting.glsl"
#include "pl_material_info.glsl"
#include "pl_fog.glsl"

void main() 
{
    vec4 AORoughnessMetalnessData = subpassLoad(tAOMetalRoughnessTexture);
    float depth = subpassLoad(tDepthSampler).r;
    vec2 tEncodedN = subpassLoad(tNormalTexture).xy;
    vec4 tBaseColor = subpassLoad(tAlbedoSampler);
    
    vec3 color = vec3(0);
    
    vec3 ndcSpace = vec3(gl_FragCoord.x / tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tViewportSize.x, gl_FragCoord.y / tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tViewportSize.y, depth);

    vec3 clipSpace = ndcSpace;
    clipSpace.xy = clipSpace.xy * 2.0 - 1.0;

    vec4 homoLocation = tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tCameraProjectionInv * vec4(clipSpace, 1.0);
    vec4 tViewPosition = homoLocation; // homo location
    tViewPosition.xyz = tViewPosition.xyz / tViewPosition.w;
    tViewPosition.w = 1.0;
    vec4 tWorldPosition = tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tCameraViewInv * tViewPosition;

    

    MaterialInfo materialInfo;

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
    vec3 v = normalize(tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tCameraPos.xyz - tWorldPosition.xyz);

    float fBaseColorAlpha = 0.0;
    {
        materialInfo.baseColor = tBaseColor.rgb;
        fBaseColorAlpha = tBaseColor.a;
    }

    vec3 n = Decode(tEncodedN);

    const float ao = AORoughnessMetalnessData.r;
    if(ao != 1.0)
    {
        float u_OcclusionStrength = 1.0;
        color = color * (1.0 + u_OcclusionStrength * (ao - 1.0)); 
    }

    vec3 f_dielectric_brdf = vec3(0.0);
    vec3 f_metal_brdf = vec3(0.0);

    // punctual stuff
    // uint cascadeIndex = 4;
    const bool bShadows = bool(iRenderingFlags & PL_RENDERING_FLAG_SHADOWS);
    if(bool(iRenderingFlags & PL_RENDERING_FLAG_USE_PUNCTUAL) && tObjectInfo.tData.iLightIndex != -1)
    {
        plGpuLight tLightData = tLightInfo.atData[tObjectInfo.tData.iLightIndex];
        float shadow = 1.0;

        vec3 pointToLight = tLightData.tPosition - tWorldPosition.xyz;

        if(bShadows && tLightData.iCastShadow > 0)
        {
            vec3 result = sampleCube(-normalize(pointToLight));
            vec4 shadowCoord = tShadowData.atData[tLightData.iShadowIndex].viewProjMat[int(result.z)] * vec4(tWorldPosition.xyz, 1.0);
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
                result.xy *= tShadowData.atData[tLightData.iShadowIndex].fFactor;
                shadowCoord.xy = result.xy;
                if(bool(iRenderingFlags & PL_RENDERING_FLAG_PCF_SHADOWS))
                {
                    shadow = filterPCF(shadowCoord, vec2(tShadowData.atData[tLightData.iShadowIndex].fXOffset, tShadowData.atData[tLightData.iShadowIndex].fYOffset) + faceoffsets[int(result.z)] * tShadowData.atData[tLightData.iShadowIndex].fFactor, tShadowData.atData[tLightData.iShadowIndex].iShadowMapTexIdx);
                }
                else
                {
                    shadow = textureProj(shadowCoord, vec2(tShadowData.atData[tLightData.iShadowIndex].fXOffset, tShadowData.atData[tLightData.iShadowIndex].fYOffset) + faceoffsets[int(result.z)] * tShadowData.atData[tLightData.iShadowIndex].fFactor, tShadowData.atData[tLightData.iShadowIndex].iShadowMapTexIdx);
                }
            }
        }


        // BSTF
        vec3 l = normalize(pointToLight);   // Direction from surface point to light
        vec3 h = normalize(l + v);          // Direction of the vector between l and v, called halfway vector
        float NdotL = clampedDot(n, l);
        float NdotV = clampedDot(n, v);

        vec3 dielectric_fresnel = pl_fresnel_schlick(materialInfo.f0_dielectric * materialInfo.specularWeight, materialInfo.f90_dielectric, abs(clampedDot(v, h)));
        vec3 metal_fresnel = pl_fresnel_schlick(materialInfo.baseColor, vec3(1.0), abs(clampedDot(v, h)));


        if (NdotL > 0.0 || NdotV > 0.0)
        {

            vec3 intensity = getLightIntensity(tLightData, pointToLight);

            vec3 l_diffuse = shadow * intensity * NdotL * pl_brdf_diffuse(materialInfo.baseColor);
            vec3 l_specular_dielectric = vec3(0.0);
            vec3 l_specular_metal = vec3(0.0);
            vec3 l_dielectric_brdf = vec3(0.0);
            vec3 l_metal_brdf = vec3(0.0);

            float NdotH = clampedDot(n, h);
            l_specular_metal = shadow * intensity * NdotL * pl_brdf_specular(materialInfo.alphaRoughness, NdotL, NdotV, NdotH);
            l_specular_dielectric = l_specular_metal;

            l_metal_brdf = metal_fresnel * l_specular_metal;
            l_dielectric_brdf = mix(l_diffuse, l_specular_dielectric, dielectric_fresnel); // Do we need to handle vec3 fresnel here?
    
            vec3 l_color = mix(l_dielectric_brdf, l_metal_brdf, materialInfo.metallic);
            color += l_color;
        }    
    }

    // Layer blending

    outColor.a = fBaseColorAlpha;

    if(tShaderDebugMode == PL_SHADER_DEBUG_MODE_NONE)
    {
        
        outColor.rgb = color.rgb;

        if(iProbeCount > 0)
        {
            if(bool(tGpuScene.tData.iSceneFlags & PL_SCENE_FLAG_HEIGHT_FOG))
            {
                outColor = fog(outColor, tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tCameraPos.xyz - tWorldPosition.xyz);
            }
            else if(bool(tGpuScene.tData.iSceneFlags & PL_SCENE_FLAG_LINEAR_FOG))
            {
                outColor = fogLinear(outColor, tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tCameraPos.xyz - tWorldPosition.xyz);
            }
        }
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
            outColor.rgb = materialInfo.baseColor;
            outColor.a = fBaseColorAlpha;
        }

        if(tShaderDebugMode == PL_SHADER_DEBUG_SHADING_NORMAL)
        {
            outColor = vec4((n + 1.0) / 2.0, fBaseColorAlpha);
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
            outColor.rgb = vec3(fBaseColorAlpha);
        }

        if(tShaderDebugMode == PL_SHADER_DEBUG_OCCLUSION)
        {
            outColor.rgb = vec3(ao);
        }

        if(tShaderDebugMode > PL_SHADER_DEBUG_SHADING_NORMAL)
        {
            outColor.rgb = vec3(n);
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