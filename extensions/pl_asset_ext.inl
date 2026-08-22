/*
   pl_asset_ext.inl
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

#ifndef PL_ASSET_EXT_INL
#define PL_ASSET_EXT_INL

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>  // uint*_t

//-----------------------------------------------------------------------------
// [SECTION] types
//-----------------------------------------------------------------------------

typedef union _plAssetHandle
{
    struct
    {
        uint32_t uGeneration;
        uint32_t uIndex;
    };
    uint64_t uData;
} plAssetHandle;

#ifdef __cplusplus
}
#endif

#endif // PL_ASSET_EXT_INL