/* Procedural night-sky appearance. No catalogue, textures, or ephemeris required. */
#ifndef PL_SKY_NIGHT_GLSL
#define PL_SKY_NIGHT_GLSL

vec4
pl_night_hash(uvec2 cell)
{
    uvec4 h = cell.x * uvec4(1597334677u, 3812015801u, 2798796415u, 1979697957u) + cell.y * uvec4(2246822519u, 3266489917u, 668265263u, 374761393u);
    h ^= h >> 16u;
    h *= 2246822519u;
    h ^= h >> 13u;
    h *= 3266489917u;
    h ^= h >> 16u;
    return vec4(h & 0x00ffffffu) / 16777216.0;
}

float
pl_night_noise(vec3 p)
{
    vec3 cell = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    // Nonnegative coordinates keep float-to-uint conversion well defined.
    uvec3 c = uvec3(cell + 1024.0);
    vec4 a = pl_night_hash(uvec2(c.x + c.z * 157u, c.y));
    vec4 b = pl_night_hash(uvec2(c.x + (c.z + 1u) * 157u, c.y));
    vec4 d = pl_night_hash(uvec2(c.x + c.z * 157u, c.y + 1u));
    vec4 e = pl_night_hash(uvec2(c.x + (c.z + 1u) * 157u, c.y + 1u));
    // Adjacent x corners need the same hash as the next cell's origin.
    float ax = pl_night_hash(uvec2(c.x + 1u + c.z * 157u, c.y)).x;
    float bx = pl_night_hash(uvec2(c.x + 1u + (c.z + 1u) * 157u, c.y)).x;
    float dx = pl_night_hash(uvec2(c.x + 1u + c.z * 157u, c.y + 1u)).x;
    float ex = pl_night_hash(uvec2(c.x + 1u + (c.z + 1u) * 157u, c.y + 1u)).x;
    return mix(mix(mix(a.x, ax, f.x), mix(d.x, dx, f.x), f.y), mix(mix(b.x, bx, f.x), mix(e.x, ex, f.x), f.y), f.z);
}

vec3
pl_night_stars(vec3 direction, float pixelWidth, float rotation)
{
    // Latitude rings with fewer cells near the poles give approximately
    // uniform density. Wrapped columns avoid a longitude/cubemap seam.
    const int rowCount = 128;
    float theta = acos(clamp(direction.y, -1.0, 1.0));
    float longitude = (dot(direction.xz, direction.xz) > 1e-12 ? atan(direction.z, direction.x) : 0.0) + rotation;
    float longitudeUnit = fract(longitude / (2.0 * PL_PI));
    int centerRow = min(int(theta * float(rowCount) / PL_PI), rowCount - 1);
    vec3 result = vec3(0.0);
    for(int rowOffset = -1; rowOffset <= 1; rowOffset++)
    {
        int row = centerRow + rowOffset;
        if(row < 0 || row >= rowCount)
            continue;
        float rowTheta = (float(row) + 0.5) * PL_PI / float(rowCount);
        int columns = max(3, int(2.0 * float(rowCount) * sin(rowTheta)));
        int centerColumn = min(int(longitudeUnit * float(columns)), columns - 1);
        for(int columnOffset = -1; columnOffset <= 1; columnOffset++)
        {
            // GLSL integer remainder is undefined for negative operands.
            // Wrap longitude first and keep both operands nonnegative.
            int column = (centerColumn + columnOffset + columns) % columns;
            vec4 random = pl_night_hash(uvec2(column, row));
            if(random.x > 0.12)
                continue;
            float starTheta = (float(row) + mix(0.15, 0.85, random.y)) * PL_PI / float(rowCount);
            float starLongitude = (float(column) + mix(0.15, 0.85, random.z)) * (2.0 * PL_PI) / float(columns) - rotation;
            vec3 starDirection = vec3(sin(starTheta) * cos(starLongitude), cos(starTheta), sin(starTheta) * sin(starLongitude));
            vec3 delta = direction - starDirection;
            float radius = mix(0.00013, 0.00033, random.w);
            float width = max(radius, pixelWidth * 0.65);
            // Pixel filtering preserves integrated brightness when a star is
            // smaller than a pixel, rather than letting it blink on and off.
            float profile = exp(-0.5 * dot(delta, delta) / (width * width)) * (radius * radius) / (width * width);
            float brightness = 4.0 * mix(0.15, 6.0, pow(random.w, 10.0));
            vec3 tint = mix(vec3(1.0, 0.72, 0.48), vec3(0.68, 0.82, 1.0), random.y);
            result += profile * brightness * tint;
        }
    }
    return result;
}

vec4
pl_night_moon(vec3 direction, vec3 moonDirection, vec3 sunDirection, float radius, float pixelWidth, float illuminatedFraction)
{
    float distanceToCenter = length(direction - moonDirection);
    float chordRadius = 2.0 * sin(0.5 * radius);
    float coverage = 1.0 - smoothstep(chordRadius - pixelWidth * 0.5, chordRadius + pixelWidth * 0.5, distanceToCenter);
    if(coverage <= 0.0)
        return vec4(0.0);

    vec3 referenceUp = abs(moonDirection.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 right = normalize(cross(moonDirection, referenceUp));
    vec3 up = cross(right, moonDirection);
    vec2 xy = vec2(dot(direction, right), dot(direction, up)) / max(sin(radius), 1e-6);
    xy *= inversesqrt(max(dot(xy, xy), 1.0));
    float z = sqrt(max(1.0 - dot(xy, xy), 0.0));
    vec3 normal = right * xy.x + up * xy.y - moonDirection * z;
    float mu = max(dot(normal, sunDirection), 0.0);
    // Approximate lunar regolith with a Lommel-Seeliger/Lambert mixture.
    // Geometry determines the terminator, including waxing and waning sides.
    float lit = mix(mu, 2.0 * mu / max(mu + z, 1e-5), 0.75);
    float earthshine = 0.012 * pow(1.0 - illuminatedFraction, 2.0);

    vec3 surface = vec3(xy, z);
    float terrain = 0.6 * pl_night_noise(surface * 3.0 + 17.0) + 0.3 * pl_night_noise(surface * 8.0 + 41.0) + 0.1 * pl_night_noise(surface * 23.0 + 73.0);
    float albedo = mix(0.48, 1.0, smoothstep(0.30, 0.66, terrain));
    // A few shallow crater rims, kept subtle at the normal angular size.
    vec2 craterUV = xy * 7.0 + 20.0;
    vec2 craterCell = floor(craterUV);
    for(int y = -1; y <= 1; y++)
    {
        for(int x = -1; x <= 1; x++)
        {
            vec2 cell = craterCell + vec2(x, y);
            vec4 random = pl_night_hash(uvec2(cell));
            vec2 center = cell + mix(vec2(0.2), vec2(0.8), random.xy);
            float craterRadius = mix(0.10, 0.28, random.z);
            float d = length(craterUV - center) / craterRadius;
            // The rim offset is negative inside the crater. GLSL pow is
            // undefined for negative bases (and becomes powr on Metal).
            float rimOffset = (d - 1.0) * 8.0;
            albedo += 0.07 * exp(-rimOffset * rimOffset) - 0.06 * exp(-d * d * 3.0);
        }
    }
    return vec4(vec3(1.0, 0.97, 0.92) * albedo * (lit + earthshine) * coverage, coverage);
}

#endif // PL_SKY_NIGHT_GLSL
