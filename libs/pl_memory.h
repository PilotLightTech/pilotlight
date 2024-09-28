/*
   pl_memory.h
     * no dependencies
     * simple memory allocators
     
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
   * override general allocators by defining PL_MEMORY_ALLOC(x), PL_MEMORY_FREE(x)
   * override assert by defining PL_ASSERT(x)
*/

// library version (format XYYZZ)
#define PL_MEMORY_VERSION    "1.0.0"
#define PL_MEMORY_VERSION_NUM 10000

/*
Index of this file:
// [SECTION] header mess
// [SECTION] defines
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] structs
// [SECTION] c/c++ file start
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_MEMORY_H
#define PL_MEMORY_H

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_MEMORY_TEMP_STACK_SIZE
    #define PL_MEMORY_TEMP_STACK_SIZE 1024
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stddef.h>  // size_t

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plTempAllocator  plTempAllocator;
typedef struct _plStackAllocator plStackAllocator;
typedef struct _plPoolAllocator  plPoolAllocator;

typedef size_t plStackAllocatorMarker;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

//~~~~~~~~~~~~~~~~~~~~~~~~~general purpose allocation~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void* pl_aligned_alloc(size_t szAlignment, size_t);
void  pl_aligned_free (void*);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~temporary allocator~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void* pl_temp_allocator_alloc  (plTempAllocator*, size_t);
void  pl_temp_allocator_reset  (plTempAllocator*);
void  pl_temp_allocator_free   (plTempAllocator*);
char* pl_temp_allocator_sprintf(plTempAllocator*, const char* cPFormat, ...);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~stack allocators~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// common
void pl_stack_allocator_init(plStackAllocator*, size_t, void*);

// single stack
void*                  pl_stack_allocator_alloc         (plStackAllocator*, size_t);
void*                  pl_stack_allocator_aligned_alloc (plStackAllocator*, size_t, size_t szAlignment);
plStackAllocatorMarker pl_stack_allocator_marker        (plStackAllocator*);
void                   pl_stack_allocator_free_to_marker(plStackAllocator*, plStackAllocatorMarker);
void                   pl_stack_allocator_reset         (plStackAllocator*);

// double sided stack
void*                  pl_stack_allocator_aligned_alloc_bottom (plStackAllocator*, size_t, size_t szAlignment);
plStackAllocatorMarker pl_stack_allocator_top_marker           (plStackAllocator*);
plStackAllocatorMarker pl_stack_allocator_bottom_marker        (plStackAllocator*);
void*                  pl_stack_allocator_alloc_bottom         (plStackAllocator*, size_t);
void*                  pl_stack_allocator_alloc_top            (plStackAllocator*, size_t);
void                   pl_stack_allocator_free_top_to_marker   (plStackAllocator*, plStackAllocatorMarker);
void                   pl_stack_allocator_free_bottom_to_marker(plStackAllocator*, plStackAllocatorMarker);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~pool allocator~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// Notes
//   - setting pBuffer to NULL, will set pszBufferSize to required buffer size
//     so you can allocate a properly sized buffer for the requested szItemCount (then call function again)
//   - to use a stack allocated buffer, first call the function with szItemCount = 0 & pszBufferSize
//     set to size of the buffer; the function will return the number of items that can be supported;
//     call function again with this number

size_t pl_pool_allocator_init (plPoolAllocator*, size_t szItemCount, size_t szItemSize, size_t szItemAlignment, size_t* pszBufferSize, void* pBuffer);
void*  pl_pool_allocator_alloc(plPoolAllocator*);
void   pl_pool_allocator_free (plPoolAllocator*, void* pItem);

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

// the details of the following structures don't matter to you, but they must
// be visible so you can handle the memory allocations for them

typedef struct _plTempAllocator
{
    size_t szSize;
    size_t szOffset;
    char*  pcBuffer;
    char   acStackBuffer[PL_MEMORY_TEMP_STACK_SIZE];
    char** ppcMemoryBlocks;
    size_t szMemoryBlockCount;
    size_t szMemoryBlockCapacity;
    size_t szCurrentBlockSizes;
    size_t szNextBlockSizes;
} plTempAllocator;

typedef struct _plStackAllocator
{
    unsigned char* pucBuffer;
    size_t         szSize;
    size_t         szBottomOffset;
    size_t         szTopOffset;
} plStackAllocator;

typedef struct _plPoolAllocatorNode plPoolAllocatorNode;
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
// [SECTION] internal api
// [SECTION] public api implementation
*/

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifdef PL_MEMORY_IMPLEMENTATION

#if defined(PL_MEMORY_ALLOC) && defined(PL_MEMORY_FREE)
// ok
#elif !defined(PL_MEMORY_ALLOC) && !defined(PL_MEMORY_FREE)
// ok
#else
#error "Must define both or none of PL_MEMORY_ALLOC and PL_MEMORY_FREE"
#endif

#ifndef PL_MEMORY_ALLOC
    #include <stdlib.h>
    #define PL_MEMORY_ALLOC(x) malloc(x)
    #define PL_MEMORY_FREE(x)  free(x)
#endif

#ifndef PL_ASSERT
    #include <assert.h>
    #define PL_ASSERT(x) assert((x))
#endif

#define PL__ALIGN_UP(num, align) (((num) + ((align)-1)) & ~((align)-1))

#ifndef pl_vnsprintf
    #include <stdio.h>
    #define pl_vnsprintf vsnprintf
#endif

#include <stdarg.h> // varargs

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

    if(szAlignment == 0)
        szAlignment = pl__get_next_power_of_2(szSize);

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
pl_temp_allocator_alloc(plTempAllocator* ptAllocator, size_t szSize)
{

    if(ptAllocator->szSize == 0) // first usage ever
    {
        ptAllocator->ppcMemoryBlocks = NULL;
        ptAllocator->szMemoryBlockCount = 0;
        ptAllocator->szMemoryBlockCapacity = 0;
        ptAllocator->szSize = PL_MEMORY_TEMP_STACK_SIZE;
        ptAllocator->pcBuffer = ptAllocator->acStackBuffer;
        ptAllocator->szOffset = 0;
        ptAllocator->szCurrentBlockSizes = PL_MEMORY_TEMP_STACK_SIZE * 2;
        ptAllocator->szNextBlockSizes = PL_MEMORY_TEMP_STACK_SIZE * 2;
        memset(ptAllocator->acStackBuffer, 0, PL_MEMORY_TEMP_STACK_SIZE); 
    }

    void* pRequestedMemory = NULL;

    // not enough room is available
    if(szSize > ptAllocator->szSize - ptAllocator->szOffset)
    {
        if(ptAllocator->szMemoryBlockCapacity == 0) // first overflow
        {
            // allocate block array
            ptAllocator->szMemoryBlockCapacity = 1;
            ptAllocator->ppcMemoryBlocks = (char**)PL_MEMORY_ALLOC(sizeof(char*) * ptAllocator->szMemoryBlockCapacity);
            memset(ptAllocator->ppcMemoryBlocks, 0, (sizeof(char*) * ptAllocator->szMemoryBlockCapacity));

            size_t szNewBlockSize = ptAllocator->szCurrentBlockSizes;
            if(szSize > szNewBlockSize)
            {
                ptAllocator->szNextBlockSizes = szSize;
                szNewBlockSize = szSize;
            }

            // allocate first block
            ptAllocator->ppcMemoryBlocks[0] = (char*)PL_MEMORY_ALLOC(szNewBlockSize);
            ptAllocator->szSize = szNewBlockSize;
            ptAllocator->szOffset = 0;
            ptAllocator->pcBuffer = ptAllocator->ppcMemoryBlocks[0];
        }
        else if(ptAllocator->szMemoryBlockCount == ptAllocator->szMemoryBlockCapacity) // grow memory block storage
        {

            size_t szNewBlockSize = ptAllocator->szCurrentBlockSizes;
            if(szSize > szNewBlockSize)
            {
                ptAllocator->szNextBlockSizes = szSize;
                szNewBlockSize = szSize;
            }

            char** ppcOldBlocks = ptAllocator->ppcMemoryBlocks;
            ptAllocator->ppcMemoryBlocks = (char**)PL_MEMORY_ALLOC(sizeof(char*) * (ptAllocator->szMemoryBlockCapacity + 1));
            memset(ptAllocator->ppcMemoryBlocks, 0, (sizeof(char*) * (ptAllocator->szMemoryBlockCapacity + 1)));
            memcpy(ptAllocator->ppcMemoryBlocks, ppcOldBlocks, sizeof(char*) * ptAllocator->szMemoryBlockCapacity);
            ptAllocator->szMemoryBlockCapacity++;
            ptAllocator->ppcMemoryBlocks[ptAllocator->szMemoryBlockCount] = (char*)PL_MEMORY_ALLOC(szNewBlockSize);
            ptAllocator->szSize = szNewBlockSize;
            ptAllocator->pcBuffer = ptAllocator->ppcMemoryBlocks[ptAllocator->szMemoryBlockCount];
            ptAllocator->szOffset = 0;
        }
        else if(szSize <= ptAllocator->szCurrentBlockSizes) // block available & small enough
        {
            ptAllocator->szSize = ptAllocator->szCurrentBlockSizes;
            ptAllocator->szOffset = 0;
            ptAllocator->pcBuffer = ptAllocator->ppcMemoryBlocks[ptAllocator->szMemoryBlockCount];
        }
        else // block available but too small
        {
            size_t szNewBlockSize = ptAllocator->szCurrentBlockSizes;
            ptAllocator->szNextBlockSizes = szSize;
            szNewBlockSize = szSize;

            char** ppcOldBlocks = ptAllocator->ppcMemoryBlocks;
            ptAllocator->ppcMemoryBlocks = (char**)PL_MEMORY_ALLOC(sizeof(char*) * (ptAllocator->szMemoryBlockCapacity + 1));
            memset(ptAllocator->ppcMemoryBlocks, 0, (sizeof(char*) * (ptAllocator->szMemoryBlockCapacity + 1)));
            memcpy(ptAllocator->ppcMemoryBlocks, ppcOldBlocks, sizeof(char*) * ptAllocator->szMemoryBlockCapacity);
            ptAllocator->szMemoryBlockCapacity++;
            ptAllocator->ppcMemoryBlocks[ptAllocator->szMemoryBlockCount] = (char*)PL_MEMORY_ALLOC(szNewBlockSize);
            ptAllocator->szSize = szNewBlockSize;
            ptAllocator->pcBuffer = ptAllocator->ppcMemoryBlocks[ptAllocator->szMemoryBlockCount];
            ptAllocator->szOffset = 0;
        }
        
        ptAllocator->szMemoryBlockCount++;
    }

    pRequestedMemory = &ptAllocator->pcBuffer[ptAllocator->szOffset];
    ptAllocator->szOffset += szSize;
    return pRequestedMemory;
}

void
pl_temp_allocator_reset(plTempAllocator* ptAllocator)
{
    ptAllocator->szSize = PL_MEMORY_TEMP_STACK_SIZE;
    ptAllocator->szOffset = 0;
    ptAllocator->szMemoryBlockCount = 0;
    ptAllocator->pcBuffer = ptAllocator->acStackBuffer;

    if(ptAllocator->szCurrentBlockSizes != ptAllocator->szNextBlockSizes)
    {
        for(size_t i = 0; i < ptAllocator->szMemoryBlockCapacity; i++)
        {
            PL_MEMORY_FREE(ptAllocator->ppcMemoryBlocks[i]);
            ptAllocator->ppcMemoryBlocks[i] = (char*)PL_MEMORY_ALLOC(ptAllocator->szNextBlockSizes);
            memset(ptAllocator->ppcMemoryBlocks[i], 0, ptAllocator->szNextBlockSizes);
        } 
        ptAllocator->szCurrentBlockSizes = ptAllocator->szNextBlockSizes;
    }
}

void
pl_temp_allocator_free(plTempAllocator* ptAllocator)
{
    for(size_t i = 0; i < ptAllocator->szMemoryBlockCapacity; i++)
    {
        PL_MEMORY_FREE(ptAllocator->ppcMemoryBlocks[i]);
    }
    if(ptAllocator->ppcMemoryBlocks)
        PL_MEMORY_FREE(ptAllocator->ppcMemoryBlocks);
    ptAllocator->ppcMemoryBlocks = NULL;
    ptAllocator->szMemoryBlockCapacity = 0;
    ptAllocator->szMemoryBlockCount = 0;
    ptAllocator->pcBuffer = NULL;
    ptAllocator->szSize = 0;
    ptAllocator->szOffset = 0;
}

inline static char*
pl__temp_allocator_sprintf_va(plTempAllocator* ptAllocator, const char* cPFormat, va_list args)
{
    void* pRequestedMemory = NULL;

    // sprint
    va_list args2;
    va_copy(args2, args);
    int32_t n = pl_vnsprintf(NULL, 0, cPFormat, args2);
    va_end(args2);

    pRequestedMemory = pl_temp_allocator_alloc(ptAllocator, n + 1);
    memset(pRequestedMemory, 0, n + 1);
    pl_vnsprintf(pRequestedMemory, n + 1, cPFormat, args);

    return pRequestedMemory;
}

char*
pl_temp_allocator_sprintf(plTempAllocator* ptAllocator, const char* cPFormat, ...)
{
    void* pRequestedMemory = NULL;

    va_list argptr;
    va_start(argptr, cPFormat);
    pRequestedMemory = pl__temp_allocator_sprintf_va(ptAllocator, cPFormat, argptr);
    va_end(argptr);

    return pRequestedMemory;   
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

    if(szOffset >= ptAllocator->szTopOffset)
        return NULL;

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

    if(pOffset + szSize > ptAllocator->szTopOffset)
        return NULL;

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

    if(pOffset + szSize > ptAllocator->szTopOffset)
        return NULL;

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

    if(szOffset < ptAllocator->szBottomOffset || szOffset > ptAllocator->szTopOffset)
        return NULL;

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

size_t
pl_pool_allocator_init(plPoolAllocator* ptAllocator, size_t szItemCount, size_t szItemSize, size_t szItemAlignment, size_t* pszBufferSize, void* pBuffer)
{
    PL_ASSERT(ptAllocator);
    PL_ASSERT(szItemSize > 0);
    PL_ASSERT(pszBufferSize);

    // gotta have room for node in unused blocks
    if(szItemSize < sizeof(plPoolAllocatorNode))
    {
        szItemSize = sizeof(plPoolAllocatorNode);
    }

    // let us calculate alignment
    if(szItemAlignment == 0)
        szItemAlignment = pl__get_next_power_of_2(szItemSize);

    // let us calculate number of items
    if(szItemCount == 0 && *pszBufferSize > 0)
    {
        size_t szAlignedItemSize = pl__align_forward_size(szItemSize, szItemAlignment);
        szItemCount = (*pszBufferSize - szItemAlignment) / (szAlignedItemSize);
        return szItemCount;
    }

    if(pBuffer == NULL)
    {
        size_t szAlignedItemSize = pl__align_forward_size(szItemSize, szItemAlignment);
        *pszBufferSize = szAlignedItemSize * szItemCount + szItemAlignment;
        return szItemCount;
    }

    ptAllocator->szFreeItems = szItemCount;
    ptAllocator->szRequestedItemSize = szItemSize;
    ptAllocator->szGivenSize = *pszBufferSize;
    ptAllocator->szUsableSize = *pszBufferSize;
    ptAllocator->pucBuffer = (unsigned char*)pBuffer;
    ptAllocator->szItemSize = pl__align_forward_size(szItemSize, szItemAlignment);

    uintptr_t pInitialStart = (uintptr_t)pBuffer;
    uintptr_t pStart = pl__align_forward_uintptr(pInitialStart, (uintptr_t)szItemAlignment);
    ptAllocator->szUsableSize -= (size_t)(pStart - pInitialStart);

    PL_ASSERT(ptAllocator->szUsableSize >= ptAllocator->szItemSize * szItemCount && "pool allocator buffer size too small");

    unsigned char* pUsableBuffer = (unsigned char*)pStart;
    for(size_t i = 0; i < szItemCount - 1; i++)
    {
        plPoolAllocatorNode* pNode0 = (plPoolAllocatorNode*)&pUsableBuffer[i * ptAllocator->szItemSize];
        plPoolAllocatorNode* pNode1 = (plPoolAllocatorNode*)&pUsableBuffer[(i + 1) * ptAllocator->szItemSize];
        pNode0->ptNextNode = pNode1;
    }
    ptAllocator->pFreeList = (plPoolAllocatorNode*)pUsableBuffer;
    return szItemCount;
}

void*
pl_pool_allocator_alloc(plPoolAllocator* ptAllocator)
{
    if(ptAllocator->szFreeItems == 0)
        return NULL;
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

#endif // PL_MEMORY_IMPLEMENTATION