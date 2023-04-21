/*
   pl_vulkan_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] defines
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api structs
// [SECTION] structs
// [SECTION] extension info
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_VULKAN_EXT_H
#define PL_VULKAN_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#define PL_EXT_VULKAN             "PL_EXT_VULKAN"
#define PL_API_DESCRIPTOR_MANAGER "PL_API_DESCRIPTOR_MANAGER"
#define PL_API_DEVICE_MEMORY      "PL_API_DEVICE_MEMORY"

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>
#include "pl_registry.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// apis
typedef struct _plDeviceMemoryApiI      plDeviceMemoryApiI;
typedef struct _plDescriptorManagerApiI plDescriptorManagerApiI;

// types
typedef struct _plDescriptorManager      plDescriptorManager;
typedef struct _plDeviceMemoryAllocatorI plDeviceMemoryAllocatorI;

// external api (pl_registry.h)
typedef struct _plApiRegistryApiI plApiRegistryApiI;

// external (pl_graphics_vulkan.h)
typedef struct _plBindGroupLayout plBindGroupLayout;

// external vulkan types
typedef struct VkDeviceMemory_T*        VkDeviceMemory;
typedef struct VkDevice_T*              VkDevice;
typedef struct VkPhysicalDevice_T*      VkPhysicalDevice;
typedef struct VkDescriptorSetLayout_T* VkDescriptorSetLayout;

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plDeviceMemoryApiI
{
    plDeviceMemoryAllocatorI (*create_device_local_allocator)(VkPhysicalDevice tPhysicalDevice, VkDevice tDevice);
    plDeviceMemoryAllocatorI (*create_staging_uncached_allocator)(VkPhysicalDevice tPhysicalDevice, VkDevice tDevice);
} plDeviceMemoryApiI;

typedef struct _plDescriptorManagerApiI
{
    VkDescriptorSetLayout (*request_layout)(plDescriptorManager* ptManager, plBindGroupLayout* ptLayout);
    void                  (*cleanup)       (plDescriptorManager* ptManager);
} plDescriptorManagerApiI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plDeviceMemoryAllocation
{
    VkDeviceMemory tMemory;
    uint64_t       ulOffset;
    uint64_t       ulSize;
    void*          pHostMapped;
} plDeviceMemoryAllocation;

typedef struct _plDescriptorManager
{
    VkDevice               tDevice;
    VkDescriptorSetLayout* _sbtDescriptorSetLayouts;
    uint32_t*              _sbuDescriptorSetLayoutHashes;
} plDescriptorManager;

typedef struct _plDeviceMemoryAllocatorI
{

    struct plDeviceMemoryAllocatorO* ptInst; // opaque pointer

    plDeviceMemoryAllocation (*allocate)(struct plDeviceMemoryAllocatorO* ptInst, uint64_t ulSize, uint64_t ulAlignment, const char* pcName);
    void                     (*free)    (struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation);

} plDeviceMemoryAllocatorI;

//-----------------------------------------------------------------------------
// [SECTION] extension info
//-----------------------------------------------------------------------------

static inline void
pl_get_vulkan_ext_info(plExtension* ptExtension)
{
    ptExtension->pcTransName     = "./pl_vulkan_ext_";
    ptExtension->pcLoadFunc      = "pl_load_vulkan_ext";
    ptExtension->pcUnloadFunc    = "pl_unload_vulkan_ext";

    #ifdef _WIN32
        ptExtension->pcLibName = "./pl_vulkan_ext.dll";
    #elif defined(__APPLE__)
        ptExtension->pcLibName = "./pl_vulkan_ext.dylib";
    #else
        ptExtension->pcLibName = "./pl_vulkan_ext.so";
    #endif
}

#endif // PL_VULKAN_EXT_H