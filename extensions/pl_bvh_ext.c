/*
   pl_bvh_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] public api implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <math.h>
#include <string.h> // memset
#include "pl.h"
#include "pl_bvh_ext.h"

#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plMemoryI*  gptMemory = NULL;
    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
#endif

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_bvh_update_node_bounds(plBVH* ptBvh, uint32_t uNodeIndex, const plAABB* ptAABBs)
{
    plBVHNode* ptNode = &ptBvh->ptNodes[uNodeIndex];
    ptNode->tAABB = (plAABB){0};
    for(uint32_t i = 0; i < ptNode->uCount; i++)
    {
        uint32_t uOffset = ptNode->uOffset + i;
        uint32_t uIndex = ptBvh->puLeafIndices[uOffset];
        ptNode->tAABB = pl_aabb_merge(&ptNode->tAABB, &ptAABBs[uIndex]);
    }
}

void
pl_bvh_subdivide(plBVH* ptBvh, uint32_t uNodeIndex, const plAABB* ptAABBs)
{
    plBVHNode* ptNode = &ptBvh->ptNodes[uNodeIndex];
    if(ptNode->uCount <= 2)
        return;

    plVec3 tExtent = pl_aabb_half_width(&ptNode->tAABB);
    plVec3 tMin = ptNode->tAABB.tMin;
    int iAxis = 0;
    if(tExtent.y > tExtent.x) iAxis = 1;
    if(tExtent.z > tExtent.d[iAxis]) iAxis = 2;

    float fSplitPos = tMin.d[iAxis] + tExtent.d[iAxis] * 0.5f;

    // in-place partition
    int i = ptNode->uOffset;
    int j = i + ptNode->uCount - 1;
    while (i <= j)
    {
        plVec3 tCenter = pl_aabb_center(&ptAABBs[ptBvh->puLeafIndices[i]]);
        float fValue = tCenter.d[iAxis];

        if (fValue < fSplitPos)
        {
            i++;
        }
        else
        {
            // swap
            uint32_t uA = ptBvh->puLeafIndices[i];
            uint32_t uB = ptBvh->puLeafIndices[j--];
            ptBvh->puLeafIndices[i] = uB;
            ptBvh->puLeafIndices[j + 1] = uA;
        }
    }

    // abort split if one of the sides is empty
    uint32_t uLeftCount = i - ptNode->uOffset;
    if (uLeftCount == 0 || uLeftCount == ptNode->uCount)
        return;

    // create child nodes
    uint32_t uLeftChildIndex = ptBvh->uNodeCount++;
    uint32_t uRightChildIndex = ptBvh->uNodeCount++;
    ptNode->uLeft = uLeftChildIndex;
    ptBvh->ptNodes[uLeftChildIndex] = (plBVHNode){0};
    ptBvh->ptNodes[uLeftChildIndex].uOffset = ptNode->uOffset;
    ptBvh->ptNodes[uLeftChildIndex].uCount = uLeftCount;
    ptBvh->ptNodes[uRightChildIndex] = (plBVHNode){0};
    ptBvh->ptNodes[uRightChildIndex].uOffset = i;
    ptBvh->ptNodes[uRightChildIndex].uCount = ptNode->uCount - uLeftCount;
    ptNode->uCount = 0;
    pl_bvh_update_node_bounds(ptBvh, uLeftChildIndex, ptAABBs);
    pl_bvh_update_node_bounds(ptBvh, uRightChildIndex, ptAABBs);

    // recurse
    pl_bvh_subdivide(ptBvh, uLeftChildIndex, ptAABBs);
    pl_bvh_subdivide(ptBvh, uRightChildIndex, ptAABBs);
}

void
pl_bvh_build(plBVH* ptBvh, const plAABB* ptAABBs, uint32_t uCount)
{
    ptBvh->uNodeCount = 0;
    if(uCount == 0)
        return;

    const uint32_t uNodeCapacity = uCount * 2 - 1;

    ptBvh->puAllocation = PL_ALLOC(sizeof(plBVHNode) * uNodeCapacity + sizeof(uint32_t) * uCount);
    memset(ptBvh->puAllocation, 0, sizeof(plBVHNode) * uNodeCapacity + sizeof(uint32_t) * uCount);

    ptBvh->ptNodes = (plBVHNode*)ptBvh->puAllocation;
    ptBvh->puLeafIndices = (uint32_t*)&ptBvh->ptNodes[uNodeCapacity];
    ptBvh->uLeafCount = uCount;

    plBVHNode* ptNode = &ptBvh->ptNodes[ptBvh->uNodeCount++];
    *ptNode = (plBVHNode){0};
    ptNode->uCount = uCount;
    for(uint32_t i = 0; i < uCount; ++i)
    {
        ptNode->tAABB = pl_aabb_merge(&ptNode->tAABB, &ptAABBs[i]);
        ptBvh->puLeafIndices[i] = i;
    }
    pl_bvh_subdivide(ptBvh, 0, ptAABBs);
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_bvh_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plBVHI tApi = {
        .build = pl_bvh_build
    };
    pl_set_api(ptApiRegistry, plBVHI, &tApi);

    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
}

PL_EXPORT void
pl_unload_bvh_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    if(bReload)
        return;

    const plBVHI* ptApi = pl_get_api_latest(ptApiRegistry, plBVHI);
    ptApiRegistry->remove_api(ptApi);
}
