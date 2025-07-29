/*
   pl_dxt_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api struct
// [SECTION] structs
// [SECTION] enums
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_DXT_EXT_H
#define PL_DXT_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plDxtI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>  // uint*_t
#include <stddef.h>  // size_t

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plDxtInfo plDxtInfo;

// flags/enums
typedef int plDxtFlags;

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plDxtI
{
    void (*compress)(const plDxtInfo*, uint8_t* dataOut, size_t* sizeOut);
} plDxtI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plDxtInfo
{
    plDxtFlags     tFlags;
    uint32_t       uWidth;
    uint32_t       uHeight;
    uint32_t       uChannels; // currently limited to 3 & 4
    const uint8_t* puData;
} plDxtInfo;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plDxtFlags
{
    PL_DXT_FLAGS_NONE         = 0,
    PL_DXT_FLAGS_HIGH_QUALITY = 1 << 0
};

#endif // PL_DXT_EXT_H