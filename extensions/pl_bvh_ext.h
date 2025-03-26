/*
   pl_bvh_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api structs
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_BVH_EXT_H
#define PL_BVH_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plBVHI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plBVH     plBVH;
typedef struct _plBVHNode plBVHNode;

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plBVHI
{
    void (*build)  (plBVH*, const plAABB*, uint32_t count);
    void (*cleanup)(plBVH*);
} plBVHI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plBVHNode
{
    plAABB   tAABB;
    uint32_t uLeft;
    uint32_t uOffset;
    uint32_t uCount;
} plBVHNode;

typedef struct _plBVH
{
    uint8_t*   puAllocation;
    plBVHNode* ptNodes;
    uint32_t*  puLeafIndices;
    uint32_t   uNodeCount;
    uint32_t   uLeafCount;
} plBVH;

#endif // PL_BVH_EXT_H