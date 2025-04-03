/*
   example_3.c
     - demonstrates loading APIs
     - demonstrates loading extensions
     - demonstrates hot reloading
     - demonstrates minimal use of graphics extension
     - demonstrates ui extension
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] apis
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
// [SECTION] full demo
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <float.h>
#include <stdlib.h> // malloc, free
#include <stdio.h>
#include <string.h> // memset
#include "pl.h"
#include "pl_ds.h"
#include "pl_memory.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extensions
#include "pl_log_ext.h"
#include "pl_window_ext.h"
#include "pl_shader_ext.h"
#include "pl_draw_ext.h"
#include "pl_ui_ext.h"
#ifdef __APPLE__
#define PL_GRAPHICS_EXPOSE_METAL
#else
#define PL_GRAPHICS_EXPOSE_VULKAN
#endif
#include "pl_graphics_ext.h"
#include "pl_draw_backend_ext.h"
#include "pl_profile_ext.h"
#include "pl_platform_ext.h"

#include "imgui.h"

#ifdef __APPLE__
#include "imgui_impl_metal.h"
#else
#include "imgui_impl_vulkan.h"
#endif

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAppData
{
    // window
    
    plWindow* ptWindow;

    // ui options
    bool bShowUiDebug;
    bool bShowUiStyle;

    // graphics & sync objects
    plDevice*                ptDevice;
    plSurface*               ptSurface;
    plSwapchain*             ptSwapchain;
    plTimelineSemaphore*     aptSemaphores[PL_MAX_FRAMES_IN_FLIGHT];
    uint64_t                 aulNextTimelineValue[PL_MAX_FRAMES_IN_FLIGHT];
    plCommandPool*           atCmdPools[PL_MAX_FRAMES_IN_FLIGHT];
    plRenderPassHandle       tMainRenderPass;
    plRenderPassLayoutHandle tMainRenderPassLayout;
    plTextureHandle          tMSAATexture;
    char*                    sbcTempBuffer;

} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

// glfw
const plIOI*             gptIO          = NULL;
const plWindowI*         gptWindow      = NULL;
const plGraphicsI*       gptGfx         = NULL;
const plDrawI*           gptDraw        = NULL;
const plUiI*             gptUi          = NULL;
const plShaderI*         gptShader      = NULL;
const plDrawBackendI*    gptDrawBackend = NULL;
const plProfileI*        gptProfile     = NULL;

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryI* ptApiRegistry, plAppData* ptAppData)
{
    // NOTE: on first load, "pAppData" will be NULL but on reloads
    //       it will be the value returned from this function

    // retrieve the data registry API, this is the API used for sharing data
    // between extensions & the runtime
    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    ImGuiContext* ptImguiContext = (ImGuiContext*)ptDataRegistry->get_data("imgui");
    ImGui::SetCurrentContext(ptImguiContext);

    // if "ptAppData" is a valid pointer, then this function is being called
    // during a hot reload.
    if(ptAppData)
    {

        // re-retrieve the apis since we are now in
        // a different dll/so
        gptIO          = pl_get_api_latest(ptApiRegistry, plIOI);
        gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);
        gptDraw        = pl_get_api_latest(ptApiRegistry, plDrawI);
        gptShader      = pl_get_api_latest(ptApiRegistry, plShaderI);
        gptDrawBackend = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
        gptUi          = pl_get_api_latest(ptApiRegistry, plUiI);
        gptProfile     = pl_get_api_latest(ptApiRegistry, plProfileI);
        gptWindow      = pl_get_api_latest(ptApiRegistry, plWindowI);

        return ptAppData;
    }

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = (plAppData*)malloc(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    // retrieve extension registry
    const plExtensionRegistryI* ptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);

    // load extensions
    ptExtensionRegistry->load("pl_unity_ext", NULL, NULL, true);
    ptExtensionRegistry->load("pl_platform_ext", NULL, NULL, false);
    
    // load required apis
    gptIO          = pl_get_api_latest(ptApiRegistry, plIOI);
    gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptDraw        = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptShader      = pl_get_api_latest(ptApiRegistry, plShaderI);
    gptDrawBackend = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
    gptUi          = pl_get_api_latest(ptApiRegistry, plUiI);
    gptProfile     = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptWindow      = pl_get_api_latest(ptApiRegistry, plWindowI);

    // initialize shader compiler
    static plShaderOptions tDefaultShaderOptions = {};
    tDefaultShaderOptions.apcIncludeDirectories[0] = "../shaders/";
    tDefaultShaderOptions.apcDirectories[0] = "../shaders/";
    tDefaultShaderOptions.tFlags = PL_SHADER_FLAGS_AUTO_OUTPUT;
    gptShader->initialize(&tDefaultShaderOptions);

    // use window API to create a window
    plWindowDesc tWindowDesc = {
        "Example 3",
        500,
        500,
        200,
        200
    };
    gptWindow->create_window(tWindowDesc, &ptAppData->ptWindow);

    // initialize graphics system
    const plGraphicsInit tGraphicsInit = {
        2,
        PL_GRAPHICS_INIT_FLAGS_VALIDATION_ENABLED | PL_GRAPHICS_INIT_FLAGS_SWAPCHAIN_ENABLED 
    };
    gptGfx->initialize(&tGraphicsInit);
    ptAppData->ptSurface = gptGfx->create_surface(ptAppData->ptWindow);

    // find suitable device
    uint32_t uDeviceCount = 16;
    plDeviceInfo atDeviceInfos[16] = {0};
    gptGfx->enumerate_devices(atDeviceInfos, &uDeviceCount);

    // we will prefer discrete, then integrated
    int iBestDvcIdx = 0;
    int iDiscreteGPUIdx   = -1;
    int iIntegratedGPUIdx = -1;
    for(uint32_t i = 0; i < uDeviceCount; i++)
    {
        
        if(atDeviceInfos[i].tType == PL_DEVICE_TYPE_DISCRETE)
            iDiscreteGPUIdx = i;
        else if(atDeviceInfos[i].tType == PL_DEVICE_TYPE_INTEGRATED)
            iIntegratedGPUIdx = i;
    }

    if(iDiscreteGPUIdx > -1)
        iBestDvcIdx = iDiscreteGPUIdx;
    else if(iIntegratedGPUIdx > -1)
        iBestDvcIdx = iIntegratedGPUIdx;

    // create device
    plDeviceInit tDeviceInit = {};
    tDeviceInit.uDeviceIdx = iBestDvcIdx;
    tDeviceInit.ptSurface = ptAppData->ptSurface;
    ptAppData->ptDevice = gptGfx->create_device(&tDeviceInit);

    // create command pools
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        ptAppData->atCmdPools[i] = gptGfx->create_command_pool(ptAppData->ptDevice, NULL);

    // create swapchain
    plSwapchainInit tSwapInit = {};
    tSwapInit.tSampleCount = atDeviceInfos[iBestDvcIdx].tMaxSampleCount;
    ptAppData->ptSwapchain = gptGfx->create_swapchain(ptAppData->ptDevice, ptAppData->ptSurface, &tSwapInit);

    uint32_t uImageCount = 0;
    plTextureHandle* atSwapchainImages = gptGfx->get_swapchain_images(ptAppData->ptSwapchain, &uImageCount);

    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptAppData->atCmdPools[0]);
    gptGfx->begin_command_recording(ptCommandBuffer, NULL);

    // begin blit pass, copy buffer, end pass
    plBlitEncoder* ptEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
    gptGfx->pipeline_barrier_blit(ptEncoder, PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE | PL_SHADER_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_SHADER_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);

    plSwapchainInfo tInfo = gptGfx->get_swapchain_info(ptAppData->ptSwapchain);
    plTextureDesc tColorTextureDesc = {};
    tColorTextureDesc.tDimensions   = {(float)tInfo.uWidth, (float)tInfo.uHeight, 1};
    tColorTextureDesc.tFormat       = gptGfx->get_swapchain_info(ptAppData->ptSwapchain).tFormat;
    tColorTextureDesc.uLayers       = 1;
    tColorTextureDesc.uMips         = 1;
    tColorTextureDesc.tType         = PL_TEXTURE_TYPE_2D;
    tColorTextureDesc.tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT;
    tColorTextureDesc.pcDebugName   = "offscreen color texture";
    tColorTextureDesc.tSampleCount  = gptGfx->get_swapchain_info(ptAppData->ptSwapchain).tSampleCount;

    // create textures
    ptAppData->tMSAATexture = gptGfx->create_texture(ptAppData->ptDevice, &tColorTextureDesc, NULL);

    // retrieve textures
    plTexture* ptColorTexture = gptGfx->get_texture(ptAppData->ptDevice, ptAppData->tMSAATexture);

    // allocate memory
    const plDeviceMemoryAllocation tColorAllocation = gptGfx->allocate_memory(ptAppData->ptDevice, 
        ptColorTexture->tMemoryRequirements.ulSize,
        PL_MEMORY_GPU,
        ptColorTexture->tMemoryRequirements.uMemoryTypeBits,
        "color texture memory");

    // bind memory
    gptGfx->bind_texture_to_memory(ptAppData->ptDevice, ptAppData->tMSAATexture, &tColorAllocation);

    // set initial usage
    gptGfx->set_texture_usage(ptEncoder, ptAppData->tMSAATexture, PL_TEXTURE_USAGE_COLOR_ATTACHMENT, 0);

    gptGfx->pipeline_barrier_blit(ptEncoder, PL_SHADER_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE | PL_SHADER_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
    gptGfx->end_blit_pass(ptEncoder);

    // finish recording
    gptGfx->end_command_recording(ptCommandBuffer);

    // submit command buffer
    gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);
    gptGfx->return_command_buffer(ptCommandBuffer);

    // create main render pass layout
    plRenderPassLayoutDesc tMainRenderPassLayoutDesc = {};
    tMainRenderPassLayoutDesc.atRenderTargets[0].tFormat = gptGfx->get_swapchain_info(ptAppData->ptSwapchain).tFormat;
    tMainRenderPassLayoutDesc.atRenderTargets[0].bResolve = true;

    tMainRenderPassLayoutDesc.atRenderTargets[1].tFormat = gptGfx->get_swapchain_info(ptAppData->ptSwapchain).tFormat;
    tMainRenderPassLayoutDesc.atRenderTargets[1].tSamples = gptGfx->get_swapchain_info(ptAppData->ptSwapchain).tSampleCount;

    tMainRenderPassLayoutDesc.atSubpasses[0].uRenderTargetCount = 2;
    tMainRenderPassLayoutDesc.atSubpasses[0].auRenderTargets[0] = 0;
    tMainRenderPassLayoutDesc.atSubpasses[0].auRenderTargets[1] = 1;


    tMainRenderPassLayoutDesc.atSubpassDependencies[0].uSourceSubpass = UINT32_MAX;
    tMainRenderPassLayoutDesc.atSubpassDependencies[0].uDestinationSubpass = 0;
    tMainRenderPassLayoutDesc.atSubpassDependencies[0].tSourceStageMask = PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS | PL_PIPELINE_STAGE_COMPUTE_SHADER;
    tMainRenderPassLayoutDesc.atSubpassDependencies[0].tDestinationStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS;
    tMainRenderPassLayoutDesc.atSubpassDependencies[0].tSourceAccessMask = PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ;
    tMainRenderPassLayoutDesc.atSubpassDependencies[0].tDestinationAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ;

    tMainRenderPassLayoutDesc.atSubpassDependencies[1].uSourceSubpass = 0;
    tMainRenderPassLayoutDesc.atSubpassDependencies[1].uDestinationSubpass = UINT32_MAX;
    tMainRenderPassLayoutDesc.atSubpassDependencies[1].tSourceStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS;
    tMainRenderPassLayoutDesc.atSubpassDependencies[1].tDestinationStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS | PL_PIPELINE_STAGE_COMPUTE_SHADER;
    tMainRenderPassLayoutDesc.atSubpassDependencies[1].tSourceAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ;
    tMainRenderPassLayoutDesc.atSubpassDependencies[1].tDestinationAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ;

    ptAppData->tMainRenderPassLayout = gptGfx->create_render_pass_layout(ptAppData->ptDevice, &tMainRenderPassLayoutDesc);

    // create main render pass
    plRenderPassDesc tMainRenderPassDesc = {};
    tMainRenderPassDesc.tLayout = ptAppData->tMainRenderPassLayout;
    tMainRenderPassDesc.tResolveTarget = {}; // swapchain image
    tMainRenderPassDesc.tResolveTarget.tLoadOp       = PL_LOAD_OP_DONT_CARE;
    tMainRenderPassDesc.tResolveTarget.tStoreOp      = PL_STORE_OP_STORE;
    tMainRenderPassDesc.tResolveTarget.tCurrentUsage = PL_TEXTURE_USAGE_UNSPECIFIED;
    tMainRenderPassDesc.tResolveTarget.tNextUsage    = PL_TEXTURE_USAGE_PRESENT;
    tMainRenderPassDesc.tResolveTarget.tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f};

    tMainRenderPassDesc.atColorTargets[0].tLoadOp       = PL_LOAD_OP_CLEAR;
    tMainRenderPassDesc.atColorTargets[0].tStoreOp      = PL_STORE_OP_STORE_MULTISAMPLE_RESOLVE;
    tMainRenderPassDesc.atColorTargets[0].tCurrentUsage = PL_TEXTURE_USAGE_COLOR_ATTACHMENT;
    tMainRenderPassDesc.atColorTargets[0].tNextUsage    = PL_TEXTURE_USAGE_COLOR_ATTACHMENT;
    tMainRenderPassDesc.atColorTargets[0].tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f};
    tMainRenderPassDesc.tDimensions = {(float)tInfo.uWidth, (float)tInfo.uHeight};
    tMainRenderPassDesc.ptSwapchain = ptAppData->ptSwapchain;

    plRenderPassAttachments atMainAttachmentSets[16] = {0};
    for(uint32_t i = 0; i < uImageCount; i++)
    {
        atMainAttachmentSets[i].atViewAttachments[0] = atSwapchainImages[i];
        atMainAttachmentSets[i].atViewAttachments[1] = ptAppData->tMSAATexture;
    }
    ptAppData->tMainRenderPass = gptGfx->create_render_pass(ptAppData->ptDevice, &tMainRenderPassDesc, atMainAttachmentSets);

    // setup draw
    gptDraw->initialize(NULL);
    gptDrawBackend->initialize(ptAppData->ptDevice);
    plFontAtlas* ptAtlas = gptDraw->create_font_atlas();
    plFont* ptDefaultFont = gptDraw->add_default_font(ptAtlas);
    plCommandPool* ptCmdPool = ptAppData->atCmdPools[gptGfx->get_current_frame_index()];
    plCommandBuffer* ptCmdBuffer = gptGfx->request_command_buffer(ptCmdPool);
    gptDrawBackend->build_font_atlas(ptCmdBuffer, ptAtlas);
    gptGfx->return_command_buffer(ptCmdBuffer);
    gptDraw->set_font_atlas(ptAtlas);

    // setup ui
    gptUi->initialize();
    gptUi->set_default_font(ptDefaultFont);

    // create timeline semaphores to syncronize GPU work submission
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        ptAppData->aptSemaphores[i] = gptGfx->create_semaphore(ptAppData->ptDevice, false);

    #ifdef __APPLE__
    id<MTLDevice> tDevice = gptGfx->get_metal_device(ptAppData->ptDevice);
    ImGui_ImplMetal_Init(tDevice);
    #else
    ImGui_ImplVulkan_InitInfo tImguiVulkanInfo;
    memset(&tImguiVulkanInfo, 0, sizeof(ImGui_ImplVulkan_InitInfo));
    tImguiVulkanInfo.ApiVersion = gptGfx->get_vulkan_api_version();
    tImguiVulkanInfo.Instance = gptGfx->get_vulkan_instance();
    tImguiVulkanInfo.PhysicalDevice = gptGfx->get_vulkan_physical_device(ptAppData->ptDevice);
    tImguiVulkanInfo.Device = gptGfx->get_vulkan_device(ptAppData->ptDevice);
    tImguiVulkanInfo.QueueFamily = gptGfx->get_vulkan_queue_family(ptAppData->ptDevice);
    tImguiVulkanInfo.Queue = gptGfx->get_vulkan_queue(ptAppData->ptDevice);
    tImguiVulkanInfo.DescriptorPool = gptGfx->get_vulkan_descriptor_pool(gptDrawBackend->get_bind_group_pool());
    tImguiVulkanInfo.MinImageCount = 2;
    tImguiVulkanInfo.MSAASamples = (VkSampleCountFlagBits)gptGfx->get_swapchain_info(ptAppData->ptSwapchain).tSampleCount;
    tImguiVulkanInfo.RenderPass = gptGfx->get_vulkan_render_pass(ptAppData->ptDevice, ptAppData->tMainRenderPass);
    gptGfx->get_swapchain_images(ptAppData->ptSwapchain, &tImguiVulkanInfo.ImageCount);
    ImGui_ImplVulkan_Init(&tImguiVulkanInfo);
    #endif

    // return app memory
    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    // ensure GPU is finished before cleanup
    gptGfx->flush_device(ptAppData->ptDevice);

#ifdef __APPLE__
    ImGui_ImplMetal_Shutdown();
#else
    ImGui_ImplVulkan_Shutdown();
#endif

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        gptGfx->cleanup_command_pool(ptAppData->atCmdPools[i]);
        gptGfx->cleanup_semaphore(ptAppData->aptSemaphores[i]);
    }
    pl_sb_free(ptAppData->sbcTempBuffer);
    gptDrawBackend->cleanup_font_atlas(NULL);
    gptUi->cleanup();
    gptDrawBackend->cleanup();
    gptGfx->destroy_texture(ptAppData->ptDevice, ptAppData->tMSAATexture);
    gptGfx->cleanup_swapchain(ptAppData->ptSwapchain);
    gptGfx->cleanup_surface(ptAppData->ptSurface);
    gptGfx->cleanup_device(ptAppData->ptDevice);
    gptGfx->cleanup();
    free(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plAppData* ptAppData)
{
    // perform any operations required during a window resize
    plIO* ptIO = gptIO->get_io();
    plSwapchainInit tDesc = {
        true,
        (uint32_t)ptIO->tMainViewportSize.x,
        (uint32_t)ptIO->tMainViewportSize.y,
        gptGfx->get_swapchain_info(ptAppData->ptSwapchain).tSampleCount,
    };
    gptGfx->recreate_swapchain(ptAppData->ptSwapchain, &tDesc);
    uint32_t uImageCount = 0;
    plTextureHandle* atSwapchainImages = gptGfx->get_swapchain_images(ptAppData->ptSwapchain, &uImageCount);

    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptAppData->atCmdPools[0]);
    gptGfx->begin_command_recording(ptCommandBuffer, NULL);

    // begin blit pass, copy buffer, end pass
    plBlitEncoder* ptEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
    gptGfx->pipeline_barrier_blit(ptEncoder, PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE | PL_SHADER_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_SHADER_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);

    plSwapchainInfo tInfo = gptGfx->get_swapchain_info(ptAppData->ptSwapchain);
    plTextureDesc tColorTextureDesc = {};
    tColorTextureDesc.tDimensions   = {(float)tInfo.uWidth, (float)tInfo.uHeight, 1};
    tColorTextureDesc.tFormat       = tInfo.tFormat;
    tColorTextureDesc.uLayers       = 1;
    tColorTextureDesc.uMips         = 1;
    tColorTextureDesc.tType         = PL_TEXTURE_TYPE_2D;
    tColorTextureDesc.tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT;
    tColorTextureDesc.pcDebugName   = "offscreen color texture";
    tColorTextureDesc.tSampleCount  = tInfo.tSampleCount;

    gptGfx->queue_texture_for_deletion(ptAppData->ptDevice, ptAppData->tMSAATexture);

    // create textures
    ptAppData->tMSAATexture = gptGfx->create_texture(ptAppData->ptDevice, &tColorTextureDesc, NULL);

    // retrieve textures
    plTexture* ptColorTexture = gptGfx->get_texture(ptAppData->ptDevice, ptAppData->tMSAATexture);

    // allocate memory

    const plDeviceMemoryAllocation tColorAllocation = gptGfx->allocate_memory(ptAppData->ptDevice, 
        ptColorTexture->tMemoryRequirements.ulSize,
        PL_MEMORY_GPU,
        ptColorTexture->tMemoryRequirements.uMemoryTypeBits,
        "color texture memory");

    // bind memory
    gptGfx->bind_texture_to_memory(ptAppData->ptDevice, ptAppData->tMSAATexture, &tColorAllocation);

    // set initial usage
    gptGfx->set_texture_usage(ptEncoder, ptAppData->tMSAATexture, PL_TEXTURE_USAGE_COLOR_ATTACHMENT, 0);

    gptGfx->pipeline_barrier_blit(ptEncoder, PL_SHADER_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE | PL_SHADER_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
    gptGfx->end_blit_pass(ptEncoder);

    // finish recording
    gptGfx->end_command_recording(ptCommandBuffer);

    // submit command buffer
    gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);
    gptGfx->return_command_buffer(ptCommandBuffer);

    plRenderPassAttachments atMainAttachmentSets[16] = {0};
    for(uint32_t i = 0; i < uImageCount; i++)
    {
        atMainAttachmentSets[i].atViewAttachments[0] = atSwapchainImages[i];
        atMainAttachmentSets[i].atViewAttachments[1] = ptAppData->tMSAATexture;
    }
    gptGfx->update_render_pass_attachments(ptAppData->ptDevice, ptAppData->tMainRenderPass, gptIO->get_io()->tMainViewportSize, atMainAttachmentSets);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    gptProfile->begin_frame();

    gptIO->new_frame();
    gptDrawBackend->new_frame();
    gptUi->new_frame();

#ifdef __APPLE__
    ImGui_ImplMetal_NewFrame(gptGfx->get_metal_render_pass_descriptor(ptAppData->ptDevice, ptAppData->tMainRenderPass));
#else
    ImGui_ImplVulkan_NewFrame();
#endif
    ImGui::NewFrame();

    // begin new frame
    gptGfx->begin_frame(ptAppData->ptDevice);
    plCommandPool* ptCmdPool = ptAppData->atCmdPools[gptGfx->get_current_frame_index()];
    gptGfx->reset_command_pool(ptCmdPool, 0);

    // acquire swapchain image

    if(!gptGfx->acquire_swapchain_image(ptAppData->ptSwapchain))
    {
        pl_app_resize(ptAppData);
        gptProfile->end_frame();
        return;
    }

    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);

    // NOTE: UI code can be placed anywhere between the UI "new_frame" & "render"

    ImGui::ShowDemoWindow();

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~UI~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    if(gptUi->begin_window("Pilot Light", NULL, false))
    {

        const float pfRatios[] = {1.0f};
        gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
        if(gptUi->begin_collapsing_header("Information", 0))
        {
            
            gptUi->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
            gptUi->text("Graphics Backend: %s", gptGfx->get_backend_string());
            gptUi->end_collapsing_header();
        }
        if(gptUi->begin_collapsing_header("User Interface", 0))
        {
            gptUi->checkbox("UI Debug", &ptAppData->bShowUiDebug);
            gptUi->checkbox("UI Style", &ptAppData->bShowUiStyle);
            gptUi->end_collapsing_header();
        }
        gptUi->end_window();
    }
        
    if(ptAppData->bShowUiStyle)
        gptUi->show_style_editor_window(&ptAppData->bShowUiStyle);

    if(ptAppData->bShowUiDebug)
        gptUi->show_debug_window(&ptAppData->bShowUiDebug);

    //~~~~~~~~~~~~~~~~~~~~~~~~begin recording command buffer~~~~~~~~~~~~~~~~~~~~~~~

    // expected timeline semaphore values
    const uint32_t uCurrentFrameIndex = gptGfx->get_current_frame_index();
    uint64_t ulValue0 = ptAppData->aulNextTimelineValue[uCurrentFrameIndex];
    uint64_t ulValue1 = ulValue0 + 1;
    ptAppData->aulNextTimelineValue[uCurrentFrameIndex] = ulValue1;

    plBeginCommandInfo tBeginInfo = {};
    tBeginInfo.uWaitSemaphoreCount   = 1;
    tBeginInfo.atWaitSempahores[0]      = ptAppData->aptSemaphores[uCurrentFrameIndex];
    tBeginInfo.auWaitSemaphoreValues[0] = ulValue0;
    gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo);

    // begin main renderpass (directly to swapchain)
    plRenderEncoder* ptEncoder = gptGfx->begin_render_pass(ptCommandBuffer, ptAppData->tMainRenderPass, NULL);

    // submits UI drawlist/layers
    plIO* ptIO = gptIO->get_io();
    gptUi->end_frame();
    gptDrawBackend->submit_2d_drawlist(gptUi->get_draw_list(), ptEncoder, ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y, gptGfx->get_swapchain_info(ptAppData->ptSwapchain).tSampleCount);
    gptDrawBackend->submit_2d_drawlist(gptUi->get_debug_draw_list(), ptEncoder, ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y, gptGfx->get_swapchain_info(ptAppData->ptSwapchain).tSampleCount);

    ImGui::Render();
    ImDrawData* main_draw_data = ImGui::GetDrawData();

    #ifdef __APPLE__
    ImGui_ImplMetal_RenderDrawData(main_draw_data, gptGfx->get_metal_command_buffer(ptCommandBuffer), gptGfx->get_metal_command_encoder(ptEncoder));
    #else
    ImGui_ImplVulkan_RenderDrawData(main_draw_data, gptGfx->get_vulkan_command_buffer(ptCommandBuffer));
    #endif

    // Update and Render additional Platform Windows
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    // end render pass
    gptGfx->end_render_pass(ptEncoder);

    // end recording
    gptGfx->end_command_recording(ptCommandBuffer);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~submit work to GPU & present~~~~~~~~~~~~~~~~~~~~~~~

    plSubmitInfo tSubmitInfo = {};
    tSubmitInfo.uSignalSemaphoreCount   = 1;
    tSubmitInfo.atSignalSempahores[0]      = ptAppData->aptSemaphores[uCurrentFrameIndex];
    tSubmitInfo.auSignalSemaphoreValues[0] = ulValue1;

    if(!gptGfx->present(ptCommandBuffer, &tSubmitInfo, &ptAppData->ptSwapchain, 1))
        pl_app_resize(ptAppData);

    gptGfx->return_command_buffer(ptCommandBuffer);
    gptProfile->end_frame();
}
