#ifndef PL_CLOUD_MATH_GLSL
#define PL_CLOUD_MATH_GLSL

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

float
cloudSampleBoundary(float index, float count, float distance2)
{
    // Count may be fractional. A new final segment starts with zero length,
    // instead of moving every sample when an integer threshold is crossed.
    float u = min(index / count, 1.0);
    return distance2 * u * u;
}

float
cloudSampleCount(float minimumCount, float distance2, float frequency, float maximumCount)
{
    return clamp(max(minimumCount, distance2 * frequency * 8.0), 16.0, maximumCount);
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
