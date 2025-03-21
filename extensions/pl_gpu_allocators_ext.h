/*
   pl_gpu_allocators_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] APIs
// [SECTION] public api structs
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_GPU_ALLOCATORS_EXT_H
#define PL_GPU_ALLOCATORS_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

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

#define plGPUAllocatorsI_version {1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plGPUAllocatorsI
{
    // allocators
    plDeviceMemoryAllocatorI* (*get_local_dedicated_allocator)       (plDevice*);
    plDeviceMemoryAllocatorI* (*get_local_buddy_allocator)           (plDevice*);
    plDeviceMemoryAllocatorI* (*get_staging_uncached_allocator)      (plDevice*);
    plDeviceMemoryAllocatorI* (*get_staging_uncached_buddy_allocator)(plDevice*);
    plDeviceMemoryAllocatorI* (*get_staging_cached_allocator)        (plDevice*);

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

#endif // PL_GPU_ALLOCATORS_EXT_H