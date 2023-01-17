#include "pl_renderer.h"
#include "pl_graphics_vulkan.h"
#include "pl_ds.h"
#include "pl_math.h"

void
pl_setup_asset_registry(plGraphics* ptGraphics, plAssetRegistry* ptRegistryOut)
{
    ptRegistryOut->ptGraphics = ptGraphics;

    // create dummy texture

    const plTextureDesc tTextureDesc = {
        .tDimensions = {.x = (float)1.0f, .y = (float)1.0f, .z = 1.0f},
        .tFormat     = VK_FORMAT_R8G8B8A8_UNORM,
        .tUsage      = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .uLayers     = 1,
        .uMips       = 1,
        .tType       = VK_IMAGE_TYPE_2D,
        .tViewType   = VK_IMAGE_VIEW_TYPE_2D
    };
    static const float afSinglePixel[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    ptRegistryOut->uDummyTexture = pl_create_texture(&ptRegistryOut->ptGraphics->tResourceManager, tTextureDesc, sizeof(unsigned char) * 4, afSinglePixel);
}

void
pl_cleanup_asset_registry(plAssetRegistry* ptRegistry)
{
    for(uint32_t i = 0; i < pl_sb_size(ptRegistry->sbtMaterials); i++)
    {
        plMaterial* ptMaterial = &ptRegistry->sbtMaterials[i];
        pl_submit_buffer_for_deletion(&ptRegistry->ptGraphics->tResourceManager, ptMaterial->uMaterialConstantBuffer);
    }
    pl_sb_free(ptRegistry->sbtMaterials);
}

void
pl_initialize_material(plMaterial* ptMaterial, const char* pcName)
{
    ptMaterial->uShader = UINT32_MAX;
    ptMaterial->tAlbedo = (plVec4){ 0.45f, 0.45f, 0.85f, 1.0f };
    ptMaterial->fAlphaCutoff = 0.1f;
    ptMaterial->bDoubleSided = false;
    strncpy(ptMaterial->acName, pcName, PL_MATERIAL_MAX_NAME_LENGTH);
}

uint32_t
pl_create_material(plAssetRegistry* ptRegistry, plMaterial* ptMaterial)
{
    const uint32_t uIndex = pl_sb_size(ptRegistry->sbtMaterials);
    
    pl_create_bind_group(ptRegistry->ptGraphics, pl_get_bind_group_layout(&ptRegistry->ptGraphics->tResourceManager, ptMaterial->uShader, 1), &ptMaterial->tMaterialBindGroup, ptMaterial->acName);

    // create constant buffers
    ptMaterial->uMaterialConstantBuffer = pl_create_constant_buffer(&ptRegistry->ptGraphics->tResourceManager, sizeof(plMaterialInfo), ptRegistry->ptGraphics->uFramesInFlight);

    // update bind group
    uint32_t* sbuTextures = NULL;
    pl_sb_push(sbuTextures, ptMaterial->uAlbedoMap);
    pl_sb_push(sbuTextures, ptMaterial->uNormalMap);
    pl_sb_push(sbuTextures, ptMaterial->uEmissiveMap);
    pl_update_bind_group(ptRegistry->ptGraphics, &ptMaterial->tMaterialBindGroup, 1, &ptMaterial->uMaterialConstantBuffer, pl_sb_size(sbuTextures), sbuTextures);
    pl_sb_free(sbuTextures);
    for(uint32_t i = 0; i < ptRegistry->ptGraphics->uFramesInFlight; i++)
    {
        plMaterialInfo* ptMaterialInfo = pl_get_constant_buffer_data_ex(&ptRegistry->ptGraphics->tResourceManager, ptMaterial->uMaterialConstantBuffer, i, 0);
        ptMaterialInfo->tAlbedo = ptMaterial->tAlbedo;
    }

    pl_sb_push(ptRegistry->sbtMaterials, *ptMaterial);
    return uIndex;
}

void
pl_setup_renderer(plGraphics* ptGraphics, plAssetRegistry* ptRegistry, plRenderer* ptRendererOut)
{
    ptRendererOut->ptGraphics = ptGraphics;
    ptRendererOut->ptAssetRegistry = ptRegistry;
    ptRendererOut->uGlobalStorageBuffer = UINT32_MAX;

    plBindGroupLayout tGlobalGroupLayout =
    {
        .uBufferCount = 2,
        .uTextureCount = 0,
        .aBuffers = {
            {
                .tType       = PL_BUFFER_BINDING_TYPE_UNIFORM,
                .uSlot       = 0,
                .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
            },
            {
                .tType       = PL_BUFFER_BINDING_TYPE_STORAGE,
                .uSlot       = 1,
                .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT
            }
        }
    };

    // create constant buffer
    ptRendererOut->uGlobalConstantBuffer = pl_create_constant_buffer(&ptRegistry->ptGraphics->tResourceManager, sizeof(plGlobalInfo), ptRegistry->ptGraphics->uFramesInFlight);

    // create & update global bind group
    pl_create_bind_group(ptRendererOut->ptGraphics, &tGlobalGroupLayout, &ptRendererOut->tGlobalBindGroup, "global bind group");
}

void
pl_cleanup_renderer(plRenderer* ptRenderer)
{
    pl_sb_free(ptRenderer->sbfStorageBuffer);
    pl_sb_free(ptRenderer->sbtDraws);
    pl_sb_free(ptRenderer->sbtDrawAreas);
    pl_submit_buffer_for_deletion(&ptRenderer->ptGraphics->tResourceManager, ptRenderer->uGlobalStorageBuffer);
    pl_submit_buffer_for_deletion(&ptRenderer->ptGraphics->tResourceManager, ptRenderer->uGlobalConstantBuffer);
}

void
pl_renderer_begin_frame(plRenderer* ptRenderer)
{

    pl_sb_reset(ptRenderer->sbtDraws);
    pl_sb_reset(ptRenderer->sbtDrawAreas);

    if(pl_sb_size(ptRenderer->sbfStorageBuffer) > 0)
    {
        if(ptRenderer->uGlobalStorageBuffer != UINT32_MAX)
        {
            pl_submit_buffer_for_deletion(&ptRenderer->ptGraphics->tResourceManager, ptRenderer->uGlobalStorageBuffer);
        }
        ptRenderer->uGlobalStorageBuffer = pl_create_storage_buffer(&ptRenderer->ptGraphics->tResourceManager, pl_sb_size(ptRenderer->sbfStorageBuffer) * sizeof(float), ptRenderer->sbfStorageBuffer);
        pl_sb_reset(ptRenderer->sbfStorageBuffer);

        uint32_t atBuffers0[] = {ptRenderer->uGlobalConstantBuffer, ptRenderer->uGlobalStorageBuffer};
        pl_update_bind_group(ptRenderer->ptGraphics, &ptRenderer->tGlobalBindGroup, 2, atBuffers0, 0, NULL);
    }
}

void
pl_update_nodes(plGraphics* ptGraphics, uint32_t* auVertexOffsets, plNode* acNodes, uint32_t uCurrentNode, uint32_t uConstantBuffer, plMat4* ptMatrix)
{
    plMat4 tTransform = pl_mul_mat4(ptMatrix, &acNodes[uCurrentNode].tMatrix);
    for(uint32_t i = 0; i < pl_sb_size(acNodes[uCurrentNode].sbuMeshes); i++)
    {
        uint32_t uMesh = acNodes[uCurrentNode].sbuMeshes[i];
        plObjectInfo* ptObjectInfo = pl_get_constant_buffer_data(&ptGraphics->tResourceManager, uConstantBuffer, uMesh);
        ptObjectInfo->tModel = tTransform;
        ptObjectInfo->uVertexOffset = auVertexOffsets[uMesh];
    }

    for(uint32_t i = 0; i < pl_sb_size(acNodes[uCurrentNode].sbuChildren); i++)
        pl_update_nodes(ptGraphics, auVertexOffsets, acNodes, acNodes[uCurrentNode].sbuChildren[i], uConstantBuffer, &tTransform);
};

void
pl_renderer_submit_meshes(plRenderer* ptRenderer, plMesh* ptMeshes, uint32_t* puMaterials, plBindGroup* ptBindGroup, uint32_t uConstantBuffer, uint32_t uMeshCount)
{
    
    pl_sb_reserve(ptRenderer->sbtDraws, pl_sb_size(ptRenderer->sbtDraws) + uMeshCount);
    for(uint32_t i = 0; i < uMeshCount; i++)
    {
        plMaterial* ptMaterial = &ptRenderer->ptAssetRegistry->sbtMaterials[puMaterials[i]];
        uint32_t uShaderVariant = UINT32_MAX;
        const plShader* ptShader = &ptRenderer->ptGraphics->tResourceManager.sbtShaders[ptMaterial->uShader];
        const plMesh* ptMesh = &ptMeshes[i];
        const uint32_t uVariantCount = pl_sb_size(ptShader->tDesc.sbtVariants);

        for(uint32_t j = 0; j < uVariantCount; j++)
        {
            plGraphicsState ptVariant = ptShader->tDesc.sbtVariants[j];
            if(ptMaterial->ulShaderTextureFlags == ptVariant.ulShaderTextureFlags && ptVariant.ulVertexStreamMask0 == ptMesh->ulVertexStreamMask0 && ptVariant.ulVertexStreamMask1 == ptMesh->ulVertexStreamMask1)
            {
                uShaderVariant = j;
                break;
            }
        }
        // PL_ASSERT(uShaderVariant != UINT32_MAX && "shader variant not found");
        if(uShaderVariant == UINT32_MAX)
        {
            plGraphicsState tShaderVariant = {
                .ulVertexStreamMask0  = ptMesh->ulVertexStreamMask0,
                .ulVertexStreamMask1  = ptMesh->ulVertexStreamMask1,
                .ulDepthMode          = PL_DEPTH_MODE_LESS,
                .ulBlendMode          = PL_BLEND_MODE_ALPHA,
                .ulCullMode           = ptMaterial->bDoubleSided ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT,
                .ulDepthWriteEnabled  = VK_TRUE
            };

            if(ptMaterial->uAlbedoMap > 0)   tShaderVariant.ulShaderTextureFlags |= PL_SHADER_TEXTURE_FLAG_BINDING_0;
            if(ptMaterial->uNormalMap > 0)   tShaderVariant.ulShaderTextureFlags |= PL_SHADER_TEXTURE_FLAG_BINDING_1;
            if(ptMaterial->uEmissiveMap > 0) tShaderVariant.ulShaderTextureFlags |= PL_SHADER_TEXTURE_FLAG_BINDING_2;

            pl_add_shader_variant(&ptRenderer->ptGraphics->tResourceManager, ptMaterial->uShader, tShaderVariant);
            uShaderVariant = pl_sb_size(ptShader->tDesc.sbtVariants) - 1;
        }

        if(uShaderVariant != UINT32_MAX)
        {
            pl_sb_push(ptRenderer->sbtDraws, ((plDraw){
                .uShader               = ptMaterial->uShader,
                .uShaderVariant        = uShaderVariant,
                .ptMesh                = &ptMeshes[i],
                .ptBindGroup1          = &ptMaterial->tMaterialBindGroup,
                .ptBindGroup2          = &ptBindGroup[i],
                .uDynamicBufferOffset1 = pl_get_constant_buffer_offset(&ptRenderer->ptGraphics->tResourceManager, ptMaterial->uMaterialConstantBuffer, 0),
                .uDynamicBufferOffset2 = pl_get_constant_buffer_offset(&ptRenderer->ptGraphics->tResourceManager, uConstantBuffer, i)
                }));
        }
    }
}