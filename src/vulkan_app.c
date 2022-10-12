/*
   vulkan_app.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] globals
// [SECTION] pl_app_setup
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_render
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "vulkan_pl.h"
#include "vulkan_pl_drawing.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct
{
    plDrawContext* ctx;
    plDrawList*    drawlist;
    plDrawLayer*   fgDrawLayer;
    plDrawLayer*   bgDrawLayer;
    plFontAtlas    fontAtlas;
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] globals
//-----------------------------------------------------------------------------

plAppData gAppData = {0};

//-----------------------------------------------------------------------------
// [SECTION] pl_app_setup
//-----------------------------------------------------------------------------

void
pl_app_setup()
{

    // create render pass
    VkAttachmentDescription colorAttachment = {
        .flags = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT,
        .format = gSwapchain.format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    VkAttachmentReference attachmentReference = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachmentReference,
        .pDepthStencilAttachment = VK_NULL_HANDLE
    };

    VkRenderPassCreateInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1u,
        .pAttachments = &colorAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 0,
        .pDependencies = VK_NULL_HANDLE
    };
    PL_VULKAN(vkCreateRenderPass(gDevice.logicalDevice, &renderPassInfo, NULL, &gGraphics.renderPass));

    // create frame buffers
    pl_create_framebuffers(&gDevice, gGraphics.renderPass, &gSwapchain);
    
    // create per frame resources
    pl_create_frame_resources(&gGraphics, &gDevice);
    
    // setup drawing api
    gAppData.ctx = pl_create_draw_context_vulkan(gDevice.physicalDevice, 3, gDevice.logicalDevice);
    gAppData.drawlist = pl_create_drawlist(gAppData.ctx);
    pl_setup_drawlist_vulkan(gAppData.drawlist, gGraphics.renderPass);
    gAppData.bgDrawLayer = pl_request_draw_layer(gAppData.drawlist, "Background Layer");
    gAppData.fgDrawLayer = pl_request_draw_layer(gAppData.drawlist, "Foreground Layer");

    // create font atlas
    pl_add_default_font(&gAppData.fontAtlas);
    pl_build_font_atlas(gAppData.ctx, &gAppData.fontAtlas);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

void
pl_app_shutdown()
{
    // ensure device is finished
    vkDeviceWaitIdle(gDevice.logicalDevice);

    // cleanup font atlas
    pl_cleanup_font_atlas(&gAppData.fontAtlas);

    // cleanup drawing api
    pl_cleanup_draw_context(gAppData.ctx);

    // destroy swapchain
    for (uint32_t i = 0u; i < gSwapchain.imageCount; i++)
    {
        vkDestroyImageView(gDevice.logicalDevice, gSwapchain.imageViews[i], NULL);
        vkDestroyFramebuffer(gDevice.logicalDevice, gSwapchain.frameBuffers[i], NULL);
    }

    // destroy default render pass
    vkDestroyRenderPass(gDevice.logicalDevice, gGraphics.renderPass, NULL);
    vkDestroySwapchainKHR(gDevice.logicalDevice, gSwapchain.swapChain, NULL);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

void
pl_app_resize()
{
    pl_create_swapchain(&gDevice, gGraphics.surface, gClientWidth, gClientHeight, &gSwapchain);
    pl_create_framebuffers(&gDevice, gGraphics.renderPass, &gSwapchain);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_render
//-----------------------------------------------------------------------------

void
pl_app_render()
{
    pl_new_draw_frame(gAppData.ctx);

    VkClearValue clearValues[2] = 
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

    plVulkanFrameContext* currentFrame = pl_get_frame_resources(&gGraphics);

    // begin frame
    PL_VULKAN(vkWaitForFences(gDevice.logicalDevice, 1, &currentFrame->inFlight, VK_TRUE, UINT64_MAX));
    VkResult err = vkAcquireNextImageKHR(gDevice.logicalDevice, gSwapchain.swapChain, UINT64_MAX, currentFrame->imageAvailable,VK_NULL_HANDLE, &gSwapchain.currentImageIndex);
    if(err == VK_SUBOPTIMAL_KHR || err == VK_ERROR_OUT_OF_DATE_KHR)
    {
        if(err == VK_ERROR_OUT_OF_DATE_KHR)
        {
            pl_create_swapchain(&gDevice, gGraphics.surface, gClientWidth, gClientHeight, &gSwapchain);
            pl_create_framebuffers(&gDevice, gGraphics.renderPass, &gSwapchain);
            return;
        }
    }
    else
    {
        PL_VULKAN(err);
    }

    if (currentFrame->inFlight != VK_NULL_HANDLE)
        PL_VULKAN(vkWaitForFences(gDevice.logicalDevice, 1, &currentFrame->inFlight, VK_TRUE, UINT64_MAX));

    // begin recording
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    };
    PL_VULKAN(vkBeginCommandBuffer(currentFrame->cmdBuf, &beginInfo));

    // begin render pass
    VkRenderPassBeginInfo renderPassBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = gGraphics.renderPass,
        .framebuffer = gSwapchain.frameBuffers[gSwapchain.currentImageIndex],
        .renderArea.offset.x = 0,
        .renderArea.offset.y = 0,
        .renderArea.extent = gSwapchain.extent,
        .clearValueCount = 2,
        .pClearValues = clearValues
    };
    vkCmdBeginRenderPass(currentFrame->cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // set viewport
    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)gSwapchain.extent.width,
        .height = (float)gSwapchain.extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    vkCmdSetViewport(currentFrame->cmdBuf, 0, 1, &viewport);

    // set scissor
    VkRect2D dynamicScissor = {.extent = gSwapchain.extent};
    vkCmdSetScissor(currentFrame->cmdBuf, 0, 1, &dynamicScissor);  

    // draw commands
    pl_add_text(gAppData.fgDrawLayer, &gAppData.fontAtlas.sbFonts[0], 13.0f, (plVec2){10.0f, 10.0f}, (plVec4){0.1f, 0.5f, 0.0f, 1.0f}, "Pilot Light\nGraphics", 0.0f);
    pl_add_triangle_filled(gAppData.bgDrawLayer, (plVec2){10.0f, 50.0f}, (plVec2){10.0f, 150.0f}, (plVec2){150.0f, 50.0f}, (plVec4){1.0f, 0.0f, 0.0f, 1.0f});
    plVec2 textSize = pl_calculate_text_size(&gAppData.fontAtlas.sbFonts[0], 13.0f, "Pilot Light\nGraphics", 0.0f);
    pl_add_rect_filled(gAppData.bgDrawLayer, (plVec2){10.0f, 10.0f}, (plVec2){10.0f + textSize.x, 10.0f + textSize.y}, (plVec4){0.0f, 0.0f, 0.8f, 0.5f});
    pl_add_line(gAppData.bgDrawLayer, (plVec2){500.0f, 10.0f}, (plVec2){10.0f, 500.0f}, (plVec4){1.0f, 1.0f, 1.0f, 0.5f}, 2.0f);
    
    // submit draw layers
    pl_submit_draw_layer(gAppData.bgDrawLayer);
    pl_submit_draw_layer(gAppData.fgDrawLayer);

    // submit draw lists
    pl_submit_drawlist_vulkan(gAppData.drawlist, (float)gClientWidth, (float)gClientHeight, currentFrame->cmdBuf, (uint32_t)gGraphics.currentFrameIndex);

    // end render pass
    vkCmdEndRenderPass(currentFrame->cmdBuf);

    // end recording
    PL_VULKAN(vkEndCommandBuffer(currentFrame->cmdBuf));

    // submit
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &currentFrame->imageAvailable,
        .pWaitDstStageMask = waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &currentFrame->cmdBuf,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &currentFrame->renderFinish
    };
    PL_VULKAN(vkResetFences(gDevice.logicalDevice, 1, &currentFrame->inFlight));
    PL_VULKAN(vkQueueSubmit(gDevice.graphicsQueue, 1, &submitInfo, currentFrame->inFlight));          
    
    // present                        
    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &currentFrame->renderFinish,
        .swapchainCount = 1,
        .pSwapchains = &gSwapchain.swapChain,
        .pImageIndices = &gSwapchain.currentImageIndex,
    };
    VkResult result = vkQueuePresentKHR(gDevice.presentQueue, &presentInfo);
    if(result == VK_SUBOPTIMAL_KHR || err == VK_ERROR_OUT_OF_DATE_KHR)
    {
        pl_create_swapchain(&gDevice, gGraphics.surface, gClientWidth, gClientHeight, &gSwapchain);
        pl_create_framebuffers(&gDevice, gGraphics.renderPass, &gSwapchain);
    }
    else
    {
        PL_VULKAN(err);
    }

    gGraphics.currentFrameIndex = (gGraphics.currentFrameIndex + 1) % gGraphics.framesInFlight;
}