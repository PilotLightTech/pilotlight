#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "bg_scene.inc"

layout(constant_id = 0) const int iRenderingFlags = 0;
layout(constant_id = 1) const int iLightCount = 0;
layout(constant_id = 2) const int iProbeCount = 0;

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

// input
layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUV;

// output
layout(location = 0) out struct plShaderOut {
    vec2 tUV;
} tShaderOut;

void main() 
{

    vec4 inPosition  = vec4(inPos, 0.0, 1.0);
    gl_Position = inPosition;
    tShaderOut.tUV = inUV;
}