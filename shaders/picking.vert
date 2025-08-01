#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "pl_shader_interop_renderer.h"

//-----------------------------------------------------------------------------
// [SECTION] bind group 0
//-----------------------------------------------------------------------------

layout(set = 0, binding = 0) uniform _plGlobalInfo
{
    plGpuGlobalData tData;
} tGlobalInfo;

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

layout(std140, set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    plGpuDynPick tData;
} tObjectInfo;

//-----------------------------------------------------------------------------
// [SECTION] input & output
//-----------------------------------------------------------------------------

// input
layout(location = 0) in vec3 inPos;

// output
layout(location = 0) flat out struct plShaderOut {
    vec2 tMousePos;
    uint uID;
} tShaderOut;

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void main()
{
    vec4 inPosition  = vec4(inPos, 1.0);
    vec4 pos = tObjectInfo.tData.tModel * inPosition;
    gl_Position = tGlobalInfo.tData.tCameraViewProjection * pos;
    tShaderOut.uID = tObjectInfo.tData.uID;
    tShaderOut.tMousePos = tObjectInfo.tData.tMousePos.xy;
}