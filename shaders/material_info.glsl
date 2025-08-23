#ifndef MATERIAL_INFO_GLSL
#define MATERIAL_INFO_GLSL

struct MaterialInfo
{
    float ior;
    float perceptualRoughness;
    vec3 f0_dielectric;
    float alphaRoughness;
    vec3 c_diff;
    vec3 f90;
    vec3 f90_dielectric;
    float metallic;
    vec3 baseColor;

    // sheen
    float sheenRoughnessFactor;
    vec3 sheenColorFactor;

    // KHR_materials_specular 
    float specularWeight; // product of specularFactor and specularTexture.a

    // clearcoat
    vec3 clearcoatF0;
    vec3 clearcoatF90;
    float clearcoatFactor;
    vec3 clearcoatNormal;
    float clearcoatRoughness;

};

#endif // MATERIAL_INFO_GLSL