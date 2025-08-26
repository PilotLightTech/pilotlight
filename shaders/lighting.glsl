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

vec3 getIBLRadianceGGX(vec3 n, vec3 v, float roughness, int u_MipCount, vec3 tWorldPos, int iProbeIndex)
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