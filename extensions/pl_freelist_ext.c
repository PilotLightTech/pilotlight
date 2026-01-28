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
    plFreeListNode* ptCurrentBlock = &ptFreeList->_tFreeList;
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

    // if the free list is not empty
    if(ptFreeList->_tFreeList._ptNext != NULL)
    {
        // Add node to free list
        plFreeListNode* ptFreeBlock = ptFreeList->_tFreeList._ptNext;   
        while(ptFreeBlock)
        {
            // Insert new node to middle
            if(ptFreeBlock > ptNode)
            {
                plFreeListNode *_ptNext = ptFreeBlock;
                plFreeListNode *_ptPrev = ptFreeBlock->_ptPrev;

                _ptNext->_ptPrev = ptNode;
                ptNode->_ptNext = _ptNext;
                ptNode->_ptPrev = _ptPrev;
                _ptPrev->_ptNext = ptNode;
                break;
                
            }

            // There isn't a next block so Insert new node to the end
            if(ptFreeBlock->_ptNext == NULL)
            {                
                ptNode->_ptNext = NULL;
                ptNode->_ptPrev = ptFreeBlock;
                ptFreeBlock->_ptNext = ptNode;
                break;
            }

            ptFreeBlock = ptFreeBlock->_ptNext;
        } 

        // Defrag
        plFreeListNode* _ptPrevBlock = ptNode->_ptPrev;
        plFreeListNode* _ptNextBlock = ptNode->_ptNext;

        if(_ptPrevBlock != &ptFreeList->_tFreeList)
        {
            // if prev block and block are adjacent
            if((_ptPrevBlock->uOffset + _ptPrevBlock->uSize) == ptNode->uOffset)
            {
                _ptPrevBlock->uSize += ptNode->uSize;
                pl_sb_push(ptFreeList->_sbuFreeNodeHoleSlot, ptNode->_uIndex);
                
                // delete node
                if(_ptNextBlock != NULL) {_ptNextBlock->_ptPrev = _ptPrevBlock; }
                _ptPrevBlock->_ptNext = _ptNextBlock;
                ptNode->_ptNext = NULL;
                ptNode->_ptPrev = NULL;
                ptNode->uOffset = 0;
                ptNode->uSize = 0;

                // Allows us to defrag with the next block without checking if we defragged with this one
                ptNode = _ptPrevBlock; 
            }
        }

        if(_ptNextBlock != NULL)
        {

            plFreeListNode* _ptNextNextBlock = _ptNextBlock->_ptNext;

            // if block and next block are adjacent
            if((ptNode->uOffset + ptNode->uSize) == _ptNextBlock->uOffset)
            {
                ptNode->uSize += _ptNextBlock->uSize;
                pl_sb_push(ptFreeList->_sbuFreeNodeHoleSlot, _ptNextBlock->_uIndex);
                
                // delete next node
                if(_ptNextNextBlock != NULL)
                {
                    _ptNextNextBlock->_ptPrev = ptNode;
                }
                ptNode->_ptNext = _ptNextNextBlock;
                _ptNextBlock->_ptNext = NULL;
                _ptNextBlock->_ptPrev = NULL;
                _ptNextBlock->uOffset = 0;
                _ptNextBlock->uSize = 0;

            }
        }
    }
    else 
    {
        // Add node to the front of the free list if list is empty
        ptNode->_ptNext = NULL;
        ptNode->_ptPrev = &ptFreeList->_tFreeList;
        ptFreeList->_tFreeList._ptNext = ptNode;
    }
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
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

PL_EXPORT void
pl_unload_freelist_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;
        
    const plFreeListI* ptApi = pl_get_api_latest(ptApiRegistry, plFreeListI);
    ptApiRegistry->remove_api(ptApi);
}
