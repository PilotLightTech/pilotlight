/*
   pl_vulkan_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] global data
// [SECTION] internal structs
// [SECTION] internal api (public api structs)
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] public api struct implementation
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
#include "pl_memory.h"
#include "pl_profile.h"
#include "pl_log.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

#ifdef _WIN32
#pragma comment(lib, "vulkan-1.lib")
#endif

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------
static uint32_t uLogChannel = UINT32_MAX;

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plVariantInfo
{
    VkShaderModuleCreateInfo tVertexShaderInfo;
    VkShaderModuleCreateInfo tPixelShaderInfo;
    VkPipelineLayout         tPipelineLayout;
    uint32_t                 uRenderPass;
    VkSampleCountFlagBits    tMSAASampleCount;
    VkSpecializationInfo     tSpecializationInfo;
} plVariantInfo;

typedef struct _plDeviceAllocatorData
{
    VkDevice                         tDevice;
    VkPhysicalDeviceMemoryProperties tMemProps;
    plDeviceAllocationBlock*         sbtBlocks;

    // buddy allocator data
    char*                   sbcDebugNames;
    char**                  sbtDebugNamesInDir;
    plDeviceAllocationNode* sbtNodes;
    uint32_t                auFreeList[PL_DEVICE_LOCAL_LEVELS];

} plDeviceAllocatorData;

//-----------------------------------------------------------------------------
// [SECTION] internal api (public api structs)
//-----------------------------------------------------------------------------

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~backend api~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

static void pl__create_instance   (plApiRegistryApiI* ptApiRegistry, plRenderBackend* ptBackend, uint32_t uVersion, bool bEnableValidation);
static void pl__create_instance_ex(plRenderBackend* ptBackend, uint32_t uVersion, uint32_t uLayerCount, const char** ppcEnabledLayers, uint32_t uExtensioncount, const char** ppcEnabledExtensions);
static void pl__cleanup_backend(plApiRegistryApiI* ptApiRegistry, plRenderBackend* ptBackend);

static void pl__create_device(plRenderBackend* ptBackend, VkSurfaceKHR tSurface, bool bEnableValidation, plDevice* ptDeviceOut);
static void pl_cleanup_device(plDevice* ptDevice);

static void pl__create_swapchain(plRenderBackend* ptBackend, plDevice* ptDevice, VkSurfaceKHR tSurface, uint32_t uWidth, uint32_t uHeight, plSwapchain* ptSwapchainOut);
static void pl__cleanup_swapchain(plRenderBackend* ptBackend, plDevice* ptDevice, plSwapchain* ptSwapchain);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~graphics api~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// setup
static void pl_setup_graphics   (plGraphics* ptGraphics, plRenderBackend* ptBackend, plApiRegistryApiI* ptApiRegistry, plTempAllocator* ptAllocator);
static void pl_cleanup_graphics (plGraphics* ptGraphics);
static void pl_resize_graphics  (plGraphics* ptGraphics);

// per frame
static bool pl_begin_frame     (plGraphics* ptGraphics);
static void pl_end_frame       (plGraphics* ptGraphics);
static void pl_begin_recording (plGraphics* ptGraphics);
static void pl_end_recording   (plGraphics* ptGraphics);

// shaders
static uint32_t           pl_create_shader             (plGraphics* ptGraphics, const plShaderDesc* ptDesc);
static uint32_t           pl_add_shader_variant        (plGraphics* ptGraphics, uint32_t uShader, plGraphicsState tVariant, uint32_t uRenderPass, VkSampleCountFlagBits tMSAASampleCount);
static bool               pl_shader_variant_exist      (plGraphics* ptGraphics, uint32_t uShader, plGraphicsState tVariant, uint32_t uRenderPass, VkSampleCountFlagBits tMSAASampleCount);
static void               pl_submit_shader_for_deletion(plGraphics* ptGraphics, uint32_t uShaderIndex);
static plBindGroupLayout* pl_get_bind_group_layout     (plGraphics* ptGraphics, uint32_t uShaderIndex, uint32_t uBindGroupIndex);
static plShaderVariant*   pl_get_shader                (plGraphics* ptGraphics, uint32_t uVariantIndex);

// descriptors
static void pl_update_bind_group(plGraphics* ptGraphics, plBindGroup* ptGroup, uint32_t uBufferCount, uint32_t* auBuffers, size_t* aszBufferRanges, uint32_t uTextureViewCount, uint32_t* auTextureViews);

// drawing
static void pl_draw_areas(plGraphics* ptGraphics, uint32_t uAreaCount, plDrawArea* atAreas, plDraw* atDraws);

// misc
static plFrameContext* pl_get_frame_resources(plGraphics* ptGraphics);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~device memory api~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

static plDeviceMemoryAllocation pl_allocate_dedicated(struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName);
static void                     pl_free_dedicated    (struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation);
static plDeviceAllocationBlock* pl_blocks_dedicated  (struct plDeviceMemoryAllocatorO* ptInst, uint32_t* puSizeOut);
static plDeviceAllocationNode*  pl_nodes_dedicated   (struct plDeviceMemoryAllocatorO* ptInst, uint32_t* puSizeOut);
static char**                   pl_names_dedicated   (struct plDeviceMemoryAllocatorO* ptInst, uint32_t* puSizeOut);
static plDeviceMemoryAllocatorI pl_create_device_local_dedicated_allocator    (VkPhysicalDevice tPhysicalDevice, VkDevice tDevice);
static void                     pl_cleanup_device_local_dedicated_allocator   (plDeviceMemoryAllocatorI* ptAllocator);

static plDeviceMemoryAllocatorI pl_create_staging_uncached_allocator (VkPhysicalDevice tPhysicalDevice, VkDevice tDevice);
static void                     pl_cleanup_staging_uncached_allocator(plDeviceMemoryAllocatorI* ptAllocator);
static plDeviceMemoryAllocation pl_allocate_staging_uncached         (struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName);
static void                     pl_free_staging_uncached             (struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation);

static plDeviceMemoryAllocatorI pl_create_staging_cached_allocator (VkPhysicalDevice tPhysicalDevice, VkDevice tDevice);
static void                     pl_cleanup_staging_cached_allocator(plDeviceMemoryAllocatorI* ptAllocator);
static plDeviceMemoryAllocation pl_allocate_staging_cached         (struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName);
static void                     pl_free_staging_cached             (struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation);

static plDeviceMemoryAllocatorI pl_create_device_local_buddy_allocator (VkPhysicalDevice tPhysicalDevice, VkDevice tDevice);
static void                     pl_cleanup_device_local_buddy_allocator(plDeviceMemoryAllocatorI* ptAllocator);
static plDeviceMemoryAllocation pl_allocate_local_buffer                (struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName);
static void                     pl_free_local_buffer                    (struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation);


//~~~~~~~~~~~~~~~~~~~~~~~~~descriptor manager api~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

static void                     pl_cleanup_descriptor_manager   (plDescriptorManager* ptManager);
static VkDescriptorSetLayout    pl_request_descriptor_set_layout(plDescriptorManager* ptManager, plBindGroupLayout* ptLayout);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~device api~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

static void pl_init_device(plApiRegistryApiI* ptApiRegistry, plDevice* ptDevice, uint32_t uFramesInFlight);

// command buffers
static VkCommandBuffer pl_begin_command_buffer (plDevice* ptDevice, VkCommandPool tCmdPool);
static void            pl_submit_command_buffer(plDevice* ptDevice, VkCommandPool tCmdPool, VkCommandBuffer tCmdBuffer);

// misc
static plFormat              pl_find_supported_format    (plDevice* ptDevice, VkFormatFeatureFlags tFlags, const plFormat* ptFormats, uint32_t uFormatCount);
static plFormat              pl_find_depth_format        (plDevice* ptDevice);
static plFormat              pl_find_depth_stencil_format(plDevice* ptDevice);
static bool                  pl_format_has_stencil       (plFormat tFormat);
static VkSampleCountFlagBits pl_get_max_sample_count     (plDevice* ptDevice);
static uint32_t              pl_find_memory_type         (VkPhysicalDeviceMemoryProperties tMemProps, uint32_t uTypeFilter, VkMemoryPropertyFlags tProperties);
static void                  pl_transition_image_layout  (VkCommandBuffer tCommandBuffer, VkImage tImage, VkImageLayout tOldLayout, VkImageLayout tNewLayout, VkImageSubresourceRange tSubresourceRange, VkPipelineStageFlags tSrcStageMask, VkPipelineStageFlags tDstStageMask);
static void                  pl_set_vulkan_object_name   (plDevice* ptDevice, uint64_t uObjectHandle, VkDebugReportObjectTypeEXT tObjectType, const char* pcName);

// resource manager per frame
static void                  pl_process_cleanup_queue (plDevice* ptDevice, uint32_t uCurrentFrame);

// resource manager dynamic buffers
static uint32_t              pl_request_dynamic_buffer(plDevice* ptDevice);
static void                  pl_return_dynamic_buffer (plDevice* ptDevice, uint32_t uNodeIndex);

// resource manager commited resources
static uint32_t              pl_create_index_buffer          (plDevice* ptDevice, size_t szSize, const void* pData, const char* pcName);
static uint32_t              pl_create_vertex_buffer         (plDevice* ptDevice, size_t szSize, size_t szStride, const void* pData, const char* pcName);
static uint32_t              pl_create_constant_buffer       (plDevice* ptDevice, size_t szSize, const char* pcName);
static uint32_t              pl_create_read_back_buffer      (plDevice* ptDevice, size_t szSize, const char* pcName);
static uint32_t              pl_create_staging_buffer        (plDevice* ptDevice, size_t szSize, const char* pcName);
static uint32_t              pl_create_texture               (plDevice* ptDevice, plTextureDesc tDesc, size_t szSize, const void* pData, const char* pcName);
static uint32_t              pl_create_storage_buffer        (plDevice* ptDevice, size_t szSize, const void* pData, const char* pcName);
static uint32_t              pl_create_texture_view          (plDevice* ptDevice, const plTextureViewDesc* ptViewDesc, const plSampler* ptSampler, uint32_t uTextureHandle, const char* pcName);
static uint32_t              pl_create_frame_buffer          (plDevice* ptDevice, plFrameBufferDesc* ptDesc, const char* pcName);
static uint32_t              pl_create_render_pass           (plDevice* ptDevice, plRenderPassDesc* ptDesc, const char* pcName);

// resource manager misc.
static void                  pl_transfer_data_to_image          (plDevice* ptDevice, plTexture* ptDest, size_t szDataSize, const void* pData);
static void                  pl_transfer_data_to_buffer         (plDevice* ptDevice, VkBuffer tDest, size_t szSize, const void* pData);
static void                  pl_transfer_image_to_buffer        (plDevice* ptDevice, VkBuffer tDest, size_t szSize, plTexture* ptDest);
static void                  pl_delete_buffer                   (plDevice* ptDevice, uint32_t uBufferIndex);
static void                  pl_submit_buffer_for_deletion      (plDevice* ptDevice, uint32_t uBufferIndex);
static void                  pl_submit_texture_for_deletion     (plDevice* ptDevice, uint32_t uTextureIndex);
static void                  pl_submit_texture_view_for_deletion(plDevice* ptDevice, uint32_t uTextureViewIndex);
static void                  pl_submit_frame_buffer_for_deletion(plDevice* ptDevice, uint32_t uFrameBufferIndex);
static void                  pl_submit_render_pass_for_deletion (plDevice* ptDevice, uint32_t uRenderPassIndex);

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static VKAPI_ATTR VkBool32 VKAPI_CALL pl__debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT tMsgSeverity, VkDebugUtilsMessageTypeFlagsEXT tMsgType, const VkDebugUtilsMessengerCallbackDataEXT* ptCallbackData, void* pUserData);
static int pl__select_physical_device(VkInstance tInstance, plDevice* ptDeviceOut);
static uint32_t pl__get_u32_max (uint32_t a, uint32_t b) { return a > b ? a : b;}
static uint32_t pl__get_u32_min (uint32_t a, uint32_t b) { return a < b ? a : b;}

// low level setup
static plFrameContext* pl__create_frame_resources(plDevice* ptDevice, uint32_t uFramesInFlight);

// low level swapchain ops
static void pl__create_framebuffers(plDevice* ptDevice, uint32_t uRenderPass, plSwapchain* ptSwapchain);

// shaders
static VkPipeline pl__create_shader_pipeline(plGraphics* ptGraphics, plGraphicsState tVariant, plVariantInfo* ptInfo);

// misc
static bool                                pl__get_free_resource_index  (uint32_t* sbuFreeIndices, uint32_t* puIndexOut);
static VkPipelineColorBlendAttachmentState pl__get_blend_state(plBlendMode tBlendMode);

static bool pl__get_free_resource_index(uint32_t* sbuFreeIndices, uint32_t* puIndexOut);

// pilotlight to vulkan conversions
static VkFilter             pl__vulkan_filter (plFilter tFilter);
static VkSamplerAddressMode pl__vulkan_wrap   (plWrapMode tWrap);
static VkCompareOp          pl__vulkan_compare(plCompareMode tCompare);
static VkFormat             pl__vulkan_format (plFormat tFormat);
static plFormat             pl__pilotlight_format (VkFormat tFormat);

// buddy allocator
static void     pl__add_node_to_freelist     (plDeviceAllocatorData* ptData, uint32_t uLevel, uint32_t uNode);
static void     pl__remove_node_from_freelist(plDeviceAllocatorData* ptData, uint32_t uLevel, uint32_t uNode);
static uint32_t pl__create_device_node       (struct plDeviceMemoryAllocatorO* ptInst, uint32_t uMemoryType);
static uint32_t pl__get_device_node          (struct plDeviceMemoryAllocatorO* ptInst, uint32_t uLevel, uint32_t uMemoryType);
//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

plRenderBackendI*
pl_load_render_backend_api(void)
{
    static plRenderBackendI tApi = 
    {
        .setup             = pl__create_instance,
        .cleanup           = pl__cleanup_backend,
        .create_device     = pl__create_device,
        .create_swapchain  = pl__create_swapchain,
        .cleanup_swapchain = pl__cleanup_swapchain,
        .cleanup_device    = pl_cleanup_device
    };
    return &tApi;
}

plDeviceMemoryApiI*
pl_load_device_memory_api(void)
{
    static plDeviceMemoryApiI tApi = {
        .create_device_local_dedicated_allocator  = pl_create_device_local_dedicated_allocator,
        .create_device_local_buddy_allocator      = pl_create_device_local_buddy_allocator,
        .create_staging_uncached_allocator        = pl_create_staging_uncached_allocator,
        .create_staging_cached_allocator          = pl_create_staging_cached_allocator,
        .cleanup_device_local_dedicated_allocator = pl_cleanup_device_local_dedicated_allocator,
        .cleanup_device_local_buddy_allocator     = pl_cleanup_device_local_buddy_allocator,
        .cleanup_staging_uncached_allocator       = pl_cleanup_staging_uncached_allocator,
        .cleanup_staging_cached_allocator         = pl_cleanup_staging_cached_allocator
    };
    return &tApi;
}

plGraphicsApiI*
pl_load_graphics_api(void)
{
    static plGraphicsApiI tApi = {
        .setup                      = pl_setup_graphics,
        .cleanup                    = pl_cleanup_graphics,
        .resize                     = pl_resize_graphics,
        .begin_frame                = pl_begin_frame,
        .end_frame                  = pl_end_frame,
        .begin_recording            = pl_begin_recording,
        .end_recording              = pl_end_recording,
        .create_shader              = pl_create_shader,
        .add_shader_variant         = pl_add_shader_variant,
        .shader_variant_exist       = pl_shader_variant_exist,
        .submit_shader_for_deletion = pl_submit_shader_for_deletion,
        .get_bind_group_layout      = pl_get_bind_group_layout,
        .get_shader                 = pl_get_shader,
        .update_bind_group          = pl_update_bind_group,
        .draw_areas                 = pl_draw_areas,
        .get_frame_resources        = pl_get_frame_resources
    };
    return &tApi;
}

plDeviceApiI*
pl_load_device_api(void)
{
    static plDeviceApiI tApi = {
        .begin_command_buffer             = pl_begin_command_buffer,
        .submit_command_buffer            = pl_submit_command_buffer,
        .find_depth_format                = pl_find_depth_format,
        .find_depth_stencil_format        = pl_find_depth_stencil_format,
        .find_supported_format            = pl_find_supported_format,
        .format_has_stencil               = pl_format_has_stencil,
        .get_max_sample_count             = pl_get_max_sample_count,
        .vulkan_format                    = pl__vulkan_format,
        .set_vulkan_object_name           = pl_set_vulkan_object_name,
        .find_memory_type                 = pl_find_memory_type,
        .transition_image_layout          = pl_transition_image_layout,
        .init                             = pl_init_device,
        .process_cleanup_queue            = pl_process_cleanup_queue,
        .create_index_buffer              = pl_create_index_buffer,
        .create_vertex_buffer             = pl_create_vertex_buffer,
        .create_constant_buffer           = pl_create_constant_buffer,
        .create_read_back_buffer          = pl_create_read_back_buffer,
        .create_staging_buffer            = pl_create_staging_buffer,
        .create_texture                   = pl_create_texture,
        .create_storage_buffer            = pl_create_storage_buffer,
        .create_frame_buffer              = pl_create_frame_buffer,
        .create_texture_view              = pl_create_texture_view,
        .create_render_pass               = pl_create_render_pass,
        .request_dynamic_buffer           = pl_request_dynamic_buffer,
        .return_dynamic_buffer            = pl_return_dynamic_buffer,
        .transfer_data_to_image           = pl_transfer_data_to_image,
        .transfer_data_to_buffer          = pl_transfer_data_to_buffer,
        .transfer_image_to_buffer         = pl_transfer_image_to_buffer,
        .submit_buffer_for_deletion       = pl_submit_buffer_for_deletion,
        .submit_texture_for_deletion      = pl_submit_texture_for_deletion,
        .submit_texture_view_for_deletion = pl_submit_texture_view_for_deletion,
        .submit_frame_buffer_for_deletion = pl_submit_frame_buffer_for_deletion,
        .submit_render_pass_for_deletion  = pl_submit_render_pass_for_deletion
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

//-----------------------------------------------------------------------------
// [SECTION] public api struct implementation
//-----------------------------------------------------------------------------

static void
pl__create_instance(plApiRegistryApiI* ptApiRegistry, plRenderBackend* ptBackend, uint32_t uVersion, bool bEnableValidation)
{
    ptBackend->ptDeviceApi = ptApiRegistry->first(PL_API_DEVICE);
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

    pl__create_instance_ex(ptBackend, uVersion, bEnableValidation ? 1 : 0, &pcKhronosValidationLayer, pl_sb_size(sbpcEnabledExtensions), sbpcEnabledExtensions);
    pl_sb_free(sbpcEnabledExtensions);
}

static void
pl__create_instance_ex(plRenderBackend* ptBackend, uint32_t uVersion, uint32_t uLayerCount, const char** ppcEnabledLayers, uint32_t uExtensioncount, const char** ppcEnabledExtensions)
{

    // check if validation should be activated
    bool bValidationEnabled = false;
    for(uint32_t i = 0; i < uLayerCount; i++)
    {
        if(strcmp("VK_LAYER_KHRONOS_validation", ppcEnabledLayers[i]) == 0)
        {
            pl_log_trace_to_f(uLogChannel, "vulkan validation enabled");
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
        ptAvailableLayers = (VkLayerProperties*)PL_ALLOC(sizeof(VkLayerProperties) * uInstanceLayersFound);
        PL_VULKAN(vkEnumerateInstanceLayerProperties(&uInstanceLayersFound, ptAvailableLayers));
    }

    // retrieve supported extensions
    uint32_t uInstanceExtensionsFound = 0u;
    VkExtensionProperties* ptAvailableExtensions = NULL;
    PL_VULKAN(vkEnumerateInstanceExtensionProperties(NULL, &uInstanceExtensionsFound, NULL));
    if(uInstanceExtensionsFound > 0)
    {
        ptAvailableExtensions = (VkExtensionProperties*)PL_ALLOC(sizeof(VkExtensionProperties) * uInstanceExtensionsFound);
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
                pl_log_trace_to_f(uLogChannel, "extension %s found", ptAvailableExtensions[j].extensionName);
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
        pl_log_error_to_f(uLogChannel, "%d %s", pl_sb_size(sbpcMissingExtensions), "Missing Extensions:");
        for(uint32_t i = 0; i < pl_sb_size(sbpcMissingExtensions); i++)
        {
            pl_log_error_to_f(uLogChannel, "  * %s", sbpcMissingExtensions[i]);
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
                pl_log_trace_to_f(uLogChannel, "layer %s found", ptAvailableLayers[j].layerName);
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
        pl_log_error_to_f(uLogChannel, "%d %s", pl_sb_size(sbpcMissingLayers), "Missing Layers:");
        for(uint32_t i = 0; i < pl_sb_size(sbpcMissingLayers); i++)
        {
            pl_log_error_to_f(uLogChannel, "  * %s", sbpcMissingLayers[i]);
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

    PL_VULKAN(vkCreateInstance(&tCreateInfo, NULL, &ptBackend->tInstance));
    pl_log_trace_to_f(uLogChannel, "created vulkan instance");

    // cleanup
    if(ptAvailableLayers)     PL_FREE(ptAvailableLayers);
    if(ptAvailableExtensions) PL_FREE(ptAvailableExtensions);
    pl_sb_free(sbpcMissingLayers);
    pl_sb_free(sbpcMissingExtensions);

    if(bValidationEnabled)
    {
        PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ptBackend->tInstance, "vkCreateDebugUtilsMessengerEXT");
        PL_ASSERT(func != NULL && "failed to set up debug messenger!");
        PL_VULKAN(func(ptBackend->tInstance, &tDebugCreateInfo, NULL, &ptBackend->tDbgMessenger));     
        pl_log_trace_to_f(uLogChannel, "enabled Vulkan validation layers");
    }
}

static void
pl__cleanup_backend(plApiRegistryApiI* ptApiRegistry, plRenderBackend* ptBackend)
{
    if(ptBackend->tDbgMessenger)
    {
        PFN_vkDestroyDebugUtilsMessengerEXT tFunc = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ptBackend->tInstance, "vkDestroyDebugUtilsMessengerEXT");
        if (tFunc != NULL)
            tFunc(ptBackend->tInstance, ptBackend->tDbgMessenger, NULL);
    }

    // destroy tSurface
    vkDestroySurfaceKHR(ptBackend->tInstance, ptBackend->tSurface, NULL);

    // destroy tInstance
    vkDestroyInstance(ptBackend->tInstance, NULL);
}

static void
pl__create_device(plRenderBackend* ptBackend, VkSurfaceKHR tSurface, bool bEnableValidation, plDevice* ptDeviceOut)
{
    ptDeviceOut->iGraphicsQueueFamily = -1;
    ptDeviceOut->iPresentQueueFamily = -1;
    int iDeviceIndex = -1;
    ptDeviceOut->tMemProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    ptDeviceOut->tMemBudgetInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
    ptDeviceOut->tMemProps2.pNext = &ptDeviceOut->tMemBudgetInfo;
    iDeviceIndex = pl__select_physical_device(ptBackend->tInstance, ptDeviceOut);
    ptDeviceOut->tMaxLocalMemSize = ptDeviceOut->tMemProps.memoryHeaps[iDeviceIndex].size;
    ptDeviceOut->uUniformBufferBlockSize = pl_minu(PL_DEVICE_ALLOCATION_BLOCK_SIZE, ptDeviceOut->tDeviceProps.limits.maxUniformBufferRange);

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

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~debug markers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	ptDeviceOut->vkDebugMarkerSetObjectTag  = (PFN_vkDebugMarkerSetObjectTagEXT)vkGetDeviceProcAddr(ptDeviceOut->tLogicalDevice, "vkDebugMarkerSetObjectTagEXT");
	ptDeviceOut->vkDebugMarkerSetObjectName = (PFN_vkDebugMarkerSetObjectNameEXT)vkGetDeviceProcAddr(ptDeviceOut->tLogicalDevice, "vkDebugMarkerSetObjectNameEXT");
	ptDeviceOut->vkCmdDebugMarkerBegin      = (PFN_vkCmdDebugMarkerBeginEXT)vkGetDeviceProcAddr(ptDeviceOut->tLogicalDevice, "vkCmdDebugMarkerBeginEXT");
	ptDeviceOut->vkCmdDebugMarkerEnd        = (PFN_vkCmdDebugMarkerEndEXT)vkGetDeviceProcAddr(ptDeviceOut->tLogicalDevice, "vkCmdDebugMarkerEndEXT");
	ptDeviceOut->vkCmdDebugMarkerInsert     = (PFN_vkCmdDebugMarkerInsertEXT)vkGetDeviceProcAddr(ptDeviceOut->tLogicalDevice, "vkCmdDebugMarkerInsertEXT");

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~debug markers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    const VkCommandPoolCreateInfo tCommandPoolInfo = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = ptDeviceOut->iGraphicsQueueFamily,
        .flags            = 0
    };
    PL_VULKAN(vkCreateCommandPool(ptDeviceOut->tLogicalDevice, &tCommandPoolInfo, NULL, &ptDeviceOut->tCmdPool));
}

static void
pl__create_swapchain(plRenderBackend* ptBackend, plDevice* ptDevice, VkSurfaceKHR tSurface, uint32_t uWidth, uint32_t uHeight, plSwapchain* ptSwapchainOut)
{
    vkDeviceWaitIdle(ptDevice->tLogicalDevice);

    ptSwapchainOut->tMsaaSamples = ptBackend->ptDeviceApi->get_max_sample_count(ptDevice);

    //-----------------------------------------------------------------------------
    // query swapchain support
    //----------------------------------------------------------------------------- 

    VkSurfaceCapabilitiesKHR tCapabilities = {0};
    PL_VULKAN(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ptDevice->tPhysicalDevice, tSurface, &tCapabilities));

    uint32_t uFormatCount = 0u;
    PL_VULKAN(vkGetPhysicalDeviceSurfaceFormatsKHR(ptDevice->tPhysicalDevice, tSurface, &uFormatCount, NULL));
    
    if(uFormatCount >ptSwapchainOut->uSurfaceFormatCapacity_)
    {
        if(ptSwapchainOut->ptSurfaceFormats_) PL_FREE(ptSwapchainOut->ptSurfaceFormats_);

        ptSwapchainOut->ptSurfaceFormats_ = PL_ALLOC(sizeof(VkSurfaceFormatKHR) * uFormatCount);
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
    ptSwapchainOut->tFormat = pl__pilotlight_format(tSurfaceFormat.format);

    for(uint32_t i = 0u; i < 4; i++)
    {
        if(bPreferenceFound) break;
        
        for(uint32_t j = 0u; j < uFormatCount; j++)
        {
            if(ptSwapchainOut->ptSurfaceFormats_[j].format == atSurfaceFormatPreference[i] && ptSwapchainOut->ptSurfaceFormats_[j].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                tSurfaceFormat = ptSwapchainOut->ptSurfaceFormats_[j];
                ptSwapchainOut->tFormat = pl__pilotlight_format(tSurfaceFormat.format);
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
            pl_submit_texture_view_for_deletion(ptDevice, ptSwapchainOut->puImageViews[i]);
            pl_submit_frame_buffer_for_deletion(ptDevice, ptSwapchainOut->puFrameBuffers[i]);
        }
        vkDestroySwapchainKHR(ptDevice->tLogicalDevice, tOldSwapChain, NULL);
    }

    PL_VULKAN(vkGetSwapchainImagesKHR(ptDevice->tLogicalDevice, ptSwapchainOut->tSwapChain, &ptSwapchainOut->uImageCount, NULL));
    if(ptSwapchainOut->uImageCount > ptSwapchainOut->uImageCapacity)
    {
        ptSwapchainOut->uImageCapacity = ptSwapchainOut->uImageCount;
        if(ptSwapchainOut->ptImages)       PL_FREE(ptSwapchainOut->ptImages);
        if(ptSwapchainOut->puImageViews)   PL_FREE(ptSwapchainOut->puImageViews);
        if(ptSwapchainOut->puFrameBuffers) PL_FREE(ptSwapchainOut->puFrameBuffers);
        ptSwapchainOut->ptImages         = PL_ALLOC(sizeof(VkImage)*ptSwapchainOut->uImageCapacity);
        ptSwapchainOut->puImageViews     = PL_ALLOC(sizeof(uint32_t)*ptSwapchainOut->uImageCapacity);
        ptSwapchainOut->puFrameBuffers   = PL_ALLOC(sizeof(uint32_t)*ptSwapchainOut->uImageCapacity);
    }
    PL_VULKAN(vkGetSwapchainImagesKHR(ptDevice->tLogicalDevice, ptSwapchainOut->tSwapChain, &ptSwapchainOut->uImageCount, ptSwapchainOut->ptImages));

    for(uint32_t i = 0; i < ptSwapchainOut->uImageCount; i++)
    {

        VkImageViewCreateInfo tViewInfo = {
            .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image            = ptSwapchainOut->ptImages[i],
            .viewType         = VK_IMAGE_VIEW_TYPE_2D,
            .format           = pl__vulkan_format(ptSwapchainOut->tFormat),
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

        uint32_t ulTextureViewIndex = 0u;
        if(!pl__get_free_resource_index(ptDevice->_sbulTextureViewFreeIndices, &ulTextureViewIndex))
            ulTextureViewIndex = pl_sb_add_n(ptDevice->sbtTextureViews, 1);

        PL_VULKAN(vkCreateImageView(ptDevice->tLogicalDevice, &tViewInfo, NULL, &ptDevice->sbtTextureViews[ulTextureViewIndex]._tImageView));
        ptSwapchainOut->puImageViews[i] = ulTextureViewIndex;  
    }  //-V1020

    // color & depth
    if(ptSwapchainOut->uColorTextureView)  pl_submit_texture_view_for_deletion(ptDevice, ptSwapchainOut->uColorTextureView);
    if(ptSwapchainOut->uColorTexture)      pl_submit_texture_for_deletion(ptDevice, ptSwapchainOut->uColorTexture);
    if(ptSwapchainOut->uDepthTextureView)  pl_submit_texture_view_for_deletion(ptDevice, ptSwapchainOut->uDepthTextureView);
    if(ptSwapchainOut->uDepthTexture)      pl_submit_texture_for_deletion(ptDevice, ptSwapchainOut->uDepthTexture);
    ptSwapchainOut->uColorTextureView = 0;
    ptSwapchainOut->uColorTexture     = 0;
    ptSwapchainOut->uDepthTextureView = 0;
    ptSwapchainOut->uDepthTexture     = 0;

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
        .format        = pl__vulkan_format(ptBackend->ptDeviceApi->find_depth_stencil_format(ptDevice)),
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
        .format        = pl__vulkan_format(ptSwapchainOut->tFormat),
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .samples       = ptSwapchainOut->tMsaaSamples,
        .flags         = 0
    };

        // find free index
    uint32_t ulDepthTextureIndex = 0u;
    if(!pl__get_free_resource_index(ptDevice->_sbulTextureFreeIndices, &ulDepthTextureIndex))
        ulDepthTextureIndex = pl_sb_add_n(ptDevice->sbtTextures, 1);

    uint32_t ulDepthTextureViewIndex = 0u;
    if(!pl__get_free_resource_index(ptDevice->_sbulTextureViewFreeIndices, &ulDepthTextureViewIndex))
        ulDepthTextureViewIndex = pl_sb_add_n(ptDevice->sbtTextureViews, 1);

    uint32_t ulColorTextureIndex = 0u;
    if(!pl__get_free_resource_index(ptDevice->_sbulTextureFreeIndices, &ulColorTextureIndex))
        ulColorTextureIndex = pl_sb_add_n(ptDevice->sbtTextures, 1);

    uint32_t ulColorTextureViewIndex = 0u;
    if(!pl__get_free_resource_index(ptDevice->_sbulTextureViewFreeIndices, &ulColorTextureViewIndex))
        ulColorTextureViewIndex = pl_sb_add_n(ptDevice->sbtTextureViews, 1);

    PL_VULKAN(vkCreateImage(ptDevice->tLogicalDevice, &tDepthImageInfo, NULL, &ptDevice->sbtTextures[ulDepthTextureIndex].tImage));
    PL_VULKAN(vkCreateImage(ptDevice->tLogicalDevice, &tColorImageInfo, NULL, &ptDevice->sbtTextures[ulColorTextureIndex].tImage));

    VkMemoryRequirements tDepthMemReqs = {0};
    VkMemoryRequirements tColorMemReqs = {0};
    vkGetImageMemoryRequirements(ptDevice->tLogicalDevice, ptDevice->sbtTextures[ulDepthTextureIndex].tImage, &tDepthMemReqs);
    vkGetImageMemoryRequirements(ptDevice->tLogicalDevice, ptDevice->sbtTextures[ulColorTextureIndex].tImage, &tColorMemReqs);

    plDeviceMemoryAllocatorI* ptAllocator = tDepthMemReqs.size > PL_DEVICE_ALLOCATION_BLOCK_SIZE ? &ptDevice->tLocalDedicatedAllocator : &ptDevice->tLocalBuddyAllocator;
    ptDevice->sbtTextures[ulColorTextureIndex].tAllocation = ptAllocator->allocate(ptAllocator->ptInst, tColorMemReqs.memoryTypeBits, tColorMemReqs.size, tColorMemReqs.alignment, "swapchain color");
    ptDevice->sbtTextures[ulDepthTextureIndex].tAllocation = ptAllocator->allocate(ptAllocator->ptInst, tDepthMemReqs.memoryTypeBits, tDepthMemReqs.size, tDepthMemReqs.alignment, "swapchain depth");

    PL_VULKAN(vkBindImageMemory(ptDevice->tLogicalDevice, ptDevice->sbtTextures[ulDepthTextureIndex].tImage, (VkDeviceMemory)ptDevice->sbtTextures[ulDepthTextureIndex].tAllocation.tMemory, ptDevice->sbtTextures[ulDepthTextureIndex].tAllocation.ulOffset));
    PL_VULKAN(vkBindImageMemory(ptDevice->tLogicalDevice, ptDevice->sbtTextures[ulColorTextureIndex].tImage, (VkDeviceMemory)ptDevice->sbtTextures[ulColorTextureIndex].tAllocation.tMemory, ptDevice->sbtTextures[ulColorTextureIndex].tAllocation.ulOffset));

    VkCommandBuffer tCommandBuffer = ptBackend->ptDeviceApi->begin_command_buffer(ptDevice, ptDevice->tCmdPool);
    VkImageSubresourceRange tRange = {
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1,
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT
    };

    ptBackend->ptDeviceApi->transition_image_layout(tCommandBuffer, ptDevice->sbtTextures[ulColorTextureIndex].tImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, tRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    tRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    ptBackend->ptDeviceApi->transition_image_layout(tCommandBuffer, ptDevice->sbtTextures[ulDepthTextureIndex].tImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, tRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    ptBackend->ptDeviceApi->submit_command_buffer(ptDevice, ptDevice->tCmdPool, tCommandBuffer);

    VkImageViewCreateInfo tDepthViewInfo = {
        .sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image                           = ptDevice->sbtTextures[ulDepthTextureIndex].tImage,
        .viewType                        = VK_IMAGE_VIEW_TYPE_2D,
        .format                          = tDepthImageInfo.format,
        .subresourceRange.baseMipLevel   = 0,
        .subresourceRange.levelCount     = 1,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount     = 1,
        .subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT,
    };

    if(ptBackend->ptDeviceApi->format_has_stencil(tDepthViewInfo.format))
        tDepthViewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

    VkImageViewCreateInfo tColorViewInfo = {
        .sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image                           = ptDevice->sbtTextures[ulColorTextureIndex].tImage,
        .viewType                        = VK_IMAGE_VIEW_TYPE_2D,
        .format                          = tColorImageInfo.format,
        .subresourceRange.baseMipLevel   = 0,
        .subresourceRange.levelCount     = 1,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount     = 1,
        .subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
    };

    PL_VULKAN(vkCreateImageView(ptDevice->tLogicalDevice, &tDepthViewInfo, NULL, &ptDevice->sbtTextureViews[ulDepthTextureViewIndex]._tImageView));
    PL_VULKAN(vkCreateImageView(ptDevice->tLogicalDevice, &tColorViewInfo, NULL, &ptDevice->sbtTextureViews[ulColorTextureViewIndex]._tImageView));

    ptSwapchainOut->uColorTexture = ulColorTextureIndex;
    ptSwapchainOut->uColorTextureView = ulColorTextureViewIndex;
    ptSwapchainOut->uDepthTexture = ulDepthTextureIndex;
    ptSwapchainOut->uDepthTextureView = ulDepthTextureViewIndex;
}

static void
pl__cleanup_swapchain(plRenderBackend* ptBackend, plDevice* ptDevice, plSwapchain* ptSwapchain)
{
    VkDevice tLogicalDevice = ptDevice->tLogicalDevice;

    // destroy default render pass
    vkDestroySwapchainKHR(tLogicalDevice, ptSwapchain->tSwapChain, NULL);

    PL_FREE(ptSwapchain->ptSurfaceFormats_);
    PL_FREE(ptSwapchain->ptImages);
    PL_FREE(ptSwapchain->puImageViews);
    PL_FREE(ptSwapchain->puFrameBuffers);
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
    static VkDescriptorSetLayoutBinding* sbtDescriptorSetLayoutBindings = NULL;
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
    pl_sb_reset(sbtDescriptorSetLayoutBindings);

    return tDescriptorSetLayout;
}

static void
pl_setup_graphics(plGraphics* ptGraphics, plRenderBackend* ptBackend, plApiRegistryApiI* ptApiRegistry, plTempAllocator* ptAllocator)
{
    plDataRegistryApiI* ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);

    if(ptGraphics->uFramesInFlight == 0)
        ptGraphics->uFramesInFlight = 2;
    
    // retrieve required apis
    ptGraphics->ptFileApi = ptApiRegistry->first(PL_API_FILE);
    ptGraphics->ptDeviceApi = ptApiRegistry->first(PL_API_DEVICE);
    ptGraphics->_ptDescriptorApi = ptApiRegistry->first(PL_API_DESCRIPTOR_MANAGER);
    ptGraphics->ptBackendApi = ptApiRegistry->first(PL_API_BACKEND_VULKAN);
    
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~create surface~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    plIOContext* ptIOCtx = pl_get_io_context();

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~devices~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // select physical device & create logical device
    plDevice* ptDevice = &ptGraphics->tDevice;
    VkDevice tLogicalDevice = ptDevice->tLogicalDevice;
    ptGraphics->_tDescriptorManager.tDevice = tLogicalDevice;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~swapchain~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    
    plSwapchain* ptSwapchain = &ptGraphics->tSwapchain;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~main renderpass~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const VkAttachmentDescription atAttachments[] = {

        // multisampled color attachment (render to this)
        {
            .format         = pl__vulkan_format(ptSwapchain->tFormat),
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
            .format         = pl__vulkan_format(ptGraphics->ptDeviceApi->find_depth_stencil_format(ptDevice)),
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
            .format         = pl__vulkan_format(ptSwapchain->tFormat),
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

    ptGraphics->uRenderPass = 0u;
    if(!pl__get_free_resource_index(ptDevice->_sbulFrameBufferFreeIndices, &ptGraphics->uRenderPass))
        ptGraphics->uRenderPass = pl_sb_add_n(ptDevice->sbtRenderPasses, 1);
    PL_VULKAN(vkCreateRenderPass(tLogicalDevice, &tRenderPassInfo, NULL, &ptDevice->sbtRenderPasses[ptGraphics->uRenderPass]._tRenderPass));

    ptDevice->sbtRenderPasses[ptGraphics->uRenderPass].tDesc.tColorFormat = ptSwapchain->tFormat;
    ptDevice->sbtRenderPasses[ptGraphics->uRenderPass].tDesc.tDepthFormat = ptGraphics->ptDeviceApi->find_depth_stencil_format(ptDevice);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~frame buffers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    pl__create_framebuffers(ptDevice, ptGraphics->uRenderPass, ptSwapchain);
    
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~per-frame resources~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    ptGraphics->sbFrames = pl__create_frame_resources(ptDevice, ptGraphics->uFramesInFlight);

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
}

static bool
pl_begin_frame(plGraphics* ptGraphics)
{
    pl_begin_profile_sample(__FUNCTION__);
    plIOContext* ptIOCtx = pl_get_io_context();
    plDevice* ptDevice = &ptGraphics->tDevice;
    VkDevice tLogicalDevice = ptGraphics->tDevice.tLogicalDevice;

    // buffer cleanup
    pl_sb_reset(ptGraphics->_sbulTempQueue);

    bool bNeedUpdate = false;

    bNeedUpdate = false;

    for(uint32_t i = 0; i < pl_sb_size(ptGraphics->_sbulShaderDeletionQueue); i++)
    {
        const uint32_t uShaderIndex = ptGraphics->_sbulShaderDeletionQueue[i];

        plShader* ptShader = &ptGraphics->sbtShaders[uShaderIndex];

        // we are hiding the frame
        if(ptGraphics->_sbulShaderHashes[uShaderIndex] > 0)
            ptGraphics->_sbulShaderHashes[uShaderIndex] -= 1;

        if(ptGraphics->_sbulShaderHashes[uShaderIndex] == 0)
        {
            PL_FREE((uint32_t*)ptShader->tPixelShaderInfo.pCode);
            PL_FREE((uint32_t*)ptShader->tVertexShaderInfo.pCode);
            vkDestroyPipelineLayout(tLogicalDevice, ptShader->tPipelineLayout, NULL);

            for(uint32_t uVariantIndex = 0; uVariantIndex < pl_sb_size(ptShader->_sbuVariantPipelines); uVariantIndex++)
                vkDestroyPipeline(tLogicalDevice, ptGraphics->sbtShaderVariants[ptShader->_sbuVariantPipelines[uVariantIndex]].tPipeline, NULL);
            ptShader->tPipelineLayout = VK_NULL_HANDLE;
            pl_sb_free(ptShader->_sbuVariantPipelines);

            // add to free indices
            pl_sb_push(ptGraphics->_sbulShaderFreeIndices, uShaderIndex);
            bNeedUpdate = true;
        }
        else
        {
            pl_sb_push(ptGraphics->_sbulTempQueue, uShaderIndex);
        }
    }

    if(bNeedUpdate)
    {
        // copy temporary queue data over
        pl_sb_reset(ptGraphics->_sbulShaderDeletionQueue);
        pl_sb_resize(ptGraphics->_sbulShaderDeletionQueue, pl_sb_size(ptGraphics->_sbulTempQueue));
        if(ptGraphics->_sbulTempQueue)
            memcpy(ptGraphics->_sbulShaderDeletionQueue, ptGraphics->_sbulTempQueue, pl_sb_size(ptGraphics->_sbulTempQueue) * sizeof(uint32_t));
    }

    plFrameContext* ptCurrentFrame = pl_get_frame_resources(ptGraphics);

    PL_VULKAN(vkWaitForFences(tLogicalDevice, 1, &ptCurrentFrame->tInFlight, VK_TRUE, UINT64_MAX));
    VkResult err = vkAcquireNextImageKHR(tLogicalDevice, ptGraphics->tSwapchain.tSwapChain, UINT64_MAX, ptCurrentFrame->tImageAvailable, VK_NULL_HANDLE, &ptGraphics->tSwapchain.uCurrentImageIndex);
    if(err == VK_SUBOPTIMAL_KHR || err == VK_ERROR_OUT_OF_DATE_KHR)
    {
        if(err == VK_ERROR_OUT_OF_DATE_KHR)
        {
            ptGraphics->ptBackendApi->create_swapchain(ptGraphics->ptBackend, &ptGraphics->tDevice, ptGraphics->ptBackend->tSurface, (uint32_t)ptIOCtx->afMainViewportSize[0], (uint32_t)ptIOCtx->afMainViewportSize[1], &ptGraphics->tSwapchain);
            pl__create_framebuffers(ptDevice, ptGraphics->uRenderPass, &ptGraphics->tSwapchain);
            pl_end_profile_sample();
            return false;
        }
    }
    else
    {
        PL_VULKAN(err);
    }

    if (ptCurrentFrame->tInFlight != VK_NULL_HANDLE)
        PL_VULKAN(vkWaitForFences(tLogicalDevice, 1, &ptCurrentFrame->tInFlight, VK_TRUE, UINT64_MAX));

    pl_end_profile_sample();
    return true; 
}

static void
pl_end_frame(plGraphics* ptGraphics)
{
    pl_begin_profile_sample(__FUNCTION__);
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
        plIOContext* ptIOCtx = pl_get_io_context();
        ptGraphics->ptBackendApi->create_swapchain(ptGraphics->ptBackend, &ptGraphics->tDevice, ptGraphics->ptBackend->tSurface, (uint32_t)ptIOCtx->afMainViewportSize[0], (uint32_t)ptIOCtx->afMainViewportSize[1], &ptGraphics->tSwapchain);
        pl__create_framebuffers(ptDevice, ptGraphics->uRenderPass, &ptGraphics->tSwapchain);
    }
    else
    {
        PL_VULKAN(tResult);
    }

    ptGraphics->szCurrentFrameIndex = (ptGraphics->szCurrentFrameIndex + 1) % ptGraphics->uFramesInFlight;
    pl_end_profile_sample();
}

static void
pl_resize_graphics(plGraphics* ptGraphics)
{
    plIOContext* ptIOCtx = pl_get_io_context();
    plDevice* ptDevice = &ptGraphics->tDevice;
    ptGraphics->ptBackendApi->create_swapchain(ptGraphics->ptBackend, &ptGraphics->tDevice, ptGraphics->ptBackend->tSurface, (uint32_t)ptIOCtx->afMainViewportSize[0], (uint32_t)ptIOCtx->afMainViewportSize[1], &ptGraphics->tSwapchain);
    pl__create_framebuffers(ptDevice, ptGraphics->uRenderPass, &ptGraphics->tSwapchain);
    ptGraphics->szCurrentFrameIndex = 0;
}

static void
pl_begin_recording(plGraphics* ptGraphics)
{
    const plFrameContext* ptCurrentFrame = pl_get_frame_resources(ptGraphics);

    const VkCommandBufferBeginInfo tBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    };
    PL_VULKAN(vkResetCommandPool(ptGraphics->tDevice.tLogicalDevice, ptCurrentFrame->tCmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT));
    PL_VULKAN(vkBeginCommandBuffer(ptCurrentFrame->tCmdBuf, &tBeginInfo));    
}

static void
pl_end_recording(plGraphics* ptGraphics)
{
    const plFrameContext* ptCurrentFrame = pl_get_frame_resources(ptGraphics);
    PL_VULKAN(vkEndCommandBuffer(ptCurrentFrame->tCmdBuf));
}

static uint32_t
pl_create_shader(plGraphics* ptGraphics, const plShaderDesc* ptDescIn)
{
    PL_ASSERT(ptDescIn->uBindGroupLayoutCount < 5 && "only 4 descriptor sets allowed per pipeline.");

    plDevice* ptDevice   = &ptGraphics->tDevice;

    plShader tShader = {
        .tDesc = *ptDescIn
    };

    // fill out descriptor set layouts
    for(uint32_t i = 0; i < tShader.tDesc.uBindGroupLayoutCount; i++)
        tShader.tDesc.atBindGroupLayouts[i]._tDescriptorSetLayout = ptGraphics->_ptDescriptorApi->request_layout(&ptGraphics->_tDescriptorManager, &tShader.tDesc.atBindGroupLayouts[i]);

    // place "default" graphics state into the variant list (for consolidation)
    const plShaderVariant tNewVariant = {
        .tGraphicsState   = tShader.tDesc.tGraphicsState,
        .uRenderPass      = ptGraphics->uRenderPass,
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
    for(uint32_t i = 0; i < pl_sb_size(ptGraphics->_sbulShaderHashes); i++)
    {
        if(ptGraphics->_sbulShaderHashes[i] == uHash)
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
    char* pcVertexByteCode = PL_ALLOC(uVertexByteCodeSize);
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
    char* pcByteCode = PL_ALLOC(uByteCodeSize);
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
            .uRenderPass         = tShader.tDesc.sbtVariants[i].uRenderPass,
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
            .tPipeline       = pl__create_shader_pipeline(ptGraphics, tShader.tDesc.sbtVariants[i].tGraphicsState, &tVariantInfo),
            .tGraphicsState  = tShader.tDesc.sbtVariants[i].tGraphicsState,
            .uRenderPass     = tShader.tDesc.sbtVariants[i].uRenderPass,
            .tMSAASampleCount= tShader.tDesc.sbtVariants[i].tMSAASampleCount
        };
        pl_sb_push(ptGraphics->sbtShaderVariants, tShaderVariant);
        pl_sb_push(tShader._sbuVariantPipelines, pl_sb_size(ptGraphics->sbtShaderVariants) - 1);
    }

    // find free index
    uint32_t uShaderIndex = 0u;
    if(!pl__get_free_resource_index(ptGraphics->_sbulShaderFreeIndices, &uShaderIndex))
    {
        uShaderIndex = pl_sb_add_n(ptGraphics->sbtShaders, 1);
        pl_sb_add_n(ptGraphics->_sbulShaderHashes, 1);
    }
    ptGraphics->sbtShaders[uShaderIndex] = tShader;
    ptGraphics->_sbulShaderHashes[uShaderIndex] = uHash;

    PL_ASSERT(pl_sb_size(ptGraphics->sbtShaders) == pl_sb_size(ptGraphics->_sbulShaderHashes));
    
    return uShaderIndex;
}

static uint32_t
pl_add_shader_variant(plGraphics* ptGraphics, uint32_t uShader, plGraphicsState tVariant, uint32_t uRenderPass, VkSampleCountFlagBits tMSAASampleCount)
{
    plShader* ptShader = &ptGraphics->sbtShaders[uShader];

    // check if variant exist already
    for(uint32_t i = 0; i < pl_sb_size(ptShader->tDesc.sbtVariants); i++)
    {
        if(ptShader->tDesc.sbtVariants[i].tGraphicsState.ulValue == tVariant.ulValue 
            && uRenderPass == ptShader->tDesc.sbtVariants[i].uRenderPass 
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
        .uRenderPass         = uRenderPass,
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
        .tPipelineLayout  = ptShader->tPipelineLayout,
        .tPipeline        = pl__create_shader_pipeline(ptGraphics, tVariant, &tVariantInfo),
        .tGraphicsState   = tVariant,
        .uRenderPass      = uRenderPass,
        .tMSAASampleCount = tMSAASampleCount
    };
    pl_sb_push(ptShader->tDesc.sbtVariants, tShaderVariant);
    pl_sb_push(ptGraphics->sbtShaderVariants, tShaderVariant);
    pl_sb_push(ptShader->_sbuVariantPipelines, pl_sb_size(ptGraphics->sbtShaderVariants) - 1);
    return pl_sb_size(ptGraphics->sbtShaderVariants) - 1;
}

static bool
pl_shader_variant_exist(plGraphics* ptGraphics, uint32_t uShader, plGraphicsState tVariant, uint32_t uRenderPass, VkSampleCountFlagBits tMSAASampleCount)
{
    plShader* ptShader = &ptGraphics->sbtShaders[uShader];
    for(uint32_t i = 0; i < pl_sb_size(ptShader->tDesc.sbtVariants); i++)
    {
        if(ptShader->tDesc.sbtVariants[i].tGraphicsState.ulValue == tVariant.ulValue 
            && uRenderPass == ptShader->tDesc.sbtVariants[i].uRenderPass
            && tMSAASampleCount == ptShader->tDesc.sbtVariants[i].tMSAASampleCount)
            return true;
    }
    return false;
}

static void
pl_submit_shader_for_deletion(plGraphics* ptGraphics, uint32_t uShaderIndex)
{

    PL_ASSERT(uShaderIndex < pl_sb_size(ptGraphics->sbtShaders)); 
    pl_sb_push(ptGraphics->_sbulShaderDeletionQueue, uShaderIndex);

    // using shader hash to store frame this buffer is ok to free
    ptGraphics->_sbulShaderHashes[uShaderIndex] = ptGraphics->uFramesInFlight;   
}

static plBindGroupLayout*
pl_get_bind_group_layout(plGraphics* ptGraphics, uint32_t uShaderIndex, uint32_t uBindGroupIndex)
{
    PL_ASSERT(uShaderIndex < pl_sb_size(ptGraphics->sbtShaders)); 
    PL_ASSERT(uBindGroupIndex < ptGraphics->sbtShaders[uShaderIndex].tDesc.uBindGroupLayoutCount); 
    return &ptGraphics->sbtShaders[uShaderIndex].tDesc.atBindGroupLayouts[uBindGroupIndex];
}

static plShaderVariant*
pl_get_shader(plGraphics* ptGraphics, uint32_t uVariantIndex)
{
    return &ptGraphics->sbtShaderVariants[uVariantIndex];
}

static void
pl_update_bind_group(plGraphics* ptGraphics, plBindGroup* ptGroup, uint32_t uBufferCount, uint32_t* auBuffers, size_t* aszBufferRanges, uint32_t uTextureViewCount, uint32_t* auTextureViews)
{
    // PL_ASSERT(uBufferCount == ptGroup->tLayout.uBufferCount && "bind group buffer count & update buffer count must match.");
    PL_ASSERT(uTextureViewCount == ptGroup->tLayout.uTextureCount && "bind group texture count & update texture view count must match.");

    plDevice* ptDevice = &ptGraphics->tDevice;
    VkDevice tLogicalDevice = ptGraphics->tDevice.tLogicalDevice;
    plDescriptorManager* ptDescManager = &ptGraphics->_tDescriptorManager;
    plDescriptorManagerApiI* ptDescApi = ptGraphics->_ptDescriptorApi;

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

        const plBuffer* ptBuffer = &ptDevice->sbtBuffers[auBuffers[i]];

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

        const plTextureView* ptTextureView = &ptDevice->sbtTextureViews[auTextureViews[i]];

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
}

static void
pl_draw_areas(plGraphics* ptGraphics, uint32_t uAreaCount, plDrawArea* atAreas, plDraw* atDraws)
{
    pl_begin_profile_sample(__FUNCTION__);
    const plFrameContext* ptCurrentFrame = pl_get_frame_resources(ptGraphics);
    static VkDeviceSize tOffsets = { 0 };
    vkCmdSetDepthBias(ptCurrentFrame->tCmdBuf, 0.0f, 0.0f, 0.0f);

    plDevice* ptDevice = &ptGraphics->tDevice;
    plBindGroup* ptCurrentBindGroup0 = NULL;
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
                plShaderVariant* ptPipeline = pl_get_shader(ptGraphics, ptDraw->uShaderVariant);
                vkCmdBindPipeline(ptCurrentFrame->tCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, ptPipeline->tPipeline);
                uCurrentVariant = ptDraw->uShaderVariant;
            }

            plShaderVariant* ptVariant = pl_get_shader(ptGraphics, uCurrentVariant);

            // mesh
            if(ptDraw->ptMesh->uIndexBuffer != uCurrentIndexBuffer)
            {
                uCurrentIndexBuffer = ptDraw->ptMesh->uIndexBuffer;
                vkCmdBindIndexBuffer(ptCurrentFrame->tCmdBuf, ptDevice->sbtBuffers[ptDraw->ptMesh->uIndexBuffer].tBuffer, 0, VK_INDEX_TYPE_UINT32);
            }

            if(ptDraw->ptMesh->uVertexBuffer != uCurrentVertexBuffer)
            {
                uCurrentVertexBuffer = ptDraw->ptMesh->uVertexBuffer;
                vkCmdBindVertexBuffers(ptCurrentFrame->tCmdBuf, 0, 1, &ptDevice->sbtBuffers[ptDraw->ptMesh->uVertexBuffer].tBuffer, &tOffsets);
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

            if(ptDraw->ptBindGroup1)
            {
                auDynamicOffsets[uSetCounter] = ptDraw->uDynamicBufferOffset1;
                atUpdateSets[uSetCounter++] = ptDraw->ptBindGroup1->_tDescriptorSet;
                uFirstSet = 1;
            }

            if(ptDraw->ptBindGroup2)
            {
                auDynamicOffsets[uSetCounter] = ptDraw->uDynamicBufferOffset2;
                atUpdateSets[uSetCounter++] = ptDraw->ptBindGroup2->_tDescriptorSet;
                uFirstSet = uFirstSet == 0 ? 2 : 1;
            }

            if(uSetCounter > 0)
                vkCmdBindDescriptorSets(ptCurrentFrame->tCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, ptVariant->tPipelineLayout, uFirstSet, uSetCounter, atUpdateSets, uSetCounter, auDynamicOffsets);

            vkCmdDrawIndexed(ptCurrentFrame->tCmdBuf, ptDraw->ptMesh->uIndexCount, 1, ptDraw->ptMesh->uIndexOffset, ptDraw->ptMesh->uVertexOffset, 0);
        }

    }
    pl_end_profile_sample();
}

static void
pl_cleanup_graphics(plGraphics* ptGraphics)
{

    plDevice* ptDevice = &ptGraphics->tDevice;
    VkDevice tLogicalDevice = ptDevice->tLogicalDevice;

    // ensure device is finished
    vkDeviceWaitIdle(ptDevice->tLogicalDevice);

    ptGraphics->_ptDescriptorApi->cleanup(&ptGraphics->_tDescriptorManager);

    for(uint32_t i = 0; i < pl_sb_size(ptGraphics->sbtShaders); i++)
    {

        if(ptGraphics->_sbulShaderHashes[i] > 0)
            pl_submit_shader_for_deletion(ptGraphics, i);
    }

    uint32_t uFramesToProcess = ptGraphics->uFramesInFlight;
    for(uint32_t i = 0; i < pl_sb_size(ptGraphics->_sbulShaderDeletionQueue); i++)
    {
        const uint32_t uShaderIndex = ptGraphics->_sbulShaderDeletionQueue[i];

        plShader* ptShader = &ptGraphics->sbtShaders[uShaderIndex];

        // we are hiding the frame
        if(ptGraphics->_sbulShaderHashes[uShaderIndex] < uFramesToProcess)
            ptGraphics->_sbulShaderHashes[uShaderIndex] = 0;
        else
            ptGraphics->_sbulShaderHashes[uShaderIndex] -= uFramesToProcess;

        if(ptGraphics->_sbulShaderHashes[uShaderIndex] == 0)
        {

            PL_FREE((uint32_t*)ptShader->tPixelShaderInfo.pCode);
            PL_FREE((uint32_t*)ptShader->tVertexShaderInfo.pCode);
            vkDestroyPipelineLayout(ptGraphics->tDevice.tLogicalDevice, ptShader->tPipelineLayout, NULL);

            for(uint32_t uVariantIndex = 0; uVariantIndex < pl_sb_size(ptShader->_sbuVariantPipelines); uVariantIndex++)
                vkDestroyPipeline(ptGraphics->tDevice.tLogicalDevice, ptGraphics->sbtShaderVariants[ptShader->_sbuVariantPipelines[uVariantIndex]].tPipeline, NULL);
            ptShader->tPipelineLayout = VK_NULL_HANDLE;
        }

        pl_sb_free(ptGraphics->sbtShaders[i].tDesc.sbtVariants);
        pl_sb_free(ptGraphics->sbtShaders[i]._sbuVariantPipelines);
    }

    pl_sb_free(ptGraphics->sbtShaders);
    pl_sb_free(ptGraphics->sbtShaderVariants);
    pl_sb_free(ptGraphics->_sbulShaderFreeIndices);
    pl_sb_free(ptGraphics->_sbulShaderDeletionQueue);
    pl_sb_free(ptGraphics->_sbulShaderHashes);
    pl_sb_free(ptGraphics->_sbulTempQueue);
    
    // destroy swapchain
    vkDestroyDescriptorPool(tLogicalDevice, ptGraphics->tDescriptorPool, NULL);

    // destroy default render pass
    pl_submit_render_pass_for_deletion(ptDevice, ptGraphics->uRenderPass);

    for(uint32_t i = 0; i < ptGraphics->uFramesInFlight; i++)
    {
        // destroy command buffers
        vkFreeCommandBuffers(tLogicalDevice, ptGraphics->sbFrames[i].tCmdPool, 1u, &ptGraphics->sbFrames[i].tCmdBuf);

        // destroy sync primitives
        vkDestroySemaphore(tLogicalDevice, ptGraphics->sbFrames[i].tImageAvailable, NULL);
        vkDestroySemaphore(tLogicalDevice, ptGraphics->sbFrames[i].tRenderFinish, NULL);
        vkDestroyFence(tLogicalDevice, ptGraphics->sbFrames[i].tInFlight, NULL);
        vkDestroyCommandPool(tLogicalDevice, ptGraphics->sbFrames[i].tCmdPool, NULL);
    }
    pl_sb_free(ptGraphics->sbFrames);
}


static plFrameContext*
pl_get_frame_resources(plGraphics* ptGraphics)
{
    return &ptGraphics->sbFrames[ptGraphics->szCurrentFrameIndex];
}

static void
pl_init_device(plApiRegistryApiI* ptApiRegistry, plDevice* ptDevice, uint32_t uFramesInFlight)
{
    plDeviceMemoryApiI* ptDeviceMemoryApi = ptApiRegistry->first(PL_API_DEVICE_MEMORY);
    pl_sb_resize(ptDevice->_sbtFrameGarbage, uFramesInFlight);
    ptDevice->uFramesInFlight = uFramesInFlight;
    
    ptDevice->tLocalDedicatedAllocator = ptDeviceMemoryApi->create_device_local_dedicated_allocator(ptDevice->tPhysicalDevice, ptDevice->tLogicalDevice);
    ptDevice->tLocalBuddyAllocator = ptDeviceMemoryApi->create_device_local_buddy_allocator(ptDevice->tPhysicalDevice, ptDevice->tLogicalDevice);
    ptDevice->tStagingUnCachedAllocator = ptDeviceMemoryApi->create_staging_uncached_allocator(ptDevice->tPhysicalDevice, ptDevice->tLogicalDevice);
    ptDevice->tStagingCachedAllocator = ptDeviceMemoryApi->create_staging_cached_allocator(ptDevice->tPhysicalDevice, ptDevice->tLogicalDevice);

    ptDevice->ptDeviceApi = ptApiRegistry->first(PL_API_DEVICE);

    const plDynamicBufferNode tDummyNode0 = {0, 0};
    const plDynamicBufferNode tDummyNode1 = {1, 1};
    pl_sb_push(ptDevice->_sbtDynamicBufferList, tDummyNode0);
    pl_sb_push(ptDevice->_sbtDynamicBufferList, tDummyNode1);

    // create dummy texture (texture slot 0 when not used)
    const plTextureDesc tTextureDesc2 = {
        .tDimensions = {.x = (float)1.0f, .y = (float)1.0f, .z = 1.0f},
        .tFormat     = PL_FORMAT_R8G8B8A8_UNORM,
        .tUsage      = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .uLayers     = 1,
        .uMips       = 1,
        .tType       = VK_IMAGE_TYPE_2D
    };
    static const float afSinglePixel[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    uint32_t uDummyTexture = pl_create_texture(ptDevice, tTextureDesc2, sizeof(unsigned char) * 4, afSinglePixel, "dummy texture");

    const plSampler tDummySampler = 
    {
        .fMinMip = 0.0f,
        .fMaxMip = 64.0f,
        .tFilter = PL_FILTER_LINEAR
    };

    const plTextureViewDesc tDummyView = {
        .tFormat     = tTextureDesc2.tFormat,
        .uLayerCount = tTextureDesc2.uLayers,
        .uMips       = tTextureDesc2.uMips
    };

    pl_create_texture_view(ptDevice, &tDummyView, &tDummySampler, uDummyTexture, "dummy texture view");
}

static plFormat
pl_find_supported_format(plDevice* ptDevice, VkFormatFeatureFlags tFlags, const plFormat* ptFormats, uint32_t uFormatCount)
{
    for(uint32_t i = 0u; i < uFormatCount; i++)
    {
        VkFormatProperties tProps = {0};
        vkGetPhysicalDeviceFormatProperties(ptDevice->tPhysicalDevice, pl__vulkan_format(ptFormats[i]), &tProps);
        if(tProps.optimalTilingFeatures & tFlags)
            return ptFormats[i];
    }

    PL_ASSERT(false && "no supported format found");
    return PL_FORMAT_UNKNOWN;
}

static plFormat
pl_find_depth_format(plDevice* ptDevice)
{
    const plFormat atFormats[] = {
        PL_FORMAT_D32_FLOAT,
        PL_FORMAT_D32_FLOAT_S8_UINT,
        PL_FORMAT_D24_UNORM_S8_UINT
    };
    return pl_find_supported_format(ptDevice, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, atFormats, 3);
}

static plFormat
pl_find_depth_stencil_format(plDevice* ptDevice)
{
     const plFormat atFormats[] = {
        PL_FORMAT_D32_FLOAT_S8_UINT,
        PL_FORMAT_D24_UNORM_S8_UINT
    };
    return pl_find_supported_format(ptDevice, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, atFormats, 2);   
}

static bool
pl_format_has_stencil(plFormat tFormat)
{
    switch(tFormat)
    {
        case PL_FORMAT_D16_UNORM_S8_UINT:
        case PL_FORMAT_D24_UNORM_S8_UINT:
        case PL_FORMAT_D32_FLOAT_S8_UINT: return true;
        case PL_FORMAT_D32_FLOAT:
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
pl_set_vulkan_object_name(plDevice* ptDevice, uint64_t uObjectHandle, VkDebugReportObjectTypeEXT tObjectType, const char* pcName)
{
    const VkDebugMarkerObjectNameInfoEXT tNameInfo = 
    {
        .sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT,
        .objectType = tObjectType,
        .object = uObjectHandle,
        .pObjectName = pcName
    };

    ptDevice->vkDebugMarkerSetObjectName(ptDevice->tLogicalDevice, &tNameInfo);
}

static VkCommandBuffer
pl_begin_command_buffer(plDevice* ptDevice, VkCommandPool tCmdPool)
{
    VkDevice tLogicalDevice = ptDevice->tLogicalDevice;
    VkCommandBuffer tCommandBuffer = {0};
    
    const VkCommandBufferAllocateInfo tAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool        = tCmdPool,
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
pl_submit_command_buffer(plDevice* ptDevice, VkCommandPool tCmdPool, VkCommandBuffer tCmdBuffer)
{
    VkDevice tLogicalDevice = ptDevice->tLogicalDevice;

    PL_VULKAN(vkEndCommandBuffer(tCmdBuffer));
    const VkSubmitInfo tSubmitInfo = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1u,
        .pCommandBuffers    = &tCmdBuffer,
    };

    PL_VULKAN(vkQueueSubmit(ptDevice->tGraphicsQueue, 1, &tSubmitInfo, VK_NULL_HANDLE));
    PL_VULKAN(vkDeviceWaitIdle(ptDevice->tLogicalDevice));
    vkFreeCommandBuffers(ptDevice->tLogicalDevice, tCmdPool, 1, &tCmdBuffer);
}

static void
pl_process_cleanup_queue(plDevice* ptDevice, uint32_t uCurrentFrame)
{
    ptDevice->uCurrentFrame = uCurrentFrame;
    plFrameGarbage* ptGarbage = &ptDevice->_sbtFrameGarbage[uCurrentFrame];
    const VkDevice tDevice = ptDevice->tLogicalDevice;

    for(uint32_t i = 2; i < pl_sb_size(ptDevice->_sbuDynamicBufferDeletionQueue); i++)
    {
        uint32_t uNodeToFree = ptDevice->_sbuDynamicBufferDeletionQueue[i];
        if(ptDevice->_sbtDynamicBufferList[uNodeToFree].uLastActiveFrame == ptDevice->szGeneration)
        {
            pl_return_dynamic_buffer(ptDevice, uNodeToFree);
            pl_sb_del_swap(ptDevice->_sbuDynamicBufferDeletionQueue, i);
        }
    }

    plDeviceMemoryAllocatorI* ptLocalAllocator = &ptDevice->tLocalDedicatedAllocator;
    plDeviceMemoryAllocatorI* ptLocalBufferAllocator = &ptDevice->tLocalBuddyAllocator;
    plDeviceMemoryAllocatorI* ptStagingAllocator = &ptDevice->tStagingUnCachedAllocator;
    plDeviceMemoryAllocatorI* ptStagingCachedAllocator = &ptDevice->tStagingCachedAllocator;
    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->_sbulBufferDeletionQueue); i++)
    {
        const uint32_t ulBufferIndex = ptGarbage->_sbulBufferDeletionQueue[i];

        plBuffer* ptBuffer = &ptDevice->sbtBuffers[ulBufferIndex];

        vkDestroyBuffer(tDevice, ptBuffer->tBuffer, NULL);

        if(ptBuffer->tAllocation.ptInst == ptLocalAllocator->ptInst)
            ptLocalAllocator->free(ptLocalAllocator->ptInst, &ptBuffer->tAllocation);
        else if(ptBuffer->tAllocation.ptInst == ptStagingAllocator->ptInst)
            ptStagingAllocator->free(ptStagingAllocator->ptInst, &ptBuffer->tAllocation);
        else if(ptBuffer->tAllocation.ptInst == ptStagingCachedAllocator->ptInst)
            ptStagingCachedAllocator->free(ptStagingCachedAllocator->ptInst, &ptBuffer->tAllocation);
        else if(ptBuffer->tAllocation.ptInst == ptLocalBufferAllocator->ptInst)
            ptLocalBufferAllocator->free(ptLocalBufferAllocator->ptInst, &ptBuffer->tAllocation);

        ptBuffer->tBuffer = VK_NULL_HANDLE;
        ptBuffer->szRequestedSize = 0;
        ptBuffer->tUsage = PL_BUFFER_USAGE_UNSPECIFIED;

        // add to free indices
        pl_sb_push(ptDevice->_sbulBufferFreeIndices, ulBufferIndex);
    }
    
    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->_sbulTextureDeletionQueue); i++)
    {
        const uint32_t ulTextureIndex = ptGarbage->_sbulTextureDeletionQueue[i];

        plTexture* ptTexture = &ptDevice->sbtTextures[ulTextureIndex];

        vkDestroyImage(tDevice, ptTexture->tImage, NULL);

        if(ptTexture->tAllocation.ptInst == ptLocalAllocator->ptInst)
            ptLocalAllocator->free(ptLocalAllocator->ptInst, &ptTexture->tAllocation);
        else if(ptTexture->tAllocation.ptInst == ptStagingAllocator->ptInst)
            ptStagingAllocator->free(ptStagingAllocator->ptInst, &ptTexture->tAllocation);
        else if(ptTexture->tAllocation.ptInst == ptStagingCachedAllocator->ptInst)
            ptStagingCachedAllocator->free(ptStagingCachedAllocator->ptInst, &ptTexture->tAllocation);
        else if(ptTexture->tAllocation.ptInst == ptLocalBufferAllocator->ptInst)
            ptLocalBufferAllocator->free(ptLocalBufferAllocator->ptInst, &ptTexture->tAllocation);

        ptTexture->tImage = VK_NULL_HANDLE;
        ptTexture->tDesc = (plTextureDesc){0};

        // add to free indices
        pl_sb_push(ptDevice->_sbulTextureFreeIndices, ulTextureIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->_sbulTextureViewDeletionQueue); i++)
    {
        const uint32_t ulTextureViewIndex = ptGarbage->_sbulTextureViewDeletionQueue[i];

        plTextureView* ptTextureView = &ptDevice->sbtTextureViews[ulTextureViewIndex];

        vkDestroyImageView(tDevice, ptTextureView->_tImageView, NULL);
        vkDestroySampler(tDevice, ptTextureView->_tSampler, NULL);

        ptTextureView->tTextureViewDesc = (plTextureViewDesc){0};
        ptTextureView->tSampler = (plSampler){0};
        ptTextureView->_tImageView = VK_NULL_HANDLE;
        ptTextureView->_tSampler = VK_NULL_HANDLE;

        // add to free indices
        pl_sb_push(ptDevice->_sbulTextureViewFreeIndices, ulTextureViewIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->_sbulFrameBufferDeletionQueue); i++)
    {
        const uint32_t uFrameBufferIndex = ptGarbage->_sbulFrameBufferDeletionQueue[i];

        plFrameBuffer* ptFrameBuffer = &ptDevice->sbtFrameBuffers[uFrameBufferIndex];

        vkDestroyFramebuffer(tDevice, ptFrameBuffer->_tFrameBuffer, NULL);
        ptFrameBuffer->_tFrameBuffer = VK_NULL_HANDLE;

        // add to free indices
        pl_sb_push(ptDevice->_sbulFrameBufferFreeIndices, uFrameBufferIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->_sbulRenderPassDeletionQueue); i++)
    {
        const uint32_t uRenderPassIndex = ptGarbage->_sbulRenderPassDeletionQueue[i];

        plRenderPass* ptRenderPass = &ptDevice->sbtRenderPasses[uRenderPassIndex];

        vkDestroyRenderPass(tDevice, ptRenderPass->_tRenderPass, NULL);
        ptRenderPass->_tRenderPass = VK_NULL_HANDLE;

        // add to free indices
        pl_sb_push(ptDevice->_sbulRenderPassFreeIndices, uRenderPassIndex);
    }

    pl_sb_reset(ptGarbage->_sbulBufferDeletionQueue);
    pl_sb_reset(ptGarbage->_sbulTextureDeletionQueue);
    pl_sb_reset(ptGarbage->_sbulTextureViewDeletionQueue);
    pl_sb_reset(ptGarbage->_sbulFrameBufferDeletionQueue);
    pl_sb_reset(ptGarbage->_sbulRenderPassDeletionQueue);

    ptDevice->szGeneration++;
}

static void
pl_transfer_data_to_buffer(plDevice* ptDevice, VkBuffer tDest, size_t szSize, const void* pData)
{

    uint32_t uStagingBuffer = pl_create_staging_buffer(ptDevice, szSize, "buffer staging");
    plBuffer* ptBuffer = &ptDevice->sbtBuffers[uStagingBuffer];

    // copy data
    memcpy(ptBuffer->tAllocation.pHostMapped, pData, szSize);

    // flush memory (incase we are using non-coherent memory)
    const VkMappedMemoryRange tMemoryRange = {
        .sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        .memory = (VkDeviceMemory)ptBuffer->tAllocation.tMemory,
        .size   = VK_WHOLE_SIZE
    };
    PL_VULKAN(vkFlushMappedMemoryRanges(ptDevice->tLogicalDevice, 1, &tMemoryRange));

    // perform copy from staging buffer to destination buffer
    VkCommandBuffer tCommandBuffer = ptDevice->ptDeviceApi->begin_command_buffer(ptDevice, ptDevice->tCmdPool);

    const VkBufferCopy tCopyRegion = {
        .size = szSize
    };
    vkCmdCopyBuffer(tCommandBuffer, ptBuffer->tBuffer, tDest, 1, &tCopyRegion);
    ptDevice->ptDeviceApi->submit_command_buffer(ptDevice, ptDevice->tCmdPool, tCommandBuffer);
    pl_delete_buffer(ptDevice, uStagingBuffer);
}

static void
pl_transfer_image_to_buffer(plDevice* ptDevice, VkBuffer tDest, size_t szSize, plTexture* ptDest)
{
    VkCommandBuffer tCommandBuffer = ptDevice->ptDeviceApi->begin_command_buffer(ptDevice, ptDevice->tCmdPool);

    const VkImageSubresourceLayers tSubResource = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1
    };

    const VkBufferImageCopy tCopyRegion = {
        .bufferImageHeight = (uint32_t)ptDest->tDesc.tDimensions.y,
        .bufferOffset = 0,
        .bufferRowLength = (uint32_t)ptDest->tDesc.tDimensions.x,
        .imageSubresource = tSubResource,
        .imageExtent = {
            .width = (uint32_t)ptDest->tDesc.tDimensions.x,
            .height = (uint32_t)ptDest->tDesc.tDimensions.y,
            .depth = (uint32_t)ptDest->tDesc.tDimensions.z
        }
    };
    vkCmdCopyImageToBuffer(tCommandBuffer, ptDest->tImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, tDest, 1, &tCopyRegion);
    ptDevice->ptDeviceApi->submit_command_buffer(ptDevice, ptDevice->tCmdPool, tCommandBuffer);    
}

static void
pl_transfer_data_to_image(plDevice* ptDevice, plTexture* ptDest, size_t szDataSize, const void* pData)
{
    uint32_t uStagingBuffer = pl_create_staging_buffer(ptDevice, szDataSize, "image staging");
    plBuffer* ptBuffer = &ptDevice->sbtBuffers[uStagingBuffer];

    // copy data
    memcpy(ptBuffer->tAllocation.pHostMapped, pData, szDataSize);

    // flush memory (incase we are using non-coherent memory)
    const VkMappedMemoryRange tMemoryRange = {
        .sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        .memory = (VkDeviceMemory)ptBuffer->tAllocation.tMemory,
        .size   = VK_WHOLE_SIZE
    };
    PL_VULKAN(vkFlushMappedMemoryRanges(ptDevice->tLogicalDevice, 1, &tMemoryRange));

    const VkImageSubresourceRange tSubResourceRange = {
        .aspectMask     = ptDest->tDesc.tUsage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT ? VK_IMAGE_ASPECT_DEPTH_BIT  : VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = ptDest->tDesc.uMips,
        .baseArrayLayer = 0,
        .layerCount     = ptDest->tDesc.uLayers
    };

    VkCommandBuffer tCommandBuffer = ptDevice->ptDeviceApi->begin_command_buffer(ptDevice, ptDevice->tCmdPool);

    // transition destination image layout to transfer destination
    ptDevice->ptDeviceApi->transition_image_layout(tCommandBuffer, ptDest->tImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, tSubResourceRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

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
    vkCmdCopyBufferToImage(tCommandBuffer, ptBuffer->tBuffer, ptDest->tImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &tCopyRegion);

    // generate mips
    if(ptDest->tDesc.uMips > 1)
    {
        // check if format supports linear blitting
        VkFormatProperties tFormatProperties = {0};
        vkGetPhysicalDeviceFormatProperties(ptDevice->tPhysicalDevice, pl__vulkan_format(ptDest->tDesc.tFormat), &tFormatProperties);

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

                ptDevice->ptDeviceApi->transition_image_layout(tCommandBuffer, ptDest->tImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, tMipSubResourceRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

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

                ptDevice->ptDeviceApi->transition_image_layout(tCommandBuffer, ptDest->tImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, tMipSubResourceRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);


                if(iMipWidth > 1)  iMipWidth /= 2;
                if(iMipHeight > 1) iMipHeight /= 2;
            }

            tMipSubResourceRange.baseMipLevel = ptDest->tDesc.uMips - 1;
            ptDevice->ptDeviceApi->transition_image_layout(tCommandBuffer, ptDest->tImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, tMipSubResourceRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        }
        else
        {
            PL_ASSERT(false && "format does not support linear blitting");
        }
    }
    else
    {
        // transition destination image layout to shader usage
        ptDevice->ptDeviceApi->transition_image_layout(tCommandBuffer, ptDest->tImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, tSubResourceRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    }

    ptDevice->ptDeviceApi->submit_command_buffer(ptDevice, ptDevice->tCmdPool, tCommandBuffer);
    pl_delete_buffer(ptDevice, uStagingBuffer);
}

static uint32_t
pl_create_index_buffer(plDevice* ptDevice, size_t szSize, const void* pData, const char* pcName)
{
    const VkDevice tDevice = ptDevice->tLogicalDevice;

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

    // allocate buffer
    plDeviceMemoryAllocatorI* ptAllocator = szSize > PL_DEVICE_ALLOCATION_BLOCK_SIZE ? &ptDevice->tLocalDedicatedAllocator : &ptDevice->tLocalBuddyAllocator;
    tBuffer.tAllocation = ptAllocator->allocate(ptAllocator->ptInst, tMemoryRequirements.memoryTypeBits, tMemoryRequirements.size, tMemoryRequirements.alignment, pcName);
    PL_VULKAN(vkBindBufferMemory(tDevice, tBuffer.tBuffer, (VkDeviceMemory)tBuffer.tAllocation.tMemory, tBuffer.tAllocation.ulOffset));

    // upload data if any is availble
    if(pData)
        pl_transfer_data_to_buffer(ptDevice, tBuffer.tBuffer, szSize, pData);

    // find free index
    uint32_t ulBufferIndex = 0u;
    if(!pl__get_free_resource_index(ptDevice->_sbulBufferFreeIndices, &ulBufferIndex))
        ulBufferIndex = pl_sb_add_n(ptDevice->sbtBuffers, 1);
    ptDevice->sbtBuffers[ulBufferIndex] = tBuffer;

    if(pcName)
        ptDevice->ptDeviceApi->set_vulkan_object_name(ptDevice, (uint64_t)tBuffer.tBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, pcName);

    return ulBufferIndex;
}

static uint32_t
pl_create_vertex_buffer(plDevice* ptDevice, size_t szSize, size_t szStride, const void* pData, const char* pcName)
{
    const VkDevice tDevice = ptDevice->tLogicalDevice;

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

    // allocate buffer
    plDeviceMemoryAllocatorI* ptAllocator = szSize > PL_DEVICE_ALLOCATION_BLOCK_SIZE ? &ptDevice->tLocalDedicatedAllocator : &ptDevice->tLocalBuddyAllocator;
    tBuffer.tAllocation = ptAllocator->allocate(ptAllocator->ptInst, tMemoryRequirements.memoryTypeBits, tMemoryRequirements.size, tMemoryRequirements.alignment, pcName);
    PL_VULKAN(vkBindBufferMemory(tDevice, tBuffer.tBuffer, (VkDeviceMemory)tBuffer.tAllocation.tMemory, tBuffer.tAllocation.ulOffset));

    // upload data if any is availble
    if(pData)
        pl_transfer_data_to_buffer(ptDevice, tBuffer.tBuffer, szSize, pData);

    // find free index
    uint32_t ulBufferIndex = 0u;
    if(!pl__get_free_resource_index(ptDevice->_sbulBufferFreeIndices, &ulBufferIndex))
        ulBufferIndex = pl_sb_add_n(ptDevice->sbtBuffers, 1);
    ptDevice->sbtBuffers[ulBufferIndex] = tBuffer;

    if(pcName)
        ptDevice->ptDeviceApi->set_vulkan_object_name(ptDevice, (uint64_t)tBuffer.tBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, pcName);

    return ulBufferIndex;  
}

static uint32_t
pl_create_constant_buffer(plDevice* ptDevice, size_t szSize, const char* pcName)
{
    const VkDevice tDevice = ptDevice->tLogicalDevice;

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
    tBuffer.szStride = tMemoryRequirements.size;

    // allocate buffer
    plDeviceMemoryAllocatorI* ptAllocator = &ptDevice->tStagingUnCachedAllocator;
    tBuffer.tAllocation = ptAllocator->allocate(ptAllocator->ptInst, tMemoryRequirements.memoryTypeBits, tMemoryRequirements.size, tMemoryRequirements.alignment, pcName);
    PL_VULKAN(vkBindBufferMemory(tDevice, tBuffer.tBuffer, (VkDeviceMemory)tBuffer.tAllocation.tMemory, tBuffer.tAllocation.ulOffset));

    // find free index
    uint32_t ulBufferIndex = 0u;
    if(!pl__get_free_resource_index(ptDevice->_sbulBufferFreeIndices, &ulBufferIndex))
        ulBufferIndex = pl_sb_add_n(ptDevice->sbtBuffers, 1);
    ptDevice->sbtBuffers[ulBufferIndex] = tBuffer;

    if(pcName)
        ptDevice->ptDeviceApi->set_vulkan_object_name(ptDevice, (uint64_t)tBuffer.tBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, pcName);

    return ulBufferIndex;      
}

static uint32_t
pl_create_read_back_buffer(plDevice* ptDevice, size_t szSize, const char* pcName)
{
    const VkDevice tDevice = ptDevice->tLogicalDevice;

    plBuffer tBuffer = {
        .tUsage          = PL_BUFFER_USAGE_CONSTANT,
        .szRequestedSize = szSize,
        .szItemCount     = 1
    };

    // create vulkan buffer
    const VkBufferCreateInfo tBufferCreateInfo = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = szSize,
        .usage       = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    PL_VULKAN(vkCreateBuffer(tDevice, &tBufferCreateInfo, NULL, &tBuffer.tBuffer));

    // get memory requirements
    VkMemoryRequirements tMemoryRequirements = {0};
    vkGetBufferMemoryRequirements(tDevice, tBuffer.tBuffer, &tMemoryRequirements);
    tBuffer.szStride = tMemoryRequirements.size;

    // allocate buffer
    plDeviceMemoryAllocatorI* ptAllocator = &ptDevice->tStagingCachedAllocator;
    tBuffer.tAllocation = ptAllocator->allocate(ptAllocator->ptInst, tMemoryRequirements.memoryTypeBits, tMemoryRequirements.size, tMemoryRequirements.alignment, pcName);
    PL_VULKAN(vkBindBufferMemory(tDevice, tBuffer.tBuffer, (VkDeviceMemory)tBuffer.tAllocation.tMemory, 0));

    // find free index
    uint32_t ulBufferIndex = 0u;
    if(!pl__get_free_resource_index(ptDevice->_sbulBufferFreeIndices, &ulBufferIndex))
        ulBufferIndex = pl_sb_add_n(ptDevice->sbtBuffers, 1);
    ptDevice->sbtBuffers[ulBufferIndex] = tBuffer;

    if(pcName)
        ptDevice->ptDeviceApi->set_vulkan_object_name(ptDevice, (uint64_t)tBuffer.tBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, pcName);

    return ulBufferIndex;      
}

static uint32_t
pl_create_staging_buffer(plDevice* ptDevice, size_t szSize, const char* pcName)
{
    const VkDevice tDevice = ptDevice->tLogicalDevice;

    plBuffer tBuffer = {
        .tUsage          = PL_BUFFER_USAGE_CONSTANT,
        .szRequestedSize = szSize,
        .szItemCount     = 1
    };

    // create vulkan buffer
    const VkBufferCreateInfo tBufferCreateInfo = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = szSize,
        .usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    PL_VULKAN(vkCreateBuffer(tDevice, &tBufferCreateInfo, NULL, &tBuffer.tBuffer));

    // get memory requirements
    VkMemoryRequirements tMemoryRequirements = {0};
    vkGetBufferMemoryRequirements(tDevice, tBuffer.tBuffer, &tMemoryRequirements);
    tBuffer.szStride = tMemoryRequirements.size;

    // allocate buffer
    plDeviceMemoryAllocatorI* ptAllocator = &ptDevice->tStagingUnCachedAllocator;
    tBuffer.tAllocation = ptAllocator->allocate(ptAllocator->ptInst, tMemoryRequirements.memoryTypeBits, tMemoryRequirements.size, tMemoryRequirements.alignment, pcName);
    PL_VULKAN(vkBindBufferMemory(tDevice, tBuffer.tBuffer, (VkDeviceMemory)tBuffer.tAllocation.tMemory, 0));

    // find free index
    uint32_t ulBufferIndex = 0u;
    if(!pl__get_free_resource_index(ptDevice->_sbulBufferFreeIndices, &ulBufferIndex))
        ulBufferIndex = pl_sb_add_n(ptDevice->sbtBuffers, 1);
    ptDevice->sbtBuffers[ulBufferIndex] = tBuffer;

    if(pcName)
        ptDevice->ptDeviceApi->set_vulkan_object_name(ptDevice, (uint64_t)tBuffer.tBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, pcName);

    return ulBufferIndex;   
}

static uint32_t
pl_create_texture_view(plDevice* ptDevice, const plTextureViewDesc* ptViewDesc, const plSampler* ptSampler, uint32_t uTextureHandle, const char* pcName)
{
    plTextureView tTextureView = {
        .tSampler         = *ptSampler,
        .tTextureViewDesc = *ptViewDesc,
        .uTextureHandle   = uTextureHandle
    };

    VkDevice tDevice = ptDevice->tLogicalDevice;
    plTexture* ptTexture = &ptDevice->sbtTextures[uTextureHandle];

    if(ptViewDesc->uMips == 0)
        tTextureView.tTextureViewDesc.uMips = ptTexture->tDesc.uMips;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~create view~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const VkImageViewType tImageViewType = ptViewDesc->uLayerCount == 6 ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;    
    PL_ASSERT((ptViewDesc->uLayerCount == 1 || ptViewDesc->uLayerCount == 6) && "unsupported layer count");

    VkImageAspectFlags tImageAspectFlags = ptTexture->tDesc.tUsage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    if(ptDevice->ptDeviceApi->format_has_stencil(ptViewDesc->tFormat))
        tImageAspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;

    VkImageViewCreateInfo tViewInfo = {
        .sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image                           = ptTexture->tImage,
        .viewType                        = tImageViewType,
        .format                          = pl__vulkan_format(ptViewDesc->tFormat),
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
        .anisotropyEnable        = (bool)ptDevice->tDeviceFeatures.samplerAnisotropy,
        .maxAnisotropy           = ptDevice->tDeviceProps.limits.maxSamplerAnisotropy,
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
    if(!pl__get_free_resource_index(ptDevice->_sbulTextureViewFreeIndices, &ulTextureViewIndex))
        ulTextureViewIndex = pl_sb_add_n(ptDevice->sbtTextureViews, 1);
    ptDevice->sbtTextureViews[ulTextureViewIndex] = tTextureView;

    if(pcName)
    {
        ptDevice->ptDeviceApi->set_vulkan_object_name(ptDevice, (uint64_t)tTextureView._tSampler, VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT, pcName);
        ptDevice->ptDeviceApi->set_vulkan_object_name(ptDevice, (uint64_t)tTextureView._tImageView, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, pcName);
    }

    return ulTextureViewIndex;
}

static uint32_t
pl_create_frame_buffer(plDevice* ptDevice, plFrameBufferDesc* ptDesc, const char* pcName)
{
    plFrameBuffer tFrameBuffer = {
        ._tFrameBuffer = VK_NULL_HANDLE
    };

    VkImageView atAttachments[16] = {0};

    for(uint32_t i = 0; i < ptDesc->uAttachmentCount; i++)
    {
        atAttachments[i] = ptDevice->sbtTextureViews[ptDesc->puAttachments[i]]._tImageView;
    }
    VkFramebufferCreateInfo tFrameBufferInfo = {
        .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass      = ptDevice->sbtRenderPasses[ptDesc->uRenderPass]._tRenderPass,
        .attachmentCount = ptDesc->uAttachmentCount,
        .pAttachments    = atAttachments,
        .width           = ptDesc->uWidth,
        .height          = ptDesc->uHeight,
        .layers          = 1u,
    };
    PL_VULKAN(vkCreateFramebuffer(ptDevice->tLogicalDevice, &tFrameBufferInfo, NULL, &tFrameBuffer._tFrameBuffer));

    // find free index
    uint32_t ulFrameBufferIndex = 0u;
    if(!pl__get_free_resource_index(ptDevice->_sbulFrameBufferFreeIndices, &ulFrameBufferIndex))
        ulFrameBufferIndex = pl_sb_add_n(ptDevice->sbtFrameBuffers, 1);
    ptDevice->sbtFrameBuffers[ulFrameBufferIndex] = tFrameBuffer;

    if(pcName)
        ptDevice->ptDeviceApi->set_vulkan_object_name(ptDevice, (uint64_t)tFrameBuffer._tFrameBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT, pcName);

    return ulFrameBufferIndex;
}

static uint32_t
pl_create_render_pass(plDevice* ptDevice, plRenderPassDesc* ptDesc, const char* pcName)
{

    plRenderPass tRenderPass = {
        .tDesc = *ptDesc
    };

    // create render pass
    VkAttachmentDescription atAttachments[] = {

        // color attachment
        {
            .flags          = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT,
            .format         = pl__vulkan_format(ptDesc->tColorFormat),
            .samples        = VK_SAMPLE_COUNT_1_BIT,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        },

        // depth attachment
        {
            .format         = pl__vulkan_format(ptDesc->tDepthFormat),
            .samples        = VK_SAMPLE_COUNT_1_BIT,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        }
    };

    VkSubpassDependency tSubpassDependencies[] = {

        // color attachment
        {
            .srcSubpass      = VK_SUBPASS_EXTERNAL,
            .dstSubpass      = 0,
            .srcStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask   = VK_ACCESS_SHADER_READ_BIT,
            .dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = 0
        },

        // color attachment out
        {
            .srcSubpass      = 0,
            .dstSubpass      = VK_SUBPASS_EXTERNAL,
            .srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask   = VK_ACCESS_SHADER_READ_BIT,
            .dependencyFlags = 0
        },
    };

    VkAttachmentReference atAttachmentReferences[] = {
        {
            .attachment = 0,
            .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        },
        {
            .attachment = 1,
            .layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL     
        }
    };

    VkSubpassDescription tSubpass = {
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount    = 1,
        .pColorAttachments       = &atAttachmentReferences[0],
        .pDepthStencilAttachment = &atAttachmentReferences[1]
    };

    VkRenderPassCreateInfo tRenderPassInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 2,
        .pAttachments    = atAttachments,
        .subpassCount    = 1,
        .pSubpasses      = &tSubpass,
        .dependencyCount = 2,
        .pDependencies   = tSubpassDependencies
    };
    PL_VULKAN(vkCreateRenderPass(ptDevice->tLogicalDevice, &tRenderPassInfo, NULL, &tRenderPass._tRenderPass));
    
    // find free index
    uint32_t ulRenderPassIndex = 0u;
    if(!pl__get_free_resource_index(ptDevice->_sbulFrameBufferFreeIndices, &ulRenderPassIndex))
        ulRenderPassIndex = pl_sb_add_n(ptDevice->sbtRenderPasses, 1);
    ptDevice->sbtRenderPasses[ulRenderPassIndex] = tRenderPass;

    if(pcName)
        ptDevice->ptDeviceApi->set_vulkan_object_name(ptDevice, (uint64_t)tRenderPass._tRenderPass, VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT, pcName);

    return ulRenderPassIndex;  
}


static uint32_t
pl_request_dynamic_buffer(plDevice* ptDevice)
{

    plDynamicBufferNode* sbtNodes = ptDevice->_sbtDynamicBufferList;
    uint32_t n = 0;

    if (sbtNodes[1].uNext != 1) // check free list
    {
        n = sbtNodes[1].uNext;
        sbtNodes[n].uLastActiveFrame = (uint32_t)ptDevice->szGeneration;

        // remove from free list
        sbtNodes[sbtNodes[n].uNext].uPrev = sbtNodes[n].uPrev;
        sbtNodes[sbtNodes[n].uPrev].uNext = sbtNodes[n].uNext;
    }
    else  // add new node
    {
        pl_sb_resize(sbtNodes, pl_sb_size(sbtNodes) + 1);
        n = pl_sb_size(sbtNodes) - 1;
        ptDevice->_sbtDynamicBufferList = sbtNodes; // just incase realloc occurs
        sbtNodes[n].uDynamicBufferOffset = 0;
        sbtNodes[n].uLastActiveFrame = (uint32_t)ptDevice->szGeneration;

        // add node to list
        sbtNodes[n].uNext = sbtNodes[0].uNext;
        sbtNodes[n].uPrev = 0;
        sbtNodes[sbtNodes[n].uNext].uPrev = n;
        sbtNodes[sbtNodes[n].uPrev].uNext = n;

        // pl_log_info_to(ptDevice->_ptGraphics->uLogChannel, "creating new dynamic buffer");
        sbtNodes[n].uDynamicBuffer = pl_create_constant_buffer(ptDevice, ptDevice->uUniformBufferBlockSize, "temp dynamic buffer");
    }
    return n;
}

static void
pl_return_dynamic_buffer(plDevice* ptDevice, uint32_t uNodeIndex)
{
    // add node to free list
    plDynamicBufferNode* sbtNodes = ptDevice->_sbtDynamicBufferList;
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
pl_create_texture(plDevice* ptDevice, plTextureDesc tDesc, size_t szSize, const void* pData, const char* pcName)
{
    VkDevice tDevice = ptDevice->tLogicalDevice;
    plDeviceApiI* ptDeviceApi = ptDevice->ptDeviceApi;

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
        .format        = pl__vulkan_format(tDesc.tFormat),
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
    plDeviceMemoryAllocatorI* ptAllocator = szSize > PL_DEVICE_ALLOCATION_BLOCK_SIZE ? &ptDevice->tLocalDedicatedAllocator : &ptDevice->tLocalBuddyAllocator;
    tTexture.tAllocation = ptAllocator->allocate(ptAllocator->ptInst, tMemoryRequirements.memoryTypeBits, tMemoryRequirements.size, tMemoryRequirements.alignment, pcName);

    PL_VULKAN(vkBindImageMemory(tDevice, tTexture.tImage, (VkDeviceMemory)tTexture.tAllocation.tMemory, tTexture.tAllocation.ulOffset));

    // upload data
    if(pData)
        pl_transfer_data_to_image(ptDevice, &tTexture, szSize, pData);

    VkImageAspectFlags tImageAspectFlags = tDesc.tUsage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    if(ptDevice->ptDeviceApi->format_has_stencil(tDesc.tFormat))
        tImageAspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;

    VkCommandBuffer tCommandBuffer = ptDeviceApi->begin_command_buffer(ptDevice, ptDevice->tCmdPool);
    VkImageSubresourceRange tRange = {
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = tDesc.uLayers,
        .aspectMask     = tImageAspectFlags
    };

    if(pData)
        ptDeviceApi->transition_image_layout(tCommandBuffer, tTexture.tImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, tRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    else if (tDesc.tUsage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
        ptDeviceApi->transition_image_layout(tCommandBuffer, tTexture.tImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, tRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    else if(tDesc.tUsage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        ptDeviceApi->transition_image_layout(tCommandBuffer, tTexture.tImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, tRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    else if(tDesc.tUsage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
        ptDeviceApi->transition_image_layout(tCommandBuffer, tTexture.tImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, tRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    ptDeviceApi->submit_command_buffer(ptDevice, ptDevice->tCmdPool, tCommandBuffer);

    // find free index
    uint32_t ulTextureIndex = 0u;
    if(!pl__get_free_resource_index(ptDevice->_sbulTextureFreeIndices, &ulTextureIndex))
        ulTextureIndex = pl_sb_add_n(ptDevice->sbtTextures, 1);
    ptDevice->sbtTextures[ulTextureIndex] = tTexture;

    if(pcName)
        ptDeviceApi->set_vulkan_object_name(ptDevice, (uint64_t)tTexture.tImage, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, pcName);

    return ulTextureIndex;      
}

static uint32_t
pl_create_storage_buffer(plDevice* ptDevice, size_t szSize, const void* pData, const char* pcName)
{
    const VkDevice tDevice = ptDevice->tLogicalDevice;

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

    // allocate buffer
    plDeviceMemoryAllocatorI* ptAllocator = szSize > PL_DEVICE_ALLOCATION_BLOCK_SIZE ? &ptDevice->tLocalDedicatedAllocator : &ptDevice->tLocalBuddyAllocator;
    tBuffer.tAllocation = ptAllocator->allocate(ptAllocator->ptInst, tMemoryRequirements.memoryTypeBits, tMemoryRequirements.size, tMemoryRequirements.alignment, pcName);
    PL_VULKAN(vkBindBufferMemory(tDevice, tBuffer.tBuffer, (VkDeviceMemory)tBuffer.tAllocation.tMemory, tBuffer.tAllocation.ulOffset));

    // upload data if any is availble
    if(pData)
        pl_transfer_data_to_buffer(ptDevice, tBuffer.tBuffer, szSize, pData);

    // find free index
    uint32_t ulBufferIndex = 0u;
    if(!pl__get_free_resource_index(ptDevice->_sbulBufferFreeIndices, &ulBufferIndex))
        ulBufferIndex = pl_sb_add_n(ptDevice->sbtBuffers, 1);
    ptDevice->sbtBuffers[ulBufferIndex] = tBuffer;

    if(pcName)
        ptDevice->ptDeviceApi->set_vulkan_object_name(ptDevice, (uint64_t)tBuffer.tBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, pcName);

    return ulBufferIndex;
}

static void
pl_delete_buffer(plDevice* ptDevice, uint32_t uBufferIndex)
{    
    plBuffer* ptBuffer = &ptDevice->sbtBuffers[uBufferIndex];

    vkDestroyBuffer(ptDevice->tLogicalDevice, ptBuffer->tBuffer, NULL);

    if(ptBuffer->tAllocation.ptInst == ptDevice->tLocalDedicatedAllocator.ptInst)
        ptDevice->tLocalDedicatedAllocator.free(ptDevice->tLocalDedicatedAllocator.ptInst, &ptBuffer->tAllocation);
    else if(ptBuffer->tAllocation.ptInst == ptDevice->tStagingUnCachedAllocator.ptInst)
        ptDevice->tStagingUnCachedAllocator.free(ptDevice->tStagingUnCachedAllocator.ptInst, &ptBuffer->tAllocation);
    else if(ptBuffer->tAllocation.ptInst == ptDevice->tStagingCachedAllocator.ptInst)
        ptDevice->tStagingCachedAllocator.free(ptDevice->tStagingCachedAllocator.ptInst, &ptBuffer->tAllocation);
    else
    {
        PL_ASSERT(false);
    }

    pl_sb_push(ptDevice->_sbulBufferFreeIndices, uBufferIndex);
}

static void
pl_submit_buffer_for_deletion(plDevice* ptDevice, uint32_t ulBufferIndex)
{
    PL_ASSERT(ulBufferIndex < pl_sb_size(ptDevice->sbtBuffers));
    pl_sb_push(ptDevice->_sbtFrameGarbage[ptDevice->uCurrentFrame]._sbulBufferDeletionQueue, ulBufferIndex);
}

static void
pl_submit_texture_for_deletion(plDevice* ptDevice, uint32_t ulTextureIndex)
{
    PL_ASSERT(ulTextureIndex < pl_sb_size(ptDevice->sbtTextures)); 
    pl_sb_push(ptDevice->_sbtFrameGarbage[ptDevice->uCurrentFrame]._sbulTextureDeletionQueue, ulTextureIndex);  
}

static void
pl_submit_texture_view_for_deletion(plDevice* ptDevice, uint32_t uTextureViewIndex)
{
    PL_ASSERT(uTextureViewIndex < pl_sb_size(ptDevice->sbtTextureViews)); 
    pl_sb_push(ptDevice->_sbtFrameGarbage[ptDevice->uCurrentFrame]._sbulTextureViewDeletionQueue, uTextureViewIndex);
}

static void
pl_submit_frame_buffer_for_deletion(plDevice* ptDevice, uint32_t uFrameBufferIndex)
{
    PL_ASSERT(uFrameBufferIndex < pl_sb_size(ptDevice->sbtFrameBuffers)); 
    pl_sb_push(ptDevice->_sbtFrameGarbage[ptDevice->uCurrentFrame]._sbulFrameBufferDeletionQueue, uFrameBufferIndex);  
}

static void
pl_submit_render_pass_for_deletion(plDevice* ptDevice, uint32_t uRenderPassIndex)
{
    PL_ASSERT(uRenderPassIndex < pl_sb_size(ptDevice->sbtRenderPasses)); 
    pl_sb_push(ptDevice->_sbtFrameGarbage[ptDevice->uCurrentFrame]._sbulRenderPassDeletionQueue, uRenderPassIndex);   
}

static void
pl_cleanup_device(plDevice* ptDevice)
{
    
    for(uint32_t i = 0; i < pl_sb_size(ptDevice->sbtBuffers); i++)
    {
        if(ptDevice->sbtBuffers[i].tBuffer)
            pl_submit_buffer_for_deletion(ptDevice, i);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptDevice->sbtTextures); i++)
    {
        if(ptDevice->sbtTextures[i].tImage)
            pl_submit_texture_for_deletion(ptDevice, i);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptDevice->sbtTextureViews); i++)
    {
        if(ptDevice->sbtTextureViews[i]._tImageView)
            pl_submit_texture_view_for_deletion(ptDevice, i);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptDevice->sbtFrameBuffers); i++)
    {
        if(ptDevice->sbtFrameBuffers[i]._tFrameBuffer)
            pl_submit_frame_buffer_for_deletion(ptDevice, i);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptDevice->sbtRenderPasses); i++)
    {
        if(ptDevice->sbtRenderPasses[i]._tRenderPass)
            pl_submit_render_pass_for_deletion(ptDevice, i);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptDevice->_sbtFrameGarbage); i++)
        pl_process_cleanup_queue(ptDevice, i);

    pl_cleanup_device_local_dedicated_allocator(&ptDevice->tLocalDedicatedAllocator);
    pl_cleanup_device_local_buddy_allocator(&ptDevice->tLocalBuddyAllocator);
    pl_cleanup_staging_cached_allocator(&ptDevice->tStagingCachedAllocator);
    pl_cleanup_staging_uncached_allocator(&ptDevice->tStagingUnCachedAllocator);

    pl_sb_free(ptDevice->sbtBuffers);
    pl_sb_free(ptDevice->sbtTextures);
    pl_sb_free(ptDevice->sbtTextureViews);
    pl_sb_free(ptDevice->sbtRenderPasses);
    pl_sb_free(ptDevice->_sbulTextureFreeIndices);
    pl_sb_free(ptDevice->_sbulBufferFreeIndices);
    pl_sb_free(ptDevice->_sbulTextureViewFreeIndices);
    pl_sb_free(ptDevice->_sbulRenderPassFreeIndices);
    pl_sb_free(ptDevice->_sbuDynamicBufferDeletionQueue);
    pl_sb_free(ptDevice->_sbtDynamicBufferList);
    pl_sb_free(ptDevice->_sbulTempQueue);
    pl_sb_free(ptDevice->_sbulFrameBufferFreeIndices);

    for(uint32_t i = 0; i < pl_sb_size(ptDevice->_sbtFrameGarbage); i++)
    {
        pl_sb_free(ptDevice->_sbtFrameGarbage[i]._sbulBufferDeletionQueue);
        pl_sb_free(ptDevice->_sbtFrameGarbage[i]._sbulTextureDeletionQueue);
        pl_sb_free(ptDevice->_sbtFrameGarbage[i]._sbulTextureViewDeletionQueue);
        pl_sb_free(ptDevice->_sbtFrameGarbage[i]._sbulRenderPassDeletionQueue);
        pl_sb_free(ptDevice->_sbtFrameGarbage[i]._sbulFrameBufferDeletionQueue);
    }

    pl_sb_free(ptDevice->_sbtFrameGarbage);
    pl_sb_free(ptDevice->sbtFrameBuffers);

    // destroy command pool
    vkDestroyCommandPool(ptDevice->tLogicalDevice, ptDevice->tCmdPool, NULL);

    // destroy device
    vkDestroyDevice(ptDevice->tLogicalDevice, NULL);
}

static plDeviceMemoryAllocation
pl_allocate_dedicated(struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;

    plDeviceMemoryAllocation tAllocation = {
        .pHostMapped = NULL,
        .tMemory     = 0,
        .ulOffset    = 0,
        .ulSize      = ulSize,
        .ptInst      = ptInst
    };

    uint32_t uMemoryType = 0u;
    bool bFound = false;
    for (uint32_t i = 0; i < ptData->tMemProps.memoryTypeCount; i++) 
    {
        if ((uTypeFilter & (1 << i)) && (ptData->tMemProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) 
        {
            uMemoryType = i;
            bFound = true;
            break;
        }
    }
    PL_ASSERT(bFound);

    const VkMemoryAllocateInfo tAllocInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = pl_maxu((uint32_t)ulSize, PL_DEVICE_ALLOCATION_BLOCK_SIZE),
        .memoryTypeIndex = uMemoryType
    };

    VkResult tResult = vkAllocateMemory(ptData->tDevice, &tAllocInfo, NULL, (VkDeviceMemory*)&tAllocation.tMemory);
    PL_VULKAN(tResult);

    plDeviceAllocationBlock tBlock = {
        .ulAddress = tAllocation.tMemory,
        .ulSize    = tAllocInfo.allocationSize
    };

    const plDeviceAllocationRange tRange = {
        .tAllocation  = tAllocation,
        .tStatus      = PL_DEVICE_ALLOCATION_STATUS_USED,
        .pcName       = pcName
    };

    pl_sb_push(tBlock.sbtRanges, tRange);
    pl_sb_push(ptData->sbtBlocks, tBlock);
    return tAllocation;
}

static void
pl_free_dedicated(struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;

    uint32_t uIndex = 0;
    for(uint32_t i = 0; i < pl_sb_size(ptData->sbtBlocks); i++)
    {
        if(ptData->sbtBlocks[i].ulAddress == ptAllocation->tMemory)
        {
            pl_sb_reset(ptData->sbtBlocks[i].sbtRanges);
            uIndex = i;
            break;
        }
    }
    pl_sb_del_swap(ptData->sbtBlocks, uIndex);

    vkFreeMemory(ptData->tDevice, (VkDeviceMemory)ptAllocation->tMemory, NULL);

    ptAllocation->pHostMapped  = NULL;
    ptAllocation->tMemory      = 0;
    ptAllocation->ulOffset     = 0;
    ptAllocation->ulSize       = 0;
}

static plDeviceAllocationBlock*
pl_blocks_dedicated(struct plDeviceMemoryAllocatorO* ptInst, uint32_t* puSizeOut)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;
    if(puSizeOut)
        *puSizeOut = pl_sb_size(ptData->sbtBlocks);
    return ptData->sbtBlocks;
}

static plDeviceAllocationNode*
pl_nodes_dedicated(struct plDeviceMemoryAllocatorO* ptInst, uint32_t* puSizeOut)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;
    if(puSizeOut)
        *puSizeOut = pl_sb_size(ptData->sbtNodes);
    return ptData->sbtNodes;    
}

static char**
pl_names_dedicated(struct plDeviceMemoryAllocatorO* ptInst, uint32_t* puSizeOut)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;
    if(puSizeOut)
        *puSizeOut = pl_sb_size(ptData->sbtDebugNamesInDir);
    return ptData->sbtDebugNamesInDir;  
}

static plDeviceMemoryAllocation
pl_allocate_staging_uncached(struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;

    for(uint32_t i = 0; i < pl_sb_size(ptData->sbtBlocks); i++)
    {
        plDeviceAllocationBlock* ptBlock = &ptData->sbtBlocks[i];
        if(ptBlock->sbtRanges[0].tStatus == PL_DEVICE_ALLOCATION_STATUS_FREE && ptBlock->ulSize >= ulSize)
        {
            ptBlock->sbtRanges[0].tStatus = PL_DEVICE_ALLOCATION_STATUS_USED;
            ptBlock->sbtRanges[0].tAllocation.ulSize = ulSize;
            ptBlock->sbtRanges[0].pcName = pcName;
            return ptBlock->sbtRanges[0].tAllocation;
        }
    }

    plDeviceAllocationBlock tBlock = {
        .ulAddress = 0,
        .ulSize    = pl_maxu((uint32_t)ulSize, PL_DEVICE_ALLOCATION_BLOCK_SIZE)
    };

    plDeviceAllocationRange tRange = {
        .tAllocation  = {
            .ulSize      = ulSize,
            .pHostMapped = NULL,
            .tMemory     = 0,
            .ulOffset    = 0,
            .ptInst      = ptInst
        },
        .tStatus      = PL_DEVICE_ALLOCATION_STATUS_USED,
        .pcName       = pcName
    };

    uint32_t uMemoryType = 0u;
    bool bFound = false;
    const VkMemoryPropertyFlags tProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (uint32_t i = 0; i < ptData->tMemProps.memoryTypeCount; i++) 
    {
        if ((uTypeFilter & (1 << i)) && (ptData->tMemProps.memoryTypes[i].propertyFlags & tProperties) == tProperties) 
        {
            uMemoryType = i;
            bFound = true;
            break;
        }
    }
    PL_ASSERT(bFound);

    const VkMemoryAllocateInfo tAllocInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = tBlock.ulSize,
        .memoryTypeIndex = uMemoryType
    };

    PL_VULKAN(vkAllocateMemory(ptData->tDevice, &tAllocInfo, NULL, (VkDeviceMemory*)&tRange.tAllocation.tMemory));

    PL_VULKAN(vkMapMemory(ptData->tDevice, (VkDeviceMemory)tRange.tAllocation.tMemory, 0, tBlock.ulSize, 0, (void**)&tRange.tAllocation.pHostMapped));

    tBlock.ulAddress = tRange.tAllocation.tMemory;

    pl_sb_push(tBlock.sbtRanges, tRange);
    pl_sb_push(ptData->sbtBlocks, tBlock);

    return tRange.tAllocation;
}


static void
pl_free_staging_uncached(struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;

    for(uint32_t i = 0; i < pl_sb_size(ptData->sbtBlocks); i++)
    {
        plDeviceAllocationBlock* ptBlock = &ptData->sbtBlocks[i];
        if(ptBlock->ulAddress == ptAllocation->tMemory)
        {
            ptBlock->sbtRanges[0].tStatus = PL_DEVICE_ALLOCATION_STATUS_FREE;
            ptBlock->sbtRanges[0].tAllocation.ulSize = 0;
            ptBlock->sbtRanges[0].tAllocation.ulOffset = 0;
            ptBlock->sbtRanges[0].pcName = "";
        }
    }
}

static plDeviceMemoryAllocation
pl_allocate_staging_cached(struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;

    for(uint32_t i = 0; i < pl_sb_size(ptData->sbtBlocks); i++)
    {
        plDeviceAllocationBlock* ptBlock = &ptData->sbtBlocks[i];
        if(ptBlock->sbtRanges[0].tStatus == PL_DEVICE_ALLOCATION_STATUS_FREE && ptBlock->ulSize >= ulSize)
        {
            ptBlock->sbtRanges[0].tStatus = PL_DEVICE_ALLOCATION_STATUS_USED;
            ptBlock->sbtRanges[0].tAllocation.ulSize = ulSize;
            ptBlock->sbtRanges[0].pcName = pcName;
            return ptBlock->sbtRanges[0].tAllocation;
        }
    }

    plDeviceAllocationBlock tBlock = {
        .ulAddress = 0,
        .ulSize    = pl_maxu((uint32_t)ulSize, PL_DEVICE_ALLOCATION_BLOCK_SIZE)
    };

    plDeviceAllocationRange tRange = {
        .tAllocation  = {
            .ulSize      = ulSize,
            .pHostMapped = NULL,
            .tMemory     = 0,
            .ulOffset    = 0,
            .ptInst      = ptInst
        },
        .tStatus      = PL_DEVICE_ALLOCATION_STATUS_USED,
        .pcName       = pcName
    };

    uint32_t uMemoryType = 0u;
    bool bFound = false;
    for (uint32_t i = 0; i < ptData->tMemProps.memoryTypeCount; i++) 
    {
        if ((uTypeFilter & (1 << i)) && (ptData->tMemProps.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT)) == (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT)) 
        {
            uMemoryType = i;
            bFound = true;
            break;
        }
    }
    PL_ASSERT(bFound);
 
    const VkMemoryAllocateInfo tAllocInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = tBlock.ulSize,
        .memoryTypeIndex = uMemoryType
    };

    PL_VULKAN(vkAllocateMemory(ptData->tDevice, &tAllocInfo, NULL, (VkDeviceMemory*)&tRange.tAllocation.tMemory));

    PL_VULKAN(vkMapMemory(ptData->tDevice, (VkDeviceMemory)tRange.tAllocation.tMemory, 0, tBlock.ulSize, 0, (void**)&tRange.tAllocation.pHostMapped));

    tBlock.ulAddress = tRange.tAllocation.tMemory;

    pl_sb_push(tBlock.sbtRanges, tRange);
    pl_sb_push(ptData->sbtBlocks, tBlock);

    return tRange.tAllocation;
}

static void
pl_free_staging_cached(struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;

    for(uint32_t i = 0; i < pl_sb_size(ptData->sbtBlocks); i++)
    {
        plDeviceAllocationBlock* ptBlock = &ptData->sbtBlocks[i];
        if(ptBlock->ulAddress == ptAllocation->tMemory)
        {
            ptBlock->sbtRanges[0].tStatus = PL_DEVICE_ALLOCATION_STATUS_FREE;
            ptBlock->sbtRanges[0].tAllocation.ulSize = 0;
            ptBlock->sbtRanges[0].tAllocation.ulOffset = 0;
            ptBlock->sbtRanges[0].pcName = "";
        }
    }

}

static plDeviceMemoryAllocatorI
pl_create_device_local_dedicated_allocator(VkPhysicalDevice tPhysicalDevice, VkDevice tDevice)
{
    plDeviceAllocatorData* ptData = PL_ALLOC(sizeof(plDeviceAllocatorData));
    memset(ptData, 0, sizeof(plDeviceAllocatorData));

    // create allocator interface
    plDeviceMemoryAllocatorI tAllocatorInterface = 
    {
        .allocate = pl_allocate_dedicated,
        .free     = pl_free_dedicated,
        .blocks   = pl_blocks_dedicated,
        .nodes    = pl_nodes_dedicated,
        .names    = pl_names_dedicated,
        .ptInst   = (struct plDeviceMemoryAllocatorO*)ptData
    };

    vkGetPhysicalDeviceMemoryProperties(tPhysicalDevice, &ptData->tMemProps);
    ptData->tDevice = tDevice;
    return tAllocatorInterface;
}

static plDeviceMemoryAllocatorI
pl_create_device_local_buddy_allocator(VkPhysicalDevice tPhysicalDevice, VkDevice tDevice)
{
    plDeviceAllocatorData* ptData = PL_ALLOC(sizeof(plDeviceAllocatorData));
    memset(ptData, 0, sizeof(plDeviceAllocatorData));

    for(uint32_t i = 0; i < PL_DEVICE_LOCAL_LEVELS; i++)
        ptData->auFreeList[i] = UINT32_MAX;
    
    // create allocator interface
    plDeviceMemoryAllocatorI tAllocatorInterface = 
    {
        .allocate = pl_allocate_local_buffer,
        .free     = pl_free_local_buffer,
        .blocks   = pl_blocks_dedicated,
        .nodes    = pl_nodes_dedicated,
        .names    = pl_names_dedicated,
        .ptInst   = (struct plDeviceMemoryAllocatorO*)ptData
    };

    vkGetPhysicalDeviceMemoryProperties(tPhysicalDevice, &ptData->tMemProps);
    ptData->tDevice = tDevice;
    return tAllocatorInterface;
}

static void
pl_cleanup_device_local_buddy_allocator(plDeviceMemoryAllocatorI* ptAllocator)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptAllocator->ptInst;

    for(uint32_t i = 0; i < pl_sb_size(ptData->sbtBlocks); i++)
    {
        plDeviceAllocationBlock* ptBlock = &ptData->sbtBlocks[i];
        vkFreeMemory(ptData->tDevice, (VkDeviceMemory)ptBlock->ulAddress, NULL);
    }
    pl_sb_free(ptData->sbtBlocks);
    pl_sb_free(ptData->sbtDebugNamesInDir);
    pl_sb_free(ptData->sbcDebugNames);
    pl_sb_free(ptData->sbtNodes);
    PL_FREE(ptData);
}

static plDeviceMemoryAllocation
pl_allocate_local_buffer(struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;

    if(ulAlignment > 0)
        ulSize = ulSize + (ulAlignment - 1);

    // find what level we need
    uint32_t uLevel = 0;
    for(uint32_t i = 0; i < PL_DEVICE_LOCAL_LEVELS; i++)
    {
        const uint64_t uLevelSize = PL_DEVICE_ALLOCATION_BLOCK_SIZE / (1 << i);
        if(uLevelSize <= ulSize)
        {
            break;
        }
        uLevel = i;
    }

    uint32_t uMemoryType = 0u;
    bool bFound = false;
    for (uint32_t i = 0; i < ptData->tMemProps.memoryTypeCount; i++) 
    {
        if ((uTypeFilter & (1 << i)) && (ptData->tMemProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) 
        {
            uMemoryType = i;
            bFound = true;
            break;
        }
    }
    PL_ASSERT(bFound);

    const uint32_t uNode = pl__get_device_node(ptInst, uLevel, uMemoryType);
    PL_ASSERT(uNode != UINT32_MAX);

    plDeviceAllocationNode* ptNode = &ptData->sbtNodes[uNode];
    ptNode->ulSizeWasted = ptNode->ulSize - ulSize;

    const uint32_t uBlockCount =  pl_sb_size(ptData->sbtBlocks);
    plDeviceAllocationBlock* ptBlock = &ptData->sbtBlocks[ptNode->uBlockIndex];

    plDeviceMemoryAllocation tAllocation = {
        .pHostMapped = NULL,
        .tMemory     = (uint64_t)ptBlock->ulAddress,
        .ulOffset    = ptNode->ulOffset,
        .ulSize      = ulSize,
        .ptInst      = ptInst,
        .uNodeIndex  = uNode,
    };

    if(ulAlignment > 0)
        tAllocation.ulOffset = (((tAllocation.ulOffset) + ((ulAlignment)-1)) & ~((ulAlignment)-1));

    if(pcName == NULL)
        strncpy(ptData->sbtDebugNamesInDir[uNode], "unnamed", PL_MAX_NAME_LENGTH);
    else
        strncpy(ptData->sbtDebugNamesInDir[uNode], pcName, PL_MAX_NAME_LENGTH);
    return tAllocation;
}

static inline bool
pl__is_node_free(plDeviceAllocatorData* ptData, uint32_t uNode)
{

    // find what level we need
    uint32_t uLevel = 0;
    for(; uLevel < PL_DEVICE_LOCAL_LEVELS; uLevel++)
    {
        const uint64_t uLevelSize = PL_DEVICE_ALLOCATION_BLOCK_SIZE / (1 << uLevel);
        if(uLevelSize == ptData->sbtNodes[uNode].ulSize)
        {
            break; 
        }
    }
    uLevel = pl_minu(uLevel, PL_DEVICE_LOCAL_LEVELS - 1);

    // check if node is in freelist
    bool bInFreeList = false;
    uint32_t uNextNode = ptData->auFreeList[uLevel];
    while(uNextNode != UINT32_MAX)
    {

        if(uNextNode == ptData->sbtNodes[uNextNode].uNext)
            break;

        if(uNextNode == uNode)
        {
            bInFreeList = true;
            break;
        }
        uNextNode = ptData->sbtNodes[uNextNode].uNext;
    }
    
    const bool bFree = ptData->sbtNodes[uNode].ulSizeWasted == (ptData->sbtNodes[uNode].ulSize + 1);
    if(bFree)
    {
        PL_ASSERT(bInFreeList && "free item was not in list");
    }
    return bFree;
}

static void
pl__coalesce_nodes(plDeviceAllocatorData* ptData, uint32_t uLevel, uint32_t uNode)
{
    plDeviceAllocationNode* ptNode = &ptData->sbtNodes[uNode];

    // just return node to freelist
    if(uLevel == 0)
    {
        pl__add_node_to_freelist(ptData, uLevel, uNode);
        return;
    }

    bool bBothFree = false;
    uint32_t uLeftNode = uNode;
    uint32_t uRightNode = uNode + 1;

    if(ptNode->uBlockIndex % 2 == 0)
    {
        if(uNode % 2 == 1) // left node
        {
            if(pl__is_node_free(ptData, uRightNode))
            {

                bBothFree = true;
                pl__remove_node_from_freelist(ptData, uLevel, uRightNode);
            }
        }
        else
        {
            uLeftNode = uNode - 1;
            uRightNode = uNode;
            if(pl__is_node_free(ptData, uLeftNode))
            {
                bBothFree = true;
                pl__remove_node_from_freelist(ptData, uLevel, uLeftNode);
            }
        }
    }
    else
    {
        if(uNode % 2 == 1) // right node
        {
            if(pl__is_node_free(ptData, uLeftNode))
            {
                bBothFree = true;
                pl__remove_node_from_freelist(ptData, uLevel, uLeftNode);
            }
        }
        else
        {
            if(pl__is_node_free(ptData, uRightNode))
            {
                bBothFree = true;
                pl__remove_node_from_freelist(ptData, uLevel, uRightNode);
            }
        }
    }
    
    if(bBothFree) // need to coalese
    {

        if(uLevel > 1)
        {
            // find parent node
            const uint64_t uSizeOfParentLevel = PL_DEVICE_ALLOCATION_BLOCK_SIZE / ((uint64_t)1 << (uint64_t)(uLevel - 1));
            const uint32_t uParentLevelBlockCount = (1 << (uLevel - 1));
            uint32_t uIndexInLevel = (uint32_t)(ptData->sbtNodes[uLeftNode].ulOffset / uSizeOfParentLevel);
            const uint32_t uParentNode = uLeftNode - uParentLevelBlockCount - uIndexInLevel;
            pl__coalesce_nodes(ptData, uLevel - 1, uParentNode);
        }
        else
        {
            // find parent node
            const uint32_t uParentNode = uLeftNode - 1;
            pl__add_node_to_freelist(ptData, 0, uParentNode);
        }
        ptNode->ulSizeWasted = ptNode->ulSize; // ignored
    }
    else
    {
        pl__add_node_to_freelist(ptData, uLevel, uNode);
    }

}

static void
pl_free_local_buffer(struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;

    // ptData->sbtDebugNames[ptAllocation->uNodeIndex] = "";
    plDeviceAllocationNode* ptNode = &ptData->sbtNodes[ptAllocation->uNodeIndex];

    // find what level we need
    uint32_t uLevel = 0;
    for(; uLevel < PL_DEVICE_LOCAL_LEVELS; uLevel++)
    {
        const uint64_t uLevelSize = PL_DEVICE_ALLOCATION_BLOCK_SIZE / (1 << uLevel);
        if(uLevelSize == ptNode->ulSize)
        {
            break; 
        }
    }
    uLevel = pl_minu(uLevel, PL_DEVICE_LOCAL_LEVELS - 1);
    pl__coalesce_nodes(ptData, uLevel, ptAllocation->uNodeIndex);
    strncpy(ptData->sbtDebugNamesInDir[ptNode->uNodeIndex], "not used", PL_MAX_NAME_LENGTH);
}

static plDeviceMemoryAllocatorI
pl_create_staging_uncached_allocator(VkPhysicalDevice tPhysicalDevice, VkDevice tDevice)
{

    plDeviceAllocatorData* ptData = PL_ALLOC(sizeof(plDeviceAllocatorData));
    memset(ptData, 0, sizeof(plDeviceAllocatorData));

    // create allocator interface
    plDeviceMemoryAllocatorI tAllocatorInterface = 
    {
        .allocate = pl_allocate_staging_uncached,
        .free     = pl_free_staging_uncached,
        .blocks   = pl_blocks_dedicated,
        .nodes    = pl_nodes_dedicated,
        .names    = pl_names_dedicated,
        .ptInst   = (struct plDeviceMemoryAllocatorO*)ptData
    };
    vkGetPhysicalDeviceMemoryProperties(tPhysicalDevice, &ptData->tMemProps);
    ptData->tDevice = tDevice;
    return tAllocatorInterface;
}

static plDeviceMemoryAllocatorI
pl_create_staging_cached_allocator(VkPhysicalDevice tPhysicalDevice, VkDevice tDevice)
{
    plDeviceAllocatorData* ptData = PL_ALLOC(sizeof(plDeviceAllocatorData));
    memset(ptData, 0, sizeof(plDeviceAllocatorData));

    // create allocator interface
    plDeviceMemoryAllocatorI tAllocatorInterface = 
    {
        .allocate = pl_allocate_staging_cached,
        .free     = pl_free_staging_cached,
        .blocks   = pl_blocks_dedicated,
        .nodes    = pl_nodes_dedicated,
        .names    = pl_names_dedicated,
        .ptInst   = (struct plDeviceMemoryAllocatorO*)ptData
    };
    vkGetPhysicalDeviceMemoryProperties(tPhysicalDevice, &ptData->tMemProps);
    ptData->tDevice = tDevice;
    return tAllocatorInterface;    
}

static void
pl_cleanup_device_local_dedicated_allocator(plDeviceMemoryAllocatorI* ptAllocator)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptAllocator->ptInst;

    for(uint32_t i = 0; i < pl_sb_size(ptData->sbtBlocks); i++)
    {
        plDeviceAllocationBlock* ptBlock = &ptData->sbtBlocks[i];
        vkFreeMemory(ptData->tDevice, (VkDeviceMemory)ptBlock->sbtRanges[0].tAllocation.tMemory, NULL);
        ptBlock->sbtRanges[0].tAllocation.pHostMapped  = NULL;
        ptBlock->sbtRanges[0].tAllocation.tMemory      = 0;
        ptBlock->sbtRanges[0].tAllocation.ulOffset     = 0;
        ptBlock->sbtRanges[0].tAllocation.ulSize       = 0;
        pl_sb_free(ptBlock->sbtRanges);
    }
    pl_sb_free(ptData->sbtBlocks);
    pl_sb_free(ptData->sbtDebugNamesInDir);
    pl_sb_free(ptData->sbcDebugNames);
    pl_sb_free(ptData->sbtNodes);
    PL_FREE(ptData);
}

static void
pl_cleanup_staging_uncached_allocator(plDeviceMemoryAllocatorI* ptAllocator)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptAllocator->ptInst;

    for(uint32_t i = 0; i < pl_sb_size(ptData->sbtBlocks); i++)
    {
        plDeviceAllocationBlock* ptBlock = &ptData->sbtBlocks[i];
        if(ptBlock->sbtRanges[0].tAllocation.pHostMapped)
            vkUnmapMemory(ptData->tDevice, (VkDeviceMemory)ptBlock->sbtRanges[0].tAllocation.tMemory);

        vkFreeMemory(ptData->tDevice, (VkDeviceMemory)ptBlock->sbtRanges[0].tAllocation.tMemory, NULL);

        ptBlock->sbtRanges[0].tAllocation.pHostMapped  = NULL;
        ptBlock->sbtRanges[0].tAllocation.tMemory      = 0;
        ptBlock->sbtRanges[0].tAllocation.ulOffset     = 0;
        ptBlock->sbtRanges[0].tAllocation.ulSize       = 0;
        
        pl_sb_free(ptBlock->sbtRanges);
    }
    pl_sb_free(ptData->sbtBlocks);
    pl_sb_free(ptData->sbtDebugNamesInDir);
    pl_sb_free(ptData->sbcDebugNames);
    pl_sb_free(ptData->sbtNodes);
    PL_FREE(ptData);
}
static void
pl_cleanup_staging_cached_allocator(plDeviceMemoryAllocatorI* ptAllocator)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptAllocator->ptInst;

    for(uint32_t i = 0; i < pl_sb_size(ptData->sbtBlocks); i++)
    {
        plDeviceAllocationBlock* ptBlock = &ptData->sbtBlocks[i];
        if(ptBlock->sbtRanges[0].tAllocation.pHostMapped)
            vkUnmapMemory(ptData->tDevice, (VkDeviceMemory)ptBlock->sbtRanges[0].tAllocation.tMemory);

        vkFreeMemory(ptData->tDevice, (VkDeviceMemory)ptBlock->sbtRanges[0].tAllocation.tMemory, NULL);

        ptBlock->sbtRanges[0].tAllocation.pHostMapped  = NULL;
        ptBlock->sbtRanges[0].tAllocation.tMemory      = 0;
        ptBlock->sbtRanges[0].tAllocation.ulOffset     = 0;
        ptBlock->sbtRanges[0].tAllocation.ulSize       = 0;

        pl_sb_free(ptBlock->sbtRanges);
    }
    pl_sb_free(ptData->sbtBlocks);
    pl_sb_free(ptData->sbtDebugNamesInDir);
    pl_sb_free(ptData->sbcDebugNames);
    pl_sb_free(ptData->sbtNodes);
    PL_FREE(ptData);
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static void
pl__add_node_to_freelist(plDeviceAllocatorData* ptData, uint32_t uLevel, uint32_t uNode)
{
    plDeviceAllocationNode* ptNode = &ptData->sbtNodes[uNode];
    ptNode->ulSizeWasted = ptNode->ulSize + 1; // free
    ptData->sbtNodes[uNode].uNext = ptData->auFreeList[uLevel];
    ptData->auFreeList[uLevel] = uNode;
}

static  void
pl__remove_node_from_freelist(plDeviceAllocatorData* ptData, uint32_t uLevel, uint32_t uNode)
{

    bool bFound = false;
    if(ptData->auFreeList[uLevel] == uNode)
    {
        ptData->auFreeList[uLevel] = ptData->sbtNodes[uNode].uNext;
        bFound = true;
    }
    else
    {
        uint32_t uNextNode = ptData->auFreeList[uLevel];
        while(uNextNode != UINT32_MAX)
        {
            uint32_t uPrevNode = uNextNode;
            uNextNode = ptData->sbtNodes[uPrevNode].uNext;
            
            if(uNextNode == uNode)
            {
                ptData->sbtNodes[uPrevNode].uNext = ptData->sbtNodes[uNode].uNext;
                bFound = true;
                break;
            }
        }
    }

    plDeviceAllocationNode* ptNode = &ptData->sbtNodes[uNode];
    ptNode->ulSizeWasted = ptNode->ulSize; // ignored
    ptNode->uNext = UINT32_MAX;
    PL_ASSERT(bFound && "could not find node to remove");
}

static uint32_t
pl__create_device_node(struct plDeviceMemoryAllocatorO* ptInst, uint32_t uMemoryType)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;
    uint32_t uNode = UINT32_MAX;

    plDeviceAllocationBlock tAllocationBlock = {
            .ulSize = PL_DEVICE_ALLOCATION_BLOCK_SIZE
    };

    const VkMemoryAllocateInfo tAllocInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = tAllocationBlock.ulSize,
        .memoryTypeIndex = uMemoryType
    };

    VkResult tResult = vkAllocateMemory(ptData->tDevice, &tAllocInfo, NULL, (VkDeviceMemory*)&tAllocationBlock.ulAddress);
    PL_VULKAN(tResult);
    
    uNode = pl_sb_size(ptData->sbtNodes);
    uint32_t uNodeIndex = uNode;
    pl_sb_resize(ptData->sbcDebugNames, pl_sb_size(ptData->sbcDebugNames) + PL_MAX_NAME_LENGTH * ((1 << PL_DEVICE_LOCAL_LEVELS) - 1));
    pl_sb_resize(ptData->sbtDebugNamesInDir, pl_sb_size(ptData->sbtDebugNamesInDir) + (1 << PL_DEVICE_LOCAL_LEVELS) - 1);
    pl_sb_resize(ptData->sbtNodes, pl_sb_size(ptData->sbtNodes) + (1 << PL_DEVICE_LOCAL_LEVELS) - 1);
    const uint32_t uBlockIndex = pl_sb_size(ptData->sbtBlocks);
    for(uint32_t uLevelIndex = 0; uLevelIndex < PL_DEVICE_LOCAL_LEVELS; uLevelIndex++)
    {
        const uint64_t uSizeOfLevel = tAllocationBlock.ulSize / ((uint64_t)1 << (uint64_t)uLevelIndex);
        const uint32_t uLevelBlockCount = (1 << uLevelIndex);
        uint64_t uCurrentOffset = 0;
        for(uint32_t i = 0; i < uLevelBlockCount; i++)
        {
            ptData->sbtNodes[uNodeIndex].uNodeIndex   = uNodeIndex;
            ptData->sbtNodes[uNodeIndex].uNext        = UINT32_MAX;
            ptData->sbtNodes[uNodeIndex].ulOffset     = uCurrentOffset;
            ptData->sbtNodes[uNodeIndex].ulSize       = uSizeOfLevel;
            ptData->sbtNodes[uNodeIndex].uBlockIndex  = uBlockIndex;
            ptData->sbtNodes[uNodeIndex].uMemoryType  = uMemoryType;
            ptData->sbtNodes[uNodeIndex].ulSizeWasted = uSizeOfLevel;
            strncpy(&ptData->sbcDebugNames[uNodeIndex * PL_MAX_NAME_LENGTH], "not used", PL_MAX_NAME_LENGTH);
            ptData->sbtDebugNamesInDir[uNodeIndex] = &ptData->sbcDebugNames[uNodeIndex * PL_MAX_NAME_LENGTH];

            uCurrentOffset += uSizeOfLevel;
            uNodeIndex++;
        }
    }
    pl_sb_push(ptData->sbtBlocks, tAllocationBlock);  
    for(uint32_t i = 0; i < pl_sb_size(ptData->sbtDebugNamesInDir); i++)
        ptData->sbtDebugNamesInDir[i] = &ptData->sbcDebugNames[i * PL_MAX_NAME_LENGTH];
    return uNode;
}

static uint32_t
pl__get_device_node(struct plDeviceMemoryAllocatorO* ptInst, uint32_t uLevel, uint32_t uMemoryType)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;
    uint32_t uNode = UINT32_MAX;

    if(uLevel == 0)
    {
        if(ptData->auFreeList[0] == UINT32_MAX)  // no nodes available
        {
            uNode = pl__create_device_node(ptInst, uMemoryType);
            pl__add_node_to_freelist(ptData, 0, uNode);
        }
        else // nodes available
        {
            // look for block with correct memory type
            uint32_t uNextNode = ptData->auFreeList[0];
            while(uNextNode != UINT32_MAX)
            {  
                if(ptData->sbtNodes[uNextNode].uMemoryType == uMemoryType)
                {
                    uNode = uNextNode;
                    break;
                }
                uNextNode = ptData->sbtNodes[uNextNode].uNext;
            }

            if(uNode == UINT32_MAX) // could not find block with correct memory type
            {
                uNode = pl__create_device_node(ptInst, uMemoryType);
                pl__add_node_to_freelist(ptData, 0, uNode);
            }
        }
    }
    else if(ptData->auFreeList[uLevel] == UINT32_MAX) // no nodes available at the required level
    {
        // get bigger block and split it and return left block
        uint32_t uParentNode = pl__get_device_node(ptInst, uLevel - 1, uMemoryType);
        plDeviceAllocationNode* ptParentNode = &ptData->sbtNodes[uParentNode];
        ptParentNode->ulSizeWasted = ptParentNode->ulSize; // ignore

        const uint64_t uSizeOfLevel = PL_DEVICE_ALLOCATION_BLOCK_SIZE / ((uint64_t)1 << (uint64_t)(uLevel - 1));
        const uint32_t uLevelBlockCount = (1 << (uLevel - 1));
        uint32_t uIndexInLevel = (uint32_t)(ptParentNode->ulOffset / uSizeOfLevel);

        const uint32_t uLeftIndex  = uParentNode + uLevelBlockCount + uIndexInLevel;
        const uint32_t uRightIndex = uParentNode + uLevelBlockCount + uIndexInLevel + 1;

        pl__add_node_to_freelist(ptData, uLevel, uLeftIndex);
        pl__add_node_to_freelist(ptData, uLevel, uRightIndex);

        uNode = uLeftIndex;
    }
    else // nodes available at required level
    {
        // look for block with correct memory type
        uint32_t uNextNode = ptData->auFreeList[uLevel];
        while(uNextNode != UINT32_MAX)
        {  
            if(ptData->sbtNodes[uNextNode].uMemoryType == uMemoryType)
            {
                uNode = uNextNode;
                break;
            }
            uNextNode = ptData->sbtNodes[uNextNode].uNext;
        }

        if(uNode == UINT32_MAX) // could not find block with correct memory type
        {
            uint32_t uParentNode = pl__get_device_node(ptInst, uLevel - 1, uMemoryType);
            plDeviceAllocationNode* ptParentNode = &ptData->sbtNodes[uParentNode];

            const uint64_t uSizeOfLevel = PL_DEVICE_ALLOCATION_BLOCK_SIZE / ((uint64_t)1 << (uint64_t)(uLevel - 1));
            const uint32_t uLevelBlockCount = (1 << (uLevel - 1));
            uint32_t uIndexInLevel = (uint32_t)(ptParentNode->ulOffset / uSizeOfLevel);

            const uint32_t uLeftIndex  = uParentNode + uLevelBlockCount + uIndexInLevel;
            const uint32_t uRightIndex = uParentNode + uLevelBlockCount + uIndexInLevel + 1;

            pl__add_node_to_freelist(ptData, uLevel, uLeftIndex);
            pl__add_node_to_freelist(ptData, uLevel, uRightIndex);
            uNode = uLeftIndex;
        }
    }

    pl__remove_node_from_freelist(ptData, uLevel, uNode);
    return uNode;
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

static VkFormat
pl__vulkan_format(plFormat tFormat)
{
    switch(tFormat)
    {
        case PL_FORMAT_R32G32B32_FLOAT:   return VK_FORMAT_R32G32B32_SFLOAT;
        case PL_FORMAT_R8G8B8A8_UNORM:    return VK_FORMAT_R8G8B8A8_UNORM;
        case PL_FORMAT_R32G32_FLOAT:      return VK_FORMAT_R32G32_SFLOAT;
        case PL_FORMAT_R8G8B8A8_SRGB:     return VK_FORMAT_R8G8B8A8_SRGB;
        case PL_FORMAT_B8G8R8A8_SRGB:     return VK_FORMAT_B8G8R8A8_SRGB;
        case PL_FORMAT_B8G8R8A8_UNORM:    return VK_FORMAT_B8G8R8A8_UNORM;
        case PL_FORMAT_D32_FLOAT:         return VK_FORMAT_D32_SFLOAT;
        case PL_FORMAT_D32_FLOAT_S8_UINT: return VK_FORMAT_D32_SFLOAT_S8_UINT;
        case PL_FORMAT_D24_UNORM_S8_UINT: return VK_FORMAT_D24_UNORM_S8_UINT;
        case PL_FORMAT_D16_UNORM_S8_UINT: return VK_FORMAT_D16_UNORM_S8_UINT;
    }

    PL_ASSERT(false && "Unsupported format");
    return VK_FORMAT_UNDEFINED;
}

static plFormat
pl__pilotlight_format(VkFormat tFormat)
{
    switch(tFormat)
    {
        case VK_FORMAT_R32G32B32_SFLOAT:   return PL_FORMAT_R32G32B32_FLOAT;
        case VK_FORMAT_R8G8B8A8_UNORM:     return PL_FORMAT_R8G8B8A8_UNORM;
        case VK_FORMAT_R32G32_SFLOAT:      return PL_FORMAT_R32G32_FLOAT;
        case VK_FORMAT_R8G8B8A8_SRGB:      return PL_FORMAT_R8G8B8A8_SRGB;
        case VK_FORMAT_B8G8R8A8_SRGB:      return PL_FORMAT_B8G8R8A8_SRGB;
        case VK_FORMAT_B8G8R8A8_UNORM:     return PL_FORMAT_B8G8R8A8_UNORM;
        case VK_FORMAT_D32_SFLOAT:         return PL_FORMAT_D32_FLOAT;
        case VK_FORMAT_D32_SFLOAT_S8_UINT: return PL_FORMAT_D32_FLOAT_S8_UINT;
        case VK_FORMAT_D24_UNORM_S8_UINT:  return PL_FORMAT_D24_UNORM_S8_UINT;
        case VK_FORMAT_D16_UNORM_S8_UINT:  return PL_FORMAT_D16_UNORM_S8_UINT;
        default:
            break;
    }

    PL_ASSERT(false && "Unsupported format");
    return PL_FORMAT_UNKNOWN;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
pl__debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT tMsgSeverity, VkDebugUtilsMessageTypeFlagsEXT tMsgType, const VkDebugUtilsMessengerCallbackDataEXT* ptCallbackData, void* pUserData) 
{
    if(tMsgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        printf("error validation layer: %s\n", ptCallbackData->pMessage);
        pl_log_error_to_f(uLogChannel, "error validation layer: %s\n", ptCallbackData->pMessage);
    }

    else if(tMsgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        printf("warn validation layer: %s\n", ptCallbackData->pMessage);
        pl_log_warn_to_f(uLogChannel, "warn validation layer: %s\n", ptCallbackData->pMessage);
    }

    else if(tMsgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    {
        // pl_log_trace_to_f(uLogChannel, "info validation layer: %s\n", ptCallbackData->pMessage);
    }
    else if(tMsgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
    {
        // pl_log_trace_to_f(uLogChannel, "trace validation layer: %s\n", ptCallbackData->pMessage);
    }
    
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
    pl_log_info_to_f(uLogChannel, "Device ID: %u", ptDeviceOut->tDeviceProps.deviceID);
    pl_log_info_to_f(uLogChannel, "Vendor ID: %u", ptDeviceOut->tDeviceProps.vendorID);
    pl_log_info_to_f(uLogChannel, "API Version: %u", ptDeviceOut->tDeviceProps.apiVersion);
    pl_log_info_to_f(uLogChannel, "Driver Version: %u", ptDeviceOut->tDeviceProps.driverVersion);
    pl_log_info_to_f(uLogChannel, "Device Type: %s", pacDeviceTypeName[ptDeviceOut->tDeviceProps.deviceType]);
    pl_log_info_to_f(uLogChannel, "Device Name: %s", ptDeviceOut->tDeviceProps.deviceName);

    uint32_t uExtensionCount = 0;
    vkEnumerateDeviceExtensionProperties(ptDeviceOut->tPhysicalDevice, NULL, &uExtensionCount, NULL);
    VkExtensionProperties* ptExtensions = PL_ALLOC(uExtensionCount * sizeof(VkExtensionProperties));
    vkEnumerateDeviceExtensionProperties(ptDeviceOut->tPhysicalDevice, NULL, &uExtensionCount, ptExtensions);

    for(uint32_t i = 0; i < uExtensionCount; i++)
    {
        if(pl_str_equal(ptExtensions[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) ptDeviceOut->bSwapchainExtPresent = true; //-V522
        if(pl_str_equal(ptExtensions[i].extensionName, "VK_KHR_portability_subset"))     ptDeviceOut->bPortabilitySubsetPresent = true; //-V522
    }

    PL_FREE(ptExtensions);
    return iBestDvcIdx;
}

static plFrameContext*
pl__create_frame_resources(plDevice* ptDevice, uint32_t uFramesInFlight)
{

    plFrameContext* sbFrames = NULL;

    VkDevice tDevice = ptDevice->tLogicalDevice;

    const VkCommandPoolCreateInfo tCommandPoolInfo = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = ptDevice->iGraphicsQueueFamily,
        .flags            = 0
    };
    
    const VkSemaphoreCreateInfo tSemaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    const VkFenceCreateInfo tFenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    pl_sb_resize(sbFrames, uFramesInFlight);
    for(uint32_t i = 0; i < uFramesInFlight; i++)
    {
        plFrameContext tFrame = {0};
        PL_VULKAN(vkCreateSemaphore(tDevice, &tSemaphoreInfo, NULL, &tFrame.tImageAvailable));
        PL_VULKAN(vkCreateSemaphore(tDevice, &tSemaphoreInfo, NULL, &tFrame.tRenderFinish));
        PL_VULKAN(vkCreateFence(tDevice, &tFenceInfo, NULL, &tFrame.tInFlight));
        PL_VULKAN(vkCreateCommandPool(tDevice, &tCommandPoolInfo, NULL, &tFrame.tCmdPool));

        const VkCommandBufferAllocateInfo tAllocInfo = {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool        = tFrame.tCmdPool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        PL_VULKAN(vkAllocateCommandBuffers(tDevice, &tAllocInfo, &tFrame.tCmdBuf));  
        sbFrames[i] = tFrame;
    }

    return sbFrames;
}

static void
pl__create_framebuffers(plDevice* ptDevice, uint32_t uRenderPass, plSwapchain* ptSwapchain)
{
    for(uint32_t i = 0; i < ptSwapchain->uImageCount; i++)
    {
        uint32_t auAttachments[] = {
            ptSwapchain->uColorTextureView,
            ptSwapchain->uDepthTextureView,
            ptSwapchain->puImageViews[i]
        };

        plFrameBufferDesc tDesc = {
            .uAttachmentCount = 3,
            .puAttachments    = auAttachments,
            .uRenderPass      = uRenderPass,
            .uWidth           = ptSwapchain->tExtent.width,
            .uHeight          = ptSwapchain->tExtent.height,
        };

        ptSwapchain->puFrameBuffers[i] = pl_create_frame_buffer(ptDevice, &tDesc, "main frame buffer");
    }
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
pl__create_shader_pipeline(plGraphics* ptGraphics, plGraphicsState tVariant, plVariantInfo* ptInfo)
{
    // plGraphics* ptGraphics = ptResourceManager->_ptGraphics;
    plDevice* ptDevice = &ptGraphics->tDevice;
    VkDevice tLogicalDevice = ptDevice->tLogicalDevice;

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
        .renderPass          = ptDevice->sbtRenderPasses[ptInfo->uRenderPass]._tRenderPass,
        .subpass             = 0u,
        .basePipelineHandle  = VK_NULL_HANDLE,
        .pDepthStencilState  = &tDepthStencil,
    };
    PL_VULKAN(vkCreateGraphicsPipelines(tLogicalDevice, VK_NULL_HANDLE, 1, &tPipelineInfo, NULL, &tPipeline));

    vkDestroyShaderModule(tLogicalDevice, tVertexShaderStageInfo.module, NULL);
    vkDestroyShaderModule(tLogicalDevice, tPixelShaderStageInfo.module, NULL);

    return tPipeline;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_vulkan_ext(plApiRegistryApiI* ptApiRegistry, bool bReload)
{
    plDataRegistryApiI* ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(ptDataRegistry->get_data("memory"));
    pl_set_profile_context(ptDataRegistry->get_data("profile"));
    pl_set_log_context(ptDataRegistry->get_data("log"));
    pl_set_io_context(ptDataRegistry->get_data(PL_CONTEXT_IO_NAME));
    
    if(bReload)
    {
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_DESCRIPTOR_MANAGER), pl_load_descriptor_manager_api());
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_DEVICE), pl_load_device_api());
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_GRAPHICS), pl_load_graphics_api());
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_DEVICE_MEMORY), pl_load_device_memory_api());
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_BACKEND_VULKAN), pl_load_render_backend_api());

        // find log channel
        uint32_t uChannelCount = 0;
        plLogChannel* ptChannels = pl_get_log_channels(&uChannelCount);
        for(uint32_t i = 0; i < uChannelCount; i++)
        {
            if(strcmp(ptChannels[i].pcName, "Vulkan") == 0)
            {
                uLogChannel = i;
                break;
            }
        }
    }
    else
    {
        ptApiRegistry->add(PL_API_DESCRIPTOR_MANAGER, pl_load_descriptor_manager_api());
        ptApiRegistry->add(PL_API_DEVICE, pl_load_device_api());
        ptApiRegistry->add(PL_API_GRAPHICS, pl_load_graphics_api());
        ptApiRegistry->add(PL_API_DEVICE_MEMORY, pl_load_device_memory_api());
        ptApiRegistry->add(PL_API_BACKEND_VULKAN, pl_load_render_backend_api());
        uLogChannel = pl_add_log_channel("Vulkan", PL_CHANNEL_TYPE_CYCLIC_BUFFER);
    }
}

PL_EXPORT void
pl_unload_vulkan_ext(plApiRegistryApiI* ptApiRegistry)
{
    
}