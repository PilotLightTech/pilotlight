#include "pl_shader_interop_renderer.h"
#include "pl_math.glsl"

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    plGpuDynAOData tData;
} tAO;

vec3
aoPosition(vec2 uv, float depth)
{
    vec4 p = tAO.tData.tProjectionInv * vec4(uv * 2.0 - 1.0, depth, 1.0);
    return p.xyz / p.w;
}

ivec2
aoSize(int level)
{
    int scale = 1 << level;
    return (ivec2(tAO.tData.tViewportSize) + scale - 1) / scale;
}

bool
aoInside(ivec2 p, ivec2 size)
{
    return all(greaterThanEqual(p, ivec2(0))) && all(lessThan(p, size));
}
