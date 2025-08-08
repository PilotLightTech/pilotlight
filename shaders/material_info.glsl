#ifndef MATERIAL_INFO_GLSL
#define MATERIAL_INFO_GLSL

struct MaterialInfo
{
    float perceptualRoughness;
    vec3 f0_dielectric;
    float alphaRoughness;
    vec3 c_diff;
    vec3 f90;
    vec3 f90_dielectric;
    float metallic;
    vec3 baseColor;

    // KHR_materials_specular 
    float specularWeight; // product of specularFactor and specularTexture.a

};

#endif // MATERIAL_INFO_GLSL