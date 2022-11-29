/*
   vulkan_app.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] pl_app_load
// [SECTION] pl_app_setup
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <string.h> // memset
#include "pilotlight.h"
#include "pl_graphics_vulkan.h"
#include "pl_profile.h"
#include "pl_log.h"
#include "pl_ds.h"
#include "pl_io.h"
#include "pl_memory.h"
#include "pl_draw_vulkan.h"
#include "pl_math.h"
#include "pl_registry.h" // data registry
#include "pl_ext.h"      // extension registry

// extensions
#include "pl_draw_extension.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAppData
{
    plVulkanDevice      device;
    plVulkanGraphics    graphics;
    plVulkanSwapchain   swapchain;
    plDrawContext       ctx;
    plDrawList          drawlist;
    plDrawLayer*        fgDrawLayer;
    plDrawLayer*        bgDrawLayer;
    plFontAtlas         fontAtlas;
    plProfileContext    tProfileCtx;
    plLogContext        tLogCtx;
    plMemoryContext     tMemoryCtx;
    plDataRegistry      tDataRegistryCtx;
    plExtensionRegistry tExtensionRegistryCtx;

    // extension apis
    plDrawExtension*    ptDrawExtApi;
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plIOContext* ptIOCtx, plAppData* ptAppData)
{
    if(ptAppData) // reload
    {
        pl_set_log_context(&ptAppData->tLogCtx);
        pl_set_profile_context(&ptAppData->tProfileCtx);
        pl_set_memory_context(&ptAppData->tMemoryCtx);
        pl_set_data_registry(&ptAppData->tDataRegistryCtx);
        pl_set_extension_registry(&ptAppData->tExtensionRegistryCtx);
        pl_set_io_context(ptIOCtx);

        plExtension* ptExtension = pl_get_extension(PL_EXT_DRAW);
        ptAppData->ptDrawExtApi = pl_get_api(ptExtension, PL_EXT_API_DRAW);

        return ptAppData;
    }

    plAppData* tPNewData = malloc(sizeof(plAppData));
    memset(tPNewData, 0, sizeof(plAppData));

    pl_set_io_context(ptIOCtx);

    // setup memory context
    pl_initialize_memory_context(&tPNewData->tMemoryCtx);

    // setup profiling context
    pl_initialize_profile_context(&tPNewData->tProfileCtx);

    // setup data registry
    pl_initialize_data_registry(&tPNewData->tDataRegistryCtx);

    // setup logging
    pl_initialize_log_context(&tPNewData->tLogCtx);
    pl_add_log_channel("Default", PL_CHANNEL_TYPE_CONSOLE);
    pl_log_info(0, "Setup logging");

    // setup extension registry
    pl_initialize_extension_registry(&tPNewData->tExtensionRegistryCtx);
    pl_register_data("memory", &tPNewData->tMemoryCtx);
    pl_register_data("profile", &tPNewData->tProfileCtx);
    pl_register_data("log", &tPNewData->tLogCtx);
    pl_register_data("io", ptIOCtx);

    plExtension tExtension = {0};
    pl_get_draw_extension_info(&tExtension);
    pl_load_extension(&tExtension);

    plExtension* ptExtension = pl_get_extension(PL_EXT_DRAW);
    tPNewData->ptDrawExtApi = pl_get_api(ptExtension, PL_EXT_API_DRAW);

    return tPNewData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_setup
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_setup(plAppData* ptAppData)
{
    // get io context
    plIOContext* ptIOCtx = pl_get_io_context();

    // create vulkan tInstance
    pl_create_instance(&ptAppData->graphics, VK_API_VERSION_1_2, true);

    // create tSurface
    #ifdef _WIN32
        VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .pNext = NULL,
            .flags = 0,
            .hinstance = GetModuleHandle(NULL),
            .hwnd = *(HWND*)ptIOCtx->pBackendPlatformData
        };
        PL_VULKAN(vkCreateWin32SurfaceKHR(ptAppData->graphics.tInstance, &surfaceCreateInfo, NULL, &ptAppData->graphics.tSurface));
    #elif defined(__APPLE__)
        VkMetalSurfaceCreateInfoEXT tSurfaceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
            .pLayer = (CAMetalLayer*)ptIOCtx->pBackendPlatformData
        };
        PL_VULKAN(vkCreateMetalSurfaceEXT(ptAppData->graphics.tInstance, &tSurfaceCreateInfo, NULL, &ptAppData->graphics.tSurface));
    #else // linux
        struct tPlatformData { xcb_connection_t* ptConnection; xcb_window_t tWindow;};
        struct tPlatformData* ptPlatformData = (struct tPlatformData*)ptIOCtx->pBackendPlatformData;
        VkXcbSurfaceCreateInfoKHR surfaceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
            .pNext = NULL,
            .flags = 0,
            .window = ptPlatformData->tWindow,
            .connection = ptPlatformData->ptConnection
        };
        PL_VULKAN(vkCreateXcbSurfaceKHR(ptAppData->graphics.tInstance, &surfaceCreateInfo, NULL, &ptAppData->graphics.tSurface));
    #endif

    // create devices
    pl_create_device(ptAppData->graphics.tInstance, ptAppData->graphics.tSurface, &ptAppData->device, true);
    
    // create swapchain
    ptAppData->swapchain.bVSync = true;
    pl_create_swapchain(&ptAppData->device, ptAppData->graphics.tSurface, (uint32_t)ptIOCtx->afMainViewportSize[0], (uint32_t)ptIOCtx->afMainViewportSize[1], &ptAppData->swapchain);

    // create render pass
    VkAttachmentDescription atAttachments[] = {

        // color attachment
        {
            .flags          = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT,
            .format         = ptAppData->swapchain.tFormat,
            .samples        = ptAppData->swapchain.tMsaaSamples,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        },

        // depth attachment
        {
            .format         = pl_find_depth_format(&ptAppData->device),
            .samples        = ptAppData->swapchain.tMsaaSamples,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        },

        // color resolve attachment
        {
            .flags          = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT,
            .format         = ptAppData->swapchain.tFormat,
            .samples        = VK_SAMPLE_COUNT_1_BIT,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        },
    };

    VkSubpassDependency tSubpassDependencies[] = {

        {
            .srcSubpass      = VK_SUBPASS_EXTERNAL,
            .dstSubpass      = 0,
            .srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask   = 0,
            .dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
        },
        {
            .srcSubpass      = VK_SUBPASS_EXTERNAL,
            .dstSubpass      = 0,
            .srcStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .srcAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dstAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
        }
    };

    VkAttachmentReference atAttachmentReferences[] = {
        {
            .attachment = 0,
            .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        },
        {
            .attachment = 1,
            .layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL     
        },
        {
            .attachment = 2,
            .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL     
        }
    };

    VkSubpassDescription tSubpass = {
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount    = 1,
        .pColorAttachments       = &atAttachmentReferences[0],
        .pDepthStencilAttachment = &atAttachmentReferences[1],
        .pResolveAttachments     = &atAttachmentReferences[2]
    };

    VkRenderPassCreateInfo tRenderPassInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 3,
        .pAttachments    = atAttachments,
        .subpassCount    = 1,
        .pSubpasses      = &tSubpass,
        .dependencyCount = 2,
        .pDependencies   = tSubpassDependencies
    };
    PL_VULKAN(vkCreateRenderPass(ptAppData->device.tLogicalDevice, &tRenderPassInfo, NULL, &ptAppData->graphics.tRenderPass));

    // create frame buffers
    pl_create_framebuffers(&ptAppData->device, ptAppData->graphics.tRenderPass, &ptAppData->swapchain);
    
    // create per frame resources
    pl_create_frame_resources(&ptAppData->graphics, &ptAppData->device);

    VkCommandBuffer tCommandBuffer = pl_begin_command_buffer(&ptAppData->graphics, &ptAppData->device);
    const VkImageSubresourceRange tRange = {
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1,
        .aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT
    };
    pl_transition_image_layout(tCommandBuffer, ptAppData->swapchain.tDepthImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, tRange, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT , VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
    pl_submit_command_buffer(&ptAppData->graphics, &ptAppData->device, tCommandBuffer);
    
    // setup drawing api
    pl_initialize_draw_context_vulkan(&ptAppData->ctx, ptAppData->device.tPhysicalDevice, 3, ptAppData->device.tLogicalDevice);
    pl_register_drawlist(&ptAppData->ctx, &ptAppData->drawlist);
    pl_setup_drawlist_vulkan(&ptAppData->drawlist, ptAppData->graphics.tRenderPass, ptAppData->swapchain.tMsaaSamples);
    ptAppData->bgDrawLayer = pl_request_draw_layer(&ptAppData->drawlist, "Background Layer");
    ptAppData->fgDrawLayer = pl_request_draw_layer(&ptAppData->drawlist, "Foreground Layer");

    // create font atlas
    pl_add_default_font(&ptAppData->fontAtlas);
    pl_build_font_atlas(&ptAppData->ctx, &ptAppData->fontAtlas);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    // ensure device is finished
    vkDeviceWaitIdle(ptAppData->device.tLogicalDevice);

    if(ptAppData->swapchain.tDepthImageView) vkDestroyImageView(ptAppData->device.tLogicalDevice, ptAppData->swapchain.tDepthImageView, NULL);
    if(ptAppData->swapchain.tDepthImage)     vkDestroyImage(ptAppData->device.tLogicalDevice, ptAppData->swapchain.tDepthImage, NULL);
    if(ptAppData->swapchain.tDepthMemory)    vkFreeMemory(ptAppData->device.tLogicalDevice, ptAppData->swapchain.tDepthMemory, NULL);

    if(ptAppData->swapchain.tColorImageView) vkDestroyImageView(ptAppData->device.tLogicalDevice, ptAppData->swapchain.tColorImageView, NULL);
    if(ptAppData->swapchain.tColorImage)     vkDestroyImage(ptAppData->device.tLogicalDevice, ptAppData->swapchain.tColorImage, NULL);
    if(ptAppData->swapchain.tColorMemory)    vkFreeMemory(ptAppData->device.tLogicalDevice, ptAppData->swapchain.tColorMemory, NULL);

    // destroy swapchain
    for (uint32_t i = 0u; i < ptAppData->swapchain.uImageCount; i++)
    {
        vkDestroyImageView(ptAppData->device.tLogicalDevice, ptAppData->swapchain.ptImageViews[i], NULL);
        vkDestroyFramebuffer(ptAppData->device.tLogicalDevice, ptAppData->swapchain.ptFrameBuffers[i], NULL);
    }

    // destroy default render pass
    vkDestroyRenderPass(ptAppData->device.tLogicalDevice, ptAppData->graphics.tRenderPass, NULL);
    vkDestroySwapchainKHR(ptAppData->device.tLogicalDevice, ptAppData->swapchain.tSwapChain, NULL);

    // clean up contexts
    pl_cleanup_font_atlas(&ptAppData->fontAtlas);
    pl_cleanup_draw_context(&ptAppData->ctx);
    pl_cleanup_graphics(&ptAppData->graphics, &ptAppData->device);
    pl_cleanup_profile_context();
    pl_cleanup_extension_registry();
    pl_cleanup_log_context();
    pl_cleanup_data_registry();
    pl_cleanup_memory_context();
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plAppData* ptAppData)
{
    // get io context
    plIOContext* ptIOCtx = pl_get_io_context();

    pl_create_swapchain(&ptAppData->device, ptAppData->graphics.tSurface, (uint32_t)ptIOCtx->afMainViewportSize[0], (uint32_t)ptIOCtx->afMainViewportSize[1], &ptAppData->swapchain);
    pl_create_framebuffers(&ptAppData->device, ptAppData->graphics.tRenderPass, &ptAppData->swapchain);

    VkCommandBuffer tCommandBuffer = pl_begin_command_buffer(&ptAppData->graphics, &ptAppData->device);
    const VkImageSubresourceRange tRange = {
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1,
        .aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT
    };
    pl_transition_image_layout(tCommandBuffer, ptAppData->swapchain.tDepthImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, tRange, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT , VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
    pl_submit_command_buffer(&ptAppData->graphics, &ptAppData->device, tCommandBuffer);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    pl_handle_extension_reloads();

    pl_new_io_frame();
    
    // get io context
    plIOContext* ptIOCtx = pl_get_io_context();

    pl_new_draw_frame(&ptAppData->ctx);

    // begin profiling frame (temporarily using drawing context frame count)
    pl__begin_profile_frame(ptAppData->ctx.frameCount);

    plVulkanFrameContext* currentFrame = pl_get_frame_resources(&ptAppData->graphics);

    // begin frame
    PL_VULKAN(vkWaitForFences(ptAppData->device.tLogicalDevice, 1, &currentFrame->tInFlight, VK_TRUE, UINT64_MAX));
    VkResult err = vkAcquireNextImageKHR(ptAppData->device.tLogicalDevice, ptAppData->swapchain.tSwapChain, UINT64_MAX, currentFrame->tImageAvailable, VK_NULL_HANDLE, &ptAppData->swapchain.uCurrentImageIndex);
    if(err == VK_SUBOPTIMAL_KHR || err == VK_ERROR_OUT_OF_DATE_KHR)
    {
        if(err == VK_ERROR_OUT_OF_DATE_KHR)
        {
            pl_create_swapchain(&ptAppData->device, ptAppData->graphics.tSurface, (uint32_t)ptIOCtx->afMainViewportSize[0], (uint32_t)ptIOCtx->afMainViewportSize[1], &ptAppData->swapchain);
            pl_create_framebuffers(&ptAppData->device, ptAppData->graphics.tRenderPass, &ptAppData->swapchain);
            return;
        }
    }
    else
    {
        PL_VULKAN(err);
    }

    if (currentFrame->tInFlight != VK_NULL_HANDLE)
        PL_VULKAN(vkWaitForFences(ptAppData->device.tLogicalDevice, 1, &currentFrame->tInFlight, VK_TRUE, UINT64_MAX));

    // begin recording
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    };
    PL_VULKAN(vkBeginCommandBuffer(currentFrame->tCmdBuf, &beginInfo));

    // begin render pass

    static const VkClearValue clearValues[2] = 
    {
        {
            .color.float32[0] = 0.1f,
            .color.float32[1] = 0.0f,
            .color.float32[2] = 0.0f,
            .color.float32[3] = 1.0f
        },
        {
            .depthStencil.depth = 1.0f,
            .depthStencil.stencil = 0
        }    
    };

    VkRenderPassBeginInfo renderPassBeginInfo = {
        .sType               = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass          = ptAppData->graphics.tRenderPass,
        .framebuffer         = ptAppData->swapchain.ptFrameBuffers[ptAppData->swapchain.uCurrentImageIndex],
        .renderArea.offset.x = 0,
        .renderArea.offset.y = 0,
        .renderArea.extent   = ptAppData->swapchain.tExtent,
        .clearValueCount     = 2,
        .pClearValues        = clearValues
    };
    vkCmdBeginRenderPass(currentFrame->tCmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // set viewport
    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)ptAppData->swapchain.tExtent.width,
        .height = (float)ptAppData->swapchain.tExtent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    vkCmdSetViewport(currentFrame->tCmdBuf, 0, 1, &viewport);

    // set scissor
    VkRect2D dynamicScissor = {.extent = ptAppData->swapchain.tExtent};
    vkCmdSetScissor(currentFrame->tCmdBuf, 0, 1, &dynamicScissor);

    ptAppData->ptDrawExtApi->pl_add_text(ptAppData->fgDrawLayer, &ptAppData->fontAtlas.sbFonts[0], 13.0f, (plVec2){100.0f, 100.0f}, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, "extension baby");

    // draw profiling info
    pl_begin_profile_sample("Draw Profiling Info");
    static char pcDeltaTime[64] = {0};
    pl_sprintf(pcDeltaTime, "%.6f ms", ptIOCtx->fDeltaTime);
    pl_add_text(ptAppData->fgDrawLayer, &ptAppData->fontAtlas.sbFonts[0], 13.0f, (plVec2){10.0f, 10.0f}, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, pcDeltaTime, 0.0f);
    char cPProfileValue[64] = {0};
    for(uint32_t i = 0u; i < pl_sb_size(ptAppData->tProfileCtx.ptLastFrame->sbtSamples); i++)
    {
        plProfileSample* tPSample = &ptAppData->tProfileCtx.ptLastFrame->sbtSamples[i];
        pl_add_text(ptAppData->fgDrawLayer, &ptAppData->fontAtlas.sbFonts[0], 13.0f, (plVec2){10.0f + (float)tPSample->uDepth * 15.0f, 50.0f + (float)i * 15.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, tPSample->pcName, 0.0f);
        plVec2 sampleTextSize = pl_calculate_text_size(&ptAppData->fontAtlas.sbFonts[0], 13.0f, tPSample->pcName, 0.0f);
        pl_sprintf(cPProfileValue, ": %0.5f", tPSample->dDuration);
        pl_add_text(ptAppData->fgDrawLayer, &ptAppData->fontAtlas.sbFonts[0], 13.0f, (plVec2){sampleTextSize.x + 15.0f + (float)tPSample->uDepth * 15.0f, 50.0f + (float)i * 15.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, cPProfileValue, 0.0f);
    }
    pl_end_profile_sample();

    // draw commands
    pl_begin_profile_sample("Add draw commands");
    pl_add_text(ptAppData->fgDrawLayer, &ptAppData->fontAtlas.sbFonts[0], 13.0f, (plVec2){300.0f, 10.0f}, (plVec4){0.1f, 0.5f, 0.0f, 1.0f}, "Pilot Light\nGraphics", 0.0f);
    pl_add_triangle_filled(ptAppData->bgDrawLayer, (plVec2){300.0f, 50.0f}, (plVec2){300.0f, 150.0f}, (plVec2){350.0f, 50.0f}, (plVec4){1.0f, 0.0f, 0.0f, 1.0f});
    pl__begin_profile_sample("Calculate text size");
    plVec2 textSize = pl_calculate_text_size(&ptAppData->fontAtlas.sbFonts[0], 13.0f, "Pilot Light\nGraphics", 0.0f);
    pl__end_profile_sample();
    pl_add_rect_filled(ptAppData->bgDrawLayer, (plVec2){300.0f, 10.0f}, (plVec2){300.0f + textSize.x, 10.0f + textSize.y}, (plVec4){0.0f, 0.0f, 0.8f, 0.5f});
    pl_add_line(ptAppData->bgDrawLayer, (plVec2){500.0f, 10.0f}, (plVec2){10.0f, 500.0f}, (plVec4){1.0f, 1.0f, 1.0f, 0.5f}, 2.0f);
    pl_end_profile_sample();

    // submit draw layers
    pl_begin_profile_sample("Submit draw layers");
    pl_submit_draw_layer(ptAppData->bgDrawLayer);
    pl_submit_draw_layer(ptAppData->fgDrawLayer);
    pl_end_profile_sample();

    // submit draw lists
    pl_submit_drawlist_vulkan(&ptAppData->drawlist, (float)ptIOCtx->afMainViewportSize[0], (float)ptIOCtx->afMainViewportSize[1], currentFrame->tCmdBuf, (uint32_t)ptAppData->graphics.szCurrentFrameIndex);

    // end render pass
    vkCmdEndRenderPass(currentFrame->tCmdBuf);

    // end recording
    PL_VULKAN(vkEndCommandBuffer(currentFrame->tCmdBuf));

    // submit
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &currentFrame->tImageAvailable,
        .pWaitDstStageMask = waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &currentFrame->tCmdBuf,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &currentFrame->tRenderFinish
    };
    PL_VULKAN(vkResetFences(ptAppData->device.tLogicalDevice, 1, &currentFrame->tInFlight));
    PL_VULKAN(vkQueueSubmit(ptAppData->device.tGraphicsQueue, 1, &submitInfo, currentFrame->tInFlight));          
    
    // present                        
    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &currentFrame->tRenderFinish,
        .swapchainCount = 1,
        .pSwapchains = &ptAppData->swapchain.tSwapChain,
        .pImageIndices = &ptAppData->swapchain.uCurrentImageIndex,
    };
    VkResult result = vkQueuePresentKHR(ptAppData->device.tPresentQueue, &presentInfo);
    if(result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        pl_create_swapchain(&ptAppData->device, ptAppData->graphics.tSurface, (uint32_t)ptIOCtx->afMainViewportSize[0], (uint32_t)ptIOCtx->afMainViewportSize[1], &ptAppData->swapchain);
        pl_create_framebuffers(&ptAppData->device, ptAppData->graphics.tRenderPass, &ptAppData->swapchain);
    }
    else
    {
        PL_VULKAN(result);
    }

    ptAppData->graphics.szCurrentFrameIndex = (ptAppData->graphics.szCurrentFrameIndex + 1) % ptAppData->graphics.uFramesInFlight;

    pl_end_io_frame();

    // end profiling frame
    pl_end_profile_frame();
}