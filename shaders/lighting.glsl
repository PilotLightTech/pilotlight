#ifndef LIGHTING_GLSL
#define LIGHTING_GLSL

vec3
getDiffuseLight(vec3 n, int iProbeIndex)
{
    // n.z = -n.z; // uncomment if not reverse z
    return texture(samplerCube(atCubeTextures[nonuniformEXT(tProbeData.atData[iProbeIndex].uLambertianEnvSampler)], tSamplerLinearClamp), n).rgb;
}

vec4
getSpecularSample(vec3 reflection, float lod, int iProbeIndex)
{
    // reflection.z = -reflection.z; // uncomment if not reverse z
    // reflection.x = -reflection.x; // uncomment if not reverse z
    // lod = max(lod, 4);
    return textureLod(samplerCube(atCubeTextures[nonuniformEXT(tProbeData.atData[iProbeIndex].uGGXEnvSampler)], tSamplerLinearClamp), reflection, lod);
}

vec4
getSheenSample(vec3 reflection, float lod, int iProbeIndex)
{
    // vec4 textureSample =  textureLod(u_CharlieEnvSampler, u_EnvRotation * reflection, lod);
    // textureSample.rgb *= u_EnvIntensity;
    // return textureSample;
    // lod = max(lod, 4);
    return textureLod(samplerCube(atCubeTextures[nonuniformEXT(tProbeData.atData[iProbeIndex].uCharlieEnvSampler)], tSamplerLinearClamp), reflection, lod);
}

vec3
getIBLGGXFresnel(vec3 n, vec3 v, float roughness, vec3 F0, float specularWeight, int iProbeIndex)
{
    // see https://bruop.github.io/ibl/#single_scattering_results at Single Scattering Results
    // Roughness dependent fresnel, from Fdez-Aguera
    float NdotV = clampedDot(n, v);
    vec2 brdfSamplePoint = clamp(vec2(NdotV, roughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
    vec2 f_ab = texture(sampler2D(at2DTextures[nonuniformEXT(tGpuScene.tData.iBrdfLutIndex)], tSamplerLinearClamp), brdfSamplePoint).rg;
    vec3 Fr = max(vec3(1.0 - roughness), F0) - F0;
    vec3 k_S = F0 + Fr * pow(1.0 - NdotV, 5.0);
    vec3 FssEss = specularWeight * (k_S * f_ab.x + f_ab.y);

    // Multiple scattering, from Fdez-Aguera
    float Ems = (1.0 - (f_ab.x + f_ab.y));
    vec3 F_avg = specularWeight * (F0 + (1.0 - F0) / 21.0);
    vec3 FmsEms = Ems * FssEss * F_avg / (1.0 - F_avg * Ems);

    return FssEss + FmsEms;
}

vec3
getIBLRadianceGGX(vec3 n, vec3 v, float roughness, int u_MipCount, vec3 tWorldPos, int iProbeIndex)
{
    float NdotV = clampedDot(n, v);
    float lod = roughness * float(u_MipCount - 1);
    vec3 reflection = normalize(reflect(-v, n));

    if(bool(tProbeData.atData[iProbeIndex].iParallaxCorrection))
    {

        // Find the ray intersection with box plane
        vec3 FirstPlaneIntersect = (tProbeData.atData[iProbeIndex].tMax.xyz - tWorldPos) / reflection;
        vec3 SecondPlaneIntersect = (tProbeData.atData[iProbeIndex].tMin.xyz - tWorldPos) / reflection;
        
        // Get the furthest of these intersections along the ray
        // (Ok because x/0 give +inf and -x/0 give –inf )
        vec3 FurthestPlane = max(FirstPlaneIntersect, SecondPlaneIntersect);

        // Find the closest far intersection
        float Distance = min(min(FurthestPlane.x, FurthestPlane.y), FurthestPlane.z);

        // Get the intersection position
        vec3 IntersectPositionWS = tWorldPos + reflection * Distance;
        
        // Get corrected reflection
        reflection = IntersectPositionWS - tProbeData.atData[iProbeIndex].tPosition;
    }

    vec4 specularSample = getSpecularSample(reflection, lod, iProbeIndex);

    vec3 specularLight = specularSample.rgb;

    return specularLight;
}

vec3
getIBLRadianceAnisotropy(vec3 n, vec3 v, float roughness, float anisotropy, vec3 anisotropyDirection, int u_MipCount, vec3 tWorldPos, int iProbeIndex)
{
    float NdotV = clampedDot(n, v);

    float tangentRoughness = mix(roughness, 1.0, anisotropy * anisotropy);
    vec3  anisotropicTangent  = cross(anisotropyDirection, v);
    vec3  anisotropicNormal   = cross(anisotropicTangent, anisotropyDirection);
    float bendFactor          = 1.0 - anisotropy * (1.0 - roughness);
    float bendFactorPow4      = bendFactor * bendFactor * bendFactor * bendFactor;
    vec3  bentNormal          = normalize(mix(anisotropicNormal, n, bendFactorPow4));

    float lod = roughness * float(u_MipCount - 1);
    vec3 reflection = normalize(reflect(-v, bentNormal));

    if(bool(tProbeData.atData[iProbeIndex].iParallaxCorrection))
    {

        // Find the ray intersection with box plane
        vec3 FirstPlaneIntersect = (tProbeData.atData[iProbeIndex].tMax.xyz - tWorldPos) / reflection;
        vec3 SecondPlaneIntersect = (tProbeData.atData[iProbeIndex].tMin.xyz - tWorldPos) / reflection;
        
        // Get the furthest of these intersections along the ray
        // (Ok because x/0 give +inf and -x/0 give –inf )
        vec3 FurthestPlane = max(FirstPlaneIntersect, SecondPlaneIntersect);

        // Find the closest far intersection
        float Distance = min(min(FurthestPlane.x, FurthestPlane.y), FurthestPlane.z);

        // Get the intersection position
        vec3 IntersectPositionWS = tWorldPos + reflection * Distance;
        
        // Get corrected reflection
        reflection = IntersectPositionWS - tProbeData.atData[iProbeIndex].tPosition;
    }

    vec4 specularSample = getSpecularSample(reflection, lod, iProbeIndex);

    vec3 specularLight = specularSample.rgb;

    return specularLight;
}

// GGX Distribution Anisotropic (Same as Babylon.js)
// https://blog.selfshadow.com/publications/s2012-shading-course/burley/s2012_pbs_disney_brdf_notes_v3.pdf Addenda
float D_GGX_anisotropic(float NdotH, float TdotH, float BdotH, float anisotropy, float at, float ab)
{
    float a2 = at * ab;
    vec3 f = vec3(ab * TdotH, at * BdotH, a2 * NdotH);
    float w2 = a2 / dot(f, f);
    return a2 * w2 * w2 / M_PI;
}

// GGX Mask/Shadowing Anisotropic (Same as Babylon.js - smithVisibility_GGXCorrelated_Anisotropic)
// Heitz http://jcgt.org/published/0003/02/03/paper.pdf
float V_GGX_anisotropic(float NdotL, float NdotV, float BdotV, float TdotV, float TdotL, float BdotL, float at, float ab)
{
    float GGXV = NdotL * length(vec3(at * TdotV, ab * BdotV, NdotV));
    float GGXL = NdotV * length(vec3(at * TdotL, ab * BdotL, NdotL));
    float v = 0.5 / (GGXV + GGXL);
    return clamp(v, 0.0, 1.0);
}

vec3 BRDF_specularGGXAnisotropy(float alphaRoughness, float anisotropy, vec3 n, vec3 v, vec3 l, vec3 h, vec3 t, vec3 b)
{
    // Roughness along the anisotropy bitangent is the material roughness, while the tangent roughness increases with anisotropy.
    float at = mix(alphaRoughness, 1.0, anisotropy * anisotropy);
    float ab = clamp(alphaRoughness, 0.001, 1.0);

    float NdotL = clamp(dot(n, l), 0.0, 1.0);
    float NdotH = clamp(dot(n, h), 0.001, 1.0);
    float NdotV = dot(n, v);

    float V = V_GGX_anisotropic(NdotL, NdotV, dot(b, v), dot(t, v), dot(t, l), dot(b, l), at, ab);
    float D = D_GGX_anisotropic(NdotH, dot(t, h), dot(b, h), anisotropy, at, ab);

    return vec3(V * D);
}

vec3
getIBLRadianceCharlie(vec3 n, vec3 v, float sheenRoughness, vec3 sheenColor, int u_MipCount, int iProbeIndex)
{
    float NdotV = clampedDot(n, v);
    float lod = sheenRoughness * float(u_MipCount - 1);
    vec3 reflection = normalize(reflect(-v, n));

    vec2 brdfSamplePoint = clamp(vec2(NdotV, sheenRoughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
    float brdf = texture(sampler2D(at2DTextures[nonuniformEXT(tGpuScene.tData.iBrdfLutIndex)], tSamplerLinearClamp), brdfSamplePoint).b;
    vec4 sheenSample = getSheenSample(reflection, lod, iProbeIndex);

    vec3 sheenLight = sheenSample.rgb;
    return sheenLight * sheenColor * brdf;
}

float applyIorToRoughness(float roughness, float ior)
{
    // Scale roughness with IOR so that an IOR of 1.0 results in no microfacet refraction and
    // an IOR of 1.5 results in the default amount of microfacet refraction.
    return roughness * clamp(ior * 2.0 - 2.0, 0.0, 1.0);
}

// Compute attenuated light as it travels through a volume.
vec3 applyVolumeAttenuation(vec3 radiance, float transmissionDistance, vec3 attenuationColor, float attenuationDistance)
{
    if (attenuationDistance == 0.0)
    {
        // Attenuation distance is +∞ (which we indicate by zero), i.e. the transmitted color is not attenuated at all.
        return radiance;
    }
    else
    {
        // Compute light attenuation using Beer's law.
        vec3 transmittance = pow(attenuationColor, vec3(transmissionDistance / attenuationDistance));
        return transmittance * radiance;
    }
}

vec3
getVolumeTransmissionRay(vec3 n, vec3 v, float thickness, float ior, mat4 modelMatrix)
{
    // Direction of refracted light.
    vec3 refractionVector = refract(-v, normalize(n), 1.0 / ior);

    // Compute rotation-independant scaling of the model matrix.
    vec3 modelScale;
    modelScale.x = length(vec3(modelMatrix[0].xyz));
    modelScale.y = length(vec3(modelMatrix[1].xyz));
    modelScale.z = length(vec3(modelMatrix[2].xyz));

    // The thickness is specified in local space.
    return normalize(refractionVector) * thickness * modelScale;
}

vec3
getTransmissionSample(vec2 fragCoord, float roughness, float ior)
{
    ivec2 u_TransmissionFramebufferSize = textureSize(sampler2D(at2DTextures[nonuniformEXT(tViewInfo.tData.iTransmissionFrameBufferIndex)], tSamplerLinearClamp), 0);
    float framebufferLod = log2(float(u_TransmissionFramebufferSize.x)) * applyIorToRoughness(roughness, ior);
    vec3 transmittedLight = textureLod(sampler2D(at2DTextures[nonuniformEXT(tViewInfo.tData.iTransmissionFrameBufferIndex)], tSamplerLinearClamp), fragCoord.xy, framebufferLod).rgb;
    return transmittedLight;
}

vec3
getIBLVolumeRefraction(vec3 n, vec3 v, float perceptualRoughness, vec3 baseColor, vec3 position, mat4 modelMatrix,
    mat4 viewMatrix, mat4 projMatrix, float ior, float thickness, vec3 attenuationColor, float attenuationDistance, float dispersion)
{
    vec3 transmittedLight;
    float transmissionRayLength;
    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_DISPERSION))
    {
        // Dispersion will spread out the ior values for each r,g,b channel
        float halfSpread = (ior - 1.0) * 0.025 * dispersion;
        vec3 iors = vec3(ior - halfSpread, ior, ior + halfSpread);


        for (int i = 0; i < 3; i++)
        {
            vec3 transmissionRay = getVolumeTransmissionRay(n, v, thickness, iors[i], modelMatrix);
            // transmissionRay.z *= -1;
            // TODO: taking length of blue ray, ideally we would take the length of the green ray. For now overwriting seems ok
            transmissionRayLength = length(transmissionRay);
            vec3 refractedRayExit = position + transmissionRay;

            // Project refracted vector on the framebuffer, while mapping to normalized device coordinates.
            vec4 ndcPos = projMatrix * viewMatrix * vec4(refractedRayExit, 1.0);
            vec2 refractionCoords = ndcPos.xy / ndcPos.w;
            ivec2 u_TransmissionFramebufferSize = textureSize(sampler2D(at2DTextures[nonuniformEXT(tViewInfo.tData.iTransmissionFrameBufferIndex)], tSamplerLinearClamp), 0);
            vec2 fac = tViewInfo.tData.tViewportSize.xy / vec2(float(u_TransmissionFramebufferSize.x), float(u_TransmissionFramebufferSize.y)); 
            refractionCoords += 1.0;
            refractionCoords /= 2.0;
            refractionCoords *= fac;

            // Sample framebuffer to get pixel the refracted ray hits for this color channel.
            transmittedLight[i] = getTransmissionSample(refractionCoords, perceptualRoughness, iors[i])[i];
        }
    }
    else
    {
        vec3 transmissionRay = getVolumeTransmissionRay(n, v, thickness, ior, modelMatrix);
        // transmissionRay.z *= -1;
        transmissionRayLength = length(transmissionRay);
        vec3 refractedRayExit = position + transmissionRay;

        // Project refracted vector on the framebuffer, while mapping to normalized device coordinates.
        vec4 ndcPos = projMatrix * viewMatrix * vec4(refractedRayExit, 1.0);
        vec2 refractionCoords = ndcPos.xy / ndcPos.w;
        ivec2 u_TransmissionFramebufferSize = textureSize(sampler2D(at2DTextures[nonuniformEXT(tViewInfo.tData.iTransmissionFrameBufferIndex)], tSamplerLinearClamp), 0);

        vec2 fac = tViewInfo.tData.tViewportSize.xy / vec2(float(u_TransmissionFramebufferSize.x), float(u_TransmissionFramebufferSize.y)); 

        // 
        refractionCoords += 1.0;
        refractionCoords /= 2.0;
        refractionCoords *= fac;

        // Sample framebuffer to get pixel the refracted ray hits.
        transmittedLight = getTransmissionSample(refractionCoords, perceptualRoughness, ior);

    }
    vec3 attenuatedColor = applyVolumeAttenuation(transmittedLight, transmissionRayLength, attenuationColor, attenuationDistance);

    return attenuatedColor * baseColor;
}

vec3 getPunctualRadianceTransmission(vec3 normal, vec3 view, vec3 pointToLight, float alphaRoughness,
    vec3 baseColor, float ior)
{
    float transmissionRougness = applyIorToRoughness(alphaRoughness, ior);

    vec3 n = normalize(normal);           // Outward direction of surface point
    vec3 v = normalize(view);             // Direction from surface point to view
    vec3 l = normalize(pointToLight);
    vec3 l_mirror = normalize(l + 2.0*n*dot(-l, n));     // Mirror light reflection vector on surface
    vec3 h = normalize(l_mirror + v);            // Halfway vector between transmission light vector and v

    float D = pl_distribution_ggx(clamp(dot(n, h), 0.0, 1.0), transmissionRougness);
    float Vis = pl_masking_shadowing_ggx(clamp(dot(n, l_mirror), 0.0, 1.0), clamp(dot(n, v), 0.0, 1.0), transmissionRougness);

    // Transmission BTDF
    return baseColor * D * Vis;
}

float
textureProj(vec4 shadowCoord, vec2 offset, int textureIndex)
{
	float shadow = 1.0;
    vec2 comp2 = shadowCoord.st + offset;

	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 )
    {
		float dist = texture(sampler2D(at2DTextures[nonuniformEXT(textureIndex)], tSamplerNearestClamp), comp2).r;
		if (shadowCoord.w > 0 && dist > shadowCoord.z)
        {
			shadow = 0.0; // ambient
		}
	}
	return shadow;
}

float
filterPCF(vec4 sc, vec2 offset, int textureIndex)
{
	ivec2 texDim = textureSize(sampler2D(at2DTextures[nonuniformEXT(textureIndex)], tSamplerNearestClamp), 0).xy;
	float scale = 1.0;
	float dx = scale * 1.0 / (float(texDim.x));
	float dy = scale * 1.0 / (float(texDim.y));

	float shadowFactor = 0.0;
	// int count = 0;
	int range = 1;
	
	for (int x = -range; x <= range; x++)
    {
		for (int y = -range; y <= range; y++)
        {
			shadowFactor += textureProj(sc, vec2(dx*x, dy*y) + offset, textureIndex);
			// count++;
		}
	}
	// return shadowFactor / count;
	return shadowFactor / 9.0;
}

#endif // LIGHTING_GLSL