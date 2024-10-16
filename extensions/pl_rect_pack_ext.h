/*
   pl_rect_pack_ext.h
     - simple rectangle packer
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] APIs
// [SECTION] public api structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_RECT_PACK_EXT_H
#define PL_RECT_PACK_EXT_H

// extension version (format XYYZZ)
#define PL_RECT_PACK_EXT_VERSION    "1.0.0"
#define PL_RECT_PACK_EXT_VERSION_NUM 10000

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define PL_API_RECT_PACK "PL_API_RECT_PACK"
typedef struct _plRectPackI plRectPackI;

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

typedef struct _plPackRect plPackRect;

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plRectPackI
{
    void (*pack_rects)(int iWidth, int iHeight, plPackRect*, uint32_t uRectCount);
} plRectPackI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plPackRect
{
    // reserved for your use
    int iId;

    // input
    int iWidth;
    int iHeight;

    // output
    int  iX;
    int  iY;
    int iWasPacked; // non-zero if valid packing
} plPackRect;

#endif // PL_RECT_PACK_EXT_H