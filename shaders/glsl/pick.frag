#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in struct plShaderIn {
    vec4 tColor;
} tShaderIn;

layout(location = 0) out vec4 outColor;

void main() 
{
    outColor = tShaderIn.tColor;
}