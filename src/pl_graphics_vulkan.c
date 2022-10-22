/*
   vulkan_pl_graphics.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] internal functions
// [SECTION] implementations
// [SECTION] internal implementations
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_graphics_vulkan.h"
#include "pl_ds.h"
#include <stdio.h>

#ifdef _WIN32
#pragma comment(lib, "vulkan-1.lib")
#endif

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

static VKAPI_ATTR VkBool32 VKAPI_CALL pl__debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT msgSeverity, VkDebugUtilsMessageTypeFlagsEXT msgType, const VkDebugUtilsMessengerCallbackDataEXT *ptrCallbackData, void *ptrUserData);

//-----------------------------------------------------------------------------
// [SECTION] internal functions
//-----------------------------------------------------------------------------

static uint32_t pl__get_u32_max(uint32_t a, uint32_t b) { return a > b ? a : b;}
static uint32_t pl__get_u32_min(uint32_t a, uint32_t b) { return a < b ? a : b;}

static int
pl__select_physical_device(VkInstance instance, plVulkanDevice* deviceOut)
{
    uint32_t deviceCount = 0u;
    int iBestDvcIdx = 0;
    bool bDiscreteGPUFound = false;
    VkDeviceSize maxLocalMemorySize = 0u;

    PL_VULKAN(vkEnumeratePhysicalDevices(instance, &deviceCount, NULL));
    PL_ASSERT(deviceCount > 0 && "failed to find GPUs with Vulkan support!");

    //-----------------------------------------------------------------------------
    // check if device is suitable
    //-----------------------------------------------------------------------------
    VkPhysicalDevice devices[16] = {0};
    PL_VULKAN(vkEnumeratePhysicalDevices(instance, &deviceCount, devices));

    // prefer discrete, then memory size
    for(uint32_t i = 0; i < deviceCount; i++)
    {
        vkGetPhysicalDeviceProperties(devices[i], &deviceOut->deviceProps);
        vkGetPhysicalDeviceMemoryProperties(devices[i], &deviceOut->memProps);

        for(uint32_t j = 0; j < deviceOut->memProps.memoryHeapCount; j++)
        {
            if(deviceOut->memProps.memoryHeaps[j].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT && deviceOut->memProps.memoryHeaps[j].size > maxLocalMemorySize && !bDiscreteGPUFound)
            {
                maxLocalMemorySize = deviceOut->memProps.memoryHeaps[j].size;
                iBestDvcIdx = i;
            }
        }

        if(deviceOut->deviceProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && !bDiscreteGPUFound)
        {
            iBestDvcIdx = i;
            bDiscreteGPUFound = true;
        }
    }

    deviceOut->physicalDevice = devices[iBestDvcIdx];
    PL_ASSERT(deviceOut->physicalDevice != VK_NULL_HANDLE && "failed to find a suitable GPU!");
    vkGetPhysicalDeviceProperties(devices[iBestDvcIdx], &deviceOut->deviceProps);
    vkGetPhysicalDeviceMemoryProperties(devices[iBestDvcIdx], &deviceOut->memProps);
    static const char *ptrDeviceTypeName[] = {"Other", "Integrated", "Discrete", "Virtual", "CPU"};

    // print info on chosen device
    printf("Device ID: %u\n", deviceOut->deviceProps.deviceID);
    printf("Vendor ID: %u\n", deviceOut->deviceProps.vendorID);
    printf("API Version: %u\n", deviceOut->deviceProps.apiVersion);
    printf("Driver Version: %u\n", deviceOut->deviceProps.driverVersion);
    printf("Device Type: %s\n", ptrDeviceTypeName[deviceOut->deviceProps.deviceType]);
    printf("Device Name: %s\n", deviceOut->deviceProps.deviceName);
    return iBestDvcIdx;
}

//-----------------------------------------------------------------------------
// [SECTION] implementations
//-----------------------------------------------------------------------------

void
pl_create_instance(plVulkanGraphics* graphics, uint32_t version, bool enableValidation)
{
    const char* khronosValidationLayer = "VK_LAYER_KHRONOS_validation";
    const char* enabledExtensions[3] = {0};

    const uint32_t extensionCount = enableValidation ? 3 : 2;
    const uint32_t layerCount = enableValidation ? 1 : 0;

    enabledExtensions[0] = VK_KHR_SURFACE_EXTENSION_NAME;

    #ifdef _WIN32
        enabledExtensions[1] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
    #elif defined(__APPLE__) // not supported yet
    #else // linux
        enabledExtensions[1] = VK_KHR_XCB_SURFACE_EXTENSION_NAME;
    #endif

    if(enableValidation)
        enabledExtensions[2] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

    pl_create_instance_ex(graphics, version, layerCount, &khronosValidationLayer, extensionCount, enabledExtensions);
}

void
pl_create_instance_ex(plVulkanGraphics* graphics, uint32_t version, uint32_t layerCount, const char** enabledLayers, uint32_t extensioncount, const char** enabledExtensions)
{

    // check if validation should be activated
    bool validationEnabled = false;
    for(uint32_t i = 0; i < layerCount; i++)
    {
        if(strcmp("VK_LAYER_KHRONOS_validation", enabledLayers[i]) == 0)
        {
            validationEnabled = true;
            break;
        }
    }

    // retrieve supported layers
    uint32_t instanceLayersFound = 0u;
    VkLayerProperties* availableLayers = NULL;
    PL_VULKAN(vkEnumerateInstanceLayerProperties(&instanceLayersFound, NULL));
    if(instanceLayersFound > 0)
    {
        availableLayers = (VkLayerProperties*)malloc(sizeof(VkLayerProperties)*instanceLayersFound);
        PL_VULKAN(vkEnumerateInstanceLayerProperties(&instanceLayersFound, availableLayers));
    }

    // retrieve supported extensions
    uint32_t instanceExtensionsFound = 0u;
    VkExtensionProperties* availableExtensions = NULL;
    PL_VULKAN(vkEnumerateInstanceExtensionProperties(NULL, &instanceExtensionsFound, NULL));
    if(instanceExtensionsFound > 0)
    {
        availableExtensions = (VkExtensionProperties*)malloc(sizeof(VkExtensionProperties)*instanceExtensionsFound);
        PL_VULKAN(vkEnumerateInstanceExtensionProperties(NULL, &instanceExtensionsFound, availableExtensions));
    }

    // ensure extensions are supported
    const char** sbMissingExtensions = NULL;
    for(uint32_t i = 0; i < extensioncount; i++)
    {
        const char* requestedExtension = enabledExtensions[i];
        bool extensionFound = false;
        for(uint32_t j = 0; j < instanceExtensionsFound; j++)
        {
            if(strcmp(requestedExtension, availableExtensions[j].extensionName) == 0)
            {
                extensionFound = true;
                break;
            }
        }

        if(!extensionFound)
        {
            pl_sb_push(sbMissingExtensions, requestedExtension);
        }
    }

    // report if all requested extensions aren't found
    if(pl_sb_size(sbMissingExtensions) > 0)
    {
        printf("%d %s\n", pl_sb_size(sbMissingExtensions), "Missing Extensions:");
        for(uint32_t i = 0; i < pl_sb_size(sbMissingExtensions); i++)
        {
            printf("  * %s\n", sbMissingExtensions[i]);
        }
        PL_ASSERT(false && "Can't find all requested extensions");
    }

    // ensure layers are supported
    const char** sbMissingLayers = NULL;
    for(uint32_t i = 0; i < layerCount; i++)
    {
        const char* requestedLayer = enabledLayers[i];
        bool layerFound = false;
        for(uint32_t j = 0; j < instanceLayersFound; j++)
        {
            if(strcmp(requestedLayer, availableLayers[j].layerName) == 0)
            {
                layerFound = true;
                break;
            }
        }

        if(!layerFound)
        {
            pl_sb_push(sbMissingLayers, requestedLayer);
        }
    }

    // report if all requested layers aren't found
    if(pl_sb_size(sbMissingLayers) > 0)
    {
        printf("%d %s\n", pl_sb_size(sbMissingLayers), "Missing Layers:");
        for(uint32_t i = 0; i < pl_sb_size(sbMissingLayers); i++)
        {
            printf("  * %s\n", sbMissingLayers[i]);
        }
        PL_ASSERT(false && "Can't find all requested layers");
    }

    // Setup debug messenger for vulkan instance
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = pl__debug_callback,
        .pNext = VK_NULL_HANDLE
    };

    // create vulkan instance
    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = version
    };

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .pNext = validationEnabled ? (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo : VK_NULL_HANDLE,

        // extensions
        .enabledExtensionCount = extensioncount,
        .ppEnabledExtensionNames = enabledExtensions,

        // layers
        .enabledLayerCount = layerCount,
        .ppEnabledLayerNames = enabledLayers,  
    };

    PL_VULKAN(vkCreateInstance(&createInfo, NULL, &graphics->instance));
    printf("%s\n", "created Vulkan instance.");

    // cleanup
    if(availableLayers)     free(availableLayers);
    if(availableExtensions) free(availableExtensions);
    pl_sb_free(sbMissingLayers);
    pl_sb_free(sbMissingExtensions);

    if(validationEnabled)
    {
        PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(graphics->instance, "vkCreateDebugUtilsMessengerEXT");
        PL_ASSERT(func != NULL && "failed to set up debug messenger!");
        PL_VULKAN(func(graphics->instance, &debugCreateInfo, NULL, &graphics->dbgMessenger));
        printf("%s\n", "enabled Vulkan validation layers.");       
    }
}

void
pl_create_device(VkInstance instance, VkSurfaceKHR surface, plVulkanDevice* deviceOut, bool enableValidation)
{
    deviceOut->graphicsQueueFamily = -1;
    deviceOut->presentQueueFamily = -1;
    int deviceIndex = -1;
    deviceOut->memProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    deviceOut->memBudgetInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
    deviceOut->memProps2.pNext = &deviceOut->memBudgetInfo;
    deviceIndex = pl__select_physical_device(instance, deviceOut);
    deviceOut->maxLocalMemSize = deviceOut->memProps.memoryHeaps[deviceIndex].size;

    // find queue families
    uint32_t uQueueFamCnt = 0u;
    vkGetPhysicalDeviceQueueFamilyProperties(deviceOut->physicalDevice, &uQueueFamCnt, NULL);

    VkQueueFamilyProperties queueFamilies[64] = {0};
    vkGetPhysicalDeviceQueueFamilyProperties(deviceOut->physicalDevice, &uQueueFamCnt, queueFamilies);

    for(uint32_t i = 0; i < uQueueFamCnt; i++)
    {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) deviceOut->graphicsQueueFamily = i;

        VkBool32 presentSupport = false;
        PL_VULKAN(vkGetPhysicalDeviceSurfaceSupportKHR(deviceOut->physicalDevice, i, surface, &presentSupport));

        if (presentSupport) deviceOut->presentQueueFamily  = i;

        if (deviceOut->graphicsQueueFamily > -1 && deviceOut->presentQueueFamily > -1) // complete
            break;
        i++;
    }

    //-----------------------------------------------------------------------------
    // create logical device
    //-----------------------------------------------------------------------------

    VkPhysicalDeviceFeatures deviceFeatures = {0};
    VkDeviceQueueCreateInfo ptrQueueCreateInfos[2] = {0};
    float fQueuePriority = 1.0f;
    for(int i = 0; i < 2; i++)
    {
        ptrQueueCreateInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        ptrQueueCreateInfos[i].queueFamilyIndex = i;
        ptrQueueCreateInfos[i].queueCount = 1;
        ptrQueueCreateInfos[i].pQueuePriorities = &fQueuePriority;
    }

    static const char* validationLayers[] = {"VK_LAYER_KHRONOS_validation"};
    VkDeviceCreateInfo createDeviceInfo = {0};
    createDeviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createDeviceInfo.queueCreateInfoCount = 1u;
    createDeviceInfo.pQueueCreateInfos = ptrQueueCreateInfos;
    createDeviceInfo.pEnabledFeatures = &deviceFeatures;
    createDeviceInfo.enabledExtensionCount = 1u;
    static const char *cPtrExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    createDeviceInfo.ppEnabledExtensionNames = cPtrExtensions;
    createDeviceInfo.enabledLayerCount = 0u;
    if(enableValidation)
    {
        createDeviceInfo.enabledLayerCount = 1u;
        createDeviceInfo.ppEnabledLayerNames = validationLayers;
    }
    PL_VULKAN(vkCreateDevice(deviceOut->physicalDevice, &createDeviceInfo, NULL, &deviceOut->logicalDevice));

    //-----------------------------------------------------------------------------
    // get device queues
    //-----------------------------------------------------------------------------

    vkGetDeviceQueue(deviceOut->logicalDevice, deviceOut->graphicsQueueFamily, 0, &deviceOut->graphicsQueue);
    vkGetDeviceQueue(deviceOut->logicalDevice, deviceOut->presentQueueFamily, 0, &deviceOut->presentQueue);
}

void
pl_create_frame_resources(plVulkanGraphics* graphics, plVulkanDevice* device)
{
    // create command pool
    VkCommandPoolCreateInfo commandPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = device->graphicsQueueFamily,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
    };
    PL_VULKAN(vkCreateCommandPool(device->logicalDevice, &commandPoolInfo, NULL, &graphics->cmdPool));

    VkSemaphoreCreateInfo semaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = graphics->cmdPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    graphics->framesInFlight = PL_MAX_FRAMES_IN_FLIGHT;
    pl_sb_reserve(graphics->sbFrames, graphics->framesInFlight);
    for(uint32_t i = 0; i < graphics->framesInFlight; i++)
    {
        plVulkanFrameContext frame = {0};
        PL_VULKAN(vkCreateSemaphore(device->logicalDevice, &semaphoreInfo, NULL, &frame.imageAvailable));
        PL_VULKAN(vkCreateSemaphore(device->logicalDevice, &semaphoreInfo, NULL, &frame.renderFinish));
        PL_VULKAN(vkCreateFence(device->logicalDevice, &fenceInfo, NULL, &frame.inFlight));
        PL_VULKAN(vkAllocateCommandBuffers(device->logicalDevice, &allocInfo, &frame.cmdBuf));  
        pl_sb_push(graphics->sbFrames, frame);
    }
}

void
pl_create_framebuffers(plVulkanDevice* device, VkRenderPass renderPass, plVulkanSwapchain* swapchain)
{
    for(uint32_t i = 0; i < swapchain->imageCount; i++)
    {
        VkImageView ptrImgViews[] = { swapchain->imageViews[i] };
        VkFramebufferCreateInfo frmbufInfo = {0};
        frmbufInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frmbufInfo.renderPass = renderPass;
        frmbufInfo.attachmentCount = 1u;
        frmbufInfo.pAttachments = ptrImgViews;
        frmbufInfo.width = swapchain->extent.width;
        frmbufInfo.height = swapchain->extent.height;
        frmbufInfo.layers = 1u;
        PL_VULKAN(vkCreateFramebuffer(device->logicalDevice, &frmbufInfo, NULL, &swapchain->frameBuffers[i]));
    }
}

void
pl_create_swapchain(plVulkanDevice* device, VkSurfaceKHR surface, uint32_t width, uint32_t height, plVulkanSwapchain* swapchainOut)
{
    vkDeviceWaitIdle(device->logicalDevice);

    //-----------------------------------------------------------------------------
    // query swapchain support
    //----------------------------------------------------------------------------- 

    VkSurfaceCapabilitiesKHR capabilities = {0};
    PL_VULKAN(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->physicalDevice, surface, &capabilities));

    uint32_t formatCount = 0u;
    PL_VULKAN(vkGetPhysicalDeviceSurfaceFormatsKHR(device->physicalDevice, surface, &formatCount, NULL));
    
    if(formatCount >swapchainOut->surfaceFormatCapacity_)
    {
        if(swapchainOut->surfaceFormats_) free(swapchainOut->surfaceFormats_);

        swapchainOut->surfaceFormats_ = (VkSurfaceFormatKHR*)malloc(sizeof(VkSurfaceFormatKHR)*formatCount);
        swapchainOut->surfaceFormatCapacity_ = formatCount;
    }

    VkBool32 presentSupport = false;
    PL_VULKAN(vkGetPhysicalDeviceSurfaceSupportKHR(device->physicalDevice, 0, surface, &presentSupport));
    PL_ASSERT(formatCount > 0);
    PL_VULKAN(vkGetPhysicalDeviceSurfaceFormatsKHR(device->physicalDevice, surface, &formatCount, swapchainOut->surfaceFormats_));

    uint32_t presentModeCount = 0u;
    PL_VULKAN(vkGetPhysicalDeviceSurfacePresentModesKHR(device->physicalDevice, surface, &presentModeCount, NULL));
    PL_ASSERT(presentModeCount > 0 && presentModeCount < 16);

    VkPresentModeKHR presentModes[16] = {0};
    PL_VULKAN(vkGetPhysicalDeviceSurfacePresentModesKHR(device->physicalDevice, surface, &presentModeCount, presentModes));

    // choose swap surface Format
    static VkFormat surfaceFormatPreference[4] = 
    {
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_B8G8R8A8_SRGB
    };

    bool preferenceFound = false;
    VkSurfaceFormatKHR surfaceFormat = swapchainOut->surfaceFormats_[0];

    for(uint32_t i = 0u; i < 4; i++)
    {
        if(preferenceFound) break;
        
        for(uint32_t j = 0u; j < formatCount; j++)
        {
            if(swapchainOut->surfaceFormats_[j].format == surfaceFormatPreference[i] && swapchainOut->surfaceFormats_[j].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                surfaceFormat = swapchainOut->surfaceFormats_[j];
                swapchainOut->format = surfaceFormat.format;
                preferenceFound = true;
                break;
            }
        }
    }
    PL_ASSERT(preferenceFound && "no preferred surface format found");

    // chose swap present mode
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    if(!swapchainOut->vsync)
    {
        for(uint32_t i = 0 ; i < presentModeCount; i++)
        {
			if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
				break;
			}
			if (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
				presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
    }

    // chose swap extent 
    VkExtent2D extent = {0};
    if(capabilities.currentExtent.width != UINT32_MAX)
        extent = capabilities.currentExtent;
    else
    {
        VkExtent2D actualExtent = { (uint32_t)width, (uint32_t)height};
        actualExtent.width = pl__get_u32_max(capabilities.minImageExtent.width, pl__get_u32_min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = pl__get_u32_max(capabilities.minImageExtent.height, pl__get_u32_min(capabilities.maxImageExtent.height, actualExtent.height));
        extent = actualExtent;
    }
    swapchainOut->extent = extent;

    // decide image count
    const uint32_t oldImageCount = swapchainOut->imageCount;
    uint32_t minImageCount = capabilities.minImageCount + 1;
    if(capabilities.maxImageCount > 0 && minImageCount > capabilities.maxImageCount) 
        minImageCount = capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = minImageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = { (uint32_t)device->graphicsQueueFamily, (uint32_t)device->presentQueueFamily};
    if (device->graphicsQueueFamily != device->presentQueueFamily)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = swapchainOut->swapChain;

    VkSwapchainKHR oldSwapChain = swapchainOut->swapChain;

    PL_VULKAN(vkCreateSwapchainKHR(device->logicalDevice, &createInfo, NULL, &swapchainOut->swapChain));

    if(oldSwapChain)
    {
        // flush resources
        for (uint32_t i = 0u; i < oldImageCount; i++)
        {
            vkDestroyImageView(device->logicalDevice, swapchainOut->imageViews[i], NULL);
            vkDestroyFramebuffer(device->logicalDevice, swapchainOut->frameBuffers[i], NULL);
        }
        vkDestroySwapchainKHR(device->logicalDevice, oldSwapChain, NULL);
    }

    vkGetSwapchainImagesKHR(device->logicalDevice, swapchainOut->swapChain, &swapchainOut->imageCount, NULL);
    if(swapchainOut->imageCount > swapchainOut->imageCapacity)
    {
        swapchainOut->imageCapacity = swapchainOut->imageCount;
        if(swapchainOut->images) free(swapchainOut->images);
        if(swapchainOut->imageViews) free(swapchainOut->imageViews);
        if(swapchainOut->frameBuffers) free(swapchainOut->frameBuffers);
        swapchainOut->images = (VkImage*)malloc(sizeof(VkImage)*swapchainOut->imageCapacity);
        swapchainOut->imageViews = (VkImageView*)malloc(sizeof(VkImageView)*swapchainOut->imageCapacity);
        swapchainOut->frameBuffers   = (VkFramebuffer*)malloc(sizeof(VkFramebuffer)*swapchainOut->imageCapacity);
    }
    vkGetSwapchainImagesKHR(device->logicalDevice, swapchainOut->swapChain, &swapchainOut->imageCount, swapchainOut->images);

    for(uint32_t i = 0; i < swapchainOut->imageCount; i++)
    {

        VkImageViewCreateInfo viewInfo = {0};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapchainOut->images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapchainOut->format;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        PL_VULKAN(vkCreateImageView(device->logicalDevice, &viewInfo, NULL, &swapchainOut->imageViews[i]));   
    }  //-V1020
}

void
pl_cleanup_graphics(plVulkanGraphics* graphics, plVulkanDevice* device)
{
    // destroy descriptor pool
    vkDestroyDescriptorPool(device->logicalDevice, graphics->descPool, NULL);    
    
    for(uint32_t i = 0; i < graphics->framesInFlight; i++)
    {
        // destroy command buffers
        vkFreeCommandBuffers(device->logicalDevice, graphics->cmdPool, 1u, &graphics->sbFrames[i].cmdBuf);

        // destroy sync primitives
        vkDestroySemaphore(device->logicalDevice, graphics->sbFrames[i].imageAvailable, NULL);
        vkDestroySemaphore(device->logicalDevice, graphics->sbFrames[i].renderFinish, NULL);
        vkDestroyFence(device->logicalDevice, graphics->sbFrames[i].inFlight, NULL);
    }

    // destroy command pool
    vkDestroyCommandPool(device->logicalDevice, graphics->cmdPool, NULL);

    if(graphics->dbgMessenger)
    {
        PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(graphics->instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != NULL)
            func(graphics->instance, graphics->dbgMessenger, NULL);
    }

    // destroy surface
    vkDestroySurfaceKHR(graphics->instance, graphics->surface, NULL);

    // destroy device
    vkDestroyDevice(device->logicalDevice, NULL);

    // destroy instance
    vkDestroyInstance(graphics->instance, NULL);
}


plVulkanFrameContext*
pl_get_frame_resources(plVulkanGraphics* graphics)
{
    return &graphics->sbFrames[graphics->currentFrameIndex];
}

uint32_t
pl_find_memory_type(VkPhysicalDeviceMemoryProperties memProps, uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    uint32_t memoryType = 0u;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) 
    {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) 
        {
            memoryType = i;
            break;
        }
    }
    return memoryType;    
}

void 
pl_transition_image_layout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageSubresourceRange subresourceRange, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask)
{
    //VkCommandBuffer commandBuffer = mvBeginSingleTimeCommands();
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = subresourceRange,
    };

    // Source layouts (old)
    // Source access mask controls actions that have to be finished on the old layout
    // before it will be transitioned to the new layout
    switch (oldLayout)
    {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        // Image layout is undefined (or does not matter)
        // Only valid as initial layout
        // No flags required, listed only for completeness
        barrier.srcAccessMask = 0;
        break;

    case VK_IMAGE_LAYOUT_PREINITIALIZED:
        // Image is preinitialized
        // Only valid as initial layout for linear images, preserves memory contents
        // Make sure host writes have been finished
        barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        // Image is a color attachment
        // Make sure any writes to the color buffer have been finished
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        // Image is a depth/stencil attachment
        // Make sure any writes to the depth/stencil buffer have been finished
        barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        // Image is a transfer source
        // Make sure any reads from the image have been finished
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        // Image is a transfer destination
        // Make sure any writes to the image have been finished
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        // Image is read by a shader
        // Make sure any shader reads from the image have been finished
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        break;
    default:
        // Other source layouts aren't handled (yet)
        break;
    }

    // Target layouts (new)
    // Destination access mask controls the dependency for the new image layout
    switch (newLayout)
    {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        // Image will be used as a transfer destination
        // Make sure any writes to the image have been finished
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        // Image will be used as a transfer source
        // Make sure any reads from the image have been finished
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        // Image will be used as a color attachment
        // Make sure any writes to the color buffer have been finished
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        // Image layout will be used as a depth/stencil attachment
        // Make sure any writes to depth/stencil buffer have been finished
        barrier.dstAccessMask = barrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        // Image will be read in a shader (sampler, input attachment)
        // Make sure any writes to the image have been finished
        if (barrier.srcAccessMask == 0)
        {
            barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        }
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        break;
    default:
        // Other source layouts aren't handled (yet)
        break;
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        srcStageMask, dstStageMask,
        0,
        0, NULL,
        0, NULL,
        1, &barrier
    );
}

//-----------------------------------------------------------------------------
// [SECTION] internal implementations
//-----------------------------------------------------------------------------

static VKAPI_ATTR VkBool32 VKAPI_CALL
pl__debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT msgSeverity, VkDebugUtilsMessageTypeFlagsEXT msgType, const VkDebugUtilsMessengerCallbackDataEXT *ptrCallbackData, void *ptrUserData) 
{
    if(msgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        printf("error validation layer: %s\n", ptrCallbackData->pMessage);
    }

    else if(msgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        printf("warn validation layer: %s\n", ptrCallbackData->pMessage);
    }

    // else if(msgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    // {
    //     printf("info validation layer: %s\n", ptrCallbackData->pMessage);
    // }
    // else if(msgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
    // {
    //     printf("trace validation layer: %s\n", ptrCallbackData->pMessage);
    // }
    
    return VK_FALSE;
}