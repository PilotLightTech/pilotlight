#ifndef SAMPLING_INC
#define SAMPLING_INC

vec2
pl_hammersley_2d(int i, int iN)
{
    // Hammersley Points on the Hemisphere
    // CC BY 3.0 (Holger Dammertz)
    // http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
    // with adapted interface
    uint uBits =  uint(i);
    uBits = (uBits << 16u) | (uBits >> 16u);
    uBits = ((uBits & 0x55555555u) << 1u) | ((uBits & 0xAAAAAAAAu) >> 1u);
    uBits = ((uBits & 0x33333333u) << 2u) | ((uBits & 0xCCCCCCCCu) >> 2u);
    uBits = ((uBits & 0x0F0F0F0Fu) << 4u) | ((uBits & 0xF0F0F0F0u) >> 4u);
    uBits = ((uBits & 0x00FF00FFu) << 8u) | ((uBits & 0xFF00FF00u) >> 8u);
    float rdi = float(uBits) * 2.3283064365386963e-10; // / 0x100000000

    // hammersley2d describes a sequence of points in the 2d unit square [0,1)^2
    // that can be used for quasi Monte Carlo integration
    return vec2(float(i)/float(iN), rdi);
}

mat3
pl_generate_tbn(vec3 tNormal)
{
    // TBN generates a tangent bitangent normal coordinate frame from the normal
    // (the normal must be normalized)
    vec3 tBitangent = vec3(0.0, 1.0, 0.0);

    const float tNdotUp = dot(tNormal, vec3(0.0, 1.0, 0.0));
    const float fEpsilon = 0.0000001;
    if (1.0 - abs(tNdotUp) <= fEpsilon)
    {
        // Sampling +Y or -Y, so we need a more robust bitangent.
        if (tNdotUp > 0.0)
        {
            tBitangent = vec3(0.0, 0.0, 1.0);
        }
        else
        {
            tBitangent = vec3(0.0, 0.0, -1.0);
        }
    }

    const vec3 tTangent = normalize(cross(tBitangent, tNormal));
    tBitangent = cross(tNormal, tTangent);

    return mat3(tTangent, tBitangent, tNormal);
}

vec3
pl_get_importance_sample_ggx(vec2 tXi, vec3 tN, float fRoughness)
{
    const float fAlpha = fRoughness * fRoughness;
    float fCosTheta = pl_saturate(sqrt((1.0 - tXi.y) / (1.0 + (fAlpha * fAlpha - 1.0) * tXi.y)));
    float fSinTheta = sqrt(1.0 - fCosTheta * fCosTheta);
    float fPhi = 2.0 * PL_PI * tXi.x;

    // transform the hemisphere sample to the normal coordinate frame
    // i.e. rotate the hemisphere to the normal direction
    const vec3 tLocalSpaceDirection = normalize(vec3(fSinTheta * cos(fPhi), fSinTheta * sin(fPhi), fCosTheta));
    mat3 tTBN = pl_generate_tbn(tN);
    vec3 tDirection = tTBN * tLocalSpaceDirection;
    return tDirection;
}

vec3
pl_get_importance_sample_cosine(vec2 tXi, vec3 tN)
{
    float fCosTheta = sqrt(1.0 - tXi.y);
    float fSinTheta = sqrt(tXi.y);
    float fPhi = 2.0 * PL_PI * tXi.x;

    // transform the hemisphere sample to the normal coordinate frame
    // i.e. rotate the hemisphere to the normal direction
    const vec3 tLocalSpaceDirection = normalize(vec3(fSinTheta * cos(fPhi), fSinTheta * sin(fPhi), fCosTheta));
    mat3 tTBN = pl_generate_tbn(tN);
    vec3 tDirection = tTBN * tLocalSpaceDirection;
    return tDirection;
}

vec3
pl_get_importance_sample_charlie(vec2 tXi, vec3 tN, float fRoughness)
{
    const float fAlpha = fRoughness * fRoughness;

    float fSinTheta = pow(tXi.y, fAlpha / (2.0 * fAlpha + 1.0));
    float fCosTheta = sqrt(1.0 - fSinTheta * fSinTheta);
    float fPhi = 2.0 * PL_PI * tXi.x;

    // transform the hemisphere sample to the normal coordinate frame
    // i.e. rotate the hemisphere to the normal direction
    const vec3 tLocalSpaceDirection = normalize(vec3(fSinTheta * cos(fPhi), fSinTheta * sin(fPhi), fCosTheta));
    mat3 tTBN = pl_generate_tbn(tN);
    vec3 tDirection = tTBN * tLocalSpaceDirection;
    return tDirection;
}

#endif // SAMPLING_INC