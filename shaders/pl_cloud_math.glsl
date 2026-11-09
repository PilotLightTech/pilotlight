#ifndef PL_CLOUD_MATH_GLSL
#define PL_CLOUD_MATH_GLSL

// Scalar helpers shared with the CPU sampling regression test.
float
cloudSampleBoundary(float index, float count, float distance2)
{
    // Fractional counts introduce the final segment continuously.
    float u = min(index / count, 1.0);
    return distance2 * u * u;
}

float
cloudSampleCount(float minimumCount, float distance2, float frequency, float maximumCount)
{
    return clamp(max(minimumCount, distance2 * frequency * 8.0), 16.0, maximumCount);
}

float
cloudViewMaxSegmentLength(float frequency)
{
    // Two samples per cell of the finest main-shape FBM octave (4.11).
    // This targets bulk-density banding, not all of the finer edge erosion.
    return 1.0 / (2.0 * 4.11 * max(frequency, 0.01));
}

float
cloudViewSampleCount(float minimumCount, float distance2, float frequency, float maximumCount)
{
    // For x(i) = L * (i/N)^2, every segment is at most 2L/N,
    // including the partial final segment when N is fractional.
    float requiredCount = 2.0 * distance2 / cloudViewMaxSegmentLength(frequency);
    // Preserve the bounded march budget. At the cap the spacing target is
    // best effort; still cover the entire interval rather than truncating it.
    return clamp(max(minimumCount, requiredCount), 16.0, maximumCount);
}

// Kept independent of renderer bindings so the ray math can be regression tested.
vec2
cloudRayUV(vec2 pixelCenter, vec2 viewportSize)
{
    return pixelCenter / viewportSize;
}

vec2
cloudSphere(vec3 origin, vec3 direction, float radius)
{
    float b = dot(origin, direction);
    float r = length(origin);
    float d = b * b - (r - radius) * (r + radius);
    if(d < 0.0)
        return vec2(1.0, -1.0);
    float root = sqrt(d);
    return vec2(-b - root, -b + root);
}

bool
cloudForwardHit(vec2 interval)
{
    // A miss uses an inverted interval, whose positive first value is NOT a hit.
    return interval.x <= interval.y && interval.x > 0.0;
}

// Return both forward shell intervals, excluding the empty inner sphere.
// Intervals of zero length are allowed and must be skipped by the caller.
bool
cloudShellIntervals(vec3 origin, vec3 direction, float innerRadius, float outerRadius, float maxDistance, out vec2 ranges[2])
{
    vec2 outer = cloudSphere(origin, direction, outerRadius);
    float start = max(outer.x, 0.0), end = min(outer.y, maxDistance);
    if(end <= start)
        return false;
    vec2 inner = cloudSphere(origin, direction, innerRadius);
    float gapStart = clamp(inner.x, start, end), gapEnd = clamp(inner.y, start, end);
    ranges[0] = gapEnd > gapStart ? vec2(start, gapStart) : vec2(start, end);
    ranges[1] = gapEnd > gapStart ? vec2(gapEnd, end) : vec2(end);
    return true;
}

// Last possible cloud contribution on the ray, matching transport's planet and
// distance limits. Never use the opacity-weighted temporal distance as an exit.
float
cloudTransportRayEnd(vec3 origin, vec3 ray, float radius, float baseHeight, float thickness)
{
    float limit = 80.0;
    if(length(origin) <= radius + 0.0001 && dot(origin, ray) < 0.0)
        return 0.0;
    vec2 planet = cloudSphere(origin, ray, radius);
    if(cloudForwardHit(planet)) limit = min(limit, planet.x);
    vec2 ranges[2];
    if(!cloudShellIntervals(origin, ray, radius + baseHeight, radius + baseHeight + thickness, limit, ranges))
        return 0.0;
    float end = ranges[0].y > ranges[0].x ? ranges[0].y : 0.0;
    if(ranges[1].y > ranges[1].x) end = max(end, ranges[1].y);
    return end;
}

bool
cloudTransportDepthsCover(float cloudEnd, float surfaceDistance, float opaqueDistance)
{
    // Conservative margin at the shell boundary; invalid/foreground endpoints
    // fall back to integration instead of borrowing clouds behind the surface.
    return min(surfaceDistance, opaqueDistance) >= cloudEnd + max(0.001, cloudEnd * 0.0001);
}

float
cloudAtlasSliceY(float v, float slice, float slices, float resolution)
{
    // Keep bilinear filtering within this slice, never an adjacent atlas tile.
    return (clamp(v, 0.5 / resolution, 1.0 - 0.5 / resolution) + slice) / slices;
}

float
cloudInterpolateOpticalDepth(float depth, float before, float after)
{
    return mix(before, after, fract(depth));
}
#endif
