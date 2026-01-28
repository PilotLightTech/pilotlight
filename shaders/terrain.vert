#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "pl_shader_interop_terrain.h"

vec3
Decode( vec2 f )
{
    f = f * 2.0 - 1.0;
 
    // https://twitter.com/Stubbesaurus/status/937994790553227264
    vec3 n = vec3( f.x, f.y, 1.0 - abs( f.x ) - abs( f.y ) );
    float t = max( -n.z, 0.0 );
    n.x += n.x >= 0.0 ? -t : t;
    n.y += n.y >= 0.0 ? -t : t;
    return normalize( n );
}

// input
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inNormal;

// output
layout(location = 0) out struct plShaderOut {
    vec4 tColor;
    vec3 tWorldPosition;
    vec3 tWorldNormal;
} tShaderOut;

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    plGpuDynTerrainData tData;
} tDynamicData;

void main() 
{
    gl_Position = tDynamicData.tData.tMvp * vec4(inPos, 1.0);
    // gl_Position = tDynamicData.mvp * vec4(inPos.x, 0.0, inPos.z, 1.0);
    // float fScale = max((inPos.y + 40.0) / 552.0, 0.2);

    // ellipsoid stuff
    
    // float blah = length(inPos) - 1737400.0 + 18256.0;
    // float fScale = max(blah / (14052.0 - -18256.0), 0.0);

    // float blah = length(inPos) - 1737400.0 + 4380.518;
    // float fScale = max(blah / (2713.087 - -4380.518), 0.0);

    // tShaderOut.tColor = vec4(fScale.xxx, 1.0);
    tShaderOut.tWorldPosition = inPos;
    tShaderOut.tWorldNormal = Decode(inNormal);

    vec3 atColors[8];
    float fColorStrength = 0.1;
    atColors[0] = vec3(fColorStrength, 0.0, 0.0);
    atColors[1] = vec3(0.0, fColorStrength, 0.0);
    atColors[2] = vec3(0.0, 0.0, fColorStrength);
    atColors[3] = vec3(fColorStrength, fColorStrength, 0.0);
    atColors[4] = vec3(fColorStrength, 0.0, fColorStrength);
    atColors[5] = vec3(0.0, fColorStrength, fColorStrength);
    atColors[6] = vec3(fColorStrength, fColorStrength, fColorStrength);
    atColors[7] = vec3(fColorStrength * 4, fColorStrength, fColorStrength);

    tShaderOut.tColor.rgb = atColors[tDynamicData.tData.iLevel];

    if(bool(tDynamicData.tData.tFlags & PL_TERRAIN_SHADER_FLAGS_WIREFRAME))
    {
        tShaderOut.tColor.rgb += vec3(0.3);
    }

    // tShaderOut.tColor = vec4(1.0);
}