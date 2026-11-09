#ifndef PL_CLOUD_TEMPORAL_MATH_GLSL
#define PL_CLOUD_TEMPORAL_MATH_GLSL

// Scalar helpers
float
cloudTemporalPixel(float sourceCenter, int viewport)
{
    int lowSize = (viewport + 1) / 2;
    float pixel = (sourceCenter - 1.5) * 0.5;
    // The final sample in an odd viewport is only one full-res pixel from
    // its neighbor. All other sample centers are two full-res pixels apart.
    if(viewport % 2 != 0 && pixel > float(lowSize - 2))
        pixel = float(lowSize - 2) + sourceCenter - (float(viewport) - 1.5);
    return pixel;
}

bool
cloudTemporalDepthMatches(float expected, float previous)
{
    // Negative = geometry, zero = empty sky. Both must reject history.
    return previous > 0.0 && abs(previous - expected) <= max(0.05, expected * 0.1);
}

#endif
