/*
   pl_image_ops_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] APIs
// [SECTION] forward declarations & basic types
// [SECTION] public api struct
// [SECTION] structs
// [SECTION] enums
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_IMAGE_OPS_EXT_H
#define PL_IMAGE_OPS_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plImageOpsI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plImageOpData   plImageOpData;
typedef struct _plImageOpInit   plImageOpInit;
typedef struct _plImageOpRegion plImageOpRegion; // internal

// enums
typedef int plImageOpFlags;
typedef int plImageOpColor;

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plImageOpsI
{
    void (*initialize)(plImageOpInit*, plImageOpData* dataOut);
    void (*cleanup)   (plImageOpData*);

    // building operations
    void (*add)       (plImageOpData*, int x, int y, uint32_t w, uint32_t h, uint8_t*);
    void (*add_region)(plImageOpData*, int x, int y, uint32_t w, uint32_t h, plImageOpColor);

    // in-place place operations
    void (*square)(plImageOpData*);

    // misc.
    uint8_t* (*extract)        (plImageOpData* dataIn, int x, int y, uint32_t w, uint32_t h, uint64_t* sizeOut);
    void     (*cleanup_extract)(uint8_t*);
} plImageOpsI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plImageOpInit
{
    uint32_t uVirtualWidth;
    uint32_t uVirtualHeight;
    uint8_t  uChannels;
    uint8_t  uStride; // bytes
} plImageOpInit;

typedef struct _plImageOpData
{
    // virtual region
    uint32_t uVirtualWidth;
    uint32_t uVirtualHeight;

    // active region
    uint32_t uActiveXOffset;
    uint32_t uActiveYOffset;
    uint32_t uActiveWidth;
    uint32_t uActiveHeight;

    // [INTERNAL]
    plImageOpRegion* _atRegions;
    uint32_t         _uRegionCount;
    uint32_t         _uRegionCapacity;
    uint8_t          _uChannels;
    uint8_t          _uStride; // bytes
    uint8_t          _uChannelStride; // bytes
} plImageOpData;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plImageOpFlags
{
    PL_IMAGE_OP_FLAGS_NONE = 0
};

enum _plImageOpColor
{
    PL_IMAGE_OP_COLOR_TRANSPARENT = 0,
    PL_IMAGE_OP_COLOR_WHITE,
};

#endif // PL_IMAGE_OPS_EXT_H