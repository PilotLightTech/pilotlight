#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "pl_shader_interop_renderer.h"
#include "bg_scene.inc"

//-----------------------------------------------------------------------------
// [SECTION] bind group 0
//-----------------------------------------------------------------------------

layout(set = 1, binding = 0)  uniform texture2D tColorTexture;
layout(set = 1, binding = 1)  uniform texture2D tMaskTexture;

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    plGpuDynPost tData;
} tObjectInfo;

//-----------------------------------------------------------------------------
// [SECTION] input
//-----------------------------------------------------------------------------

layout(location = 0) in vec2 tUV;

//-----------------------------------------------------------------------------
// [SECTION] output
//-----------------------------------------------------------------------------

layout(location = 0) out vec4 outColor;

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void
main() 
{

    vec2 tUVMod = tUV;
    tUVMod.x *= tObjectInfo.tData.fXScale;
    tUVMod.y *= tObjectInfo.tData.fYScale;
    vec4 color = texture(sampler2D(tColorTexture, tSamplerLinearRepeat), tUVMod);
    vec2 closestSeed = texture(sampler2D(tMaskTexture, tSamplerLinearRepeat), tUVMod).xy;
    vec2 h = closestSeed - tUVMod;
    float xdist = h.x * float(textureSize(sampler2D(tColorTexture, tSamplerLinearRepeat),0).x);
    float ydist = h.y * float(textureSize(sampler2D(tColorTexture, tSamplerLinearRepeat),0).y);
    float tdist2 = xdist * xdist + ydist * ydist;
    float dist = distance(closestSeed, tUVMod);
    if (closestSeed.x > 0 && closestSeed.y > 0 && dist > 0.0001 && tdist2 < tObjectInfo.tData.fTargetWidth * tObjectInfo.tData.fTargetWidth)
    {
        color = tObjectInfo.tData.tOutlineColor;
    }
    outColor = color;
}