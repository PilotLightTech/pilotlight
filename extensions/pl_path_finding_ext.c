/*
   pl_path_finding_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal structures
// [SECTION] internal helper functions
// [SECTION] public api implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pl_path_finding_ext.h"
#include "pl.h"

//-----------------------------------------------------------------------------
// [SECTION] internal structures
//-----------------------------------------------------------------------------

// TODO: add internal structures for A* algorithm
//       - priority queue node
//       - closed set node
//       - etc.

//-----------------------------------------------------------------------------
// [SECTION] internal helper functions
//-----------------------------------------------------------------------------

// TODO: add bit manipulation helpers
//       - get bit at index
//       - set bit at index
//       - clear bit at index

static uint32_t
pl_voxel_index(const plPathFindingVoxelGrid* ptGrid, uint32_t uX, uint32_t uY, uint32_t uZ)
{
    return uX + uY * ptGrid->uDimX + uZ * ptGrid->uDimX * ptGrid->uDimY;
}

// TODO: add A* algorithm helpers
//       - heuristic function
//       - neighbor generation
//       - path reconstruction
//       - etc.

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// grid management
//-----------------------------------------------------------------------------

static plPathFindingVoxelGrid*
pl_create_voxel_grid_impl(uint32_t uDimX, uint32_t uDimY, uint32_t uDimZ, float fVoxelSize, plVec3 tOrigin)
{
    plPathFindingVoxelGrid* tVoxelGrid = malloc(sizeof(plPathFindingVoxelGrid));
    if(tVoxelGrid == NULL)
        return NULL; // allocation failed

    uint32_t uTotalVoxels = uDimX * uDimY * uDimZ;
    uint32_t uBitArraySize = (uTotalVoxels + 31) / 32; // round up to always have space 

    tVoxelGrid->apOccupancyBits = malloc(sizeof(uint32_t) * uBitArraySize);
    if(tVoxelGrid->apOccupancyBits == NULL)
        return NULL; // allocation failed 

    memset(tVoxelGrid->apOccupancyBits, 0, sizeof(uint32_t) * uBitArraySize); // set all to unoccupied by default 

    // store values 
    tVoxelGrid->uBitArraySize = uBitArraySize;
    tVoxelGrid->uDimX = uDimX;
    tVoxelGrid->uDimY = uDimY;
    tVoxelGrid->uDimZ = uDimZ;
    tVoxelGrid->tOrigin = tOrigin;
    tVoxelGrid->fVoxelSize = fVoxelSize;

    return tVoxelGrid;
}

static void
pl_destroy_voxel_grid_impl(plPathFindingVoxelGrid* ptGrid)
{
    if(ptGrid)
    {
        // free occupancy bits first 
        if(ptGrid->apOccupancyBits != NULL)
            free(ptGrid->apOccupancyBits);
        if(ptGrid != NULL)
            free(ptGrid);
    }
}

static void
pl_set_voxel_impl(plPathFindingVoxelGrid* ptGrid, uint32_t uX, uint32_t uY, uint32_t uZ, bool bOccupied)
{
    if(uX >= ptGrid->uDimX || uY >= ptGrid->uDimY || uZ >= ptGrid->uDimZ)
        return; // out of bounds
    uint32_t uVoxelIndex = pl_voxel_index(ptGrid, uX, uY, uZ); // which voxel
    uint32_t uArrayIndex = uVoxelIndex / 32;                   // which uint32_t
    uint32_t uBitOffset = uVoxelIndex % 32;                    // which bit

    // set bit
    if(bOccupied)
    {
        ptGrid->apOccupancyBits[uArrayIndex] |= (1 << uBitOffset);
    }
    else
    {
        ptGrid->apOccupancyBits[uArrayIndex] &= ~(1 << uBitOffset);
    }
}

static bool
pl_is_voxel_occupied_impl(const plPathFindingVoxelGrid* ptGrid, uint32_t uX, uint32_t uY, uint32_t uZ)
{
    // bounds check
    if(uX >= ptGrid->uDimX || uY >= ptGrid->uDimY || uZ >= ptGrid->uDimZ)
        return false; // out of bounds treated as empty
    
    uint32_t uVoxelIndex = pl_voxel_index(ptGrid, uX, uY, uZ); // which voxel
    uint32_t uArrayIndex = uVoxelIndex / 32;                   // which uint32_t
    uint32_t uBitOffset = uVoxelIndex % 32;                    // which bit

    // test the bit
    return (ptGrid->apOccupancyBits[uArrayIndex] & (1 << uBitOffset)) != 0;
}

static void
pl_clear_grid_impl(plPathFindingVoxelGrid* ptGrid)
{
    if(ptGrid == NULL || ptGrid->apOccupancyBits == NULL)
        return; // safety check

    memset(ptGrid->apOccupancyBits, 0, ptGrid->uBitArraySize * sizeof(uint32_t));
}

//-----------------------------------------------------------------------------
// mesh voxelization
//-----------------------------------------------------------------------------

static void
pl_voxelize_mesh_impl(plPathFindingVoxelGrid* ptGrid, const float* pfVertices, uint32_t uVertexCount, const uint32_t* puIndices, uint32_t uIndexCount)
{
    // TODO: 
}

//-----------------------------------------------------------------------------
// pathfinding queries
//-----------------------------------------------------------------------------

static plPathFindingResult
pl_find_path_impl(const plPathFindingVoxelGrid* ptGrid, const plPathFindingQuery* ptQuery)
{
    // TODO: 
    plPathFindingResult tResult = {0};
    return tResult;
}

static void
pl_free_result_impl(plPathFindingResult* ptResult)
{
    // TODO: 
}

//-----------------------------------------------------------------------------
// coordinate conversion utilities
//-----------------------------------------------------------------------------

static void
pl_world_to_voxel_impl(const plPathFindingVoxelGrid* ptGrid, plVec3 tWorldPos, uint32_t* puOutX, uint32_t* puOutY, uint32_t* puOutZ)
{
    // calculate relative position from origin
    float uRelativeX = tWorldPos.x - ptGrid->tOrigin.x;
    float uRelativeY = tWorldPos.y - ptGrid->tOrigin.y;
    float uRelativeZ = tWorldPos.z - ptGrid->tOrigin.z;
    
    // divide by voxel size and floor
    *puOutX = (uint32_t)floor(uRelativeX / ptGrid->fVoxelSize); // TODO: should the "floor" function be replaced 
    *puOutY = (uint32_t)floor(uRelativeY / ptGrid->fVoxelSize);
    *puOutZ = (uint32_t)floor(uRelativeZ / ptGrid->fVoxelSize);
}

static plVec3
pl_voxel_to_world_impl(const plPathFindingVoxelGrid* ptGrid, uint32_t uX, uint32_t uY, uint32_t uZ)
{
    // get center of voxel from world position coordinates
    plVec3 tResult = {0};
    tResult.x = ptGrid->tOrigin.x + ((float)uX + 0.5f) * ptGrid->fVoxelSize;
    tResult.y = ptGrid->tOrigin.y + ((float)uY + 0.5f) * ptGrid->fVoxelSize;
    tResult.z = ptGrid->tOrigin.z + ((float)uZ + 0.5f) * ptGrid->fVoxelSize;
    return tResult;
}

static bool
pl_is_valid_voxel_impl(const plPathFindingVoxelGrid* ptGrid, uint32_t uX, uint32_t uY, uint32_t uZ)
{
    // check if any coordinate is out of bounds
    if(uX >= ptGrid->uDimX)
        return false;
    if(uY >= ptGrid->uDimY)
        return false;
    if(uZ >= ptGrid->uDimZ)
        return false;
    
    // all coordinates are valid
    return true;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_path_finding_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plPathFindingI tApi = {
        .create_voxel_grid  = pl_create_voxel_grid_impl,
        .destroy_voxel_grid = pl_destroy_voxel_grid_impl,
        .set_voxel          = pl_set_voxel_impl,
        .is_voxel_occupied  = pl_is_voxel_occupied_impl,
        .clear_grid         = pl_clear_grid_impl,
        .voxelize_mesh      = pl_voxelize_mesh_impl,
        .find_path          = pl_find_path_impl,
        .free_result        = pl_free_result_impl,
        .world_to_voxel     = pl_world_to_voxel_impl,
        .voxel_to_world     = pl_voxel_to_world_impl,
        .is_valid_voxel     = pl_is_valid_voxel_impl
    };
    pl_set_api(ptApiRegistry, plPathFindingI, &tApi);
}

PL_EXPORT void
pl_unload_path_finding_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;
    
    const plPathFindingI* ptApi = pl_get_api_latest(ptApiRegistry, plPathFindingI);
    ptApiRegistry->remove_api(ptApi);
}