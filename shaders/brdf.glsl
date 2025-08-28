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

float
pl_fresnel_schlick(float f0, float f90, float VdotH)
{
    float x = clamp(1.0 - VdotH, 0.0, 1.0);
    float x2 = x * x;
    float x5 = x * x2 * x2;
    return f0 + (f90 - f0) * x5;
}

float
pl_fresnel_schlick(float f0, float VdotH)
{
    float f90 = 1.0; //clamp(50.0 * f0, 0.0, 1.0);
    return pl_fresnel_schlick(f0, f90, VdotH);
}

vec3
pl_fresnel_schlick(vec3 f0, float f90, float VdotH)
{
    float x = clamp(1.0 - VdotH, 0.0, 1.0);
    float x2 = x * x;
    float x5 = x * x2 * x2;
    return f0 + (f90 - f0) * x5;
}

vec3
pl_fresnel_schlick(vec3 f0, float VdotH)
{
    float f90 = 1.0; //clamp(dot(f0, vec3(50.0 * 0.33)), 0.0, 1.0);
    return pl_fresnel_schlick(f0, f90, VdotH);
}

// Normal distribution function term of the specular equation (D)
// Real-Time Rendering 4th Edition, Page 340, Equation 9.41
float
pl_distribution_ggx(float fNdotH, float alphaRoughness)
{
    float alphaRoughnessSq = alphaRoughness * alphaRoughness;
    float f = (fNdotH * fNdotH) * (alphaRoughnessSq - 1.0) + 1.0;
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
    float a = alphaRoughness;
    float GGXV = NdotL * (NdotV * (1.0 - a) + a);
    float GGXL = NdotV * (NdotL * (1.0 - a) + a);
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
pl_brdf_diffuse(vec3 diffuseColor)
{
    // disney
    // float Fd_Burley(float NoV, float NoL, float LoH, float roughness) {
    //     float f90 = 0.5 + 2.0 * roughness * LoH * LoH;
    //     float lightScatter = F_Schlick(NoL, 1.0, f90);
    //     float viewScatter = F_Schlick(NoV, 1.0, f90);
    //     return lightScatter * viewScatter * (1.0 / PI);
    // }

    return diffuseColor / M_PI;
}

// BRDF for specular (fs)
// Real-Time Rendering 4th Edition, Page 337, Equation 9.34
vec3
pl_brdf_specular(float alphaRoughness, float NdotL, float NdotV, float NdotH)
{
    float G2 = pl_masking_shadowing_ggx(NdotL, NdotV, alphaRoughness);
    float D = pl_distribution_ggx(NdotH, alphaRoughness);
    return vec3(G2 * D);
}

////////////////////////////////////////////////////////////////

//Sheen implementation-------------------------------------------------------------------------------------
// See  https://github.com/sebavan/glTF/tree/KHR_materials_sheen/extensions/2.0/Khronos/KHR_materials_sheen

// Estevez and Kulla http://www.aconty.com/pdf/s2017_pbs_imageworks_sheen.pdf
float
D_Charlie(float sheenRoughness, float NdotH)
{
    sheenRoughness = max(sheenRoughness, 0.000001); //clamp (0,1]
    float alphaG = sheenRoughness * sheenRoughness;
    float invR = 1.0 / alphaG;
    float cos2h = NdotH * NdotH;
    float sin2h = 1.0 - cos2h;
    return (2.0 + invR) * pow(sin2h, invR * 0.5) / (2.0 * M_PI);
}

// NDF
float D_Charlie2(float sheenRoughness, float NdotH)
{
    sheenRoughness = max(sheenRoughness, 0.000001); //clamp (0,1]
    float invR = 1.0 / sheenRoughness;
    float cos2h = NdotH * NdotH;
    float sin2h = 1.0 - cos2h;
    return (2.0 + invR) * pow(sin2h, invR * 0.5) / (2.0 * M_PI);
}

float
lambdaSheenNumericHelper(float x, float alphaG)
{
    float oneMinusAlphaSq = (1.0 - alphaG) * (1.0 - alphaG);
    float a = mix(21.5473, 25.3245, oneMinusAlphaSq);
    float b = mix(3.82987, 3.32435, oneMinusAlphaSq);
    float c = mix(0.19823, 0.16801, oneMinusAlphaSq);
    float d = mix(-1.97760, -1.27393, oneMinusAlphaSq);
    float e = mix(-4.32054, -4.85967, oneMinusAlphaSq);
    return a / (1.0 + b * pow(x, c)) + d * x + e;
}


float
lambdaSheen(float cosTheta, float alphaG)
{
    if (abs(cosTheta) < 0.5)
    {
        return exp(lambdaSheenNumericHelper(cosTheta, alphaG));
    }
    else
    {
        return exp(2.0 * lambdaSheenNumericHelper(0.5, alphaG) - lambdaSheenNumericHelper(1.0 - cosTheta, alphaG));
    }
}


float
V_Sheen(float NdotL, float NdotV, float sheenRoughness)
{
    sheenRoughness = max(sheenRoughness, 0.000001); //clamp (0,1]
    float alphaG = sheenRoughness * sheenRoughness;

    return clamp(1.0 / ((1.0 + lambdaSheen(NdotV, alphaG) + lambdaSheen(NdotL, alphaG)) *
        (4.0 * NdotV * NdotL)), 0.0, 1.0);
}

// f_sheen
vec3
BRDF_specularSheen(vec3 sheenColor, float sheenRoughness, float NdotL, float NdotV, float NdotH)
{
    float sheenDistribution = D_Charlie(sheenRoughness, NdotH);
    float sheenVisibility = V_Sheen(NdotL, NdotV, sheenRoughness);
    return sheenColor * sheenDistribution * sheenVisibility;
}


#endif // BRDF_GLSL