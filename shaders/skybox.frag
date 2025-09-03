#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "pl_shader_interop_renderer.h"
#include "bg_scene.inc"
#include "bg_view.inc"
#include "math.glsl"
#include "fog.glsl"

//-----------------------------------------------------------------------------
// [SECTION] bind group 2
//-----------------------------------------------------------------------------

layout(set = 2, binding = 0) uniform textureCube samplerCubeMap;

//-----------------------------------------------------------------------------
// [SECTION] input
//-----------------------------------------------------------------------------

layout(location = 0) in struct plShaderOut {
    vec3 tWorldPosition;
} tShaderIn;

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
    vec3 tVectorOut = normalize(tShaderIn.tWorldPosition);
    outColor = vec4(texture(samplerCube(samplerCubeMap, tSamplerLinearRepeat), tVectorOut).rgb, 1.0);
    
    if(bool(tGpuScene.tData.iSceneFlags & PL_SCENE_FLAG_HEIGHT_FOG) || bool(tGpuScene.tData.iSceneFlags & PL_SCENE_FLAG_LINEAR_FOG))
    {
        tVectorOut.y *= -1.0;
        outColor = fog(outColor, tVectorOut * tViewInfo.tData.fFogCutOffDistance * 0.75);
    }
    // outColor.rgb = outColor.rgb * (1.0 - tViewInfo.tData.fFogMaxOpacity) + tViewInfo.tData.fFogMaxOpacity * tViewInfo.tData.tFogColor;
}