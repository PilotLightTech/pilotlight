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
#include <stdbool.h>
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plBVH     plBVH;
typedef struct _plBVHNode plBVHNode;

// callback
typedef bool (*plBVHCallback)(uint32_t index, void* userData);

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plBVHI
{
    // setup/shutdown
    void (*cleanup)(plBVH*);

    // basic usage (stable)
    void (*build)(plBVH*, const plAABB*, uint32_t count);
    
    // intersects (stable)
    void (*intersects_aabb)      (plBVH*, plAABB, plBVHCallback, void* userData);
    bool (*intersects_aabb_first)(plBVH*, plAABB, plBVHCallback, void* userData);

    // helpers
    bool (*traverse)(plBVH*, plBVHNode** nodeOut, uint32_t* indexOut);
} plBVHI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plBVHNode
{
    plAABB   tAABB;

    // [INTERNAL]
    uint32_t _uLeft;   // if not leaf, index into left node (+1 for right node)
    uint32_t _uOffset; // offset into leaf indices
    uint32_t _uCount;  // leaf node if > 0
    uint32_t _uTraverseOffset;
} plBVHNode;

typedef struct _plBVH
{
    // [INTERNAL]
    plBVHNode*  _ptNodes;
    uint32_t*   _puLeafIndices;
    uint32_t    _uNodeCount;
    uint8_t*    _puAllocation;
    uint32_t    _uLeafCount;
    plBVHNode** _sbtNodeStack;
} plBVH;

#endif // PL_BVH_EXT_H