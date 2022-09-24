/*
   metal_pl.h
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] globals
*/

#ifndef APPLE_PL_H
#define APPLE_PL_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "metal_pl_graphics.h"

//-----------------------------------------------------------------------------
// [SECTION] globals
//-----------------------------------------------------------------------------

plMetalDevice   gDevice       = {0};
plMetalGraphics gGraphics     = {0};
bool            gRunning      = true;
int             gActualWidth  = 0;
int             gActualHeight = 0;
int             gClientWidth  = 500;
int             gClientHeight = 500;

#endif // APPLE_PL_H