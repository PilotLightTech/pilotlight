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
#include "pl_ui.h"
#include "pl_draw.h"
#include "pl_ds.h"
#include "pl_io.h"
#include "pl_memory.h"
#include "pl_log.h"

//-----------------------------------------------------------------------------
// [SECTION] implementations
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

void
pl_create_render_target(plGraphics* ptGraphics, const plRenderTargetDesc* ptDesc, plRenderTarget* ptTargetOut)
{
    ptTargetOut->tDesc = *ptDesc;

    const plTextureDesc tColorTextureDesc = {
        .tDimensions = {.x = ptDesc->tSize.x, .y = ptDesc->tSize.y, .z = 1.0f},
        .tFormat     = ptDesc->tRenderPass.tDesc.tColorFormat,
        .tUsage      = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .uLayers     = 1,
        .uMips       = 1,
        .tType       = VK_IMAGE_TYPE_2D,
        .tViewType   = VK_IMAGE_VIEW_TYPE_2D
    };

    const plTextureDesc tDepthTextureDesc = {
        .tDimensions = {.x = ptDesc->tSize.x, .y = ptDesc->tSize.y, .z = 1.0f},
        .tFormat     = ptDesc->tRenderPass.tDesc.tDepthFormat,
        .tUsage      = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .uLayers     = 1,
        .uMips       = 1,
        .tType       = VK_IMAGE_TYPE_2D,
        .tViewType   = VK_IMAGE_VIEW_TYPE_2D
    };

    for(uint32_t i = 0; i < ptGraphics->tSwapchain.uImageCount; i++)
    {
        pl_sb_push(ptTargetOut->sbuColorTextures, pl_create_texture(&ptGraphics->tResourceManager, tColorTextureDesc, 0, NULL, "offscreen color"));
    }
    ptTargetOut->uDepthTexture = pl_create_texture(&ptGraphics->tResourceManager, tDepthTextureDesc, 0, NULL, "offscreen depth");

    plTexture* ptDepthTexture = &ptGraphics->tResourceManager.sbtTextures[ptTargetOut->uDepthTexture];

    for(uint32_t i = 0; i < ptGraphics->tSwapchain.uImageCount; i++)
    {
        plTexture* ptColorTexture = &ptGraphics->tResourceManager.sbtTextures[ptTargetOut->sbuColorTextures[i]];
        VkImageView atAttachments[] = {
            ptColorTexture->tImageView,
            ptDepthTexture->tImageView
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

void
pl_create_main_render_target(plGraphics* ptGraphics, plRenderTarget* ptTargetOut)
{
    plIOContext* ptIOCtx = pl_get_io_context();
    ptTargetOut->bMSAA = true;
    ptTargetOut->sbtFrameBuffers = ptGraphics->tSwapchain.ptFrameBuffers;
    ptTargetOut->tDesc.tRenderPass._tRenderPass = ptGraphics->tRenderPass;
    ptTargetOut->tDesc.tRenderPass.tDesc.tColorFormat = ptGraphics->tSwapchain.tFormat;
    ptTargetOut->tDesc.tRenderPass.tDesc.tDepthFormat = ptGraphics->tSwapchain.tDepthFormat;
    ptTargetOut->tDesc.tSize.x = ptIOCtx->afMainViewportSize[0];
    ptTargetOut->tDesc.tSize.y = ptIOCtx->afMainViewportSize[1];
}

void
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

        {
            .srcSubpass      = VK_SUBPASS_EXTERNAL,
            .dstSubpass      = 0,
            .srcStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask   = VK_ACCESS_SHADER_READ_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
        },
        {
            .srcSubpass      = 0,
            .dstSubpass      = VK_SUBPASS_EXTERNAL,
            .srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstAccessMask   = VK_ACCESS_SHADER_READ_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
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

void
pl_begin_render_target(plGraphics* ptGraphics, plRenderTarget* ptTarget)
{
    const plFrameContext* ptCurrentFrame = pl_get_frame_resources(ptGraphics);

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

void
pl_end_render_target(plGraphics* ptGraphics)
{
    const plFrameContext* ptCurrentFrame = pl_get_frame_resources(ptGraphics);

    vkCmdEndRenderPass(ptCurrentFrame->tCmdBuf);
}

void
pl_cleanup_render_target(plGraphics* ptGraphics, plRenderTarget* ptTarget)
{
    for (uint32_t i = 0u; i < pl_sb_size(ptTarget->sbtFrameBuffers); i++)
    {
        vkDestroyFramebuffer(ptGraphics->tDevice.tLogicalDevice, ptTarget->sbtFrameBuffers[i], NULL);
    }
}

void
pl_cleanup_render_pass(plGraphics* ptGraphics, plRenderPass* ptPass)
{
    vkDestroyRenderPass(ptGraphics->tDevice.tLogicalDevice, ptPass->_tRenderPass, NULL);
}

void
pl_setup_renderer(plGraphics* ptGraphics, plRenderer* ptRenderer)
{
    memset(ptRenderer, 0, sizeof(plRenderer));
    ptRenderer->tNextEntity = 1;
    ptRenderer->uLogChannel = pl_add_log_channel("renderer", PL_CHANNEL_TYPE_CONSOLE | PL_CHANNEL_TYPE_BUFFER);

    // create dummy texture (texture slot 0 when not used)
    const plTextureDesc tTextureDesc2 = {
        .tDimensions = {.x = (float)1.0f, .y = (float)1.0f, .z = 1.0f},
        .tFormat     = VK_FORMAT_R8G8B8A8_UNORM,
        .tUsage      = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .uLayers     = 1,
        .uMips       = 1,
        .tType       = VK_IMAGE_TYPE_2D,
        .tViewType   = VK_IMAGE_VIEW_TYPE_2D
    };
    static const float afSinglePixel[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    pl_create_texture(&ptGraphics->tResourceManager, tTextureDesc2, sizeof(unsigned char) * 4, afSinglePixel, "dummy texture");

    ptRenderer->ptGraphics = ptGraphics;

    ptRenderer->uGlobalStorageBuffer = UINT32_MAX;

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
    ptRenderer->tGlobalBindGroup.tLayout = tGlobalGroupLayout;

    //~~~~~~~~~~~~~~~~~~~~~~~~create shader descriptions~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // main
    plShaderDesc tMainShaderDesc = {
        .pcPixelShader                       = "phong.frag.spv",
        .pcVertexShader                      = "primitive.vert.spv",
        .tGraphicsState.ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_HAS_NORMAL | PL_MESH_FORMAT_FLAG_HAS_COLOR_0,
        .tGraphicsState.ulDepthMode          = PL_DEPTH_MODE_LESS,
        .tGraphicsState.ulBlendMode          = PL_BLEND_MODE_ALPHA,
        .tGraphicsState.ulCullMode           = VK_CULL_MODE_BACK_BIT,
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

    ptRenderer->uMainShader    = pl_create_shader(&ptGraphics->tResourceManager, &tMainShaderDesc);
    ptRenderer->uOutlineShader = pl_create_shader(&ptGraphics->tResourceManager, &tOutlineShaderDesc);
    ptRenderer->uSkyboxShader  = pl_create_shader(&ptGraphics->tResourceManager, &tSkyboxShaderDesc);
}

void
pl_cleanup_renderer(plRenderer* ptRenderer)
{
    pl_sb_free(ptRenderer->sbfStorageBuffer);
    pl_submit_buffer_for_deletion(&ptRenderer->ptGraphics->tResourceManager, ptRenderer->uGlobalStorageBuffer);
}

void
pl_create_scene(plRenderer* ptRenderer, plScene* ptSceneOut)
{
    plGraphics* ptGraphics = ptRenderer->ptGraphics;
    plResourceManager* ptResourceManager = &ptGraphics->tResourceManager;

    memset(ptSceneOut, 0, sizeof(plScene));

    ptSceneOut->ptRenderer = ptRenderer;

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

    ptSceneOut->uDynamicBufferSize = pl_minu(ptGraphics->tDevice.tDeviceProps.limits.maxUniformBufferRange, 65536);
    ptSceneOut->uDynamicBuffer0 = pl_create_constant_buffer_ex(ptResourceManager, ptSceneOut->uDynamicBufferSize, "renderer dynamic buffer 0");
    ptSceneOut->uDynamicBuffer1 = pl_create_constant_buffer_ex(ptResourceManager, ptSceneOut->uDynamicBufferSize, "renderer dynamic buffer 1");
    ptSceneOut->uDynamicBuffer2 = pl_create_constant_buffer_ex(ptResourceManager, ptSceneOut->uDynamicBufferSize, "renderer dynamic buffer 2");

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
    unsigned char* rawBytes0 = stbi_load("../data/pilotlight-assets-master/SkyBox/right.png", &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
    unsigned char* rawBytes1 = stbi_load("../data/pilotlight-assets-master/SkyBox/left.png", &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
    unsigned char* rawBytes2 = stbi_load("../data/pilotlight-assets-master/SkyBox/top.png", &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
    unsigned char* rawBytes3 = stbi_load("../data/pilotlight-assets-master/SkyBox/bottom.png", &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
    unsigned char* rawBytes4 = stbi_load("../data/pilotlight-assets-master/SkyBox/front.png", &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
    unsigned char* rawBytes5 = stbi_load("../data/pilotlight-assets-master/SkyBox/back.png", &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
    PL_ASSERT(rawBytes0);
    PL_ASSERT(rawBytes1);
    PL_ASSERT(rawBytes2);
    PL_ASSERT(rawBytes3);
    PL_ASSERT(rawBytes4);
    PL_ASSERT(rawBytes5);

    unsigned char* rawBytes = pl_alloc(texWidth * texHeight * texNumChannels * 6);
    memcpy(&rawBytes[texWidth * texHeight * texNumChannels * 0], rawBytes0, texWidth * texHeight * texNumChannels); //-V522 
    memcpy(&rawBytes[texWidth * texHeight * texNumChannels * 1], rawBytes1, texWidth * texHeight * texNumChannels); //-V522
    memcpy(&rawBytes[texWidth * texHeight * texNumChannels * 2], rawBytes2, texWidth * texHeight * texNumChannels); //-V522
    memcpy(&rawBytes[texWidth * texHeight * texNumChannels * 3], rawBytes3, texWidth * texHeight * texNumChannels); //-V522
    memcpy(&rawBytes[texWidth * texHeight * texNumChannels * 4], rawBytes4, texWidth * texHeight * texNumChannels); //-V522
    memcpy(&rawBytes[texWidth * texHeight * texNumChannels * 5], rawBytes5, texWidth * texHeight * texNumChannels); //-V522

    stbi_image_free(rawBytes0);
    stbi_image_free(rawBytes1);
    stbi_image_free(rawBytes2);
    stbi_image_free(rawBytes3);
    stbi_image_free(rawBytes4);
    stbi_image_free(rawBytes5);

    const plTextureDesc tTextureDesc = {
        .tDimensions = {.x = (float)texWidth, .y = (float)texHeight, .z = 1.0f},
        .tFormat     = VK_FORMAT_R8G8B8A8_UNORM,
        .tUsage      = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .uLayers     = 6,
        .uMips       = 1,
        .tType       = VK_IMAGE_TYPE_2D,
        .tViewType   = VK_IMAGE_VIEW_TYPE_CUBE
    };
    ptSceneOut->uSkyboxTexture  = pl_create_texture(&ptGraphics->tResourceManager, tTextureDesc, sizeof(unsigned char) * texHeight * texHeight * texNumChannels * 6, rawBytes, "skybox texture");
    pl_free(rawBytes);

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
            .uIndexBuffer  = pl_create_index_buffer(&ptGraphics->tResourceManager, sizeof(uint32_t) * 36, acSkyboxIndices, "skybox index buffer"),
            .uVertexBuffer = pl_create_vertex_buffer(&ptGraphics->tResourceManager, sizeof(float) * 24, sizeof(float), acSkyBoxVertices, "skybox vertex buffer"),
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
    pl_update_bind_group(ptGraphics, &ptSceneOut->tSkyboxBindGroup0, 1, &ptSceneOut->uDynamicBuffer0, &szSkyboxRangeSize, 1, &ptSceneOut->uSkyboxTexture);
}

void
pl_reset_scene(plScene* ptScene)
{
    ptScene->uDynamicBuffer0_Offset = 0;
}

void
pl_draw_scene(plScene* ptScene)
{
    plGraphics* ptGraphics = ptScene->ptRenderer->ptGraphics;
    plRenderer* ptRenderer = ptScene->ptRenderer;
    plResourceManager* ptResourceManager = &ptGraphics->tResourceManager;

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
                .ptBindGroup1          = &ptMaterial->tMaterialBindGroup,
                .ptBindGroup2          = &ptTransformComponent->tBindGroup2,
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
                    .ptBindGroup1          = &ptOutlineMaterial->tMaterialBindGroup,
                    .ptBindGroup2          = &ptTransformComponent->tBindGroup2,
                    .uDynamicBufferOffset1 = ptOutlineMaterial->uBufferOffset,
                    .uDynamicBufferOffset2 = ptTransformComponent->uBufferOffset
                    }));
            }
        }
    }

    // record draw area
    pl_sb_push(ptRenderer->sbtDrawAreas, ((plDrawArea){
        .ptBindGroup0          = &ptRenderer->tGlobalBindGroup,
        .uDrawOffset           = uDrawOffset,
        .uDrawCount            = pl_sb_size(ptRenderer->sbtDraws),
        .uDynamicBufferOffset0 = uBufferFrameOffset0
    }));

    pl_draw_areas(ptRenderer->ptGraphics, pl_sb_size(ptRenderer->sbtDrawAreas), ptRenderer->sbtDrawAreas, ptRenderer->sbtDraws);

    pl_sb_reset(ptRenderer->sbtDrawAreas);

    // record outlines draw areas
    pl_sb_push(ptRenderer->sbtDrawAreas, ((plDrawArea){
        .ptBindGroup0          = &ptRenderer->tGlobalBindGroup,
        .uDrawOffset           = uOutlineDrawOffset,
        .uDrawCount            = pl_sb_size(ptRenderer->sbtOutlineDraws),
        .uDynamicBufferOffset0 = uBufferFrameOffset0
    }));

    pl_draw_areas(ptRenderer->ptGraphics, pl_sb_size(ptRenderer->sbtDrawAreas), ptRenderer->sbtDrawAreas, ptRenderer->sbtOutlineDraws);

    pl_sb_reset(ptRenderer->sbtDraws);
    pl_sb_reset(ptRenderer->sbtOutlineDraws);
    pl_sb_reset(ptRenderer->sbtDrawAreas);

    ptScene->uDynamicBuffer0_Offset = (uint32_t)pl_align_up((size_t)ptScene->uDynamicBuffer0_Offset + sizeof(plGlobalInfo), ptGraphics->tDevice.tDeviceProps.limits.minUniformBufferOffsetAlignment);
}

void
pl_scene_update_ecs(plScene* ptScene)
{
    pl_ecs_update(ptScene, &ptScene->tComponentLibrary.tMaterialComponentManager);
    pl_ecs_update(ptScene, &ptScene->tComponentLibrary.tOutlineMaterialComponentManager);
    pl_ecs_update(ptScene, &ptScene->tComponentLibrary.tObjectComponentManager); 
    pl_ecs_update(ptScene, &ptScene->tComponentLibrary.tHierarchyComponentManager); 
    pl_ecs_update(ptScene, &ptScene->tComponentLibrary.tTransformComponentManager); 
}

void
pl_scene_bind_target(plScene* ptScene, plRenderTarget* ptTarget)
{
    ptScene->ptRenderTarget = ptTarget;
}

void
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

    plIOContext* ptIOCtx = pl_get_io_context();
    ptGlobalInfo->fTime  = (float)ptIOCtx->dTime;
}

void
pl_draw_sky(plScene* ptScene)
{

    plGraphics* ptGraphics = ptScene->ptRenderer->ptGraphics;
    plRenderer* ptRenderer = ptScene->ptRenderer;
    plResourceManager* ptResourceManager = &ptGraphics->tResourceManager;
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
        uSkyboxShaderVariant = pl_add_shader_variant(ptResourceManager, ptRenderer->uSkyboxShader, tFillStateTemplate, ptScene->ptRenderTarget->tDesc.tRenderPass._tRenderPass, tMSAASampleCount);
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


    pl_draw_areas(ptRenderer->ptGraphics, pl_sb_size(ptRenderer->sbtDrawAreas), ptRenderer->sbtDrawAreas, ptRenderer->sbtDraws);

    pl_sb_reset(ptRenderer->sbtDraws);
    pl_sb_reset(ptRenderer->sbtDrawAreas);

    ptScene->uDynamicBuffer0_Offset = (uint32_t)pl_align_up((size_t)ptScene->uDynamicBuffer0_Offset + sizeof(plGlobalInfo), ptGraphics->tDevice.tDeviceProps.limits.minUniformBufferOffsetAlignment);
}

plEntity
pl_ecs_create_entity(plRenderer* ptRenderer)
{
    plEntity tNewEntity = ptRenderer->tNextEntity++;
    return tNewEntity;
}

size_t
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

void*
pl_ecs_get_component(plComponentManager* ptManager, plEntity tEntity)
{
    PL_ASSERT(tEntity != PL_INVALID_ENTITY_HANDLE);
    size_t szIndex = pl_ecs_get_index(ptManager, tEntity);
    unsigned char* pucData = ptManager->pData;
    return &pucData[szIndex * ptManager->szStride];
}

void*
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
        pl_sb_push(sbComponents, ((plTransformComponent){.bDirty = true, .tWorld = pl_identity_mat4(), .tFinalTransform = pl_identity_mat4()}));
        ptManager->pData = sbComponents;
        pl_sb_push(ptManager->sbtEntities, tEntity);
        return &pl_sb_back(sbComponents);
    }

    case PL_COMPONENT_TYPE_MATERIAL:
    {
        plMaterialComponent* sbComponents = ptManager->pData;
        pl_sb_push(sbComponents, (plMaterialComponent){.bDirty = true});
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

bool
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

void
pl_ecs_update(plScene* ptScene, plComponentManager* ptManager)
{

    plGraphics* ptGraphics = ptScene->ptRenderer->ptGraphics;
    plResourceManager* ptResourceManager = &ptGraphics->tResourceManager;

    switch (ptManager->tComponentType)
    {
    case PL_COMPONENT_TYPE_MATERIAL:
    {

        VkSampleCountFlagBits tMSAASampleCount = ptScene->ptRenderTarget->bMSAA ? ptGraphics->tSwapchain.tMsaaSamples : VK_SAMPLE_COUNT_1_BIT;
        uint32_t* sbuTextures = NULL;
        ptScene->uDynamicBuffer1_Offset = 0;
        size_t szRangeSize = sizeof(plMaterialInfo);

        plMaterialComponent* sbtComponents = ptManager->pData;

        const plBuffer* ptBuffer = &ptResourceManager->sbtBuffers[ptScene->uDynamicBuffer1];

        for(uint32_t i = 0; i < pl_sb_size(sbtComponents); i++)
        {
            plMaterialComponent* ptMaterial = &sbtComponents[i];

            if(ptMaterial->bDirty)
            {

                const uint32_t acShaderLookup[] = {
                    ptScene->ptRenderer->uMainShader,
                    ptScene->ptRenderer->uOutlineShader,
                    ptMaterial->uShader
                };

                ptMaterial->uShader = acShaderLookup[ptMaterial->tShaderType];

                pl_sb_push(sbuTextures, ptMaterial->uAlbedoMap);
                pl_sb_push(sbuTextures, ptMaterial->uNormalMap);
                pl_sb_push(sbuTextures, ptMaterial->uEmissiveMap);
                ptMaterial->tMaterialBindGroup.tLayout = *pl_get_bind_group_layout(ptResourceManager, ptMaterial->uShader, 1);
                pl_update_bind_group(ptGraphics, &ptMaterial->tMaterialBindGroup, 1, &ptScene->uDynamicBuffer1, &szRangeSize, pl_sb_size(sbuTextures), sbuTextures);
            }
                
            // find variants
            ptMaterial->uShaderVariant = UINT32_MAX;

            if(ptMaterial->uAlbedoMap > 0)   ptMaterial->tGraphicsState.ulShaderTextureFlags |= PL_SHADER_TEXTURE_FLAG_BINDING_0;
            if(ptMaterial->uNormalMap > 0)   ptMaterial->tGraphicsState.ulShaderTextureFlags |= PL_SHADER_TEXTURE_FLAG_BINDING_1;
            if(ptMaterial->uEmissiveMap > 0) ptMaterial->tGraphicsState.ulShaderTextureFlags |= PL_SHADER_TEXTURE_FLAG_BINDING_2;

            const plShader* ptShader = &ptResourceManager->sbtShaders[ptMaterial->uShader];   
            const uint32_t uVariantCount = pl_sb_size(ptShader->tDesc.sbtVariants);

            for(uint32_t j = 0; j < uVariantCount; j++)
            {
                plGraphicsState ptVariant = ptShader->tDesc.sbtVariants[j].tGraphicsState;
                if(ptVariant.ulValue == ptMaterial->tGraphicsState.ulValue 
                    && ptShader->tDesc.sbtVariants[j].tRenderPass == ptScene->ptRenderTarget->tDesc.tRenderPass._tRenderPass
                    && tMSAASampleCount == ptShader->tDesc.sbtVariants[j].tMSAASampleCount)
                {
                        ptMaterial->uShaderVariant = ptShader->_sbuVariantPipelines[j];
                        break;
                }
            }

            // create variant that matches texture count, vertex stream, and culling
            if(ptMaterial->uShaderVariant == UINT32_MAX)
            {
                ptMaterial->uShaderVariant = pl_add_shader_variant(ptResourceManager, ptMaterial->uShader, ptMaterial->tGraphicsState, ptScene->ptRenderTarget->tDesc.tRenderPass._tRenderPass, tMSAASampleCount);
            }
            
            ptMaterial->bDirty = false;
            pl_sb_reset(sbuTextures);

            const uint32_t uBufferFrameOffset = ((uint32_t)ptBuffer->szSize / ptGraphics->uFramesInFlight) * (uint32_t)ptGraphics->szCurrentFrameIndex;

            plMaterialInfo* ptMaterialInfo = (plMaterialInfo*)(ptBuffer->pucMapping + ptScene->uDynamicBuffer1_Offset + uBufferFrameOffset);
            ptMaterialInfo->tAlbedo = ptMaterial->tAlbedo;

            ptMaterial->uBufferOffset = ((uint32_t)ptBuffer->szSize / ptGraphics->uFramesInFlight) * (uint32_t)ptGraphics->szCurrentFrameIndex + ptScene->uDynamicBuffer1_Offset;

            ptScene->uDynamicBuffer1_Offset = (uint32_t)pl_align_up((size_t)ptScene->uDynamicBuffer1_Offset + sizeof(plMaterialInfo), ptGraphics->tDevice.tDeviceProps.limits.minUniformBufferOffsetAlignment);   
        }
        break;
    }

    case PL_COMPONENT_TYPE_TRANSFORM:
    {
        ptScene->uDynamicBuffer2_Offset = 0;
        size_t szRangeSize = sizeof(plTransformComponent);

        plTransformComponent* sbtComponents = ptManager->pData;

        const plBuffer* ptBuffer = &ptResourceManager->sbtBuffers[ptScene->uDynamicBuffer2];

        for(uint32_t i = 0; i < pl_sb_size(sbtComponents); i++)
        {
            plTransformComponent* ptTransform = &sbtComponents[i];

            if(ptTransform->bDirty)
            {
                const plBindGroupLayout tGroupLayout2 = {
                    .uBufferCount = 1,
                    .aBuffers      = {
                        { .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}  
                    }
                };
                ptTransform->tBindGroup2.tLayout = tGroupLayout2;
                pl_update_bind_group(ptGraphics, &ptTransform->tBindGroup2, 1, &ptScene->uDynamicBuffer2, &szRangeSize, 0, NULL);
                ptTransform->bDirty = false;
            }

            const uint32_t uBufferFrameOffset = ((uint32_t)ptBuffer->szSize / ptGraphics->uFramesInFlight) * (uint32_t)ptGraphics->szCurrentFrameIndex;

            plObjectInfo* ptObjectInfo = (plObjectInfo*)(ptBuffer->pucMapping + ptScene->uDynamicBuffer2_Offset + uBufferFrameOffset);
            *ptObjectInfo = ptTransform->tInfo;
            ptObjectInfo->tModel = ptTransform->tFinalTransform;

            ptTransform->uBufferOffset = uBufferFrameOffset + ptScene->uDynamicBuffer2_Offset;

            ptScene->uDynamicBuffer2_Offset = (uint32_t)pl_align_up((size_t)ptScene->uDynamicBuffer2_Offset + sizeof(plObjectInfo), ptGraphics->tDevice.tDeviceProps.limits.minUniformBufferOffsetAlignment);   
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
            plObjectComponent* ptParent = pl_ecs_get_component(ptScene->ptObjectComponentManager, ptHierarchyComponent->tParent);
            plObjectComponent* ptChild = pl_ecs_get_component(ptScene->ptObjectComponentManager, tChildEntity);
            plTransformComponent* ptParentTransform = pl_ecs_get_component(ptScene->ptTransformComponentManager, ptParent->tTransform);
            plTransformComponent* ptChildTransform = pl_ecs_get_component(ptScene->ptTransformComponentManager, ptChild->tTransform);
            ptChildTransform->tFinalTransform = pl_mul_mat4(&ptParentTransform->tFinalTransform, &ptChildTransform->tWorld);
        }
        break;
    }
    }
}

plEntity
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

plMaterialComponent*
pl__ecs_create_outline_material(plScene* ptScene, plEntity tEntity)
{
    plRenderer* ptRenderer = ptScene->ptRenderer;

    plMaterialComponent* ptMaterial = pl_ecs_create_component(&ptScene->tComponentLibrary.tOutlineMaterialComponentManager, tEntity);
    memset(ptMaterial, 0, sizeof(plMaterialComponent));
    ptMaterial->bDirty                              = true;
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

plEntity
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
    ptMaterial->bDirty = true;
    ptMaterial->uShader = UINT32_MAX;
    ptMaterial->tAlbedo = (plVec4){ 1.0f, 1.0f, 1.0f, 1.0f };
    ptMaterial->fAlphaCutoff = 0.1f;
    ptMaterial->bDoubleSided = false;
    ptMaterial->tShaderType = PL_SHADER_TYPE_PBR;
    ptMaterial->tGraphicsState.ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_HAS_NORMAL;
    ptMaterial->tGraphicsState.ulDepthMode          = PL_DEPTH_MODE_LESS_OR_EQUAL;
    ptMaterial->tGraphicsState.ulDepthWriteEnabled  = true;
    ptMaterial->tGraphicsState.ulCullMode           = VK_CULL_MODE_BACK_BIT;
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

plEntity
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
    ptTransform->bDirty = true;
    ptTransform->tInfo.tModel = pl_identity_mat4();
    ptTransform->tWorld = pl_identity_mat4();

    plMeshComponent* ptMesh = pl_ecs_create_component(&ptScene->tComponentLibrary.tMeshComponentManager, tNewEntity);

    ptObject->tTransform = tNewEntity;
    ptObject->tMesh = tNewEntity;

    return tNewEntity;    
}

plEntity
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
    ptTransform->bDirty = true;
    ptTransform->tInfo.tModel = pl_identity_mat4();
    ptTransform->tWorld = pl_identity_mat4();

    return tNewEntity;  
}

plEntity
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

void
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

void
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

void
pl_material_outline(plScene* ptScene, plEntity tEntity)
{
    plMaterialComponent* ptMaterial = pl_ecs_get_component(&ptScene->tComponentLibrary.tMaterialComponentManager, tEntity);
    ptMaterial->tGraphicsState.ulStencilOpFail      = VK_STENCIL_OP_REPLACE;
    ptMaterial->tGraphicsState.ulStencilOpDepthFail = VK_STENCIL_OP_REPLACE;
    ptMaterial->tGraphicsState.ulStencilOpPass      = VK_STENCIL_OP_REPLACE;
    ptMaterial->bOutline                            = true;

    plMaterialComponent* ptOutlineMaterial = pl__ecs_create_outline_material(ptScene, tEntity);
    ptOutlineMaterial->tGraphicsState.ulVertexStreamMask   = ptMaterial->tGraphicsState.ulVertexStreamMask;

}

void
pl_camera_set_fov(plCameraComponent* ptCamera, float fYFov)
{
    ptCamera->fFieldOfView = fYFov;
}

void
pl_camera_set_clip_planes(plCameraComponent* ptCamera, float fNearZ, float fFarZ)
{
    ptCamera->fNearZ = fNearZ;
    ptCamera->fFarZ = fFarZ;
}

void
pl_camera_set_aspect(plCameraComponent* ptCamera, float fAspect)
{
    ptCamera->fAspectRatio = fAspect;
}

void
pl_camera_set_pos(plCameraComponent* ptCamera, float fX, float fY, float fZ)
{
    ptCamera->tPos.x = fX;
    ptCamera->tPos.y = fY;
    ptCamera->tPos.z = fZ;
}

void
pl_camera_set_pitch_yaw(plCameraComponent* ptCamera, float fPitch, float fYaw)
{
    ptCamera->fPitch = fPitch;
    ptCamera->fYaw = fYaw;
}

void
pl_camera_translate(plCameraComponent* ptCamera, float fDx, float fDy, float fDz)
{
    ptCamera->tPos = pl_add_vec3(ptCamera->tPos, pl_mul_vec3_scalarf(ptCamera->_tRightVec, fDx));
    ptCamera->tPos = pl_add_vec3(ptCamera->tPos, pl_mul_vec3_scalarf(ptCamera->_tForwardVec, fDz));
    ptCamera->tPos.y += fDy;
}

void
pl_camera_rotate(plCameraComponent* ptCamera, float fDPitch, float fDYaw)
{
    ptCamera->fPitch += fDPitch;
    ptCamera->fYaw += fDYaw;

    ptCamera->fYaw = pl__wrap_angle(ptCamera->fYaw);
    ptCamera->fPitch = pl_clampf(0.995f * -PL_PI_2, ptCamera->fPitch, 0.995f * PL_PI_2);
}

void
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
