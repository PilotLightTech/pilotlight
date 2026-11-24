#ifndef PL_PROBE_COMMON_GLSL
#define PL_PROBE_COMMON_GLSL

// Probe math with no resource dependencies: safe to include from the
// capture/filter compute shaders as well as scene shaders.

//-----------------------------------------------------------------------------
// [SECTION] spherical harmonics (L2)
//-----------------------------------------------------------------------------

float
pl_sh_basis(int iCoefficient, vec3 n)
{
    if(iCoefficient == 0) return 0.2820947918;
    if(iCoefficient == 1) return 0.4886025119 * n.y;
    if(iCoefficient == 2) return 0.4886025119 * n.z;
    if(iCoefficient == 3) return 0.4886025119 * n.x;
    if(iCoefficient == 4) return 1.0925484306 * n.x * n.y;
    if(iCoefficient == 5) return 1.0925484306 * n.y * n.z;
    if(iCoefficient == 6) return 0.3153915653 * (3.0 * n.z * n.z - 1.0);
    if(iCoefficient == 7) return 1.0925484306 * n.x * n.z;
    return 0.5462742153 * (n.x * n.x - n.y * n.y);
}

//-----------------------------------------------------------------------------
// [SECTION] visibility (octahedral moments)
//-----------------------------------------------------------------------------

vec2
pl_probe_oct_encode(vec3 n)
{
    n /= abs(n.x) + abs(n.y) + abs(n.z);
    vec2 tSign = mix(vec2(-1.0), vec2(1.0), greaterThanEqual(n.xy, vec2(0.0)));
    return n.z >= 0.0 ? n.xy : (1.0 - abs(n.yx)) * tSign;
}

vec3
pl_probe_oct_decode(vec2 p)
{
    vec3 n = vec3(p, 1.0 - abs(p.x) - abs(p.y));
    float t = max(-n.z, 0.0);
    n.xy += mix(vec2(t), vec2(-t), greaterThanEqual(n.xy, vec2(0.0)));
    return normalize(n);
}

float
pl_probe_visibility(vec2 tMoments, float fDistance)
{
    if(fDistance <= tMoments.x)
        return 1.0;
    // Half-precision moments can round the variance slightly below zero.
    float fVariance = max(0.0, tMoments.y - tMoments.x * tMoments.x);
    float fDelta = fDistance - tMoments.x;
    float fWeight = fVariance / max(fVariance + fDelta * fDelta, 0.00000001);
    return fWeight * fWeight * fWeight;
}

#endif // PL_PROBE_COMMON_GLSL
