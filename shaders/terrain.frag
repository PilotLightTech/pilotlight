#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "pl_shader_interop_terrain.h"

//-----------------------------------------------------------------------------
// [SECTION] input & output
//-----------------------------------------------------------------------------

// input
layout(location = 0) in struct plShaderIn {
    vec4 tColor;
    vec3 tWorldPosition;
    vec3 tWorldNormal;
} tShaderIn;

// output
layout(location = 0) out vec4 outColor;

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    plGpuDynTerrainData tData;
} tDynamicData;

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void main() 
{
    // vec3 dx = dFdx(tShaderIn.tWorldPosition);
    // vec3 dy = dFdy(tShaderIn.tWorldPosition);
    // vec3 normal = normalize(cross(dx, dy));
    vec3 normal = normalize(tShaderIn.tWorldNormal);

    vec3 sunlightColor = vec3(1.0, 1.0, 1.0);
    vec3 diffuse = vec3(0.5);
    vec3 ambient = vec3(0);
    
    vec3 w_i = normalize(-vec3(-1.0, -1.0, -1.0));
    // vec3 w_i = normalize(-vec3(0.0, 1.0, 0.0));

    outColor.xyz = diffuse * (max(0.0, dot(normal, w_i)) * sunlightColor + ambient);
    // outColor.rgb = normal;

    if(bool(tDynamicData.tData.tFlags & PL_TERRAIN_SHADER_FLAGS_SHOW_LEVELS))
    {
        outColor += tShaderIn.tColor;
    }

    if(bool(tDynamicData.tData.tFlags & PL_TERRAIN_SHADER_FLAGS_WIREFRAME))
    {
        outColor = tShaderIn.tColor;
    }

}