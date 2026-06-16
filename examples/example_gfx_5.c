/*
     - demonstrates loading APIs
     - demonstrates loading extensions
     - demonstrates hot reloading
     - demonstrates starter extension
     - demonstrates camera extension
     - demonstrates drawing extension (2D & 3D)
     - demonstrates render passes

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
#include "pl_graphics_ext.h"
#include "pl_draw_ext.h"
#include "pl_starter_ext.h"
#include "pl_camera_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAppData
{
    // window
    plWindow* ptWindow;

    // drawing
    plDrawList2D*  ptAppDrawlist;
    plDrawLayer2D* ptFGLayer;

    // 3d drawing
    plCamera      tCamera;
    plDrawList3D* pt3dDrawlist;

    // offscreen rendering
    bool               bResize;
    plSamplerHandle    tDefaultSampler;
    // plRenderPassHandle tOffscreenRenderPass;
    plVec2             tOffscreenSize;
    plTextureHandle    tColorTexture;
    plBindGroupHandle  tColorTextureBg;
    plTextureHandle    tDepthTexture;

} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

const plIOI*          gptIO          = NULL;
const plWindowI*      gptWindows     = NULL;
const plGraphicsI*    gptGfx         = NULL;
const plDrawI*        gptDraw        = NULL;
const plStarterI*     gptStarter     = NULL;
const plCameraI*      gptCamera      = NULL;

//-----------------------------------------------------------------------------
// [SECTION] helper function declarations
//-----------------------------------------------------------------------------

void resize_offscreen_resources(plAppData* ptAppData);

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
        gptStarter     = pl_get_api_latest(ptApiRegistry, plStarterI);
        gptCamera      = pl_get_api_latest(ptApiRegistry, plCameraI);

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
    ptExtensionRegistry->load("pl_platform_ext", "pl_load_platform_ext", "pl_unload_platform_ext", false);
    
    // load required apis
    gptIO      = pl_get_api_latest(ptApiRegistry, plIOI);
    gptWindows = pl_get_api_latest(ptApiRegistry, plWindowI);

    // load required apis (these are provided though extensions)
    gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptDraw        = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptStarter     = pl_get_api_latest(ptApiRegistry, plStarterI);
    gptCamera      = pl_get_api_latest(ptApiRegistry, plCameraI);

    // use window API to create a window
    plWindowDesc tWindowDesc = {
        .pcTitle = "Example GFX 5",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 600,
        .uHeight = 600,
    };
    gptWindows->create(tWindowDesc, &ptAppData->ptWindow);
    gptWindows->show(ptAppData->ptWindow);
    ptAppData->tOffscreenSize = (plVec2){600.0f, 600.0f};

    // setup starter extension
    plStarterInit tStarterInit = {
        .eFlags   = PL_STARTER_FLAGS_ALL_EXTENSIONS,
        .ptWindow = ptAppData->ptWindow
    };

    // from a graphics standpoint, the starter extension is handling device, swapchain, renderpass
    // etc. which we will get to in later examples
    gptStarter->initialize(tStarterInit);

    // give starter extension chance to do its work now that we
    // setup the shader extension
    gptStarter->finalize();

    // request drawlist to render full screen quad
    ptAppData->ptAppDrawlist = gptDraw->request_2d_drawlist();
    ptAppData->ptFGLayer     = gptDraw->request_2d_layer(ptAppData->ptAppDrawlist);

    // request 3D drawlist
    ptAppData->pt3dDrawlist  = gptDraw->request_3d_drawlist();

    // for convience
    plDevice* ptDevice = gptStarter->get_device();

    // create camera
    gptCamera->init(&ptAppData->tCamera);
    plCameraPerspectiveDesc tCameraDesc = {
        .fNearZ       = 0.01f,
        .fFarZ        = 50.0f,
        .fYFov        = PL_PI_3,
        .fAspectRatio = 1.0f,
        .eDepthMode   = PL_CAMERA_DEPTH_MODE_STANDARD
    };
    gptCamera->set_perspective(&ptAppData->tCamera, &tCameraDesc);
    gptCamera->set_position(&ptAppData->tCamera, (plVec3d){5.0, 10.0, 10.0});
    gptCamera->set_euler(&ptAppData->tCamera, -PL_PI_4, PL_PI + PL_PI_4, 0.0f);
    gptCamera->update(&ptAppData->tCamera);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~samplers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // create default sampler
    const plSamplerDesc tSamplerDesc = {
        .eMagFilter      = PL_FILTER_LINEAR,
        .eMinFilter      = PL_FILTER_LINEAR,
        .fMinMip         = 0.0f,
        .fMaxMip         = 64.0f,
        .eVAddressMode   = PL_ADDRESS_MODE_WRAP,
        .eUAddressMode   = PL_ADDRESS_MODE_WRAP,
        .pcDebugName     = "default sampler"
    };
    ptAppData->tDefaultSampler = gptGfx->create_sampler(ptDevice, &tSamplerDesc);

    // create offscreen per-frame resources

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~textures~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const plTextureDesc tColorTextureDesc = {
        .tDimensions   = {ptAppData->tOffscreenSize.x, ptAppData->tOffscreenSize.y, 1},
        .eFormat       = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .eType         = PL_TEXTURE_TYPE_2D,
        .eUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
        .pcDebugName   = "offscreen color texture"
    };

    const plTextureDesc tDepthTextureDesc = {
        .tDimensions   = {ptAppData->tOffscreenSize.x, ptAppData->tOffscreenSize.y, 1},
        .eFormat       = PL_FORMAT_D32_FLOAT_S8_UINT,
        .uLayers       = 1,
        .uMips         = 1,
        .eType         = PL_TEXTURE_TYPE_2D,
        .eUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
        .pcDebugName   = "offscreen depth texture"
    };

    // create textures
    ptAppData->tColorTexture = gptGfx->create_texture(ptDevice, &tColorTextureDesc, NULL);
    ptAppData->tDepthTexture = gptGfx->create_texture(ptDevice, &tDepthTextureDesc, NULL);

    // retrieve textures (we aren't using the out params above since the second allocation
    //                    could invalidate the first)
    plTexture* ptColorTexture = gptGfx->get_texture(ptDevice, ptAppData->tColorTexture);
    plTexture* ptDepthTexture = gptGfx->get_texture(ptDevice, ptAppData->tDepthTexture);

    // allocate memory
    const plDeviceMemoryAllocation tColorAllocation = gptGfx->allocate_memory(ptDevice, 
        ptColorTexture->tMemoryRequirements.ulSize,
        PL_MEMORY_FLAGS_DEVICE_LOCAL,
        ptColorTexture->tMemoryRequirements.uMemoryTypeBits,
        "color texture memory");

    const plDeviceMemoryAllocation tDepthAllocation = gptGfx->allocate_memory(ptDevice, 
        ptDepthTexture->tMemoryRequirements.ulSize,
        PL_MEMORY_FLAGS_DEVICE_LOCAL,
        ptDepthTexture->tMemoryRequirements.uMemoryTypeBits,
        "depth texture memory");

    // bind memory
    gptGfx->bind_texture_to_memory(ptDevice, ptAppData->tColorTexture, &tColorAllocation);
    gptGfx->bind_texture_to_memory(ptDevice, ptAppData->tDepthTexture, &tDepthAllocation);

    // using a helper from the draw backend extension to allocate
    // a bind group for use to use for drawing
    ptAppData->tColorTextureBg = gptDraw->create_bind_group_for_texture(ptAppData->tColorTexture);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~render pass stuff~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // NOTE: even if you don't need to double buffer a texture, the number of
    //       render pass attachment sets must equal the frames in flight
    //       (backend implementation details require this currently)
    //
    // NOTE: Render passes directly map to render passes in Vulkan. In Metal
    //       we emulate them by places appropriate barriers & fences.

    // return app memory
    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    plDevice* ptDevice = gptStarter->get_device();

    // ensure the GPU is done with our resources
    gptGfx->flush_device(ptDevice);

    // cleanup our resources
    gptGfx->destroy_texture(ptDevice, ptAppData->tColorTexture);
    gptGfx->destroy_texture(ptDevice, ptAppData->tDepthTexture);

    gptStarter->cleanup();
    gptWindows->destroy(ptAppData->ptWindow);
    free(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plWindow* ptWindow, plAppData* ptAppData)
{
    gptStarter->resize();

    // NOTE: you could recreate the offscreen resources here but we prefer
    //       not to do that since you could get several resize calls before
    //       you actually get a chance to render again, so we defer until the
    //       update.
    ptAppData->bResize = true;

    plIO* ptIO = gptIO->get_io();
    gptCamera->set_viewport(&ptAppData->tCamera, ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    if(!gptStarter->begin_frame())
        return;

    // for convience
    plIO* ptIO = gptIO->get_io();

    if(ptAppData->bResize)
    {
        ptAppData->tOffscreenSize.x = ptIO->tMainViewportSize.x;
        ptAppData->tOffscreenSize.y = ptIO->tMainViewportSize.y;
        resize_offscreen_resources(ptAppData);
        ptAppData->bResize = false;
    }

    static const float fCameraTravelSpeed = 4.0f;
    static const float fCameraRotationSpeed = 0.005f;

    plCamera* ptCamera = &ptAppData->tCamera;

    // camera space
    if(gptIO->is_key_down(PL_KEY_W)) gptCamera->translate_local(ptCamera, (plVec3d){ 0.0f,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime});
    if(gptIO->is_key_down(PL_KEY_S)) gptCamera->translate_local(ptCamera, (plVec3d){ 0.0f,  0.0f, -fCameraTravelSpeed* ptIO->fDeltaTime});
    if(gptIO->is_key_down(PL_KEY_A)) gptCamera->translate_local(ptCamera, (plVec3d){ fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f});
    if(gptIO->is_key_down(PL_KEY_D)) gptCamera->translate_local(ptCamera, (plVec3d){-fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f});

    // world space
    if(gptIO->is_key_down(PL_KEY_F)) { gptCamera->translate(ptCamera, (plVec3d){ 0.0f, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f }); }
    if(gptIO->is_key_down(PL_KEY_R)) { gptCamera->translate(ptCamera, (plVec3d){ 0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f }); }

    if(gptIO->is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 1.0f))
    {
        const plVec2 tMouseDelta = gptIO->get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f);
        gptCamera->rotate_euler(ptCamera,  -tMouseDelta.y * fCameraRotationSpeed,  -tMouseDelta.x * fCameraRotationSpeed, 0.0f);
        gptIO->reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
    }
    gptCamera->update(ptCamera);

    // 3d drawing API usage
    const plMat4 tOrigin = pl_identity_mat4();
    gptDraw->add_3d_transform(ptAppData->pt3dDrawlist, &tOrigin, 10.0f, (plDrawLineOptions){.fThickness = 0.2f});

    plCylinder tCylinderDesc = {
        .fRadius = 1.5f,
        .tBasePos = {-2.5f, 1.0f, 0.0f},
        .tTipPos  = {-2.5f, 4.0f, 0.0f}
    };
    gptDraw->add_3d_cylinder_filled(ptAppData->pt3dDrawlist, tCylinderDesc, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 1.0f, 0.0f, 0.75f)});

    gptDraw->add_3d_triangle_filled(ptAppData->pt3dDrawlist,
        (plVec3){1.0f, 1.0f, 0.0f},
        (plVec3){4.0f, 1.0f, 0.0f},
        (plVec3){1.0f, 4.0f, 0.0f},
        (plDrawSolidOptions){.uColor = PL_COLOR_32_YELLOW});

    gptDraw->add_3d_sphere_filled(ptAppData->pt3dDrawlist,
        (plSphere){.fRadius = 1.0F, .tCenter = {5.5f, 2.5f, 0.0f}}, 0, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 1.0f, 0.0f, 0.75f)});

    gptDraw->add_3d_circle_xz_filled(ptAppData->pt3dDrawlist,
        (plVec3){8.5f, 2.5f, 0.0f}, 1.5f, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 1.0f, 0.0f, 0.75f)});

    gptDraw->add_3d_band_xz_filled(ptAppData->pt3dDrawlist, (plVec3){11.5f, 2.5f, 0.0f}, 0.75f, 1.5f, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 1.0f, 0.0f, 0.75f)});
    gptDraw->add_3d_band_xy_filled(ptAppData->pt3dDrawlist, (plVec3){11.5f, 2.5f, 0.0f}, 0.75f, 1.5f, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 0.0f, 0.0f, 0.75f)});
    gptDraw->add_3d_band_yz_filled(ptAppData->pt3dDrawlist, (plVec3){11.5f, 2.5f, 0.0f}, 0.75f, 1.5f, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 0.0f, 1.0f, 0.75f)});

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~drawing prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // submit our draw layers
    gptDraw->submit_2d_layer(ptAppData->ptFGLayer);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~offscreen~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // NOTE: we are letting the starter extension handle command buffer management
    //       and coarse syncronization (utilizing timeline semaphores to ensure
    //       previous submissions are finished and this work finishes before the main
    //       pass below)

    // retrieve command buffer (already in recording state)
    plFormat tFormat = PL_FORMAT_R32G32B32A32_FLOAT;
    plCommandBuffer* ptCommandBuffer = NULL;
    #if 1
    ptCommandBuffer = gptStarter->get_command_buffer();

    plRenderInfo tRenderInfo = {
        .tRenderArea = {
            .tMin = {0},
            .tMax = ptAppData->tOffscreenSize
        },
        .atColorAttachments = {
            {
                .tTexture       = ptAppData->tColorTexture,
                .eLoadOp        = PL_LOAD_OP_CLEAR,
                .eStoreOp       = PL_STORE_OP_STORE,
                .eUsage         = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .tClearColor    = {0.0f, 0.0f, 0.0f, 1.0f}
            }
        },
        .tDepthAttachment = {
            .tTexture        = ptAppData->tDepthTexture,
            .eLoadOp         = PL_LOAD_OP_CLEAR,
            .eStoreOp        = PL_STORE_OP_DONT_CARE,
            .eUsage          = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
            .fClearZ         = 1.0f
        },
        .tStencilAttachment = {
            .tTexture        = ptAppData->tDepthTexture,
            .eLoadOp         = PL_LOAD_OP_CLEAR,
            .eStoreOp        = PL_STORE_OP_DONT_CARE,
            .eUsage          = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
            .uClearStencil   = 0
        }
    };

    // begin offscreen renderpass
    gptGfx->begin_render_pass(ptCommandBuffer, &tRenderInfo, NULL);

    // submit our 3D drawlist

    plRenderAttachmentInfo tInfo = {
        .aeColorFormats = {
            PL_FORMAT_R32G32B32A32_FLOAT
        },
        .eDepthFormat = PL_FORMAT_D32_FLOAT_S8_UINT,
        .eStencilFormat = PL_FORMAT_D32_FLOAT_S8_UINT
    };
    
    gptDraw->submit_3d_drawlist(ptAppData->pt3dDrawlist,
        ptCommandBuffer,
        ptAppData->tOffscreenSize.x,
        ptAppData->tOffscreenSize.y,
        &ptAppData->tCamera.tViewProjMat,
        PL_DRAW_FLAG_DEPTH_TEST | PL_DRAW_FLAG_DEPTH_WRITE, 1, &tInfo);

    // end offscreen render pass
    gptGfx->end_render_pass(ptCommandBuffer);

    // submit and return our command buffer
    gptStarter->submit_command_buffer(ptCommandBuffer);
    #endif

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~main~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // add full screen quad for offscreen render
    gptDraw->add_image(ptAppData->ptFGLayer, ptAppData->tColorTextureBg.uData, (plVec2){0}, ptIO->tMainViewportSize);

    // start main pass & return the command buffer being used
    ptCommandBuffer = gptStarter->begin_main_pass();

    // submit drawlists
    plRenderAttachmentInfo tRenderAttachmentInfo = {0};
    gptStarter->get_render_attachment_info(&tRenderAttachmentInfo);
    gptDraw->submit_2d_drawlist(ptAppData->ptAppDrawlist, ptCommandBuffer, ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y, 1, &tRenderAttachmentInfo);

    // allows the starter extension to handle some things then ends the main pass
    gptStarter->end_main_pass();

    // must be the last function called when using the starter extension
    gptStarter->end_frame(); 
}

//-----------------------------------------------------------------------------
// [SECTION] helper function declarations
//-----------------------------------------------------------------------------

void
resize_offscreen_resources(plAppData* ptAppData)
{
    plDevice* ptDevice = gptStarter->get_device();

    const plTextureDesc tColorTextureDesc = {
        .tDimensions   = {ptAppData->tOffscreenSize.x, ptAppData->tOffscreenSize.y, 1},
        .eFormat       = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .eType         = PL_TEXTURE_TYPE_2D,
        .eUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
        .pcDebugName   = "offscreen color texture"
    };

    const plTextureDesc tDepthTextureDesc = {
        .tDimensions   = {ptAppData->tOffscreenSize.x, ptAppData->tOffscreenSize.y, 1},
        .eFormat       = PL_FORMAT_D32_FLOAT_S8_UINT,
        .uLayers       = 1,
        .uMips         = 1,
        .eType         = PL_TEXTURE_TYPE_2D,
        .eUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
        .pcDebugName   = "offscreen depth texture"
    };

    // queue old resources for deletion
    gptGfx->queue_texture_for_deletion(ptDevice, ptAppData->tColorTexture);
    gptGfx->queue_texture_for_deletion(ptDevice, ptAppData->tDepthTexture);

    // create new textures
    ptAppData->tColorTexture = gptGfx->create_texture(ptDevice, &tColorTextureDesc, NULL);
    ptAppData->tDepthTexture = gptGfx->create_texture(ptDevice, &tDepthTextureDesc, NULL);

    // retrieve textures
    plTexture* ptColorTexture = gptGfx->get_texture(ptDevice, ptAppData->tColorTexture);
    plTexture* ptDepthTexture = gptGfx->get_texture(ptDevice, ptAppData->tDepthTexture);

    const plDeviceMemoryAllocation tColorAllocation = gptGfx->allocate_memory(ptDevice, 
        ptColorTexture->tMemoryRequirements.ulSize,
        PL_MEMORY_FLAGS_DEVICE_LOCAL,
        ptColorTexture->tMemoryRequirements.uMemoryTypeBits,
        "color texture memory");

    const plDeviceMemoryAllocation tDepthAllocation = gptGfx->allocate_memory(ptDevice, 
        ptDepthTexture->tMemoryRequirements.ulSize,
        PL_MEMORY_FLAGS_DEVICE_LOCAL,
        ptDepthTexture->tMemoryRequirements.uMemoryTypeBits,
        "depth texture memory");

    // bind memory
    gptGfx->bind_texture_to_memory(ptDevice, ptAppData->tColorTexture, &tColorAllocation);
    gptGfx->bind_texture_to_memory(ptDevice, ptAppData->tDepthTexture, &tDepthAllocation);
    

    // update our previous allocated bind group
    const plBindGroupUpdateData tBGData = {
        .atTextureBindings = {
            { .tTexture = ptAppData->tColorTexture, .uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
        }
    };
    gptGfx->update_bind_group(ptDevice, ptAppData->tColorTextureBg, &tBGData);
}
