/*
   metal_pl_graphics.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] forward declarations
// [SECTION] structs
*/

#ifndef PL_GRAPHICS_METAL_H
#define PL_GRAPHICS_METAL_H

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#include "pilotlight.h"
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

PL_DECLARE_STRUCT(plMetalDevice);   // device resources & info
PL_DECLARE_STRUCT(plMetalGraphics); // graphics context

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plMetalDevice
{
    id<MTLDevice> device;
} plMetalDevice;

typedef struct _plMetalGraphics
{
    id<MTLCommandQueue> cmdQueue;
    uint32_t            currentFrame;
    CAMetalLayer*       metalLayer; 
} plMetalGraphics;

#endif // PL_GRAPHICS_METAL_H