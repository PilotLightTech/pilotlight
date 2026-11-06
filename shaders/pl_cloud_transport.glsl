#ifndef PL_CLOUD_TRANSPORT_GLSL
#define PL_CLOUD_TRANSPORT_GLSL

#include "pl_math.glsl"
#include "pl_cloud_shadows.glsl"

float
cloudPhase(float mu, float g)
{
    return (1.0 - g * g) / (4.0 * PL_PI * pow(max(1.0 + g*g - 2.0*g*mu, 0.001), 1.5));
}

float
cloudSunDepth(vec3 p, vec3 sun)
{
    float cachedDepth = cloudCachedOpticalDepth(p - vec3(0.0, tGpuScene.tData.planetRadius, 0.0));
    if(cachedDepth >= 0.0)
        return cachedDepth;
    float outer = tGpuScene.tData.planetRadius + tGpuScene.tData.tCloudLayer.y + tGpuScene.tData.tCloudLayer.z;
    float distanceToExit = min(max(cloudSphere(p, sun, outer).y, 0.0), 30.0);
    float opticalDepth = 0.0;
    // Quadratic spacing resolves nearby occluders while covering a long
    // slanted path at sunset. Coarser density keeps nested marching bounded.
    for(int j = 0; j < 6; j++)
    {
        float a = float(j) / 6.0, b = float(j + 1) / 6.0;
        float start = a*a * distanceToExit, end = b*b * distanceToExit;
        opticalDepth += cloudDensity(p + sun * (start + end) * 0.5, false) * (end - start);
    }
    return opticalDepth * tGpuScene.tData.tCloudLayer.w;
}

// Radiance and transmittance from the camera to an endpoint. Distance is
// in atmosphere units; shared by the background and transparent surface passes.
vec4
cloudIntegrateTransport(vec3 worldOrigin, vec3 ray, float maxDistance)
{
    if(tGpuScene.tData.tCloudLayer.x <= 0.0 || tGpuScene.tData.tCloudLayer.w <= 0.0 || maxDistance <= 0.0)
        return vec4(0.0, 0.0, 0.0, 1.0);
    float scale = max(tGpuScene.tData.fAtmosphereConversion, 1e-7);
    float radius = tGpuScene.tData.planetRadius;
    vec3 origin = worldOrigin * scale + vec3(0.0, radius, 0.0);
    maxDistance = min(maxDistance, 80.0);
    vec2 planet = cloudSphere(origin, ray, radius);
    if(length(origin) <= radius + 0.0001 && dot(origin, ray) < 0.0)
        return vec4(0.0, 0.0, 0.0, 1.0);
    if(cloudForwardHit(planet))
    {
        maxDistance = min(maxDistance, planet.x);
    }

    float innerRadius = radius + tGpuScene.tData.tCloudLayer.y;
    vec2 ranges[2];
    if(!cloudShellIntervals(origin, ray, innerRadius, innerRadius + tGpuScene.tData.tCloudLayer.z, maxDistance, ranges))
        return vec4(0.0, 0.0, 0.0, 1.0);
    vec3 sun = -tGpuScene.tData.tDirection * inversesqrt(max(dot(tGpuScene.tData.tDirection, tGpuScene.tData.tDirection), 1e-20));
    float phase = mix(cloudPhase(dot(ray, sun), -0.2), cloudPhase(dot(ray, sun), 0.65), 0.85);
    vec3 radiance = vec3(0.0);
    float transmittance = 1.0;
    for(int interval = 0; interval < 2; interval++)
    {
        float intervalLength = ranges[interval].y - ranges[interval].x;
        if(intervalLength <= 0.0)
            continue;
        // The authored count is a quality floor. Long grazing rays need more
        // samples than short vertical rays, particularly when inside the layer.
        float steps = cloudSampleCount(tGpuScene.tData.tCloudNoise.w, intervalLength, tGpuScene.tData.tCloudNoise.x, 256.0);
        for(int i = 0; i < 256; i++)
        {
            if(float(i) >= steps || transmittance < 0.005)
                break;
            float a = cloudSampleBoundary(float(i), steps, intervalLength);
            float b = cloudSampleBoundary(float(i + 1), steps, intervalLength);
            float ds = b - a;
            // Deterministic midpoint quadrature: no screen-space stipple without TAA.
            // Quadratic spacing resolves nearby density when flying through clouds.
            float distance = ranges[interval].x + (a + b) * 0.5;
            vec3 p = origin + ray * distance;
            float density = cloudDensity(p, true);
            if(density <= 0.001)
                continue;
            float sunDepth = cloudSunDepth(p, sun);
            vec3 up = normalize(p);
            float sunElevation = dot(up, sun);
            vec2 sunPlanet = cloudSphere(p, sun, radius);
            float daylight = cloudForwardHit(sunPlanet) ? 0.0 : 1.0;
            // Approximate atmospheric sun attenuation and low-order multiple
            // scattering. Neither approximation adds energy independent of the sun.
            vec3 sunTransmission = exp(-tGpuScene.tData.extinctionRayleighGround * 8.0 / max(sunElevation, 0.04));
            vec3 sunlight = max(tGpuScene.tData.tColor, vec3(0.0)) * max(tGpuScene.tData.fIntensity, 0.0);
            float direct = phase * exp(-sunDepth);
            float multiple = 0.08 * exp(-0.35 * sunDepth) + 0.025 * exp(-0.1 * sunDepth);
            float height = clamp((length(p) - innerRadius) / tGpuScene.tData.tCloudLayer.z, 0.0, 1.0);
            vec3 ambient = vec3(0.035, 0.055, 0.09) * mix(0.35, 1.0, height) * smoothstep(-0.15, 0.2, sunElevation);
            vec3 source = sunlight * (daylight * sunTransmission * (direct + multiple) + ambient);
            float segmentT = exp(-density * tGpuScene.tData.tCloudLayer.w * ds);
            radiance += transmittance * (1.0 - segmentT) * source;
            transmittance *= segmentT;
        }
    }
    return vec4(radiance, transmittance);
}

vec4
cloudSurfaceTransport(vec3 worldPosition, uint cameraIndex)
{
    if(tGpuScene.tData.tCloudLayer.x <= 0.0 || tGpuScene.tData.tCloudLayer.w <= 0.0)
        return vec4(0.0, 0.0, 0.0, 1.0);
    mat4 view = tViewInfo.tData.tCameraView[cameraIndex];
    mat4 invView = tViewInfo.tData.tCameraViewInv[cameraIndex];
    vec3 worldOrigin = tViewInfo.tData.tCameraPos.xyz;
    if(abs(tViewInfo.tData.tCameraProjection[cameraIndex][3][3]) >= 0.5)
    {
        // Orthographic rays start on the near plane, not at the camera center.
        vec3 viewPosition = (view * vec4(worldPosition, 1.0)).xyz;
        vec4 nearH = tViewInfo.tData.tCameraProjectionInv[cameraIndex] * vec4(0.0, 0.0, 1.0, 1.0);
        worldOrigin = (invView * vec4(viewPosition.xy, nearH.z / nearH.w, 1.0)).xyz;
    }
    vec3 delta = worldPosition - worldOrigin;
    float distanceToSurface = length(delta);
    if(distanceToSurface <= 1e-6)
        return vec4(0.0, 0.0, 0.0, 1.0);
    return cloudIntegrateTransport(worldOrigin, delta / distanceToSurface,
        distanceToSurface * max(tGpuScene.tData.fAtmosphereConversion, 1e-7));
}
#endif
