#ifndef PL_PROBE_DEBUG_GLSL
#define PL_PROBE_DEBUG_GLSL

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
#endif
