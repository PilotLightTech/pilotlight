#include "pl_renderer.h"
#include "pl_graphics_vulkan.h"
#include "pl_ds.h"

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
}