#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "global.inc"

//-----------------------------------------------------------------------------
// [SECTION] specialication constants
//-----------------------------------------------------------------------------

layout(constant_id = 0) const int iMeshVariantFlags = 0;
layout(constant_id = 1) const int iDataStride = 0;
layout(constant_id = 2) const int iTextureMappingFlags = 0;
layout(constant_id = 3) const int iMaterialFlags = 0;
layout(constant_id = 4) const int iRenderingFlags = 0;
layout(constant_id = 5) const int iLightCount = 0;
layout(constant_id = 6) const int iProbeCount = 0;

//-----------------------------------------------------------------------------
// [SECTION] bind group 1
//-----------------------------------------------------------------------------

layout(set = 1, binding = 0) readonly buffer _plGlobalInfo
{
    plGpuGlobalData data[];
} tGlobalInfo;

layout(set = 1, binding = 1) uniform _plLightInfo
{
    plGpuLight atData[1];
} tLightInfo;

layout(set = 1, binding = 2) readonly buffer plDShadowData
{
    plGpuLightShadow atData[];
} tDShadowData;

layout(set = 1, binding = 3) readonly buffer plShadowData
{
    plGpuLightShadow atData[];
} tShadowData;

layout(set = 1, binding = 4) readonly buffer plProbeData
{
    plGpuProbe atData[];
} tProbeData;

layout(set = 1, binding = 5)  uniform sampler tShadowSampler;

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

#define PL_FRAGMENT
#include "math.glsl"
#include "lighting.glsl"

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void main() 
{

    plGpuMaterial material = tMaterialInfo.atMaterials[tObjectInfo.tData.iMaterialIndex];
    vec4 tBaseColor = getBaseColor(material.tBaseColorFactor, material.iBaseColorUVSet);
    vec3 color = vec3(0);

    if(tBaseColor.a <  material.fAlphaCutoff)
    {
        discard;
    }

    NormalInfo tNormalInfo = pl_get_normal_info(material.iNormalUVSet);

    vec3 n = tNormalInfo.n;
    vec3 t = tNormalInfo.t;
    vec3 b = tNormalInfo.b;

    MaterialInfo materialInfo;
    materialInfo.baseColor = tBaseColor.rgb;

    // The default index of refraction of 1.5 yields a dielectric normal incidence reflectance of 0.04.
    materialInfo.f0_dielectric = vec3(0.04);
    materialInfo.specularWeight = 1.0;

    if(bool(iMaterialFlags & PL_INFO_MATERIAL_METALLICROUGHNESS))
    {
        materialInfo = getMetallicRoughnessInfo(materialInfo, material.fMetallicFactor, material.fRoughnessFactor, material.iMetallicRoughnessUVSet);
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
    
    vec3 v = normalize(tGlobalInfo.data[tObjectInfo.tData.uGlobalIndex].tCameraPos.xyz - tShaderIn.tWorldPosition.xyz);

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

            int iMips = textureQueryLevels(samplerCube(atCubeTextures[nonuniformEXT(tProbeData.atData[iProbeIndex].uGGXEnvSampler)], tEnvSampler));
            f_specular_metal = getIBLRadianceGGX(n, v, materialInfo.perceptualRoughness, iMips, tShaderIn.tWorldPosition.xyz, iProbeIndex);
            f_specular_dielectric = f_specular_metal;

            // Calculate fresnel mix for IBL  

            vec3 f_metal_fresnel_ibl = getIBLGGXFresnel(n, v, materialInfo.perceptualRoughness, tBaseColor.rgb, 1.0, iProbeIndex);
            f_metal_brdf_ibl = f_metal_fresnel_ibl * f_specular_metal;
        
            vec3 f_dielectric_fresnel_ibl = getIBLGGXFresnel(n, v, materialInfo.perceptualRoughness, materialInfo.f0_dielectric, materialInfo.specularWeight, iProbeIndex);
            f_dielectric_brdf_ibl = mix(f_diffuse, f_specular_dielectric,  f_dielectric_fresnel_ibl);

            color = mix(f_dielectric_brdf_ibl, f_metal_brdf_ibl, materialInfo.metallic);
        }
    }

    // ambient occlusion
    
    if(bool(iTextureMappingFlags & PL_HAS_OCCLUSION_MAP))
    {
        float u_OcclusionStrength = 1.0;
        float ao = 1.0;
        ao = texture(sampler2D(at2DTextures[nonuniformEXT(material.iOcclusionTexIdx)], tDefaultSampler), tShaderIn.tUV[material.iOcclusionUVSet]).r;
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

                    // Get cascade index for the current fragment's view position
                    
                    vec4 inViewPos = tGlobalInfo.data[tObjectInfo.tData.uGlobalIndex].tCameraView * vec4(tShaderIn.tWorldPosition.xyz, 1.0);
                    for(uint j = 0; j < tLightData.iCascadeCount - 1; ++j)
                    {
                        if(inViewPos.z > tShadowData.cascadeSplits[j])
                        {	
                            cascadeIndex = j + 1;
                        }
                    }  
                    
                    // Depth compare for shadowing
                    mat4 abiasMat = biasMat;
                    abiasMat[0][0] *= tShadowData.fFactor;
                    abiasMat[1][1] *= tShadowData.fFactor;
                    abiasMat[3][0] *= tShadowData.fFactor;
                    abiasMat[3][1] *= tShadowData.fFactor;
                    vec4 shadowCoord = (abiasMat * tShadowData.viewProjMat[cascadeIndex]) * vec4(tShaderIn.tWorldPosition.xyz, 1.0);	
                    shadow = 0;
                    // shadow = textureProj(shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset) + vec2(cascadeIndex * tShadowData.fFactor, 0), tShadowData.iShadowMapTexIdx);
                    shadow = filterPCF(shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset) + vec2(cascadeIndex * tShadowData.fFactor, 0), tShadowData.iShadowMapTexIdx);

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

                    vec3 l_diffuse = shadow * intensity * NdotL * pl_brdf_lambertian(tBaseColor.rgb);
                    vec3 l_specular_dielectric = vec3(0.0);
                    vec3 l_specular_metal = vec3(0.0);
                    vec3 l_dielectric_brdf = vec3(0.0);
                    vec3 l_metal_brdf = vec3(0.0);

                    l_specular_metal = shadow * intensity * NdotL * BRDF_specularGGX(materialInfo.alphaRoughness, NdotL, NdotV, NdotH);
                    l_specular_dielectric = l_specular_metal;

                    l_metal_brdf = metal_fresnel * l_specular_metal;
                    l_dielectric_brdf = mix(l_diffuse, l_specular_dielectric, dielectric_fresnel); // Do we need to handle vec3 fresnel here?
            
                    vec3 l_color = mix(l_dielectric_brdf, l_metal_brdf, materialInfo.metallic);
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
                        vec2 sampleLocation = vec2(tShadowData.fXOffset, tShadowData.fYOffset) + (result.xy * tShadowData.fFactor) + (faceoffsets[int(result.z)] * tShadowData.fFactor);
                        float dist = texture(sampler2D(at2DTextures[nonuniformEXT(tShadowData.iShadowMapTexIdx)], tShadowSampler), sampleLocation).r;
                        float fDist = shadowCoord.z;
                        dist = dist * shadowCoord.w;
                        // dist = 1 - dist * shadowCoord.w;

                        if(shadowCoord.w > 0 && dist > fDist)
                        {
                            shadow = 0.0;
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

                    vec3 l_diffuse = shadow * intensity * NdotL * pl_brdf_lambertian(tBaseColor.rgb);
                    vec3 l_specular_dielectric = vec3(0.0);
                    vec3 l_specular_metal = vec3(0.0);
                    vec3 l_dielectric_brdf = vec3(0.0);
                    vec3 l_metal_brdf = vec3(0.0);

                    l_specular_metal = shadow * intensity * NdotL * BRDF_specularGGX(materialInfo.alphaRoughness, NdotL, NdotV, NdotH);
                    l_specular_dielectric = l_specular_metal;

                    l_metal_brdf = metal_fresnel * l_specular_metal;
                    l_dielectric_brdf = mix(l_diffuse, l_specular_dielectric, dielectric_fresnel); // Do we need to handle vec3 fresnel here?
            
                    vec3 l_color = mix(l_dielectric_brdf, l_metal_brdf, materialInfo.metallic);
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
                        // shadow = textureProj2(shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset), tShadowData.iShadowMapTexIdx);
                        shadow = filterPCF2(shadowCoord, vec2(tShadowData.fXOffset, tShadowData.fYOffset), tShadowData.iShadowMapTexIdx);

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

                    vec3 l_diffuse = shadow * intensity * NdotL * pl_brdf_lambertian(tBaseColor.rgb);
                    vec3 l_specular_dielectric = vec3(0.0);
                    vec3 l_specular_metal = vec3(0.0);
                    vec3 l_dielectric_brdf = vec3(0.0);
                    vec3 l_metal_brdf = vec3(0.0);

                    l_specular_metal = shadow * intensity * NdotL * BRDF_specularGGX(materialInfo.alphaRoughness, NdotL, NdotV, NdotH);
                    l_specular_dielectric = l_specular_metal;

                    l_metal_brdf = metal_fresnel * l_specular_metal;
                    l_dielectric_brdf = mix(l_diffuse, l_specular_dielectric, dielectric_fresnel); // Do we need to handle vec3 fresnel here?
            
                    vec3 l_color = mix(l_dielectric_brdf, l_metal_brdf, materialInfo.metallic);
                    color += l_color;
                }
            }


        }

    }

    // emissive
    f_emissive = material.tEmissiveFactor;
    if(bool(iTextureMappingFlags & PL_HAS_EMISSIVE_MAP))
    {
        f_emissive *= pl_srgb_to_linear(texture(sampler2D(at2DTextures[nonuniformEXT(material.iEmissiveTexIdx)], tDefaultSampler), tShaderIn.tUV[material.iEmissiveUVSet]).rgb);
    }

    color = f_emissive + color;
    outColor = vec4(color.rgb, tBaseColor.a);

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