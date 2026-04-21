/*
   pl_rect_pack_ext.h
     - simple rectangle packer
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] APIs
// [SECTION] public api
// [SECTION] public api struct
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_RECT_PACK_EXT_H
#define PL_RECT_PACK_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plRectPackI_version {2, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

typedef struct _plPackRect plPackRect;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_rect_pack_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_rect_pack_ext(plApiRegistryI*, bool reload);

PL_API void pl_rect_pack_pack(int width, int height, plPackRect*, uint32_t rectCount);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plRectPackI
{
    void (*pack)(int width, int height, plPackRect*, uint32_t rectCount);
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

#ifdef __cplusplus
}
#endif

#endif // PL_RECT_PACK_EXT_H