
struct tMaterial
{
    // Metallic Roughness
    int   u_MipCount;
    float u_MetallicFactor;
    float u_RoughnessFactor;
    //-------------------------- ( 16 bytes )

    vec4 u_BaseColorFactor;
    //-------------------------- ( 16 bytes )

    // Emissive Strength
    vec3 u_EmissiveFactor;
    float u_EmissiveStrength;
    //-------------------------- ( 16 bytes )
    
    // Alpha mode
    float u_AlphaCutoff;
    float u_OcclusionStrength;
    int BaseColorUVSet;
    int NormalUVSet;
    //-------------------------- ( 16 bytes )

    int EmissiveUVSet;
    int OcclusionUVSet;
    int MetallicRoughnessUVSet;
    int iBaseColorTexIdx;
    //-------------------------- ( 16 bytes )

    int iNormalTexIdx;
    int iEmissiveTexIdx;
    int iMetallicRoughnessTexIdx;
    int iOcclusionTexIdx;
    //-------------------------- ( 16 bytes )
};

struct MaterialInfo
{
    float perceptualRoughness;      // roughness value, as authored by the model creator (input to shader)
    vec3 f0;                        // full reflectance color (n incidence angle)

    float alphaRoughness;           // roughness mapped to a more linear change in the roughness (proposed by [2])
    vec3 c_diff;

    vec3 f90;                       // reflectance color at grazing angle
    float metallic;

    vec3 baseColor;
};