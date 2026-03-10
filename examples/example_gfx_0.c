/*
   example_gfx_0.c
     - demonstrates loading APIs
     - demonstrates loading extensions
     - demonstrates hot reloading
     - demonstrates starter extension
     - demonstrates vertex buffers
     - demonstrates shaders
     - demonstrates non-index drawing
*/

/*
Index of this file:
// [SECTION] quick notes
// [SECTION] includes
// [SECTION] structs
// [SECTION] apis
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
*/

//-----------------------------------------------------------------------------
// [SECTION] quick notes
//-----------------------------------------------------------------------------

/*

    WARNING:

    The purpose of the graphics extension is NOT to make low level graphics
    programming easier.
    
    The purpose of the graphics extension is NOT to be an abstraction for the
    sake of abstraction.

    The graphics extension does not hold your hand. You are expected to be
    familar with either Vulkan or Metal 3.0 concepts.

    These examples mostly assume you understand low level graphics and will
    not attempt to explain those concepts (i.e. what is a vertex buffer?)

    BACKGROUND:

    The graphics extension is meant to be an extremely lightweight abstraction
    over the "modern" explicit graphics APIs (Vulkan/DirectX 12/Metal 3.0).
    Ideally it should be 1 to 1 when possible. The explicit control provided
    by these APIs are their power, so the extension tries to preserve that
    as much as possible while also allowing a graphics programmer the ability
    to write cross platform graphics code without too much consideration of
    the differences between the APIs. This is accomplished by careful
    consideration of the APIs and their common concepts and features. In some
    cases, the API is required to be stricter than the underlying API. For
    example, Vulkan makes it easy to issue draw calls and compute dispatches
    directly to command buffers while Metal requires these to be submitted
    to different "encoders" which can't be recording simutaneously. So the
    graphics extension introduces the concept of encoder to match the stricter
    API at the cost of some freedom Vulkan would normally allow. There is only
    a few cases like this but we will note them in these examples.

    EXAMPLE:
    
    This example introduces the graphics and shader extensions. It will also
    utilize the starter extension introduced in the "basic" examples. Yes, one
    of the starter extension's main goals is to help with graphics extension
    but for learning purposes it will help to narrow the scope of each example
    since the graphics API contains alot of boilerplate and is very verbose.
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdlib.h> // malloc, free
#include <stdio.h>
#include <string.h> // memset
#include "pl.h"
#include "pl_memory.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extensions
#include "pl_graphics_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAppData
{
    // window
    plWindow*  ptWindow;
    plSurface* ptSurface;
    plDevice*  ptDevice;

    // buffers
    plBufferHandle tVertexBuffer;
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

const plIOI*       gptIO      = NULL;
const plWindowI*   gptWindows = NULL;
const plGraphicsI* gptGfx     = NULL;

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryI* ptApiRegistry, plAppData* ptAppData)
{
    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(ptAppData)
    {

        gptIO      = pl_get_api_latest(ptApiRegistry, plIOI);
        gptWindows = pl_get_api_latest(ptApiRegistry, plWindowI);
        gptGfx     = pl_get_api_latest(ptApiRegistry, plGraphicsI);

        return ptAppData;
    }

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = malloc(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    // retrieve extension registry
    const plExtensionRegistryI* ptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);

    // load extensions
    ptExtensionRegistry->load("pl_unity_ext", NULL, NULL, true);
    ptExtensionRegistry->load("pl_platform_ext", NULL, NULL, false);
    
    // load required apis
    gptIO      = pl_get_api_latest(ptApiRegistry, plIOI);
    gptWindows = pl_get_api_latest(ptApiRegistry, plWindowI);

    // load required apis (these are provided though extensions)
    gptGfx     = pl_get_api_latest(ptApiRegistry, plGraphicsI);


    // use window API to create a window
    plWindowDesc tWindowDesc = {
        .pcTitle = "Example GFX 0",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 600,
        .uHeight = 600,
    };
    gptWindows->create(tWindowDesc, &ptAppData->ptWindow);
    gptWindows->show(ptAppData->ptWindow);


    plSurface* ptSurface = gptGfx->create_surface(ptAppData->ptWindow);
    ptAppData->ptSurface = ptSurface;

    const plDeviceInit tDeviceInit = {
        .uDeviceIdx = 0,
        .szDynamicBufferBlockSize = 65536,
        .szDynamicDataMaxSize = 65536,
        .ptSurface = ptSurface
    };
    ptAppData->ptDevice = gptGfx->create_device(&tDeviceInit);

    const plBufferDesc tBufferDesc = {
        .pcDebugName = "test buffer",
        .szByteSize = sizeof(float) * 4,
        .tUsage = PL_BUFFER_USAGE_VERTEX | PL_BUFFER_USAGE_TRANSFER_DESTINATION
    };

    gptGfx->create_buffer(ptAppData->ptDevice, &tBufferDesc, NULL);
    
    plBuffer* ptVertexBuffer = gptGfx->get_buffer(ptAppData->ptDevice, ptAppData->tVertexBuffer);


    // return app memory
    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    free(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plWindow* ptWindow, plAppData* ptAppData)
{

}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{

}
