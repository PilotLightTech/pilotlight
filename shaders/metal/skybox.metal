#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

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
    device float4 *atUnused0;
    device float4 *atUnused1;
    sampler          tDefaultSampler;
    sampler          tEnvSampler;
    texturecube<float> u_LambertianEnvSampler;
    texturecube<float> u_GGXEnvSampler;
    texture2d<float> u_GGXLUT;
};

//-----------------------------------------------------------------------------
// [SECTION] bind group 1
//-----------------------------------------------------------------------------

struct BindGroup_1
{
    texturecube<float> texture_0;
};

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

struct DynamicData
{
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
    float4 tPosition [[position]];
    float4 tWorldPosition;
};

//-----------------------------------------------------------------------------
// [SECTION] helpers
//-----------------------------------------------------------------------------

constant const float GAMMA = 2.2;
constant const float INV_GAMMA = 1.0 / GAMMA;

float3
pl_linear_to_srgb(float3 color)
{
    return pow(color, float3(INV_GAMMA));
}

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

vertex VertexOut vertex_main(
    uint                vertexID [[ vertex_id ]],
    VertexIn            in [[stage_in]],
    device BindGroup_0& bg0 [[ buffer(1) ]],
    device BindGroup_1& bg1 [[ buffer(2) ]],
    device DynamicData& bg3 [[ buffer(3) ]]
    )
{
    VertexOut tOut;
    tOut.tPosition = bg0.data->tCameraProjection * bg0.data->tCameraView * bg3.tModel * float4(in.tPosition, 1.0);
    tOut.tPosition.w = tOut.tPosition.z;
    tOut.tPosition.y = tOut.tPosition.y * -1;
    tOut.tWorldPosition = float4(in.tPosition, 1.0);
    return tOut;
}

fragment float4 fragment_main(
    VertexOut in [[stage_in]],
    device BindGroup_0& bg0 [[ buffer(1) ]],
    device BindGroup_1& bg1 [[ buffer(2) ]],
    device DynamicData& bg3 [[ buffer(3) ]])
{

    float3 textureSample = bg1.texture_0.sample(bg0.tDefaultSampler, in.tWorldPosition.xyz).rgb;
    return float4(pl_linear_to_srgb(textureSample), 1.0);
}