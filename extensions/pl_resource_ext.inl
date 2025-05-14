/*
   pl_resource_ext.inl
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] types
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_RESOURCE_EXT_INL
#define PL_RESOURCE_EXT_INL

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>  // uint*_t

//-----------------------------------------------------------------------------
// [SECTION] types
//-----------------------------------------------------------------------------

typedef union _plResourceHandle
{
    struct
    {
        uint32_t uType       : 10;
        uint32_t uGeneration : 22;
        uint32_t uIndex      : 32;
    };
    uint64_t ulData;
} plResourceHandle;

#endif // PL_RESOURCE_EXT_INL