/*
   pl_freelist_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] global data
// [SECTION] public api implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.h"
#include "pl_freelist_ext.h"

#undef pl_vnsprintf
#include "pl_memory.h"

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

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

#endif

#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_freelist_create(uint64_t uSize, uint64_t uMinSize, plFreeList* ptFreelistOut)
{
    ptFreelistOut->uSize = uSize;
    ptFreelistOut->_uMinNodeSize = uMinSize;

    ptFreelistOut->_tFreeList._ptPrev = NULL;
    ptFreelistOut->_tFreeList._ptNext = NULL;
    ptFreelistOut->_tFreeList.uSize  = 0;

    uint64_t uMaxNodeCount = uSize / ptFreelistOut->_uMinNodeSize;
    ptFreelistOut->_atNodeHoles = PL_ALLOC(uMaxNodeCount * sizeof(plFreeListNode));
    memset(ptFreelistOut->_atNodeHoles, 0, uMaxNodeCount * sizeof(plFreeListNode));

    pl_sb_resize(ptFreelistOut->_sbuFreeNodeHoleSlot, (uint32_t)uMaxNodeCount);
    for(uint64_t i = 0; i < uMaxNodeCount; i++)
    {
        ptFreelistOut->_sbuFreeNodeHoleSlot[i] = i;
        ptFreelistOut->_atNodeHoles[i]._uIndex = i;
    }

    plFreeListNode* ptNewBlock = &ptFreelistOut->_atNodeHoles[pl_sb_pop(ptFreelistOut->_sbuFreeNodeHoleSlot)];
    ptNewBlock->uSize = uSize;

    // add node
    ptNewBlock->_ptNext = ptFreelistOut->_tFreeList._ptNext;
    ptNewBlock->_ptPrev = &ptFreelistOut->_tFreeList;
    ptFreelistOut->_tFreeList._ptNext = ptNewBlock;
}

void
pl_freelist_cleanup(plFreeList* ptFreeList)
{
    pl_sb_free(ptFreeList->_sbuFreeNodeHoleSlot);
    ptFreeList->_sbuFreeNodeHoleSlot = NULL;
    PL_FREE(ptFreeList->_atNodeHoles);
    ptFreeList->_atNodeHoles = NULL;
    ptFreeList->uSize = 0;
    ptFreeList->uUsedSpace = 0;
    ptFreeList->_uMinNodeSize = 0;
    ptFreeList->_tFreeList._ptNext = NULL;
    ptFreeList->_tFreeList._ptPrev = NULL;
    ptFreeList->_tFreeList.uOffset = 0;
    ptFreeList->_tFreeList.uSize = 0;
    ptFreeList->_tFreeList._uIndex = 0;
}

plFreeListNode*
pl_freelist_get_node(plFreeList* ptFreeList, uint64_t uSize)
{
    plFreeListNode* ptBlock = NULL;

    // find best block
    uint64_t szSmallestDiff = ~(uint64_t)0;
    plFreeListNode* ptCurrentBlock = ptFreeList->_tFreeList._ptNext;
    while(ptCurrentBlock)
    {

        if(ptCurrentBlock->uSize >= uSize && (ptCurrentBlock->uSize - uSize < szSmallestDiff))
        {
            ptBlock = ptCurrentBlock;
            szSmallestDiff = ptCurrentBlock->uSize - uSize;
        }
        ptCurrentBlock = ptCurrentBlock->_ptNext;
    }

    if (ptBlock != NULL) 
    {
        // split block if big enough
        if( (ptBlock->uSize - uSize) >= ptFreeList->_uMinNodeSize)
        {
            plFreeListNode* ptNewBlock = &ptFreeList->_atNodeHoles[pl_sb_pop(ptFreeList->_sbuFreeNodeHoleSlot)];
            ptNewBlock->uSize = ptBlock->uSize - uSize;
            ptBlock->uSize = uSize;
            ptNewBlock->uOffset = ptBlock->uOffset + ptBlock->uSize;

            // add node
            if(ptBlock->_ptNext)
            {
                ptBlock->_ptNext->_ptPrev = ptNewBlock;
            }
            ptNewBlock->_ptNext = ptBlock->_ptNext;
            ptNewBlock->_ptPrev = ptBlock;
            ptBlock->_ptNext = ptNewBlock;
        }
        ptFreeList->uUsedSpace += ptBlock->uSize;

        // delete node
        if(ptBlock->_ptNext)
        {
            ptBlock->_ptNext->_ptPrev = ptBlock->_ptPrev;
        }
        ptBlock->_ptPrev->_ptNext = ptBlock->_ptNext;
        ptBlock->_ptNext = NULL;
        ptBlock->_ptPrev = NULL;
    }
    return ptBlock;
}

void
pl_freelist_return_node(plFreeList* ptFreeList, plFreeListNode* ptNode)
{
    ptFreeList->uUsedSpace -= ptNode->uSize;


    // If free list is empty, add as first node
    if (ptFreeList->_tFreeList._ptNext == NULL)
    {
        ptNode->_ptPrev = &ptFreeList->_tFreeList;
        ptNode->_ptNext = NULL;
        ptFreeList->_tFreeList._ptNext = ptNode;
        return;
    }

    // 1) Insert by uOffset order (not by pointer!)
    plFreeListNode* it = ptFreeList->_tFreeList._ptNext;
    plFreeListNode* insert_after = &ptFreeList->_tFreeList;

    while (it && it->uOffset < ptNode->uOffset)
    {
        insert_after = it;
        it = it->_ptNext;
    }

    ptNode->_ptPrev = insert_after;
    ptNode->_ptNext = it;
    insert_after->_ptNext = ptNode;
    if (it) it->_ptPrev = ptNode;

    // 2) Coalesce with previous if adjacent
    if (ptNode->_ptPrev != &ptFreeList->_tFreeList)
    {
        plFreeListNode* prev = ptNode->_ptPrev;
        if (prev->uOffset + prev->uSize == ptNode->uOffset)
        {
            prev->uSize += ptNode->uSize;

            // Recycle this node descriptor
            pl_sb_push(ptFreeList->_sbuFreeNodeHoleSlot, ptNode->_uIndex);

            // Remove ptNode from the list
            if (ptNode->_ptNext) ptNode->_ptNext->_ptPrev = prev;
            prev->_ptNext = ptNode->_ptNext;

            // Clean up metadata
            ptNode->_ptPrev = ptNode->_ptNext = NULL;
            ptNode->uOffset = 0;
            ptNode->uSize   = 0;

            // Continue coalescing using the merged 'prev' block
            ptNode = prev;
        }
    }

    // 3) Coalesce forward as long as next is adjacent
    while (ptNode->_ptNext &&
           (ptNode->uOffset + ptNode->uSize) == ptNode->_ptNext->uOffset)
    {
        plFreeListNode* next = ptNode->_ptNext;
        plFreeListNode* nn   = next->_ptNext;

        ptNode->uSize += next->uSize;
        pl_sb_push(ptFreeList->_sbuFreeNodeHoleSlot, next->_uIndex);

        // unlink next
        ptNode->_ptNext = nn;
        if (nn) nn->_ptPrev = ptNode;

        next->_ptPrev = next->_ptNext = NULL;
        next->uOffset = 0;
        next->uSize   = 0;
    }

}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_freelist_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plFreeListI tApi = {
        .create      = pl_freelist_create,
        .cleanup     = pl_freelist_cleanup,
        .get_node    = pl_freelist_get_node,
        .return_node = pl_freelist_return_node
    };
    pl_set_api(ptApiRegistry, plFreeListI, &tApi);
}

void
pl_unload_freelist_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;
        
    const plFreeListI* ptApi = pl_get_api_latest(ptApiRegistry, plFreeListI);
    ptApiRegistry->remove_api(ptApi);
}
