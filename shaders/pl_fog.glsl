
vec4
fog(vec4 color, vec3 view)
{
    float height = tViewInfo.tData.fFogHeight;
    float fHeightFalloff = -tViewInfo.tData.fFogHeightFalloff;

    float iblLuminance = 1.0;
    float fogStart = tViewInfo.tData.fFogStart;
    float fogCutOffDistance = tViewInfo.tData.fFogCutOffDistance;
    float fogMaxOpacity = tViewInfo.tData.fFogMaxOpacity;
    vec3 fogDensity = tViewInfo.tData.tFogDensity;

    // note: d can be +inf with the skybox
    float d = length(view);

    // early exit for object "in front" of the fog
    if (d < fogStart)
    {
        return color;
    }

    // fogCutOffDistance is set to +inf to disable the cutoff distance
    if (d > fogCutOffDistance)
    {
        return color;
    }

    // height falloff [1/m]
    float falloff = fHeightFalloff;

    // Compute the fog's optical path (unitless) at a distance of 1m at a given height.
    float fogOpticalPathAtOneMeter = fogDensity.z;
    float fh = falloff * view.y;
    if (abs(fh) > 0.00125)
    {
        // The function below is continuous at fh=0, so to avoid a divide-by-zero, we just clamp fh
        fogOpticalPathAtOneMeter = (fogDensity.z - fogDensity.x * exp(fogDensity.y - fh)) / fh;
    }

    // Compute the integral of the fog density at a given height from fogStart to the fragment
    float fogOpticalPath = fogOpticalPathAtOneMeter * max(d - fogStart, 0.0);

    // Compute the transmittance [0,1] using the Beer-Lambert Law
    float fogTransmittance = exp(-fogOpticalPath);

    // Compute the opacity from the transmittance
    float fogOpacity = min(1.0 - fogTransmittance, fogMaxOpacity);

    // compute fog color
    vec3 fogColor = tViewInfo.tData.tFogColor;
    fogColor *= iblLuminance * fogOpacity;
    fogColor *= color.a;
    color.rgb = color.rgb * (1.0 - fogOpacity) + fogColor;
    return color;
}

// A linear approximation of the fog function
vec4
fogLinear(vec4 color, vec3 view)
{
    float fogStart = tViewInfo.tData.fFogStart;
    float fogCutOffDistance = tViewInfo.tData.fFogCutOffDistance;

    // note: d can be +inf with the skybox
    float d = length(view);

    // early exit for object "in front" of the fog
    if (d < fogStart)
    {
        return color;
    }

    // fogCutOffDistance is set to +inf to disable the cutoff distance
    if (d > fogCutOffDistance)
    {
        return color;
    }

    // compute fog color
    float A = tViewInfo.tData.fFogLinearParam0;
    float B = tViewInfo.tData.fFogLinearParam1;
    float fogOpacity = pl_saturate(A * d + B);

    vec3 fogColor = tViewInfo.tData.tFogColor;

    float iblLuminance = 1.0;
    fogColor *= iblLuminance * fogOpacity;
    fogColor *= color.a;
    color.rgb = color.rgb * (1.0 - fogOpacity) + fogColor;
    return color;
}