#version 450
#extension GL_ARB_separate_shader_objects : enable

//-----------------------------------------------------------------------------
// [SECTION] bind group 0
//-----------------------------------------------------------------------------

layout(set = 0, binding = 0)  uniform sampler tSampler;
layout(set = 0, binding = 1)  uniform texture2D tColorTexture;
layout(set = 0, binding = 2)  uniform texture2D tMaskTexture;

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

layout(set = 1, binding = 0) uniform _plObjectInfo
{
    float fTargetWidth;
    vec4 tOutlineColor;
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

    vec4 color = texture(sampler2D(tColorTexture, tSampler), tShaderIn.tUV);
    vec2 closestSeed = texture(sampler2D(tMaskTexture, tSampler), tShaderIn.tUV).xy;
    vec2 h = closestSeed - tShaderIn.tUV;
    float xdist = h.x * float(textureSize(sampler2D(tColorTexture, tSampler),0).x);
    float ydist = h.y * float(textureSize(sampler2D(tColorTexture, tSampler),0).y);
    float tdist2 = xdist * xdist + ydist * ydist;
    float dist = distance(closestSeed, tShaderIn.tUV);
    if (closestSeed.x > 0 && closestSeed.y > 0 && dist > 0 && tdist2 < tObjectInfo.fTargetWidth * tObjectInfo.fTargetWidth)
    {
        color = tObjectInfo.tOutlineColor;
    }
    outColor = color;
    outColor.rgb = pl_linear_to_srgb(outColor.rgb);
}