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
typedef struct _plImageOpData plImageOpData;
typedef struct _plImageOpInfo plImageOpInfo;

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plImageOpsI
{
    void (*initialize)(plImageOpInfo*, plImageOpData* dataOut);
    void (*cleanup)(plImageOpData*);

    // building operations
    void (*add)(plImageOpData*, plImageOpInfo, uint32_t uXOffset, uint32_t uYOffset);

    // in-place place operations
    void (*upsample)(plImageOpData* dataIn, uint32_t uFactor);
    void (*downsample)(plImageOpData* dataIn, uint32_t uFactor);
    
    // misc.
    void (*extract)(plImageOpData* dataIn, uint32_t uXOffset, uint32_t uYOffset, uint32_t uWidth, uint32_t uHeight, plImageOpData* dataOut);

} plImageOpsI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plImageOpInfo
{
    uint32_t uWidth;
    uint32_t uHeight;
    uint8_t  uChannels;
    uint8_t  uStride; // bytes
    uint8_t* puData;
} plImageOpInfo;

typedef struct _plImageOpData
{
    uint64_t uDataSize;
    uint8_t* puData;

    uint32_t uWidth;
    uint32_t uHeight;
    uint8_t  uChannels;
    uint8_t  uStride; // bytes
    uint8_t  uChannelStride; // bytes
} plImageOpData;

#endif // PL_IMAGE_OPS_EXT_H