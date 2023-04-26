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

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

// graphics
static void pl_create_main_render_target(plGraphics* ptGraphics, plRenderTarget* ptTargetOut);
static void pl_create_render_pass  (plGraphics* ptGraphics, const plRenderPassDesc* ptDesc, plRenderPass* ptPassOut);
static void pl_create_render_target(plResourceManager0ApiI* ptResourceApi, plGraphics* ptGraphics, const plRenderTargetDesc* ptDesc, plRenderTarget* ptTargetOut);
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
static plEntity pl_ecs_create_entity   (plRenderer* ptRenderer);
static size_t   pl_ecs_get_index       (plComponentManager* ptManager, plEntity tEntity);
static void*    pl_ecs_get_component   (plComponentManager* ptManager, plEntity tEntity);
static void*    pl_ecs_create_component(plComponentManager* ptManager, plEntity tEntity);
static bool     pl_ecs_has_entity      (plComponentManager* ptManager, plEntity tEntity);
static void     pl_ecs_update          (plScene* ptScene, plComponentManager* ptManager);

// components
static plEntity pl_ecs_create_mesh     (plScene* ptScene, const char* pcName);
static plEntity pl_ecs_create_material (plScene* ptScene, const char* pcName);
static plEntity pl_ecs_create_object   (plScene* ptScene, const char* pcName);
static plEntity pl_ecs_create_transform(plScene* ptScene, const char* pcName);
static plEntity pl_ecs_create_camera   (plScene* ptScene, const char* pcName, plVec3 tPos, float fYFov, float fAspect, float fNearZ, float fFarZ);

// hierarchy
static void     pl_ecs_attach_component  (plScene* ptScene, plEntity tEntity, plEntity tParent);
static void     pl_ecs_deattach_component(plScene* ptScene, plEntity tEntity);

// material
static void     pl_material_outline(plScene* ptScene, plEntity tEntity);

// camera
static void     pl_camera_set_fov        (plCameraComponent* ptCamera, float fYFov);
static void     pl_camera_set_clip_planes(plCameraComponent* ptCamera, float fNearZ, float fFarZ);
static void     pl_camera_set_aspect     (plCameraComponent* ptCamera, float fAspect);
static void     pl_camera_set_pos        (plCameraComponent* ptCamera, float fX, float fY, float fZ);
static void     pl_camera_set_pitch_yaw  (plCameraComponent* ptCamera, float fPitch, float fYaw);
static void     pl_camera_translate      (plCameraComponent* ptCamera, float fDx, float fDy, float fDz);
static void     pl_camera_rotate         (plCameraComponent* ptCamera, float fDPitch, float fDYaw);
static void     pl_camera_update         (plCameraComponent* ptCamera);

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
        .ecs_create_entity         = pl_ecs_create_entity,
        .ecs_get_index             = pl_ecs_get_index,
        .ecs_get_component         = pl_ecs_get_component,
        .ecs_create_component      = pl_ecs_create_component,
        .ecs_has_entity            = pl_ecs_has_entity,
        .ecs_update                = pl_ecs_update,
        .ecs_create_mesh           = pl_ecs_create_mesh,
        .ecs_create_material       = pl_ecs_create_material,
        .ecs_create_object         = pl_ecs_create_object,
        .ecs_create_transform      = pl_ecs_create_transform,
        .ecs_create_camera         = pl_ecs_create_camera,
        .ecs_attach_component      = pl_ecs_attach_component,
        .ecs_deattach_component    = pl_ecs_deattach_component,
        .material_outline          = pl_material_outline,
        .camera_set_fov            = pl_camera_set_fov,
        .camera_set_clip_planes    = pl_camera_set_clip_planes,
        .camera_set_aspect         = pl_camera_set_aspect,
        .camera_set_pos            = pl_camera_set_pos,
        .camera_set_pitch_yaw      = pl_camera_set_pitch_yaw,
        .camera_translate          = pl_camera_translate,
        .camera_rotate             = pl_camera_rotate,
        .camera_update             = pl_camera_update
    };
    return &tApi0;
}

//-----------------------------------------------------------------------------
// [SECTION] internal implementations
//-----------------------------------------------------------------------------

static float
pl__wrap_angle(float tTheta)
{
    static const float f2Pi = 2.0f * PL_PI;
    const float fMod = fmodf(tTheta, f2Pi);
    if (fMod > PL_PI)       return fMod - f2Pi;
    else if (fMod < -PL_PI) return fMod + f2Pi;
    return fMod;
}

static void
pl_create_render_target(plResourceManager0ApiI* ptResourceApi, plGraphics* ptGraphics, const plRenderTargetDesc* ptDesc, plRenderTarget* ptTargetOut)
{
    ptTargetOut->tDesc = *ptDesc;

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
        uint32_t uColorTexture = ptResourceApi->create_texture(&ptGraphics->tResourceManager, tColorTextureDesc, 0, NULL, "offscreen color texture");
        pl_sb_push(ptTargetOut->sbuColorTextureViews, ptResourceApi->create_texture_view(&ptGraphics->tResourceManager, &tColorView, &tColorSampler, uColorTexture, "offscreen color view"));
    }

    uint32_t uDepthTexture = ptResourceApi->create_texture(&ptGraphics->tResourceManager, tDepthTextureDesc, 0, NULL, "offscreen depth texture");

    const plTextureViewDesc tDepthView = {
        .tFormat     = tDepthTextureDesc.tFormat,
        .uLayerCount = tDepthTextureDesc.uLayers,
        .uMips       = tDepthTextureDesc.uMips
    };

    ptTargetOut->uDepthTextureView = ptResourceApi->create_texture_view(&ptGraphics->tResourceManager, &tDepthView, &tColorSampler, uDepthTexture, "offscreen depth view");

    plTextureView* ptDepthTextureView = &ptGraphics->tResourceManager.sbtTextureViews[ptTargetOut->uDepthTextureView];

    for(uint32_t i = 0; i < ptGraphics->tSwapchain.uImageCount; i++)
    {
        plTextureView* ptColorTextureView = &ptGraphics->tResourceManager.sbtTextureViews[ptTargetOut->sbuColorTextureViews[i]];
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

    // create render pass
    VkAttachmentDescription atAttachments[] = {

        // color attachment
        {
            .flags          = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT,
            .format         = ptDesc->tColorFormat,
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
            .format         = ptDesc->tDepthFormat,
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
    ptRenderer->tNextEntity = 1;
    
    ptRenderer->ptGfx = ptApiRegistry->first(PL_API_GRAPHICS);
    ptRenderer->ptMemoryApi = ptApiRegistry->first(PL_API_MEMORY);
    ptRenderer->ptResourceApi = ptApiRegistry->first(PL_API_RESOURCE_MANAGER_0);
    ptRenderer->ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    ptRenderer->ptProtoApi = ptApiRegistry->first(PL_API_PROTO);
    ptRenderer->ptImageApi = ptApiRegistry->first(PL_API_IMAGE);

	pl_set_log_context(ptRenderer->ptDataRegistry->get_data("log"));
    ptRenderer->uLogChannel = pl_add_log_channel("renderer", PL_CHANNEL_TYPE_CONSOLE | PL_CHANNEL_TYPE_BUFFER);

    plGraphicsApiI* ptGfx = ptRenderer->ptGfx;
    plResourceManager0ApiI* ptResourceApi = ptRenderer->ptResourceApi;

    // create dummy texture (texture slot 0 when not used)
    const plTextureDesc tTextureDesc2 = {
        .tDimensions = {.x = (float)1.0f, .y = (float)1.0f, .z = 1.0f},
        .tFormat     = VK_FORMAT_R8G8B8A8_UNORM,
        .tUsage      = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .uLayers     = 1,
        .uMips       = 1,
        .tType       = VK_IMAGE_TYPE_2D
    };
    static const float afSinglePixel[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    uint32_t uDummyTexture = ptResourceApi->create_texture(&ptGraphics->tResourceManager, tTextureDesc2, sizeof(unsigned char) * 4, afSinglePixel, "dummy texture");

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

    ptResourceApi->create_texture_view(&ptGraphics->tResourceManager, &tDummyView, &tDummySampler, uDummyTexture, "dummy texture view");

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

    ptRenderer->uMainShader    = ptGfx->create_shader(&ptGraphics->tResourceManager, &tMainShaderDesc);
    ptRenderer->uOutlineShader = ptGfx->create_shader(&ptGraphics->tResourceManager, &tOutlineShaderDesc);
    ptRenderer->uSkyboxShader  = ptGfx->create_shader(&ptGraphics->tResourceManager, &tSkyboxShaderDesc);

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
    plResourceManager* ptResourceManager = &ptGraphics->tResourceManager;
    plGraphicsApiI* ptGfx = ptRenderer->ptGfx;
    plMemoryApiI* ptMemoryApi = ptRenderer->ptMemoryApi;
    plResourceManager0ApiI* ptResourceApi = ptRenderer->ptResourceApi;
    plImageApiI* ptImageApi = ptRenderer->ptImageApi;

    memset(ptSceneOut, 0, sizeof(plScene));

    ptSceneOut->uGlobalStorageBuffer = UINT32_MAX;
    ptSceneOut->ptRenderer = ptRenderer;
    ptSceneOut->bMaterialsNeedUpdate = true;

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

    ptSceneOut->uDynamicBuffer0 = ptResourceApi->create_constant_buffer(ptResourceManager, ptRenderer->ptGraphics->tResourceManager._uDynamicBufferSize, "renderer dynamic buffer 0");

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

    unsigned char* rawBytes = ptMemoryApi->alloc(texWidth * texHeight * texNumChannels * 6);
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
        .tFormat     = VK_FORMAT_R8G8B8A8_UNORM,
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

    uint32_t uSkyboxTexture = ptResourceApi->create_texture(&ptGraphics->tResourceManager, tTextureDesc, sizeof(unsigned char) * texWidth * texHeight * texNumChannels * 6, rawBytes, "skybox texture");
    ptSceneOut->uSkyboxTextureView  = ptResourceApi->create_texture_view(&ptGraphics->tResourceManager, &tSkyboxView, &tSkyboxSampler, uSkyboxTexture, "skybox texture view");
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
            .uIndexBuffer  = ptResourceApi->create_index_buffer(&ptGraphics->tResourceManager, sizeof(uint32_t) * 36, acSkyboxIndices, "skybox index buffer"),
            .uVertexBuffer = ptResourceApi->create_vertex_buffer(&ptGraphics->tResourceManager, sizeof(float) * 24, sizeof(float), acSkyBoxVertices, "skybox vertex buffer"),
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
    plResourceManager* ptResourceManager = &ptGraphics->tResourceManager;
    plGraphicsApiI* ptGfx = ptRenderer->ptGfx;

    const plBuffer* ptBuffer0 = &ptResourceManager->sbtBuffers[ptScene->uDynamicBuffer0];
    const uint32_t uBufferFrameOffset0 = ((uint32_t)ptBuffer0->szSize / ptGraphics->uFramesInFlight) * (uint32_t)ptGraphics->szCurrentFrameIndex + ptScene->uDynamicBuffer0_Offset;

    const uint32_t uDrawOffset = pl_sb_size(ptRenderer->sbtDraws);
    const uint32_t uOutlineDrawOffset = pl_sb_size(ptRenderer->sbtOutlineDraws);

    // record draws
    for(uint32_t i = 0; i < pl_sb_size(ptRenderer->sbtObjectEntities); i++)
    {
        plObjectComponent* ptObjectComponent = pl_ecs_get_component(&ptScene->tComponentLibrary.tObjectComponentManager, ptRenderer->sbtObjectEntities[i]);
        plMeshComponent* ptMeshComponent = pl_ecs_get_component(&ptScene->tComponentLibrary.tMeshComponentManager, ptObjectComponent->tMesh);
        plTransformComponent* ptTransformComponent = pl_ecs_get_component(&ptScene->tComponentLibrary.tTransformComponentManager, ptObjectComponent->tTransform);
        for(uint32_t j = 0; j < pl_sb_size(ptMeshComponent->sbtSubmeshes); j++)
        {
            plSubMesh* ptSubmesh = &ptMeshComponent->sbtSubmeshes[j];
            plMaterialComponent* ptMaterial = pl_ecs_get_component(&ptScene->tComponentLibrary.tMaterialComponentManager, ptSubmesh->tMaterial);

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
                plMaterialComponent* ptOutlineMaterial = pl_ecs_get_component(&ptScene->tComponentLibrary.tOutlineMaterialComponentManager, ptSubmesh->tMaterial);

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
    plResourceManager* ptResourceManager = &ptGraphics->tResourceManager;
    plGraphicsApiI* ptGfx = ptRenderer->ptGfx;
    plResourceManager0ApiI* ptResourceApi = ptRenderer->ptResourceApi;

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


            ptSubMesh->tMesh.uIndexBuffer = ptResourceApi->create_index_buffer(ptResourceManager, 
                sizeof(uint32_t) * ptSubMesh->tMesh.uIndexCount,
                ptSubMesh->sbuIndices, "unnamed index buffer");

            ptSubMesh->tMesh.uVertexBuffer = ptResourceApi->create_vertex_buffer(ptResourceManager, 
                ptSubMesh->tMesh.uVertexCount * sizeof(plVec3), sizeof(plVec3),
                ptSubMesh->sbtVertexPositions, "unnamed vertex buffer");

            plMaterialComponent* ptMaterialComponent = pl_ecs_get_component(ptScene->ptMaterialComponentManager, ptSubMesh->tMaterial);
            ptMaterialComponent->tGraphicsState.ulVertexStreamMask = ptSubMesh->tMesh.ulVertexStreamMask;

            uStorageOffset += uStride * ptSubMesh->tMesh.uVertexCount;
        }
    }

    if(pl_sb_size(ptScene->sbfStorageBuffer) > 0)
    {
        if(ptScene->uGlobalStorageBuffer != UINT32_MAX)
        {
            ptResourceApi->submit_buffer_for_deletion(ptResourceManager, ptScene->uGlobalStorageBuffer);
        }
        ptScene->uGlobalStorageBuffer = ptResourceApi->create_storage_buffer(ptResourceManager, pl_sb_size(ptScene->sbfStorageBuffer) * sizeof(float), ptScene->sbfStorageBuffer, "global storage");
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

    const plBuffer* ptBuffer0 = &ptGraphics->tResourceManager.sbtBuffers[ptScene->uDynamicBuffer0];
    const uint32_t uBufferFrameOffset0 = ((uint32_t)ptBuffer0->szSize / ptGraphics->uFramesInFlight) * (uint32_t)ptGraphics->szCurrentFrameIndex + ptScene->uDynamicBuffer0_Offset;

    plGlobalInfo* ptGlobalInfo    = (plGlobalInfo*)&ptBuffer0->pucMapping[uBufferFrameOffset0];
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
    plResourceManager* ptResourceManager = &ptGraphics->tResourceManager;
    plGraphicsApiI* ptGfx = ptRenderer->ptGfx;
    VkSampleCountFlagBits tMSAASampleCount = ptScene->ptRenderTarget->bMSAA ? ptGraphics->tSwapchain.tMsaaSamples : VK_SAMPLE_COUNT_1_BIT;

    const plBuffer* ptBuffer0 = &ptResourceManager->sbtBuffers[ptScene->uDynamicBuffer0];
    const uint32_t uBufferFrameOffset0 = ((uint32_t)ptBuffer0->szSize / ptGraphics->uFramesInFlight) * (uint32_t)ptGraphics->szCurrentFrameIndex + ptScene->uDynamicBuffer0_Offset;

    plGlobalInfo* ptGlobalInfo    = (plGlobalInfo*)&ptBuffer0->pucMapping[uBufferFrameOffset0];
    ptGlobalInfo->tCameraPos      = (plVec4){.xyz = ptScene->ptCamera->tPos, .w = 0.0f};
    const plMat4 tRemoveTranslation = pl_mat4_translate_xyz(ptScene->ptCamera->tPos.x, ptScene->ptCamera->tPos.y, ptScene->ptCamera->tPos.z);
    ptGlobalInfo->tCameraView     = pl_mul_mat4(&ptScene->ptCamera->tViewMat, &tRemoveTranslation);
    ptGlobalInfo->tCameraViewProj = pl_mul_mat4(&ptScene->ptCamera->tProjMat, &ptGlobalInfo->tCameraView);

    uint32_t uSkyboxShaderVariant = UINT32_MAX;

    const plShader* ptShader = &ptResourceManager->sbtShaders[ptRenderer->uSkyboxShader];   
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
        uSkyboxShaderVariant = ptGfx->add_shader_variant(ptResourceManager, ptRenderer->uSkyboxShader, tFillStateTemplate, ptScene->ptRenderTarget->tDesc.tRenderPass._tRenderPass, tMSAASampleCount);
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

static plEntity
pl_ecs_create_entity(plRenderer* ptRenderer)
{
    plEntity tNewEntity = ptRenderer->tNextEntity++;
    return tNewEntity;
}

static size_t
pl_ecs_get_index(plComponentManager* ptManager, plEntity tEntity)
{ 
    PL_ASSERT(tEntity != PL_INVALID_ENTITY_HANDLE);
    bool bFound = false;
    size_t szIndex = 0;
    for(uint32_t i = 0; i < pl_sb_size(ptManager->sbtEntities); i++)
    {
        if(ptManager->sbtEntities[i] == tEntity)
        {
            szIndex = (size_t)i;
            bFound = true;
            break;
        }
    }
    PL_ASSERT(bFound);
    return szIndex;
}

static void*
pl_ecs_get_component(plComponentManager* ptManager, plEntity tEntity)
{
    PL_ASSERT(tEntity != PL_INVALID_ENTITY_HANDLE);
    size_t szIndex = pl_ecs_get_index(ptManager, tEntity);
    unsigned char* pucData = ptManager->pData;
    return &pucData[szIndex * ptManager->szStride];
}

static void*
pl_ecs_create_component(plComponentManager* ptManager, plEntity tEntity)
{
    PL_ASSERT(tEntity != PL_INVALID_ENTITY_HANDLE);

    switch (ptManager->tComponentType)
    {
    case PL_COMPONENT_TYPE_TAG:
    {
        plTagComponent* sbComponents = ptManager->pData;
        pl_sb_push(sbComponents, (plTagComponent){0});
        ptManager->pData = sbComponents;
        pl_sb_push(ptManager->sbtEntities, tEntity);
        return &pl_sb_back(sbComponents);
    }

    case PL_COMPONENT_TYPE_MESH:
    {
        plMeshComponent* sbComponents = ptManager->pData;
        pl_sb_push(sbComponents, (plMeshComponent){0});
        ptManager->pData = sbComponents;
        pl_sb_push(ptManager->sbtEntities, tEntity);
        return &pl_sb_back(sbComponents);
    }

    case PL_COMPONENT_TYPE_TRANSFORM:
    {
        plTransformComponent* sbComponents = ptManager->pData;
        pl_sb_push(sbComponents, ((plTransformComponent){.tWorld = pl_identity_mat4(), .tFinalTransform = pl_identity_mat4()}));
        ptManager->pData = sbComponents;
        pl_sb_push(ptManager->sbtEntities, tEntity);
        return &pl_sb_back(sbComponents);
    }

    case PL_COMPONENT_TYPE_MATERIAL:
    {
        plMaterialComponent* sbComponents = ptManager->pData;
        pl_sb_push(sbComponents, (plMaterialComponent){0});
        ptManager->pData = sbComponents;
        pl_sb_push(ptManager->sbtEntities, tEntity);
        return &pl_sb_back(sbComponents);
    }

    case PL_COMPONENT_TYPE_OBJECT:
    {
        plObjectComponent* sbComponents = ptManager->pData;
        pl_sb_push(sbComponents, (plObjectComponent){0});
        ptManager->pData = sbComponents;
        pl_sb_push(ptManager->sbtEntities, tEntity);
        return &pl_sb_back(sbComponents);
    }

    case PL_COMPONENT_TYPE_CAMERA:
    {
        plCameraComponent* sbComponents = ptManager->pData;
        pl_sb_push(sbComponents, (plCameraComponent){0});
        ptManager->pData = sbComponents;
        pl_sb_push(ptManager->sbtEntities, tEntity);
        return &pl_sb_back(sbComponents);
    }

    case PL_COMPONENT_TYPE_HIERARCHY:
    {
        plHierarchyComponent* sbComponents = ptManager->pData;
        pl_sb_push(sbComponents, (plHierarchyComponent){0});
        ptManager->pData = sbComponents;
        pl_sb_push(ptManager->sbtEntities, tEntity);
        return &pl_sb_back(sbComponents);
    }
    }

    return NULL;
}

static bool
pl_ecs_has_entity(plComponentManager* ptManager, plEntity tEntity)
{
    PL_ASSERT(tEntity != PL_INVALID_ENTITY_HANDLE);

    for(uint32_t i = 0; i < pl_sb_size(ptManager->sbtEntities); i++)
    {
        if(ptManager->sbtEntities[i] == tEntity)
            return true;
    }
    return false;
}

static void
pl_ecs_update(plScene* ptScene, plComponentManager* ptManager)
{

    plRenderer* ptRenderer = ptScene->ptRenderer;
    plGraphics* ptGraphics = ptRenderer->ptGraphics;
    plResourceManager* ptResourceManager = &ptGraphics->tResourceManager;
    plGraphicsApiI* ptGfx = ptRenderer->ptGfx;
    plResourceManager0ApiI* ptResourceApi = ptRenderer->ptResourceApi;

    switch (ptManager->tComponentType)
    {
    case PL_COMPONENT_TYPE_MATERIAL:
    {

        VkSampleCountFlagBits tMSAASampleCount = ptScene->ptRenderTarget->bMSAA ? ptGraphics->tSwapchain.tMsaaSamples : VK_SAMPLE_COUNT_1_BIT;
        uint32_t* sbuTextures = NULL;

        size_t szRangeSize = pl_align_up(sizeof(plMaterialInfo), ptGraphics->tDevice.tDeviceProps.limits.minUniformBufferOffsetAlignment);
        size_t szRangeSize2 = sizeof(plMaterialInfo);
        plMaterialComponent* sbtComponents = ptManager->pData;

        const uint32_t uMaxMaterialsPerBuffer = (uint32_t)(ptResourceManager->_uDynamicBufferSize / (uint32_t)szRangeSize) - 1;
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
                uDynamicBufferIndex = ptResourceApi->request_dynamic_buffer(ptResourceManager);
                ptDynamicBufferNode = &ptResourceManager->_sbtDynamicBufferList[uDynamicBufferIndex];
                ptBuffer = &ptResourceManager->sbtBuffers[ptDynamicBufferNode->uDynamicBuffer];
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
                            .tLayout = *ptGfx->get_bind_group_layout(ptResourceManager, ptMaterial->uShader, 1)
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

                    plMaterialInfo* ptMaterialInfo = (plMaterialInfo*)(ptBuffer->pucMapping + ptDynamicBufferNode->uDynamicBufferOffset);
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

                const plShader* ptShader = &ptResourceManager->sbtShaders[ptMaterial->uShader];   
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
                    ptMaterial->uShaderVariant = ptGfx->add_shader_variant(ptResourceManager, ptMaterial->uShader, ptMaterial->tGraphicsState, ptScene->ptRenderTarget->tDesc.tRenderPass._tRenderPass, tMSAASampleCount);
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

        const uint32_t uMaxObjectsPerBuffer = (uint32_t)(ptResourceManager->_uDynamicBufferSize / (uint32_t)szRangeSize) - 1;
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
            const uint32_t uDynamicBufferIndex = ptResourceApi->request_dynamic_buffer(ptResourceManager);
            plDynamicBufferNode* ptDynamicBufferNode = &ptResourceManager->_sbtDynamicBufferList[uDynamicBufferIndex];
            plBuffer* ptBuffer = &ptResourceManager->sbtBuffers[ptDynamicBufferNode->uDynamicBuffer];

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

                plObjectInfo* ptObjectInfo = (plObjectInfo*)(ptBuffer->pucMapping + ptDynamicBufferNode->uDynamicBufferOffset);
                *ptObjectInfo = ptTransform->tInfo;
                ptObjectInfo->tModel = ptTransform->tFinalTransform;
                ptTransform->uBufferOffset = ptDynamicBufferNode->uDynamicBufferOffset;
                ptDynamicBufferNode->uDynamicBufferOffset += (uint32_t)szRangeSize;
                uCurrentTransform++;
            }
            uObjectCount = uObjectCount - uIterationObjectCount;

            pl_sb_push(ptResourceManager->_sbuDynamicBufferDeletionQueue, uDynamicBufferIndex);

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
            plTransformComponent* ptParentTransform = pl_ecs_get_component(ptScene->ptTransformComponentManager, ptHierarchyComponent->tParent);
            plTransformComponent* ptChildTransform = pl_ecs_get_component(ptScene->ptTransformComponentManager, tChildEntity);
            ptChildTransform->tFinalTransform = pl_mul_mat4(&ptParentTransform->tFinalTransform, &ptChildTransform->tWorld);
        }
        break;
    }
    }
}

static plEntity
pl_ecs_create_mesh(plScene* ptScene, const char* pcName)
{
    plRenderer* ptRenderer = ptScene->ptRenderer;
    plEntity tNewEntity = pl_ecs_create_entity(ptRenderer);

    plTagComponent* ptTag = pl_ecs_create_component(&ptScene->tComponentLibrary.tTagComponentManager, tNewEntity);
    if(pcName)
    {
        pl_log_debug_to_f(ptRenderer->uLogChannel, "created mesh: %s", pcName);
        strncpy(ptTag->acName, pcName, PL_MAX_NAME_LENGTH);
    }
    else
    {
        pl_log_debug_to_f(ptRenderer->uLogChannel, "created unnamed mesh");
        strncpy(ptTag->acName, "unnamed", PL_MAX_NAME_LENGTH);
    }

    plMeshComponent* ptMesh = pl_ecs_create_component(&ptScene->tComponentLibrary.tMeshComponentManager, tNewEntity);
    return tNewEntity;
}

static plMaterialComponent*
pl_ecs_create_outline_material(plScene* ptScene, plEntity tEntity)
{
    plRenderer* ptRenderer = ptScene->ptRenderer;

    plMaterialComponent* ptMaterial = pl_ecs_create_component(&ptScene->tComponentLibrary.tOutlineMaterialComponentManager, tEntity);
    memset(ptMaterial, 0, sizeof(plMaterialComponent));
    ptMaterial->uShader                             = UINT32_MAX;
    ptMaterial->tAlbedo                             = (plVec4){ 1.0f, 1.0f, 1.0f, 1.0f };
    ptMaterial->fAlphaCutoff                        = 0.1f;
    ptMaterial->bDoubleSided                        = false;
    ptMaterial->tShaderType                         = PL_SHADER_TYPE_UNLIT;
    ptMaterial->tGraphicsState.ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_HAS_NORMAL;
    ptMaterial->tGraphicsState.ulDepthMode          = PL_DEPTH_MODE_ALWAYS;
    ptMaterial->tGraphicsState.ulDepthWriteEnabled  = false;
    ptMaterial->tGraphicsState.ulCullMode           = VK_CULL_MODE_FRONT_BIT;
    ptMaterial->tGraphicsState.ulBlendMode          = PL_BLEND_MODE_ALPHA;
    ptMaterial->tGraphicsState.ulShaderTextureFlags = 0;
    ptMaterial->tGraphicsState.ulStencilMode        = PL_STENCIL_MODE_NOT_EQUAL;
    ptMaterial->tGraphicsState.ulStencilRef         = 0xff;
    ptMaterial->tGraphicsState.ulStencilMask        = 0xff;
    ptMaterial->tGraphicsState.ulStencilOpFail      = VK_STENCIL_OP_KEEP;
    ptMaterial->tGraphicsState.ulStencilOpDepthFail = VK_STENCIL_OP_KEEP;
    ptMaterial->tGraphicsState.ulStencilOpPass      = VK_STENCIL_OP_REPLACE;

    return ptMaterial;
}

static plEntity
pl_ecs_create_material(plScene* ptScene, const char* pcName)
{
    plRenderer* ptRenderer = ptScene->ptRenderer;
    plEntity tNewEntity = pl_ecs_create_entity(ptRenderer);

    plTagComponent* ptTag = pl_ecs_create_component(&ptScene->tComponentLibrary.tTagComponentManager, tNewEntity);
    if(pcName)
    {
        pl_log_debug_to_f(ptRenderer->uLogChannel, "created material: %s", pcName);
        strncpy(ptTag->acName, pcName, PL_MAX_NAME_LENGTH);
    }
    else
    {
        pl_log_debug_to_f(ptRenderer->uLogChannel, "created unnamed material");
        strncpy(ptTag->acName, "unnamed", PL_MAX_NAME_LENGTH);
    }

    plMaterialComponent* ptMaterial = pl_ecs_create_component(&ptScene->tComponentLibrary.tMaterialComponentManager, tNewEntity);
    memset(ptMaterial, 0, sizeof(plMaterialComponent));
    ptMaterial->uShader = UINT32_MAX;
    ptMaterial->tAlbedo = (plVec4){ 1.0f, 1.0f, 1.0f, 1.0f };
    ptMaterial->fAlphaCutoff = 0.1f;
    ptMaterial->bDoubleSided = false;
    ptMaterial->tShaderType = PL_SHADER_TYPE_PBR;
    ptMaterial->tGraphicsState.ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_HAS_NORMAL;
    ptMaterial->tGraphicsState.ulDepthMode          = PL_DEPTH_MODE_LESS_OR_EQUAL;
    ptMaterial->tGraphicsState.ulDepthWriteEnabled  = true;
    ptMaterial->tGraphicsState.ulCullMode           = VK_CULL_MODE_NONE;
    ptMaterial->tGraphicsState.ulBlendMode          = PL_BLEND_MODE_ALPHA;
    ptMaterial->tGraphicsState.ulShaderTextureFlags = 0;
    ptMaterial->tGraphicsState.ulStencilMode        = PL_STENCIL_MODE_ALWAYS;
    ptMaterial->tGraphicsState.ulStencilRef         = 0xff;
    ptMaterial->tGraphicsState.ulStencilMask        = 0xff;
    ptMaterial->tGraphicsState.ulStencilOpFail      = VK_STENCIL_OP_KEEP;
    ptMaterial->tGraphicsState.ulStencilOpDepthFail = VK_STENCIL_OP_KEEP;
    ptMaterial->tGraphicsState.ulStencilOpPass      = VK_STENCIL_OP_KEEP;

    return tNewEntity;    
}

static plEntity
pl_ecs_create_object(plScene* ptScene, const char* pcName)
{
    plRenderer* ptRenderer = ptScene->ptRenderer;
    plEntity tNewEntity = pl_ecs_create_entity(ptRenderer);

    plTagComponent* ptTag = pl_ecs_create_component(&ptScene->tComponentLibrary.tTagComponentManager, tNewEntity);
    if(pcName)
    {
        pl_log_debug_to_f(ptRenderer->uLogChannel, "created object: %s", pcName);
        strncpy(ptTag->acName, pcName, PL_MAX_NAME_LENGTH);
    }
    else
    {
        pl_log_debug_to_f(ptRenderer->uLogChannel, "created unnamed object");
        strncpy(ptTag->acName, "unnamed", PL_MAX_NAME_LENGTH);
    }

    plObjectComponent* ptObject = pl_ecs_create_component(&ptScene->tComponentLibrary.tObjectComponentManager, tNewEntity);
    memset(ptObject, 0, sizeof(plObjectComponent));

    plTransformComponent* ptTransform = pl_ecs_create_component(&ptScene->tComponentLibrary.tTransformComponentManager, tNewEntity);
    memset(ptTransform, 0, sizeof(plTransformComponent));
    ptTransform->tInfo.tModel = pl_identity_mat4();
    ptTransform->tWorld = pl_identity_mat4();

    plMeshComponent* ptMesh = pl_ecs_create_component(&ptScene->tComponentLibrary.tMeshComponentManager, tNewEntity);
    memset(ptMesh, 0, sizeof(plMeshComponent));

    ptObject->tTransform = tNewEntity;
    ptObject->tMesh = tNewEntity;

    return tNewEntity;    
}

static plEntity
pl_ecs_create_transform(plScene* ptScene, const char* pcName)
{
    plRenderer* ptRenderer = ptScene->ptRenderer;
    plEntity tNewEntity = pl_ecs_create_entity(ptRenderer);

    plTagComponent* ptTag = pl_ecs_create_component(&ptScene->tComponentLibrary.tTagComponentManager, tNewEntity);
    if(pcName)
    {
        pl_log_debug_to_f(ptRenderer->uLogChannel, "created transform: %s", pcName);
        strncpy(ptTag->acName, pcName, PL_MAX_NAME_LENGTH);
    }
    else
    {
        pl_log_debug_to_f(ptRenderer->uLogChannel, "created unnamed transform");
        strncpy(ptTag->acName, "unnamed", PL_MAX_NAME_LENGTH);
    }

    plTransformComponent* ptTransform = pl_ecs_create_component(&ptScene->tComponentLibrary.tTransformComponentManager, tNewEntity);
    memset(ptTransform, 0, sizeof(plTransformComponent));
    ptTransform->tInfo.tModel = pl_identity_mat4();
    ptTransform->tWorld = pl_identity_mat4();

    return tNewEntity;  
}

static plEntity
pl_ecs_create_camera(plScene* ptScene, const char* pcName, plVec3 tPos, float fYFov, float fAspect, float fNearZ, float fFarZ)
{
    plRenderer* ptRenderer = ptScene->ptRenderer;
    plEntity tNewEntity = pl_ecs_create_entity(ptRenderer);

    plTagComponent* ptTag = pl_ecs_create_component(&ptScene->tComponentLibrary.tTagComponentManager, tNewEntity);
    if(pcName)
    {
        pl_log_debug_to_f(ptRenderer->uLogChannel, "created camera: %s", pcName);
        strncpy(ptTag->acName, pcName, PL_MAX_NAME_LENGTH);
    }
    else
    {
        pl_log_debug_to_f(ptRenderer->uLogChannel, "created unnamed camera");
        strncpy(ptTag->acName, "unnamed", PL_MAX_NAME_LENGTH);
    }

    const plCameraComponent tCamera = {
        .tPos         = tPos,
        .fNearZ       = fNearZ,
        .fFarZ        = fFarZ,
        .fFieldOfView = fYFov,
        .fAspectRatio = fAspect
    };

    plCameraComponent* ptCamera = pl_ecs_create_component(&ptScene->tComponentLibrary.tCameraComponentManager, tNewEntity);
    memset(ptCamera, 0, sizeof(plCameraComponent));
    *ptCamera = tCamera;
    pl_camera_update(ptCamera);

    return tNewEntity; 
}

static void
pl_ecs_attach_component(plScene* ptScene, plEntity tEntity, plEntity tParent)
{
    plHierarchyComponent* ptHierarchyComponent = NULL;

    // check if entity already has a hierarchy component
    if(pl_ecs_has_entity(ptScene->ptHierarchyComponentManager, tEntity))
    {
        ptHierarchyComponent = pl_ecs_get_component(ptScene->ptHierarchyComponentManager, tEntity);

    }
    else
    {
        ptHierarchyComponent = pl_ecs_create_component(ptScene->ptHierarchyComponentManager, tEntity);
    }
    ptHierarchyComponent->tParent = tParent;
}

static void
pl_ecs_deattach_component(plScene* ptScene, plEntity tEntity)
{
    plHierarchyComponent* ptHierarchyComponent = NULL;

    // check if entity already has a hierarchy component
    if(pl_ecs_has_entity(ptScene->ptHierarchyComponentManager, tEntity))
    {
        ptHierarchyComponent = pl_ecs_get_component(ptScene->ptHierarchyComponentManager, tEntity);

    }
    else
    {
        ptHierarchyComponent = pl_ecs_create_component(ptScene->ptHierarchyComponentManager, tEntity);
    }
    ptHierarchyComponent->tParent = PL_INVALID_ENTITY_HANDLE;
}

static void
pl_material_outline(plScene* ptScene, plEntity tEntity)
{
    plMaterialComponent* ptMaterial = pl_ecs_get_component(&ptScene->tComponentLibrary.tMaterialComponentManager, tEntity);
    ptMaterial->tGraphicsState.ulStencilOpFail      = VK_STENCIL_OP_REPLACE;
    ptMaterial->tGraphicsState.ulStencilOpDepthFail = VK_STENCIL_OP_REPLACE;
    ptMaterial->tGraphicsState.ulStencilOpPass      = VK_STENCIL_OP_REPLACE;
    ptMaterial->bOutline                            = true;

    plMaterialComponent* ptOutlineMaterial = pl_ecs_create_outline_material(ptScene, tEntity);
    ptOutlineMaterial->tGraphicsState.ulVertexStreamMask   = ptMaterial->tGraphicsState.ulVertexStreamMask;

}

static void
pl_camera_set_fov(plCameraComponent* ptCamera, float fYFov)
{
    ptCamera->fFieldOfView = fYFov;
}

static void
pl_camera_set_clip_planes(plCameraComponent* ptCamera, float fNearZ, float fFarZ)
{
    ptCamera->fNearZ = fNearZ;
    ptCamera->fFarZ = fFarZ;
}

static void
pl_camera_set_aspect(plCameraComponent* ptCamera, float fAspect)
{
    ptCamera->fAspectRatio = fAspect;
}

static void
pl_camera_set_pos(plCameraComponent* ptCamera, float fX, float fY, float fZ)
{
    ptCamera->tPos.x = fX;
    ptCamera->tPos.y = fY;
    ptCamera->tPos.z = fZ;
}

static void
pl_camera_set_pitch_yaw(plCameraComponent* ptCamera, float fPitch, float fYaw)
{
    ptCamera->fPitch = fPitch;
    ptCamera->fYaw = fYaw;
}

static void
pl_camera_translate(plCameraComponent* ptCamera, float fDx, float fDy, float fDz)
{
    ptCamera->tPos = pl_add_vec3(ptCamera->tPos, pl_mul_vec3_scalarf(ptCamera->_tRightVec, fDx));
    ptCamera->tPos = pl_add_vec3(ptCamera->tPos, pl_mul_vec3_scalarf(ptCamera->_tForwardVec, fDz));
    ptCamera->tPos.y += fDy;
}

static void
pl_camera_rotate(plCameraComponent* ptCamera, float fDPitch, float fDYaw)
{
    ptCamera->fPitch += fDPitch;
    ptCamera->fYaw += fDYaw;

    ptCamera->fYaw = pl__wrap_angle(ptCamera->fYaw);
    ptCamera->fPitch = pl_clampf(0.995f * -PL_PI_2, ptCamera->fPitch, 0.995f * PL_PI_2);
}

static void
pl_camera_update(plCameraComponent* ptCamera)
{
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~update view~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // world space
    static const plVec4 tOriginalUpVec      = {0.0f, 1.0f, 0.0f, 0.0f};
    static const plVec4 tOriginalForwardVec = {0.0f, 0.0f, 1.0f, 0.0f};
    static const plVec4 tOriginalRightVec   = {-1.0f, 0.0f, 0.0f, 0.0f};

    const plMat4 tXRotMat   = pl_mat4_rotate_vec3(ptCamera->fPitch, tOriginalRightVec.xyz);
    const plMat4 tYRotMat   = pl_mat4_rotate_vec3(ptCamera->fYaw, tOriginalUpVec.xyz);
    const plMat4 tZRotMat   = pl_mat4_rotate_vec3(ptCamera->fRoll, tOriginalForwardVec.xyz);
    const plMat4 tTranslate = pl_mat4_translate_vec3((plVec3){ptCamera->tPos.x, ptCamera->tPos.y, ptCamera->tPos.z});

    // rotations: rotY * rotX * rotZ
    plMat4 tRotations = pl_mul_mat4t(&tXRotMat, &tZRotMat);
    tRotations        = pl_mul_mat4t(&tYRotMat, &tRotations);

    // update camera vectors
    ptCamera->_tRightVec   = pl_norm_vec4(pl_mul_mat4_vec4(&tRotations, tOriginalRightVec)).xyz;
    ptCamera->_tUpVec      = pl_norm_vec4(pl_mul_mat4_vec4(&tRotations, tOriginalUpVec)).xyz;
    ptCamera->_tForwardVec = pl_norm_vec4(pl_mul_mat4_vec4(&tRotations, tOriginalForwardVec)).xyz;

    // update camera transform: translate * rotate
    ptCamera->tTransformMat = pl_mul_mat4t(&tTranslate, &tRotations);

    // update camera view matrix
    ptCamera->tViewMat   = pl_mat4t_invert(&ptCamera->tTransformMat);

    // flip x & y so camera looks down +z and remains right handed (+x to the right)
    const plMat4 tFlipXY = pl_mat4_scale_xyz(-1.0f, -1.0f, 1.0f);
    ptCamera->tViewMat   = pl_mul_mat4t(&tFlipXY, &ptCamera->tViewMat);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~update projection~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    const float fInvtanHalfFovy = 1.0f / tanf(ptCamera->fFieldOfView / 2.0f);
    ptCamera->tProjMat.col[0].x = fInvtanHalfFovy / ptCamera->fAspectRatio;
    ptCamera->tProjMat.col[1].y = fInvtanHalfFovy;
    ptCamera->tProjMat.col[2].z = ptCamera->fFarZ / (ptCamera->fFarZ - ptCamera->fNearZ);
    ptCamera->tProjMat.col[2].w = 1.0f;
    ptCamera->tProjMat.col[3].z = -ptCamera->fNearZ * ptCamera->fFarZ / (ptCamera->fFarZ - ptCamera->fNearZ);
    ptCamera->tProjMat.col[3].w = 0.0f;     
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