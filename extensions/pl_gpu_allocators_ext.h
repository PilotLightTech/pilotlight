/*
   pl_gpu_allocators_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] APIs
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*

    Implementation:
        The provided implementation of this extension depends on the following
        APIs being available:

        * plGraphicsI (v1.x)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_GPU_ALLOCATORS_EXT_H
#define PL_GPU_ALLOCATORS_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plDeviceAllocationRange  plDeviceAllocationRange;
typedef struct _plDeviceMemoryAllocation plDeviceMemoryAllocation;

// external (pl_graphics_ext.h)
typedef struct _plDeviceMemoryAllocatorI plDeviceMemoryAllocatorI;
typedef struct _plDevice plDevice;

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plGPUAllocatorsI_version {1, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_gpu_allocators_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_gpu_allocators_ext(plApiRegistryI*, bool reload);

// allocators
PL_API plDeviceMemoryAllocatorI* pl_gpu_allocators_get_local_dedicated_allocator       (plDevice*);
PL_API plDeviceMemoryAllocatorI* pl_gpu_allocators_get_local_buddy_allocator           (plDevice*);
PL_API plDeviceMemoryAllocatorI* pl_gpu_allocators_get_staging_uncached_allocator      (plDevice*);
PL_API plDeviceMemoryAllocatorI* pl_gpu_allocators_get_staging_uncached_buddy_allocator(plDevice*);
PL_API plDeviceMemoryAllocatorI* pl_gpu_allocators_get_staging_cached_allocator        (plDevice*);

// misc
PL_API size_t                   pl_gpu_allocators_get_buddy_block_size(void);
PL_API void                     pl_gpu_allocators_cleanup(plDevice*);

// for debug viewing
PL_API plDeviceMemoryAllocation* pl_gpu_allocators_get_blocks(const plDeviceMemoryAllocatorI*, uint32_t* sizeOut);
PL_API plDeviceAllocationRange*  pl_gpu_allocators_get_ranges(const plDeviceMemoryAllocatorI*, uint32_t* sizeOut);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plGPUAllocatorsI
{
    // allocators
    plDeviceMemoryAllocatorI* (*get_local_dedicated_allocator)       (plDevice*);
    plDeviceMemoryAllocatorI* (*get_local_buddy_allocator)           (plDevice*);
    plDeviceMemoryAllocatorI* (*get_staging_uncached_allocator)      (plDevice*);
    plDeviceMemoryAllocatorI* (*get_staging_uncached_buddy_allocator)(plDevice*);
    plDeviceMemoryAllocatorI* (*get_staging_cached_allocator)        (plDevice*);

    // misc
    size_t (*get_buddy_block_size)(void);

    void (*cleanup)(plDevice*);

    // for debug viewing
    plDeviceMemoryAllocation* (*get_blocks)(const plDeviceMemoryAllocatorI*, uint32_t* sizeOut);
    plDeviceAllocationRange*  (*get_ranges)(const plDeviceMemoryAllocatorI*, uint32_t* sizeOut);

} plGPUAllocatorsI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plDeviceAllocationRange
{
    char     acName[PL_MAX_NAME_LENGTH];
    uint64_t ulOffset;
    uint64_t ulUsedSize;
    uint64_t ulTotalSize;
    uint64_t ulBlockIndex;
    uint32_t uNodeIndex;
    uint32_t uNextNode;
} plDeviceAllocationRange;

#ifdef __cplusplus
}
#endif

#endif // PL_GPU_ALLOCATORS_EXT_H