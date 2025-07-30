/*
   pl_dxt_ext.h
*/

/*
Index of this file:
// [SECTION] quick notes
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api struct
// [SECTION] structs
// [SECTION] enums
*/

//-----------------------------------------------------------------------------
// [SECTION] quick notes
//-----------------------------------------------------------------------------

/*
    The following channel counts correspond to following formats:
      
      * 1 channel -> PL_FORMAT_BC4_*
      * 2 channel -> PL_FORMAT_BC5_*
      * 3 channel -> PL_FORMAT_BC1_*
      * 4 channel -> PL_FORMAT_BC2_*
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_DXT_EXT_H
#define PL_DXT_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plDxtI_version {0, 2, 0}

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
    uint32_t       uChannels;
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