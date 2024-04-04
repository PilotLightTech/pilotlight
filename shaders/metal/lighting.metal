#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

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
    sampler          tDefaultSampler;
};

struct BindGroup_1
{
    texture2d<float> tAlbedoTexture;
    texture2d<float> tNormalTexture;
    texture2d<float> tPositionTexture;
    texture2d<float> tDepthTexture;
};

struct BindGroup_2
{
    texture2d<float> tSkinningTexture;
};

struct VertexIn {
    float3 tPosition [[attribute(0)]];
};

struct VertexOut {
    float4 tPositionOut [[position]];
    float2 tUV;
};

struct DynamicData
{
    int      iDataOffset;
    int      iVertexOffset;
    int      iPadding[2];
};

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
    float2 inTexCoord0 = float2(0.0, 0.0);

    int iCurrentAttribute = 0;
    const uint iVertexDataOffset = 1 * (vertexID - tObjectInfo.iVertexOffset) + tObjectInfo.iDataOffset;
    inTexCoord0 = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].xy;  iCurrentAttribute++;
    tShaderOut.tPositionOut = float4(inPosition, 1.0);
    tShaderOut.tUV = inTexCoord0;
    tShaderOut.tPositionOut.y = tShaderOut.tPositionOut.y * -1.0;
    return tShaderOut;
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

    float4 tBaseColor = bg1.tAlbedoTexture.sample(bg0.tDefaultSampler, in.tUV);
    float3 tNormal = bg1.tNormalTexture.sample(bg0.tDefaultSampler, in.tUV).xyz;
    float fDepth = bg1.tDepthTexture.sample(bg0.tDefaultSampler, in.tUV).r;
    float3 tSunlightColor = float3(1.0, 1.0, 1.0);
    float3 tSunLightDirection = float3(-1.0, -1.0, -1.0);
    float fDiffuseIntensity = max(0.0, dot(tNormal, -normalize(tSunLightDirection)));
    float4 outColor = tBaseColor * float4(tSunlightColor * (0.05 + fDiffuseIntensity), 1.0);
    outColor = float4(linearTosRGB(outColor.rgb), tBaseColor.a);
    // outColor.r = fDepth;
    return outColor;
}