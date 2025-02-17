/*
   example_8.c
     - demonstrates loading APIs
     - demonstrates loading extensions
     - demonstrates hot reloading
     - demonstrates drawing extension (2D & 3D)
     - demonstrates minimal use of graphics extension

    Notes:
     - We are performing offscreen rendering for this example
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] apis
// [SECTION] helper function declarations
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
// [SECTION] helper function definitions
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdio.h>
#include "pl.h"
#include "pl_ds.h"
#include "pl_memory.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extensions
#include "pl_log_ext.h"
#include "pl_window_ext.h"
#include "pl_graphics_ext.h"
#include "pl_draw_ext.h"
#include "pl_shader_ext.h"
#include "pl_draw_backend_ext.h"
#include "pl_profile_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plCamera
{
    plVec3 tPos;
    float  fNearZ;
    float  fFarZ;
    float  fFieldOfView;
    float  fAspectRatio;  // width/height
    plMat4 tViewMat;      // cached
    plMat4 tProjMat;      // cached
    plMat4 tTransformMat; // cached

    // rotations
    float fPitch; // rotation about right vector
    float fYaw;   // rotation about up vector
    float fRoll;  // rotation about forward vector

    // direction vectors
    plVec3 _tUpVec;
    plVec3 _tForwardVec;
    plVec3 _tRightVec;
} plCamera;

typedef struct _plAppData
{
    // window
    plWindow* ptWindow;

    // ui options
    bool bShowUiDemo;

    // drawing
    plDrawList2D*  ptAppDrawlist;
    plDrawLayer2D* ptFGLayer;

    // 3d drawing
    plCamera      tCamera;
    plDrawList3D* pt3dDrawlist;

    // graphics & sync objects
    plDevice*                ptDevice;
    plSurface*               ptSurface;
    plSwapchain*             ptSwapchain;
    plTimelineSemaphore*     aptSemaphores[PL_MAX_FRAMES_IN_FLIGHT];
    uint64_t                 aulNextTimelineValue[PL_MAX_FRAMES_IN_FLIGHT];
    plCommandPool*           atCmdPools[PL_MAX_FRAMES_IN_FLIGHT];
    plRenderPassHandle       tMainRenderPass;
    plRenderPassLayoutHandle tMainRenderPassLayout;

    // offscreen rendering
    bool               bResize;
    plSamplerHandle    tDefaultSampler;
    plRenderPassHandle tOffscreenRenderPass;
    plVec2             tOffscreenSize;
    plTextureHandle    atColorTexture[PL_MAX_FRAMES_IN_FLIGHT];
    plBindGroupHandle  atColorTextureBg[PL_MAX_FRAMES_IN_FLIGHT];
    plTextureHandle    atDepthTexture[PL_MAX_FRAMES_IN_FLIGHT];

} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

const plIOI*          gptIO          = NULL;
const plWindowI*      gptWindows     = NULL;
const plGraphicsI*    gptGfx         = NULL;
const plDrawI*        gptDraw        = NULL;
const plShaderI*      gptShader      = NULL;
const plDrawBackendI* gptDrawBackend = NULL;
const plProfileI*     gptProfile     = NULL;

//-----------------------------------------------------------------------------
// [SECTION] helper function declarations
//-----------------------------------------------------------------------------

void resize_offscreen_resources(plAppData* ptAppData);

// camera helpers
void camera_translate(plCamera*, float fDx, float fDy, float fDz);
void camera_rotate   (plCamera*, float fDPitch, float fDYaw);
void camera_rotate   (plCamera*, float fDPitch, float fDYaw);
void camera_update   (plCamera*);

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
        gptProfile     = pl_get_api_latest(ptApiRegistry, plProfileI);

        return ptAppData;
    }

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = malloc(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    // retrieve extension registry
    const plExtensionRegistryI* ptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);

    // load extensions (makes their APIs available)
    ptExtensionRegistry->load("pl_unity_ext", NULL, NULL, true);
    
    // load required apis
    gptIO          = pl_get_api_latest(ptApiRegistry, plIOI);
    gptWindows     = pl_get_api_latest(ptApiRegistry, plWindowI);
    gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptDraw        = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptShader      = pl_get_api_latest(ptApiRegistry, plShaderI);
    gptDrawBackend = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
    gptProfile     = pl_get_api_latest(ptApiRegistry, plProfileI);

    // use window API to create a window
    plWindowDesc tWindowDesc = {
        .pcTitle = "Example 9",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 600,
        .uHeight = 600,
    };
    gptWindows->create_window(tWindowDesc, &ptAppData->ptWindow);
    ptAppData->tOffscreenSize = (plVec2){600.0f, 600.0f};

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

    // create swapchain
    const plSwapchainInit tSwapInit = {.bVSync = true};
    ptAppData->ptSwapchain = gptGfx->create_swapchain(ptAppData->ptDevice, ptAppData->ptSurface, &tSwapInit);

    // create main render pass layout
    const plRenderPassLayoutDesc tMainRenderPassLayoutDesc = {
        .atRenderTargets = {
            { .tFormat = gptGfx->get_swapchain_info(ptAppData->ptSwapchain).tFormat },  // depth buffer
        },
        .atSubpasses = {
            {
                .uRenderTargetCount = 1,
                .auRenderTargets = {0}
            }
        },
        .atSubpassDependencies = {
            {
                .uSourceSubpass = UINT32_MAX,
                .uDestinationSubpass = 0,
                .tSourceStageMask = PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS | PL_PIPELINE_STAGE_COMPUTE_SHADER,
                .tDestinationStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tSourceAccessMask = PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
                .tDestinationAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
            },
            {
                .uSourceSubpass = 0,
                .uDestinationSubpass = UINT32_MAX,
                .tSourceStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tDestinationStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS | PL_PIPELINE_STAGE_COMPUTE_SHADER,
                .tSourceAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
                .tDestinationAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
            },
        }
    };
    ptAppData->tMainRenderPassLayout = gptGfx->create_render_pass_layout(ptAppData->ptDevice, &tMainRenderPassLayoutDesc);

    // create main render pass
    const plRenderPassDesc tMainRenderPassDesc = {
        .tLayout = ptAppData->tMainRenderPassLayout,
        .atColorTargets = {
            {
                .tLoadOp       = PL_LOAD_OP_CLEAR,
                .tStoreOp      = PL_STORE_OP_STORE,
                .tCurrentUsage = PL_TEXTURE_USAGE_UNSPECIFIED,
                .tNextUsage    = PL_TEXTURE_USAGE_PRESENT,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
            }
        },
        .tDimensions = {.x = gptIO->get_io()->tMainViewportSize.x, .y = gptIO->get_io()->tMainViewportSize.y},
        .ptSwapchain = ptAppData->ptSwapchain
    };
    uint32_t uImageCount = 0;
    plTextureHandle* atSwapchainImages = gptGfx->get_swapchain_images(ptAppData->ptSwapchain, &uImageCount);
    plRenderPassAttachments atMainAttachmentSets[16] = {0};
    for(uint32_t i = 0; i < uImageCount; i++)
    {
        atMainAttachmentSets[i].atViewAttachments[0] = atSwapchainImages[i];
    }
    ptAppData->tMainRenderPass = gptGfx->create_render_pass(ptAppData->ptDevice, &tMainRenderPassDesc, atMainAttachmentSets);


    // create command pools
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        ptAppData->atCmdPools[i] = gptGfx->create_command_pool(ptAppData->ptDevice, NULL);

    // initialize shader extension
    static const plShaderOptions tDefaultShaderOptions = {
        .apcIncludeDirectories = {
            "../examples/shaders/"
        },
        .apcDirectories = {
            "../shaders/"
        },
        .tFlags = PL_SHADER_FLAGS_AUTO_OUTPUT
    };
    gptShader->initialize(&tDefaultShaderOptions);

    // setup draw
    gptDraw->initialize(NULL);
    gptDrawBackend->initialize(ptAppData->ptDevice);

    // request drawlists
    ptAppData->ptAppDrawlist = gptDraw->request_2d_drawlist();
    ptAppData->ptFGLayer = gptDraw->request_2d_layer(ptAppData->ptAppDrawlist);
    ptAppData->pt3dDrawlist = gptDraw->request_3d_drawlist();

    // for convience
    plDevice* ptDevice = ptAppData->ptDevice;

    // create camera
    ptAppData->tCamera = (plCamera){
        .tPos         = {5.0f, 10.0f, 10.0f},
        .fNearZ       = 0.01f,
        .fFarZ        = 50.0f,
        .fFieldOfView = PL_PI_3,
        .fAspectRatio = ptAppData->tOffscreenSize.x / ptAppData->tOffscreenSize.y,
        .fYaw         = PL_PI + PL_PI_4,
        .fPitch       = -PL_PI_4,
    };
    camera_update(&ptAppData->tCamera);

    // create timeline semaphores to syncronize GPU work submission
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        ptAppData->aptSemaphores[i] = gptGfx->create_semaphore(ptDevice, false);

    // create default sampler
    const plSamplerDesc tSamplerDesc = {
        .tMagFilter      = PL_FILTER_LINEAR,
        .tMinFilter      = PL_FILTER_LINEAR,
        .fMinMip         = 0.0f,
        .fMaxMip         = 64.0f,
        .tVAddressMode   = PL_ADDRESS_MODE_WRAP,
        .tUAddressMode   = PL_ADDRESS_MODE_WRAP,
        .pcDebugName     = "default sampler"
    };
    ptAppData->tDefaultSampler = gptGfx->create_sampler(ptDevice, &tSamplerDesc);

    // create offscreen per-frame resources

    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptAppData->atCmdPools[0]);
    gptGfx->begin_command_recording(ptCommandBuffer, NULL);

    // begin blit pass, copy buffer, end pass
    plBlitEncoder* ptEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
    gptGfx->pipeline_barrier_blit(ptEncoder, PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE | PL_SHADER_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_SHADER_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);

    const plTextureDesc tColorTextureDesc = {
        .tDimensions   = {ptAppData->tOffscreenSize.x, ptAppData->tOffscreenSize.y, 1},
        .tFormat       = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
        .pcDebugName   = "offscreen color texture"
    };

    const plTextureDesc tDepthTextureDesc = {
        .tDimensions   = {ptAppData->tOffscreenSize.x, ptAppData->tOffscreenSize.y, 1},
        .tFormat       = PL_FORMAT_D32_FLOAT_S8_UINT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
        .pcDebugName   = "offscreen depth texture"
    };

    plRenderPassAttachments atAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        // create textures
        ptAppData->atColorTexture[i] = gptGfx->create_texture(ptDevice, &tColorTextureDesc, NULL);
        ptAppData->atDepthTexture[i] = gptGfx->create_texture(ptDevice, &tDepthTextureDesc, NULL);

        // retrieve textures
        plTexture* ptColorTexture = gptGfx->get_texture(ptDevice, ptAppData->atColorTexture[i]);
        plTexture* ptDepthTexture = gptGfx->get_texture(ptDevice, ptAppData->atDepthTexture[i]);

        // allocate memory

        const plDeviceMemoryAllocation tColorAllocation = gptGfx->allocate_memory(ptDevice, 
            ptColorTexture->tMemoryRequirements.ulSize,
            PL_MEMORY_GPU,
            ptColorTexture->tMemoryRequirements.uMemoryTypeBits,
            "color texture memory");

        const plDeviceMemoryAllocation tDepthAllocation = gptGfx->allocate_memory(ptDevice, 
            ptDepthTexture->tMemoryRequirements.ulSize,
            PL_MEMORY_GPU,
            ptDepthTexture->tMemoryRequirements.uMemoryTypeBits,
            "depth texture memory");

        // bind memory
        gptGfx->bind_texture_to_memory(ptDevice, ptAppData->atColorTexture[i], &tColorAllocation);
        gptGfx->bind_texture_to_memory(ptDevice, ptAppData->atDepthTexture[i], &tDepthAllocation);
        ptAppData->atColorTextureBg[i] = gptDrawBackend->create_bind_group_for_texture(ptAppData->atColorTexture[i]);

        // set initialial usage
        gptGfx->set_texture_usage(ptEncoder, ptAppData->atColorTexture[i], PL_TEXTURE_USAGE_SAMPLED, 0);
        gptGfx->set_texture_usage(ptEncoder, ptAppData->atDepthTexture[i], PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT, 0);

        // add textures to attachment set for render pass
        atAttachmentSets[i].atViewAttachments[0] = ptAppData->atDepthTexture[i];
        atAttachmentSets[i].atViewAttachments[1] = ptAppData->atColorTexture[i];
    }

    gptGfx->pipeline_barrier_blit(ptEncoder, PL_SHADER_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE | PL_SHADER_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
    gptGfx->end_blit_pass(ptEncoder);

    // finish recording
    gptGfx->end_command_recording(ptCommandBuffer);

    // submit command buffer
    gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);
    gptGfx->return_command_buffer(ptCommandBuffer);

    // create offscreen renderpass layout
    const plRenderPassLayoutDesc tRenderPassLayoutDesc = {
        .atRenderTargets = {
            { .tFormat = PL_FORMAT_D32_FLOAT_S8_UINT, .bDepth = true }, // depth buffer
            { .tFormat = PL_FORMAT_R32G32B32A32_FLOAT } // color
        },
        .atSubpasses = {
            {
                .uRenderTargetCount = 2,
                .auRenderTargets = {0, 1},
            },
        },
        .atSubpassDependencies = {
            {
                .uSourceSubpass = UINT32_MAX,
                .uDestinationSubpass = 0,
                .tSourceStageMask = PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS | PL_PIPELINE_STAGE_COMPUTE_SHADER,
                .tDestinationStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tSourceAccessMask = PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
                .tDestinationAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
            },
            {
                .uSourceSubpass = 0,
                .uDestinationSubpass = UINT32_MAX,
                .tSourceStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tDestinationStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS | PL_PIPELINE_STAGE_COMPUTE_SHADER,
                .tSourceAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
                .tDestinationAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
            },
        }
    };

    // create offscreen renderpass
    const plRenderPassDesc tRenderPassDesc = {
        .tLayout = gptGfx->create_render_pass_layout(ptDevice, &tRenderPassLayoutDesc),
        .tDepthTarget = {
                .tLoadOp         = PL_LOAD_OP_CLEAR,
                .tStoreOp        = PL_STORE_OP_DONT_CARE,
                .tStencilLoadOp  = PL_LOAD_OP_CLEAR,
                .tStencilStoreOp = PL_STORE_OP_DONT_CARE,
                .tCurrentUsage   = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
                .tNextUsage      = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
                .fClearZ         = 1.0f
        },
        .atColorTargets = {
            {
                .tLoadOp       = PL_LOAD_OP_CLEAR,
                .tStoreOp      = PL_STORE_OP_STORE,
                .tCurrentUsage = PL_TEXTURE_USAGE_SAMPLED,
                .tNextUsage    = PL_TEXTURE_USAGE_SAMPLED,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
            }
        },
        .tDimensions = {.x = ptAppData->tOffscreenSize.x, .y = ptAppData->tOffscreenSize.y}
    };
    ptAppData->tOffscreenRenderPass = gptGfx->create_render_pass(ptAppData->ptDevice, &tRenderPassDesc, atAttachmentSets);

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
    gptDrawBackend->cleanup();

    // cleanup textures
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        gptGfx->destroy_texture(ptAppData->ptDevice, ptAppData->atColorTexture[i]);
        gptGfx->destroy_texture(ptAppData->ptDevice, ptAppData->atDepthTexture[i]);
    }

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
    ptAppData->bResize = true;
    plIO* ptIO = gptIO->get_io();
    plSwapchainInit tDesc = {
        .bVSync  = true,
        .uWidth  = (uint32_t)ptIO->tMainViewportSize.x,
        .uHeight = (uint32_t)ptIO->tMainViewportSize.y
    };
    gptGfx->recreate_swapchain(ptAppData->ptSwapchain, &tDesc);
    uint32_t uImageCount = 0;
    plTextureHandle* atSwapchainImages = gptGfx->get_swapchain_images(ptAppData->ptSwapchain, &uImageCount);
    plRenderPassAttachments atMainAttachmentSets[16] = {0};
    for(uint32_t i = 0; i < uImageCount; i++)
    {
        atMainAttachmentSets[i].atViewAttachments[0] = atSwapchainImages[i];
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

    // for convience
    plIO* ptIO = gptIO->get_io();

    if(ptAppData->bResize)
    {
        plDevice* ptDevice = ptAppData->ptDevice;
        ptAppData->tOffscreenSize.x = ptIO->tMainViewportSize.x;
        ptAppData->tOffscreenSize.y = ptIO->tMainViewportSize.y;
        resize_offscreen_resources(ptAppData);
        ptAppData->bResize = false;
    }
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

    const uint32_t uCurrentFrameIndex = gptGfx->get_current_frame_index();

    static const float fCameraTravelSpeed = 4.0f;
    static const float fCameraRotationSpeed = 0.005f;

    plCamera* ptCamera = &ptAppData->tCamera;

    // camera space
    if(gptIO->is_key_down(PL_KEY_W)) camera_translate(ptCamera,  0.0f,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime);
    if(gptIO->is_key_down(PL_KEY_S)) camera_translate(ptCamera,  0.0f,  0.0f, -fCameraTravelSpeed* ptIO->fDeltaTime);
    if(gptIO->is_key_down(PL_KEY_A)) camera_translate(ptCamera, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);
    if(gptIO->is_key_down(PL_KEY_D)) camera_translate(ptCamera,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);

    // world space
    if(gptIO->is_key_down(PL_KEY_F)) { camera_translate(ptCamera,  0.0f, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f); }
    if(gptIO->is_key_down(PL_KEY_R)) { camera_translate(ptCamera,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f); }

    if(gptIO->is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 1.0f))
    {
        const plVec2 tMouseDelta = gptIO->get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f);
        camera_rotate(ptCamera,  -tMouseDelta.y * fCameraRotationSpeed,  -tMouseDelta.x * fCameraRotationSpeed);
        gptIO->reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
    }
    camera_update(ptCamera);

    // add full screen quad for offscreen render
    gptDraw->add_image(ptAppData->ptFGLayer, ptAppData->atColorTextureBg[uCurrentFrameIndex].uData, (plVec2){0}, ptIO->tMainViewportSize);

    // 3d drawing API usage
    const plMat4 tOrigin = pl_identity_mat4();
    gptDraw->add_3d_transform(ptAppData->pt3dDrawlist, &tOrigin, 10.0f, (plDrawLineOptions){.fThickness = 0.2f});

    plDrawCylinderDesc tCylinderDesc = {
        .fRadius = 1.5f,
        .tBasePos = {-2.5f, 1.0f, 0.0f},
        .tTipPos  = {-2.5f, 4.0f, 0.0f},
        .uSegments = 12
    };
    gptDraw->add_3d_cylinder_filled(ptAppData->pt3dDrawlist, tCylinderDesc, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 1.0f, 0.0f, 0.75f)});

    gptDraw->add_3d_triangle_filled(ptAppData->pt3dDrawlist,
        (plVec3){1.0f, 1.0f, 0.0f},
        (plVec3){4.0f, 1.0f, 0.0f},
        (plVec3){1.0f, 4.0f, 0.0f},
        (plDrawSolidOptions){.uColor = PL_COLOR_32_YELLOW});

    gptDraw->add_3d_sphere_filled(ptAppData->pt3dDrawlist,
        (plDrawSphereDesc){.fRadius = 1.0F, .tCenter = {5.5f, 2.5f, 0.0f}}, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 1.0f, 0.0f, 0.75f)});

    gptDraw->add_3d_circle_xz_filled(ptAppData->pt3dDrawlist,
        (plVec3){8.5f, 2.5f, 0.0f}, 1.5f, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 1.0f, 0.0f, 0.75f)});

    gptDraw->add_3d_band_xz_filled(ptAppData->pt3dDrawlist, (plVec3){11.5f, 2.5f, 0.0f}, 0.75f, 1.5f, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 1.0f, 0.0f, 0.75f)});
    gptDraw->add_3d_band_xy_filled(ptAppData->pt3dDrawlist, (plVec3){11.5f, 2.5f, 0.0f}, 0.75f, 1.5f, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 0.0f, 0.0f, 0.75f)});
    gptDraw->add_3d_band_yz_filled(ptAppData->pt3dDrawlist, (plVec3){11.5f, 2.5f, 0.0f}, 0.75f, 1.5f, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 0.0f, 1.0f, 0.75f)});

    // gptDraw->add_3d_triangle_filled(ptAppData->pt3dDrawlist,
    //     (plVec3){1.0f, 1.0f, 3.0f},
    //     (plVec3){4.0f, 1.0f, 3.0f},
    //     (plVec3){1.0f, 4.0f, 3.0f},
    //     (plVec4){1.0f, 0.5f, 0.3f, 0.75f});

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~drawing prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // submit our draw layers
    gptDraw->submit_2d_layer(ptAppData->ptFGLayer);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~command buffer 0~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // expected timeline semaphore values
    uint64_t ulValue0 = ptAppData->aulNextTimelineValue[uCurrentFrameIndex];
    uint64_t ulValue1 = ulValue0 + 1;
    uint64_t ulValue2 = ulValue0 + 2;
    ptAppData->aulNextTimelineValue[uCurrentFrameIndex] = ulValue2;

    const plBeginCommandInfo tBeginInfo0 = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {ptAppData->aptSemaphores[uCurrentFrameIndex]},
        .auWaitSemaphoreValues = {ulValue0},
    };
    gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo0);

    // begin offscreen renderpass
    plRenderEncoder* ptEncoder0 = gptGfx->begin_render_pass(ptCommandBuffer, ptAppData->tOffscreenRenderPass, NULL);

    const plMat4 tMVP = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);
    gptDrawBackend->submit_3d_drawlist(ptAppData->pt3dDrawlist,
        ptEncoder0,
        ptAppData->tOffscreenSize.x,
        ptAppData->tOffscreenSize.y,
        &tMVP,
        PL_DRAW_FLAG_DEPTH_TEST | PL_DRAW_FLAG_DEPTH_WRITE, 1);

    // end offscreen render pass
    gptGfx->end_render_pass(ptEncoder0);

    // end recording
    gptGfx->end_command_recording(ptCommandBuffer);

    const plSubmitInfo tSubmitInfo0 = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {ptAppData->aptSemaphores[uCurrentFrameIndex]},
        .auSignalSemaphoreValues = {ulValue1},
    };
    gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo0);
    
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~command buffer 1~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const plBeginCommandInfo tBeginInfo1 = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {ptAppData->aptSemaphores[uCurrentFrameIndex]},
        .auWaitSemaphoreValues = {ulValue1},
    };
    gptGfx->reset_command_buffer(ptCommandBuffer);
    gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo1);

    // begin main renderpass (directly to swapchain)
    plRenderEncoder* ptEncoder1 = gptGfx->begin_render_pass(ptCommandBuffer, ptAppData->tMainRenderPass, NULL);

    // submit drawlists
    gptDrawBackend->submit_2d_drawlist(ptAppData->ptAppDrawlist, ptEncoder1, ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y, 1);

    // end render pass
    gptGfx->end_render_pass(ptEncoder1);

    // end recording
    gptGfx->end_command_recording(ptCommandBuffer);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~submit work to GPU & present~~~~~~~~~~~~~~~~~~~~~~~

    const plSubmitInfo tSubmitInfo1 = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {ptAppData->aptSemaphores[uCurrentFrameIndex]},
        .auSignalSemaphoreValues = {ulValue2},
    };

    if(!gptGfx->present(ptCommandBuffer, &tSubmitInfo1, &ptAppData->ptSwapchain, 1))
        pl_app_resize(ptAppData);

    gptGfx->return_command_buffer(ptCommandBuffer);
    gptProfile->end_frame();
}

//-----------------------------------------------------------------------------
// [SECTION] helper function declarations
//-----------------------------------------------------------------------------

static inline float
wrap_angle(float tTheta)
{
    static const float f2Pi = 2.0f * PL_PI;
    const float fMod = fmodf(tTheta, f2Pi);
    if (fMod > PL_PI)       return fMod - f2Pi;
    else if (fMod < -PL_PI) return fMod + f2Pi;
    return fMod;
}

void
camera_translate(plCamera* ptCamera, float fDx, float fDy, float fDz)
{
    ptCamera->tPos = pl_add_vec3(ptCamera->tPos, pl_mul_vec3_scalarf(ptCamera->_tRightVec, fDx));
    ptCamera->tPos = pl_add_vec3(ptCamera->tPos, pl_mul_vec3_scalarf(ptCamera->_tForwardVec, fDz));
    ptCamera->tPos.y += fDy;
}

void
camera_rotate(plCamera* ptCamera, float fDPitch, float fDYaw)
{
    ptCamera->fPitch += fDPitch;
    ptCamera->fYaw += fDYaw;

    ptCamera->fYaw = wrap_angle(ptCamera->fYaw);
    ptCamera->fPitch = pl_clampf(0.995f * -PL_PI_2, ptCamera->fPitch, 0.995f * PL_PI_2);
}

void
camera_update(plCamera* ptCamera)
{
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~update view~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // world space
    static const plVec4 tOriginalUpVec      = {0.0f, 1.0f, 0.0f, 0.0f};
    static const plVec4 tOriginalForwardVec = {0.0f, 0.0f, 1.0f, 0.0f};
    static const plVec4 tOriginalRightVec   = {-1.0f, 0.0f, 0.0f, 0.0f};

    const plMat4 tXRotMat   = pl_mat4_rotate_vec3(ptCamera->fPitch, tOriginalRightVec.xyz);
    const plMat4 tYRotMat   = pl_mat4_rotate_vec3(ptCamera->fYaw, tOriginalUpVec.xyz);
    const plMat4 tZRotMat   = pl_mat4_rotate_vec3(ptCamera->fRoll, tOriginalForwardVec.xyz);
    const plMat4 tTranslate = pl_mat4_translate_vec3((plVec3){ptCamera->tPos.x, ptCamera->tPos.y, ptCamera->tPos.z});

    // rotations: rotY * rotX * rotZ
    plMat4 tRotations = pl_mul_mat4t(&tXRotMat, &tZRotMat);
    tRotations        = pl_mul_mat4t(&tYRotMat, &tRotations);

    // update camera vectors
    ptCamera->_tRightVec   = pl_norm_vec4(pl_mul_mat4_vec4(&tRotations, tOriginalRightVec)).xyz;
    ptCamera->_tUpVec      = pl_norm_vec4(pl_mul_mat4_vec4(&tRotations, tOriginalUpVec)).xyz;
    ptCamera->_tForwardVec = pl_norm_vec4(pl_mul_mat4_vec4(&tRotations, tOriginalForwardVec)).xyz;

    // update camera transform: translate * rotate
    ptCamera->tTransformMat = pl_mul_mat4t(&tTranslate, &tRotations);

    // update camera view matrix
    ptCamera->tViewMat   = pl_mat4t_invert(&ptCamera->tTransformMat);

    // flip x & y so camera looks down +z and remains right handed (+x to the right)
    const plMat4 tFlipXY = pl_mat4_scale_xyz(-1.0f, -1.0f, 1.0f);
    ptCamera->tViewMat   = pl_mul_mat4t(&tFlipXY, &ptCamera->tViewMat);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~update projection~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    const float fInvtanHalfFovy = 1.0f / tanf(ptCamera->fFieldOfView / 2.0f);
    ptCamera->tProjMat.col[0].x = fInvtanHalfFovy / ptCamera->fAspectRatio;
    ptCamera->tProjMat.col[1].y = fInvtanHalfFovy;
    ptCamera->tProjMat.col[2].z = ptCamera->fFarZ / (ptCamera->fFarZ - ptCamera->fNearZ);
    ptCamera->tProjMat.col[2].w = 1.0f;
    ptCamera->tProjMat.col[3].z = -ptCamera->fNearZ * ptCamera->fFarZ / (ptCamera->fFarZ - ptCamera->fNearZ);
    ptCamera->tProjMat.col[3].w = 0.0f;  
}

void
resize_offscreen_resources(plAppData* ptAppData)
{
    plDevice* ptDevice = ptAppData->ptDevice;

    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptAppData->atCmdPools[0]);
    gptGfx->begin_command_recording(ptCommandBuffer, NULL);

    // begin blit pass, copy buffer, end pass
    plBlitEncoder* ptEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
    gptGfx->pipeline_barrier_blit(ptEncoder, PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE | PL_SHADER_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_SHADER_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);

    plIO* ptIO = gptIO->get_io();
    ptAppData->tCamera.fAspectRatio = ptIO->tMainViewportSize.x / ptIO->tMainViewportSize.y;

    const plTextureDesc tColorTextureDesc = {
        .tDimensions   = {ptAppData->tOffscreenSize.x, ptAppData->tOffscreenSize.y, 1},
        .tFormat       = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
        .pcDebugName   = "offscreen color texture"
    };

    const plTextureDesc tDepthTextureDesc = {
        .tDimensions   = {ptAppData->tOffscreenSize.x, ptAppData->tOffscreenSize.y, 1},
        .tFormat       = PL_FORMAT_D32_FLOAT_S8_UINT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
        .pcDebugName   = "offscreen depth texture"
    };

    plRenderPassAttachments atAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        // queue old resources for deletion
        gptGfx->queue_texture_for_deletion(ptDevice, ptAppData->atColorTexture[i]);
        gptGfx->queue_texture_for_deletion(ptDevice, ptAppData->atDepthTexture[i]);

        // create new textures
        ptAppData->atColorTexture[i] = gptGfx->create_texture(ptDevice, &tColorTextureDesc, NULL);
        ptAppData->atDepthTexture[i] = gptGfx->create_texture(ptDevice, &tDepthTextureDesc, NULL);

        // retrieve textures
        plTexture* ptColorTexture = gptGfx->get_texture(ptDevice, ptAppData->atColorTexture[i]);
        plTexture* ptDepthTexture = gptGfx->get_texture(ptDevice, ptAppData->atDepthTexture[i]);

        const plDeviceMemoryAllocation tColorAllocation = gptGfx->allocate_memory(ptDevice, 
            ptColorTexture->tMemoryRequirements.ulSize,
            PL_MEMORY_GPU,
            ptColorTexture->tMemoryRequirements.uMemoryTypeBits,
            "color texture memory");

        const plDeviceMemoryAllocation tDepthAllocation = gptGfx->allocate_memory(ptDevice, 
            ptDepthTexture->tMemoryRequirements.ulSize,
            PL_MEMORY_GPU,
            ptDepthTexture->tMemoryRequirements.uMemoryTypeBits,
            "depth texture memory");

        // bind memory
        gptGfx->bind_texture_to_memory(ptDevice, ptAppData->atColorTexture[i], &tColorAllocation);
        gptGfx->bind_texture_to_memory(ptDevice, ptAppData->atDepthTexture[i], &tDepthAllocation);
        
        // set initialial usage
        gptGfx->set_texture_usage(ptEncoder, ptAppData->atColorTexture[i], PL_TEXTURE_USAGE_SAMPLED, 0);
        gptGfx->set_texture_usage(ptEncoder, ptAppData->atDepthTexture[i], PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT, 0);
        ptAppData->atColorTextureBg[i] = gptDrawBackend->create_bind_group_for_texture(ptAppData->atColorTexture[i]);

        // add textures to attachment set for render pass
        atAttachmentSets[i].atViewAttachments[0] = ptAppData->atDepthTexture[i];
        atAttachmentSets[i].atViewAttachments[1] = ptAppData->atColorTexture[i];
    }

    gptGfx->pipeline_barrier_blit(ptEncoder, PL_SHADER_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_COMPUTE | PL_SHADER_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
    gptGfx->end_blit_pass(ptEncoder);

    // finish recording
    gptGfx->end_command_recording(ptCommandBuffer);

    // submit command buffer
    gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);
    gptGfx->return_command_buffer(ptCommandBuffer);

    gptGfx->update_render_pass_attachments(ptDevice, ptAppData->tOffscreenRenderPass, ptAppData->tOffscreenSize, atAttachmentSets);
}
