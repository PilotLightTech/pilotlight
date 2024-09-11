#version 450
#extension GL_ARB_separate_shader_objects : enable

//-----------------------------------------------------------------------------
// [SECTION] bind group 0
//-----------------------------------------------------------------------------

layout(set = 0, binding = 0)  uniform sampler tSampler;
layout(set = 0, binding = 1)  uniform texture2D tTexture;

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

layout(set = 1, binding = 0) uniform PL_DYNAMIC_DATA
{
    vec4 tTintColor;
} tDynamicData;

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
    vec4 tColor = texture(sampler2D(tTexture, tSampler), tShaderIn.tUV);
    outColor = tColor * tDynamicData.tTintColor;
}