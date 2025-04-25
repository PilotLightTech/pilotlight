// Implemented features:
//  [!] Renderer: User texture binding. Use 'VkDescriptorSet' as ImTextureID. Call ImGui_ImplVulkan_AddTexture() to register one. Read the FAQ about ImTextureID! See https://github.com/ocornut/imgui/pull/914 for discussions.
//  [X] Renderer: Large meshes support (64k+ vertices) even with 16-bit indices (ImGuiBackendFlags_RendererHasVtxOffset).
//  [X] Renderer: Expose selected render state for draw callbacks to use. Access in '(ImGui_ImplXXXX_RenderState*)GetPlatformIO().Renderer_RenderState'.
//  [x] Renderer: Multi-viewport / platform windows. With issues (flickering when creating a new viewport).

#pragma once

#include "imgui.h"

#if defined(VK_USE_PLATFORM_WIN32_KHR) && !defined(NOMINMAX)
#define NOMINMAX
#endif

// Vulkan includes
#include <vulkan/vulkan.h>

// Initialization data, for ImGui_ImplVulkan_Init()
struct ImGui_ImplVulkan_InitInfo
{
    uint32_t              ApiVersion;                 // Fill with API version of Instance, e.g. VK_API_VERSION_1_3 or your value of VkApplicationInfo::apiVersion. May be lower than header version (VK_HEADER_VERSION_COMPLETE)
    VkInstance            Instance;
    VkPhysicalDevice      PhysicalDevice;
    VkDevice              Device;
    uint32_t              QueueFamily;
    VkQueue               Queue;
    VkRenderPass          RenderPass;                 // Ignored if using dynamic rendering
    uint32_t              MinImageCount;              // >= 2
    uint32_t              ImageCount;                 // >= MinImageCount
    VkSampleCountFlagBits MSAASamples;                // 0 defaults to VK_SAMPLE_COUNT_1_BIT
    uint32_t              Subpass;
    uint32_t              DescriptorPoolSize;

    // (Optional) Allocation, Debugging
    const VkAllocationCallbacks* Allocator;
    VkDeviceSize MinAllocationSize; // Minimum allocation size. Set to 1024*1024 to satisfy zealous best practices validation layer and waste a little memory.
};

bool ImGui_ImplVulkan_Init(ImGui_ImplVulkan_InitInfo* info);
void ImGui_ImplVulkan_Shutdown();
void ImGui_ImplVulkan_NewFrame();
void ImGui_ImplVulkan_RenderDrawData(ImDrawData* draw_data, VkCommandBuffer command_buffer, VkPipeline pipeline = VK_NULL_HANDLE);
bool ImGui_ImplVulkan_CreateFontsTexture();
void ImGui_ImplVulkan_DestroyFontsTexture();
void ImGui_ImplVulkan_SetMinImageCount(uint32_t min_image_count); // To override MinImageCount after initialization (e.g. if swap chain is recreated)
