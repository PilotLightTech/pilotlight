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

struct BindGroup_0
{
    device BindGroupData_0 *data; 
};

struct BindGroup_1
{
    texturecube<half>  texture_0;
    sampler            sampler_0;
};

struct BindGroup_2
{
    texture2d<float> tSkinningTexture;
    sampler          tSkinningSampler;
};

struct DynamicData
{
    float4x4 tModel;
};

struct VertexIn {
    float3 tPosition [[attribute(0)]];
};

struct VertexOut {
    float4 tPosition [[position]];
    float4 tWorldPosition;
};

vertex VertexOut vertex_main(
    uint                vertexID [[ vertex_id ]],
    VertexIn            in [[stage_in]],
    device BindGroup_0& bg0 [[ buffer(1) ]],
    device BindGroup_1& bg1 [[ buffer(2) ]],
    device BindGroup_2& bg2 [[ buffer(3) ]],
    device DynamicData& bg3 [[ buffer(4) ]]
    )
{
    VertexOut tOut;
    tOut.tPosition = bg0.data->tCameraProjection * bg0.data->tCameraView * bg3.tModel * float4(in.tPosition, 1.0);
    tOut.tPosition.w = tOut.tPosition.z;
    tOut.tPosition.y = tOut.tPosition.y * -1;
    tOut.tWorldPosition = float4(in.tPosition.xy, in.tPosition.z, 1.0);
    return tOut;
}

fragment float4 fragment_main(
    VertexOut in [[stage_in]],
    device BindGroup_0& bg0 [[ buffer(1) ]],
    device BindGroup_1& bg1 [[ buffer(2) ]],
    device BindGroup_2& bg2 [[ buffer(3) ]],
    device DynamicData& bg3 [[ buffer(4) ]])
{

    half4 textureSample = bg1.texture_0.sample(bg1.sampler_0, in.tWorldPosition.xyz);
    return float4(textureSample);
}