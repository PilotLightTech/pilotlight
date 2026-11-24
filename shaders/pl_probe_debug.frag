#version 450
#extension GL_EXT_nonuniform_qualifier : enable

#include "pl_shader_interop_renderer.h"
#include "pl_bg_scene.inc"
#include "pl_probe_common.glsl"

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    plGpuDynProbeDebug tData;
} tObjectInfo;

layout(location = 0) in vec3 tNormal;
layout(location = 1) flat in int iSample;
layout(location = 0) out vec4 outColor;

void main()
{
    if(tObjectInfo.tData.uDiffuseSHSampler == 0xffffffffu)
    {
        outColor = vec4(1.0, 0.7, 0.1, 1.0);
        return;
    }
    vec3 n = normalize(tNormal);
    vec3 tLight = vec3(0.0);
    for(int i = 0; i < 9; i++)
        tLight += texelFetch(sampler2D(at2DTextures[nonuniformEXT(tObjectInfo.tData.uDiffuseSHSampler)], tSamplerNearestClamp), ivec2(i, iSample), 0).rgb * pl_sh_basis(i, n);
    outColor = vec4(max(tLight, vec3(0.0)), 1.0);
}
