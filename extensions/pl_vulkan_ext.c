/*
   pl_vulkan_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal structs
// [SECTION] internal api
// [SECTION] extension functions
// [SECTION] internal api implementation
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pilotlight.h"
#include "pl_vulkan_ext.h"
#include "pl_graphics_vulkan.h"
#include "pl_string.h"
#include "pl_registry.h"
#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plDeviceDedicatedAllocatorData
{
    uint32_t uAllocations;
    uint32_t uMemoryType;
    VkDevice tDevice;

} plDeviceDedicatedAllocatorData;

typedef struct _plDeviceStagedUncachedAllocatorData
{ 
    uint32_t uAllocations;
    uint32_t uMemoryType;
    VkDevice tDevice;

} plDeviceStagedUncachedAllocatorData;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static plDeviceMemoryAllocation pl__allocate_dedicated(struct plDeviceMemoryAllocatorO* ptInst, uint64_t ulSize, uint64_t ulAlignment, const char* pcName);
static void                     pl__free_dedicated    (struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation);
static plDeviceMemoryAllocation pl__allocate_staging_uncached(struct plDeviceMemoryAllocatorO* ptInst, uint64_t ulSize, uint64_t ulAlignment, const char* pcName);
static void                     pl__free_staging_uncached    (struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation);

static plDeviceMemoryAllocatorI pl__create_device_local_allocator    (VkPhysicalDevice tPhysicalDevice, VkDevice tDevice);
static plDeviceMemoryAllocatorI pl__create_staging_uncached_allocator(VkPhysicalDevice tPhysicalDevice, VkDevice tDevice);

static void                     pl__cleanup_descriptor_manager   (plDescriptorManager* ptManager);
static VkDescriptorSetLayout    pl__request_descriptor_set_layout(plDescriptorManager* ptManager, plBindGroupLayout* ptLayout);

//-----------------------------------------------------------------------------
// [SECTION] extension functions
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_vulkan_ext(plApiRegistryApiI* ptApiRegistry, plExtension* ptExtension, bool bReload)
{

    static plDescriptorManagerApiI tApi0 = {
        .request_layout = pl__request_descriptor_set_layout,
        .cleanup        = pl__cleanup_descriptor_manager
    };
    

    static plDeviceMemoryApiI tApi1 = {
        .create_device_local_allocator     = pl__create_device_local_allocator,
        .create_staging_uncached_allocator = pl__create_staging_uncached_allocator
    };

    if(bReload)
    {
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_DESCRIPTOR_MANAGER), &tApi0);
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_DEVICE_MEMORY), &tApi1);
    }
    else
    {
        ptApiRegistry->add(PL_API_DESCRIPTOR_MANAGER, &tApi0);
        ptApiRegistry->add(PL_API_DEVICE_MEMORY, &tApi1);
    }

    plExtensionRegistryApiI* ptExtensionRegistry = ptApiRegistry->first(PL_API_EXTENSION_REGISTRY);
    ptExtensionRegistry->load(ptApiRegistry, ptExtension);
}

PL_EXPORT void
pl_unload_vulkan_ext(plApiRegistryApiI* ptApiRegistry, plExtension* ptExtension)
{
    
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static plDeviceMemoryAllocation
pl__allocate_dedicated(struct plDeviceMemoryAllocatorO* ptInst, uint64_t ulSize, uint64_t ulAlignment, const char* pcName)
{
    plDeviceDedicatedAllocatorData* ptData = (plDeviceDedicatedAllocatorData*)ptInst;
    ptData->uAllocations++;

    plDeviceMemoryAllocation tAllocation = {
        .pHostMapped = NULL,
        .tMemory     = VK_NULL_HANDLE,
        .ulOffset    = 0,
        .ulSize      = ulSize
    };

    const VkMemoryAllocateInfo tAllocInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = ulSize,
        .memoryTypeIndex = ptData->uMemoryType
    };

    PL_VULKAN(vkAllocateMemory(ptData->tDevice, &tAllocInfo, NULL, &tAllocation.tMemory));

    return tAllocation;
}


static void
pl__free_dedicated(struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation)
{
    plDeviceDedicatedAllocatorData* ptData = (plDeviceDedicatedAllocatorData*)ptInst;

    vkFreeMemory(ptData->tDevice, ptAllocation->tMemory, NULL);

    ptAllocation->pHostMapped  = NULL;
    ptAllocation->tMemory      = VK_NULL_HANDLE;
    ptAllocation->ulOffset     = 0;
    ptAllocation->ulSize       = 0;

    ptData->uAllocations--;
}

static plDeviceMemoryAllocation
pl__allocate_staging_uncached(struct plDeviceMemoryAllocatorO* ptInst, uint64_t ulSize, uint64_t ulAlignment, const char* pcName)
{
    plDeviceDedicatedAllocatorData* ptData = (plDeviceDedicatedAllocatorData*)ptInst;
    ptData->uAllocations++;

    plDeviceMemoryAllocation tAllocation = {
        .pHostMapped = NULL,
        .tMemory     = VK_NULL_HANDLE,
        .ulOffset    = 0,
        .ulSize      = ulSize
    };

    const VkMemoryAllocateInfo tAllocInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = ulSize,
        .memoryTypeIndex = ptData->uMemoryType
    };

    PL_VULKAN(vkAllocateMemory(ptData->tDevice, &tAllocInfo, NULL, &tAllocation.tMemory));

    PL_VULKAN(vkMapMemory(ptData->tDevice, tAllocation.tMemory, 0, ulSize, 0, (void**)&tAllocation.pHostMapped));

    return tAllocation;
}


static void
pl__free_staging_uncached(struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation)
{
    plDeviceDedicatedAllocatorData* ptData = (plDeviceDedicatedAllocatorData*)ptInst;

    vkUnmapMemory(ptData->tDevice, ptAllocation->tMemory);

    vkFreeMemory(ptData->tDevice, ptAllocation->tMemory, NULL);

    ptAllocation->pHostMapped  = NULL;
    ptAllocation->tMemory      = VK_NULL_HANDLE;
    ptAllocation->ulOffset     = 0;
    ptAllocation->ulSize       = 0;

    ptData->uAllocations--;
}

static plDeviceMemoryAllocatorI
pl__create_device_local_allocator(VkPhysicalDevice tPhysicalDevice, VkDevice tDevice)
{
    plDeviceDedicatedAllocatorData* ptData = malloc(sizeof(plDeviceDedicatedAllocatorData));
    memset(ptData, 0, sizeof(plDeviceDedicatedAllocatorData));

    // create allocator interface
    plDeviceMemoryAllocatorI tAllocatorInterface = 
    {
        .allocate = pl__allocate_dedicated,
        .free     = pl__free_dedicated,
        .ptInst   = (struct plDeviceMemoryAllocatorO*)ptData
    };

    VkPhysicalDeviceMemoryProperties tMemProps;
    vkGetPhysicalDeviceMemoryProperties(tPhysicalDevice, &tMemProps);

    ptData->uMemoryType = 0u;
    bool bFound = false;
    for (uint32_t i = 0; i < tMemProps.memoryTypeCount; i++) 
    {
        if (tMemProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) 
        {
            ptData->uMemoryType = i;
            bFound = true;
            break;
        }
    }
    PL_ASSERT(bFound);

    ptData->tDevice = tDevice;
    return tAllocatorInterface;
}

static plDeviceMemoryAllocatorI
pl__create_staging_uncached_allocator(VkPhysicalDevice tPhysicalDevice, VkDevice tDevice)
{

    plDeviceStagedUncachedAllocatorData* ptData = malloc(sizeof(plDeviceStagedUncachedAllocatorData));
    memset(ptData, 0, sizeof(plDeviceStagedUncachedAllocatorData));

    // create allocator interface
    plDeviceMemoryAllocatorI tAllocatorInterface = 
    {
        .allocate = pl__allocate_staging_uncached,
        .free     = pl__free_staging_uncached,
        .ptInst   = (struct plDeviceMemoryAllocatorO*)ptData
    };

    VkPhysicalDeviceMemoryProperties tMemProps;
    vkGetPhysicalDeviceMemoryProperties(tPhysicalDevice, &tMemProps);

    ptData->uMemoryType = 0u;
    bool bFound = false;
    for (uint32_t i = 0; i < tMemProps.memoryTypeCount; i++) 
    {
        if (tMemProps.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) 
        {
            ptData->uMemoryType = i;
            bFound = true;
            break;
        }
    }
    PL_ASSERT(bFound);

    ptData->tDevice = tDevice;
    return tAllocatorInterface;
}

static void
pl__cleanup_descriptor_manager(plDescriptorManager* ptManager)
{
    for(uint32_t i = 0; i < pl_sb_size(ptManager->_sbtDescriptorSetLayouts); i++)
    {
        vkDestroyDescriptorSetLayout(ptManager->tDevice, ptManager->_sbtDescriptorSetLayouts[i], NULL);
        ptManager->_sbtDescriptorSetLayouts[i] = VK_NULL_HANDLE;
    }

    pl_sb_free(ptManager->_sbuDescriptorSetLayoutHashes);
    pl_sb_free(ptManager->_sbtDescriptorSetLayouts);
}

static VkDescriptorSetLayout
pl__request_descriptor_set_layout(plDescriptorManager* ptManager, plBindGroupLayout* ptLayout)
{
    
    // generate hash
    uint32_t uHash = 0;
    VkDescriptorSetLayoutBinding* sbtDescriptorSetLayoutBindings = NULL;
    for(uint32_t i = 0 ; i < ptLayout->uBufferCount; i++)
    {
        uHash = pl_str_hash_data(&ptLayout->aBuffers[i].uSlot, sizeof(uint32_t), uHash);
        uHash = pl_str_hash_data(&ptLayout->aBuffers[i].tType, sizeof(int), uHash);
        uHash = pl_str_hash_data(&ptLayout->aBuffers[i].tStageFlags, sizeof(VkShaderStageFlags), uHash);

        VkDescriptorSetLayoutBinding tBinding = {
            .binding            = ptLayout->aBuffers[i].uSlot,
            .descriptorType     = ptLayout->aBuffers[i].tType == PL_BUFFER_BINDING_TYPE_STORAGE ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .descriptorCount    = 1,
            .stageFlags         = ptLayout->aBuffers[i].tStageFlags,
            .pImmutableSamplers = NULL
        };
        pl_sb_push(sbtDescriptorSetLayoutBindings, tBinding);
    }

    for(uint32_t i = 0 ; i < ptLayout->uTextureCount; i++)
    {
        uHash = pl_str_hash_data(&ptLayout->aTextures[i].uSlot, sizeof(uint32_t), uHash);
        uHash = pl_str_hash_data(&ptLayout->aTextures[i].tStageFlags, sizeof(VkShaderStageFlags), uHash);

        VkDescriptorSetLayoutBinding tBinding = {
            .binding            = ptLayout->aTextures[i].uSlot,
            .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount    = 1,
            .stageFlags         = ptLayout->aTextures[i].tStageFlags,
            .pImmutableSamplers = NULL
        };
        pl_sb_push(sbtDescriptorSetLayoutBindings, tBinding);
    }

    // check if hash exists
    for(uint32_t i = 0; i < pl_sb_size(ptManager->_sbuDescriptorSetLayoutHashes); i++)
    {
        if(ptManager->_sbuDescriptorSetLayoutHashes[i] == uHash)
        {
            pl_sb_free(sbtDescriptorSetLayoutBindings);
            return ptManager->_sbtDescriptorSetLayouts[i];
        }
    }

    // create descriptor set layout
    VkDescriptorSetLayout tDescriptorSetLayout = VK_NULL_HANDLE;
    const VkDescriptorSetLayoutCreateInfo tDescriptorSetLayoutInfo = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = pl_sb_size(sbtDescriptorSetLayoutBindings),
        .pBindings    = sbtDescriptorSetLayoutBindings,
    };
    PL_VULKAN(vkCreateDescriptorSetLayout(ptManager->tDevice, &tDescriptorSetLayoutInfo, NULL, &tDescriptorSetLayout));

    pl_sb_push(ptManager->_sbuDescriptorSetLayoutHashes, uHash);
    pl_sb_push(ptManager->_sbtDescriptorSetLayouts, tDescriptorSetLayout);
    pl_sb_free(sbtDescriptorSetLayoutBindings);

    return tDescriptorSetLayout;
}
