/*
   pl_gpu_allocators_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] defines
// [SECTION] forward declarations
// [SECTION] APIs
// [SECTION] public api
// [SECTION] public api structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_GPU_ALLOCATORS_EXT_H
#define PL_GPU_ALLOCATORS_EXT_H

#define PL_GPU_ALLOCATORS_EXT_VERSION    "0.9.0"
#define PL_GPU_ALLOCATORS_EXT_VERSION_NUM 000900

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
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

typedef struct _plDeviceMemoryAllocatorI plDeviceMemoryAllocatorI;
typedef struct _plDevice plDevice;

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define PL_API_GPU_ALLOCATORS "PL_API_GPU_ALLOCATORS"
typedef struct _plGPUAllocatorsI plGPUAllocatorsI;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

const plGPUAllocatorsI* pl_load_gpu_allocators_api(void);

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plGPUAllocatorsI
{
    plDeviceMemoryAllocatorI* (*create_local_dedicated_allocator) (plDevice* ptDevice);
    plDeviceMemoryAllocatorI* (*create_local_buddy_allocator)     (plDevice* ptDevice);
    plDeviceMemoryAllocatorI* (*create_staging_uncached_allocator)(plDevice* ptDevice);

    void (*cleanup_allocators)(plDevice* ptDevice);
} plGPUAllocatorsI;

#endif // PL_GPU_ALLOCATORS_EXT_H