#ifndef PL_CLOUD_DENSITY_GLSL
#define PL_CLOUD_DENSITY_GLSL

float
cloudHash(vec3 p)
{
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

float
cloudNoise(vec3 p)
{
    vec3 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(cloudHash(i), cloudHash(i + vec3(1,0,0)), f.x), mix(cloudHash(i + vec3(0,1,0)), cloudHash(i + vec3(1,1,0)), f.x), f.y),
               mix(mix(cloudHash(i + vec3(0,0,1)), cloudHash(i + vec3(1,0,1)), f.x), mix(cloudHash(i + vec3(0,1,1)), cloudHash(i + vec3(1,1,1)), f.x), f.y), f.z);
}

float
cloudFbm(vec3 p)
{
    return 0.57143 * cloudNoise(p) + 0.28571 * cloudNoise(p * 2.03 + 17.1) + 0.14286 * cloudNoise(p * 4.11 + 31.7);
}

// p is planet-relative, in atmosphere units. Noise remains fixed in world
// space as the camera moves; only the authored wind advects the density.
float
cloudDensity(vec3 p, bool detail)
{
    vec4 layer = tGpuScene.tData.tCloudLayer;
    float h = (length(p) - tGpuScene.tData.planetRadius - layer.y) / layer.z;
    if(h <= 0.0 || h >= 1.0)
        return 0.0;
    p.y -= tGpuScene.tData.planetRadius;
    p.xz -= tGpuScene.tData.tCloudNoise.yz * tGpuScene.tData.g_time;
    vec3 q = p * tGpuScene.tData.tCloudNoise.x;
    float weather = cloudNoise(vec3(q.x * 0.18, 7.0, q.z * 0.18));
    float coverage = clamp(layer.x + (weather - 0.5) * 0.35, 0.0, 1.0);
    float shape = clamp((cloudFbm(q) - 0.25) / 0.6, 0.0, 1.0);
    float density = clamp((shape - (1.0 - coverage)) / max(coverage, 0.01), 0.0, 1.0);
    // Flat bases, rounded tops, and fine erosion along the exposed edges.
    density *= smoothstep(0.0, 0.12, h) * (1.0 - smoothstep(0.55, 1.0, h));
    if(detail && density > 0.0)
    {
        density = max(0.0, density - (1.0 - cloudFbm(q * 6.0)) * 0.16 * (1.0 - density));
    }
    return density;
}

#endif
