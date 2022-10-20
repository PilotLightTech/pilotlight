/*
   vulkan_pl.h
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] globals
*/

#ifndef VULKAN_PL_H
#define VULKAN_PL_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_io.h"
#include "vulkan_pl_graphics.h"

//-----------------------------------------------------------------------------
// [SECTION] globals
//-----------------------------------------------------------------------------

typedef struct plAppData_t
{
   plVulkanDevice    device;
   plVulkanGraphics  graphics;
   plVulkanSwapchain swapchain;
   bool              running;
   int               actualWidth;
   int               actualHeight;
   int               clientWidth;
   int               clientHeight;
   plIOContext       tIOContext;
} plAppData;

#endif // VULKAN_PL_H