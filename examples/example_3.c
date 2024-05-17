/*
   example_3.c
     - demonstrates loading APIs
     - demonstrates loading extensions
     - demonstrates hot reloading
     - demonstrates UI library
     - demonstrates 3d debug drawing extension
     - demonstrates minimal use of graphics extension

    Notes:
     - We are performing offscreen rendering for this example because
       the graphics extension does not current expose renderpass options
       and does not have a depth buffer by default.
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
#include "pilotlight.h"
#include "pl_profile.h"
#include "pl_log.h"
#include "pl_ds.h"
#include "pl_os.h"
#include "pl_memory.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"
#include "pl_ui.h"

// extensions
#include "pl_graphics_ext.h"
#include "pl_draw_3d_ext.h"
#include "pl_gpu_allocators_ext.h"

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
    plFontAtlas  tFontAtlas;
    plDrawList   tAppDrawlist;
    plDrawLayer* ptFGLayer;

    // 3d drawing
    plCamera      tCamera;
    plDrawList3D* pt3dDrawlist;

    // graphics & sync objects
    plGraphics        tGraphics;
    plSemaphoreHandle atSempahore[PL_FRAMES_IN_FLIGHT];
    uint64_t          aulNextTimelineValue[PL_FRAMES_IN_FLIGHT];

    // offscreen rendering
    bool               bResize;
    plSamplerHandle    tDefaultSampler;
    plRenderPassHandle tOffscreenRenderPass;
    plVec2             tOffscreenSize;
    plTextureId        atColorTextureId[PL_FRAMES_IN_FLIGHT]; // used by UI to draw
    plTextureHandle    atColorTexture[PL_FRAMES_IN_FLIGHT];
    plTextureHandle    atDepthTexture[PL_FRAMES_IN_FLIGHT];

} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

const plWindowI*        gptWindows       = NULL;
const plGraphicsI*      gptGfx           = NULL;
const plDeviceI*        gptDevice        = NULL;
const plDraw3dI*        gptDraw3d        = NULL;
const plGPUAllocatorsI* gptGpuAllocators = NULL;

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
    const plDataRegistryI* ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);

    // retrieve the UI context (provided by the runtime) and
    // set it (required to use plIO for "talking" with runtime & keyboard/mouse input)
    pl_set_context(ptDataRegistry->get_data("ui"));

    // retrieve the memory context (provided by the runtime) and
    // set it to allow for memory tracking when using PL_ALLOC/PL_FREE
    pl_set_memory_context(ptDataRegistry->get_data(PL_CONTEXT_MEMORY));

    // if "ptAppData" is a valid pointer, then this function is being called
    // during a hot reload.
    if(ptAppData)
    {
        // set contexts again since we are now in a
        // differenct dll/so
        pl_set_log_context(ptDataRegistry->get_data("log"));
        pl_set_profile_context(ptDataRegistry->get_data("profile"));

        // re-retrieve the apis since we are now in
        // a different dll/so
        gptWindows       = ptApiRegistry->first(PL_API_WINDOW);
        gptGfx           = ptApiRegistry->first(PL_API_GRAPHICS);
        gptDevice        = ptApiRegistry->first(PL_API_DEVICE);
        gptDraw3d        = ptApiRegistry->first(PL_API_DRAW_3D);
        gptGpuAllocators = ptApiRegistry->first(PL_API_GPU_ALLOCATORS);

        return ptAppData;
    }

    // this path is taken only during first load, so we
    // allocate app memory here (using PL_ALLOC for memory tracking)
    ptAppData = PL_ALLOC(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    // create profiling & logging contexts (used by extension here)
    plProfileContext* ptProfileCtx = pl_create_profile_context();
    plLogContext*     ptLogCtx     = pl_create_log_context();

    // add log channel (ignoring the return here)
    pl_add_log_channel("Default", PL_CHANNEL_TYPE_CONSOLE);
    pl_log_info("Setup logging");
    
    // add these to data registry so they can be retrieved by extension
    // and subsequent app reloads
    ptDataRegistry->set_data("profile", ptProfileCtx);
    ptDataRegistry->set_data("log", ptLogCtx);

    // retrieve extension registry
    const plExtensionRegistryI* ptExtensionRegistry = ptApiRegistry->first(PL_API_EXTENSION_REGISTRY);

    // load extensions (makes their APIs available)
    ptExtensionRegistry->load("pl_graphics_ext",       NULL, NULL, false);
    ptExtensionRegistry->load("pl_gpu_allocators_ext", NULL, NULL, false);
    ptExtensionRegistry->load("pl_draw_3d_ext",        NULL, NULL, true);
    
    // load required apis (NULL if not available)
    gptWindows       = ptApiRegistry->first(PL_API_WINDOW);
    gptGfx           = ptApiRegistry->first(PL_API_GRAPHICS);
    gptDevice        = ptApiRegistry->first(PL_API_DEVICE);
    gptDraw3d        = ptApiRegistry->first(PL_API_DRAW_3D);
    gptGpuAllocators = ptApiRegistry->first(PL_API_GPU_ALLOCATORS);

    // use window API to create a window
    const plWindowDesc tWindowDesc = {
        .pcName  = "Example 3",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 600,
        .uHeight = 600,
    };
    ptAppData->ptWindow = gptWindows->create_window(&tWindowDesc);
    ptAppData->tOffscreenSize = (plVec2){600.0f, 600.0f};

    // initialize graphics system
    const plGraphicsDesc tGraphicsDesc = {
        .bEnableValidation = true
    };
    gptGfx->initialize(ptAppData->ptWindow, &tGraphicsDesc, &ptAppData->tGraphics);

    // for convience
    plGraphics* ptGraphics = &ptAppData->tGraphics;
    plDevice* ptDevice = &ptGraphics->tDevice;

    // initialize 3D drawing system
    gptDraw3d->initialize(ptGraphics);
    ptAppData->pt3dDrawlist = gptDraw3d->request_drawlist();
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

    // setup ui
    pl_add_default_font(&ptAppData->tFontAtlas); // Proggy.ttf w/ 13 pt
    pl_build_font_atlas(&ptAppData->tFontAtlas); // generates font atlas data
    gptGfx->setup_ui(ptGraphics, ptAppData->tGraphics.tMainRenderPass); // prepares any graphics backend specifics
    gptGfx->create_font_atlas(&ptAppData->tFontAtlas); // creates font atlas texture
    pl_set_default_font(&ptAppData->tFontAtlas.sbtFonts[0]); // sets default font to use for UI rendering

    // register our app drawlist
    pl_register_drawlist(&ptAppData->tAppDrawlist);
    ptAppData->ptFGLayer = pl_request_layer(&ptAppData->tAppDrawlist, "foreground layer");

    // create timeline semaphores to syncronize GPU work submission
    for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
        ptAppData->atSempahore[i] = gptDevice->create_semaphore(ptDevice, false);

    // create default sampler
    const plSamplerDesc tSamplerDesc = {
        .tFilter         = PL_FILTER_LINEAR,
        .fMinMip         = 0.0f,
        .fMaxMip         = 64.0f,
        .tVerticalWrap   = PL_WRAP_MODE_WRAP,
        .tHorizontalWrap = PL_WRAP_MODE_WRAP
    };
    ptAppData->tDefaultSampler = gptDevice->create_sampler(ptDevice, &tSamplerDesc, "default sampler");

    // create offscreen per-frame resources

    const plTextureDesc tColorTextureDesc = {
        .tDimensions   = {ptAppData->tOffscreenSize.x, ptAppData->tOffscreenSize.y, 1},
        .tFormat       = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
        .tInitialUsage = PL_TEXTURE_USAGE_SAMPLED
    };

    const plTextureDesc tDepthTextureDesc = {
        .tDimensions   = {ptAppData->tOffscreenSize.x, ptAppData->tOffscreenSize.y, 1},
        .tFormat       = PL_FORMAT_D32_FLOAT_S8_UINT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
        .tInitialUsage = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT
    };

    plRenderPassAttachments atAttachmentSets[PL_FRAMES_IN_FLIGHT] = {0};

    for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
    {
        // create textures
        ptAppData->atColorTexture[i] = gptDevice->create_texture(ptDevice, &tColorTextureDesc, "color texture");
        ptAppData->atDepthTexture[i] = gptDevice->create_texture(ptDevice, &tDepthTextureDesc, "depth texture");

        // retrieve textures
        plTexture* ptColorTexture = gptDevice->get_texture(ptDevice, ptAppData->atColorTexture[i]);
        plTexture* ptDepthTexture = gptDevice->get_texture(ptDevice, ptAppData->atDepthTexture[i]);

        plDeviceMemoryAllocatorI* ptAllocator = gptGpuAllocators->get_local_dedicated_allocator(ptDevice);

        // allocate memory
        const plDeviceMemoryAllocation tColorAllocation = ptAllocator->allocate(ptAllocator->ptInst, 
            ptColorTexture->tMemoryRequirements.uMemoryTypeBits,
            ptColorTexture->tMemoryRequirements.ulSize,
            ptColorTexture->tMemoryRequirements.ulAlignment,
            "color texture memory");

        const plDeviceMemoryAllocation tDepthAllocation = ptAllocator->allocate(ptAllocator->ptInst, 
            ptDepthTexture->tMemoryRequirements.uMemoryTypeBits,
            ptDepthTexture->tMemoryRequirements.ulSize,
            ptDepthTexture->tMemoryRequirements.ulAlignment,
            "depth texture memory");

        // bind memory
        gptDevice->bind_texture_to_memory(ptDevice, ptAppData->atColorTexture[i], &tColorAllocation);
        gptDevice->bind_texture_to_memory(ptDevice, ptAppData->atDepthTexture[i], &tDepthAllocation);

        // get UI texture handle
        ptAppData->atColorTextureId[i] = gptGfx->get_ui_texture_handle(ptGraphics, ptAppData->atColorTexture[i], ptAppData->tDefaultSampler);

        // add textures to attachment set for render pass
        atAttachmentSets[i].atViewAttachments[0] = ptAppData->atDepthTexture[i];
        atAttachmentSets[i].atViewAttachments[1] = ptAppData->atColorTexture[i];
    }

    // create offscreen renderpass layout
    const plRenderPassLayoutDescription tRenderPassLayoutDesc = {
        .atRenderTargets = {
            { .tFormat = PL_FORMAT_D32_FLOAT_S8_UINT }, // depth buffer
            { .tFormat = PL_FORMAT_R32G32B32A32_FLOAT } // color
        },
        .uSubpassCount = 1,
        .atSubpasses = {
            {
                .uRenderTargetCount = 2,
                .auRenderTargets = {0, 1},
            },
        }
    };

    // create offscreen renderpass
    const plRenderPassDescription tRenderPassDesc = {
        .tLayout = gptDevice->create_render_pass_layout(ptDevice, &tRenderPassLayoutDesc),
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
    ptAppData->tOffscreenRenderPass = gptDevice->create_render_pass(&ptGraphics->tDevice, &tRenderPassDesc, atAttachmentSets);

    // return app memory
    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    // cleanup
    gptGfx->destroy_font_atlas(&ptAppData->tFontAtlas); // backend specific cleanup
    pl_cleanup_font_atlas(&ptAppData->tFontAtlas);

    // ensure GPU is finished before cleanup
    gptDevice->flush_device(&ptAppData->tGraphics.tDevice);
    gptDraw3d->cleanup();
    gptGpuAllocators->cleanup_allocators(&ptAppData->tGraphics.tDevice);
    gptGfx->cleanup(&ptAppData->tGraphics);
    gptWindows->destroy_window(ptAppData->ptWindow);
    pl_cleanup_profile_context();
    pl_cleanup_log_context();
    PL_FREE(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plAppData* ptAppData)
{
    // perform any operations required during a window resize
    gptGfx->resize(&ptAppData->tGraphics); // recreates swapchain
    ptAppData->bResize = true;
    
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    pl_begin_profile_frame();

    // for convience
    plGraphics* ptGraphics = &ptAppData->tGraphics;
    plIO* ptIO = pl_get_io();

    if(ptAppData->bResize)
    {
        plDevice* ptDevice = &ptGraphics->tDevice;
        ptAppData->tOffscreenSize.x = ptIO->afMainViewportSize[0];
        ptAppData->tOffscreenSize.y = ptIO->afMainViewportSize[1];
        resize_offscreen_resources(ptAppData);
        ptAppData->bResize = false;
    }

    // begin new frame
    if(!gptGfx->begin_frame(ptGraphics))
    {
        gptGfx->resize(ptGraphics);
        pl_end_profile_frame();
        return;
    }

    static const float fCameraTravelSpeed = 4.0f;
    static const float fCameraRotationSpeed = 0.005f;

    bool bOwnKeyboard = ptIO->bWantCaptureKeyboard;
    plCamera* ptCamera = &ptAppData->tCamera;
    if(!bOwnKeyboard)
    {
        // camera space
        if(pl_is_key_down(PL_KEY_W)) camera_translate(ptCamera,  0.0f,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime);
        if(pl_is_key_down(PL_KEY_S)) camera_translate(ptCamera,  0.0f,  0.0f, -fCameraTravelSpeed* ptIO->fDeltaTime);
        if(pl_is_key_down(PL_KEY_A)) camera_translate(ptCamera, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);
        if(pl_is_key_down(PL_KEY_D)) camera_translate(ptCamera,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);

        // world space
        if(pl_is_key_down(PL_KEY_F)) { camera_translate(ptCamera,  0.0f, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f); }
        if(pl_is_key_down(PL_KEY_R)) { camera_translate(ptCamera,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f); }
    }

    bool bOwnMouse = ptIO->bWantCaptureMouse;
    if(!bOwnMouse && pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 1.0f))
    {
        const plVec2 tMouseDelta = pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f);
        camera_rotate(ptCamera,  -tMouseDelta.y * fCameraRotationSpeed,  -tMouseDelta.x * fCameraRotationSpeed);
        pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
    }
    camera_update(ptCamera);

    gptDraw3d->new_frame();

    pl_new_frame(); // must be called once at the beginning of a frame

    // create a UI window
    if(pl_begin_window("Pilot Light", NULL, false))
    {

        const float pfRatios[] = {1.0f};
        pl_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
        if(pl_collapsing_header("Information"))
        {
            pl_text("Pilot Light %s", PILOTLIGHT_VERSION);
            pl_text("Pilot Light UI %s", PL_UI_VERSION);
            pl_text("Pilot Light DS %s", PL_DS_VERSION);
            pl_end_collapsing_header();
        }

        if(pl_collapsing_header("User Interface"))
        {
            pl_checkbox("UI Demo", &ptAppData->bShowUiDemo);
            pl_end_collapsing_header();
        }
        pl_end_window();
    }

    if(ptAppData->bShowUiDemo)
        pl_show_demo_window(&ptAppData->bShowUiDemo);

    // add full screen quad for offscreen render
    pl_add_image(ptAppData->ptFGLayer, ptAppData->atColorTextureId[ptGraphics->uCurrentFrameIndex], (plVec2){0}, (plVec2){ptIO->afMainViewportSize[0], ptIO->afMainViewportSize[1]});

    // 3d drawing API usage
    const plMat4 tOrigin = pl_identity_mat4();
    gptDraw3d->add_transform(ptAppData->pt3dDrawlist, &tOrigin, 10.0f, 0.2f);

    gptDraw3d->add_triangle_filled(ptAppData->pt3dDrawlist,
        (plVec3){1.0f, 1.0f, 1.0f},
        (plVec3){4.0f, 1.0f, 1.0f},
        (plVec3){1.0f, 4.0f, 1.0f},
        (plVec4){1.0f, 1.0f, 0.0f, 0.75f});

    gptDraw3d->add_triangle_filled(ptAppData->pt3dDrawlist,
        (plVec3){1.0f, 1.0f, 3.0f},
        (plVec3){4.0f, 1.0f, 3.0f},
        (plVec3){1.0f, 4.0f, 3.0f},
        (plVec4){1.0f, 0.5f, 0.3f, 0.75f});

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~UI & drawing prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // submit our draw layers
    pl_submit_layer(ptAppData->ptFGLayer);

    // build UI render data (and submits layers in correct order)
    pl_render();

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~command buffer 0~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // expected timeline semaphore values
    uint64_t ulValue0 = ptAppData->aulNextTimelineValue[ptGraphics->uCurrentFrameIndex];
    uint64_t ulValue1 = ulValue0 + 1;
    uint64_t ulValue2 = ulValue0 + 2;
    ptAppData->aulNextTimelineValue[ptGraphics->uCurrentFrameIndex] = ulValue2;

    const plBeginCommandInfo tBeginInfo0 = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {ptAppData->atSempahore[ptGraphics->uCurrentFrameIndex]},
        .auWaitSemaphoreValues = {ulValue0},
    };
    plCommandBuffer tCommandBuffer0 = gptGfx->begin_command_recording(ptGraphics, &tBeginInfo0);

    // begin offscreen renderpass
    plRenderEncoder tEncoder0 = gptGfx->begin_render_pass(ptGraphics, &tCommandBuffer0, ptAppData->tOffscreenRenderPass);

    const plMat4 tMVP = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);
    gptDraw3d->submit_drawlist(ptAppData->pt3dDrawlist,
        tEncoder0,
        ptAppData->tOffscreenSize.x,
        ptAppData->tOffscreenSize.y,
        &tMVP,
        PL_3D_DRAW_FLAG_DEPTH_TEST | PL_3D_DRAW_FLAG_DEPTH_WRITE, 1);

    // end offscreen render pass
    gptGfx->end_render_pass(&tEncoder0);

    // end recording
    gptGfx->end_command_recording(ptGraphics, &tCommandBuffer0);

    const plSubmitInfo tSubmitInfo0 = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {ptAppData->atSempahore[ptGraphics->uCurrentFrameIndex]},
        .auSignalSemaphoreValues = {ulValue1},
    };
    gptGfx->submit_command_buffer(ptGraphics, &tCommandBuffer0, &tSubmitInfo0);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~command buffer 1~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const plBeginCommandInfo tBeginInfo1 = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {ptAppData->atSempahore[ptGraphics->uCurrentFrameIndex]},
        .auWaitSemaphoreValues = {ulValue1},
    };
    plCommandBuffer tCommandBuffer1 = gptGfx->begin_command_recording(ptGraphics, &tBeginInfo1);

    // begin main renderpass (directly to swapchain)
    plRenderEncoder tEncoder1 = gptGfx->begin_render_pass(ptGraphics, &tCommandBuffer1, ptGraphics->tMainRenderPass);

    // submit drawlists
    gptGfx->draw_lists(ptGraphics, tEncoder1, 1, &ptAppData->tAppDrawlist);
    gptGfx->draw_lists(ptGraphics, tEncoder1, 1, pl_get_draw_list(NULL));
    gptGfx->draw_lists(ptGraphics, tEncoder1, 1, pl_get_debug_draw_list(NULL));
    
    // end render pass
    gptGfx->end_render_pass(&tEncoder1);

    // end recording
    gptGfx->end_command_recording(ptGraphics, &tCommandBuffer1);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~submit work to GPU & present~~~~~~~~~~~~~~~~~~~~~~~

    const plSubmitInfo tSubmitInfo1 = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {ptAppData->atSempahore[ptGraphics->uCurrentFrameIndex]},
        .auSignalSemaphoreValues = {ulValue2},
    };

    if(!gptGfx->present(ptGraphics, &tCommandBuffer1, &tSubmitInfo1))
        gptGfx->resize(ptGraphics);

    pl_end_profile_frame();
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
    plGraphics* ptGraphics = &ptAppData->tGraphics;
    plDevice* ptDevice = &ptGraphics->tDevice;

    plIO* ptIO = pl_get_io();
    ptAppData->tCamera.fAspectRatio = ptIO->afMainViewportSize[0] / ptIO->afMainViewportSize[1];

    const plTextureDesc tColorTextureDesc = {
        .tDimensions   = {ptAppData->tOffscreenSize.x, ptAppData->tOffscreenSize.y, 1},
        .tFormat       = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
        .tInitialUsage = PL_TEXTURE_USAGE_SAMPLED
    };

    const plTextureDesc tDepthTextureDesc = {
        .tDimensions   = {ptAppData->tOffscreenSize.x, ptAppData->tOffscreenSize.y, 1},
        .tFormat       = PL_FORMAT_D32_FLOAT_S8_UINT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
        .tInitialUsage = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT
    };

    plRenderPassAttachments atAttachmentSets[PL_FRAMES_IN_FLIGHT] = {0};

    for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
    {
        // queue old resources for deletion
        gptDevice->queue_texture_for_deletion(ptDevice, ptAppData->atColorTexture[i]);
        gptDevice->queue_texture_for_deletion(ptDevice, ptAppData->atDepthTexture[i]);

        // create new textures
        ptAppData->atColorTexture[i] = gptDevice->create_texture(ptDevice, &tColorTextureDesc, "color texture");
        ptAppData->atDepthTexture[i] = gptDevice->create_texture(ptDevice, &tDepthTextureDesc, "depth texture");

        // retrieve textures
        plTexture* ptColorTexture = gptDevice->get_texture(ptDevice, ptAppData->atColorTexture[i]);
        plTexture* ptDepthTexture = gptDevice->get_texture(ptDevice, ptAppData->atDepthTexture[i]);

        plDeviceMemoryAllocatorI* ptAllocator = gptGpuAllocators->get_local_dedicated_allocator(ptDevice);

        // allocate memory
        const plDeviceMemoryAllocation tColorAllocation = ptAllocator->allocate(ptAllocator->ptInst, 
            ptColorTexture->tMemoryRequirements.uMemoryTypeBits,
            ptColorTexture->tMemoryRequirements.ulSize,
            ptColorTexture->tMemoryRequirements.ulAlignment,
            "color texture memory");

        const plDeviceMemoryAllocation tDepthAllocation = ptAllocator->allocate(ptAllocator->ptInst, 
            ptDepthTexture->tMemoryRequirements.uMemoryTypeBits,
            ptDepthTexture->tMemoryRequirements.ulSize,
            ptDepthTexture->tMemoryRequirements.ulAlignment,
            "depth texture memory");

        // bind memory
        gptDevice->bind_texture_to_memory(ptDevice, ptAppData->atColorTexture[i], &tColorAllocation);
        gptDevice->bind_texture_to_memory(ptDevice, ptAppData->atDepthTexture[i], &tDepthAllocation);

        // get UI texture handle
        ptAppData->atColorTextureId[i] = gptGfx->get_ui_texture_handle(ptGraphics, ptAppData->atColorTexture[i], ptAppData->tDefaultSampler);

        // add textures to attachment set for render pass
        atAttachmentSets[i].atViewAttachments[0] = ptAppData->atDepthTexture[i];
        atAttachmentSets[i].atViewAttachments[1] = ptAppData->atColorTexture[i];
    }
    gptDevice->update_render_pass_attachments(ptDevice, ptAppData->tOffscreenRenderPass, ptAppData->tOffscreenSize, atAttachmentSets);
}