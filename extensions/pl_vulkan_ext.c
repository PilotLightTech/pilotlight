/*
   pl_vulkan_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal structs
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] internal api implementation
// [SECTION] extension functions
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdio.h>
#include "pilotlight.h"
#include "pl_vulkan_ext.h"
#include "pl_string.h"
#include "pl_ds.h"
#include "pl_io.h"
#include "pl_os.h"
#include "pl_profile.h"
#include "pl_log.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

#ifdef _WIN32
#pragma comment(lib, "vulkan-1.lib")
#endif

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plVariantInfo
{
    VkShaderModuleCreateInfo tVertexShaderInfo;
    VkShaderModuleCreateInfo tPixelShaderInfo;
    VkPipelineLayout         tPipelineLayout;
    VkRenderPass             tRenderPass;
    VkSampleCountFlagBits    tMSAASampleCount;
    VkSpecializationInfo     tSpecializationInfo;
} plVariantInfo;

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

// setup
static void                  pl_setup_graphics               (plGraphics* ptGraphics, plApiRegistryApiI* ptApiRegistry);
static void                  pl_cleanup_graphics             (plGraphics* ptGraphics);
static void                  pl_resize_graphics              (plGraphics* ptGraphics);

// per frame
static bool                  pl_begin_frame                  (plGraphics* ptGraphics);
static void                  pl_end_frame                    (plGraphics* ptGraphics);
static void                  pl_begin_recording              (plGraphics* ptGraphics);
static void                  pl_end_recording                (plGraphics* ptGraphics);

// resource manager per frame
static void                  pl_process_cleanup_queue        (plResourceManager* ptResourceManager, uint32_t uFramesToProcess);

// resource manager commited resources
static uint32_t              pl_create_index_buffer          (plResourceManager* ptResourceManager, size_t szSize, const void* pData, const char* pcName);
static uint32_t              pl_create_vertex_buffer         (plResourceManager* ptResourceManager, size_t szSize, size_t szStride, const void* pData, const char* pcName);
static uint32_t              pl_create_constant_buffer       (plResourceManager* ptResourceManager, size_t szSize, const char* pcName);
static uint32_t              pl_create_texture               (plResourceManager* ptResourceManager, plTextureDesc tDesc, size_t szSize, const void* pData, const char* pcName);
static uint32_t              pl_create_storage_buffer        (plResourceManager* ptResourceManager, size_t szSize, const void* pData, const char* pcName);
static uint32_t              pl_create_texture_view          (plResourceManager* ptResourceManager, const plTextureViewDesc* ptViewDesc, const plSampler* ptSampler, uint32_t uTextureHandle, const char* pcName);

// resource manager dynamic buffers
static uint32_t              pl_request_dynamic_buffer         (plResourceManager* ptResourceManager);
static void                  pl_return_dynamic_buffer          (plResourceManager* ptResourceManager, uint32_t uNodeIndex);

// resource manager misc.
static void                  pl_transfer_data_to_image          (plResourceManager* ptResourceManager, plTexture* ptDest, size_t szDataSize, const void* pData);
static void                  pl_transfer_data_to_buffer         (plResourceManager* ptResourceManager, VkBuffer tDest, size_t szSize, const void* pData);
static void                  pl_submit_buffer_for_deletion      (plResourceManager* ptResourceManager, uint32_t uBufferIndex);
static void                  pl_submit_texture_for_deletion     (plResourceManager* ptResourceManager, uint32_t uTextureIndex);
static void                  pl_submit_texture_view_for_deletion(plResourceManager* ptResourceManager, uint32_t uTextureViewIndex);

// command buffers
static VkCommandBuffer       pl_begin_command_buffer         (plGraphics* ptGraphics);
static void                  pl_submit_command_buffer        (plGraphics* ptGraphics, VkCommandBuffer tCmdBuffer);

// shaders
static uint32_t              pl_create_shader             (plResourceManager* ptResourceManager, const plShaderDesc* ptDesc);
static uint32_t              pl_add_shader_variant        (plResourceManager* ptResourceManager, uint32_t uShader, plGraphicsState tVariant, VkRenderPass ptRenderPass, VkSampleCountFlagBits tMSAASampleCount);
static bool                  pl_shader_variant_exist      (plResourceManager* ptResourceManager, uint32_t uShader, plGraphicsState tVariant, VkRenderPass ptRenderPass, VkSampleCountFlagBits tMSAASampleCount);
static void                  pl_submit_shader_for_deletion(plResourceManager* ptResourceManager, uint32_t uShaderIndex);
static plBindGroupLayout*    pl_get_bind_group_layout     (plResourceManager* ptResourceManager, uint32_t uShaderIndex, uint32_t uBindGroupIndex);
static plShaderVariant*      pl_get_shader                (plResourceManager* ptResourceManager, uint32_t uVariantIndex);

// descriptors
static void                  pl_update_bind_group            (plGraphics* ptGraphics, plBindGroup* ptGroup, uint32_t uBufferCount, uint32_t* auBuffers, size_t* aszBufferRanges, uint32_t uTextureViewCount, uint32_t* auTextureViews);

// drawing
static void                  pl_draw_areas                   (plGraphics* ptGraphics, uint32_t uAreaCount, plDrawArea* atAreas, plDraw* atDraws);

// misc
static plFrameContext*       pl_get_frame_resources          (plGraphics* ptGraphics);
static uint32_t              pl_find_memory_type             (VkPhysicalDeviceMemoryProperties tMemProps, uint32_t uTypeFilter, VkMemoryPropertyFlags tProperties);
static void                  pl_transition_image_layout      (VkCommandBuffer tCommandBuffer, VkImage tImage, VkImageLayout tOldLayout, VkImageLayout tNewLayout, VkImageSubresourceRange tSubresourceRange, VkPipelineStageFlags tSrcStageMask, VkPipelineStageFlags tDstStageMask);
static void                  pl_set_vulkan_object_name       (plGraphics* ptGraphics, uint64_t uObjectHandle, VkDebugReportObjectTypeEXT tObjectType, const char* pcName);

static VKAPI_ATTR VkBool32 VKAPI_CALL pl__debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT tMsgSeverity, VkDebugUtilsMessageTypeFlagsEXT tMsgType, const VkDebugUtilsMessengerCallbackDataEXT* ptCallbackData, void* pUserData);

// low level setup
static void pl__create_instance       (plGraphics* ptGraphics, uint32_t uVersion, bool bEnableValidation);
static void pl__create_instance_ex    (plGraphics* ptGraphics, uint32_t uVersion, uint32_t uLayerCount, const char** ppcEnabledLayers, uint32_t uExtensioncount, const char** ppcEnabledExtensions);
static void pl__create_frame_resources(plGraphics* ptGraphics, plDevice* ptDevice);
static void pl__create_device         (VkInstance tInstance, VkSurfaceKHR tSurface, plDevice* ptDeviceOut, bool bEnableValidation);

// low level swapchain ops
static void pl__create_swapchain   (plGraphics* ptGraphics, VkSurfaceKHR tSurface, uint32_t uWidth, uint32_t uHeight, plSwapchain* ptSwapchainOut);
static void pl__create_framebuffers(plDevice* ptDevice, VkRenderPass tRenderPass, plSwapchain* ptSwapchain);

// resource manager setup
static void pl__create_resource_manager  (plApiRegistryApiI* ptApiRegistry, plGraphics* ptGraphics, plDevice* ptDevice, plResourceManager* ptResourceManagerOut);
static void pl__cleanup_resource_manager (plResourceManager* ptResourceManager);

// shaders
static VkPipeline pl__create_shader_pipeline(plResourceManager* ptResourceManager, plGraphicsState tVariant, plVariantInfo* ptInfo);

// misc
static uint32_t                            pl__get_u32_max              (uint32_t a, uint32_t b) { return a > b ? a : b;}
static uint32_t                            pl__get_u32_min              (uint32_t a, uint32_t b) { return a < b ? a : b;}
static int                                 pl__select_physical_device   (VkInstance tInstance, plDevice* ptDeviceOut);
static void                                pl__staging_buffer_realloc   (plResourceManager* ptResourceManager, size_t szNewSize);
static inline void                         pl__staging_buffer_may_grow  (plResourceManager* ptResourceManager, size_t szSize);
static bool                                pl__get_free_resource_index  (uint32_t* sbuFreeIndices, uint32_t* puIndexOut);
static size_t                              pl__get_const_buffer_req_size(plDevice* ptDevice, size_t szSize);
static VkPipelineColorBlendAttachmentState pl__get_blend_state(plBlendMode tBlendMode);

// pilotlight to vulkan conversions
static VkFilter             pl__vulkan_filter (plFilter tFilter);
static VkSamplerAddressMode pl__vulkan_wrap   (plWrapMode tWrap);
static VkCompareOp          pl__vulkan_compare(plCompareMode tCompare);

// vulkan to pilotlight conversions
static plFilter      pl__pilotlight_filter (VkFilter tFilter);
static plWrapMode    pl__pilotlight_wrap   (VkSamplerAddressMode tWrap);
static plCompareMode pl__pilotlight_compare(VkCompareOp tCompare);

static plDeviceMemoryAllocation pl_allocate_dedicated(struct plDeviceMemoryAllocatorO* ptInst, uint64_t ulSize, uint64_t ulAlignment, const char* pcName);
static void                     pl_free_dedicated    (struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation);
static plDeviceMemoryAllocation pl_allocate_staging_uncached(struct plDeviceMemoryAllocatorO* ptInst, uint64_t ulSize, uint64_t ulAlignment, const char* pcName);
static void                     pl_free_staging_uncached    (struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation);

static plDeviceMemoryAllocatorI pl_create_device_local_allocator    (VkPhysicalDevice tPhysicalDevice, VkDevice tDevice);
static plDeviceMemoryAllocatorI pl_create_staging_uncached_allocator(VkPhysicalDevice tPhysicalDevice, VkDevice tDevice);

static void                     pl_cleanup_descriptor_manager   (plDescriptorManager* ptManager);
static VkDescriptorSetLayout    pl_request_descriptor_set_layout(plDescriptorManager* ptManager, plBindGroupLayout* ptLayout);


static VkFormat              pl_find_supported_format    (plDevice* ptDevice, VkFormatFeatureFlags tFlags, const VkFormat* ptFormats, uint32_t uFormatCount);
static VkFormat              pl_find_depth_format        (plDevice* ptDevice);
static VkFormat              pl_find_depth_stencil_format(plDevice* ptDevice);
static bool                  pl_format_has_stencil       (VkFormat tFormat);
static VkSampleCountFlagBits pl_get_max_sample_count     (plDevice* ptDevice);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

plGraphicsApiI*
pl_load_graphics_api(void)
{
    static plGraphicsApiI tApi = {
        .setup_graphics             = pl_setup_graphics,
        .cleanup_graphics           = pl_cleanup_graphics,
        .resize_graphics            = pl_resize_graphics,
        .begin_frame                = pl_begin_frame,
        .end_frame                  = pl_end_frame,
        .begin_recording            = pl_begin_recording,
        .end_recording              = pl_end_recording,
        .begin_command_buffer       = pl_begin_command_buffer,
        .submit_command_buffer      = pl_submit_command_buffer,
        .create_shader              = pl_create_shader,
        .add_shader_variant         = pl_add_shader_variant,
        .shader_variant_exist       = pl_shader_variant_exist,
        .submit_shader_for_deletion = pl_submit_shader_for_deletion,
        .get_bind_group_layout      = pl_get_bind_group_layout,
        .get_shader                 = pl_get_shader,
        .update_bind_group          = pl_update_bind_group,
        .draw_areas                 = pl_draw_areas,
        .get_frame_resources        = pl_get_frame_resources,
        .find_memory_type           = pl_find_memory_type,
        .transition_image_layout    = pl_transition_image_layout,
        .set_vulkan_object_name     = pl_set_vulkan_object_name
    };
    return &tApi;
}

plDeviceApiI*
pl_load_device_api(void)
{
    static plDeviceApiI tApi = {
        .find_depth_format         = pl_find_depth_format,
        .find_depth_stencil_format = pl_find_depth_stencil_format,
        .find_supported_format     = pl_find_supported_format,
        .format_has_stencil        = pl_format_has_stencil,
        .get_max_sample_count      = pl_get_max_sample_count
    };
    return &tApi;
}

plDeviceMemoryApiI*
pl_load_device_memory_api(void)
{
    static plDeviceMemoryApiI tApi = {
        .create_device_local_allocator     = pl_create_device_local_allocator,
        .create_staging_uncached_allocator = pl_create_staging_uncached_allocator
    };
    return &tApi;
}

plDescriptorManagerApiI*
pl_load_descriptor_manager_api(void)
{
    static plDescriptorManagerApiI tApi = {
        .request_layout = pl_request_descriptor_set_layout,
        .cleanup        = pl_cleanup_descriptor_manager
    };
    return &tApi;
}

plResourceManager0ApiI*
pl_load_resource_manager_api(void)
{
    static plResourceManager0ApiI tApi = {
        .process_cleanup_queue            = pl_process_cleanup_queue,
        .create_index_buffer              = pl_create_index_buffer,
        .create_vertex_buffer             = pl_create_vertex_buffer,
        .create_constant_buffer           = pl_create_constant_buffer,
        .create_texture                   = pl_create_texture,
        .create_storage_buffer            = pl_create_storage_buffer,
        .create_texture_view              = pl_create_texture_view,
        .request_dynamic_buffer           = pl_request_dynamic_buffer,
        .return_dynamic_buffer            = pl_return_dynamic_buffer,
        .transfer_data_to_image           = pl_transfer_data_to_image,
        .transfer_data_to_buffer          = pl_transfer_data_to_buffer,
        .submit_buffer_for_deletion       = pl_submit_buffer_for_deletion,
        .submit_texture_for_deletion      = pl_submit_texture_for_deletion,
        .submit_texture_view_for_deletion = pl_submit_texture_for_deletion,
    };
    return &tApi;
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static void
pl__create_instance(plGraphics* ptGraphics, uint32_t uVersion, bool bEnableValidation)
{
    static const char* pcKhronosValidationLayer = "VK_LAYER_KHRONOS_validation";

    const char** sbpcEnabledExtensions = NULL;
    pl_sb_push(sbpcEnabledExtensions, VK_KHR_SURFACE_EXTENSION_NAME);

    #ifdef _WIN32
        pl_sb_push(sbpcEnabledExtensions, VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
    #elif defined(__APPLE__)
        pl_sb_push(sbpcEnabledExtensions, "VK_EXT_metal_surface");
        pl_sb_push(sbpcEnabledExtensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    #else // linux
        pl_sb_push(sbpcEnabledExtensions, VK_KHR_XCB_SURFACE_EXTENSION_NAME);
    #endif

    if(bEnableValidation)
    {
        pl_sb_push(sbpcEnabledExtensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        pl_sb_push(sbpcEnabledExtensions, VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }

    pl__create_instance_ex(ptGraphics, uVersion, bEnableValidation ? 1 : 0, &pcKhronosValidationLayer, pl_sb_size(sbpcEnabledExtensions), sbpcEnabledExtensions);
    pl_sb_free(sbpcEnabledExtensions);
}

static void
pl__create_instance_ex(plGraphics* ptGraphics, uint32_t uVersion, uint32_t uLayerCount, const char** ppcEnabledLayers, uint32_t uExtensioncount, const char** ppcEnabledExtensions)
{

    // check if validation should be activated
    bool bValidationEnabled = false;
    for(uint32_t i = 0; i < uLayerCount; i++)
    {
        if(strcmp("VK_LAYER_KHRONOS_validation", ppcEnabledLayers[i]) == 0)
        {
            pl_log_trace_to_f(ptGraphics->uLogChannel, "vulkan validation enabled");
            bValidationEnabled = true;
            break;
        }
    }

    // retrieve supported layers
    uint32_t uInstanceLayersFound = 0u;
    VkLayerProperties* ptAvailableLayers = NULL;
    PL_VULKAN(vkEnumerateInstanceLayerProperties(&uInstanceLayersFound, NULL));
    if(uInstanceLayersFound > 0)
    {
        ptAvailableLayers = (VkLayerProperties*)ptGraphics->ptMemoryApi->alloc(sizeof(VkLayerProperties) * uInstanceLayersFound);
        PL_VULKAN(vkEnumerateInstanceLayerProperties(&uInstanceLayersFound, ptAvailableLayers));
    }

    // retrieve supported extensions
    uint32_t uInstanceExtensionsFound = 0u;
    VkExtensionProperties* ptAvailableExtensions = NULL;
    PL_VULKAN(vkEnumerateInstanceExtensionProperties(NULL, &uInstanceExtensionsFound, NULL));
    if(uInstanceExtensionsFound > 0)
    {
        ptAvailableExtensions = (VkExtensionProperties*)ptGraphics->ptMemoryApi->alloc(sizeof(VkExtensionProperties) * uInstanceExtensionsFound);
        PL_VULKAN(vkEnumerateInstanceExtensionProperties(NULL, &uInstanceExtensionsFound, ptAvailableExtensions));
    }

    // ensure extensions are supported
    const char** sbpcMissingExtensions = NULL;
    for(uint32_t i = 0; i < uExtensioncount; i++)
    {
        const char* requestedExtension = ppcEnabledExtensions[i];
        bool extensionFound = false;
        for(uint32_t j = 0; j < uInstanceExtensionsFound; j++)
        {
            if(strcmp(requestedExtension, ptAvailableExtensions[j].extensionName) == 0)
            {
                pl_log_trace_to_f(ptGraphics->uLogChannel, "extension %s found", ptAvailableExtensions[j].extensionName);
                extensionFound = true;
                break;
            }
        }

        if(!extensionFound)
        {
            pl_sb_push(sbpcMissingExtensions, requestedExtension);
        }
    }

    // report if all requested extensions aren't found
    if(pl_sb_size(sbpcMissingExtensions) > 0)
    {
        pl_log_error_to_f(ptGraphics->uLogChannel, "%d %s", pl_sb_size(sbpcMissingExtensions), "Missing Extensions:");
        for(uint32_t i = 0; i < pl_sb_size(sbpcMissingExtensions); i++)
        {
            pl_log_error_to_f(ptGraphics->uLogChannel, "  * %s", sbpcMissingExtensions[i]);
        }

        PL_ASSERT(false && "Can't find all requested extensions");
    }

    // ensure layers are supported
    const char** sbpcMissingLayers = NULL;
    for(uint32_t i = 0; i < uLayerCount; i++)
    {
        const char* pcRequestedLayer = ppcEnabledLayers[i];
        bool bLayerFound = false;
        for(uint32_t j = 0; j < uInstanceLayersFound; j++)
        {
            if(strcmp(pcRequestedLayer, ptAvailableLayers[j].layerName) == 0)
            {
                pl_log_trace_to_f(ptGraphics->uLogChannel, "layer %s found", ptAvailableLayers[j].layerName);
                bLayerFound = true;
                break;
            }
        }

        if(!bLayerFound)
        {
            pl_sb_push(sbpcMissingLayers, pcRequestedLayer);
        }
    }

    // report if all requested layers aren't found
    if(pl_sb_size(sbpcMissingLayers) > 0)
    {
        pl_log_error_to_f(ptGraphics->uLogChannel, "%d %s", pl_sb_size(sbpcMissingLayers), "Missing Layers:");
        for(uint32_t i = 0; i < pl_sb_size(sbpcMissingLayers); i++)
        {
            pl_log_error_to_f(ptGraphics->uLogChannel, "  * %s", sbpcMissingLayers[i]);
        }
        PL_ASSERT(false && "Can't find all requested layers");
    }

    // Setup debug messenger for vulkan tInstance
    VkDebugUtilsMessengerCreateInfoEXT tDebugCreateInfo = {
        .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = pl__debug_callback,
        .pNext           = VK_NULL_HANDLE
    };

    // create vulkan tInstance
    VkApplicationInfo tAppInfo = {
        .sType      = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = uVersion
    };

    VkInstanceCreateInfo tCreateInfo = {
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo        = &tAppInfo,
        .pNext                   = bValidationEnabled ? (VkDebugUtilsMessengerCreateInfoEXT*)&tDebugCreateInfo : VK_NULL_HANDLE,
        .enabledExtensionCount   = uExtensioncount,
        .ppEnabledExtensionNames = ppcEnabledExtensions,
        .enabledLayerCount       = uLayerCount,
        .ppEnabledLayerNames     = ppcEnabledLayers,

        #ifdef __APPLE__
        .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR
        #endif
    };

    PL_VULKAN(vkCreateInstance(&tCreateInfo, NULL, &ptGraphics->tInstance));
    pl_log_trace_to_f(ptGraphics->uLogChannel, "created vulkan instance");

    // cleanup
    if(ptAvailableLayers)     ptGraphics->ptMemoryApi->free(ptAvailableLayers);
    if(ptAvailableExtensions) ptGraphics->ptMemoryApi->free(ptAvailableExtensions);
    pl_sb_free(sbpcMissingLayers);
    pl_sb_free(sbpcMissingExtensions);

    if(bValidationEnabled)
    {
        PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ptGraphics->tInstance, "vkCreateDebugUtilsMessengerEXT");
        PL_ASSERT(func != NULL && "failed to set up debug messenger!");
        PL_VULKAN(func(ptGraphics->tInstance, &tDebugCreateInfo, NULL, &ptGraphics->tDbgMessenger));     
        pl_log_trace_to_f(ptGraphics->uLogChannel, "enabled Vulkan validation layers");
    }
}

static void
pl__create_device(VkInstance tInstance, VkSurfaceKHR tSurface, plDevice* ptDeviceOut, bool bEnableValidation)
{
    ptDeviceOut->iGraphicsQueueFamily = -1;
    ptDeviceOut->iPresentQueueFamily = -1;
    int iDeviceIndex = -1;
    ptDeviceOut->tMemProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    ptDeviceOut->tMemBudgetInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
    ptDeviceOut->tMemProps2.pNext = &ptDeviceOut->tMemBudgetInfo;
    iDeviceIndex = pl__select_physical_device(tInstance, ptDeviceOut);
    ptDeviceOut->tMaxLocalMemSize = ptDeviceOut->tMemProps.memoryHeaps[iDeviceIndex].size;

    // find queue families
    uint32_t uQueueFamCnt = 0u;
    vkGetPhysicalDeviceQueueFamilyProperties(ptDeviceOut->tPhysicalDevice, &uQueueFamCnt, NULL);

    VkQueueFamilyProperties auQueueFamilies[64] = {0};
    vkGetPhysicalDeviceQueueFamilyProperties(ptDeviceOut->tPhysicalDevice, &uQueueFamCnt, auQueueFamilies);

    for(uint32_t i = 0; i < uQueueFamCnt; i++)
    {
        if (auQueueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) ptDeviceOut->iGraphicsQueueFamily = i;

        VkBool32 tPresentSupport = false;
        PL_VULKAN(vkGetPhysicalDeviceSurfaceSupportKHR(ptDeviceOut->tPhysicalDevice, i, tSurface, &tPresentSupport));

        if (tPresentSupport) ptDeviceOut->iPresentQueueFamily  = i;

        if (ptDeviceOut->iGraphicsQueueFamily > -1 && ptDeviceOut->iPresentQueueFamily > -1) // complete
            break;
        i++;
    }

    //-----------------------------------------------------------------------------
    // create logical device
    //-----------------------------------------------------------------------------

    vkGetPhysicalDeviceFeatures(ptDeviceOut->tPhysicalDevice, &ptDeviceOut->tDeviceFeatures);

    const float fQueuePriority = 1.0f;
    VkDeviceQueueCreateInfo atQueueCreateInfos[] = {
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = ptDeviceOut->iGraphicsQueueFamily,
            .queueCount = 1,
            .pQueuePriorities = &fQueuePriority
        },
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = ptDeviceOut->iPresentQueueFamily,
            .queueCount = 1,
            .pQueuePriorities = &fQueuePriority   
        }
    };
    
    static const char* pcValidationLayers = "VK_LAYER_KHRONOS_validation";

    const char** sbpcDeviceExts = NULL;
    if(ptDeviceOut->bSwapchainExtPresent)      pl_sb_push(sbpcDeviceExts, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    if(ptDeviceOut->bPortabilitySubsetPresent) pl_sb_push(sbpcDeviceExts, "VK_KHR_portability_subset");
    pl_sb_push(sbpcDeviceExts, VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
    VkDeviceCreateInfo tCreateDeviceInfo = {
        .sType                    = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount     = atQueueCreateInfos[0].queueFamilyIndex == atQueueCreateInfos[1].queueFamilyIndex ? 1 : 2,
        .pQueueCreateInfos        = atQueueCreateInfos,
        .pEnabledFeatures         = &ptDeviceOut->tDeviceFeatures,
        .ppEnabledExtensionNames  = sbpcDeviceExts,
        .enabledLayerCount        = bEnableValidation ? 1 : 0,
        .ppEnabledLayerNames      = bEnableValidation ? &pcValidationLayers : NULL,
        .enabledExtensionCount    = pl_sb_size(sbpcDeviceExts)
    };
    PL_VULKAN(vkCreateDevice(ptDeviceOut->tPhysicalDevice, &tCreateDeviceInfo, NULL, &ptDeviceOut->tLogicalDevice));

    pl_sb_free(sbpcDeviceExts);

    // get device queues
    vkGetDeviceQueue(ptDeviceOut->tLogicalDevice, ptDeviceOut->iGraphicsQueueFamily, 0, &ptDeviceOut->tGraphicsQueue);
    vkGetDeviceQueue(ptDeviceOut->tLogicalDevice, ptDeviceOut->iPresentQueueFamily, 0, &ptDeviceOut->tPresentQueue);
}

static void
pl__create_frame_resources(plGraphics* ptGraphics, plDevice* ptDevice)
{
    const VkSemaphoreCreateInfo tSemaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    const VkFenceCreateInfo tFenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    const VkCommandBufferAllocateInfo tAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = ptGraphics->tCmdPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    pl_sb_reserve(ptGraphics->sbFrames, ptGraphics->uFramesInFlight);
    for(uint32_t i = 0; i < ptGraphics->uFramesInFlight; i++)
    {
        plFrameContext tFrame = {0};
        PL_VULKAN(vkCreateSemaphore(ptDevice->tLogicalDevice, &tSemaphoreInfo, NULL, &tFrame.tImageAvailable));
        PL_VULKAN(vkCreateSemaphore(ptDevice->tLogicalDevice, &tSemaphoreInfo, NULL, &tFrame.tRenderFinish));
        PL_VULKAN(vkCreateFence(ptDevice->tLogicalDevice, &tFenceInfo, NULL, &tFrame.tInFlight));
        PL_VULKAN(vkAllocateCommandBuffers(ptDevice->tLogicalDevice, &tAllocInfo, &tFrame.tCmdBuf));  
        pl_sb_push(ptGraphics->sbFrames, tFrame);
    }
}

static void
pl__create_framebuffers(plDevice* ptDevice, VkRenderPass tRenderPass, plSwapchain* ptSwapchain)
{
    for(uint32_t i = 0; i < ptSwapchain->uImageCount; i++)
    {
        VkImageView atAttachments[] = {
            ptSwapchain->tColorImageView,
            ptSwapchain->tDepthImageView,
            ptSwapchain->ptImageViews[i]
        };
        VkFramebufferCreateInfo tFrameBufferInfo = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = tRenderPass,
            .attachmentCount = 3u,
            .pAttachments    = atAttachments,
            .width           = ptSwapchain->tExtent.width,
            .height          = ptSwapchain->tExtent.height,
            .layers          = 1u,
        };
        PL_VULKAN(vkCreateFramebuffer(ptDevice->tLogicalDevice, &tFrameBufferInfo, NULL, &ptSwapchain->ptFrameBuffers[i]));
    }
}

static void
pl__create_swapchain(plGraphics* ptGraphics, VkSurfaceKHR tSurface, uint32_t uWidth, uint32_t uHeight, plSwapchain* ptSwapchainOut)
{
    plDevice* ptDevice = &ptGraphics->tDevice;
    vkDeviceWaitIdle(ptDevice->tLogicalDevice);

    ptSwapchainOut->tMsaaSamples = ptGraphics->ptDeviceApi->get_max_sample_count(ptDevice);

    //-----------------------------------------------------------------------------
    // query swapchain support
    //----------------------------------------------------------------------------- 

    VkSurfaceCapabilitiesKHR tCapabilities = {0};
    PL_VULKAN(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ptDevice->tPhysicalDevice, tSurface, &tCapabilities));

    uint32_t uFormatCount = 0u;
    PL_VULKAN(vkGetPhysicalDeviceSurfaceFormatsKHR(ptDevice->tPhysicalDevice, tSurface, &uFormatCount, NULL));
    
    if(uFormatCount >ptSwapchainOut->uSurfaceFormatCapacity_)
    {
        if(ptSwapchainOut->ptSurfaceFormats_) ptGraphics->ptMemoryApi->free(ptSwapchainOut->ptSurfaceFormats_);

        ptSwapchainOut->ptSurfaceFormats_ = ptGraphics->ptMemoryApi->alloc(sizeof(VkSurfaceFormatKHR) * uFormatCount);
        ptSwapchainOut->uSurfaceFormatCapacity_ = uFormatCount;
    }

    VkBool32 tPresentSupport = false;
    PL_VULKAN(vkGetPhysicalDeviceSurfaceSupportKHR(ptDevice->tPhysicalDevice, 0, tSurface, &tPresentSupport));
    PL_ASSERT(uFormatCount > 0);
    PL_VULKAN(vkGetPhysicalDeviceSurfaceFormatsKHR(ptDevice->tPhysicalDevice, tSurface, &uFormatCount, ptSwapchainOut->ptSurfaceFormats_));

    uint32_t uPresentModeCount = 0u;
    PL_VULKAN(vkGetPhysicalDeviceSurfacePresentModesKHR(ptDevice->tPhysicalDevice, tSurface, &uPresentModeCount, NULL));
    PL_ASSERT(uPresentModeCount > 0 && uPresentModeCount < 16);

    VkPresentModeKHR atPresentModes[16] = {0};
    PL_VULKAN(vkGetPhysicalDeviceSurfacePresentModesKHR(ptDevice->tPhysicalDevice, tSurface, &uPresentModeCount, atPresentModes));

    // choose swap tSurface Format
    static VkFormat atSurfaceFormatPreference[4] = 
    {
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_B8G8R8A8_SRGB
    };

    bool bPreferenceFound = false;
    VkSurfaceFormatKHR tSurfaceFormat = ptSwapchainOut->ptSurfaceFormats_[0];

    for(uint32_t i = 0u; i < 4; i++)
    {
        if(bPreferenceFound) break;
        
        for(uint32_t j = 0u; j < uFormatCount; j++)
        {
            if(ptSwapchainOut->ptSurfaceFormats_[j].format == atSurfaceFormatPreference[i] && ptSwapchainOut->ptSurfaceFormats_[j].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                tSurfaceFormat = ptSwapchainOut->ptSurfaceFormats_[j];
                ptSwapchainOut->tFormat = tSurfaceFormat.format;
                bPreferenceFound = true;
                break;
            }
        }
    }
    PL_ASSERT(bPreferenceFound && "no preferred surface format found");

    // chose swap present mode
    VkPresentModeKHR tPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    if(!ptSwapchainOut->bVSync)
    {
        for(uint32_t i = 0 ; i < uPresentModeCount; i++)
        {
			if (atPresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				tPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
				break;
			}
			if (atPresentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
				tPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
    }

    // chose swap extent 
    VkExtent2D tExtent = {0};
    if(tCapabilities.currentExtent.width != UINT32_MAX)
        tExtent = tCapabilities.currentExtent;
    else
    {
        tExtent.width = pl__get_u32_max(tCapabilities.minImageExtent.width, pl__get_u32_min(tCapabilities.maxImageExtent.width, uWidth));
        tExtent.height = pl__get_u32_max(tCapabilities.minImageExtent.height, pl__get_u32_min(tCapabilities.maxImageExtent.height, uHeight));
    }
    ptSwapchainOut->tExtent = tExtent;

    // decide image count
    const uint32_t uOldImageCount = ptSwapchainOut->uImageCount;
    uint32_t uDesiredMinImageCount = tCapabilities.minImageCount + 1;
    if(tCapabilities.maxImageCount > 0 && uDesiredMinImageCount > tCapabilities.maxImageCount) 
        uDesiredMinImageCount = tCapabilities.maxImageCount;

	// find the transformation of the surface
	VkSurfaceTransformFlagsKHR tPreTransform;
	if (tCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
	{
		// We prefer a non-rotated transform
		tPreTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}
	else
	{
		tPreTransform = tCapabilities.currentTransform;
	}

	// find a supported composite alpha format (not all devices support alpha opaque)
	VkCompositeAlphaFlagBitsKHR tCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	
    // simply select the first composite alpha format available
	static const VkCompositeAlphaFlagBitsKHR atCompositeAlphaFlags[] = {
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
	};

	for (int i = 0; i < 4; i++)
    {
		if (tCapabilities.supportedCompositeAlpha & atCompositeAlphaFlags[i]) 
        {
			tCompositeAlpha = atCompositeAlphaFlags[i];
			break;
		};
	}

    VkSwapchainCreateInfoKHR tCreateSwapchainInfo = {
        .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface          = tSurface,
        .minImageCount    = uDesiredMinImageCount,
        .imageFormat      = tSurfaceFormat.format,
        .imageColorSpace  = tSurfaceFormat.colorSpace,
        .imageExtent      = tExtent,
        .imageArrayLayers = 1,
        .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform     = (VkSurfaceTransformFlagBitsKHR)tPreTransform,
        .compositeAlpha   = tCompositeAlpha,
        .presentMode      = tPresentMode,
        .clipped          = VK_TRUE, // setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
        .oldSwapchain     = ptSwapchainOut->tSwapChain, // setting oldSwapChain to the saved handle of the previous swapchain aids in resource reuse and makes sure that we can still present already acquired images
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

	// enable transfer source on swap chain images if supported
	if (tCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
		tCreateSwapchainInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	// enable transfer destination on swap chain images if supported
	if (tCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		tCreateSwapchainInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    uint32_t auQueueFamilyIndices[] = { (uint32_t)ptDevice->iGraphicsQueueFamily, (uint32_t)ptDevice->iPresentQueueFamily};
    if (ptDevice->iGraphicsQueueFamily != ptDevice->iPresentQueueFamily)
    {
        tCreateSwapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        tCreateSwapchainInfo.queueFamilyIndexCount = 2;
        tCreateSwapchainInfo.pQueueFamilyIndices = auQueueFamilyIndices;
    }

    VkSwapchainKHR tOldSwapChain = ptSwapchainOut->tSwapChain;

    PL_VULKAN(vkCreateSwapchainKHR(ptDevice->tLogicalDevice, &tCreateSwapchainInfo, NULL, &ptSwapchainOut->tSwapChain));

    if(tOldSwapChain)
    {
        for (uint32_t i = 0u; i < uOldImageCount; i++)
        {
            vkDestroyImageView(ptDevice->tLogicalDevice, ptSwapchainOut->ptImageViews[i], NULL);
            vkDestroyFramebuffer(ptDevice->tLogicalDevice, ptSwapchainOut->ptFrameBuffers[i], NULL);
        }
        vkDestroySwapchainKHR(ptDevice->tLogicalDevice, tOldSwapChain, NULL);
    }

    PL_VULKAN(vkGetSwapchainImagesKHR(ptDevice->tLogicalDevice, ptSwapchainOut->tSwapChain, &ptSwapchainOut->uImageCount, NULL));
    if(ptSwapchainOut->uImageCount > ptSwapchainOut->uImageCapacity)
    {
        ptSwapchainOut->uImageCapacity = ptSwapchainOut->uImageCount;
        if(ptSwapchainOut->ptImages)       ptGraphics->ptMemoryApi->free(ptSwapchainOut->ptImages);
        if(ptSwapchainOut->ptImageViews)   ptGraphics->ptMemoryApi->free(ptSwapchainOut->ptImageViews);
        if(ptSwapchainOut->ptFrameBuffers) ptGraphics->ptMemoryApi->free(ptSwapchainOut->ptFrameBuffers);
        ptSwapchainOut->ptImages         = ptGraphics->ptMemoryApi->alloc(sizeof(VkImage)*ptSwapchainOut->uImageCapacity);
        ptSwapchainOut->ptImageViews     = ptGraphics->ptMemoryApi->alloc(sizeof(VkImageView)*ptSwapchainOut->uImageCapacity);
        ptSwapchainOut->ptFrameBuffers   = ptGraphics->ptMemoryApi->alloc(sizeof(VkFramebuffer)*ptSwapchainOut->uImageCapacity);
    }
    PL_VULKAN(vkGetSwapchainImagesKHR(ptDevice->tLogicalDevice, ptSwapchainOut->tSwapChain, &ptSwapchainOut->uImageCount, ptSwapchainOut->ptImages));

    for(uint32_t i = 0; i < ptSwapchainOut->uImageCount; i++)
    {

        VkImageViewCreateInfo tViewInfo = {
            .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image            = ptSwapchainOut->ptImages[i],
            .viewType         = VK_IMAGE_VIEW_TYPE_2D,
            .format           = ptSwapchainOut->tFormat,
            .subresourceRange = {
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT
            },
            .components = {
                        VK_COMPONENT_SWIZZLE_R,
                        VK_COMPONENT_SWIZZLE_G,
                        VK_COMPONENT_SWIZZLE_B,
                        VK_COMPONENT_SWIZZLE_A
                    }
        };

        PL_VULKAN(vkCreateImageView(ptDevice->tLogicalDevice, &tViewInfo, NULL, &ptSwapchainOut->ptImageViews[i]));   
    }  //-V1020

    // color & depth
    if(ptSwapchainOut->tColorImageView) vkDestroyImageView(ptDevice->tLogicalDevice, ptSwapchainOut->tColorImageView, NULL);
    if(ptSwapchainOut->tColorImage)     vkDestroyImage(ptDevice->tLogicalDevice, ptSwapchainOut->tColorImage, NULL);
    if(ptSwapchainOut->tColorMemory)    vkFreeMemory(ptDevice->tLogicalDevice, ptSwapchainOut->tColorMemory, NULL);
    ptSwapchainOut->tColorImageView = VK_NULL_HANDLE;
    ptSwapchainOut->tColorImage     = VK_NULL_HANDLE;
    ptSwapchainOut->tColorMemory    = VK_NULL_HANDLE;
    if(ptSwapchainOut->tDepthImageView) vkDestroyImageView(ptDevice->tLogicalDevice, ptSwapchainOut->tDepthImageView, NULL);
    if(ptSwapchainOut->tDepthImage)     vkDestroyImage(ptDevice->tLogicalDevice, ptSwapchainOut->tDepthImage, NULL);
    if(ptSwapchainOut->tDepthMemory)    vkFreeMemory(ptDevice->tLogicalDevice, ptSwapchainOut->tDepthMemory, NULL);
    ptSwapchainOut->tDepthImageView = VK_NULL_HANDLE;
    ptSwapchainOut->tDepthImage     = VK_NULL_HANDLE;
    ptSwapchainOut->tDepthMemory    = VK_NULL_HANDLE;

    VkImageCreateInfo tDepthImageInfo = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .extent        = 
        {
            .width  = ptSwapchainOut->tExtent.width,
            .height = ptSwapchainOut->tExtent.height,
            .depth  = 1
        },
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .format        = ptGraphics->ptDeviceApi->find_depth_stencil_format(ptDevice),
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage         = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .samples       = ptSwapchainOut->tMsaaSamples,
        .flags         = 0
    };

    VkImageCreateInfo tColorImageInfo = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .extent        = 
        {
            .width  = ptSwapchainOut->tExtent.width,
            .height = ptSwapchainOut->tExtent.height,
            .depth  = 1
        },
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .format        = ptSwapchainOut->tFormat,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .samples       = ptSwapchainOut->tMsaaSamples,
        .flags         = 0
    };

    PL_VULKAN(vkCreateImage(ptDevice->tLogicalDevice, &tDepthImageInfo, NULL, &ptSwapchainOut->tDepthImage));
    PL_VULKAN(vkCreateImage(ptDevice->tLogicalDevice, &tColorImageInfo, NULL, &ptSwapchainOut->tColorImage));

    VkMemoryRequirements tDepthMemReqs = {0};
    VkMemoryRequirements tColorMemReqs = {0};
    vkGetImageMemoryRequirements(ptDevice->tLogicalDevice, ptSwapchainOut->tDepthImage, &tDepthMemReqs);
    vkGetImageMemoryRequirements(ptDevice->tLogicalDevice, ptSwapchainOut->tColorImage, &tColorMemReqs);

    VkMemoryAllocateInfo tDepthAllocInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = tDepthMemReqs.size,
        .memoryTypeIndex = pl_find_memory_type(ptDevice->tMemProps, tDepthMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };

    VkMemoryAllocateInfo tColorAllocInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = tColorMemReqs.size,
        .memoryTypeIndex = pl_find_memory_type(ptDevice->tMemProps, tColorMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };

    PL_VULKAN(vkAllocateMemory(ptDevice->tLogicalDevice, &tDepthAllocInfo, NULL, &ptSwapchainOut->tDepthMemory));
    PL_VULKAN(vkAllocateMemory(ptDevice->tLogicalDevice, &tColorAllocInfo, NULL, &ptSwapchainOut->tColorMemory));
    PL_VULKAN(vkBindImageMemory(ptDevice->tLogicalDevice, ptSwapchainOut->tDepthImage, ptSwapchainOut->tDepthMemory, 0));
    PL_VULKAN(vkBindImageMemory(ptDevice->tLogicalDevice, ptSwapchainOut->tColorImage, ptSwapchainOut->tColorMemory, 0));

    VkCommandBuffer tCommandBuffer = pl_begin_command_buffer(ptGraphics);
    VkImageSubresourceRange tRange = {
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1,
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT
    };

    pl_transition_image_layout(tCommandBuffer, ptSwapchainOut->tColorImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, tRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    tRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    pl_transition_image_layout(tCommandBuffer, ptSwapchainOut->tDepthImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, tRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    pl_submit_command_buffer(ptGraphics, tCommandBuffer);

    VkImageViewCreateInfo tDepthViewInfo = {
        .sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image                           = ptSwapchainOut->tDepthImage,
        .viewType                        = VK_IMAGE_VIEW_TYPE_2D,
        .format                          = tDepthImageInfo.format,
        .subresourceRange.baseMipLevel   = 0,
        .subresourceRange.levelCount     = 1,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount     = 1,
        .subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT,
    };

    if(ptGraphics->ptDeviceApi->format_has_stencil(tDepthViewInfo.format))
        tDepthViewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

    VkImageViewCreateInfo tColorViewInfo = {
        .sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image                           = ptSwapchainOut->tColorImage,
        .viewType                        = VK_IMAGE_VIEW_TYPE_2D,
        .format                          = tColorImageInfo.format,
        .subresourceRange.baseMipLevel   = 0,
        .subresourceRange.levelCount     = 1,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount     = 1,
        .subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
    };

    PL_VULKAN(vkCreateImageView(ptDevice->tLogicalDevice, &tDepthViewInfo, NULL, &ptSwapchainOut->tDepthImageView));
    PL_VULKAN(vkCreateImageView(ptDevice->tLogicalDevice, &tColorViewInfo, NULL, &ptSwapchainOut->tColorImageView));
}

static void
pl__create_resource_manager(plApiRegistryApiI* ptApiRegistry, plGraphics* ptGraphics, plDevice* ptDevice, plResourceManager* ptResourceManagerOut)
{
    ptResourceManagerOut->_ptGraphics = ptGraphics;
    ptResourceManagerOut->_ptDevice = ptDevice;
    ptResourceManagerOut->_uDynamicBufferSize = pl_minu(ptGraphics->tDevice.tDeviceProps.limits.maxUniformBufferRange, 65536);
    ptResourceManagerOut->_ptDescriptorApi = ptApiRegistry->first(PL_API_DESCRIPTOR_MANAGER);
    ptResourceManagerOut->_tDescriptorManager.tDevice = ptDevice->tLogicalDevice;

    const plDynamicBufferNode tDummyNode0 = {0, 0};
    const plDynamicBufferNode tDummyNode1 = {1, 1};
    pl_sb_push(ptResourceManagerOut->_sbtDynamicBufferList, tDummyNode0);
    pl_sb_push(ptResourceManagerOut->_sbtDynamicBufferList, tDummyNode1);
}

static void
pl__cleanup_resource_manager(plResourceManager* ptResourceManager)
{
    pl__staging_buffer_realloc(ptResourceManager, 0); // free staging buffer

    ptResourceManager->_ptDescriptorApi->cleanup(&ptResourceManager->_tDescriptorManager);

    for(uint32_t i = 0; i < pl_sb_size(ptResourceManager->sbtBuffers); i++)
    {
        if(ptResourceManager->sbtBuffers[i].szSize > 0)
            pl_submit_buffer_for_deletion(ptResourceManager, i);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptResourceManager->sbtTextures); i++)
    {
        if(ptResourceManager->sbtTextures[i].tDesc.uMips > 0)
            pl_submit_texture_for_deletion(ptResourceManager, i);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptResourceManager->sbtTextureViews); i++)
    {
        if(ptResourceManager->sbtTextureViews[i].tTextureViewDesc.uLayerCount > 0)
            pl_submit_texture_view_for_deletion(ptResourceManager, i);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptResourceManager->sbtShaders); i++)
    {
        if(ptResourceManager->_sbulShaderHashes[i] > 0)
            pl_submit_shader_for_deletion(ptResourceManager, i);
    }

    pl_process_cleanup_queue(ptResourceManager, 100); // free deletion queued resources

    pl_sb_free(ptResourceManager->sbtBuffers);
    pl_sb_free(ptResourceManager->sbtTextures);
    pl_sb_free(ptResourceManager->sbtShaders);
    pl_sb_free(ptResourceManager->_sbulTextureFreeIndices);
    pl_sb_free(ptResourceManager->_sbulBufferFreeIndices);
    pl_sb_free(ptResourceManager->_sbulShaderFreeIndices);
    pl_sb_free(ptResourceManager->_sbulBufferDeletionQueue);
    pl_sb_free(ptResourceManager->_sbulTextureDeletionQueue);
    pl_sb_free(ptResourceManager->_sbulShaderDeletionQueue);
    pl_sb_free(ptResourceManager->_sbulTempQueue);


    ptResourceManager->_ptDevice = NULL;
    ptResourceManager->_ptGraphics = NULL;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
pl__debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT tMsgSeverity, VkDebugUtilsMessageTypeFlagsEXT tMsgType, const VkDebugUtilsMessengerCallbackDataEXT* ptCallbackData, void* pUserData) 
{
    if(tMsgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        printf("error validation layer: %s\n", ptCallbackData->pMessage);
    }

    else if(tMsgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        printf("warn validation layer: %s\n", ptCallbackData->pMessage);
    }

    // else if(tMsgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    // {
    //     printf("info validation layer: %s\n", ptCallbackData->pMessage);
    // }
    // else if(tMsgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
    // {
    //     printf("trace validation layer: %s\n", ptCallbackData->pMessage);
    // }
    
    return VK_FALSE;
}

static int
pl__select_physical_device(VkInstance tInstance, plDevice* ptDeviceOut)
{
    uint32_t uDeviceCount = 0u;
    int iBestDvcIdx = 0;
    bool bDiscreteGPUFound = false;
    VkDeviceSize tMaxLocalMemorySize = 0u;

    PL_VULKAN(vkEnumeratePhysicalDevices(tInstance, &uDeviceCount, NULL));
    PL_ASSERT(uDeviceCount > 0 && "failed to find GPUs with Vulkan support!");

    // check if device is suitable
    VkPhysicalDevice atDevices[16] = {0};
    PL_VULKAN(vkEnumeratePhysicalDevices(tInstance, &uDeviceCount, atDevices));

    // prefer discrete, then memory size
    for(uint32_t i = 0; i < uDeviceCount; i++)
    {
        vkGetPhysicalDeviceProperties(atDevices[i], &ptDeviceOut->tDeviceProps);
        vkGetPhysicalDeviceMemoryProperties(atDevices[i], &ptDeviceOut->tMemProps);

        for(uint32_t j = 0; j < ptDeviceOut->tMemProps.memoryHeapCount; j++)
        {
            if(ptDeviceOut->tMemProps.memoryHeaps[j].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT && ptDeviceOut->tMemProps.memoryHeaps[j].size > tMaxLocalMemorySize && !bDiscreteGPUFound)
            {
                tMaxLocalMemorySize = ptDeviceOut->tMemProps.memoryHeaps[j].size;
                iBestDvcIdx = i;
            }
        }

        if(ptDeviceOut->tDeviceProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && !bDiscreteGPUFound)
        {
            iBestDvcIdx = i;
            bDiscreteGPUFound = true;
        }
    }

    ptDeviceOut->tPhysicalDevice = atDevices[iBestDvcIdx];

    PL_ASSERT(ptDeviceOut->tPhysicalDevice != VK_NULL_HANDLE && "failed to find a suitable GPU!");
    vkGetPhysicalDeviceProperties(atDevices[iBestDvcIdx], &ptDeviceOut->tDeviceProps);
    vkGetPhysicalDeviceMemoryProperties(atDevices[iBestDvcIdx], &ptDeviceOut->tMemProps);
    static const char* pacDeviceTypeName[] = {"Other", "Integrated", "Discrete", "Virtual", "CPU"};

    // print info on chosen device
    pl_log_debug_f("Device ID: %u", ptDeviceOut->tDeviceProps.deviceID);
    pl_log_debug_f("Vendor ID: %u", ptDeviceOut->tDeviceProps.vendorID);
    pl_log_debug_f("API Version: %u", ptDeviceOut->tDeviceProps.apiVersion);
    pl_log_debug_f("Driver Version: %u", ptDeviceOut->tDeviceProps.driverVersion);
    pl_log_debug_f("Device Type: %s", pacDeviceTypeName[ptDeviceOut->tDeviceProps.deviceType]);
    pl_log_debug_f("Device Name: %s", ptDeviceOut->tDeviceProps.deviceName);

    uint32_t uExtensionCount = 0;
    vkEnumerateDeviceExtensionProperties(ptDeviceOut->tPhysicalDevice, NULL, &uExtensionCount, NULL);
    VkExtensionProperties* ptExtensions = malloc(uExtensionCount * sizeof(VkExtensionProperties));
    vkEnumerateDeviceExtensionProperties(ptDeviceOut->tPhysicalDevice, NULL, &uExtensionCount, ptExtensions);

    for(uint32_t i = 0; i < uExtensionCount; i++)
    {
        if(pl_str_equal(ptExtensions[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) ptDeviceOut->bSwapchainExtPresent = true; //-V522
        if(pl_str_equal(ptExtensions[i].extensionName, "VK_KHR_portability_subset"))     ptDeviceOut->bPortabilitySubsetPresent = true; //-V522
    }

    free(ptExtensions);
    return iBestDvcIdx;
}

static inline void
pl__staging_buffer_may_grow(plResourceManager* ptResourceManager, size_t szSize)
{
    if(ptResourceManager->_szStagingBufferSize < szSize)
        pl__staging_buffer_realloc(ptResourceManager, szSize * 2);
}

static void
pl__staging_buffer_realloc(plResourceManager* ptResourceManager, size_t szNewSize)
{

    const VkDevice tDevice = ptResourceManager->_ptDevice->tLogicalDevice;

    // unmap host visible address
    if(ptResourceManager->_pucMapping && szNewSize != ptResourceManager->_szStagingBufferSize)
    {
        vkUnmapMemory(tDevice, ptResourceManager->_tStagingBufferMemory);
        ptResourceManager->_pucMapping = NULL;
    }

    if(szNewSize == 0) // free
    {
        if(ptResourceManager->_tStagingBuffer)       vkDestroyBuffer(tDevice, ptResourceManager->_tStagingBuffer, NULL);
        if(ptResourceManager->_tStagingBufferMemory) vkFreeMemory(tDevice, ptResourceManager->_tStagingBufferMemory, NULL);

        ptResourceManager->_tStagingBuffer       = VK_NULL_HANDLE;
        ptResourceManager->_tStagingBufferMemory = VK_NULL_HANDLE;
        ptResourceManager->_szStagingBufferSize  = 0;
    }
    else if(szNewSize != ptResourceManager->_szStagingBufferSize)
    {
        // free old buffer if needed
        if(ptResourceManager->_tStagingBuffer)       vkDestroyBuffer(tDevice, ptResourceManager->_tStagingBuffer, NULL);
        if(ptResourceManager->_tStagingBufferMemory) vkFreeMemory(tDevice, ptResourceManager->_tStagingBufferMemory, NULL);

        ptResourceManager->_tStagingBuffer       = VK_NULL_HANDLE;
        ptResourceManager->_tStagingBufferMemory = VK_NULL_HANDLE;

        // create buffer
        const VkBufferCreateInfo tBufferCreateInfo = {
            .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size        = szNewSize,
            .usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };
        PL_VULKAN(vkCreateBuffer(tDevice, &tBufferCreateInfo, NULL, &ptResourceManager->_tStagingBuffer));

        // find memory requirements
        VkMemoryRequirements tMemoryRequirements = {0};
        vkGetBufferMemoryRequirements(tDevice, ptResourceManager->_tStagingBuffer, &tMemoryRequirements);
        ptResourceManager->_szStagingBufferSize = szNewSize;

        // allocate & bind buffer
        VkMemoryAllocateInfo tAllocInfo = {
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize  = tMemoryRequirements.size,
            .memoryTypeIndex = pl_find_memory_type(ptResourceManager->_ptDevice->tMemProps, tMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
        };
        PL_VULKAN(vkAllocateMemory(tDevice, &tAllocInfo, NULL, &ptResourceManager->_tStagingBufferMemory));
        PL_VULKAN(vkBindBufferMemory(tDevice, ptResourceManager->_tStagingBuffer, ptResourceManager->_tStagingBufferMemory, 0));   

        // map memory to host visible address
        PL_VULKAN(vkMapMemory(tDevice, ptResourceManager->_tStagingBufferMemory, 0, VK_WHOLE_SIZE, 0, (void**)&ptResourceManager->_pucMapping));
    }   
}

static bool
pl__get_free_resource_index(uint32_t* sbuFreeIndices, uint32_t* puIndexOut)
{
    // check if previous index is availble
    if(pl_sb_size(sbuFreeIndices) > 0)
    {
        const uint32_t uFreeIndex = pl_sb_pop(sbuFreeIndices);
        *puIndexOut = uFreeIndex;
        return true;
    }
    return false;    
}

static size_t
pl__get_const_buffer_req_size(plDevice* ptDevice, size_t szSize)
{
    // Calculate required alignment based on minimum device offset alignment
    size_t minUboAlignment = ptDevice->tDeviceProps.limits.minUniformBufferOffsetAlignment;
    size_t alignedSize = szSize;
    if (minUboAlignment > 0) alignedSize = alignedSize + (minUboAlignment - alignedSize % minUboAlignment);
    return alignedSize; 
}

static VkPipelineColorBlendAttachmentState
pl__get_blend_state(plBlendMode tBlendMode)
{

    static const VkPipelineColorBlendAttachmentState atStateMap[PL_BLEND_MODE_COUNT] =
    {
        // PL_BLEND_MODE_NONE
        { 
            .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            .blendEnable         = VK_FALSE,
        },

        // PL_BLEND_MODE_ALPHA
        {
            .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            .blendEnable         = VK_TRUE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .colorBlendOp        = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .alphaBlendOp        = VK_BLEND_OP_ADD
        },

        // PL_BLEND_MODE_ADDITIVE
        {
            .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            .blendEnable         = VK_TRUE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
            .colorBlendOp        = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .alphaBlendOp        = VK_BLEND_OP_ADD
        },

        // PL_BLEND_MODE_PREMULTIPLY
        {
            .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            .blendEnable         = VK_TRUE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .colorBlendOp        = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .alphaBlendOp        = VK_BLEND_OP_ADD
        },

        // PL_BLEND_MODE_MULTIPLY
        {
            .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            .blendEnable         = VK_TRUE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .colorBlendOp        = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .alphaBlendOp        = VK_BLEND_OP_ADD
        },

        // PL_BLEND_MODE_CLIP_MASK
        {
            .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            .blendEnable         = VK_TRUE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_ZERO,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .colorBlendOp        = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp        = VK_BLEND_OP_ADD
        }
    };

    PL_ASSERT(tBlendMode < PL_BLEND_MODE_COUNT && "blend mode out of range");
    return atStateMap[tBlendMode];
}

static VkPipeline
pl__create_shader_pipeline(plResourceManager* ptResourceManager, plGraphicsState tVariant, plVariantInfo* ptInfo)
{
    plGraphics* ptGraphics = ptResourceManager->_ptGraphics;
    plDevice* ptDevice = &ptGraphics->tDevice;
    VkDevice tLogicalDevice = ptGraphics->tDevice.tLogicalDevice;

    VkPipeline tPipeline = VK_NULL_HANDLE;

    //---------------------------------------------------------------------
    // input assembler stage
    //---------------------------------------------------------------------
    VkPipelineInputAssemblyStateCreateInfo tInputAssembly = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    const VkVertexInputAttributeDescription tAttributeDescription =
    {
        .binding  = 0,
        .format   = VK_FORMAT_R32G32B32_SFLOAT,
        .location = 0,
        .offset   = 0
    };

    const VkVertexInputBindingDescription tBindingDescription =
    {
        .binding   = 0,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        .stride    = sizeof(float) * 3
    };

    const VkPipelineVertexInputStateCreateInfo tVertexInputInfo = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 1,
        .vertexAttributeDescriptionCount = 1,
        .pVertexBindingDescriptions      = &tBindingDescription,
        .pVertexAttributeDescriptions    = &tAttributeDescription
    };

    //---------------------------------------------------------------------
    // vertex shader stage
    //---------------------------------------------------------------------
    VkPipelineShaderStageCreateInfo tVertexShaderStageInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .pName = "main",
        .pSpecializationInfo = &ptInfo->tSpecializationInfo
    };
    PL_VULKAN(vkCreateShaderModule(tLogicalDevice, &ptInfo->tVertexShaderInfo, NULL, &tVertexShaderStageInfo.module));

    //---------------------------------------------------------------------
    // pixel shader stage
    //---------------------------------------------------------------------

    VkPipelineShaderStageCreateInfo tPixelShaderStageInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pName = "main",
        .pSpecializationInfo = &ptInfo->tSpecializationInfo
    };
    PL_VULKAN(vkCreateShaderModule(tLogicalDevice, &ptInfo->tPixelShaderInfo, NULL, &tPixelShaderStageInfo.module));

    //---------------------------------------------------------------------
    // color blending stage
    //---------------------------------------------------------------------

    const VkPipelineColorBlendAttachmentState tColorBlendAttachment = pl__get_blend_state((plBlendMode)tVariant.ulBlendMode);

    const VkPipelineColorBlendStateCreateInfo tColorBlending = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable   = VK_FALSE,
        .logicOp         = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments    = &tColorBlendAttachment
    };


    const VkPipelineMultisampleStateCreateInfo tMultisampling = {
        .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable  = (bool)ptDevice->tDeviceFeatures.sampleRateShading,
        .minSampleShading     = 0.2f,
        .rasterizationSamples = ptInfo->tMSAASampleCount
    };

    //---------------------------------------------------------------------
    // depth stencil
    //---------------------------------------------------------------------

    VkPipelineDepthStencilStateCreateInfo tDepthStencil = {
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable       = tVariant.ulDepthMode == PL_DEPTH_MODE_ALWAYS ? VK_FALSE : VK_TRUE,
        .depthWriteEnable      = tVariant.ulDepthWriteEnabled ? VK_TRUE : VK_FALSE,
        .depthCompareOp        = tVariant.ulDepthMode,
        .depthBoundsTestEnable = VK_FALSE,
        .maxDepthBounds        = 1.0f,
        .stencilTestEnable     = VK_TRUE,
        .back.compareOp        = (uint32_t)tVariant.ulStencilMode,
        .back.failOp           = (uint32_t)tVariant.ulStencilOpFail,
        .back.depthFailOp      = (uint32_t)tVariant.ulStencilOpDepthFail,
        .back.passOp           = (uint32_t)tVariant.ulStencilOpPass,
        .back.compareMask      = (uint32_t)tVariant.ulStencilRef,
        .back.writeMask        = (uint32_t)tVariant.ulStencilMask,
        .back.reference        = 1
    };
    tDepthStencil.front = tDepthStencil.back;

    //---------------------------------------------------------------------
    // other
    //---------------------------------------------------------------------

    // dynamic (set later)
    const VkViewport tViewport = {0};
    const VkRect2D tScissor    = {0};

    const VkPipelineViewportStateCreateInfo tViewportState = {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports    = &tViewport,
        .scissorCount  = 1,
        .pScissors     = &tScissor,
    };

    const VkPipelineRasterizationStateCreateInfo tRasterizer = {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable        = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode             = VK_POLYGON_MODE_FILL,
        .lineWidth               = 1.0f,
        .cullMode                = (int)tVariant.ulCullMode,
        .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE, // VK_FRONT_FACE_CLOCKWISE
        .depthBiasEnable         = VK_FALSE
    };

    //---------------------------------------------------------------------
    // Create Pipeline
    //---------------------------------------------------------------------
    const VkPipelineShaderStageCreateInfo atShaderStages[] = { 
        tVertexShaderStageInfo, 
        tPixelShaderStageInfo
    };

    VkDynamicState atDynamicStateEnables[] = { 
        VK_DYNAMIC_STATE_VIEWPORT, 
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo tDynamicState = {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2u,
        .pDynamicStates    = atDynamicStateEnables
    };

    VkGraphicsPipelineCreateInfo tPipelineInfo = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount          = 2u,
        .pStages             = atShaderStages,
        .pVertexInputState   = &tVertexInputInfo,
        .pInputAssemblyState = &tInputAssembly,
        .pViewportState      = &tViewportState,
        .pRasterizationState = &tRasterizer,
        .pMultisampleState   = &tMultisampling,
        .pColorBlendState    = &tColorBlending,
        .pDynamicState       = &tDynamicState,
        .layout              = ptInfo->tPipelineLayout,
        .renderPass          = ptInfo->tRenderPass,
        .subpass             = 0u,
        .basePipelineHandle  = VK_NULL_HANDLE,
        .pDepthStencilState  = &tDepthStencil,
    };
    PL_VULKAN(vkCreateGraphicsPipelines(tLogicalDevice, VK_NULL_HANDLE, 1, &tPipelineInfo, NULL, &tPipeline));

    vkDestroyShaderModule(tLogicalDevice, tVertexShaderStageInfo.module, NULL);
    vkDestroyShaderModule(tLogicalDevice, tPixelShaderStageInfo.module, NULL);

    pl_log_debug_to_f(ptGraphics->uLogChannel, "created new shader pipeline");

    return tPipeline;
}

static VkFilter
pl__vulkan_filter(plFilter tFilter)
{
    switch(tFilter)
    {
        case PL_FILTER_UNSPECIFIED:
        case PL_FILTER_NEAREST: return VK_FILTER_NEAREST;
        case PL_FILTER_LINEAR:  return VK_FILTER_LINEAR;
    }

    PL_ASSERT(false && "Unsupported filter mode");
    return VK_FILTER_LINEAR;
}

static VkSamplerAddressMode
pl__vulkan_wrap(plWrapMode tWrap)
{
    switch(tWrap)
    {
        case PL_WRAP_MODE_UNSPECIFIED:
        case PL_WRAP_MODE_WRAP:   return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case PL_WRAP_MODE_CLAMP:  return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case PL_WRAP_MODE_MIRROR: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    }

    PL_ASSERT(false && "Unsupported wrap mode");
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

static VkCompareOp
pl__vulkan_compare(plCompareMode tCompare)
{
    switch(tCompare)
    {
        case PL_COMPARE_MODE_UNSPECIFIED:
        case PL_COMPARE_MODE_NEVER:            return VK_COMPARE_OP_NEVER;
        case PL_COMPARE_MODE_LESS:             return VK_COMPARE_OP_LESS;
        case PL_COMPARE_MODE_EQUAL:            return VK_COMPARE_OP_EQUAL;
        case PL_COMPARE_MODE_LESS_OR_EQUAL:    return VK_COMPARE_OP_LESS_OR_EQUAL;
        case PL_COMPARE_MODE_GREATER:          return VK_COMPARE_OP_GREATER;
        case PL_COMPARE_MODE_NOT_EQUAL:        return VK_COMPARE_OP_NOT_EQUAL;
        case PL_COMPARE_MODE_GREATER_OR_EQUAL: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case PL_COMPARE_MODE_ALWAYS:           return VK_COMPARE_OP_ALWAYS;
    }

    PL_ASSERT(false && "Unsupported compare mode");
    return VK_COMPARE_OP_NEVER;
}

static plFilter
pl__pilotlight_filter(VkFilter tFilter)
{
    switch(tFilter)
    {
        case VK_FILTER_NEAREST: return PL_FILTER_NEAREST;
        case VK_FILTER_LINEAR:  return PL_FILTER_LINEAR;
    }

    PL_ASSERT(false && "Unsupported compare mode");
    return PL_FILTER_NEAREST;
}

static plWrapMode
pl__pilotlight_wrap(VkSamplerAddressMode tWrap)
{
    switch(tWrap)
    {
        case VK_SAMPLER_ADDRESS_MODE_REPEAT:
            return PL_WRAP_MODE_WRAP;

        case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:
            return PL_WRAP_MODE_CLAMP;

        case VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT:
            return PL_WRAP_MODE_MIRROR;
    }

    PL_ASSERT(false && "Unsupported wrap mode");
    return PL_WRAP_MODE_WRAP;
}

static plCompareMode
pl__pilotlight_compare(VkCompareOp tCompare)
{
    switch(tCompare)
    {
        case VK_COMPARE_OP_NEVER:            return PL_COMPARE_MODE_NEVER;
        case VK_COMPARE_OP_LESS:             return PL_COMPARE_MODE_LESS;
        case VK_COMPARE_OP_EQUAL:            return PL_COMPARE_MODE_EQUAL;
        case VK_COMPARE_OP_LESS_OR_EQUAL:    return PL_COMPARE_MODE_LESS_OR_EQUAL;
        case VK_COMPARE_OP_GREATER:          return PL_COMPARE_MODE_GREATER;
        case VK_COMPARE_OP_NOT_EQUAL:        return PL_COMPARE_MODE_NOT_EQUAL;
        case VK_COMPARE_OP_GREATER_OR_EQUAL: return PL_COMPARE_MODE_GREATER_OR_EQUAL;
        case VK_COMPARE_OP_ALWAYS:           return PL_COMPARE_MODE_ALWAYS;
    }

    PL_ASSERT(false && "Unsupported compare mode");
    return PL_COMPARE_MODE_NEVER;
}


static VkFormat
pl_find_supported_format(plDevice* ptDevice, VkFormatFeatureFlags tFlags, const VkFormat* ptFormats, uint32_t uFormatCount)
{
    for(uint32_t i = 0u; i < uFormatCount; i++)
    {
        VkFormatProperties tProps = {0};
        vkGetPhysicalDeviceFormatProperties(ptDevice->tPhysicalDevice, ptFormats[i], &tProps);
        if(tProps.optimalTilingFeatures & tFlags)
            return ptFormats[i];
    }

    PL_ASSERT(false && "no supported format found");
    return VK_FORMAT_UNDEFINED;   
}

static VkFormat
pl_find_depth_format(plDevice* ptDevice)
{
    const VkFormat atFormats[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };
    return pl_find_supported_format(ptDevice, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, atFormats, 3);
}

static VkFormat
pl_find_depth_stencil_format(plDevice* ptDevice)
{
     const VkFormat atFormats[] = {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };
    return pl_find_supported_format(ptDevice, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, atFormats, 2);   
}

static bool
pl_format_has_stencil(VkFormat tFormat)
{
    switch(tFormat)
    {
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT: return true;
        case VK_FORMAT_D32_SFLOAT:
        default: return false;
    }
}

static VkSampleCountFlagBits
pl_get_max_sample_count(plDevice* ptDevice)
{
    VkPhysicalDeviceProperties tPhysicalDeviceProperties = {0};
    vkGetPhysicalDeviceProperties(ptDevice->tPhysicalDevice, &tPhysicalDeviceProperties);

    VkSampleCountFlags tCounts = tPhysicalDeviceProperties.limits.framebufferColorSampleCounts & tPhysicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (tCounts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
    if (tCounts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
    if (tCounts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
    if (tCounts & VK_SAMPLE_COUNT_8_BIT)  { return VK_SAMPLE_COUNT_8_BIT; }
    if (tCounts & VK_SAMPLE_COUNT_4_BIT)  { return VK_SAMPLE_COUNT_4_BIT; }
    if (tCounts & VK_SAMPLE_COUNT_2_BIT)  { return VK_SAMPLE_COUNT_2_BIT; }
    return VK_SAMPLE_COUNT_1_BIT;    
}

static plDeviceMemoryAllocation
pl_allocate_dedicated(struct plDeviceMemoryAllocatorO* ptInst, uint64_t ulSize, uint64_t ulAlignment, const char* pcName)
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
pl_free_dedicated(struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation)
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
pl_allocate_staging_uncached(struct plDeviceMemoryAllocatorO* ptInst, uint64_t ulSize, uint64_t ulAlignment, const char* pcName)
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
pl_free_staging_uncached(struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation)
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
pl_create_device_local_allocator(VkPhysicalDevice tPhysicalDevice, VkDevice tDevice)
{
    plDeviceDedicatedAllocatorData* ptData = malloc(sizeof(plDeviceDedicatedAllocatorData));
    memset(ptData, 0, sizeof(plDeviceDedicatedAllocatorData));

    // create allocator interface
    plDeviceMemoryAllocatorI tAllocatorInterface = 
    {
        .allocate = pl_allocate_dedicated,
        .free     = pl_free_dedicated,
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
pl_create_staging_uncached_allocator(VkPhysicalDevice tPhysicalDevice, VkDevice tDevice)
{

    plDeviceStagedUncachedAllocatorData* ptData = malloc(sizeof(plDeviceStagedUncachedAllocatorData));
    memset(ptData, 0, sizeof(plDeviceStagedUncachedAllocatorData));

    // create allocator interface
    plDeviceMemoryAllocatorI tAllocatorInterface = 
    {
        .allocate = pl_allocate_staging_uncached,
        .free     = pl_free_staging_uncached,
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
pl_cleanup_descriptor_manager(plDescriptorManager* ptManager)
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
pl_request_descriptor_set_layout(plDescriptorManager* ptManager, plBindGroupLayout* ptLayout)
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

static void
pl_setup_graphics(plGraphics* ptGraphics, plApiRegistryApiI* ptApiRegistry)
{
    plDataRegistryApiI* ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_log_context(ptDataRegistry->get_data("log"));
    pl_set_profile_context(ptDataRegistry->get_data("profile"));

    if(ptGraphics->uFramesInFlight == 0)
        ptGraphics->uFramesInFlight = 2;
    ptGraphics->ptResourceApi = ptApiRegistry->first(PL_API_RESOURCE_MANAGER_0);
    ptGraphics->ptMemoryApi = ptApiRegistry->first(PL_API_MEMORY);
    ptGraphics->ptIoInterface = ptApiRegistry->first(PL_API_IO);
    ptGraphics->ptFileApi = ptApiRegistry->first(PL_API_FILE);
    ptGraphics->ptDeviceApi = ptApiRegistry->first(PL_API_DEVICE);
    ptGraphics->uLogChannel = pl_add_log_channel("graphics", PL_CHANNEL_TYPE_CONSOLE | PL_CHANNEL_TYPE_BUFFER);

    // create vulkan instance
    pl__create_instance(ptGraphics, VK_API_VERSION_1_2, true);
    
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~create surface~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    plIOContext* ptIOCtx = ptGraphics->ptIoInterface->get_context();

    #ifdef _WIN32
        const VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .pNext = NULL,
            .flags = 0,
            .hinstance = GetModuleHandle(NULL),
            .hwnd = *(HWND*)ptIOCtx->pBackendPlatformData
        };
        PL_VULKAN(vkCreateWin32SurfaceKHR(ptGraphics->tInstance, &surfaceCreateInfo, NULL, &ptGraphics->tSurface));
    #elif defined(__APPLE__)
        const VkMetalSurfaceCreateInfoEXT tSurfaceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
            .pLayer = (CAMetalLayer*)ptIOCtx->pBackendPlatformData
        };
        PL_VULKAN(vkCreateMetalSurfaceEXT(ptGraphics->tInstance, &tSurfaceCreateInfo, NULL, &ptGraphics->tSurface));
    #else // linux
        struct tPlatformData { xcb_connection_t* ptConnection; xcb_window_t tWindow;};
        struct tPlatformData* ptPlatformData = (struct tPlatformData*)ptIOCtx->pBackendPlatformData;
        const VkXcbSurfaceCreateInfoKHR surfaceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
            .pNext = NULL,
            .flags = 0,
            .window = ptPlatformData->tWindow,
            .connection = ptPlatformData->ptConnection
        };
        PL_VULKAN(vkCreateXcbSurfaceKHR(ptGraphics->tInstance, &surfaceCreateInfo, NULL, &ptGraphics->tSurface));
    #endif

    pl_log_trace_to_f(ptGraphics->uLogChannel, "created vulkan surface");

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~devices~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // select physical device & create logical device
    pl_set_current_log_channel(ptGraphics->uLogChannel);
    pl__create_device(ptGraphics->tInstance, ptGraphics->tSurface, &ptGraphics->tDevice, true);
    plDevice* ptDevice = &ptGraphics->tDevice;
    VkDevice tLogicalDevice = ptDevice->tLogicalDevice;
    pl_set_current_log_channel(0);

    plDeviceMemoryApiI* ptDeviceMemoryApi = ptApiRegistry->first(PL_API_DEVICE_MEMORY);
    ptGraphics->tLocalAllocator = ptDeviceMemoryApi->create_device_local_allocator(ptDevice->tPhysicalDevice, tLogicalDevice);
    ptGraphics->tStagingUnCachedAllocator = ptDeviceMemoryApi->create_staging_uncached_allocator(ptDevice->tPhysicalDevice, tLogicalDevice);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~debug markers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	ptGraphics->vkDebugMarkerSetObjectTag  = (PFN_vkDebugMarkerSetObjectTagEXT)vkGetDeviceProcAddr(tLogicalDevice, "vkDebugMarkerSetObjectTagEXT");
	ptGraphics->vkDebugMarkerSetObjectName = (PFN_vkDebugMarkerSetObjectNameEXT)vkGetDeviceProcAddr(tLogicalDevice, "vkDebugMarkerSetObjectNameEXT");
	ptGraphics->vkCmdDebugMarkerBegin      = (PFN_vkCmdDebugMarkerBeginEXT)vkGetDeviceProcAddr(tLogicalDevice, "vkCmdDebugMarkerBeginEXT");
	ptGraphics->vkCmdDebugMarkerEnd        = (PFN_vkCmdDebugMarkerEndEXT)vkGetDeviceProcAddr(tLogicalDevice, "vkCmdDebugMarkerEndEXT");
	ptGraphics->vkCmdDebugMarkerInsert     = (PFN_vkCmdDebugMarkerInsertEXT)vkGetDeviceProcAddr(tLogicalDevice, "vkCmdDebugMarkerInsertEXT");
    
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~command pool~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const VkCommandPoolCreateInfo tCommandPoolInfo = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = ptGraphics->tDevice.iGraphicsQueueFamily,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
    };
    PL_VULKAN(vkCreateCommandPool(tLogicalDevice, &tCommandPoolInfo, NULL, &ptGraphics->tCmdPool));

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~swapchain~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    ptGraphics->tSwapchain.bVSync = true;
    pl__create_swapchain(ptGraphics, ptGraphics->tSurface, (uint32_t)ptIOCtx->afMainViewportSize[0], (uint32_t)ptIOCtx->afMainViewportSize[1], &ptGraphics->tSwapchain);
    plSwapchain* ptSwapchain = &ptGraphics->tSwapchain;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~main renderpass~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const VkAttachmentDescription atAttachments[] = {

        // multisampled color attachment (render to this)
        {
            .format         = ptSwapchain->tFormat,
            .samples        = ptSwapchain->tMsaaSamples,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        },

        // depth attachment
        {
            .format         = ptGraphics->ptDeviceApi->find_depth_stencil_format(ptDevice),
            .samples        = ptSwapchain->tMsaaSamples,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        },

        // color resolve attachment
        {
            .format         = ptSwapchain->tFormat,
            .samples        = VK_SAMPLE_COUNT_1_BIT,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        },
    };

    const VkSubpassDependency tSubpassDependencies[] = {

        // color attachment
        { 
            .srcSubpass      = VK_SUBPASS_EXTERNAL,
            .dstSubpass      = 0,
            .srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask   = 0,
            .dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
            .dependencyFlags = 0
        },
        // depth attachment
        {
            .srcSubpass      = VK_SUBPASS_EXTERNAL,
            .dstSubpass      = 0,
            .srcStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .srcAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
            .dependencyFlags = 0
        }
    };

    const VkAttachmentReference atAttachmentReferences[] = {
        {
            .attachment = 0,
            .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        },
        {
            .attachment = 1,
            .layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        },
        {
            .attachment = 2,
            .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL     
        }
    };

    const VkSubpassDescription tSubpass = {
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount    = 1,
        .pColorAttachments       = &atAttachmentReferences[0],
        .pDepthStencilAttachment = &atAttachmentReferences[1],
        .pResolveAttachments     = &atAttachmentReferences[2]
    };

    const VkRenderPassCreateInfo tRenderPassInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 3,
        .pAttachments    = atAttachments,
        .subpassCount    = 1,
        .pSubpasses      = &tSubpass,
        .dependencyCount = 2,
        .pDependencies   = tSubpassDependencies
    };
    PL_VULKAN(vkCreateRenderPass(tLogicalDevice, &tRenderPassInfo, NULL, &ptGraphics->tRenderPass));

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~frame buffers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    pl__create_framebuffers(ptDevice, ptGraphics->tRenderPass, ptSwapchain);
    
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~per-frame resources~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    pl__create_frame_resources(ptGraphics, ptDevice);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~main descriptor pool~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    VkDescriptorPoolSize atPoolSizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER,                100000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          100000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          100000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   100000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   100000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         100000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         100000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       100000 }
    };
    VkDescriptorPoolCreateInfo tDescriptorPoolInfo = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = 100000 * 11,
        .poolSizeCount = 11u,
        .pPoolSizes    = atPoolSizes,
    };
    PL_VULKAN(vkCreateDescriptorPool(tLogicalDevice, &tDescriptorPoolInfo, NULL, &ptGraphics->tDescriptorPool));

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~resource manager~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    pl__create_resource_manager(ptApiRegistry, ptGraphics, ptDevice, &ptGraphics->tResourceManager);
}

static bool
pl_begin_frame(plGraphics* ptGraphics)
{
    plIOContext* ptIOCtx = ptGraphics->ptIoInterface->get_context();
    plDevice* ptDevice = &ptGraphics->tDevice;
    VkDevice tLogicalDevice = ptGraphics->tDevice.tLogicalDevice;

    plFrameContext* ptCurrentFrame = pl_get_frame_resources(ptGraphics);

    PL_VULKAN(vkWaitForFences(tLogicalDevice, 1, &ptCurrentFrame->tInFlight, VK_TRUE, UINT64_MAX));
    VkResult err = vkAcquireNextImageKHR(tLogicalDevice, ptGraphics->tSwapchain.tSwapChain, UINT64_MAX, ptCurrentFrame->tImageAvailable, VK_NULL_HANDLE, &ptGraphics->tSwapchain.uCurrentImageIndex);
    if(err == VK_SUBOPTIMAL_KHR || err == VK_ERROR_OUT_OF_DATE_KHR)
    {
        if(err == VK_ERROR_OUT_OF_DATE_KHR)
        {
            pl__create_swapchain(ptGraphics, ptGraphics->tSurface, (uint32_t)ptIOCtx->afMainViewportSize[0], (uint32_t)ptIOCtx->afMainViewportSize[1], &ptGraphics->tSwapchain);
            pl__create_framebuffers(ptDevice, ptGraphics->tRenderPass, &ptGraphics->tSwapchain);
            return false;
        }
    }
    else
    {
        PL_VULKAN(err);
    }

    if (ptCurrentFrame->tInFlight != VK_NULL_HANDLE)
        PL_VULKAN(vkWaitForFences(tLogicalDevice, 1, &ptCurrentFrame->tInFlight, VK_TRUE, UINT64_MAX));

    return true; 
}

static void
pl_end_frame(plGraphics* ptGraphics)
{
    plFrameContext* ptCurrentFrame = pl_get_frame_resources(ptGraphics);
    plDevice* ptDevice = &ptGraphics->tDevice;
    VkDevice tLogicalDevice = ptGraphics->tDevice.tLogicalDevice;
    
    // submit
    const VkPipelineStageFlags atWaitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    const VkSubmitInfo tSubmitInfo = {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = &ptCurrentFrame->tImageAvailable,
        .pWaitDstStageMask    = atWaitStages,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &ptCurrentFrame->tCmdBuf,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &ptCurrentFrame->tRenderFinish
    };
    PL_VULKAN(vkResetFences(tLogicalDevice, 1, &ptCurrentFrame->tInFlight));
    PL_VULKAN(vkQueueSubmit(ptDevice->tGraphicsQueue, 1, &tSubmitInfo, ptCurrentFrame->tInFlight));          
    
    // present                        
    const VkPresentInfoKHR tPresentInfo = {
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &ptCurrentFrame->tRenderFinish,
        .swapchainCount     = 1,
        .pSwapchains        = &ptGraphics->tSwapchain.tSwapChain,
        .pImageIndices      = &ptGraphics->tSwapchain.uCurrentImageIndex,
    };
    const VkResult tResult = vkQueuePresentKHR(ptDevice->tPresentQueue, &tPresentInfo);
    if(tResult == VK_SUBOPTIMAL_KHR || tResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        plIOContext* ptIOCtx = ptGraphics->ptIoInterface->get_context();
        pl__create_swapchain(ptGraphics, ptGraphics->tSurface, (uint32_t)ptIOCtx->afMainViewportSize[0], (uint32_t)ptIOCtx->afMainViewportSize[1], &ptGraphics->tSwapchain);
        pl__create_framebuffers(ptDevice, ptGraphics->tRenderPass, &ptGraphics->tSwapchain);
    }
    else
    {
        PL_VULKAN(tResult);
    }

    ptGraphics->szCurrentFrameIndex = (ptGraphics->szCurrentFrameIndex + 1) % ptGraphics->uFramesInFlight;
}

static void
pl_resize_graphics(plGraphics* ptGraphics)
{
    plIOContext* ptIOCtx = ptGraphics->ptIoInterface->get_context();
    plDevice* ptDevice = &ptGraphics->tDevice;
    pl__create_swapchain(ptGraphics, ptGraphics->tSurface, (uint32_t)ptIOCtx->afMainViewportSize[0], (uint32_t)ptIOCtx->afMainViewportSize[1], &ptGraphics->tSwapchain);
    pl__create_framebuffers(ptDevice, ptGraphics->tRenderPass, &ptGraphics->tSwapchain);
    ptGraphics->szCurrentFrameIndex = 0;
}

static void
pl_begin_recording(plGraphics* ptGraphics)
{
    const plFrameContext* ptCurrentFrame = pl_get_frame_resources(ptGraphics);

    const VkCommandBufferBeginInfo tBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    };
    vkResetCommandBuffer(ptCurrentFrame->tCmdBuf, 0);
    PL_VULKAN(vkBeginCommandBuffer(ptCurrentFrame->tCmdBuf, &tBeginInfo));    
}

static void
pl_end_recording(plGraphics* ptGraphics)
{
    const plFrameContext* ptCurrentFrame = pl_get_frame_resources(ptGraphics);
    PL_VULKAN(vkEndCommandBuffer(ptCurrentFrame->tCmdBuf));
}

static VkCommandBuffer
pl_begin_command_buffer(plGraphics* ptGraphics)
{
    plDevice* ptDevice = &ptGraphics->tDevice;
    VkDevice tLogicalDevice = ptGraphics->tDevice.tLogicalDevice;
    VkCommandBuffer tCommandBuffer = {0};
    
    const VkCommandBufferAllocateInfo tAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool        = ptGraphics->tCmdPool,
        .commandBufferCount = 1u,
    };
    vkAllocateCommandBuffers(tLogicalDevice, &tAllocInfo, &tCommandBuffer);

    const VkCommandBufferBeginInfo tBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    vkBeginCommandBuffer(tCommandBuffer, &tBeginInfo);

    return tCommandBuffer;  
}

static void
pl_submit_command_buffer(plGraphics* ptGraphics, VkCommandBuffer tCmdBuffer)
{
    plDevice* ptDevice = &ptGraphics->tDevice;
    VkDevice tLogicalDevice = ptGraphics->tDevice.tLogicalDevice;

    vkEndCommandBuffer(tCmdBuffer);
    const VkSubmitInfo tSubmitInfo = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1u,
        .pCommandBuffers    = &tCmdBuffer,
    };

    vkQueueSubmit(ptDevice->tGraphicsQueue, 1, &tSubmitInfo, VK_NULL_HANDLE);
    vkDeviceWaitIdle(ptDevice->tLogicalDevice);
    vkFreeCommandBuffers(ptDevice->tLogicalDevice, ptGraphics->tCmdPool, 1, &tCmdBuffer);
}

static uint32_t
pl_create_shader(plResourceManager* ptResourceManager, const plShaderDesc* ptDescIn)
{
    PL_ASSERT(ptDescIn->uBindGroupLayoutCount < 5 && "only 4 descriptor sets allowed per pipeline.");

    plGraphics* ptGraphics = ptResourceManager->_ptGraphics;
    plDevice*   ptDevice   = &ptGraphics->tDevice;

    plShader tShader = {
        .tDesc = *ptDescIn
    };

    // fill out descriptor set layouts
    for(uint32_t i = 0; i < tShader.tDesc.uBindGroupLayoutCount; i++)
        tShader.tDesc.atBindGroupLayouts[i]._tDescriptorSetLayout = ptResourceManager->_ptDescriptorApi->request_layout(&ptResourceManager->_tDescriptorManager, &tShader.tDesc.atBindGroupLayouts[i]);

    // place "default" graphics state into the variant list (for consolidation)
    const plShaderVariant tNewVariant = {
        .tGraphicsState   = tShader.tDesc.tGraphicsState,
        .tRenderPass      = ptGraphics->tRenderPass,
        .tMSAASampleCount = ptGraphics->tSwapchain.tMsaaSamples
    };
    pl_sb_push(tShader.tDesc.sbtVariants, tNewVariant);

    // hash shader
    uint32_t uHash = pl_str_hash_data(&tShader.tDesc.tGraphicsState.ulValue, sizeof(uint64_t), 0);
    const uint32_t uVariantCount = pl_sb_size(tShader.tDesc.sbtVariants);
    uHash = pl_str_hash_data(&uVariantCount, sizeof(uint32_t), uHash);
    for(uint32_t i = 0; i < tShader.tDesc.uBindGroupLayoutCount; i++)
    {
        uHash += tShader.tDesc.atBindGroupLayouts[i].uTextureCount;
        uHash += tShader.tDesc.atBindGroupLayouts[i].uBufferCount;
    }
    uHash = pl_str_hash(tShader.tDesc.pcPixelShader, 0, uHash);
    uHash = pl_str_hash(tShader.tDesc.pcVertexShader, 0, uHash);

    // TODO: set a max shader count & use a lookup table
    for(uint32_t i = 0; i < pl_sb_size(ptResourceManager->_sbulShaderHashes); i++)
    {
        if(ptResourceManager->_sbulShaderHashes[i] == uHash)
            return i;
    }
    
    VkDescriptorSetLayout atDescriptorSetLayouts[4] = {0};
    for(uint32_t i = 0; i < tShader.tDesc.uBindGroupLayoutCount; i++)
        atDescriptorSetLayouts[i] = tShader.tDesc.atBindGroupLayouts[i]._tDescriptorSetLayout;

    const VkPipelineLayoutCreateInfo tPipelineLayoutInfo = {
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = tShader.tDesc.uBindGroupLayoutCount,
        .pSetLayouts    = atDescriptorSetLayouts
    };
    PL_VULKAN(vkCreatePipelineLayout(ptDevice->tLogicalDevice, &tPipelineLayoutInfo, NULL, &tShader.tPipelineLayout));

    //---------------------------------------------------------------------
    // vertex shader stage
    //---------------------------------------------------------------------

    uint32_t uVertexByteCodeSize = 0;
    ptGraphics->ptFileApi->read(tShader.tDesc.pcVertexShader, &uVertexByteCodeSize, NULL, "rb");
    char* pcVertexByteCode = ptGraphics->ptMemoryApi->alloc(uVertexByteCodeSize);
    ptGraphics->ptFileApi->read(tShader.tDesc.pcVertexShader, &uVertexByteCodeSize, pcVertexByteCode, "rb");

    tShader.tVertexShaderInfo = (VkShaderModuleCreateInfo){
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = uVertexByteCodeSize,
        .pCode    = (const uint32_t*)(pcVertexByteCode)
    };

    //---------------------------------------------------------------------
    // pixel shader stage
    //---------------------------------------------------------------------

    uint32_t uByteCodeSize = 0;
    ptGraphics->ptFileApi->read(tShader.tDesc.pcPixelShader, &uByteCodeSize, NULL, "rb");
    char* pcByteCode = ptGraphics->ptMemoryApi->alloc(uByteCodeSize);
    ptGraphics->ptFileApi->read(tShader.tDesc.pcPixelShader, &uByteCodeSize, pcByteCode, "rb");

    tShader.tPixelShaderInfo = (VkShaderModuleCreateInfo){
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = uByteCodeSize,
        .pCode    = (const uint32_t*)(pcByteCode)
    };

    const VkSpecializationMapEntry tSpecializationEntries[] = 
    {
        {
            .constantID = 0,
            .offset     = 0,
            .size       = sizeof(int)
        },
        {
            .constantID = 1,
            .offset     = sizeof(int),
            .size       = sizeof(int)
        },
        {
            .constantID = 2,
            .offset     = sizeof(int) * 2,
            .size       = sizeof(int)
        },

    };

    // create pipelines for variants
    for(uint32_t i = 0; i < pl_sb_size(tShader.tDesc.sbtVariants); i++)
    {

        int aiData[3] = {
            (int)tShader.tDesc.sbtVariants[i].tGraphicsState.ulVertexStreamMask,
            0,
            (int)tShader.tDesc.sbtVariants[i].tGraphicsState.ulShaderTextureFlags
        };

        int iFlagCopy = (int)tShader.tDesc.sbtVariants[i].tGraphicsState.ulVertexStreamMask;
        while(iFlagCopy)
        {
            aiData[1] += iFlagCopy & 1;
            iFlagCopy >>= 1;
        }

        plVariantInfo tVariantInfo = {
            .tRenderPass         = tShader.tDesc.sbtVariants[i].tRenderPass,
            .tPipelineLayout     = tShader.tPipelineLayout,
            .tPixelShaderInfo    = tShader.tPixelShaderInfo,
            .tVertexShaderInfo   = tShader.tVertexShaderInfo,
            .tMSAASampleCount    = tShader.tDesc.sbtVariants[i].tMSAASampleCount,
            .tSpecializationInfo = {
                .mapEntryCount = 3,
                .pMapEntries   = tSpecializationEntries,
                .dataSize      = sizeof(int) * 3,
                .pData         = aiData
            }
        };

        const plShaderVariant tShaderVariant = {
            .tPipelineLayout = tShader.tPipelineLayout,
            .tPipeline       = pl__create_shader_pipeline(ptResourceManager, tShader.tDesc.sbtVariants[i].tGraphicsState, &tVariantInfo),
            .tGraphicsState  = tShader.tDesc.sbtVariants[i].tGraphicsState,
            .tRenderPass     = tShader.tDesc.sbtVariants[i].tRenderPass,
            .tMSAASampleCount= tShader.tDesc.sbtVariants[i].tMSAASampleCount
        };
        pl_sb_push(ptResourceManager->sbtShaderVariants, tShaderVariant);
        pl_sb_push(tShader._sbuVariantPipelines, pl_sb_size(ptResourceManager->sbtShaderVariants) - 1);
    }

    // find free index
    uint32_t uShaderIndex = 0u;
    if(!pl__get_free_resource_index(ptResourceManager->_sbulShaderFreeIndices, &uShaderIndex))
    {
        uShaderIndex = pl_sb_add_n(ptResourceManager->sbtShaders, 1);
        pl_sb_add_n(ptResourceManager->_sbulShaderHashes, 1);
    }
    ptResourceManager->sbtShaders[uShaderIndex] = tShader;
    ptResourceManager->_sbulShaderHashes[uShaderIndex] = uHash;

    PL_ASSERT(pl_sb_size(ptResourceManager->sbtShaders) == pl_sb_size(ptResourceManager->_sbulShaderHashes));
    
    return uShaderIndex;
}

static uint32_t
pl_add_shader_variant(plResourceManager* ptResourceManager, uint32_t uShader, plGraphicsState tVariant, VkRenderPass ptRenderPass, VkSampleCountFlagBits tMSAASampleCount)
{
    plShader* ptShader = &ptResourceManager->sbtShaders[uShader];

    // check if variant exist already
    for(uint32_t i = 0; i < pl_sb_size(ptShader->tDesc.sbtVariants); i++)
    {
        if(ptShader->tDesc.sbtVariants[i].tGraphicsState.ulValue == tVariant.ulValue 
            && ptRenderPass == ptShader->tDesc.sbtVariants[i].tRenderPass 
            && tMSAASampleCount == ptShader->tDesc.sbtVariants[i].tMSAASampleCount)
            return ptShader->_sbuVariantPipelines[i];
    }
    

    const VkSpecializationMapEntry tSpecializationEntries[] = 
    {
        {
            .constantID = 0,
            .offset     = 0,
            .size       = sizeof(int)
        },
        {
            .constantID = 1,
            .offset     = sizeof(int),
            .size       = sizeof(int)
        },
        {
            .constantID = 2,
            .offset     = sizeof(int) * 2,
            .size       = sizeof(int)
        },

    };

    // create pipeline for variant
    int aiData[3] = {
        (int)tVariant.ulVertexStreamMask,
        0,
        (int)tVariant.ulShaderTextureFlags
    };

    int iFlagCopy = (int)tVariant.ulVertexStreamMask;
    while(iFlagCopy)
    {
        aiData[1] += iFlagCopy & 1;
        iFlagCopy >>= 1;
    }

    plVariantInfo tVariantInfo = {
        .tRenderPass         = ptRenderPass,
        .tPipelineLayout     = ptShader->tPipelineLayout,
        .tPixelShaderInfo    = ptShader->tPixelShaderInfo,
        .tVertexShaderInfo   = ptShader->tVertexShaderInfo,
        .tMSAASampleCount    = tMSAASampleCount,
        .tSpecializationInfo = {
            .mapEntryCount = 3,
            .pMapEntries   = tSpecializationEntries,
            .dataSize      = sizeof(int) * 3,
            .pData         = aiData
        }
    };
    plShaderVariant tShaderVariant = {
        .tPipelineLayout = ptShader->tPipelineLayout,
        .tPipeline       = pl__create_shader_pipeline(ptResourceManager, tVariant, &tVariantInfo),
        .tGraphicsState  = tVariant,
        .tRenderPass = ptRenderPass,
        .tMSAASampleCount = tMSAASampleCount
    };
    pl_sb_push(ptShader->tDesc.sbtVariants, tShaderVariant);
    pl_sb_push(ptResourceManager->sbtShaderVariants, tShaderVariant);
    pl_sb_push(ptShader->_sbuVariantPipelines, pl_sb_size(ptResourceManager->sbtShaderVariants) - 1);
    return pl_sb_size(ptResourceManager->sbtShaderVariants) - 1;
}

static bool
pl_shader_variant_exist(plResourceManager* ptResourceManager, uint32_t uShader, plGraphicsState tVariant, VkRenderPass ptRenderPass, VkSampleCountFlagBits tMSAASampleCount)
{
    plShader* ptShader = &ptResourceManager->sbtShaders[uShader];
    for(uint32_t i = 0; i < pl_sb_size(ptShader->tDesc.sbtVariants); i++)
    {
        if(ptShader->tDesc.sbtVariants[i].tGraphicsState.ulValue == tVariant.ulValue 
            && ptRenderPass == ptShader->tDesc.sbtVariants[i].tRenderPass
            && tMSAASampleCount == ptShader->tDesc.sbtVariants[i].tMSAASampleCount)
            return true;
    }
    return false;
}

static void
pl_submit_shader_for_deletion(plResourceManager* ptResourceManager, uint32_t uShaderIndex)
{

    PL_ASSERT(uShaderIndex < pl_sb_size(ptResourceManager->sbtShaders)); 
    pl_sb_push(ptResourceManager->_sbulShaderDeletionQueue, uShaderIndex);

    // using shader hash to store frame this buffer is ok to free
    ptResourceManager->_sbulShaderHashes[uShaderIndex] = ptResourceManager->_ptGraphics->uFramesInFlight;   
}

static plBindGroupLayout*
pl_get_bind_group_layout(plResourceManager* ptResourceManager, uint32_t uShaderIndex, uint32_t uBindGroupIndex)
{
    PL_ASSERT(uShaderIndex < pl_sb_size(ptResourceManager->sbtShaders)); 
    PL_ASSERT(uBindGroupIndex < ptResourceManager->sbtShaders[uShaderIndex].tDesc.uBindGroupLayoutCount); 
    return &ptResourceManager->sbtShaders[uShaderIndex].tDesc.atBindGroupLayouts[uBindGroupIndex];
}

static plShaderVariant*
pl_get_shader(plResourceManager* ptResourceManager, uint32_t uVariantIndex)
{
    return &ptResourceManager->sbtShaderVariants[uVariantIndex];
}

static void
pl_update_bind_group(plGraphics* ptGraphics, plBindGroup* ptGroup, uint32_t uBufferCount, uint32_t* auBuffers, size_t* aszBufferRanges, uint32_t uTextureViewCount, uint32_t* auTextureViews)
{
    PL_ASSERT(uBufferCount == ptGroup->tLayout.uBufferCount && "bind group buffer count & update buffer count must match.");
    PL_ASSERT(uTextureViewCount == ptGroup->tLayout.uTextureCount && "bind group texture count & update texture view count must match.");

    plDevice* ptDevice = &ptGraphics->tDevice;
    VkDevice tLogicalDevice = ptGraphics->tDevice.tLogicalDevice;
    plDescriptorManager* ptDescManager = &ptGraphics->tResourceManager._tDescriptorManager;
    plDescriptorManagerApiI* ptDescApi = ptGraphics->tResourceManager._ptDescriptorApi;

    // allocate descriptors if not done already
    if(ptGroup->_tDescriptorSet == VK_NULL_HANDLE)
    {
        ptGroup->tLayout._tDescriptorSetLayout = ptDescApi->request_layout(ptDescManager, &ptGroup->tLayout);

        // allocate descriptor sets
        const VkDescriptorSetAllocateInfo tDescriptorSetAllocInfo = {
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool     = ptGraphics->tDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts        = &ptGroup->tLayout._tDescriptorSetLayout
        };
        PL_VULKAN(vkAllocateDescriptorSets(tLogicalDevice, &tDescriptorSetAllocInfo, &ptGroup->_tDescriptorSet));

        pl_log_debug_to_f(ptGraphics->uLogChannel, "allocating new descriptor sets");
    }

    VkWriteDescriptorSet* sbtWrites = NULL;
    VkDescriptorBufferInfo* sbtBufferDescInfos = NULL;
    VkDescriptorImageInfo* sbtImageDescInfos = NULL;
    pl_sb_resize(sbtWrites, uBufferCount + uTextureViewCount);
    pl_sb_resize(sbtBufferDescInfos, uBufferCount);
    pl_sb_resize(sbtImageDescInfos, uTextureViewCount);

    static const VkDescriptorType atDescriptorTypeLUT[] =
    {
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    };

    uint32_t uCurrentWrite = 0;
    for(uint32_t i = 0 ; i < uBufferCount; i++)
    {

        const plBuffer* ptBuffer = &ptGraphics->tResourceManager.sbtBuffers[auBuffers[i]];

        sbtBufferDescInfos[i].buffer = ptBuffer->tBuffer;
        sbtBufferDescInfos[i].offset = 0;
        sbtBufferDescInfos[i].range  = aszBufferRanges[i];

        sbtWrites[uCurrentWrite].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sbtWrites[uCurrentWrite].dstBinding      = ptGroup->tLayout.aBuffers[i].uSlot;
        sbtWrites[uCurrentWrite].dstArrayElement = 0;
        sbtWrites[uCurrentWrite].descriptorType  = atDescriptorTypeLUT[ptGroup->tLayout.aBuffers[i].tType - 1];
        sbtWrites[uCurrentWrite].descriptorCount = 1;
        sbtWrites[uCurrentWrite].dstSet          = ptGroup->_tDescriptorSet;
        sbtWrites[uCurrentWrite].pBufferInfo     = &sbtBufferDescInfos[i];
        sbtWrites[uCurrentWrite].pNext           = NULL;
        uCurrentWrite++;
    }

    for(uint32_t i = 0 ; i < uTextureViewCount; i++)
    {

        const plTextureView* ptTextureView = &ptGraphics->tResourceManager.sbtTextureViews[auTextureViews[i]];

        sbtImageDescInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        sbtImageDescInfos[i].imageView   = ptTextureView->_tImageView;
        sbtImageDescInfos[i].sampler     = ptTextureView->_tSampler;
        
        sbtWrites[uCurrentWrite].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sbtWrites[uCurrentWrite].dstBinding      = ptGroup->tLayout.aTextures[i].uSlot;
        sbtWrites[uCurrentWrite].dstArrayElement = 0;
        sbtWrites[uCurrentWrite].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sbtWrites[uCurrentWrite].descriptorCount = 1;
        sbtWrites[uCurrentWrite].dstSet          = ptGroup->_tDescriptorSet;
        sbtWrites[uCurrentWrite].pImageInfo      = &sbtImageDescInfos[i];
        sbtWrites[uCurrentWrite].pNext           = NULL;
        uCurrentWrite++;
    }
    vkUpdateDescriptorSets(tLogicalDevice, uCurrentWrite, sbtWrites, 0, NULL);
    pl_sb_free(sbtWrites);
    pl_sb_free(sbtBufferDescInfos);
    pl_sb_free(sbtImageDescInfos);

    pl_log_debug_to_f(ptGraphics->uLogChannel, "updating descriptor sets");
}

static void
pl_draw_areas(plGraphics* ptGraphics, uint32_t uAreaCount, plDrawArea* atAreas, plDraw* atDraws)
{
    pl_begin_profile_sample(__FUNCTION__);
    
    const plFrameContext* ptCurrentFrame = pl_get_frame_resources(ptGraphics);
    static VkDeviceSize tOffsets = { 0 };
    vkCmdSetDepthBias(ptCurrentFrame->tCmdBuf, 0.0f, 0.0f, 0.0f);

    plBindGroup* ptCurrentBindGroup0 = NULL;
    plBindGroup* ptCurrentBindGroup1 = NULL;
    plBindGroup* ptCurrentBindGroup2 = NULL;
    uint32_t uCurrentVariant = UINT32_MAX;
    uint32_t uCurrentIndexBuffer = UINT32_MAX;
    uint32_t uCurrentVertexBuffer = UINT32_MAX;

    for(uint32_t i = 0; i < uAreaCount; i++)
    {
        const plDrawArea* ptArea = &atAreas[i];
    
        for(uint32_t j = 0; j < ptArea->uDrawCount; j++)
        {
            plDraw* ptDraw = &atDraws[ptArea->uDrawOffset + j];

            // shader
            if(ptDraw->uShaderVariant != uCurrentVariant)
            {
                plShaderVariant* ptPipeline = pl_get_shader(&ptGraphics->tResourceManager, ptDraw->uShaderVariant);
                vkCmdBindPipeline(ptCurrentFrame->tCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, ptPipeline->tPipeline);
                uCurrentVariant = ptDraw->uShaderVariant;
            }

            plShaderVariant* ptVariant = pl_get_shader(&ptGraphics->tResourceManager, uCurrentVariant);

            // mesh
            if(ptDraw->ptMesh->uIndexBuffer != uCurrentIndexBuffer)
            {
                uCurrentIndexBuffer = ptDraw->ptMesh->uIndexBuffer;
                vkCmdBindIndexBuffer(ptCurrentFrame->tCmdBuf, ptGraphics->tResourceManager.sbtBuffers[ptDraw->ptMesh->uIndexBuffer].tBuffer, 0, VK_INDEX_TYPE_UINT32);
            }

            if(ptDraw->ptMesh->uVertexBuffer != uCurrentVertexBuffer)
            {
                uCurrentVertexBuffer = ptDraw->ptMesh->uVertexBuffer;
                vkCmdBindVertexBuffers(ptCurrentFrame->tCmdBuf, 0, 1, &ptGraphics->tResourceManager.sbtBuffers[ptDraw->ptMesh->uVertexBuffer].tBuffer, &tOffsets);
            }

            // bind group (set 0)
            if(ptArea->ptBindGroup0 && ptArea->ptBindGroup0 != ptCurrentBindGroup0)
            {
                ptCurrentBindGroup0 = ptArea->ptBindGroup0;
                vkCmdBindDescriptorSets(ptCurrentFrame->tCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, ptVariant->tPipelineLayout, 0, 1, &ptCurrentBindGroup0->_tDescriptorSet, 1, &ptArea->uDynamicBufferOffset0); 
            }

            // bind groups (sets 1 & 2)
            VkDescriptorSet atUpdateSets[2] = {0};
            uint32_t auDynamicOffsets[2] = {0};
            uint32_t uSetCounter = 0;
            uint32_t uFirstSet = 0;

            if(ptDraw->ptBindGroup1 && ptDraw->ptBindGroup1 != ptCurrentBindGroup1)
            {
                ptCurrentBindGroup1 = ptDraw->ptBindGroup1;
                auDynamicOffsets[uSetCounter] = ptDraw->uDynamicBufferOffset1;
                atUpdateSets[uSetCounter++] = ptDraw->ptBindGroup1->_tDescriptorSet;
                uFirstSet = 1;
            }

            if(ptDraw->ptBindGroup2 && ptDraw->ptBindGroup2 != ptCurrentBindGroup2)
            {
                ptCurrentBindGroup2 = ptDraw->ptBindGroup1;
                auDynamicOffsets[uSetCounter] = ptDraw->uDynamicBufferOffset2;
                atUpdateSets[uSetCounter++] = ptDraw->ptBindGroup2->_tDescriptorSet;
                uFirstSet = uFirstSet == 0 ? 2 : 1;
            }

            if(uSetCounter > 0)
                vkCmdBindDescriptorSets(ptCurrentFrame->tCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, ptVariant->tPipelineLayout, uFirstSet, uSetCounter, atUpdateSets, uSetCounter, auDynamicOffsets);

            vkCmdDrawIndexed(ptCurrentFrame->tCmdBuf, ptDraw->ptMesh->uIndexCount, 1, 0, ptDraw->ptMesh->uVertexOffset, 0);
        }

    }

    pl_end_profile_sample();
}

static void
pl_process_cleanup_queue(plResourceManager* ptResourceManager, uint32_t uFramesToProcess)
{

    const VkDevice tDevice = ptResourceManager->_ptDevice->tLogicalDevice;
    plGraphics* ptGraphics = ptResourceManager->_ptGraphics;

    for(uint32_t i = 2; i < pl_sb_size(ptResourceManager->_sbuDynamicBufferDeletionQueue); i++)
    {
        uint32_t uNodeToFree = ptResourceManager->_sbuDynamicBufferDeletionQueue[i];
        if(ptResourceManager->_sbtDynamicBufferList[uNodeToFree].uLastActiveFrame == ptGraphics->szCurrentFrameIndex)
        {
            pl_return_dynamic_buffer(ptResourceManager, uNodeToFree);
            pl_sb_del_swap(ptResourceManager->_sbuDynamicBufferDeletionQueue, i);
        }
    }

    // buffer cleanup
    pl_sb_reset(ptResourceManager->_sbulTempQueue);

    bool bNeedUpdate = false;

    for(uint32_t i = 0; i < pl_sb_size(ptResourceManager->_sbulBufferDeletionQueue); i++)
    {
        const uint32_t ulBufferIndex = ptResourceManager->_sbulBufferDeletionQueue[i];

        plBuffer* ptBuffer = &ptResourceManager->sbtBuffers[ulBufferIndex];

        // we are hiding the frame
        if(ptBuffer->szStride < uFramesToProcess)
            ptBuffer->szStride = 0;
        else
            ptBuffer->szStride -= uFramesToProcess;

        if(ptBuffer->szStride == 0)
        {
            if(ptBuffer->pucMapping)
                vkUnmapMemory(tDevice, ptBuffer->tBufferMemory);

            vkDestroyBuffer(tDevice, ptBuffer->tBuffer, NULL);
            vkFreeMemory(tDevice, ptBuffer->tBufferMemory, NULL);

            pl_log_debug_to_f(ptGraphics->uLogChannel, "destroying buffer %u", ulBufferIndex);

            ptBuffer->pucMapping = NULL;
            ptBuffer->tBuffer = VK_NULL_HANDLE;
            ptBuffer->tBufferMemory = VK_NULL_HANDLE;
            ptBuffer->szSize = 0;
            ptBuffer->szRequestedSize = 0;
            ptBuffer->tUsage = PL_BUFFER_USAGE_UNSPECIFIED;

            // add to free indices
            pl_sb_push(ptResourceManager->_sbulBufferFreeIndices, ulBufferIndex);

            bNeedUpdate = true;
        }
        else
        {
            pl_sb_push(ptResourceManager->_sbulTempQueue, ulBufferIndex);
        }
    }

    if(bNeedUpdate)
    {
        // copy temporary queue data over
        pl_sb_reset(ptResourceManager->_sbulBufferDeletionQueue);
        pl_sb_resize(ptResourceManager->_sbulBufferDeletionQueue, pl_sb_size(ptResourceManager->_sbulTempQueue));
        if(ptResourceManager->_sbulTempQueue)
            memcpy(ptResourceManager->_sbulBufferDeletionQueue, ptResourceManager->_sbulTempQueue, pl_sb_size(ptResourceManager->_sbulTempQueue) * sizeof(uint32_t));
    }

    // texture cleanup
    pl_sb_reset(ptResourceManager->_sbulTempQueue);

    bNeedUpdate = false;

    for(uint32_t i = 0; i < pl_sb_size(ptResourceManager->_sbulTextureDeletionQueue); i++)
    {
        const uint32_t ulTextureIndex = ptResourceManager->_sbulTextureDeletionQueue[i];

        plTexture* ptTexture = &ptResourceManager->sbtTextures[ulTextureIndex];

        // we are hiding the frame
        if(ptTexture->tDesc.uMips < uFramesToProcess)
            ptTexture->tDesc.uMips = 0;
        else
            ptTexture->tDesc.uMips -= uFramesToProcess;

        if(ptTexture->tDesc.uMips == 0)
        {
            vkDestroyImage(tDevice, ptTexture->tImage, NULL);
            vkFreeMemory(tDevice, ptTexture->tMemory, NULL);

            pl_log_debug_to_f(ptGraphics->uLogChannel, "destroying texture %u", ulTextureIndex);

            ptTexture->tImage = VK_NULL_HANDLE;
            ptTexture->tMemory = VK_NULL_HANDLE;
            ptTexture->tDesc = (plTextureDesc){0};

            // add to free indices
            pl_sb_push(ptResourceManager->_sbulTextureFreeIndices, ulTextureIndex);

            bNeedUpdate = true;
        }
        else
        {
            pl_sb_push(ptResourceManager->_sbulTempQueue, ulTextureIndex);
        }
    }

    if(bNeedUpdate)
    {
        // copy temporary queue data over
        pl_sb_reset(ptResourceManager->_sbulTextureDeletionQueue);
        pl_sb_resize(ptResourceManager->_sbulTextureDeletionQueue, pl_sb_size(ptResourceManager->_sbulTempQueue));
        if(ptResourceManager->_sbulTempQueue)
            memcpy(ptResourceManager->_sbulTextureDeletionQueue, ptResourceManager->_sbulTempQueue, pl_sb_size(ptResourceManager->_sbulTempQueue) * sizeof(uint32_t));
    }

    // texture view cleanup
    pl_sb_reset(ptResourceManager->_sbulTempQueue);

    bNeedUpdate = false;

    for(uint32_t i = 0; i < pl_sb_size(ptResourceManager->_sbulTextureViewDeletionQueue); i++)
    {
        const uint32_t ulTextureViewIndex = ptResourceManager->_sbulTextureViewDeletionQueue[i];

        plTextureView* ptTextureView = &ptResourceManager->sbtTextureViews[ulTextureViewIndex];

        // we are hiding the frame
        if(ptTextureView->tTextureViewDesc.uLayerCount < uFramesToProcess)
            ptTextureView->tTextureViewDesc.uLayerCount = 0;
        else
            ptTextureView->tTextureViewDesc.uLayerCount -= uFramesToProcess;

        if(ptTextureView->tTextureViewDesc.uLayerCount == 0)
        {
            vkDestroyImageView(tDevice, ptTextureView->_tImageView, NULL);
            vkDestroySampler(tDevice, ptTextureView->_tSampler, NULL);

            pl_log_debug_to_f(ptGraphics->uLogChannel, "destroying texture view %u", ulTextureViewIndex);

            ptTextureView->tTextureViewDesc = (plTextureViewDesc){0};
            ptTextureView->tSampler = (plSampler){0};
            ptTextureView->_tImageView = VK_NULL_HANDLE;
            ptTextureView->_tSampler = VK_NULL_HANDLE;


            // add to free indices
            pl_sb_push(ptResourceManager->_sbulTextureViewFreeIndices, ulTextureViewIndex);

            bNeedUpdate = true;
        }
        else
        {
            pl_sb_push(ptResourceManager->_sbulTempQueue, ulTextureViewIndex);
        }
    }

    if(bNeedUpdate)
    {
        // copy temporary queue data over
        pl_sb_reset(ptResourceManager->_sbulTextureDeletionQueue);
        pl_sb_resize(ptResourceManager->_sbulTextureDeletionQueue, pl_sb_size(ptResourceManager->_sbulTempQueue));
        if(ptResourceManager->_sbulTempQueue)
            memcpy(ptResourceManager->_sbulTextureDeletionQueue, ptResourceManager->_sbulTempQueue, pl_sb_size(ptResourceManager->_sbulTempQueue) * sizeof(uint32_t));
    }

    // shader cleanup
    pl_sb_reset(ptResourceManager->_sbulTempQueue);

    bNeedUpdate = false;

    for(uint32_t i = 0; i < pl_sb_size(ptResourceManager->_sbulShaderDeletionQueue); i++)
    {
        const uint32_t uShaderIndex = ptResourceManager->_sbulShaderDeletionQueue[i];

        plShader* ptShader = &ptResourceManager->sbtShaders[uShaderIndex];

        // we are hiding the frame
        if(ptResourceManager->_sbulShaderHashes[uShaderIndex] < uFramesToProcess)
            ptResourceManager->_sbulShaderHashes[uShaderIndex] = 0;
        else
            ptResourceManager->_sbulShaderHashes[uShaderIndex] -= uFramesToProcess;

        if(ptResourceManager->_sbulShaderHashes[uShaderIndex] == 0)
        {

            ptGraphics->ptMemoryApi->free((uint32_t*)ptShader->tPixelShaderInfo.pCode);
            ptGraphics->ptMemoryApi->free((uint32_t*)ptShader->tVertexShaderInfo.pCode);
            vkDestroyPipelineLayout(ptResourceManager->_ptGraphics->tDevice.tLogicalDevice, ptShader->tPipelineLayout, NULL);

            for(uint32_t uVariantIndex = 0; uVariantIndex < pl_sb_size(ptShader->_sbuVariantPipelines); uVariantIndex++)
                vkDestroyPipeline(ptResourceManager->_ptGraphics->tDevice.tLogicalDevice, ptResourceManager->sbtShaderVariants[ptShader->_sbuVariantPipelines[uVariantIndex]].tPipeline, NULL);
            ptShader->tPipelineLayout = VK_NULL_HANDLE;
            pl_sb_free(ptShader->_sbuVariantPipelines);

            // add to free indices
            pl_sb_push(ptResourceManager->_sbulShaderFreeIndices, uShaderIndex);
            bNeedUpdate = true;

            pl_log_debug_to_f(ptGraphics->uLogChannel, "destroying shader %u", uShaderIndex);
        }
        else
        {
            pl_sb_push(ptResourceManager->_sbulTempQueue, uShaderIndex);
        }
    }

    if(bNeedUpdate)
    {
        // copy temporary queue data over
        pl_sb_reset(ptResourceManager->_sbulShaderDeletionQueue);
        pl_sb_resize(ptResourceManager->_sbulShaderDeletionQueue, pl_sb_size(ptResourceManager->_sbulTempQueue));
        if(ptResourceManager->_sbulTempQueue)
            memcpy(ptResourceManager->_sbulShaderDeletionQueue, ptResourceManager->_sbulTempQueue, pl_sb_size(ptResourceManager->_sbulTempQueue) * sizeof(uint32_t));
    }
}

static void
pl_transfer_data_to_buffer(plResourceManager* ptResourceManager, VkBuffer tDest, size_t szSize, const void* pData)
{
    pl__staging_buffer_may_grow(ptResourceManager, szSize);

    // copy data
    memcpy(ptResourceManager->_pucMapping, pData, szSize);

    // flush memory (incase we are using non-coherent memory)
    const VkMappedMemoryRange tMemoryRange = {
        .sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        .memory = ptResourceManager->_tStagingBufferMemory,
        .size   = VK_WHOLE_SIZE
    };
    PL_VULKAN(vkFlushMappedMemoryRanges(ptResourceManager->_ptDevice->tLogicalDevice, 1, &tMemoryRange));

    // perform copy from staging buffer to destination buffer
    VkCommandBuffer tCommandBuffer = pl_begin_command_buffer(ptResourceManager->_ptGraphics);

    const VkBufferCopy tCopyRegion = {
        .size = szSize
    };
    vkCmdCopyBuffer(tCommandBuffer, ptResourceManager->_tStagingBuffer, tDest, 1, &tCopyRegion);
    pl_submit_command_buffer(ptResourceManager->_ptGraphics, tCommandBuffer);

}

static void
pl_transfer_data_to_image(plResourceManager* ptResourceManager, plTexture* ptDest, size_t szDataSize, const void* pData)
{

    pl__staging_buffer_may_grow(ptResourceManager, szDataSize);

    // copy data to staging buffer
    memcpy(ptResourceManager->_pucMapping, pData, szDataSize);

    // flush memory (incase we are using non-coherent memory)
    const VkMappedMemoryRange tMemoryRange = {
        .sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        .memory = ptResourceManager->_tStagingBufferMemory,
        .size   = VK_WHOLE_SIZE
    };
    PL_VULKAN(vkFlushMappedMemoryRanges(ptResourceManager->_ptDevice->tLogicalDevice, 1, &tMemoryRange));

    const VkImageSubresourceRange tSubResourceRange = {
        .aspectMask     = ptDest->tDesc.tUsage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT ? VK_IMAGE_ASPECT_DEPTH_BIT  : VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = ptDest->tDesc.uMips,
        .baseArrayLayer = 0,
        .layerCount     = ptDest->tDesc.uLayers
    };

    VkCommandBuffer tCommandBuffer = pl_begin_command_buffer(ptResourceManager->_ptGraphics);

    // transition destination image layout to transfer destination
    pl_transition_image_layout(tCommandBuffer, ptDest->tImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, tSubResourceRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    // copy regions
    const VkBufferImageCopy tCopyRegion = {
        .bufferOffset                    = 0u,
        .bufferRowLength                 = 0u,
        .bufferImageHeight               = 0u,
        .imageSubresource.aspectMask     = tSubResourceRange.aspectMask,
        .imageSubresource.mipLevel       = 0,
        .imageSubresource.baseArrayLayer = 0,
        .imageSubresource.layerCount     = ptDest->tDesc.uLayers,
        .imageExtent                     = {.width = (uint32_t)ptDest->tDesc.tDimensions.x, .height = (uint32_t)ptDest->tDesc.tDimensions.y, .depth = (uint32_t)ptDest->tDesc.tDimensions.z},
    };
    vkCmdCopyBufferToImage(tCommandBuffer, ptResourceManager->_tStagingBuffer, ptDest->tImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &tCopyRegion);


    // generate mips
    if(ptDest->tDesc.uMips > 1)
    {
        // check if format supports linear blitting
        VkFormatProperties tFormatProperties = {0};
        vkGetPhysicalDeviceFormatProperties(ptResourceManager->_ptDevice->tPhysicalDevice, ptDest->tDesc.tFormat, &tFormatProperties);

        if(tFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)
        {
            VkImageSubresourceRange tMipSubResourceRange = {
                .aspectMask     = tSubResourceRange.aspectMask,
                .baseArrayLayer = 0,
                .layerCount     = ptDest->tDesc.uLayers,
                .levelCount     = 1
            };

            int iMipWidth = (int)ptDest->tDesc.tDimensions.x;
            int iMipHeight = (int)ptDest->tDesc.tDimensions.y;

            for(uint32_t i = 1; i < ptDest->tDesc.uMips; i++)
            {
                tMipSubResourceRange.baseMipLevel = i - 1;

                pl_transition_image_layout(tCommandBuffer, ptDest->tImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, tMipSubResourceRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

                VkImageBlit tBlit = {
                    .srcOffsets[1].x               = iMipWidth,
                    .srcOffsets[1].y               = iMipHeight,
                    .srcOffsets[1].z               = 1,
                    .srcSubresource.aspectMask     = tSubResourceRange.aspectMask,
                    .srcSubresource.mipLevel       = i - 1,
                    .srcSubresource.baseArrayLayer = 0,
                    .srcSubresource.layerCount     = 1,     
                    .dstOffsets[1].x               = 1,
                    .dstOffsets[1].y               = 1,
                    .dstOffsets[1].z               = 1,
                    .dstSubresource.aspectMask     = tSubResourceRange.aspectMask,
                    .dstSubresource.mipLevel       = i,
                    .dstSubresource.baseArrayLayer = 0,
                    .dstSubresource.layerCount     = 1,
                };

                if(iMipWidth > 1)  tBlit.dstOffsets[1].x = iMipWidth / 2;
                if(iMipHeight > 1) tBlit.dstOffsets[1].y = iMipHeight / 2;

                vkCmdBlitImage(tCommandBuffer, ptDest->tImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, ptDest->tImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &tBlit, VK_FILTER_LINEAR);

                pl_transition_image_layout(tCommandBuffer, ptDest->tImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, tMipSubResourceRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);


                if(iMipWidth > 1)  iMipWidth /= 2;
                if(iMipHeight > 1) iMipHeight /= 2;
            }

            tMipSubResourceRange.baseMipLevel = ptDest->tDesc.uMips - 1;
            pl_transition_image_layout(tCommandBuffer, ptDest->tImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, tMipSubResourceRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        }
        else
        {
            PL_ASSERT(false && "format does not support linear blitting");
        }
    }
    else
    {
        // transition destination image layout to shader usage
        pl_transition_image_layout(tCommandBuffer, ptDest->tImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, tSubResourceRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    }

    pl_submit_command_buffer(ptResourceManager->_ptGraphics, tCommandBuffer);
}

static uint32_t
pl_create_index_buffer(plResourceManager* ptResourceManager, size_t szSize, const void* pData, const char* pcName)
{
    const VkDevice tDevice = ptResourceManager->_ptDevice->tLogicalDevice;

    plBuffer tBuffer = {
        .tUsage          = PL_BUFFER_USAGE_INDEX,
        .szRequestedSize = szSize
    };

    // create vulkan buffer
    const VkBufferCreateInfo tBufferCreateInfo = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = szSize,
        .usage       = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    PL_VULKAN(vkCreateBuffer(tDevice, &tBufferCreateInfo, NULL, &tBuffer.tBuffer));

    // get memory requirements
    VkMemoryRequirements tMemoryRequirements = {0};
    vkGetBufferMemoryRequirements(tDevice, tBuffer.tBuffer, &tMemoryRequirements);
    tBuffer.szSize = tMemoryRequirements.size;

    // allocate buffer
    plDeviceMemoryAllocatorI* ptAllocator = &ptResourceManager->_ptGraphics->tLocalAllocator;
    plDeviceMemoryAllocation tDeviceAllocation = ptAllocator->allocate(ptAllocator->ptInst, tMemoryRequirements.size, tMemoryRequirements.alignment, pcName);
    tBuffer.tBufferMemory = tDeviceAllocation.tMemory;
    PL_VULKAN(vkBindBufferMemory(tDevice, tBuffer.tBuffer, tBuffer.tBufferMemory, 0));

    // upload data if any is availble
    if(pData)
        pl_transfer_data_to_buffer(ptResourceManager, tBuffer.tBuffer, szSize, pData);

    // find free index
    uint32_t ulBufferIndex = 0u;
    if(!pl__get_free_resource_index(ptResourceManager->_sbulBufferFreeIndices, &ulBufferIndex))
        ulBufferIndex = pl_sb_add_n(ptResourceManager->sbtBuffers, 1);
    ptResourceManager->sbtBuffers[ulBufferIndex] = tBuffer;

    if(pcName)
        pl_set_vulkan_object_name(ptResourceManager->_ptGraphics, (uint64_t)tBuffer.tBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, pcName);

    return ulBufferIndex;
}

static uint32_t
pl_create_vertex_buffer(plResourceManager* ptResourceManager, size_t szSize, size_t szStride, const void* pData, const char* pcName)
{
    const VkDevice tDevice = ptResourceManager->_ptDevice->tLogicalDevice;

    plBuffer tBuffer = {
        .tUsage          = PL_BUFFER_USAGE_VERTEX,
        .szRequestedSize = szSize,
        .szStride        = szStride
    };

    // create vulkan buffer
    const VkBufferCreateInfo tBufferCreateInfo = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = szSize,
        .usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    PL_VULKAN(vkCreateBuffer(tDevice, &tBufferCreateInfo, NULL, &tBuffer.tBuffer));

    // get memory requirements
    VkMemoryRequirements tMemoryRequirements = {0};
    vkGetBufferMemoryRequirements(tDevice, tBuffer.tBuffer, &tMemoryRequirements);
    tBuffer.szSize = tMemoryRequirements.size;

    // allocate buffer
    plDeviceMemoryAllocatorI* ptAllocator = &ptResourceManager->_ptGraphics->tLocalAllocator;
    plDeviceMemoryAllocation tDeviceAllocation = ptAllocator->allocate(ptAllocator->ptInst, tMemoryRequirements.size, tMemoryRequirements.alignment, pcName);
    tBuffer.tBufferMemory = tDeviceAllocation.tMemory;
    PL_VULKAN(vkBindBufferMemory(tDevice, tBuffer.tBuffer, tBuffer.tBufferMemory, 0));

    // upload data if any is availble
    if(pData)
        pl_transfer_data_to_buffer(ptResourceManager, tBuffer.tBuffer, szSize, pData);

    // find free index
    uint32_t ulBufferIndex = 0u;
    if(!pl__get_free_resource_index(ptResourceManager->_sbulBufferFreeIndices, &ulBufferIndex))
        ulBufferIndex = pl_sb_add_n(ptResourceManager->sbtBuffers, 1);
    ptResourceManager->sbtBuffers[ulBufferIndex] = tBuffer;

    if(pcName)
        pl_set_vulkan_object_name(ptResourceManager->_ptGraphics, (uint64_t)tBuffer.tBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, pcName);

    return ulBufferIndex;  
}

static uint32_t
pl_create_constant_buffer(plResourceManager* ptResourceManager, size_t szSize, const char* pcName)
{
    const VkDevice tDevice = ptResourceManager->_ptDevice->tLogicalDevice;

    plBuffer tBuffer = {
        .tUsage          = PL_BUFFER_USAGE_CONSTANT,
        .szRequestedSize = szSize,
        .szItemCount     = 1
    };

    // create vulkan buffer
    const VkBufferCreateInfo tBufferCreateInfo = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = szSize,
        .usage       = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    PL_VULKAN(vkCreateBuffer(tDevice, &tBufferCreateInfo, NULL, &tBuffer.tBuffer));

    // get memory requirements
    VkMemoryRequirements tMemoryRequirements = {0};
    vkGetBufferMemoryRequirements(tDevice, tBuffer.tBuffer, &tMemoryRequirements);
    tBuffer.szSize = tMemoryRequirements.size;
    tBuffer.szStride = tMemoryRequirements.size;

    // allocate buffer
    plDeviceMemoryAllocatorI* ptAllocator = &ptResourceManager->_ptGraphics->tStagingUnCachedAllocator;
    plDeviceMemoryAllocation tDeviceAllocation = ptAllocator->allocate(ptAllocator->ptInst, tMemoryRequirements.size, tMemoryRequirements.alignment, pcName);
    tBuffer.tBufferMemory = tDeviceAllocation.tMemory;
    tBuffer.pucMapping = tDeviceAllocation.pHostMapped;

    PL_VULKAN(vkBindBufferMemory(tDevice, tBuffer.tBuffer, tBuffer.tBufferMemory, 0));

    // find free index
    uint32_t ulBufferIndex = 0u;
    if(!pl__get_free_resource_index(ptResourceManager->_sbulBufferFreeIndices, &ulBufferIndex))
        ulBufferIndex = pl_sb_add_n(ptResourceManager->sbtBuffers, 1);
    ptResourceManager->sbtBuffers[ulBufferIndex] = tBuffer;

    if(pcName)
        pl_set_vulkan_object_name(ptResourceManager->_ptGraphics, (uint64_t)tBuffer.tBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, pcName);

    return ulBufferIndex;      
}

static uint32_t
pl_create_texture_view(plResourceManager* ptResourceManager, const plTextureViewDesc* ptViewDesc, const plSampler* ptSampler, uint32_t uTextureHandle, const char* pcName)
{
    plTextureView tTextureView = {
        .tSampler         = *ptSampler,
        .tTextureViewDesc = *ptViewDesc,
        .uTextureHandle   = uTextureHandle
    };

    VkDevice tDevice = ptResourceManager->_ptDevice->tLogicalDevice;
    plTexture* ptTexture = &ptResourceManager->sbtTextures[uTextureHandle];

    if(ptViewDesc->uMips == 0)
        tTextureView.tTextureViewDesc.uMips = ptTexture->tDesc.uMips;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~create view~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const VkImageViewType tImageViewType = ptViewDesc->uLayerCount == 6 ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;    
    PL_ASSERT((ptViewDesc->uLayerCount == 1 || ptViewDesc->uLayerCount == 6) && "unsupported layer count");

    VkImageAspectFlags tImageAspectFlags = ptTexture->tDesc.tUsage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    if(ptResourceManager->_ptGraphics->ptDeviceApi->format_has_stencil(ptViewDesc->tFormat))
        tImageAspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;

    VkImageViewCreateInfo tViewInfo = {
        .sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image                           = ptTexture->tImage,
        .viewType                        = tImageViewType,
        .format                          = ptViewDesc->tFormat,
        .subresourceRange.baseMipLevel   = ptViewDesc->uBaseMip,
        .subresourceRange.levelCount     = tTextureView.tTextureViewDesc.uMips,
        .subresourceRange.baseArrayLayer = ptViewDesc->uBaseLayer,
        .subresourceRange.layerCount     = ptViewDesc->uLayerCount,
        .subresourceRange.aspectMask     = tImageAspectFlags,
    };
    PL_VULKAN(vkCreateImageView(tDevice, &tViewInfo, NULL, &tTextureView._tImageView));

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~create sampler~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    VkSamplerCreateInfo tSamplerInfo = {
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter               = pl__vulkan_filter(ptSampler->tFilter),
        .addressModeU            = pl__vulkan_wrap(ptSampler->tHorizontalWrap),
        .addressModeV            = pl__vulkan_wrap(ptSampler->tVerticalWrap),
        .anisotropyEnable        = (bool)ptResourceManager->_ptDevice->tDeviceFeatures.samplerAnisotropy,
        .maxAnisotropy           = ptResourceManager->_ptDevice->tDeviceProps.limits.maxSamplerAnisotropy,
        .borderColor             = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable           = VK_FALSE,
        .compareOp               = pl__vulkan_compare(ptSampler->tCompare),
        .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .mipLodBias              = ptSampler->fMipBias,
        .minLod                  = ptSampler->fMinMip,
        .maxLod                  = ptSampler->fMaxMip,
    };

    // .compareOp               = VK_COMPARE_OP_ALWAYS,
    tSamplerInfo.minFilter    = tSamplerInfo.magFilter;
    tSamplerInfo.addressModeW = tSamplerInfo.addressModeU;

    PL_VULKAN(vkCreateSampler(tDevice, &tSamplerInfo, NULL, &tTextureView._tSampler));

    // find free index
    uint32_t ulTextureViewIndex = 0u;
    if(!pl__get_free_resource_index(ptResourceManager->_sbulTextureViewFreeIndices, &ulTextureViewIndex))
        ulTextureViewIndex = pl_sb_add_n(ptResourceManager->sbtTextureViews, 1);
    ptResourceManager->sbtTextureViews[ulTextureViewIndex] = tTextureView;

    if(pcName)
    {
        pl_set_vulkan_object_name(ptResourceManager->_ptGraphics, (uint64_t)tTextureView._tSampler, VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT, pcName);
        pl_set_vulkan_object_name(ptResourceManager->_ptGraphics, (uint64_t)tTextureView._tImageView, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, pcName);
    }

    return ulTextureViewIndex;
}

static uint32_t
pl_request_dynamic_buffer(plResourceManager* ptResourceManager)
{

    plDynamicBufferNode* sbtNodes = ptResourceManager->_sbtDynamicBufferList;
    uint32_t n = 0;

    if (sbtNodes[1].uNext != 1) // check free list
    {
        n = sbtNodes[1].uNext;
        sbtNodes[n].uLastActiveFrame = (uint32_t)ptResourceManager->_ptGraphics->szCurrentFrameIndex;

        // remove from free list
        sbtNodes[sbtNodes[n].uNext].uPrev = sbtNodes[n].uPrev;
        sbtNodes[sbtNodes[n].uPrev].uNext = sbtNodes[n].uNext;
    }
    else  // add new node
    {
        pl_sb_resize(sbtNodes, pl_sb_size(sbtNodes) + 1); //-V1004
        n = pl_sb_size(sbtNodes) - 1;
        ptResourceManager->_sbtDynamicBufferList = sbtNodes; // just incase realloc occurs
        sbtNodes[n].uDynamicBufferOffset = 0;
        sbtNodes[n].uLastActiveFrame = (uint32_t)ptResourceManager->_ptGraphics->szCurrentFrameIndex;

        // add node to list
        sbtNodes[n].uNext = sbtNodes[0].uNext;
        sbtNodes[n].uPrev = 0;
        sbtNodes[sbtNodes[n].uNext].uPrev = n;
        sbtNodes[sbtNodes[n].uPrev].uNext = n;

        pl_log_info_to(ptResourceManager->_ptGraphics->uLogChannel, "creating new dynamic buffer");
        sbtNodes[n].uDynamicBuffer = pl_create_constant_buffer(ptResourceManager, ptResourceManager->_uDynamicBufferSize, "temp dynamic buffer");

    }
    return n;
}

static void
pl_return_dynamic_buffer(plResourceManager* ptResourceManager, uint32_t uNodeIndex)
{
    // add node to free list
    plDynamicBufferNode* sbtNodes = ptResourceManager->_sbtDynamicBufferList;
    plDynamicBufferNode* ptNode = &sbtNodes[uNodeIndex];
    plDynamicBufferNode* ptFreeNode = &sbtNodes[1];
    ptNode->uNext = ptFreeNode->uNext;
    ptNode->uPrev = 1;
    ptNode->uDynamicBufferOffset = 0;
    ptNode->uLastActiveFrame = 0;
    sbtNodes[ptNode->uNext].uPrev = uNodeIndex;
    ptFreeNode->uNext = uNodeIndex;
}

static uint32_t
pl_create_texture(plResourceManager* ptResourceManager, plTextureDesc tDesc, size_t szSize, const void* pData, const char* pcName)
{
    VkDevice tDevice = ptResourceManager->_ptDevice->tLogicalDevice;

    if(tDesc.uMips == 0)
    {
        tDesc.uMips = (uint32_t)floorf(log2f((float)pl_maxi((int)tDesc.tDimensions.x, (int)tDesc.tDimensions.y))) + 1u;
    }

    plTexture tTexture = {
        .tDesc = tDesc
    };

    const VkImageViewType tImageViewType = tDesc.uLayers == 6 ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;    
    PL_ASSERT((tDesc.uLayers == 1 || tDesc.uLayers == 6) && "unsupported layer count");

    // create vulkan image
    VkImageCreateInfo tImageInfo = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = tDesc.tType,
        .extent.width  = (uint32_t)tDesc.tDimensions.x,
        .extent.height = (uint32_t)tDesc.tDimensions.y,
        .extent.depth  = (uint32_t)tDesc.tDimensions.z,
        .mipLevels     = tDesc.uMips,
        .arrayLayers   = tDesc.uLayers,
        .format        = tDesc.tFormat,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage         = tDesc.tUsage,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .flags         = tImageViewType == VK_IMAGE_VIEW_TYPE_CUBE ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0
    };

    if(pData)
        tImageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    PL_VULKAN(vkCreateImage(tDevice, &tImageInfo, NULL, &tTexture.tImage));

    // get memory requirements
    VkMemoryRequirements tMemoryRequirements = {0};
    vkGetImageMemoryRequirements(tDevice, tTexture.tImage, &tMemoryRequirements);

    // allocate buffer
    plDeviceMemoryAllocatorI* ptAllocator = &ptResourceManager->_ptGraphics->tLocalAllocator;
    plDeviceMemoryAllocation tDeviceAllocation = ptAllocator->allocate(ptAllocator->ptInst, tMemoryRequirements.size, tMemoryRequirements.alignment, pcName);
    tTexture.tMemory = tDeviceAllocation.tMemory;

    PL_VULKAN(vkBindImageMemory(tDevice, tTexture.tImage, tTexture.tMemory, 0));

    // upload data
    if(pData)
        pl_transfer_data_to_image(ptResourceManager, &tTexture, szSize, pData);

    VkImageAspectFlags tImageAspectFlags = tDesc.tUsage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    if(ptResourceManager->_ptGraphics->ptDeviceApi->format_has_stencil(tDesc.tFormat))
        tImageAspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;

    VkCommandBuffer tCommandBuffer = pl_begin_command_buffer(ptResourceManager->_ptGraphics);
    VkImageSubresourceRange tRange = {
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = tDesc.uLayers,
        .aspectMask     = tImageAspectFlags
    };

    if(pData)
        pl_transition_image_layout(tCommandBuffer, tTexture.tImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, tRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    else if(tDesc.tUsage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        pl_transition_image_layout(tCommandBuffer, tTexture.tImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, tRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    else if(tDesc.tUsage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
        pl_transition_image_layout(tCommandBuffer, tTexture.tImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, tRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    pl_submit_command_buffer(ptResourceManager->_ptGraphics, tCommandBuffer);

    // find free index
    uint32_t ulTextureIndex = 0u;
    if(!pl__get_free_resource_index(ptResourceManager->_sbulTextureFreeIndices, &ulTextureIndex))
        ulTextureIndex = pl_sb_add_n(ptResourceManager->sbtTextures, 1);
    ptResourceManager->sbtTextures[ulTextureIndex] = tTexture;

    if(pcName)
        pl_set_vulkan_object_name(ptResourceManager->_ptGraphics, (uint64_t)tTexture.tImage, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, pcName);

    return ulTextureIndex;      
}

static uint32_t
pl_create_storage_buffer(plResourceManager* ptResourceManager, size_t szSize, const void* pData, const char* pcName)
{
    const VkDevice tDevice = ptResourceManager->_ptDevice->tLogicalDevice;

    plBuffer tBuffer = {
        .tUsage          = PL_BUFFER_USAGE_STORAGE,
        .szRequestedSize = szSize
    };

    // create vulkan buffer
    const VkBufferCreateInfo tBufferCreateInfo = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = szSize,
        .usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    PL_VULKAN(vkCreateBuffer(tDevice, &tBufferCreateInfo, NULL, &tBuffer.tBuffer));

    // get memory requirements
    VkMemoryRequirements tMemoryRequirements = {0};
    vkGetBufferMemoryRequirements(tDevice, tBuffer.tBuffer, &tMemoryRequirements);
    tBuffer.szSize = tMemoryRequirements.size;

    // allocate buffer
    plDeviceMemoryAllocatorI* ptAllocator = &ptResourceManager->_ptGraphics->tLocalAllocator;
    plDeviceMemoryAllocation tDeviceAllocation = ptAllocator->allocate(ptAllocator->ptInst, tMemoryRequirements.size, tMemoryRequirements.alignment, pcName);
    tBuffer.tBufferMemory = tDeviceAllocation.tMemory;
    PL_VULKAN(vkBindBufferMemory(tDevice, tBuffer.tBuffer, tBuffer.tBufferMemory, 0));

    // upload data if any is availble
    if(pData)
        pl_transfer_data_to_buffer(ptResourceManager, tBuffer.tBuffer, szSize, pData);

    // find free index
    uint32_t ulBufferIndex = 0u;
    if(!pl__get_free_resource_index(ptResourceManager->_sbulBufferFreeIndices, &ulBufferIndex))
        ulBufferIndex = pl_sb_add_n(ptResourceManager->sbtBuffers, 1);
    ptResourceManager->sbtBuffers[ulBufferIndex] = tBuffer;

    if(pcName)
        pl_set_vulkan_object_name(ptResourceManager->_ptGraphics, (uint64_t)tBuffer.tBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, pcName);

    return ulBufferIndex;
}

static void
pl_submit_buffer_for_deletion(plResourceManager* ptResourceManager, uint32_t ulBufferIndex)
{
    PL_ASSERT(ulBufferIndex < pl_sb_size(ptResourceManager->sbtBuffers)); 
    pl_sb_push(ptResourceManager->_sbulBufferDeletionQueue, ulBufferIndex);

    // using szStride member to store frame this buffer is ok to free
    ptResourceManager->sbtBuffers[ulBufferIndex].szStride = (size_t)ptResourceManager->_ptGraphics->uFramesInFlight;
}

static void
pl_submit_texture_for_deletion(plResourceManager* ptResourceManager, uint32_t ulTextureIndex)
{
    PL_ASSERT(ulTextureIndex < pl_sb_size(ptResourceManager->sbtTextures)); 
    pl_sb_push(ptResourceManager->_sbulTextureDeletionQueue, ulTextureIndex);

    // using szStride member to store frame this buffer is ok to free
    ptResourceManager->sbtTextures[ulTextureIndex].tDesc.uMips = ptResourceManager->_ptGraphics->uFramesInFlight;    
}

static void
pl_submit_texture_view_for_deletion(plResourceManager* ptResourceManager, uint32_t uTextureViewIndex)
{
    PL_ASSERT(uTextureViewIndex < pl_sb_size(ptResourceManager->sbtTextureViews)); 
    pl_sb_push(ptResourceManager->_sbulTextureViewDeletionQueue, uTextureViewIndex);

    // using tTextureViewDesc.uLayerCount member to store frame this buffer is ok to free
    ptResourceManager->sbtTextureViews[uTextureViewIndex].tTextureViewDesc.uLayerCount = ptResourceManager->_ptGraphics->uFramesInFlight;    
}

static void
pl_cleanup_graphics(plGraphics* ptGraphics)
{

    // ensure device is finished
    vkDeviceWaitIdle(ptGraphics->tDevice.tLogicalDevice);

    pl__cleanup_resource_manager(&ptGraphics->tResourceManager);

    if(ptGraphics->tSwapchain.tDepthImageView) vkDestroyImageView(ptGraphics->tDevice.tLogicalDevice, ptGraphics->tSwapchain.tDepthImageView, NULL);
    if(ptGraphics->tSwapchain.tDepthImage)     vkDestroyImage(ptGraphics->tDevice.tLogicalDevice, ptGraphics->tSwapchain.tDepthImage, NULL);
    if(ptGraphics->tSwapchain.tDepthMemory)    vkFreeMemory(ptGraphics->tDevice.tLogicalDevice, ptGraphics->tSwapchain.tDepthMemory, NULL);
    if(ptGraphics->tSwapchain.tColorImageView) vkDestroyImageView(ptGraphics->tDevice.tLogicalDevice, ptGraphics->tSwapchain.tColorImageView, NULL);
    if(ptGraphics->tSwapchain.tColorImage)     vkDestroyImage(ptGraphics->tDevice.tLogicalDevice, ptGraphics->tSwapchain.tColorImage, NULL);
    if(ptGraphics->tSwapchain.tColorMemory)    vkFreeMemory(ptGraphics->tDevice.tLogicalDevice, ptGraphics->tSwapchain.tColorMemory, NULL);

    // destroy swapchain
    for (uint32_t i = 0u; i < ptGraphics->tSwapchain.uImageCount; i++)
    {
        vkDestroyImageView(ptGraphics->tDevice.tLogicalDevice, ptGraphics->tSwapchain.ptImageViews[i], NULL);
        vkDestroyFramebuffer(ptGraphics->tDevice.tLogicalDevice, ptGraphics->tSwapchain.ptFrameBuffers[i], NULL);
    }

    vkDestroyDescriptorPool(ptGraphics->tDevice.tLogicalDevice, ptGraphics->tDescriptorPool, NULL);

    // destroy default render pass
    vkDestroyRenderPass(ptGraphics->tDevice.tLogicalDevice, ptGraphics->tRenderPass, NULL);
    vkDestroySwapchainKHR(ptGraphics->tDevice.tLogicalDevice, ptGraphics->tSwapchain.tSwapChain, NULL);

    for(uint32_t i = 0; i < ptGraphics->uFramesInFlight; i++)
    {
        // destroy command buffers
        vkFreeCommandBuffers(ptGraphics->tDevice.tLogicalDevice, ptGraphics->tCmdPool, 1u, &ptGraphics->sbFrames[i].tCmdBuf);

        // destroy sync primitives
        vkDestroySemaphore(ptGraphics->tDevice.tLogicalDevice, ptGraphics->sbFrames[i].tImageAvailable, NULL);
        vkDestroySemaphore(ptGraphics->tDevice.tLogicalDevice, ptGraphics->sbFrames[i].tRenderFinish, NULL);
        vkDestroyFence(ptGraphics->tDevice.tLogicalDevice, ptGraphics->sbFrames[i].tInFlight, NULL);
    }

    // destroy command pool
    vkDestroyCommandPool(ptGraphics->tDevice.tLogicalDevice, ptGraphics->tCmdPool, NULL);

    if(ptGraphics->tDbgMessenger)
    {
        PFN_vkDestroyDebugUtilsMessengerEXT tFunc = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ptGraphics->tInstance, "vkDestroyDebugUtilsMessengerEXT");
        if (tFunc != NULL)
            tFunc(ptGraphics->tInstance, ptGraphics->tDbgMessenger, NULL);
    }

    // destroy tSurface
    vkDestroySurfaceKHR(ptGraphics->tInstance, ptGraphics->tSurface, NULL);

    // destroy device
    vkDestroyDevice(ptGraphics->tDevice.tLogicalDevice, NULL);

    // destroy tInstance
    vkDestroyInstance(ptGraphics->tInstance, NULL);

    ptGraphics->ptMemoryApi->free(ptGraphics->tSwapchain.ptSurfaceFormats_);
    ptGraphics->ptMemoryApi->free(ptGraphics->tSwapchain.ptImages);
    ptGraphics->ptMemoryApi->free(ptGraphics->tSwapchain.ptImageViews);
    ptGraphics->ptMemoryApi->free(ptGraphics->tSwapchain.ptFrameBuffers);
}


static plFrameContext*
pl_get_frame_resources(plGraphics* ptGraphics)
{
    return &ptGraphics->sbFrames[ptGraphics->szCurrentFrameIndex];
}

static uint32_t
pl_find_memory_type(VkPhysicalDeviceMemoryProperties tMemProps, uint32_t uTypeFilter, VkMemoryPropertyFlags tProperties)
{
    uint32_t uMemoryType = 0u;
    for (uint32_t i = 0; i < tMemProps.memoryTypeCount; i++) 
    {
        if ((uTypeFilter & (1 << i)) && (tMemProps.memoryTypes[i].propertyFlags & tProperties) == tProperties) 
        {
            uMemoryType = i;
            break;
        }
    }
    return uMemoryType;    
}

static void
pl_transition_image_layout(VkCommandBuffer tCommandBuffer, VkImage tImage, VkImageLayout tOldLayout, VkImageLayout tNewLayout, VkImageSubresourceRange tSubresourceRange, VkPipelineStageFlags tSrcStageMask, VkPipelineStageFlags tDstStageMask)
{
    //VkCommandBuffer commandBuffer = mvBeginSingleTimeCommands();
    VkImageMemoryBarrier tBarrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout           = tOldLayout,
        .newLayout           = tNewLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = tImage,
        .subresourceRange    = tSubresourceRange,
    };

    // Source layouts (old)
    // Source access mask controls actions that have to be finished on the old layout
    // before it will be transitioned to the new layout
    switch (tOldLayout)
    {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            // Image layout is undefined (or does not matter)
            // Only valid as initial layout
            // No flags required, listed only for completeness
            tBarrier.srcAccessMask = 0;
            break;

        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            // Image is preinitialized
            // Only valid as initial layout for linear images, preserves memory contents
            // Make sure host writes have been finished
            tBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            // Image is a color attachment
            // Make sure any writes to the color buffer have been finished
            tBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            // Image is a depth/stencil attachment
            // Make sure any writes to the depth/stencil buffer have been finished
            tBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            // Image is a transfer source
            // Make sure any reads from the image have been finished
            tBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            // Image is a transfer destination
            // Make sure any writes to the image have been finished
            tBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            // Image is read by a shader
            // Make sure any shader reads from the image have been finished
            tBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;
        default:
            // Other source layouts aren't handled (yet)
            break;
        }

        // Target layouts (new)
        // Destination access mask controls the dependency for the new image layout
        switch (tNewLayout)
        {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            // Image will be used as a transfer destination
            // Make sure any writes to the image have been finished
            tBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            // Image will be used as a transfer source
            // Make sure any reads from the image have been finished
            tBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            // Image will be used as a color attachment
            // Make sure any writes to the color buffer have been finished
            tBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            // Image layout will be used as a depth/stencil attachment
            // Make sure any writes to depth/stencil buffer have been finished
            tBarrier.dstAccessMask = tBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            // Image will be read in a shader (sampler, input attachment)
            // Make sure any writes to the image have been finished
            if (tBarrier.srcAccessMask == 0)
                tBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
            tBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;
        default:
            // Other source layouts aren't handled (yet)
            break;
    }
    vkCmdPipelineBarrier(tCommandBuffer, tSrcStageMask, tDstStageMask, 0, 0, NULL, 0, NULL, 1, &tBarrier);
}

static void
pl_set_vulkan_object_name(plGraphics* ptGraphics, uint64_t uObjectHandle, VkDebugReportObjectTypeEXT tObjectType, const char* pcName)
{
    const VkDebugMarkerObjectNameInfoEXT tNameInfo = 
    {
        .sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT,
        .objectType = tObjectType,
        .object = uObjectHandle,
        .pObjectName = pcName
    };

    ptGraphics->vkDebugMarkerSetObjectName(ptGraphics->tDevice.tLogicalDevice, &tNameInfo);
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_vulkan_ext(plApiRegistryApiI* ptApiRegistry, bool bReload)
{

    if(bReload)
    {
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_DESCRIPTOR_MANAGER), pl_load_descriptor_manager_api());
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_DEVICE_MEMORY), pl_load_device_memory_api());
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_DEVICE), pl_load_device_api());
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_GRAPHICS), pl_load_graphics_api());
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_RESOURCE_MANAGER_0), pl_load_resource_manager_api());
    }
    else
    {
        ptApiRegistry->add(PL_API_DESCRIPTOR_MANAGER, pl_load_descriptor_manager_api());
        ptApiRegistry->add(PL_API_DEVICE_MEMORY, pl_load_device_memory_api());
        ptApiRegistry->add(PL_API_DEVICE, pl_load_device_api());
        ptApiRegistry->add(PL_API_GRAPHICS, pl_load_graphics_api());
        ptApiRegistry->add(PL_API_RESOURCE_MANAGER_0, pl_load_resource_manager_api());
    }
}

PL_EXPORT void
pl_unload_vulkan_ext(plApiRegistryApiI* ptApiRegistry)
{
    
}