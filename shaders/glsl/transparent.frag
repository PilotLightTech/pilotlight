#version 450
#extension GL_ARB_separate_shader_objects : enable



const float GAMMA = 2.2;
const float INV_GAMMA = 1.0 / GAMMA;

// linear to sRGB approximation
// see http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
vec3 linearTosRGB(vec3 color)
{
    return pow(color, vec3(INV_GAMMA));
}

layout(location = 0) out vec4 outColor;

// output
layout(location = 0) in struct plShaderIn {
    vec3 tPosition;
    vec4 tWorldPosition;
    vec2 tUV[2];
    vec4 tColor;
    vec3 tWorldNormal;
    mat3 tTBN;
} tShaderIn;

#define PL_FRAGMENT_SHADER 
#include "gbuffer_common.glsl"

void
main() 
{
    vec4 tBaseColor = getBaseColor(tMaterialInfo.atMaterials[tObjectInfo.iMaterialIndex].u_BaseColorFactor);
    if(tBaseColor.a < 0.1)
    {
        discard;
    }
    vec3 tSunlightColor = vec3(1.0, 1.0, 1.0);
    NormalInfo tNormalInfo = pl_get_normal_info();
    vec3 tSunLightDirection = vec3(-1.0, -1.0, -1.0);
    float fDiffuseIntensity = max(0.0, dot(normalize(tNormalInfo.n), -normalize(tSunLightDirection)));
    outColor = tBaseColor * vec4(tSunlightColor * (0.05 + fDiffuseIntensity), 1.0);

    outColor = vec4(linearTosRGB(outColor.rgb), tBaseColor.a);
}