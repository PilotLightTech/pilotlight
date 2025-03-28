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

// extensions
#include "pl_collision_ext.h"

#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plMemoryI*  gptMemory = NULL;
    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

    #ifndef PL_DS_ALLOC
        #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
        #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif

    static const plCollisionI* gptCollision = NULL;
#endif

#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_bvh_update_node_bounds(plBVH* ptBvh, uint32_t uNodeIndex, const plAABB* ptAABBs)
{
    plBVHNode* ptNode = &ptBvh->_ptNodes[uNodeIndex];
    ptNode->tAABB = (plAABB){0};
    for(uint32_t i = 0; i < ptNode->_uCount; i++)
    {
        uint32_t uOffset = ptNode->_uOffset + i;
        uint32_t uIndex = ptBvh->_puLeafIndices[uOffset];
        ptNode->tAABB = pl_aabb_merge(&ptNode->tAABB, &ptAABBs[uIndex]);
    }
}

void
pl_bvh_subdivide(plBVH* ptBvh, uint32_t uNodeIndex, const plAABB* ptAABBs)
{
    plBVHNode* ptNode = &ptBvh->_ptNodes[uNodeIndex];
    if(ptNode->_uCount <= 2)
        return;

    plVec3 tExtent = pl_aabb_half_width(&ptNode->tAABB);
    plVec3 tMin = ptNode->tAABB.tMin;
    int iAxis = 0;
    if(tExtent.y > tExtent.x) iAxis = 1;
    if(tExtent.z > tExtent.d[iAxis]) iAxis = 2;

    float fSplitPos = tMin.d[iAxis] + tExtent.d[iAxis] * 0.5f;

    // in-place partition
    int i = ptNode->_uOffset;
    int j = i + ptNode->_uCount - 1;
    while (i <= j)
    {
        plVec3 tCenter = pl_aabb_center(&ptAABBs[ptBvh->_puLeafIndices[i]]);
        float fValue = tCenter.d[iAxis];

        if (fValue < fSplitPos)
        {
            i++;
        }
        else
        {
            // swap
            uint32_t uA = ptBvh->_puLeafIndices[i];
            uint32_t uB = ptBvh->_puLeafIndices[j--];
            ptBvh->_puLeafIndices[i] = uB;
            ptBvh->_puLeafIndices[j + 1] = uA;
        }
    }

    // abort split if one of the sides is empty
    uint32_t _uLeftCount = i - ptNode->_uOffset;
    if (_uLeftCount == 0 || _uLeftCount == ptNode->_uCount)
        return;

    // create child nodes
    uint32_t _uLeftChildIndex = ptBvh->_uNodeCount++;
    uint32_t uRightChildIndex = ptBvh->_uNodeCount++;
    ptNode->_uLeft = _uLeftChildIndex;
    ptBvh->_ptNodes[_uLeftChildIndex] = (plBVHNode){0};
    ptBvh->_ptNodes[_uLeftChildIndex]._uOffset = ptNode->_uOffset;
    ptBvh->_ptNodes[_uLeftChildIndex]._uCount = _uLeftCount;
    ptBvh->_ptNodes[uRightChildIndex] = (plBVHNode){0};
    ptBvh->_ptNodes[uRightChildIndex]._uOffset = i;
    ptBvh->_ptNodes[uRightChildIndex]._uCount = ptNode->_uCount - _uLeftCount;
    ptNode->_uCount = 0;
    pl_bvh_update_node_bounds(ptBvh, _uLeftChildIndex, ptAABBs);
    pl_bvh_update_node_bounds(ptBvh, uRightChildIndex, ptAABBs);

    // recurse
    pl_bvh_subdivide(ptBvh, _uLeftChildIndex, ptAABBs);
    pl_bvh_subdivide(ptBvh, uRightChildIndex, ptAABBs);
}

void
pl_bvh_build(plBVH* ptBvh, const plAABB* ptAABBs, uint32_t uCount)
{
    if(ptBvh->_puAllocation)
    {
        PL_FREE(ptBvh->_puAllocation);
        ptBvh->_puAllocation = NULL;
    }
    ptBvh->_uNodeCount = 0;
    if(uCount == 0)
        return;

    const uint32_t uNodeCapacity = uCount * 2 - 1;

    ptBvh->_puAllocation = PL_ALLOC(sizeof(plBVHNode) * uNodeCapacity + sizeof(uint32_t) * uCount);
    memset(ptBvh->_puAllocation, 0, sizeof(plBVHNode) * uNodeCapacity + sizeof(uint32_t) * uCount);

    ptBvh->_ptNodes = (plBVHNode*)ptBvh->_puAllocation;
    ptBvh->_puLeafIndices = (uint32_t*)(ptBvh->_ptNodes + uNodeCapacity);
    ptBvh->_uLeafCount = uCount;

    plBVHNode* ptNode = &ptBvh->_ptNodes[ptBvh->_uNodeCount++];
    *ptNode = (plBVHNode){0};
    ptNode->_uCount = uCount;
    for(uint32_t i = 0; i < uCount; ++i)
    {
        ptNode->tAABB = pl_aabb_merge(&ptNode->tAABB, &ptAABBs[i]);
        ptBvh->_puLeafIndices[i] = i;
    }
    pl_bvh_subdivide(ptBvh, 0, ptAABBs);
}

void
pl_bvh_cleanup(plBVH* ptBvh)
{
    if(ptBvh->_puAllocation)
    {
        PL_FREE(ptBvh->_puAllocation);
        ptBvh->_puAllocation = NULL;
    }
    pl_sb_free(ptBvh->_sbtNodeStack);
    *ptBvh = (plBVH){0};
}

void
pl_bvh_intersects_aabb(plBVH* ptBvh, plAABB tAABB, plBVHCallback tCallback, void* pUserData)
{
    if(ptBvh->_uNodeCount > 0)
    {
        pl_sb_push(ptBvh->_sbtNodeStack, &ptBvh->_ptNodes[0]);

        while(pl_sb_size(ptBvh->_sbtNodeStack) > 0)
        {
            plBVHNode* ptNode = pl_sb_pop(ptBvh->_sbtNodeStack);

            if(gptCollision->aabb_aabb(&ptNode->tAABB, &tAABB))
            {
                if(ptNode->_uCount > 0) // is leaf
                {
                    for(uint32_t i = 0; i < ptNode->_uCount; i++)
                    {
                        uint32_t uIndex = ptBvh->_puLeafIndices[ptNode->_uOffset + i];
                        tCallback(uIndex, pUserData);
                    }
                }
                else
                {
                    pl_sb_push(ptBvh->_sbtNodeStack, &ptBvh->_ptNodes[ptNode->_uLeft]);
                    pl_sb_push(ptBvh->_sbtNodeStack, &ptBvh->_ptNodes[ptNode->_uLeft + 1]);
                }
            }
        }
    }

    pl_sb_reset(ptBvh->_sbtNodeStack); 
}

bool
pl_bvh_intersects_aabb_first(plBVH* ptBvh, plAABB tAABB, plBVHCallback tCallback, void* pUserData)
{
    if(ptBvh->_uNodeCount > 0)
    {
        pl_sb_push(ptBvh->_sbtNodeStack, &ptBvh->_ptNodes[0]);

        while(pl_sb_size(ptBvh->_sbtNodeStack) > 0)
        {
            plBVHNode* ptNode = pl_sb_pop(ptBvh->_sbtNodeStack);

            if(gptCollision->aabb_aabb(&ptNode->tAABB, &tAABB))
            {
                if(ptNode->_uCount > 0) // is leaf
                {
                    for(uint32_t i = 0; i < ptNode->_uCount; i++)
                    {
                        uint32_t uIndex = ptBvh->_puLeafIndices[ptNode->_uOffset + i];
                        if(tCallback(uIndex, pUserData))
                        {
                            pl_sb_reset(ptBvh->_sbtNodeStack); 
                            return true;
                        }
                    }
                }
                else
                {
                    pl_sb_push(ptBvh->_sbtNodeStack, &ptBvh->_ptNodes[ptNode->_uLeft]);
                    pl_sb_push(ptBvh->_sbtNodeStack, &ptBvh->_ptNodes[ptNode->_uLeft + 1]);
                }
            }
        }
    }

    pl_sb_reset(ptBvh->_sbtNodeStack);
    return false;
}

bool
pl_bvh_traverse(plBVH* ptBvh, plBVHNode** pptNodeOut, uint32_t* puIndexOut)
{
    plBVHNode* ptNode = *pptNodeOut;

    if(ptNode == NULL) // first run
    {
        if(ptBvh->_uNodeCount > 0)
        {
            ptNode = &ptBvh->_ptNodes[0];
            *pptNodeOut = ptNode;
            *puIndexOut = UINT32_MAX;
            return true;
        }
        else
            return false;
    }

    if(ptNode->_uCount > 0) // is leaf
    {

        if(*puIndexOut == UINT32_MAX) // first leaf
        {
            ptNode->_uTraverseOffset = 0;
            *puIndexOut = ptBvh->_puLeafIndices[ptNode->_uOffset];
            return true;
        }
        else if(ptNode->_uTraverseOffset < ptNode->_uCount - 1) // next leaf
        {
            ptNode->_uTraverseOffset++;
            *puIndexOut = ptBvh->_puLeafIndices[ptNode->_uOffset + ptNode->_uTraverseOffset];
            return true;
        }
        else // no more leaves
            *puIndexOut = UINT32_MAX;
    }
    else
    {
        pl_sb_push(ptBvh->_sbtNodeStack, &ptBvh->_ptNodes[ptNode->_uLeft]);
        pl_sb_push(ptBvh->_sbtNodeStack, &ptBvh->_ptNodes[ptNode->_uLeft + 1]);
    }

    // next node
    if(pl_sb_size(ptBvh->_sbtNodeStack) == 0)
    {
        *pptNodeOut = NULL;
        *puIndexOut = UINT32_MAX;
        return false;
    }
    ptNode = pl_sb_pop(ptBvh->_sbtNodeStack);
    *pptNodeOut = ptNode;
    return true;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_bvh_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plBVHI tApi = {
        .build                 = pl_bvh_build,
        .cleanup               = pl_bvh_cleanup,
        .intersects_aabb       = pl_bvh_intersects_aabb,
        .intersects_aabb_first = pl_bvh_intersects_aabb_first,
        .traverse = pl_bvh_traverse,
    };
    pl_set_api(ptApiRegistry, plBVHI, &tApi);

    gptCollision = pl_get_api_latest(ptApiRegistry, plCollisionI);
    gptMemory    = pl_get_api_latest(ptApiRegistry, plMemoryI);
}

PL_EXPORT void
pl_unload_bvh_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    if(bReload)
        return;

    const plBVHI* ptApi = pl_get_api_latest(ptApiRegistry, plBVHI);
    ptApiRegistry->remove_api(ptApi);
}
