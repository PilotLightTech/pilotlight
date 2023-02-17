/*
   pl_vulkan.h
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
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
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plVulkanInit
{
   VkPhysicalDevice      tPhysicalDevice;
   VkDevice              tLogicalDevice;
   uint32_t              uImageCount;
   VkRenderPass          tRenderPass; // default render pass
   VkSampleCountFlagBits tMSAASampleCount;
} plVulkanInit;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

void pl_initialize_draw_context_vulkan(plDrawContext* ptCtx, const plVulkanInit* ptInit);
void pl_submit_drawlist_vulkan        (plDrawList* ptDrawlist, float fWidth, float fHeight, VkCommandBuffer tCmdBuf, uint32_t uFrameIndex);
void pl_submit_drawlist_vulkan_ex     (plDrawList* ptDrawlist, float fWidth, float fHeight, VkCommandBuffer tCmdBuf, uint32_t uFrameIndex, VkRenderPass tRenderPass, VkSampleCountFlagBits tMSAASampleCount);
void pl_new_draw_frame                (plDrawContext* ptCtx);

// misc
VkDescriptorSet pl_add_texture(plDrawContext* ptCtx, VkImageView tImageView, VkImageLayout tImageLayout);

#endif // PL_VULKAN_H