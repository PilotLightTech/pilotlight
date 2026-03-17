#version 450
#extension GL_ARB_separate_shader_objects : enable

//-----------------------------------------------------------------------------
// [SECTION] input & output
//-----------------------------------------------------------------------------

// input
layout(location = 0) in vec2 tUV;

// output
layout(location = 0) out vec2 outColor;

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void main() 
{
    outColor = vec2(tUV);
}