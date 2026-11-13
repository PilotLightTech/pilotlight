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

#undef pl_vsnprintf
#include "pl_memory.h"

#define PL_FREELIST_NODES_PER_CHUNK 64

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
// [SECTION] internal api
//-----------------------------------------------------------------------------

#define PL_FREELIST_NODES_PER_CHUNK 64

static plFreeListNode*
pl__freelist_acquire_node(plFreeList* ptFreeList)
{
    // Grow the node pool when no free descriptors remain
    if(pl_sb_size(ptFreeList->_sbuFreeNodeHoleSlot) == 0)
    {
        const uint64_t uChunkIndex = pl_sb_size(ptFreeList->_sbptNodeChunks);
        const uint64_t uBaseIndex = uChunkIndex * PL_FREELIST_NODES_PER_CHUNK;

        // Assuming 32-bit node indices / stretchy-buffer sizes
        if(uBaseIndex + PL_FREELIST_NODES_PER_CHUNK > UINT32_MAX)
            return NULL;

        // Allocate a new, permanent chunk
        plFreeListNode* ptChunk = PL_ALLOC(sizeof(plFreeListNode) * PL_FREELIST_NODES_PER_CHUNK);
        memset(ptChunk, 0, sizeof(plFreeListNode) * PL_FREELIST_NODES_PER_CHUNK);

        // Store the pointer, not the nodes themselves.
        // This array can safely be reallocated.
        pl_sb_push(ptFreeList->_sbptNodeChunks, ptChunk);

        // Register all new descriptors as available
        const uint32_t uOldCount = pl_sb_size(ptFreeList->_sbuFreeNodeHoleSlot);

        pl_sb_resize(ptFreeList->_sbuFreeNodeHoleSlot,uOldCount + PL_FREELIST_NODES_PER_CHUNK);

        for(uint32_t i = 0; i < PL_FREELIST_NODES_PER_CHUNK; i++)
        {
            const uint32_t uIndex = (uint32_t)uBaseIndex + i;
            ptChunk[i]._uIndex = uIndex;
            ptFreeList->_sbuFreeNodeHoleSlot[uOldCount + i] = uIndex;
        }
    }

    // Acquire an available descriptor
    const uint32_t uIndex = (uint32_t)pl_sb_pop(ptFreeList->_sbuFreeNodeHoleSlot);
    const uint32_t uChunk = uIndex / PL_FREELIST_NODES_PER_CHUNK;
    const uint32_t uLocal = uIndex % PL_FREELIST_NODES_PER_CHUNK;
    return &ptFreeList->_sbptNodeChunks[uChunk][uLocal];
}

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_freelist_create(uint64_t uSize, uint64_t uMinSize, plFreeList* ptFreelistOut)
{
    memset(ptFreelistOut, 0, sizeof(plFreeList));
    ptFreelistOut->uSize = uSize;
    ptFreelistOut->_uMinNodeSize = uMinSize;

    ptFreelistOut->_tFreeList._ptPrev = NULL;
    ptFreelistOut->_tFreeList._ptNext = NULL;
    ptFreelistOut->_tFreeList.uSize  = 0;
    ptFreelistOut->uUsedSpace  = 0;

    plFreeListNode* ptNewBlock = pl__freelist_acquire_node(ptFreelistOut);
    PL_ASSERT(ptNewBlock);

    ptNewBlock->uSize = uSize;
    ptNewBlock->uOffset = 0;

    // add node
    ptNewBlock->_ptNext = NULL;
    ptNewBlock->_ptPrev = &ptFreelistOut->_tFreeList;
    ptFreelistOut->_tFreeList._ptNext = ptNewBlock;
}

void
pl_freelist_cleanup(plFreeList* ptFreeList)
{
    // Free every allocated node chunk
    for(uint32_t i = 0; i < pl_sb_size(ptFreeList->_sbptNodeChunks); i++)
    {
        PL_FREE(ptFreeList->_sbptNodeChunks[i]);
    }

    // Free the chunk pointer array
    pl_sb_free(ptFreeList->_sbptNodeChunks);

    // Free the descriptor index stack
    pl_sb_free(ptFreeList->_sbuFreeNodeHoleSlot);

    memset(ptFreeList, 0, sizeof(plFreeList));
}

plFreeListNode*
pl_freelist_get_node(plFreeList* ptFreeList, uint64_t uSize)
{
    uSize = uSize > ptFreeList->_uMinNodeSize ? uSize : ptFreeList->_uMinNodeSize;

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

        if(szSmallestDiff == 0)
            break;

        ptCurrentBlock = ptCurrentBlock->_ptNext;
    }

    if (ptBlock != NULL) 
    {
        // split block if big enough
        if(ptBlock->uSize - uSize >= ptFreeList->_uMinNodeSize)
        {
            plFreeListNode* ptNewBlock = pl__freelist_acquire_node(ptFreeList);

            if(ptNewBlock)
            {
                ptNewBlock->uSize = ptBlock->uSize - uSize;
                ptBlock->uSize = uSize;
                ptNewBlock->uOffset = ptBlock->uOffset + ptBlock->uSize;
                ptNewBlock->_bAllocated = false;

                // add node
                if(ptBlock->_ptNext)
                {
                    ptBlock->_ptNext->_ptPrev = ptNewBlock;
                }

                ptNewBlock->_ptNext = ptBlock->_ptNext;
                ptNewBlock->_ptPrev = ptBlock;
                ptBlock->_ptNext = ptNewBlock;
            }
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
    if(ptBlock)
        ptBlock->_bAllocated = true;
    return ptBlock;
}

void
pl_freelist_return_node(plFreeList* ptFreeList, plFreeListNode* ptNode)
{
    ptFreeList->uUsedSpace -= ptNode->uSize;
    PL_ASSERT(ptNode->_bAllocated);
    ptNode->_bAllocated = false;

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
    if (it)
        it->_ptPrev = ptNode;

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
            if (ptNode->_ptNext)
                ptNode->_ptNext->_ptPrev = prev;
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
    while (ptNode->_ptNext && (ptNode->uOffset + ptNode->uSize) == ptNode->_ptNext->uOffset)
    {
        plFreeListNode* next = ptNode->_ptNext;
        plFreeListNode* nn   = next->_ptNext;

        ptNode->uSize += next->uSize;
        pl_sb_push(ptFreeList->_sbuFreeNodeHoleSlot, next->_uIndex);

        // unlink next
        ptNode->_ptNext = nn;
        if (nn)
            nn->_ptPrev = ptNode;

        next->_ptPrev = next->_ptNext = NULL;
        next->uOffset = 0;
        next->uSize   = 0;
    }
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

bool
pl_freelist_grow(plFreeList* ptFreeList, uint64_t uSize)
{
    if(uSize <= ptFreeList->uSize)
        return true;

    plFreeListNode* ptTail = &ptFreeList->_tFreeList;
    while(ptTail->_ptNext)
        ptTail = ptTail->_ptNext;

    if(ptTail != &ptFreeList->_tFreeList && ptTail->uOffset + ptTail->uSize == ptFreeList->uSize)
        ptTail->uSize += uSize - ptFreeList->uSize;
    else
    {
        plFreeListNode* ptNew = pl__freelist_acquire_node(ptFreeList);
        if(!ptNew)
            return false;
        ptNew->uOffset = ptFreeList->uSize;
        ptNew->uSize = uSize - ptFreeList->uSize;
        ptNew->_bAllocated = false;
        ptNew->_ptPrev = ptTail;
        ptNew->_ptNext = NULL;
        ptTail->_ptNext = ptNew;
    }
    ptFreeList->uSize = uSize;
    return true;
}

void
pl_load_freelist_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plFreeListI tApi = {
        .create      = pl_freelist_create,
        .cleanup     = pl_freelist_cleanup,
        .get_node    = pl_freelist_get_node,
        .return_node = pl_freelist_return_node,
        .grow        = pl_freelist_grow
    };
    pl_set_api(ptApiRegistry, plFreeListI, &tApi);
    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
}

void
pl_unload_freelist_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;
        
    const plFreeListI* ptApi = pl_get_api_latest(ptApiRegistry, plFreeListI);
    ptApiRegistry->remove_api(ptApi);
}
