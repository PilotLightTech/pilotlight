#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "common.glsl"

layout(location = 0) out vec4 outColor;

// output
layout(location = 0) in struct plShaderIn {
    vec4 tPosition;
    vec2 tUV;
    vec4 tColor;
    vec3 tNormal;
    mat3 tTBN;
} tShaderIn;

struct NormalInfo {
    vec3 ng;   // Geometry normal
    vec3 t;    // Geometry tangent
    vec3 b;    // Geometry bitangent
    vec3 n;    // Shading normal
    vec3 ntex; // Normal from texture, scaling is accounted for.
};

NormalInfo pl_get_normal_info(vec3 v)
{
    vec2 UV = tShaderIn.tUV;
    vec2 uv_dx = dFdx(UV);
    vec2 uv_dy = dFdy(UV);

    if (length(uv_dx) <= 1e-2) {
      uv_dx = vec2(1.0, 0.0);
    }

    if (length(uv_dy) <= 1e-2) {
      uv_dy = vec2(0.0, 1.0);
    }

    vec3 t_ = (uv_dy.t * dFdx(tShaderIn.tPosition.xyz) - uv_dx.t * dFdy(tShaderIn.tPosition.xyz)) /
        (uv_dx.s * uv_dy.t - uv_dy.s * uv_dx.t);

    vec3 n, t, b, ng;

    // Compute geometrical TBN:
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
            // Normals are either present as vertex attributes or approximated.
            ng = normalize(tShaderIn.tNormal);
            t = normalize(t_ - ng * dot(ng, t_));
            b = cross(ng, t);
        }
    }
    else
    {
        ng = normalize(cross(dFdx(tShaderIn.tPosition.xyz), dFdy(tShaderIn.tPosition.xyz)));
        t = normalize(t_ - ng * dot(ng, t_));
        b = cross(ng, t);
    }


    // For a back-facing surface, the tangential basis vectors are negated.
    if (gl_FrontFacing == false)
    {
        t *= -1.0;
        b *= -1.0;
        ng *= -1.0;
    }

    // Compute normals:
    NormalInfo info;
    info.ng = ng;
    if(bool(ShaderTextureFlags & PL_TEXTURE_HAS_NORMAL)) 
    {
        info.ntex = texture(tSampler1, UV).rgb * 2.0 - vec3(1.0);
        info.ntex *= vec3(1.0, -1.0, 1.0);
        // info.ntex *= vec3(u_NormalScale, u_NormalScale, 1.0);
        info.ntex = normalize(info.ntex);
        info.n = normalize(mat3(t, b, ng) * info.ntex);
    }
    else
    {
        info.n = ng;
    }
    info.t = t;
    info.b = b;
    return info;
}

void main() 
{
    vec4 tBaseColor = tShaderIn.tColor;
    vec3 v = normalize(tGlobalInfo.tCameraPos.xyz - tShaderIn.tPosition.xyz);
    NormalInfo tNormalInfo = pl_get_normal_info(v);

    if(bool(ShaderTextureFlags & PL_TEXTURE_HAS_BASE_COLOR)) 
    {
        tBaseColor = texture(tSampler0, tShaderIn.tUV);
    }

    outColor = tBaseColor;
    // outColor = vec4(tNormalInfo.n, 1.0);
    // outColor = vec4((tShaderIn.tNormal + 1.0) / 2.0, 1.0);
}