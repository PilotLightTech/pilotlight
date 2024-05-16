#version 450 core

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

layout(set = 0, binding = 0) uniform _plObjectInfo { mat4 tMVP;} tObjectInfo;

//-----------------------------------------------------------------------------
// [SECTION] input
//-----------------------------------------------------------------------------

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;

//-----------------------------------------------------------------------------
// [SECTION] output
//-----------------------------------------------------------------------------

layout(location = 0) out struct { vec4 Color; } Out;


void main()
{
    Out.Color = aColor;
    gl_Position = tObjectInfo.tMVP * vec4(aPos, 1.0);
}