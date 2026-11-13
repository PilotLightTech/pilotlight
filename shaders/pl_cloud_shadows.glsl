#ifndef PL_CLOUD_SHADOWS_GLSL
#define PL_CLOUD_SHADOWS_GLSL

#include "pl_cloud_math.glsl"
#include "pl_cloud_density.glsl"

// Attenuate direct celestial light. Trace from the receiver rather than projecting
// a camera-centered mask, so terrain, meshes and foliage share world-fixed shadows.
float cloudLightTransmissionTrace(vec3 worldPosition, vec3 sun)
{
    vec4 layer = tGpuScene.tData.tCloudLayer;
    if(layer.x <= 0.0 || layer.w <= 0.0) return 1.0;
    float directionLength = length(sun);
    if(directionLength < 1e-6) return 1.0;
    sun /= directionLength;
    float radius = tGpuScene.tData.planetRadius;
    vec3 origin = worldPosition * max(tGpuScene.tData.fAtmosphereConversion, 1e-7) + vec3(0.0, radius, 0.0);
    // No artificial camera-distance cutoff: a high layer still shadows ground.
    float maxDistance = 2.0 * (radius + layer.y + layer.z);
    vec2 planet = cloudSphere(origin, sun, radius);
    if(cloudForwardHit(planet)) maxDistance = min(maxDistance, planet.x);
    vec2 ranges[2];
    if(!cloudShellIntervals(origin, sun, radius + layer.y, radius + layer.y + layer.z, maxDistance, ranges))
        return 1.0;
    float opticalDepth = 0.0;
    for(int interval = 0; interval < 2; interval++)
    {
        float distance2 = ranges[interval].y - ranges[interval].x;
        if(distance2 <= 0.0)
            continue;
        // Do not spend samples on the empty distance from the ground to the
        // cloud base. Grazing sun rays use extra samples through the layer.
        float steps = cloudSampleCount(16.0, distance2, tGpuScene.tData.tCloudNoise.x, 64.0);
        for(int i = 0; i < 64; i++)
        {
            if(float(i) >= steps) break;
            float a = float(i) / steps * distance2;
            float b = min(float(i + 1) / steps, 1.0) * distance2;
            float ds = b - a;
            vec3 p = origin + sun * (ranges[interval].x + (a + b) * 0.5);
            // Exactly the visible density, including erosion and wind.
            opticalDepth += cloudDensity(p, true) * layer.w * ds;
            if(opticalDepth >= 8.0)
                return exp(-8.0);
        }
    }
    return exp(-opticalDepth);
}

// Returns negative outside the cached footprint. Atmosphere-space coordinates
// retain world altitude (the planet-radius offset is not part of the transform).
float
cloudCachedOpticalDepth(vec3 position)
{
    if(tGpuScene.tData.tCloudShadowCenter.w < 0.0) return -1.0;
    vec3 delta = position - tGpuScene.tData.tCloudShadowCenter.xyz;
    vec2 uv = vec2(dot(delta, tGpuScene.tData.tCloudShadowRight.xyz) / tGpuScene.tData.tCloudShadowRight.w, dot(delta, tGpuScene.tData.tCloudShadowUp.xyz) / tGpuScene.tData.tCloudShadowUp.w) * 0.5 + 0.5;
    if(any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))))
        return -1.0;
    int index = int(tGpuScene.tData.tCloudShadowCenter.w);
    ivec2 size = textureSize(sampler2D(at2DTextures[nonuniformEXT(index)], tSamplerLinearClamp), 0);
    float slices = float(size.y / size.x);
    uv = clamp(uv, vec2(0.5 / float(size.x)), vec2(1.0 - 0.5 / float(size.x)));
    float depth = (dot(delta, tGpuScene.tData.tCloudShadowForward.xyz) / tGpuScene.tData.tCloudShadowForward.w * 0.5 + 0.5) * slices;
    if(depth <= 0.0)
        return 0.0;
    depth = min(depth, slices);
    float boundary = floor(depth);
    float lower = clamp(boundary - 1.0, 0.0, slices - 1.0);
    float upper = min(boundary, slices - 1.0);
    float a = boundary < 1.0 ? 0.0 : textureLod(sampler2D(at2DTextures[nonuniformEXT(index)], tSamplerLinearClamp), vec2(uv.x, cloudAtlasSliceY(uv.y, lower, slices, float(size.x))), 0.0).r;
    float b = textureLod(sampler2D(at2DTextures[nonuniformEXT(index)], tSamplerLinearClamp), vec2(uv.x, cloudAtlasSliceY(uv.y, upper, slices, float(size.x))), 0.0).r;
    return cloudInterpolateOpticalDepth(depth, a, b);
}

float
cloudSunTransmission(vec3 worldPosition)
{
    if(tGpuScene.tData.tCloudLayer.x <= 0.0 || tGpuScene.tData.tCloudLayer.w <= 0.0)
        return 1.0;
    float depth = cloudCachedOpticalDepth(worldPosition * max(tGpuScene.tData.fAtmosphereConversion, 1e-7));
    // Distant probes/receivers retain correct shadows outside the main-view cache.
    return depth >= 0.0 ? exp(-depth) : cloudLightTransmissionTrace(worldPosition, -tGpuScene.tData.tDirection);
}

#endif
