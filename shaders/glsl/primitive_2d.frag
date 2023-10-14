#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "common_2d.glsl"

layout(location = 0) out vec4 outColor;

// output
layout(location = 0) in struct plShaderIn {
    vec4 tPosition;
    vec2 tUV;
    vec4 tColor;
} tShaderIn;

void main() 
{
    outColor = tShaderIn.tColor * texture(tSampler0, tShaderIn.tUV);
    outColor = tShaderInfo.shaderSpecific + outColor;
}