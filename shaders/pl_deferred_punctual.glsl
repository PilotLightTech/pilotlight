#ifndef PL_DEFERRED_PUNCTUAL_GLSL
#define PL_DEFERRED_PUNCTUAL_GLSL

#include "pl_contact_shadows.glsl"

// Shared BRDF and shadow sampling for volume and clustered passes.

vec3
pl_deferred_point(int lightIndex, vec3 worldPosition, vec3 n, vec3 v, MaterialInfo materialInfo, bool bShadows)
{
    vec4 tWorldPosition = vec4(worldPosition, 1.0);
    vec3 color = vec3(0.0);
    {
        plGpuPointLight tLightData = tPointLightInfo.atData[lightIndex];
        float shadow = 1.0;

        vec3 pointToLight = tLightData.tPosition - tWorldPosition.xyz;

        if(bShadows && tLightData.iCastShadow > 0)
        {
            vec3 result = sampleCube(-normalize(pointToLight));
            plGpuPointLightShadow tShadowData = tPointShadowData.atData[tLightData.iShadowIndex];
            vec4 shadowCoord = tShadowData.viewProjMat[int(result.z)] * vec4(tWorldPosition.xyz, 1.0);
            plShadowReceiver shadowReceiver = pl_shadow_receiver(tShadowData.viewProjMat[int(result.z)], shadowCoord, 0.5 * tShadowData.fFactor, tShadowData.fFactor);
            if(shadowCoord.w > 0.0 && shadowCoord.z >= 0.0 && shadowCoord.z <= shadowCoord.w)
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
                // Use the same face projection that rasterized the atlas.
                    shadowCoord.xy = (shadowCoord.xy * 0.5 + 0.5) * tShadowData.fFactor;
                if(bool(tGpuScene.tData.iSceneFlags & PL_SCENE_FLAG_PCF_SHADOWS))
                {
                    shadow = filterPCFSimple(shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset) + faceoffsets[int(result.z)] * tShadowData.fFactor, tShadowData.iShadowMapTexIdx, shadowReceiver);
                }
                else
                {
                    shadow = textureProj(shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset) + faceoffsets[int(result.z)] * tShadowData.fFactor, tShadowData.iShadowMapTexIdx, shadowReceiver);
                }
            }
        }


        if(bShadows && tLightData.iContactShadow != 0 && tLightData.iCastShadow > 0 && shadow > 0.0)
        {
            vec3 viewPosition = (tViewInfo.tData.tCameraView[tObjectInfo.tData.uGlobalIndex] * tWorldPosition).xyz;
            shadow *= pl_contact_shadow(viewPosition, n, pointToLight, length(pointToLight));
        }

        // BSTF
        vec3 l = normalize(pointToLight);   // Direction from surface point to light
        vec3 h = normalize(l + v);          // Direction of the vector between l and v, called halfway vector
        float NdotL = clampedDot(n, l);
        float NdotV = clampedDot(n, v);

        vec3 dielectric_fresnel = pl_fresnel_schlick(materialInfo.f0_dielectric * materialInfo.specularWeight, materialInfo.f90_dielectric, abs(clampedDot(v, h)));
        vec3 metal_fresnel = pl_fresnel_schlick(materialInfo.baseColor, vec3(1.0), abs(clampedDot(v, h)));


        if (NdotL > 0.0 && NdotV > 0.0)
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

    return color;
}

vec3
pl_deferred_spot(int lightIndex, vec3 worldPosition, vec3 n, vec3 v, MaterialInfo materialInfo, bool bShadows)
{
    vec4 tWorldPosition = vec4(worldPosition, 1.0);
    vec3 color = vec3(0.0);
    {
        plGpuSpotLight tLightData = tSpotLightInfo.atData[lightIndex];
        float shadow = 1.0;

        vec3 pointToLight = tLightData.tPosition - tWorldPosition.xyz;

        if(bShadows && tLightData.iCastShadow > 0)
        {
            plGpuSpotLightShadow tShadowData = tSpotShadowData.atData[tLightData.iShadowIndex];
            vec4 shadowCoord = tShadowData.viewProjMat * vec4(tWorldPosition.xyz, 1.0);
            plShadowReceiver shadowReceiver = pl_shadow_receiver(tShadowData.viewProjMat, shadowCoord, 0.5 * tShadowData.fFactor, tShadowData.fFactor);
            if(shadowCoord.w > 0.0 && shadowCoord.z >= 0.0 && shadowCoord.z <= shadowCoord.w)
            {
                shadowCoord.xyz /= shadowCoord.w;
                shadow = 0.0;
                shadowCoord.x = shadowCoord.x/2 + 0.5;
                shadowCoord.y = shadowCoord.y/2 + 0.5;
                shadowCoord.xy *= tShadowData.fFactor;

                if(bool(tGpuScene.tData.iSceneFlags & PL_SCENE_FLAG_PCF_SHADOWS))
                {
                    shadow = filterPCFSimple(shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset), tShadowData.iShadowMapTexIdx, shadowReceiver);
                }
                else
                {
                    shadow = textureProj(shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset), tShadowData.iShadowMapTexIdx, shadowReceiver);
                }
            }
        }

        if(bShadows && tLightData.iContactShadow != 0 && tLightData.iCastShadow > 0 && shadow > 0.0)
        {
            vec3 viewPosition = (tViewInfo.tData.tCameraView[tObjectInfo.tData.uGlobalIndex] * tWorldPosition).xyz;
            shadow *= pl_contact_shadow(viewPosition, n, pointToLight, length(pointToLight));
        }

        // BSTF
        vec3 l = normalize(pointToLight);   // Direction from surface point to light
        vec3 h = normalize(l + v);          // Direction of the vector between l and v, called halfway vector
        float NdotL = clampedDot(n, l);
        float NdotV = clampedDot(n, v);

        vec3 dielectric_fresnel = pl_fresnel_schlick(materialInfo.f0_dielectric * materialInfo.specularWeight, materialInfo.f90_dielectric, abs(clampedDot(v, h)));
        vec3 metal_fresnel = pl_fresnel_schlick(materialInfo.baseColor, vec3(1.0), abs(clampedDot(v, h)));


        if (NdotL > 0.0 && NdotV > 0.0)
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

    return color;
}

#endif
