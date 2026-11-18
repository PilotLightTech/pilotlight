/*
   pl_voxel_ext.h
     - CPU voxelization and compact occupancy grids.
       Core operations need plMemoryI;
       scene helpers additionally need ECS, asset, transform and renderer APIs.
       Debug drawing needs plDrawI. No GPU needed for core operations.
       Zero initialize grids, do not shallow copy them, and call cleanup().
       Mutations require exclusive access; immutable grids support concurrent reads.
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public apis
// [SECTION] public api structs
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_VOXEL_EXT_H
#define PL_VOXEL_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plVoxelI_version {1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include "pl_math.h"
#include <stdbool.h> // bool
#include <stdint.h>  // uint*_t
#include <stddef.h>  // size_t

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plVoxelGrid   plVoxelGrid;
typedef struct _plVoxelCoord  plVoxelCoord;
typedef struct _plVoxelRayHit plVoxelRayHit;

// external
typedef struct _plComponentLibrary plComponentLibrary; // pl_ecs_ext.h
typedef struct _plMesh             plMesh;             // pl_mesh_ext.h
typedef struct _plDrawList3D       plDrawList3D;       // pl_draw_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public apis
//-----------------------------------------------------------------------------

PL_API void pl_load_voxel_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_voxel_ext(plApiRegistryI*, bool reload);

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plVoxelI
{
    // Upper bounds round OUT to whole cells. init creates an empty grid.
    // Invalid input/allocation failure preserves the previous grid.
    // Each dimension <= INT_MAX; total cells <= UINT32_MAX; rounded bounds finite.
    bool (*init)   (plVoxelGrid*, plAABB bounds, float voxelSize);
    void (*cleanup)(plVoxelGrid*); // NULL/repeated cleanup is safe
    void (*clear)  (plVoxelGrid*); // retains dimensions and allocation

    // Point ownership is half-open [min, max). Failure leaves coordOut unchanged.
    bool   (*world_to_coord)(const plVoxelGrid*, plVec3, plVoxelCoord* coordOut);
    plVec3 (*coord_to_world)(const plVoxelGrid*, plVoxelCoord); // center; invalid returns zero
    bool   (*is_valid)      (const plVoxelGrid*, plVoxelCoord);
    bool   (*check)         (const plVoxelGrid*, plVoxelCoord); // invalid/outside is false
    void   (*set)           (plVoxelGrid*, plVoxelCoord, bool occupied); // outside is a no-op

    // Conservative CLOSED-cell overlap: face/edge/corner contacts count on both
    // sides of a boundary. Finite degenerate shapes are supported. Invalid shapes
    // are ignored; off-grid portions are clipped. Subtraction clears touched bits;
    // overlapping objects are NOT reference counted.
    void (*inject_aabb)    (plVoxelGrid*, plAABB, bool subtract);
    void (*inject_triangle)(plVoxelGrid*, plVec3, plVec3, plVec3, bool subtract);
    void (*inject_sphere)  (plVoxelGrid*, plVec3 center, float radius, bool subtract);

    // Surface voxelization; indexed/nonindexed triangle lists. NULL matrix =
    // identity; otherwise an affine transform is required. Range is clamped;
    // count zero does nothing. Invalid triangles and incomplete tails are skipped.
    // Returns finite, valid triangles submitted (including off-grid triangles),
    // saturating at UINT32_MAX. Both helpers ADD occupancy; neither clears it.
    uint32_t (*inject_mesh)(plVoxelGrid*, const plMesh*, const plMat4*, uint32_t firstSubmesh, uint32_t submeshCount);

    // Static renderable mesh snapshot. Update transforms/hierarchy first.
    // No skinning, terrain, water or foliage. Missing dependencies return zero.
    uint32_t (*voxelize_scene)(plVoxelGrid*, const plComponentLibrary*);

    // Fills empty cells not 6-connected to an empty boundary cell. Can fill sealed
    // rooms too; use only when that is intended. Temporary bitset + 4 bytes/cell
    // queue. Allocation failure leaves occupancy unchanged. Synchronous.
    bool (*fill_interior)(plVoxelGrid*);

    // maxVoxels limits boxes, not cells inspected. Negative slice = all Y slices.
    // Draws grid bounds even when maxVoxels is zero. Returns occupied boxes drawn.
    uint32_t (*debug_draw)(const plVoxelGrid*, plDrawList3D*, uint32_t color, int ySlice, uint32_t maxVoxels);

    // Deep copy into a zero-initialized or live destination. Failure preserves it;
    // self-copy succeeds. Source must be initialized. No shared storage afterward.
    bool     (*copy)          (plVoxelGrid* destination, const plVoxelGrid* source);
    plAABB   (*get_bounds)    (const plVoxelGrid*); // rounded grid bounds; invalid returns zero
    uint32_t (*count_occupied)(const plVoxelGrid*); // valid cells only; invalid returns zero

    // First occupied cell touched by the CLOSED world-space segment [start, end].
    // Allocation-free supercover traversal, including edge/corner grazing and
    // rays lying on cell faces. Endpoints may be outside. A zero-length segment
    // tests every cell touching that point. Equal-time hit order is unspecified.
    // Optional hitOut is unchanged on miss/invalid input. This is a point ray,
    // not an agent-body sweep; empty cells need not have ground support.
    bool (*raycast)(const plVoxelGrid*, plVec3 start, plVec3 end, plVoxelRayHit* hitOut);
} plVoxelI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plVoxelCoord
{
    int x;
    int y;
    int z;
} plVoxelCoord;

typedef struct _plVoxelGrid
{
    // Metadata is read-only to callers; modify occupancy through the API.
    plVec3   tMin;             // world-space lower corner; Y is up
    float    fVoxelSize;       // full cell side length, in world units
    uint32_t auResolution[3];
    uint32_t uCellCount;
    size_t   szMemory;         // one bit/cell, rounded up to whole uint64_t words

    // [INTERNAL]
    uint64_t* _puBits;
} plVoxelGrid;

typedef struct _plVoxelRayHit
{
    plVoxelCoord tCoord;
    plVec3       tPosition;
    double       dFraction; // [0, 1] along start -> end; zero when starting in solid
} plVoxelRayHit;

#ifdef __cplusplus
}
#endif

#endif // PL_VOXEL_EXT_H
