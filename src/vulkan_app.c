/*
   vulkan_app.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] forward declarations
// [SECTION] globals
// [SECTION] pl_app_setup
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_render
// [SECTION] misc
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
    VkBuffer              indexBuffer;
    VkBuffer              vertexBuffer;
    VkDeviceMemory        indexDeviceMemory;
    VkDeviceMemory        vertexDeviceMemory;
    VkDeviceMemory        textureImageMemory;
    VkImage               textureImage;
    VkDescriptorImageInfo imageInfo;
    VkPipelineLayout      pipelineLayout;
    VkPipeline            pipeline;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorSet*      descriptorSets;
    VkBuffer              stagingBuffer;
    VkDeviceMemory        stagingBufferDeviceMemory;
    void*                 stageMapping;
    plDrawContext         ctx;
    plDrawList*           drawlist;
    plDrawLayer*          fgDrawLayer;
    plDrawLayer*          bgDrawLayer;
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------
void create_vertex_buffer();
void create_index_buffer();
void create_texture();

//-----------------------------------------------------------------------------
// [SECTION] globals
//-----------------------------------------------------------------------------

plAppData gAppData = {0};

static const float g_vertexData[] = { // x, y, u, v
    -0.5f,  0.5f, 0.0f, 0.0f,
    -0.5f, -0.5f, 0.0f, 1.0f,
    0.5f, -0.5f, 1.0f, 1.0f,
    0.5f,  0.5f, 1.0f, 0.0f,
};

static const unsigned g_indices[] = { 
    2, 1, 0,
    3, 2, 0
};

static unsigned char g_image[] = {
    255,   0,   0, 255,
    0, 255,   0, 255,
    0,   0, 255, 255,
    255,   0, 255, 255
};

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
    
    // create descriptor pool
    VkDescriptorPoolSize poolSizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER,                1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       1000 }
    };

    VkDescriptorPoolCreateInfo descPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1000 * 11,
        .poolSizeCount = 11u,
        .pPoolSizes = poolSizes
    };
    // VkDescriptorPool descriptorPool;
    PL_VULKAN(vkCreateDescriptorPool(gDevice.logicalDevice, &descPoolInfo, NULL, &gGraphics.descPool));

    // create per frame resources
    pl_create_frame_resources(&gGraphics, &gDevice);
    
    // create vertex layout
    VkVertexInputBindingDescription bindingDescription = {
        .binding = 0,
        .stride = sizeof(float)*4,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    VkVertexInputAttributeDescription attributeDescriptions[2] = 
    {
        {
            .binding = 0,
            .location = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = 0u
        },
        {
            .binding = 0,
            .location = 1,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = sizeof(float)*2
        }
    };

    // create descriptor set layout
    VkDescriptorSetLayoutBinding binding = {
        .binding = 0u,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = NULL
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &binding
    };
    PL_VULKAN(vkCreateDescriptorSetLayout(gDevice.logicalDevice, &layoutInfo, NULL, &gAppData.descriptorSetLayout));

    // allocate descriptor sets
    gAppData.descriptorSets = (VkDescriptorSet*)malloc(gGraphics.framesInFlight*sizeof(VkDescriptorSet));
    VkDescriptorSetLayout* layouts = (VkDescriptorSetLayout*)malloc(gGraphics.framesInFlight*sizeof(VkDescriptorSetLayout));
    for(uint32_t i = 0; i < gGraphics.framesInFlight; i++)
        layouts[i] = gAppData.descriptorSetLayout;

    VkDescriptorSetAllocateInfo allocDescInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = gGraphics.descPool,
        .descriptorSetCount = gGraphics.framesInFlight,
        .pSetLayouts = layouts
    };
    PL_VULKAN(vkAllocateDescriptorSets(gDevice.logicalDevice, &allocDescInfo, gAppData.descriptorSets));
    free(layouts);
    
    // create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &gAppData.descriptorSetLayout
    };
    PL_VULKAN(vkCreatePipelineLayout(gDevice.logicalDevice, &pipelineLayoutInfo, NULL, &gAppData.pipelineLayout));

    // create pipeline

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1u,
        .vertexAttributeDescriptionCount = 2u,
        .pVertexBindingDescriptions = &bindingDescription,
        .pVertexAttributeDescriptions = attributeDescriptions
    };

    unsigned vertexFileSize = 0u;
    pl_read_file("simple.vert.spv", &vertexFileSize, NULL, "rb");
    char* vertexShaderCode = (char*)malloc(vertexFileSize);
    pl_read_file("simple.vert.spv", &vertexFileSize, vertexShaderCode, "rb");
    VkShaderModuleCreateInfo vertShaderCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = vertexFileSize,
        .pCode = (const uint32_t*)(vertexShaderCode)
    };
    VkShaderModule vertShaderModule;
    PL_VULKAN(vkCreateShaderModule(gDevice.logicalDevice, &vertShaderCreateInfo, NULL, &vertShaderModule));

    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertShaderModule,
        .pName = "main"
    };

    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)gClientWidth,
        .height = (float)gClientHeight,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    VkRect2D scissor = {
        .offset.x = 0u,
        .offset.y = 0u,
        .extent.width = (unsigned)viewport.width,
        .extent.height = (unsigned)viewport.height
    };

    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE
    };

    unsigned fragFileSize = 0u;
    pl_read_file("simple.frag.spv", &fragFileSize, NULL, "rb");
    char* fragShaderCode = (char*)malloc(fragFileSize);
    pl_read_file("simple.frag.spv", &fragFileSize, fragShaderCode, "rb");
    VkShaderModuleCreateInfo fragShaderCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = fragFileSize,
        .pCode = (const uint32_t*)(fragShaderCode)
    };
    VkShaderModule fragShaderModule;
    PL_VULKAN(vkCreateShaderModule(gDevice.logicalDevice, &fragShaderCreateInfo, NULL, &fragShaderModule));

    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragShaderModule,
        .pName = "main"
    };

    VkPipelineDepthStencilStateCreateInfo depthStencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO
    };

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD
    };

    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
        .blendConstants[0] = 0.0f,
        .blendConstants[1] = 0.0f,
        .blendConstants[2] = 0.0f,
        .blendConstants[3] = 0.0f
    };

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    VkPipelineShaderStageCreateInfo shaderStages[] = {
        vertShaderStageInfo,
        fragShaderStageInfo
    };

    VkDynamicState dynamicStateEnables[3] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamicStateEnables
    };

    VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2u,
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = gAppData.pipelineLayout,
        .renderPass = gGraphics.renderPass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .pDepthStencilState = &depthStencil
    };
    PL_VULKAN(vkCreateGraphicsPipelines(gDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &gAppData.pipeline));

    // no longer need these
    vkDestroyShaderModule(gDevice.logicalDevice, vertShaderModule, NULL);
    vkDestroyShaderModule(gDevice.logicalDevice, fragShaderModule, NULL);
    vertShaderModule = VK_NULL_HANDLE;
    fragShaderModule = VK_NULL_HANDLE;

    free(vertexShaderCode);
    free(fragShaderCode);

    // create staging buffer
    VkBufferCreateInfo stagingBufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = 1024,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    PL_VULKAN(vkCreateBuffer(gDevice.logicalDevice, &stagingBufferInfo, NULL, &gAppData.stagingBuffer));

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(gDevice.logicalDevice, gAppData.stagingBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = pl_find_memory_type(gDevice.memProps, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };

    PL_VULKAN(vkAllocateMemory(gDevice.logicalDevice, &allocInfo, NULL, &gAppData.stagingBufferDeviceMemory));
    PL_VULKAN(vkBindBufferMemory(gDevice.logicalDevice, gAppData.stagingBuffer, gAppData.stagingBufferDeviceMemory, 0));

    PL_VULKAN(vkMapMemory(gDevice.logicalDevice, gAppData.stagingBufferDeviceMemory, 0, 1024, 0, &gAppData.stageMapping));

    create_vertex_buffer();
    create_index_buffer();
    create_texture();

    pl_setup_draw_context_vulkan(&gAppData.ctx, gDevice.physicalDevice, 3, gDevice.logicalDevice);
    gAppData.drawlist = malloc(sizeof(plDrawList));
    pl_create_drawlist(&gAppData.ctx, gAppData.drawlist);
    pl_setup_drawlist_vulkan(gAppData.drawlist, gGraphics.renderPass);
    gAppData.bgDrawLayer = pl_request_draw_layer(gAppData.drawlist, "Background Layer");
    gAppData.fgDrawLayer = pl_request_draw_layer(gAppData.drawlist, "Foreground Layer");
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

void
pl_app_shutdown()
{
    // ensure device is finished
    vkDeviceWaitIdle(gDevice.logicalDevice);

    // destroy sampler
    vkDestroySampler(gDevice.logicalDevice, gAppData.imageInfo.sampler, NULL);

    // destroy image views
    vkDestroyImageView(gDevice.logicalDevice, gAppData.imageInfo.imageView, NULL);

    // destroy staging buffer
    vkUnmapMemory(gDevice.logicalDevice, gAppData.stagingBufferDeviceMemory);
    vkDestroyBuffer(gDevice.logicalDevice, gAppData.stagingBuffer, NULL);
    vkFreeMemory(gDevice.logicalDevice, gAppData.stagingBufferDeviceMemory, NULL);

    // destroy buffers & images
    vkDestroyBuffer(gDevice.logicalDevice, gAppData.indexBuffer, NULL);
    vkDestroyBuffer(gDevice.logicalDevice, gAppData.vertexBuffer, NULL);
    vkDestroyImage(gDevice.logicalDevice, gAppData.textureImage, NULL);

    // destroy backing memory
    vkFreeMemory(gDevice.logicalDevice, gAppData.indexDeviceMemory, NULL);
    vkFreeMemory(gDevice.logicalDevice, gAppData.vertexDeviceMemory, NULL);
    vkFreeMemory(gDevice.logicalDevice, gAppData.textureImageMemory, NULL);

    // destroy pipeline
    vkDestroyPipelineLayout(gDevice.logicalDevice, gAppData.pipelineLayout, NULL);
    vkDestroyPipeline(gDevice.logicalDevice, gAppData.pipeline, NULL);

    // destroy descriptor set layouts
    vkDestroyDescriptorSetLayout(gDevice.logicalDevice, gAppData.descriptorSetLayout, NULL);

    // cleanup drawing api
    pl_cleanup_draw_context(&gAppData.ctx);

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

    pl_new_draw_frame(&gAppData.ctx);

    VkClearValue clearValues[2] = 
    {
        {
            .color.float32[0] = 60.0f/255.0f,
            .color.float32[1] = 60.0f/255.0f,
            .color.float32[2] = 60.0f/255.0f,
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

    // update descriptors
    VkWriteDescriptorSet descriptorWrite = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = 0u,
        .dstArrayElement = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .dstSet = gAppData.descriptorSets[gGraphics.currentFrameIndex],
        .pImageInfo = &gAppData.imageInfo,
        .pNext = NULL
    };
    vkUpdateDescriptorSets(gDevice.logicalDevice, 1, &descriptorWrite, 0, NULL);

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
    
    // set pipeline state
    static VkDeviceSize offsets = { 0 };
    vkCmdSetDepthBias(currentFrame->cmdBuf, 0.0f, 0.0f, 0.0f);
    vkCmdBindPipeline(currentFrame->cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, gAppData.pipeline);
    vkCmdBindDescriptorSets(currentFrame->cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, gAppData.pipelineLayout, 0, 1, &gAppData.descriptorSets[gGraphics.currentFrameIndex], 0u, NULL);
    vkCmdBindIndexBuffer(currentFrame->cmdBuf, gAppData.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindVertexBuffers(currentFrame->cmdBuf, 0, 1, &gAppData.vertexBuffer, &offsets);
    
    // draw
    vkCmdDrawIndexed(currentFrame->cmdBuf, 6, 1, 0, 0, 0);

    // draw commands
    pl_add_triangle_filled(gAppData.bgDrawLayer, (plVec2){10.0f, 10.0f}, (plVec2){10.0f, 200.0f}, (plVec2){200.0f, 200.0f}, (plVec4){1.0f, 0.0f, 0.0f, 1.0f});
    pl_add_triangle_filled(gAppData.bgDrawLayer, (plVec2){10.0f, 10.0f}, (plVec2){200.0f, 200.0f}, (plVec2){200.0f, 10.0f}, (plVec4){0.0f, 1.0f, 0.0f, 1.0f});

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

//-----------------------------------------------------------------------------
// [SECTION] misc
//-----------------------------------------------------------------------------

void
create_vertex_buffer()
{
    memcpy(gAppData.stageMapping, g_vertexData, sizeof(float)*16);

    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(float)*16,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    PL_VULKAN(vkCreateBuffer(gDevice.logicalDevice, &bufferInfo, NULL, &gAppData.vertexBuffer));

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(gDevice.logicalDevice, gAppData.vertexBuffer, &memRequirements);

    VkMemoryAllocateInfo allocBufferInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = pl_find_memory_type(gDevice.memProps, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    PL_VULKAN(vkAllocateMemory(gDevice.logicalDevice, &allocBufferInfo, NULL, &gAppData.vertexDeviceMemory));
    PL_VULKAN(vkBindBufferMemory(gDevice.logicalDevice, gAppData.vertexBuffer, gAppData.vertexDeviceMemory, 0));

    // copy buffer
    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool = gGraphics.cmdPool,
        .commandBufferCount = 1
    };

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(gDevice.logicalDevice, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    vkBeginCommandBuffer(commandBuffer, &beginInfo); 

    VkBufferCopy copyRegion = {
        .size = sizeof(float)*16
    };
    vkCmdCopyBuffer(commandBuffer, gAppData.stagingBuffer, gAppData.vertexBuffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer
    };

    vkQueueSubmit(gDevice.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkDeviceWaitIdle(gDevice.logicalDevice);

    vkFreeCommandBuffers(gDevice.logicalDevice, gGraphics.cmdPool, 1, &commandBuffer);
}

void
create_index_buffer()
{

    memcpy(gAppData.stageMapping, g_indices, sizeof(unsigned)*6);

    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(unsigned)*6,
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    PL_VULKAN(vkCreateBuffer(gDevice.logicalDevice, &bufferInfo, NULL, &gAppData.indexBuffer));

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(gDevice.logicalDevice, gAppData.indexBuffer, &memRequirements);

    VkMemoryAllocateInfo allocBufferInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = pl_find_memory_type(gDevice.memProps, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
    };

    PL_VULKAN(vkAllocateMemory(gDevice.logicalDevice, &allocBufferInfo, NULL, &gAppData.indexDeviceMemory));
    PL_VULKAN(vkBindBufferMemory(gDevice.logicalDevice, gAppData.indexBuffer, gAppData.indexDeviceMemory, 0));

    // copy buffer
    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool = gGraphics.cmdPool,
        .commandBufferCount = 1
    };

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(gDevice.logicalDevice, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    vkBeginCommandBuffer(commandBuffer, &beginInfo); 

    VkBufferCopy copyRegion = {
        .size = sizeof(unsigned)*6
    };
    vkCmdCopyBuffer(commandBuffer, gAppData.stagingBuffer, gAppData.indexBuffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer
    };

    vkQueueSubmit(gDevice.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkDeviceWaitIdle(gDevice.logicalDevice);

    vkFreeCommandBuffers(gDevice.logicalDevice, gGraphics.cmdPool, 1, &commandBuffer);
}

void
create_texture()
{

    gAppData.imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    memcpy(gAppData.stageMapping, g_image, sizeof(unsigned char)*16);

    VkImageCreateInfo imageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .extent.width = 2,
        .extent.height = 2,
        .extent.depth = 1,
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .flags = 0
    };

    PL_VULKAN(vkCreateImage(gDevice.logicalDevice, &imageInfo, NULL, &gAppData.textureImage));

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(gDevice.logicalDevice, gAppData.textureImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = pl_find_memory_type(gDevice.memProps, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    PL_VULKAN(vkAllocateMemory(gDevice.logicalDevice, &allocInfo, NULL, &gAppData.textureImageMemory));
    PL_VULKAN(vkBindImageMemory(gDevice.logicalDevice, gAppData.textureImage, gAppData.textureImageMemory, 0));

    //-----------------------------------------------------------------------------
    // final image
    //-----------------------------------------------------------------------------

    VkImageSubresourceRange subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = imageInfo.mipLevels,
        .baseArrayLayer = 0,
        .layerCount = 1
    };

    VkCommandBufferAllocateInfo allocCommandInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool = gGraphics.cmdPool,
        .commandBufferCount = 1
    };

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(gDevice.logicalDevice, &allocCommandInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };


    vkBeginCommandBuffer(commandBuffer, &beginInfo); 
    pl_transition_image_layout(commandBuffer, gAppData.textureImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    VkBufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,

        .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .imageSubresource.mipLevel = 0,
        .imageSubresource.baseArrayLayer = 0,
        .imageSubresource.layerCount = 1,

        .imageOffset.x = 0,
        .imageOffset.y = 0,
        .imageOffset.z = 0,

        .imageExtent.width = (unsigned)2,
        .imageExtent.height = (unsigned)2,
        .imageExtent.depth = 1

    };

    vkCmdCopyBufferToImage(
        commandBuffer,
        gAppData.stagingBuffer,
        gAppData.textureImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    pl_transition_image_layout(commandBuffer, gAppData.textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    
    // submit
    vkEndCommandBuffer(commandBuffer);
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer
    };

    vkQueueSubmit(gDevice.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkDeviceWaitIdle(gDevice.logicalDevice);
    vkFreeCommandBuffers(gDevice.logicalDevice, gGraphics.cmdPool, 1, &commandBuffer);

    VkPhysicalDeviceProperties properties = {0};
    vkGetPhysicalDeviceProperties(gDevice.physicalDevice, &properties);

    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = gAppData.textureImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .subresourceRange.baseMipLevel = 0,
        .subresourceRange.levelCount = imageInfo.mipLevels,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount = 1,
        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT
    };

    PL_VULKAN(vkCreateImageView(gDevice.logicalDevice, &viewInfo, NULL, &gAppData.imageInfo.imageView));

    VkSamplerCreateInfo samplerInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .mipLodBias = 0.0f,
        .minLod = 0.0f,
        .maxLod = 1.0f
    };

    PL_VULKAN(vkCreateSampler(gDevice.logicalDevice, &samplerInfo, NULL, &gAppData.imageInfo.sampler));
}
