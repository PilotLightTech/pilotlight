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
#define PL_DECLARE_STRUCT(name) typedef struct name ##_t name
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

extern plMemoryContext* gTPMemoryContext;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// context
void                   pl_initialize_memory_context(plMemoryContext* pCtx);
void                   pl_cleanup_memory_context   (void);
void                   pl_set_memory_context       (plMemoryContext* pCtx);
plMemoryContext*       pl_get_memory_context       (void);

// general purpose allocation
void*                  pl_alloc        (size_t szSize);
void                   pl_free         (void* pBuffer);
void*                  pl_aligned_alloc(size_t szAlignment, size_t szSize);
void                   pl_aligned_free (void* pBuffer);
void*                  pl_realloc      (void* pBuffer, size_t szSize);

// stack allocator
void                   pl_stack_allocator_init          (plStackAllocator* pAllocator, size_t szSize, void* pBuffer);
void*                  pl_stack_allocator_alloc         (plStackAllocator* pAllocator, size_t szSize);
void*                  pl_stack_allocator_aligned_alloc (plStackAllocator* pAllocator, size_t szSize, size_t szAlignment);
plStackAllocatorMarker pl_stack_allocator_marker        (plStackAllocator* pAllocator);
void                   pl_stack_allocator_free_to_marker(plStackAllocator* pAllocator, plStackAllocatorMarker tMarker);
void                   pl_stack_allocator_reset         (plStackAllocator* pAllocator);

// double stack allocator (using regular stack allocator)
void*                  pl_stack_allocator_aligned_alloc_bottom(plStackAllocator* pAllocator, size_t szSize, size_t szAlignment);
plStackAllocatorMarker pl_stack_allocator_bottom_marker       (plStackAllocator* pAllocator);
plStackAllocatorMarker pl_stack_allocator_top_marker          (plStackAllocator* pAllocator);
void*                  pl_stack_allocator_alloc_bottom        (plStackAllocator* pAllocator, size_t szSize);
void*                  pl_stack_allocator_alloc_top           (plStackAllocator* pAllocator, size_t szSize);

// pool allocator
void                   pl_pool_allocator_init (plPoolAllocator* pAllocator, size_t szItemCount, size_t szItemSize, size_t szItemAlignment, size_t szBufferSize, void* pBuffer);
void*                  pl_pool_allocator_alloc(plPoolAllocator* pAllocator);
void                   pl_pool_allocator_free (plPoolAllocator* pAllocator, void* pItem);

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct plMemoryContext_t
{
    uint32_t uActiveAllocations;
} plMemoryContext;

typedef struct plStackAllocatorMarker_t
{
    bool   bTop;
    size_t szOffset;
} plStackAllocatorMarker;

typedef struct plStackAllocator_t
{
    unsigned char* pBuffer;
    size_t         szSize;
    size_t         szBottomOffset;
    size_t         szTopOffset;
} plStackAllocator;

typedef struct plPoolAllocatorNode_t
{
    plPoolAllocatorNode* pNextNode;
} plPoolAllocatorNode;

typedef struct plPoolAllocator_t
{
    unsigned char*       pBuffer;
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
#define PL_ASSERT(x) assert(x)
#endif

#define PL__ALIGN_UP(num, align) (((num) + ((align)-1)) & ~((align)-1))

plMemoryContext* gTPMemoryContext = NULL;

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
pl__align_forward_uintptr(uintptr_t ptr, size_t ulAlign) 
{
	PL_ASSERT((ulAlign & (ulAlign-1)) == 0 && "alignment must be power of 2");
	uintptr_t a = (uintptr_t)ulAlign;
	uintptr_t p = ptr;
	uintptr_t modulo = p & (a - 1);
	if (modulo != 0){ p += a - modulo;}
	return p;
}

static inline size_t 
pl__align_forward_size(size_t ptr, size_t ulAlign) 
{
	PL_ASSERT((ulAlign & (ulAlign-1)) == 0 && "alignment must be power of 2");
	size_t a = ulAlign;
	size_t p = ptr;
	size_t modulo = p & (a - 1);
	if (modulo != 0){ p += a - modulo;}
	return p;
}

void
pl_initialize_memory_context(plMemoryContext* pCtx)
{
    memset(pCtx, 0, sizeof(plMemoryContext));
    gTPMemoryContext = pCtx;
}

void
pl_cleanup_memory_context(void)
{
    PL_ASSERT(gTPMemoryContext->uActiveAllocations == 0 && "memory leak");
    gTPMemoryContext->uActiveAllocations = 0u;
}

void
pl_set_memory_context(plMemoryContext* pCtx)
{
    gTPMemoryContext = pCtx;
}

plMemoryContext*
pl_get_memory_context(void)
{
    return gTPMemoryContext;
}

void*
pl_alloc(size_t szSize)
{
    gTPMemoryContext->uActiveAllocations++;
    return PL_ALLOC(szSize);
}

void
pl_free(void* pBuffer)
{
    gTPMemoryContext->uActiveAllocations--;
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
        gTPMemoryContext->uActiveAllocations--;
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
        gTPMemoryContext->uActiveAllocations++;
        pNewBuffer = PL_ALLOC(szSize);
    }
    return pNewBuffer;
}

void
pl_stack_allocator_init(plStackAllocator* pAllocator, size_t szSize, void* pBuffer)
{
    PL_ASSERT(pAllocator);
    PL_ASSERT(szSize > 0);
    PL_ASSERT(pBuffer);

    pAllocator->pBuffer = pBuffer;
    pAllocator->szSize = szSize;
    pAllocator->szBottomOffset = 0;
    pAllocator->szTopOffset = szSize;
}

void*
pl_stack_allocator_alloc(plStackAllocator* pAllocator, size_t szSize)
{
    size_t szOffset = pAllocator->szBottomOffset + szSize;

    PL_ASSERT(szOffset < pAllocator->szTopOffset && "stack allocator full");

    // update offset
    void* pBuffer = (unsigned char*)pAllocator->pBuffer + pAllocator->szBottomOffset;
    pAllocator->szBottomOffset = szOffset;

    return pBuffer;
}

void*
pl_stack_allocator_aligned_alloc(plStackAllocator* pAllocator, size_t szSize, size_t szAlignment)
{
    void* pBuffer = NULL;

    szAlignment = pl__get_next_power_of_2(szAlignment);
    uintptr_t pCurrentPointer = (uintptr_t)pAllocator->pBuffer + (uintptr_t)pAllocator->szBottomOffset;
    uintptr_t pOffset = pl__align_forward_uintptr(pCurrentPointer, szAlignment);
    pOffset -= (uintptr_t)pAllocator->pBuffer;

    PL_ASSERT(pOffset + szSize <= pAllocator->szTopOffset && "linear allocator full");

    // check if allocator has enough space left
    if(pOffset + szSize <= pAllocator->szSize)
    {
        pBuffer = &pAllocator->pBuffer[pOffset];
        pAllocator->szBottomOffset = pOffset + szSize;

        // zero new memory
        memset(pBuffer, 0, szSize);
    }
    return pBuffer;
}

void*
pl_stack_allocator_aligned_alloc_bottom(plStackAllocator* pAllocator, size_t szSize, size_t szAlignment)
{
    return pl_stack_allocator_aligned_alloc(pAllocator, szSize, szAlignment);
}

void*
pl_stack_allocator_aligned_alloc_top(plStackAllocator* pAllocator, size_t szSize, size_t szAlignment)
{
    void* pBuffer = NULL;

    szAlignment = pl__get_next_power_of_2(szAlignment);
    uintptr_t pCurrentPointer = (uintptr_t)pAllocator->pBuffer + (uintptr_t)pAllocator->szBottomOffset;
    uintptr_t pOffset = pl__align_forward_uintptr(pCurrentPointer, szAlignment);
    pOffset -= (uintptr_t)pAllocator->pBuffer;

    PL_ASSERT(pOffset + szSize <= pAllocator->szTopOffset && "linear allocator full");

    // check if allocator has enough space left
    if(pOffset + szSize <= pAllocator->szSize)
    {
        pBuffer = &pAllocator->pBuffer[pOffset];
        pAllocator->szBottomOffset = pOffset + szSize;

        // zero new memory
        memset(pBuffer, 0, szSize);
    }
    return pBuffer;
}

void*
pl_stack_allocator_alloc_bottom(plStackAllocator* pAllocator, size_t szSize)
{
    return pl_stack_allocator_alloc(pAllocator, szSize);
}

void*
pl_stack_allocator_alloc_top(plStackAllocator* pAllocator, size_t szSize)
{
    size_t szOffset = pAllocator->szTopOffset - szSize;

    PL_ASSERT(szOffset > pAllocator->szBottomOffset && szOffset < pAllocator->szTopOffset && "stack allocator full");

    // update offset
    void* pBuffer = (unsigned char*)pAllocator->pBuffer + szOffset;
    pAllocator->szTopOffset = szOffset;

    return pBuffer;
}

plStackAllocatorMarker
pl_stack_allocator_marker(plStackAllocator* pAllocator)
{
    plStackAllocatorMarker tMarker;
    tMarker.szOffset = pAllocator->szBottomOffset;
    tMarker.bTop = false;
    return tMarker;
}

plStackAllocatorMarker
pl_stack_allocator_bottom_marker(plStackAllocator* pAllocator)
{
    return pl_stack_allocator_marker(pAllocator);
}

plStackAllocatorMarker
pl_stack_allocator_top_marker(plStackAllocator* pAllocator)
{
    plStackAllocatorMarker tMarker;
    tMarker.szOffset = pAllocator->szTopOffset;
    tMarker.bTop = true;
    return tMarker;
}

void
pl_stack_allocator_free_to_marker(plStackAllocator* pAllocator, plStackAllocatorMarker tMarker)
{
    if(tMarker.bTop)
        pAllocator->szTopOffset = tMarker.szOffset;
    else
        pAllocator->szBottomOffset = tMarker.szOffset;

    #ifdef _DEBUG
    memset(&pAllocator->pBuffer[pAllocator->szBottomOffset], 0, pAllocator->szTopOffset - pAllocator->szBottomOffset);
    #endif
}

void
pl_stack_allocator_reset(plStackAllocator* pAllocator)
{
    pAllocator->szBottomOffset = 0;
    pAllocator->szTopOffset = pAllocator->szSize;

    #ifdef _DEBUG
    memset(pAllocator->pBuffer, 0, pAllocator->szSize);
    #endif
}

void
pl_pool_allocator_init(plPoolAllocator* pAllocator, size_t szItemCount, size_t szItemSize, size_t szItemAlignment, size_t szBufferSize, void* pBuffer)
{
    PL_ASSERT(pAllocator);
    PL_ASSERT(szItemCount > 0);
    PL_ASSERT(szItemSize > 0);
    PL_ASSERT(szBufferSize > 0);
    PL_ASSERT(pBuffer);

    pAllocator->szFreeItems = szItemCount;
    pAllocator->szRequestedItemSize = szItemSize;
    pAllocator->szGivenSize = szBufferSize;
    pAllocator->szUsableSize = szBufferSize;
    pAllocator->pBuffer = pBuffer;
    pAllocator->szItemSize = pl__align_forward_size(szItemSize, szItemAlignment);

    uintptr_t pInitialStart = (uintptr_t)pBuffer;
    uintptr_t pStart = pl__align_forward_uintptr(pInitialStart, (uintptr_t)szItemAlignment);
    pAllocator->szUsableSize -= (size_t)(pStart - pInitialStart);

    PL_ASSERT(pAllocator->szItemSize >= sizeof(plPoolAllocatorNode) && "pool allocator item size too small");
    PL_ASSERT(pAllocator->szUsableSize >= pAllocator->szItemSize * szItemCount && "pool allocator buffer size too small");

    unsigned char* pUsableBuffer = (unsigned char*)pStart;
    for(size_t i = 0; i < szItemCount - 1; i++)
    {
        plPoolAllocatorNode* pNode0 = (plPoolAllocatorNode*)&pUsableBuffer[i * pAllocator->szItemSize];
        plPoolAllocatorNode* pNode1 = (plPoolAllocatorNode*)&pUsableBuffer[(i + 1) * pAllocator->szItemSize];
        pNode0->pNextNode = pNode1;
    }
    pAllocator->pFreeList = (plPoolAllocatorNode*)pUsableBuffer;
}

void*
pl_pool_allocator_alloc(plPoolAllocator* pAllocator)
{
    PL_ASSERT(pAllocator->szFreeItems > 0 && "pool allocator is full");
    pAllocator->szFreeItems--;
    plPoolAllocatorNode* pFirstNode = pAllocator->pFreeList;
    plPoolAllocatorNode* pNextNode = pFirstNode->pNextNode;
    pAllocator->pFreeList = pNextNode;
    memset(pFirstNode, 0, pAllocator->szItemSize);
    return pFirstNode;
}

void
pl_pool_allocator_free(plPoolAllocator* pAllocator, void* pItem)
{
    pAllocator->szFreeItems++;
    plPoolAllocatorNode* pOldFreeNode = pAllocator->pFreeList;
    pAllocator->pFreeList = pItem;
    pAllocator->pFreeList->pNextNode = pOldFreeNode;
}

#endif