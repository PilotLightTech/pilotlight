/*
   vulkan_app.c
    - just some nasty testing code
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
#include "pl_memory.h"

#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"
#include "pl_stl.h"

// backends
#include "pl_vulkan.h"

// extensions
#include "pl_image_ext.h"
#include "pl_vulkan_ext.h"
#include "pl_proto_ext.h"
#include "pl_gltf_ext.h"
#include "pl_draw_ext.h"
#include "pl_ui_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAppData
{
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
    plIOApiI*      os_io;
    plFileApiI*    files;
    
    // renderer
    plRenderer      tRenderer;
    plAssetRegistry tAssetRegistry;
    plScene         tScene;

    // cameras
    plEntity tCameraEntity;
    plEntity tOffscreenCameraEntity;

    // shaders
    uint32_t uGrassShader;

    // objects
    plEntity tGrassEntity;
    plEntity tStlEntity;
    plEntity tStl2Entity;

    // materials
    plEntity tGrassMaterial;
    plEntity tSolidMaterial;
    plEntity tSolid2Material;

    // new stuff
    plRenderPass     tOffscreenPass;
    plRenderTarget   tMainTarget;
    plRenderTarget   tOffscreenTarget;
    VkDescriptorSet* sbtTextures;

    plApiRegistryApiI* api;

} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

plProtoApiI*            proto     = NULL;
plGraphicsApiI*         gfx       = NULL;
plDrawApiI*             draw      = NULL;
plVulkanDrawApiI*       vkDraw    = NULL;
plUiApiI*               gui       = NULL;
plIOApiI*               os_io     = NULL;
plResourceManager0ApiI* resources = NULL;

static void
pl__api_update_callback(void* newInterface, void* oldInterface, void* data)
{
    plAppData* appData = data;
    plDataRegistryApiI* dataReg = appData->api->first(PL_API_DATA_REGISTRY);

    if(oldInterface == gui)
    {
        gui = newInterface;
        gui->set_context(dataReg->get_data("gui"));
        gui->set_draw_api(draw);
    }
    else if(oldInterface == draw)
    {
        draw = newInterface;
        draw->set_context(dataReg->get_data("draw"));
        gui->set_draw_api(draw);
    }
}

PL_EXPORT void*
pl_app_load(plApiRegistryApiI* api, void* data)
{
    plAppData* app = data;
    plDataRegistryApiI* dataReg = api->first(PL_API_DATA_REGISTRY);

    if(app) // reload
    {
        pl_set_log_context(dataReg->get_data("log"));
        pl_set_profile_context(dataReg->get_data("profile"));
        
        // must resubscribe (can't do in callback since callback is from previous binary)
        api->subscribe(gui, pl__api_update_callback, app);
        api->subscribe(draw, pl__api_update_callback, app);

        // reload apis for this binary
        proto     = api->first(PL_API_PROTO);
        gfx         = api->first(PL_API_GRAPHICS);
        draw      = api->first(PL_API_DRAW);
        vkDraw    = api->first(PL_API_VULKAN_DRAW);
        gui        = api->first(PL_API_UI);
        os_io        = api->first(PL_API_IO);
        resources = api->first(PL_API_RESOURCE_MANAGER_0);

        draw->set_context(dataReg->get_data("draw"));
        gui->set_context(dataReg->get_data("gui"));

        gfx->reload_contexts(api);
        return app;
    }

    plMemoryApiI* ptMemoryApi   = api->first(PL_API_MEMORY);
    
    app = ptMemoryApi->alloc(sizeof(plAppData));
    memset(app, 0, sizeof(plAppData));
    app->api   = api;
    app->os_io    = api->first(PL_API_IO);
    app->files = api->first(PL_API_FILE);
    
    // create profile context
    dataReg->set_data("profile", pl_create_profile_context());

    // create log context
    dataReg->set_data("log", pl_create_log_context());
    pl_add_log_channel("Default", PL_CHANNEL_TYPE_CONSOLE);
    pl_log_info("Setup logging");
    
    // load extensions
    plExtensionRegistryApiI* extensions = api->first(PL_API_EXTENSION_REGISTRY);
    extensions->load_from_config(api, "../src/pl_config.json");

    gfx      = api->first(PL_API_GRAPHICS);
    draw   = api->first(PL_API_DRAW);
    vkDraw = api->first(PL_API_VULKAN_DRAW);
    gui     = api->first(PL_API_UI);
    os_io     = api->first(PL_API_IO);

    // subscribe to api updates
    api->subscribe(gui, pl__api_update_callback, app);
    api->subscribe(draw, pl__api_update_callback, app);

    // setup renderer
    gfx->setup_graphics(&app->tGraphics, api, &app->tTempAllocator);

    plDeviceApiI* device = app->tGraphics.ptDeviceApi;
    resources = api->first(PL_API_RESOURCE_MANAGER_0);

    // create gui context
    dataReg->set_data("gui", gui->create_context(os_io, draw));

    // setup drawing api
    const plVulkanInit vkInit = {
        .tPhysicalDevice  = app->tGraphics.tDevice.tPhysicalDevice,
        .tLogicalDevice   = app->tGraphics.tDevice.tLogicalDevice,
        .uImageCount      = app->tGraphics.tSwapchain.uImageCount,
        .tRenderPass      = app->tGraphics.tRenderPass,
        .tMSAASampleCount = app->tGraphics.tSwapchain.tMsaaSamples,
        .uFramesInFlight  = app->tGraphics.uFramesInFlight
    };
    vkDraw->initialize_context(gui->get_draw_context(NULL), &vkInit);
    draw->register_drawlist(gui->get_draw_context(NULL), &app->drawlist);
    draw->register_drawlist(gui->get_draw_context(NULL), &app->drawlist2);
    draw->register_3d_drawlist(gui->get_draw_context(NULL), &app->drawlist3d);
    app->bgDrawLayer = draw->request_layer(&app->drawlist, "Background Layer");
    app->fgDrawLayer = draw->request_layer(&app->drawlist, "Foreground Layer");
    app->offscreenDrawLayer = draw->request_layer(&app->drawlist2, "Foreground Layer");

    // create font atlas
    draw->add_default_font(&app->fontAtlas);
    draw->build_font_atlas(gui->get_draw_context(NULL), &app->fontAtlas);
    gui->set_default_font(&app->fontAtlas.sbFonts[0]);
    dataReg->set_data("draw", gui->get_draw_context(NULL));

    // renderer
    proto = api->first(PL_API_PROTO);
    proto->setup_renderer(api, &app->tGraphics, &app->tRenderer);
    proto->create_scene(&app->tRenderer, &app->tScene);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~entity IDs~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    // cameras
    plIOContext* ioCtx = app->os_io->get_context();
    app->tOffscreenCameraEntity = proto->ecs_create_camera(&app->tScene, "offscreen camera", (plVec3){0.0f, 0.35f, 1.2f}, PL_PI_3, 1280.0f / 720.0f, 0.1f, 10.0f);
    app->tCameraEntity = proto->ecs_create_camera(&app->tScene, "main camera", (plVec3){-6.211f, 3.647f, 0.827f}, PL_PI_3, ioCtx->afMainViewportSize[0] / ioCtx->afMainViewportSize[1], 0.01f, 400.0f);
    plCameraComponent* cam0 = proto->ecs_get_component(&app->tScene.tComponentLibrary.tCameraComponentManager, app->tCameraEntity);
    plCameraComponent* cam1 = proto->ecs_get_component(&app->tScene.tComponentLibrary.tCameraComponentManager, app->tOffscreenCameraEntity);
    proto->camera_set_pitch_yaw(cam0, -0.244f, 1.488f);
    proto->camera_set_pitch_yaw(cam1, 0.0f, -PL_PI);

    // objects
    app->tStlEntity   = proto->ecs_create_object(&app->tScene, "stl object");
    app->tStl2Entity  = proto->ecs_create_object(&app->tScene, "stl object 2");
    app->tGrassEntity = proto->ecs_create_object(&app->tScene, "grass object");
    pl_sb_push(app->tRenderer.sbtObjectEntities, app->tGrassEntity);
    pl_sb_push(app->tRenderer.sbtObjectEntities, app->tStlEntity);
    pl_sb_push(app->tRenderer.sbtObjectEntities, app->tStl2Entity);

    plGltfApiI* gltf = api->first(PL_API_GLTF);
    gltf->load(&app->tScene, "../data/glTF-Sample-Models-master/2.0/FlightHelmet/glTF/FlightHelmet.gltf");
    // pl_ext_load_gltf(&app->tScene, "../data/glTF-Sample-Models-master/2.0/sponza/glTF/sponza.gltf");

    // materials
    app->tGrassMaterial   = proto->ecs_create_material(&app->tScene, "grass material");
    app->tSolidMaterial   = proto->ecs_create_material(&app->tScene, "solid material");
    app->tSolid2Material  = proto->ecs_create_material(&app->tScene, "solid material 2");

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~materials~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // grass
    plShaderDesc grassShaderDesc = {
        .pcPixelShader                       = "grass.frag.spv",
        .pcVertexShader                      = "grass.vert.spv",
        .tGraphicsState.ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_HAS_NORMAL | PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0,
        .tGraphicsState.ulDepthMode          = PL_DEPTH_MODE_LESS,
        .tGraphicsState.ulBlendMode          = PL_BLEND_MODE_ALPHA,
        .tGraphicsState.ulCullMode           = VK_CULL_MODE_NONE,
        .tGraphicsState.ulDepthWriteEnabled  = VK_TRUE,
        .tGraphicsState.ulShaderTextureFlags = PL_SHADER_TEXTURE_FLAG_BINDING_0,
        .tGraphicsState.ulStencilMode        = PL_STENCIL_MODE_ALWAYS,
        .uBindGroupLayoutCount               = 3,
        .atBindGroupLayouts                  = {
            {
                .uBufferCount = 2,
                .aBuffers = {
                    { .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 1, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT }
                },
            },
            {
                .uBufferCount = 1,
                .aBuffers      = {
                    { .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_FRAGMENT_BIT }
                },
                .uTextureCount = 3,
                .aTextures     = {
                    { .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uSlot = 1, .tStageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
                    { .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uSlot = 2, .tStageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
                    { .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uSlot = 3, .tStageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
                }
            },
            {
                .uBufferCount  = 1,
                .aBuffers      = {
                    { .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}
                }
            }
        },   
    };

    app->uGrassShader   = gfx->create_shader(&app->tGraphics.tResourceManager, &grassShaderDesc);

    // grass material
    int texWidth, texHeight, texNumChannels;
    int texForceNumChannels = 4;
    plImageApiI* images = api->first(PL_API_IMAGE);
    unsigned char* rawBytes = images->load("../data/pilotlight-assets-master/images/grass.png", &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
    PL_ASSERT(rawBytes);

    const plTextureDesc tTextureDesc = {
        .tDimensions = {.x = (float)texWidth, .y = (float)texHeight, .z = 1.0f},
        .tFormat     = VK_FORMAT_R8G8B8A8_UNORM,
        .tUsage      = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .uLayers     = 1,
        .uMips       = 1, // means all mips
        .tType       = VK_IMAGE_TYPE_2D
    };

	const plSampler tSampler = 
	{
		.fMinMip         = 0.0f,
		.fMaxMip         = 64.0f,
		.tFilter         = PL_FILTER_NEAREST,
        .tHorizontalWrap = PL_WRAP_MODE_CLAMP,
        .tVerticalWrap   = PL_WRAP_MODE_CLAMP
	};

	const plTextureViewDesc tView = {
		.tFormat     = tTextureDesc.tFormat,
		.uLayerCount = tTextureDesc.uLayers,
		.uMips       = tTextureDesc.uMips
	};

	uint32_t uGrassTexture = resources->create_texture(&app->tGraphics.tResourceManager, tTextureDesc, sizeof(unsigned char) * texWidth * texHeight * 4, rawBytes, "../data/pilotlight-assets-master/images/grass.png");
	
    // grass material
    plMaterialComponent* ptGrassMaterial = proto->ecs_get_component(&app->tScene.tComponentLibrary.tMaterialComponentManager, app->tGrassMaterial);
    ptGrassMaterial->uShader = app->uGrassShader;
    ptGrassMaterial->tShaderType = PL_SHADER_TYPE_CUSTOM;
    ptGrassMaterial->bDoubleSided = true;
    ptGrassMaterial->uAlbedoMap = resources->create_texture_view(&app->tGraphics.tResourceManager, &tView, &tSampler, uGrassTexture, "grass texture");

    images->free(rawBytes);

    // solid materials
    proto->material_outline(&app->tScene, app->tSolid2Material);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~STL Model~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    plTempAllocatorApiI* tempAlloc = api->first(PL_API_TEMP_ALLOCATOR);
    plStlInfo tStlInfo = {0};
    uint32_t fileSize = 0u;
    app->files->read("../data/pilotlight-assets-master/meshes/monkey.stl", &fileSize, NULL, "rb");
    char* acFileData = tempAlloc->alloc(&app->tTempAllocator, fileSize);
    memset(acFileData, 0, fileSize);
    app->files->read("../data/pilotlight-assets-master/meshes/monkey.stl", &fileSize, acFileData, "rb");
    pl_load_stl(acFileData, fileSize, NULL, NULL, NULL, &tStlInfo);

    plSubMesh tSubMesh = {
        .tMaterial = app->tSolidMaterial
    };

    pl_sb_resize(tSubMesh.sbtVertexPositions, (uint32_t)tStlInfo.szPositionStreamSize / 3);
    pl_sb_resize(tSubMesh.sbtVertexNormals, (uint32_t)tStlInfo.szNormalStreamSize / 3);
    pl_sb_resize(tSubMesh.sbuIndices, (uint32_t)tStlInfo.szIndexBufferSize);
    pl_load_stl(acFileData, (size_t)fileSize, (float*)tSubMesh.sbtVertexPositions, (float*)tSubMesh.sbtVertexNormals, tSubMesh.sbuIndices, &tStlInfo);
    tempAlloc->reset(&app->tTempAllocator);
    plMeshComponent* ptMeshComponent = proto->ecs_get_component(&app->tScene.tComponentLibrary.tMeshComponentManager, app->tStlEntity);
    plMeshComponent* ptMeshComponent2 = proto->ecs_get_component(&app->tScene.tComponentLibrary.tMeshComponentManager, app->tStl2Entity);
    pl_sb_push(ptMeshComponent->sbtSubmeshes, tSubMesh);
    tSubMesh.tMaterial = app->tSolid2Material;
    pl_sb_push(ptMeshComponent2->sbtSubmeshes, tSubMesh);

    {
		plTransformComponent* transform = proto->ecs_get_component(app->tScene.ptTransformComponentManager, app->tStlEntity);

		transform->tRotation    = (plVec4){.w = 1.0f};
		transform->tScale       = (plVec3){1.0f, 1.0f, 1.0f};
		transform->tTranslation = (plVec3){0};
		transform->tWorld = pl_rotation_translation_scale(transform->tRotation, transform->tTranslation, transform->tScale);
		transform->tFinalTransform = transform->tWorld;
    }

    {
		plTransformComponent* transform = proto->ecs_get_component(app->tScene.ptTransformComponentManager, app->tStl2Entity);

		transform->tRotation    = (plVec4){.w = 1.0f};
		transform->tScale       = (plVec3){1.0f, 1.0f, 1.0f};
		transform->tTranslation = (plVec3){0};
		transform->tWorld = pl_rotation_translation_scale(transform->tRotation, transform->tTranslation, transform->tScale);
		transform->tFinalTransform = transform->tWorld;
    }
    
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~grass~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const float fGrassX = 0.0f;
    const float fGrassY = 0.0f;
    const float fGrassZ = 0.0f;
    const float fGrassHeight = 1.0f;

    const plVec3 vtxBuf[12] = {

        // first quad
        { -0.5f + fGrassX, fGrassHeight + fGrassY, 0.0f + fGrassZ},
        {  0.5f + fGrassX, fGrassHeight + fGrassY, 0.0f + fGrassZ},
        {  0.5f + fGrassX,    0.0f + fGrassY, 0.0f + fGrassZ},
        { -0.5f + fGrassX,    0.0f + fGrassY,  0.0f + fGrassZ},

        // second quad
        { -0.35f + fGrassX,    1.0f + fGrassY, -0.35f + fGrassZ},
        {  0.35f + fGrassX, fGrassHeight + fGrassY,  0.35f + fGrassZ},
        {  0.35f + fGrassX,    0.0f + fGrassY,  0.35f + fGrassZ},
        { -0.35f + fGrassX,    0.0f + fGrassY, -0.35f + fGrassZ},

        // third quad
        { -0.35f + fGrassX, fGrassHeight + fGrassY,  0.35f + fGrassZ},
        {  0.35f + fGrassX, fGrassHeight + fGrassY, -0.35f + fGrassZ},
        {  0.35f + fGrassX,    0.0f + fGrassY, -0.35f + fGrassZ},
        { -0.35f + fGrassX,    0.0f + fGrassY,  0.35f + fGrassZ}
    };

    // uvs
    const plVec2 uvs[12] = 
    {
        {.x = 0.0f, .y = 0.0f},
        {.x = 1.0f, .y = 0.0f},
        {.x = 1.0f, .y = 1.0f},
        {.x = 0.0f, .y = 1.0f},

        {.x = 0.0f, .y = 0.0f},
        {.x = 1.0f, .y = 0.0f},
        {.x = 1.0f, .y = 1.0f}, 
        {.x = 0.0f, .y = 1.0f},

        {.x = 0.0f, .y = 0.0f},
        {.x = 1.0f, .y = 0.0f},
        {.x = 1.0f, .y = 1.0f}, 
        {.x = 0.0f, .y = 1.0f},
    };

    const plVec3 norms[12] = 
    {
        {.y = 2.0f},
        {.y = 2.0f},
        {.y = 2.0f},
        {.y = 2.0f},

        {.y = 2.0f},
        {.y = 2.0f},
        {.y = 2.0f},
        {.y = 2.0f},

        {.y = 2.0f},
        {.y = 2.0f},
        {.y = 2.0f},
        {.y = 2.0f}
    };

    const uint32_t idxBuf[] = {
        0,  3,  2, 0,  2, 1,
        4,  7,  6, 4,  6, 5,
        8, 11, 10, 8, 10, 9
    };

    plObjectComponent* grassObject = proto->ecs_get_component(&app->tScene.tComponentLibrary.tObjectComponentManager, app->tGrassEntity);
    plMeshComponent* grassMesh = proto->ecs_get_component(&app->tScene.tComponentLibrary.tMeshComponentManager, grassObject->tMesh);
    plTransformComponent* grassTransform = proto->ecs_get_component(&app->tScene.tComponentLibrary.tTransformComponentManager, grassObject->tTransform);
    plSubMesh grassSubMesh = {
        .tMaterial = app->tGrassMaterial
    };

    pl_sb_resize(grassSubMesh.sbtVertexPositions, 12);
    pl_sb_resize(grassSubMesh.sbtVertexTextureCoordinates0, 12);
    pl_sb_resize(grassSubMesh.sbtVertexNormals, 12);
    pl_sb_resize(grassSubMesh.sbuIndices, 18);

    memcpy(grassSubMesh.sbtVertexPositions, vtxBuf, sizeof(plVec3) * pl_sb_size(grassSubMesh.sbtVertexPositions)); //-V1004
    memcpy(grassSubMesh.sbtVertexTextureCoordinates0, uvs, sizeof(plVec2) * pl_sb_size(grassSubMesh.sbtVertexTextureCoordinates0)); //-V1004
    memcpy(grassSubMesh.sbtVertexNormals, norms, sizeof(plVec3) * pl_sb_size(grassSubMesh.sbtVertexNormals)); //-V1004
    memcpy(grassSubMesh.sbuIndices, idxBuf, sizeof(uint32_t) * pl_sb_size(grassSubMesh.sbuIndices)); //-V1004
    pl_sb_push(grassMesh->sbtSubmeshes, grassSubMesh);

    proto->ecs_attach_component(&app->tScene, app->tStl2Entity, app->tStlEntity);

    {
        plObjectComponent* object = proto->ecs_get_component(&app->tScene.tComponentLibrary.tObjectComponentManager, app->tStl2Entity);
        plTransformComponent* transform = proto->ecs_get_component(&app->tScene.tComponentLibrary.tTransformComponentManager, object->tTransform);
        const plMat4 tStlTranslation = pl_mat4_translate_xyz(4.0f, 0.0f, 0.0f);
        transform->tWorld = tStlTranslation;
    }

    {
        plObjectComponent* object = proto->ecs_get_component(&app->tScene.tComponentLibrary.tObjectComponentManager, app->tGrassEntity);
		plTransformComponent* transform = proto->ecs_get_component(app->tScene.ptTransformComponentManager, object->tTransform);

		transform->tRotation    = (plVec4){.w = 1.0f};
		transform->tScale       = (plVec3){1.0f, 1.0f, 1.0f};
		transform->tTranslation = (plVec3){2.0f, 0.0f, 2.0f};
		transform->tWorld = pl_rotation_translation_scale(transform->tRotation, transform->tTranslation, transform->tScale);
		transform->tFinalTransform = transform->tWorld;
    }

    // offscreen
    plRenderPassDesc renderPassDesc = {
        .tColorFormat = VK_FORMAT_R8G8B8A8_UNORM,
        .tDepthFormat = device->find_depth_stencil_format(&app->tGraphics.tDevice)
    };
    proto->create_render_pass(&app->tGraphics, &renderPassDesc, &app->tOffscreenPass);

    plRenderTargetDesc targetDesc = {
        .tRenderPass = app->tOffscreenPass,
        .tSize = {1280.0f, 720.0f},
    };
    proto->create_render_target(resources, &app->tGraphics, &targetDesc, &app->tOffscreenTarget);

    for(uint32_t i = 0; i < app->tGraphics.tSwapchain.uImageCount; i++)
    {
        plTextureView* colorView = &app->tGraphics.tResourceManager.sbtTextureViews[app->tOffscreenTarget.sbuColorTextureViews[i]];
        pl_sb_push(app->sbtTextures, vkDraw->add_texture(gui->get_draw_context(NULL), colorView->_tImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
    }

    proto->create_main_render_target(&app->tGraphics, &app->tMainTarget);

    proto->scene_prepare(&app->tScene);

    return app;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(void* pAppData)
{
    
    plAppData* app = pAppData; 
    plTempAllocatorApiI* tempAlloc = app->api->first(PL_API_TEMP_ALLOCATOR);
    plMemoryApiI* memoryApi = app->api->first(PL_API_MEMORY);
    vkDeviceWaitIdle(app->tGraphics.tDevice.tLogicalDevice);
    draw->cleanup_font_atlas(&app->fontAtlas);
    gui->destroy_context(NULL);
    proto->cleanup_render_pass(&app->tGraphics, &app->tOffscreenPass);
    proto->cleanup_render_target(&app->tGraphics, &app->tOffscreenTarget);
    proto->cleanup_renderer(&app->tRenderer);
    gfx->cleanup_graphics(&app->tGraphics);
    pl_cleanup_profile_context();
    pl_cleanup_log_context();
    tempAlloc->free(&app->tTempAllocator);
    memoryApi->free(pAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(void* pAppData)
{
    plAppData* app = pAppData;
    plIOContext* ioCtx = app->os_io->get_context();
    gfx->resize_graphics(&app->tGraphics);
    plCameraComponent* cam0 = proto->ecs_get_component(&app->tScene.tComponentLibrary.tCameraComponentManager, app->tCameraEntity);
    proto->camera_set_aspect(cam0, ioCtx->afMainViewportSize[0] / ioCtx->afMainViewportSize[1]);
    proto->create_main_render_target(&app->tGraphics, &app->tMainTarget);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* app)
{

    static bool vsyncChange = false;

    if(vsyncChange)
    {
        gfx->resize_graphics(&app->tGraphics);
        proto->create_main_render_target(&app->tGraphics, &app->tMainTarget);
        vsyncChange = false;
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~frame setup~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    plIOContext* ioCtx = os_io->get_context();
    pl_begin_profile_frame();
    resources->process_cleanup_queue(&app->tGraphics.tResourceManager, 1);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~input handling~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    static const float camSpeed = 8.0f;
    plCameraComponent* cam0 = proto->ecs_get_component(&app->tScene.tComponentLibrary.tCameraComponentManager, app->tCameraEntity);
    plCameraComponent* cam1 = proto->ecs_get_component(&app->tScene.tComponentLibrary.tCameraComponentManager, app->tOffscreenCameraEntity);

    // camera space
    if(os_io->is_key_pressed(PL_KEY_W, true)) proto->camera_translate(cam0,  0.0f,  0.0f,  camSpeed * ioCtx->fDeltaTime);
    if(os_io->is_key_pressed(PL_KEY_S, true)) proto->camera_translate(cam0,  0.0f,  0.0f, -camSpeed* ioCtx->fDeltaTime);
    if(os_io->is_key_pressed(PL_KEY_A, true)) proto->camera_translate(cam0, -camSpeed * ioCtx->fDeltaTime,  0.0f,  0.0f);
    if(os_io->is_key_pressed(PL_KEY_D, true)) proto->camera_translate(cam0,  camSpeed * ioCtx->fDeltaTime,  0.0f,  0.0f);

    // world space
    if(os_io->is_key_pressed(PL_KEY_F, true)) proto->camera_translate(cam0,  0.0f, -camSpeed * ioCtx->fDeltaTime,  0.0f);
    if(os_io->is_key_pressed(PL_KEY_R, true)) proto->camera_translate(cam0,  0.0f,  camSpeed * ioCtx->fDeltaTime,  0.0f);

    plFrameContext* ptCurrentFrame = gfx->get_frame_resources(&app->tGraphics);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~begin frame~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    if(gfx->begin_frame(&app->tGraphics))
    {
        gui->new_frame();

        if(!gui->is_mouse_owned() && os_io->is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 1.0f))
        {
            const plVec2 delta = os_io->get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f);
            proto->camera_rotate(cam0,  -delta.y * 0.1f * ioCtx->fDeltaTime,  -delta.x * 0.1f * ioCtx->fDeltaTime);
            os_io->reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
        }
        proto->camera_update(cam0);
        proto->camera_update(cam1);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~3D drawing api~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        draw->add_3d_transform(&app->drawlist3d, &cam1->tTransformMat, 0.2f, 0.02f);
        draw->add_3d_frustum(&app->drawlist3d, 
            &cam1->tTransformMat, cam1->fFieldOfView, cam1->fAspectRatio, 
            cam1->fNearZ, cam1->fFarZ, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 0.02f);

        const plMat4 transform0 = pl_identity_mat4();
        draw->add_3d_transform(&app->drawlist3d, &transform0, 10.0f, 0.02f);
        draw->add_3d_bezier_quad(&app->drawlist3d, (plVec3){0.0f,0.0f,0.0f}, (plVec3){5.0f,5.0f,5.0f}, (plVec3){3.0f,4.0f,3.0f}, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 0.02f, 20);
        draw->add_3d_bezier_cubic(&app->drawlist3d, (plVec3){0.0f,0.0f,0.0f}, (plVec3){-0.5f,1.0f,-0.5f}, (plVec3){5.0f,3.5f,5.0f}, (plVec3){3.0f,4.0f,3.0f}, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 0.02f, 20);

        // gui

        if(gui->begin_window("Offscreen", NULL, true))
        {
            gui->layout_static(720.0f / 2.0f, 1280.0f / 2.0f, 1);
            gui->image(app->sbtTextures[app->tGraphics.tSwapchain.uCurrentImageIndex], (plVec2){1280.0f / 2.0f, 720.0f / 2.0f});
            gui->end_window();
        }

        static int selectedEntity = 1;

        gui->set_next_window_pos((plVec2){0, 0}, PL_UI_COND_ONCE);

        if(gui->begin_window("Pilot Light", NULL, false))
        {
    
            const float pfRatios[] = {1.0f};
            gui->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
            
            gui->checkbox("UI Debug", &app->bShowUiDebug);
            gui->checkbox("UI Demo", &app->bShowUiDemo);
            gui->checkbox("UI Style", &app->bShowUiStyle);
            gui->checkbox("Device Memory", &app->bShowUiMemory);
            
            if(gui->checkbox("VSync", &app->tGraphics.tSwapchain.bVSync))
            {
                vsyncChange = true;
            }

            if(gui->collapsing_header("Renderer"))
            {
                gui->text("Dynamic Buffers");
                gui->progress_bar((float)app->tScene.uDynamicBuffer0_Offset / (float)app->tGraphics.tResourceManager._uDynamicBufferSize, (plVec2){-1.0f, 0.0f}, NULL);
                // gui->progress_bar((float)app->tScene.uDynamicBuffer1_Offset / (float)app->tRenderer.uDynamicBufferSize, (plVec2){-1.0f, 0.0f}, NULL);
                // gui->progress_bar((float)app->tScene.uDynamicBuffer2_Offset / (float)app->tScene.uDynamicBufferSize, (plVec2){-1.0f, 0.0f}, NULL);
                gui->end_collapsing_header();
            }

            if(gui->collapsing_header("Entities"))
            {
                plTagComponent* sbtTagComponents = app->tScene.ptTagComponentManager->pData;
                for(uint32_t i = 0; i < pl_sb_size(sbtTagComponents); i++)
                {
                    plTagComponent* ptTagComponent = &sbtTagComponents[i];
                    gui->radio_button(ptTagComponent->acName, &selectedEntity, i + 1);
                }

                gui->end_collapsing_header();
            }
            gui->end_window();
        }

        if(gui->begin_window("Components", NULL, false))
        {
            const float pfRatios[] = {1.0f};
            gui->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);

            if(gui->collapsing_header("Tag"))
            {
                plTagComponent* ptTagComponent = proto->ecs_get_component(app->tScene.ptTagComponentManager, selectedEntity);
                gui->text("Name: %s", ptTagComponent->acName);
                gui->end_collapsing_header();
            }

            if(proto->ecs_has_entity(app->tScene.ptTransformComponentManager, selectedEntity))
            {
                
                if(gui->collapsing_header("Transform"))
                {
                    plTransformComponent* transform = proto->ecs_get_component(app->tScene.ptTransformComponentManager, selectedEntity);
                    gui->text("Rotation: %0.3f, %0.3f, %0.3f, %0.3f", transform->tRotation.x, transform->tRotation.y, transform->tRotation.z, transform->tRotation.w);
                    gui->text("Scale: %0.3f, %0.3f, %0.3f", transform->tScale.x, transform->tScale.y, transform->tScale.z);
                    gui->text("Translation: %0.3f, %0.3f, %0.3f", transform->tTranslation.x, transform->tTranslation.y, transform->tTranslation.z);
                    gui->end_collapsing_header();
                }  
            }

            if(proto->ecs_has_entity(app->tScene.ptMeshComponentManager, selectedEntity))
            {
                
                if(gui->collapsing_header("Mesh"))
                {
                    // plMeshComponent* ptMeshComponent = pl_ecs_get_component(app->tScene.ptMeshComponentManager, selectedEntity);
                    gui->end_collapsing_header();
                }  
            }

            if(proto->ecs_has_entity(app->tScene.ptMaterialComponentManager, selectedEntity))
            {
                if(gui->collapsing_header("Material"))
                {
                    plMaterialComponent* ptMaterialComponent = proto->ecs_get_component(app->tScene.ptMaterialComponentManager, selectedEntity);
                    gui->text("Albedo: %0.3f, %0.3f, %0.3f, %0.3f", ptMaterialComponent->tAlbedo.r, ptMaterialComponent->tAlbedo.g, ptMaterialComponent->tAlbedo.b, ptMaterialComponent->tAlbedo.a);
                    gui->text("Alpha Cutoff: %0.3f", ptMaterialComponent->fAlphaCutoff);
                    gui->text("Double Sided: %s", ptMaterialComponent->bDoubleSided ? "true" : "false");
                    gui->text("Outline: %s", ptMaterialComponent->bOutline ? "true" : "false");
                    gui->end_collapsing_header();
                }  
            }

            if(proto->ecs_has_entity(app->tScene.ptObjectComponentManager, selectedEntity))
            {
                if(gui->collapsing_header("Object"))
                {
                    plObjectComponent* object = proto->ecs_get_component(app->tScene.ptObjectComponentManager, selectedEntity);
                    plTagComponent* ptTransformTag = proto->ecs_get_component(app->tScene.ptTagComponentManager, object->tTransform);
                    plTagComponent* ptMeshTag = proto->ecs_get_component(app->tScene.ptTagComponentManager, object->tMesh);
                    gui->text("Mesh: %s", ptMeshTag->acName);
                    gui->text("Transform: %s", ptTransformTag->acName);

                    gui->end_collapsing_header();
                }  
            }

            if(proto->ecs_has_entity(app->tScene.ptCameraComponentManager, selectedEntity))
            {
                if(gui->collapsing_header("Camera"))
                {
                    plCameraComponent* cam = proto->ecs_get_component(app->tScene.ptCameraComponentManager, selectedEntity);
                    gui->text("Pitch: %0.3f", cam->fPitch);
                    gui->text("Yaw: %0.3f", cam->fYaw);
                    gui->text("Roll: %0.3f", cam->fRoll);
                    gui->text("Near Z: %0.3f", cam->fNearZ);
                    gui->text("Far Z: %0.3f", cam->fFarZ);
                    gui->text("Y Field Of View: %0.3f", cam->fFieldOfView);
                    gui->text("Aspect Ratio: %0.3f", cam->fAspectRatio);
                    gui->text("Up Vector: %0.3f, %0.3f, %0.3f", cam->_tUpVec.x, cam->_tUpVec.y, cam->_tUpVec.z);
                    gui->text("Forward Vector: %0.3f, %0.3f, %0.3f", cam->_tForwardVec.x, cam->_tForwardVec.y, cam->_tForwardVec.z);
                    gui->text("Right Vector: %0.3f, %0.3f, %0.3f", cam->_tRightVec.x, cam->_tRightVec.y, cam->_tRightVec.z);
                    gui->end_collapsing_header();
                }  
            }

            if(proto->ecs_has_entity(app->tScene.ptHierarchyComponentManager, selectedEntity))
            {
                if(gui->collapsing_header("Hierarchy"))
                {
                    plHierarchyComponent* hierarchy = proto->ecs_get_component(app->tScene.ptHierarchyComponentManager, selectedEntity);
                    plTagComponent* ptParent = proto->ecs_get_component(app->tScene.ptTagComponentManager, hierarchy->tParent);
                    gui->text("Parent: %s", ptParent->acName);
                    gui->end_collapsing_header();
                }  
            }
            gui->end_window();
        }

        if(app->bShowUiDemo)
            gui->demo(&app->bShowUiDemo);
            
        if(app->bShowUiStyle)
            gui->style(&app->bShowUiStyle);

        if(app->bShowUiDebug)
            gui->debug(&app->bShowUiDebug);

        if(app->bShowUiMemory)
        {
            if(gui->begin_window("Device Memory", &app->bShowUiMemory, false))
            {
                plDrawLayer* ptFgLayer = gui->get_window_fg_drawlayer();
                const plVec2 tWindowSize = gui->get_window_size();
                const plVec2 tWindowPos = gui->get_window_pos();
                const plVec2 tWindowEnd = pl_add_vec2(tWindowSize, tWindowPos);

                gui->layout_template_begin(30.0f);
                gui->layout_template_push_static(150.0f);
                gui->layout_template_push_variable(300.0f);
                gui->layout_template_end();

                gui->button("Block 0: 256MB");

                plVec2 tCursor0 = gui->get_cursor_pos();
                float fWidthAvailable = tWindowEnd.x - tCursor0.x;
                gui->invisible_button("Block 0", (plVec2){fWidthAvailable - 5.0f, 30.0f});
                if(gui->was_last_item_active())
                {
                    draw->add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x +  + fWidthAvailable - 5.0f, 30.0f + tCursor0.y}, (plVec4){0.234f, 0.703f, 0.234f, 1.0f});
                    draw->add_rect(ptFgLayer, tCursor0, (plVec2){tCursor0.x +  + fWidthAvailable - 5.0f, 30.0f + tCursor0.y}, (plVec4){ 1.f, 1.0f, 1.0f, 1.0f}, 2.0f);
                }
                else
                    draw->add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x +  + fWidthAvailable - 5.0f, 30.0f + tCursor0.y}, (plVec4){0.234f, 0.703f, 0.234f, 1.0f});

                gui->button("Block 1: 256MB");
                tCursor0 = gui->get_cursor_pos();
                fWidthAvailable = tWindowEnd.x - tCursor0.x;
                gui->dummy((plVec2){fWidthAvailable - 5.0f, 30.0f});
                draw->add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x +  + fWidthAvailable - 5.0f, 30.0f + tCursor0.y}, (plVec4){0.234f, 0.703f, 0.234f, 1.0f});
                draw->add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x +  + fWidthAvailable / 3.0f - 5.0f, 30.0f + tCursor0.y}, (plVec4){0.703f, 0.234f, 0.234f, 1.0f}); // red
                draw->add_rect_filled(ptFgLayer, (plVec2){tCursor0.x +  + fWidthAvailable / 2.0f - 5.0f, tCursor0.y}, (plVec2){tCursor0.x +  fWidthAvailable - 5.0f, 30.0f + tCursor0.y}, (plVec4){0.703f, 0.703f, 0.234f, 1.0f}); // yellow

                gui->end_window();
            }
        }

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~renderer begin frame~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // rotate stl model
        {
            plObjectComponent* object = proto->ecs_get_component(&app->tScene.tComponentLibrary.tObjectComponentManager, app->tStlEntity);
            plTransformComponent* transform = proto->ecs_get_component(&app->tScene.tComponentLibrary.tTransformComponentManager, object->tTransform);
            const plMat4 tStlRotation = pl_mat4_rotate_xyz(PL_PI * (float)app->os_io->get_context()->dTime, 0.0f, 1.0f, 0.0f);
            const plMat4 tStlTranslation = pl_mat4_translate_xyz(0.0f, 2.0f, 0.0f);
            const plMat4 tStlTransform = pl_mul_mat4(&tStlTranslation, &tStlRotation);
            transform->tFinalTransform = tStlTransform;
        }

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~submit draws~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // submit draw layers
        pl_begin_profile_sample("Submit draw layers");
        draw->submit_layer(app->bgDrawLayer);
        draw->submit_layer(app->fgDrawLayer);
        pl_end_profile_sample();

        gfx->begin_recording(&app->tGraphics);

        proto->reset_scene(&app->tScene);

        proto->begin_render_target(gfx, &app->tGraphics, &app->tOffscreenTarget);
        proto->scene_bind_target(&app->tScene, &app->tOffscreenTarget);
        proto->scene_update_ecs(&app->tScene);
        proto->scene_bind_camera(&app->tScene, cam1);
        proto->draw_scene(&app->tScene);
        proto->draw_sky(&app->tScene);

        draw->submit_layer(app->offscreenDrawLayer);
        vkDraw->submit_drawlist_ex(&app->drawlist2, 1280.0f, 720.0f, ptCurrentFrame->tCmdBuf, (uint32_t)app->tGraphics.szCurrentFrameIndex, app->tOffscreenPass._tRenderPass, VK_SAMPLE_COUNT_1_BIT);
        proto->end_render_target(gfx, &app->tGraphics);

        proto->begin_render_target(gfx, &app->tGraphics, &app->tMainTarget);
        proto->scene_bind_target(&app->tScene, &app->tMainTarget);
        proto->scene_update_ecs(&app->tScene);
        proto->scene_bind_camera(&app->tScene, cam0);
        proto->draw_scene(&app->tScene);
        proto->draw_sky(&app->tScene);

        // submit 3D draw list
        const plMat4 tMVP = pl_mul_mat4(&cam0->tProjMat, &cam0->tViewMat);
        vkDraw->submit_3d_drawlist(&app->drawlist3d, (float)ioCtx->afMainViewportSize[0], (float)ioCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)app->tGraphics.szCurrentFrameIndex, &tMVP, PL_PIPELINE_FLAG_DEPTH_TEST);

        gui->get_draw_context(NULL)->tFrameBufferScale.x = ioCtx->afMainFramebufferScale[0];
        gui->get_draw_context(NULL)->tFrameBufferScale.y = ioCtx->afMainFramebufferScale[1];

        // submit draw lists
        vkDraw->submit_drawlist(&app->drawlist, (float)ioCtx->afMainViewportSize[0], (float)ioCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)app->tGraphics.szCurrentFrameIndex);

        // submit gui drawlist
        gui->render();

        vkDraw->submit_drawlist(gui->get_draw_list(NULL), (float)ioCtx->afMainViewportSize[0], (float)ioCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)app->tGraphics.szCurrentFrameIndex);
        vkDraw->submit_drawlist(gui->get_debug_draw_list(NULL), (float)ioCtx->afMainViewportSize[0], (float)ioCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)app->tGraphics.szCurrentFrameIndex);
        proto->end_render_target(gfx, &app->tGraphics);
        gfx->end_recording(&app->tGraphics);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~end frame~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        gfx->end_frame(&app->tGraphics);
    } 
    pl_end_profile_frame();
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#define PL_STL_IMPLEMENTATION
#include "pl_stl.h"
#undef PL_STL_IMPLEMENTATION
