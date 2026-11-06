#ifndef PL_DECALS_GLSL
#define PL_DECALS_GLSL

#include "pl_bg_scene.inc"
#include "pl_math.glsl"

// Run in surface shaders before lighting. Receiver coverage, normals and PBR
// parameters are preserved. Derivatives are taken outside divergent box tests.
vec3
pl_apply_decals(vec3 baseColor, vec3 worldPosition, vec3 worldNormal)
{
    vec3 dx = dFdx(worldPosition);
    vec3 dy = dFdy(worldPosition);
    for(int i = 0; i < tGpuScene.tData.iDecalCount; i++)
    {
        plGpuDecal decal = tGpuScene.tData.atDecals[i];
        vec3 local = (decal.tWorldToDecal * vec4(worldPosition, 1.0)).xyz;
        if(any(greaterThan(abs(local), vec3(0.5))))
            continue;
        if(dot(normalize(worldNormal), decal.tNormalOpacity.xyz) <= decal.fNormalCutoff)
            continue;

        plGpuMaterial material = tMaterialInfo.atMaterials[decal.iMaterialIndex];
        vec4 color = material.tBaseColorFactor;
        if(bool(material.iTextureMappingFlags & PL_HAS_BASE_COLOR_MAP))
        {
            mat3 uvTransform = mat3(material.atTextureTransforms[PL_TEXTURE_BASE_COLOR]);
            vec2 uv = (uvTransform * vec3(local.xy + 0.5, 1.0)).xy;
            vec2 uvDx = (uvTransform * vec3((decal.tWorldToDecal * vec4(dx, 0.0)).xy, 0.0)).xy;
            vec2 uvDy = (uvTransform * vec3((decal.tWorldToDecal * vec4(dy, 0.0)).xy, 0.0)).xy;
            color *= pl_srgb_to_linear(textureGrad(sampler2D(at2DTextures[nonuniformEXT(material.aiTextureIndices[PL_TEXTURE_BASE_COLOR])], tSamplerLinearClamp), uv, uvDx, uvDy));
        }
        float alpha = clamp(color.a * decal.tNormalOpacity.w, 0.0, 1.0);
        if(decal.fEdgeFade > 0.0)
        {
            vec3 edge = (vec3(0.5) - abs(local)) * 2.0;
            alpha *= smoothstep(0.0, decal.fEdgeFade, min(edge.x, min(edge.y, edge.z)));
        }
        baseColor = mix(baseColor, color.rgb, alpha);
    }
    return baseColor;
}

#endif
