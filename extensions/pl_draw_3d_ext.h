/*
   pl_draw_3d_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] defines
// [SECTION] apis
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api struct
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_DRAW_3D_EXT_H
#define PL_DRAW_3D_EXT_H

#define PL_DRAW_3D_EXT_VERSION    "0.1.0"
#define PL_DRAW_3D_EXT_VERSION_NUM 000100

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define PL_API_DRAW_3D "PL_API_DRAW_3D"
typedef struct _plDraw3dI plDraw3dI;

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_MAX_3D_DRAWLISTS
    #define PL_MAX_3D_DRAWLISTS 64
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plDraw3dContext     plDraw3dContext;
typedef struct _plDrawList3D        plDrawList3D;
typedef struct _plDrawVertex3DSolid plDrawVertex3DSolid; // single vertex (3D pos + uv + color)
typedef struct _plDrawVertex3DLine  plDrawVertex3DLine; // single vertex (pos + uv + color)

// enums
typedef int pl3DDrawFlags;

// external
typedef struct _plGraphics      plGraphics;      // pl_graphics_ext.h
typedef struct _plRenderEncoder plRenderEncoder; // pl_graphics_ext.h

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _pl3DDrawFlags
{
    PL_PIPELINE_FLAG_NONE          = 0,
    PL_PIPELINE_FLAG_DEPTH_TEST    = 1 << 0,
    PL_PIPELINE_FLAG_DEPTH_WRITE   = 1 << 1,
    PL_PIPELINE_FLAG_CULL_FRONT    = 1 << 2,
    PL_PIPELINE_FLAG_CULL_BACK     = 1 << 3,
    PL_PIPELINE_FLAG_FRONT_FACE_CW = 1 << 4,
};

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plDraw3dI
{
    // init/cleanup
    void (*initialize)(plGraphics*);
    void (*cleanup)   (void);

    // setup
    plDrawList3D* (*request_drawlist)(void);
    void          (*return_drawlist)(plDrawList3D*);

    // per frame
    void (*new_frame)(void);
    void (*submit_drawlist)(plDrawList3D*, plRenderEncoder, float fWidth, float fHeight, const plMat4* ptMVP, pl3DDrawFlags, uint32_t uMSAASampleCount);

    // drawing
    void (*add_triangle_filled)(plDrawList3D*, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec4 tColor);
    void (*add_line)           (plDrawList3D*, plVec3 tP0, plVec3 tP1, plVec4 tColor, float fThickness);
    void (*add_point)          (plDrawList3D*, plVec3 tP0, plVec4 tColor, float fLength, float fThickness);
    void (*add_transform)      (plDrawList3D*, const plMat4* ptTransform, float fLength, float fThickness);
    void (*add_frustum)        (plDrawList3D*, const plMat4* ptTransform, float fYFov, float fAspect, float fNearZ, float fFarZ, plVec4 tColor, float fThickness);
    void (*add_centered_box)   (plDrawList3D*, plVec3 tCenter, float fWidth, float fHeight, float fDepth, plVec4 tColor, float fThickness);
    void (*add_aabb)           (plDrawList3D*, plVec3 tMin, plVec3 tMax, plVec4 tColor, float fThickness);
    void (*add_bezier_quad)    (plDrawList3D*, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec4 tColor, float fThickness, uint32_t uSegments);
    void (*add_bezier_cubic)   (plDrawList3D*, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec3 tP3, plVec4 tColor, float fThickness, uint32_t uSegments);
} plDraw3dI;

#endif // PL_DRAW_3D_EXT_H