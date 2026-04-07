#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_ARB_shader_viewport_layer_array : enable

#include "pl_bg_scene.inc"

//-----------------------------------------------------------------------------
// [SECTION] specialication constants
//-----------------------------------------------------------------------------

layout(constant_id = 0) const int iMeshVariantFlags = 0;
layout(constant_id = 1) const int iDataStride = 0;

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
layout(location = 1) in vec2 inNormal;
layout(location = 2) in vec2 inUV;

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void main()
{

    vec4 inPosition  = vec4(inPos, 1.0);

    // uint uTransformIndex = plInstanceInfo.atData[gl_InstanceIndex].uTransformIndex;
    // int uViewportIndex = plInstanceInfo.atData[gl_InstanceIndex].iViewportIndex;


    gl_Position = tCameraInfo.atCameraProjs[tObjectInfo.tData.iIndex] * inPosition;
    gl_ViewportIndex = 0;
    // if(gl_Position.z > 1.0)
    // {
    //     gl_Position.z = 0.0;
    // }
}