/*
   pl_vulkan.h
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] public api
// [SECTION] c file
// [SECTION] includes
// [SECTION] shaders
// [SECTION] internal structs
// [SECTION] internal helper forward declarations
// [SECTION] implementation
// [SECTION] internal helpers implementation
*/

#ifndef PL_VULKAN_H
#define PL_VULKAN_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_draw.h"
#include "vulkan/vulkan.h"

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

void pl_initialize_draw_context_vulkan(plDrawContext* ctx, VkPhysicalDevice tPhysicalDevice, uint32_t imageCount, VkDevice tLogicalDevice);
void pl_setup_drawlist_vulkan         (plDrawList* drawlist, VkRenderPass tRenderPass, VkSampleCountFlagBits tMSAASampleCount);
void pl_submit_drawlist_vulkan        (plDrawList* drawlist, float width, float height, VkCommandBuffer cmdBuf, uint32_t currentFrameIndex);
void pl_new_draw_frame                (plDrawContext* ctx);

// misc
VkDescriptorSet pl_add_texture(plDrawContext* drawContext, VkImageView imageView, VkImageLayout imageLayout);

#endif // PL_VULKAN_H