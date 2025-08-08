#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "pl_shader_interop_renderer.h"
#include "bg_scene.inc"
#include "bg_view.inc"

//-----------------------------------------------------------------------------
// [SECTION] bind group 2
//-----------------------------------------------------------------------------

layout(set = 2, binding = 0) uniform textureCube samplerCubeMap;

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
    gl_Position = tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tCameraProjection * tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tCameraView * tObjectInfo.tData.tModel * vec4(inPos, 1.0);
    gl_Position.z = 0.0;
    // gl_Position.w = gl_Position.z; uncomment if not reverse z
    tShaderOut.tWorldPosition = inPos;
    tShaderOut.tWorldPosition.z = -inPos.z;
}