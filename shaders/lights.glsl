
struct plDLightData
{
    vec3  tPosition;
    float fIntensity;

    vec3  tDirection;
    int   iType;

    vec3  tColor;
    float fRange;

    int iShadowIndex;
    int iCascadeCount;
    int iCastShadow;
};

struct plPLightData
{
    vec3  tPosition;
    float fIntensity;

    vec3  tDirection;
    int   iType;

    vec3  tColor;
    float fRange;

    int iShadowIndex;
    int iCastShadow;
};

struct plDLightShadowData
{
	vec4 cascadeSplits;
	mat4 viewProjMat[4];
    int iShadowMapTexIdx;
    float fFactor;
    float fXOffset;
    float fYOffset;
};

struct plPLightShadowData
{
	mat4 viewProjMat[6];
    int iShadowMapTexIdx;
    float fFactor;
    float fXOffset;
    float fYOffset;
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
getDLightIntensity(plDLightData light, vec3 pointToLight)
{
    float rangeAttenuation = 1.0;
    return rangeAttenuation * light.fIntensity * light.tColor;
}

vec3
getPLightIntensity(plPLightData light, vec3 pointToLight)
{
    float rangeAttenuation = getRangeAttenuation(light.fRange, length(pointToLight));
    return rangeAttenuation * light.fIntensity * light.tColor;
}