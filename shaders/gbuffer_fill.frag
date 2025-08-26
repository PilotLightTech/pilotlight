#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

/*
   gbuffer_fill.frag
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] specialization constants
// [SECTION] bind group 0
// [SECTION] bind group 1
// [SECTION] dynamic bind group
// [SECTION] input & output
// [SECTION] helpers
// [SECTION] entry
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "bg_scene.inc"
#include "bg_view.inc"

//-----------------------------------------------------------------------------
// [SECTION] specialication constants
//-----------------------------------------------------------------------------

layout(constant_id = 0) const int iMeshVariantFlags = 0;
layout(constant_id = 1) const int iTextureMappingFlags = 0;
layout(constant_id = 2) const int iMaterialFlags = 0;
layout(constant_id = 3) const int tShaderDebugMode = 0;
layout(constant_id = 4) const int iRenderingFlags = 0;

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    plGpuDynData tData;
} tObjectInfo;

//-----------------------------------------------------------------------------
// [SECTION] input & output
//-----------------------------------------------------------------------------

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec2 outNormal;
layout(location = 2) out vec4 outAOMetalnessRoughness;

// output
layout(location = 0) in struct plShaderIn {
    vec3 tWorldPosition;
    vec4 tViewPosition;
    vec2 tUV[8];
    vec4 tColor;
    vec3 tWorldNormal;
    mat3 tTBN;
} tShaderIn;

#include "math.glsl"
#include "material_info.glsl"

struct NormalInfo {
    vec3 ng;   // Geometry normal
    vec3 t;    // Geometry tangent
    vec3 b;    // Geometry bitangent
    vec3 n;    // Shading normal
    vec3 ntex; // Normal from texture, scaling is accounted for.
};

NormalInfo
pl_get_normal_info(int iUVSet)
{
    vec2 UV = tShaderIn.tUV[iUVSet];
    vec2 uv_dx = dFdx(UV);
    vec2 uv_dy = dFdy(UV);

    // if (length(uv_dx) <= 1e-2) {
    //   uv_dx = vec2(1.0, 0.0);
    // }

    // if (length(uv_dy) <= 1e-2) {
    //   uv_dy = vec2(0.0, 1.0);
    // }

    vec3 t_ = (uv_dy.t * dFdx(tShaderIn.tWorldPosition) - uv_dx.t * dFdy(tShaderIn.tWorldPosition)) /
        (uv_dx.s * uv_dy.t - uv_dy.s * uv_dx.t);

    vec3 n, t, b, ng;

    // Compute geometrical TBN:
    if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_NORMAL))
    {

        if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TANGENT))
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
            ng = normalize(tShaderIn.tWorldNormal);
            t = normalize(t_ - ng * dot(ng, t_));
            b = cross(ng, t);
        }
    }
    else
    {
        ng = normalize(cross(dFdx(tShaderIn.tWorldPosition), dFdy(tShaderIn.tWorldPosition)));
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
    if(bool(iTextureMappingFlags & PL_HAS_NORMAL_MAP) && bool(iRenderingFlags & PL_RENDERING_FLAG_USE_NORMAL_MAPS)) 
    {
        plGpuMaterial material = tMaterialInfo.atMaterials[tObjectInfo.tData.iMaterialIndex];
        info.ntex = texture(sampler2D(at2DTextures[nonuniformEXT(material.aiTextureIndices[PL_TEXTURE_NORMAL])], tSamplerLinearRepeat), UV).rgb * 2.0 - vec3(1.0);
        info.ntex *= vec3(material.fNormalMapStrength, material.fNormalMapStrength, 1.0);
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

vec4
getBaseColor(vec4 u_ColorFactor, int iUVSet)
{
    vec4 baseColor = vec4(1);

    // if(bool(MATERIAL_SPECULARGLOSSINESS))
    // {
    //     baseColor = u_DiffuseFactor;
    // }
    // else if(bool(MATERIAL_METALLICROUGHNESS))
    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_METALLIC_ROUGHNESS))
    {
        // baseColor = u_BaseColorFactor;
        baseColor = u_ColorFactor;
    }

    // if(bool(MATERIAL_SPECULARGLOSSINESS) && bool(HAS_DIFFUSE_MAP))
    // {
    //     // baseColor *= texture(u_DiffuseSampler, getDiffuseUV());
    //     baseColor *= texture(u_DiffuseSampler, tShaderIn.tUV);
    // }
    // else if(bool(MATERIAL_METALLICROUGHNESS) && bool(HAS_BASE_COLOR_MAP))
    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_METALLIC_ROUGHNESS) && bool(iTextureMappingFlags & PL_HAS_BASE_COLOR_MAP))
    {
        plGpuMaterial material = tMaterialInfo.atMaterials[tObjectInfo.tData.iMaterialIndex];
        baseColor *= pl_srgb_to_linear(texture(sampler2D(at2DTextures[nonuniformEXT(material.aiTextureIndices[PL_TEXTURE_BASE_COLOR])], tSamplerLinearRepeat), tShaderIn.tUV[iUVSet]));
    }
    return baseColor * tShaderIn.tColor;
}

MaterialInfo
getMetallicRoughnessInfo(MaterialInfo info, float u_MetallicFactor, float u_RoughnessFactor, int UVSet)
{
    info.metallic = u_MetallicFactor;
    info.perceptualRoughness = u_RoughnessFactor;

    if(bool(iTextureMappingFlags & PL_HAS_METALLIC_ROUGHNESS_MAP))
    {
        // Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
        // This layout intentionally reserves the 'r' channel for (optional) occlusion map data
        plGpuMaterial material = tMaterialInfo.atMaterials[tObjectInfo.tData.iMaterialIndex];
        vec4 mrSample = texture(sampler2D(at2DTextures[nonuniformEXT(material.aiTextureIndices[PL_TEXTURE_METAL_ROUGHNESS])], tSamplerLinearRepeat), tShaderIn.tUV[UVSet]);
        info.perceptualRoughness *= mrSample.g;
        info.metallic *= mrSample.b;
    }

    // Achromatic f0 based on IOR.
    info.c_diff = mix(info.baseColor.rgb,  vec3(0), info.metallic);
    info.f0_dielectric = mix(info.f0_dielectric, info.baseColor.rgb, info.metallic);
    return info;
}

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void main() 
{
    
    plGpuMaterial material = tMaterialInfo.atMaterials[tObjectInfo.tData.iMaterialIndex];
    NormalInfo tNormalInfo = pl_get_normal_info(material.aiTextureUVSet[PL_TEXTURE_NORMAL]);
    vec4 tBaseColor = getBaseColor(material.tBaseColorFactor, material.aiTextureUVSet[PL_TEXTURE_BASE_COLOR]);

    if(tShaderDebugMode == PL_SHADER_DEBUG_MODE_NONE)
    {
        if(tBaseColor.a <  material.fAlphaCutoff)
        {
            discard;
        }
    }
    
    MaterialInfo materialInfo;
    materialInfo.f0_dielectric = vec3(0.04);
    materialInfo.baseColor = tBaseColor.rgb;
    
    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_METALLIC_ROUGHNESS))
    {
        materialInfo = getMetallicRoughnessInfo(materialInfo, material.fMetallicFactor, material.fRoughnessFactor, material.aiTextureUVSet[PL_TEXTURE_METAL_ROUGHNESS]);
    }

    materialInfo.perceptualRoughness = clamp(materialInfo.perceptualRoughness, 0.0, 1.0);
    materialInfo.metallic = clamp(materialInfo.metallic, 0.0, 1.0);

    // Roughness is authored as perceptual roughness; as is convention,
    // convert to material roughness by squaring the perceptual roughness.
    materialInfo.alphaRoughness = materialInfo.perceptualRoughness * materialInfo.perceptualRoughness;
    
    // Anything less than 2% is physically impossible and is instead considered to be shadowing. Compare to "Real-Time-Rendering" 4th editon on page 325.
    materialInfo.f90 = vec3(1.0);
    materialInfo.f90_dielectric = materialInfo.f90;

    // ambient occlusion
    float ao = 1.0;
    if(bool(iTextureMappingFlags & PL_HAS_OCCLUSION_MAP))
    {
        ao = texture(sampler2D(at2DTextures[nonuniformEXT(material.aiTextureIndices[PL_TEXTURE_OCCLUSION])], tSamplerLinearRepeat), tShaderIn.tUV[material.aiTextureUVSet[PL_TEXTURE_OCCLUSION]]).r;
    }

    // fill g-buffer
    outAlbedo = tBaseColor;
    outNormal = Encode(tNormalInfo.n);
    outAOMetalnessRoughness = vec4(ao, materialInfo.metallic, materialInfo.perceptualRoughness, 1.0);

    if(tShaderDebugMode != PL_SHADER_DEBUG_MODE_NONE)
    {

        // In case of missing data for a debug view, render a checkerboard.
        if(tShaderDebugMode != PL_SHADER_DEBUG_BASE_COLOR)
        {
            outAlbedo = vec4(1.0);
            {
                float frequency = 0.02;
                float gray = 0.9;

                vec2 v1 = step(0.5, fract(frequency * gl_FragCoord.xy));
                vec2 v2 = step(0.5, vec2(1.0) - fract(frequency * gl_FragCoord.xy));
                outAlbedo.rgb *= gray + v1.x * v1.y + v2.x * v2.y;
            }
        }

        if(tShaderDebugMode == PL_SHADER_DEBUG_UV0)
        {
            if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0))
                outAlbedo.rgb = vec3(tShaderIn.tUV[0], 0.0);
        }

        if(tShaderDebugMode == PL_SHADER_DEBUG_GEOMETRY_NORMAL)
        {
            outAlbedo.rgb = vec3((1.0 + tNormalInfo.ng) / 2.0);
        }

        if(tShaderDebugMode == PL_SHADER_DEBUG_GEOMETRY_TANGENT)
        {
            outAlbedo.rgb = vec3((1.0 + tNormalInfo.t) / 2.0);
        }

        if(tShaderDebugMode == PL_SHADER_DEBUG_GEOMETRY_BITANGENT)
        {
            outAlbedo.rgb = vec3((1.0 + tNormalInfo.b) / 2.0);
        }

        if(tShaderDebugMode == PL_SHADER_DEBUG_TEXTURE_NORMAL)
        {
            if(bool(iTextureMappingFlags & PL_HAS_NORMAL_MAP))
            {
                outAlbedo.rgb = tNormalInfo.ntex;
            }
        }
    }
}
