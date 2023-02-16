/*
   pl_prototype.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal functions
// [SECTION] implementations
// [SECTION] internal implementations
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_prototype.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"
#include "pl_renderer.h"
#include "pl_ui.h"
#include "pl_draw.h"

//-----------------------------------------------------------------------------
// [SECTION] internal functions
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// [SECTION] implementations
//-----------------------------------------------------------------------------

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

void
pl_update_nodes(plGraphics* ptGraphics, plNode* acNodes, uint32_t uCurrentNode, plMat4* ptMatrix)
{
    acNodes[uCurrentNode].tFinalTransform = pl_mul_mat4(ptMatrix, &acNodes[uCurrentNode].tMatrix);

    for(uint32_t i = 0; i < pl_sb_size(acNodes[uCurrentNode].sbuChildren); i++)
        pl_update_nodes(ptGraphics, acNodes, acNodes[uCurrentNode].sbuChildren[i], &acNodes[uCurrentNode].tFinalTransform);
};