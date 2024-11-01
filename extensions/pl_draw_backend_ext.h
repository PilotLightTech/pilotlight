/*
   pl_draw_backend_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] includes
// [SECTION] public api struct
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_DRAW_BACKEND_EXT_H
#define PL_DRAW_BACKEND_EXT_H

// extension version (format XYYZZ)
#define PL_DRAW_BACKEND_EXT_VERSION    "1.0.0"
#define PL_DRAW_BACKEND_EXT_VERSION_NUM 10000

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

// external
typedef struct _plFontAtlas     plFontAtlas;       // pl_draw_ext.h
typedef struct _plDrawList2D    plDrawList2D;      // pl_draw_ext.h
typedef struct _plDrawList3D    plDrawList3D;      // pl_draw_ext.h
typedef int    plDrawFlags;                        // pl_draw_ext.h
typedef struct _plDevice        plDevice;          // pl_graphics_ext.h
typedef struct _plRenderEncoder plRenderEncoder;   // pl_graphics_ext.h
typedef struct _plCommandBuffer plCommandBuffer;   // pl_graphics_ext.h
typedef union plBindGroupHandle plBindGroupHandle; // pl_graphics_ext.h
typedef union plTextureHandle   plTextureHandle;   // pl_graphics_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plDrawBackendI
{
    // init/cleanup
    void (*initialize)(plDevice*);
    void (*cleanup)   (void);

    void (*new_frame)(void);

    bool (*build_font_atlas)  (plCommandBuffer*, plFontAtlas*);
    void (*cleanup_font_atlas)(plFontAtlas*);

    plBindGroupHandle (*create_bind_group_for_texture)(plTextureHandle);

    void (*submit_2d_drawlist)(plDrawList2D*, plRenderEncoder*, float fWidth, float fHeight, uint32_t sampleCount);
    void (*submit_3d_drawlist)(plDrawList3D*, plRenderEncoder*, float fWidth, float fHeight, const plMat4* ptMVP, plDrawFlags, uint32_t sampleCount);

} plDrawBackendI;

#endif // PL_DRAW_BACKEND_EXT_H