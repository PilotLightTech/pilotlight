/*
   pl_renderer_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] global data
// [SECTION] internal api
// [SECTION] public api implementations
// [SECTION] internal api implementations
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pilotlight.h"
#include "pl_renderer_ext.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"
#include "pl_ds.h"
#include "pl_io.h"
#include "pl_memory.h"
#include "pl_log.h"
#include "pl_profile.h"

// required extensions
#include "pl_ui_ext.h"
#include "pl_draw_ext.h"
#include "pl_image_ext.h"
#include "pl_vulkan_ext.h"
#include "pl_ecs_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------
static uint32_t uLogChannel = UINT32_MAX;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

// graphics
static void pl_create_main_render_target(plGraphics* ptGraphics, plRenderTarget* ptTargetOut);
static void pl_create_render_target(plGraphics* ptGraphics, const plRenderTargetDesc* ptDesc, plRenderTarget* ptTargetOut);
static void pl_begin_render_target (const plGraphicsApiI* ptGfx, plGraphics* ptGraphics, plRenderTarget* ptTarget);
static void pl_end_render_target   (const plGraphicsApiI* ptGfx, plGraphics* ptGraphics);
static void pl_cleanup_render_target(plGraphics* ptGraphics, plRenderTarget* ptTarget);
static void pl_setup_renderer  (const plApiRegistryApiI* ptApiRegistry, plComponentLibrary* ptComponentLibrary, plGraphics* ptGraphics, plRenderer* ptRenderer);
static void pl_cleanup_renderer(plRenderer* ptRenderer);
static void pl_resize_renderer (plRenderer* ptRenderer, float fWidth, float fHeight);
static void pl_draw_sky        (plScene* ptScene);

// scene
static void pl_create_scene      (plRenderer* ptRenderer, plComponentLibrary* ptComponentLibrary, plScene* ptSceneOut);
static void pl_cleanup_scene     (plScene* ptScene);
static void pl_reset_scene       (plScene* ptScene);
static void pl_draw_scene        (plScene* ptScene);
static void pl_draw_pick_scene   (plScene* ptScene);
static void pl_scene_bind_camera (plScene* ptScene, const plCameraComponent* ptCamera);
static void pl_scene_bind_target (plScene* ptScene, plRenderTarget* ptTarget);
static void pl_scene_prepare     (plScene* ptScene);

// entity component system
static void pl_prepare_gpu_data(plScene* ptScene);
static void pl__prepare_material_gpu_data(plScene* ptScene, plComponentManager* ptManager);
static void pl__prepare_object_gpu_data(plScene* ptScene, plComponentManager* ptManager);

// helpers
static void pl__add_primary_mesh_data         (plScene* ptScene, plMeshComponent* ptMesh);
static void pl__add_secondary_mesh_vertex_data(plScene* ptScene, plMeshComponent* ptMesh);

static bool
pl__get_free_resource_index(uint32_t* sbuFreeIndices, uint32_t* puIndexOut)
{
    // check if previous index is availble
    if(pl_sb_size(sbuFreeIndices) > 0)
    {
        const uint32_t uFreeIndex = pl_sb_pop(sbuFreeIndices);
        *puIndexOut = uFreeIndex;
        return true;
    }
    return false;    
}

//-----------------------------------------------------------------------------
// [SECTION] public api implementations
//-----------------------------------------------------------------------------

const plRendererI*
pl_load_renderer_api(void)
{
    static const plRendererI tApi0 = {
        .create_main_render_target = pl_create_main_render_target,
        .create_render_target      = pl_create_render_target,
        .begin_render_target       = pl_begin_render_target,
        .end_render_target         = pl_end_render_target,
        .cleanup_render_target     = pl_cleanup_render_target,
        .setup_renderer            = pl_setup_renderer,
        .resize                    = pl_resize_renderer,
        .cleanup_renderer          = pl_cleanup_renderer,
        .draw_sky                  = pl_draw_sky,
        .create_scene              = pl_create_scene,
        .cleanup_scene             = pl_cleanup_scene,
        .reset_scene               = pl_reset_scene,
        .draw_scene                = pl_draw_scene,
        .draw_pick_scene           = pl_draw_pick_scene,
        .scene_bind_camera         = pl_scene_bind_camera,
        .scene_bind_target         = pl_scene_bind_target,
        .scene_prepare             = pl_scene_prepare,
        .prepare_scene_gpu_data    = pl_prepare_gpu_data
    };
    return &tApi0;
}

//-----------------------------------------------------------------------------
// [SECTION] internal implementations
//-----------------------------------------------------------------------------

static void
pl_create_render_target2(plGraphics* ptGraphics, const plRenderTargetDesc* ptDesc, plRenderTarget* ptTargetOut, const char* pcName)
{
    ptTargetOut->tDesc = *ptDesc;
    const plDeviceApiI* ptDeviceApi = ptGraphics->ptDeviceApi;
    plDevice* ptDevice = &ptGraphics->tDevice;

    const plTextureDesc tColorTextureDesc = {
        .tDimensions = {.x = ptDesc->tSize.x, .y = ptDesc->tSize.y, .z = 1.0f},
        .tFormat     = ptDevice->sbtRenderPasses[ptDesc->uRenderPass].tDesc.tColorFormat,
        .tUsage      = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .uLayers     = 1,
        .uMips       = 1,
        .tType       = VK_IMAGE_TYPE_2D
    };

    const plTextureDesc tDepthTextureDesc = {
        .tDimensions = {.x = ptDesc->tSize.x, .y = ptDesc->tSize.y, .z = 1.0f},
        .tFormat     = ptDevice->sbtRenderPasses[ptDesc->uRenderPass].tDesc.tDepthFormat,
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

    pl_sb_reset(ptTargetOut->sbuColorTextureViews);
    for(uint32_t i = 0; i < ptGraphics->tSwapchain.uImageCount; i++)
    {
        uint32_t uColorTexture = ptDeviceApi->create_texture(ptDevice, tColorTextureDesc, 0, NULL, "pick color texture");
        pl_sb_push(ptTargetOut->sbuColorTextureViews, ptDeviceApi->create_texture_view(ptDevice, &tColorView, &tColorSampler, uColorTexture, "offscreen color view"));
    }

    uint32_t uDepthTexture = ptDeviceApi->create_texture(ptDevice, tDepthTextureDesc, 0, NULL, "pick depth texture");

    const plTextureViewDesc tDepthView = {
        .tFormat     = tDepthTextureDesc.tFormat,
        .uLayerCount = tDepthTextureDesc.uLayers,
        .uMips       = tDepthTextureDesc.uMips
    };

    ptTargetOut->uDepthTextureView = ptDeviceApi->create_texture_view(ptDevice, &tDepthView, &tColorSampler, uDepthTexture, "offscreen depth view");

    plTextureView* ptDepthTextureView = &ptDevice->sbtTextureViews[ptTargetOut->uDepthTextureView];

    pl_sb_reset(ptTargetOut->sbuFrameBuffers);
    for(uint32_t i = 0; i < ptGraphics->tSwapchain.uImageCount; i++)
    {

        uint32_t auAttachments[] = {
            ptTargetOut->sbuColorTextureViews[i],
            ptTargetOut->uDepthTextureView
        };

        plFrameBufferDesc tFBDesc = {
            .uAttachmentCount = 2,
            .puAttachments    = auAttachments,
            .uRenderPass      = ptDesc->uRenderPass,
            .uWidth           = (uint32_t)ptDesc->tSize.x,
            .uHeight          = (uint32_t)ptDesc->tSize.y,
        };
        pl_sb_push(ptTargetOut->sbuFrameBuffers, ptDeviceApi->create_frame_buffer(ptDevice, &tFBDesc, pcName));    
    }
}

static void
pl_create_render_target(plGraphics* ptGraphics, const plRenderTargetDesc* ptDesc, plRenderTarget* ptTargetOut)
{
    ptTargetOut->tDesc = *ptDesc;
    const plDeviceApiI* ptDeviceApi = ptGraphics->ptDeviceApi;
    plDevice* ptDevice = &ptGraphics->tDevice;

    const plTextureDesc tColorTextureDesc = {
        .tDimensions = {.x = ptDesc->tSize.x, .y = ptDesc->tSize.y, .z = 1.0f},
        .tFormat     = ptDevice->sbtRenderPasses[ptDesc->uRenderPass].tDesc.tColorFormat,
        .tUsage      = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .uLayers     = 1,
        .uMips       = 1,
        .tType       = VK_IMAGE_TYPE_2D
    };

    const plTextureDesc tDepthTextureDesc = {
        .tDimensions = {.x = ptDesc->tSize.x, .y = ptDesc->tSize.y, .z = 1.0f},
        .tFormat     = ptDevice->sbtRenderPasses[ptDesc->uRenderPass].tDesc.tDepthFormat,
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

    for(uint32_t i = 0; i < ptGraphics->tSwapchain.uImageCount; i++)
    {
        uint32_t auAttachments[] = {
            ptTargetOut->sbuColorTextureViews[i],
            ptTargetOut->uDepthTextureView
        };

        plFrameBufferDesc tFrameBufferDesc = {
            .uAttachmentCount = 2,
            .uWidth           = (uint32_t)ptDesc->tSize.x,
            .uHeight          = (uint32_t)ptDesc->tSize.y,
            .uRenderPass      = ptDesc->uRenderPass,
            .puAttachments    = auAttachments
        };
        pl_sb_push(ptTargetOut->sbuFrameBuffers, ptDeviceApi->create_frame_buffer(ptDevice, &tFrameBufferDesc, "offscreen target"));
    }
}

static void
pl_create_main_render_target(plGraphics* ptGraphics, plRenderTarget* ptTargetOut)
{
    plIOContext* ptIOCtx = pl_get_io_context();
    plDevice* ptDevice = &ptGraphics->tDevice;
    ptTargetOut->bMSAA = true;
    ptTargetOut->sbuFrameBuffers = ptGraphics->tSwapchain.puFrameBuffers;
    ptTargetOut->tDesc.uRenderPass = ptGraphics->uRenderPass;
    ptDevice->sbtRenderPasses[ptTargetOut->tDesc.uRenderPass].tDesc.tColorFormat = ptGraphics->tSwapchain.tFormat;
    ptDevice->sbtRenderPasses[ptTargetOut->tDesc.uRenderPass].tDesc.tDepthFormat = ptGraphics->tSwapchain.tDepthFormat;
    ptTargetOut->tDesc.tSize.x = (float)ptGraphics->tSwapchain.tExtent.width;
    ptTargetOut->tDesc.tSize.y = (float)ptGraphics->tSwapchain.tExtent.height;
}

static void
pl_begin_render_target(const plGraphicsApiI* ptGfx, plGraphics* ptGraphics, plRenderTarget* ptTarget)
{
    const plFrameContext* ptCurrentFrame = ptGfx->get_frame_resources(ptGraphics);
    plDevice* ptDevice = &ptGraphics->tDevice;

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
        .renderPass          = ptDevice->sbtRenderPasses[ptTarget->tDesc.uRenderPass]._tRenderPass,
        .framebuffer         = ptDevice->sbtFrameBuffers[ptTarget->sbuFrameBuffers[ptGraphics->tSwapchain.uCurrentImageIndex]]._tFrameBuffer,
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
pl_end_render_target(const plGraphicsApiI* ptGfx, plGraphics* ptGraphics)
{
    const plFrameContext* ptCurrentFrame = ptGfx->get_frame_resources(ptGraphics);

    vkCmdEndRenderPass(ptCurrentFrame->tCmdBuf);
}

static void
pl_cleanup_render_target(plGraphics* ptGraphics, plRenderTarget* ptTarget)
{
    plDevice* ptDevice = &ptGraphics->tDevice;
    const plDeviceApiI* ptDeviceApi = ptGraphics->ptDeviceApi;

    for (uint32_t i = 0u; i < pl_sb_size(ptTarget->sbuFrameBuffers); i++)
        ptDeviceApi->submit_frame_buffer_for_deletion(ptDevice, ptTarget->sbuFrameBuffers[i]);
    pl_sb_free(ptTarget->sbuFrameBuffers);
    pl_sb_free(ptTarget->sbuColorTextureViews);
}

static void
pl_setup_renderer(const plApiRegistryApiI* ptApiRegistry, plComponentLibrary* ptComponentLibrary, plGraphics* ptGraphics, plRenderer* ptRenderer)
{
    memset(ptRenderer, 0, sizeof(plRenderer));
    ptRenderer->ptGfx = ptApiRegistry->first(PL_API_GRAPHICS);
    ptRenderer->ptDeviceApi = ptApiRegistry->first(PL_API_DEVICE);
    ptRenderer->ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    ptRenderer->ptRendererApi = ptApiRegistry->first(PL_API_RENDERER);
    ptRenderer->ptImageApi = ptApiRegistry->first(PL_API_IMAGE);
    ptRenderer->ptEcs = ptApiRegistry->first(PL_API_ECS);

    const plGraphicsApiI* ptGfx = ptRenderer->ptGfx;
    const plDeviceApiI* ptDeviceApi = ptGraphics->ptDeviceApi;
    plDevice* ptDevice = &ptGraphics->tDevice;

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
                .uBufferCount = 3,
                .aBuffers = {
                    { .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 1, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT },
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 2, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}
                },
            },
            {
                .uBufferCount = 1,
                .aBuffers = {
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
                .uBufferCount = 3,
                .aBuffers = {
                    { .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 1, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT },
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 2, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}
                },
            },
            {
                .uBufferCount = 1,
                .aBuffers = {
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

    // pick
    plShaderDesc tPickShaderDesc = {
        .pcPixelShader                       = "pick.frag.spv",
        .pcVertexShader                      = "pick.vert.spv",
        .tGraphicsState.ulDepthMode          = PL_DEPTH_MODE_LESS,
        .tGraphicsState.ulBlendMode          = PL_BLEND_MODE_NONE,
        .tGraphicsState.ulCullMode           = VK_CULL_MODE_BACK_BIT,
        .tGraphicsState.ulDepthWriteEnabled  = VK_TRUE,
        .tGraphicsState.ulShaderTextureFlags = PL_SHADER_TEXTURE_FLAG_BINDING_NONE,
        .tGraphicsState.ulStencilMode        = PL_STENCIL_MODE_ALWAYS,
        .uBindGroupLayoutCount               = 2,
        .atBindGroupLayouts                  = {
            {
                .uBufferCount = 2,
                .aBuffers = {
                    { .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 1, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT }
                },
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
    ptRenderer->uPickShader    = ptGfx->create_shader(ptGraphics, &tPickShaderDesc);

    // offscreen
    plRenderPassDesc tRenderPassDesc = {
        .tColorFormat = PL_FORMAT_R8G8B8A8_UNORM,
        .tDepthFormat = ptRenderer->ptDeviceApi->find_depth_stencil_format(ptDevice)
    };

    ptRenderer->uPickPass = 0u;
    if(!pl__get_free_resource_index(ptDevice->_sbulFrameBufferFreeIndices, &ptRenderer->uPickPass))
        ptRenderer->uPickPass = pl_sb_add_n(ptDevice->sbtRenderPasses, 1);
    ptDevice->sbtRenderPasses[ptRenderer->uPickPass].tDesc = tRenderPassDesc;
    // pl_create_render_pass(ptGraphics, &tRenderPassDesc, &ptRenderer->tPickPass);

    // create render pass
    VkAttachmentDescription atAttachments[] = {

        // color attachment
        {
            .flags          = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT,
            .format         = ptRenderer->ptDeviceApi->vulkan_format(tRenderPassDesc.tColorFormat),
            .samples        = VK_SAMPLE_COUNT_1_BIT,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .finalLayout    = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
        },

        // depth attachment
        {
            .format         = ptRenderer->ptDeviceApi->vulkan_format(tRenderPassDesc.tDepthFormat),
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
    PL_VULKAN(vkCreateRenderPass(ptGraphics->tDevice.tLogicalDevice, &tRenderPassInfo, NULL, &ptDevice->sbtRenderPasses[ptRenderer->uPickPass]._tRenderPass));

    plRenderTargetDesc tRenderTargetDesc = {
        .uRenderPass = ptRenderer->uPickPass,
        .tSize = {500.0f, 500.0f}
    };
    pl_create_render_target2(ptGraphics, &tRenderTargetDesc, &ptRenderer->tPickTarget, "pick render target");

    ptRenderer->uPickMaterial = ptGfx->add_shader_variant(ptGraphics, ptRenderer->uPickShader, 
        tPickShaderDesc.tGraphicsState, ptRenderer->tPickTarget.tDesc.uRenderPass, VK_SAMPLE_COUNT_1_BIT);
}

static void
pl_cleanup_renderer(plRenderer* ptRenderer)
{
    pl_sb_free(ptRenderer->sbtVisibleMeshes);
    pl_sb_free(ptRenderer->sbtVisibleOutlinedMeshes);
    pl_sb_free(ptRenderer->sbtMaterialBindGroups);
    pl_sb_free(ptRenderer->sbtObjectBindGroups);
    pl_sb_free(ptRenderer->sbtDraws);
    pl_sb_free(ptRenderer->sbtDrawAreas);
    pl_sb_free(ptRenderer->sbtTextures);
}

static void
pl_resize_renderer(plRenderer* ptRenderer, float fWidth, float fHeight)
{
    const plGraphicsApiI* ptGfx = ptRenderer->ptGfx;
    plGraphics* ptGraphics = ptRenderer->ptGraphics;
    plDevice* ptDevice = &ptGraphics->tDevice;
    const plDeviceApiI* ptDeviceApi = ptRenderer->ptDeviceApi;

    for(uint32_t i = 0; i < pl_sb_size(ptRenderer->tPickTarget.sbuColorTextureViews); i++)
    {
        uint32_t uTextureView = ptRenderer->tPickTarget.sbuColorTextureViews[i];
        uint32_t uTexture = ptDevice->sbtTextureViews[uTextureView].uTextureHandle;
        ptDeviceApi->submit_texture_for_deletion(ptDevice, uTexture);
        ptDeviceApi->submit_texture_view_for_deletion(ptDevice, uTextureView);
    }
    
    pl_cleanup_render_target(ptGraphics, &ptRenderer->tPickTarget);
    pl_sb_reset(ptRenderer->tPickTarget.sbuColorTextureViews);
    pl_sb_reset(ptRenderer->tPickTarget.sbuFrameBuffers);

    uint32_t uDepthTextureView = ptRenderer->tPickTarget.uDepthTextureView;
    uint32_t uDepthTexture = ptDevice->sbtTextureViews[uDepthTextureView].uTextureHandle;
    ptDeviceApi->submit_texture_for_deletion(ptDevice, uDepthTexture);
    ptDeviceApi->submit_texture_view_for_deletion(ptDevice, uDepthTextureView);

    plRenderTargetDesc tRenderTargetDesc = {
        .uRenderPass = ptRenderer->uPickPass,
        .tSize = {fWidth, fHeight}
    };
    pl_create_render_target2(ptGraphics, &tRenderTargetDesc, &ptRenderer->tPickTarget, "pick target");
}

static void
pl_create_scene(plRenderer* ptRenderer, plComponentLibrary* ptComponentLibrary, plScene* ptSceneOut)
{
    plGraphics* ptGraphics = ptRenderer->ptGraphics;
    const plGraphicsApiI* ptGfx = ptRenderer->ptGfx;
    const plImageApiI* ptImageApi = ptRenderer->ptImageApi;
    const plDeviceApiI* ptDeviceApi = ptGraphics->ptDeviceApi;
    plDevice* ptDevice = &ptGraphics->tDevice;
    const plEcsI* ptEcsApi = ptRenderer->ptEcs;

    memset(ptSceneOut, 0, sizeof(plScene));

    ptSceneOut->ptComponentLibrary = ptComponentLibrary;
    ptSceneOut->uGlobalVertexData = UINT32_MAX;
    ptSceneOut->ptRenderer = ptRenderer;
    ptSceneOut->bMaterialsNeedUpdate = true;
    ptSceneOut->bMeshesNeedUpdate = true;

    ptSceneOut->uDynamicBuffer0 = ptDeviceApi->create_constant_buffer(ptDevice, ptDevice->uUniformBufferBlockSize, "renderer dynamic buffer 0");

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

    unsigned char* rawBytes = PL_ALLOC(texWidth * texHeight * texForceNumChannels * 6);
    memcpy(&rawBytes[texWidth * texHeight * texForceNumChannels * 0], rawBytes0, texWidth * texHeight * texForceNumChannels); //-V522 
    memcpy(&rawBytes[texWidth * texHeight * texForceNumChannels * 1], rawBytes1, texWidth * texHeight * texForceNumChannels); //-V522
    memcpy(&rawBytes[texWidth * texHeight * texForceNumChannels * 2], rawBytes2, texWidth * texHeight * texForceNumChannels); //-V522
    memcpy(&rawBytes[texWidth * texHeight * texForceNumChannels * 3], rawBytes3, texWidth * texHeight * texForceNumChannels); //-V522
    memcpy(&rawBytes[texWidth * texHeight * texForceNumChannels * 4], rawBytes4, texWidth * texHeight * texForceNumChannels); //-V522
    memcpy(&rawBytes[texWidth * texHeight * texForceNumChannels * 5], rawBytes5, texWidth * texHeight * texForceNumChannels); //-V522

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

    uint32_t uSkyboxTexture = ptDeviceApi->create_texture(ptDevice, tTextureDesc, sizeof(unsigned char) * texWidth * texHeight * texForceNumChannels * 6, rawBytes, "skybox texture");
    ptSceneOut->uSkyboxTextureView  = ptDeviceApi->create_texture_view(ptDevice, &tSkyboxView, &tSkyboxSampler, uSkyboxTexture, "skybox texture view");
    PL_FREE(rawBytes);

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

    ptSceneOut->tSkyboxMesh = (plMesh) {
        .uIndexCount   = 36,
        .uVertexCount  = 24,
        .uIndexBuffer  = ptDeviceApi->create_index_buffer(ptDevice, sizeof(uint32_t) * 36, acSkyboxIndices, "skybox index buffer"),
        .uVertexBuffer = ptDeviceApi->create_vertex_buffer(ptDevice, sizeof(float) * 24, sizeof(float), acSkyBoxVertices, "skybox vertex buffer"),
        .ulVertexStreamMask = PL_MESH_FORMAT_FLAG_NONE
    };

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

    ptSceneOut->uGlobalVertexData = UINT32_MAX;
    ptSceneOut->uGlobalMaterialData = UINT32_MAX;
    ptSceneOut->uGlobalPickData = UINT32_MAX;

    plBindGroupLayout tGlobalGroupLayout =
    {
        .uBufferCount = 3,
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
            },
            {
                .tType       = PL_BUFFER_BINDING_TYPE_STORAGE,
                .uSlot       = 2,
                .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
            }
        }
    };
    
    // create & update global bind group
    ptSceneOut->tGlobalBindGroup.tLayout = tGlobalGroupLayout;

    plBindGroupLayout tGlobalPickGroupLayout =
    {
        .uBufferCount = 2,
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
            },
        }
    };

    // create & update global bind group
    ptSceneOut->tGlobalPickBindGroup.tLayout = tGlobalPickGroupLayout;
}

static void
pl_cleanup_scene(plScene* ptScene)
{
    pl_sb_free(ptScene->sbfGlobalVertexData);
    pl_sb_free(ptScene->sbtGlobalMaterialData);
    pl_sb_free(ptScene->sbtGlobalPickData);
    pl_sb_free(ptScene->sbtVertexData);
    pl_sb_free(ptScene->sbuIndexData);
}

static void
pl_reset_scene(plScene* ptScene)
{
    ptScene->uDynamicBuffer0_Offset = 0;
    plObjectSystemData* ptObjectSystemData = ptScene->ptComponentLibrary->tObjectComponentManager.pSystemData;
    ptObjectSystemData->bDirty = true;
}

static void
pl_draw_scene(plScene* ptScene)
{
    pl_begin_profile_sample(__FUNCTION__);
    plGraphics* ptGraphics = ptScene->ptRenderer->ptGraphics;
    plRenderer* ptRenderer = ptScene->ptRenderer;
    const plGraphicsApiI* ptGfx = ptRenderer->ptGfx;
    const plEcsI* ptEcs = ptScene->ptRenderer->ptEcs;

    const uint32_t uDrawOffset = pl_sb_size(ptRenderer->sbtDraws);

    // record draws
    for(uint32_t i = 0; i < pl_sb_size(ptRenderer->sbtVisibleMeshes); i++)
    {
        plMeshComponent* ptMeshComponent = ptEcs->get_component(&ptScene->ptComponentLibrary->tMeshComponentManager, ptRenderer->sbtVisibleMeshes[i]);
        plMaterialComponent* ptMaterial = ptEcs->get_component(&ptScene->ptComponentLibrary->tMaterialComponentManager, ptMeshComponent->tMaterial);

        pl_sb_push(ptRenderer->sbtDraws, ((plDraw){
            .uShaderVariant        = ptMaterial->uShaderVariant,
            .ptMesh                = &ptMeshComponent->tMesh,
            .aptBindGroups          = {
                &ptRenderer->sbtMaterialBindGroups[ptMaterial->uBindGroup1],
                &ptRenderer->sbtObjectBindGroups[ptMeshComponent->uBindGroup2]},
            .auDynamicBufferOffset = {0, ptMeshComponent->uBufferOffset}
            }));
    }

    // record draws
    for(uint32_t i = 0; i < pl_sb_size(ptRenderer->sbtVisibleOutlinedMeshes); i++)
    {
        plMeshComponent* ptMeshComponent = ptEcs->get_component(&ptScene->ptComponentLibrary->tMeshComponentManager, ptRenderer->sbtVisibleOutlinedMeshes[i]);
        plMaterialComponent* ptMaterial = ptEcs->get_component(&ptScene->ptComponentLibrary->tMaterialComponentManager, ptMeshComponent->tMaterial);
        plMaterialComponent* ptOutlineMaterial = ptEcs->get_component(&ptScene->ptComponentLibrary->tMaterialComponentManager, ptMeshComponent->tOutlineMaterial);

        pl_sb_push(ptRenderer->sbtDraws, ((plDraw){
            .uShaderVariant        = ptOutlineMaterial->uShaderVariant,
            .ptMesh                = &ptMeshComponent->tMesh,
            .aptBindGroups         = { 
                    &ptRenderer->sbtMaterialBindGroups[ptMaterial->uBindGroup1],
                    &ptRenderer->sbtObjectBindGroups[ptMeshComponent->uBindGroup2] },
            .auDynamicBufferOffset = { 0, ptMeshComponent->uBufferOffset }
            }));
    }

    // record draw area
    const plBuffer* ptBuffer0 = &ptGraphics->tDevice.sbtBuffers[ptScene->uDynamicBuffer0];
    const uint32_t uBufferFrameOffset0 = ((uint32_t)ptBuffer0->tAllocation.ulSize / ptGraphics->uFramesInFlight) * (uint32_t)ptGraphics->szCurrentFrameIndex + ptScene->uDynamicBuffer0_Offset;

    pl_sb_push(ptRenderer->sbtDrawAreas, ((plDrawArea){
        .ptBindGroup0          = &ptScene->tGlobalBindGroup,
        .uDrawOffset           = uDrawOffset,
        .uDrawCount            = pl_sb_size(ptRenderer->sbtDraws),
        .uDynamicBufferOffset0 = uBufferFrameOffset0
    }));

    ptGfx->draw_areas(ptRenderer->ptGraphics, pl_sb_size(ptRenderer->sbtDrawAreas), ptRenderer->sbtDrawAreas, ptRenderer->sbtDraws);

    pl_sb_reset(ptRenderer->sbtDraws);
    pl_sb_reset(ptRenderer->sbtDrawAreas);

    ptScene->uDynamicBuffer0_Offset = (uint32_t)pl_align_up((size_t)ptScene->uDynamicBuffer0_Offset + sizeof(plGlobalInfo), ptGraphics->tDevice.tDeviceProps.limits.minUniformBufferOffsetAlignment);
    pl_end_profile_sample();
}

static void
pl_draw_pick_scene(plScene* ptScene)
{
    pl_begin_profile_sample(__FUNCTION__);
    plGraphics* ptGraphics = ptScene->ptRenderer->ptGraphics;
    plRenderer* ptRenderer = ptScene->ptRenderer;
    const plGraphicsApiI* ptGfx = ptRenderer->ptGfx;
    const plEcsI* ptEcs = ptScene->ptRenderer->ptEcs;

    const uint32_t uDrawOffset = pl_sb_size(ptRenderer->sbtDraws);

    // record draws
    for(uint32_t i = 0; i < pl_sb_size(ptRenderer->sbtVisibleMeshes); i++)
    {
        plMeshComponent* ptMeshComponent = ptEcs->get_component(&ptScene->ptComponentLibrary->tMeshComponentManager, ptRenderer->sbtVisibleMeshes[i]);
        pl_sb_push(ptRenderer->sbtDraws, ((plDraw){
            .uShaderVariant        = ptRenderer->uPickMaterial,
            .ptMesh                = &ptMeshComponent->tMesh,
            .aptBindGroups         = { &ptRenderer->sbtObjectBindGroups[ptMeshComponent->uBindGroup2], NULL },
            .auDynamicBufferOffset = { ptMeshComponent->uBufferOffset, 0}
            }));
    }

    // record draw area
    const plBuffer* ptBuffer0 = &ptGraphics->tDevice.sbtBuffers[ptScene->uDynamicBuffer0];
    const uint32_t uBufferFrameOffset0 = ((uint32_t)ptBuffer0->tAllocation.ulSize / ptGraphics->uFramesInFlight) * (uint32_t)ptGraphics->szCurrentFrameIndex + ptScene->uDynamicBuffer0_Offset;

    pl_sb_push(ptRenderer->sbtDrawAreas, ((plDrawArea){
        .ptBindGroup0          = &ptScene->tGlobalPickBindGroup,
        .uDrawOffset           = uDrawOffset,
        .uDrawCount            = pl_sb_size(ptRenderer->sbtDraws),
        .uDynamicBufferOffset0 = uBufferFrameOffset0
    }));

    ptGfx->draw_areas(ptRenderer->ptGraphics, pl_sb_size(ptRenderer->sbtDrawAreas), ptRenderer->sbtDrawAreas, ptRenderer->sbtDraws);

    pl_sb_reset(ptRenderer->sbtDraws);
    pl_sb_reset(ptRenderer->sbtDrawAreas);

    ptScene->uDynamicBuffer0_Offset = (uint32_t)pl_align_up((size_t)ptScene->uDynamicBuffer0_Offset + sizeof(plGlobalInfo), ptGraphics->tDevice.tDeviceProps.limits.minUniformBufferOffsetAlignment);
    pl_end_profile_sample();
}

static void
pl_prepare_gpu_data(plScene* ptScene)
{
    pl_begin_profile_sample(__FUNCTION__);
    pl__prepare_material_gpu_data(ptScene, &ptScene->ptComponentLibrary->tMaterialComponentManager);
    pl__prepare_object_gpu_data(ptScene, &ptScene->ptComponentLibrary->tObjectComponentManager);
    pl_end_profile_sample();
}

static void
pl_scene_bind_target(plScene* ptScene, plRenderTarget* ptTarget)
{
    ptScene->ptRenderTarget = ptTarget;
}

static void
pl__add_primary_mesh_data(plScene* ptScene, plMeshComponent* ptMesh)
{

    // current location in global buffers
    ptMesh->tMesh.uIndexOffset = pl_sb_size(ptScene->sbuIndexData);
    ptMesh->tMesh.uIndexCount = pl_sb_size(ptMesh->sbuIndices);
    ptMesh->tMesh.uVertexOffset = pl_sb_size(ptScene->sbtVertexData);
    ptMesh->tMesh.uVertexCount = pl_sb_size(ptMesh->sbtVertexPositions);
    ptMesh->tInfo.uVertexPosOffset = ptMesh->tMesh.uVertexOffset;
    
    // copy data to global buffer
    pl_sb_add_n(ptScene->sbuIndexData, ptMesh->tMesh.uIndexCount);
    pl_sb_add_n(ptScene->sbtVertexData, ptMesh->tMesh.uVertexCount);
    memcpy(&ptScene->sbuIndexData[ptMesh->tMesh.uIndexOffset], ptMesh->sbuIndices, sizeof(uint32_t) * ptMesh->tMesh.uIndexCount);
    memcpy(&ptScene->sbtVertexData[ptMesh->tMesh.uVertexOffset], ptMesh->sbtVertexPositions, sizeof(plVec3) * ptMesh->tMesh.uVertexCount); 
}

static void
pl__add_secondary_mesh_vertex_data(plScene* ptScene, plMeshComponent* ptMesh)
{
    // update global vertex buffer offset
    ptMesh->tInfo.uVertexDataOffset = ptScene->uGlobalVtxDataOffset / 4;

    // stride within storage buffer
    uint32_t uStride = 0;

    // calculate vertex stream mask based on provided data
    if(pl_sb_size(ptMesh->sbtVertexNormals) > 0)             { uStride += 4; ptMesh->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_NORMAL; }
    if(pl_sb_size(ptMesh->sbtVertexTangents) > 0)            { uStride += 4; ptMesh->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TANGENT; }
    if(pl_sb_size(ptMesh->sbtVertexColors0) > 0)             { uStride += 4; ptMesh->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_COLOR_0; }
    if(pl_sb_size(ptMesh->sbtVertexColors1) > 0)             { uStride += 4; ptMesh->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_COLOR_1; }
    if(pl_sb_size(ptMesh->sbtVertexWeights0) > 0)            { uStride += 4; ptMesh->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0; }
    if(pl_sb_size(ptMesh->sbtVertexWeights1) > 0)            { uStride += 4; ptMesh->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1; }
    if(pl_sb_size(ptMesh->sbtVertexJoints0) > 0)             { uStride += 4; ptMesh->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_JOINTS_0; }
    if(pl_sb_size(ptMesh->sbtVertexJoints1) > 0)             { uStride += 4; ptMesh->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_JOINTS_1; }
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates0) > 0) { uStride += 4; ptMesh->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0; }
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates1) > 0) { uStride += 4; ptMesh->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1; }

    pl_sb_add_n(ptScene->sbfGlobalVertexData, uStride * ptMesh->tMesh.uVertexCount);

    // current attribute offset
    uint32_t uOffset = 0;

    // normals
    for(uint32_t i = 0; i < pl_sb_size(ptMesh->sbtVertexNormals); i++)
    {
        ptMesh->sbtVertexNormals[i] = pl_norm_vec3(ptMesh->sbtVertexNormals[i]);
        const plVec3* ptNormal = &ptMesh->sbtVertexNormals[i];
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + 0] = ptNormal->x;
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + 1] = ptNormal->y;
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + 2] = ptNormal->z;
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + 3] = 0.0f;
    }

    if(ptMesh->tMesh.ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_NORMAL)
        uOffset += 4;

    // tangents
    for(uint32_t i = 0; i < pl_sb_size(ptMesh->sbtVertexTangents); i++)
    {
        const plVec4* ptTangent = &ptMesh->sbtVertexTangents[i];
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 0] = ptTangent->x;
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 1] = ptTangent->y;
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 2] = ptTangent->z;
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 3] = ptTangent->w;
    }

    if(pl_sb_size(ptMesh->sbtVertexTangents) > 0)
        uOffset += 4;

    // texture coordinates 0
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates0) > 0)
    {
        for(uint32_t i = 0; i < ptMesh->tMesh.uVertexCount; i++)
        {
            const plVec2* ptTextureCoordinates = &ptMesh->sbtVertexTextureCoordinates0[i];
            ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 0] = ptTextureCoordinates->u;
            ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 1] = ptTextureCoordinates->v;
            ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 2] = 0.0f;
            ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 3] = 0.0f;

        }
        uOffset += 4;
    }

    // texture coordinates 1
    for(uint32_t i = 0; i < pl_sb_size(ptMesh->sbtVertexTextureCoordinates1); i++)
    {
        const plVec2* ptTextureCoordinates = &ptMesh->sbtVertexTextureCoordinates1[i];
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 0] = ptTextureCoordinates->u;
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 1] = ptTextureCoordinates->v;
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 2] = 0.0f;
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 3] = 0.0f;
    }

    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates1) > 0)
        uOffset += 4;

    // color 0
    for(uint32_t i = 0; i < pl_sb_size(ptMesh->sbtVertexColors0); i++)
    {
        const plVec4* ptColor = &ptMesh->sbtVertexColors0[i];
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 0] = ptColor->r;
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 1] = ptColor->g;
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 2] = ptColor->b;
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 3] = ptColor->a;
    }

    if(pl_sb_size(ptMesh->sbtVertexColors0) > 0)
        uOffset += 4;

    // color 1
    for(uint32_t i = 0; i < pl_sb_size(ptMesh->sbtVertexColors1); i++)
    {
        const plVec4* ptColor = &ptMesh->sbtVertexColors1[i];
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 0] = ptColor->r;
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 1] = ptColor->g;
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 2] = ptColor->b;
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 3] = ptColor->a;
    }

    if(pl_sb_size(ptMesh->sbtVertexColors1) > 0)
        uOffset += 4;

    // joints 0
    for(uint32_t i = 0; i < pl_sb_size(ptMesh->sbtVertexJoints0); i++)
    {
        const plVec4* ptJoint = &ptMesh->sbtVertexJoints0[i];
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 0] = ptJoint->x;
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 1] = ptJoint->y;
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 2] = ptJoint->z;
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 3] = ptJoint->w;
    }

    if(pl_sb_size(ptMesh->sbtVertexJoints0) > 0)
        uOffset += 4;

    // joints 1
    for(uint32_t i = 0; i < pl_sb_size(ptMesh->sbtVertexJoints1); i++)
    {
        const plVec4* ptJoint = &ptMesh->sbtVertexJoints1[i];
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 0] = ptJoint->x;
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 1] = ptJoint->y;
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 2] = ptJoint->z;
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 3] = ptJoint->w;
    }

    if(pl_sb_size(ptMesh->sbtVertexJoints1) > 0)
        uOffset += 4;

    // weights 0
    for(uint32_t i = 0; i < pl_sb_size(ptMesh->sbtVertexWeights0); i++)
    {
        const plVec4* ptWeight = &ptMesh->sbtVertexWeights0[i];
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 0] = ptWeight->x;
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 1] = ptWeight->y;
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 2] = ptWeight->z;
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 3] = ptWeight->w;
    }

    if(pl_sb_size(ptMesh->sbtVertexWeights0) > 0)
        uOffset += 4;

    // weights 1
    for(uint32_t i = 0; i < pl_sb_size(ptMesh->sbtVertexWeights1); i++)
    {
        const plVec4* ptWeight = &ptMesh->sbtVertexWeights1[i];
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 0] = ptWeight->x;
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 1] = ptWeight->y;
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 2] = ptWeight->z;
        ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 3] = ptWeight->w;
    }

    if(pl_sb_size(ptMesh->sbtVertexWeights1) > 0)
        uOffset += 4;

    PL_ASSERT(uOffset == uStride && "sanity check");

    ptScene->uGlobalVtxDataOffset += uStride * ptMesh->tMesh.uVertexCount;

    // update material vertex stream to match actual mesh
    const plEcsI* ptEcs = ptScene->ptRenderer->ptEcs;
    plMaterialComponent* ptMaterialComponent = ptEcs->get_component(&ptScene->ptComponentLibrary->tMaterialComponentManager, ptMesh->tMaterial);
    ptMaterialComponent->tGraphicsState.ulVertexStreamMask = ptMesh->tMesh.ulVertexStreamMask;
}

static void
pl__update_global_buffers(plScene* ptScene)
{
    // for convience
    plGraphics*           ptGraphics  = ptScene->ptRenderer->ptGraphics;
    plDevice*             ptDevice    = &ptGraphics->tDevice;
    const plDeviceApiI*   ptDeviceApi = ptGraphics->ptDeviceApi;
    plRenderer*           ptRenderer  = ptScene->ptRenderer;
    const plGraphicsApiI* ptGfx       = ptRenderer->ptGfx;

    // submit old global buffers for deletion
    if(ptScene->uGlobalVertexData   != UINT32_MAX) ptDeviceApi->submit_buffer_for_deletion(ptDevice, ptScene->uGlobalVertexData);
    if(ptScene->uGlobalMaterialData != UINT32_MAX) ptDeviceApi->submit_buffer_for_deletion(ptDevice, ptScene->uGlobalMaterialData);
    if(ptScene->uGlobalPickData     != UINT32_MAX) ptDeviceApi->submit_buffer_for_deletion(ptDevice, ptScene->uGlobalPickData);

    // create new storage global buffers
    ptScene->uGlobalVertexData   = ptDeviceApi->create_storage_buffer(ptDevice, pl_sb_size(ptScene->sbfGlobalVertexData) * sizeof(float), ptScene->sbfGlobalVertexData, "global vertex data");
    ptScene->uGlobalMaterialData = ptDeviceApi->create_storage_buffer(ptDevice, pl_sb_size(ptScene->sbtGlobalMaterialData) * sizeof(plMaterialInfo), ptScene->sbtGlobalMaterialData, "global material data");
    ptScene->uGlobalPickData     = ptDeviceApi->create_storage_buffer(ptDevice, pl_sb_size(ptScene->sbtGlobalPickData) * sizeof(plPickInfo), ptScene->sbtGlobalPickData, "global pick data");

    // update global bind group
    uint32_t atBuffers0[] = {ptScene->uDynamicBuffer0, ptScene->uGlobalVertexData, ptScene->uGlobalMaterialData};
    size_t aszRangeSizes[] = {sizeof(plGlobalInfo), VK_WHOLE_SIZE, VK_WHOLE_SIZE};
    ptGfx->update_bind_group(ptGraphics, &ptScene->tGlobalBindGroup, 3, atBuffers0, aszRangeSizes, 0, NULL);

    // update global picking bind group
    uint32_t atBuffers1[] = {ptScene->uDynamicBuffer0, ptScene->uGlobalPickData};
    size_t aszRangeSizes1[] = {sizeof(plGlobalInfo), VK_WHOLE_SIZE};
    ptGfx->update_bind_group(ptGraphics, &ptScene->tGlobalPickBindGroup, 2, atBuffers1, aszRangeSizes1, 0, NULL);

    // create new global index buffer
    ptScene->uIndexBuffer = ptDeviceApi->create_index_buffer(ptDevice, 
        sizeof(uint32_t) * pl_sb_size(ptScene->sbuIndexData),
        ptScene->sbuIndexData, "global index buffer");

    // create new global vertex buffer
    ptScene->uVertexBuffer = ptDeviceApi->create_vertex_buffer(ptDevice, 
        sizeof(plVec3) * pl_sb_size(ptScene->sbtVertexData), sizeof(plVec3),
        ptScene->sbtVertexData, "global vertex buffer");

    // reset global cpu side buffers
    pl_sb_reset(ptScene->sbfGlobalVertexData);
    pl_sb_reset(ptScene->sbtGlobalMaterialData);
    pl_sb_reset(ptScene->sbtGlobalPickData);
    pl_sb_reset(ptScene->sbuIndexData);
    pl_sb_reset(ptScene->sbtVertexData);
}

static void
pl_scene_prepare(plScene* ptScene)
{

    if(!ptScene->bMeshesNeedUpdate)
        return;

    pl_begin_profile_sample(__FUNCTION__);
    ptScene->bMeshesNeedUpdate = false;
    plGraphics*           ptGraphics  = ptScene->ptRenderer->ptGraphics;
    plRenderer*           ptRenderer  = ptScene->ptRenderer;
    const plGraphicsApiI* ptGfx       = ptRenderer->ptGfx;
    const plDeviceApiI*   ptDeviceApi = ptGraphics->ptDeviceApi;
    plDevice*             ptDevice    = &ptGraphics->tDevice;
    const plEcsI*         ptEcs       = ptScene->ptRenderer->ptEcs;

    plMeshComponent* sbtMeshes = ptScene->ptComponentLibrary->tMeshComponentManager.pComponents;

    const uint32_t uMeshCount = pl_sb_size(sbtMeshes);
    for(uint32_t uMeshIndex = 0; uMeshIndex < uMeshCount; uMeshIndex++)
    {
        plMeshComponent* ptMesh = &sbtMeshes[uMeshIndex];

        plMaterialComponent* ptMaterial = ptEcs->get_component(&ptScene->ptComponentLibrary->tMaterialComponentManager, ptMesh->tMaterial);

        // update material data
        const plMaterialInfo tMaterialInfo = {
            .tAlbedo = ptMaterial->tAlbedo
        };
        pl_sb_push(ptScene->sbtGlobalMaterialData, tMaterialInfo);
        ptMesh->tInfo.uMaterialIndex = pl_sb_size(ptScene->sbtGlobalMaterialData) - 1;
        
        // update pick data
        const plPickInfo tPickInfo = {
            .tColor = ptEcs->entity_to_color(ptScene->ptComponentLibrary->tMeshComponentManager.sbtEntities[uMeshIndex])
        };
        pl_sb_push(ptScene->sbtGlobalPickData, tPickInfo);
        
        // add mesh to global buffers
        pl__add_primary_mesh_data(ptScene, ptMesh);
        pl__add_secondary_mesh_vertex_data(ptScene, ptMesh);
    }

    // update cpu buffer and upload to cpu buffers
    pl__update_global_buffers(ptScene);

    pl_end_profile_sample();
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
    ptGlobalInfo->fTime           = (float)pl_get_io_context()->dTime;

    plLightComponent* sbtLights = ptScene->ptComponentLibrary->tLightComponentManager.pComponents;
    if(pl_sb_size(sbtLights) > 0)
    {
        ptGlobalInfo->tLightPos = (plVec4){.x = sbtLights[0].tPosition.x, .y = sbtLights[0].tPosition.y, .z = sbtLights[0].tPosition.z};
        ptGlobalInfo->tLightColor = (plVec4){.rgb = sbtLights[0].tColor};
    }
}

static void
pl_draw_sky(plScene* ptScene)
{
    pl_begin_profile_sample(__FUNCTION__);
    plGraphics* ptGraphics = ptScene->ptRenderer->ptGraphics;
    plRenderer* ptRenderer = ptScene->ptRenderer;
    const plGraphicsApiI* ptGfx = ptRenderer->ptGfx;
    const plEcsI* ptEcs = ptRenderer->ptEcs;
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
            && ptScene->ptRenderTarget->tDesc.uRenderPass == ptShader->tDesc.sbtVariants[j].uRenderPass)
        {
                uSkyboxShaderVariant = ptShader->_sbuVariantPipelines[j];
                break;
        }
    }

    // create variant that matches texture count, vertex stream, and culling
    if(uSkyboxShaderVariant == UINT32_MAX)
    {
        pl_log_debug_to_f(uLogChannel, "adding skybox shader variant");
        uSkyboxShaderVariant = ptGfx->add_shader_variant(ptGraphics, ptRenderer->uSkyboxShader, tFillStateTemplate, ptScene->ptRenderTarget->tDesc.uRenderPass, tMSAASampleCount);
    }

    pl_sb_push(ptRenderer->sbtDrawAreas, ((plDrawArea){
        .ptBindGroup0          = &ptScene->tSkyboxBindGroup0,
        .uDrawOffset           = pl_sb_size(ptRenderer->sbtDraws),
        .uDrawCount            = 1,
        .uDynamicBufferOffset0 = uBufferFrameOffset0
    }));

    pl_sb_push(ptRenderer->sbtDraws, ((plDraw){
        .uShaderVariant        = uSkyboxShaderVariant,
        .ptMesh                = &ptScene->tSkyboxMesh,
        .aptBindGroups         = { NULL, NULL },
        .auDynamicBufferOffset = {0, 0}
        }));

    ptGfx->draw_areas(ptRenderer->ptGraphics, pl_sb_size(ptRenderer->sbtDrawAreas), ptRenderer->sbtDrawAreas, ptRenderer->sbtDraws);

    pl_sb_reset(ptRenderer->sbtDraws);
    pl_sb_reset(ptRenderer->sbtDrawAreas);

    ptScene->uDynamicBuffer0_Offset = (uint32_t)pl_align_up((size_t)ptScene->uDynamicBuffer0_Offset + sizeof(plGlobalInfo), ptGraphics->tDevice.tDeviceProps.limits.minUniformBufferOffsetAlignment);
    pl_end_profile_sample();
}

static void
pl__prepare_material_gpu_data(plScene* ptScene, plComponentManager* ptManager)
{
    pl_begin_profile_sample(__FUNCTION__);
    plRenderer* ptRenderer = ptScene->ptRenderer;
    plGraphics* ptGraphics = ptRenderer->ptGraphics;
    const plGraphicsApiI* ptGfx = ptRenderer->ptGfx;
    const plDeviceApiI* ptDeviceApi = ptGraphics->ptDeviceApi;
    plDevice* ptDevice = &ptGraphics->tDevice;

    VkSampleCountFlagBits tMSAASampleCount = ptScene->ptRenderTarget->bMSAA ? ptGraphics->tSwapchain.tMsaaSamples : VK_SAMPLE_COUNT_1_BIT;

    plMaterialComponent* sbtComponents = ptManager->pComponents;
    for(uint32_t i = 0; i < pl_sb_size(sbtComponents); i++)
    {

        plMaterialComponent* ptMaterial = &sbtComponents[i];

        const uint32_t acShaderLookup[] = {
            ptScene->ptRenderer->uMainShader,
            ptScene->ptRenderer->uOutlineShader,
            ptMaterial->uShader
        };

        ptMaterial->uShader = acShaderLookup[ptMaterial->tShaderType];
        uint32_t sbuTextures[16] = {0};
        sbuTextures[0] = ptMaterial->uAlbedoMap;
        sbuTextures[1] = ptMaterial->uNormalMap;
        sbuTextures[2] = ptMaterial->uEmissiveMap;

        if(ptScene->bMaterialsNeedUpdate)
        {
            uint64_t uHashKey = pl_hm_hash(&ptMaterial->uShader, sizeof(uint32_t), 0);
            uHashKey = pl_hm_hash(&ptMaterial->uAlbedoMap, sizeof(uint32_t), uHashKey);
            uHashKey = pl_hm_hash(&ptMaterial->uNormalMap, sizeof(uint32_t), uHashKey);
            uHashKey = pl_hm_hash(&ptMaterial->uEmissiveMap, sizeof(uint32_t), uHashKey);
            uHashKey = pl_hm_hash(&ptMaterial->tGraphicsState, sizeof(uint64_t), uHashKey);

            // check if bind group for this buffer exist
            uint64_t uMaterialBindGroupIndex = pl_hm_lookup(&ptRenderer->tMaterialBindGroupdHashMap, uHashKey);

            if(uMaterialBindGroupIndex == UINT64_MAX) // doesn't exist
            {

                plBindGroup tNewBindGroup = {
                    .tLayout = *ptGfx->get_bind_group_layout(ptGraphics, ptMaterial->uShader, 1)
                };
                ptGfx->update_bind_group(ptGraphics, &tNewBindGroup, 0, NULL, NULL, 3, sbuTextures);

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
                && ptShader->tDesc.sbtVariants[k].uRenderPass == ptScene->ptRenderTarget->tDesc.uRenderPass
                && tMSAASampleCount == ptShader->tDesc.sbtVariants[k].tMSAASampleCount)
            {
                    ptMaterial->uShaderVariant = ptShader->_sbuVariantPipelines[k];
                    break;
            }
        }

        // create variant that matches texture count, vertex stream, and culling
        if(ptMaterial->uShaderVariant == UINT32_MAX)
        {
            ptMaterial->uShaderVariant = ptGfx->add_shader_variant(ptGraphics, ptMaterial->uShader, ptMaterial->tGraphicsState, ptScene->ptRenderTarget->tDesc.uRenderPass, tMSAASampleCount);
        }
    }

    ptScene->bMaterialsNeedUpdate = false;
    pl_end_profile_sample();
}

static void
pl__prepare_object_gpu_data(plScene* ptScene, plComponentManager* ptManager)
{
    plRenderer* ptRenderer = ptScene->ptRenderer;
    plGraphics* ptGraphics = ptRenderer->ptGraphics;
    const plGraphicsApiI* ptGfx = ptRenderer->ptGfx;
    const plDeviceApiI* ptDeviceApi = ptGraphics->ptDeviceApi;
    plDevice* ptDevice = &ptGraphics->tDevice;

    plObjectSystemData* ptObjectSystemData = ptManager->pSystemData;
    if(!ptObjectSystemData->bDirty)
        return;

    pl_begin_profile_sample(__FUNCTION__);

    const size_t szSizeWithPadding = pl_align_up(sizeof(plObjectInfo), ptGraphics->tDevice.tDeviceProps.limits.minUniformBufferOffsetAlignment);
    plObjectComponent* sbtComponents = ptManager->pComponents;

    const uint32_t uMaxObjectsPerBuffer = (uint32_t)(ptDevice->uUniformBufferBlockSize / (uint32_t)szSizeWithPadding) - 1;
    uint32_t uMeshCount = pl_sb_size(ptObjectSystemData->sbtMeshes);

    const uint32_t uMinBuffersNeeded = (uint32_t)ceilf((float)uMeshCount / (float)uMaxObjectsPerBuffer);
    uint32_t uCurrentObject = 0;

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

        uint32_t uIterationObjectCount = pl_minu(uMaxObjectsPerBuffer, uMeshCount);
        for(uint32_t j = 0; j < uIterationObjectCount; j++)
        {
            ptObjectSystemData->sbtMeshes[uCurrentObject]->tMesh.uIndexBuffer = ptScene->uIndexBuffer;
            ptObjectSystemData->sbtMeshes[uCurrentObject]->tMesh.uVertexBuffer = ptScene->uVertexBuffer;
            plObjectInfo* ptObjectInfo = (plObjectInfo*)(ptBuffer->tAllocation.pHostMapped + ptDynamicBufferNode->uDynamicBufferOffset);
            *ptObjectInfo = ptObjectSystemData->sbtMeshes[uCurrentObject]->tInfo;
            ptObjectSystemData->sbtMeshes[uCurrentObject]->uBindGroup2 = uObjectBindGroupIndex;
            ptObjectSystemData->sbtMeshes[uCurrentObject]->uBufferOffset = ptDynamicBufferNode->uDynamicBufferOffset;
            ptDynamicBufferNode->uDynamicBufferOffset += (uint32_t)szSizeWithPadding;
            uCurrentObject++;
        }
        uMeshCount = uMeshCount - uIterationObjectCount;

        pl_sb_push(ptDevice->_sbuDynamicBufferDeletionQueue, uDynamicBufferIndex);

        if(uMeshCount == 0)
            break;
    }
    ptObjectSystemData->bDirty = false;
    pl_end_profile_sample();
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_renderer_ext(plApiRegistryApiI* ptApiRegistry, bool bReload)
{
    const plDataRegistryApiI* ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(ptDataRegistry->get_data(PL_CONTEXT_MEMORY));
    pl_set_profile_context(ptDataRegistry->get_data("profile"));
    pl_set_log_context(ptDataRegistry->get_data("log"));
    pl_set_io_context(ptDataRegistry->get_data(PL_CONTEXT_IO_NAME));

    if(bReload)
    {
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_RENDERER), pl_load_renderer_api());

        // find log channel
        uint32_t uChannelCount = 0;
        plLogChannel* ptChannels = pl_get_log_channels(&uChannelCount);
        for(uint32_t i = 0; i < uChannelCount; i++)
        {
            if(strcmp(ptChannels[i].pcName, "Renderer") == 0)
            {
                uLogChannel = i;
                break;
            }
        }
    }
    else
    {
        ptApiRegistry->add(PL_API_RENDERER, pl_load_renderer_api());
        uLogChannel = pl_add_log_channel("Renderer", PL_CHANNEL_TYPE_CYCLIC_BUFFER);
    }
}

PL_EXPORT void
pl_unload_renderer_ext(plApiRegistryApiI* ptApiRegistry)
{
    
}