#ifndef PL_CLUSTER_LIGHTING_GLSL
#define PL_CLUSTER_LIGHTING_GLSL

// Use projected world position instead of framebuffer coordinates so viewport
// origin conventions do not affect cluster lookup.
int
pl_cluster_base(vec3 worldPosition, uint viewIndex)
{
    if(tViewInfo.tData.iClusterEnabled == 0 || viewIndex != 0u)
        return -1;
    vec4 position = tViewInfo.tData.tCameraView[0] * vec4(worldPosition, 1.0);
    float nearZ = tViewInfo.tData.fCameraNearZ;
    float farZ = nearZ + tViewInfo.tData.fCameraRange;
    if(position.z < nearZ || position.z > farZ)
        return -1;
    vec4 clip = tViewInfo.tData.tCameraProjection[0] * position;
    vec2 uv = clip.xy / clip.w * 0.5 + 0.5;
    ivec2 tile = clamp(ivec2(floor(uv * vec2(PL_CLUSTER_X, PL_CLUSTER_Y))), ivec2(0), ivec2(PL_CLUSTER_X - 1, PL_CLUSTER_Y - 1));
    int slice = clamp(int(floor(log(position.z / nearZ) / log(farZ / nearZ) * float(PL_CLUSTER_Z))), 0, PL_CLUSTER_Z - 1);
    return ((slice * PL_CLUSTER_Y + tile.y) * PL_CLUSTER_X + tile.x) * PL_CLUSTER_STRIDE;
}

int
pl_cluster_count(int base, int type, int fullCount)
{
    if(base < 0)
        return fullCount;
    uint count = tClusterData.auData[base + type];
    return count > PL_CLUSTER_LIGHT_CAPACITY ? fullCount : int(count);
}

int
pl_cluster_light(int base, int type, int index)
{
    if(base < 0 || tClusterData.auData[base + type] > PL_CLUSTER_LIGHT_CAPACITY)
        return index;
    return int(tClusterData.auData[base + 2 + type * PL_CLUSTER_LIGHT_CAPACITY + index]);
}

#endif
