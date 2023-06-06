/*
   vulkan_app.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <string.h> // memset
#include "pilotlight.h"
#include "pl_profile.h"
#include "pl_log.h"
#include "pl_ds.h"
#include "pl_io.h"
#include "pl_os.h"
#include "pl_memory.h"

#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extensions
#include "pl_image_ext.h"
#include "pl_vulkan_ext.h"
#include "pl_ecs_ext.h"
#include "pl_renderer_ext.h"
#include "pl_gltf_ext.h"
#include "pl_draw_ext.h"
#include "pl_ui_ext.h"
#include "pl_stats_ext.h"
#include "pl_debug_ext.h"

// backends
#include "pl_vulkan.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAppData
{

    // vulkan
    plRenderBackend tBackend;
    plGraphics   tGraphics;
    plDrawList   drawlist;
    plDrawList   drawlist2;
    plDrawList3D drawlist3d;
    plDrawLayer* fgDrawLayer;
    plDrawLayer* bgDrawLayer;
    plDrawLayer* offscreenDrawLayer;
    plFontAtlas  fontAtlas;
    bool         bShowUiDemo;
    bool         bShowUiDebug;
    bool         bShowUiStyle;
    bool         bShowEcs;

    // allocators
    plTempAllocator tTempAllocator;

    // apis
    plLibraryApiI*    ptLibraryApi;
    plFileApiI*       ptFileApi;
    plRendererI*      ptRendererApi;
    plGraphicsApiI*   ptGfx;
    plDrawApiI*       ptDrawApi;
    plVulkanDrawApiI* ptVulkanDrawApi;
    plUiApiI*         ptUi;
    plDeviceApiI*     ptDeviceApi;
    plEcsI*           ptEcs;
    plCameraI*        ptCameraApi;
    plStatsApiI*      ptStatsApi;
    plDebugApiI*      ptDebugApi;
    
    // renderer
    plRenderer         tRenderer;
    plScene            tScene;
    plComponentLibrary tComponentLibrary;

    // cameras
    plEntity tCameraEntity;
    plEntity tOffscreenCameraEntity;

    // new stuff
    uint32_t         uOffscreenPass;
    plRenderTarget   tMainTarget;
    plRenderTarget   tOffscreenTarget;
    VkDescriptorSet* sbtTextures;

    plApiRegistryApiI* ptApiRegistry;
    plDebugApiInfo     tDebugInfo;
    int                iSelectedEntity;
    bool               bVSyncChanged;

} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] helper functions
//-----------------------------------------------------------------------------

static void pl__show_main_window(plAppData* ptAppData);
static void pl__show_ecs_window (plAppData* ptAppData);
static void pl__select_entity   (plAppData* ptAppData);

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

static VkSurfaceKHR pl__create_surface(VkInstance tInstance, plIOContext* ptIoCtx);

PL_EXPORT void*
pl_app_load(plApiRegistryApiI* ptApiRegistry, void* pAppData)
{
    plAppData* ptAppData = pAppData;
    plDataRegistryApiI* ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(ptDataRegistry->get_data("memory"));
    pl_set_io_context(ptDataRegistry->get_data(PL_CONTEXT_IO_NAME));

    if(ptAppData) // reload
    {
        pl_set_log_context(ptDataRegistry->get_data("log"));
        pl_set_profile_context(ptDataRegistry->get_data("profile"));

        
        ptAppData->ptRendererApi   = ptApiRegistry->first(PL_API_RENDERER);
        ptAppData->ptGfx           = ptApiRegistry->first(PL_API_GRAPHICS);
        ptAppData->ptDrawApi       = ptApiRegistry->first(PL_API_DRAW);
        ptAppData->ptVulkanDrawApi = ptApiRegistry->first(PL_API_VULKAN_DRAW);
        ptAppData->ptUi            = ptApiRegistry->first(PL_API_UI);
        ptAppData->ptDeviceApi     = ptApiRegistry->first(PL_API_DEVICE);
        ptAppData->ptEcs           = ptApiRegistry->first(PL_API_ECS);
        ptAppData->ptCameraApi     = ptApiRegistry->first(PL_API_CAMERA);
        ptAppData->ptDebugApi      = ptApiRegistry->first(PL_API_DEBUG);
        return ptAppData;
    }

    plProfileContext* ptProfileCtx = pl_create_profile_context();
    plLogContext*     ptLogCtx     = pl_create_log_context();

    // allocate original app memory
    ptAppData = PL_ALLOC(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));
    ptAppData->ptApiRegistry = ptApiRegistry;
    ptDataRegistry->set_data("profile", ptProfileCtx);
    ptDataRegistry->set_data("log", ptLogCtx);
    ptAppData->iSelectedEntity = 1;

    // load extensions
    plExtensionRegistryApiI* ptExtensionRegistry = ptApiRegistry->first(PL_API_EXTENSION_REGISTRY);
    ptExtensionRegistry->load_from_config(ptApiRegistry, "../apps/pl_config.json");

    // load apis 
    plLibraryApiI*       ptLibraryApi    = ptApiRegistry->first(PL_API_LIBRARY);
    plFileApiI*          ptFileApi       = ptApiRegistry->first(PL_API_FILE);
    plDeviceApiI*        ptDeviceApi     = ptApiRegistry->first(PL_API_DEVICE);
    plRenderBackendI*    ptBackendApi    = ptApiRegistry->first(PL_API_BACKEND_VULKAN);
    plImageApiI*         ptImageApi      = ptApiRegistry->first(PL_API_IMAGE);
    plGltfApiI*          ptGltfApi       = ptApiRegistry->first(PL_API_GLTF);
    plRendererI*         ptRendererApi   = ptApiRegistry->first(PL_API_RENDERER);
    plGraphicsApiI*      ptGfx           = ptApiRegistry->first(PL_API_GRAPHICS);
    plDrawApiI*          ptDrawApi       = ptApiRegistry->first(PL_API_DRAW);
    plVulkanDrawApiI*    ptVulkanDrawApi = ptApiRegistry->first(PL_API_VULKAN_DRAW);
    plUiApiI*            ptUi            = ptApiRegistry->first(PL_API_UI);
    plEcsI*              ptEcs           = ptApiRegistry->first(PL_API_ECS);
    plCameraI*           ptCameraApi     = ptApiRegistry->first(PL_API_CAMERA);
    plStatsApiI*         ptStatsApi      = ptApiRegistry->first(PL_API_STATS);
    plDebugApiI*         ptDebugApi      = ptApiRegistry->first(PL_API_DEBUG);

    // save apis that are used often
    ptAppData->ptRendererApi = ptRendererApi;
    ptAppData->ptGfx = ptGfx;
    ptAppData->ptDrawApi = ptDrawApi;
    ptAppData->ptVulkanDrawApi = ptVulkanDrawApi;
    ptAppData->ptUi = ptUi;
    ptAppData->ptDeviceApi = ptDeviceApi;
    ptAppData->ptEcs = ptEcs;
    ptAppData->ptCameraApi = ptCameraApi;
    ptAppData->ptStatsApi = ptStatsApi;
    ptAppData->ptDebugApi = ptDebugApi;

    // for convience
    plGraphics*         ptGraphics         = &ptAppData->tGraphics;
    plRenderBackend*    ptBackend          = &ptAppData->tBackend;
    plDevice*           ptDevice           = &ptGraphics->tDevice;
    plRenderer*         ptRenderer         = &ptAppData->tRenderer;
    plScene*            ptScene            = &ptAppData->tScene;
    plComponentLibrary* ptComponentLibrary = &ptAppData->tComponentLibrary;
    
    // contexts
    plIOContext*      ptIoCtx      = pl_get_io_context();
    plUiContext*      ptUiContext  = ptUi->create_context();

    // add some context to data registry
    ptDataRegistry->set_data("ui", ptUiContext);
    ptDataRegistry->set_data(PL_CONTEXT_DRAW_NAME, ptDrawApi->get_context());
    ptDataRegistry->set_data("device", ptDevice);

    // setup sbackend
    ptBackendApi->setup(ptApiRegistry, ptBackend, VK_API_VERSION_1_2, true);

    // create surface
    ptBackend->tSurface = pl__create_surface(ptBackend->tInstance, ptIoCtx);

    // create & init device
    ptBackendApi->create_device(ptBackend, ptBackend->tSurface, true, ptDevice);
    ptDeviceApi->init(ptApiRegistry, ptDevice, 2);

    // create swapchain
    ptGraphics->tSwapchain.bVSync = true;
    ptBackendApi->create_swapchain(ptBackend, ptDevice, ptBackend->tSurface, (uint32_t)ptIoCtx->afMainViewportSize[0], (uint32_t)ptIoCtx->afMainViewportSize[1], &ptGraphics->tSwapchain);
    
    // setup graphics
    ptGraphics->ptBackend = ptBackend;
    ptGfx->setup(ptGraphics, ptBackend, ptApiRegistry, &ptAppData->tTempAllocator);
    
    // setup drawing api
    const plVulkanInit tVulkanInit = {
        .tPhysicalDevice  = ptDevice->tPhysicalDevice,
        .tLogicalDevice   = ptDevice->tLogicalDevice,
        .uImageCount      = ptGraphics->tSwapchain.uImageCount,
        .tRenderPass      = ptDevice->sbtRenderPasses[ptGraphics->uRenderPass]._tRenderPass,
        .tMSAASampleCount = ptGraphics->tSwapchain.tMsaaSamples,
        .uFramesInFlight  = ptGraphics->uFramesInFlight
    };
    ptVulkanDrawApi->initialize_context(&tVulkanInit);
    plDrawContext* ptDrawCtx = ptDrawApi->get_context();
    ptDrawApi->register_drawlist(ptDrawCtx, &ptAppData->drawlist);
    ptDrawApi->register_drawlist(ptDrawCtx, &ptAppData->drawlist2);
    ptDrawApi->register_3d_drawlist(ptDrawCtx, &ptAppData->drawlist3d);
    ptAppData->bgDrawLayer = ptDrawApi->request_layer(&ptAppData->drawlist, "Background Layer");
    ptAppData->fgDrawLayer = ptDrawApi->request_layer(&ptAppData->drawlist, "Foreground Layer");
    ptAppData->offscreenDrawLayer = ptDrawApi->request_layer(&ptAppData->drawlist2, "Foreground Layer");

    // create font atlas
    ptDrawApi->add_default_font(&ptAppData->fontAtlas);
    ptDrawApi->build_font_atlas(ptDrawCtx, &ptAppData->fontAtlas);
    ptUi->set_default_font(&ptAppData->fontAtlas.sbFonts[0]);
    
    // renderer
    ptEcs->init_component_library(ptApiRegistry, ptComponentLibrary);
    ptRendererApi->setup_renderer(ptApiRegistry, ptComponentLibrary, ptGraphics, &ptAppData->tRenderer);
    ptRendererApi->create_scene(&ptAppData->tRenderer, ptComponentLibrary, &ptAppData->tScene);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~entity IDs~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    // cameras
    ptAppData->tOffscreenCameraEntity = ptEcs->create_camera(ptComponentLibrary, "offscreen camera", (plVec3){0.0f, 0.35f, 1.2f}, PL_PI_3, 1280.0f / 720.0f, 0.1f, 10.0f);
    ptAppData->tCameraEntity = ptEcs->create_camera(ptComponentLibrary, "main camera", (plVec3){-6.211f, 3.647f, 0.827f}, PL_PI_3, ptIoCtx->afMainViewportSize[0] / ptIoCtx->afMainViewportSize[1], 0.01f, 400.0f);
    plCameraComponent* ptCamera = ptEcs->get_component(&ptComponentLibrary->tCameraComponentManager, ptAppData->tCameraEntity);
    plCameraComponent* ptCamera2 = ptEcs->get_component(&ptComponentLibrary->tCameraComponentManager, ptAppData->tOffscreenCameraEntity);
    ptAppData->ptCameraApi->set_pitch_yaw(ptCamera, -0.244f, 1.488f);
    ptAppData->ptCameraApi->set_pitch_yaw(ptCamera2, 0.0f, -PL_PI);

    ptGltfApi->load(ptScene, ptComponentLibrary, "../data/glTF-Sample-Models-master/2.0/FlightHelmet/glTF/FlightHelmet.gltf");
    // ptGltfApi->load(ptScene, ptComponentLibrary, "../data/glTF-Sample-Models-master/2.0/DamagedHelmet/glTF/DamagedHelmet.gltf");
    // ptGltfApi->load(ptScene, ptComponentLibrary, "../data/glTF-Sample-Models-master/2.0/Sponza/glTF/Sponza.gltf");

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~materials~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // offscreen
    plRenderPassDesc tRenderPassDesc = {
        .tColorFormat = PL_FORMAT_R8G8B8A8_UNORM,
        .tDepthFormat = ptDeviceApi->find_depth_stencil_format(ptDevice)
    };
    ptAppData->uOffscreenPass = ptDeviceApi->create_render_pass(ptDevice, &tRenderPassDesc, "offscreen renderpass");


    plRenderTargetDesc tRenderTargetDesc = {
        .uRenderPass = ptAppData->uOffscreenPass,
        .tSize = {1280.0f, 720.0f},
    };
    ptRendererApi->create_render_target(ptGraphics, &tRenderTargetDesc, &ptAppData->tOffscreenTarget);

    for(uint32_t i = 0; i < ptGraphics->tSwapchain.uImageCount; i++)
    {
        plTextureView* ptColorTextureView = &ptDevice->sbtTextureViews[ptAppData->tOffscreenTarget.sbuColorTextureViews[i]];
        pl_sb_push(ptAppData->sbtTextures, ptVulkanDrawApi->add_texture(ptDrawCtx, ptColorTextureView->_tImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
    }

    ptRendererApi->create_main_render_target(ptGraphics, &ptAppData->tMainTarget);
    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(void* pAppData)
{
    plAppData* ptAppData = pAppData; 

    plGraphics*      ptGraphics = &ptAppData->tGraphics;
    plRenderBackend* ptBackend  = &ptAppData->tBackend;
    plDevice*        ptDevice   = &ptGraphics->tDevice;
    plRenderer*      ptRenderer = &ptAppData->tRenderer;

    vkDeviceWaitIdle(ptGraphics->tDevice.tLogicalDevice);

    plRendererI*            ptRendererApi     = ptAppData->ptApiRegistry->first(PL_API_RENDERER);
    plGraphicsApiI*         ptGfx       = ptAppData->ptApiRegistry->first(PL_API_GRAPHICS);
    plDrawApiI*             ptDrawApi      = ptAppData->ptApiRegistry->first(PL_API_DRAW);
    plUiApiI*               ptUiApi       = ptAppData->ptApiRegistry->first(PL_API_UI);
    plDeviceApiI*           deviceApi = ptAppData->ptApiRegistry->first(PL_API_DEVICE);
    plRenderBackendI*       ptBackendApi = ptAppData->ptApiRegistry->first(PL_API_BACKEND_VULKAN);
    plEcsI*                 ptEcs = ptAppData->ptEcs;
    ptDrawApi->cleanup_font_atlas(&ptAppData->fontAtlas);
    ptDrawApi->cleanup_context();
    ptUiApi->destroy_context(NULL);
    
    ptRendererApi->cleanup_render_target(&ptAppData->tGraphics, &ptAppData->tOffscreenTarget);
    ptRendererApi->cleanup_render_target(&ptAppData->tGraphics, &ptAppData->tRenderer.tPickTarget);
    ptRendererApi->cleanup_scene(&ptAppData->tScene);
    ptRendererApi->cleanup_renderer(&ptAppData->tRenderer);
    ptEcs->cleanup_systems(ptAppData->ptApiRegistry, &ptAppData->tComponentLibrary);
    ptGfx->cleanup(&ptAppData->tGraphics);
    ptBackendApi->cleanup_swapchain(ptBackend, ptDevice, &ptGraphics->tSwapchain);
    ptBackendApi->cleanup_device(&ptGraphics->tDevice);
    pl_cleanup_profile_context();
    pl_cleanup_log_context();
    pl_temp_allocator_free(&ptAppData->tTempAllocator);
    pl_sb_free(ptAppData->sbtTextures);
    PL_FREE(pAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(void* pAppData)
{
    plAppData* ptAppData = pAppData;

    // load apis 
    plDeviceApiI*     ptDeviceApi     = ptAppData->ptDeviceApi;
    plRendererI*      ptRendererApi      = ptAppData->ptRendererApi;
    plGraphicsApiI*   ptGfx           = ptAppData->ptGfx;
    plDrawApiI*       ptDrawApi       = ptAppData->ptDrawApi;
    plVulkanDrawApiI* ptVulkanDrawApi = ptAppData->ptVulkanDrawApi;
    plUiApiI*         ptUi            = ptAppData->ptUi;
    plEcsI*           ptEcs           = ptAppData->ptEcs;
    plCameraI*        ptCameraApi     = ptAppData->ptCameraApi;

    // for convience
    plGraphics*         ptGraphics         = &ptAppData->tGraphics;
    plRenderBackend*    ptBackend          = &ptAppData->tBackend;
    plDevice*           ptDevice           = &ptGraphics->tDevice;
    plRenderer*         ptRenderer         = &ptAppData->tRenderer;
    plScene*            ptScene            = &ptAppData->tScene;
    plComponentLibrary* ptComponentLibrary = &ptAppData->tComponentLibrary;

    // contexts
    plIOContext*      ptIoCtx      = pl_get_io_context();
    plProfileContext* ptProfileCtx = pl_get_profile_context();
    plLogContext*     ptLogCtx     = pl_get_log_context();
    plDrawContext*    ptDrawCtx    = ptDrawApi->get_context();

    ptGfx->resize(ptGraphics);
    ptRendererApi->resize(ptRenderer, ptIoCtx->afMainViewportSize[0], ptIoCtx->afMainViewportSize[1]);
    plCameraComponent* ptCamera = ptEcs->get_component(&ptComponentLibrary->tCameraComponentManager, ptAppData->tCameraEntity);
    ptCameraApi->set_aspect(ptCamera, ptIoCtx->afMainViewportSize[0] / ptIoCtx->afMainViewportSize[1]);
    ptRendererApi->create_main_render_target(ptGraphics, &ptAppData->tMainTarget);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{

    pl_begin_profile_frame();
    pl_begin_profile_sample(__FUNCTION__);

    // load apis
    plDeviceApiI*        ptDeviceApi     = ptAppData->ptDeviceApi;
    plRendererI*         ptRendererApi   = ptAppData->ptRendererApi;
    plGraphicsApiI*      ptGfx           = ptAppData->ptGfx;
    plDrawApiI*          ptDrawApi       = ptAppData->ptDrawApi;
    plVulkanDrawApiI*    ptVulkanDrawApi = ptAppData->ptVulkanDrawApi;
    plUiApiI*            ptUi            = ptAppData->ptUi;
    plEcsI*              ptEcs           = ptAppData->ptEcs;
    plCameraI*           ptCameraApi     = ptAppData->ptCameraApi;

    // for convience
    plGraphics*         ptGraphics         = &ptAppData->tGraphics;
    plRenderBackend*    ptBackend          = &ptAppData->tBackend;
    plDevice*           ptDevice           = &ptGraphics->tDevice;
    plRenderer*         ptRenderer         = &ptAppData->tRenderer;
    plScene*            ptScene            = &ptAppData->tScene;

    // contexts
    plIOContext*      ptIoCtx      = pl_get_io_context();
    plProfileContext* ptProfileCtx = pl_get_profile_context();
    plLogContext*     ptLogCtx     = pl_get_log_context();
    plDrawContext*    ptDrawCtx    = ptDrawApi->get_context();

    ptAppData->ptStatsApi->new_frame();

    {
        static double* pdFrameTimeCounter = NULL;
        if(!pdFrameTimeCounter)
            pdFrameTimeCounter = ptAppData->ptStatsApi->get_counter("frame rate");
        *pdFrameTimeCounter = (double)ptIoCtx->fFrameRate;
    }

    if(ptAppData->bVSyncChanged)
    {
        ptGfx->resize(&ptAppData->tGraphics);
        ptRendererApi->create_main_render_target(&ptAppData->tGraphics, &ptAppData->tMainTarget);
        ptAppData->bVSyncChanged = false;
    }
    
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~input handling~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    static const float fCameraTravelSpeed = 8.0f;
    plCameraComponent* ptCamera = ptEcs->get_component(&ptAppData->tComponentLibrary.tCameraComponentManager, ptAppData->tCameraEntity);
    plCameraComponent* ptOffscreenCamera = ptEcs->get_component(&ptAppData->tComponentLibrary.tCameraComponentManager, ptAppData->tOffscreenCameraEntity);

    // camera space
    if(pl_is_key_pressed(PL_KEY_W, true)) ptCameraApi->translate(ptCamera,  0.0f,  0.0f,  fCameraTravelSpeed * ptIoCtx->fDeltaTime);
    if(pl_is_key_pressed(PL_KEY_S, true)) ptCameraApi->translate(ptCamera,  0.0f,  0.0f, -fCameraTravelSpeed* ptIoCtx->fDeltaTime);
    if(pl_is_key_pressed(PL_KEY_A, true)) ptCameraApi->translate(ptCamera, -fCameraTravelSpeed * ptIoCtx->fDeltaTime,  0.0f,  0.0f);
    if(pl_is_key_pressed(PL_KEY_D, true)) ptCameraApi->translate(ptCamera,  fCameraTravelSpeed * ptIoCtx->fDeltaTime,  0.0f,  0.0f);

    // world space
    if(pl_is_key_pressed(PL_KEY_F, true)) ptCameraApi->translate(ptCamera,  0.0f, -fCameraTravelSpeed * ptIoCtx->fDeltaTime,  0.0f);
    if(pl_is_key_pressed(PL_KEY_R, true)) ptCameraApi->translate(ptCamera,  0.0f,  fCameraTravelSpeed * ptIoCtx->fDeltaTime,  0.0f);

    plFrameContext* ptCurrentFrame = ptGfx->get_frame_resources(ptGraphics);
    
    static double* pdFrameTimeCounter = NULL;
    if(!pdFrameTimeCounter)
        pdFrameTimeCounter = ptAppData->ptStatsApi->get_counter("frame");
    *pdFrameTimeCounter = (double)ptDevice->uCurrentFrame;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~begin frame~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    if(ptGfx->begin_frame(ptGraphics))
    {
        pl_begin_profile_sample("process_cleanup_queue");
        ptDeviceApi->process_cleanup_queue(&ptGraphics->tDevice, (uint32_t)ptGraphics->szCurrentFrameIndex);
        pl_end_profile_sample();

        bool bOwnMouse = ptIoCtx->bWantCaptureMouse;
        ptUi->new_frame();

        if(!bOwnMouse && pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 1.0f))
        {
            const plVec2 tMouseDelta = pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f);
            ptCameraApi->rotate(ptCamera,  -tMouseDelta.y * 0.1f * ptIoCtx->fDeltaTime,  -tMouseDelta.x * 0.1f * ptIoCtx->fDeltaTime);
            pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
        }
        ptCameraApi->update(ptCamera);
        ptCameraApi->update(ptOffscreenCamera);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~3D drawing api~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        ptDrawApi->add_3d_transform(&ptAppData->drawlist3d, &ptOffscreenCamera->tTransformMat, 0.2f, 0.02f);
        ptDrawApi->add_3d_frustum(&ptAppData->drawlist3d, 
            &ptOffscreenCamera->tTransformMat, ptOffscreenCamera->fFieldOfView, ptOffscreenCamera->fAspectRatio, 
            ptOffscreenCamera->fNearZ, ptOffscreenCamera->fFarZ, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 0.02f);

        const plMat4 tTransform0 = pl_identity_mat4();
        ptDrawApi->add_3d_transform(&ptAppData->drawlist3d, &tTransform0, 10.0f, 0.02f);
        ptDrawApi->add_3d_bezier_quad(&ptAppData->drawlist3d, (plVec3){0.0f,0.0f,0.0f}, (plVec3){5.0f,5.0f,5.0f}, (plVec3){3.0f,4.0f,3.0f}, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 0.02f, 20);
        ptDrawApi->add_3d_bezier_cubic(&ptAppData->drawlist3d, (plVec3){0.0f,0.0f,0.0f}, (plVec3){-0.5f,1.0f,-0.5f}, (plVec3){5.0f,3.5f,5.0f}, (plVec3){3.0f,4.0f,3.0f}, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 0.02f, 20);

        if(pl_is_mouse_clicked(PL_MOUSE_BUTTON_RIGHT, false))
            pl__select_entity(ptAppData);

        // ui
        if(ptUi->begin_window("Offscreen", NULL, true))
        {
            ptUi->layout_static(720.0f / 2.0f, 1280.0f / 2.0f, 1);
            ptUi->image(ptAppData->sbtTextures[ptGraphics->tSwapchain.uCurrentImageIndex], (plVec2){1280.0f / 2.0f, 720.0f / 2.0f});
            ptUi->end_window();
        }

        pl__show_main_window(ptAppData);

        if(ptAppData->bShowEcs)
            pl__show_ecs_window(ptAppData);

        ptAppData->ptDebugApi->show_windows(&ptAppData->tDebugInfo);

        if(ptAppData->bShowUiDemo)
        {
            pl_begin_profile_sample("ui demo");
            ptUi->demo(&ptAppData->bShowUiDemo);
            pl_end_profile_sample();
        }
            
        if(ptAppData->bShowUiStyle)
            ptUi->style(&ptAppData->bShowUiStyle);

        if(ptAppData->bShowUiDebug)
            ptUi->debug(&ptAppData->bShowUiDebug);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~renderer begin frame~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~submit draws~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // submit draw layers
        ptDrawApi->submit_layer(ptAppData->bgDrawLayer);
        ptDrawApi->submit_layer(ptAppData->fgDrawLayer);
        
        ptGfx->begin_recording(ptGraphics);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~scene prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        ptRendererApi->reset_scene(ptScene);
        ptEcs->run_mesh_update_system(&ptAppData->tComponentLibrary);
        ptEcs->run_hierarchy_update_system(&ptAppData->tComponentLibrary);
        ptEcs->run_object_update_system(&ptAppData->tComponentLibrary);
        ptRendererApi->scene_prepare(&ptAppData->tScene);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~offscreen target~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        ptRendererApi->begin_render_target(ptGfx, ptGraphics, &ptAppData->tOffscreenTarget);
        ptRendererApi->scene_bind_target(ptScene, &ptAppData->tOffscreenTarget);
        ptRendererApi->prepare_scene_gpu_data(ptScene);
        ptRendererApi->scene_bind_camera(ptScene, ptOffscreenCamera);
        ptRendererApi->draw_scene(ptScene);
        ptRendererApi->draw_sky(ptScene);

        ptDrawApi->submit_layer(ptAppData->offscreenDrawLayer);
        ptVulkanDrawApi->submit_drawlist_ex(&ptAppData->drawlist2, 1280.0f, 720.0f, ptCurrentFrame->tCmdBuf, 
            (uint32_t)ptGraphics->szCurrentFrameIndex, 
            ptDevice->sbtRenderPasses[ptAppData->uOffscreenPass]._tRenderPass, 
            VK_SAMPLE_COUNT_1_BIT);
        ptRendererApi->end_render_target(ptGfx, ptGraphics);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~main target~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        
        ptRendererApi->begin_render_target(ptGfx, ptGraphics, &ptAppData->tMainTarget);
        ptRendererApi->scene_bind_target(ptScene, &ptAppData->tMainTarget);
        ptRendererApi->prepare_scene_gpu_data(ptScene);
        ptRendererApi->scene_bind_camera(ptScene, ptCamera);
        ptRendererApi->draw_scene(ptScene);
        ptRendererApi->draw_sky(ptScene);

        // submit 3D draw list
        const plMat4 tMVP = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);
        ptVulkanDrawApi->submit_3d_drawlist(&ptAppData->drawlist3d, (float)ptIoCtx->afMainViewportSize[0], (float)ptIoCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptGraphics->szCurrentFrameIndex, &tMVP, PL_PIPELINE_FLAG_DEPTH_TEST);

        ptDrawCtx->tFrameBufferScale.x = ptIoCtx->afMainFramebufferScale[0];
        ptDrawCtx->tFrameBufferScale.y = ptIoCtx->afMainFramebufferScale[1];

        // submit draw lists
        ptVulkanDrawApi->submit_drawlist(&ptAppData->drawlist, (float)ptIoCtx->afMainViewportSize[0], (float)ptIoCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptGraphics->szCurrentFrameIndex);

        // submit ui drawlist
        ptUi->render();

        ptVulkanDrawApi->submit_drawlist(ptUi->get_draw_list(NULL), (float)ptIoCtx->afMainViewportSize[0], (float)ptIoCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptGraphics->szCurrentFrameIndex);
        ptVulkanDrawApi->submit_drawlist(ptUi->get_debug_draw_list(NULL), (float)ptIoCtx->afMainViewportSize[0], (float)ptIoCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptGraphics->szCurrentFrameIndex);
        ptRendererApi->end_render_target(ptGfx, ptGraphics);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~pick target~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        ptRendererApi->begin_render_target(ptGfx, ptGraphics, &ptAppData->tRenderer.tPickTarget);
        ptRendererApi->scene_bind_target(ptScene, &ptAppData->tRenderer.tPickTarget);
        ptRendererApi->scene_bind_camera(ptScene, ptCamera);
        ptRendererApi->draw_pick_scene(ptScene);
        ptRendererApi->end_render_target(ptGfx, ptGraphics);

        ptGfx->end_recording(ptGraphics);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~end frame~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        ptGfx->end_frame(ptGraphics);
    } 
    pl_end_profile_sample();
    pl_end_profile_frame();
}

static void
pl__show_ecs_window(plAppData* ptAppData)
{
    plUiApiI* ptUi = ptAppData->ptUi;
    plEcsI* ptEcs  = ptAppData->ptEcs;

    if(ptUi->begin_window("Components", &ptAppData->bShowEcs, false))
    {
        const float pfRatios[] = {1.0f};
        ptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);

        if(ptAppData->iSelectedEntity > 0)
        {

            if(ptUi->collapsing_header("Tag"))
            {
                plTagComponent* ptTagComponent = ptEcs->get_component(&ptAppData->tComponentLibrary.tTagComponentManager, ptAppData->iSelectedEntity);
                ptUi->text("Name: %s", ptTagComponent->acName);
                ptUi->end_collapsing_header();
            }

            if(ptEcs->has_entity(&ptAppData->tComponentLibrary.tHierarchyComponentManager, ptAppData->iSelectedEntity))
            {
                
                if(ptUi->collapsing_header("Transform"))
                {
                    plTransformComponent* ptTransformComponent = ptEcs->get_component(&ptAppData->tComponentLibrary.tTransformComponentManager, ptAppData->iSelectedEntity);
                    ptUi->text("Rotation: %0.3f, %0.3f, %0.3f, %0.3f", ptTransformComponent->tRotation.x, ptTransformComponent->tRotation.y, ptTransformComponent->tRotation.z, ptTransformComponent->tRotation.w);
                    ptUi->text("Scale: %0.3f, %0.3f, %0.3f", ptTransformComponent->tScale.x, ptTransformComponent->tScale.y, ptTransformComponent->tScale.z);
                    ptUi->text("Translation: %0.3f, %0.3f, %0.3f", ptTransformComponent->tTranslation.x, ptTransformComponent->tTranslation.y, ptTransformComponent->tTranslation.z);
                    ptUi->end_collapsing_header();
                }  
            }

            if(ptEcs->has_entity(&ptAppData->tComponentLibrary.tMeshComponentManager, ptAppData->iSelectedEntity))
            {
                
                if(ptUi->collapsing_header("Mesh"))
                {
                    // plMeshComponent* ptMeshComponent = pl_ecs_get_component(ptScene->ptMeshComponentManager, iSelectedEntity);
                    ptUi->end_collapsing_header();
                }  
            }

            if(ptEcs->has_entity(&ptAppData->tComponentLibrary.tMaterialComponentManager, ptAppData->iSelectedEntity))
            {
                if(ptUi->collapsing_header("Material"))
                {
                    plMaterialComponent* ptMaterialComponent = ptEcs->get_component(&ptAppData->tComponentLibrary.tMaterialComponentManager, ptAppData->iSelectedEntity);
                    ptUi->text("Albedo: %0.3f, %0.3f, %0.3f, %0.3f", ptMaterialComponent->tAlbedo.r, ptMaterialComponent->tAlbedo.g, ptMaterialComponent->tAlbedo.b, ptMaterialComponent->tAlbedo.a);
                    ptUi->text("Alpha Cutoff: %0.3f", ptMaterialComponent->fAlphaCutoff);
                    ptUi->text("Double Sided: %s", ptMaterialComponent->bDoubleSided ? "true" : "false");
                    ptUi->end_collapsing_header();
                }  
            }

            if(ptEcs->has_entity(&ptAppData->tComponentLibrary.tObjectComponentManager, ptAppData->iSelectedEntity))
            {
                if(ptUi->collapsing_header("Object"))
                {
                    plObjectComponent* ptObjectComponent = ptEcs->get_component(&ptAppData->tComponentLibrary.tObjectComponentManager, ptAppData->iSelectedEntity);
                    plTagComponent* ptTransformTag = ptEcs->get_component(&ptAppData->tComponentLibrary.tTagComponentManager, ptObjectComponent->tTransform);
                    plTagComponent* ptMeshTag = ptEcs->get_component(&ptAppData->tComponentLibrary.tTagComponentManager, ptObjectComponent->tMesh);
                    ptUi->text("Mesh: %s", ptMeshTag->acName);
                    ptUi->text("Transform: %s", ptTransformTag->acName);

                    ptUi->end_collapsing_header();
                }  
            }

            if(ptEcs->has_entity(&ptAppData->tComponentLibrary.tCameraComponentManager, ptAppData->iSelectedEntity))
            {
                if(ptUi->collapsing_header("Camera"))
                {
                    plCameraComponent* ptCameraComponent = ptEcs->get_component(&ptAppData->tComponentLibrary.tCameraComponentManager, ptAppData->iSelectedEntity);
                    ptUi->text("Pitch: %0.3f", ptCameraComponent->fPitch);
                    ptUi->text("Yaw: %0.3f", ptCameraComponent->fYaw);
                    ptUi->text("Roll: %0.3f", ptCameraComponent->fRoll);
                    ptUi->text("Near Z: %0.3f", ptCameraComponent->fNearZ);
                    ptUi->text("Far Z: %0.3f", ptCameraComponent->fFarZ);
                    ptUi->text("Y Field Of View: %0.3f", ptCameraComponent->fFieldOfView);
                    ptUi->text("Aspect Ratio: %0.3f", ptCameraComponent->fAspectRatio);
                    ptUi->text("Up Vector: %0.3f, %0.3f, %0.3f", ptCameraComponent->_tUpVec.x, ptCameraComponent->_tUpVec.y, ptCameraComponent->_tUpVec.z);
                    ptUi->text("Forward Vector: %0.3f, %0.3f, %0.3f", ptCameraComponent->_tForwardVec.x, ptCameraComponent->_tForwardVec.y, ptCameraComponent->_tForwardVec.z);
                    ptUi->text("Right Vector: %0.3f, %0.3f, %0.3f", ptCameraComponent->_tRightVec.x, ptCameraComponent->_tRightVec.y, ptCameraComponent->_tRightVec.z);
                    ptUi->end_collapsing_header();
                }  
            }

            if(ptEcs->has_entity(&ptAppData->tComponentLibrary.tHierarchyComponentManager, ptAppData->iSelectedEntity))
            {
                if(ptUi->collapsing_header("Hierarchy"))
                {
                    plHierarchyComponent* ptHierarchyComponent = ptEcs->get_component(&ptAppData->tComponentLibrary.tHierarchyComponentManager, ptAppData->iSelectedEntity);
                    plTagComponent* ptParent = ptEcs->get_component(&ptAppData->tComponentLibrary.tTagComponentManager, ptHierarchyComponent->tParent);
                    ptUi->text("Parent: %s", ptParent->acName);
                    ptUi->end_collapsing_header();
                }  
            } 
        }
        ptUi->end_window();
    }
}

static void
pl__select_entity(plAppData* ptAppData)
{
    plEcsI* ptEcs = ptAppData->ptEcs;
    plDeviceApiI* ptDeviceApi = ptAppData->ptDeviceApi;
    plDevice* ptDevice = &ptAppData->tGraphics.tDevice;
    plIOContext* ptIoCtx = pl_get_io_context();
    plGraphics* ptGraphics = &ptAppData->tGraphics;
    plRenderer* ptRenderer = &ptAppData->tRenderer;
    plComponentLibrary* ptComponentLibrary = &ptAppData->tComponentLibrary;

    uint32_t uReadBackBuffer = ptDeviceApi->create_read_back_buffer(ptDevice, (size_t)(ptIoCtx->afMainViewportSize[0] * ptIoCtx->afMainViewportSize[1]) * 4, "pick readback");
    ptDeviceApi->transfer_image_to_buffer(ptDevice, ptDevice->sbtBuffers[uReadBackBuffer].tBuffer, 
        (size_t)(ptIoCtx->afMainViewportSize[0] * ptIoCtx->afMainViewportSize[1]) * 4, 
        &ptDevice->sbtTextures[ptDevice->sbtTextureViews[ptRenderer->tPickTarget.sbuColorTextureViews[ptGraphics->tSwapchain.uCurrentImageIndex]].uTextureHandle]);

    unsigned char* mapping = (unsigned char*)ptDevice->sbtBuffers[uReadBackBuffer].tAllocation.pHostMapped;

    const plVec2 tMousePos = pl_get_mouse_pos();
    
    uint32_t uRowWidth = (uint32_t)ptIoCtx->afMainViewportSize[0] * 4;
    uint32_t uPos = uRowWidth * (uint32_t)tMousePos.y + (uint32_t)tMousePos.x * 4;

    static uint32_t uPickedID = 0;
    if(ptAppData->iSelectedEntity > 0)
    {
        ptEcs->remove_mesh_outline(ptComponentLibrary, (plEntity)ptAppData->iSelectedEntity);
        pl_sb_reset(ptRenderer->sbtVisibleOutlinedMeshes);
    }

    uPickedID = mapping[uPos] | mapping[uPos + 1] << 8 | mapping[uPos + 2] << 16;
    ptAppData->iSelectedEntity = (int)uPickedID;
    if(uPickedID > 0)
    {
        ptEcs->add_mesh_outline(ptComponentLibrary, (plEntity)uPickedID);
        pl_sb_reset(ptRenderer->sbtVisibleOutlinedMeshes);
        pl_sb_push(ptRenderer->sbtVisibleOutlinedMeshes, (plEntity)uPickedID);
    }

    ptDeviceApi->submit_buffer_for_deletion(ptDevice, uReadBackBuffer);
}

static void
pl__show_main_window(plAppData* ptAppData)
{
    plUiApiI* ptUi = ptAppData->ptUi;
    plGraphics* ptGraphics = &ptAppData->tGraphics;

    ptUi->set_next_window_pos((plVec2){0, 0}, PL_UI_COND_ONCE);

    if(ptUi->begin_window("Pilot Light", NULL, false))
    {

        const float pfRatios[] = {1.0f};
        ptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
        
        if(ptUi->collapsing_header("General"))
        {
            if(ptUi->checkbox("VSync", &ptGraphics->tSwapchain.bVSync))
                ptAppData->bVSyncChanged = true;
            ptUi->end_collapsing_header();
        }

        if(ptUi->collapsing_header("Tools"))
        {
            ptUi->checkbox("Device Memory Analyzer", &ptAppData->tDebugInfo.bShowDeviceMemoryAnalyzer);
            ptUi->checkbox("Memory Allocations", &ptAppData->tDebugInfo.bShowMemoryAllocations);
            ptUi->checkbox("Profiling", &ptAppData->tDebugInfo.bShowProfiling);
            ptUi->checkbox("Statistics", &ptAppData->tDebugInfo.bShowStats);
            ptUi->checkbox("Logging", &ptAppData->tDebugInfo.bShowLogging);
            ptUi->end_collapsing_header();
        }

        if(ptUi->collapsing_header("User Interface"))
        {
            ptUi->checkbox("UI Debug", &ptAppData->bShowUiDebug);
            ptUi->checkbox("UI Demo", &ptAppData->bShowUiDemo);
            ptUi->checkbox("UI Style", &ptAppData->bShowUiStyle);
            ptUi->end_collapsing_header();
        }

        if(ptUi->collapsing_header("Entities"))
        {
            ptUi->checkbox("Show Components", &ptAppData->bShowEcs);
            plTagComponent* sbtTagComponents = ptAppData->tComponentLibrary.tTagComponentManager.pComponents;
            for(uint32_t i = 0; i < pl_sb_size(sbtTagComponents); i++)
            {
                plTagComponent* ptTagComponent = &sbtTagComponents[i];
                ptUi->radio_button(ptTagComponent->acName, &ptAppData->iSelectedEntity, i + 1);
            }

            ptUi->end_collapsing_header();
        }
        ptUi->end_window();
    }
}

static VkSurfaceKHR
pl__create_surface(VkInstance tInstance, plIOContext* ptIoCtx)
{
    VkSurfaceKHR tSurface = VK_NULL_HANDLE;
    #ifdef _WIN32
        const VkWin32SurfaceCreateInfoKHR tSurfaceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .pNext = NULL,
            .flags = 0,
            .hinstance = GetModuleHandle(NULL),
            .hwnd = *(HWND*)ptIoCtx->pBackendPlatformData
        };
        PL_VULKAN(vkCreateWin32SurfaceKHR(tInstance, &tSurfaceCreateInfo, NULL, &tSurface));
    #elif defined(__APPLE__)
        const VkMetalSurfaceCreateInfoEXT tSurfaceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
            .pLayer = (CAMetalLayer*)ptIoCtx->pBackendPlatformData
        };
        PL_VULKAN(vkCreateMetalSurfaceEXT(tInstance, &tSurfaceCreateInfo, NULL, &tSurface));
    #else // linux
        struct tPlatformData { xcb_connection_t* ptConnection; xcb_window_t tWindow;};
        struct tPlatformData* ptPlatformData = (struct tPlatformData*)ptIoCtx->pBackendPlatformData;
        const VkXcbSurfaceCreateInfoKHR tSurfaceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
            .pNext = NULL,
            .flags = 0,
            .window = ptPlatformData->tWindow,
            .connection = ptPlatformData->ptConnection
        };
        PL_VULKAN(vkCreateXcbSurfaceKHR(tInstance, &tSurfaceCreateInfo, NULL, &tSurface));
    #endif   
    return tSurface; 
}
