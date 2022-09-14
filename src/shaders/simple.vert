#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 uv;
layout(location = 0) out vec2 outPos;
layout(location = 1) out vec2 outUV;

void main() 
{
    gl_Position = vec4(pos, 0.0, 1.0);
    outPos = gl_Position.xy;
    outUV = uv;
}