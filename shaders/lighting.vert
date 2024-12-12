#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

struct tMaterial
{
    // Metallic Roughness
    int   u_MipCount;
    float u_MetallicFactor;
    float u_RoughnessFactor;
    //-------------------------- ( 16 bytes )

    vec4 u_BaseColorFactor;
    //-------------------------- ( 16 bytes )

    // // Specular Glossiness
    // vec3 u_SpecularFactor;
    // vec4 u_DiffuseFactor;
    // float u_GlossinessFactor;

    // // Sheen
    // float u_SheenRoughnessFactor;
    // vec3 u_SheenColorFactor;

    // Clearcoat
    float u_ClearcoatFactor;
    float u_ClearcoatRoughnessFactor;
    vec2 _unused1;
    //-------------------------- ( 16 bytes )

    // Specular
    vec3 u_KHR_materials_specular_specularColorFactor;
    float u_KHR_materials_specular_specularFactor;
    //-------------------------- ( 16 bytes )

    // // Transmission
    // float u_TransmissionFactor;

    // // Volume
    // float u_ThicknessFactor;
    // vec3 u_AttenuationColor;
    // float u_AttenuationDistance;

    // Iridescence
    float u_IridescenceFactor;
    float u_IridescenceIor;
    float u_IridescenceThicknessMinimum;
    float u_IridescenceThicknessMaximum;
    //-------------------------- ( 16 bytes )

    // Emissive Strength
    vec3 u_EmissiveFactor;
    float u_EmissiveStrength;
    //-------------------------- ( 16 bytes )
    

    // // IOR
    float u_Ior;

    // // Anisotropy
    // vec3 u_Anisotropy;

    // Alpha mode
    float u_AlphaCutoff;
    float u_OcclusionStrength;
    float u_Unuses;
    //-------------------------- ( 16 bytes )

    int BaseColorUVSet;
    int NormalUVSet;
    int EmissiveUVSet;
    int OcclusionUVSet;
    int MetallicRoughnessUVSet;
    int ClearcoatUVSet;
    int ClearcoatRoughnessUVSet;
    int ClearcoatNormalUVSet;
    int SpecularUVSet;
    int SpecularColorUVSet;
    int IridescenceUVSet;
    int IridescenceThicknessUVSet;
};

layout(std140, set = 0, binding = 0) readonly buffer _tVertexBuffer
{
	vec4 atVertexData[];
} tVertexBuffer;

layout(set = 0, binding = 1) readonly buffer plMaterialInfo
{
    tMaterial atMaterials[];
} tMaterialInfo;

layout(set = 0, binding = 2)  uniform sampler tDefaultSampler;
layout(set = 0, binding = 3)  uniform sampler tEnvSampler;

layout(set = 0, binding = 4)  uniform texture2D at2DTextures[4096];
layout(set = 0, binding = 4100)  uniform textureCube atCubeTextures[4096];

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    int iDataOffset;
    int iVertexOffset;
} tObjectInfo;

// input
layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUV;

// output
layout(location = 0) out struct plShaderOut {
    vec2 tUV;
} tShaderOut;

void main() 
{

    vec4 inPosition  = vec4(inPos, 0.0, 1.0);
    gl_Position = inPosition;
    tShaderOut.tUV = inUV;
}