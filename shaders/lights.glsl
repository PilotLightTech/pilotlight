
struct plLightData
{
    vec3  tPosition;
    float fIntensity;

    vec3  tDirection;
    float fInnerConeCos;

    vec3  tColor;
    float fRange;

    int iShadowIndex;
    int iCascadeCount;
    int iCastShadow;
    float fOuterConeCos;

    int iType;
};

struct plLightShadowData
{
    vec4 cascadeSplits;
	mat4 viewProjMat[6];
    int iShadowMapTexIdx;
    float fFactor;
    float fXOffset;
    float fYOffset;
};

float
getRangeAttenuation(float range2, float dist2)
{
    if (range2 <= 0.0)
    {
        // negative range means unlimited
        return 1.0 / dist2;
    }
    float dist_per_range = dist2 / range2;
    dist_per_range *= dist_per_range;
    return max(min(1.0 - dist_per_range, 1.0), 0.0) / dist2;
}

// https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_lights_punctual/README.md#inner-and-outer-cone-angles
float getSpotAttenuation(vec3 pointToLight, vec3 spotDirection, float outerConeCos, float innerConeCos)
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
getLightIntensity(plLightData light, vec3 pointToLight)
{
    float rangeAttenuation = 1.0;
    float spotAttenuation = 1.0;

    if (light.iType != 0)
    {
        rangeAttenuation = getRangeAttenuation(light.fRange * light.fRange, pointToLight.x * pointToLight.x + pointToLight.y * pointToLight.y + pointToLight.z * pointToLight.z);
    }
    if (light.iType == 2)
    {
        spotAttenuation = getSpotAttenuation(pointToLight, light.tDirection, light.fOuterConeCos, light.fInnerConeCos);
    }

    return rangeAttenuation * spotAttenuation * light.fIntensity * light.tColor;
}