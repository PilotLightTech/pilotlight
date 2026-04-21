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

#include "pl.inc"
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
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_dear_imgui_ext  (const plApiRegistryI*, bool reload);
PL_API void pl_unload_dear_imgui_ext(const plApiRegistryI*, bool reload);

// setup/shutdown
PL_API void pl_dear_imgui_initialize(plDevice*, plSwapchain*, plRenderPassHandle tMainRenderPass);
PL_API void pl_dear_imgui_cleanup   (void);

// per frame
PL_API void pl_dear_imgui_new_frame(plDevice*, plRenderPassHandle tMainRenderPass);
PL_API void pl_dear_imgui_render   (plRenderEncoder*, plCommandBuffer*);

// for using texture in Dear ImGui specifically
PL_API ImTextureID pl_dear_imgui_get_texture_id_from_bindgroup(plDevice*, plBindGroupHandle);

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