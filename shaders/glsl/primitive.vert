#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 pos;
layout(location = 1) in vec4 color;
layout(location = 0) out vec4 outPos;
layout(location = 1) out vec4 outColor;

void main() 
{
    gl_Position = vec4(pos, 1.0);
    outPos = gl_Position;
    outColor = color;
}