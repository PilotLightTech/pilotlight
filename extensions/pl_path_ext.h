/*
   pl_path_ext.h
     - bounded A* queries on plVoxelGrid.
       Dependencies: plMemoryI, plVoxelI;
       debug drawing additionally uses plDrawI. No renderer or GPU needed to query.
       Queries own reusable scratch/result storage; zero initialize and cleanup.
       One query per worker, immutable grid for the entire begin/step lifetime.
       No concurrent calls on the same query. No global query state.
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api structs
// [SECTION] structs
// [SECTION] enums
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_PATH_EXT_H
#define PL_PATH_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plPathI_version {1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h> // bool
#include <stdint.h>  // uint*_t

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plPathSettings plPathSettings;
typedef struct _plPathQuery    plPathQuery;

// enums & flags
typedef int plPathStatus;
typedef int plPathGroundSupport;

// external
typedef struct _plDrawList3D plDrawList3D; // pl_draw_ext.h
typedef struct _plVoxelGrid  plVoxelGrid;  // pl_voxel_ext.h
typedef struct _plVoxelCoord plVoxelCoord; // pl_voxel_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public apis
//-----------------------------------------------------------------------------

PL_API void pl_load_path_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_path_ext(plApiRegistryI*, bool reload);

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plPathI
{
    // Defaults: ground, simplify, height 2, radius 0, step 1, snap 0,
    // 65536 nodes, diagonals enabled, full ground support. NULL settings in
    // process/begin/is_walkable/is_segment_traversable select these defaults.
    plPathSettings (*default_settings)(void);

    // Synchronous. Failure clears prior results. Endpoints outside grid fail;
    // optional snapping searches nearby valid cells and can cross a thin wall.
    // Waypoints are centers of the agent's bottom cell, not exact input positions.
    // Ground support follows tGroundSupport. No jumping/falling.
    plPathStatus (*process)      (plPathQuery*, const plVoxelGrid*, plVec3 start, plVec3 goal, const plPathSettings*);
    bool         (*is_walkable)  (const plVoxelGrid*, plVoxelCoord, const plPathSettings*);

    // Replaces any previous query; copies settings, borrows grid until terminal
    // status/cancel/cleanup. Caller must keep grid alive and UNCHANGED throughout.
    // Allocation, scratch reset and endpoint snapping happen synchronously here.
    plPathStatus (*begin)(plPathQuery*, const plVoxelGrid*, plVec3 start, plVec3 goal, const plPathSettings*);

    // At most maxWork units: one node expansion, parent/waypoint, or smoothing
    // grid crossing per unit. This is a work budget, NOT a wall-clock guarantee.
    // Zero budget polls. Terminal calls are no-ops. Results valid only on SUCCESS;
    // waypoint counts stay zero while pending. No allocations after a successful begin().
    plPathStatus (*step)(plPathQuery*, uint32_t maxWork);

    // Clears results, releases borrowed grid, retains scratch for reuse.
    void (*cancel) (plPathQuery*);
    void (*cleanup)(plPathQuery*);

    // Read-only, allocation-free check between CELL CENTERS, as in smoothing.
    // Checks the full body and selected support at every touched cell, including
    // edge/corner contacts. Ground endpoints must have equal Y: returns false for
    // steps/slopes/lift/lower segments, even if process() can find a stepped route.
    // Flying permits arbitrary directions. Not a physics sweep or a check of
    // arbitrary sub-cell world positions. No snapping. Invalid inputs return false.
    // bAllowDiagonal, bSimplify, iMaxStep, iSnapRadius and uMaxNodes do not affect
    // traversal here; the complete settings structure must still be valid.
    bool (*is_segment_traversable)(const plVoxelGrid*, plVoxelCoord start, plVoxelCoord goal, const plPathSettings*);

    // misc.
    const char* (*status_string)(plPathStatus);    
    void        (*debug_draw)   (const plPathQuery*, plDrawList3D*, bool rawPath, bool waypoints); // Cyan raw path, magenta simplified path, green start and red goal.
} plPathI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plPathSettings
{
    bool                bFlying;
    bool                bSimplify;
    int                 iAgentHeight;   // occupied vertical cell span, >= 1
    int                 iAgentRadius;   // [0,1024]; horizontal width = 2 * radius + 1 cells
    int                 iMaxStep;       // [0,64]; ground step up/down, in cells; default 1
    int                 iSnapRadius;    // [0,32]; default 0 (disabled); opt-in snapping can cross walls
    uint32_t            uMaxNodes;      // [1,1048576]; discovered node/scratch memory budget
    bool                bAllowDiagonal; // search only; default true, 8 horizontal / 26 flying neighbors
    plPathGroundSupport tGroundSupport; // ignored for flying
} plPathSettings;

typedef struct _plPathQuery
{
    plPathStatus tStatus;
    plVec3       tStart;         // bottom-cell centers, even with snapping off; valid on success
    plVec3       tGoal;    
    plVec3*      atWaypoints;    // start -> goal; borrowed until begin/process/cancel/cleanup
    uint32_t     uWaypointCount;
    plVec3*      atRawWaypoints; // unsimplified path, including step clearance elbows
    uint32_t     uRawWaypointCount;
    uint32_t     uVisitedNodes;
    uint32_t     uExpandedNodes;
    float        fLength;         // simplified path length in world units

    // [INTERNAL]
    void*    _pScratch;
    uint32_t _uCapacity;
} plPathQuery;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plPathStatus
{
    PL_PATH_STATUS_NONE = 0,
    PL_PATH_STATUS_SUCCESS,
    PL_PATH_STATUS_NO_PATH, // frontier exhausted; no route in this movement graph
    PL_PATH_STATUS_INVALID_START,
    PL_PATH_STATUS_INVALID_GOAL,
    PL_PATH_STATUS_NODE_LIMIT, // discovery budget exhausted; route may still exist
    PL_PATH_STATUS_OUT_OF_MEMORY,
    PL_PATH_STATUS_INVALID_INPUT,
    PL_PATH_STATUS_IN_PROGRESS,
    PL_PATH_STATUS_CANCELLED
};

enum _plPathGroundSupport
{
    PL_PATH_GROUND_SUPPORT_FULL = 0, // support under the entire footprint (default)
    PL_PATH_GROUND_SUPPORT_CENTER    // support under the center; body clearance unchanged
};

#ifdef __cplusplus
}
#endif
#endif
