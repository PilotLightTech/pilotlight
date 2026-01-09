#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "pl_shader_interop_terrain.h"
#include "terrain.glsl"

layout(set = 0, binding = 0)  uniform sampler tSampler;
layout(set = 0, binding = 1)  uniform texture2D tHeightMap;
layout(set = 0, binding = 2)  uniform texture2D tNoiseTexture;
layout(set = 0, binding = 3)  uniform texture2D tDiffuseTexture;
layout(set = 0, binding = 4)  uniform sampler tMirrorSampler;
layout(set = 0, binding = 5)  uniform texture2D tHeightMap2;

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    plTerrainDynamicData tInfo;
} tObjectInfo;


layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;

layout(location = 0) out struct plShaderOut {
    vec3 tPosition;
    vec2 tUV;
} tShaderIn;

void main()
{
    float fMaxHeight = tObjectInfo.tInfo.fGlobalMaxHeight;
    float fMinHeight = tObjectInfo.tInfo.fGlobalMinHeight;

    tShaderIn.tUV = inUV;

    vec4 tPosition = vec4(inPos, 1.0);

    tPosition.y = texture(sampler2D(tHeightMap, tMirrorSampler), inUV).w * (fMaxHeight - fMinHeight) + tObjectInfo.tInfo.fGlobalMinHeight;

    // TODO: handle properly

    // float fRatio0 = clamp((tObjectInfo.tInfo.tPos.y - tObjectInfo.tInfo.fGlobalMinHeight) / 100000.0, 0.0, 1.0);
    // // tPosition.y += -100.0 * (fRatio0);
    // tPosition.y += -100.0 + 100.0 * (fRatio0);

    tShaderIn.tPosition = tPosition.xyz;
    gl_Position = tObjectInfo.tInfo.tCameraViewProjection * tPosition;
}

