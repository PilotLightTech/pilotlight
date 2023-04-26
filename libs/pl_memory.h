/*
   pl_memory
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
   * general allocation uses malloc, free, & realloc by default
   * override general allocators by defining PL_MEMORY_ALLOC(x), PL_MEMORY_FREE(x), & PL_MEMORY_REALLOC(x, y) OR PL_MEMORY_REALLOC_SIZED(x, y, x)
   * override assert by defining PL_ASSERT(x)
*/

// library version
#define PL_MEMORY_VERSION    "0.2.0"
#define PL_MEMORY_VERSION_NUM 00200

/*
Index of this file:
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] structs
// [SECTION] c/c++ file start
*/

#ifndef PL_MEMORY_H
#define PL_MEMORY_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>  // uint*_t
#include <stddef.h>  // size_t
#include <stdbool.h> // bool

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

typedef struct _plMemoryContext     plMemoryContext;
typedef struct _plStackAllocator    plStackAllocator;
typedef struct _plPoolAllocator     plPoolAllocator;
typedef struct _plPoolAllocatorNode plPoolAllocatorNode;

typedef size_t plStackAllocatorMarker;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

//~~~~~~~~~~~~~~~~~~~~~~~~~general purpose allocation~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void*                  pl_aligned_alloc(size_t szAlignment, size_t szSize);
void                   pl_aligned_free (void* pBuffer);
void*                  pl_realloc      (void* pBuffer, size_t szSize);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~virtual memory system~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// Notes
//   - API subject to change slightly
//   - additional error checks needs to be added
//   - committed memory does not necessarily mean the memory has been mapped to physical
//     memory. This is happens when the memory is actually touched. Even so, on Windows
//     you can not commit more memmory then you have in your page file.
//   - uncommitted memory does not necessarily mean the memory will be immediately
//     evicted. It is up to the OS.

size_t                 pl_get_page_size   (void);                          // returns memory page size
void*                  pl_virtual_alloc   (void* pAddress, size_t szSize); // reserves & commits a block of memory. pAddress is starting address or use NULL to have system choose. szSize must be a multiple of memory page size.
void*                  pl_virtual_reserve (void* pAddress, size_t szSize); // reserves a block of memory. pAddress is starting address or use NULL to have system choose. szSize must be a multiple of memory page size.
void*                  pl_virtual_commit  (void* pAddress, size_t szSize); // commits a block of reserved memory. szSize must be a multiple of memory page size.
void                   pl_virtual_uncommit(void* pAddress, size_t szSize); // uncommits a block of committed memory.
void                   pl_virtual_free    (void* pAddress, size_t szSize); // frees a block of previously reserved/committed memory. Must be the starting address returned from "pl_virtual_reserve()" or "pl_virtual_alloc()"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~stack allocators~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// single stack
void                   pl_stack_allocator_init          (plStackAllocator* ptAllocator, size_t szSize, void* pBuffer);
void*                  pl_stack_allocator_alloc         (plStackAllocator* ptAllocator, size_t szSize);
void*                  pl_stack_allocator_aligned_alloc (plStackAllocator* ptAllocator, size_t szSize, size_t szAlignment);
plStackAllocatorMarker pl_stack_allocator_marker        (plStackAllocator* ptAllocator);
void                   pl_stack_allocator_free_to_marker(plStackAllocator* ptAllocator, plStackAllocatorMarker tMarker);
void                   pl_stack_allocator_reset         (plStackAllocator* ptAllocator);

// double stack allocator (using regular stack allocator)
void*                  pl_stack_allocator_aligned_alloc_bottom (plStackAllocator* ptAllocator, size_t szSize, size_t szAlignment);
plStackAllocatorMarker pl_stack_allocator_bottom_marker        (plStackAllocator* ptAllocator);
plStackAllocatorMarker pl_stack_allocator_top_marker           (plStackAllocator* ptAllocator);
void*                  pl_stack_allocator_alloc_bottom         (plStackAllocator* ptAllocator, size_t szSize);
void*                  pl_stack_allocator_alloc_top            (plStackAllocator* ptAllocator, size_t szSize);
void                   pl_stack_allocator_free_top_to_marker   (plStackAllocator* ptAllocator, plStackAllocatorMarker tMarker);
void                   pl_stack_allocator_free_bottom_to_marker(plStackAllocator* ptAllocator, plStackAllocatorMarker tMarker);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~pool allocator~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void                   pl_pool_allocator_init (plPoolAllocator* ptAllocator, size_t szItemCount, size_t szItemSize, size_t szItemAlignment, size_t szBufferSize, void* pBuffer);
void*                  pl_pool_allocator_alloc(plPoolAllocator* ptAllocator);
void                   pl_pool_allocator_free (plPoolAllocator* ptAllocator, void* pItem);

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

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
// [SECTION] c/c++ file start
//-----------------------------------------------------------------------------

/*
Index of this file:
// [SECTION] defines
// [SECTION] internal structs
// [SECTION] internal api
// [SECTION] public api implementation
*/

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifdef PL_MEMORY_IMPLEMENTATION

#if defined(PL_MEMORY_ALLOC) && defined(PL_MEMORY_FREE) && (defined(PL_MEMORY_REALLOC) || defined(PL_MEMORY_REALLOC_SIZED))
// ok
#elif !defined(PL_MEMORY_ALLOC) && !defined(PL_MEMORY_FREE) && !defined(PL_MEMORY_REALLOC) && !defined(PL_MEMORY_REALLOC_SIZED)
// ok
#else
#error "Must define all or none of PL_MEMORY_ALLOC, PL_MEMORY_FREE, and PL_MEMORY_REALLOC (or PL_MEMORY_REALLOC_SIZED)."
#endif

#ifndef PL_MEMORY_ALLOC
    #include <stdlib.h>
    #define PL_MEMORY_ALLOC(x)      malloc(x)
    #define PL_MEMORY_REALLOC(x, y) realloc(x, y)
    #define PL_MEMORY_FREE(x)       free(x)
#endif

#ifndef PL_MEMORY_REALLOC_SIZED
    #define PL_MEMORY_REALLOC_SIZED(x, y, z) PL_MEMORY_REALLOC(x, z)
#endif

#ifndef PL_ASSERT
#include <assert.h>
#define PL_ASSERT(x) assert((x))
#endif

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>  // VirtualAlloc, VirtualFree
    #include <sysinfoapi.h> // page size
#elif defined(__APPLE__)
    #include <unistd.h>
    #include <sys/mman.h>
#else // linux
    #include <unistd.h>
    #include <sys/mman.h>
#endif

#define PL__ALIGN_UP(num, align) (((num) + ((align)-1)) & ~((align)-1))

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plMemoryContext
{
    uint32_t uActiveAllocations;
} plMemoryContext;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

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

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

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
        void* pActualBuffer = PL_MEMORY_ALLOC(szSize + ulHeaderSize);

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
    PL_MEMORY_FREE(pActualBuffer);
}

void*
pl_realloc(void* pBuffer, size_t szSize)
{
    void* pNewBuffer = NULL;

    if(szSize == 0 && pBuffer)  // free
    { 
        PL_MEMORY_FREE(pBuffer);
        pNewBuffer = NULL;
    }
    else if (szSize == 0)  // free
    { 
        pNewBuffer = NULL;
    }
    else if(pBuffer) // resizing
    {
        pNewBuffer = PL_MEMORY_REALLOC(pBuffer, szSize);
    }
    else
    {
        pNewBuffer = PL_MEMORY_ALLOC(szSize);
    }
    return pNewBuffer;
}

void
pl_stack_allocator_init(plStackAllocator* ptAllocator, size_t szSize, void* pBuffer)
{
    PL_ASSERT(ptAllocator);
    PL_ASSERT(szSize > 0);
    PL_ASSERT(pBuffer);

    ptAllocator->pucBuffer = (unsigned char*)pBuffer;
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
    plStackAllocatorMarker tMarker = ptAllocator->szBottomOffset;
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
    plStackAllocatorMarker tMarker = ptAllocator->szTopOffset;
    return tMarker;
}

void
pl_stack_allocator_free_to_marker(plStackAllocator* ptAllocator, plStackAllocatorMarker tMarker)
{
    ptAllocator->szBottomOffset = tMarker;

    #ifdef _DEBUG
    memset(&ptAllocator->pucBuffer[ptAllocator->szBottomOffset], 0, ptAllocator->szTopOffset - ptAllocator->szBottomOffset);
    #endif
}

void
pl_stack_allocator_free_top_to_marker(plStackAllocator* ptAllocator, plStackAllocatorMarker tMarker)
{
    ptAllocator->szTopOffset = tMarker;

    #ifdef _DEBUG
    memset(&ptAllocator->pucBuffer[ptAllocator->szBottomOffset], 0, ptAllocator->szTopOffset - ptAllocator->szBottomOffset);
    #endif
}

void
pl_stack_allocator_free_bottom_to_marker(plStackAllocator* ptAllocator, plStackAllocatorMarker tMarker)
{
    ptAllocator->szBottomOffset = tMarker;

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
    ptAllocator->pucBuffer = (unsigned char*)pBuffer;
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
    ptAllocator->pFreeList = (plPoolAllocatorNode*)pItem;
    ptAllocator->pFreeList->ptNextNode = pOldFreeNode;
}

size_t
pl_get_page_size(void)
{
    #ifdef _WIN32
        SYSTEM_INFO tInfo = {0};
        GetSystemInfo(&tInfo);
        return (size_t)tInfo.dwPageSize;
    #elif defined(__APPLE__)
        return (size_t)getpagesize();
    #else // linux
        return (size_t)getpagesize();
    #endif
}

void*
pl_virtual_alloc(void* pAddress, size_t szSize)
{
    #ifdef _WIN32
        return VirtualAlloc(pAddress, szSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    #elif defined(__APPLE__)
        void* pResult = mmap(pAddress, szSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        return pResult;
    #else // linux
        void* pResult = mmap(pAddress, szSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        return pResult;
    #endif
}

void*
pl_virtual_reserve(void* pAddress, size_t szSize)
{
    #ifdef _WIN32
        return VirtualAlloc(pAddress, szSize, MEM_RESERVE, PAGE_READWRITE);
    #elif defined(__APPLE__)
        void* pResult = mmap(pAddress, szSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        return pResult;
    #else // linux
        void* pResult = mmap(pAddress, szSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        return pResult;
    #endif
}

void*
pl_virtual_commit(void* pAddress, size_t szSize)
{
    #ifdef _WIN32
        return VirtualAlloc(pAddress, szSize, MEM_COMMIT, PAGE_READWRITE);
    #elif defined(__APPLE__)
        mprotect(pAddress, szSize, PROT_READ | PROT_WRITE);
        return pAddress;
    #else // linux
        mprotect(pAddress, szSize, PROT_READ | PROT_WRITE);
        return pAddress;
    #endif
}

void
pl_virtual_free(void* pAddress, size_t szSize)
{
    #ifdef _WIN32
        PL_ASSERT(VirtualFree(pAddress, szSize, MEM_RELEASE));
    #elif defined(__APPLE__)
        PL_ASSERT(munmap(pAddress, szSize) == 0);
    #else // linux
        PL_ASSERT(munmap(pAddress, szSize) == 0); //-V586
    #endif
}

void
pl_virtual_uncommit(void* pAddress, size_t szSize)
{
    #ifdef _WIN32
        PL_ASSERT(VirtualFree(pAddress, szSize, MEM_DECOMMIT));
    #elif defined(__APPLE__)
        mprotect(pAddress, szSize, PROT_NONE);
    #else // linux
        mprotect(pAddress, szSize, PROT_NONE);
    #endif
}

#endif
