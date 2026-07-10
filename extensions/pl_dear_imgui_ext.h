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

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plDearImGuiI_version {0, 3, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plImGuiPipelineInitInfo plImGuiPipelineInitInfo;
typedef struct _plImGuiGraphicsInitInfo plImGuiGraphicsInitInfo;

// external (pl_graphics_ext.h)
typedef struct _plCommandBuffer        plCommandBuffer;
typedef struct _plDevice               plDevice;
typedef struct _plRenderAttachmentInfo plRenderAttachmentInfo;
typedef int plSampleCount;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_dear_imgui_ext  (const plApiRegistryI*, bool reload);
PL_API void pl_unload_dear_imgui_ext(const plApiRegistryI*, bool reload);

// setup/shutdown
PL_API void pl_dear_imgui_initialize(plImGuiGraphicsInitInfo*); // use NULL to auto fill using starter
PL_API void pl_dear_imgui_cleanup   (void);

// per frame
PL_API void pl_dear_imgui_new_frame(plDevice*);
PL_API void pl_dear_imgui_render   (plCommandBuffer*);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plDearImGuiI
{
    // setup/shutdown
    void (*initialize)(plImGuiGraphicsInitInfo*);
    void (*cleanup)   (void);

    // per frame
    void (*new_frame)(plDevice*);
    void (*render)   (plCommandBuffer*);
} plDearImGuiI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plImGuiPipelineInitInfo
{
    plRenderAttachmentInfo* ptRenderAttachmentInfo;
    plSampleCount           eMsaaSamples;
} plImGuiPipelineInitInfo;

typedef struct _plImGuiGraphicsInitInfo
{
    plDevice*               ptDevice;
    uint32_t                uImageCount;
    plImGuiPipelineInitInfo tPipelineInfoMain;
} plImGuiGraphicsInitInfo;

#endif // PL_DEAR_IMGUI_EXT_H