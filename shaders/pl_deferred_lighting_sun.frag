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
    
    vec3 ndcSpace = vec3(gl_FragCoord.x / tViewInfo.tData.tViewportSize.x, gl_FragCoord.y / tViewInfo.tData.tViewportSize.y, depth);

    vec3 clipSpace = ndcSpace;
    clipSpace.xy = clipSpace.xy * 2.0 - 1.0;

    vec4 homoLocation = tViewInfo.tData.tCameraProjectionInv[tObjectInfo.tData.uGlobalIndex] * vec4(clipSpace, 1.0);
    vec4 tViewPosition = homoLocation; // homo location
    tViewPosition.xyz = tViewPosition.xyz / tViewPosition.w;
    tViewPosition.w = 1.0;
    vec4 tWorldPosition = tViewInfo.tData.tCameraViewInv[tObjectInfo.tData.uGlobalIndex] * tViewPosition;

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
    vec3 v = normalize(tViewInfo.tData.tCameraPos.xyz - tWorldPosition.xyz);

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
    uint cascadeIndex = tGpuScene.tData.iCascadeCount - 1;
    const bool bShadows = bool(iRenderingFlags & PL_RENDERING_FLAG_SHADOWS);
    {
        float shadow = 1.0;
        vec3 pointToLight = -tGpuScene.tData.tDirection;
        int iCascadeCount = tGpuScene.tData.iCascadeCount;

        if(bShadows)
        {

            // Depth compare for shadowing
            mat4 abiasMat = biasMat;
            abiasMat[0][0] *= tGpuScene.tData.fFactor;
            abiasMat[1][1] *= tGpuScene.tData.fFactor;
            abiasMat[3][0] *= tGpuScene.tData.fFactor;
            abiasMat[3][1] *= tGpuScene.tData.fFactor;
            shadow = 1.0;

            // Get cascade index for the current fragment's view position
            
            float viewDepth = (tViewPosition.z - tViewInfo.tData.fCameraNearZ) / tViewInfo.tData.fCameraRange;

            vec4 tWorldPos2 = vec4(tWorldPosition.xyz, 1.0);

            if(tObjectInfo.tData.iProbe == 0)
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
            
                if(bool(iRenderingFlags & PL_RENDERING_FLAG_PCF_SHADOWS))
                {
                    shadow = filterPCF(
                        shadowCoord,
                        vec2(tGpuScene.tData.fXOffset, tGpuScene.tData.fYOffset) + vec2(cascadeIndex * tGpuScene.tData.fFactor, 0),
                        tGpuScene.tData.iShadowMapTexIdx, cascadeIndex);
                }
                else
                {
                    shadow = textureProj2(shadowCoord, vec2(tGpuScene.tData.fXOffset, tGpuScene.tData.fYOffset) + vec2(cascadeIndex * tGpuScene.tData.fFactor, 0), tGpuScene.tData.iShadowMapTexIdx);
                }

                
                // if(abs(rawshadowCoord.x - pl_saturate(rawshadowCoord.x)) < 0.00001 && abs(rawshadowCoord.y - pl_saturate(rawshadowCoord.y)) < 0.00001 && abs(rawshadowCoord.z - pl_saturate(rawshadowCoord.z)) < 0.00001)
                if(cascadeIndex < (iCascadeCount - 1))
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
        float NdotV = clampedDot(n, v);

        vec3 dielectric_fresnel = pl_fresnel_schlick(materialInfo.f0_dielectric * materialInfo.specularWeight, materialInfo.f90_dielectric, abs(clampedDot(v, h)));
        vec3 metal_fresnel = pl_fresnel_schlick(materialInfo.baseColor, vec3(1.0), abs(clampedDot(v, h)));


        if (NdotL > 0.0)
        {

            vec3 intensity = tGpuScene.tData.fIntensity * tGpuScene.tData.tColor;
            // vec3 intensity = vec3(0);

            vec3 l_diffuse = shadow * intensity * NdotL * pl_brdf_diffuse(materialInfo.baseColor);
            vec3 l_specular_dielectric = vec3(0.0);
            vec3 l_specular_metal = vec3(0.0);
            vec3 l_dielectric_brdf = vec3(0.0);
            vec3 l_metal_brdf = vec3(0.0);

            if(NdotV > 0)
            {
                float NdotH = clampedDot(n, h);
                l_specular_metal = shadow * intensity * NdotL * pl_brdf_specular(materialInfo.alphaRoughness, NdotL, NdotV, NdotH);
                l_specular_dielectric = l_specular_metal;
            }

            l_metal_brdf = metal_fresnel * l_specular_metal;
            l_dielectric_brdf = mix(l_diffuse, l_specular_dielectric, dielectric_fresnel); // Do we need to handle vec3 fresnel here?
    
            vec3 l_color = mix(l_dielectric_brdf, l_metal_brdf, materialInfo.metallic);
            color += l_color;
        }
    }

    // Layer blending

    outColor.a = fBaseColorAlpha;
    outColor.rgb = color.rgb;

    if(tGpuScene.tData.bCascadeDebug == 1)
    {
        // if(gl_FragCoord.x < 1800.0)
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