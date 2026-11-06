#ifndef PL_SCREEN_AO_GLSL
#define PL_SCREEN_AO_GLSL

// Probe views leave iAOEnabled zero. Fetch exact active-viewport pixels;
// allocations can be larger than the viewport.
float
pl_screen_ao()
{
    if(tViewInfo.tData.iAOEnabled == 0)
        return 1.0;
    return texelFetch(sampler2D(tScreenAO, tSamplerNearestRepeat), ivec2(gl_FragCoord.xy), 0).r;
}
#endif
