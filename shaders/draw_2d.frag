#version 450 core
#extension GL_ARB_separate_shader_objects : enable

//-----------------------------------------------------------------------------
// [SECTION] bind group 0
//-----------------------------------------------------------------------------

layout(set = 0, binding = 0)  uniform sampler tFontSampler;

//-----------------------------------------------------------------------------
// [SECTION] bind group 1
//-----------------------------------------------------------------------------

layout(set = 1, binding = 0)  uniform texture2D tFontAtlas;

//-----------------------------------------------------------------------------
// [SECTION] input
//-----------------------------------------------------------------------------

layout(location = 0) in struct { vec4 Color; vec2 UV; } In;

//-----------------------------------------------------------------------------
// [SECTION] output
//-----------------------------------------------------------------------------

layout(location = 0) out vec4 fColor;

void main()
{
    fColor = In.Color * texture(sampler2D(tFontAtlas, tFontSampler), In.UV.st);
}