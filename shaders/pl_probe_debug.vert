#version 450

#include "pl_shader_interop_renderer.h"

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    plGpuDynProbeDebug tData;
} tObjectInfo;

layout(location = 0) out vec3 tNormal;
layout(location = 1) flat out int iSample;

const ivec2 atCorners[6] = ivec2[6](ivec2(0, 0), ivec2(1, 0), ivec2(1, 1), ivec2(1, 1), ivec2(0, 1), ivec2(0, 0));

void main()
{
    int iQuad = gl_VertexIndex / 6;
    ivec2 tCorner = ivec2(iQuad % 16, iQuad / 16) + atCorners[gl_VertexIndex % 6];
    float fLongitude = float(tCorner.x) * 6.28318530718 / 16.0;
    float fLatitude = float(tCorner.y) * 3.14159265359 / 8.0;
    tNormal = vec3(sin(fLatitude) * cos(fLongitude), cos(fLatitude), sin(fLatitude) * sin(fLongitude));
    iSample = tObjectInfo.tData.iFirstSample + gl_InstanceIndex;
    ivec3 tCell = ivec3(iSample % tObjectInfo.tData.iGridX,
        (iSample / tObjectInfo.tData.iGridX) % tObjectInfo.tData.iGridY,
        iSample / (tObjectInfo.tData.iGridX * tObjectInfo.tData.iGridY));
    vec3 tPosition = tObjectInfo.tData.tOriginRadius.xyz + vec3(tCell) * tObjectInfo.tData.tSpacing.xyz;
    gl_Position = tObjectInfo.tData.tCameraViewProjection * vec4(tPosition + tNormal * tObjectInfo.tData.tOriginRadius.w, 1.0);
}
