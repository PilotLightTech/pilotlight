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
// [SECTION] pl_app_render
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.h"
#include "vulkan_pl.h"
#include "vulkan_pl_drawing.h"
#include <string.h> // memset

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct plUserData_t
{
    plDrawContext* ctx;
    plDrawList*    drawlist;
    plDrawLayer*   fgDrawLayer;
    plDrawLayer*   bgDrawLayer;
    plFontAtlas    fontAtlas;
} plUserData;

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plAppData* appData, plUserData* userData)
{
    if(userData)
        return userData;
    plUserData* newUserData = malloc(sizeof(plUserData));
    memset(newUserData, 0, sizeof(plUserData));
    return newUserData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_setup
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_setup(plAppData* appData, plUserData* userData)
{

    // create render pass
    VkAttachmentDescription colorAttachment = {
        .flags = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT,
        .format = appData->swapchain.format,
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
    PL_VULKAN(vkCreateRenderPass(appData->device.logicalDevice, &renderPassInfo, NULL, &appData->graphics.renderPass));

    // create frame buffers
    pl_create_framebuffers(&appData->device, appData->graphics.renderPass, &appData->swapchain);
    
    // create per frame resources
    pl_create_frame_resources(&appData->graphics, &appData->device);
    
    // setup drawing api
    userData->ctx = pl_create_draw_context_vulkan(appData->device.physicalDevice, 3, appData->device.logicalDevice);
    userData->drawlist = pl_create_drawlist(userData->ctx);
    pl_setup_drawlist_vulkan(userData->drawlist, appData->graphics.renderPass);
    userData->bgDrawLayer = pl_request_draw_layer(userData->drawlist, "Background Layer");
    userData->fgDrawLayer = pl_request_draw_layer(userData->drawlist, "Foreground Layer");

    // create font atlas
    pl_add_default_font(&userData->fontAtlas);
    pl_build_font_atlas(userData->ctx, &userData->fontAtlas);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* appData, plUserData* userData)
{
    // ensure device is finished
    vkDeviceWaitIdle(appData->device.logicalDevice);

    // cleanup font atlas
    pl_cleanup_font_atlas(&userData->fontAtlas);

    // cleanup drawing api
    pl_cleanup_draw_context(userData->ctx);

    // destroy swapchain
    for (uint32_t i = 0u; i < appData->swapchain.imageCount; i++)
    {
        vkDestroyImageView(appData->device.logicalDevice, appData->swapchain.imageViews[i], NULL);
        vkDestroyFramebuffer(appData->device.logicalDevice, appData->swapchain.frameBuffers[i], NULL);
    }

    // destroy default render pass
    vkDestroyRenderPass(appData->device.logicalDevice, appData->graphics.renderPass, NULL);
    vkDestroySwapchainKHR(appData->device.logicalDevice, appData->swapchain.swapChain, NULL);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plAppData* appData, plUserData* userData)
{
    pl_create_swapchain(&appData->device, appData->graphics.surface, appData->clientWidth, appData->clientHeight, &appData->swapchain);
    pl_create_framebuffers(&appData->device, appData->graphics.renderPass, &appData->swapchain);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_render
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_render(plAppData* appData, plUserData* userData)
{
    pl_new_draw_frame(userData->ctx);

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

    plVulkanFrameContext* currentFrame = pl_get_frame_resources(&appData->graphics);

    // begin frame
    PL_VULKAN(vkWaitForFences(appData->device.logicalDevice, 1, &currentFrame->inFlight, VK_TRUE, UINT64_MAX));
    VkResult err = vkAcquireNextImageKHR(appData->device.logicalDevice, appData->swapchain.swapChain, UINT64_MAX, currentFrame->imageAvailable,VK_NULL_HANDLE, &appData->swapchain.currentImageIndex);
    if(err == VK_SUBOPTIMAL_KHR || err == VK_ERROR_OUT_OF_DATE_KHR)
    {
        if(err == VK_ERROR_OUT_OF_DATE_KHR)
        {
            pl_create_swapchain(&appData->device, appData->graphics.surface, appData->clientWidth, appData->clientHeight, &appData->swapchain);
            pl_create_framebuffers(&appData->device, appData->graphics.renderPass, &appData->swapchain);
            return;
        }
    }
    else
    {
        PL_VULKAN(err);
    }

    if (currentFrame->inFlight != VK_NULL_HANDLE)
        PL_VULKAN(vkWaitForFences(appData->device.logicalDevice, 1, &currentFrame->inFlight, VK_TRUE, UINT64_MAX));

    // begin recording
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    };
    PL_VULKAN(vkBeginCommandBuffer(currentFrame->cmdBuf, &beginInfo));

    // begin render pass
    VkRenderPassBeginInfo renderPassBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = appData->graphics.renderPass,
        .framebuffer = appData->swapchain.frameBuffers[appData->swapchain.currentImageIndex],
        .renderArea.offset.x = 0,
        .renderArea.offset.y = 0,
        .renderArea.extent = appData->swapchain.extent,
        .clearValueCount = 2,
        .pClearValues = clearValues
    };
    vkCmdBeginRenderPass(currentFrame->cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // set viewport
    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)appData->swapchain.extent.width,
        .height = (float)appData->swapchain.extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    vkCmdSetViewport(currentFrame->cmdBuf, 0, 1, &viewport);

    // set scissor
    VkRect2D dynamicScissor = {.extent = appData->swapchain.extent};
    vkCmdSetScissor(currentFrame->cmdBuf, 0, 1, &dynamicScissor);  

    // draw commands
    pl_add_text(userData->fgDrawLayer, &userData->fontAtlas.sbFonts[0], 13.0f, (plVec2){10.0f, 10.0f}, (plVec4){0.1f, 0.5f, 0.0f, 1.0f}, "Pilot Light\nGraphics", 0.0f);
    pl_add_triangle_filled(userData->bgDrawLayer, (plVec2){10.0f, 50.0f}, (plVec2){10.0f, 150.0f}, (plVec2){150.0f, 50.0f}, (plVec4){1.0f, 0.0f, 0.0f, 1.0f});
    plVec2 textSize = pl_calculate_text_size(&userData->fontAtlas.sbFonts[0], 13.0f, "Pilot Light\nGraphics", 0.0f);
    pl_add_rect_filled(userData->bgDrawLayer, (plVec2){10.0f, 10.0f}, (plVec2){10.0f + textSize.x, 10.0f + textSize.y}, (plVec4){0.0f, 0.0f, 0.8f, 0.5f});
    pl_add_line(userData->bgDrawLayer, (plVec2){500.0f, 10.0f}, (plVec2){10.0f, 500.0f}, (plVec4){1.0f, 1.0f, 1.0f, 0.5f}, 2.0f);
    
    // submit draw layers
    pl_submit_draw_layer(userData->bgDrawLayer);
    pl_submit_draw_layer(userData->fgDrawLayer);

    // submit draw lists
    pl_submit_drawlist_vulkan(userData->drawlist, (float)appData->clientWidth, (float)appData->clientHeight, currentFrame->cmdBuf, (uint32_t)appData->graphics.currentFrameIndex);

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
    PL_VULKAN(vkResetFences(appData->device.logicalDevice, 1, &currentFrame->inFlight));
    PL_VULKAN(vkQueueSubmit(appData->device.graphicsQueue, 1, &submitInfo, currentFrame->inFlight));          
    
    // present                        
    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &currentFrame->renderFinish,
        .swapchainCount = 1,
        .pSwapchains = &appData->swapchain.swapChain,
        .pImageIndices = &appData->swapchain.currentImageIndex,
    };
    VkResult result = vkQueuePresentKHR(appData->device.presentQueue, &presentInfo);
    if(result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        pl_create_swapchain(&appData->device, appData->graphics.surface, appData->clientWidth, appData->clientHeight, &appData->swapchain);
        pl_create_framebuffers(&appData->device, appData->graphics.renderPass, &appData->swapchain);
    }
    else
    {
        PL_VULKAN(result);
    }

    appData->graphics.currentFrameIndex = (appData->graphics.currentFrameIndex + 1) % appData->graphics.framesInFlight;
}