/*
   pl_app.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] structs
*/

#ifndef PL_APP_H
#define PL_APP_H

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#include "pl.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

PL_DECLARE_STRUCT(plApp);

// external
PL_DECLARE_STRUCT(plWindow);
PL_DECLARE_STRUCT(plVulkanDevice);
PL_DECLARE_STRUCT(plVulkanGraphics);
PL_DECLARE_STRUCT(plVulkanSwapchain);

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

void pl_app_setup   (plApp* app, void* data);
void pl_app_shutdown(plApp* app);
void pl_app_resize  (plApp* app);
void pl_app_render  (plApp* app);

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct plApp_t
{
    plWindow*          window;
    plVulkanDevice*    device;
    plVulkanGraphics*  graphics;
    plVulkanSwapchain* swapchain;
    void*              data;
} plApp;

#endif // PL_APP_H