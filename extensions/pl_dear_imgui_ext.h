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

#define plDearImGuiI_version {0, 2, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// external (pl_graphics_ext.h)
typedef union  plBindGroupHandle  plBindGroupHandle;
typedef struct _plSwapchain       plSwapchain;
typedef struct _plCommandBuffer   plCommandBuffer;
typedef struct _plDevice          plDevice;
typedef struct _plRenderAttachmentInfo plRenderAttachmentInfo;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_dear_imgui_ext  (const plApiRegistryI*, bool reload);
PL_API void pl_unload_dear_imgui_ext(const plApiRegistryI*, bool reload);

// setup/shutdown
PL_API void pl_dear_imgui_initialize(plDevice*, plSwapchain*, const plRenderAttachmentInfo*);
PL_API void pl_dear_imgui_cleanup   (void);

// per frame
PL_API void pl_dear_imgui_new_frame(plDevice*);
PL_API void pl_dear_imgui_render   (plCommandBuffer*);

// for using texture in Dear ImGui specifically
PL_API ImTextureID pl_dear_imgui_get_texture_id_from_bindgroup(plDevice*, plBindGroupHandle);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plDearImGuiI
{
    // setup/shutdown
    void (*initialize)(plDevice*, plSwapchain*, const plRenderAttachmentInfo*);
    void (*cleanup)   (void);

    // per frame
    void (*new_frame)(plDevice*);
    void (*render)   (plCommandBuffer*);

    ImTextureID (*get_texture_id_from_bindgroup)(plDevice*, plBindGroupHandle);
} plDearImGuiI;

#endif // PL_DEAR_IMGUI_EXT_H