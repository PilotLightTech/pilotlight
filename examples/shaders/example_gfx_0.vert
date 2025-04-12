#version 450
#extension GL_ARB_separate_shader_objects : enable

// input
layout(location = 0) in vec2 inPos;
layout(location = 1) in vec4 inColor;

// output
layout(location = 0) out struct plShaderOut {
    vec4 tColor;
} tShaderOut;

void main() 
{
    gl_Position = vec4(inPos, 0.0, 1.0);
    tShaderOut.tColor = inColor;
}