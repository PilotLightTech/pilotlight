#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0)  uniform sampler tDefaultSampler;
layout (set = 0, binding = 1) uniform texture2D tTexture;

layout(set = 1, binding = 0) uniform _plObjectInfo
{
    int iDataOffset;
    int iVertexOffset;
} tObjectInfo;

//-----------------------------------------------------------------------------
// [SECTION] input
//-----------------------------------------------------------------------------

layout(location = 0) in struct plShaderOut {
    vec2 tUV;
} tShaderIn;

//-----------------------------------------------------------------------------
// [SECTION] output
//-----------------------------------------------------------------------------

layout(location = 0) out vec4 outColor;

//-----------------------------------------------------------------------------
// [SECTION] helpers
//-----------------------------------------------------------------------------

const float GAMMA = 2.2;
const float INV_GAMMA = 1.0 / GAMMA;

vec3
pl_linear_to_srgb(vec3 color)
{
    return pow(color, vec3(INV_GAMMA));
}

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void
main() 
{
    outColor = vec4(pl_linear_to_srgb(texture(sampler2D(tTexture, tDefaultSampler), tShaderIn.tUV).rgb), 1.0);
}