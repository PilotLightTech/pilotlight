#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "pl_bg_scene.inc"
#include "pl_bg_view.inc"
#include "pl_brdf.glsl"
#include "pl_math.glsl"


//-----------------------------------------------------------------------------
// [SECTION] specialication constants
//-----------------------------------------------------------------------------

layout(constant_id = 0) const int iMeshVariantFlags = 0;
layout(constant_id = 1) const int iTextureMappingFlags = 0;
layout(constant_id = 2) const int iMaterialFlags = 0;
layout(constant_id = 3) const int iRenderingFlags = 0;
layout(constant_id = 4) const int tShaderDebugMode = 0;

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

layout(location = 0) out vec4 outColor;

// output
layout(location = 0) in struct plShaderIn {
    vec3 tWorldPosition;
    vec3 tViewPosition;
    vec2 tUV[2];
    vec4 tColor;
    vec3 tWorldNormal;
    mat3 tTBN;
    mat4 tModel; 
} tShaderIn;

#include "pl_math.glsl"
#include "pl_lighting.glsl"
#include "pl_fog.glsl"

#define PL_INCLUDE_MATERIAL_FUNCTIONS
#include "pl_material_info.glsl"

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void main() 
{

    plGpuMaterial material = tMaterialInfo.atMaterials[tObjectInfo.tData.iMaterialIndex];
    vec4 tBaseColor = getBaseColor(material.tBaseColorFactor, material.aiTextureUVSet[PL_TEXTURE_BASE_COLOR]);

    if(material.tAlphaMode == PL_SHADER_ALPHA_MODE_OPAQUE)
    {
        tBaseColor.a = 1.0;
    }
    vec3 color = vec3(0);

    NormalInfo tNormalInfo = pl_get_normal_info();

    vec3 n = tNormalInfo.n;
    vec3 t = tNormalInfo.t;

    vec3 vraw = tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tCameraPos.xyz - tShaderIn.tWorldPosition.xyz;
    vec3 v = normalize(vraw);

    MaterialInfo materialInfo;
    materialInfo.baseColor = tBaseColor.rgb;
    materialInfo.thickness = material.fThickness;
    materialInfo.attenuationDistance = material.fAttenuationDistance;
    materialInfo.dispersion = 0.0;
    materialInfo.attenuationColor = material.tAttenuationColor;

    // The default index of refraction of 1.5 yields a dielectric normal incidence reflectance of 0.04.
    materialInfo.ior = 1.5;
    materialInfo.f0_dielectric = vec3(0.04);
    materialInfo.specularWeight = 1.0;

    materialInfo = getIorInfo(materialInfo);

    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_METALLIC_ROUGHNESS))
    {
        materialInfo = getMetallicRoughnessInfo(materialInfo, material.fMetallicFactor, material.fRoughnessFactor);
    }

    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_SHEEN))
    {
        materialInfo = getSheenInfo(materialInfo, material.tSheenColorFactor, material.fSheenRoughnessFactor);
    }

    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_CLEARCOAT))
    {
        materialInfo = getClearCoatInfo(materialInfo, tNormalInfo, material.fClearcoatFactor, material.fClearcoatRoughnessFactor);
    }

    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_TRANSMISSION))
    {
        materialInfo = getTransmissionInfo(materialInfo, material.fTransmissionFactor);
    }

    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_VOLUME))
    {
        materialInfo = getVolumeInfo(materialInfo);
    }

    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_IRIDESCENCE))
    {
        materialInfo = getIridescenceInfo(materialInfo, material.fIridescenceFactor, material.fIridescenceIor, material.fIridescenceThicknessMax, material.fIridescenceThicknessMin);
    }

    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_DIFFUSE_TRANSMISSION))
    {
        materialInfo = getDiffuseTransmissionInfo(materialInfo);
    }

    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_ANISOTROPY))
    {
        materialInfo = getAnisotropyInfo(materialInfo, tNormalInfo, material.tAnisotropy);
    }

    materialInfo.perceptualRoughness = clamp(materialInfo.perceptualRoughness, 0.0, 1.0);
    materialInfo.metallic = clamp(materialInfo.metallic, 0.0, 1.0);

    // Roughness is authored as perceptual roughness; as is convention,
    // convert to material roughness by squaring the perceptual roughness.
    materialInfo.alphaRoughness = materialInfo.perceptualRoughness * materialInfo.perceptualRoughness;

    // Anything less than 2% is physically impossible and is instead considered to be shadowing. Compare to "Real-Time-Rendering" 4th editon on page 325.
    materialInfo.f90 = vec3(1.0);
    materialInfo.f90_dielectric = materialInfo.f90;

    // LIGHTING
    vec3 f_emissive = vec3(0.0);

    float clearcoatFactor = 0.0;



    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_IRIDESCENCE))
    {
        if (materialInfo.iridescenceThickness == 0.0)
        {
            materialInfo.iridescenceFactor = 0.0;
        }
    }

    mat4 u_ViewMatrix = tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tCameraView;
    mat4 u_ProjectionMatrix = tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tCameraProjection;
    mat4 u_ModelMatrix = tShaderIn.tModel;

    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_CLEARCOAT))
    {
        clearcoatFactor = materialInfo.clearcoatFactor;
    }

    // ambient occlusion
    
    float ao = 1.0;
    if(bool(iTextureMappingFlags & PL_HAS_OCCLUSION_MAP))
    {
        ao = texture(sampler2D(at2DTextures[nonuniformEXT(material.aiTextureIndices[PL_TEXTURE_OCCLUSION])], tSamplerLinearRepeat), pl_get_uv(PL_TEXTURE_OCCLUSION)).r;
        color = color * (1.0 + material.fOcclusionStrength * (ao - 1.0)); 
    }


    // emissive
    f_emissive = material.tEmissiveFactor * material.fEmissiveStrength;
    if(bool(iTextureMappingFlags & PL_HAS_EMISSIVE_MAP))
    {
        f_emissive *= pl_srgb_to_linear(texture(sampler2D(at2DTextures[nonuniformEXT(material.aiTextureIndices[PL_TEXTURE_EMISSIVE])], tSamplerLinearRepeat), pl_get_uv(PL_TEXTURE_EMISSIVE)).rgb);
    }


    // In case of missing data for a debug view, render a checkerboard.
    outColor = vec4(1.0);
    {
        float frequency = 0.02;
        float gray = 0.9;

        vec2 v1 = step(0.5, fract(frequency * gl_FragCoord.xy));
        vec2 v2 = step(0.5, vec2(1.0) - fract(frequency * gl_FragCoord.xy));
        outColor.rgb *= gray + v1.x * v1.y + v2.x * v2.y;
    }

    if(tShaderDebugMode == PL_SHADER_DEBUG_BASE_COLOR)
    {
        outColor = tBaseColor;
    }

    if(tShaderDebugMode == PL_SHADER_DEBUG_SHADING_NORMAL)
    {
        outColor = vec4((n + 1.0) / 2.0, tBaseColor.a);
        // outColor = vec4(vec3(albedoSheenScaling), tBaseColor.a);
    }

    if(tShaderDebugMode == PL_SHADER_DEBUG_GEOMETRY_NORMAL)
    {
        outColor = vec4((tNormalInfo.ng + 1.0) / 2.0, tBaseColor.a);
    }

    if(tShaderDebugMode == PL_SHADER_DEBUG_GEOMETRY_TANGENT)
    {
        outColor = vec4((tNormalInfo.t + 1.0) / 2.0, tBaseColor.a);
    }

    if(tShaderDebugMode == PL_SHADER_DEBUG_GEOMETRY_BITANGENT)
    {
        outColor.rgb = (tNormalInfo.b + 1.0) / 2.0;
    }

    if(tShaderDebugMode == PL_SHADER_DEBUG_TEXTURE_NORMAL)
    {
        if(bool(iTextureMappingFlags & PL_HAS_NORMAL_MAP))
        {
            outColor = vec4((tNormalInfo.ntex + 1.0) / 2.0, tBaseColor.a);
        }
    }

    if(tShaderDebugMode == PL_SHADER_DEBUG_METALLIC)
    {
        outColor.rgb = vec3(materialInfo.metallic);
    }

    if(tShaderDebugMode == PL_SHADER_DEBUG_ROUGHNESS)
    {
        outColor.rgb = vec3(materialInfo.perceptualRoughness);
    }

    if(tShaderDebugMode == PL_SHADER_DEBUG_UV0)
    {
        if(bool(iMeshVariantFlags & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0))
        {
            outColor.rgb = vec3(tShaderIn.tUV[0], 0.0);
        }
    }

    if(tShaderDebugMode == PL_SHADER_DEBUG_ALPHA)
    {
        outColor.rgb = vec3(tBaseColor.a);
    }

    if(tShaderDebugMode == PL_SHADER_DEBUG_EMMISSIVE)
    {
        outColor.rgb = pl_linear_to_srgb(f_emissive);
    }

    if(tShaderDebugMode == PL_SHADER_DEBUG_OCCLUSION)
    {
        outColor.rgb = vec3(ao);
    }

    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_CLEARCOAT))
    {

        if(tShaderDebugMode == PL_SHADER_DEBUG_CLEARCOAT)
        {
            outColor.rgb = vec3(materialInfo.clearcoatFactor);
        }

        if(tShaderDebugMode == PL_SHADER_DEBUG_CLEARCOAT_ROUGHNESS)
        {
            outColor.rgb = vec3(materialInfo.clearcoatRoughness);
        }

        if(tShaderDebugMode == PL_SHADER_DEBUG_CLEARCOAT_NORMAL)
        {
            outColor.rgb = (materialInfo.clearcoatNormal + vec3(1)) / 2.0;
        }
    }

    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_SHEEN))
    {
        if(tShaderDebugMode == PL_SHADER_DEBUG_SHEEN_COLOR)
        {
            outColor.rgb = materialInfo.sheenColorFactor;
        }
        if(tShaderDebugMode == PL_SHADER_DEBUG_SHEEN_ROUGHNESS)
        {
            outColor.rgb = vec3(materialInfo.sheenRoughnessFactor);
        }
    }

    if(tShaderDebugMode == PL_SHADER_DEBUG_IRIDESCENCE_FACTOR)
    {
        outColor.rgb = vec3(materialInfo.iridescenceFactor);
    }

    if(tShaderDebugMode == PL_SHADER_DEBUG_IRIDESCENCE_THICKNESS)
    {
        outColor.rgb = vec3(materialInfo.iridescenceThickness / 1200.0);
    }

    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_ANISOTROPY))
    {
        if(tShaderDebugMode == PL_SHADER_DEBUG_ANISOTROPY_STRENGTH)
        {
            outColor.rgb = vec3(materialInfo.anisotropyStrength);
        }
        if(tShaderDebugMode == PL_SHADER_DEBUG_ANISOTROPY_DIRECTION)
        {
            vec2 direction = vec2(1.0, 0.0);
            if(bool(iTextureMappingFlags & PL_HAS_ANISOTROPY_MAP))
            {
                direction = texture(sampler2D(at2DTextures[nonuniformEXT(material.aiTextureIndices[PL_TEXTURE_ANISOTROPY])], tSamplerLinearRepeat), pl_get_uv(PL_TEXTURE_ANISOTROPY)).xy;
                direction = direction * 2.0 - vec2(1.0); // [0, 1] -> [-1, 1]
            }
            vec2 directionRotation = material.tAnisotropy.xy; // cos(theta), sin(theta)
            mat2 rotationMatrix = mat2(directionRotation.x, directionRotation.y, -directionRotation.y, directionRotation.x);
            direction = (direction + vec2(1.0)) * 0.5; // [-1, 1] -> [0, 1]
            outColor.rgb = vec3(direction, 0.0);
        }
    }

    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_TRANSMISSION))
    {
        if(tShaderDebugMode == PL_SHADER_DEBUG_TRANSMISSION_STRENGTH)
        {
            outColor.rgb = vec3(materialInfo.transmissionFactor);
        }
    }

    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_VOLUME))
    {
        if(tShaderDebugMode == PL_SHADER_DEBUG_VOLUME_THICKNESS)
        {
            outColor.rgb = vec3(materialInfo.thickness / material.fThickness);
        }
    }

    if(bool(iMaterialFlags & PL_MATERIAL_SHADER_FLAG_DIFFUSE_TRANSMISSION))
    {
        if(tShaderDebugMode == PL_SHADER_DEBUG_DIFFUSE_TRANSMISSION_STRENGTH)
        {
            outColor.rgb = pl_linear_to_srgb(vec3(materialInfo.diffuseTransmissionFactor));
        }
        if(tShaderDebugMode == PL_SHADER_DEBUG_DIFFUSE_TRANSMISSION_COLOR)
        {
            outColor.rgb = pl_linear_to_srgb(vec3(materialInfo.diffuseTransmissionColorFactor));
        }
    }
}