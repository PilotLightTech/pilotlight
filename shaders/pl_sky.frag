#version 450
#extension GL_ARB_separate_shader_objects : enable

/*
   sky.frag
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] bind groups
// [SECTION] input/output
// [SECTION] camera ray
// [SECTION] sky-view LUT mapping
// [SECTION] transmission LUT
// [SECTION] Sun disk
// [SECTION] entry
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_sky.inc"
#include "pl_sky_night.glsl"
#include "pl_bg_scene.inc"
#include "pl_bg_view.inc"

//-----------------------------------------------------------------------------
// [SECTION] bind groups
//-----------------------------------------------------------------------------

// bind group 0 (global)
// layout(set = 0, binding = 0) readonly uniform _plAtmosphereSettings { plAtmosphereSettings tData; } tAtmosphereSettings;
// layout(set = 0, binding = 1)  uniform sampler tSamplerLinearClamp;
// layout(set = 0, binding = 2)  uniform sampler tSamplerNearestClamp;

// bind group 2
layout(set = 2, binding = 0) uniform texture2D skyLut;
layout(set = 2, binding = 1) uniform texture2D transmissionLut;

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    uint uCameraIndex;
} tObjectInfo;

//-----------------------------------------------------------------------------
// [SECTION] input/output
//-----------------------------------------------------------------------------

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

//-----------------------------------------------------------------------------
// [SECTION] camera ray
//-----------------------------------------------------------------------------

vec3
getPerspectiveSkyDirection(vec2 uv)
{
    vec2 ndc = uv * 2.0 - 1.0;

    // The inverse projection diagonal contains the signed tangent of the
    // horizontal and vertical half-FOV for a symmetric perspective camera.
    vec3 viewDirection = normalize(vec3(
        ndc.x * (tViewInfo.tData.tCameraProjectionInv[tObjectInfo.uCameraIndex])[0][0],
        ndc.y * (tViewInfo.tData.tCameraProjectionInv[tObjectInfo.uCameraIndex])[1][1],
        1.0
    ));

    return normalize(mat3(tViewInfo.tData.tInvViewMatNoTranslation[tObjectInfo.uCameraIndex]) * viewDirection);
}

//-----------------------------------------------------------------------------
// [SECTION] sky-view LUT mapping
//-----------------------------------------------------------------------------

vec3
pl__sample_sky_lut(vec3 direction, float viewHeight, float planetRadius)
{
    ivec2 lutSize = textureSize(sampler2D(skyLut, tSamplerLinearClamp), 0);
    vec2 uv = pl_to_sky_lut(direction, lutSize, viewHeight, planetRadius);
    return textureLod(sampler2D(skyLut, tSamplerLinearClamp), uv, 0.0).rgb;
}

//-----------------------------------------------------------------------------
// [SECTION] transmission LUT
//-----------------------------------------------------------------------------

vec3
pl__sample_transmission_at_pos(vec3 atmospherePosition, vec3 rayDirection, float planetRadius, float atmosphereHeight)
{
    float radialDistance = length(atmospherePosition);

    vec3 localUp = atmospherePosition / max(radialDistance, 1e-20);

    float altitude = clamp(radialDistance - planetRadius, 0.0, atmosphereHeight);

    vec2 uv = pl_compute_lut_uv(altitude, atmosphereHeight, localUp, normalize(rayDirection));

    ivec2 lutSize = textureSize(sampler2D(transmissionLut, tSamplerLinearClamp), 0);

    // The LUT generator places altitude/direction endpoints at texel centers.
    // Linear filtering avoids visible steps as the small disk crosses rows.
    uv = vec2(skyLutUnitToSubUv(uv.x, float(lutSize.x)), skyLutUnitToSubUv(uv.y, float(lutSize.y)));

    return textureLod(sampler2D(transmissionLut, tSamplerLinearClamp), uv, 0.0).rgb;
}

vec3
pl__sample_sun_transmission(vec3 atmosphereCameraPosition, vec3 rayDirection, float planetRadius, float atmosphereHeight)
{
    rayDirection = normalize(rayDirection);

    float atmosphereRadius = planetRadius + atmosphereHeight;

    float cameraRadius = length(atmosphereCameraPosition);

    vec3 samplePosition;

    if(cameraRadius <= atmosphereRadius)
    {
        // Camera is already inside the atmosphere.
        samplePosition = atmosphereCameraPosition;
    }
    else
    {
        // Camera is above the atmosphere. Determine whether this ray enters
        // the atmosphere before reaching the Sun.
        float atmosphereEntry = pl_final_ray_sphere_nearest(atmosphereCameraPosition, rayDirection, atmosphereRadius);

        // This ray never touches the atmosphere.
        if(atmosphereEntry < 0.0)
            return vec3(1.0);

        // Move slightly inside the atmosphere sphere to avoid evaluating
        // exactly on its numerical boundary.
        float entryBias = max(atmosphereHeight * 1e-5, 1e-6);

        samplePosition = atmosphereCameraPosition + rayDirection * (atmosphereEntry + entryBias);
    }

    return pl__sample_transmission_at_pos(samplePosition, rayDirection, planetRadius, atmosphereHeight);
}

//-----------------------------------------------------------------------------
// [SECTION] Sun disk
//-----------------------------------------------------------------------------

float
pl_sun_disk(vec3 viewDirection, vec3 sunDirection, float angularRadius)
{
    if(angularRadius <= 0.0)
        return 0.0;

    viewDirection = normalize(viewDirection);
    sunDirection = normalize(sunDirection);

    // Chord distance retains precision for a small disk, unlike a cosine
    // comparison very close to 1. Keep the silhouette about one pixel wide.
    float diskRadius = 2.0 * sin(0.5 * angularRadius);
    float diskDistance = length(viewDirection - sunDirection);
    float edgeWidth = max(fwidth(diskDistance), 1e-7);
    float coverage = 1.0 - smoothstep(diskRadius - 0.5 * edgeWidth, diskRadius + 0.5 * edgeWidth, diskDistance);

    // Approximate visible-light limb darkening: the photosphere is brighter
    // at the center. Normalize the profile to preserve the existing average
    // disk intensity (the small-angle disk average of mu is 2/3).
    float radialFraction = length(cross(viewDirection, sunDirection)) / max(sin(angularRadius), 1e-7);
    float mu = sqrt(max(1.0 - radialFraction * radialFraction, 0.0));
    float limbDarkening = (1.0 - 0.6 * (1.0 - mu)) / (1.0 - 0.6 / 3.0);
    return coverage * limbDarkening;
}

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void main()
{
    float planetRadius = tGpuScene.tData.planetRadius;
    float atmosphereHeight = tGpuScene.tData.atmosphereHeight;
    float worldToAtmosphereScale = tGpuScene.tData.fAtmosphereConversion;

    float cameraAltitude = max(tViewInfo.tData.tCameraPos.y * worldToAtmosphereScale, 0.0);

    // Match the sky-view LUT origin and avoid a zero-distance planet hit
    // when the camera is exactly at sea level.
    float cameraSurfaceOffset = max(planetRadius * 1e-7, 1e-6);
    float viewHeight = planetRadius + cameraAltitude + cameraSurfaceOffset;

    // reconstruct view direction
    //-----------------------------------------------------------------------------

    vec3 worldViewDirection = getPerspectiveSkyDirection(inUV);

    // Both world space and the atmosphere's camera origin use positive Y up.
    vec3 atmosphereViewDirection = worldViewDirection;

    // sample atmospheric sky
    vec3 color = pl__sample_sky_lut(atmosphereViewDirection, viewHeight, planetRadius);

    // add attenuated Sun disk
    //-----------------------------------------------------------------------------

    vec3 atmosphereSunDirection = -normalize(tGpuScene.tData.tSunDirection.xyz);

    float sunAngularRadius = tGpuScene.tData.fSunRadius;

    float disk = pl_sun_disk(atmosphereViewDirection, atmosphereSunDirection, sunAngularRadius);

    vec3 atmosphereCameraPosition = vec3(0.0, viewHeight, 0.0);

    // Clip against the geometric horizon, including its dip at altitude.
    // Smooth the horizon by one pixel so a setting Sun does not stair-step.
    float horizonCos = -sqrt(max((viewHeight - planetRadius) * (viewHeight + planetRadius), 0.0)) / viewHeight;
    float horizonWidth = max(fwidth(atmosphereViewDirection.y), 1e-7);
    float horizonVisibility = smoothstep(horizonCos - 0.5 * horizonWidth, horizonCos + 0.5 * horizonWidth, atmosphereViewDirection.y);
    disk *= horizonVisibility;

    // Derivatives must be evaluated before the per-pixel visibility branches.
    float pixelWidth = max(max(length(dFdx(atmosphereViewDirection)), length(dFdy(atmosphereViewDirection))), 1e-6);
    if((tGpuScene.tData.tNightSky.x > 0.0 || tGpuScene.tData.tNightSky.z > 0.0) && horizonVisibility > 0.0)
    {
        vec3 transmission = pl__sample_sun_transmission(atmosphereCameraPosition, atmosphereViewDirection, planetRadius, atmosphereHeight);
        // Bright stars begin to appear during twilight; the full field emerges
        // by astronomical night. The existing atmosphere LUT supplies twilight.
        float night = 1.0 - smoothstep(sin(radians(-18.0)), sin(radians(-4.0)), atmosphereSunDirection.y);
        vec3 moonDirection = normalize(tGpuScene.tData.tMoonDirection.xyz);
        float moonFraction = tGpuScene.tData.tMoonDirection.w;
        float moonIntensity = tGpuScene.tData.tNightSky.z;
        vec4 moon = vec4(0.0);
        if(moonIntensity > 0.0)
            moon = pl_night_moon(atmosphereViewDirection, moonDirection, atmosphereSunDirection, tGpuScene.tData.tNightSky.w, pixelWidth, moonFraction);

        // Moonlight scattering now comes from the sky LUT. Retain the faint
        // airglow floor and use the visual moon only for star-contrast suppression.
        float moonAboveHorizon = smoothstep(horizonCos, horizonCos + 0.05, moonDirection.y);
        float moonlight = moonIntensity * moonFraction * moonFraction * moonAboveHorizon;
        float moonHaze = exp(-6.0 * length(atmosphereViewDirection - moonDirection)) * moonlight;
        vec3 nightBackground = night * vec3(0.00015, 0.00022, 0.00035);
        vec3 stars = vec3(0.0);
        if(night > 0.0 && tGpuScene.tData.tNightSky.x > 0.0)
        {
            float skyLuminance = dot(max(color, vec3(0.0)), vec3(0.2126, 0.7152, 0.0722));
            float visibility = night / (1.0 + 80.0 * skyLuminance + 2.0 * moonlight + 12.0 * moonHaze);
            stars = pl_night_stars(atmosphereViewDirection, pixelWidth, tGpuScene.tData.tNightSky.y) * tGpuScene.tData.tNightSky.x * visibility;
        }
        // The complete lunar silhouette occludes stars, including its dark side.
        // Foreground atmosphere stays visible; clouds composite over this pass.
        color += horizonVisibility * (nightBackground + transmission * (stars * (1.0 - moon.a) + moon.rgb * moonIntensity));
    }

    if(disk > 0.0)
    {
        // Use the pixel direction rather than only the center direction.
        // This permits differential attenuation across the solar disk near
        // the horizon.
        vec3 transmissionToSun = pl__sample_sun_transmission(atmosphereCameraPosition, atmosphereViewDirection, planetRadius, atmosphereHeight);

        vec3 sunRadiance = tGpuScene.tData.tSunRadiance.rgb;
        color += disk * sunRadiance * transmissionToSun;
    }

    // final output
    //-----------------------------------------------------------------------------

    // Display-space dithering already happens after tonemapping. Adding 8-bit
    // noise here would overwhelm faint stars and introduce negative HDR values.
    outColor = vec4(max(color, vec3(0.0)), 1.0);
}
