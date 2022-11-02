/*
   pl_memory, v0.1 (WIP)
   * no dependencies
   * simple
   Do this:
        #define PL_MEMORY_IMPLEMENTATION
   before you include this file in *one* C or C++ file to create the implementation.
   // i.e. it should look like this:
   #include ...
   #include ...
   #include ...
   #define PL_MEMORY_IMPLEMENTATION
   #include "pl_memory.h"
   Notes:
   * allocations return NULL on failure
*/

/*
Index of this file:
// [SECTION] defines
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] global context
// [SECTION] public api
// [SECTION] structs
// [SECTION] implementation
*/

#ifndef PL_MEMORY_H
#define PL_MEMORY_H

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_DECLARE_STRUCT
#define PL_DECLARE_STRUCT(name) typedef struct _ ## name  name
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>  // uint*_t
#include <stddef.h>  // size_t
#include <stdbool.h> // bool

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

PL_DECLARE_STRUCT(plMemoryContext);
PL_DECLARE_STRUCT(plStackAllocator);
PL_DECLARE_STRUCT(plStackAllocatorMarker);
PL_DECLARE_STRUCT(plPoolAllocator);
PL_DECLARE_STRUCT(plPoolAllocatorNode);

//-----------------------------------------------------------------------------
// [SECTION] global context
//-----------------------------------------------------------------------------

extern plMemoryContext* gptMemoryContext;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// context
void                   pl_initialize_memory_context(plMemoryContext* ptCtx);
void                   pl_cleanup_memory_context   (void);
void                   pl_set_memory_context       (plMemoryContext* ptCtx);
plMemoryContext*       pl_get_memory_context       (void);

// general purpose allocation
void*                  pl_alloc        (size_t szSize);
void                   pl_free         (void* pBuffer);
void*                  pl_aligned_alloc(size_t szAlignment, size_t szSize);
void                   pl_aligned_free (void* pBuffer);
void*                  pl_realloc      (void* pBuffer, size_t szSize);

// stack allocator
void                   pl_stack_allocator_init          (plStackAllocator* ptAllocator, size_t szSize, void* pBuffer);
void*                  pl_stack_allocator_alloc         (plStackAllocator* ptAllocator, size_t szSize);
void*                  pl_stack_allocator_aligned_alloc (plStackAllocator* ptAllocator, size_t szSize, size_t szAlignment);
plStackAllocatorMarker pl_stack_allocator_marker        (plStackAllocator* ptAllocator);
void                   pl_stack_allocator_free_to_marker(plStackAllocator* ptAllocator, plStackAllocatorMarker tMarker);
void                   pl_stack_allocator_reset         (plStackAllocator* ptAllocator);

// double stack allocator (using regular stack allocator)
void*                  pl_stack_allocator_aligned_alloc_bottom(plStackAllocator* ptAllocator, size_t szSize, size_t szAlignment);
plStackAllocatorMarker pl_stack_allocator_bottom_marker       (plStackAllocator* ptAllocator);
plStackAllocatorMarker pl_stack_allocator_top_marker          (plStackAllocator* ptAllocator);
void*                  pl_stack_allocator_alloc_bottom        (plStackAllocator* ptAllocator, size_t szSize);
void*                  pl_stack_allocator_alloc_top           (plStackAllocator* ptAllocator, size_t szSize);

// pool allocator
void                   pl_pool_allocator_init (plPoolAllocator* ptAllocator, size_t szItemCount, size_t szItemSize, size_t szItemAlignment, size_t szBufferSize, void* pBuffer);
void*                  pl_pool_allocator_alloc(plPoolAllocator* ptAllocator);
void                   pl_pool_allocator_free (plPoolAllocator* ptAllocator, void* pItem);

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plMemoryContext
{
    uint32_t uActiveAllocations;
} plMemoryContext;

typedef struct _plStackAllocatorMarker
{
    bool   bTop;
    size_t szOffset;
} plStackAllocatorMarker;

typedef struct _plStackAllocator
{
    unsigned char* pucBuffer;
    size_t         szSize;
    size_t         szBottomOffset;
    size_t         szTopOffset;
} plStackAllocator;

typedef struct _plPoolAllocatorNode
{
    plPoolAllocatorNode* ptNextNode;
} plPoolAllocatorNode;

typedef struct _plPoolAllocator
{
    unsigned char*       pucBuffer;
    size_t               szGivenSize;
    size_t               szUsableSize;
    size_t               szRequestedItemSize;
    size_t               szItemSize;
    size_t               szFreeItems;
    plPoolAllocatorNode* pFreeList;
} plPoolAllocator;

#endif // PL_MEMORY_H

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

#ifdef PL_MEMORY_IMPLEMENTATION

#ifndef PL_ALLOC
#include <stdlib.h>
#define PL_ALLOC(x) malloc(x)
#endif

#ifndef PL_REALLOC
#define PL_REALLOC(x, y) realloc(x, y)
#endif

#ifndef PL_FREE
#define PL_FREE(x) free(x)
#endif

#ifndef PL_ASSERT
#include <assert.h>
#define PL_ASSERT(x) assert((x))
#endif

#define PL__ALIGN_UP(num, align) (((num) + ((align)-1)) & ~((align)-1))

plMemoryContext* gptMemoryContext = NULL;

static inline size_t
pl__get_next_power_of_2(size_t n)
{ 
    size_t ulResult = 1;
    if (n && !(n & (n - 1))) 
        ulResult = n;
    while (ulResult < n)
        ulResult <<= 1;
    return ulResult; 
}

static inline uintptr_t
pl__align_forward_uintptr(uintptr_t ptr, size_t szAlign) 
{
	PL_ASSERT((szAlign & (szAlign-1)) == 0 && "alignment must be power of 2");
	uintptr_t a = (uintptr_t)szAlign;
	uintptr_t p = ptr;
	uintptr_t pModulo = p & (a - 1);
	if (pModulo != 0){ p += a - pModulo;}
	return p;
}

static inline size_t 
pl__align_forward_size(size_t szPtr, size_t szAlign) 
{
	PL_ASSERT((szAlign & (szAlign-1)) == 0 && "alignment must be power of 2");
	size_t a = szAlign;
	size_t p = szPtr;
	size_t szModulo = p & (a - 1);
	if (szModulo != 0){ p += a - szModulo;}
	return p;
}

void
pl_initialize_memory_context(plMemoryContext* ptCtx)
{
    memset(ptCtx, 0, sizeof(plMemoryContext));
    gptMemoryContext = ptCtx;
}

void
pl_cleanup_memory_context(void)
{
    PL_ASSERT(gptMemoryContext->uActiveAllocations == 0 && "memory leak");
    gptMemoryContext->uActiveAllocations = 0u;
}

void
pl_set_memory_context(plMemoryContext* ptCtx)
{
    gptMemoryContext = ptCtx;
}

plMemoryContext*
pl_get_memory_context(void)
{
    return gptMemoryContext;
}

void*
pl_alloc(size_t szSize)
{
    gptMemoryContext->uActiveAllocations++;
    return PL_ALLOC(szSize);
}

void
pl_free(void* pBuffer)
{
    gptMemoryContext->uActiveAllocations--;
    PL_FREE(pBuffer);
}

void*
pl_aligned_alloc(size_t szAlignment, size_t szSize)
{
    void* pBuffer = NULL;

    // ensure power of 2
    PL_ASSERT((szAlignment & (szAlignment -1)) == 0 && "alignment must be a power of 2");

    if(szAlignment && szSize)
    {
        // allocate extra bytes for alignment
        uint64_t ulHeaderSize = sizeof(uint64_t) + (szAlignment - 1);
        void* pActualBuffer = pl_alloc(szSize + ulHeaderSize);

        if(pActualBuffer)
        {
            // add offset size to pointer & align
            pBuffer = (void*)PL__ALIGN_UP(((uintptr_t)pActualBuffer + sizeof(uint64_t)), szAlignment);

            // calculate offset & store it behind aligned pointer
            *((uint64_t*)pBuffer - 1) = (uint64_t)((uintptr_t)pBuffer - (uintptr_t)pActualBuffer);
        }
    }
    return pBuffer;
}

void
pl_aligned_free(void* pBuffer)
{
    PL_ASSERT(pBuffer);

    // get stored offset
    uint64_t ulOffset = *((uint64_t*)pBuffer - 1);

    // get original buffer to free
    void* pActualBuffer = ((uint8_t*)pBuffer - ulOffset);
    pl_free(pActualBuffer);
}

void*
pl_realloc(void* pBuffer, size_t szSize)
{
    void* pNewBuffer = NULL;

    if(szSize == 0 && pBuffer)  // free
    { 
        gptMemoryContext->uActiveAllocations--;
        PL_FREE(pBuffer);
        pNewBuffer = NULL;
    }
    else if (szSize == 0)  // free
    { 
        pNewBuffer = NULL;
    }
    else if(pBuffer) // resizing
    {
        pNewBuffer = PL_REALLOC(pBuffer, szSize);
    }
    else
    {
        gptMemoryContext->uActiveAllocations++;
        pNewBuffer = PL_ALLOC(szSize);
    }
    return pNewBuffer;
}

void
pl_stack_allocator_init(plStackAllocator* ptAllocator, size_t szSize, void* pBuffer)
{
    PL_ASSERT(ptAllocator);
    PL_ASSERT(szSize > 0);
    PL_ASSERT(pBuffer);

    ptAllocator->pucBuffer = pBuffer;
    ptAllocator->szSize = szSize;
    ptAllocator->szBottomOffset = 0;
    ptAllocator->szTopOffset = szSize;
}

void*
pl_stack_allocator_alloc(plStackAllocator* ptAllocator, size_t szSize)
{
    size_t szOffset = ptAllocator->szBottomOffset + szSize;

    PL_ASSERT(szOffset < ptAllocator->szTopOffset && "stack allocator full");

    // update offset
    void* pBuffer = ptAllocator->pucBuffer + ptAllocator->szBottomOffset;
    ptAllocator->szBottomOffset = szOffset;

    return pBuffer;
}

void*
pl_stack_allocator_aligned_alloc(plStackAllocator* ptAllocator, size_t szSize, size_t szAlignment)
{
    void* pBuffer = NULL;

    szAlignment = pl__get_next_power_of_2(szAlignment);
    uintptr_t pCurrentPointer = (uintptr_t)ptAllocator->pucBuffer + (uintptr_t)ptAllocator->szBottomOffset;
    uintptr_t pOffset = pl__align_forward_uintptr(pCurrentPointer, szAlignment);
    pOffset -= (uintptr_t)ptAllocator->pucBuffer;

    PL_ASSERT(pOffset + szSize <= ptAllocator->szTopOffset && "linear allocator full");

    // check if allocator has enough space left
    if(pOffset + szSize <= ptAllocator->szSize)
    {
        pBuffer = &ptAllocator->pucBuffer[pOffset];
        ptAllocator->szBottomOffset = pOffset + szSize;

        // zero new memory
        memset(pBuffer, 0, szSize);
    }
    return pBuffer;
}

void*
pl_stack_allocator_aligned_alloc_bottom(plStackAllocator* ptAllocator, size_t szSize, size_t szAlignment)
{
    return pl_stack_allocator_aligned_alloc(ptAllocator, szSize, szAlignment);
}

void*
pl_stack_allocator_aligned_alloc_top(plStackAllocator* ptAllocator, size_t szSize, size_t szAlignment)
{
    void* pBuffer = NULL;

    szAlignment = pl__get_next_power_of_2(szAlignment);
    uintptr_t pCurrentPointer = (uintptr_t)ptAllocator->pucBuffer + (uintptr_t)ptAllocator->szBottomOffset;
    uintptr_t pOffset = pl__align_forward_uintptr(pCurrentPointer, szAlignment);
    pOffset -= (uintptr_t)ptAllocator->pucBuffer;

    PL_ASSERT(pOffset + szSize <= ptAllocator->szTopOffset && "linear allocator full");

    // check if allocator has enough space left
    if(pOffset + szSize <= ptAllocator->szSize)
    {
        pBuffer = &ptAllocator->pucBuffer[pOffset];
        ptAllocator->szBottomOffset = pOffset + szSize;

        // zero new memory
        memset(pBuffer, 0, szSize);
    }
    return pBuffer;
}

void*
pl_stack_allocator_alloc_bottom(plStackAllocator* ptAllocator, size_t szSize)
{
    return pl_stack_allocator_alloc(ptAllocator, szSize);
}

void*
pl_stack_allocator_alloc_top(plStackAllocator* ptAllocator, size_t szSize)
{
    size_t szOffset = ptAllocator->szTopOffset - szSize;

    PL_ASSERT(szOffset > ptAllocator->szBottomOffset && szOffset < ptAllocator->szTopOffset && "stack allocator full");

    // update offset
    void* pBuffer = ptAllocator->pucBuffer + szOffset;
    ptAllocator->szTopOffset = szOffset;

    return pBuffer;
}

plStackAllocatorMarker
pl_stack_allocator_marker(plStackAllocator* ptAllocator)
{
    plStackAllocatorMarker tMarker;
    tMarker.szOffset = ptAllocator->szBottomOffset;
    tMarker.bTop = false;
    return tMarker;
}

plStackAllocatorMarker
pl_stack_allocator_bottom_marker(plStackAllocator* ptAllocator)
{
    return pl_stack_allocator_marker(ptAllocator);
}

plStackAllocatorMarker
pl_stack_allocator_top_marker(plStackAllocator* ptAllocator)
{
    plStackAllocatorMarker tMarker;
    tMarker.szOffset = ptAllocator->szTopOffset;
    tMarker.bTop = true;
    return tMarker;
}

void
pl_stack_allocator_free_to_marker(plStackAllocator* ptAllocator, plStackAllocatorMarker tMarker)
{
    if(tMarker.bTop)
        ptAllocator->szTopOffset = tMarker.szOffset;
    else
        ptAllocator->szBottomOffset = tMarker.szOffset;

    #ifdef _DEBUG
    memset(&ptAllocator->pucBuffer[ptAllocator->szBottomOffset], 0, ptAllocator->szTopOffset - ptAllocator->szBottomOffset);
    #endif
}

void
pl_stack_allocator_reset(plStackAllocator* ptAllocator)
{
    ptAllocator->szBottomOffset = 0;
    ptAllocator->szTopOffset = ptAllocator->szSize;

    #ifdef _DEBUG
    memset(ptAllocator->pucBuffer, 0, ptAllocator->szSize);
    #endif
}

void
pl_pool_allocator_init(plPoolAllocator* ptAllocator, size_t szItemCount, size_t szItemSize, size_t szItemAlignment, size_t szBufferSize, void* pBuffer)
{
    PL_ASSERT(ptAllocator);
    PL_ASSERT(szItemCount > 0);
    PL_ASSERT(szItemSize > 0);
    PL_ASSERT(szBufferSize > 0);
    PL_ASSERT(pBuffer);

    ptAllocator->szFreeItems = szItemCount;
    ptAllocator->szRequestedItemSize = szItemSize;
    ptAllocator->szGivenSize = szBufferSize;
    ptAllocator->szUsableSize = szBufferSize;
    ptAllocator->pucBuffer = pBuffer;
    ptAllocator->szItemSize = pl__align_forward_size(szItemSize, szItemAlignment);

    uintptr_t pInitialStart = (uintptr_t)pBuffer;
    uintptr_t pStart = pl__align_forward_uintptr(pInitialStart, (uintptr_t)szItemAlignment);
    ptAllocator->szUsableSize -= (size_t)(pStart - pInitialStart);

    PL_ASSERT(ptAllocator->szItemSize >= sizeof(plPoolAllocatorNode) && "pool allocator item size too small");
    PL_ASSERT(ptAllocator->szUsableSize >= ptAllocator->szItemSize * szItemCount && "pool allocator buffer size too small");

    unsigned char* pUsableBuffer = (unsigned char*)pStart;
    for(size_t i = 0; i < szItemCount - 1; i++)
    {
        plPoolAllocatorNode* pNode0 = (plPoolAllocatorNode*)&pUsableBuffer[i * ptAllocator->szItemSize];
        plPoolAllocatorNode* pNode1 = (plPoolAllocatorNode*)&pUsableBuffer[(i + 1) * ptAllocator->szItemSize];
        pNode0->ptNextNode = pNode1;
    }
    ptAllocator->pFreeList = (plPoolAllocatorNode*)pUsableBuffer;
}

void*
pl_pool_allocator_alloc(plPoolAllocator* ptAllocator)
{
    PL_ASSERT(ptAllocator->szFreeItems > 0 && "pool allocator is full");
    ptAllocator->szFreeItems--;
    plPoolAllocatorNode* pFirstNode = ptAllocator->pFreeList;
    plPoolAllocatorNode* ptNextNode = pFirstNode->ptNextNode;
    ptAllocator->pFreeList = ptNextNode;
    memset(pFirstNode, 0, ptAllocator->szItemSize);
    return pFirstNode;
}

void
pl_pool_allocator_free(plPoolAllocator* ptAllocator, void* pItem)
{
    ptAllocator->szFreeItems++;
    plPoolAllocatorNode* pOldFreeNode = ptAllocator->pFreeList;
    ptAllocator->pFreeList = pItem;
    ptAllocator->pFreeList->ptNextNode = pOldFreeNode;
}

#endif
