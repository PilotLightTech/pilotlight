#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

//-----------------------------------------------------------------------------
// [SECTION] specialization constants
//-----------------------------------------------------------------------------

constant int iMeshVariantFlags [[ function_constant(0) ]];
constant int iDataStride [[ function_constant(1) ]];
constant int iTextureMappingFlags [[ function_constant(2) ]];
constant int iMaterialFlags [[ function_constant(3) ]];

//-----------------------------------------------------------------------------
// [SECTION] defines & structs
//-----------------------------------------------------------------------------

constant const float GAMMA = 2.2;
constant const float INV_GAMMA = 1.0 / GAMMA;

// iMeshVariantFlags
#define PL_MESH_FORMAT_FLAG_NONE           0
#define PL_MESH_FORMAT_FLAG_HAS_POSITION   1 << 0
#define PL_MESH_FORMAT_FLAG_HAS_NORMAL     1 << 1
#define PL_MESH_FORMAT_FLAG_HAS_TANGENT    1 << 2
#define PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0 1 << 3
#define PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1 1 << 4
#define PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_2 1 << 5
#define PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_3 1 << 6
#define PL_MESH_FORMAT_FLAG_HAS_COLOR_0    1 << 7
#define PL_MESH_FORMAT_FLAG_HAS_COLOR_1    1 << 8
#define PL_MESH_FORMAT_FLAG_HAS_JOINTS_0   1 << 9
#define PL_MESH_FORMAT_FLAG_HAS_JOINTS_1   1 << 10
#define PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0  1 << 11
#define PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1  1 << 12

// iTextureMappingFlags
#define PL_HAS_BASE_COLOR_MAP            1 << 0
#define PL_HAS_NORMAL_MAP                1 << 1
#define PL_HAS_EMISSIVE_MAP              1 << 2
#define PL_HAS_OCCLUSION_MAP             1 << 3
#define PL_HAS_METALLIC_ROUGHNESS_MAP    1 << 4

// iMaterialFlags
#define PL_MATERIAL_METALLICROUGHNESS  1 << 0

// iRenderingFlags
#define PL_RENDERING_FLAG_USE_PUNCTUAL 1 << 0
#define PL_RENDERING_FLAG_USE_IBL      1 << 1

struct tMaterial
{
    // Metallic Roughness
    int   u_MipCount;
    float u_MetallicFactor;
    float u_RoughnessFactor;
    //-------------------------- ( 16 bytes )

    float4 u_BaseColorFactor;
    //-------------------------- ( 16 bytes )

    // Clearcoat
    float u_ClearcoatFactor;
    float u_ClearcoatRoughnessFactor;
    int _unused2[2];
    //-------------------------- ( 16 bytes )

    // Specular
    packed_float3 u_KHR_materials_specular_specularColorFactor;
    float u_KHR_materials_specular_specularFactor;
    //-------------------------- ( 16 bytes )

    // Iridescence
    float u_IridescenceFactor;
    float u_IridescenceIor;
    float u_IridescenceThicknessMinimum;
    float u_IridescenceThicknessMaximum;
    //-------------------------- ( 16 bytes )

    // Emissive Strength
    packed_float3 u_EmissiveFactor;
    float u_EmissiveStrength;
    //-------------------------- ( 16 bytes )
    

    // IOR
    float u_Ior;

    // Alpha mode
    float u_AlphaCutoff;
    float u_OcclusionStrength;
    float u_Unuses;
    //-------------------------- ( 16 bytes )

    int BaseColorUVSet;
    int NormalUVSet;
    int EmissiveUVSet;
    int OcclusionUVSet;
    //-------------------------- ( 16 bytes )

    int MetallicRoughnessUVSet;
    int ClearcoatUVSet;
    int ClearcoatRoughnessUVSet;
    int ClearcoatNormalUVSet;
    //-------------------------- ( 16 bytes )

    int SpecularUVSet;
    int SpecularColorUVSet;
    int IridescenceUVSet;
    int IridescenceThicknessUVSet;
    //-------------------------- ( 16 bytes )
};

//-----------------------------------------------------------------------------
// [SECTION] bind group 0
//-----------------------------------------------------------------------------

struct BindGroupData_0
{
    float4   tCameraPosition;
    float4x4 tCameraView;
    float4x4 tCameraProjection;   
    float4x4 tCameraViewProjection;   
};

struct BindGroup_0
{
    device BindGroupData_0 *data;  
    device float4 *atVertexData;
    device tMaterial *atMaterials;
    sampler          tDefaultSampler;
};

//-----------------------------------------------------------------------------
// [SECTION] bind group 1
//-----------------------------------------------------------------------------

struct BindGroup_1
{
    texture2d<float> tBaseColorTexture;
};

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

struct DynamicData
{
    int      iIndex;
    int      iDataOffset;
    int      iVertexOffset;
    int      iMaterialIndex;
    float4x4 tModel;
};

//-----------------------------------------------------------------------------
// [SECTION] input
//-----------------------------------------------------------------------------

struct VertexIn {
    float3 tPosition [[attribute(0)]];
};

//-----------------------------------------------------------------------------
// [SECTION] output
//-----------------------------------------------------------------------------

struct VertexOut {
    float4 tPositionOut [[position]];
    float2 tUV0;
    float2 tUV1;
    float2 tUV2;
    float2 tUV3;
    float2 tUV4;
    float2 tUV5;
    float2 tUV6;
    float2 tUV7;
};

//-----------------------------------------------------------------------------
// [SECTION] helpers
//-----------------------------------------------------------------------------

float4
getBaseColor(device const BindGroup_0& bg0, device const BindGroup_1& bg1, float4 u_ColorFactor, VertexOut tShaderIn, float2 UV)
{
    float4 baseColor = float4(1);

    if(bool(iMaterialFlags & PL_MATERIAL_METALLICROUGHNESS))
    {
        baseColor = u_ColorFactor;
    }

    if(bool(iMaterialFlags & PL_MATERIAL_METALLICROUGHNESS) && bool(iTextureMappingFlags & PL_HAS_BASE_COLOR_MAP))
    {
        baseColor *= bg1.tBaseColorTexture.sample(bg0.tDefaultSampler, UV);
    }
    return baseColor;
}

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

vertex VertexOut vertex_main(
    uint                vertexID [[ vertex_id ]],
    VertexIn            in [[stage_in]],
    device const BindGroup_0& bg0 [[ buffer(1) ]],
    device const BindGroup_1& bg1 [[ buffer(2) ]],
    device const DynamicData& tObjectInfo [[ buffer(3) ]]
    )
{

    VertexOut tShaderOut;
    float4 inPosition = float4(in.tPosition, 1.0);
    float2 inTexCoord0 = float2(0.0, 0.0);
    float2 inTexCoord1 = float2(0.0, 0.0);
    float2 inTexCoord2 = float2(0.0, 0.0);
    float2 inTexCoord3 = float2(0.0, 0.0);
    float2 inTexCoord4 = float2(0.0, 0.0);
    float2 inTexCoord5 = float2(0.0, 0.0);
    float2 inTexCoord6 = float2(0.0, 0.0);
    float2 inTexCoord7 = float2(0.0, 0.0);
    int iCurrentAttribute = 0;

    const uint iVertexDataOffset = iDataStride * (vertexID - tObjectInfo.iVertexOffset) + tObjectInfo.iDataOffset;

    
    if(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_POSITION)  { iCurrentAttribute++;}
    if(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_NORMAL)    { iCurrentAttribute++;}
    if(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TANGENT)   { iCurrentAttribute++;}
    if(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0){
        inTexCoord0 = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].xy; 
        inTexCoord1 = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].zw; 
        iCurrentAttribute++;
    }
    if(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1){
        inTexCoord2 = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].xy; 
        inTexCoord3 = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].zw; 
        iCurrentAttribute++;
    }
    if(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_2){
        inTexCoord4 = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].xy; 
        inTexCoord5 = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].zw; 
        iCurrentAttribute++;
    }
    if(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_3){
        inTexCoord6 = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].xy; 
        inTexCoord7 = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].zw; 
        iCurrentAttribute++;
    }

    float4 pos = tObjectInfo.tModel * inPosition;
    tShaderOut.tPositionOut = bg0.data[tObjectInfo.iIndex].tCameraViewProjection * pos;
    tShaderOut.tUV0 = inTexCoord0;
    tShaderOut.tUV1 = inTexCoord1;
    tShaderOut.tUV2 = inTexCoord2;
    tShaderOut.tUV3 = inTexCoord3;
    tShaderOut.tUV4 = inTexCoord4;
    tShaderOut.tUV5 = inTexCoord5;
    tShaderOut.tUV6 = inTexCoord6;
    tShaderOut.tUV7 = inTexCoord7;
    tShaderOut.tPositionOut.y = tShaderOut.tPositionOut.y * -1.0;
    return tShaderOut;
}

fragment void fragment_main(
    VertexOut in [[stage_in]],
    device const BindGroup_0& bg0 [[ buffer(1) ]],
    device const BindGroup_1& bg1 [[ buffer(2) ]],
    device const DynamicData& tObjectInfo [[ buffer(3) ]]
    )
{
    const float2 tUV[8] = {
        in.tUV0,
        in.tUV1,
        in.tUV2,
        in.tUV3,
        in.tUV4,
        in.tUV5,
        in.tUV6,
        in.tUV7
    };
    tMaterial material = bg0.atMaterials[tObjectInfo.iMaterialIndex];
    float4 tBaseColor = getBaseColor(bg0, bg1, material.u_BaseColorFactor, in, tUV[material.BaseColorUVSet]);
    if(tBaseColor.a < material.u_AlphaCutoff)
    {
        discard_fragment();
    }
}