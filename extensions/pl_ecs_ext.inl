/*
   pl_ecs_ext.inl
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

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>  // uint*_t

//-----------------------------------------------------------------------------
// [SECTION] types
//-----------------------------------------------------------------------------

typedef uint32_t plEcsTypeKey;

typedef union _plEntity
{
    struct
    {
        uint32_t uIndex;
        uint32_t uGeneration;
    };
    uint64_t uData;
} plEntity;

typedef struct _plPackedEntity
{
    plEntity                    tEntity;
    struct _plComponentLibrary* ptLibrary;
} plPackedEntity;


#endif // PL_ECS_EXT_INL