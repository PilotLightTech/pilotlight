#ifndef MATERIAL_INFO_GLSL
#define MATERIAL_INFO_GLSL

struct MaterialInfo
{
    float ior;
    float perceptualRoughness;
    vec3 f0_dielectric;
    float alphaRoughness;
    vec3 c_diff;
    vec3 f90;
    vec3 f90_dielectric;
    float metallic;
    vec3 baseColor;

    // sheen
    float sheenRoughnessFactor;
    vec3 sheenColorFactor;

    // KHR_materials_specular 
    float specularWeight; // product of specularFactor and specularTexture.a

    // clearcoat
    vec3 clearcoatF0;
    vec3 clearcoatF90;
    float clearcoatFactor;
    vec3 clearcoatNormal;
    float clearcoatRoughness;

    // KHR_materials_iridescence
    float iridescenceFactor;
    float iridescenceIor;
    float iridescenceThickness;

    float diffuseTransmissionFactor;
    vec3 diffuseTransmissionColorFactor;

    // KHR_materials_anisotropy
    vec3 anisotropicT;
    vec3 anisotropicB;
    float anisotropyStrength;

    float transmissionFactor;

    float thickness;
    vec3 attenuationColor;
    float attenuationDistance;

    // KHR_materials_dispersion
    float dispersion;
};

#ifdef PL_INCLUDE_MATERIAL_FUNCTIONS

vec2
pl_get_uv(int iSlot)
{
    plGpuMaterial material = tMaterialInfo.atMaterials[tObjectInfo.tData.iMaterialIndex];
    vec2 UV = (mat3(material.atTextureTransforms[iSlot]) * vec3(tShaderIn.tUV[material.aiTextureUVSet[iSlot]], 1.0)).xy;
    return UV;
}

struct NormalInfo {
    vec3 ng;   // Geometry normal
    vec3 t;    // Geometry tangent
    vec3 b;    // Geometry bitangent
    vec3 n;    // Shading normal
    vec3 ntex; // Normal from texture, scaling is accounted for.
};

NormalInfo
pl_get_normal_info()
{
    plGpuMaterial material = tMaterialInfo.atMaterials[tObjectInfo.tData.iMaterialIndex];
    vec2 UV = pl_get_uv(PL_TEXTURE_NORMAL);

    vec2 uv_dx = dFdx(UV);
    vec2 uv_dy = dFdy(UV);

    // if (length(uv_dx) <= 0.0001) {
    //   uv_dx = vec2(1.0, 0.0);
    // }

    // if (length(uv_dy) <= 0.0001) {
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
    plGpuMaterial material = tMaterialInfo.atMaterials[tObjectInfo.tData.iMaterialIndex];
    vec4 baseColor = vec4(1);
    vec2 UV = pl_get_uv(PL_TEXTURE_BASE_COLOR);

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
        baseColor *= pl_srgb_to_linear(texture(sampler2D(at2DTextures[nonuniformEXT(material.aiTextureIndices[PL_TEXTURE_BASE_COLOR])], tSamplerLinearRepeat), UV));
    }
    return baseColor * tShaderIn.tColor;
}

MaterialInfo
getMetallicRoughnessInfo(MaterialInfo info, float u_MetallicFactor, float u_RoughnessFactor)
{
    plGpuMaterial material = tMaterialInfo.atMaterials[tObjectInfo.tData.iMaterialIndex];
    vec2 UV = pl_get_uv(PL_TEXTURE_METAL_ROUGHNESS);

    info.metallic = u_MetallicFactor;
    info.perceptualRoughness = u_RoughnessFactor;

    if(bool(iTextureMappingFlags & PL_HAS_METALLIC_ROUGHNESS_MAP))
    {
        // Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
        // This layout intentionally reserves the 'r' channel for (optional) occlusion map data
        vec4 mrSample = texture(sampler2D(at2DTextures[nonuniformEXT(material.aiTextureIndices[PL_TEXTURE_METAL_ROUGHNESS])], tSamplerLinearRepeat), UV);
        info.perceptualRoughness *= mrSample.g;
        info.metallic *= mrSample.b;
    }

    // Achromatic f0 based on IOR.
    info.c_diff = mix(info.baseColor.rgb,  vec3(0), info.metallic);
    info.f0_dielectric = mix(info.f0_dielectric, info.baseColor.rgb, info.metallic);
    return info;
}

MaterialInfo
getSheenInfo(MaterialInfo info, vec3 u_SheenColorFactor, float u_SheenRoughnessFactor)
{

    plGpuMaterial material = tMaterialInfo.atMaterials[tObjectInfo.tData.iMaterialIndex];

    info.sheenColorFactor = u_SheenColorFactor;
    info.sheenRoughnessFactor = u_SheenRoughnessFactor;
    

    if(bool(iTextureMappingFlags & PL_HAS_SHEEN_COLOR_MAP))
    {
        vec4 sheenColorSample = pl_srgb_to_linear(texture(sampler2D(at2DTextures[nonuniformEXT(material.aiTextureIndices[PL_TEXTURE_SHEEN_COLOR])], tSamplerLinearRepeat), pl_get_uv(PL_TEXTURE_SHEEN_COLOR)));
        info.sheenColorFactor *= sheenColorSample.rgb;
    }

    if(bool(iTextureMappingFlags & PL_HAS_SHEEN_ROUGHNESS_MAP))
    {
        vec4 sheenRoughnessSample = texture(sampler2D(at2DTextures[nonuniformEXT(material.aiTextureIndices[PL_TEXTURE_SHEEN_ROUGHNESS])], tSamplerLinearRepeat), pl_get_uv(PL_TEXTURE_SHEEN_ROUGHNESS));
        info.sheenRoughnessFactor *= sheenRoughnessSample.a;
    }
    return info;
}

vec3
getClearcoatNormal(NormalInfo normalInfo)
{
    plGpuMaterial material = tMaterialInfo.atMaterials[tObjectInfo.tData.iMaterialIndex];
    if(bool(iTextureMappingFlags & PL_HAS_CLEARCOAT_NORMAL_MAP))
    {
        vec3 n = texture(sampler2D(at2DTextures[nonuniformEXT(material.aiTextureIndices[PL_TEXTURE_CLEARCOAT_NORMAL])], tSamplerLinearRepeat), pl_get_uv(PL_TEXTURE_CLEARCOAT_NORMAL)).rgb * 2.0 - vec3(1.0);
        // n *= vec3(u_ClearcoatNormalScale, u_ClearcoatNormalScale, 1.0);
        n = mat3(normalInfo.t, normalInfo.b, normalInfo.ng) * normalize(n);
        return n;
    }
    else
    {
        return normalInfo.ng;
    }
}

MaterialInfo
getClearCoatInfo(MaterialInfo info, NormalInfo tNormalInfo, float u_ClearcoatFactor, float u_ClearcoatRoughnessFactor)
{
    info.clearcoatFactor = u_ClearcoatFactor;
    info.clearcoatRoughness = u_ClearcoatRoughnessFactor;
    info.clearcoatF0 = vec3(pow((info.ior - 1.0) / (info.ior + 1.0), 2.0));
    info.clearcoatF90 = vec3(1.0);

    plGpuMaterial material = tMaterialInfo.atMaterials[tObjectInfo.tData.iMaterialIndex];

    if(bool(iTextureMappingFlags & PL_HAS_CLEARCOAT_MAP))
    {
        vec4 clearcoatSample = texture(sampler2D(at2DTextures[nonuniformEXT(material.aiTextureIndices[PL_TEXTURE_CLEARCOAT])], tSamplerLinearRepeat), pl_get_uv(PL_TEXTURE_CLEARCOAT));
        info.clearcoatFactor *= clearcoatSample.r;
    }

    if(bool(iTextureMappingFlags & PL_HAS_CLEARCOAT_ROUGHNESS_MAP))
    {
        vec4 clearcoatSampleRoughness = texture(sampler2D(at2DTextures[nonuniformEXT(material.aiTextureIndices[PL_TEXTURE_CLEARCOAT_ROUGHNESS])], tSamplerLinearRepeat), pl_get_uv(PL_TEXTURE_CLEARCOAT_ROUGHNESS));
        info.clearcoatRoughness *= clearcoatSampleRoughness.g;
    }

    info.clearcoatNormal = getClearcoatNormal(tNormalInfo);
    info.clearcoatRoughness = clamp(info.clearcoatRoughness, 0.0, 1.0);
    return info;
}

MaterialInfo
getIridescenceInfo(MaterialInfo info, float u_IridescenceFactor, float u_IridescenceIor, float u_IridescenceThicknessMaximum, float u_IridescenceThicknessMinimum)
{
    plGpuMaterial material = tMaterialInfo.atMaterials[tObjectInfo.tData.iMaterialIndex];
    info.iridescenceFactor = u_IridescenceFactor;
    info.iridescenceIor = u_IridescenceIor;
    info.iridescenceThickness = u_IridescenceThicknessMaximum;

    if(bool(iTextureMappingFlags & PL_HAS_CLEARCOAT_ROUGHNESS_MAP))
    {
        info.iridescenceFactor *= texture(sampler2D(at2DTextures[nonuniformEXT(material.aiTextureIndices[PL_TEXTURE_IRIDESCENCE])], tSamplerLinearRepeat), pl_get_uv(PL_TEXTURE_IRIDESCENCE)).r;
    }

    if(bool(iTextureMappingFlags & PL_HAS_CLEARCOAT_ROUGHNESS_MAP))
    {
        float thicknessSampled = texture(sampler2D(at2DTextures[nonuniformEXT(material.aiTextureIndices[PL_TEXTURE_IRIDESCENCE_THICKNESS])], tSamplerLinearRepeat), pl_get_uv(PL_TEXTURE_IRIDESCENCE_THICKNESS)).g;
        float thickness = mix(u_IridescenceThicknessMinimum, u_IridescenceThicknessMaximum, thicknessSampled);
        info.iridescenceThickness = thickness;
    }

    return info;
}

MaterialInfo
getAnisotropyInfo(MaterialInfo info, NormalInfo normalInfo, vec3 u_Anisotropy)
{
    plGpuMaterial material = tMaterialInfo.atMaterials[tObjectInfo.tData.iMaterialIndex];
    vec2 direction = vec2(1.0, 0.0);
    float strengthFactor = 1.0;
    if(bool(iTextureMappingFlags & PL_HAS_ANISOTROPY_MAP))
    {
        vec3 anisotropySample = texture(sampler2D(at2DTextures[nonuniformEXT(material.aiTextureIndices[PL_TEXTURE_ANISOTROPY])], tSamplerLinearRepeat), pl_get_uv(PL_TEXTURE_ANISOTROPY)).xyz;
        direction = anisotropySample.xy * 2.0 - vec2(1.0);
        strengthFactor = anisotropySample.z;
    }
    vec2 directionRotation = u_Anisotropy.xy; // cos(theta), sin(theta)
    mat2 rotationMatrix = mat2(directionRotation.x, directionRotation.y, -directionRotation.y, directionRotation.x);
    direction = rotationMatrix * direction.xy;

    info.anisotropicT = mat3(normalInfo.t, normalInfo.b, normalInfo.n) * normalize(vec3(direction, 0.0));
    info.anisotropicB = cross(normalInfo.ng, info.anisotropicT);
    info.anisotropyStrength = clamp(u_Anisotropy.z * strengthFactor, 0.0, 1.0);
    return info;
}

MaterialInfo
getIorInfo(MaterialInfo info)
{
    plGpuMaterial material = tMaterialInfo.atMaterials[tObjectInfo.tData.iMaterialIndex];
    info.f0_dielectric = vec3(pow(( material.fIor - 1.0) /  (material.fIor + 1.0), 2.0));
    info.ior = material.fIor;
    return info;
}

MaterialInfo
getTransmissionInfo(MaterialInfo info, float u_TransmissionFactor)
{
    plGpuMaterial material = tMaterialInfo.atMaterials[tObjectInfo.tData.iMaterialIndex];
    info.transmissionFactor = u_TransmissionFactor;

    if(bool(iTextureMappingFlags & PL_HAS_TRANSMISSION_MAP))
    {
        vec4 transmissionSample = texture(sampler2D(at2DTextures[nonuniformEXT(material.aiTextureIndices[PL_TEXTURE_TRANSMISSION])], tSamplerLinearClamp), pl_get_uv(PL_TEXTURE_TRANSMISSION));
        info.transmissionFactor *= transmissionSample.r;
    }

    info.dispersion = material.fDispersion;
    return info;
}

MaterialInfo
getVolumeInfo(MaterialInfo info)
{
    plGpuMaterial material = tMaterialInfo.atMaterials[tObjectInfo.tData.iMaterialIndex];
    if(bool(iTextureMappingFlags & PL_HAS_THICKNESS_MAP))
    {
        vec4 thicknessSample = texture(sampler2D(at2DTextures[nonuniformEXT(material.aiTextureIndices[PL_TEXTURE_THICKNESS])], tSamplerLinearClamp), pl_get_uv(PL_TEXTURE_THICKNESS));
        info.thickness *= thicknessSample.g;
    }
    return info;
}

MaterialInfo
getDiffuseTransmissionInfo(MaterialInfo info)
{
    plGpuMaterial material = tMaterialInfo.atMaterials[tObjectInfo.tData.iMaterialIndex];
    info.diffuseTransmissionFactor = material.fDiffuseTransmission;
    info.diffuseTransmissionColorFactor = material.tDiffuseTransmissionColor;

    if(bool(iTextureMappingFlags & PL_HAS_DIFFUSE_TRANSMISSION_MAP))
    {
        info.diffuseTransmissionFactor *= texture(sampler2D(at2DTextures[nonuniformEXT(material.aiTextureIndices[PL_TEXTURE_DIFFUSE_TRANSMISSION])], tSamplerLinearClamp), pl_get_uv(PL_TEXTURE_DIFFUSE_TRANSMISSION)).a;
    }

    if(bool(iTextureMappingFlags & PL_HAS_DIFFUSE_TRANSMISSION_COLOR_MAP))
    {
        info.diffuseTransmissionColorFactor *= texture(sampler2D(at2DTextures[nonuniformEXT(material.aiTextureIndices[PL_TEXTURE_DIFFUSE_TRANSMISSION_COLOR])], tSamplerLinearClamp), pl_get_uv(PL_TEXTURE_DIFFUSE_TRANSMISSION_COLOR)).rgb;
    }

    return info;
}

#endif // PL_INCLUDE_MATERIAL_FUNCTIONS

#endif // MATERIAL_INFO_GLSL