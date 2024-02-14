#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

#define PL_MESH_FORMAT_FLAG_NONE            0
#define PL_MESH_FORMAT_FLAG_HAS_POSITION    1 << 0
#define PL_MESH_FORMAT_FLAG_HAS_NORMAL      1 << 1
#define PL_MESH_FORMAT_FLAG_HAS_TANGENT     1 << 2
#define PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0  1 << 3
#define PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1  1 << 4
#define PL_MESH_FORMAT_FLAG_HAS_COLOR_0     1 << 5
#define PL_MESH_FORMAT_FLAG_HAS_COLOR_1     1 << 6
#define PL_MESH_FORMAT_FLAG_HAS_JOINTS_0    1 << 7
#define PL_MESH_FORMAT_FLAG_HAS_JOINTS_1    1 << 8
#define PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0   1 << 9
#define PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1   1 << 10
#define PL_TEXTURE_HAS_BASE_COLOR           1 << 0
#define PL_TEXTURE_HAS_NORMAL               1 << 1

struct BindGroupData_0
{
    float4   tCameraPosition;
    float4x4 tCameraView;
    float4x4 tCameraProjection;   
    float4x4 tCameraViewProjection;   
};

struct tMaterial
{
    float4 tColor;
};

struct BindGroup_0
{
    device BindGroupData_0 *data;  

    device float4 *atVertexData;
    device tMaterial *atMaterials;
};

struct BindGroup_1
{
    texture2d<float>  tBaseColorTexture;
    sampler          tBaseColorSampler;

    texture2d<float>  tNormalTexture;
    sampler          tNormalSampler;
};

struct BindGroup_2
{
    texture2d<float>  tSkinningTexture;
    sampler            tSkinningSampler;
};

struct VertexIn {
    float3 tPosition [[attribute(0)]];
};

struct VertexOut {
    float4 tPositionOut [[position]];
    float3 tPosition;
    float4 tWorldPosition;
    float2 tUV;
    float4 tColor;
    float3 tWorldNormal;
    float3 tTBN0;
    float3 tTBN1;
    float3 tTBN2;
};

struct DynamicData
{
    int      iDataOffset;
    int      iVertexOffset;
    int      iMaterialIndex;
    int      iPadding[1];
    float4x4 tModel;
};

constant int MeshVariantFlags [[ function_constant(0) ]];
constant int PL_DATA_STRIDE [[ function_constant(1) ]];
constant int PL_HAS_BASE_COLOR_MAP [[ function_constant(2) ]];
constant int PL_HAS_NORMAL_MAP [[ function_constant(3) ]];
constant int PL_USE_SKINNING [[ function_constant(4) ]];

float4x4 get_matrix_from_texture(device const texture2d<float>& s, int index)
{
    float4x4 result = float4x4(1);
    int texSize = s.get_width();
    int pixelIndex = index * 4;
    for (int i = 0; i < 4; ++i)
    {
        int x = (pixelIndex + i) % texSize;
        //Rounding mode of integers is undefined:
        //https://www.khronos.org/registry/OpenGL/specs/es/3.0/GLSL_ES_Specification_3.00.pdf (section 12.33)
        int y = (pixelIndex + i - x) / texSize; 
        result[i] = s.read(uint2(x,y));
    }
    return result;
}

float4x4 get_skinning_matrix(device const texture2d<float>& s, float4 inJoints0, float4 inWeights0)
{
    float4x4 skin = float4x4(0);

    skin +=
        inWeights0.x * get_matrix_from_texture(s, int(inJoints0.x) * 2) +
        inWeights0.y * get_matrix_from_texture(s, int(inJoints0.y) * 2) +
        inWeights0.z * get_matrix_from_texture(s, int(inJoints0.z) * 2) +
        inWeights0.w * get_matrix_from_texture(s, int(inJoints0.w) * 2);

    // if (skin == float4x4(0)) { 
    //     return float4x4(1); 
    // }
    return skin;
}

float4 get_position(device const texture2d<float>& s, float3 inPos, float4 inJoints0, float4 inWeights0)
{
    float4 pos = float4(inPos, 1.0);
    if(bool(PL_USE_SKINNING))
    {
        pos = get_skinning_matrix(s, inJoints0, inWeights0) * pos;
    }
    return pos;
}

float4 get_normal(device const texture2d<float>& s, float3 inNormal, float4 inJoints0, float4 inWeights0)
{
    float4 tNormal = float4(inNormal, 0.0);
    if(bool(PL_USE_SKINNING))
    {
        tNormal = get_skinning_matrix(s, inJoints0, inWeights0) * tNormal;
    }
    return normalize(tNormal);
}

float4 get_tangent(device const texture2d<float>& s, float4 inTangent, float4 inJoints0, float4 inWeights0)
{
    float4 tTangent = float4(inTangent.xyz, 0.0);
    if(bool(PL_USE_SKINNING))
    {
        tTangent = get_skinning_matrix(s, inJoints0, inWeights0) * tTangent;
    }
    return normalize(tTangent);
}

vertex VertexOut vertex_main(
    uint                vertexID [[ vertex_id ]],
    VertexIn            in [[stage_in]],
    device const BindGroup_0& bg0 [[ buffer(1) ]],
    device const BindGroup_1& bg1 [[ buffer(2) ]],
    device const BindGroup_2& bg2 [[ buffer(3) ]],
    device const DynamicData& tObjectInfo [[ buffer(4) ]]
    )
{

    VertexOut tShaderOut;
    float3 inPosition = in.tPosition;
    float3 inNormal = float3(0.0, 0.0, 0.0);
    float4 inTangent = float4(0.0, 0.0, 0.0, 0.0);
    float2 inTexCoord0 = float2(0.0, 0.0);
    float2 inTexCoord1 = float2(0.0, 0.0);
    float4 inColor0 = float4(1.0, 1.0, 1.0, 1.0);
    float4 inColor1 = float4(0.0, 0.0, 0.0, 0.0);
    float4 inJoints0 = float4(0.0, 0.0, 0.0, 0.0);
    float4 inJoints1 = float4(0.0, 0.0, 0.0, 0.0);
    float4 inWeights0 = float4(0.0, 0.0, 0.0, 0.0);
    float4 inWeights1 = float4(0.0, 0.0, 0.0, 0.0);
    int iCurrentAttribute = 0;

    const uint iVertexDataOffset = PL_DATA_STRIDE * (vertexID - tObjectInfo.iVertexOffset) + tObjectInfo.iDataOffset;

    
    if(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_POSITION)  { inPosition.xyz = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].xyz; iCurrentAttribute++;}
    if(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_NORMAL)    { inNormal       = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].xyz; iCurrentAttribute++;}
    if(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TANGENT)   { inTangent      = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}
    if(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0){ inTexCoord0    = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].xy;  iCurrentAttribute++;}
    if(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1){ inTexCoord1    = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].xy;  iCurrentAttribute++;}
    if(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_COLOR_0)   { inColor0       = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}
    if(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_COLOR_1)   { inColor1       = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}
    if(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_JOINTS_0)  { inJoints0      = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}
    if(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_JOINTS_1)  { inJoints1      = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}
    if(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0) { inWeights0     = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}
    if(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1) { inWeights1     = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute];     iCurrentAttribute++;}

    float4 tWorldNormal4 = tObjectInfo.tModel * get_normal(bg2.tSkinningTexture, inNormal, inJoints0, inWeights0);
    tShaderOut.tWorldNormal = tWorldNormal4.xyz;
    if(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_NORMAL)
    {

        if(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TANGENT)
        {
            float4 tangent = get_tangent(bg2.tSkinningTexture, inTangent, inJoints0, inWeights0);
            float4 WorldTangent = tObjectInfo.tModel * tangent;
            float4 WorldBitangent = float4(cross(get_normal(bg2.tSkinningTexture, inNormal, inJoints0, inWeights0).xyz, tangent.xyz) * inTangent.w, 0.0);
            WorldBitangent = tObjectInfo.tModel * WorldBitangent;
            tShaderOut.tTBN0 = WorldTangent.xyz;
            tShaderOut.tTBN1 = WorldBitangent.xyz;
            tShaderOut.tTBN2 = tShaderOut.tWorldNormal.xyz;
        }
    }

    float4 pos = tObjectInfo.tModel * get_position(bg2.tSkinningTexture, inPosition, inJoints0, inWeights0);
    tShaderOut.tPosition = pos.xyz / pos.w;
    tShaderOut.tPositionOut = bg0.data->tCameraViewProjection * pos;
    tShaderOut.tUV = inTexCoord0;
    tShaderOut.tWorldPosition = tShaderOut.tPositionOut / tShaderOut.tPositionOut.w;
    tShaderOut.tColor = inColor0;

    tShaderOut.tPositionOut.y = tShaderOut.tPositionOut.y * -1.0;
    return tShaderOut;
}

struct NormalInfo {
    float3 ng;   // Geometry normal
    float3 t;    // Geometry tangent
    float3 b;    // Geometry bitangent
    float3 n;    // Shading normal
    float3 ntex; // Normal from texture, scaling is accounted for.
};

NormalInfo pl_get_normal_info(device const BindGroup_1& bg1, VertexOut tShaderIn, bool front_facing)
{
    float2 UV = tShaderIn.tUV;
    float2 uv_dx = dfdx(UV);
    float2 uv_dy = dfdy(UV);

    if (length(uv_dx) <= 1e-2) {
      uv_dx = float2(1.0, 0.0);
    }

    if (length(uv_dy) <= 1e-2) {
      uv_dy = float2(0.0, 1.0);
    }

    float3 t_ = (uv_dy.y * dfdx(tShaderIn.tPosition) - uv_dx.y * dfdy(tShaderIn.tPosition)) /
        (uv_dx.x * uv_dy.y - uv_dy.x * uv_dx.y);

    float3 t, b, ng;

    // Compute geometrical TBN:
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_NORMAL))
    {

        if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TANGENT))
        {
            // Trivial TBN computation, present as vertex attribute.
            // Normalize eigenvectors as matrix is linearly interpolated.
            t = normalize(tShaderIn.tTBN0);
            b = normalize(tShaderIn.tTBN1);
            ng = normalize(tShaderIn.tTBN2);
        }
        else
        {
            // Normals are either present as vertex attributes or approximated.
            ng = normalize(tShaderIn.tWorldNormal);
            t = normalize(t_ - ng * dot(ng, t_));
            b = cross(ng, t);
        }
    }
    else
    {
        ng = normalize(cross(dfdx(tShaderIn.tPosition), dfdy(tShaderIn.tPosition)));
        t = normalize(t_ - ng * dot(ng, t_));
        b = cross(ng, t);
    }


    // For a back-facing surface, the tangential basis vectors are negated.
    if (front_facing == false)
    {
        t *= -1.0;
        b *= -1.0;
        ng *= -1.0;
    }

    // Compute normals:
    NormalInfo info;
    info.ng = ng;
    if(bool(PL_HAS_NORMAL_MAP)) 
    {
        info.ntex = bg1.tNormalTexture.sample(bg1.tNormalSampler, UV).rgb * 2.0 - float3(1.0);
        // info.ntex *= vec3(0.2, 0.2, 1.0);
        // info.ntex *= vec3(u_NormalScale, u_NormalScale, 1.0);
        info.ntex = normalize(info.ntex);
        info.n = normalize(float3x3(t, b, ng) * info.ntex);
    }
    else
    {
        info.n = ng;
    }
    info.t = t;
    info.b = b;
    return info;
}

float4 getBaseColor(device const BindGroup_1& bg1, float4 u_ColorFactor, VertexOut tShaderIn)
{
    float4 baseColor = u_ColorFactor;

    if(bool(PL_HAS_BASE_COLOR_MAP))
    {
        baseColor *= bg1.tBaseColorTexture.sample(bg1.tBaseColorSampler, tShaderIn.tUV);
    }
    return baseColor;
}

constant const float GAMMA = 2.2;
constant const float INV_GAMMA = 1.0 / GAMMA;

// linear to sRGB approximation
// see http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
float3 linearTosRGB(float3 color)
{
    return pow(color, float3(INV_GAMMA));
}

fragment float4 fragment_main(
    VertexOut in [[stage_in]],
    device const BindGroup_0& bg0 [[ buffer(1) ]],
    device const BindGroup_1& bg1 [[ buffer(2) ]],
    device const BindGroup_2& bg2 [[ buffer(3) ]],
    device const DynamicData& tObjectInfo [[ buffer(4) ]],
    bool front_facing [[front_facing]]
    )
{

    float4 tBaseColor = getBaseColor(bg1, bg0.atMaterials[tObjectInfo.iMaterialIndex].tColor, in);
    float3 tSunlightColor = float3(1.0, 1.0, 1.0);
    NormalInfo tNormalInfo = pl_get_normal_info(bg1, in, front_facing);
    float3 tSunLightDirection = float3(-1.0, -1.0, -1.0);
    float fDiffuseIntensity = max(0.0, dot(tNormalInfo.n, -normalize(tSunLightDirection)));
    float4 outColor = tBaseColor * float4(tSunlightColor * (0.05 + fDiffuseIntensity), 1.0);
    outColor = float4(linearTosRGB(outColor.rgb), tBaseColor.a);
    return outColor;
}