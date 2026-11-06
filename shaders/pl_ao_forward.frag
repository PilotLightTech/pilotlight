#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#include "pl_bg_scene.inc"
#include "pl_bg_view.inc"
#include "pl_math.glsl"
layout(constant_id = 0) const int iFoliage = 0;
layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    plGpuDynForwardData tData;
} tObjectInfo;

layout(location = 0) in plShaderOut {
    vec3 tWorldPosition;
    vec3 tViewPosition;
    vec2 tUV[2];
    vec4 tColor;
    vec3 tWorldNormal;
    mat3 tTBN;
    mat4 tModel;
    flat uvec4 tSelectedProbes;
    flat uint uProbeSelection;
} tShaderIn;

layout(location = 1) out vec4 outAlbedo;
layout(location = 2) out vec2 outNormal;
layout(location = 3) out vec4 outMaterial;

#include "pl_decals.glsl"
#define PL_INCLUDE_MATERIAL_FUNCTIONS
#include "pl_material_info.glsl"
void main()
{
    plGpuMaterial material = tMaterialInfo.atMaterials[tObjectInfo.tData.iMaterialIndex];
    if(material.eAlphaMode == PL_SHADER_ALPHA_MODE_BLEND ||
       bool(material.iMaterialFlags & (PL_MATERIAL_SHADER_FLAG_TRANSMISSION | PL_MATERIAL_SHADER_FLAG_VOLUME | PL_MATERIAL_SHADER_FLAG_DIFFUSE_TRANSMISSION)))
        discard;
    vec4 base = getBaseColor(material.tBaseColorFactor, material.aiTextureUVSet[PL_TEXTURE_BASE_COLOR], tObjectInfo.tData.iMaterialIndex);
    base.rgb = pl_apply_decals(base.rgb, tShaderIn.tWorldPosition, tShaderIn.tWorldNormal);
    if((iFoliage != 0 || material.eAlphaMode == PL_SHADER_ALPHA_MODE_MASK) && base.a < material.fAlphaCutoff)
        discard;
    NormalInfo normal = pl_get_normal_info(tObjectInfo.tData.iMaterialIndex);
    outNormal = Encode(normal.n);
    MaterialInfo info;
    info.baseColor = base.rgb;
    info.f0_dielectric = vec3(0.04);
    info = getMetallicRoughnessInfo(info, material.fMetallicFactor, material.fRoughnessFactor, tObjectInfo.tData.iMaterialIndex);
    float ao = 1.0;
    if(bool(material.iTextureMappingFlags & PL_HAS_OCCLUSION_MAP))
    {
        ao = texture(sampler2D(at2DTextures[nonuniformEXT(material.aiTextureIndices[PL_TEXTURE_OCCLUSION])], tSamplerLinearRepeat), pl_get_uv(PL_TEXTURE_OCCLUSION, tObjectInfo.tData.iMaterialIndex)).r;
        ao = 1.0 + material.fOcclusionStrength * (ao - 1.0);
    }
    // Albedo alpha marks valid opaque SSGI data. ORM alpha stays zero so
    // deferred lighting skips these pixels; forward shading still owns them.
    outAlbedo = vec4(base.rgb, 1.0);
    outMaterial = vec4(ao, clamp(info.metallic, 0.0, 1.0), clamp(info.perceptualRoughness, 0.0, 1.0), 0.0);
}
