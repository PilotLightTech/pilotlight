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
static void  pl_generate_neighbors(const plPathFindingVoxelGrid* ptGrid, uint32_t uCurrentX, uint32_t uCurrentY, uint32_t uCurrentZ, plPathNode* atNeighborsOut, uint32_t* puNeighborCountOut, bool bGenDiag);

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
pl_generate_neighbors(const plPathFindingVoxelGrid* ptGrid, uint32_t uCurrentX, uint32_t uCurrentY, uint32_t uCurrentZ, plPathNode* atNeighborsOut, uint32_t* puNeighborCountOut, bool bGenDiag)
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

                if(!bGenDiag) // skip diagonals if disabled
                {
                    int32_t iAxisCount = (iDeltaX != 0 ? 1 : 0) + (iDeltaY != 0 ? 1 : 0) + (iDeltaZ != 0 ? 1 : 0);
                    if(iAxisCount > 1) // diagonal move
                        continue;
                }

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

    // bubble up 
    uint32_t uCurrentIndex = ptQueue->uCount;
    while(uCurrentIndex > 0)
    {
        uint32_t uParentIndex = (uCurrentIndex - 1) / 2;
        
        if(ptQueue->atNodes[uCurrentIndex].fFCost < ptQueue->atNodes[uParentIndex].fFCost)
        {
            // swap with parent
            plPathNode temp = ptQueue->atNodes[uCurrentIndex];
            ptQueue->atNodes[uCurrentIndex] = ptQueue->atNodes[uParentIndex];
            ptQueue->atNodes[uParentIndex] = temp;
            
            uCurrentIndex = uParentIndex;  // move up
        }
        else
        {
            break;  // heap property satisfied
        }
    }

    ptQueue->uCount++;
}

static plPathNode
pl_pq_pop(plPriorityQueue* ptQueue)
{
    // root is minimum
    plPathNode tResult = ptQueue->atNodes[0];
    
    // move last element to root
    ptQueue->uCount--;
    ptQueue->atNodes[0] = ptQueue->atNodes[ptQueue->uCount];
    
    // bubble down to maintain heap property
    uint32_t uCurrentIndex = 0;
    while(true)
    {
        uint32_t uLeftChild = 2 * uCurrentIndex + 1;
        uint32_t uRightChild = 2 * uCurrentIndex + 2;
        uint32_t uSmallest = uCurrentIndex;
        
        // find smallest of current, left, right
        if(uLeftChild < ptQueue->uCount && ptQueue->atNodes[uLeftChild].fFCost < ptQueue->atNodes[uSmallest].fFCost)
        {
            uSmallest = uLeftChild;
        }
        if(uRightChild < ptQueue->uCount && ptQueue->atNodes[uRightChild].fFCost < ptQueue->atNodes[uSmallest].fFCost)
        {
            uSmallest = uRightChild;
        }
        
        // if current is smallest, done
        if(uSmallest == uCurrentIndex)
            break;
        
        // swap with smallest child
        plPathNode temp = ptQueue->atNodes[uCurrentIndex];
        ptQueue->atNodes[uCurrentIndex] = ptQueue->atNodes[uSmallest];
        ptQueue->atNodes[uSmallest] = temp;
        
        uCurrentIndex = uSmallest;  // move down
    }
    
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

plVec3 
get_vertex(const float* pfVertices, uint32_t uIndex)
{
    return (plVec3){
        pfVertices[uIndex * 3 + 0],
        pfVertices[uIndex * 3 + 1],
        pfVertices[uIndex * 3 + 2]
    };
}

bool 
aabb_overlap(plAABB* triAABB, plAABB* voxelAABB)
{
    return (triAABB->tMax.x >= voxelAABB->tMin.x && triAABB->tMin.x <= voxelAABB->tMax.x) &&
           (triAABB->tMax.y >= voxelAABB->tMin.y && triAABB->tMin.y <= voxelAABB->tMax.y) &&
           (triAABB->tMax.z >= voxelAABB->tMin.z && triAABB->tMin.z <= voxelAABB->tMax.z);
}

bool 
point_in_box(plVec3 point, plVec3 boxMin, plVec3 boxMax)
{
    return (point.x >= boxMin.x && point.x <= boxMax.x) &&
           (point.y >= boxMin.y && point.y <= boxMax.y) &&
           (point.z >= boxMin.z && point.z <= boxMax.z);
}

bool 
edge_intersects_box(plVec3 v0, plVec3 v1, plVec3 tVoxelMin, plVec3 tVoxelMax)
{
    plVec3 dir = {v1.x - v0.x, v1.y - v0.y, v1.z - v0.z};
    
    // test all 6 faces
    // +X face
    if(fabsf(dir.x) > 0.0001f) 
    {
        float t = (tVoxelMax.x - v0.x) / dir.x;
        if(t >= 0.0f && t <= 1.0f) 
        {
            float y = v0.y + t * dir.y;
            float z = v0.z + t * dir.z;
            if(y >= tVoxelMin.y && y <= tVoxelMax.y && z >= tVoxelMin.z && z <= tVoxelMax.z)
                return true;
        }
    }
    // -X face
    if(fabsf(dir.x) > 0.0001f) 
    {
        float t = (tVoxelMin.x - v0.x) / dir.x;
        if(t >= 0.0f && t <= 1.0f) 
        {
            float y = v0.y + t * dir.y;
            float z = v0.z + t * dir.z;
            if(y >= tVoxelMin.y && y <= tVoxelMax.y && z >= tVoxelMin.z && z <= tVoxelMax.z)
                return true;
        }
    }
    // +Y face
    if(fabsf(dir.y) > 0.0001f) 
    {
        float t = (tVoxelMax.y - v0.y) / dir.y;
        if(t >= 0.0f && t <= 1.0f) 
        {
            float x = v0.x + t * dir.x;
            float z = v0.z + t * dir.z;
            if(x >= tVoxelMin.x && x <= tVoxelMax.x && z >= tVoxelMin.z && z <= tVoxelMax.z)
                return true;
        }
    }
    // -Y face
    if(fabsf(dir.y) > 0.0001f) 
    {
        float t = (tVoxelMin.y - v0.y) / dir.y;
        if(t >= 0.0f && t <= 1.0f) 
        {
            float x = v0.x + t * dir.x;
            float z = v0.z + t * dir.z;
            if(x >= tVoxelMin.x && x <= tVoxelMax.x && z >= tVoxelMin.z && z <= tVoxelMax.z)
                return true;
        }
    }
    // +Z face
    if(fabsf(dir.z) > 0.0001f) 
    {
        float t = (tVoxelMax.z - v0.z) / dir.z;
        if(t >= 0.0f && t <= 1.0f) 
        {
            float x = v0.x + t * dir.x;
            float y = v0.y + t * dir.y;
            if(x >= tVoxelMin.x && x <= tVoxelMax.x && y >= tVoxelMin.y && y <= tVoxelMax.y)
                return true;
        }
    }
    // -Z face
    if(fabsf(dir.z) > 0.0001f) 
    {
        float t = (tVoxelMin.z - v0.z) / dir.z;
        if(t >= 0.0f && t <= 1.0f) 
        {
            float x = v0.x + t * dir.x;
            float y = v0.y + t * dir.y;
            if(x >= tVoxelMin.x && x <= tVoxelMax.x && y >= tVoxelMin.y && y <= tVoxelMax.y)
                return true;
        }
    }
    
    return false;
}

static bool
triangle_intersects_box(plVec3 v0, plVec3 v1, plVec3 v2, plVec3 boxMin, plVec3 boxMax)
{
    // separating axis theorem (sat) test for triangle-aabb intersection
    // credit to: tomas akenine-möller's algorithm
    // https://fileadmin.cs.lth.se/cs/Personal/Tomas_Akenine-Moller/code/tribox2.txt
    
    // compute box center and half-extents
    plVec3 boxCenter = {
        (boxMin.x + boxMax.x) * 0.5f,
        (boxMin.y + boxMax.y) * 0.5f,
        (boxMin.z + boxMax.z) * 0.5f
    };
    plVec3 boxHalfSize = {
        (boxMax.x - boxMin.x) * 0.5f,
        (boxMax.y - boxMin.y) * 0.5f,
        (boxMax.z - boxMin.z) * 0.5f
    };
    
    // translate triangle to box-centered coordinates & compute triangle edges
    plVec3 t0 = {v0.x - boxCenter.x, v0.y - boxCenter.y, v0.z - boxCenter.z};
    plVec3 t1 = {v1.x - boxCenter.x, v1.y - boxCenter.y, v1.z - boxCenter.z};
    plVec3 t2 = {v2.x - boxCenter.x, v2.y - boxCenter.y, v2.z - boxCenter.z};
    plVec3 e0 = {t1.x - t0.x, t1.y - t0.y, t1.z - t0.z};
    plVec3 e1 = {t2.x - t1.x, t2.y - t1.y, t2.z - t1.z};
    plVec3 e2 = {t0.x - t2.x, t0.y - t2.y, t0.z - t2.z};
    
    // test axes from cross products of triangle edges with box axes
    // axis test macros
    #define AXISTEST_X01(a, b, fa, fb) \
        p0 = a * t0.y - b * t0.z; \
        p2 = a * t2.y - b * t2.z; \
        if(p0 < p2) { min = p0; max = p2; } else { min = p2; max = p0; } \
        rad = fa * boxHalfSize.y + fb * boxHalfSize.z; \
        if(min > rad || max < -rad) return false;
    
    #define AXISTEST_X2(a, b, fa, fb) \
        p0 = a * t0.y - b * t0.z; \
        p1 = a * t1.y - b * t1.z; \
        if(p0 < p1) { min = p0; max = p1; } else { min = p1; max = p0; } \
        rad = fa * boxHalfSize.y + fb * boxHalfSize.z; \
        if(min > rad || max < -rad) return false;
    
    #define AXISTEST_Y02(a, b, fa, fb) \
        p0 = -a * t0.x + b * t0.z; \
        p2 = -a * t2.x + b * t2.z; \
        if(p0 < p2) { min = p0; max = p2; } else { min = p2; max = p0; } \
        rad = fa * boxHalfSize.x + fb * boxHalfSize.z; \
        if(min > rad || max < -rad) return false;
    
    #define AXISTEST_Y1(a, b, fa, fb) \
        p0 = -a * t0.x + b * t0.z; \
        p1 = -a * t1.x + b * t1.z; \
        if(p0 < p1) { min = p0; max = p1; } else { min = p1; max = p0; } \
        rad = fa * boxHalfSize.x + fb * boxHalfSize.z; \
        if(min > rad || max < -rad) return false;
    
    #define AXISTEST_Z12(a, b, fa, fb) \
        p1 = a * t1.x - b * t1.y; \
        p2 = a * t2.x - b * t2.y; \
        if(p1 < p2) { min = p1; max = p2; } else { min = p2; max = p1; } \
        rad = fa * boxHalfSize.x + fb * boxHalfSize.y; \
        if(min > rad || max < -rad) return false;
    
    #define AXISTEST_Z0(a, b, fa, fb) \
        p0 = a * t0.x - b * t0.y; \
        p1 = a * t1.x - b * t1.y; \
        if(p0 < p1) { min = p0; max = p1; } else { min = p1; max = p0; } \
        rad = fa * boxHalfSize.x + fb * boxHalfSize.y; \
        if(min > rad || max < -rad) return false;
    
    float min, max, p0, p1, p2, rad, fex, fey, fez;
    
    // test edge 0
    fex = fabsf(e0.x);
    fey = fabsf(e0.y);
    fez = fabsf(e0.z);
    AXISTEST_X01(e0.z, e0.y, fez, fey);
    AXISTEST_Y02(e0.z, e0.x, fez, fex);
    AXISTEST_Z12(e0.y, e0.x, fey, fex);
    
    // test edge 1
    fex = fabsf(e1.x);
    fey = fabsf(e1.y);
    fez = fabsf(e1.z);
    AXISTEST_X01(e1.z, e1.y, fez, fey);
    AXISTEST_Y02(e1.z, e1.x, fez, fex);
    AXISTEST_Z0(e1.y, e1.x, fey, fex);
    
    // test edge 2
    fex = fabsf(e2.x);
    fey = fabsf(e2.y);
    fez = fabsf(e2.z);
    AXISTEST_X2(e2.z, e2.y, fez, fey);
    AXISTEST_Y1(e2.z, e2.x, fez, fex);
    AXISTEST_Z12(e2.y, e2.x, fey, fex);
    
    #undef AXISTEST_X01
    #undef AXISTEST_X2
    #undef AXISTEST_Y02
    #undef AXISTEST_Y1
    #undef AXISTEST_Z12
    #undef AXISTEST_Z0
    
    // test overlap in x, y, z axes (box faces)
    // x axis
    min = t0.x < t1.x ? (t0.x < t2.x ? t0.x : t2.x) : (t1.x < t2.x ? t1.x : t2.x);
    max = t0.x > t1.x ? (t0.x > t2.x ? t0.x : t2.x) : (t1.x > t2.x ? t1.x : t2.x);
    if(min > boxHalfSize.x || max < -boxHalfSize.x) return false;
    
    // y axis
    min = t0.y < t1.y ? (t0.y < t2.y ? t0.y : t2.y) : (t1.y < t2.y ? t1.y : t2.y);
    max = t0.y > t1.y ? (t0.y > t2.y ? t0.y : t2.y) : (t1.y > t2.y ? t1.y : t2.y);
    if(min > boxHalfSize.y || max < -boxHalfSize.y) return false;
    
    // z axis
    min = t0.z < t1.z ? (t0.z < t2.z ? t0.z : t2.z) : (t1.z < t2.z ? t1.z : t2.z);
    max = t0.z > t1.z ? (t0.z > t2.z ? t0.z : t2.z) : (t1.z > t2.z ? t1.z : t2.z);
    if(min > boxHalfSize.z || max < -boxHalfSize.z) return false;
    
    // test triangle plane
    plVec3 normal = {
        e0.y * e1.z - e0.z * e1.y,
        e0.z * e1.x - e0.x * e1.z,
        e0.x * e1.y - e0.y * e1.x
    };
    float d = -(normal.x * t0.x + normal.y * t0.y + normal.z * t0.z);
    
    // project box onto triangle normal
    float r = boxHalfSize.x * fabsf(normal.x) + 
              boxHalfSize.y * fabsf(normal.y) + 
              boxHalfSize.z * fabsf(normal.z);
    
    if(d > r || d < -r) return false;
    
    return true;
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
    for(uint32_t i = 0; i < uIndexCount; i += 3)
    {
        uint32_t uIndex0 = puIndices[i];
        uint32_t uIndex1 = puIndices[i + 1];
        uint32_t uIndex2 = puIndices[i + 2];

        plVec3 tVertex0 = get_vertex(pfVertices, uIndex0);
        plVec3 tVertex1 = get_vertex(pfVertices, uIndex1);
        plVec3 tVertex2 = get_vertex(pfVertices, uIndex2);

        // compute triangle AABB
        plAABB tTriangleAABB = {
            .tMin = {
                .x = fminf(fminf(tVertex0.x, tVertex1.x), tVertex2.x),
                .y = fminf(fminf(tVertex0.y, tVertex1.y), tVertex2.y),
                .z = fminf(fminf(tVertex0.z, tVertex1.z), tVertex2.z)
            },
            .tMax = {
                .x = fmaxf(fmaxf(tVertex0.x, tVertex1.x), tVertex2.x),
                .y = fmaxf(fmaxf(tVertex0.y, tVertex1.y), tVertex2.y),
                .z = fmaxf(fmaxf(tVertex0.z, tVertex1.z), tVertex2.z)
            }
        };
        
        // expand flat triangles to handle parallel lines 
        const float epsilon = 0.01f;
        if(tTriangleAABB.tMax.x - tTriangleAABB.tMin.x < epsilon)
        {
            tTriangleAABB.tMin.x -= epsilon;
            tTriangleAABB.tMax.x += epsilon;
        }
        if(tTriangleAABB.tMax.y - tTriangleAABB.tMin.y < epsilon)
        {
            tTriangleAABB.tMin.y -= epsilon;
            tTriangleAABB.tMax.y += epsilon;
        }
        if(tTriangleAABB.tMax.z - tTriangleAABB.tMin.z < epsilon)
        {
            tTriangleAABB.tMin.z -= epsilon;
            tTriangleAABB.tMax.z += epsilon;
        }
        
        int32_t iStartX = (int32_t)floorf((tTriangleAABB.tMin.x - ptGrid->tOrigin.x) / ptGrid->fVoxelSize);
        int32_t iStartY = (int32_t)floorf((tTriangleAABB.tMin.y - ptGrid->tOrigin.y) / ptGrid->fVoxelSize);
        int32_t iStartZ = (int32_t)floorf((tTriangleAABB.tMin.z - ptGrid->tOrigin.z) / ptGrid->fVoxelSize);

        int32_t iEndX = (int32_t)floorf((tTriangleAABB.tMax.x - ptGrid->tOrigin.x) / ptGrid->fVoxelSize);
        int32_t iEndY = (int32_t)floorf((tTriangleAABB.tMax.y - ptGrid->tOrigin.y) / ptGrid->fVoxelSize);
        int32_t iEndZ = (int32_t)floorf((tTriangleAABB.tMax.z - ptGrid->tOrigin.z) / ptGrid->fVoxelSize);

        // clamp to grid
        iStartX = (int32_t)fmax(0, fmin(iStartX, (int32_t)ptGrid->uDimX - 1));
        iStartY = (int32_t)fmax(0, fmin(iStartY, (int32_t)ptGrid->uDimY - 1));
        iStartZ = (int32_t)fmax(0, fmin(iStartZ, (int32_t)ptGrid->uDimZ - 1));
        iEndX = (int32_t)fmax(0, fmin(iEndX, (int32_t)ptGrid->uDimX - 1));
        iEndY = (int32_t)fmax(0, fmin(iEndY, (int32_t)ptGrid->uDimY - 1));
        iEndZ = (int32_t)fmax(0, fmin(iEndZ, (int32_t)ptGrid->uDimZ - 1));

        for(int32_t iZ = iStartZ; iZ <= iEndZ; iZ++)
        {
            for(int32_t iY = iStartY; iY <= iEndY; iY++)
            {
                for(int32_t iX = iStartX; iX <= iEndX; iX++)
                {
                    plVec3 tVoxelMin = {
                        ptGrid->tOrigin.x + (float)iX * ptGrid->fVoxelSize,
                        ptGrid->tOrigin.y + (float)iY * ptGrid->fVoxelSize,
                        ptGrid->tOrigin.z + (float)iZ * ptGrid->fVoxelSize
                    };
                    
                    plAABB tVoxelAABB = {
                        .tMin = tVoxelMin,
                        .tMax = {
                            tVoxelMin.x + ptGrid->fVoxelSize,
                            tVoxelMin.y + ptGrid->fVoxelSize,
                            tVoxelMin.z + ptGrid->fVoxelSize
                        }
                    };
                    
                    // checks for voxel occupation
                    bool bVertexInside = point_in_box(tVertex0, tVoxelAABB.tMin, tVoxelAABB.tMax) ||
                                         point_in_box(tVertex1, tVoxelAABB.tMin, tVoxelAABB.tMax) ||
                                         point_in_box(tVertex2, tVoxelAABB.tMin, tVoxelAABB.tMax);
                    bool bEdgeIntersects = edge_intersects_box(tVertex0, tVertex1, tVoxelAABB.tMin, tVoxelAABB.tMax) ||
                                           edge_intersects_box(tVertex1, tVertex2, tVoxelAABB.tMin, tVoxelAABB.tMax) ||
                                           edge_intersects_box(tVertex2, tVertex0, tVoxelAABB.tMin, tVoxelAABB.tMax);
                    bool bTriangleIntersects = triangle_intersects_box(tVertex0, tVertex1, tVertex2, 
                                                                    tVoxelAABB.tMin, tVoxelAABB.tMax);
                    // set if occupied
                    if(bVertexInside || bEdgeIntersects || bTriangleIntersects)
                    {
                        pl_set_voxel_impl(ptGrid, iX, iY, iZ, true);
                    }
                }
            }
        }
    }
}

//-----------------------------------------------------------------------------
// pathfinding queries
//-----------------------------------------------------------------------------

static plPathFindingResult
pl_find_path_impl(const plPathFindingVoxelGrid* ptGrid, const plPathFindingQuery* ptQuery, bool bSearchDiagonal)
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
    uint32_t uExploredCount = 0;
    if(!atExploredNodes)
    {
        pl_destroy_priority_queue(ptOpenSet);
        pl_destroy_closed_set(ptClosedSet);
        tResult.bSuccess = false;
        return tResult;
    }
    
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
        pl_generate_neighbors(ptGrid, tCurrentNode.uX, tCurrentNode.uY, tCurrentNode.uZ, tNeighbors, &uNeighborCount, bSearchDiagonal);
 
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