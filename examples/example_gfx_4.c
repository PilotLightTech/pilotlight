/*
   example_gfx_4.c
     - demonstrates loading APIs
     - demonstrates loading extensions
     - demonstrates hot reloading
     - demonstrates starter extension
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
#include "pl_window_ext.h"
#include "pl_graphics_ext.h"
#include "pl_draw_ext.h"
#include "pl_draw_backend_ext.h"
#include "pl_starter_ext.h"

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

    // drawing
    plDrawList2D*  ptAppDrawlist;
    plDrawLayer2D* ptFGLayer;

    // 3d drawing
    plCamera      tCamera;
    plDrawList3D* pt3dDrawlist;

    // offscreen rendering
    bool               bResize;
    plSamplerHandle    tDefaultSampler;
    plRenderPassHandle tOffscreenRenderPass;
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
const plDrawBackendI* gptDrawBackend = NULL;
const plStarterI*     gptStarter     = NULL;

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
        gptDrawBackend = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
        gptStarter     = pl_get_api_latest(ptApiRegistry, plStarterI);

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
    ptExtensionRegistry->load("pl_platform_ext", NULL, NULL, false);
    
    // load required apis
    gptIO      = pl_get_api_latest(ptApiRegistry, plIOI);
    gptWindows = pl_get_api_latest(ptApiRegistry, plWindowI);

    // load required apis (these are provided though extensions)
    gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptDraw        = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptDrawBackend = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
    gptStarter     = pl_get_api_latest(ptApiRegistry, plStarterI);

    // use window API to create a window
    plWindowDesc tWindowDesc = {
        .pcTitle = "Example GFX 4",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 600,
        .uHeight = 600,
    };
    gptWindows->create_window(tWindowDesc, &ptAppData->ptWindow);
    ptAppData->tOffscreenSize = (plVec2){600.0f, 600.0f};

    // setup starter extension
    plStarterInit tStarterInit = {
        .tFlags   = PL_STARTER_FLAGS_ALL_EXTENSIONS,
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

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~samplers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~textures~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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
        PL_MEMORY_GPU,
        ptColorTexture->tMemoryRequirements.uMemoryTypeBits,
        "color texture memory");

    const plDeviceMemoryAllocation tDepthAllocation = gptGfx->allocate_memory(ptDevice, 
        ptDepthTexture->tMemoryRequirements.ulSize,
        PL_MEMORY_GPU,
        ptDepthTexture->tMemoryRequirements.uMemoryTypeBits,
        "depth texture memory");

    // bind memory
    gptGfx->bind_texture_to_memory(ptDevice, ptAppData->tColorTexture, &tColorAllocation);
    gptGfx->bind_texture_to_memory(ptDevice, ptAppData->tDepthTexture, &tDepthAllocation);

    // using a helper from the draw backend extension to allocate
    // a bind group for use to use for drawing
    ptAppData->tColorTextureBg = gptDrawBackend->create_bind_group_for_texture(ptAppData->tColorTexture);

    // begin blit pass, copy buffer, end pass
    plBlitEncoder* ptEncoder = gptStarter->get_blit_encoder();

    // set the initial texture usage (this is a no-op in metal but does layout transition for vulkan)
    gptGfx->set_texture_usage(ptEncoder, ptAppData->tColorTexture, PL_TEXTURE_USAGE_SAMPLED, 0);
    gptGfx->set_texture_usage(ptEncoder, ptAppData->tDepthTexture, PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT, 0);

    gptStarter->return_blit_encoder(ptEncoder);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~render pass stuff~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // NOTE: even if you don't need to double buffer a texture, the number of
    //       render pass attachment sets must equal the frames in flight
    //       (backend implementation details require this currently)
    //
    // NOTE: Render passes directly map to render passes in Vulkan. In Metal
    //       we emulate them by places appropriate barriers & fences.

    plRenderPassAttachments atAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        // add textures to attachment set for render pass
        atAttachmentSets[i].atViewAttachments[0] = ptAppData->tDepthTexture;
        atAttachmentSets[i].atViewAttachments[1] = ptAppData->tColorTexture;
    }

    // create offscreen renderpass layout
    const plRenderPassLayoutDesc tRenderPassLayoutDesc = {
        .atRenderTargets = {
            { .tFormat = PL_FORMAT_D32_FLOAT_S8_UINT, .bDepth = true }, // depth buffer
            { .tFormat = PL_FORMAT_R32G32B32A32_FLOAT } // color
        },
        .atSubpasses = {
            {
                .uRenderTargetCount = 2,
                .auRenderTargets = {0, 1}, // these are indices into the render targets above (depth/resolve must be before colors)
            },
        },
        .atSubpassDependencies = { // this map directly in Vulkan
            {
                .uSourceSubpass         = UINT32_MAX,
                .uDestinationSubpass    = 0,
                .tSourceStageMask       = PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS | PL_PIPELINE_STAGE_COMPUTE_SHADER,
                .tDestinationStageMask  = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tSourceAccessMask      = PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
                .tDestinationAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
            },
            {
                .uSourceSubpass         = 0,
                .uDestinationSubpass    = UINT32_MAX,
                .tSourceStageMask       = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tDestinationStageMask  = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS | PL_PIPELINE_STAGE_COMPUTE_SHADER,
                .tSourceAccessMask      = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
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
    ptAppData->tOffscreenRenderPass = gptGfx->create_render_pass(ptDevice, &tRenderPassDesc, atAttachmentSets);

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
    gptWindows->destroy_window(ptAppData->ptWindow);
    free(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plAppData* ptAppData)
{
    gptStarter->resize();

    // NOTE: you could recreate the offscreen resources here but we prefer
    //       not to do that since you could get several resize calls before
    //       you actually get a chance to render again, so we defer until the
    //       update.
    ptAppData->bResize = true;

    plIO* ptIO = gptIO->get_io();
    ptAppData->tCamera.fAspectRatio = ptIO->tMainViewportSize.x / ptIO->tMainViewportSize.y;
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
    plCommandBuffer* ptCommandBuffer = gptStarter->get_command_buffer();

    // begin offscreen renderpass
    plRenderEncoder* ptEncoder = gptGfx->begin_render_pass(ptCommandBuffer, ptAppData->tOffscreenRenderPass, NULL);

    const plMat4 tMVP = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);

    // submit our 3D drawlist
    gptDrawBackend->submit_3d_drawlist(ptAppData->pt3dDrawlist,
        ptEncoder,
        ptAppData->tOffscreenSize.x,
        ptAppData->tOffscreenSize.y,
        &tMVP,
        PL_DRAW_FLAG_DEPTH_TEST | PL_DRAW_FLAG_DEPTH_WRITE, 1);

    // end offscreen render pass
    gptGfx->end_render_pass(ptEncoder);

    // submit and return our command buffer
    gptStarter->submit_command_buffer(ptCommandBuffer);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~main~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // add full screen quad for offscreen render
    gptDraw->add_image(ptAppData->ptFGLayer, ptAppData->tColorTextureBg.uData, (plVec2){0}, ptIO->tMainViewportSize);

    // begin main renderpass (directly to swapchain)
    plRenderEncoder* ptMainEncoder = gptStarter->begin_main_pass();

    // submit drawlists
    gptDrawBackend->submit_2d_drawlist(ptAppData->ptAppDrawlist, ptMainEncoder, ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y, 1);

    // allows the starter extension to handle some things then ends the main pass
    gptStarter->end_main_pass();

    // must be the last function called when using the starter extension
    gptStarter->end_frame(); 
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
    plDevice* ptDevice = gptStarter->get_device();

    plBlitEncoder* ptEncoder = gptStarter->get_blit_encoder();

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
        PL_MEMORY_GPU,
        ptColorTexture->tMemoryRequirements.uMemoryTypeBits,
        "color texture memory");

    const plDeviceMemoryAllocation tDepthAllocation = gptGfx->allocate_memory(ptDevice, 
        ptDepthTexture->tMemoryRequirements.ulSize,
        PL_MEMORY_GPU,
        ptDepthTexture->tMemoryRequirements.uMemoryTypeBits,
        "depth texture memory");

    // bind memory
    gptGfx->bind_texture_to_memory(ptDevice, ptAppData->tColorTexture, &tColorAllocation);
    gptGfx->bind_texture_to_memory(ptDevice, ptAppData->tDepthTexture, &tDepthAllocation);
    
    // set initialial usage
    gptGfx->set_texture_usage(ptEncoder, ptAppData->tColorTexture, PL_TEXTURE_USAGE_SAMPLED, 0);
    gptGfx->set_texture_usage(ptEncoder, ptAppData->tDepthTexture, PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT, 0);

    // update our previous allocated bind group
    const plBindGroupUpdateTextureData atBGTextureData[] = {
        {
            .tTexture = ptAppData->tColorTexture,
            .uSlot    = 0,
            .tType    = PL_TEXTURE_BINDING_TYPE_SAMPLED
        }
    };
    const plBindGroupUpdateData tBGData = {
        .uTextureCount     = 1,
        .atTextureBindings = atBGTextureData
    };
    gptGfx->update_bind_group(ptDevice, ptAppData->tColorTextureBg, &tBGData);

    plRenderPassAttachments atAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        // add textures to attachment set for render pass
        atAttachmentSets[i].atViewAttachments[0] = ptAppData->tDepthTexture;
        atAttachmentSets[i].atViewAttachments[1] = ptAppData->tColorTexture;
    }

    gptStarter->return_blit_encoder(ptEncoder);

    // don't create new render pass, just update the attachments
    gptGfx->update_render_pass_attachments(ptDevice, ptAppData->tOffscreenRenderPass, ptAppData->tOffscreenSize, atAttachmentSets);
}
