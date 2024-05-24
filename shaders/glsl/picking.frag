#version 450
#extension GL_ARB_separate_shader_objects : enable
precision mediump float;

//-----------------------------------------------------------------------------
// [SECTION] input & output
//-----------------------------------------------------------------------------

// input
layout(location = 0) in struct plShaderIn {
    vec4 tColor;
} tShaderIn;

// output
layout(location = 0) out vec4 outColor;


//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void main() 
{
    outColor = tShaderIn.tColor;
}