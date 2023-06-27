#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "common.glsl"

// input
layout(location = 0) in struct plShaderOut {
    vec3 tPosition;
    vec3 tWorldPosition;
    vec3 tNormal;
    vec3 tWorldNormal;
    vec2 tUV;
    vec4 tColor;
    mat3 tTBN;
} tShaderIn;

// output
layout(location = 0) out vec4 outColor;

struct NormalInfo
{
    vec3 ng; // Geometry normal
    vec3 t; // Geometry tangent
    vec3 b; // Geometry bitangent
    vec3 n; // Shading normal
    vec3 ntex; // Normal from texture, scaling is accounted for.
};

NormalInfo getNormalInfo()
{
    vec2 UV = tShaderIn.tUV;
    vec3 uv_dx = dFdx(vec3(UV, 0.0));
    vec3 uv_dy = dFdy(vec3(UV, 0.0));

    if (length(uv_dx) + length(uv_dy) <= 1e-6) {
        uv_dx = vec3(1.0, 0.0, 0.0);
        uv_dy = vec3(0.0, 1.0, 0.0);
    }

    vec3 t_ = (uv_dy.t * dFdx(tShaderIn.tPosition) - uv_dx.t * dFdy(tShaderIn.tPosition)) /
        (uv_dx.s * uv_dy.t - uv_dy.s * uv_dx.t);

    vec3 n;
    vec3 t;
    vec3 b;
    vec3 ng;

    // Compute geometrical TBN
    if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_NORMAL))
    {
        if(bool(MeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TANGENT))
        {
            // Trivial TBN computation, present as vertex attribute.
            // Normalize eigenvectors as matrix is linearly interpolated.
            t = normalize(tShaderIn.tTBN[0]);
            b = normalize(tShaderIn.tTBN[1]);
            ng = normalize(tShaderIn.tTBN[2]);
        }
        else
        {
            ng = normalize(tShaderIn.tWorldNormal);
            t = normalize(t_ - ng * dot(ng, t_));
            b = cross(ng, t);
        }
    }
    else
    {
        ng = normalize(cross(dFdx(tShaderIn.tPosition), dFdy(tShaderIn.tPosition)));
        t = normalize(t_ - ng * dot(ng, t_));
        b = cross(ng, t);
    }
    
    // For a back-facing surface, the tangential basis vectors are negated.
    if(!gl_FrontFacing)
    {
        t  *= -1.0;
        b  *= -1.0;
        ng *= -1.0;
    }

    // Compute normals:
    NormalInfo normalInfo;
    normalInfo.ng = ng;
    if(bool(ShaderTextureFlags & PL_TEXTURE_HAS_NORMAL)) 
    {
        normalInfo.ntex = texture(tNormalSampler, tShaderIn.tUV).xyz * 2.0 - vec3(1.0);
        normalInfo.ntex = normalize(normalInfo.ntex);
        normalInfo.n = normalize(mat3(t, b, ng) * normalInfo.ntex);
    }
    else
    {
        normalInfo.n = ng;
    }

    normalInfo.t = t;
    normalInfo.b = b;
    return normalInfo; 
}

float clampedDot(vec3 x, vec3 y)
{
    return clamp(dot(x, y), 0.0, 1.0);
}

void main() 
{
    NormalInfo normalInfo = getNormalInfo();
    vec3 n = normalize(normalInfo.n);
    const vec3 tLightDir0 = normalize(tGlobalInfo.tLightPos.xyz - tShaderIn.tWorldPosition);
    const vec4 tLightColor = vec4(tGlobalInfo.tLightColor.rgb, 1.0);
    const vec4 tDiffuseColor = tLightColor * clampedDot(n, tLightDir0);
    vec4 tMaterialColor = tMaterialBuffer.atMaterialData[tObjectInfo.uMaterialIndex].tAlbedo * tShaderIn.tColor;
    vec4 tEmissiveColor = vec4(0.0);

    if(bool(ShaderTextureFlags & PL_TEXTURE_HAS_BASE_COLOR)) 
    {
        tMaterialColor = texture(tColorSampler, tShaderIn.tUV) * tShaderIn.tColor;
    }

    if(bool(ShaderTextureFlags & PL_TEXTURE_HAS_EMISSIVE)) 
    {
        tEmissiveColor = texture(tEmissiveSampler, tShaderIn.tUV);
    }

    outColor = (tGlobalInfo.tAmbientColor + tDiffuseColor) * tMaterialColor + tEmissiveColor;

    outColor.a = tMaterialColor.a;
    if(outColor.a < 0.01)
    {
        discard;
    }

}