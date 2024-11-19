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
#include "pl_graphics_ext.h"
#include "pl_draw_backend_ext.h"
#include "pl_profile_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAppData
{
    // window
    plWindow* ptWindow;

    // ui options
    bool bShowUiDemo;
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

const plIOI*          gptIO          = NULL;
const plWindowI*      gptWindows     = NULL;
const plGraphicsI*    gptGfx         = NULL;
const plDrawI*        gptDraw        = NULL;
const plUiI*          gptUi          = NULL;
const plShaderI*      gptShader      = NULL;
const plDrawBackendI* gptDrawBackend = NULL;
const plProfileI*     gptProfile     = NULL;

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------


void pl_show_ui_demo_window(plAppData* ptAppData);

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

    // if "ptAppData" is a valid pointer, then this function is being called
    // during a hot reload.
    if(ptAppData)
    {

        // re-retrieve the apis since we are now in
        // a different dll/so
        gptIO          = pl_get_api_latest(ptApiRegistry, plIOI);
        gptWindows     = pl_get_api_latest(ptApiRegistry, plWindowI);
        gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);
        gptDraw        = pl_get_api_latest(ptApiRegistry, plDrawI);
        gptShader      = pl_get_api_latest(ptApiRegistry, plShaderI);
        gptDrawBackend = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
        gptUi          = pl_get_api_latest(ptApiRegistry, plUiI);
        gptProfile     = pl_get_api_latest(ptApiRegistry, plProfileI);

        return ptAppData;
    }

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = malloc(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    // retrieve extension registry
    const plExtensionRegistryI* ptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);

    // load extensions
    ptExtensionRegistry->load("pl_unity_ext", NULL, NULL, true);
    
    // load required apis (NULL if not available)
    gptIO          = pl_get_api_latest(ptApiRegistry, plIOI);
    gptWindows     = pl_get_api_latest(ptApiRegistry, plWindowI);
    gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptDraw        = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptShader      = pl_get_api_latest(ptApiRegistry, plShaderI);
    gptDrawBackend = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
    gptUi          = pl_get_api_latest(ptApiRegistry, plUiI);
    gptProfile     = pl_get_api_latest(ptApiRegistry, plProfileI);

    // initialize shader compiler
    static const plShaderOptions tDefaultShaderOptions = {
        .uIncludeDirectoriesCount = 1,
        .apcIncludeDirectories = {
            "../shaders/"
        }
    };
    gptShader->initialize(&tDefaultShaderOptions);

    // use window API to create a window
    plWindowDesc tWindowDesc = {
        .pcTitle = "Example 3",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 500,
        .uHeight = 500,
    };
    gptWindows->create_window(tWindowDesc, &ptAppData->ptWindow);

    // initialize graphics system
    const plGraphicsInit tGraphicsInit = {
        .tFlags = PL_GRAPHICS_INIT_FLAGS_VALIDATION_ENABLED | PL_GRAPHICS_INIT_FLAGS_SWAPCHAIN_ENABLED 
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
    const plDeviceInit tDeviceInit = {
        .uDeviceIdx = iBestDvcIdx,
        .ptSurface = ptAppData->ptSurface
    };
    ptAppData->ptDevice = gptGfx->create_device(&tDeviceInit);

    // create command pools
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        ptAppData->atCmdPools[i] = gptGfx->create_command_pool(ptAppData->ptDevice, NULL);

    // create swapchain
    plSwapchainInit tSwapInit = {
        .tSampleCount = atDeviceInfos[iBestDvcIdx].tMaxSampleCount
    };
    ptAppData->ptSwapchain = gptGfx->create_swapchain(ptAppData->ptDevice, ptAppData->ptSurface, &tSwapInit);

    uint32_t uImageCount = 0;
    plTextureHandle* atSwapchainImages = gptGfx->get_swapchain_images(ptAppData->ptSwapchain, &uImageCount);

    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptAppData->atCmdPools[0]);
    gptGfx->begin_command_recording(ptCommandBuffer, NULL);

    // begin blit pass, copy buffer, end pass
    plBlitEncoder* ptEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
    plSwapchainInfo tInfo = gptGfx->get_swapchain_info(ptAppData->ptSwapchain);
    const plTextureDesc tColorTextureDesc = {
        .tDimensions   = {(float)tInfo.uWidth, (float)tInfo.uHeight, 1},
        .tFormat       = gptGfx->get_swapchain_info(ptAppData->ptSwapchain).tFormat,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
        .pcDebugName   = "offscreen color texture",
        .tSampleCount  = gptGfx->get_swapchain_info(ptAppData->ptSwapchain).tSampleCount
    };

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

    gptGfx->end_blit_pass(ptEncoder);

    // finish recording
    gptGfx->end_command_recording(ptCommandBuffer);

    // submit command buffer
    gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);
    gptGfx->return_command_buffer(ptCommandBuffer);

    // create main render pass layout
    const plRenderPassLayoutDesc tMainRenderPassLayoutDesc = {
        .atRenderTargets = {
            { .tFormat = gptGfx->get_swapchain_info(ptAppData->ptSwapchain).tFormat, .bResolve = true }, // swapchain
            { .tFormat = gptGfx->get_swapchain_info(ptAppData->ptSwapchain).tFormat, .tSamples = gptGfx->get_swapchain_info(ptAppData->ptSwapchain).tSampleCount}, // msaa
        },
        .atSubpasses = {
            {
                .uRenderTargetCount = 2,
                .auRenderTargets = {0, 1}
            }
        }
    };
    ptAppData->tMainRenderPassLayout = gptGfx->create_render_pass_layout(ptAppData->ptDevice, &tMainRenderPassLayoutDesc);

    // create main render pass
    const plRenderPassDesc tMainRenderPassDesc = {
        .tLayout = ptAppData->tMainRenderPassLayout,
        .tResolveTarget = { // swapchain image
            .tLoadOp       = PL_LOAD_OP_DONT_CARE,
            .tStoreOp      = PL_STORE_OP_STORE,
            .tCurrentUsage = PL_TEXTURE_USAGE_UNSPECIFIED,
            .tNextUsage    = PL_TEXTURE_USAGE_PRESENT,
            .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
        },
        .atColorTargets = { // msaa
            {
                .tLoadOp       = PL_LOAD_OP_CLEAR,
                .tStoreOp      = PL_STORE_OP_STORE_MULTISAMPLE_RESOLVE,
                .tCurrentUsage = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .tNextUsage    = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
            }
        },
        .tDimensions = {(float)tInfo.uWidth, (float)tInfo.uHeight},
        .ptSwapchain = ptAppData->ptSwapchain
    };

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
    gptWindows->destroy_window(ptAppData->ptWindow);
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
        .bVSync  = true,
        .uWidth  = (uint32_t)ptIO->tMainViewportSize.x,
        .uHeight = (uint32_t)ptIO->tMainViewportSize.y,
        .tSampleCount = gptGfx->get_swapchain_info(ptAppData->ptSwapchain).tSampleCount,
    };
    gptGfx->recreate_swapchain(ptAppData->ptSwapchain, &tDesc);
    uint32_t uImageCount = 0;
    plTextureHandle* atSwapchainImages = gptGfx->get_swapchain_images(ptAppData->ptSwapchain, &uImageCount);

    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptAppData->atCmdPools[0]);
    gptGfx->begin_command_recording(ptCommandBuffer, NULL);

    // begin blit pass, copy buffer, end pass
    plBlitEncoder* ptEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
    plSwapchainInfo tInfo = gptGfx->get_swapchain_info(ptAppData->ptSwapchain);
    const plTextureDesc tColorTextureDesc = {
        .tDimensions   = {(float)tInfo.uWidth, (float)tInfo.uHeight, 1},
        .tFormat       = tInfo.tFormat,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
        .pcDebugName   = "offscreen color texture",
        .tSampleCount  = tInfo.tSampleCount
    };

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

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~UI~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    if(gptUi->begin_window("Pilot Light", NULL, false))
    {

        const float pfRatios[] = {1.0f};
        gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
        if(gptUi->begin_collapsing_header("Information", 0))
        {
            
            gptUi->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
            #ifdef PL_METAL_BACKEND
            gptUi->text("Graphics Backend: Metal");
            #elif PL_VULKAN_BACKEND
            gptUi->text("Graphics Backend: Vulkan");
            #else
            gptUi->text("Graphics Backend: Unknown");
            #endif

            gptUi->end_collapsing_header();
        }
        if(gptUi->begin_collapsing_header("User Interface", 0))
        {
            gptUi->checkbox("UI Debug", &ptAppData->bShowUiDebug);
            gptUi->checkbox("UI Demo", &ptAppData->bShowUiDemo);
            gptUi->checkbox("UI Style", &ptAppData->bShowUiStyle);
            gptUi->end_collapsing_header();
        }
        gptUi->end_window();
    }

    if(ptAppData->bShowUiDemo)
        pl_show_ui_demo_window(ptAppData);
        
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

    const plBeginCommandInfo tBeginInfo = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {ptAppData->aptSemaphores[uCurrentFrameIndex]},
        .auWaitSemaphoreValues = {ulValue0},
    };
    gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo);

    // begin main renderpass (directly to swapchain)
    plRenderEncoder* ptEncoder = gptGfx->begin_render_pass(ptCommandBuffer, ptAppData->tMainRenderPass);

    // submits UI drawlist/layers
    plIO* ptIO = gptIO->get_io();
    gptUi->end_frame();
    gptDrawBackend->submit_2d_drawlist(gptUi->get_draw_list(), ptEncoder, ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y, gptGfx->get_swapchain_info(ptAppData->ptSwapchain).tSampleCount);
    gptDrawBackend->submit_2d_drawlist(gptUi->get_debug_draw_list(), ptEncoder, ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y, gptGfx->get_swapchain_info(ptAppData->ptSwapchain).tSampleCount);

    // end render pass
    gptGfx->end_render_pass(ptEncoder);

    // end recording
    gptGfx->end_command_recording(ptCommandBuffer);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~submit work to GPU & present~~~~~~~~~~~~~~~~~~~~~~~

    const plSubmitInfo tSubmitInfo = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {ptAppData->aptSemaphores[uCurrentFrameIndex]},
        .auSignalSemaphoreValues = {ulValue1},
    };

    if(!gptGfx->present(ptCommandBuffer, &tSubmitInfo, &ptAppData->ptSwapchain, 1))
        pl_app_resize(ptAppData);

    gptGfx->return_command_buffer(ptCommandBuffer);
    gptProfile->end_frame();
}

//-----------------------------------------------------------------------------
// [SECTION] full demo
//-----------------------------------------------------------------------------

void
pl_show_ui_demo_window(plAppData* ptAppData)
{
    if(gptUi->begin_window("UI Demo", &ptAppData->bShowUiDemo, PL_UI_WINDOW_FLAGS_HORIZONTAL_SCROLLBAR))
    {

        static const float pfRatios0[] = {1.0f};
        gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios0);

        if(gptUi->begin_collapsing_header("Help", 0))
        {
            gptUi->text("Under construction");
            gptUi->end_collapsing_header();
        }
    
        if(gptUi->begin_collapsing_header("Window Options", 0))
        {
            gptUi->text("Under construction");
            gptUi->end_collapsing_header();
        }

        if(gptUi->begin_collapsing_header("Widgets", 0))
        {
            if(gptUi->tree_node("Basic", 0))
            {

                gptUi->layout_static(0.0f, 100, 2);
                gptUi->button("Button");
                gptUi->checkbox("Checkbox", NULL);

                gptUi->layout_dynamic(0.0f, 2);
                gptUi->button("Button");
                gptUi->checkbox("Checkbox", NULL);

                gptUi->layout_dynamic(0.0f, 1);
                static char buff[64] = {'c', 'a', 'a'};
                gptUi->input_text("label 0", buff, 64, 0);
                static char buff2[64] = {'c', 'c', 'c'};
                gptUi->input_text_hint("label 1", "hint", buff2, 64, 0);

                static float fValue = 3.14f;
                static int iValue117 = 117;

                gptUi->input_float("label 2", &fValue, "%0.3f", 0);
                gptUi->input_int("label 3", &iValue117, 0);

                static int iValue = 0;
                gptUi->layout_row_begin(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 3);

                gptUi->layout_row_push(0.33f);
                gptUi->radio_button("Option 1", &iValue, 0);

                gptUi->layout_row_push(0.33f);
                gptUi->radio_button("Option 2", &iValue, 1);

                gptUi->layout_row_push(0.34f);
                gptUi->radio_button("Option 3", &iValue, 2);

                gptUi->layout_row_end();

                const float pfRatios[] = {1.0f};
                gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
                gptUi->separator();
                gptUi->labeled_text("Label", "Value");
                static int iValue1 = 0;
                static float fValue1 = 23.0f;
                static float fValue2 = 100.0f;
                static int iValue2 = 3;
                gptUi->slider_float("float slider 1", &fValue1, 0.0f, 100.0f, 0);
                gptUi->slider_float("float slider 2", &fValue2, -50.0f, 100.0f, 0);
                gptUi->slider_int("int slider 1", &iValue1, 0, 10, 0);
                gptUi->slider_int("int slider 2", &iValue2, -5, 10, 0);
                gptUi->drag_float("float drag", &fValue2, 1.0f, -100.0f, 100.0f, 0);
                static int aiIntArray[4] = {0};
                gptUi->input_int2("input int 2", aiIntArray, 0);
                gptUi->input_int3("input int 3", aiIntArray, 0);
                gptUi->input_int4("input int 4", aiIntArray, 0);

                static float afFloatArray[4] = {0};
                gptUi->input_float2("input float 2", afFloatArray, "%0.3f", 0);
                gptUi->input_float3("input float 3", afFloatArray, "%0.3f", 0);
                gptUi->input_float4("input float 4", afFloatArray, "%0.3f", 0);

                if(gptUi->menu_item("Menu item 0", NULL, false, true))
                {
                    printf("menu item 0\n");
                }

                if(gptUi->menu_item("Menu item selected", "CTRL+M", true, true))
                {
                    printf("menu item selected\n");
                }

                if(gptUi->menu_item("Menu item disabled", NULL, false, false))
                {
                    printf("menu item disabled\n");
                }

                static bool bMenuSelection = false;
                if(gptUi->menu_item_toggle("Menu item toggle", NULL, &bMenuSelection, true))
                {
                    printf("menu item toggle\n");
                }

                if(gptUi->begin_menu("menu (not ready)", true))
                {

                    if(gptUi->menu_item("Menu item 0", NULL, false, true))
                    {
                        printf("menu item 0\n");
                    }

                    if(gptUi->menu_item("Menu item selected", "CTRL+M", true, true))
                    {
                        printf("menu item selected\n");
                    }

                    if(gptUi->menu_item("Menu item disabled", NULL, false, false))
                    {
                        printf("menu item disabled\n");
                    }
                    if(gptUi->begin_menu("sub menu", true))
                    {

                        if(gptUi->menu_item("Menu item 0", NULL, false, true))
                        {
                            printf("menu item 0\n");
                        }
                        gptUi->end_menu();
                    }
                    gptUi->end_menu();
                }


                static uint32_t uComboSelect = 0;
                static const char* apcCombo[] = {
                    "Tomato",
                    "Onion",
                    "Carrot",
                    "Lettuce",
                    "Fish"
                };
                bool abCombo[5] = {0};
                abCombo[uComboSelect] = true;
                if(gptUi->begin_combo("Combo", apcCombo[uComboSelect], PL_UI_COMBO_FLAGS_NONE))
                {
                    for(uint32_t i = 0; i < 5; i++)
                    {
                        if(gptUi->selectable(apcCombo[i], &abCombo[i], 0))
                        {
                            uComboSelect = i;
                            gptUi->close_current_popup();
                        }
                    }
                    gptUi->end_combo();
                }

                const float pfRatios22[] = {200.0f, 120.0f};
                gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 2, pfRatios22);
                gptUi->button("Hover me!");
                if(gptUi->was_last_item_hovered())
                {
                    gptUi->begin_tooltip();
                    gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios22);
                    gptUi->text("I'm a tooltip!");
                    gptUi->end_tooltip();
                }
                gptUi->button("Just a button");

                gptUi->tree_pop();
            }

            if(gptUi->tree_node("Selectables", 0))
            {
                static bool bSelectable0 = false;
                static bool bSelectable1 = false;
                static bool bSelectable2 = false;
                gptUi->selectable("Selectable 1", &bSelectable0, 0);
                gptUi->selectable("Selectable 2", &bSelectable1, 0);
                gptUi->selectable("Selectable 3", &bSelectable2, 0);
                gptUi->tree_pop();
            }

            if(gptUi->tree_node("Combo", 0))
            {
                plUiComboFlags tComboFlags = PL_UI_COMBO_FLAGS_NONE;

                static bool bComboHeightSmall = false;
                static bool bComboHeightRegular = false;
                static bool bComboHeightLarge = false;
                static bool bComboNoArrow = false;

                gptUi->checkbox("PL_UI_COMBO_FLAGS_HEIGHT_SMALL", &bComboHeightSmall);
                gptUi->checkbox("PL_UI_COMBO_FLAGS_HEIGHT_REGULAR", &bComboHeightRegular);
                gptUi->checkbox("PL_UI_COMBO_FLAGS_HEIGHT_LARGE", &bComboHeightLarge);
                gptUi->checkbox("PL_UI_COMBO_FLAGS_NO_ARROW_BUTTON", &bComboNoArrow);

                if(bComboHeightSmall)   tComboFlags |= PL_UI_COMBO_FLAGS_HEIGHT_SMALL;
                if(bComboHeightRegular) tComboFlags |= PL_UI_COMBO_FLAGS_HEIGHT_REGULAR;
                if(bComboHeightLarge)   tComboFlags |= PL_UI_COMBO_FLAGS_HEIGHT_LARGE;
                if(bComboNoArrow)       tComboFlags |= PL_UI_COMBO_FLAGS_NO_ARROW_BUTTON;

                static uint32_t uComboSelect = 0;
                static const char* apcCombo[] = {
                    "Tomato",
                    "Onion",
                    "Carrot",
                    "Lettuce",
                    "Fish",
                    "Beef",
                    "Chicken",
                    "Cereal",
                    "Wheat",
                    "Cane",
                };
                bool abCombo[10] = {0};
                abCombo[uComboSelect] = true;
                if(gptUi->begin_combo("Combo", apcCombo[uComboSelect], tComboFlags))
                {
                    for(uint32_t i = 0; i < 10; i++)
                    {
                        if(gptUi->selectable(apcCombo[i], &abCombo[i], 0))
                        {
                            uComboSelect = i;
                            gptUi->close_current_popup();
                        }
                    }
                    gptUi->end_combo();
                }
                gptUi->tree_pop();
            }

            if(gptUi->tree_node("Plotting", 0))
            {
                gptUi->progress_bar(0.75f, (plVec2){-1.0f, 0.0f}, NULL);
                gptUi->tree_pop();
            }

            if(gptUi->tree_node("Trees", 0))
            {
                
                if(gptUi->tree_node("Root Node", 0))
                {
                    if(gptUi->tree_node("Child 1", 0))
                    {
                        gptUi->button("Press me");
                        gptUi->tree_pop();
                    }
                    if(gptUi->tree_node("Child 2", 0))
                    {
                        gptUi->button("Press me");
                        gptUi->tree_pop();
                    }
                    gptUi->tree_pop();
                }
                gptUi->tree_pop();
            }

            if(gptUi->tree_node("Tabs", 0))
            {
                if(gptUi->begin_tab_bar("Tabs1", 0))
                {
                    if(gptUi->begin_tab("Tab 0", 0))
                    {
                        static bool bSelectable0 = false;
                        static bool bSelectable1 = false;
                        static bool bSelectable2 = false;
                        gptUi->selectable("Selectable 1", &bSelectable0, 0);
                        gptUi->selectable("Selectable 2", &bSelectable1, 0);
                        gptUi->selectable("Selectable 3", &bSelectable2, 0);
                        gptUi->end_tab();
                    }

                    if(gptUi->begin_tab("Tab 1", 0))
                    {
                        static int iValue = 0;
                        gptUi->radio_button("Option 1", &iValue, 0);
                        gptUi->radio_button("Option 2", &iValue, 1);
                        gptUi->radio_button("Option 3", &iValue, 2);
                        gptUi->end_tab();
                    }

                    if(gptUi->begin_tab("Tab 2", 0))
                    {
                        if(gptUi->begin_child("CHILD2", 0, 0))
                        {
                            const float pfRatios3[] = {600.0f};
                            gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios3);

                            for(uint32_t i = 0; i < 25; i++)
                                gptUi->text("Long text is happening11111111111111111111111111111111111111111111111111111111123456789");
                            gptUi->end_child();
                        }
                        
                        gptUi->end_tab();
                    }
                    gptUi->end_tab_bar();
                }
                gptUi->tree_pop();
            }
            gptUi->end_collapsing_header();
        }

        if(gptUi->begin_collapsing_header("Scrolling", 0))
        {
            const float pfRatios2[] = {0.5f, 0.50f};
            const float pfRatios3[] = {600.0f};

            gptUi->layout_static(0.0f, 200, 1);
            static bool bUseClipper = true;
            gptUi->checkbox("Use Clipper", &bUseClipper);
            
            gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 300.0f, 2, pfRatios2);
            if(gptUi->begin_child("CHILD", 0, 0))
            {

                gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 2, pfRatios2);


                if(bUseClipper)
                {
                    plUiClipper tClipper = {1000000};
                    while(gptUi->step_clipper(&tClipper))
                    {
                        for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
                        {
                            gptUi->text("%u Label", i);
                            gptUi->text("%u Value", i);
                        } 
                    }
                }
                else
                {
                    for(uint32_t i = 0; i < 1000000; i++)
                    {
                            gptUi->text("%u Label", i);
                            gptUi->text("%u Value", i);
                    }
                }


                gptUi->end_child();
            }
            

            if(gptUi->begin_child("CHILD2", 0, 0))
            {
                gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios3);

                for(uint32_t i = 0; i < 25; i++)
                    gptUi->text("Long text is happening11111111111111111111111111111111111111111111111111111111123456789");

                gptUi->end_child();
            }
            

            gptUi->end_collapsing_header();
        }

        if(gptUi->begin_collapsing_header("Layout Systems", 0))
        {
            gptUi->text("General Notes");
            gptUi->text("  - systems ordered by increasing flexibility");
            gptUi->separator();

            if(gptUi->tree_node("System 1 - simple dynamic", 0))
            {
                static int iWidgetCount = 5;
                static float fWidgetHeight = 0.0f;
                gptUi->separator_text("Notes");
                gptUi->text("  - wraps (i.e. will add rows)");
                gptUi->text("  - evenly spaces widgets based on available space");
                gptUi->text("  - height of 0.0f sets row height equal to minimum height");
                gptUi->text("    of maximum height widget");
                gptUi->vertical_spacing();

                gptUi->separator_text("Options");
                gptUi->slider_int("Widget Count", &iWidgetCount, 1, 10, 0);
                gptUi->slider_float("Height", &fWidgetHeight, 0.0f, 100.0f, 0);
                gptUi->vertical_spacing();

                gptUi->separator_text("Example");
                gptUi->layout_dynamic(fWidgetHeight, (uint32_t)iWidgetCount);
                gptUi->vertical_spacing();
                for(int i = 0; i < iWidgetCount * 2; i++)
                {
                    pl_sb_sprintf(ptAppData->sbcTempBuffer, "Button %d", i);
                    gptUi->button(ptAppData->sbcTempBuffer);
                    pl_sb_reset(ptAppData->sbcTempBuffer);
                }
                gptUi->tree_pop();
            }

            if(gptUi->tree_node("System 2 - simple static", 0))
            {
                static int iWidgetCount = 5;
                static float fWidgetWidth = 100.0f;
                static float fWidgetHeight = 0.0f;
                gptUi->separator_text("Notes");
                gptUi->text("  - wraps (i.e. will add rows)");
                gptUi->text("  - provides each widget with the same specified width");
                gptUi->text("  - height of 0.0f sets row height equal to minimum height");
                gptUi->text("    of maximum height widget");
                gptUi->vertical_spacing();

                gptUi->separator_text("Options");
                gptUi->slider_int("Widget Count", &iWidgetCount, 1, 10, 0);
                gptUi->slider_float("Width", &fWidgetWidth, 50.0f, 500.0f, 0);
                gptUi->slider_float("Height", &fWidgetHeight, 0.0f, 100.0f, 0);
                gptUi->vertical_spacing();

                gptUi->separator_text("Example");
                gptUi->layout_static(fWidgetHeight, fWidgetWidth, (uint32_t)iWidgetCount);
                gptUi->vertical_spacing();
                for(int i = 0; i < iWidgetCount * 2; i++)
                {
                    pl_sb_sprintf(ptAppData->sbcTempBuffer, "Button %d", i);
                    gptUi->button(ptAppData->sbcTempBuffer);
                    pl_sb_reset(ptAppData->sbcTempBuffer);
                }
                gptUi->tree_pop();
            }

            if(gptUi->tree_node("System 3 - single system row", 0))
            {
                static bool bDynamicRow = false;
                static int iWidgetCount = 2;
                static float afWidgetStaticWidths[4] = {
                    100.0f, 100.0f, 100.0f, 100.0f
                };
                static float afWidgetDynamicWidths[4] = {
                    0.25f, 0.25f, 0.25f, 0.25f
                };

                static float fWidgetHeight = 0.0f;

                gptUi->separator_text("Notes");
                gptUi->text("  - does not wrap (i.e. will not add rows)");
                gptUi->text("  - allows user to change widget widths individually");
                gptUi->text("  - widths interpreted as ratios of available width when");
                gptUi->text("    using PL_UI_LAYOUT_ROW_TYPE_DYNAMIC");
                gptUi->text("  - widths interpreted as pixel width when using PL_UI_LAYOUT_ROW_TYPE_STATIC");
                gptUi->text("  - height of 0.0f sets row height equal to minimum height");
                gptUi->text("    of maximum height widget");
                gptUi->vertical_spacing();

                gptUi->separator_text("Options");
                gptUi->checkbox("Dynamic", &bDynamicRow);
                gptUi->slider_int("Widget Count", &iWidgetCount, 1, 4, 0);
                gptUi->slider_float("Height", &fWidgetHeight, 0.0f, 100.0f, 0);

                if(bDynamicRow)
                {
                    for(int i = 0; i < iWidgetCount; i++)
                    {
                        gptUi->push_id_uint((uint32_t)i);
                        gptUi->slider_float("Widget Width", &afWidgetDynamicWidths[i], 0.05f, 1.2f, 0);
                        gptUi->pop_id();
                    }
                }
                else
                {
                    for(int i = 0; i < iWidgetCount; i++)
                    {
                        gptUi->push_id_uint((uint32_t)i);
                        gptUi->slider_float("Widget Width", &afWidgetStaticWidths[i], 50.0f, 500.0f, 0);
                        gptUi->pop_id();
                    }
                }
                gptUi->vertical_spacing();

                gptUi->separator_text("Example");
                gptUi->layout_row_begin(bDynamicRow ? PL_UI_LAYOUT_ROW_TYPE_DYNAMIC : PL_UI_LAYOUT_ROW_TYPE_STATIC, fWidgetHeight, (uint32_t)iWidgetCount);
                float* afWidgetWidths = bDynamicRow ? afWidgetDynamicWidths : afWidgetStaticWidths;
                for(int i = 0; i < iWidgetCount; i++)
                {
                    gptUi->layout_row_push(afWidgetWidths[i]);
                    pl_sb_sprintf(ptAppData->sbcTempBuffer, "Button %d", i);
                    gptUi->button(ptAppData->sbcTempBuffer);
                    pl_sb_reset(ptAppData->sbcTempBuffer);
                }
                gptUi->layout_row_end();
                gptUi->vertical_spacing();
                gptUi->tree_pop();
            }

            if(gptUi->tree_node("System 4 - single system row (array form)", 0))
            {
                static bool bDynamicRow = false;
                static int iWidgetCount = 2;
                static float afWidgetStaticWidths[4] = {
                    100.0f, 100.0f, 100.0f, 100.0f
                };
                static float afWidgetDynamicWidths[4] = {
                    0.25f, 0.25f, 0.25f, 0.25f
                };

                static float fWidgetHeight = 0.0f;

                gptUi->separator_text("Notes");
                gptUi->text("  - same as System 3 but array form");
                gptUi->text("  - wraps (i.e. will add rows)");
                gptUi->text("  - allows user to change widget widths individually");
                gptUi->text("  - widths interpreted as ratios of available width when");
                gptUi->text("    using PL_UI_LAYOUT_ROW_TYPE_DYNAMIC");
                gptUi->text("  - widths interpreted as pixel width when using PL_UI_LAYOUT_ROW_TYPE_STATIC");
                gptUi->text("  - height of 0.0f sets row height equal to minimum height");
                gptUi->text("    of maximum height widget");
                gptUi->vertical_spacing();

                gptUi->separator_text("Options");
                gptUi->checkbox("Dynamic", &bDynamicRow);
                gptUi->slider_int("Widget Count", &iWidgetCount, 1, 4, 0);
                gptUi->slider_float("Height", &fWidgetHeight, 0.0f, 100.0f, 0);

                if(bDynamicRow)
                {
                    for(int i = 0; i < iWidgetCount; i++)
                    {
                        gptUi->push_id_uint((uint32_t)i);
                        gptUi->slider_float("Widget Width", &afWidgetDynamicWidths[i], 0.05f, 1.2f, 0);
                        gptUi->pop_id();
                    }
                }
                else
                {
                    for(int i = 0; i < iWidgetCount; i++)
                    {
                        gptUi->push_id_uint((uint32_t)i);
                        gptUi->slider_float("Widget Width", &afWidgetStaticWidths[i], 50.0f, 500.0f, 0);
                        gptUi->pop_id();
                    }
                }
                gptUi->vertical_spacing();

                gptUi->separator_text("Example");
                float* afWidgetWidths = bDynamicRow ? afWidgetDynamicWidths : afWidgetStaticWidths;
                gptUi->layout_row(bDynamicRow ? PL_UI_LAYOUT_ROW_TYPE_DYNAMIC : PL_UI_LAYOUT_ROW_TYPE_STATIC, fWidgetHeight, (uint32_t)iWidgetCount, afWidgetWidths);
                for(int i = 0; i < iWidgetCount * 2; i++)
                {
                    pl_sb_sprintf(ptAppData->sbcTempBuffer, "Button %d", i);
                    gptUi->button(ptAppData->sbcTempBuffer);
                    pl_sb_reset(ptAppData->sbcTempBuffer);
                }
                gptUi->vertical_spacing();
                gptUi->tree_pop();
            }

            if(gptUi->tree_node("System 5 - template", 0))
            {
                static int iWidgetCount = 6;
                static float fWidgetHeight = 0.0f;

                gptUi->separator_text("Notes");
                gptUi->text("  - most complex and second most flexible system");
                gptUi->text("  - wraps (i.e. will add rows)");
                gptUi->text("  - allows user to change widget systems individually");
                gptUi->text("    - dynamic: changes based on available space");
                gptUi->text("    - variable: same as dynamic but minimum width specified by user");
                gptUi->text("    - static: pixel width explicitely specified by user");
                gptUi->text("  - height of 0.0f sets row height equal to minimum height");
                gptUi->text("    of maximum height widget");
                gptUi->vertical_spacing();

                gptUi->separator_text("Options");
                gptUi->slider_float("Height", &fWidgetHeight, 0.0f, 100.0f, 0);
                gptUi->vertical_spacing();

                gptUi->separator_text("Example 0");

                gptUi->layout_template_begin(fWidgetHeight);
                gptUi->layout_template_push_dynamic();
                gptUi->layout_template_push_variable(150.0f);
                gptUi->layout_template_push_static(150.0f);
                gptUi->layout_template_end();
                gptUi->button("dynamic##0");
                gptUi->button("variable 150.0f##0");
                gptUi->button("static 150.0f##0");
                gptUi->checkbox("dynamic##1", NULL);
                gptUi->checkbox("variable 150.0f##1", NULL);
                gptUi->checkbox("static 150.0f##1", NULL);
                gptUi->vertical_spacing();

                gptUi->layout_dynamic(0.0f, 1);
                gptUi->separator_text("Example 1");
                gptUi->layout_template_begin(fWidgetHeight);
                gptUi->layout_template_push_static(150.0f);
                gptUi->layout_template_push_variable(150.0f);
                gptUi->layout_template_push_dynamic();
                gptUi->layout_template_end();
                gptUi->button("static 150.0f##2");
                gptUi->button("variable 150.0f##2");
                gptUi->button("dynamic##2");
                gptUi->checkbox("static 150.0f##3", NULL);
                gptUi->checkbox("variable 150.0f##3", NULL);
                gptUi->checkbox("dynamic##3", NULL);

                gptUi->layout_dynamic(0.0f, 1);
                gptUi->separator_text("Example 2");
                gptUi->layout_template_begin(fWidgetHeight);
                gptUi->layout_template_push_variable(150.0f);
                gptUi->layout_template_push_variable(300.0f);
                gptUi->layout_template_push_dynamic();
                gptUi->layout_template_end();
                gptUi->button("variable 150.0f##4");
                gptUi->button("variable 300.0f##4");
                gptUi->button("dynamic##4");
                gptUi->checkbox("static 150.0f##5", NULL);
                gptUi->button("variable 300.0f##5");
                gptUi->checkbox("dynamic##5", NULL);
                
                gptUi->vertical_spacing();
                gptUi->tree_pop();
            }

            if(gptUi->tree_node("System 6 - space", 0))
            {
                gptUi->separator_text("Notes");
                gptUi->text("  - most flexible system");
                gptUi->vertical_spacing();

                gptUi->separator_text("Example - static");

                gptUi->layout_space_begin(PL_UI_LAYOUT_ROW_TYPE_STATIC, 500.0f, UINT32_MAX);

                gptUi->layout_space_push(0.0f, 0.0f, 100.0f, 100.0f);
                gptUi->button("w100 h100");

                gptUi->layout_space_push(105.0f, 105.0f, 300.0f, 100.0f);
                gptUi->button("x105 y105 w300 h100");

                gptUi->layout_space_end();

                gptUi->layout_dynamic(0.0f, 1);
                gptUi->separator_text("Example - dynamic");

                gptUi->layout_space_begin(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 300.0f, 2);

                gptUi->layout_space_push(0.0f, 0.0f, 0.5f, 0.5f);
                gptUi->button("x0 y0 w0.5 h0.5");

                gptUi->layout_space_push(0.5f, 0.5f, 0.5f, 0.5f);
                gptUi->button("x0.5 y0.5 w0.5 h0.5");

                gptUi->layout_space_end();

                gptUi->tree_pop();
            }

            if(gptUi->tree_node("Misc. Testing", 0))
            {
                const float pfRatios[] = {1.0f};
                const float pfRatios2[] = {0.5f, 0.5f};
                const float pfRatios3[] = {0.5f * 0.5f, 0.25f * 0.5f, 0.25f * 0.5f};
                gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 2, pfRatios2);
                if(gptUi->begin_collapsing_header("Information", 0))
                {
                    gptUi->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
                    #ifdef PL_METAL_BACKEND
                    gptUi->text("Graphics Backend: Metal");
                    #elif PL_VULKAN_BACKEND
                    gptUi->text("Graphics Backend: Vulkan");
                    #else
                    gptUi->text("Graphics Backend: Unknown");
                    #endif

                    gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 3, pfRatios3);
                    if(gptUi->begin_collapsing_header("sub0", 0))
                    {
                        gptUi->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
                        gptUi->end_collapsing_header();
                    }
                    if(gptUi->begin_collapsing_header("sub1", 0))
                    {
                        gptUi->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
                        gptUi->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
                        gptUi->end_collapsing_header();
                    }
                    if(gptUi->begin_collapsing_header("sub2", 0))
                    {
                        gptUi->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
                        gptUi->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
                        gptUi->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
                        gptUi->end_collapsing_header();
                    }

                    gptUi->end_collapsing_header();
                }
                if(gptUi->begin_collapsing_header("App Options", 0))
                {
                    gptUi->checkbox("Freeze Culling Camera", NULL);
                    int iCascadeCount  = 2;
                    gptUi->slider_int("Sunlight Cascades", &iCascadeCount, 1, 4, 0);

                    gptUi->end_collapsing_header();
                }
                
                if(gptUi->begin_collapsing_header("Graphics", 0))
                {
                    gptUi->checkbox("Freeze Culling Camera", NULL);
                    int iCascadeCount  = 2;
                    gptUi->slider_int("Sunlight Cascades", &iCascadeCount, 1, 4, 0);

                    gptUi->end_collapsing_header();
                }
                if(gptUi->begin_tab_bar("tab bar2", 0))
                {
                    if(gptUi->begin_tab("tab0000000000", 0))
                    {
                        gptUi->checkbox("Entities", NULL);
                        gptUi->end_tab();
                    }
                    if(gptUi->begin_tab("tab1", 0))
                    {
                        gptUi->checkbox("Profiling", NULL);
                        gptUi->checkbox("Profiling", NULL);
                        gptUi->checkbox("Profiling", NULL);
                        gptUi->checkbox("Profiling", NULL);
                        gptUi->end_tab();
                    }
                    gptUi->end_tab_bar();
                }

                gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
                if(gptUi->begin_collapsing_header("Tools", 0))
                {
                    gptUi->checkbox("Device Memory Analyzer", NULL);
                    gptUi->checkbox("Device Memory Analyzer", NULL);
                    gptUi->end_collapsing_header();
                }

                if(gptUi->begin_collapsing_header("Debug", 0))
                {
                    gptUi->button("resize");
                    gptUi->checkbox("Always Resize", NULL);
                    gptUi->end_collapsing_header();
                }

                gptUi->tree_pop();
            }
            gptUi->end_collapsing_header();
        }
    }
    gptUi->end_window();
}
