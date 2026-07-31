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
// [SECTION] helpers
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
#include "pl_noise.inc"
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
// [SECTION] helpers
//-----------------------------------------------------------------------------

vec3
ditherRGB8(vec3 c, ivec2 uv, float g_time)
{
    vec3 noise = hash32(uvec2(uv * g_time));
    noise += hash32(uvec2((uv + vec2(165, 1292))*g_time));
    noise -= 1.f;
    noise /= 255.f; //least significant of 8 bits
    return c + noise;
}

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

    ivec2 lutSize = textureSize(sampler2D(transmissionLut, tSamplerNearestClamp), 0);

    // The bound sampler repeats, but keeping the coordinates inside the
    // texel-center range prevents filtering across either LUT boundary.
    uv = pl_clamp_lut_uv(uv, lutSize);

    return textureLod(sampler2D(transmissionLut, tSamplerNearestClamp), uv, 0.0).rgb;
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
    float viewSunCos = dot(normalize(viewDirection), normalize(sunDirection));
    float radiusCos = cos(angularRadius);

    // antialias the very small cosine-space edge
    float edgeWidth = max(fwidth(viewSunCos) * 1.5, 1e-7);
    return smoothstep(radiusCos - edgeWidth, radiusCos + edgeWidth, viewSunCos);
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

    float viewHeight = planetRadius + cameraAltitude;

    // reconstruct view direction
    //-----------------------------------------------------------------------------

    vec3 worldViewDirection = getPerspectiveSkyDirection(inUV);

    // World space uses positive Y as up. The atmosphere model places the
    // camera on the negative Y axis, so atmosphere-local up is negative Y.
    vec3 atmosphereViewDirection = normalize(vec3(worldViewDirection.x, worldViewDirection.y, worldViewDirection.z));
    // atmosphereViewDirection.y *= -1.0;

    // sample atmospheric sky
    vec3 color = pl__sample_sky_lut(atmosphereViewDirection, viewHeight, planetRadius);

    // add attenuated Sun disk
    //-----------------------------------------------------------------------------

    vec3 atmosphereSunDirection = -normalize(tGpuScene.tData.tDirection);
    // atmosphereSunDirection.y *= -1.0;

    float sunAngularRadius = tGpuScene.tData.fSunRadius;

    float disk = pl_sun_disk(atmosphereViewDirection, atmosphereSunDirection, sunAngularRadius);

    vec3 atmosphereCameraPosition = vec3(0.0, viewHeight, 0.0);

    // hide the Sun when the planet blocks this pixel's ray.
    float planetHit = pl_final_ray_sphere_nearest(atmosphereCameraPosition, atmosphereViewDirection, planetRadius);

    if(planetHit >= 0.0)
        disk = 0.0;

    if(disk > 0.0)
    {
        // Use the pixel direction rather than only the center direction.
        // This permits differential attenuation across the solar disk near
        // the horizon.
        // vec3 transmissionToSun = pl__sample_sun_transmission(atmosphereCameraPosition, atmosphereViewDirection, planetRadius, atmosphereHeight);

        vec3 sunRadiance = tGpuScene.tData.tColor* tGpuScene.tData.tAerialInfo.w;
        // color += disk * sunRadiance * transmissionToSun;
        color += disk * sunRadiance;
    }

    // final output
    //-----------------------------------------------------------------------------

    outColor = vec4(color, 1.0);
    outColor.rgb = ditherRGB8(outColor.rgb, ivec2(gl_FragCoord.xy), tGpuScene.tData.g_time);
}