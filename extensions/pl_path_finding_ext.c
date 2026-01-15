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

typedef struct _plPathNode
{
    uint32_t uX;       // voxel coordinates
    uint32_t uY;
    uint32_t uZ;            
    float    fGCost;   // distance from start
    float    fHCost;   // estimated distance to goal
    float    fFCost;   // g + h (total cost)
    uint32_t uParentX; // for path reconstruction
    uint32_t uParentY;
    uint32_t uParentZ; 
} plPathNode;

typedef struct _plPriorityQueue
{
    plPathNode* atNodes;   // array of nodes
    uint32_t    uCount;    // current number of nodes
    uint32_t    uCapacity; // allocated capacity
} plPriorityQueue;

typedef struct _plClosedSet
{
    uint32_t* auBits;        // bit array: 1 = visited, 0 = not visited
    uint32_t  uBitArraySize; // size in uint32_t elements
} plClosedSet;

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// voxel grid helpers
static uint32_t pl_voxel_index(const plPathFindingVoxelGrid* ptGrid, uint32_t uX, uint32_t uY, uint32_t uZ);
static bool     pl_is_voxel_occupied_impl(const plPathFindingVoxelGrid* ptGrid, uint32_t uX, uint32_t uY, uint32_t uZ);
static bool     pl_is_valid_voxel_impl(const plPathFindingVoxelGrid* ptGrid, uint32_t uX, uint32_t uY, uint32_t uZ);
static void     pl_world_to_voxel_impl(const plPathFindingVoxelGrid* ptGrid, plVec3 tWorldPos, uint32_t* puOutX, uint32_t* puOutY, uint32_t* puOutZ);
static plVec3   pl_voxel_to_world_impl(const plPathFindingVoxelGrid* ptGrid, uint32_t uX, uint32_t uY, uint32_t uZ);

// pathfinding helpers
static float pl_heuristic(uint32_t uFromX, uint32_t uFromY, uint32_t uFromZ, uint32_t uToX, uint32_t uToY, uint32_t uToZ);
static float pl_movement_cost(int32_t iDeltaX, int32_t iDeltaY, int32_t iDeltaZ);
static void  pl_generate_neighbors(const plPathFindingVoxelGrid* ptGrid, uint32_t uCurrentX, uint32_t uCurrentY, uint32_t uCurrentZ, plPathNode* atNeighborsOut, uint32_t* puNeighborCountOut);

// priority queue
static plPriorityQueue* pl_create_priority_queue(uint32_t uInitialCapacity);
static void             pl_destroy_priority_queue(plPriorityQueue* ptQueue);
static bool             pl_pq_is_empty(plPriorityQueue* ptQueue);
static void             pl_pq_push(plPriorityQueue* ptQueue, plPathNode tNode);
static plPathNode       pl_pq_pop(plPriorityQueue* ptQueue);
static plPathNode*      pl_pq_find(plPriorityQueue* ptQueue, uint32_t uX, uint32_t uY, uint32_t uZ);

// closed set
static plClosedSet* pl_create_closed_set(uint32_t uTotalVoxels);
static void         pl_destroy_closed_set(plClosedSet* ptSet);
static void         pl_closed_set_add(plClosedSet* ptSet, uint32_t uVoxelIndex);
static bool         pl_closed_set_contains(plClosedSet* ptSet, uint32_t uVoxelIndex);

// path reconstruction
static plPathFindingResult pl_reconstruct_path(const plPathFindingVoxelGrid* ptGrid, plPathNode tGoalNode, plPathNode* atExploredNodes, uint32_t uExploredCount, uint32_t uStartX, uint32_t uStartY, uint32_t uStartZ);

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

static float
pl_heuristic(uint32_t uFromX, uint32_t uFromY, uint32_t uFromZ, uint32_t uToX, uint32_t uToY, uint32_t uToZ)
{
    int32_t iDeltaX = (int32_t)uToX - (int32_t)uFromX;
    int32_t iDeltaY = (int32_t)uToY - (int32_t)uFromY;
    int32_t iDeltaZ = (int32_t)uToZ - (int32_t)uFromZ;
    
    // euclidean distance: sqrt(deltaX² + deltaY² + deltaZ²)
    return sqrtf((float)(iDeltaX*iDeltaX + iDeltaY*iDeltaY + iDeltaZ*iDeltaZ));
}

static float
pl_movement_cost(int32_t iDeltaX, int32_t iDeltaY, int32_t iDeltaZ)
{
    // count how many axes we're moving on
    int32_t iAxisCount = (iDeltaX == 0 ? 0 : 1) + (iDeltaY == 0 ? 0 : 1) + (iDeltaZ == 0 ? 0 : 1);
    
    if(iAxisCount == 1)
        return 1.0f;           // straight move
    else if(iAxisCount == 2)
        return 1.414214f;      // diagonal move (√2)
    else
        return 1.732051f;      // 3d diagonal move (√3)
}

static void
pl_generate_neighbors(const plPathFindingVoxelGrid* ptGrid, uint32_t uCurrentX, uint32_t uCurrentY, uint32_t uCurrentZ, plPathNode* atNeighborsOut, uint32_t* puNeighborCountOut)
{
    *puNeighborCountOut = 0;
    
    // iterate through all 26 directions (-1, 0, +1 for each axis)
    for(int32_t iDeltaX = -1; iDeltaX <= 1; iDeltaX++)
    {
        for(int32_t iDeltaY = -1; iDeltaY <= 1; iDeltaY++)
        {
            for(int32_t iDeltaZ = -1; iDeltaZ <= 1; iDeltaZ++)
            {
                // don't include current as neighbor
                if(iDeltaX == 0 && iDeltaY == 0 && iDeltaZ == 0)
                    continue;
                int32_t iNeighborX = (int32_t)uCurrentX + iDeltaX;
                int32_t iNeighborY = (int32_t)uCurrentY + iDeltaY;
                int32_t iNeighborZ = (int32_t)uCurrentZ + iDeltaZ;
                
                // bounds check 
                if(iNeighborX < 0 || iNeighborX >= (int32_t)ptGrid->uDimX ||
                   iNeighborY < 0 || iNeighborY >= (int32_t)ptGrid->uDimY ||
                   iNeighborZ < 0 || iNeighborZ >= (int32_t)ptGrid->uDimZ)
                    continue;
                
                if(pl_is_voxel_occupied_impl(ptGrid, (uint32_t)iNeighborX, (uint32_t)iNeighborY, (uint32_t)iNeighborZ))
                    continue;
                
                // valid neighbor - add to output array
                atNeighborsOut[*puNeighborCountOut].uX = (uint32_t)iNeighborX;
                atNeighborsOut[*puNeighborCountOut].uY = (uint32_t)iNeighborY;
                atNeighborsOut[*puNeighborCountOut].uZ = (uint32_t)iNeighborZ;
                (*puNeighborCountOut)++;
            }
        }
    }
}

static plPriorityQueue*
pl_create_priority_queue(uint32_t uInitialCapacity)
{
    plPriorityQueue* ptQueue = malloc(sizeof(plPriorityQueue));
    if(!ptQueue)
        return NULL;
    
    ptQueue->atNodes = malloc(sizeof(plPathNode) * uInitialCapacity);
    if(!ptQueue->atNodes)
    {
        free(ptQueue);
        return NULL;
    }
    
    ptQueue->uCount = 0;
    ptQueue->uCapacity = uInitialCapacity;
    
    return ptQueue;
}

static void
pl_destroy_priority_queue(plPriorityQueue* ptQueue)
{
    if(!ptQueue)
        return;
    
    if(ptQueue->atNodes)
        free(ptQueue->atNodes);
    
    free(ptQueue);
}

static bool
pl_pq_is_empty(plPriorityQueue* ptQueue)
{
    return ptQueue->uCount == 0;
}

// TODO: needs to be optimized later 
static void
pl_pq_push(plPriorityQueue* ptQueue, plPathNode tNode)
{
    // check if we need to grow the array
    if(ptQueue->uCount >= ptQueue->uCapacity)
    {
        uint32_t uNewCapacity = ptQueue->uCapacity * 2;
        plPathNode* atNewNodes = realloc(ptQueue->atNodes, sizeof(plPathNode) * uNewCapacity);
        
        if(!atNewNodes)
            return; // allocation failed
        
        ptQueue->atNodes = atNewNodes;
        ptQueue->uCapacity = uNewCapacity;
    }
    
    // add node to end of array
    ptQueue->atNodes[ptQueue->uCount] = tNode;
    ptQueue->uCount++;
}

static plPathNode
pl_pq_pop(plPriorityQueue* ptQueue)
{
    // find node with lowest f-cost
    uint32_t uLowestIndex = 0;
    float fLowestCost = ptQueue->atNodes[0].fFCost;
    
    for(uint32_t i = 1; i < ptQueue->uCount; i++)
    {
        if(ptQueue->atNodes[i].fFCost < fLowestCost)
        {
            fLowestCost = ptQueue->atNodes[i].fFCost;
            uLowestIndex = i;
        }
    }
    plPathNode tResult = ptQueue->atNodes[uLowestIndex];
    
    // simple swap delete
    ptQueue->atNodes[uLowestIndex] = ptQueue->atNodes[ptQueue->uCount - 1];
    ptQueue->uCount--;
    
    return tResult;
}

static plPathNode* 
pl_pq_find(plPriorityQueue* ptQueue, uint32_t uX, uint32_t uY, uint32_t uZ)
{
    if(!ptQueue)
    return NULL;

    for(uint32_t i = 0; i < ptQueue->uCount; i++)
    {
        if(ptQueue->atNodes[i].uX == uX && ptQueue->atNodes[i].uY == uY && ptQueue->atNodes[i].uZ == uZ)
        return &ptQueue->atNodes[i];
    }
    return NULL;
}

static plClosedSet*
pl_create_closed_set(uint32_t uTotalVoxels)
{
    plClosedSet* ptSet = malloc(sizeof(plClosedSet));
    if(!ptSet)
        return NULL;
    
    uint32_t uBitArraySize = (uTotalVoxels + 31) / 32;  // round up to always have capacity
    
    ptSet->auBits = malloc(sizeof(uint32_t) * uBitArraySize);
    if(!ptSet->auBits)
    {
        free(ptSet);
        return NULL;
    }
    
    // initialize all to 0 (unoccupied)
    memset(ptSet->auBits, 0, sizeof(uint32_t) * uBitArraySize);
    ptSet->uBitArraySize = uBitArraySize;
    
    return ptSet;
}

static void
pl_destroy_closed_set(plClosedSet* ptSet)
{
    if(!ptSet)
        return;
    
    if(ptSet->auBits)
        free(ptSet->auBits);
    
    free(ptSet);
}

static void
pl_closed_set_add(plClosedSet* ptSet, uint32_t uVoxelIndex)
{
    uint32_t uArrayIndex = uVoxelIndex / 32;
    uint32_t uBitOffset = uVoxelIndex % 32;
    
    // set bit to 1
    ptSet->auBits[uArrayIndex] |= (1 << uBitOffset);
}

static bool
pl_closed_set_contains(plClosedSet* ptSet, uint32_t uVoxelIndex)
{
    uint32_t uArrayIndex = uVoxelIndex / 32;
    uint32_t uBitOffset = uVoxelIndex % 32;
    
    // check if bit is 1
    return (ptSet->auBits[uArrayIndex] & (1 << uBitOffset)) != 0;
}

static plPathFindingResult
pl_reconstruct_path(const plPathFindingVoxelGrid* ptGrid, plPathNode tGoalNode, plPathNode* atExploredNodes, uint32_t uExploredCount, 
        uint32_t uStartX, uint32_t uStartY, uint32_t uStartZ)
{
    plPathFindingResult tResult = {0};
    // allocate temporary path array
    plPathNode* atPath = malloc(sizeof(plPathNode) * uExploredCount);
    if(!atPath)
    {
        // allocation failed
        tResult.bSuccess = false;
        return tResult;
    }

    uint32_t uPathCount = 0;
    plPathNode tCurrent = tGoalNode;

    // check explored and retrieve path
    while(tCurrent.uX != uStartX || tCurrent.uY != uStartY || tCurrent.uZ != uStartZ)
    {
        // add current to path
        atPath[uPathCount] = tCurrent;
        uPathCount++;

        for(uint32_t i = 0; i < uExploredCount; i++)
        {
            if(atExploredNodes[i].uX == tCurrent.uParentX && atExploredNodes[i].uY == tCurrent.uParentY && atExploredNodes[i].uZ == tCurrent.uParentZ)
            {
                tCurrent = atExploredNodes[i];
                break;
            }
        }
    }
    atPath[uPathCount] = tCurrent;  // tCurrent is the start node, add to list
    uPathCount++;

    // reverse the path
    for(uint32_t i = 0; i < uPathCount / 2; i++)
    {
        plPathNode temp = atPath[i];
        atPath[i] = atPath[uPathCount - 1 - i];
        atPath[uPathCount - 1 - i] = temp;
    }

    tResult.atWaypoints = malloc(sizeof(plVec3) * uPathCount);
    if(!tResult.atWaypoints)
    {
        free(atPath);
        tResult.bSuccess = false;
        return tResult;
    }
    
    // convert voxel coords to world positions
    for(uint32_t i = 0; i < uPathCount; i++)
    {
        tResult.atWaypoints[i] = pl_voxel_to_world_impl(ptGrid, atPath[i].uX, atPath[i].uY, atPath[i].uZ);
    }
    tResult.uWaypointCount = uPathCount;
    tResult.bSuccess = true;

    free(atPath);
    return tResult;
}

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
        return;

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
    plPathFindingResult tResult = {0};

    uint32_t uStartX;
    uint32_t uStartY;
    uint32_t uStartZ;
    uint32_t uGoalX;
    uint32_t uGoalY;
    uint32_t uGoalZ;

    pl_world_to_voxel_impl(ptGrid, ptQuery->tStart, &uStartX, &uStartY, &uStartZ);
    pl_world_to_voxel_impl(ptGrid, ptQuery->tGoal, &uGoalX, &uGoalY, &uGoalZ);
    
    // validation checks
    if(!pl_is_valid_voxel_impl(ptGrid, uStartX, uStartY, uStartZ) || !pl_is_valid_voxel_impl(ptGrid, uGoalX, uGoalY, uGoalZ))
    {
        tResult.bSuccess = false;
        return tResult;
    } 
    if(pl_is_voxel_occupied_impl(ptGrid, uStartX, uStartY, uStartZ) || pl_is_voxel_occupied_impl(ptGrid, uGoalX, uGoalY, uGoalZ))
    {
        tResult.bSuccess = false;
        return tResult;
    }

    // create data structures
    uint32_t uTotalVoxels = ptGrid->uDimX * ptGrid->uDimY * ptGrid->uDimZ;
    uint32_t uInitCapacity =  (uint32_t)(uTotalVoxels * 0.3f); // TODO: test for ideal starting capacity
    plPriorityQueue* ptOpenSet = pl_create_priority_queue(uInitCapacity);
    plClosedSet* ptClosedSet = pl_create_closed_set(uTotalVoxels);

    // for path reconstruction
    plPathNode* atExploredNodes = malloc(sizeof(plPathNode) * uTotalVoxels);
    if(!atExploredNodes)
    {
        pl_destroy_priority_queue(ptOpenSet);
        pl_destroy_closed_set(ptClosedSet);
        tResult.bSuccess = false;
        return tResult;
    }
    uint32_t uExploredCount = 0;
    
    // create start node and add to open set
    plPathNode tStartNode = {0};
    tStartNode.uX = uStartX;
    tStartNode.uY = uStartY;
    tStartNode.uZ = uStartZ; 
    tStartNode.fGCost = 0;
    tStartNode.fHCost = pl_heuristic(uStartX, uStartY, uStartZ, uGoalX, uGoalY, uGoalZ);
    tStartNode.fFCost = tStartNode.fGCost + tStartNode.fHCost;
    // setting to self (first node no parent)
    tStartNode.uParentX = uStartX;
    tStartNode.uParentY = uStartY;
    tStartNode.uParentZ = uStartZ;

    pl_pq_push(ptOpenSet, tStartNode);
    
    while(!pl_pq_is_empty(ptOpenSet))
    {
        plPathNode tCurrentNode = pl_pq_pop(ptOpenSet); // this is the node we're exploring (lowest f cost)
        atExploredNodes[uExploredCount] = tCurrentNode; // store for path recreation 
        uExploredCount++;  
        
        // check if we reached the goal
        if(tCurrentNode.uX == uGoalX && tCurrentNode.uY == uGoalY && tCurrentNode.uZ == uGoalZ)
        {
            tResult = pl_reconstruct_path(ptGrid, tCurrentNode, atExploredNodes, uExploredCount, uStartX, uStartY, uStartZ);
            free(atExploredNodes);
            pl_destroy_priority_queue(ptOpenSet);
            pl_destroy_closed_set(ptClosedSet);
            return tResult;
        }
        
        uint32_t uCurrentVoxelIndex = pl_voxel_index(ptGrid, tCurrentNode.uX, tCurrentNode.uY, tCurrentNode.uZ);
        pl_closed_set_add(ptClosedSet, uCurrentVoxelIndex);
        
        // get all neighbors of current
        plPathNode tNeighbors[26] = {0}; // 26 is max possible for 3D
        uint32_t uNeighborCount = 0;
        pl_generate_neighbors(ptGrid, tCurrentNode.uX, tCurrentNode.uY, tCurrentNode.uZ, tNeighbors, &uNeighborCount);
 
        // process each neighbor
        for(uint32_t uNeighborToCheck = 0; uNeighborToCheck < uNeighborCount; uNeighborToCheck++)
        {   
            // skip if already explored
            uint32_t uIndexForNeighbor = pl_voxel_index(ptGrid, tNeighbors[uNeighborToCheck].uX, tNeighbors[uNeighborToCheck].uY, tNeighbors[uNeighborToCheck].uZ);
            if(pl_closed_set_contains(ptClosedSet, uIndexForNeighbor))
                continue;
            
            // calculate cost to reach this neighbor
            int32_t iDeltaX = (int32_t)tNeighbors[uNeighborToCheck].uX - (int32_t)tCurrentNode.uX;
            int32_t iDeltaY = (int32_t)tNeighbors[uNeighborToCheck].uY - (int32_t)tCurrentNode.uY;
            int32_t iDeltaZ = (int32_t)tNeighbors[uNeighborToCheck].uZ - (int32_t)tCurrentNode.uZ;

            float fMovementCost = pl_movement_cost(iDeltaX, iDeltaY, iDeltaZ);
            float fTentativeGCost = tCurrentNode.fGCost + fMovementCost; 

            // check if this neighbor is already in open set - returns NULL if not in openset 
            plPathNode* ptExistingNode = pl_pq_find(ptOpenSet, tNeighbors[uNeighborToCheck].uX, tNeighbors[uNeighborToCheck].uY, tNeighbors[uNeighborToCheck].uZ);
             
            if(ptExistingNode == NULL)
            {
                // new node - add to open set
                tNeighbors[uNeighborToCheck].fGCost = fTentativeGCost;
                tNeighbors[uNeighborToCheck].fHCost = pl_heuristic(tNeighbors[uNeighborToCheck].uX, 
                                                                   tNeighbors[uNeighborToCheck].uY, 
                                                                   tNeighbors[uNeighborToCheck].uZ, 
                                                                   uGoalX, uGoalY, uGoalZ); 
                tNeighbors[uNeighborToCheck].fFCost   = tNeighbors[uNeighborToCheck].fGCost + tNeighbors[uNeighborToCheck].fHCost;
                tNeighbors[uNeighborToCheck].uParentX = tCurrentNode.uX;
                tNeighbors[uNeighborToCheck].uParentY = tCurrentNode.uY;
                tNeighbors[uNeighborToCheck].uParentZ = tCurrentNode.uZ;

                pl_pq_push(ptOpenSet, tNeighbors[uNeighborToCheck]);
            }
            else if(fTentativeGCost < ptExistingNode->fGCost)
            {
                ptExistingNode->fGCost = fTentativeGCost;
                ptExistingNode->fFCost = ptExistingNode->fGCost + ptExistingNode->fHCost;
                ptExistingNode->uParentX = tCurrentNode.uX;
                ptExistingNode->uParentY = tCurrentNode.uY;
                ptExistingNode->uParentZ = tCurrentNode.uZ;
            }
        }
    }

    // if we get here, open set is empty and we never reached goal
    free(atExploredNodes);
    pl_destroy_closed_set(ptClosedSet);
    pl_destroy_priority_queue(ptOpenSet);
    tResult.bSuccess = false;
    return tResult;
}

static void
pl_free_result_impl(plPathFindingResult* ptResult)
{
    if(!ptResult)
        return;
    
    if(ptResult->atWaypoints)
        free(ptResult->atWaypoints);
    
    ptResult->atWaypoints = NULL;
    ptResult->uWaypointCount = 0;
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