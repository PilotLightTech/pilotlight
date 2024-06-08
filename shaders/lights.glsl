
struct plLightData
{
    vec3  tPosition;
    float fIntensity;

    vec3  tDirection;
    int   iType;

    vec3  tColor;
    float fRange;

    int iShadowIndex;
    int iCascadeCount;
};

struct plLightShadowData
{
	vec4 cascadeSplits;
	mat4 cascadeViewProjMat[4];
};

float
getRangeAttenuation(float range, float dist)
{
    if (range <= 0.0)
    {
        // negative range means unlimited
        return 1.0 / pow(dist, 2.0);
    }
    return max(min(1.0 - pow(dist / range, 4.0), 1.0), 0.0) / pow(dist, 2.0);
}

vec3
getLighIntensity(plLightData light, vec3 pointToLight)
{
    float rangeAttenuation = 1.0;

    if (light.iType != PL_LIGHT_TYPE_DIRECTIONAL)
    {
        rangeAttenuation = getRangeAttenuation(light.fRange, length(pointToLight));
    }


    return rangeAttenuation * light.fIntensity * light.tColor;
}