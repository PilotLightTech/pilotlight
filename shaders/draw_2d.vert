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
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

layout(set = 2, binding = 0) uniform PL_DYNAMIC_DATA {
    vec2 uScale;
    vec2 uTranslate;
} tObjectInfo;

//-----------------------------------------------------------------------------
// [SECTION] input
//-----------------------------------------------------------------------------

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in uint aColor;

//-----------------------------------------------------------------------------
// [SECTION] output
//-----------------------------------------------------------------------------

layout(location = 0) out struct { vec4 Color; vec2 UV; } Out;


void main()
{
    Out.Color = unpackUnorm4x8(aColor);
    Out.UV = aUV;
    gl_Position = vec4(aPos * tObjectInfo.uScale + tObjectInfo.uTranslate, 0, 1);
}