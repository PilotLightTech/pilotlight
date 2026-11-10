#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "pl_shader_interop_renderer.h"
#include "pl_bg_scene.inc"
#include "pl_bg_view.inc"
#include "pl_math.glsl"
#include "pl_brdf.glsl"

//-----------------------------------------------------------------------------
// [SECTION] bind group 2
//-----------------------------------------------------------------------------

layout(input_attachment_index = 1, set = 2, binding = 0)  uniform subpassInput tAlbedoSampler;
layout(input_attachment_index = 2, set = 2, binding = 1)  uniform subpassInput tNormalTexture;
layout(input_attachment_index = 3, set = 2, binding = 2)  uniform subpassInput tAOMetalRoughnessTexture;
layout(input_attachment_index = 0, set = 2, binding = 3)  uniform subpassInput tDepthSampler;

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    plGpuDynDeferredLighting tData;
} tObjectInfo;

//-----------------------------------------------------------------------------
// [SECTION] input & output
//-----------------------------------------------------------------------------

layout(location = 0) out vec4 outColor;

// layout(location = 0) in vec2 tUV;

const int iMaterialFlags = 0;
#include "pl_lighting.glsl"
#include "pl_material_info.glsl"
#include "pl_fog.glsl"
#include "pl_deferred_punctual.glsl"
#include "pl_cluster_lighting.glsl"

void main()
{
    vec4 packedMaterial = subpassLoad(tAOMetalRoughnessTexture);
    // ORM alpha holds a positive probe-selection index for geometry; zero is background.
    float depth = subpassLoad(tDepthSampler).r;
    vec2 ndc = gl_FragCoord.xy / tViewInfo.tData.tViewportSize.xy * 2.0 - 1.0;
    uint viewIndex = tObjectInfo.tData.uGlobalIndex;
    vec4 viewPosition = tViewInfo.tData.tCameraProjectionInv[viewIndex] * vec4(ndc, depth, 1.0);
    viewPosition /= viewPosition.w;
    vec3 worldPosition = (tViewInfo.tData.tCameraViewInv[viewIndex] * viewPosition).xyz;
    pl_shadow_surface(worldPosition.xyz);
    if(packedMaterial.a == 0.0)
        discard;

    vec3 n = Decode(subpassLoad(tNormalTexture).xy);
    vec3 v = normalize(tViewInfo.tData.tCameraPos.xyz - worldPosition);
    MaterialInfo materialInfo;
    materialInfo.baseColor = subpassLoad(tAlbedoSampler).rgb;
    materialInfo.f0_dielectric = vec3(0.04);
    materialInfo.specularWeight = 1.0;
    materialInfo.f90 = vec3(1.0);
    materialInfo.f90_dielectric = vec3(1.0);
    materialInfo.perceptualRoughness = packedMaterial.b;
    materialInfo.alphaRoughness = packedMaterial.b * packedMaterial.b;
    materialInfo.metallic = packedMaterial.g;
    bool shadows = bool(tObjectInfo.tData.iObjectShaderFlags & PL_OBJECT_SHADER_FLAG_SHADOWS)
        && bool(tGpuScene.tData.iSceneFlags & PL_SCENE_FLAG_SHADOWS);
    int base = pl_cluster_base(worldPosition, viewIndex);
    int pointCount = pl_cluster_count(base, 0, tViewInfo.tData.iClusterPointCount);
    int spotCount = pl_cluster_count(base, 1, tViewInfo.tData.iClusterSpotCount);
    vec3 color = vec3(0.0);
    for(int i = 0; i < pointCount; ++i)
        color += pl_deferred_point(pl_cluster_light(base, 0, i), worldPosition, n, v, materialInfo, shadows);
    for(int i = 0; i < spotCount; ++i)
        color += pl_deferred_spot(pl_cluster_light(base, 1, i), worldPosition, n, v, materialInfo, shadows);
    outColor = vec4(color, 0.0);
}
