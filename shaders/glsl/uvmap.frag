#version 450
#extension GL_ARB_separate_shader_objects : enable

//-----------------------------------------------------------------------------
// [SECTION] input & output
//-----------------------------------------------------------------------------

// input
layout(location = 0) in struct plShaderIn {
    vec2 tUV;
} tShaderIn;

// output
layout(location = 0) out vec4 outColor;

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void main() 
{
    outColor = vec4(tShaderIn.tUV, 0, 1);
}