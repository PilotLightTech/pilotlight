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
layout(constant_id = 1) const int iDataStride = 0;
layout(constant_id = 2) const int iTextureMappingFlags = 0;
layout(constant_id = 3) const int iMaterialFlags = 0;
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

#define PL_FRAGMENT
#include "math.glsl"

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void main() 
{
    
    plGpuMaterial material = tMaterialInfo.atMaterials[tObjectInfo.tData.iMaterialIndex];
    NormalInfo tNormalInfo = pl_get_normal_info(material.iNormalUVSet);
    vec4 tBaseColor = getBaseColor(material.tBaseColorFactor, material.iBaseColorUVSet);

    if(tBaseColor.a <  material.fAlphaCutoff)
    {
        discard;
    }
    
    MaterialInfo materialInfo;
    materialInfo.f0_dielectric = vec3(0.04);
    materialInfo.baseColor = tBaseColor.rgb;
    
    if(bool(iMaterialFlags & PL_INFO_MATERIAL_METALLICROUGHNESS))
    {
        materialInfo = getMetallicRoughnessInfo(materialInfo, material.fMetallicFactor, material.fRoughnessFactor, material.iMetallicRoughnessUVSet);
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
        ao = texture(sampler2D(at2DTextures[nonuniformEXT(material.iOcclusionTexIdx)], tSamplerLinearRepeat), tShaderIn.tUV[material.iOcclusionUVSet]).r;
    }

    // fill g-buffer
    outAlbedo = tBaseColor;
    outNormal = Encode(tNormalInfo.n);
    outAOMetalnessRoughness = vec4(ao, materialInfo.metallic, materialInfo.perceptualRoughness, 1.0);
}
