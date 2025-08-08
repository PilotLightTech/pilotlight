#ifndef BRDF_GLSL
#define BRDF_GLSL

const float M_PI = 3.141592653589793;

// Fresnel reflectance term of the specular equation (F)
// Real-Time Rendering 4th Edition, Page 321, Equation 9.18 modified
vec3
pl_fresnel_schlick(vec3 f0, vec3 f90, float VdotH)
{
    return f0 + (f90 - f0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

// Normal distribution function term of the specular equation (D)
// Real-Time Rendering 4th Edition, Page 340, Equation 9.41
float
pl_distribution_ggx(float NdotH, float alphaRoughness)
{
    float alphaRoughnessSq = alphaRoughness * alphaRoughness;
    float f = (NdotH * NdotH) * (alphaRoughnessSq - 1.0) + 1.0;
    return alphaRoughnessSq / (M_PI * f * f);
}

// Masking-Shadowing function (G2)
// Real-Time Rendering 4th Edition, Page 341, Equation 9.42
float
pl_masking_shadowing_ggx(float NdotL, float NdotV, float alphaRoughness)
{
    float alphaRoughnessSq = alphaRoughness * alphaRoughness;

    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);

    float GGX = GGXV + GGXL;
    if (GGX > 0.0)
    {
        return 0.5 / GGX;
    }
    return 0.0;
}

// Masking-Shadowing function fast (G2)
// Real-Time Rendering 4th Edition, Page 341, Equation 9.42
float
pl_masking_shadowing_fast_ggx(float NdotL, float NdotV, float alphaRoughness)
{
    float a2 = alphaRoughness * alphaRoughness;
    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
    float GGX = GGXV + GGXL;
    if (GGX > 0.0)
    {
        return 0.5 / GGX;
    }
    return 0.0;
}


// Simple Lambertian BRDF (fd)
// Real-Time Rendering 4th Edition, Page 314, Equation 9.11
vec3
pl_brdf_lambertian(vec3 diffuseColor)
{
    return diffuseColor / M_PI;

    // disney
    // float Fd_Burley(float NoV, float NoL, float LoH, float roughness) {
    //     float f90 = 0.5 + 2.0 * roughness * LoH * LoH;
    //     float lightScatter = F_Schlick(NoL, 1.0, f90);
    //     float viewScatter = F_Schlick(NoV, 1.0, f90);
    //     return lightScatter * viewScatter * (1.0 / PI);
    // }
}

//  https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#acknowledgments AppendixB
vec3
BRDF_specularGGX(float alphaRoughness, float NdotL, float NdotV, float NdotH)
{
    float G2 = pl_masking_shadowing_ggx(NdotL, NdotV, alphaRoughness);
    float D = pl_distribution_ggx(NdotH, alphaRoughness);
    return vec3(G2 * D);
}

#endif // BRDF_GLSL