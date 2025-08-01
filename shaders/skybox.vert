#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "pl_shader_interop_renderer.h"

//-----------------------------------------------------------------------------
// [SECTION] bind group 0
//-----------------------------------------------------------------------------

layout(set = 0, binding = 0) readonly buffer _plGlobalInfo
{
    plGpuGlobalData data[];
} tGlobalInfo;

layout(set = 0, binding = 1)  uniform sampler tDefaultSampler;

//-----------------------------------------------------------------------------
// [SECTION] bind group 1
//-----------------------------------------------------------------------------

layout(set = 1, binding = 0) uniform textureCube samplerCubeMap;

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA {
    plGpuDynSkybox tData;
} tObjectInfo;

//-----------------------------------------------------------------------------
// [SECTION] input
//-----------------------------------------------------------------------------

layout(location = 0) in vec3 inPos;

//-----------------------------------------------------------------------------
// [SECTION] output
//-----------------------------------------------------------------------------

layout(location = 0) out struct plShaderOut {
    vec3 tWorldPosition;
} tShaderOut;

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void
main() 
{
    gl_Position = tGlobalInfo.data[tObjectInfo.tData.uGlobalIndex].tCameraProjection * tGlobalInfo.data[tObjectInfo.tData.uGlobalIndex].tCameraView * tObjectInfo.tData.tModel * vec4(inPos, 1.0);
    gl_Position.z = 0.0;
    // gl_Position.w = gl_Position.z; uncomment if not reverse z
    tShaderOut.tWorldPosition = inPos;
    tShaderOut.tWorldPosition.z = -inPos.z;
}