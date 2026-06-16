/*
   pl_dear_imgui_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] enums
// [SECTION] internal structs
// [SECTION] global data
// [SECTION] public api implementation
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.h"
#include "pl_dear_imgui_ext.h"

#include "imgui.h"
#include "implot.h"

#ifdef PL_CPU_BACKEND
#define PL_GRAPHICS_EXPOSE_CPU
#elif defined(PL_VULKAN_BACKEND)
#define PL_GRAPHICS_EXPOSE_VULKAN
#include "imgui_impl_vulkan.h"
#elif defined(PL_METAL_BACKEND)
#define PL_GRAPHICS_EXPOSE_METAL
#include "imgui_impl_metal.h"
#else
#endif

#include "pl_graphics_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

static const plGraphicsI *gptGfx = NULL;
static const plDataRegistryI *gptDataRegistry = NULL;
static const plMemoryI *gptMemory = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_dear_imgui_initialize(plDevice *ptDevice, plSwapchain *ptSwap, const plRenderAttachmentInfo* ptInfo)
{

    ImPlotContext *ptImPlotContext = ImPlot::CreateContext();
    gptDataRegistry->set_data("implot", ptImPlotContext);

#ifdef PL_CPU_BACKEND
#elif defined(PL_VULKAN_BACKEND)

    VkFormat atColorFormats[PL_MAX_RENDER_TARGETS] = {};
    uint32_t uColorCount = 0;
    for(uint32_t i = 0; i < PL_MAX_RENDER_TARGETS; i++)
    {
        if(ptInfo->aeColorFormats[i] == 0)
            break;
        atColorFormats[i] = gptGfx->get_vulkan_format(ptInfo->aeColorFormats[i]);
        uColorCount++;
    }
    VkPipelineRenderingCreateInfo tPipelineRenderingCreateInfo = {};
    tPipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    tPipelineRenderingCreateInfo.pNext = NULL;
    tPipelineRenderingCreateInfo.colorAttachmentCount = uColorCount;
    tPipelineRenderingCreateInfo.pColorAttachmentFormats = atColorFormats;
    tPipelineRenderingCreateInfo.depthAttachmentFormat = ptInfo->eDepthFormat == 0 ? VK_FORMAT_UNDEFINED : gptGfx->get_vulkan_format(ptInfo->eDepthFormat);
    tPipelineRenderingCreateInfo.stencilAttachmentFormat = ptInfo->eStencilFormat == 0 ? VK_FORMAT_UNDEFINED : gptGfx->get_vulkan_format(ptInfo->eStencilFormat);

    ImGui_ImplVulkan_InitInfo tImguiVulkanInfo = PL_ZERO_INIT;
    tImguiVulkanInfo.ApiVersion = gptGfx->get_vulkan_api_version();
    tImguiVulkanInfo.Instance = gptGfx->get_vulkan_instance();
    tImguiVulkanInfo.PhysicalDevice = gptGfx->get_vulkan_physical_device(ptDevice);
    tImguiVulkanInfo.Device = gptGfx->get_vulkan_device(ptDevice);
    tImguiVulkanInfo.QueueFamily = gptGfx->get_vulkan_queue_family(ptDevice);
    tImguiVulkanInfo.Queue = gptGfx->get_vulkan_queue(ptDevice);
    // tImguiVulkanInfo.DescriptorPool = gptGfx->get_vulkan_descriptor_pool(gptDraw->get_bind_group_pool());
    tImguiVulkanInfo.DescriptorPoolSize = 100000;
    tImguiVulkanInfo.MinImageCount = 2;
    tImguiVulkanInfo.UseDynamicRendering = true;
    tImguiVulkanInfo.Allocator = gptGfx->get_vulkan_allocation_callbacks();
    tImguiVulkanInfo.PipelineInfoMain.MSAASamples = (VkSampleCountFlagBits)gptGfx->get_swapchain_info(ptSwap).eSampleCount;
    tImguiVulkanInfo.PipelineInfoMain.PipelineRenderingCreateInfo = tPipelineRenderingCreateInfo;
    tImguiVulkanInfo.PipelineInfoForViewports.PipelineRenderingCreateInfo = tPipelineRenderingCreateInfo;
    // tImguiVulkanInfo.PipelineInfoMain.RenderPass = gptGfx->get_vulkan_render_pass(ptDevice, tMainRenderPass);
    tImguiVulkanInfo.PipelineInfoForViewports.MSAASamples = (VkSampleCountFlagBits)gptGfx->get_swapchain_info(ptSwap).eSampleCount;
    // tImguiVulkanInfo.PipelineInfoForViewports.RenderPass = gptGfx->get_vulkan_render_pass(ptDevice, tMainRenderPass);
    gptGfx->get_swapchain_images(ptSwap, &tImguiVulkanInfo.ImageCount);
    ImGui_ImplVulkan_Init(&tImguiVulkanInfo);
#elif defined(PL_METAL_BACKEND)
    id<MTLDevice> tDevice = gptGfx->get_metal_device(ptDevice);
    ImGui_ImplMetal_Init(tDevice);
#else
#endif
}

void
pl_dear_imgui_cleanup(void)
{
#ifdef PL_CPU_BACKEND
#elif defined(PL_VULKAN_BACKEND)
    ImGui_ImplVulkan_Shutdown();
#elif defined(PL_METAL_BACKEND)
    ImGui_ImplMetal_Shutdown();
#else
#endif

    ImPlot::DestroyContext();
}

void
pl_dear_imgui_new_frame(plDevice *ptDevice)
{
#ifdef PL_CPU_BACKEND
#elif defined(PL_VULKAN_BACKEND)
    ImGui_ImplVulkan_NewFrame();

#endif

    ImGui::NewFrame();
}

void
pl_dear_imgui_render(plCommandBuffer *ptCommandBuffer)
{
    #ifdef PL_METAL_BACKEND
        ImGui_ImplMetal_NewFrame(gptGfx->get_metal_render_pass_descriptor(ptCommandBuffer));
    #endif

    ImGui::Render();
    ImDrawData *main_draw_data = ImGui::GetDrawData();

    #ifdef PL_CPU_BACKEND
    #elif defined(PL_VULKAN_BACKEND)
        ImGui_ImplVulkan_RenderDrawData(main_draw_data, gptGfx->get_vulkan_command_buffer(ptCommandBuffer));
    #elif defined(PL_METAL_BACKEND)
        ImGui_ImplMetal_RenderDrawData(main_draw_data, gptGfx->get_metal_command_buffer(ptCommandBuffer), gptGfx->get_metal_command_encoder(ptCommandBuffer));
    #else
    #endif

    // Update and Render additional Platform Windows
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

ImTextureID
pl_dear_imgui_get_texture_id_from_bindgroup(plDevice *ptDevice, plBindGroupHandle tHandle)
{
#if defined(PL_VULKAN_BACKEND)
    return (ImTextureID)gptGfx->get_vulkan_descriptor_set(ptDevice, tHandle);
#elif defined(PL_METAL_BACKEND)
    plTextureHandle tTexture = gptGfx->get_metal_bind_group_texture(ptDevice, tHandle);
    return (ImTextureID)gptGfx->get_metal_texture(ptDevice, tTexture);
#else
#endif
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_dear_imgui_ext(const plApiRegistryI *ptApiRegistry, bool bReload)
{
    plDearImGuiI tApi = PL_ZERO_INIT;
    tApi.initialize                    = pl_dear_imgui_initialize;
    tApi.cleanup                       = pl_dear_imgui_cleanup;
    tApi.new_frame                     = pl_dear_imgui_new_frame;
    tApi.render                        = pl_dear_imgui_render;
    tApi.get_texture_id_from_bindgroup = pl_dear_imgui_get_texture_id_from_bindgroup;
    pl_set_api(ptApiRegistry, plDearImGuiI, &tApi);

    gptGfx = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);
    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);

    ImGuiContext *ptImguiContext = (ImGuiContext*)gptDataRegistry->get_data("imgui");
    ImGui::SetCurrentContext(ptImguiContext);

    ImGuiMemAllocFunc p_alloc_func = (ImGuiMemAllocFunc)gptDataRegistry->get_data("imgui allocate");
    ImGuiMemFreeFunc p_free_func = (ImGuiMemFreeFunc)gptDataRegistry->get_data("imgui free");
    ImGui::SetAllocatorFunctions(p_alloc_func, p_free_func, nullptr);

    if (bReload)
    {
        ImPlotContext *ptImPlotContext = (ImPlotContext *)gptDataRegistry->get_data("implot");
        ImPlot::SetCurrentContext(ptImPlotContext);
    }
}

void
pl_unload_dear_imgui_ext(const plApiRegistryI *ptApiRegistry, bool bReload)
{

    if (bReload)
        return;

    const plDearImGuiI *ptApi = pl_get_api_latest(ptApiRegistry, plDearImGuiI);
    ptApiRegistry->remove_api(ptApi);
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#ifdef PL_CPU_BACKEND
#elif defined(PL_VULKAN_BACKEND)
#include "imgui_impl_vulkan.cpp"
#elif defined(PL_METAL_BACKEND)
#include "imgui_impl_metal.mm"
#else
#endif