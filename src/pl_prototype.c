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

void
pl_ui_demo(bool* pbOpen)
{
    if(pl_ui_begin_window("UI Demo", pbOpen, false))
    {
        pl_ui_progress_bar(0.75f, (plVec2){-1.0f, 0.0f}, NULL);
        pl_ui_button("Hover me!");

        if(pl_ui_was_last_item_hovered())
        {
            pl_ui_begin_tooltip();
            pl_ui_text("I'm a tooltip!");
            pl_ui_end_tooltip();
        }
        static int iValue = 0;
        pl_ui_text("Radio Buttons");
        pl_ui_radio_button("Option 1", &iValue, 0);
        pl_ui_same_line(0.0f, -1.0f);
        pl_ui_radio_button("Option 2", &iValue, 1);
        pl_ui_same_line(0.0f, -1.0f);
        pl_ui_radio_button("Option 3", &iValue, 2);
        pl_ui_text("Selectables");
        static bool bSelectable0 = false;
        static bool bSelectable1 = false;
        static bool bSelectable2 = false;
        pl_ui_selectable("Selectable 1", &bSelectable0);
        pl_ui_selectable("Selectable 2", &bSelectable1);
        pl_ui_selectable("Selectable 3", &bSelectable2);
        if(pl_ui_tree_node("Root Node"))
        {
            if(pl_ui_tree_node("Child 1"))
            {
                pl_ui_button("Press me");
                pl_ui_tree_pop();
            }
            if(pl_ui_tree_node("Child 2"))
            {
                pl_ui_button("Press me");
                pl_ui_tree_pop();
            }
            pl_ui_tree_pop();
        }
        if(pl_ui_collapsing_header("Collapsing Header"))
        {
            pl_ui_checkbox("Camera window2", NULL);
        }

        if(pl_ui_begin_tab_bar("Tabs1"))
        {
            if(pl_ui_begin_tab("Tab 0"))
            {
                pl_ui_selectable("Selectable 1", &bSelectable0);
                pl_ui_selectable("Selectable 2", &bSelectable1);
                pl_ui_selectable("Selectable 3", &bSelectable2);   
            }
            pl_ui_end_tab();

            if(pl_ui_begin_tab("Tab 1"))
            {
                pl_ui_radio_button("Option 1", &iValue, 0);
                pl_ui_radio_button("Option 2", &iValue, 1);
                pl_ui_radio_button("Option 3", &iValue, 2);
            }
            pl_ui_end_tab();

            if(pl_ui_begin_tab("Tab 2"))
            {
                pl_ui_radio_button("Option 1", &iValue, 0);
                pl_ui_selectable("Selectable 2", &bSelectable1);
            }
            pl_ui_end_tab();
        }
        pl_ui_end_tab_bar();
    }
    pl_ui_end_window();
}

//-----------------------------------------------------------------------------
// [SECTION] internal implementations
//-----------------------------------------------------------------------------