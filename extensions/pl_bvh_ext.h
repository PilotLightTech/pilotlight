/*
   pl_bvh_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*

    Implementation:
        The provided implementation of this extension depends on the following
        APIs being available:

        unstable APIs:
        * plCollisionI
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_BVH_EXT_H
#define PL_BVH_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plBVHI_version {0, 2, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
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
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_bvh_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_bvh_ext(plApiRegistryI*, bool reload);

// setup/shutdown
PL_API void pl_bvh_cleanup              (plBVH*);

// basic usage (stable)
PL_API void pl_bvh_build                (plBVH*, const plAABB*, uint32_t count);

// intersects (stable)
PL_API void pl_bvh_intersects_aabb      (plBVH*, plAABB, plBVHCallback, void* userData);
PL_API bool pl_bvh_intersects_aabb_first(plBVH*, plAABB, plBVHCallback, void* userData);

// helpers
PL_API bool pl_bvh_traverse             (plBVH*, plBVHNode** nodeOut, uint32_t* indexOut);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
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

#ifdef __cplusplus
}
#endif

#endif // PL_BVH_EXT_H