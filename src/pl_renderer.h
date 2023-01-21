/*
   pl_renderer.h, v0.1 (WIP)
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] defines
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
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_DECLARE_STRUCT
    #define PL_DECLARE_STRUCT(name) typedef struct _ ## name  name
#endif

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
PL_DECLARE_STRUCT(plRenderer);
PL_DECLARE_STRUCT(plAssetRegistry);

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