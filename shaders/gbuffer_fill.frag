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

#include "defines.glsl"
#include "material.glsl"
#include "math.glsl"

//-----------------------------------------------------------------------------
// [SECTION] specialication constants
//-----------------------------------------------------------------------------

layout(constant_id = 0) const int iMeshVariantFlags = 0;
layout(constant_id = 1) const int iDataStride = 0;
layout(constant_id = 2) const int iTextureMappingFlags = 0;
layout(constant_id = 3) const int iMaterialFlags = 0;
layout(constant_id = 4) const int iRenderingFlags = 0;

//-----------------------------------------------------------------------------
// [SECTION] bind group 0
//-----------------------------------------------------------------------------

layout(std140, set = 0, binding = 0) readonly buffer _tVertexBuffer
{
	vec4 atVertexData[];
} tVertexBuffer;

layout(std140, set = 0, binding = 1) readonly buffer _tTransformBuffer
{
	mat4 atTransform[];
} tTransformBuffer;

layout(set = 0, binding = 2) readonly buffer plMaterialInfo
{
    tMaterial atMaterials[];
} tMaterialInfo;

layout(set = 0, binding = 3)  uniform sampler tDefaultSampler;
layout(set = 0, binding = 4)  uniform sampler tEnvSampler;
layout(set = 0, binding = 5)  uniform texture2D at2DTextures[PL_MAX_BINDLESS_TEXTURES];
layout(set = 0, binding = PL_MAX_BINDLESS_CUBE_TEXTURE_SLOT)  uniform textureCube atCubeTextures[PL_MAX_BINDLESS_TEXTURES];

//-----------------------------------------------------------------------------
// [SECTION] bind group 1
//-----------------------------------------------------------------------------

struct tGlobalData
{
    vec4 tViewportSize;
    vec4 tViewportInfo;
    vec4 tCameraPos;
    mat4 tCameraView;
    mat4 tCameraProjection;
    mat4 tCameraViewProjection;
};

layout(set = 1, binding = 0) readonly buffer _plGlobalInfo
{
    tGlobalData data[];
} tGlobalInfo;

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    int  iDataOffset;
    int  iVertexOffset;
    int  iMaterialIndex;
    uint uGlobalIndex;
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

//-----------------------------------------------------------------------------
// [SECTION] helpers
//-----------------------------------------------------------------------------

struct NormalInfo {
    vec3 ng;   // Geometry normal
    vec3 t;    // Geometry tangent
    vec3 b;    // Geometry bitangent
    vec3 n;    // Shading normal
    vec3 ntex; // Normal from texture, scaling is accounted for.
};

NormalInfo pl_get_normal_info(int iUVSet)
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
    if(bool(iTextureMappingFlags & PL_HAS_NORMAL_MAP)) 
    {
        tMaterial material = tMaterialInfo.atMaterials[tObjectInfo.iMaterialIndex];
        info.ntex = texture(sampler2D(at2DTextures[nonuniformEXT(material.iNormalTexIdx)], tDefaultSampler), UV).rgb * 2.0 - vec3(1.0);
        // info.ntex *= vec3(0.2, 0.2, 1.0);
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

vec4 getBaseColor(vec4 u_ColorFactor, int iUVSet)
{
    vec4 baseColor = vec4(1);

    // if(bool(MATERIAL_SPECULARGLOSSINESS))
    // {
    //     baseColor = u_DiffuseFactor;
    // }
    // else if(bool(MATERIAL_METALLICROUGHNESS))
    if(bool(iMaterialFlags & PL_MATERIAL_METALLICROUGHNESS))
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
    if(bool(iMaterialFlags & PL_MATERIAL_METALLICROUGHNESS) && bool(iTextureMappingFlags & PL_HAS_BASE_COLOR_MAP))
    {
        tMaterial material = tMaterialInfo.atMaterials[tObjectInfo.iMaterialIndex];
        baseColor *= pl_srgb_to_linear(texture(sampler2D(at2DTextures[nonuniformEXT(material.iBaseColorTexIdx)], tDefaultSampler), tShaderIn.tUV[iUVSet]));
    }
    return baseColor * tShaderIn.tColor;
}

MaterialInfo getMetallicRoughnessInfo(MaterialInfo info, float u_MetallicFactor, float u_RoughnessFactor, int UVSet)
{
    info.metallic = u_MetallicFactor;
    info.perceptualRoughness = u_RoughnessFactor;

    if(bool(iTextureMappingFlags & PL_HAS_METALLIC_ROUGHNESS_MAP))
    {
        // Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
        // This layout intentionally reserves the 'r' channel for (optional) occlusion map data
        tMaterial material = tMaterialInfo.atMaterials[tObjectInfo.iMaterialIndex];
        vec4 mrSample = texture(sampler2D(at2DTextures[nonuniformEXT(material.iMetallicRoughnessTexIdx)], tDefaultSampler), tShaderIn.tUV[UVSet]);
        info.perceptualRoughness *= mrSample.g;
        info.metallic *= mrSample.b;
    }

    // Achromatic f0 based on IOR.
    info.c_diff = mix(info.baseColor.rgb,  vec3(0), info.metallic);
    info.f0 = mix(info.f0, info.baseColor.rgb, info.metallic);
    return info;
}

vec2 OctWrap( vec2 v ) {
    vec2 w = 1.0 - abs( v.yx );
    if (v.x < 0.0) w.x = -w.x;
    if (v.y < 0.0) w.y = -w.y;
    return w;
}
 
vec2 Encode( vec3 n ) {
    n /= ( abs( n.x ) + abs( n.y ) + abs( n.z ) );
    n.xy = n.z > 0.0 ? n.xy : OctWrap( n.xy );
    n.xy = n.xy * 0.5 + 0.5;
    return n.xy;
}

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void main() 
{
    
    tMaterial material = tMaterialInfo.atMaterials[tObjectInfo.iMaterialIndex];
    NormalInfo tNormalInfo = pl_get_normal_info(material.NormalUVSet);
    vec4 tBaseColor = getBaseColor(material.u_BaseColorFactor, material.BaseColorUVSet);

    if(tBaseColor.a <  material.u_AlphaCutoff)
    {
        discard;
    }
    
    MaterialInfo materialInfo;
    materialInfo.f0 = vec3(0.04);
    materialInfo.baseColor = tBaseColor.rgb;
    
    if(bool(iMaterialFlags & PL_MATERIAL_METALLICROUGHNESS))
    {
        materialInfo = getMetallicRoughnessInfo(materialInfo, material.u_MetallicFactor, material.u_RoughnessFactor, material.MetallicRoughnessUVSet);
    }

    materialInfo.perceptualRoughness = clamp(materialInfo.perceptualRoughness, 0.0, 1.0);
    materialInfo.metallic = clamp(materialInfo.metallic, 0.0, 1.0);

    // Roughness is authored as perceptual roughness; as is convention,
    // convert to material roughness by squaring the perceptual roughness.
    materialInfo.alphaRoughness = materialInfo.perceptualRoughness * materialInfo.perceptualRoughness;
    
    // Anything less than 2% is physically impossible and is instead considered to be shadowing. Compare to "Real-Time-Rendering" 4th editon on page 325.
    materialInfo.f90 = vec3(1.0);

    // ambient occlusion
    float ao = 1.0;
    if(bool(iTextureMappingFlags & PL_HAS_OCCLUSION_MAP))
    {
        ao = texture(sampler2D(at2DTextures[nonuniformEXT(material.iOcclusionTexIdx)], tDefaultSampler), tShaderIn.tUV[material.OcclusionUVSet]).r;
    }

    // fill g-buffer
    outAlbedo = tBaseColor;
    outNormal = Encode(tNormalInfo.n);
    outAOMetalnessRoughness = vec4(ao, materialInfo.metallic, materialInfo.perceptualRoughness, 1.0);
}

