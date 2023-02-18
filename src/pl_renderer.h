/*
   pl_renderer.h, v0.1 (WIP)
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_RENDERER_H
#define PL_RENDERER_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>    // uint*_t
#include <stdbool.h>   // bool
#include "pl_graphics_vulkan.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// forward declarations
typedef struct _plRenderer      plRenderer;
typedef struct _plAssetRegistry plAssetRegistry;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// renderer
void pl_setup_renderer  (plGraphics* ptGraphics, plAssetRegistry* ptRegistry, plRenderer* ptRendererOut);
void pl_cleanup_renderer(plRenderer* ptRenderer);

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plRenderer
{
    plGraphics*      ptGraphics;
    plAssetRegistry* ptAssetRegistry;
    float*           sbfStorageBuffer;
    plDraw*          sbtDraws;
    plDrawArea*      sbtDrawAreas;
    plBindGroup      tGlobalBindGroup;
    uint32_t         uGlobalStorageBuffer;
} plRenderer;

#endif // PL_RENDERER_H