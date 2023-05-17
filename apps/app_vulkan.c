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

// pl_ds.h allocators (so they can be tracked)
#define PL_DS_ALLOC(x, FILE, LINE) pl_alloc((x), FILE, LINE)
#define PL_DS_FREE(x)  pl_free((x))

#include <string.h> // memset
#include "pilotlight.h"
#include "pl_profile.h"
#include "pl_log.h"
#include "pl_ds.h"
#include "pl_io.h"
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
    bool         bShowUiMemory;

    // allocators
    plTempAllocator tTempAllocator;

    // apis
    plIOApiI*            ptIoI;
    plLibraryApiI*       ptLibraryApi;
    plFileApiI*          ptFileApi;
    plRendererI*         ptRendererApi;
    plGraphicsApiI*      ptGfx;
    plDrawApiI*          ptDrawApi;
    plVulkanDrawApiI*    ptVulkanDrawApi;
    plUiApiI*            ptUi;
    plUiApiI*            ptDebugApi;
    plDeviceApiI*        ptDeviceApi;
    plEcsI*              ptEcs;
    plCameraI*           ptCameraApi;
    plTempAllocatorApiI* ptTempMemoryApi;
    
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

    bool bLayoutDebug;

} plAppData;

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

    if(ptAppData) // reload
    {
        pl_set_log_context(ptDataRegistry->get_data("log"));
        pl_set_profile_context(ptDataRegistry->get_data("profile"));
    
        ptAppData->ptRendererApi   = ptApiRegistry->first(PL_API_RENDERER);
        ptAppData->ptGfx           = ptApiRegistry->first(PL_API_GRAPHICS);
        ptAppData->ptDrawApi       = ptApiRegistry->first(PL_API_DRAW);
        ptAppData->ptVulkanDrawApi = ptApiRegistry->first(PL_API_VULKAN_DRAW);
        ptAppData->ptUi            = ptApiRegistry->first(PL_API_UI);
        ptAppData->ptIoI           = ptApiRegistry->first(PL_API_IO);
        ptAppData->ptDeviceApi     = ptApiRegistry->first(PL_API_DEVICE);
        ptAppData->ptEcs           = ptApiRegistry->first(PL_API_ECS);
        ptAppData->ptCameraApi     = ptApiRegistry->first(PL_API_CAMERA);
        ptAppData->ptTempMemoryApi = ptApiRegistry->first(PL_API_TEMP_ALLOCATOR);

        ptAppData->ptUi->set_draw_api(ptAppData->ptDrawApi);
        return ptAppData;
    }

    // allocate original app memory
    ptAppData = malloc(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));
    ptAppData->ptApiRegistry = ptApiRegistry;
    pl_set_memory_context(ptDataRegistry->get_data("memory"));

    // load extensions
    plExtensionRegistryApiI* ptExtensionRegistry = ptApiRegistry->first(PL_API_EXTENSION_REGISTRY);
    ptExtensionRegistry->load_from_config(ptApiRegistry, "../src/pl_config.json");

    // load apis 
    plIOApiI*            ptIoI           = ptApiRegistry->first(PL_API_IO);
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
    plUiApiI*            ptDebugApi      = ptApiRegistry->next(ptUi);
    plEcsI*              ptEcs           = ptApiRegistry->first(PL_API_ECS);
    plCameraI*           ptCameraApi     = ptApiRegistry->first(PL_API_CAMERA);
    plTempAllocatorApiI* ptTempMemoryApi = ptApiRegistry->first(PL_API_TEMP_ALLOCATOR);

    // save apis that are used often
    ptAppData->ptRendererApi = ptRendererApi;
    ptAppData->ptGfx = ptGfx;
    ptAppData->ptDrawApi = ptDrawApi;
    ptAppData->ptVulkanDrawApi = ptVulkanDrawApi;
    ptAppData->ptUi = ptUi;
    ptAppData->ptDebugApi = ptDebugApi;
    ptAppData->ptDeviceApi = ptDeviceApi;
    ptAppData->ptIoI = ptIoI;
    ptAppData->ptEcs = ptEcs;
    ptAppData->ptCameraApi = ptCameraApi;
    ptAppData->ptTempMemoryApi = ptTempMemoryApi;

    // for convience
    plGraphics*         ptGraphics         = &ptAppData->tGraphics;
    plRenderBackend*    ptBackend          = &ptAppData->tBackend;
    plDevice*           ptDevice           = &ptGraphics->tDevice;
    plRenderer*         ptRenderer         = &ptAppData->tRenderer;
    plScene*            ptScene            = &ptAppData->tScene;
    plComponentLibrary* ptComponentLibrary = &ptAppData->tComponentLibrary;
    
    // contexts
    plIOContext*      ptIoCtx      = ptIoI->get_context();
    plProfileContext* ptProfileCtx = pl_create_profile_context();
    plLogContext*     ptLogCtx     = pl_create_log_context();
    plUiContext*      ptUiContext  = ptUi->create_context(ptIoI, ptDrawApi);
    plDrawContext*    ptDrawCtx    = ptUi->get_draw_context(NULL);

    // add some context to data registry
    ptDataRegistry->set_data("profile", ptProfileCtx);
    ptDataRegistry->set_data("log", ptLogCtx);
    ptDataRegistry->set_data("ui", ptUiContext);
    ptDataRegistry->set_data("draw", ptDrawCtx);

    // setup log channels
    pl_add_log_channel("Default", PL_CHANNEL_TYPE_CONSOLE);
    pl_log_info("Setup logging");

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
    ptVulkanDrawApi->initialize_context(ptDrawCtx, &tVulkanInit);
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

    ptGltfApi->load(ptScene, ptComponentLibrary, "../data/glTF-Sample-Models-master/2.0/DamagedHelmet/glTF/DamagedHelmet.gltf");
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
        pl_sb_push(ptAppData->sbtTextures, ptVulkanDrawApi->add_texture(ptUi->get_draw_context(NULL), ptColorTextureView->_tImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
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
    plTempAllocatorApiI*    tempAlloc = ptAppData->ptApiRegistry->first(PL_API_TEMP_ALLOCATOR);
    plTempAllocatorApiI*           memoryApi = ptAppData->ptApiRegistry->first(PL_API_TEMP_ALLOCATOR);
    plDeviceApiI*           deviceApi = ptAppData->ptApiRegistry->first(PL_API_DEVICE);
    plRenderBackendI*       ptBackendApi = ptAppData->ptApiRegistry->first(PL_API_BACKEND_VULKAN);
    plEcsI*                 ptEcs = ptAppData->ptEcs;
    ptDrawApi->cleanup_font_atlas(&ptAppData->fontAtlas);
    ptUiApi->destroy_context(NULL);
    // ptRendererApi->(&ptAppData->tGraphics, &ptAppData->tOffscreenPass);
    ptRendererApi->cleanup_render_target(&ptAppData->tGraphics, &ptAppData->tOffscreenTarget);
    ptRendererApi->cleanup_renderer(&ptAppData->tRenderer);
    ptEcs->cleanup_systems(ptAppData->ptApiRegistry, &ptAppData->tComponentLibrary);
    ptGfx->cleanup(&ptAppData->tGraphics);
    ptBackendApi->cleanup_device(&ptGraphics->tDevice);
    pl_cleanup_profile_context();
    pl_cleanup_log_context();
    tempAlloc->free(&ptAppData->tTempAllocator);
    free(pAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(void* pAppData)
{
    plAppData* ptAppData = pAppData;

    // load apis 
    plIOApiI*         ptIoI           = ptAppData->ptIoI;
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
    plIOContext*      ptIoCtx      = ptAppData->ptIoI->get_context();
    plProfileContext* ptProfileCtx = pl_get_profile_context();
    plLogContext*     ptLogCtx     = pl_get_log_context();
    plDrawContext*    ptDrawCtx    = ptAppData->ptUi->get_draw_context(NULL);

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

    // load apis 
    plIOApiI*         ptIoI           = ptAppData->ptIoI;
    plDeviceApiI*     ptDeviceApi     = ptAppData->ptDeviceApi;
    plRendererI*      ptRendererApi   = ptAppData->ptRendererApi;
    plGraphicsApiI*   ptGfx           = ptAppData->ptGfx;
    plDrawApiI*       ptDrawApi       = ptAppData->ptDrawApi;
    plVulkanDrawApiI* ptVulkanDrawApi = ptAppData->ptVulkanDrawApi;
    plUiApiI*         ptUi            = ptAppData->ptUi;
    plEcsI*           ptEcs           = ptAppData->ptEcs;
    plCameraI*        ptCameraApi     = ptAppData->ptCameraApi;
    plTempAllocatorApiI*     ptTempMemoryApi     = ptAppData->ptTempMemoryApi;

    // for convience
    plGraphics*         ptGraphics         = &ptAppData->tGraphics;
    plRenderBackend*    ptBackend          = &ptAppData->tBackend;
    plDevice*           ptDevice           = &ptGraphics->tDevice;
    plRenderer*         ptRenderer         = &ptAppData->tRenderer;
    plScene*            ptScene            = &ptAppData->tScene;
    plComponentLibrary* ptComponentLibrary = &ptAppData->tComponentLibrary;

    // contexts
    plIOContext*      ptIoCtx      = ptAppData->ptIoI->get_context();
    plProfileContext* ptProfileCtx = pl_get_profile_context();
    plLogContext*     ptLogCtx     = pl_get_log_context();
    plDrawContext*    ptDrawCtx    = ptAppData->ptUi->get_draw_context(NULL);

    if(ptAppData->bLayoutDebug)
        ptUi = ptAppData->ptDebugApi;

    static bool bVSyncChanged = false;

    if(bVSyncChanged)
    {
        ptGfx->resize(&ptAppData->tGraphics);
        ptRendererApi->create_main_render_target(&ptAppData->tGraphics, &ptAppData->tMainTarget);
        bVSyncChanged = false;
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~frame setup~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    plIOApiI* pTIoI = ptAppData->ptIoI;
    plIOContext* ptIOCtx = pTIoI->get_context();
    pl_begin_profile_frame();
    
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~input handling~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    static const float fCameraTravelSpeed = 8.0f;
    plCameraComponent* ptCamera = ptEcs->get_component(&ptAppData->tComponentLibrary.tCameraComponentManager, ptAppData->tCameraEntity);
    plCameraComponent* ptOffscreenCamera = ptEcs->get_component(&ptAppData->tComponentLibrary.tCameraComponentManager, ptAppData->tOffscreenCameraEntity);

    // camera space
    if(pTIoI->is_key_pressed(PL_KEY_W, true)) ptCameraApi->translate(ptCamera,  0.0f,  0.0f,  fCameraTravelSpeed * ptIOCtx->fDeltaTime);
    if(pTIoI->is_key_pressed(PL_KEY_S, true)) ptCameraApi->translate(ptCamera,  0.0f,  0.0f, -fCameraTravelSpeed* ptIOCtx->fDeltaTime);
    if(pTIoI->is_key_pressed(PL_KEY_A, true)) ptCameraApi->translate(ptCamera, -fCameraTravelSpeed * ptIOCtx->fDeltaTime,  0.0f,  0.0f);
    if(pTIoI->is_key_pressed(PL_KEY_D, true)) ptCameraApi->translate(ptCamera,  fCameraTravelSpeed * ptIOCtx->fDeltaTime,  0.0f,  0.0f);

    // world space
    if(pTIoI->is_key_pressed(PL_KEY_F, true)) ptCameraApi->translate(ptCamera,  0.0f, -fCameraTravelSpeed * ptIOCtx->fDeltaTime,  0.0f);
    if(pTIoI->is_key_pressed(PL_KEY_R, true)) ptCameraApi->translate(ptCamera,  0.0f,  fCameraTravelSpeed * ptIOCtx->fDeltaTime,  0.0f);

    plFrameContext* ptCurrentFrame = ptGfx->get_frame_resources(ptGraphics);
    static uint32_t uPickedID = 0;
    static int iSelectedEntity = 1;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~begin frame~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    if(ptGfx->begin_frame(ptGraphics))
    {
        ptDeviceApi->process_cleanup_queue(&ptGraphics->tDevice, (uint32_t)ptGraphics->szCurrentFrameIndex);
        ptUi->new_frame();

        if(!ptUi->is_mouse_owned() && pTIoI->is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 1.0f))
        {
            const plVec2 tMouseDelta = pTIoI->get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f);
            ptCameraApi->rotate(ptCamera,  -tMouseDelta.y * 0.1f * ptIOCtx->fDeltaTime,  -tMouseDelta.x * 0.1f * ptIOCtx->fDeltaTime);
            pTIoI->reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
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

        if(pTIoI->is_mouse_clicked(PL_MOUSE_BUTTON_RIGHT, false))
        {
            uint32_t uReadBackBuffer = ptDeviceApi->create_read_back_buffer(ptDevice, (size_t)(ptIOCtx->afMainViewportSize[0] * ptIOCtx->afMainViewportSize[1]) * 4, "pick readback");
            ptDeviceApi->transfer_image_to_buffer(ptDevice, ptDevice->sbtBuffers[uReadBackBuffer].tBuffer, 
                (size_t)(ptIOCtx->afMainViewportSize[0] * ptIOCtx->afMainViewportSize[1]) * 4, 
                &ptDevice->sbtTextures[ptDevice->sbtTextureViews[ptRenderer->tPickTarget.sbuColorTextureViews[ptGraphics->tSwapchain.uCurrentImageIndex]].uTextureHandle]);

            unsigned char* mapping = (unsigned char*)ptDevice->sbtBuffers[uReadBackBuffer].tAllocation.pHostMapped;

            const plVec2 tMousePos = pTIoI->get_mouse_pos();
            
            uint32_t uRowWidth = (uint32_t)ptIOCtx->afMainViewportSize[0] * 4;
            uint32_t uPos = uRowWidth * (uint32_t)tMousePos.y + (uint32_t)tMousePos.x * 4;

            if(uPickedID > 0)
                ptEcs->remove_mesh_outline(ptComponentLibrary, (plEntity)uPickedID);

            uPickedID = mapping[uPos] | mapping[uPos + 1] << 8 | mapping[uPos + 2] << 16;
            iSelectedEntity = (int)uPickedID;
            if(uPickedID > 0)
            {
                ptEcs->add_mesh_outline(ptComponentLibrary, (plEntity)uPickedID);
                pl_sb_reset(ptRenderer->sbtVisibleOutlinedMeshes);
                pl_sb_push(ptRenderer->sbtVisibleOutlinedMeshes, (plEntity)uPickedID);
            }

            ptDeviceApi->submit_buffer_for_deletion(ptDevice, uReadBackBuffer);
        }

        // ui

        if(ptUi->begin_window("Offscreen", NULL, true))
        {
            ptUi->layout_static(720.0f / 2.0f, 1280.0f / 2.0f, 1);
            ptUi->image(ptAppData->sbtTextures[ptGraphics->tSwapchain.uCurrentImageIndex], (plVec2){1280.0f / 2.0f, 720.0f / 2.0f});
            ptUi->end_window();
        }

        ptUi->set_next_window_pos((plVec2){0, 0}, PL_UI_COND_ONCE);

        if(ptUi->begin_window("Pilot Light", NULL, false))
        {
    
            const float pfRatios[] = {1.0f};
            ptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
            
            ptUi->checkbox("UI Debug", &ptAppData->bShowUiDebug);
            ptUi->checkbox("UI Demo", &ptAppData->bShowUiDemo);
            ptUi->checkbox("UI Style", &ptAppData->bShowUiStyle);
            ptUi->checkbox("Device Memory", &ptAppData->bShowUiMemory);
            
            if(ptUi->checkbox("VSync", &ptGraphics->tSwapchain.bVSync))
            {
                bVSyncChanged = true;
            }

            if(ptUi->collapsing_header("Entities"))
            {
                plTagComponent* sbtTagComponents = ptAppData->tComponentLibrary.tTagComponentManager.pComponents;
                for(uint32_t i = 0; i < pl_sb_size(sbtTagComponents); i++)
                {
                    plTagComponent* ptTagComponent = &sbtTagComponents[i];
                    ptUi->radio_button(ptTagComponent->acName, &iSelectedEntity, i + 1);
                }

                ptUi->end_collapsing_header();
            }
            ptUi->end_window();
        }

        if(iSelectedEntity > 0)
        {
            if(ptUi->begin_window("Components", NULL, false))
            {
                const float pfRatios[] = {1.0f};
                ptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);

                if(ptUi->collapsing_header("Tag"))
                {
                    plTagComponent* ptTagComponent = ptEcs->get_component(&ptAppData->tComponentLibrary.tTagComponentManager, iSelectedEntity);
                    ptUi->text("Name: %s", ptTagComponent->acName);
                    ptUi->end_collapsing_header();
                }

                if(ptEcs->has_entity(&ptAppData->tComponentLibrary.tHierarchyComponentManager, iSelectedEntity))
                {
                    
                    if(ptUi->collapsing_header("Transform"))
                    {
                        plTransformComponent* ptTransformComponent = ptEcs->get_component(&ptAppData->tComponentLibrary.tTransformComponentManager, iSelectedEntity);
                        ptUi->text("Rotation: %0.3f, %0.3f, %0.3f, %0.3f", ptTransformComponent->tRotation.x, ptTransformComponent->tRotation.y, ptTransformComponent->tRotation.z, ptTransformComponent->tRotation.w);
                        ptUi->text("Scale: %0.3f, %0.3f, %0.3f", ptTransformComponent->tScale.x, ptTransformComponent->tScale.y, ptTransformComponent->tScale.z);
                        ptUi->text("Translation: %0.3f, %0.3f, %0.3f", ptTransformComponent->tTranslation.x, ptTransformComponent->tTranslation.y, ptTransformComponent->tTranslation.z);
                        ptUi->end_collapsing_header();
                    }  
                }

                if(ptEcs->has_entity(&ptAppData->tComponentLibrary.tMeshComponentManager, iSelectedEntity))
                {
                    
                    if(ptUi->collapsing_header("Mesh"))
                    {
                        // plMeshComponent* ptMeshComponent = pl_ecs_get_component(ptScene->ptMeshComponentManager, iSelectedEntity);
                        ptUi->end_collapsing_header();
                    }  
                }

                if(ptEcs->has_entity(&ptAppData->tComponentLibrary.tMaterialComponentManager, iSelectedEntity))
                {
                    if(ptUi->collapsing_header("Material"))
                    {
                        plMaterialComponent* ptMaterialComponent = ptEcs->get_component(&ptAppData->tComponentLibrary.tMaterialComponentManager, iSelectedEntity);
                        ptUi->text("Albedo: %0.3f, %0.3f, %0.3f, %0.3f", ptMaterialComponent->tAlbedo.r, ptMaterialComponent->tAlbedo.g, ptMaterialComponent->tAlbedo.b, ptMaterialComponent->tAlbedo.a);
                        ptUi->text("Alpha Cutoff: %0.3f", ptMaterialComponent->fAlphaCutoff);
                        ptUi->text("Double Sided: %s", ptMaterialComponent->bDoubleSided ? "true" : "false");
                        ptUi->end_collapsing_header();
                    }  
                }

                if(ptEcs->has_entity(&ptAppData->tComponentLibrary.tObjectComponentManager, iSelectedEntity))
                {
                    if(ptUi->collapsing_header("Object"))
                    {
                        plObjectComponent* ptObjectComponent = ptEcs->get_component(&ptAppData->tComponentLibrary.tObjectComponentManager, iSelectedEntity);
                        plTagComponent* ptTransformTag = ptEcs->get_component(&ptAppData->tComponentLibrary.tTagComponentManager, ptObjectComponent->tTransform);
                        plTagComponent* ptMeshTag = ptEcs->get_component(&ptAppData->tComponentLibrary.tTagComponentManager, ptObjectComponent->tMesh);
                        ptUi->text("Mesh: %s", ptMeshTag->acName);
                        ptUi->text("Transform: %s", ptTransformTag->acName);

                        ptUi->end_collapsing_header();
                    }  
                }

                if(ptEcs->has_entity(&ptAppData->tComponentLibrary.tCameraComponentManager, iSelectedEntity))
                {
                    if(ptUi->collapsing_header("Camera"))
                    {
                        plCameraComponent* ptCameraComponent = ptEcs->get_component(&ptAppData->tComponentLibrary.tCameraComponentManager, iSelectedEntity);
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

                if(ptEcs->has_entity(&ptAppData->tComponentLibrary.tHierarchyComponentManager, iSelectedEntity))
                {
                    if(ptUi->collapsing_header("Hierarchy"))
                    {
                        plHierarchyComponent* ptHierarchyComponent = ptEcs->get_component(&ptAppData->tComponentLibrary.tHierarchyComponentManager, iSelectedEntity);
                        plTagComponent* ptParent = ptEcs->get_component(&ptAppData->tComponentLibrary.tTagComponentManager, ptHierarchyComponent->tParent);
                        ptUi->text("Parent: %s", ptParent->acName);
                        ptUi->end_collapsing_header();
                    }  
                }
                ptUi->end_window();
            }
        }

        if(ptUi->begin_window("Memory Allocations", NULL, false))
        {

            plMemoryContext* ptMemoryCtx = pl_get_memory_context();
            ptUi->layout_dynamic(0.0f, 1);

            ptUi->text("Active Allocations: %u", ptMemoryCtx->szActiveAllocations);
            ptUi->text("Freed Allocations: %u", pl_sb_size(ptMemoryCtx->sbtFreeAllocations));

            if(ptUi->begin_tab_bar("main toolbar"))
            {
                static char pcFile[1024] = {0};
                if(ptUi->begin_tab("Active Allocations"))
                {
                    ptUi->layout_template_begin(30.0f);
                    ptUi->layout_template_push_static(50.0f);
                    ptUi->layout_template_push_variable(300.0f);
                    ptUi->layout_template_push_variable(50.0f);
                    ptUi->layout_template_push_variable(50.0f);
                    ptUi->layout_template_end();

                    ptUi->text("%s", "Entry");
                    ptUi->text("%s", "File");
                    ptUi->text("%s", "Line");
                    ptUi->text("%s", "Size");

                    const uint32_t uOriginalAllocationCount = pl_sb_size(ptMemoryCtx->sbtAllocations);
                    
                    for(uint32_t i = 0; i < uOriginalAllocationCount; i++)
                    {
                        plAllocationEntry tEntry = ptMemoryCtx->sbtAllocations[i];
                        strncpy(pcFile, tEntry.pcFile, 1024);
                        ptUi->text("%i", i);
                        ptUi->text("%s", pcFile);
                        ptUi->text("%i", tEntry.iLine);
                        ptUi->text("%u", tEntry.szSize);
                    }

                    ptUi->end_tab();
                }
                ptUi->layout_dynamic(0.0f, 1);
                if(ptUi->begin_tab("Freed Allocations"))
                {
                    ptUi->layout_template_begin(30.0f);
                    ptUi->layout_template_push_static(50.0f);
                    ptUi->layout_template_push_variable(300.0f);
                    ptUi->layout_template_push_variable(50.0f);
                    ptUi->layout_template_push_variable(50.0f);
                    ptUi->layout_template_end();

                    ptUi->text("%s", "Entry");
                    ptUi->text("%s", "File");
                    ptUi->text("%s", "Line");
                    ptUi->text("%s", "Size");

                    const uint32_t uOriginalAllocationCount = pl_sb_size(ptMemoryCtx->sbtFreeAllocations);
                    for(uint32_t i = 0; i < uOriginalAllocationCount; i++)
                    {
                        plAllocationEntry tEntry = ptMemoryCtx->sbtFreeAllocations[i];
                        strncpy(pcFile, tEntry.pcFile, 1024);
                        ptUi->text("%i", i);
                        ptUi->text("%s", pcFile);
                        ptUi->text("%i", tEntry.iLine);
                        ptUi->text("%u", tEntry.szSize);
                    }
                    ptUi->end_tab();
                }
                ptUi->end_tab_bar();
            }
            ptUi->end_window();
        }

        if(ptAppData->bShowUiDemo)
            ptUi->demo(&ptAppData->bShowUiDemo);
            
        if(ptAppData->bShowUiStyle)
            ptUi->style(&ptAppData->bShowUiStyle);

        if(ptAppData->bShowUiDebug)
            ptUi->debug(&ptAppData->bShowUiDebug);

        if(ptAppData->bShowUiMemory)
        {
            if(ptUi->begin_window("Device Memory", &ptAppData->bShowUiMemory, false))
            {
                plDrawLayer* ptFgLayer = ptUi->get_window_fg_drawlayer();
                const plVec2 tWindowSize = ptUi->get_window_size();
                const plVec2 tWindowPos = ptUi->get_window_pos();
                const plVec2 tWindowEnd = pl_add_vec2(tWindowSize, tWindowPos);

                ptUi->text("Device Memory: Staging Cached");

                {

                    ptUi->layout_template_begin(30.0f);
                    ptUi->layout_template_push_static(150.0f);
                    ptUi->layout_template_push_variable(300.0f);
                    ptUi->layout_template_end();

                    uint32_t uBlockCount = 0;
                    plDeviceAllocationBlock* sbtBlocks = ptDevice->tStagingCachedAllocator.blocks(ptDevice->tStagingCachedAllocator.ptInst, &uBlockCount);

                    static const uint64_t ulMaxBlockSize = PL_DEVICE_ALLOCATION_BLOCK_SIZE;

                    for(uint32_t i = 0; i < uBlockCount; i++)
                    {
                        // ptAppData->tTempAllocator
                        plDeviceAllocationBlock* ptBlock = &sbtBlocks[i];
                        char* pcTempBuffer0 = ptTempMemoryApi->printf(&ptAppData->tTempAllocator, "Block %u: %0.1fMB##sc", i, ((double)ptBlock->ulSize)/1000000.0);
                        char* pcTempBuffer1 = ptTempMemoryApi->printf(&ptAppData->tTempAllocator, "Block %u##sc", i);

                        ptUi->button(pcTempBuffer0);
                        

                        plVec2 tCursor0 = ptUi->get_cursor_pos();
                        const float fWidthAvailable = tWindowEnd.x - tCursor0.x;
                        float fTotalWidth = fWidthAvailable * ((float)ptBlock->ulSize) / (float)ulMaxBlockSize;
                        float fUsedWidth = fWidthAvailable * ((float)ptBlock->sbtRanges[0].tAllocation.ulSize) / (float)ulMaxBlockSize;

                        ptUi->invisible_button(pcTempBuffer1, (plVec2){fTotalWidth, 30.0f});
                        ptDrawApi->add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x + fTotalWidth, 30.0f + tCursor0.y}, (plVec4){0.234f, 0.703f, 0.234f, 1.0f});
                        ptDrawApi->add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x + fUsedWidth, 30.0f + tCursor0.y}, (plVec4){0.703f, 0.234f, 0.234f, 1.0f});
                        if(ptUi->was_last_item_active())
                            ptDrawApi->add_rect(ptFgLayer, tCursor0, (plVec2){tCursor0.x +  fTotalWidth, 30.0f + tCursor0.y}, (plVec4){ 1.f, 1.0f, 1.0f, 1.0f}, 2.0f);

                        if(ptUi->was_last_item_hovered())
                        {
                            ptUi->begin_tooltip();
                            ptUi->text(ptBlock->sbtRanges[0].pcName);
                            ptUi->end_tooltip();
                        }

                        ptTempMemoryApi->reset(&ptAppData->tTempAllocator);
                    }

                }

                ptUi->layout_dynamic(0.0f, 1);
                ptUi->separator();
                ptUi->text("Device Memory: Staging Uncached");

                {

                    ptUi->layout_template_begin(30.0f);
                    ptUi->layout_template_push_static(150.0f);
                    ptUi->layout_template_push_variable(300.0f);
                    ptUi->layout_template_end();

                    uint32_t uBlockCount = 0;
                    plDeviceAllocationBlock* sbtBlocks = ptDevice->tStagingUnCachedAllocator.blocks(ptDevice->tStagingUnCachedAllocator.ptInst, &uBlockCount);

                    static const uint64_t ulMaxBlockSize = PL_DEVICE_ALLOCATION_BLOCK_SIZE;

                    for(uint32_t i = 0; i < uBlockCount; i++)
                    {
                        plDeviceAllocationBlock* ptBlock = &sbtBlocks[i];
                        char* pcTempBuffer0 = ptTempMemoryApi->printf(&ptAppData->tTempAllocator, "Block %u: %0.1fMB##suc", i, ((double)ptBlock->ulSize)/1000000.0);
                        char* pcTempBuffer1 = ptTempMemoryApi->printf(&ptAppData->tTempAllocator, "Block %u##suc", i);

                        ptUi->button(pcTempBuffer0);
                        

                        plVec2 tCursor0 = ptUi->get_cursor_pos();
                        const float fWidthAvailable = tWindowEnd.x - tCursor0.x;
                        float fTotalWidth = fWidthAvailable * ((float)ptBlock->ulSize) / (float)ulMaxBlockSize;
                        float fUsedWidth = fWidthAvailable * ((float)ptBlock->sbtRanges[0].tAllocation.ulSize) / (float)ulMaxBlockSize;

                        ptUi->invisible_button(pcTempBuffer1, (plVec2){fTotalWidth, 30.0f});
                        ptDrawApi->add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x + fTotalWidth, 30.0f + tCursor0.y}, (plVec4){0.234f, 0.703f, 0.234f, 1.0f});
                        ptDrawApi->add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x + fUsedWidth, 30.0f + tCursor0.y}, (plVec4){0.703f, 0.234f, 0.234f, 1.0f});
                        if(ptUi->was_last_item_active())
                            ptDrawApi->add_rect(ptFgLayer, tCursor0, (plVec2){tCursor0.x +  fTotalWidth, 30.0f + tCursor0.y}, (plVec4){ 1.f, 1.0f, 1.0f, 1.0f}, 2.0f);

                        if(ptUi->was_last_item_hovered())
                        {
                            ptUi->begin_tooltip();
                            ptUi->text(ptBlock->sbtRanges[0].pcName);
                            ptUi->end_tooltip();
                        }

                        ptTempMemoryApi->reset(&ptAppData->tTempAllocator);
                    }

                }

                ptUi->layout_dynamic(0.0f, 1);
                ptUi->separator();
                ptUi->text("Device Memory: Local Buddy");

                {

                    ptUi->layout_template_begin(30.0f);
                    ptUi->layout_template_push_static(150.0f);
                    ptUi->layout_template_push_variable(300.0f);
                    ptUi->layout_template_end();

                    uint32_t uBlockCount = 0;
                    uint32_t uNodeCount = 0;
                    plDeviceAllocationBlock* sbtBlocks = ptDevice->tLocalBuddyAllocator.blocks(ptDevice->tLocalBuddyAllocator.ptInst, &uBlockCount);
                    plDeviceAllocationNode* sbtNodes = ptDevice->tLocalBuddyAllocator.nodes(ptDevice->tLocalBuddyAllocator.ptInst, &uNodeCount);
                    const char** sbDebugNames = ptDevice->tLocalBuddyAllocator.names(ptDevice->tLocalBuddyAllocator.ptInst, &uNodeCount);

                    const uint32_t uNodesPerBlock = uNodeCount / uBlockCount;
                    for(uint32_t i = 0; i < uBlockCount; i++)
                    {
                        plDeviceAllocationBlock* ptBlock = &sbtBlocks[i];
                        char* pcTempBuffer0 = ptTempMemoryApi->printf(&ptAppData->tTempAllocator, "Block %u: 256 MB##b", i);
                        char* pcTempBuffer1 = ptTempMemoryApi->printf(&ptAppData->tTempAllocator, "Block %u ##b", i);
                        ptUi->button(pcTempBuffer0);

                        plVec2 tCursor0 = ptUi->get_cursor_pos();
                        const plVec2 tMousePos = ptIoI->get_mouse_pos();
                        uint32_t uHoveredNode = 0;
                        const float fWidthAvailable = tWindowEnd.x - tCursor0.x;
                        float fTotalWidth = fWidthAvailable * ((float)ptBlock->ulSize) / (float)PL_DEVICE_ALLOCATION_BLOCK_SIZE;
                        ptUi->invisible_button(pcTempBuffer1, (plVec2){fTotalWidth, 30.0f});
                        ptDrawApi->add_rect_filled(ptFgLayer, (plVec2){tCursor0.x, tCursor0.y}, (plVec2){tCursor0.x + fTotalWidth, 30.0f + tCursor0.y}, (plVec4){0.234f, 0.703f, 0.234f, 1.0f}); 


                        for(uint32_t j = 0; j < uNodesPerBlock; j++)
                        {
                            plDeviceAllocationNode* ptNode = &sbtNodes[uNodesPerBlock * i + j];

                            if(ptNode->ulSizeWasted >= ptNode->ulSize)
                                continue;

                            float fUsedWidth = fWidthAvailable * ((float)ptNode->ulSize - ptNode->ulSizeWasted) / (float)PL_DEVICE_ALLOCATION_BLOCK_SIZE;
                            float fStart = fWidthAvailable * ((float) ptNode->ulOffset) / (float)PL_DEVICE_ALLOCATION_BLOCK_SIZE;
                            ptDrawApi->add_rect_filled(ptFgLayer, (plVec2){tCursor0.x + fStart, tCursor0.y}, (plVec2){tCursor0.x + fStart + fUsedWidth, 30.0f + tCursor0.y}, (plVec4){0.703f, 0.234f, 0.234f, 1.0f}); 

                            if(ptNode->ulSizeWasted > 0)
                            {
                                
                                const float fWastedWidth = fWidthAvailable * ((float)ptNode->ulSizeWasted) / (float)PL_DEVICE_ALLOCATION_BLOCK_SIZE;
                                const float fWasteStart = fStart + fUsedWidth;
                                ptDrawApi->add_rect_filled(ptFgLayer, (plVec2){tCursor0.x + fWasteStart, tCursor0.y}, (plVec2){tCursor0.x + fWasteStart + fWastedWidth, 30.0f + tCursor0.y}, (plVec4){0.703f, 0.703f, 0.234f, 1.0f}); 
                                if(tMousePos.x > tCursor0.x + fStart && tMousePos.x < tCursor0.x + fWasteStart + fWastedWidth)
                                    uHoveredNode = (uint32_t)ptNode->uNodeIndex;
                            }
                            else if(tMousePos.x > tCursor0.x + fStart && tMousePos.x < tCursor0.x + fStart + fUsedWidth)
                                uHoveredNode = (uint32_t)ptNode->uNodeIndex;
                        }

                        if(ptUi->was_last_item_hovered())
                        {
                            ptUi->begin_tooltip();
                            ptUi->text(sbDebugNames[uHoveredNode]);
                            ptUi->end_tooltip();
                        }
                    }

                }


                ptUi->layout_dynamic(0.0f, 1);
                ptUi->separator();
                ptUi->text("Device Memory: Local Dedicated");

                {

                    ptUi->layout_template_begin(30.0f);
                    ptUi->layout_template_push_static(150.0f);
                    ptUi->layout_template_push_variable(300.0f);
                    ptUi->layout_template_end();

                    uint32_t uBlockCount = 0;
                    plDeviceAllocationBlock* sbtBlocks = ptDevice->tLocalDedicatedAllocator.blocks(ptDevice->tLocalDedicatedAllocator.ptInst, &uBlockCount);

                    static const uint64_t ulMaxBlockSize = PL_DEVICE_ALLOCATION_BLOCK_SIZE;

                    for(uint32_t i = 0; i < uBlockCount; i++)
                    {
                        // ptAppData->tTempAllocator
                        plDeviceAllocationBlock* ptBlock = &sbtBlocks[i];
                        char* pcTempBuffer0 = ptTempMemoryApi->printf(&ptAppData->tTempAllocator, "Block %u: %0.1fMB###d", i, ((double)ptBlock->ulSize)/1000000.0);
                        char* pcTempBuffer1 = ptTempMemoryApi->printf(&ptAppData->tTempAllocator, "Block %u###d", i);

                        ptUi->button(pcTempBuffer0);
                        
                        plVec2 tCursor0 = ptUi->get_cursor_pos();
                        const float fWidthAvailable = tWindowEnd.x - tCursor0.x;
                        float fTotalWidth = fWidthAvailable * ((float)ptBlock->ulSize) / (float)ulMaxBlockSize;
                        float fUsedWidth = fWidthAvailable * ((float)ptBlock->sbtRanges[0].tAllocation.ulSize) / (float)ulMaxBlockSize;

                        if(fUsedWidth < 10.0f)
                            fUsedWidth = 10.0f;
                        else if (fUsedWidth > 500.0f)
                            fUsedWidth = 525.0f;
                            
                        if (fTotalWidth > 500.0f)
                            fTotalWidth = 525.0f;
                        
                        ptUi->invisible_button(pcTempBuffer1, (plVec2){fTotalWidth, 30.0f});
                        if(ptUi->was_last_item_active())
                        {
                            ptDrawApi->add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x + fTotalWidth, 30.0f + tCursor0.y}, (plVec4){0.703f, 0.703f, 0.234f, 1.0f});
                            ptDrawApi->add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x + fUsedWidth, 30.0f + tCursor0.y}, (plVec4){0.703f, 0.234f, 0.234f, 1.0f});
                            ptDrawApi->add_rect(ptFgLayer, tCursor0, (plVec2){tCursor0.x +  fTotalWidth, 30.0f + tCursor0.y}, (plVec4){ 1.f, 1.0f, 1.0f, 1.0f}, 2.0f);
                        }
                        else
                        {
                            ptDrawApi->add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x + fTotalWidth, 30.0f + tCursor0.y}, (plVec4){0.703f, 0.703f, 0.234f, 1.0f});
                            ptDrawApi->add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x + fUsedWidth, 30.0f + tCursor0.y}, (plVec4){0.703f, 0.234f, 0.234f, 1.0f});
                        }

                        if(ptUi->was_last_item_hovered())
                        {
                            ptUi->begin_tooltip();
                            ptUi->text(ptBlock->sbtRanges[0].pcName);
                            ptUi->end_tooltip();
                        }

                        ptTempMemoryApi->reset(&ptAppData->tTempAllocator);
                    }
                }

                ptUi->end_window();
            }
        }

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~renderer begin frame~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~submit draws~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // submit draw layers
        pl_begin_profile_sample("Submit draw layers");
        ptDrawApi->submit_layer(ptAppData->bgDrawLayer);
        ptDrawApi->submit_layer(ptAppData->fgDrawLayer);
        pl_end_profile_sample();

        ptGfx->begin_recording(ptGraphics);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~scene prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        ptRendererApi->reset_scene(ptScene);
        ptEcs->run_mesh_update_system(ptComponentLibrary);
        ptEcs->run_hierarchy_update_system(ptComponentLibrary);
        ptEcs->run_object_update_system(ptComponentLibrary);
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
        ptVulkanDrawApi->submit_3d_drawlist(&ptAppData->drawlist3d, (float)ptIOCtx->afMainViewportSize[0], (float)ptIOCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptGraphics->szCurrentFrameIndex, &tMVP, PL_PIPELINE_FLAG_DEPTH_TEST);

        ptUi->get_draw_context(NULL)->tFrameBufferScale.x = ptIOCtx->afMainFramebufferScale[0];
        ptUi->get_draw_context(NULL)->tFrameBufferScale.y = ptIOCtx->afMainFramebufferScale[1];

        // submit draw lists
        ptVulkanDrawApi->submit_drawlist(&ptAppData->drawlist, (float)ptIOCtx->afMainViewportSize[0], (float)ptIOCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptGraphics->szCurrentFrameIndex);

        // submit ui drawlist
        ptUi->render();

        ptVulkanDrawApi->submit_drawlist(ptUi->get_draw_list(NULL), (float)ptIOCtx->afMainViewportSize[0], (float)ptIOCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptGraphics->szCurrentFrameIndex);
        ptVulkanDrawApi->submit_drawlist(ptUi->get_debug_draw_list(NULL), (float)ptIOCtx->afMainViewportSize[0], (float)ptIOCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptGraphics->szCurrentFrameIndex);
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
    pl_end_profile_frame();
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
