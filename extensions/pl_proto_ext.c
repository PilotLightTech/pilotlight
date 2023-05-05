/*
   pl_proto_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal api
// [SECTION] public api implementations
// [SECTION] internal api implementations
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pilotlight.h"
#include "pl_proto_ext.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"
#include "pl_ds.h"
#include "pl_io.h"
#include "pl_memory.h"
#include "pl_log.h"

// required extensions
#include "pl_ui_ext.h"
#include "pl_draw_ext.h"
#include "pl_image_ext.h"
#include "pl_vulkan_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

// graphics
static void pl_create_main_render_target(plGraphics* ptGraphics, plRenderTarget* ptTargetOut);
static void pl_create_render_pass  (plGraphics* ptGraphics, const plRenderPassDesc* ptDesc, plRenderPass* ptPassOut);
static void pl_create_render_target(plGraphics* ptGraphics, const plRenderTargetDesc* ptDesc, plRenderTarget* ptTargetOut);
static void pl_begin_render_target (plGraphicsApiI* ptGfx, plGraphics* ptGraphics, plRenderTarget* ptTarget);
static void pl_end_render_target   (plGraphicsApiI* ptGfx, plGraphics* ptGraphics);
static void pl_cleanup_render_target(plGraphics* ptGraphics, plRenderTarget* ptTarget);
static void pl_cleanup_render_pass(plGraphics* ptGraphics, plRenderPass* ptPass);
static void pl_setup_renderer  (plApiRegistryApiI* ptApiRegistry, plGraphics* ptGraphics, plRenderer* ptRenderer);
static void pl_cleanup_renderer(plRenderer* ptRenderer);
static void pl_draw_sky        (plScene* ptScene);

// scene
static void pl_create_scene      (plRenderer* ptRenderer, plScene* ptSceneOut);
static void pl_reset_scene       (plScene* ptScene);
static void pl_draw_scene        (plScene* ptScene);
static void pl_scene_bind_camera (plScene* ptScene, const plCameraComponent* ptCamera);
static void pl_scene_update_ecs  (plScene* ptScene);
static void pl_scene_bind_target (plScene* ptScene, plRenderTarget* ptTarget);
static void pl_scene_prepare     (plScene* ptScene);

// entity component system
static void pl_ecs_update(plScene* ptScene, plComponentManager* ptManager);

//-----------------------------------------------------------------------------
// [SECTION] public api implementations
//-----------------------------------------------------------------------------

plProtoApiI*
pl_load_proto_api(void)
{
    static plProtoApiI tApi0 = {
        .create_main_render_target = pl_create_main_render_target,
        .create_render_pass        = pl_create_render_pass,
        .create_render_target      = pl_create_render_target,
        .begin_render_target       = pl_begin_render_target,
        .end_render_target         = pl_end_render_target,
        .cleanup_render_target     = pl_cleanup_render_target,
        .cleanup_render_pass       = pl_cleanup_render_pass,
        .setup_renderer            = pl_setup_renderer,
        .cleanup_renderer          = pl_cleanup_renderer,
        .draw_sky                  = pl_draw_sky,
        .create_scene              = pl_create_scene,
        .reset_scene               = pl_reset_scene,
        .draw_scene                = pl_draw_scene,
        .scene_bind_camera         = pl_scene_bind_camera,
        .scene_update_ecs          = pl_scene_update_ecs,
        .scene_bind_target         = pl_scene_bind_target,
        .scene_prepare             = pl_scene_prepare,
        .ecs_update                = pl_ecs_update
    };
    return &tApi0;
}

//-----------------------------------------------------------------------------
// [SECTION] internal implementations
//-----------------------------------------------------------------------------

static void
pl_create_render_target(plGraphics* ptGraphics, const plRenderTargetDesc* ptDesc, plRenderTarget* ptTargetOut)
{
    ptTargetOut->tDesc = *ptDesc;
    plDeviceApiI* ptDeviceApi = ptGraphics->ptDeviceApi;
    plDevice* ptDevice = &ptGraphics->tDevice;

    const plTextureDesc tColorTextureDesc = {
        .tDimensions = {.x = ptDesc->tSize.x, .y = ptDesc->tSize.y, .z = 1.0f},
        .tFormat     = ptDesc->tRenderPass.tDesc.tColorFormat,
        .tUsage      = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .uLayers     = 1,
        .uMips       = 1,
        .tType       = VK_IMAGE_TYPE_2D
    };

    const plTextureDesc tDepthTextureDesc = {
        .tDimensions = {.x = ptDesc->tSize.x, .y = ptDesc->tSize.y, .z = 1.0f},
        .tFormat     = ptDesc->tRenderPass.tDesc.tDepthFormat,
        .tUsage      = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .uLayers     = 1,
        .uMips       = 1,
        .tType       = VK_IMAGE_TYPE_2D
    };

    const plSampler tColorSampler = 
    {
        .fMinMip = 0.0f,
        .fMaxMip = 64.0f,
        .tFilter = PL_FILTER_LINEAR
    };

    const plTextureViewDesc tColorView = {
        .tFormat     = tColorTextureDesc.tFormat,
        .uLayerCount = tColorTextureDesc.uLayers,
        .uMips       = tColorTextureDesc.uMips
    };

    for(uint32_t i = 0; i < ptGraphics->tSwapchain.uImageCount; i++)
    {
        uint32_t uColorTexture = ptDeviceApi->create_texture(ptDevice, tColorTextureDesc, 0, NULL, "offscreen color texture");
        pl_sb_push(ptTargetOut->sbuColorTextureViews, ptDeviceApi->create_texture_view(ptDevice, &tColorView, &tColorSampler, uColorTexture, "offscreen color view"));
    }

    uint32_t uDepthTexture = ptDeviceApi->create_texture(ptDevice, tDepthTextureDesc, 0, NULL, "offscreen depth texture");

    const plTextureViewDesc tDepthView = {
        .tFormat     = tDepthTextureDesc.tFormat,
        .uLayerCount = tDepthTextureDesc.uLayers,
        .uMips       = tDepthTextureDesc.uMips
    };

    ptTargetOut->uDepthTextureView = ptDeviceApi->create_texture_view(ptDevice, &tDepthView, &tColorSampler, uDepthTexture, "offscreen depth view");

    plTextureView* ptDepthTextureView = &ptDevice->sbtTextureViews[ptTargetOut->uDepthTextureView];

    for(uint32_t i = 0; i < ptGraphics->tSwapchain.uImageCount; i++)
    {
        plTextureView* ptColorTextureView = &ptDevice->sbtTextureViews[ptTargetOut->sbuColorTextureViews[i]];
        VkImageView atAttachments[] = {
            ptColorTextureView->_tImageView,
            ptDepthTextureView->_tImageView
        };
        VkFramebufferCreateInfo tFrameBufferInfo = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = ptDesc->tRenderPass._tRenderPass,
            .attachmentCount = 2u,
            .pAttachments    = atAttachments,
            .width           = (uint32_t)ptDesc->tSize.x,
            .height          = (uint32_t)ptDesc->tSize.y,
            .layers          = 1u,
        };
        VkFramebuffer tFrameBuffer = {0};
        PL_VULKAN(vkCreateFramebuffer(ptGraphics->tDevice.tLogicalDevice, &tFrameBufferInfo, NULL, &tFrameBuffer));
        pl_sb_push(ptTargetOut->sbtFrameBuffers, tFrameBuffer);
    }
}

static void
pl_create_main_render_target(plGraphics* ptGraphics, plRenderTarget* ptTargetOut)
{
    plIOContext* ptIOCtx = ptGraphics->ptIoInterface->get_context();
    ptTargetOut->bMSAA = true;
    ptTargetOut->sbtFrameBuffers = ptGraphics->tSwapchain.ptFrameBuffers;
    ptTargetOut->tDesc.tRenderPass._tRenderPass = ptGraphics->tRenderPass;
    ptTargetOut->tDesc.tRenderPass.tDesc.tColorFormat = ptGraphics->tSwapchain.tFormat;
    ptTargetOut->tDesc.tRenderPass.tDesc.tDepthFormat = ptGraphics->tSwapchain.tDepthFormat;
    ptTargetOut->tDesc.tSize.x = (float)ptGraphics->tSwapchain.tExtent.width;
    ptTargetOut->tDesc.tSize.y = (float)ptGraphics->tSwapchain.tExtent.height;
}

static void
pl_create_render_pass(plGraphics* ptGraphics, const plRenderPassDesc* ptDesc, plRenderPass* ptPassOut)
{
    ptPassOut->tDesc = *ptDesc;
    plDeviceApiI* ptDeviceApi = ptGraphics->ptDeviceApi;

    // create render pass
    VkAttachmentDescription atAttachments[] = {

        // color attachment
        {
            .flags          = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT,
            .format         = ptDeviceApi->vulkan_format(ptDesc->tColorFormat),
            .samples        = VK_SAMPLE_COUNT_1_BIT,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        },

        // depth attachment
        {
            .format         = ptDeviceApi->vulkan_format(ptDesc->tDepthFormat),
            .samples        = VK_SAMPLE_COUNT_1_BIT,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        }
    };

    VkSubpassDependency tSubpassDependencies[] = {

        // color attachment
        {
            .srcSubpass      = VK_SUBPASS_EXTERNAL,
            .dstSubpass      = 0,
            .srcStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask   = VK_ACCESS_SHADER_READ_BIT,
            .dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = 0
        },

        // color attachment out
        {
            .srcSubpass      = 0,
            .dstSubpass      = VK_SUBPASS_EXTERNAL,
            .srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask   = VK_ACCESS_SHADER_READ_BIT,
            .dependencyFlags = 0
        },
    };

    VkAttachmentReference atAttachmentReferences[] = {
        {
            .attachment = 0,
            .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        },
        {
            .attachment = 1,
            .layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL     
        }
    };

    VkSubpassDescription tSubpass = {
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount    = 1,
        .pColorAttachments       = &atAttachmentReferences[0],
        .pDepthStencilAttachment = &atAttachmentReferences[1]
    };

    VkRenderPassCreateInfo tRenderPassInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 2,
        .pAttachments    = atAttachments,
        .subpassCount    = 1,
        .pSubpasses      = &tSubpass,
        .dependencyCount = 2,
        .pDependencies   = tSubpassDependencies
    };
    PL_VULKAN(vkCreateRenderPass(ptGraphics->tDevice.tLogicalDevice, &tRenderPassInfo, NULL, &ptPassOut->_tRenderPass));
}

static void
pl_begin_render_target(plGraphicsApiI* ptGfx, plGraphics* ptGraphics, plRenderTarget* ptTarget)
{
    const plFrameContext* ptCurrentFrame = ptGfx->get_frame_resources(ptGraphics);

    static const VkClearValue atClearValues[2] = 
    {
        {
            .color.float32[0] = 0.0f,
            .color.float32[1] = 0.0f,
            .color.float32[2] = 0.0f,
            .color.float32[3] = 1.0f
        },
        {
            .depthStencil.depth = 1.0f,
            .depthStencil.stencil = 0
        }    
    };

    // set viewport
    const VkViewport tViewport = {
        .x        = 0.0f,
        .y        = 0.0f,
        .width    = ptTarget->tDesc.tSize.x,
        .height   = ptTarget->tDesc.tSize.y,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    vkCmdSetViewport(ptCurrentFrame->tCmdBuf, 0, 1, &tViewport);

    // set scissor
    const VkRect2D tDynamicScissor = {
        .extent = {
            .width    = (uint32_t)ptTarget->tDesc.tSize.x,
            .height   = (uint32_t)ptTarget->tDesc.tSize.y,
        }
    };
    vkCmdSetScissor(ptCurrentFrame->tCmdBuf, 0, 1, &tDynamicScissor);

    const VkRenderPassBeginInfo tRenderPassBeginInfo = {
        .sType               = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass          = ptTarget->tDesc.tRenderPass._tRenderPass,
        .framebuffer         = ptTarget->sbtFrameBuffers[ptGraphics->tSwapchain.uCurrentImageIndex],
        .renderArea          = {
                                    .extent = {
                                        .width  = (uint32_t)ptTarget->tDesc.tSize.x,
                                        .height = (uint32_t)ptTarget->tDesc.tSize.y,
                                    }
                                },
        .clearValueCount     = 2,
        .pClearValues        = atClearValues
    };
    vkCmdBeginRenderPass(ptCurrentFrame->tCmdBuf, &tRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

static void
pl_end_render_target(plGraphicsApiI* ptGfx, plGraphics* ptGraphics)
{
    const plFrameContext* ptCurrentFrame = ptGfx->get_frame_resources(ptGraphics);

    vkCmdEndRenderPass(ptCurrentFrame->tCmdBuf);
}

static void
pl_cleanup_render_target(plGraphics* ptGraphics, plRenderTarget* ptTarget)
{
    for (uint32_t i = 0u; i < pl_sb_size(ptTarget->sbtFrameBuffers); i++)
    {
        vkDestroyFramebuffer(ptGraphics->tDevice.tLogicalDevice, ptTarget->sbtFrameBuffers[i], NULL);
    }
}

static void
pl_cleanup_render_pass(plGraphics* ptGraphics, plRenderPass* ptPass)
{
    vkDestroyRenderPass(ptGraphics->tDevice.tLogicalDevice, ptPass->_tRenderPass, NULL);
}

static void
pl_setup_renderer(plApiRegistryApiI* ptApiRegistry, plGraphics* ptGraphics, plRenderer* ptRenderer)
{
    memset(ptRenderer, 0, sizeof(plRenderer));

    ptRenderer->ptGfx = ptApiRegistry->first(PL_API_GRAPHICS);
    ptRenderer->ptMemoryApi = ptApiRegistry->first(PL_API_MEMORY);
    ptRenderer->ptDeviceApi = ptApiRegistry->first(PL_API_DEVICE);
    ptRenderer->ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    ptRenderer->ptProtoApi = ptApiRegistry->first(PL_API_PROTO);
    ptRenderer->ptImageApi = ptApiRegistry->first(PL_API_IMAGE);
    ptRenderer->ptEcs = ptApiRegistry->first(PL_API_ECS);

	pl_set_log_context(ptRenderer->ptDataRegistry->get_data("log"));
    ptRenderer->uLogChannel = pl_add_log_channel("renderer", PL_CHANNEL_TYPE_CONSOLE | PL_CHANNEL_TYPE_BUFFER);

    plGraphicsApiI* ptGfx = ptRenderer->ptGfx;
    plDeviceApiI* ptDeviceApi = ptGraphics->ptDeviceApi;
    plDevice* ptDevice = &ptGraphics->tDevice;

    // create dummy texture (texture slot 0 when not used)
    const plTextureDesc tTextureDesc2 = {
        .tDimensions = {.x = (float)1.0f, .y = (float)1.0f, .z = 1.0f},
        .tFormat     = PL_FORMAT_R8G8B8A8_UNORM,
        .tUsage      = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .uLayers     = 1,
        .uMips       = 1,
        .tType       = VK_IMAGE_TYPE_2D
    };
    static const float afSinglePixel[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    uint32_t uDummyTexture = ptDeviceApi->create_texture(ptDevice, tTextureDesc2, sizeof(unsigned char) * 4, afSinglePixel, "dummy texture");

    const plSampler tDummySampler = 
    {
        .fMinMip = 0.0f,
        .fMaxMip = 64.0f,
        .tFilter = PL_FILTER_LINEAR
    };

    const plTextureViewDesc tDummyView = {
        .tFormat     = tTextureDesc2.tFormat,
        .uLayerCount = tTextureDesc2.uLayers,
        .uMips       = tTextureDesc2.uMips
    };

    ptDeviceApi->create_texture_view(ptDevice, &tDummyView, &tDummySampler, uDummyTexture, "dummy texture view");

    ptRenderer->ptGraphics = ptGraphics;

    //~~~~~~~~~~~~~~~~~~~~~~~~create shader descriptions~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // main
    plShaderDesc tMainShaderDesc = {
        .pcPixelShader                       = "phong.frag.spv",
        .pcVertexShader                      = "primitive.vert.spv",
        .tGraphicsState.ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_HAS_NORMAL | PL_MESH_FORMAT_FLAG_HAS_COLOR_0,
        .tGraphicsState.ulDepthMode          = PL_DEPTH_MODE_LESS,
        .tGraphicsState.ulBlendMode          = PL_BLEND_MODE_ALPHA,
        .tGraphicsState.ulCullMode           = VK_CULL_MODE_NONE,
        .tGraphicsState.ulDepthWriteEnabled  = VK_TRUE,
        .tGraphicsState.ulShaderTextureFlags = PL_SHADER_TEXTURE_FLAG_BINDING_NONE,
        .tGraphicsState.ulStencilMode        = PL_STENCIL_MODE_ALWAYS,
        .uBindGroupLayoutCount               = 3,
        .atBindGroupLayouts                  = {
            {
                .uBufferCount = 2,
                .aBuffers = {
                    { .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 1, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT }
                },
            },
            {
                .uBufferCount = 1,
                .aBuffers      = {
                    { .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_FRAGMENT_BIT }
                },
                .uTextureCount = 3,
                .aTextures     = {
                    { .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uSlot = 1, .tStageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
                    { .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uSlot = 2, .tStageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
                    { .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uSlot = 3, .tStageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
                }
            },
            {
                .uBufferCount  = 1,
                .aBuffers      = {
                    { .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}
                }
            }
        },   
    };

    // skybox
    plShaderDesc tSkyboxShaderDesc = {
        .pcPixelShader                       = "skybox.frag.spv",
        .pcVertexShader                      = "skybox.vert.spv",
        .tGraphicsState.ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_NONE,
        .tGraphicsState.ulDepthMode          = PL_DEPTH_MODE_LESS_OR_EQUAL,
        .tGraphicsState.ulBlendMode          = PL_BLEND_MODE_ADDITIVE,
        .tGraphicsState.ulCullMode           = VK_CULL_MODE_NONE,
        .tGraphicsState.ulDepthWriteEnabled  = VK_FALSE,
        .tGraphicsState.ulShaderTextureFlags = PL_SHADER_TEXTURE_FLAG_BINDING_NONE,
        .tGraphicsState.ulStencilMode        = PL_STENCIL_MODE_ALWAYS,
        .uBindGroupLayoutCount               = 1,
        .atBindGroupLayouts                  = {
            {
                .uBufferCount = 1,
                .aBuffers = {
                    { .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}
                },
                .uTextureCount = 1,
                .aTextures     = {
                    { .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uSlot = 1, .tStageFlags = VK_SHADER_STAGE_FRAGMENT_BIT }
                }
            }
        },   
    };

    // outline
    plShaderDesc tOutlineShaderDesc = {
        .pcPixelShader                       = "outline.frag.spv",
        .pcVertexShader                      = "outline.vert.spv",
        .tGraphicsState.ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_HAS_NORMAL,
        .tGraphicsState.ulDepthMode          = PL_DEPTH_MODE_ALWAYS,
        .tGraphicsState.ulBlendMode          = PL_BLEND_MODE_ALPHA,
        .tGraphicsState.ulCullMode           = VK_CULL_MODE_FRONT_BIT,
        .tGraphicsState.ulDepthWriteEnabled  = VK_TRUE,
        .tGraphicsState.ulShaderTextureFlags = PL_SHADER_TEXTURE_FLAG_BINDING_NONE,
        .tGraphicsState.ulStencilMode        = PL_STENCIL_MODE_ALWAYS,
        .uBindGroupLayoutCount               = 3,
        .atBindGroupLayouts                  = {
            {
                .uBufferCount = 2,
                .aBuffers = {
                    { .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 1, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT }
                },
            },
            {
                .uBufferCount = 1,
                .aBuffers      = {
                    { .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_FRAGMENT_BIT }
                },
                .uTextureCount = 3,
                .aTextures     = {
                    { .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uSlot = 1, .tStageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
                    { .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uSlot = 2, .tStageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
                    { .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uSlot = 3, .tStageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
                }
            },
            {
                .uBufferCount  = 1,
                .aBuffers      = {
                    { .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}
                }
            }
        },   
    };

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~create shaders~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    ptRenderer->uMainShader    = ptGfx->create_shader(ptGraphics, &tMainShaderDesc);
    ptRenderer->uOutlineShader = ptGfx->create_shader(ptGraphics, &tOutlineShaderDesc);
    ptRenderer->uSkyboxShader  = ptGfx->create_shader(ptGraphics, &tSkyboxShaderDesc);

}

static void
pl_cleanup_renderer(plRenderer* ptRenderer)
{
    // pl_sb_free(ptRenderer->sbfStorageBuffer);
    // pl_submit_buffer_for_deletion(&ptRenderer->ptGraphics->tResourceManager, ptRenderer->uGlobalStorageBuffer);
}

static void
pl_create_scene(plRenderer* ptRenderer, plScene* ptSceneOut)
{
    plGraphics* ptGraphics = ptRenderer->ptGraphics;
    plGraphicsApiI* ptGfx = ptRenderer->ptGfx;
    plMemoryApiI* ptMemoryApi = ptRenderer->ptMemoryApi;
    plImageApiI* ptImageApi = ptRenderer->ptImageApi;
    plDeviceApiI* ptDeviceApi = ptGraphics->ptDeviceApi;
    plDevice* ptDevice = &ptGraphics->tDevice;

    memset(ptSceneOut, 0, sizeof(plScene));

    ptSceneOut->uGlobalStorageBuffer = UINT32_MAX;
    ptSceneOut->ptRenderer = ptRenderer;
    ptSceneOut->bMaterialsNeedUpdate = true;
    ptSceneOut->tComponentLibrary.tNextEntity = 1;

    // initialize component managers
    ptSceneOut->tComponentLibrary.tTagComponentManager.tComponentType = PL_COMPONENT_TYPE_TAG;
    ptSceneOut->tComponentLibrary.tTagComponentManager.szStride = sizeof(plTagComponent);

    ptSceneOut->tComponentLibrary.tTransformComponentManager.tComponentType = PL_COMPONENT_TYPE_TRANSFORM;
    ptSceneOut->tComponentLibrary.tTransformComponentManager.szStride = sizeof(plTransformComponent);

    ptSceneOut->tComponentLibrary.tObjectComponentManager.tComponentType = PL_COMPONENT_TYPE_OBJECT;
    ptSceneOut->tComponentLibrary.tObjectComponentManager.szStride = sizeof(plObjectComponent);

    ptSceneOut->tComponentLibrary.tMaterialComponentManager.tComponentType = PL_COMPONENT_TYPE_MATERIAL;
    ptSceneOut->tComponentLibrary.tMaterialComponentManager.szStride = sizeof(plMaterialComponent);

    ptSceneOut->tComponentLibrary.tOutlineMaterialComponentManager.tComponentType = PL_COMPONENT_TYPE_MATERIAL;
    ptSceneOut->tComponentLibrary.tOutlineMaterialComponentManager.szStride = sizeof(plMaterialComponent);

    ptSceneOut->tComponentLibrary.tMeshComponentManager.tComponentType = PL_COMPONENT_TYPE_MESH;
    ptSceneOut->tComponentLibrary.tMeshComponentManager.szStride = sizeof(plMeshComponent);

    ptSceneOut->tComponentLibrary.tCameraComponentManager.tComponentType = PL_COMPONENT_TYPE_CAMERA;
    ptSceneOut->tComponentLibrary.tCameraComponentManager.szStride = sizeof(plCameraComponent);

    ptSceneOut->tComponentLibrary.tHierarchyComponentManager.tComponentType = PL_COMPONENT_TYPE_HIERARCHY;
    ptSceneOut->tComponentLibrary.tHierarchyComponentManager.szStride = sizeof(plHierarchyComponent);

    ptSceneOut->uDynamicBuffer0 = ptDeviceApi->create_constant_buffer(ptDevice, ptDevice->tDeviceProps.limits.maxUniformBufferRange, "renderer dynamic buffer 0");

    ptSceneOut->ptOutlineMaterialComponentManager = &ptSceneOut->tComponentLibrary.tOutlineMaterialComponentManager;
    ptSceneOut->ptTagComponentManager = &ptSceneOut->tComponentLibrary.tTagComponentManager;
    ptSceneOut->ptTransformComponentManager = &ptSceneOut->tComponentLibrary.tTransformComponentManager;
    ptSceneOut->ptMeshComponentManager = &ptSceneOut->tComponentLibrary.tMeshComponentManager;
    ptSceneOut->ptMaterialComponentManager = &ptSceneOut->tComponentLibrary.tMaterialComponentManager;
    ptSceneOut->ptObjectComponentManager = &ptSceneOut->tComponentLibrary.tObjectComponentManager;
    ptSceneOut->ptCameraComponentManager = &ptSceneOut->tComponentLibrary.tCameraComponentManager;
    ptSceneOut->ptHierarchyComponentManager = &ptSceneOut->tComponentLibrary.tHierarchyComponentManager;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~skybox~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


    int texWidth, texHeight, texNumChannels;
    int texForceNumChannels = 4;
    unsigned char* rawBytes0 = ptImageApi->load("../data/pilotlight-assets-master/SkyBox/right.png", &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
    unsigned char* rawBytes1 = ptImageApi->load("../data/pilotlight-assets-master/SkyBox/left.png", &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
    unsigned char* rawBytes2 = ptImageApi->load("../data/pilotlight-assets-master/SkyBox/top.png", &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
    unsigned char* rawBytes3 = ptImageApi->load("../data/pilotlight-assets-master/SkyBox/bottom.png", &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
    unsigned char* rawBytes4 = ptImageApi->load("../data/pilotlight-assets-master/SkyBox/front.png", &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
    unsigned char* rawBytes5 = ptImageApi->load("../data/pilotlight-assets-master/SkyBox/back.png", &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
    PL_ASSERT(rawBytes0);
    PL_ASSERT(rawBytes1);
    PL_ASSERT(rawBytes2);
    PL_ASSERT(rawBytes3);
    PL_ASSERT(rawBytes4);
    PL_ASSERT(rawBytes5);

    unsigned char* rawBytes = ptMemoryApi->alloc(texWidth * texHeight * texNumChannels * 6, __FUNCTION__, __LINE__);
    memcpy(&rawBytes[texWidth * texHeight * texNumChannels * 0], rawBytes0, texWidth * texHeight * texNumChannels); //-V522 
    memcpy(&rawBytes[texWidth * texHeight * texNumChannels * 1], rawBytes1, texWidth * texHeight * texNumChannels); //-V522
    memcpy(&rawBytes[texWidth * texHeight * texNumChannels * 2], rawBytes2, texWidth * texHeight * texNumChannels); //-V522
    memcpy(&rawBytes[texWidth * texHeight * texNumChannels * 3], rawBytes3, texWidth * texHeight * texNumChannels); //-V522
    memcpy(&rawBytes[texWidth * texHeight * texNumChannels * 4], rawBytes4, texWidth * texHeight * texNumChannels); //-V522
    memcpy(&rawBytes[texWidth * texHeight * texNumChannels * 5], rawBytes5, texWidth * texHeight * texNumChannels); //-V522

    ptImageApi->free(rawBytes0);
    ptImageApi->free(rawBytes1);
    ptImageApi->free(rawBytes2);
    ptImageApi->free(rawBytes3);
    ptImageApi->free(rawBytes4);
    ptImageApi->free(rawBytes5);

    const plTextureDesc tTextureDesc = {
        .tDimensions = {.x = (float)texWidth, .y = (float)texHeight, .z = 1.0f},
        .tFormat     = PL_FORMAT_R8G8B8A8_UNORM,
        .tUsage      = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .uLayers     = 6,
        .uMips       = 1,
        .tType       = VK_IMAGE_TYPE_2D
    };

    const plSampler tSkyboxSampler = 
    {
        .fMinMip = 0.0f,
        .fMaxMip = 64.0f,
        .tFilter = PL_FILTER_LINEAR
    };

    const plTextureViewDesc tSkyboxView = {
        .tFormat     = tTextureDesc.tFormat,
        .uLayerCount = tTextureDesc.uLayers,
        .uMips       = tTextureDesc.uMips
    };

    uint32_t uSkyboxTexture = ptDeviceApi->create_texture(ptDevice, tTextureDesc, sizeof(unsigned char) * texWidth * texHeight * texNumChannels * 6, rawBytes, "skybox texture");
    ptSceneOut->uSkyboxTextureView  = ptDeviceApi->create_texture_view(ptDevice, &tSkyboxView, &tSkyboxSampler, uSkyboxTexture, "skybox texture view");
    ptMemoryApi->free(rawBytes);

    const float fCubeSide = 0.5f;
    float acSkyBoxVertices[] = {
        -fCubeSide, -fCubeSide, -fCubeSide,
         fCubeSide, -fCubeSide, -fCubeSide,
        -fCubeSide,  fCubeSide, -fCubeSide,
         fCubeSide,  fCubeSide, -fCubeSide,
        -fCubeSide, -fCubeSide,  fCubeSide,
         fCubeSide, -fCubeSide,  fCubeSide,
        -fCubeSide,  fCubeSide,  fCubeSide,
         fCubeSide,  fCubeSide,  fCubeSide 
    };

    uint32_t acSkyboxIndices[] =
    {
        0, 2, 1, 2, 3, 1,
        1, 3, 5, 3, 7, 5,
        2, 6, 3, 3, 6, 7,
        4, 5, 7, 4, 7, 6,
        0, 4, 2, 2, 4, 6,
        0, 1, 4, 1, 5, 4
    };

    const plSubMesh tSubMesh = {
        .tMesh = {
            .uIndexCount   = 36,
            .uVertexCount  = 24,
            .uIndexBuffer  = ptDeviceApi->create_index_buffer(ptDevice, sizeof(uint32_t) * 36, acSkyboxIndices, "skybox index buffer"),
            .uVertexBuffer = ptDeviceApi->create_vertex_buffer(ptDevice, sizeof(float) * 24, sizeof(float), acSkyBoxVertices, "skybox vertex buffer"),
            .ulVertexStreamMask = PL_MESH_FORMAT_FLAG_NONE
        }
    };
    pl_sb_push(ptSceneOut->tSkyboxMesh.sbtSubmeshes, tSubMesh);

    plBindGroupLayout tSkyboxGroupLayout0 = {
        .uBufferCount = 1,
        .aBuffers      = {
            { .tType       = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}
        },
        .uTextureCount = 1,
        .aTextures     = {
            { .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uSlot = 1, .tStageFlags = VK_SHADER_STAGE_FRAGMENT_BIT }
        }
    };
    size_t szSkyboxRangeSize = sizeof(plGlobalInfo);
    ptSceneOut->tSkyboxBindGroup0.tLayout = tSkyboxGroupLayout0;
    ptGfx->update_bind_group(ptGraphics, &ptSceneOut->tSkyboxBindGroup0, 1, &ptSceneOut->uDynamicBuffer0, &szSkyboxRangeSize, 1, &ptSceneOut->uSkyboxTextureView);

    ptSceneOut->uGlobalStorageBuffer = UINT32_MAX;

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
    ptSceneOut->tGlobalBindGroup.tLayout = tGlobalGroupLayout;
}

static void
pl_reset_scene(plScene* ptScene)
{
    ptScene->bFirstEcsUpdate = true;
    ptScene->uDynamicBuffer0_Offset = 0;
}

static void
pl_draw_scene(plScene* ptScene)
{
    plGraphics* ptGraphics = ptScene->ptRenderer->ptGraphics;
    plRenderer* ptRenderer = ptScene->ptRenderer;
    plGraphicsApiI* ptGfx = ptRenderer->ptGfx;
    plEcsI* ptEcs = ptScene->ptRenderer->ptEcs;

    const plBuffer* ptBuffer0 = &ptGraphics->tDevice.sbtBuffers[ptScene->uDynamicBuffer0];
    const uint32_t uBufferFrameOffset0 = ((uint32_t)ptBuffer0->tAllocation.ulSize / ptGraphics->uFramesInFlight) * (uint32_t)ptGraphics->szCurrentFrameIndex + ptScene->uDynamicBuffer0_Offset;

    const uint32_t uDrawOffset = pl_sb_size(ptRenderer->sbtDraws);
    const uint32_t uOutlineDrawOffset = pl_sb_size(ptRenderer->sbtOutlineDraws);

    // record draws
    for(uint32_t i = 0; i < pl_sb_size(ptRenderer->sbtObjectEntities); i++)
    {
        plObjectComponent* ptObjectComponent = ptEcs->get_component(&ptScene->tComponentLibrary.tObjectComponentManager, ptRenderer->sbtObjectEntities[i]);
        plMeshComponent* ptMeshComponent = ptEcs->get_component(&ptScene->tComponentLibrary.tMeshComponentManager, ptObjectComponent->tMesh);
        plTransformComponent* ptTransformComponent = ptEcs->get_component(&ptScene->tComponentLibrary.tTransformComponentManager, ptObjectComponent->tTransform);
        for(uint32_t j = 0; j < pl_sb_size(ptMeshComponent->sbtSubmeshes); j++)
        {
            plSubMesh* ptSubmesh = &ptMeshComponent->sbtSubmeshes[j];
            plMaterialComponent* ptMaterial = ptEcs->get_component(&ptScene->tComponentLibrary.tMaterialComponentManager, ptSubmesh->tMaterial);

            pl_sb_push(ptRenderer->sbtDraws, ((plDraw){
                .uShaderVariant        = ptMaterial->uShaderVariant,
                .ptMesh                = &ptSubmesh->tMesh,
                .ptBindGroup1          = &ptRenderer->sbtMaterialBindGroups[ptMaterial->uBindGroup1],
                .ptBindGroup2          = &ptRenderer->sbtObjectBindGroups[ptTransformComponent->uBindGroup2],
                .uDynamicBufferOffset1 = ptMaterial->uBufferOffset,
                .uDynamicBufferOffset2 = ptTransformComponent->uBufferOffset
                }));
            ptTransformComponent->tInfo.uVertexOffset = ptSubmesh->uStorageOffset;

            if(ptMaterial->bOutline)
            {
                plMaterialComponent* ptOutlineMaterial = ptEcs->get_component(&ptScene->tComponentLibrary.tOutlineMaterialComponentManager, ptSubmesh->tMaterial);

                pl_sb_push(ptRenderer->sbtOutlineDraws, ((plDraw){
                    .uShaderVariant        = ptOutlineMaterial->uShaderVariant,
                    .ptMesh                = &ptSubmesh->tMesh,
                    .ptBindGroup1          = &ptRenderer->sbtMaterialBindGroups[ptMaterial->uBindGroup1],
                    .ptBindGroup2          = &ptRenderer->sbtObjectBindGroups[ptTransformComponent->uBindGroup2],
                    .uDynamicBufferOffset1 = ptOutlineMaterial->uBufferOffset,
                    .uDynamicBufferOffset2 = ptTransformComponent->uBufferOffset
                    }));
            }
        }
    }

    // record draw area
    pl_sb_push(ptRenderer->sbtDrawAreas, ((plDrawArea){
        .ptBindGroup0          = &ptScene->tGlobalBindGroup,
        .uDrawOffset           = uDrawOffset,
        .uDrawCount            = pl_sb_size(ptRenderer->sbtDraws),
        .uDynamicBufferOffset0 = uBufferFrameOffset0
    }));

    ptGfx->draw_areas(ptRenderer->ptGraphics, pl_sb_size(ptRenderer->sbtDrawAreas), ptRenderer->sbtDrawAreas, ptRenderer->sbtDraws);

    pl_sb_reset(ptRenderer->sbtDrawAreas);

    // record outlines draw areas
    pl_sb_push(ptRenderer->sbtDrawAreas, ((plDrawArea){
        .ptBindGroup0          = &ptScene->tGlobalBindGroup,
        .uDrawOffset           = uOutlineDrawOffset,
        .uDrawCount            = pl_sb_size(ptRenderer->sbtOutlineDraws),
        .uDynamicBufferOffset0 = uBufferFrameOffset0
    }));

    ptGfx->draw_areas(ptRenderer->ptGraphics, pl_sb_size(ptRenderer->sbtDrawAreas), ptRenderer->sbtDrawAreas, ptRenderer->sbtOutlineDraws);

    pl_sb_reset(ptRenderer->sbtDraws);
    pl_sb_reset(ptRenderer->sbtOutlineDraws);
    pl_sb_reset(ptRenderer->sbtDrawAreas);

    ptScene->uDynamicBuffer0_Offset = (uint32_t)pl_align_up((size_t)ptScene->uDynamicBuffer0_Offset + sizeof(plGlobalInfo), ptGraphics->tDevice.tDeviceProps.limits.minUniformBufferOffsetAlignment);
}

static void
pl_scene_update_ecs(plScene* ptScene)
{

    pl_ecs_update(ptScene, &ptScene->tComponentLibrary.tMaterialComponentManager);
    pl_ecs_update(ptScene, &ptScene->tComponentLibrary.tOutlineMaterialComponentManager);

    if(ptScene->bFirstEcsUpdate)
    {
        pl_ecs_update(ptScene, &ptScene->tComponentLibrary.tObjectComponentManager); 
        pl_ecs_update(ptScene, &ptScene->tComponentLibrary.tHierarchyComponentManager); 
        pl_ecs_update(ptScene, &ptScene->tComponentLibrary.tTransformComponentManager); 
    }

    ptScene->bFirstEcsUpdate = false;
}

static void
pl_scene_bind_target(plScene* ptScene, plRenderTarget* ptTarget)
{
    ptScene->ptRenderTarget = ptTarget;
}

static void
pl_scene_prepare(plScene* ptScene)
{

    plGraphics* ptGraphics = ptScene->ptRenderer->ptGraphics;
    plRenderer* ptRenderer = ptScene->ptRenderer;
    plGraphicsApiI* ptGfx = ptRenderer->ptGfx;
    plDeviceApiI* ptDeviceApi = ptGraphics->ptDeviceApi;
    plDevice* ptDevice = &ptGraphics->tDevice;
    plEcsI* ptEcs = ptScene->ptRenderer->ptEcs;

    uint32_t uStorageOffset = pl_sb_size(ptScene->sbfStorageBuffer) / 4;

    plMeshComponent* sbtMeshes = ptScene->tComponentLibrary.tMeshComponentManager.pData;

    // calculate normals and tangents
    for(uint32_t uMeshIndex = 0; uMeshIndex < pl_sb_size(sbtMeshes); uMeshIndex++)
    {
        plMeshComponent* ptMesh = &sbtMeshes[uMeshIndex];

        for(uint32_t uSubMeshIndex = 0; uSubMeshIndex < pl_sb_size(ptMesh->sbtSubmeshes); uSubMeshIndex++)
        {
            plSubMesh* ptSubMesh = &ptMesh->sbtSubmeshes[uSubMeshIndex];

            if(pl_sb_size(ptSubMesh->sbtVertexNormals) == 0)
            {
                pl_sb_resize(ptSubMesh->sbtVertexNormals, pl_sb_size(ptSubMesh->sbtVertexPositions));
                for(uint32_t i = 0; i < pl_sb_size(ptSubMesh->sbuIndices) - 2; i += 3)
                {
					const uint32_t uIndex0 = ptSubMesh->sbuIndices[i + 0];
					const uint32_t uIndex1 = ptSubMesh->sbuIndices[i + 1];
					const uint32_t uIndex2 = ptSubMesh->sbuIndices[i + 2];

					const plVec3 tP0 = ptSubMesh->sbtVertexPositions[uIndex0];
					const plVec3 tP1 = ptSubMesh->sbtVertexPositions[uIndex1];
					const plVec3 tP2 = ptSubMesh->sbtVertexPositions[uIndex2];

					const plVec3 tEdge1 = pl_sub_vec3(tP1, tP0);
					const plVec3 tEdge2 = pl_sub_vec3(tP2, tP0);

					const plVec3 tNorm = pl_norm_vec3(pl_cross_vec3(tEdge1, tEdge2));

                    ptSubMesh->sbtVertexNormals[uIndex0] = tNorm;
                    ptSubMesh->sbtVertexNormals[uIndex1] = tNorm;
                    ptSubMesh->sbtVertexNormals[uIndex2] = tNorm;
                }
            }

            if(pl_sb_size(ptSubMesh->sbtVertexTangents) == 0 && pl_sb_size(ptSubMesh->sbtVertexTextureCoordinates0) > 0)
            {
                pl_sb_resize(ptSubMesh->sbtVertexTangents, pl_sb_size(ptSubMesh->sbtVertexPositions));
                for(uint32_t i = 0; i < pl_sb_size(ptSubMesh->sbuIndices) - 2; i += 3)
                {
					const uint32_t uIndex0 = ptSubMesh->sbuIndices[i + 0];
					const uint32_t uIndex1 = ptSubMesh->sbuIndices[i + 1];
					const uint32_t uIndex2 = ptSubMesh->sbuIndices[i + 2];

					const plVec3 tP0 = ptSubMesh->sbtVertexPositions[uIndex0];
					const plVec3 tP1 = ptSubMesh->sbtVertexPositions[uIndex1];
					const plVec3 tP2 = ptSubMesh->sbtVertexPositions[uIndex2];

					const plVec2 tTex0 = ptSubMesh->sbtVertexTextureCoordinates0[uIndex0];
					const plVec2 tTex1 = ptSubMesh->sbtVertexTextureCoordinates0[uIndex1];
					const plVec2 tTex2 = ptSubMesh->sbtVertexTextureCoordinates0[uIndex2];

					const plVec3 tEdge1 = pl_sub_vec3(tP1, tP0);
					const plVec3 tEdge2 = pl_sub_vec3(tP2, tP0);

					const float fDeltaU1 = tTex1.x - tTex0.x;
					const float fDeltaV1 = tTex1.y - tTex0.y;
					const float fDeltaU2 = tTex2.x - tTex0.x;
					const float fDeltaV2 = tTex2.y - tTex0.y;

					const float fDividend = (fDeltaU1 * fDeltaV2 - fDeltaU2 * fDeltaV1);
					const float fC = 1.0f / fDividend;

					const float fSx = fDeltaU1;
					const float fSy = fDeltaU2;
					const float fTx = fDeltaV1;
					const float fTy = fDeltaV2;
					const float fHandedness = ((fTx * fSy - fTy * fSx) < 0.0f) ? -1.0f : 1.0f;

					const plVec3 tTangent = 
						pl_norm_vec3((plVec3){
							fC * (fDeltaV2 * tEdge1.x - fDeltaV1 * tEdge2.x),
							fC * (fDeltaV2 * tEdge1.y - fDeltaV1 * tEdge2.y),
							fC * (fDeltaV2 * tEdge1.z - fDeltaV1 * tEdge2.z)
					});

                    ptSubMesh->sbtVertexTangents[uIndex0] = (plVec4){tTangent.x, tTangent.y, tTangent.z, fHandedness};
                    ptSubMesh->sbtVertexTangents[uIndex1] = (plVec4){tTangent.x, tTangent.y, tTangent.z, fHandedness};
                    ptSubMesh->sbtVertexTangents[uIndex2] = (plVec4){tTangent.x, tTangent.y, tTangent.z, fHandedness};
                } 
            }
        }
    }

    for(uint32_t uMeshIndex = 0; uMeshIndex < pl_sb_size(sbtMeshes); uMeshIndex++)
    {
        plMeshComponent* ptMesh = &sbtMeshes[uMeshIndex];

        for(uint32_t uSubMeshIndex = 0; uSubMeshIndex < pl_sb_size(ptMesh->sbtSubmeshes); uSubMeshIndex++)
        {
            plSubMesh* ptSubMesh = &ptMesh->sbtSubmeshes[uSubMeshIndex];

            ptSubMesh->uStorageOffset = uStorageOffset / 4;
            ptSubMesh->tMesh.uVertexCount = pl_sb_size(ptSubMesh->sbtVertexPositions);
            ptSubMesh->tMesh.uIndexCount = pl_sb_size(ptSubMesh->sbuIndices);
            ptSubMesh->tMesh.uIndexOffset = 0;

            // stride within storage buffer
            uint32_t uStride = 0;

            if(pl_sb_size(ptSubMesh->sbtVertexNormals) > 0)             { uStride += 4; ptSubMesh->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_NORMAL; }
            if(pl_sb_size(ptSubMesh->sbtVertexTangents) > 0)            { uStride += 4; ptSubMesh->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TANGENT; }
            if(pl_sb_size(ptSubMesh->sbtVertexColors0) > 0)             { uStride += 4; ptSubMesh->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_COLOR_0; }
            if(pl_sb_size(ptSubMesh->sbtVertexColors1) > 0)             { uStride += 4; ptSubMesh->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_COLOR_1; }
            if(pl_sb_size(ptSubMesh->sbtVertexWeights0) > 0)            { uStride += 4; ptSubMesh->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0; }
            if(pl_sb_size(ptSubMesh->sbtVertexWeights1) > 0)            { uStride += 4; ptSubMesh->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1; }
            if(pl_sb_size(ptSubMesh->sbtVertexJoints0) > 0)             { uStride += 4; ptSubMesh->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_JOINTS_0; }
            if(pl_sb_size(ptSubMesh->sbtVertexJoints1) > 0)             { uStride += 4; ptSubMesh->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_JOINTS_1; }
            if(pl_sb_size(ptSubMesh->sbtVertexTextureCoordinates0) > 0) { uStride += 4; ptSubMesh->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0; }
            if(pl_sb_size(ptSubMesh->sbtVertexTextureCoordinates1) > 0) { uStride += 4; ptSubMesh->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1; }

            pl_sb_add_n(ptScene->sbfStorageBuffer, uStride * ptSubMesh->tMesh.uVertexCount);

            // current attribute offset
            uint32_t uOffset = 0;

            // normals
            for(uint32_t i = 0; i < pl_sb_size(ptSubMesh->sbtVertexNormals); i++)
            {
                const plVec3* ptNormal = &ptSubMesh->sbtVertexNormals[i];
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + 0] = ptNormal->x;
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + 1] = ptNormal->y;
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + 2] = ptNormal->z;
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + 3] = 0.0f;
            }

            if(pl_sb_size(ptSubMesh->sbtVertexNormals) > 0)
                uOffset += 4;

            // tangents
            for(uint32_t i = 0; i < pl_sb_size(ptSubMesh->sbtVertexTangents); i++)
            {
                const plVec4* ptTangent = &ptSubMesh->sbtVertexTangents[i];
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 0] = ptTangent->x;
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 1] = ptTangent->y;
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 2] = ptTangent->z;
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 3] = ptTangent->w;
            }

            if(pl_sb_size(ptSubMesh->sbtVertexTangents) > 0)
                uOffset += 4;

            // texture coordinates 0
            for(uint32_t i = 0; i < pl_sb_size(ptSubMesh->sbtVertexTextureCoordinates0); i++)
            {
                const plVec2* ptTextureCoordinates = &ptSubMesh->sbtVertexTextureCoordinates0[i];
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 0] = ptTextureCoordinates->u;
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 1] = ptTextureCoordinates->v;
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 2] = 0.0f;
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 3] = 0.0f;
            }

            if(pl_sb_size(ptSubMesh->sbtVertexTextureCoordinates0) > 0)
                uOffset += 4;

            // texture coordinates 1
            for(uint32_t i = 0; i < pl_sb_size(ptSubMesh->sbtVertexTextureCoordinates1); i++)
            {
                const plVec2* ptTextureCoordinates = &ptSubMesh->sbtVertexTextureCoordinates1[i];
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 0] = ptTextureCoordinates->u;
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 1] = ptTextureCoordinates->v;
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 2] = 0.0f;
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 3] = 0.0f;
            }

            if(pl_sb_size(ptSubMesh->sbtVertexTextureCoordinates1) > 0)
                uOffset += 4;

            // color 0
            for(uint32_t i = 0; i < pl_sb_size(ptSubMesh->sbtVertexColors0); i++)
            {
                const plVec4* ptColor = &ptSubMesh->sbtVertexColors0[i];
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 0] = ptColor->r;
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 1] = ptColor->g;
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 2] = ptColor->b;
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 3] = ptColor->a;
            }

            if(pl_sb_size(ptSubMesh->sbtVertexColors0) > 0)
                uOffset += 4;

            // color 1
            for(uint32_t i = 0; i < pl_sb_size(ptSubMesh->sbtVertexColors1); i++)
            {
                const plVec4* ptColor = &ptSubMesh->sbtVertexColors1[i];
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 0] = ptColor->r;
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 1] = ptColor->g;
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 2] = ptColor->b;
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 3] = ptColor->a;
            }

            if(pl_sb_size(ptSubMesh->sbtVertexColors1) > 0)
                uOffset += 4;

            // joints 0
            for(uint32_t i = 0; i < pl_sb_size(ptSubMesh->sbtVertexJoints0); i++)
            {
                const plVec4* ptJoint = &ptSubMesh->sbtVertexJoints0[i];
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 0] = ptJoint->x;
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 1] = ptJoint->y;
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 2] = ptJoint->z;
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 3] = ptJoint->w;
            }

            if(pl_sb_size(ptSubMesh->sbtVertexJoints0) > 0)
                uOffset += 4;

            // joints 1
            for(uint32_t i = 0; i < pl_sb_size(ptSubMesh->sbtVertexJoints1); i++)
            {
                const plVec4* ptJoint = &ptSubMesh->sbtVertexJoints1[i];
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 0] = ptJoint->x;
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 1] = ptJoint->y;
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 2] = ptJoint->z;
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 3] = ptJoint->w;
            }

            if(pl_sb_size(ptSubMesh->sbtVertexJoints1) > 0)
                uOffset += 4;

            // weights 0
            for(uint32_t i = 0; i < pl_sb_size(ptSubMesh->sbtVertexWeights0); i++)
            {
                const plVec4* ptWeight = &ptSubMesh->sbtVertexWeights0[i];
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 0] = ptWeight->x;
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 1] = ptWeight->y;
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 2] = ptWeight->z;
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 3] = ptWeight->w;
            }

            if(pl_sb_size(ptSubMesh->sbtVertexWeights0) > 0)
                uOffset += 4;

            // weights 1
            for(uint32_t i = 0; i < pl_sb_size(ptSubMesh->sbtVertexWeights1); i++)
            {
                const plVec4* ptWeight = &ptSubMesh->sbtVertexWeights1[i];
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 0] = ptWeight->x;
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 1] = ptWeight->y;
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 2] = ptWeight->z;
                ptScene->sbfStorageBuffer[uStorageOffset + i * uStride + uOffset + 3] = ptWeight->w;
            }

            if(pl_sb_size(ptSubMesh->sbtVertexWeights1) > 0)
                uOffset += 4;

            PL_ASSERT(uOffset == uStride && "sanity check");


            ptSubMesh->tMesh.uIndexBuffer = ptDeviceApi->create_index_buffer(ptDevice, 
                sizeof(uint32_t) * ptSubMesh->tMesh.uIndexCount,
                ptSubMesh->sbuIndices, "unnamed index buffer");

            ptSubMesh->tMesh.uVertexBuffer = ptDeviceApi->create_vertex_buffer(ptDevice, 
                ptSubMesh->tMesh.uVertexCount * sizeof(plVec3), sizeof(plVec3),
                ptSubMesh->sbtVertexPositions, "unnamed vertex buffer");

            plMaterialComponent* ptMaterialComponent = ptEcs->get_component(ptScene->ptMaterialComponentManager, ptSubMesh->tMaterial);
            ptMaterialComponent->tGraphicsState.ulVertexStreamMask = ptSubMesh->tMesh.ulVertexStreamMask;

            uStorageOffset += uStride * ptSubMesh->tMesh.uVertexCount;
        }
    }

    if(pl_sb_size(ptScene->sbfStorageBuffer) > 0)
    {
        if(ptScene->uGlobalStorageBuffer != UINT32_MAX)
        {
            ptDeviceApi->submit_buffer_for_deletion(ptDevice, ptScene->uGlobalStorageBuffer);
        }
        ptScene->uGlobalStorageBuffer = ptDeviceApi->create_storage_buffer(ptDevice, pl_sb_size(ptScene->sbfStorageBuffer) * sizeof(float), ptScene->sbfStorageBuffer, "global storage");
        pl_sb_reset(ptScene->sbfStorageBuffer);

        uint32_t atBuffers0[] = {ptScene->uDynamicBuffer0, ptScene->uGlobalStorageBuffer};
        size_t aszRangeSizes[] = {sizeof(plGlobalInfo), VK_WHOLE_SIZE};
        ptGfx->update_bind_group(ptGraphics, &ptScene->tGlobalBindGroup, 2, atBuffers0, aszRangeSizes, 0, NULL);
    }

}

static void
pl_scene_bind_camera(plScene* ptScene, const plCameraComponent* ptCamera)
{
    ptScene->ptCamera = ptCamera;

    plGraphics* ptGraphics = ptScene->ptRenderer->ptGraphics;
    plRenderer* ptRenderer = ptScene->ptRenderer;

    const plBuffer* ptBuffer0 = &ptGraphics->tDevice.sbtBuffers[ptScene->uDynamicBuffer0];
    const uint32_t uBufferFrameOffset0 = ((uint32_t)ptBuffer0->tAllocation.ulSize / ptGraphics->uFramesInFlight) * (uint32_t)ptGraphics->szCurrentFrameIndex + ptScene->uDynamicBuffer0_Offset;

    plGlobalInfo* ptGlobalInfo    = (plGlobalInfo*)&ptBuffer0->tAllocation.pHostMapped[uBufferFrameOffset0];
    ptGlobalInfo->tAmbientColor   = (plVec4){0.0f, 0.0f, 0.0f, 1.0f};
    ptGlobalInfo->tCameraPos      = (plVec4){.xyz = ptCamera->tPos, .w = 0.0f};
    ptGlobalInfo->tCameraView     = ptCamera->tViewMat;
    ptGlobalInfo->tCameraViewProj = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);

    plIOContext* ptIOCtx = ptGraphics->ptIoInterface->get_context();
    ptGlobalInfo->fTime  = (float)ptIOCtx->dTime;
}

static void
pl_draw_sky(plScene* ptScene)
{

    plGraphics* ptGraphics = ptScene->ptRenderer->ptGraphics;
    plRenderer* ptRenderer = ptScene->ptRenderer;
    plGraphicsApiI* ptGfx = ptRenderer->ptGfx;
    VkSampleCountFlagBits tMSAASampleCount = ptScene->ptRenderTarget->bMSAA ? ptGraphics->tSwapchain.tMsaaSamples : VK_SAMPLE_COUNT_1_BIT;

    const plBuffer* ptBuffer0 = &ptGraphics->tDevice.sbtBuffers[ptScene->uDynamicBuffer0];
    const uint32_t uBufferFrameOffset0 = ((uint32_t)ptBuffer0->tAllocation.ulSize / ptGraphics->uFramesInFlight) * (uint32_t)ptGraphics->szCurrentFrameIndex + ptScene->uDynamicBuffer0_Offset;

    plGlobalInfo* ptGlobalInfo    = (plGlobalInfo*)&ptBuffer0->tAllocation.pHostMapped[uBufferFrameOffset0];
    ptGlobalInfo->tCameraPos      = (plVec4){.xyz = ptScene->ptCamera->tPos, .w = 0.0f};
    const plMat4 tRemoveTranslation = pl_mat4_translate_xyz(ptScene->ptCamera->tPos.x, ptScene->ptCamera->tPos.y, ptScene->ptCamera->tPos.z);
    ptGlobalInfo->tCameraView     = pl_mul_mat4(&ptScene->ptCamera->tViewMat, &tRemoveTranslation);
    ptGlobalInfo->tCameraViewProj = pl_mul_mat4(&ptScene->ptCamera->tProjMat, &ptGlobalInfo->tCameraView);

    uint32_t uSkyboxShaderVariant = UINT32_MAX;

    const plShader* ptShader = &ptGraphics->sbtShaders[ptRenderer->uSkyboxShader];   
    const uint32_t uFillVariantCount = pl_sb_size(ptShader->tDesc.sbtVariants);

    plGraphicsState tFillStateTemplate = {
        .ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_NONE,
        .ulDepthMode          = PL_DEPTH_MODE_LESS_OR_EQUAL,
        .ulDepthWriteEnabled  = false,
        .ulCullMode           = VK_CULL_MODE_NONE,
        .ulBlendMode          = PL_BLEND_MODE_ALPHA,
        .ulShaderTextureFlags = PL_SHADER_TEXTURE_FLAG_BINDING_0,
        .ulStencilMode        = PL_STENCIL_MODE_NOT_EQUAL,
        .ulStencilRef         = 0xff,
        .ulStencilMask        = 0xff,
        .ulStencilOpFail      = VK_STENCIL_OP_KEEP,
        .ulStencilOpDepthFail = VK_STENCIL_OP_KEEP,
        .ulStencilOpPass      = VK_STENCIL_OP_KEEP
    };

    for(uint32_t j = 0; j < uFillVariantCount; j++)
    {
        if(ptShader->tDesc.sbtVariants[j].tGraphicsState.ulValue == tFillStateTemplate.ulValue 
            && ptScene->ptRenderTarget->tDesc.tRenderPass._tRenderPass == ptShader->tDesc.sbtVariants[j].tRenderPass)
        {
                uSkyboxShaderVariant = ptShader->_sbuVariantPipelines[j];
                break;
        }
    }

    // create variant that matches texture count, vertex stream, and culling
    if(uSkyboxShaderVariant == UINT32_MAX)
    {
        pl_log_debug_to_f(ptRenderer->uLogChannel, "adding skybox shader variant");
        uSkyboxShaderVariant = ptGfx->add_shader_variant(ptGraphics, ptRenderer->uSkyboxShader, tFillStateTemplate, ptScene->ptRenderTarget->tDesc.tRenderPass._tRenderPass, tMSAASampleCount);
    }

    pl_sb_push(ptRenderer->sbtDrawAreas, ((plDrawArea){
        .ptBindGroup0          = &ptScene->tSkyboxBindGroup0,
        .uDrawOffset           = pl_sb_size(ptRenderer->sbtDraws),
        .uDrawCount            = 1,
        .uDynamicBufferOffset0 = uBufferFrameOffset0
    }));

    pl_sb_push(ptRenderer->sbtDraws, ((plDraw){
        .uShaderVariant        = uSkyboxShaderVariant,
        .ptMesh                = &ptScene->tSkyboxMesh.sbtSubmeshes[0].tMesh,
        .ptBindGroup1          = NULL,
        .ptBindGroup2          = NULL,
        .uDynamicBufferOffset1 = 0,
        .uDynamicBufferOffset2 = 0
        }));

    ptGfx->draw_areas(ptRenderer->ptGraphics, pl_sb_size(ptRenderer->sbtDrawAreas), ptRenderer->sbtDrawAreas, ptRenderer->sbtDraws);

    pl_sb_reset(ptRenderer->sbtDraws);
    pl_sb_reset(ptRenderer->sbtDrawAreas);

    ptScene->uDynamicBuffer0_Offset = (uint32_t)pl_align_up((size_t)ptScene->uDynamicBuffer0_Offset + sizeof(plGlobalInfo), ptGraphics->tDevice.tDeviceProps.limits.minUniformBufferOffsetAlignment);
}

static void
pl_ecs_update(plScene* ptScene, plComponentManager* ptManager)
{
    plRenderer* ptRenderer = ptScene->ptRenderer;
    plGraphics* ptGraphics = ptRenderer->ptGraphics;
    plGraphicsApiI* ptGfx = ptRenderer->ptGfx;
    plDeviceApiI* ptDeviceApi = ptGraphics->ptDeviceApi;
    plDevice* ptDevice = &ptGraphics->tDevice;
    plEcsI* ptEcs = ptRenderer->ptEcs;

    switch (ptManager->tComponentType)
    {
    case PL_COMPONENT_TYPE_MATERIAL:
    {

        VkSampleCountFlagBits tMSAASampleCount = ptScene->ptRenderTarget->bMSAA ? ptGraphics->tSwapchain.tMsaaSamples : VK_SAMPLE_COUNT_1_BIT;
        uint32_t* sbuTextures = NULL;

        size_t szRangeSize = pl_align_up(sizeof(plMaterialInfo), ptDevice->tDeviceProps.limits.minUniformBufferOffsetAlignment);
        size_t szRangeSize2 = sizeof(plMaterialInfo);
        plMaterialComponent* sbtComponents = ptManager->pData;

        const uint32_t uMaxMaterialsPerBuffer = (uint32_t)(ptDevice->tDeviceProps.limits.maxUniformBufferRange / (uint32_t)szRangeSize) - 1;
        uint32_t uMaterialCount = pl_sb_size(sbtComponents);
        const uint32_t uMinBuffersNeeded = (uint32_t)ceilf((float)uMaterialCount / (float)uMaxMaterialsPerBuffer);
        uint32_t uCurrentMaterial = 0;

        for(uint32_t i = 0; i < uMinBuffersNeeded; i++)
        {
            uint32_t uDynamicBufferIndex = UINT32_MAX;
            plDynamicBufferNode* ptDynamicBufferNode = NULL;
            plBuffer* ptBuffer = NULL;

            if(ptScene->bMaterialsNeedUpdate)
            {
                uDynamicBufferIndex = ptDeviceApi->request_dynamic_buffer(ptDevice);
                ptDynamicBufferNode = &ptDevice->_sbtDynamicBufferList[uDynamicBufferIndex];
                ptBuffer = &ptDevice->sbtBuffers[ptDynamicBufferNode->uDynamicBuffer];
            }

            uint32_t uIterationMaterialCount = pl_minu(uMaxMaterialsPerBuffer, uMaterialCount);
            for(uint32_t j = 0; j < uIterationMaterialCount; j++)
            {
                plMaterialComponent* ptMaterial = &sbtComponents[uCurrentMaterial];

                const uint32_t acShaderLookup[] = {
                    ptScene->ptRenderer->uMainShader,
                    ptScene->ptRenderer->uOutlineShader,
                    ptMaterial->uShader
                };

                ptMaterial->uShader = acShaderLookup[ptMaterial->tShaderType];
                pl_sb_push(sbuTextures, ptMaterial->uAlbedoMap);
                pl_sb_push(sbuTextures, ptMaterial->uNormalMap);
                pl_sb_push(sbuTextures, ptMaterial->uEmissiveMap);

                if(ptScene->bMaterialsNeedUpdate)
                {
                    uint64_t uHashKey = pl_hm_hash(&ptMaterial->uShader, sizeof(uint32_t), 0);
                    uHashKey = pl_hm_hash(&ptMaterial->uAlbedoMap, sizeof(uint32_t), uHashKey);
                    uHashKey = pl_hm_hash(&ptMaterial->uNormalMap, sizeof(uint32_t), uHashKey);
                    uHashKey = pl_hm_hash(&ptMaterial->uEmissiveMap, sizeof(uint32_t), uHashKey);

                    // check if bind group for this buffer exist
                    uint64_t uMaterialBindGroupIndex = pl_hm_lookup(&ptRenderer->tMaterialBindGroupdHashMap, uHashKey);

                    if(uMaterialBindGroupIndex == UINT64_MAX) // doesn't exist
                    {

                        plBindGroup tNewBindGroup = {
                            .tLayout = *ptGfx->get_bind_group_layout(ptGraphics, ptMaterial->uShader, 1)
                        };
                        ptGfx->update_bind_group(ptGraphics, &tNewBindGroup, 1, &ptDynamicBufferNode->uDynamicBuffer, &szRangeSize2, pl_sb_size(sbuTextures), sbuTextures);

                        // check for free index
                        uMaterialBindGroupIndex = pl_hm_get_free_index(&ptRenderer->tMaterialBindGroupdHashMap);

                        if(uMaterialBindGroupIndex == UINT64_MAX) // no free index
                        {
                            pl_sb_push(ptRenderer->sbtMaterialBindGroups, tNewBindGroup);
                            uMaterialBindGroupIndex = pl_sb_size(ptRenderer->sbtMaterialBindGroups) - 1;
                            pl_hm_insert(&ptRenderer->tMaterialBindGroupdHashMap, uHashKey, uMaterialBindGroupIndex);
                        }
                        else // resuse free index
                        {
                            ptRenderer->sbtMaterialBindGroups[uMaterialBindGroupIndex] = tNewBindGroup;
                            pl_hm_insert(&ptRenderer->tMaterialBindGroupdHashMap, uHashKey, uMaterialBindGroupIndex);
                        }  
                    }
                    ptMaterial->uBindGroup1 = uMaterialBindGroupIndex;

                    plMaterialInfo* ptMaterialInfo = (plMaterialInfo*)(ptBuffer->tAllocation.pHostMapped + ptDynamicBufferNode->uDynamicBufferOffset);
                    ptMaterialInfo->tAlbedo = ptMaterial->tAlbedo;
                    ptMaterial->uBufferOffset = ptDynamicBufferNode->uDynamicBufferOffset;
                    ptDynamicBufferNode->uDynamicBufferOffset += (uint32_t)szRangeSize;
                }
                    
                // find variants
                ptMaterial->uShaderVariant = UINT32_MAX;

                ptMaterial->tGraphicsState.ulCullMode = ptMaterial->bDoubleSided ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;
                if(ptMaterial->uAlbedoMap > 0)   ptMaterial->tGraphicsState.ulShaderTextureFlags |= PL_SHADER_TEXTURE_FLAG_BINDING_0;
                if(ptMaterial->uNormalMap > 0)   ptMaterial->tGraphicsState.ulShaderTextureFlags |= PL_SHADER_TEXTURE_FLAG_BINDING_1;
                if(ptMaterial->uEmissiveMap > 0) ptMaterial->tGraphicsState.ulShaderTextureFlags |= PL_SHADER_TEXTURE_FLAG_BINDING_2;

                const plShader* ptShader = &ptGraphics->sbtShaders[ptMaterial->uShader];   
                const uint32_t uVariantCount = pl_sb_size(ptShader->tDesc.sbtVariants);

                for(uint32_t k = 0; k < uVariantCount; k++)
                {
                    plGraphicsState ptVariant = ptShader->tDesc.sbtVariants[k].tGraphicsState;
                    if(ptVariant.ulValue == ptMaterial->tGraphicsState.ulValue 
                        && ptShader->tDesc.sbtVariants[k].tRenderPass == ptScene->ptRenderTarget->tDesc.tRenderPass._tRenderPass
                        && tMSAASampleCount == ptShader->tDesc.sbtVariants[k].tMSAASampleCount)
                    {
                            ptMaterial->uShaderVariant = ptShader->_sbuVariantPipelines[k];
                            break;
                    }
                }

                // create variant that matches texture count, vertex stream, and culling
                if(ptMaterial->uShaderVariant == UINT32_MAX)
                {
                    ptMaterial->uShaderVariant = ptGfx->add_shader_variant(ptGraphics, ptMaterial->uShader, ptMaterial->tGraphicsState, ptScene->ptRenderTarget->tDesc.tRenderPass._tRenderPass, tMSAASampleCount);
                }
                
                pl_sb_reset(sbuTextures);

                uCurrentMaterial++;
            }
            uMaterialCount = uMaterialCount - uIterationMaterialCount;
            // pl_sb_push(ptRenderer->sbuDynamicBufferDeletionQueue, uDynamicBufferIndex);

            if(uMaterialCount == 0)
                break;
        }

        ptScene->bMaterialsNeedUpdate = false;
        break;
    }

    case PL_COMPONENT_TYPE_TRANSFORM:
    {
        size_t szRangeSize = pl_align_up(sizeof(plObjectInfo), ptGraphics->tDevice.tDeviceProps.limits.minUniformBufferOffsetAlignment);
        plTransformComponent* sbtComponents = ptManager->pData;

        const uint32_t uMaxObjectsPerBuffer = (uint32_t)(ptDevice->tDeviceProps.limits.maxUniformBufferRange / (uint32_t)szRangeSize) - 1;
        uint32_t uObjectCount = pl_sb_size(sbtComponents);
        const uint32_t uMinBuffersNeeded = (uint32_t)ceilf((float)uObjectCount / (float)uMaxObjectsPerBuffer);
        uint32_t uCurrentTransform = 0;

        const plBindGroupLayout tGroupLayout2 = {
            .uBufferCount = 1,
            .aBuffers      = {
                { .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}  
            }
        };
        size_t szRangeSize2 = sizeof(plObjectInfo);

        for(uint32_t i = 0; i < uMinBuffersNeeded; i++)
        {
            const uint32_t uDynamicBufferIndex = ptDeviceApi->request_dynamic_buffer(ptDevice);
            plDynamicBufferNode* ptDynamicBufferNode = &ptDevice->_sbtDynamicBufferList[uDynamicBufferIndex];
            plBuffer* ptBuffer = &ptDevice->sbtBuffers[ptDynamicBufferNode->uDynamicBuffer];

            const uint64_t uHashKey = pl_hm_hash(&uDynamicBufferIndex, sizeof(uint64_t), 0);

            // check if bind group for this buffer exist
            uint64_t uObjectBindGroupIndex = pl_hm_lookup(&ptRenderer->tObjectBindGroupdHashMap, uHashKey);

            if(uObjectBindGroupIndex == UINT64_MAX) // doesn't exist
            {
                plBindGroup tNewBindGroup = {
                    .tLayout = tGroupLayout2
                };
                ptGfx->update_bind_group(ptGraphics, &tNewBindGroup, 1, &ptDynamicBufferNode->uDynamicBuffer, &szRangeSize2, 0, NULL);

                // check for free index
                uObjectBindGroupIndex = pl_hm_get_free_index(&ptRenderer->tObjectBindGroupdHashMap);

                if(uObjectBindGroupIndex == UINT64_MAX) // no free index
                {
                    pl_sb_push(ptRenderer->sbtObjectBindGroups, tNewBindGroup);
                    uObjectBindGroupIndex = pl_sb_size(ptRenderer->sbtObjectBindGroups) - 1;
                    pl_hm_insert(&ptRenderer->tObjectBindGroupdHashMap, uHashKey, uObjectBindGroupIndex);
                }
                else // resuse free index
                {
                    ptRenderer->sbtObjectBindGroups[uObjectBindGroupIndex] = tNewBindGroup;
                    pl_hm_insert(&ptRenderer->tObjectBindGroupdHashMap, uHashKey, uObjectBindGroupIndex);
                }  
            }

            uint32_t uIterationObjectCount = pl_minu(uMaxObjectsPerBuffer, uObjectCount);
            for(uint32_t j = 0; j < uIterationObjectCount; j++)
            {
                plTransformComponent* ptTransform = &sbtComponents[uCurrentTransform];
                ptTransform->uBindGroup2 = uObjectBindGroupIndex;

                plObjectInfo* ptObjectInfo = (plObjectInfo*)(ptBuffer->tAllocation.pHostMapped + ptDynamicBufferNode->uDynamicBufferOffset);
                *ptObjectInfo = ptTransform->tInfo;
                ptObjectInfo->tModel = ptTransform->tFinalTransform;
                ptTransform->uBufferOffset = ptDynamicBufferNode->uDynamicBufferOffset;
                ptDynamicBufferNode->uDynamicBufferOffset += (uint32_t)szRangeSize;
                uCurrentTransform++;
            }
            uObjectCount = uObjectCount - uIterationObjectCount;

            pl_sb_push(ptDevice->_sbuDynamicBufferDeletionQueue, uDynamicBufferIndex);

            if(uObjectCount == 0)
                break;
        }
        break;
    }

    case PL_COMPONENT_TYPE_HIERARCHY:
    {
        // TODO: currently this assumes children come after their parents, fix this
        plHierarchyComponent* sbtComponents = ptManager->pData;

        for(uint32_t i = 0; i < pl_sb_size(sbtComponents); i++)
        {
            plHierarchyComponent* ptHierarchyComponent = &sbtComponents[i];
            plEntity tChildEntity = ptManager->sbtEntities[i];
            plTransformComponent* ptParentTransform = ptEcs->get_component(ptScene->ptTransformComponentManager, ptHierarchyComponent->tParent);
            plTransformComponent* ptChildTransform = ptEcs->get_component(ptScene->ptTransformComponentManager, tChildEntity);
            ptChildTransform->tFinalTransform = pl_mul_mat4(&ptParentTransform->tFinalTransform, &ptChildTransform->tWorld);
        }
        break;
    }
    }
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_proto_ext(plApiRegistryApiI* ptApiRegistry, bool bReload)
{

    if(bReload)
    {
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_PROTO), pl_load_proto_api());
    }
    else
    {
        ptApiRegistry->add(PL_API_PROTO, pl_load_proto_api());
    }
}

PL_EXPORT void
pl_unload_proto_ext(plApiRegistryApiI* ptApiRegistry)
{
    
}