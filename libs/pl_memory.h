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
#define PL_MEMORY_VERSION    "1.1.3"
#define PL_MEMORY_VERSION_NUM 10103

/*
Index of this file:
// [SECTION] header mess
// [SECTION] defines
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] structs
// [SECTION] c/c++ file start
// [SECTION] revision history
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
#include <stdint.h>  // uint*_t

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plTempAllocator    plTempAllocator;
typedef struct _plStackAllocator   plStackAllocator;
typedef struct _plPoolAllocator    plPoolAllocator;
typedef struct _plGeneralAllocator plGeneralAllocator;

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

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~general allocators~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void  pl_general_allocator_init         (plGeneralAllocator*, size_t, void*);
void* pl_general_allocator_realloc      (plGeneralAllocator*, void*, size_t);
void* pl_general_allocator_alloc        (plGeneralAllocator*, size_t);
void* pl_general_allocator_aligned_alloc(plGeneralAllocator*, size_t, size_t szAlignment);
void  pl_general_allocator_free         (plGeneralAllocator*, void*);
void  pl_general_allocator_aligned_free (plGeneralAllocator*, void*);

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

// the details of the following structures don't matter to you, but they must
// be visible so you can handle the memory allocations for them

typedef struct _plTempAllocatorBlock
{
    char*  pcBuffer;
    size_t szSize;
} plTempAllocatorBlock;

typedef struct _plTempAllocator
{
    size_t                szOffset; // offset into pcBuffer
    char                  acStackBuffer[PL_MEMORY_TEMP_STACK_SIZE];
    plTempAllocatorBlock* atMemoryBlocks;
    size_t                szCurrentBlockIndex;
    size_t                szMemoryBlockCapacity;
    size_t                szTotalBytes; // heap only
    size_t                szAvailableBytes;
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

typedef struct _plGeneralAllocatorNode plGeneralAllocatorNode;
typedef struct _plGeneralAllocatorNode
{
    plGeneralAllocatorNode* ptPrev;
    plGeneralAllocatorNode* ptNext;
    size_t                  szSize;
    uint8_t*                puBlock;
} plGeneralAllocatorNode;

typedef struct _plGeneralAllocator
{
    uint8_t*               puBuffer;
    size_t                 szBufferSize;
    size_t                 szUsed;
    size_t                 szMaxHit;
    plGeneralAllocatorNode tFreeList;
} plGeneralAllocator;

#endif // PL_MEMORY_H

//-----------------------------------------------------------------------------
// [SECTION] C/C++ file start
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

#define PL_MEMORY_ALLOC_HEADER_SZ (sizeof(plGeneralAllocatorNode) - sizeof(void*))
#define PL_MEMORY_SMALLEST_ALLOC 128

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

static inline size_t
pl__align_up_sz(size_t x, size_t a)
{
    return (x + (a - 1)) & ~(a - 1);
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

    if(ptAllocator->szTotalBytes == 0) // first usage ever
    {
        ptAllocator->atMemoryBlocks = NULL;
        ptAllocator->szMemoryBlockCapacity = 0;
        ptAllocator->szTotalBytes = PL_MEMORY_TEMP_STACK_SIZE;
        ptAllocator->szAvailableBytes = PL_MEMORY_TEMP_STACK_SIZE;
        ptAllocator->szCurrentBlockIndex = 0;
        ptAllocator->szOffset = 0;
        memset(ptAllocator->acStackBuffer, 0, PL_MEMORY_TEMP_STACK_SIZE); 
    }

    const size_t ALIGN = 8;

// Align offset + size BEFORE checking capacity
    size_t szAlignedOffset = pl__align_up_sz(ptAllocator->szOffset, ALIGN);
    size_t szAlignedSize   = pl__align_up_sz(szSize, ALIGN);

    // Total bytes consumed from the current block includes padding to alignment
    size_t szPadding = szAlignedOffset - ptAllocator->szOffset;
    size_t szNeeded  = szPadding + szAlignedSize;

    void* pRequestedMemory = NULL;

    // not enough room is available
    if(szNeeded > ptAllocator->szAvailableBytes)
    {

        size_t szNewBlockSize = PL_MEMORY_TEMP_STACK_SIZE * 2;
        size_t szNewBlockIndex = 0;

        if(ptAllocator->atMemoryBlocks) // grow
        {
            // Move to next block
            szNewBlockIndex = ++ptAllocator->szCurrentBlockIndex;

            // Ensure metadata capacity
            if(ptAllocator->szCurrentBlockIndex >= ptAllocator->szMemoryBlockCapacity)
            {
                size_t szOldCap = ptAllocator->szMemoryBlockCapacity;
                size_t szNewCap = (szOldCap == 0) ? 4 : (szOldCap * 2);

                plTempAllocatorBlock* atOldBlocks = ptAllocator->atMemoryBlocks;
                ptAllocator->atMemoryBlocks = (plTempAllocatorBlock*)PL_MEMORY_ALLOC(sizeof(plTempAllocatorBlock) * szNewCap);
                memset(ptAllocator->atMemoryBlocks, 0, sizeof(plTempAllocatorBlock) * szNewCap);

                memcpy(ptAllocator->atMemoryBlocks, atOldBlocks, sizeof(plTempAllocatorBlock) * szOldCap);
                ptAllocator->szMemoryBlockCapacity = szNewCap;
                PL_MEMORY_FREE(atOldBlocks);
            }

            // Grow block size based on previous total (your original intent),
            // but use at least 2x stack as a sane floor.
            szNewBlockSize = ptAllocator->szTotalBytes * 2;
            if(szNewBlockSize < PL_MEMORY_TEMP_STACK_SIZE * 2)
                szNewBlockSize = PL_MEMORY_TEMP_STACK_SIZE * 2;
        }
        else // first overflow
        {
            ptAllocator->szTotalBytes = 0;
            ptAllocator->szCurrentBlockIndex = 0;
            ptAllocator->szMemoryBlockCapacity = 4;
            ptAllocator->atMemoryBlocks = (plTempAllocatorBlock*)PL_MEMORY_ALLOC(sizeof(plTempAllocatorBlock) * ptAllocator->szMemoryBlockCapacity);
            memset(ptAllocator->atMemoryBlocks, 0, (sizeof(plTempAllocatorBlock) * ptAllocator->szMemoryBlockCapacity));

            szNewBlockIndex = 0;
        }


        if(szNeeded > szNewBlockSize)
            szNewBlockSize = pl__get_next_power_of_2(szNeeded);

        // Allocate the new block
        ptAllocator->atMemoryBlocks[szNewBlockIndex].pcBuffer = (char*)PL_MEMORY_ALLOC(szNewBlockSize);
        ptAllocator->atMemoryBlocks[szNewBlockIndex].szSize   = szNewBlockSize;
        memset(ptAllocator->atMemoryBlocks[szNewBlockIndex].pcBuffer, 0, szNewBlockSize);

        ptAllocator->szTotalBytes += szNewBlockSize;
        ptAllocator->szOffset = 0;
        ptAllocator->szAvailableBytes = szNewBlockSize;

        // Recompute alignment relative to the fresh block start
        szAlignedOffset = pl__align_up_sz(ptAllocator->szOffset, ALIGN);
        szAlignedSize   = pl__align_up_sz(szSize, ALIGN);
        szPadding       = szAlignedOffset - ptAllocator->szOffset;
        szNeeded        = szPadding + szAlignedSize;
    }

    // Apply padding (align the offset) BEFORE taking the pointer
    ptAllocator->szOffset = szAlignedOffset;

    if(ptAllocator->atMemoryBlocks)
        pRequestedMemory = (void*)&ptAllocator->atMemoryBlocks[ptAllocator->szCurrentBlockIndex].pcBuffer[ptAllocator->szOffset];
    else
        pRequestedMemory = (void*)&ptAllocator->acStackBuffer[ptAllocator->szOffset];

    ptAllocator->szOffset += szAlignedSize;
    ptAllocator->szAvailableBytes -= szNeeded;
    return pRequestedMemory;
}

void
pl_temp_allocator_reset(plTempAllocator* ptAllocator)
{
    ptAllocator->szOffset = 0;

    // Stack-only case
    if(!ptAllocator->atMemoryBlocks)
    {
        ptAllocator->szAvailableBytes = ptAllocator->szTotalBytes; // stack size
        return;
    }

    // If multiple blocks were used, consolidate into one block sized to total bytes.
    // (This also ensures szAvailableBytes matches the one active block.)
    if(ptAllocator->szCurrentBlockIndex > 0)
    {
        // Free all existing buffers
        for(size_t i = 0; i < ptAllocator->szMemoryBlockCapacity; i++)
        {
            if(ptAllocator->atMemoryBlocks[i].pcBuffer)
            {
                PL_MEMORY_FREE(ptAllocator->atMemoryBlocks[i].pcBuffer);
                ptAllocator->atMemoryBlocks[i].pcBuffer = NULL;
                ptAllocator->atMemoryBlocks[i].szSize = 0;
            }
        }

        ptAllocator->atMemoryBlocks[0].pcBuffer = (char*)PL_MEMORY_ALLOC(ptAllocator->szTotalBytes);
        ptAllocator->atMemoryBlocks[0].szSize   = ptAllocator->szTotalBytes;
        memset(ptAllocator->atMemoryBlocks[0].pcBuffer, 0, ptAllocator->szTotalBytes);
        ptAllocator->szCurrentBlockIndex = 0;
    }

    // Available bytes must reflect the CURRENT active block size
    ptAllocator->szAvailableBytes = ptAllocator->atMemoryBlocks[ptAllocator->szCurrentBlockIndex].szSize;
}

void
pl_temp_allocator_free(plTempAllocator* ptAllocator)
{
    for(size_t i = 0; i < ptAllocator->szMemoryBlockCapacity; i++)
    {
        if(ptAllocator->atMemoryBlocks[i].pcBuffer)
        {
            PL_MEMORY_FREE(ptAllocator->atMemoryBlocks[i].pcBuffer);
            ptAllocator->atMemoryBlocks[i].pcBuffer = NULL;
        }
    }
    if(ptAllocator->atMemoryBlocks)
        PL_MEMORY_FREE(ptAllocator->atMemoryBlocks);
    ptAllocator->atMemoryBlocks = NULL;
    ptAllocator->szMemoryBlockCapacity = 0;
    ptAllocator->szTotalBytes = 0;
    ptAllocator->szCurrentBlockIndex = 0;
    ptAllocator->szAvailableBytes = 0;
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
    pl_vnsprintf((char*)pRequestedMemory, n + 1, cPFormat, args);

    return (char*)pRequestedMemory;
}

char*
pl_temp_allocator_sprintf(plTempAllocator* ptAllocator, const char* cPFormat, ...)
{
    void* pRequestedMemory = NULL;

    va_list argptr;
    va_start(argptr, cPFormat);
    pRequestedMemory = pl__temp_allocator_sprintf_va(ptAllocator, cPFormat, argptr);
    va_end(argptr);

    return (char*)pRequestedMemory;   
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

void
pl_general_allocator_init(plGeneralAllocator* ptAllocator, size_t szSize, void* pData)
{
    memset(ptAllocator, 0, sizeof(plGeneralAllocator));
    ptAllocator->puBuffer = (uint8_t*)pData;
    ptAllocator->szBufferSize = szSize;
    ptAllocator->szUsed = 0u;
    ptAllocator->szMaxHit = 0u;

    // align the start addr of our block to the next pointer aligned addr
    plGeneralAllocatorNode* ptNewBlock = (plGeneralAllocatorNode *)PL__ALIGN_UP((uintptr_t)pData, sizeof(void*));
    ptNewBlock->ptNext = NULL;

    // calculate actual size - mgmt overhead
    ptNewBlock->szSize = (uintptr_t)pData + szSize - (uintptr_t)ptNewBlock - PL_MEMORY_ALLOC_HEADER_SZ;

    ptAllocator->tFreeList.ptPrev = NULL;
    ptAllocator->tFreeList.ptNext = NULL;
    ptAllocator->tFreeList.szSize= 0;

    // add node
    ptNewBlock->ptNext = ptAllocator->tFreeList.ptNext;
    ptNewBlock->ptPrev = &ptAllocator->tFreeList;
    ptAllocator->tFreeList.ptNext = ptNewBlock;
}

void*
pl_general_allocator_realloc(plGeneralAllocator* ptAllocator, void* pBuffer, size_t szSize)
{
    // free memory
    if(szSize == 0 && pBuffer)
    {
        pl_general_allocator_free(ptAllocator, pBuffer);
        return NULL;
    }

    // get block information
    plGeneralAllocatorNode* ptBlock = (plGeneralAllocatorNode*)((uintptr_t)pBuffer - PL_MEMORY_ALLOC_HEADER_SZ);

    // check if block already has enough room
    if(ptBlock->szSize > szSize)
        return pBuffer;
    
    // get to block to copy into
    void* pNewBuffer = pl_general_allocator_alloc(ptAllocator, szSize);
    memcpy(pNewBuffer, pBuffer, ptBlock->szSize);

    // free old block
    pl_general_allocator_free(ptAllocator, pBuffer);
    return pNewBuffer;
}

void*
pl_general_allocator_alloc(plGeneralAllocator* ptAllocator, size_t szSize)
{
    void* pBuffer = NULL;

    szSize = PL__ALIGN_UP(szSize, sizeof(void*));
    plGeneralAllocatorNode* ptBlock = NULL;

    // find best block
    size_t szSmallestDiff = ~(size_t)0;
    plGeneralAllocatorNode* ptCurrentBlock = &ptAllocator->tFreeList;
    while(ptCurrentBlock)
    {

        if(ptCurrentBlock->szSize >= szSize && (ptCurrentBlock->szSize - szSize < szSmallestDiff))
        {
            ptBlock = ptCurrentBlock;
            szSmallestDiff = ptCurrentBlock->szSize - szSize;
        }
        ptCurrentBlock = ptCurrentBlock->ptNext;
    }

    if (ptBlock != NULL) 
    {
        // split block if big enough
        if( (ptBlock->szSize - szSize) >= PL_MEMORY_ALLOC_HEADER_SZ + PL_MEMORY_SMALLEST_ALLOC)
        {
            plGeneralAllocatorNode* ptNewBlock = NULL;
            ptNewBlock = (plGeneralAllocatorNode *)((uintptr_t)(&ptBlock->puBlock) + szSize);
            ptNewBlock->szSize = ptBlock->szSize - szSize - PL_MEMORY_ALLOC_HEADER_SZ;
            ptBlock->szSize = szSize;

            // add node
            if(ptBlock->ptNext) {ptBlock->ptNext->ptPrev = ptNewBlock;}
            ptNewBlock->ptNext = ptBlock->ptNext;
            ptNewBlock->ptPrev = ptBlock;
            ptBlock->ptNext = ptNewBlock;
        }

        // delete node
        if(ptBlock->ptNext)
        {
            ptBlock->ptNext->ptPrev = ptBlock->ptPrev;
        }
        ptBlock->ptPrev->ptNext = ptBlock->ptNext;
        ptBlock->ptNext = NULL;
        ptBlock->ptPrev = NULL;

        ptAllocator->szUsed += ptBlock->szSize + PL_MEMORY_ALLOC_HEADER_SZ;

        if((uintptr_t)(&ptBlock->puBlock) + ptBlock->szSize  - (uintptr_t)ptAllocator->puBuffer > ptAllocator->szMaxHit)
        {
            ptAllocator->szMaxHit = (uintptr_t)(&ptBlock->puBlock) + ptBlock->szSize  - (uintptr_t)ptAllocator->puBuffer;
        }
        pBuffer = &ptBlock->puBlock;
    }
    return pBuffer;
}

void*
pl_general_allocator_aligned_alloc(plGeneralAllocator* ptAllocator, size_t szSize, size_t szAlignment)
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
        void* pActualBuffer = pl_general_allocator_alloc(ptAllocator, szSize + ulHeaderSize);

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
pl_general_allocator_free(plGeneralAllocator* ptAllocator, void* pData)
{
    PL_ASSERT(pData);

    plGeneralAllocatorNode* ptBlock = (plGeneralAllocatorNode*)((uintptr_t)pData - PL_MEMORY_ALLOC_HEADER_SZ);
    ptAllocator->szUsed -= ptBlock->szSize + PL_MEMORY_ALLOC_HEADER_SZ;

    // If the free list is not empty
    if(ptAllocator->tFreeList.ptNext != NULL)
    {
        // Add node to free list
        plGeneralAllocatorNode* ptFreeBlock = ptAllocator->tFreeList.ptNext;   
        while(ptFreeBlock) // GCOVR_EXCL_LINE
        {
            // Insert new node to middle
            if(ptFreeBlock > ptBlock)
            {
                plGeneralAllocatorNode *ptNext = ptFreeBlock;
                plGeneralAllocatorNode *ptPrev = ptFreeBlock->ptPrev;

                ptNext->ptPrev = ptBlock;
                ptBlock->ptNext = ptNext;
                ptBlock->ptPrev = ptPrev;
                ptPrev->ptNext = ptBlock;
                break;
                
            }

            // There isn't a next block so Insert new node to the end
            if(ptFreeBlock->ptNext == NULL)
            {                
                ptBlock->ptNext = NULL;
                ptBlock->ptPrev = ptFreeBlock;
                ptFreeBlock->ptNext = ptBlock;
                break;
            }

            ptFreeBlock = ptFreeBlock->ptNext;
        } 

        // Defrag
        plGeneralAllocatorNode* ptPrevBlock = ptBlock->ptPrev;
        plGeneralAllocatorNode* ptNextBlock = ptBlock->ptNext;

        if(ptPrevBlock != &ptAllocator->tFreeList)
        {
            // if prev block and block are adjacent
            if(((uintptr_t)ptPrevBlock + PL_MEMORY_ALLOC_HEADER_SZ + ptPrevBlock->szSize) == (uintptr_t)ptBlock)
            {
                ptPrevBlock->szSize += PL_MEMORY_ALLOC_HEADER_SZ + ptBlock->szSize;
                
                // delete node
                if(ptNextBlock != NULL) {ptNextBlock->ptPrev = ptPrevBlock; }
                ptPrevBlock->ptNext = ptNextBlock;
                ptBlock->ptNext = NULL;
                ptBlock->ptPrev = NULL;

                // Allows us to defrag with the next block without checking if we defragged with this one
                ptBlock = ptPrevBlock; 
            }
        }

        if(ptNextBlock != NULL)
        {

            plGeneralAllocatorNode* ptNextNextBlock = ptNextBlock->ptNext;

            // if block and next block are adjacent
            if(((uintptr_t)ptBlock + PL_MEMORY_ALLOC_HEADER_SZ + ptBlock->szSize) == (uintptr_t)ptNextBlock)
            {
                ptBlock->szSize += PL_MEMORY_ALLOC_HEADER_SZ + ptNextBlock->szSize;
                
                // delete next node
                if(ptNextNextBlock != NULL)
                {
                    ptNextNextBlock->ptPrev = ptBlock;
                }
                ptBlock->ptNext = ptNextNextBlock;
                ptNextBlock->ptNext = NULL;
                ptNextBlock->ptPrev = NULL;

            }
        }
    }
    else 
    {
        // Add node to the front of the free list if list is empty
        ptBlock->ptNext = NULL;
        ptBlock->ptPrev = &ptAllocator->tFreeList;
        ptAllocator->tFreeList.ptNext = ptBlock;
    }
}

void
pl_general_allocator_aligned_free(plGeneralAllocator* ptAllocator, void* pData)
{
    PL_ASSERT(pData);

    // get stored offset
    uint64_t ulOffset = *((uint64_t*)pData - 1);

    // get original buffer to free
    void* pActualBuffer = ((uint8_t*)pData - ulOffset);
    pl_general_allocator_free(ptAllocator, pActualBuffer);
}

#endif // PL_MEMORY_IMPLEMENTATION


//-----------------------------------------------------------------------------
// [SECTION] revision history
//-----------------------------------------------------------------------------

// 1.1.0  (2025-04-16) add simple linked list general allocator
