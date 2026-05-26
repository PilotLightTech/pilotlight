#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "pl_shader_interop_renderer.h"
#include "pl_bg_scene.inc"
#include "pl_bg_view.inc"
#include "pl_math.glsl"
#include "pl_fog.glsl"

//-----------------------------------------------------------------------------
// [SECTION] bind group 2
//-----------------------------------------------------------------------------

layout(set = 2, binding = 0) uniform textureCube samplerCubeMap;

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA {
    plGpuDynSkybox tData;
} tObjectInfo;

//-----------------------------------------------------------------------------
// [SECTION] input
//-----------------------------------------------------------------------------

// layout(location = 0) in struct plShaderOut {
//     vec3 tWorldPosition;
// } tShaderIn;

//-----------------------------------------------------------------------------
// [SECTION] output
//-----------------------------------------------------------------------------

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

vec3 get_perspective_sky_dir(vec2 uv)
{
    // vec2 ndc = uv - 1.0;
    vec2 ndc = 2.0 * uv - 1.0;

    // Because your projection currently has negative X and Y scale:
    // ndc.x *= -1.0;
    // ndc.y *= -1.0;

    vec4 viewPos = tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tCameraProjectionInv * vec4(ndc, 0.0, 1.0);
    viewPos.xyz /= viewPos.w;
    

    vec3 viewDir = normalize(viewPos.xyz);

    return normalize((tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tInvViewMatNoTranslation * vec4(viewDir, 0.0)).xyz);
}

vec3 get_ortho_sky_dir_fake(vec2 uv)
{
    vec2 ndc = uv * 2.0 - 1.0;

    // Match your negative projection X/Y convention.
    ndc.x *= -1.0;
    ndc.y *= -1.0;

    // Fake skybox FOV. This is only for the background appearance.
    float fakeTanHalfFovY = tan(radians(60.0) * 0.5);

    vec3 viewDir = normalize(vec3(
        ndc.x * fakeTanHalfFovY * tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].fAspectRatio,
        ndc.y * fakeTanHalfFovY,
        1.0
    ));

    return normalize((tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tInvViewMatNoTranslation * vec4(viewDir, 0.0)).xyz);
}

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

// void
// main() 
// {
//     vec3 tVectorOut = normalize(tShaderIn.tWorldPosition);
//     outColor = vec4(texture(samplerCube(samplerCubeMap, tSamplerLinearRepeat), tVectorOut).rgb, 1.0);
    
//     if(bool(tGpuScene.tData.iSceneFlags & PL_SCENE_FLAG_HEIGHT_FOG) || bool(tGpuScene.tData.iSceneFlags & PL_SCENE_FLAG_LINEAR_FOG))
//     {
//         tVectorOut.y *= -1.0;
//         outColor = fog(outColor, tVectorOut * tViewInfo.tData.fFogCutOffDistance * 0.75);
//     }
//     // outColor.rgb = outColor.rgb * (1.0 - tViewInfo.tData.fFogMaxOpacity) + tViewInfo.tData.fFogMaxOpacity * tViewInfo.tData.tFogColor;
// }

void main()
{
    vec3 tVectorOut;

    if(tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].iCameraProjectType == 1)
        tVectorOut = get_ortho_sky_dir_fake(inUV);
    else
        tVectorOut = get_perspective_sky_dir(inUV);

    tVectorOut.z *= -1.0;

    outColor = vec4(texture(samplerCube(samplerCubeMap, tSamplerLinearRepeat), tVectorOut).rgb, 1.0);

    if(bool(tGpuScene.tData.iSceneFlags & PL_SCENE_FLAG_HEIGHT_FOG) ||
       bool(tGpuScene.tData.iSceneFlags & PL_SCENE_FLAG_LINEAR_FOG))
    {
        vec3 fogDir = tVectorOut;
        fogDir.y *= -1.0;

        outColor = fog(
            outColor,
            fogDir * tViewInfo.tData.fFogCutOffDistance * 0.75
        );
    }
}