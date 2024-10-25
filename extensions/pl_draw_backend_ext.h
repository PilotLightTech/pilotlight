/*
   pl_draw_backend_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] includes
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_DRAW_BACKEND_EXT_H
#define PL_DRAW_BACKEND_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define PL_API_DRAW_BACKEND "PL_API_DRAW_BACKEND"
typedef struct _plDrawBackendI plDrawBackendI;

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

typedef struct _plFontAtlas   plFontAtlas;   // pl_draw_ext.h
typedef struct _plDrawList2D  plDrawList2D;  // pl_draw_ext.h
typedef struct _plDrawList3D  plDrawList3D;  // pl_draw_ext.h
typedef int    plDrawFlags;                  // pl_draw_ext.h

typedef struct _plDevice        plDevice;        // pl_graphics_ext.h
typedef struct _plRenderEncoder plRenderEncoder; // pl_graphics_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plDrawBackendI
{
    // init/cleanup
    void (*initialize)(plDevice*);
    void (*cleanup)   (void);

    void (*new_frame)(void);

    bool (*build_font_atlas)  (plFontAtlas*);
    void (*cleanup_font_atlas)(plFontAtlas*);

    void (*submit_2d_drawlist)(plDrawList2D*, plRenderEncoder*, float fWidth, float fHeight, uint32_t uMSAASampleCount);
    void (*submit_3d_drawlist)(plDrawList3D*, plRenderEncoder*, float fWidth, float fHeight, const plMat4* ptMVP, plDrawFlags, uint32_t uMSAASampleCount);

} plDrawBackendI;

#endif // PL_DRAW_BACKEND_EXT_H