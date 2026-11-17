/*
   pl_ecs_ext.inl
     - entity & type key types (include without pulling in the full extension header)
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

#ifndef PL_ECS_EXT_INL
#define PL_ECS_EXT_INL

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

typedef uint32_t plEcsTypeKey;
typedef uint64_t plEntityId; // persistent (serialized) entity identifier

typedef union _plEntity
{
    struct
    {
        uint32_t uIndex;
        uint32_t uGeneration;
    };
    uint64_t uData;
} plEntity;

#ifdef __cplusplus
}
#endif

#endif // PL_ECS_EXT_INL