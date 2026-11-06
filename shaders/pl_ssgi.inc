#include "pl_shader_interop_renderer.h"
#include "pl_math.glsl"

layout(local_size_x = 8, local_size_y = 8) in;
layout(set = 0, binding = 0, rgba16f) uniform image2D target;
layout(set = 0, binding = 1) uniform texture2D sourceTexture;
layout(set = 0, binding = 2) uniform texture2D depthTexture;
layout(set = 0, binding = 3) uniform texture2D normalTexture;
layout(set = 0, binding = 4) uniform texture2D albedoTexture;
layout(set = 0, binding = 5) uniform texture2D materialTexture;
layout(set = 0, binding = 6) uniform sampler pointSampler;
layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    plGpuDynSSGI tData;
} info;

vec3
positionAt(ivec2 pixel, float depth)
{
    vec2 uv = (vec2(pixel) + 0.5) / vec2(info.tData.uWidth, info.tData.uHeight);
    vec4 p = info.tData.tProjectionInv * vec4(uv * 2.0 - 1.0, depth, 1.0);
    return p.xyz / p.w;
}

float
depthAt(ivec2 p)
{
    return texelFetch(sampler2D(depthTexture, pointSampler), p, 0).r;
}

vec3
normalAt(ivec2 p)
{
    return normalize(mat3(info.tData.tView) * Decode(texelFetch(sampler2D(normalTexture, pointSampler), p, 0).xy));
}

bool
inside(ivec2 p)
{
    return all(greaterThanEqual(p, ivec2(0))) && all(lessThan(p, ivec2(info.tData.uWidth, info.tData.uHeight)));
}

// Positive ORM alpha identifies deferred geometry (including terrain).
// Forward prepass pixels have zero ORM alpha but valid albedo alpha.
bool
surfaceAt(ivec2 p)
{
    return texelFetch(sampler2D(materialTexture, pointSampler), p, 0).a > 0.0 || texelFetch(sampler2D(albedoTexture, pointSampler), p, 0).a > 0.0;
}
