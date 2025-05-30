/*
   pl_gpu_allocaters_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] defines
// [SECTION] internal api implementation
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.h"
#include "pl_gpu_allocators_ext.h"
#include "pl_graphics_ext.h"
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

    static const plGraphicsI* gptGfx = NULL;
    static const plDataRegistryI* gptDataRegistry = NULL;
#endif

#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_DEVICE_BUDDY_BLOCK_SIZE
    #define PL_DEVICE_BUDDY_BLOCK_SIZE 268435456
#endif

#ifndef PL_DEVICE_LOCAL_LEVELS
    #define PL_DEVICE_LOCAL_LEVELS 8
#endif

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

typedef struct _plDeviceAllocatorData
{
    plDeviceMemoryAllocatorI* ptAllocator;
    plDevice*                 ptDevice;
    plDeviceMemoryAllocation* sbtBlocks;
    uint32_t*                 sbtFreeBlockIndices;

    // buddy allocator data
    plDeviceAllocationRange*  sbtNodes;
    uint32_t                  auFreeList[PL_DEVICE_LOCAL_LEVELS];
} plDeviceAllocatorData;

static plDeviceMemoryAllocation*
pl_get_allocator_blocks(const plDeviceMemoryAllocatorI* ptAllocator, uint32_t* puSizeOut)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptAllocator->ptInst;

    if(puSizeOut)
    {
        *puSizeOut = pl_sb_size(ptData->sbtBlocks);
    }
    return ptData->sbtBlocks;
}

static plDeviceAllocationRange*
pl_get_allocator_ranges(const plDeviceMemoryAllocatorI* ptAllocator, uint32_t* puSizeOut)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptAllocator->ptInst;

    if(puSizeOut)
    {
        *puSizeOut = pl_sb_size(ptData->sbtNodes);
    }
    return ptData->sbtNodes;
}

static void
pl__add_node_to_freelist(plDeviceAllocatorData* ptData, uint32_t uLevel, uint32_t uNode)
{
    plDeviceAllocationRange* ptNode = &ptData->sbtNodes[uNode];
    ptNode->ulUsedSize = 0;
    ptData->sbtNodes[uNode].uNextNode = ptData->auFreeList[uLevel];
    ptData->auFreeList[uLevel] = uNode;
}

static void
pl__remove_node_from_freelist(plDeviceAllocatorData* ptData, uint32_t uLevel, uint32_t uNode)
{

    bool bFound = false;
    if(ptData->auFreeList[uLevel] == uNode)
    {
        ptData->auFreeList[uLevel] = ptData->sbtNodes[uNode].uNextNode;
        bFound = true;
    }
    else
    {
        uint32_t uNextNode = ptData->auFreeList[uLevel];
        while(uNextNode != UINT32_MAX)
        {
            uint32_t uPrevNode = uNextNode;
            uNextNode = ptData->sbtNodes[uPrevNode].uNextNode;
            
            if(uNextNode == uNode)
            {
                ptData->sbtNodes[uPrevNode].uNextNode = ptData->sbtNodes[uNode].uNextNode;
                bFound = true;
                break;
            }
        }
    }

    plDeviceAllocationRange* ptNode = &ptData->sbtNodes[uNode];
    ptNode->ulUsedSize = UINT64_MAX; // ignored
    ptNode->uNextNode = UINT32_MAX;
    PL_ASSERT(bFound && "could not find node to remove");
}

static uint32_t
pl_create_device_node(struct plDeviceMemoryAllocatorO* ptInst, uint32_t uMemoryType)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;
    uint32_t uNode = UINT32_MAX;

    plDeviceMemoryAllocation tBlock = {
        .uHandle      = 0,
        .ulSize       = PL_DEVICE_BUDDY_BLOCK_SIZE,
        .ulMemoryType = uMemoryType
    };

    uNode = pl_sb_size(ptData->sbtNodes);
    uint32_t uNodeIndex = uNode;
    pl_sb_resize(ptData->sbtNodes, pl_sb_size(ptData->sbtNodes) + (1 << PL_DEVICE_LOCAL_LEVELS) - 1);
    const uint32_t uBlockIndex = pl_sb_size(ptData->sbtBlocks);
    for(uint32_t uLevelIndex = 0; uLevelIndex < PL_DEVICE_LOCAL_LEVELS; uLevelIndex++)
    {
        const uint64_t uSizeOfLevel = PL_DEVICE_BUDDY_BLOCK_SIZE / ((uint64_t)1 << (uint64_t)uLevelIndex);
        const uint32_t uLevelBlockCount = (1 << uLevelIndex);
        uint64_t uCurrentOffset = 0;
        for(uint32_t i = 0; i < uLevelBlockCount; i++)
        {
            ptData->sbtNodes[uNodeIndex].uNodeIndex   = uNodeIndex;
            ptData->sbtNodes[uNodeIndex].uNextNode    = UINT32_MAX;
            ptData->sbtNodes[uNodeIndex].ulOffset     = uCurrentOffset;
            ptData->sbtNodes[uNodeIndex].ulTotalSize  = uSizeOfLevel;
            ptData->sbtNodes[uNodeIndex].ulBlockIndex = uBlockIndex;
            strncpy(ptData->sbtNodes[uNodeIndex].acName, "not used", PL_MAX_NAME_LENGTH);
            uCurrentOffset += uSizeOfLevel;
            uNodeIndex++;
        }
    }
    pl_sb_push(ptData->sbtBlocks, tBlock);
    return uNode;
}

static uint32_t
pl__get_device_node(struct plDeviceMemoryAllocatorO* ptInst, uint32_t uLevel, uint32_t uMemoryType)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;
    uint32_t uNode = UINT32_MAX;

    if(uLevel == 0)
    {
        if(ptData->auFreeList[0] == UINT32_MAX)  // no nodes available
        {
            uNode = pl_create_device_node(ptInst, uMemoryType);
            pl__add_node_to_freelist(ptData, 0, uNode);
        }
        else // nodes available
        {
            // look for block with correct memory type
            uint32_t uNextNode = ptData->auFreeList[0];
            while(uNextNode != UINT32_MAX)
            {  
                if(ptData->sbtBlocks[ptData->sbtNodes[uNextNode].ulBlockIndex].ulMemoryType == (uint64_t)uMemoryType)
                {
                    uNode = uNextNode;
                    break;
                }
                uNextNode = ptData->sbtNodes[uNextNode].uNextNode;
            }

            if(uNode == UINT32_MAX) // could not find block with correct memory type
            {
                uNode = pl_create_device_node(ptInst, uMemoryType);
                pl__add_node_to_freelist(ptData, 0, uNode);
            }
        }
    }
    else if(ptData->auFreeList[uLevel] == UINT32_MAX) // no nodes available at the required level
    {
        // get bigger block and split it and return left block
        uint32_t uParentNode = pl__get_device_node(ptInst, uLevel - 1, uMemoryType);
        plDeviceAllocationRange* ptParentNode = &ptData->sbtNodes[uParentNode];
        ptParentNode->ulUsedSize = UINT64_MAX; // ignore

        const uint64_t uSizeOfLevel = PL_DEVICE_BUDDY_BLOCK_SIZE / ((uint64_t)1 << (uint64_t)(uLevel - 1));
        const uint32_t uLevelBlockCount = (1 << (uLevel - 1));
        uint32_t uIndexInLevel = (uint32_t)(ptParentNode->ulOffset / uSizeOfLevel);

        const uint32_t uLeftIndex  = uParentNode + uLevelBlockCount + uIndexInLevel;
        const uint32_t uRightIndex = uParentNode + uLevelBlockCount + uIndexInLevel + 1;

        pl__add_node_to_freelist(ptData, uLevel, uLeftIndex);
        pl__add_node_to_freelist(ptData, uLevel, uRightIndex);

        uNode = uLeftIndex;
    }
    else // nodes available at required level
    {
        // look for block with correct memory type
        uint32_t uNextNode = ptData->auFreeList[uLevel];
        while(uNextNode != UINT32_MAX)
        {  
            const uint64_t ulBlockIndex = ptData->sbtNodes[uNextNode].ulBlockIndex;
            if(ptData->sbtBlocks[ulBlockIndex].ulMemoryType == (uint64_t)uMemoryType)
            {
                uNode = uNextNode;
                break;
            }
            uNextNode = ptData->sbtNodes[uNextNode].uNextNode;
        }

        if(uNode == UINT32_MAX) // could not find block with correct memory type
        {
            uint32_t uParentNode = pl__get_device_node(ptInst, uLevel - 1, uMemoryType);
            plDeviceAllocationRange* ptParentNode = &ptData->sbtNodes[uParentNode];

            const uint64_t uSizeOfLevel = PL_DEVICE_BUDDY_BLOCK_SIZE / ((uint64_t)1 << (uint64_t)(uLevel - 1));
            const uint32_t uLevelBlockCount = (1 << (uLevel - 1));
            uint32_t uIndexInLevel = (uint32_t)(ptParentNode->ulOffset / uSizeOfLevel);

            const uint32_t uLeftIndex  = uParentNode + uLevelBlockCount + uIndexInLevel;
            const uint32_t uRightIndex = uParentNode + uLevelBlockCount + uIndexInLevel + 1;

            pl__add_node_to_freelist(ptData, uLevel, uLeftIndex);
            pl__add_node_to_freelist(ptData, uLevel, uRightIndex);
            uNode = uLeftIndex;
        }
    }

    pl__remove_node_from_freelist(ptData, uLevel, uNode);
    return uNode;
}

static inline bool
pl__is_node_free(plDeviceAllocatorData* ptData, uint32_t uNode)
{

    // find what level we need
    uint32_t uLevel = 0;
    for(; uLevel < PL_DEVICE_LOCAL_LEVELS; uLevel++)
    {
        const uint64_t uLevelSize = PL_DEVICE_BUDDY_BLOCK_SIZE / (1 << uLevel);
        if(uLevelSize == ptData->sbtNodes[uNode].ulTotalSize)
        {
            break; 
        }
    }
    uLevel = pl_minu(uLevel, PL_DEVICE_LOCAL_LEVELS - 1);

    // check if node is in freelist
    bool bInFreeList = false;
    uint32_t uNextNode = ptData->auFreeList[uLevel];
    while(uNextNode != UINT32_MAX)
    {

        if(uNextNode == ptData->sbtNodes[uNextNode].uNextNode)
            break;

        if(uNextNode == uNode)
        {
            bInFreeList = true;
            break;
        }
        uNextNode = ptData->sbtNodes[uNextNode].uNextNode;
    }
    
    const bool bFree = ptData->sbtNodes[uNode].ulUsedSize == 0;
    if(bFree)
    {
        PL_ASSERT(bInFreeList && "free item was not in list");
    }
    return bFree;
}

static void
pl__coalesce_nodes(plDeviceAllocatorData* ptData, uint32_t uLevel, uint32_t uNode)
{
    plDeviceAllocationRange* ptNode = &ptData->sbtNodes[uNode];

    // just return node to freelist
    if(uLevel == 0)
    {
        pl__add_node_to_freelist(ptData, uLevel, uNode);
        return;
    }

    bool bBothFree = false;
    uint32_t uLeftNode = uNode;
    uint32_t uRightNode = uNode + 1;

    if(ptNode->ulBlockIndex % 2 == 0)
    {
        if(uNode % 2 == 1) // left node
        {
            if(pl__is_node_free(ptData, uRightNode))
            {

                bBothFree = true;
                pl__remove_node_from_freelist(ptData, uLevel, uRightNode);
            }
        }
        else
        {
            uLeftNode = uNode - 1;
            uRightNode = uNode;
            if(pl__is_node_free(ptData, uLeftNode))
            {
                bBothFree = true;
                pl__remove_node_from_freelist(ptData, uLevel, uLeftNode);
            }
        }
    }
    else
    {
        if(uNode % 2 == 1) // right node
        {
            if(pl__is_node_free(ptData, uLeftNode))
            {
                bBothFree = true;
                pl__remove_node_from_freelist(ptData, uLevel, uLeftNode);
            }
        }
        else
        {
            if(pl__is_node_free(ptData, uRightNode))
            {
                bBothFree = true;
                pl__remove_node_from_freelist(ptData, uLevel, uRightNode);
            }
        }
    }
    
    if(bBothFree) // need to coalese
    {

        if(uLevel > 1)
        {
            // find parent node
            const uint64_t uSizeOfParentLevel = PL_DEVICE_BUDDY_BLOCK_SIZE / ((uint64_t)1 << (uint64_t)(uLevel - 1));
            const uint32_t uParentLevelBlockCount = (1 << (uLevel - 1));
            uint32_t uIndexInLevel = (uint32_t)(ptData->sbtNodes[uLeftNode].ulOffset / uSizeOfParentLevel);
            const uint32_t uParentNode = uLeftNode - uParentLevelBlockCount - uIndexInLevel;
            pl__coalesce_nodes(ptData, uLevel - 1, uParentNode);
        }
        else
        {
            // find parent node
            const uint32_t uParentNode = uLeftNode - 1;
            pl__add_node_to_freelist(ptData, 0, uParentNode);
        }
        ptNode->ulUsedSize = UINT64_MAX; // ignored
    }
    else
    {
        pl__add_node_to_freelist(ptData, uLevel, uNode);
    }

}

static plDeviceMemoryAllocation
pl_allocate_dedicated(
    struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter,
    uint64_t ulSize, uint64_t ulAlignment, const char* pcName)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;

    plDeviceMemoryAllocation tBlock = gptGfx->allocate_memory(ptData->ptDevice, ulSize, PL_MEMORY_FLAGS_DEVICE_LOCAL, uTypeFilter, pcName);

    plDeviceMemoryAllocation tAllocation = {
        .pHostMapped  = NULL,
        .uHandle      = tBlock.uHandle,
        .ulOffset     = 0,
        .ulSize       = ulSize,
        .ptAllocator  = ptData->ptAllocator,
        .tMemoryFlags = PL_MEMORY_FLAGS_DEVICE_LOCAL
    };

    uint32_t uBlockIndex = pl_sb_size(ptData->sbtBlocks);
    if(pl_sb_size(ptData->sbtFreeBlockIndices) > 0)
        uBlockIndex = pl_sb_pop(ptData->sbtFreeBlockIndices);
    else
        pl_sb_add(ptData->sbtBlocks);

    plDeviceAllocationRange tRange = {
        .ulOffset     = 0,
        .ulTotalSize  = ulSize,
        .ulUsedSize   = ulSize,
        .ulBlockIndex = uBlockIndex
    };
    pl_sprintf(tRange.acName, "%s", pcName);

    pl_sb_push(ptData->sbtNodes, tRange);
    ptData->sbtBlocks[uBlockIndex] = tBlock;
    return tAllocation;
}

static void
pl_free_dedicated(struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;

    uint32_t uBlockIndex = 0;
    uint32_t uNodeIndex = 0;
    for(uint32_t i = 0; i < pl_sb_size(ptData->sbtNodes); i++)
    {
        plDeviceAllocationRange* ptNode = &ptData->sbtNodes[i];
        plDeviceMemoryAllocation* ptBlock = &ptData->sbtBlocks[ptNode->ulBlockIndex];

        if(ptBlock->uHandle == ptAllocation->uHandle)
        {
            uNodeIndex = i;
            uBlockIndex = (uint32_t)ptNode->ulBlockIndex;
            ptBlock->ulSize = 0;
            break;
        }
    }
    pl_sb_del_swap(ptData->sbtNodes, uNodeIndex);
    pl_sb_push(ptData->sbtFreeBlockIndices, uBlockIndex);

    gptGfx->free_memory(ptData->ptDevice, &ptData->sbtBlocks[uBlockIndex]);

    ptAllocation->pHostMapped  = NULL;
    ptAllocation->uHandle      = 0;
    ptAllocation->ulOffset     = 0;
    ptAllocation->ulSize       = 0;
}

static inline uint32_t
pl__get_buddy_level(uint64_t ulSize)
{
    uint32_t uLevel = 0;
    for(uint32_t i = 0; i < PL_DEVICE_LOCAL_LEVELS; i++)
    {
        const uint64_t uLevelSize = PL_DEVICE_BUDDY_BLOCK_SIZE / (1 << i);
        if(uLevelSize <= ulSize)
        {
            break;
        }
        uLevel = i;
    }
    return uLevel;
}

static plDeviceMemoryAllocation
pl_allocate_buddy(
    struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter,
    uint64_t ulSize, uint64_t ulAlignment, const char* pcName)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;

    if(ulAlignment > 0 && ulSize < PL_DEVICE_BUDDY_BLOCK_SIZE)
        ulSize = ulSize + (ulAlignment - 1);

    const uint32_t uLevel = pl__get_buddy_level(ulSize);
    const uint32_t uNode = pl__get_device_node(ptInst, uLevel, 0);
    PL_ASSERT(uNode != UINT32_MAX);

    plDeviceAllocationRange* ptNode = &ptData->sbtNodes[uNode];
    strncpy(ptNode->acName, pcName, PL_MAX_NAME_LENGTH);
    ptNode->ulUsedSize = ulSize;

    const uint32_t uBlockCount =  pl_sb_size(ptData->sbtBlocks);
    plDeviceMemoryAllocation* ptBlock = &ptData->sbtBlocks[ptNode->ulBlockIndex];

    plDeviceMemoryAllocation tAllocation = {
        .pHostMapped  = NULL,
        .uHandle      = (uint64_t)ptBlock->uHandle,
        .ulOffset     = ptNode->ulOffset,
        .ulSize       = ulSize,
        .ptAllocator  = ptData->ptAllocator,
        .tMemoryFlags = PL_MEMORY_FLAGS_DEVICE_LOCAL
    };

    if(ulAlignment > 0)
        tAllocation.ulOffset = (((tAllocation.ulOffset) + ((ulAlignment)-1)) & ~((ulAlignment)-1));

    if(tAllocation.uHandle == 0)
    {
        ptBlock->uHandle = gptGfx->allocate_memory(ptData->ptDevice, PL_DEVICE_BUDDY_BLOCK_SIZE, PL_MEMORY_FLAGS_DEVICE_LOCAL, uTypeFilter, "Buddy Heap").uHandle;
        tAllocation.uHandle = (uint64_t)ptBlock->uHandle;
    }

    return tAllocation;
}

static void
pl_free_buddy(struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;

    // find associated node
    uint32_t uNodeIndex = 0;
    plDeviceAllocationRange* ptNode = NULL;
    for(uint32_t i = 0; i < pl_sb_size(ptData->sbtNodes); i++)
    {
        plDeviceAllocationRange* ptIntermediateNode = &ptData->sbtNodes[i];
        plDeviceMemoryAllocation* ptBlock = &ptData->sbtBlocks[ptIntermediateNode->ulBlockIndex];

        if(ptBlock->uHandle == ptAllocation->uHandle &&
            ptIntermediateNode->ulOffset == ptAllocation->ulOffset &&
            ptIntermediateNode->ulUsedSize == ptAllocation->ulSize)
        {
            ptNode = &ptData->sbtNodes[i];
            uNodeIndex = (uint32_t)i;
            break;
        }
    }

    // find what level we need
    uint32_t uLevel = 0;
    for(; uLevel < PL_DEVICE_LOCAL_LEVELS; uLevel++)
    {
        const uint64_t uLevelSize = PL_DEVICE_BUDDY_BLOCK_SIZE / (1 << uLevel);
        if(uLevelSize == ptNode->ulTotalSize)
        {
            break; 
        }
    }
    uLevel = pl_minu(uLevel, PL_DEVICE_LOCAL_LEVELS - 1);
    pl__coalesce_nodes(ptData, uLevel, uNodeIndex);
    strncpy(ptNode->acName, "not used", PL_MAX_NAME_LENGTH);
}

static plDeviceMemoryAllocation
pl_allocate_staging_uncached(
    struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter,
    uint64_t ulSize, uint64_t ulAlignment, const char* pcName)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;

    plDeviceMemoryAllocation tBlock = gptGfx->allocate_memory(ptData->ptDevice, ulSize,
        PL_MEMORY_FLAGS_HOST_VISIBLE | PL_MEMORY_FLAGS_HOST_COHERENT, uTypeFilter, pcName);

    plDeviceMemoryAllocation tAllocation = {
        .pHostMapped  = tBlock.pHostMapped,
        .uHandle      = tBlock.uHandle,
        .ulOffset     = 0,
        .ulSize       = ulSize,
        .ptAllocator  = ptData->ptAllocator,
        .tMemoryFlags = PL_MEMORY_FLAGS_HOST_VISIBLE | PL_MEMORY_FLAGS_HOST_COHERENT
    };

    uint32_t uBlockIndex = pl_sb_size(ptData->sbtBlocks);
    if(pl_sb_size(ptData->sbtFreeBlockIndices) > 0)
        uBlockIndex = pl_sb_pop(ptData->sbtFreeBlockIndices);
    else
        pl_sb_add(ptData->sbtBlocks);

    plDeviceAllocationRange tRange = {
        .ulOffset     = 0,
        .ulTotalSize  = ulSize,
        .ulUsedSize   = ulSize,
        .ulBlockIndex = uBlockIndex
    };
    pl_sprintf(tRange.acName, "%s", pcName);

    pl_sb_push(ptData->sbtNodes, tRange);
    ptData->sbtBlocks[uBlockIndex] = tBlock;
    return tAllocation;
}

static void
pl_free_staging_uncached(struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;

    uint32_t uBlockIndex = 0;
    uint32_t uNodeIndex = 0;
    for(uint32_t i = 0; i < pl_sb_size(ptData->sbtNodes); i++)
    {
        plDeviceAllocationRange* ptNode = &ptData->sbtNodes[i];
        plDeviceMemoryAllocation* ptBlock = &ptData->sbtBlocks[ptNode->ulBlockIndex];

        if(ptBlock->uHandle == ptAllocation->uHandle)
        {
            uNodeIndex = i;
            uBlockIndex = (uint32_t)ptNode->ulBlockIndex;
            ptBlock->ulSize = 0;
            break;
        }
    }
    pl_sb_del_swap(ptData->sbtNodes, uNodeIndex);
    pl_sb_push(ptData->sbtFreeBlockIndices, uBlockIndex);

    gptGfx->free_memory(ptData->ptDevice, &ptData->sbtBlocks[uBlockIndex]);

    ptAllocation->pHostMapped  = NULL;
    ptAllocation->uHandle      = 0;
    ptAllocation->ulOffset     = 0;
    ptAllocation->ulSize       = 0;
}

static plDeviceMemoryAllocation
pl_allocate_staging_uncached_buddy(
    struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter,
    uint64_t ulSize, uint64_t ulAlignment, const char* pcName)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;

    if(ulAlignment > 0 && ulSize < PL_DEVICE_BUDDY_BLOCK_SIZE)
        ulSize = ulSize + (ulAlignment - 1);

    const uint32_t uLevel = pl__get_buddy_level(ulSize);
    const uint32_t uNode = pl__get_device_node(ptInst, uLevel, 0);
    PL_ASSERT(uNode != UINT32_MAX);

    plDeviceAllocationRange* ptNode = &ptData->sbtNodes[uNode];
    strncpy(ptNode->acName, pcName, PL_MAX_NAME_LENGTH);
    ptNode->ulUsedSize = ulSize;

    const uint32_t uBlockCount =  pl_sb_size(ptData->sbtBlocks);
    plDeviceMemoryAllocation* ptBlock = &ptData->sbtBlocks[ptNode->ulBlockIndex];

    plDeviceMemoryAllocation tAllocation = {
        .pHostMapped  = NULL,
        .uHandle      = (uint64_t)ptBlock->uHandle,
        .ulOffset     = ptNode->ulOffset,
        .ulSize       = ulSize,
        .ptAllocator  = ptData->ptAllocator,
        .tMemoryFlags = PL_MEMORY_FLAGS_HOST_VISIBLE | PL_MEMORY_FLAGS_HOST_COHERENT
    };

    if(ulAlignment > 0)
        tAllocation.ulOffset = (((tAllocation.ulOffset) + ((ulAlignment)-1)) & ~((ulAlignment)-1));

    if(tAllocation.uHandle == 0)
    {
        plDeviceMemoryAllocation tActualAllocation = gptGfx->allocate_memory(ptData->ptDevice, PL_DEVICE_BUDDY_BLOCK_SIZE, PL_MEMORY_FLAGS_HOST_VISIBLE | PL_MEMORY_FLAGS_HOST_COHERENT, uTypeFilter, "Staging Uncached Buddy Heap");
        ptBlock->uHandle = tActualAllocation.uHandle;
        ptBlock->pHostMapped = tActualAllocation.pHostMapped;
        tAllocation.uHandle = (uint64_t)ptBlock->uHandle;
    }
    tAllocation.pHostMapped = &ptBlock->pHostMapped[tAllocation.ulOffset];

    return tAllocation;
}

static plDeviceMemoryAllocation
pl_allocate_staging_cached(
    struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter,
    uint64_t ulSize, uint64_t ulAlignment, const char* pcName)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;

    plDeviceMemoryAllocation tBlock = gptGfx->allocate_memory(ptData->ptDevice, ulSize, PL_MEMORY_FLAGS_HOST_VISIBLE | PL_MEMORY_FLAGS_HOST_COHERENT | PL_MEMORY_FLAGS_HOST_CACHED, uTypeFilter, pcName);

    plDeviceMemoryAllocation tAllocation = {
        .pHostMapped  = tBlock.pHostMapped,
        .uHandle      = tBlock.uHandle,
        .ulOffset     = 0,
        .ulSize       = ulSize,
        .ptAllocator  = ptData->ptAllocator,
        .tMemoryFlags = PL_MEMORY_FLAGS_HOST_VISIBLE | PL_MEMORY_FLAGS_HOST_COHERENT | PL_MEMORY_FLAGS_HOST_CACHED
    };

    uint32_t uBlockIndex = pl_sb_size(ptData->sbtBlocks);
    if(pl_sb_size(ptData->sbtFreeBlockIndices) > 0)
        uBlockIndex = pl_sb_pop(ptData->sbtFreeBlockIndices);
    else
        pl_sb_add(ptData->sbtBlocks);

    plDeviceAllocationRange tRange = {
        .ulOffset     = 0,
        .ulTotalSize  = ulSize,
        .ulUsedSize   = ulSize,
        .ulBlockIndex = uBlockIndex
    };
    pl_sprintf(tRange.acName, "%s", pcName);

    pl_sb_push(ptData->sbtNodes, tRange);
    ptData->sbtBlocks[uBlockIndex] = tBlock;
    return tAllocation;
}

static void
pl_free_staging_cached(struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;

    uint32_t uBlockIndex = 0;
    uint32_t uNodeIndex = 0;
    for(uint32_t i = 0; i < pl_sb_size(ptData->sbtNodes); i++)
    {
        plDeviceAllocationRange* ptNode = &ptData->sbtNodes[i];
        plDeviceMemoryAllocation* ptBlock = &ptData->sbtBlocks[ptNode->ulBlockIndex];

        if(ptBlock->uHandle == ptAllocation->uHandle)
        {
            uNodeIndex = i;
            uBlockIndex = (uint32_t)ptNode->ulBlockIndex;
            ptBlock->ulSize = 0;
            break;
        }
    }
    pl_sb_del_swap(ptData->sbtNodes, uNodeIndex);
    pl_sb_push(ptData->sbtFreeBlockIndices, uBlockIndex);

    gptGfx->free_memory(ptData->ptDevice, &ptData->sbtBlocks[uBlockIndex]);

    ptAllocation->pHostMapped  = NULL;
    ptAllocation->uHandle      = 0;
    ptAllocation->ulOffset     = 0;
    ptAllocation->ulSize       = 0;
}

static plDeviceMemoryAllocatorI*
pl_get_local_dedicated_allocator(plDevice* ptDevice)
{

    static plDeviceAllocatorData* ptAllocatorData = NULL;
    static plDeviceMemoryAllocatorI* ptAllocator = NULL;

    if(ptAllocator == NULL)
    {
        ptAllocator = gptDataRegistry->get_data("LocalDedicatedAllocator");

        if(ptAllocator)
            ptAllocatorData = gptDataRegistry->get_data("LocalDedicatedAllocatorData");
        
        else
        {
            static plDeviceMemoryAllocatorI gtAllocator = {0};
            static plDeviceAllocatorData gtAllocatorData = {0};
            gptDataRegistry->set_data("LocalDedicatedAllocator", &gtAllocator);
            gptDataRegistry->set_data("LocalDedicatedAllocatorData", &gtAllocatorData);
            gtAllocatorData.ptDevice = ptDevice;
            gtAllocatorData.ptAllocator = &gtAllocator;
            gtAllocator.ptInst = (struct plDeviceMemoryAllocatorO*)&gtAllocatorData;

            ptAllocatorData = &gtAllocatorData;
            ptAllocator = &gtAllocator;
        }
    }
    ptAllocator->allocate = pl_allocate_dedicated;
    ptAllocator->free = pl_free_dedicated;
    return ptAllocator;
}

static plDeviceMemoryAllocatorI*
pl_get_local_buddy_allocator(plDevice* ptDevice)
{

    static plDeviceAllocatorData* ptAllocatorData = NULL;
    static plDeviceMemoryAllocatorI* ptAllocator = NULL;

    if(ptAllocator == NULL)
    {
        ptAllocator = gptDataRegistry->get_data("LocalBuddyAllocator");

        if(ptAllocator)
            ptAllocatorData = gptDataRegistry->get_data("LocalBuddyAllocatorData");
        else
        {

            static plDeviceMemoryAllocatorI gtAllocator = {0};
            static plDeviceAllocatorData gtAllocatorData = {0};
            gptDataRegistry->set_data("LocalBuddyAllocator", &gtAllocator);
            gptDataRegistry->set_data("LocalBuddyAllocatorData", &gtAllocatorData);
            gtAllocatorData.ptDevice = ptDevice;
            gtAllocatorData.ptAllocator = &gtAllocator;
            gtAllocator.ptInst = (struct plDeviceMemoryAllocatorO*)&gtAllocatorData;

            ptAllocatorData = &gtAllocatorData;
            ptAllocator = &gtAllocator;

            if(ptAllocatorData->auFreeList[0] == 0)
            {
                for(uint32_t i = 0; i < PL_DEVICE_LOCAL_LEVELS; i++)
                    ptAllocatorData->auFreeList[i] = UINT32_MAX;
            }
        }
    }
    ptAllocator->allocate = pl_allocate_buddy;
    ptAllocator->free = pl_free_buddy;
    return ptAllocator;
}

static plDeviceMemoryAllocatorI*
pl_get_staging_uncached_allocator(plDevice* ptDevice)
{
    static plDeviceAllocatorData* ptAllocatorData = NULL;
    static plDeviceMemoryAllocatorI* ptAllocator = NULL;

    if(ptAllocator == NULL)
    {
        ptAllocator = gptDataRegistry->get_data("StagingUncachedAllocator");

        if(ptAllocator)
            ptAllocatorData = gptDataRegistry->get_data("StagingUncachedAllocatorData");
        
        else
        {

            static plDeviceMemoryAllocatorI gtAllocator = {0};
            static plDeviceAllocatorData gtAllocatorData = {0};
            gptDataRegistry->set_data("StagingUncachedAllocator", &gtAllocator);
            gptDataRegistry->set_data("StagingUncachedAllocatorData", &gtAllocatorData);
            gtAllocatorData.ptDevice = ptDevice;
            gtAllocatorData.ptAllocator = &gtAllocator;
            gtAllocator.ptInst = (struct plDeviceMemoryAllocatorO*)&gtAllocatorData;

            ptAllocatorData = &gtAllocatorData;
            ptAllocator = &gtAllocator;
        }
    }
    ptAllocator->allocate = pl_allocate_staging_uncached;
    ptAllocator->free = pl_free_staging_uncached;
    return ptAllocator;
}

static plDeviceMemoryAllocatorI*
pl_get_staging_uncached_allocator_buddy(plDevice* ptDevice)
{
    static plDeviceAllocatorData* ptAllocatorData = NULL;
    static plDeviceMemoryAllocatorI* ptAllocator = NULL;

    if(ptAllocator == NULL)
    {
        ptAllocator = gptDataRegistry->get_data("StagingUncachedAllocatorBuddy");

        if(ptAllocator)
            ptAllocatorData = gptDataRegistry->get_data("StagingUncachedAllocatorBuddyData");
        
        else
        {

            static plDeviceMemoryAllocatorI gtAllocator = {0};
            static plDeviceAllocatorData gtAllocatorData = {0};
            gptDataRegistry->set_data("StagingUncachedAllocatorBuddy", &gtAllocator);
            gptDataRegistry->set_data("StagingUncachedAllocatorBuddyData", &gtAllocatorData);
            gtAllocatorData.ptDevice = ptDevice;
            gtAllocatorData.ptAllocator = &gtAllocator;
            gtAllocator.ptInst = (struct plDeviceMemoryAllocatorO*)&gtAllocatorData;

            ptAllocatorData = &gtAllocatorData;
            ptAllocator = &gtAllocator;

            if(ptAllocatorData->auFreeList[0] == 0)
            {
                for(uint32_t i = 0; i < PL_DEVICE_LOCAL_LEVELS; i++)
                    ptAllocatorData->auFreeList[i] = UINT32_MAX;
            }
        }
    }
    ptAllocator->allocate = pl_allocate_staging_uncached_buddy;
    ptAllocator->free = pl_free_buddy;
    return ptAllocator;
}

static plDeviceMemoryAllocatorI*
pl_get_staging_cached_allocator(plDevice* ptDevice)
{
    static plDeviceAllocatorData* ptAllocatorData = NULL;
    static plDeviceMemoryAllocatorI* ptAllocator = NULL;

    if(ptAllocator == NULL)
    {
        ptAllocator = gptDataRegistry->get_data("StagingCachedAllocator");

        if(ptAllocator)
            ptAllocatorData = gptDataRegistry->get_data("StagingCachedAllocatorData");
        
        else
        {

            static plDeviceMemoryAllocatorI gtAllocator = {0};
            static plDeviceAllocatorData gtAllocatorData = {0};
            gptDataRegistry->set_data("StagingCachedAllocator", &gtAllocator);
            gptDataRegistry->set_data("StagingCachedAllocatorData", &gtAllocatorData);
            gtAllocatorData.ptDevice = ptDevice;
            gtAllocatorData.ptAllocator = &gtAllocator;
            gtAllocator.ptInst = (struct plDeviceMemoryAllocatorO*)&gtAllocatorData;

            ptAllocatorData = &gtAllocatorData;
            ptAllocator = &gtAllocator;
        }
    }
    ptAllocator->allocate = pl_allocate_staging_cached;
    ptAllocator->free = pl_free_staging_cached;
    return ptAllocator;
}

static void
pl_cleanup_allocators(plDevice* ptDevice)
{
    plDeviceMemoryAllocatorI* ptAllocator = pl_get_local_buddy_allocator(ptDevice);
    plDeviceAllocatorData* ptAllocatorData = (plDeviceAllocatorData*)ptAllocator->ptInst;

    for(uint32_t i = 0; i < pl_sb_size(ptAllocatorData->sbtBlocks); i++)
    {
        if(ptAllocatorData->sbtBlocks[i].uHandle)
            gptGfx->free_memory(ptDevice, &ptAllocatorData->sbtBlocks[i]);
    }
    pl_sb_free(ptAllocatorData->sbtBlocks);
    pl_sb_free(ptAllocatorData->sbtNodes);
    pl_sb_free(ptAllocatorData->sbtFreeBlockIndices);

    ptAllocator = pl_get_local_dedicated_allocator(ptDevice);
    ptAllocatorData = (plDeviceAllocatorData*)ptAllocator->ptInst;
    for(uint32_t i = 0; i < pl_sb_size(ptAllocatorData->sbtBlocks); i++)
    {
        if(ptAllocatorData->sbtBlocks[i].uHandle)
            gptGfx->free_memory(ptDevice, &ptAllocatorData->sbtBlocks[i]);
    }
    pl_sb_free(ptAllocatorData->sbtBlocks);
    pl_sb_free(ptAllocatorData->sbtNodes);
    pl_sb_free(ptAllocatorData->sbtFreeBlockIndices);

    ptAllocator = pl_get_staging_uncached_allocator(ptDevice);
    ptAllocatorData = (plDeviceAllocatorData*)ptAllocator->ptInst;
    for(uint32_t i = 0; i < pl_sb_size(ptAllocatorData->sbtBlocks); i++)
    {
        if(ptAllocatorData->sbtBlocks[i].uHandle)
            gptGfx->free_memory(ptDevice, &ptAllocatorData->sbtBlocks[i]);
    }
    pl_sb_free(ptAllocatorData->sbtBlocks);
    pl_sb_free(ptAllocatorData->sbtNodes);
    pl_sb_free(ptAllocatorData->sbtFreeBlockIndices);

    ptAllocator = pl_get_staging_cached_allocator(ptDevice);
    ptAllocatorData = (plDeviceAllocatorData*)ptAllocator->ptInst;
    for(uint32_t i = 0; i < pl_sb_size(ptAllocatorData->sbtBlocks); i++)
    {
        if(ptAllocatorData->sbtBlocks[i].uHandle)
            gptGfx->free_memory(ptDevice, &ptAllocatorData->sbtBlocks[i]);
    }
    pl_sb_free(ptAllocatorData->sbtBlocks);
    pl_sb_free(ptAllocatorData->sbtNodes);
    pl_sb_free(ptAllocatorData->sbtFreeBlockIndices);

    ptAllocator = pl_get_staging_uncached_allocator_buddy(ptDevice);
    ptAllocatorData = (plDeviceAllocatorData*)ptAllocator->ptInst;
    for(uint32_t i = 0; i < pl_sb_size(ptAllocatorData->sbtBlocks); i++)
    {
        if(ptAllocatorData->sbtBlocks[i].uHandle)
            gptGfx->free_memory(ptDevice, &ptAllocatorData->sbtBlocks[i]);
    }
    pl_sb_free(ptAllocatorData->sbtBlocks);
    pl_sb_free(ptAllocatorData->sbtNodes);
    pl_sb_free(ptAllocatorData->sbtFreeBlockIndices);
}

static size_t
pl_get_buddy_block_size(void)
{
    return PL_DEVICE_BUDDY_BLOCK_SIZE;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_gpu_allocators_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plGPUAllocatorsI tApi = {
        .get_local_dedicated_allocator        = pl_get_local_dedicated_allocator,
        .get_local_buddy_allocator            = pl_get_local_buddy_allocator,
        .get_staging_uncached_allocator       = pl_get_staging_uncached_allocator,
        .get_staging_cached_allocator         = pl_get_staging_cached_allocator,
        .get_staging_uncached_buddy_allocator = pl_get_staging_uncached_allocator_buddy,
        .get_blocks                           = pl_get_allocator_blocks,
        .get_ranges                           = pl_get_allocator_ranges,
        .cleanup                              = pl_cleanup_allocators,
        .get_buddy_block_size                 = pl_get_buddy_block_size
    };
    pl_set_api(ptApiRegistry, plGPUAllocatorsI, &tApi);

    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);
    gptGfx = pl_get_api_latest(ptApiRegistry, plGraphicsI);

}

PL_EXPORT void
pl_unload_gpu_allocators_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    const plGPUAllocatorsI* ptApi = pl_get_api_latest(ptApiRegistry, plGPUAllocatorsI);
    ptApiRegistry->remove_api(ptApi);
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#ifndef PL_UNITY_BUILD
    #ifdef PL_USE_STB_SPRINTF
        #define STB_SPRINTF_IMPLEMENTATION
        #include "stb_sprintf.h"
        #undef STB_SPRINTF_IMPLEMENTATION
    #endif
#endif