/*
   pl_gpu_allocators_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] defines
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

// extension version (format XYYZZ)
#define PL_GPU_ALLOCATORS_EXT_VERSION    "1.0.0"
#define PL_GPU_ALLOCATORS_EXT_VERSION_NUM 10000

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_MAX_NAME_LENGTH
    #define PL_MAX_NAME_LENGTH 1024
#endif

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

#define PL_API_GPU_ALLOCATORS "PL_API_GPU_ALLOCATORS"
typedef struct _plGPUAllocatorsI plGPUAllocatorsI;

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plGPUAllocatorsI
{
    // allocators
    plDeviceMemoryAllocatorI* (*get_local_dedicated_allocator) (plDevice*);
    plDeviceMemoryAllocatorI* (*get_local_buddy_allocator)     (plDevice*);
    plDeviceMemoryAllocatorI* (*get_staging_uncached_allocator)(plDevice*);
    plDeviceMemoryAllocatorI* (*get_staging_cached_allocator)  (plDevice*);

    void (*cleanup)(plDevice*);

    // for debug viewing
    plDeviceMemoryAllocation* (*get_blocks)(const plDeviceMemoryAllocatorI*, uint32_t* puSizeOut);
    plDeviceAllocationRange*  (*get_ranges)(const plDeviceMemoryAllocatorI*, uint32_t* puSizeOut);

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