#ifndef PL_LIGHTS_GLSL
#define PL_LIGHTS_GLSL

#include "pl_shader_interop_renderer.h"

float
getRangeAttenuation(float range, float distance)
{
    if (range <= 0.0)
    {
        // negative range means unlimited
        return 1.0 / pow(distance, 2.0);
    }
    return max(min(1.0 - pow(distance / range, 4.0), 1.0), 0.0) / pow(distance, 2.0);
}

// https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_lights_punctual/README.md#inner-and-outer-cone-angles
float
getSpotAttenuation(vec3 pointToLight, vec3 spotDirection, float outerConeCos, float innerConeCos)
{
    float actualCos = dot(normalize(spotDirection), normalize(-pointToLight));
    if (actualCos > outerConeCos)
    {
        if (actualCos < innerConeCos)
        {
            float angularAttenuation = (actualCos - outerConeCos) / (innerConeCos - outerConeCos);
            return angularAttenuation * angularAttenuation;
        }
        return 1.0;
    }
    return 0.0;
}

vec3
getLightIntensity(plGpuLight light, vec3 pointToLight)
{
    float rangeAttenuation = 1.0;
    float spotAttenuation = 1.0;

    if (light.iType != PL_LIGHT_TYPE_DIRECTIONAL)
    {
        rangeAttenuation = getRangeAttenuation(light.fRange, length(pointToLight));
    }
    if (light.iType == PL_LIGHT_TYPE_SPOT)
    {
        spotAttenuation = getSpotAttenuation(pointToLight, light.tDirection, light.fOuterConeCos, light.fInnerConeCos);
    }

    return rangeAttenuation * spotAttenuation * light.fIntensity * light.tColor;
}

#endif // PL_LIGHTS_GLSL