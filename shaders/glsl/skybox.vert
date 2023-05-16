#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "common.glsl"

// input
layout(location = 0) in vec3 inPos;

// output
layout(location = 0) out struct plShaderOut {
    vec3 tPosition;
    vec3 tWorldPosition;
} tShaderOut;

void main() 
{
    gl_Position = tGlobalInfo.tCameraViewProj * vec4(inPos, 1.0);
    gl_Position.w = gl_Position.z;
    tShaderOut.tPosition = gl_Position.xyz;
    tShaderOut.tWorldPosition = inPos;
}