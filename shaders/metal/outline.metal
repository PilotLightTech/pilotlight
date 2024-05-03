#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

//-----------------------------------------------------------------------------
// [SECTION] specialization constants
//-----------------------------------------------------------------------------

constant int iMeshVariantFlags [[ function_constant(0) ]];
constant int iDataStride [[ function_constant(1) ]];

//-----------------------------------------------------------------------------
// [SECTION] defines & structs
//-----------------------------------------------------------------------------

// iMeshVariantFlags
#define PL_MESH_FORMAT_FLAG_HAS_POSITION   1 << 0
#define PL_MESH_FORMAT_FLAG_HAS_NORMAL     1 << 1

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
};

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

struct DynamicData
{
    float4   tColor;
    float    fThickness;
    int      iDataOffset;
    int      iVertexOffset;
    int      iPadding[1];
    float4x4 tModel;
};

//-----------------------------------------------------------------------------
// [SECTION] input
//-----------------------------------------------------------------------------

struct VertexIn {
    float3 tPosition [[attribute(0)]];
};


//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

struct VertexOut {
    float4 tPositionOut [[position]];
};

vertex VertexOut vertex_main(
    uint                vertexID [[ vertex_id ]],
    VertexIn            in [[stage_in]],
    device const BindGroup_0& bg0 [[ buffer(1) ]],
    device const DynamicData& tObjectInfo [[ buffer(2) ]]
    )
{

    const float4x4 tMVP = bg0.data->tCameraViewProjection * tObjectInfo.tModel;
    float4 inPosition = float4(in.tPosition, 1.0);
    float3 inNormal = float3(0.0, 0.0, 0.0);

    int iCurrentAttribute = 0;

    const uint iVertexDataOffset = iDataStride * (vertexID - tObjectInfo.iVertexOffset) + tObjectInfo.iDataOffset;

    if(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_POSITION)  { iCurrentAttribute++;}
    if(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_NORMAL)    { inNormal = bg0.atVertexData[iVertexDataOffset + iCurrentAttribute].xyz; iCurrentAttribute++;}

    float4 tPos = tMVP * inPosition;
    float4 tNorm = fast::normalize(tMVP * float4(inNormal, 0.0));
    tPos = float4(tPos.xyz + tNorm.xyz * tObjectInfo.fThickness, tPos.w);

    VertexOut tOut;
    tOut.tPositionOut = tPos;
    tOut.tPositionOut.y = tOut.tPositionOut.y * -1.0;
    return tOut;
}

fragment float4 fragment_main(
    VertexOut in [[stage_in]],
    device const BindGroup_0& bg0 [[ buffer(1) ]],
    device const DynamicData& tObjectInfo [[ buffer(2) ]],
    bool front_facing [[front_facing]]
    )
{
    return tObjectInfo.tColor;
}