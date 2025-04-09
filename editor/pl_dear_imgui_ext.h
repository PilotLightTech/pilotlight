/*
   pl_dear_imgui_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api struct
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_DEAR_IMGUI_EXT_H
#define PL_DEAR_IMGUI_EXT_H

#define ImTextureID void*

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plDearImGuiI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// external (pl_graphics_ext.h)
typedef union  plRenderPassHandle plRenderPassHandle;
typedef union  plBindGroupHandle  plBindGroupHandle;
typedef struct _plRenderEncoder   plRenderEncoder;
typedef struct _plSwapchain       plSwapchain;
typedef struct _plCommandBuffer   plCommandBuffer;
typedef struct _plDevice          plDevice;

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plDearImGuiI
{
    // setup/shutdown
    void (*initialize)(plDevice*, plSwapchain*, plRenderPassHandle tMainRenderPass);
    void (*cleanup)   (void);

    // per frame
    void (*new_frame)(plDevice*, plRenderPassHandle tMainRenderPass);
    void (*render)   (plRenderEncoder*, plCommandBuffer*);

    ImTextureID (*get_texture_id_from_bindgroup)(plDevice*, plBindGroupHandle);
} plDearImGuiI;

#endif // PL_DEAR_IMGUI_EXT_H