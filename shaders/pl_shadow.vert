#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_ARB_shader_viewport_layer_array : enable

#include "pl_bg_scene.inc"

//-----------------------------------------------------------------------------
// [SECTION] bind group 1
//-----------------------------------------------------------------------------

layout(set = 1, binding = 0) readonly buffer _plCameraInfo
{
    mat4 atCameraProjs[];
} tCameraInfo;

layout(set = 1, binding = 1) readonly buffer _plInstanceInfo
{
    plShadowInstanceBufferData atData[];
} plInstanceInfo;

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

layout(std140, set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    plGpuDynShadow tData;
} tObjectInfo;

//-----------------------------------------------------------------------------
// [SECTION] input & output
//-----------------------------------------------------------------------------

// input
layout(location = 0) in vec3 inPos;

// output
layout(location = 0) out struct plShaderOut {
    vec2 tUV;
} tShaderIn;

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void main()
{

    const int iDataStride = 4;
    vec4 inPosition  = vec4(inPos, 1.0);

    uint uTransformIndex = plInstanceInfo.atData[gl_InstanceIndex].uTransformIndex;
    int uViewportIndex = plInstanceInfo.atData[gl_InstanceIndex].iViewportIndex;


    // offset = offset into current mesh + offset into global buffer
    const uint iVertexDataOffset = iDataStride * (gl_VertexIndex - tObjectInfo.tData.iVertexOffset) + tObjectInfo.tData.iDataOffset;

    vec3 inNormal = tVertexBuffer.atVertexData[iVertexDataOffset + 0].xyz;
    vec4 inTangent = tVertexBuffer.atVertexData[iVertexDataOffset + 1];
    vec2 inTexCoord0 = tVertexBuffer.atVertexData[iVertexDataOffset + 2].xy;
    vec2 inTexCoord1 = tVertexBuffer.atVertexData[iVertexDataOffset + 2].zw;
    vec4 inColor = tVertexBuffer.atVertexData[iVertexDataOffset + 3];


    int iUVSet = tMaterialInfo.atMaterials[tObjectInfo.tData.iMaterialIndex].aiTextureUVSet[PL_TEXTURE_BASE_COLOR];

    tShaderIn.tUV = inTexCoord0;
    if(iUVSet == 1)
    {
        tShaderIn.tUV = inTexCoord1;
    }

    // gl_Position = tCameraInfo.atCameraProjs[tObjectInfo.tData.iIndex + gl_InstanceIndex] * pos;
    // gl_ViewportIndex = gl_InstanceIndex;

    gl_Position = tCameraInfo.atCameraProjs[tObjectInfo.tData.iIndex + uViewportIndex] * tTransformBuffer.atTransform[uTransformIndex] * inPosition;
    gl_ViewportIndex = uViewportIndex;

    // if(tCameraInfo.atCameraProjs[tObjectInfo.tData.iIndex + uViewportIndex][3][3] == 1.0) // orthographic
    // {
    //     // gl_Position.z = 1.0 - gl_Position.z;
    //     if(gl_Position.z > 1.0)
    //     {
    //         gl_Position.z = 0.0;
    //     }
    // }
}