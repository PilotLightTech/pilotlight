


//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

// temp
#include "pl_math.glsl"
#include "pl_shader_interop_renderer.h"

// Converts a texel-center UV into a unit UV whose first and last texel
// centers represent exactly 0 and 1.
float
skyLutSubUvToUnit(float subUv, float resolution)
{
    if(resolution <= 1.0)
        return 0.0;

    return clamp((subUv * resolution - 0.5) / (resolution - 1.0), 0.0, 1.0);
}

// Converts a unit UV back into the valid texel-center range.
float
skyLutUnitToSubUv(float unitUv, float resolution)
{
    if(resolution <= 1.0)
        return 0.5;

    return (clamp(unitUv, 0.0, 1.0) * (resolution - 1.0) + 0.5) / resolution;
}

// viewHeight is the camera's distance from the planet center:
//
//     viewHeight = planetRadius + cameraAltitude
//
// The camera is assumed to be positioned on the negative Y axis, making
// vec3(0, -1, 0) the local upward direction.
vec3
fromSkyLut(vec2 subUv, ivec2 lutSize, float viewHeight, float planetRadius)
{
    vec2 uv = vec2(skyLutSubUvToUnit(subUv.x, float(lutSize.x)), skyLutSubUvToUnit(subUv.y, float(lutSize.y)));

    viewHeight = max(viewHeight, planetRadius);

    // Geometry of the tangent ray from the camera to the planet.
    float horizonDistance = sqrt(max(viewHeight * viewHeight - planetRadius * planetRadius, 0.0));

    float cosBeta = clamp(horizonDistance / max(viewHeight, 1e-6), 0.0, 1.0);

    // Beta is the angle between the tangent ray and the inward radial
    // direction. At ground level it is pi/2.
    float beta = acos(cosBeta);

    // Angle measured from local up to the geometric horizon.
    //
    // Ground level: pi/2
    // Above ground: greater than pi/2
    float zenithToHorizonAngle = PL_PI - beta;

    float viewZenithAngle;

    if(uv.y < 0.5)
    {
        // Upper, non-ground-intersecting half.
        //
        // uv.y = 0.0 -> zenith
        // uv.y = 0.5 -> geometric horizon
        float coord = uv.y * 2.0;

        coord = 1.0 - coord;
        coord *= coord;
        coord = 1.0 - coord;

        viewZenithAngle = zenithToHorizonAngle * coord;
    }
    else
    {
        // Lower, ground-intersecting half.
        //
        // uv.y = 0.5 -> geometric horizon
        // uv.y = 1.0 -> nadir
        float coord = uv.y * 2.0 - 1.0;
        coord *= coord;

        viewZenithAngle = zenithToHorizonAngle + beta * coord;
    }

    // Preserve your existing full 360-degree azimuth mapping.
    float phi = (0.5 - uv.x) * 2.0 * PL_PI;

    float sinZenith = sin(viewZenithAngle);
    float cosZenith = cos(viewZenithAngle);

    // Local up is -Y because the camera is at (0, -viewHeight, 0).
    return vec3(sinZenith * cos(phi), cosZenith, sinZenith * sin(phi));
}

//-----------------------------------------------------------------------------
// [SECTION] phase functions
//-----------------------------------------------------------------------------

// scatter direction is determined based on the phase function

float
pl_phase_cornette_shanks(float VoL, float g)
{
    float nominator = 3.0 / (8.0 * PL_PI) * (1.0 - g * g) * (1.0 + VoL * VoL);
    float denominator = (2.0 + g * g) * pow(1.0 + g * g - 2.0 * g * VoL, 1.5);
    return nominator / denominator;
}

float
pl_phase_rayleigh(float VoL)
{
    return 3.0 / (16.0 * PL_PI) * (1.0 + VoL * VoL);
}

float
pl_phase_greenstein(float VoL, float g)
{
    return (1.0 - g * g) / (4.0 * PL_PI * pow(1.0 + g * g - 2.0 * g * VoL, 1.0));
}

//approximates greenstein, has sign error
float
pl_phase_schlick(float VoL, float g)
{
    float k = 1.55 * g - 0.55 * g * g * g;
    return (1.0 - k * k) / (4.0 * PL_PI * pow(1.0 + k * VoL, 2.0));
}

//-----------------------------------------------------------------------------
// [SECTION] intersection functions
//-----------------------------------------------------------------------------

struct plIntersection
{
    vec3 tPos;
    float fDistance;
    bool bHitEarth;
};

plIntersection
pl_ray_intersection_planet(vec3 P, vec3 D, vec3 C, float planetRadius, float atmosphere)
{
    vec3 L = C - P;
    float t_ca = dot(L, D);
    float d = sqrt(dot(L, L) - t_ca * t_ca);

    // planet intersection
    float t_hc_planet = sqrt(planetRadius * planetRadius - d * d);
    float t_planet = t_ca - t_hc_planet; //from outside, so uses different sign

    // atmosphere intersection
    float r = planetRadius + atmosphere;
    float t_hc_atmosphere = sqrt(r * r - d * d);
    float t_atmosphere = t_ca + abs(t_hc_atmosphere);

    // if ray intersect with planet in front then view is blocked by planet
    plIntersection tResult;
    tResult.bHitEarth = t_planet >= 0;
    float t = tResult.bHitEarth ? t_planet : t_atmosphere; 
    tResult.fDistance = t;
    tResult.tPos = P + t * D;
    return tResult;
}

float
pl_ray_intersect_sphere_nearest(vec3 tOrigin, vec3 tDirection, vec3 tCenter, float fRadius)
{

    // returns the nearest nonnegative ray/sphere intersection, or -1.0 when the
    // ray misses the sphere.

    vec3 tRelativeOrigin = tOrigin - tCenter;

    float a = dot(tDirection, tDirection);
    float b = 2.0 * dot(tDirection, tRelativeOrigin);
    float c = dot(tRelativeOrigin, tRelativeOrigin) - fRadius * fRadius;

    float fDiscriminant = b * b - 4.0 * a * c;

    if(fDiscriminant < 0.0 || a <= 0.0)
        return -1.0;

    float fRoot = sqrt(max(fDiscriminant, 0.0));
    float fInverseDenominator = 0.5 / a;

    float t0 = (-b - fRoot) * fInverseDenominator;
    float t1 = (-b + fRoot) * fInverseDenominator;

    if(t0 < 0.0 && t1 < 0.0)
        return -1.0;

    if(t0 < 0.0)
        return max(t1, 0.0);

    if(t1 < 0.0)
        return max(t0, 0.0);

    return max(min(t0, t1), 0.0);
}

vec3
pl_safe_normalize(vec3 tValue)
{
    float fLengthSquared = dot(tValue, tValue);
    return tValue * inversesqrt(max(fLengthSquared, 1e-20));
}

float
pl_planet_visibility(vec3 tPosition, vec3 tDirectionToSun, vec3 tEarthCenter, float fEarthRadius)
{

    // returns 1 when the sun is visible and 0 when the planet blocks it.

    vec3 tRadialDirection = pl_safe_normalize(tPosition - tEarthCenter);

    // Scale-aware offset. This is roughly sub-metre to metre scale for an
    // Earth-sized planet, regardless of whether radii use metres or km.
    float fSurfaceEpsilon = max(fEarthRadius * 1e-7, 1e-6);

    vec3 tBiasedPosition = tPosition + tRadialDirection * fSurfaceEpsilon;

    float bEarthHit = pl_ray_intersect_sphere_nearest(tBiasedPosition, tDirectionToSun,
        tEarthCenter, fEarthRadius);

    return bEarthHit >= 0.0 ? 0.0 : 1.0;
}

float
pl_final_ray_sphere_nearest(vec3 rayOrigin, vec3 rayDirection, float sphereRadius)
{
    float b = dot(rayOrigin, rayDirection);

    float c = dot(rayOrigin, rayOrigin) - sphereRadius * sphereRadius;

    float discriminant = b * b - c;

    if(discriminant < 0.0)
        return -1.0;

    float root =
        sqrt(max(discriminant, 0.0));

    float t0 = -b - root;
    float t1 = -b + root;

    if(t0 >= 0.0)
        return t0;

    if(t1 >= 0.0)
        return t1;

    return -1.0;
}

//-----------------------------------------------------------------------------
// [SECTION] misc functions
//-----------------------------------------------------------------------------

vec2
pl_clamp_lut_uv(vec2 tUv, ivec2 tLutSize)
{
    // Keeps bilinear samples inside actual texel centers instead of allowing
    // filtering against an exact LUT boundary.

    vec2 tHalfTexel = 0.5 / vec2(tLutSize);
    return clamp(tUv, tHalfTexel, vec2(1.0) - tHalfTexel);
}

struct plAtmosphereCoefficients
{
    vec3 tScatterRayleigh;
    vec3 tScatterMie;
    vec3 tExtinction;
};

float
pl_height_factor_rayleigh(float fHeight)
{
    return exp(-fHeight * (1.0 / 8.0));
}

float
pl_height_factor_mie(float fHeight)
{
    return exp(-fHeight * (1.0 / 1.2));
}

float
pl_height_factor_ozone(float fHeight)
{
    return max(0.0, 1.0 - abs(fHeight - 25.0) / 15.0);
}

plAtmosphereCoefficients
pl_calculate_coefficients(float fHeight, plGpuSceneData tAtmosphere)
{
    float fRayleighFactor = pl_height_factor_rayleigh(fHeight);
    float fMieFactor = pl_height_factor_mie(fHeight);
    float fOzoneFactor = pl_height_factor_ozone(fHeight);
    
    plAtmosphereCoefficients coefficients;
    coefficients.tScatterRayleigh    = fRayleighFactor    * tAtmosphere.scatteringRayleighGround;
    coefficients.tScatterMie         = vec3(fMieFactor)   * tAtmosphere.scatteringMieGround;
    coefficients.tExtinction = 
        fRayleighFactor * tAtmosphere.extinctionRayleighGround + 
        fMieFactor      * tAtmosphere.extinctionMieGround + 
        fOzoneFactor    * tAtmosphere.ozoneExtinction;

    return coefficients;
}

vec2
pl_compute_lut_uv(float height, float atmoshphereHeight, vec3 up, vec3 direction)
{
    vec2 lutUV;
    lutUV.x = height / atmoshphereHeight;
    lutUV.y = dot(up, direction) * 0.5 + 0.5;
    return lutUV;
}

vec3
pl_integrate_inscattering(vec3 inscattering, vec3 extinctionCoefficients, float length)
{
    // reference: "Physically Based and Unified Volumetric Rendering in Frostbite", page 29
    return (inscattering - inscattering * exp(-extinctionCoefficients * length)) / max(extinctionCoefficients, 0.00001f);
}

vec2
pl_to_sky_lut(vec3 direction,ivec2 lutSize, float viewHeight, float planetRadius)
{
    direction = normalize(direction);
    viewHeight = max(viewHeight, planetRadius);

    float horizonDistance = sqrt(max(viewHeight * viewHeight - planetRadius * planetRadius, 0.0));

    float cosBeta = clamp(horizonDistance / max(viewHeight, 1e-6), 0.0, 1.0);

    float beta = acos(cosBeta);

    // Zenith angle at which the geometric horizon occurs.
    float zenithToHorizonAngle = PL_PI - beta;

    // Atmosphere-local up is negative Y.
    float viewZenithAngle = acos(clamp(direction.y, -1.0, 1.0));

    vec2 uv;

    if(viewZenithAngle <= zenithToHorizonAngle)
    {
        // Zenith to geometric horizon.
        float coord = viewZenithAngle / max(zenithToHorizonAngle, 1e-6);

        coord = 1.0 - coord;
        coord = sqrt(max(coord, 0.0));
        coord = 1.0 - coord;

        uv.y = coord * 0.5;
    }
    else
    {
        // Geometric horizon to nadir.
        float coord = (viewZenithAngle - zenithToHorizonAngle) / max(beta, 1e-6);

        coord = sqrt(clamp(coord, 0.0, 1.0));
        uv.y = 0.5 + coord * 0.5;
    }

    float phi = atan(direction.z, direction.x);

    uv.x = fract(0.5 - phi / (2.0 * PL_PI));

    return vec2( skyLutUnitToSubUv(uv.x, float(lutSize.x)), skyLutUnitToSubUv(uv.y, float(lutSize.y)));
}