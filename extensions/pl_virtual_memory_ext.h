/*
   pl_virtual_memory_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] APIs
// [SECTION] public api
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_VIRTUAL_MEMORY_EXT_H
#define PL_VIRTUAL_MEMORY_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stddef.h> // size_t

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plVirtualMemoryI_version {1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

typedef struct _plVirtualMemoryI
{

    // Notes
    //   - committed memory does not necessarily mean the memory has been mapped to physical
    //     memory. This is happens when the memory is actually touched. Even so, on Windows
    //     you can not commit more memmory then you have in your page file.
    //   - uncommitted memory does not necessarily mean the memory will be immediately
    //     evicted. It is up to the OS.

    size_t (*get_page_size)(void);                  // returns memory page size
    void*  (*alloc)        (void* address, size_t); // reserves & commits a block of memory. pAddress is starting address or use NULL to have system choose. szSize must be a multiple of memory page size.
    void*  (*reserve)      (void* address, size_t); // reserves a block of memory. pAddress is starting address or use NULL to have system choose. szSize must be a multiple of memory page size.
    void*  (*commit)       (void* address, size_t); // commits a block of reserved memory. szSize must be a multiple of memory page size.
    void   (*uncommit)     (void* address, size_t); // uncommits a block of committed memory.
    void   (*free)         (void* address, size_t); // frees a block of previously reserved/committed memory. Must be the starting address returned from "reserve()" or "alloc()"
    
} plVirtualMemoryI;

#endif // PL_VIRTUAL_MEMORY_EXT_H