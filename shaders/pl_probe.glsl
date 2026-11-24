#ifndef PL_PROBE_GLSL
#define PL_PROBE_GLSL

// Probe helpers that read scene resources (probe records, selections, bindless
// textures). Include after pl_bg_scene.inc.

#include "pl_probe_common.glsl"

//-----------------------------------------------------------------------------
// [SECTION] diffuse probe volumes
//-----------------------------------------------------------------------------

float
pl_probe_volume_weights(plGpuProbe tProbe, vec3 tPosition, vec3 n, out int aiRows[8], out float afWeights[8])
{
    ivec3 tCount = ivec3(tProbe.iGridX, tProbe.iGridY, tProbe.iGridZ);
    bool bVisibility = tProbe.uVisibilitySampler != 0u;
    vec3 tQuery = tPosition + (bVisibility ? n * tProbe.fNormalBias : vec3(0.0));
    vec3 tExtent = max(tProbe.tMax.xyz - tProbe.tMin.xyz, vec3(0.0001));
    vec3 tGrid = clamp((tQuery - tProbe.tMin.xyz) / tExtent, vec3(0.0), vec3(1.0)) * vec3(tCount - 1);
    ivec3 tCell = ivec3(floor(tGrid));
    vec3 tFraction = fract(tGrid);
    float fWeightSum = 0.0;
    float fVisible = 0.0;
    for(int j = 0; j < 8; j++)
    {
        ivec3 tCorner = ivec3(j & 1, (j >> 1) & 1, (j >> 2) & 1);
        ivec3 tSample = min(tCell + tCorner, tCount - 1);
        vec3 tWeight = mix(vec3(1.0) - tFraction, tFraction, vec3(tCorner));
        int iRow = (tSample.z * tCount.y + tSample.y) * tCount.x + tSample.x;
        aiRows[j] = iRow;
        float w = tWeight.x * tWeight.y * tWeight.z;
        if(bVisibility && w > 0.0)
        {
            vec3 tSamplePosition = tProbe.tMin.xyz + vec3(tSample) * tExtent / vec3(max(tCount - 1, ivec3(1)));
            vec3 tDelta = tQuery - tSamplePosition;
            float fDistance = length(tDelta);
            vec3 d = fDistance > 0.00001 ? tDelta / fDistance : n;
            int iColumns = min(tCount.x * tCount.y * tCount.z, PL_PROBE_VISIBILITY_COLUMNS);
            vec2 tTile = vec2(iRow % iColumns, iRow / iColumns) * float(PL_PROBE_VISIBILITY_TILE);
            vec2 tUV = tTile + 1.0 + (pl_probe_oct_encode(d) * 0.5 + 0.5) * float(PL_PROBE_VISIBILITY_SIZE);
            vec2 tSize = vec2(textureSize(sampler2D(at2DTextures[nonuniformEXT(tProbe.uVisibilitySampler)], tSamplerLinearClamp), 0));
            vec2 tMoments = textureLod(sampler2D(at2DTextures[nonuniformEXT(tProbe.uVisibilitySampler)], tSamplerLinearClamp), tUV / tSize, 0.0).rg;
            float fVisibility = pl_probe_visibility(tMoments, min(fDistance / tProbe.tMin.w, 1.0));
            fVisible += w * fVisibility;
            float fNormalWeight = fDistance > 0.00001 ? max(0.01, 0.5 - 0.5 * dot(n, d)) : 1.0;
            w *= fVisibility * fNormalWeight * fNormalWeight;
        }
        afWeights[j] = w;
        fWeightSum += w;
    }
    if(bVisibility)
    {
        // Fade unsupported queries to black instead of amplifying tiny leaking weights or using sky.
        for(int j = 0; j < 8; j++)
            afWeights[j] /= max(fWeightSum, 0.01);
    }
    return bVisibility ? fVisible : 1.0;
}

vec3
pl_probe_visibility_debug(uint uSelection, vec3 tPosition, vec3 n)
{
    uSelection &= ~PL_PROBE_SELECTION_FALLBACK_FLAG;
    if(uSelection == 0u)
        return vec3(0.0, 0.2, 1.0);
    plGpuProbeSelection tSelection = tProbeSelections.atData[uSelection];
    int aiProbes[3] = int[3](tSelection.iProbe0, tSelection.iProbe1, tSelection.iProbe2);
    for(int i = 0; i < tSelection.iCount; i++)
    {
        plGpuProbe tProbe = tProbeData.atData[aiProbes[i]];
        if(tProbe.iGridX == 0 || tProbe.uVisibilitySampler == 0u)
            continue;
        int aiRows[8]; float afWeights[8];
        float fVisibility = pl_probe_volume_weights(tProbe, tPosition, n, aiRows, afWeights);
        return mix(vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), fVisibility);
    }
    return vec3(0.0, 0.2, 1.0);
}

//-----------------------------------------------------------------------------
// [SECTION] debug
//-----------------------------------------------------------------------------

// Colors identify the nearest probe, not the deduplicated table entry.
vec3 pl_probe_debug_color(uint selection)
{
    if((selection & PL_PROBE_SELECTION_FALLBACK_FLAG) != 0u)
        return vec3(1.0, 0.0, 1.0);
    plGpuProbeSelection probes = tProbeSelections.atData[selection];
    if(probes.iCount == 0)
        return vec3(0.0);
    if(probes.iProbe0 == 0)
        return vec3(0.5);
    float hue = fract(float(probes.iProbe0) * 0.61803398875);
    vec3 rgb = clamp(abs(fract(hue + vec3(0.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0) - 1.0, 0.0, 1.0);
    return vec3(0.2) + 0.6 * rgb;
}

#endif // PL_PROBE_GLSL
