#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
// const float M_PI = 3.141592653589793;
// const int iMaterialFlags = 0;
#include "pl_math.glsl"
#include "pl_shader_interop_terrain.h"
#include "pl_bg_scene.inc"
#include "pl_bg_view.inc"
// #include "pl_brdf.glsl"
// #include "pl_lighting.glsl"


//-----------------------------------------------------------------------------
// [SECTION] input & output
//-----------------------------------------------------------------------------

// input
layout(location = 0) in struct plShaderIn {
    vec4 tColor;
    vec3 tWorldPosition;
    vec3 tWorldNormal;
    vec2 tUV;
} tShaderIn;

// output
layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec2 outNormal;
layout(location = 2) out vec4 outAOMetalnessRoughness;

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
    // vec3 normal = normalize(cross(dy, dx));

    vec2 tUVActual = tShaderIn.tUV;
    tUVActual = tUVActual * tDynamicData.tData.tUVInfo.xy;
    tUVActual = tUVActual + tDynamicData.tData.tUVInfo.zw;

    vec3 normal = normalize(tShaderIn.tWorldNormal);
    // vec4 diffuse = pl_srgb_to_linear(texture(sampler2D(at2DTextures[tDynamicData.tData.uTextureIndex], tSamplerLinearRepeat), tUVActual));

    float fSlopeThing = dot(normal, vec3(0, 1, 0));

    // if(tShaderIn.tWorldPosition.y > 200.0)
    // {
    //     outAlbedo = vec4(1.0, 1.0, 1.0, 1.0);
    // }
    // else if(fSlopeThing > 0.94)
    // {
    //     outAlbedo = vec4(0.05, 0.3, 0.05, 1.0);
    // }
    // else if(fSlopeThing > 0.8)
    // {
    //     outAlbedo = vec4(0.66, 0.598, 0.402, 1.0);
    // }
    // else
    // {
    //     outAlbedo = vec4(0.05, 0.05, 0.05, 1.0);
    // }
    outAlbedo = vec4(0.1, 0.1, 0.1, 1.0);
    outNormal = Encode(normal);
    outAOMetalnessRoughness = vec4(1.0, 1.0, 1.0, 1.0);


    // if(bool(tDynamicData.tData.tFlags & PL_TERRAIN_SHADER_FLAGS_SHOW_LEVELS))
    // {
    //     // outColor = tShaderIn.tColor;
        // outAlbedo = tShaderIn.tColor;
        // outAlbedo += tShaderIn.tColor;
    // }

    // if(bool(tDynamicData.tData.tFlags & PL_TERRAIN_SHADER_FLAGS_WIREFRAME))
    // {
    //     outColor = tShaderIn.tColor;
    // }

}