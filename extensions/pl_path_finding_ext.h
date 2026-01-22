/*
   pl_path_finding_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api structs
// [SECTION] structs
// [SECTION] enums
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*
   A standalone, voxel-based 3D pathfinding extension for Pilot Light.
   Inspired by Wicked Engine's voxel pathfinding implementation, also
   inspired by Sebastian Lague "A* Pathfinding" series on Youtube
   
   Design Goals:
   - Zero external dependencies (except pilot light math)
   - Fast real-time pathfinding for game agents
   - Support both ground-based and flying agents
   - Simple, clean API
   
   Implementation:
        The provided implementation of this extension depends on the following
        APIs being available:

        * plMath (v1.x)

*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_PATH_FINDING_EXT_H
#define PL_PATH_FINDING_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plPathFindingI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>  // uint
#include <stdbool.h> // bool
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

typedef struct _plPathFindingVoxelGrid plPathFindingVoxelGrid;
typedef struct _plPathFindingQuery     plPathFindingQuery;
typedef struct _plPathFindingResult    plPathFindingResult;
typedef struct _plPathNode             plPathNode;

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plPathFindingI
{
    // grid management
    plPathFindingVoxelGrid* (*create_voxel_grid)(uint32_t uDimX, uint32_t uDimY, uint32_t uDimZ, float fVoxelSize, plVec3 tOrigin);
    void                    (*destroy_voxel_grid)(plPathFindingVoxelGrid* ptGrid);
    void                    (*set_voxel)(plPathFindingVoxelGrid* ptGrid, uint32_t uX, uint32_t uY, uint32_t uZ, bool bOccupied);
    bool                    (*is_voxel_occupied)(const plPathFindingVoxelGrid* ptGrid, uint32_t uX, uint32_t uY, uint32_t uZ);
    void                    (*clear_grid)(plPathFindingVoxelGrid* ptGrid);

    // mesh voxelization
    void (*voxelize_mesh)(plPathFindingVoxelGrid* ptGrid, const float* pfVertices, uint32_t uVertexCount, const uint32_t* puIndices, uint32_t uIndexCount);

    // pathfinding queries
    plPathFindingResult (*find_path)(const plPathFindingVoxelGrid* ptGrid, const plPathFindingQuery* ptQuery, bool bSearchDiagonal);
    void                (*free_result)(plPathFindingResult* ptResult);

    // coordinate conversion utilities
    void   (*world_to_voxel)(const plPathFindingVoxelGrid* ptGrid, plVec3 tWorldPos, uint32_t* puX, uint32_t* puY, uint32_t* puZ);
    plVec3 (*voxel_to_world)(const plPathFindingVoxelGrid* ptGrid, uint32_t uX, uint32_t uY, uint32_t uZ);
    bool   (*is_valid_voxel)(const plPathFindingVoxelGrid* ptGrid, uint32_t uX, uint32_t uY, uint32_t uZ);

} plPathFindingI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plPathFindingVoxelGrid
{
    uint32_t* apOccupancyBits; // bit array: 1 = occupied, 0 = empty
    uint32_t  uDimX;           // grid dimensions in voxels
    uint32_t  uDimY;
    uint32_t  uDimZ;
    float     fVoxelSize;      // size of each voxel in world units
    plVec3    tOrigin;         // world space origin (bottom left back corner)
    uint32_t  uBitArraySize;   // size of bit array in uint32_t elements
} plPathFindingVoxelGrid;

typedef struct _plPathFindingQuery
{
    plVec3   tStart;       // start position in world space
    plVec3   tGoal;        // goal position in world space
    uint32_t uAgentWidth;  // agent width in voxels (for clearance)
    uint32_t uAgentHeight; // agent height in voxels (for clearance)
    bool     bFlying;      // true = flying agent, false = ground agent
} plPathFindingQuery;

typedef struct _plPathFindingResult
{
    plVec3*  atWaypoints;    // array of waypoint positions in world space
    uint32_t uWaypointCount; // number of waypoints in path
    bool     bSuccess;       // true if path was found
    float    fPathLength;    // total path length in world units
} plPathFindingResult;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

// reserved for future use (path smoothing options, neighbor connectivity, etc.)


#endif // PL_PATH_FINDING_EXT_H