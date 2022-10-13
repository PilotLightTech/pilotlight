/*
   metal_pl.h
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] globals
*/

#ifndef METAL_PL_H
#define METAL_PL_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "metal_pl_graphics.h"

//-----------------------------------------------------------------------------
// [SECTION] globals
//-----------------------------------------------------------------------------

typedef struct plAppData_t
{
    plMetalDevice   device;
    plMetalGraphics graphics;
    bool            running;
    int             actualWidth;
    int             actualHeight;
    int             clientWidth;
    int             clientHeight;
} plAppData;

#endif // METAL_PL_H