/*
   vulkan_app.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] global apis
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#define PL_MATH_INCLUDE_FUNCTIONS
#include <string.h> // memset

// core
#include "pilotlight.h"
#include "pl_profile.h"
#include "pl_log.h"
#include "pl_ds.h"
#include "pl_io.h"
#include "pl_os.h"
#include "pl_memory.h"
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
    plGraphics      tGraphics;
    plDrawList      drawlist;
    plDrawList      drawlist2;
    plDrawList3D    drawlist3d;
    plDrawLayer*    fgDrawLayer;
    plDrawLayer*    bgDrawLayer;
    plDrawLayer*    offscreenDrawLayer;
    plFontAtlas     fontAtlas;
    bool            bShowUiDemo;
    bool            bShowUiDebug;
    bool            bShowUiStyle;
    bool            bShowEcs;

    // allocators
    plTempAllocator tTempAllocator;
    
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

    // lights
    plEntity tLightEntity;

    plDebugApiInfo     tDebugInfo;
    int                iSelectedEntity;
    bool               bVSyncChanged;

} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] global apis
//-----------------------------------------------------------------------------

const plApiRegistryApiI*       gptApiRegistry       = NULL;
const plDataRegistryApiI*      gptDataRegistry      = NULL;
const plLibraryApiI*           gptLibrary           = NULL;
const plFileApiI*              gptFile              = NULL;
const plDeviceApiI*            gptDevice            = NULL;
const plRenderBackendI*        gptBackend           = NULL;
const plImageApiI*             gptImage             = NULL;
const plGltfApiI*              gptGltf              = NULL;
const plRendererI*             gptRenderer          = NULL;
const plGraphicsApiI*          gptGfx               = NULL;
const plDrawApiI*              gptDraw              = NULL;
const plVulkanDrawApiI*        gptVulkanDraw        = NULL;
const plUiApiI*                gptUi                = NULL;
const plEcsI*                  gptEcs               = NULL;
const plCameraI*               gptCamera            = NULL;
const plStatsApiI*             gptStats             = NULL;
const plDebugApiI*             gptDebug             = NULL;
const plExtensionRegistryApiI* gptExtensionRegistry = NULL;

//-----------------------------------------------------------------------------
// [SECTION] helper functions
//-----------------------------------------------------------------------------

void         pl__show_main_window(plAppData* ptAppData);
void         pl__show_ecs_window (plAppData* ptAppData);
void         pl__select_entity   (plAppData* ptAppData);
VkSurfaceKHR pl__create_surface  (VkInstance tInstance, plIOContext* ptIoCtx);

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(const plApiRegistryApiI* ptApiRegistry, plAppData* ptAppData)
{
    gptApiRegistry  = ptApiRegistry;
    gptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(gptDataRegistry->get_data(PL_CONTEXT_MEMORY));
    pl_set_io_context(gptDataRegistry->get_data(PL_CONTEXT_IO_NAME));

    if(ptAppData) // reload
    {
        pl_set_log_context(gptDataRegistry->get_data("log"));
        pl_set_profile_context(gptDataRegistry->get_data("profile"));

        // reload global apis
        gptLibrary    = ptApiRegistry->first(PL_API_LIBRARY);
        gptFile       = ptApiRegistry->first(PL_API_FILE);
        gptDevice     = ptApiRegistry->first(PL_API_DEVICE);
        gptBackend    = ptApiRegistry->first(PL_API_BACKEND_VULKAN);
        gptImage      = ptApiRegistry->first(PL_API_IMAGE);
        gptGltf       = ptApiRegistry->first(PL_API_GLTF);
        gptRenderer   = ptApiRegistry->first(PL_API_RENDERER);
        gptGfx        = ptApiRegistry->first(PL_API_GRAPHICS);
        gptDraw       = ptApiRegistry->first(PL_API_DRAW);
        gptVulkanDraw = ptApiRegistry->first(PL_API_VULKAN_DRAW);
        gptUi         = ptApiRegistry->first(PL_API_UI);
        gptEcs        = ptApiRegistry->first(PL_API_ECS);
        gptCamera     = ptApiRegistry->first(PL_API_CAMERA);
        gptStats      = ptApiRegistry->first(PL_API_STATS);
        gptDebug      = ptApiRegistry->first(PL_API_DEBUG);

        return ptAppData;
    }

    // allocate intial app memory
    ptAppData = PL_ALLOC(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    // add contexts to data registry (so extensions can use them)
    gptDataRegistry->set_data("profile", pl_create_profile_context());
    gptDataRegistry->set_data("log", pl_create_log_context());
    ptAppData->iSelectedEntity = 1;

    // load extensions
    gptExtensionRegistry = ptApiRegistry->first(PL_API_EXTENSION_REGISTRY);
    gptExtensionRegistry->load_from_config(ptApiRegistry, "../apps/pl_config.json");

    // load global apis
    gptLibrary    = ptApiRegistry->first(PL_API_LIBRARY);
    gptFile       = ptApiRegistry->first(PL_API_FILE);
    gptDevice     = ptApiRegistry->first(PL_API_DEVICE);
    gptBackend    = ptApiRegistry->first(PL_API_BACKEND_VULKAN);
    gptImage      = ptApiRegistry->first(PL_API_IMAGE);
    gptGltf       = ptApiRegistry->first(PL_API_GLTF);
    gptRenderer   = ptApiRegistry->first(PL_API_RENDERER);
    gptGfx        = ptApiRegistry->first(PL_API_GRAPHICS);
    gptDraw       = ptApiRegistry->first(PL_API_DRAW);
    gptVulkanDraw = ptApiRegistry->first(PL_API_VULKAN_DRAW);
    gptUi         = ptApiRegistry->first(PL_API_UI);
    gptEcs        = ptApiRegistry->first(PL_API_ECS);
    gptCamera     = ptApiRegistry->first(PL_API_CAMERA);
    gptStats      = ptApiRegistry->first(PL_API_STATS);
    gptDebug      = ptApiRegistry->first(PL_API_DEBUG);

    // for convience
    plGraphics*         ptGraphics         = &ptAppData->tGraphics;
    plRenderBackend*    ptBackend          = &ptAppData->tBackend;
    plDevice*           ptDevice           = &ptGraphics->tDevice;
    plRenderer*         ptRenderer         = &ptAppData->tRenderer;
    plScene*            ptScene            = &ptAppData->tScene;
    plComponentLibrary* ptComponentLibrary = &ptAppData->tComponentLibrary;
    
    // contexts
    plIOContext* ptIoCtx = pl_get_io_context();

    // add some context to data registry (for reloads)
    gptDataRegistry->set_data("ui", gptUi->create_context());
    gptDataRegistry->set_data(PL_CONTEXT_DRAW_NAME, gptDraw->get_context());
    gptDataRegistry->set_data("device", ptDevice);

    // setup backend
    gptBackend->setup(ptApiRegistry, ptBackend, VK_API_VERSION_1_2, true);

    // create surface
    ptBackend->tSurface = pl__create_surface(ptBackend->tInstance, ptIoCtx);

    // create & init device
    gptBackend->create_device(ptBackend, ptBackend->tSurface, true, ptDevice);
    gptDevice->init(ptApiRegistry, ptDevice, 2);

    // create swapchain
    ptGraphics->tSwapchain.bVSync = true;
    gptBackend->create_swapchain(ptBackend, ptDevice, ptBackend->tSurface, (uint32_t)ptIoCtx->afMainViewportSize[0], (uint32_t)ptIoCtx->afMainViewportSize[1], &ptGraphics->tSwapchain);
    
    // setup graphics
    ptGraphics->ptBackend = ptBackend;
    gptGfx->setup(ptGraphics, ptBackend, ptApiRegistry, &ptAppData->tTempAllocator);
    
    // setup drawing api
    const plVulkanInit tVulkanInit = {
        .tPhysicalDevice  = ptDevice->tPhysicalDevice,
        .tLogicalDevice   = ptDevice->tLogicalDevice,
        .uImageCount      = ptGraphics->tSwapchain.uImageCount,
        .tRenderPass      = ptDevice->sbtRenderPasses[ptGraphics->uRenderPass]._tRenderPass,
        .tMSAASampleCount = ptGraphics->tSwapchain.tMsaaSamples,
        .uFramesInFlight  = ptGraphics->uFramesInFlight
    };
    gptVulkanDraw->initialize_context(&tVulkanInit);
    plDrawContext* ptDrawCtx = gptDraw->get_context();
    gptDraw->register_drawlist(ptDrawCtx, &ptAppData->drawlist);
    gptDraw->register_drawlist(ptDrawCtx, &ptAppData->drawlist2);
    gptDraw->register_3d_drawlist(ptDrawCtx, &ptAppData->drawlist3d);
    ptAppData->bgDrawLayer = gptDraw->request_layer(&ptAppData->drawlist, "Background Layer");
    ptAppData->fgDrawLayer = gptDraw->request_layer(&ptAppData->drawlist, "Foreground Layer");
    ptAppData->offscreenDrawLayer = gptDraw->request_layer(&ptAppData->drawlist2, "Foreground Layer");

    // create font atlas
    gptDraw->add_default_font(&ptAppData->fontAtlas);
    gptDraw->build_font_atlas(ptDrawCtx, &ptAppData->fontAtlas);
    gptUi->set_default_font(&ptAppData->fontAtlas.sbFonts[0]);
    
    // renderer
    gptEcs->init_component_library(ptApiRegistry, ptComponentLibrary);
    gptRenderer->setup_renderer(ptApiRegistry, ptComponentLibrary, ptGraphics, &ptAppData->tRenderer);
    gptRenderer->create_scene(&ptAppData->tRenderer, ptComponentLibrary, &ptAppData->tScene);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~entity IDs~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    // cameras
    ptAppData->tOffscreenCameraEntity = gptEcs->create_camera(ptComponentLibrary, "offscreen camera", (plVec3){0.0f, 0.35f, 1.2f}, PL_PI_3, 1280.0f / 720.0f, 0.1f, 10.0f);
    ptAppData->tCameraEntity = gptEcs->create_camera(ptComponentLibrary, "main camera", (plVec3){-6.211f, 3.647f, 0.827f}, PL_PI_3, ptIoCtx->afMainViewportSize[0] / ptIoCtx->afMainViewportSize[1], 0.01f, 400.0f);
    plCameraComponent* ptCamera = gptEcs->get_component(&ptComponentLibrary->tCameraComponentManager, ptAppData->tCameraEntity);
    plCameraComponent* ptCamera2 = gptEcs->get_component(&ptComponentLibrary->tCameraComponentManager, ptAppData->tOffscreenCameraEntity);
    gptCamera->set_pitch_yaw(ptCamera, -0.244f, 1.488f);
    gptCamera->set_pitch_yaw(ptCamera2, 0.0f, -PL_PI);

    // gptGltf->load(ptScene, ptComponentLibrary, "../data/glTF-Sample-Models-master/2.0/FlightHelmet/glTF/FlightHelmet.gltf");
    // gptGltf->load(ptScene, ptComponentLibrary, "../data/glTF-Sample-Models-master/2.0/DamagedHelmet/glTF/DamagedHelmet.gltf");
    gptGltf->load(ptScene, ptComponentLibrary, "../data/glTF-Sample-Models-master/2.0/Sponza/glTF/Sponza.gltf");

    plMeshComponent* sbtMeshes = (plMeshComponent*)ptComponentLibrary->tMeshComponentManager.pComponents;
    gptEcs->calculate_normals(sbtMeshes, pl_sb_size(sbtMeshes));
    gptEcs->calculate_tangents(sbtMeshes, pl_sb_size(sbtMeshes));

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~materials~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // offscreen
    plRenderPassDesc tRenderPassDesc = {
        .tColorFormat = PL_FORMAT_R8G8B8A8_UNORM,
        .tDepthFormat = gptDevice->find_depth_stencil_format(ptDevice)
    };
    ptAppData->uOffscreenPass = gptDevice->create_render_pass(ptDevice, &tRenderPassDesc, "offscreen renderpass");

    plRenderTargetDesc tRenderTargetDesc = {
        .uRenderPass = ptAppData->uOffscreenPass,
        .tSize = {1280.0f, 720.0f},
    };
    gptRenderer->create_render_target(ptGraphics, &tRenderTargetDesc, &ptAppData->tOffscreenTarget);

    for(uint32_t i = 0; i < ptGraphics->tSwapchain.uImageCount; i++)
    {
        plTextureView* ptColorTextureView = &ptDevice->sbtTextureViews[ptAppData->tOffscreenTarget.sbuColorTextureViews[i]];
        pl_sb_push(ptAppData->sbtTextures, gptVulkanDraw->add_texture(ptDrawCtx, ptColorTextureView->_tImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
    }

    gptRenderer->create_main_render_target(ptGraphics, &ptAppData->tMainTarget);

    // lights
    ptAppData->tLightEntity = gptEcs->create_light(ptComponentLibrary, "light", (plVec3){1.0f, 1.0f, 1.0f}, (plVec3){1.0f, 1.0f, 1.0f});

    gptRenderer->scene_prepare(&ptAppData->tScene);

    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{

    plGraphics*      ptGraphics = &ptAppData->tGraphics;
    plRenderBackend* ptBackend  = &ptAppData->tBackend;
    plDevice*        ptDevice   = &ptGraphics->tDevice;
    plRenderer*      ptRenderer = &ptAppData->tRenderer;

    vkDeviceWaitIdle(ptGraphics->tDevice.tLogicalDevice);

    gptDraw->cleanup_font_atlas(&ptAppData->fontAtlas);
    gptDraw->cleanup_context();
    gptUi->destroy_context(NULL);
    
    gptRenderer->cleanup_render_target(&ptAppData->tGraphics, &ptAppData->tOffscreenTarget);
    gptRenderer->cleanup_render_target(&ptAppData->tGraphics, &ptAppData->tRenderer.tPickTarget);
    gptRenderer->cleanup_scene(&ptAppData->tScene);
    gptRenderer->cleanup_renderer(&ptAppData->tRenderer);
    gptEcs->cleanup_systems(gptApiRegistry, &ptAppData->tComponentLibrary);
    gptGfx->cleanup(&ptAppData->tGraphics);
    gptBackend->cleanup_swapchain(ptBackend, ptDevice, &ptGraphics->tSwapchain);
    gptBackend->cleanup_device(&ptGraphics->tDevice);
    pl_cleanup_profile_context();
    pl_cleanup_log_context();
    pl_temp_allocator_free(&ptAppData->tTempAllocator);
    pl_sb_free(ptAppData->sbtTextures);
    PL_FREE(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plAppData* ptAppData)
{

    // for convience
    plGraphics*         ptGraphics         = &ptAppData->tGraphics;
    plRenderer*         ptRenderer         = &ptAppData->tRenderer;
    plComponentLibrary* ptComponentLibrary = &ptAppData->tComponentLibrary;

    // contexts
    plIOContext* ptIoCtx = pl_get_io_context();

    gptGfx->resize(ptGraphics);
    gptRenderer->resize(ptRenderer, ptIoCtx->afMainViewportSize[0], ptIoCtx->afMainViewportSize[1]);
    plCameraComponent* ptCamera = gptEcs->get_component(&ptComponentLibrary->tCameraComponentManager, ptAppData->tCameraEntity);
    gptCamera->set_aspect(ptCamera, ptIoCtx->afMainViewportSize[0] / ptIoCtx->afMainViewportSize[1]);
    gptRenderer->create_main_render_target(ptGraphics, &ptAppData->tMainTarget);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{

    pl_begin_profile_frame();
    pl_begin_profile_sample(__FUNCTION__);

    // for convience
    plGraphics* ptGraphics = &ptAppData->tGraphics;
    plDevice*   ptDevice   = &ptGraphics->tDevice;
    plRenderer* ptRenderer = &ptAppData->tRenderer;
    plScene*    ptScene    = &ptAppData->tScene;

    // contexts
    plIOContext*   ptIoCtx      = pl_get_io_context();
    plDrawContext* ptDrawCtx    = gptDraw->get_context();

    gptStats->new_frame();

    {
        static double* pdFrameTimeCounter = NULL;
        if(!pdFrameTimeCounter)
            pdFrameTimeCounter = gptStats->get_counter("frame rate");
        *pdFrameTimeCounter = (double)ptIoCtx->fFrameRate;
    }

    if(ptAppData->bVSyncChanged)
    {
        gptGfx->resize(&ptAppData->tGraphics);
        gptRenderer->create_main_render_target(&ptAppData->tGraphics, &ptAppData->tMainTarget);
        ptAppData->bVSyncChanged = false;
    }
    
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~input handling~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    static const float fCameraTravelSpeed = 8.0f;
    plCameraComponent* ptCamera = gptEcs->get_component(&ptAppData->tComponentLibrary.tCameraComponentManager, ptAppData->tCameraEntity);
    plCameraComponent* ptOffscreenCamera = gptEcs->get_component(&ptAppData->tComponentLibrary.tCameraComponentManager, ptAppData->tOffscreenCameraEntity);

    // camera space
    if(pl_is_key_pressed(PL_KEY_W, true)) gptCamera->translate(ptCamera,  0.0f,  0.0f,  fCameraTravelSpeed * ptIoCtx->fDeltaTime);
    if(pl_is_key_pressed(PL_KEY_S, true)) gptCamera->translate(ptCamera,  0.0f,  0.0f, -fCameraTravelSpeed* ptIoCtx->fDeltaTime);
    if(pl_is_key_pressed(PL_KEY_A, true)) gptCamera->translate(ptCamera, -fCameraTravelSpeed * ptIoCtx->fDeltaTime,  0.0f,  0.0f);
    if(pl_is_key_pressed(PL_KEY_D, true)) gptCamera->translate(ptCamera,  fCameraTravelSpeed * ptIoCtx->fDeltaTime,  0.0f,  0.0f);

    // world space
    if(pl_is_key_pressed(PL_KEY_F, true)) gptCamera->translate(ptCamera,  0.0f, -fCameraTravelSpeed * ptIoCtx->fDeltaTime,  0.0f);
    if(pl_is_key_pressed(PL_KEY_R, true)) gptCamera->translate(ptCamera,  0.0f,  fCameraTravelSpeed * ptIoCtx->fDeltaTime,  0.0f);

    plFrameContext* ptCurrentFrame = gptGfx->get_frame_resources(ptGraphics);
    
    static double* pdFrameTimeCounter = NULL;
    if(!pdFrameTimeCounter)
        pdFrameTimeCounter = gptStats->get_counter("frame");
    *pdFrameTimeCounter = (double)ptDevice->uCurrentFrame;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~begin frame~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    if(gptGfx->begin_frame(ptGraphics))
    {
        pl_begin_profile_sample("process_cleanup_queue");
        gptDevice->process_cleanup_queue(&ptGraphics->tDevice, (uint32_t)ptGraphics->szCurrentFrameIndex);
        pl_end_profile_sample();

        bool bOwnMouse = ptIoCtx->bWantCaptureMouse;
        gptUi->new_frame();

        if(!bOwnMouse && pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 1.0f))
        {
            const plVec2 tMouseDelta = pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f);
            gptCamera->rotate(ptCamera,  -tMouseDelta.y * 0.1f * ptIoCtx->fDeltaTime,  -tMouseDelta.x * 0.1f * ptIoCtx->fDeltaTime);
            pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
        }
        gptCamera->update(ptCamera);
        gptCamera->update(ptOffscreenCamera);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~3D drawing api~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        gptDraw->add_3d_transform(&ptAppData->drawlist3d, &ptOffscreenCamera->tTransformMat, 0.2f, 0.02f);
        gptDraw->add_3d_frustum(&ptAppData->drawlist3d, 
            &ptOffscreenCamera->tTransformMat, ptOffscreenCamera->fFieldOfView, ptOffscreenCamera->fAspectRatio, 
            ptOffscreenCamera->fNearZ, ptOffscreenCamera->fFarZ, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 0.02f);

        const plMat4 tTransform0 = pl_identity_mat4();
        gptDraw->add_3d_transform(&ptAppData->drawlist3d, &tTransform0, 10.0f, 0.02f);

        if(pl_is_mouse_clicked(PL_MOUSE_BUTTON_RIGHT, false))
            pl__select_entity(ptAppData);

        // ui
        if(gptUi->begin_window("Offscreen", NULL, true))
        {
            gptUi->layout_static(720.0f / 2.0f, 1280.0f / 2.0f, 1);
            gptUi->image(ptAppData->sbtTextures[ptGraphics->tSwapchain.uCurrentImageIndex], (plVec2){1280.0f / 2.0f, 720.0f / 2.0f});
            gptUi->end_window();
        }

        plLightComponent* ptLightComponent = gptEcs->get_component(&ptAppData->tComponentLibrary.tLightComponentManager, ptAppData->tLightEntity);
        if(gptUi->begin_window("Lights", NULL, false))
        {
            
            gptUi->layout_dynamic(0.0f, 1);
            gptUi->slider_float("X", &ptLightComponent->tPosition.x, -10.0f, 10.0f);
            gptUi->slider_float("Y", &ptLightComponent->tPosition.y, -10.0f, 10.0f);
            gptUi->slider_float("Z", &ptLightComponent->tPosition.z, -10.0f, 10.0f);
            gptUi->slider_float("R", &ptLightComponent->tColor.r, 0.0f, 1.0f);
            gptUi->slider_float("G", &ptLightComponent->tColor.g, 0.0f, 1.0f);
            gptUi->slider_float("B", &ptLightComponent->tColor.b, 0.0f, 1.0f);
            gptUi->end_window();
        }

        gptDraw->add_3d_point(&ptAppData->drawlist3d, ptLightComponent->tPosition, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, 0.1f, 0.01f);

        if(ptAppData->iSelectedEntity > 1)
        {

            static plVec3 tRotation = {0.0f, 0.0f, 0.0f};

            plTransformComponent* ptSelectedTransform = gptEcs->get_component(&ptAppData->tComponentLibrary.tTransformComponentManager, ptAppData->iSelectedEntity);

            // ui
            if(gptUi->begin_window("Testing", NULL, false))
            {
                gptUi->layout_dynamic(0.0f, 1);
                gptUi->slider_float("Scale", &ptSelectedTransform->tScale.x, 0.001f, 5.0f);
                gptUi->slider_float("X", &ptSelectedTransform->tTranslation.x, -10.0f, 10.0f);
                gptUi->slider_float("Y", &ptSelectedTransform->tTranslation.y, -10.0f, 10.0f);
                gptUi->slider_float("Z", &ptSelectedTransform->tTranslation.z, -10.0f, 10.0f);
                gptUi->slider_float("RX", &tRotation.x, -PL_PI, PL_PI);
                gptUi->slider_float("RY", &tRotation.y, -PL_PI, PL_PI);
                gptUi->slider_float("RZ", &tRotation.z, -PL_PI, PL_PI);
                gptUi->end_window();
            }

            const plMat4 tScale0 = pl_mat4_scale_xyz(ptSelectedTransform->tScale.x, ptSelectedTransform->tScale.x, ptSelectedTransform->tScale.x);
            const plMat4 tTranslation0 = pl_mat4_translate_vec3(ptSelectedTransform->tTranslation);
            const plMat4 tTransform2X = pl_mat4_rotate_xyz(tRotation.x, 1.0f, 0.0f, 0.0f);
            const plMat4 tTransform2Y = pl_mat4_rotate_xyz(tRotation.y, 0.0f, 1.0f, 0.0f);
            const plMat4 tTransform2Z = pl_mat4_rotate_xyz(tRotation.z, 0.0f, 0.0f, 1.0f);
            
            plMat4 tFinalTransform = pl_mul_mat4(&tTransform2Z, &tScale0);
            tFinalTransform = pl_mul_mat4(&tTransform2X, &tFinalTransform);
            tFinalTransform = pl_mul_mat4(&tTransform2Y, &tFinalTransform);
            tFinalTransform = pl_mul_mat4(&tTranslation0, &tFinalTransform);

            ptSelectedTransform->tWorld = tFinalTransform;
            ptSelectedTransform->tFinalTransform = tFinalTransform;
        }

        pl__show_main_window(ptAppData);

        if(ptAppData->bShowEcs)
            pl__show_ecs_window(ptAppData);

        gptDebug->show_windows(&ptAppData->tDebugInfo);

        if(ptAppData->bShowUiDemo)
        {
            pl_begin_profile_sample("ui demo");
            gptUi->demo(&ptAppData->bShowUiDemo);
            pl_end_profile_sample();
        }
            
        if(ptAppData->bShowUiStyle)
            gptUi->style(&ptAppData->bShowUiStyle);

        if(ptAppData->bShowUiDebug)
            gptUi->debug(&ptAppData->bShowUiDebug);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~renderer begin frame~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~submit draws~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // submit draw layers
        gptDraw->submit_layer(ptAppData->bgDrawLayer);
        gptDraw->submit_layer(ptAppData->fgDrawLayer);
        
        gptGfx->begin_recording(ptGraphics);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~scene prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        gptRenderer->reset_scene(ptScene);                                  // mark object data dirty, reset dynamic buffer offset
        gptEcs->run_hierarchy_update_system(&ptAppData->tComponentLibrary); // calculate final transforms
        gptEcs->run_object_update_system(&ptAppData->tComponentLibrary);    // set final tranforms

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~offscreen target~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        gptRenderer->begin_render_target(gptGfx, ptGraphics, &ptAppData->tOffscreenTarget);
        gptRenderer->scene_bind_target(ptScene, &ptAppData->tOffscreenTarget);
        gptRenderer->prepare_scene_gpu_data(ptScene);
        gptRenderer->scene_bind_camera(ptScene, ptOffscreenCamera);
        gptRenderer->draw_scene(ptScene);
        gptRenderer->draw_sky(ptScene);

        gptDraw->submit_layer(ptAppData->offscreenDrawLayer);
        gptVulkanDraw->submit_drawlist_ex(&ptAppData->drawlist2, 1280.0f, 720.0f, ptCurrentFrame->tCmdBuf, 
            (uint32_t)ptGraphics->szCurrentFrameIndex, 
            ptDevice->sbtRenderPasses[ptAppData->uOffscreenPass]._tRenderPass, 
            VK_SAMPLE_COUNT_1_BIT);
        gptRenderer->end_render_target(gptGfx, ptGraphics);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~main target~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        
        gptRenderer->begin_render_target(gptGfx, ptGraphics, &ptAppData->tMainTarget);
        gptRenderer->scene_bind_target(ptScene, &ptAppData->tMainTarget);
        gptRenderer->prepare_scene_gpu_data(ptScene);
        gptRenderer->scene_bind_camera(ptScene, ptCamera);
        gptRenderer->draw_scene(ptScene);
        gptRenderer->draw_sky(ptScene);

        // submit 3D draw list
        const plMat4 tMVP = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);
        gptVulkanDraw->submit_3d_drawlist(&ptAppData->drawlist3d, (float)ptIoCtx->afMainViewportSize[0], (float)ptIoCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptGraphics->szCurrentFrameIndex, &tMVP, PL_PIPELINE_FLAG_DEPTH_TEST);

        ptDrawCtx->tFrameBufferScale.x = ptIoCtx->afMainFramebufferScale[0];
        ptDrawCtx->tFrameBufferScale.y = ptIoCtx->afMainFramebufferScale[1];

        // submit draw lists
        gptVulkanDraw->submit_drawlist(&ptAppData->drawlist, (float)ptIoCtx->afMainViewportSize[0], (float)ptIoCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptGraphics->szCurrentFrameIndex);

        // submit ui drawlist
        gptUi->render();

        gptVulkanDraw->submit_drawlist(gptUi->get_draw_list(NULL), (float)ptIoCtx->afMainViewportSize[0], (float)ptIoCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptGraphics->szCurrentFrameIndex);
        gptVulkanDraw->submit_drawlist(gptUi->get_debug_draw_list(NULL), (float)ptIoCtx->afMainViewportSize[0], (float)ptIoCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptGraphics->szCurrentFrameIndex);
        gptRenderer->end_render_target(gptGfx, ptGraphics);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~pick target~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        gptRenderer->begin_render_target(gptGfx, ptGraphics, &ptAppData->tRenderer.tPickTarget);
        gptRenderer->scene_bind_target(ptScene, &ptAppData->tRenderer.tPickTarget);
        gptRenderer->scene_bind_camera(ptScene, ptCamera);
        gptRenderer->draw_pick_scene(ptScene);
        gptRenderer->end_render_target(gptGfx, ptGraphics);

        gptGfx->end_recording(ptGraphics);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~end frame~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        gptGfx->end_frame(ptGraphics);
    } 
    pl_end_profile_sample();
    pl_end_profile_frame();
}

void
pl__show_ecs_window(plAppData* ptAppData)
{
    if(gptUi->begin_window("Components", &ptAppData->bShowEcs, false))
    {
        const float pfRatios[] = {1.0f};
        gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);

        if(ptAppData->iSelectedEntity > 0)
        {

            if(gptUi->collapsing_header("Tag"))
            {
                plTagComponent* ptTagComponent = gptEcs->get_component(&ptAppData->tComponentLibrary.tTagComponentManager, ptAppData->iSelectedEntity);
                gptUi->text("Name: %s", ptTagComponent->acName);
                gptUi->end_collapsing_header();
            }

            if(gptEcs->has_entity(&ptAppData->tComponentLibrary.tHierarchyComponentManager, ptAppData->iSelectedEntity))
            {
                
                if(gptUi->collapsing_header("Transform"))
                {
                    plTransformComponent* ptTransformComponent = gptEcs->get_component(&ptAppData->tComponentLibrary.tTransformComponentManager, ptAppData->iSelectedEntity);
                    gptUi->text("Rotation: %0.3f, %0.3f, %0.3f, %0.3f", ptTransformComponent->tRotation.x, ptTransformComponent->tRotation.y, ptTransformComponent->tRotation.z, ptTransformComponent->tRotation.w);
                    gptUi->text("Scale: %0.3f, %0.3f, %0.3f", ptTransformComponent->tScale.x, ptTransformComponent->tScale.y, ptTransformComponent->tScale.z);
                    gptUi->text("Translation: %0.3f, %0.3f, %0.3f", ptTransformComponent->tTranslation.x, ptTransformComponent->tTranslation.y, ptTransformComponent->tTranslation.z);
                    gptUi->end_collapsing_header();
                }  
            }

            if(gptEcs->has_entity(&ptAppData->tComponentLibrary.tMeshComponentManager, ptAppData->iSelectedEntity))
            {
                
                if(gptUi->collapsing_header("Mesh"))
                {
                    // plMeshComponent* ptMeshComponent = pl_ecs_get_component(ptScene->ptMeshComponentManager, iSelectedEntity);
                    gptUi->end_collapsing_header();
                }  
            }

            if(gptEcs->has_entity(&ptAppData->tComponentLibrary.tMaterialComponentManager, ptAppData->iSelectedEntity))
            {
                if(gptUi->collapsing_header("Material"))
                {
                    plMaterialComponent* ptMaterialComponent = gptEcs->get_component(&ptAppData->tComponentLibrary.tMaterialComponentManager, ptAppData->iSelectedEntity);
                    gptUi->text("Albedo: %0.3f, %0.3f, %0.3f, %0.3f", ptMaterialComponent->tAlbedo.r, ptMaterialComponent->tAlbedo.g, ptMaterialComponent->tAlbedo.b, ptMaterialComponent->tAlbedo.a);
                    gptUi->text("Alpha Cutoff: %0.3f", ptMaterialComponent->fAlphaCutoff);
                    gptUi->text("Double Sided: %s", ptMaterialComponent->bDoubleSided ? "true" : "false");
                    gptUi->end_collapsing_header();
                }  
            }

            if(gptEcs->has_entity(&ptAppData->tComponentLibrary.tObjectComponentManager, ptAppData->iSelectedEntity))
            {
                if(gptUi->collapsing_header("Object"))
                {
                    plObjectComponent* ptObjectComponent = gptEcs->get_component(&ptAppData->tComponentLibrary.tObjectComponentManager, ptAppData->iSelectedEntity);
                    plTagComponent* ptTransformTag = gptEcs->get_component(&ptAppData->tComponentLibrary.tTagComponentManager, ptObjectComponent->tTransform);
                    plTagComponent* ptMeshTag = gptEcs->get_component(&ptAppData->tComponentLibrary.tTagComponentManager, ptObjectComponent->tMesh);
                    gptUi->text("Mesh: %s", ptMeshTag->acName);
                    gptUi->text("Transform: %s", ptTransformTag->acName);

                    gptUi->end_collapsing_header();
                }  
            }

            if(gptEcs->has_entity(&ptAppData->tComponentLibrary.tCameraComponentManager, ptAppData->iSelectedEntity))
            {
                if(gptUi->collapsing_header("Camera"))
                {
                    plCameraComponent* ptCameraComponent = gptEcs->get_component(&ptAppData->tComponentLibrary.tCameraComponentManager, ptAppData->iSelectedEntity);
                    gptUi->text("Pitch: %0.3f", ptCameraComponent->fPitch);
                    gptUi->text("Yaw: %0.3f", ptCameraComponent->fYaw);
                    gptUi->text("Roll: %0.3f", ptCameraComponent->fRoll);
                    gptUi->text("Near Z: %0.3f", ptCameraComponent->fNearZ);
                    gptUi->text("Far Z: %0.3f", ptCameraComponent->fFarZ);
                    gptUi->text("Y Field Of View: %0.3f", ptCameraComponent->fFieldOfView);
                    gptUi->text("Aspect Ratio: %0.3f", ptCameraComponent->fAspectRatio);
                    gptUi->text("Up Vector: %0.3f, %0.3f, %0.3f", ptCameraComponent->_tUpVec.x, ptCameraComponent->_tUpVec.y, ptCameraComponent->_tUpVec.z);
                    gptUi->text("Forward Vector: %0.3f, %0.3f, %0.3f", ptCameraComponent->_tForwardVec.x, ptCameraComponent->_tForwardVec.y, ptCameraComponent->_tForwardVec.z);
                    gptUi->text("Right Vector: %0.3f, %0.3f, %0.3f", ptCameraComponent->_tRightVec.x, ptCameraComponent->_tRightVec.y, ptCameraComponent->_tRightVec.z);
                    gptUi->end_collapsing_header();
                }  
            }

            if(gptEcs->has_entity(&ptAppData->tComponentLibrary.tHierarchyComponentManager, ptAppData->iSelectedEntity))
            {
                if(gptUi->collapsing_header("Hierarchy"))
                {
                    plHierarchyComponent* ptHierarchyComponent = gptEcs->get_component(&ptAppData->tComponentLibrary.tHierarchyComponentManager, ptAppData->iSelectedEntity);
                    plTagComponent* ptParent = gptEcs->get_component(&ptAppData->tComponentLibrary.tTagComponentManager, ptHierarchyComponent->tParent);
                    gptUi->text("Parent: %s", ptParent->acName);
                    gptUi->end_collapsing_header();
                }  
            } 
        }
        gptUi->end_window();
    }
}

void
pl__select_entity(plAppData* ptAppData)
{
    plDevice* ptDevice = &ptAppData->tGraphics.tDevice;
    plIOContext* ptIoCtx = pl_get_io_context();
    plGraphics* ptGraphics = &ptAppData->tGraphics;
    plRenderer* ptRenderer = &ptAppData->tRenderer;
    plComponentLibrary* ptComponentLibrary = &ptAppData->tComponentLibrary;

    uint32_t uReadBackBuffer = gptDevice->create_read_back_buffer(ptDevice, (size_t)(ptIoCtx->afMainViewportSize[0] * ptIoCtx->afMainViewportSize[1]) * 4, "pick readback");
    gptDevice->transfer_image_to_buffer(ptDevice, ptDevice->sbtBuffers[uReadBackBuffer].tBuffer, 
        (size_t)(ptIoCtx->afMainViewportSize[0] * ptIoCtx->afMainViewportSize[1]) * 4, 
        &ptDevice->sbtTextures[ptDevice->sbtTextureViews[ptRenderer->tPickTarget.sbuColorTextureViews[ptGraphics->tSwapchain.uCurrentImageIndex]].uTextureHandle]);

    unsigned char* mapping = (unsigned char*)ptDevice->sbtBuffers[uReadBackBuffer].tAllocation.pHostMapped;

    const plVec2 tMousePos = pl_get_mouse_pos();
    
    uint32_t uRowWidth = (uint32_t)ptIoCtx->afMainViewportSize[0] * 4;
    uint32_t uPos = uRowWidth * (uint32_t)tMousePos.y + (uint32_t)tMousePos.x * 4;

    static uint32_t uPickedID = 0;
    if(ptAppData->iSelectedEntity > 0)
    {
        gptEcs->remove_mesh_outline(ptComponentLibrary, (plEntity)ptAppData->iSelectedEntity);
        pl_sb_reset(ptRenderer->sbtVisibleOutlinedMeshes);
    }

    uPickedID = (uint32_t)gptEcs->color_to_entity((plVec4*)&mapping[uPos]);
    ptAppData->iSelectedEntity = (int)uPickedID;
    if(uPickedID > 0)
    {
        gptEcs->add_mesh_outline(ptComponentLibrary, (plEntity)uPickedID);
        pl_sb_reset(ptRenderer->sbtVisibleOutlinedMeshes);
        pl_sb_push(ptRenderer->sbtVisibleOutlinedMeshes, (plEntity)uPickedID);
    }

    gptDevice->submit_buffer_for_deletion(ptDevice, uReadBackBuffer);
}

void
pl__show_main_window(plAppData* ptAppData)
{
    plGraphics* ptGraphics = &ptAppData->tGraphics;

    gptUi->set_next_window_pos((plVec2){0, 0}, PL_UI_COND_ONCE);

    if(gptUi->begin_window("Pilot Light", NULL, false))
    {

        const float pfRatios[] = {1.0f};
        gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
        
        if(gptUi->collapsing_header("General"))
        {
            if(gptUi->checkbox("VSync", &ptGraphics->tSwapchain.bVSync))
                ptAppData->bVSyncChanged = true;
            gptUi->end_collapsing_header();
        }

        if(gptUi->collapsing_header("Tools"))
        {
            gptUi->checkbox("Device Memory Analyzer", &ptAppData->tDebugInfo.bShowDeviceMemoryAnalyzer);
            gptUi->checkbox("Memory Allocations", &ptAppData->tDebugInfo.bShowMemoryAllocations);
            gptUi->checkbox("Profiling", &ptAppData->tDebugInfo.bShowProfiling);
            gptUi->checkbox("Statistics", &ptAppData->tDebugInfo.bShowStats);
            gptUi->checkbox("Logging", &ptAppData->tDebugInfo.bShowLogging);
            gptUi->end_collapsing_header();
        }

        if(gptUi->collapsing_header("User Interface"))
        {
            gptUi->checkbox("UI Debug", &ptAppData->bShowUiDebug);
            gptUi->checkbox("UI Demo", &ptAppData->bShowUiDemo);
            gptUi->checkbox("UI Style", &ptAppData->bShowUiStyle);
            gptUi->end_collapsing_header();
        }

        if(gptUi->collapsing_header("Entities"))
        {
            gptUi->checkbox("Show Components", &ptAppData->bShowEcs);
            plTagComponent* sbtTagComponents = ptAppData->tComponentLibrary.tTagComponentManager.pComponents;
            for(uint32_t i = 0; i < pl_sb_size(sbtTagComponents); i++)
            {
                plTagComponent* ptTagComponent = &sbtTagComponents[i];
                gptUi->radio_button(ptTagComponent->acName, &ptAppData->iSelectedEntity, i + 1);
            }

            gptUi->end_collapsing_header();
        }
        gptUi->end_window();
    }
}

VkSurfaceKHR
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
