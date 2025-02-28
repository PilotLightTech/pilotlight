#version 450
#extension GL_ARB_separate_shader_objects : enable

//-----------------------------------------------------------------------------
// [SECTION] bind group 0
//-----------------------------------------------------------------------------

layout(set = 0, binding = 0) uniform _plGlobalInfo
{
    vec4 tViewportSize;
    vec4 tViewportInfo;
    vec4 tCameraPos;
    mat4 tCameraView;
    mat4 tCameraProjection;
    mat4 tCameraViewProjection;
} tGlobalInfo;

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

layout(std140, set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    uint uID;
    vec4 tMousePos;
    mat4 tModel;
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
    vec4 pos = tObjectInfo.tModel * inPosition;
    gl_Position = tGlobalInfo.tCameraViewProjection * pos;
    tShaderOut.uID = tObjectInfo.uID;
    tShaderOut.tMousePos = tObjectInfo.tMousePos.xy;
}