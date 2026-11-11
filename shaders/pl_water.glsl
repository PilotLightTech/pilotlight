#ifndef PL_WATER_GLSL
#define PL_WATER_GLSL

#include "pl_shader_interop_renderer.h"

#ifndef PL_WATER_WAVE_MODE_GLSL
#define PL_WATER_WAVE_MODE_GLSL

// Horizontal water in world units. Keep the long swells shared by both stages.
// The terrain water grid has 160-unit cells (at least 6 samples per swell).
vec3
pl_water_swell(vec2 position, vec2 direction, float wavelength, float amplitude, float time)
{
    float k = 6.28318530718 / wavelength;
    vec2 across = vec2(-direction.y, direction.x);
    float bendK = k / 3.7;
    float bendPhase = bendK * dot(position, across) - time * 0.017 + wavelength * 0.013;
    float phase = k * dot(position, direction) - sqrt(9.81 * k) * time + 0.8 * sin(bendPhase);
    vec2 gradient = k * direction + 0.8 * bendK * cos(bendPhase) * across;
    return vec3(gradient * (amplitude * cos(phase)), amplitude * sin(phase));
}

// xy = height gradient, z = height. Bounds use the sum of material amplitudes.
vec3
pl_water_legacy_surface(vec2 position, float time, plGpuWater water)
{
    vec2 direction0 = vec2(cos(radians(water.tSwell0.w)), sin(radians(water.tSwell0.w)));
    vec2 direction1 = vec2(cos(radians(water.tSwell1.w)), sin(radians(water.tSwell1.w)));
    return pl_water_swell(position, direction0, water.tSwell0.y, water.tSwell0.x, time * water.tSwell0.z) + pl_water_swell(position, direction1, water.tSwell1.y, water.tSwell1.x, time * water.tSwell1.z);
}


// Displaced position and its tangents with respect to the undisplaced XZ plane.
void
pl_water_gerstner_wave(vec2 p, float time, vec4 wave, float q, inout vec3 displacement, inout vec3 dx, inout vec3 dz)
{
    vec2 d = vec2(cos(radians(wave.w)), sin(radians(wave.w)));
    float k = 6.28318530718 / wave.y;
    float phase = k * dot(d, p) - sqrt(9.81 * k) * time * wave.z;
    float s = sin(phase), c = cos(phase);
    displacement += vec3(q * wave.x * d.x * c, wave.x * s, q * wave.x * d.y * c);
    vec3 derivative = vec3(-q * wave.x * d.x * s, wave.x * c, -q * wave.x * d.y * s) * k;
    dx += derivative * d.x;
    dz += derivative * d.y;
}

void
pl_water_gerstner_frame(vec2 p, float time, plGpuWater water, out vec3 displacement, out vec3 dx, out vec3 dz)
{
    displacement = vec3(0); dx = vec3(1,0,0); dz = vec3(0,0,1);
    float totalSlope = 6.28318530718 * (water.tSwell0.x / water.tSwell0.y + water.tSwell1.x / water.tSwell1.y);
    float q = water.tWaves.y / max(1.0, totalSlope);
    pl_water_gerstner_wave(p, time, water.tSwell0, q, displacement, dx, dz);
    pl_water_gerstner_wave(p, time, water.tSwell1, q, displacement, dx, dz);
}

#endif

#ifndef PL_WATER_VOLUME_GLSL
#define PL_WATER_VOLUME_GLSL

struct plWaterVolume
{
    vec3 transmittance;
    vec3 scattering;
};

// Normalized Henyey-Greenstein phase. Positive g favors travel in the incoming
// photon's direction (looking toward the sun produces forward scattering).
float
pl_water_phase(float cosine, float directionality)
{
    float g = clamp(directionality, -0.9, 0.9);
    float denominator = max(1.0 + g*g - 2.0*g*clamp(cosine, -1.0, 1.0), 0.0001);
    return (1.0 - g*g) / (12.56637061436 * denominator * sqrt(denominator));
}

vec3
pl_water_absorption(vec3 colorAtDistance, float distance)
{
    return -log(clamp(colorAtDistance, vec3(0.0001), vec3(1.0))) / max(distance, 0.001);
}

// Analytic single scattering for a homogeneous medium and constant incident
// illumination. No view-ray march, shadow march, or hidden color reconstruction.
plWaterVolume
pl_water_integrate(vec3 absorption, vec3 scattering, float distance, vec3 illumination)
{
    distance = max(distance, 0.0);
    vec3 extinction = max(absorption, vec3(0.0)) + max(scattering, vec3(0.0));
    vec3 opticalDepth = extinction * distance;
    plWaterVolume result;
    result.transmittance = exp(-opticalDepth);
    vec3 integral = (vec3(1.0) - result.transmittance) / max(extinction, vec3(0.00000001));
    // Avoid cancellation near clear water, zero length and the shoreline.
    for(int i = 0; i < 3; i++)
        if(opticalDepth[i] < 0.001)
            integral[i] = distance * (1.0 - 0.5 * opticalDepth[i] + opticalDepth[i]*opticalDepth[i] / 6.0);
    result.scattering = max(illumination, vec3(0.0)) * max(scattering, vec3(0.0)) * integral;
    return result;
}

vec3
pl_water_composite(vec3 background, vec3 reflection, plWaterVolume volume, float fresnel, vec3 fogScatter, vec3 fogTransmission)
{
    vec3 transmission = (1.0 - fresnel) * volume.transmittance;
    // Background already contains atmospheric fog. Add only the missing fog
    // share, rather than fogging the transmitted background a second time.
    return transmission * background + fogTransmission * (reflection + (1.0 - fresnel) * volume.scattering) + (vec3(1.0) - transmission) * fogScatter;
}
#endif

#ifndef PL_WATER_REFRACTION_GLSL
#define PL_WATER_REFRACTION_GLSL

// Reverse-Z opaque snapshot. The caller clamps to the active viewport.
float pl_water_refraction_texel(ivec2 pixel);

float
pl_water_depth_gradient(float before, float center, float after)
{
    if(before <= 0.0)
        return after > 0.0 ? after - center : 0.0;
    if(after <= 0.0)
        return center - before;
    float backward = center - before, forward = after - center;
    return abs(backward) < abs(forward) ? backward : forward;
}

vec3
pl_water_depth_position(vec2 pixel, float depth, mat4 inverseProjection, vec2 viewport)
{
    vec4 p = inverseProjection * vec4(pixel / viewport * 2.0 - 1.0, depth, 1.0);
    return p.xyz / p.w;
}

vec2
pl_water_distortion_offset(vec3 normalDeltaView, float ior, float strength, float maxPixels)
{
    // No constant IOR displacement, and no division by grazing view angles.
    vec2 offset = -normalDeltaView.xy * (4.0 * strength * maxPixels * (1.0 - 1.0 / max(ior, 1.0)));
    return offset * min(1.0, maxPixels / max(length(offset), 0.0001));
}

float
pl_water_distortion_confidence(vec2 candidate, vec2 original, float depth, vec2 gradient, vec3 surface, vec3 interfaceNormal, mat4 inverseProjection, vec2 viewport, float threshold)
{
    if(any(lessThan(candidate, vec2(0.5))) || any(greaterThan(candidate, viewport - 0.5)))
        return 0.0;
    
    // Check the complete bilinear color footprint so a valid depth sample cannot
    // drag foreground color across the waterline or an object's silhouette.
    ivec2 corner = ivec2(floor(candidate - 0.5));
    float confidence = 1.0;
    for(int y = 0; y < 2; y++)
    for(int x = 0; x < 2; x++)
    {
        vec2 pixel = vec2(corner + ivec2(x,y)) + 0.5;
        float actual = pl_water_refraction_texel(corner + ivec2(x,y));
        float expected = depth + dot(gradient, pixel - original);
        if(actual <= 0.0 || expected <= 0.0)
            return 0.0;
        vec3 position = pl_water_depth_position(pixel, actual, inverseProjection, viewport);
        if(dot(position - surface, interfaceNormal) > 0.0)
            return 0.0;
        vec3 plane = pl_water_depth_position(pixel, expected, inverseProjection, viewport);
        float error = length(position - plane);
        confidence = min(confidence, 1.0 - smoothstep(threshold * 0.25, threshold, error));
        // Subsequent taps can only lower confidence. Zero already rejects it.
        if(confidence <= 0.0)
            return 0.0;
    }
    return confidence;
}

vec2
pl_water_distort(vec2 pixel, vec2 offset, vec3 surface, vec3 interfaceNormal, mat4 inverseProjection, vec2 viewport, float maxDistance, float edgeFade, float intersectionFade, out float pathLength)
{
    float depth = pl_water_refraction_texel(ivec2(pixel));
    pathLength = maxDistance;
    if(depth <= 0.0)
        return pixel;
    vec3 background = pl_water_depth_position(pixel, depth, inverseProjection, viewport);
    if(dot(background - surface, interfaceNormal) > 0.0)
        return pixel;
    pathLength = min(length(background - surface), maxDistance);
    if(length(offset) < 0.0001)
        return pixel;
    vec2 edge = min(pixel, viewport - pixel);
    offset *= smoothstep(0.0, max(edgeFade, 0.001), min(edge.x, edge.y));
    offset *= smoothstep(0.0, max(intersectionFade, 0.001), pathLength);
    ivec2 texel = ivec2(pixel);
    vec2 gradient = vec2(
        pl_water_depth_gradient(pl_water_refraction_texel(texel-ivec2(1,0)), depth, pl_water_refraction_texel(texel+ivec2(1,0))),
        pl_water_depth_gradient(pl_water_refraction_texel(texel-ivec2(0,1)), depth, pl_water_refraction_texel(texel+ivec2(0,1))));
    float threshold = max(0.01, max(intersectionFade, pathLength * 0.05));
    float confidence = 1.0;
    // Validate the path as well as its endpoint: don't jump across thin objects.
    for(int i = 1; i <= 4; i++)
    {
        confidence = min(confidence, pl_water_distortion_confidence(pixel + offset * (float(i) / 4.0), pixel, depth, gradient, surface, interfaceNormal, inverseProjection, viewport, threshold));
        // Preserve the original undistorted pathLength on rejection.
        if(confidence <= 0.0)
            return pixel;
    }

    vec2 candidate = pixel + offset * confidence;
    if(confidence <= 0.0 || pl_water_distortion_confidence(candidate, pixel, depth, gradient, surface, interfaceNormal, inverseProjection, viewport, threshold) <= 0.0)
        return pixel;
    
    // Use the same accepted coordinate for color and absorption. Reconstruct
    // within the texel on its local plane rather than using stair-stepped depth.
    ivec2 c = ivec2(candidate);
    float d = pl_water_refraction_texel(c);
    vec2 g = vec2(
        pl_water_depth_gradient(pl_water_refraction_texel(c-ivec2(1,0)), d, pl_water_refraction_texel(c+ivec2(1,0))),
        pl_water_depth_gradient(pl_water_refraction_texel(c-ivec2(0,1)), d, pl_water_refraction_texel(c+ivec2(0,1))));
    
    d += dot(g, candidate - (vec2(c) + 0.5));
    if(d <= 0.0)
        return pixel;
    pathLength = min(length(pl_water_depth_position(candidate, d, inverseProjection, viewport) - surface), maxDistance);
    return candidate;
}
#endif

#ifndef PL_WATER_MATH_ONLY

vec3
pl_water_fft_displacement(vec2 p, plGpuWater water)
{
    if(water.tOcean.x < 0.0)
        return vec3(0);
    vec2 uv = p / water.tOcean.y + vec2(0.5 / water.tOcean.z);
    return textureLod(sampler2D(at2DTextures[nonuniformEXT(int(water.tOcean.x))], tSamplerLinearRepeat), uv, 0.0).xyz;
}

void
pl_water_frame(vec2 p, float time, plGpuWater water, out vec3 displacement, out vec3 dx, out vec3 dz)
{
    dx = vec3(1,0,0); dz = vec3(0,0,1); displacement = vec3(0);
    if(water.tWaves.x == 2.0)
    {
        displacement = pl_water_fft_displacement(p, water);
        float stepSize = water.tOcean.y / water.tOcean.z;
        dx += (pl_water_fft_displacement(p + vec2(stepSize,0), water) - pl_water_fft_displacement(p - vec2(stepSize,0), water)) / (2.0 * stepSize);
        dz += (pl_water_fft_displacement(p + vec2(0,stepSize), water) - pl_water_fft_displacement(p - vec2(0,stepSize), water)) / (2.0 * stepSize);
    }
    else if(water.tWaves.x == 1.0)
    {
        // Sum(q*k*A) <= steepness < 1 prevents Gerstner foldover.
        pl_water_gerstner_frame(p, time, water, displacement, dx, dz);
    }
    else
    {
        vec3 surface = pl_water_legacy_surface(p, time, water);
        displacement.y = surface.z; dx.y = surface.x; dz.y = surface.y;
    }
}

vec3
pl_water_displacement(vec2 p, float time, plGpuWater water)
{
    if(water.tWaves.x == 2.0)
        return pl_water_fft_displacement(p, water);
    vec3 displacement, dx, dz;
    pl_water_frame(p, time, water, displacement, dx, dz);
    return displacement;
}

vec3
pl_water_surface_normal(vec2 p, float time, plGpuWater water)
{
    vec3 displacement, dx, dz;
    pl_water_frame(p, time, water, displacement, dx, dz);
    vec3 normal = cross(dz, dx);
    return length(normal) > 0.00001 ? normalize(normal) : vec3(0,1,0);
}

vec3
pl_water_surface(vec2 p, float time, plGpuWater water)
{
    vec3 displacement, dx, dz;
    pl_water_frame(p, time, water, displacement, dx, dz);
    vec3 normal = cross(dz, dx);
    return vec3(-normal.xz / max(abs(normal.y), 0.05), displacement.y);
}

#ifdef PL_WATER_FRAGMENT


// Reconstruct the same rays used by half-resolution cloud integration. Depth
// zero means sky; source depth is saved with the cloud sample, not read from a
// depth buffer that later forward passes may have modified.
void
pl_water_cloud_ray(vec2 pixel, float depth, out vec3 origin, out vec3 ray, out float distance)
{
    mat4 invProjection = tViewInfo.tData.tCameraProjectionInv[0];
    mat4 invView = tViewInfo.tData.tCameraViewInv[0];
    vec2 ndc = pixel / tViewInfo.tData.tViewportSize.xy * 2.0 - 1.0;
    vec4 nearH = invProjection * vec4(ndc, 1.0, 1.0);
    vec3 nearView = nearH.xyz / nearH.w;
    bool perspective = abs(tViewInfo.tData.tCameraProjection[0][3][3]) < 0.5;
    ray = normalize(mat3(invView) * (perspective ? nearView : vec3(0,0,1)));
    vec3 worldOrigin = perspective ? tViewInfo.tData.tCameraPos.xyz : (invView * vec4(nearView, 1.0)).xyz;
    float scale = max(tGpuScene.tData.fAtmosphereConversion, 1e-7);
    origin = worldOrigin * scale + vec3(0, tGpuScene.tData.planetRadius, 0);
    distance = 1e30;
    if(depth > 0.0)
    {
        vec4 point = invProjection * vec4(ndc, depth, 1.0);
        vec3 world = (invView * vec4(point.xyz / point.w, 1.0)).xyz;
        distance = max(dot(world - worldOrigin, ray) * scale, 0.0);
    }
}

bool
pl_water_cached_cloud(out vec4 transport)
{
    transport = vec4(0,0,0,1);
    if(tGpuScene.tData.tCloudLayer.x <= 0.0 || tGpuScene.tData.tCloudLayer.w <= 0.0)
        return true;
    int colorIndex = int(tViewInfo.tData.tCloudTransportCache.x);
    int depthIndex = int(tViewInfo.tData.tCloudTransportCache.y);
    if(colorIndex < 0 || depthIndex < 0) return false;
    vec3 origin, ray;
    float surfaceDistance;
    pl_water_cloud_ray(gl_FragCoord.xy, gl_FragCoord.z, origin, ray, surfaceDistance);
    float radius = tGpuScene.tData.planetRadius;
    vec2 layer = tGpuScene.tData.tCloudLayer.yz;
    float end = cloudTransportRayEnd(origin, ray, radius, layer.x, layer.y);
    if(end == 0.0) return true;
    if(!cloudTransportDepthsCover(end, surfaceDistance, 1e30)) return false;

    ivec2 viewport = ivec2(tViewInfo.tData.tViewportSize.xy);
    ivec2 lowSize = (viewport + 1) / 2;
    float centerDepth = texelFetch(sampler2D(at2DTextures[nonuniformEXT(tViewInfo.tData.iTransmissionDepthIndex)], tSamplerNearestClamp), ivec2(gl_FragCoord.xy), 0).r;
    // Match the main composite's sample centers, clamping, and depth weights.
    vec2 coordinate = (floor(gl_FragCoord.xy) - 1.0) * 0.5;
    ivec2 base = ivec2(floor(coordinate));
    vec2 fraction = fract(coordinate);
    vec4 sum = vec4(0);
    float totalWeight = 0.0;
    for(int y = 0; y < 2; y++)
    {
        for(int x = 0; x < 2; x++)
        {
            float weight = (x == 0 ? 1.0 - fraction.x : fraction.x) * (y == 0 ? 1.0 - fraction.y : fraction.y);
            if(weight <= 0.0)
                continue;
            ivec2 p = clamp(base + ivec2(x,y), ivec2(0), lowSize - 1);
            float depth = texelFetch(sampler2D(at2DTextures[nonuniformEXT(depthIndex)], tSamplerNearestClamp), p, 0).g;
            if((centerDepth > 0.0) != (depth > 0.0))
                continue;
            float difference = abs(depth - centerDepth) / max(max(depth, centerDepth), 1e-8);
            weight *= exp(-difference * 80.0);
            if(weight <= 0.0)
                continue;
            vec2 source = vec2(min(p * 2 + 1, viewport - 1)) + 0.5;
            float opaqueDistance, tapSurfaceDistance;
            pl_water_cloud_ray(source, gl_FragCoord.z, origin, ray, tapSurfaceDistance);
            pl_water_cloud_ray(source, depth, origin, ray, opaqueDistance);
            float tapEnd = cloudTransportRayEnd(origin, ray, radius, layer.x, layer.y);
            // Reject the whole lookup if a contributing sample integrated an
            // incomplete layer or includes clouds beyond the water surface.
            if(!cloudTransportDepthsCover(tapEnd, tapSurfaceDistance, opaqueDistance))
                return false;
            sum += weight * texelFetch(sampler2D(at2DTextures[nonuniformEXT(colorIndex)], tSamplerNearestClamp), p, 0);
            totalWeight += weight;
        }
    }
    if(totalWeight < 1e-5)
        return false;
    transport = sum / totalWeight;
    transport.a = clamp(transport.a, 0.0, 1.0);
    return true;
}

// Pixel differentials on the analytic swell tangent plane. Raster derivatives
// of the coarse displaced triangles have discontinuous slopes at shared edges;
// using them for filtering makes those edges visible in normals and foam.
mat2
pl_water_pixel_differentials(vec3 normal)
{
    uint camera = tObjectInfo.tData.uGlobalIndex;
    mat4 invProjection = tViewInfo.tData.tCameraProjectionInv[camera];
    vec2 pixel = gl_FragCoord.xy;
    vec2 viewport = tViewInfo.tData.tViewportSize.xy;
    vec4 center = invProjection * vec4(pixel / viewport * 2.0 - 1.0, gl_FragCoord.z, 1.0);
    vec4 right = invProjection * vec4((pixel + vec2(1, 0)) / viewport * 2.0 - 1.0, gl_FragCoord.z, 1.0);
    vec4 down = invProjection * vec4((pixel + vec2(0, 1)) / viewport * 2.0 - 1.0, gl_FragCoord.z, 1.0);
    vec4 nearPoint = invProjection * vec4(pixel / viewport * 2.0 - 1.0, 1.0, 1.0);
    vec4 farPoint = invProjection * vec4(pixel / viewport * 2.0 - 1.0, 0.5, 1.0);
    mat3 viewInv = mat3(tViewInfo.tData.tCameraViewInv[camera]);
    vec3 ray = normalize(viewInv * (farPoint.xyz / farPoint.w - nearPoint.xyz / nearPoint.w));
    vec3 dx = viewInv * (right.xyz / right.w - center.xyz / center.w);
    vec3 dy = viewInv * (down.xyz / down.w - center.xyz / center.w);
    float denominator = dot(normal, ray);
    denominator = (denominator < 0.0 ? -1.0 : 1.0) * max(abs(denominator), 0.001);
    dx -= ray * (dot(normal, dx) / denominator);
    dy -= ray * (dot(normal, dy) / denominator);
    return mat2(dx.xz, dy.xz);
}

// Reconstruct the opaque view position from a pixel in the active viewport.
vec3
pl_water_view_position(vec2 pixel, float depth)
{
    vec2 ndc = pixel / tViewInfo.tData.tViewportSize.xy * 2.0 - 1.0;
    vec4 p = tViewInfo.tData.tCameraProjectionInv[0] * vec4(ndc, depth, 1.0);
    return p.xyz / p.w;
}

float
pl_water_refraction_texel(ivec2 pixel)
{
    pixel = clamp(pixel, ivec2(0), ivec2(tViewInfo.tData.tViewportSize.xy) - 1);
    return texelFetch(sampler2D(at2DTextures[nonuniformEXT(tViewInfo.tData.iTransmissionDepthIndex)], tSamplerNearestClamp), pixel, 0).r;
}

vec3
pl_water_refraction(vec3 normal, vec3 view, float ior, out float pathLength, out float reflection, plGpuWater water)
{
    vec2 pixel = gl_FragCoord.xy;
    vec2 viewport = tViewInfo.tData.tViewportSize.xy;
    int colorIndex = tViewInfo.tData.iTransmissionFrameBufferIndex;
    vec2 colorSize = vec2(textureSize(sampler2D(at2DTextures[nonuniformEXT(colorIndex)], tSamplerLinearClamp), 0));
    vec3 surface = pl_water_view_position(pixel, gl_FragCoord.z);
    vec3 referenceNormal = vec3(0.0, view.y >= 0.0 ? 1.0 : -1.0, 0.0);
    mat3 toView = mat3(tViewInfo.tData.tCameraView[0]);
    ior = max(ior, 1.0);
    vec3 ray = refract(-view, normal, view.y >= 0.0 ? 1.0 / ior : ior);
    reflection = dot(ray, ray) < 0.001 ? 1.0 : 0.0;
    vec2 offset = pl_water_distortion_offset(toView * (normal - referenceNormal), ior, water.tDistortion.x, water.tDistortion.y);
    if(reflection > 0.0)
        offset = vec2(0.0);
    vec2 samplePixel = pl_water_distort(pixel, offset, surface, toView * referenceNormal, tViewInfo.tData.tCameraProjectionInv[0], viewport, water.tOptics.x, water.tOptics.z, water.tOptics.w, pathLength);
    samplePixel = clamp(samplePixel, vec2(0.5), viewport - 0.5);
    return textureLod(sampler2D(at2DTextures[nonuniformEXT(colorIndex)], tSamplerLinearClamp), samplePixel / colorSize, 0.0).rgb;
}

float
pl_water_foam_hash(ivec2 cell)
{
    // Hash lattice coordinates with integer arithmetic. Float/fract hashes
    // amplify rounding differences (including fused multiply-add contraction)
    // between a cell's corner and the same corner evaluated by its neighbor.
    // That breaks value-noise continuity and exposes entire rows of cells.
    uvec2 p = uvec2(cell);
    uint h = p.x * 0x8da6b343u ^ p.y * 0xd8163841u;
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return float(h >> 8) * (1.0 / 16777216.0);
}

float
pl_water_foam_noise(vec2 p)
{
    ivec2 cell = ivec2(floor(p));
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(pl_water_foam_hash(cell), pl_water_foam_hash(cell + ivec2(1, 0)), f.x), mix(pl_water_foam_hash(cell + ivec2(0, 1)), pl_water_foam_hash(cell + ivec2(1, 1)), f.x), f.y);
}

vec3
pl_water_foam_pattern(vec3 position, float time, plGpuWater water, mat2 pixelSteps)
{
    float footprint = max(length(pixelSteps[0]), length(pixelSteps[1])) / water.tDetail.w;
    float heading = radians(water.tSwell0.w);
    vec2 direction = vec2(cos(heading), sin(heading));
    vec2 p = position.xz / water.tDetail.w - direction * (time * water.tDetail.z * 2.0);
    // Slowly warped, world-space patches move together instead of independent
    // noise layers sliding across each other like smoke.
    vec2 warp = vec2(pl_water_foam_noise(p / 24.0), pl_water_foam_noise(p / 24.0 + 19.3));
    p += (warp - 0.5) * 2.5;
    float coarse = pl_water_foam_noise(p / 18.0);
    vec2 flow = vec2(dot(p, direction), dot(p, vec2(-direction.y, direction.x)));
    float fine = pl_water_foam_noise(flow / vec2(9.0, 3.0) + vec2(17.2, -9.6));
    coarse = mix(coarse, 0.5, smoothstep(9.0, 36.0, footprint));
    fine = mix(fine, 0.5, smoothstep(1.5, 6.0, footprint));

    // Air pockets leave irregular thin films between bubbles. Keep their layout
    // fixed in the advected coordinates, and filter unresolved pores to coverage.
    vec2 bubbles = p / 1.1;
    ivec2 cell = ivec2(floor(bubbles));
    vec2 local = fract(bubbles);
    float hole = 0.0;
    float aa = max(0.015, footprint / 1.1 * 0.65);
    float resolved = 1.0 - smoothstep(0.25, 0.8, footprint / 1.1);
    if(resolved > 0.0)
    {
        for(int y = -1; y <= 1; y++)
        {
            for(int x = -1; x <= 1; x++)
            {
                ivec2 neighbor = cell + ivec2(x,y);
                float seed = pl_water_foam_hash(neighbor);
                vec2 center = vec2(x,y) + 0.5 + 0.9 * (vec2(seed, pl_water_foam_hash(neighbor + ivec2(137,71))) - 0.5);
                float radius = mix(0.20, 0.43, pl_water_foam_hash(neighbor + ivec2(37,193)));
                float d = length(local - center);
                hole = max(hole, 1.0 - smoothstep(radius - aa, radius + aa, d));
            }
        }
    }
    float films = mix(0.67, 1.0 - hole, resolved);
    return vec3(coarse, fine, films);
}

float
pl_water_whitecaps(vec3 position, plGpuWater water, vec3 pattern, vec3 surface)
{
    // Artistic breaking-wave proxy: upper swell shoulders with enough slope.
    // This has no history buffer; patches form and dissolve with the crest.
    float amplitude = water.tWaves.x == 2.0 ? water.tOcean.w : water.tSwell0.x + water.tSwell1.x;
    float crest = smoothstep(water.tCaps.y, max(water.tCaps.z, water.tCaps.y + 0.0001), surface.z / max(amplitude, 0.0001));
    float steepness = smoothstep(water.tCaps.w, max(water.tAppearance.x, water.tCaps.w + 0.0001), length(surface.xy));

    float breakup = smoothstep(0.38, 0.72, pattern.x * 0.65 + pattern.y * 0.35);
    float coverage = water.tCaps.x * crest * steepness * breakup * pattern.z;
    return tViewInfo.tData.tCameraPos.y > position.y ? coverage : 0.0;
}

float
pl_water_shore_foam(vec3 position, plGpuWater water, vec3 pattern)
{
    int depthIndex = tViewInfo.tData.iTransmissionDepthIndex;
    float depth = texelFetch(sampler2D(at2DTextures[nonuniformEXT(depthIndex)], tSamplerNearestClamp), ivec2(gl_FragCoord.xy), 0).r;
    bool validDepth = depth > 0.0 && depth < gl_FragCoord.z;
    // Subtract in view space before rotating: avoid subtracting large translated
    // world positions, and use raster depth consistently on both sides.
    vec3 surfaceView = pl_water_view_position(gl_FragCoord.xy, gl_FragCoord.z);
    vec3 bedView = pl_water_view_position(gl_FragCoord.xy, validDepth ? depth : gl_FragCoord.z);
    vec3 separation = mat3(tViewInfo.tData.tCameraViewInv[0]) * (bedView - surfaceView);
    float waterDepth = -separation.y;
    // No foam on sky, foreground geometry, or the underside of water.
    if(!validDepth || tViewInfo.tData.tCameraPos.y <= position.y || waterDepth < 0.0)
        return 0.0;

    // Vertical separation makes the band stable when the camera tilts. Also
    // limit ray separation so distant silhouettes do not create horizon foam.
    float foamDepth = water.tShore.y * mix(0.45, 1.2, pattern.x);
    float shore = 1.0 - smoothstep(foamDepth * 0.04, foamDepth, waterDepth);
    float contact = 1.0 - smoothstep(water.tShore.z, max(water.tShore.w, water.tShore.z + 0.0001), length(separation));
    // Advected noise supplies the motion. A sine of bed depth produces parallel
    // contour stripes and makes every change in the underlying triangles visible.
    float breakup = smoothstep(0.22, 0.72, pattern.x * 0.65 + pattern.y * 0.35 + shore * 0.15);
    float films = mix(pattern.z, 1.0, smoothstep(0.65, 1.0, shore) * 0.45);
    return water.tShore.x * clamp(shore * contact * breakup * films, 0.0, 1.0);
}

vec2
pl_water_wave(vec2 position, vec2 direction, float wavelength, float amplitude, float time, mat2 pixelSteps)
{
    direction = normalize(direction);
    float k = 6.28318530718 / wavelength;
    vec2 across = vec2(-direction.y, direction.x);
    float bendK = k / 2.9;
    float seed = wavelength * 1.731;
    float bendPhase = bendK * dot(position, across) - sqrt(9.81 * bendK) * time * 0.19 + seed;
    float phase = k * dot(position, direction) - sqrt(9.81 * k) * time + seed + 1.2 * sin(bendPhase);
    // Differentiate the bent phase for both normals and pixel filtering.
    vec2 gradient = k * direction + 1.2 * bendK * cos(bendPhase) * across;
    // Fade waves smaller than a pixel to avoid distant specular shimmer.
    float footprint = max(abs(dot(gradient, pixelSteps[0])), abs(dot(gradient, pixelSteps[1])));
    float visibility = 1.0 - smoothstep(0.5, 3.0, footprint);
    return gradient * (amplitude * cos(phase) * visibility);
}

vec3
pl_water_normal(vec2 position, float time, float strength, plGpuWater water, vec3 surfaceNormal, mat2 pixelSteps)
{
    // Preserve the original legacy slope/detail weighting.
    if(water.tWaves.x == 0.0)
        surfaceNormal /= max(surfaceNormal.y, 0.0001);
    position /= water.tDetail.x;
    pixelSteps /= water.tDetail.x;
    time *= water.tDetail.y;
    // Uneven wavelengths, phases and headings avoid a small repeating crosshatch.
    vec2 slope = pl_water_wave(position, vec2(-0.94, 0.34), 37.1, 0.18, time, pixelSteps);
    slope += pl_water_wave(position, vec2(-0.79, -0.61), 23.7, 0.12, time, pixelSteps);
    slope += pl_water_wave(position, vec2(-0.31, 0.95), 13.3, 0.09, time, pixelSteps);
    slope += pl_water_wave(position, vec2(-0.98, -0.20), 8.3, 0.06, time, pixelSteps);
    slope += pl_water_wave(position, vec2(0.43, 0.90), 5.1, 0.035, time, pixelSteps);
    slope += pl_water_wave(position, vec2(-0.66, 0.75), 3.2, 0.022, time, pixelSteps);
    slope += pl_water_wave(position, vec2(0.86, -0.51), 2.1, 0.014, time, pixelSteps);
    slope += pl_water_wave(position, vec2(-0.48, -0.88), 1.37, 0.009, time, pixelSteps);
    return normalize(surfaceNormal + vec3(-slope.x * strength, 0.0, -slope.y * strength));
}
#endif
#endif // PL_WATER_MATH_ONLY
#endif // PL_WATER_GLSL
