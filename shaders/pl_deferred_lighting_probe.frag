#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "pl_shader_interop_renderer.h"
#include "pl_bg_scene.inc"
#include "pl_bg_view.inc"
#include "pl_math.glsl"
#include "pl_brdf.glsl"

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
    outColor = vec4(0);
    vec4 AORoughnessMetalnessData = subpassLoad(tAOMetalRoughnessTexture);
    float depth = subpassLoad(tDepthSampler).r;
    vec2 tEncodedN = subpassLoad(tNormalTexture).xy;
    vec4 tBaseColor = subpassLoad(tAlbedoSampler);
    
    vec3 ndcSpace = vec3(gl_FragCoord.x / tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tViewportSize.x, gl_FragCoord.y / tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tViewportSize.y, depth);

    vec3 clipSpace = ndcSpace;
    clipSpace.xy = clipSpace.xy * 2.0 - 1.0;

    vec4 tViewPosition = tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tCameraProjectionInv * vec4(clipSpace, 1.0); // homo location
    tViewPosition.xyz = tViewPosition.xyz / tViewPosition.w;
    tViewPosition.w = 1.0;
    vec4 tWorldPosition = tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tCameraViewInv * tViewPosition;
    

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
    vec3 v = normalize(tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tCameraPos.xyz - tWorldPosition.xyz);

    float fBaseColorAlpha = 0.0;
    {
        materialInfo.baseColor = tBaseColor.rgb;
        fBaseColorAlpha = tBaseColor.a;
    }

    vec3 n = Decode(tEncodedN);

    // Calculate lighting contribution from image based lighting source (IBL)
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

        for(int i = 0; i < tObjectInfo.tData.iProbeCount; i++)
        {
            vec3 tDist = tProbeData.atData[i].tPosition - tWorldPosition.xyz;
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
        float summing = computeProbeWeights(tWorldPosition.xyz, R, 2.0, K, aiActiveProbes, weights);

        int iClosestProbeIndex = aiActiveProbes[iClosestIndex];

        vec3 f_specular_metal = getIBLRadianceGGX(n, v, materialInfo.perceptualRoughness, tProbeData.atData[iClosestProbeIndex].iMips, tWorldPosition.xyz, iClosestProbeIndex);
        // vec3 f_specular_dielectric = f_specular_metal;

        for(int i = 0; i < K; i++)
        {
            int iProbeIndex = aiActiveProbes[i];
            vec3 f_diffuse = getDiffuseLight(n, iProbeIndex) * materialInfo.baseColor;

            // int iMips = textureQueryLevels(samplerCube(atCubeTextures[nonuniformEXT(tProbeData.atData[iProbeIndex].uGGXEnvSampler)], tSamplerNearestRepeat));

            // Calculate fresnel mix for IBL  

            vec3 f_metal_fresnel_ibl = getIBLGGXFresnel(n, v, materialInfo.perceptualRoughness, materialInfo.baseColor, 1.0, iProbeIndex);
            vec3 f_metal_brdf_ibl = f_metal_fresnel_ibl * f_specular_metal;
        
            vec3 f_dielectric_fresnel_ibl = getIBLGGXFresnel(n, v, materialInfo.perceptualRoughness, materialInfo.f0_dielectric, materialInfo.specularWeight, iProbeIndex);
            vec3 f_dielectric_brdf_ibl = mix(f_diffuse, f_specular_metal,  f_dielectric_fresnel_ibl);

            outColor.rgb += weights[i] * mix(f_dielectric_brdf_ibl, f_metal_brdf_ibl, materialInfo.metallic);
        }
    }

    const float ao = AORoughnessMetalnessData.r;
    if(ao != 1.0)
    {
        float u_OcclusionStrength = 1.0;
        outColor.rgb = outColor.rgb * (1.0 + u_OcclusionStrength * (ao - 1.0)); 
    }

    // Layer blending
    outColor.a = fBaseColorAlpha;

    if(bool(tGpuScene.tData.iSceneFlags & PL_SCENE_FLAG_HEIGHT_FOG))
    {
        outColor = fog(outColor, tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tCameraPos.xyz - tWorldPosition.xyz);
    }
    else if(bool(tGpuScene.tData.iSceneFlags & PL_SCENE_FLAG_LINEAR_FOG))
    {
        outColor = fogLinear(outColor, tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tCameraPos.xyz - tWorldPosition.xyz);
    }

}