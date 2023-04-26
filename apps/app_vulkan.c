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
#include "pl_os.h"
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
    plGraphics          tGraphics;
    plDrawList          drawlist;
    plDrawList          drawlist2;
    plDrawList3D        drawlist3d;
    plDrawLayer*        fgDrawLayer;
    plDrawLayer*        bgDrawLayer;
    plDrawLayer*        offscreenDrawLayer;
    plFontAtlas         fontAtlas;
    bool                bShowUiDemo;
    bool                bShowUiDebug;
    bool                bShowUiStyle;
    bool                bShowUiMemory;

    // apis
    plIOApiI*               ptIoI;
    plLibraryApiI*          ptLibraryApi;
    plFileApiI*             ptFileApi;
    
    // extension apis
    plGraphicsApiI*         ptGfx;
    plResourceManager0ApiI* ptResourceApi;
    plProtoApiI*            ptProtoApi;
    plDrawApiI*             ptDrawApi;
    plVulkanDrawApiI*       ptVulkanDrawApi;
    plUiApiI*               ptUiApi;
    
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

    plApiRegistryApiI* ptApiRegistry;

} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

static void
pl__api_update_callback(void* pNewInterface, void* pOldInterface, void* pAppData)
{
    plAppData* ptAppData = pAppData;
    plDataRegistryApiI* ptDataRegistry = ptAppData->ptApiRegistry->first(PL_API_DATA_REGISTRY);

    if(pOldInterface == ptAppData->ptUiApi)
    {
        ptAppData->ptUiApi = pNewInterface;
        ptAppData->ptUiApi->set_context(ptDataRegistry->get_data("ui"));
        ptAppData->ptUiApi->set_draw_api(ptAppData->ptDrawApi);
    }
    else if(pOldInterface == ptAppData->ptDrawApi)
    {
        ptAppData->ptDrawApi = pNewInterface;
        ptAppData->ptDrawApi->set_context(ptDataRegistry->get_data("draw"));
        ptAppData->ptUiApi->set_draw_api(ptAppData->ptDrawApi);
    }
}

PL_EXPORT void*
pl_app_load(plApiRegistryApiI* ptApiRegistry, void* pAppData)
{
    plAppData* ptAppData = pAppData;
    plDataRegistryApiI* ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);

    if(ptAppData) // reload
    {
        pl_set_log_context(ptDataRegistry->get_data("log"));
        pl_set_profile_context(ptDataRegistry->get_data("profile"));
        
        // must resubscribe (can't do in callback since callback is from previous binary)
        ptApiRegistry->subscribe(ptAppData->ptUiApi, pl__api_update_callback, ptAppData);
        ptApiRegistry->subscribe(ptAppData->ptDrawApi, pl__api_update_callback, ptAppData);
        return ptAppData;
    }

    plIOApiI* ptIoI = ptApiRegistry->first(PL_API_IO);
    plLibraryApiI* ptLibraryApi = ptApiRegistry->first(PL_API_LIBRARY);
    plFileApiI* ptFileApi = ptApiRegistry->first(PL_API_FILE);
    plMemoryApiI* ptMemoryApi = ptApiRegistry->first(PL_API_MEMORY);
    
    ptAppData = malloc(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));
    ptAppData->ptApiRegistry = ptApiRegistry;
    ptAppData->ptIoI = ptIoI;
    ptAppData->ptLibraryApi = ptLibraryApi;
    ptAppData->ptFileApi = ptFileApi;
    
    // create profile context
    plProfileContext* ptProfileCtx = pl_create_profile_context();
    ptDataRegistry->set_data("profile", ptProfileCtx);

    // create log context
    plLogContext* ptLogCtx = pl_create_log_context();
    pl_add_log_channel("Default", PL_CHANNEL_TYPE_CONSOLE);
    pl_log_info("Setup logging");
    ptDataRegistry->set_data("log", ptLogCtx);

    // load extensions
    plExtensionRegistryApiI* ptExtensionRegistry = ptApiRegistry->first(PL_API_EXTENSION_REGISTRY);

    ptExtensionRegistry->load_from_config(ptApiRegistry, "../src/pl_config.json");

    plProtoApiI* ptProtoApi = ptApiRegistry->first(PL_API_PROTO);
    plGltfApiI* ptGltfApi = ptApiRegistry->first(PL_API_GLTF);
    plGraphicsApiI* ptGfx = ptApiRegistry->first(PL_API_GRAPHICS);
    plImageApiI* ptImageApi = ptApiRegistry->first(PL_API_IMAGE);
    plDrawApiI* ptDrawApi = ptApiRegistry->first(PL_API_DRAW);
    plVulkanDrawApiI* ptVulkanDrawApi = ptApiRegistry->first(PL_API_VULKAN_DRAW);
    plUiApiI* ptUi = ptApiRegistry->first(PL_API_UI);
    ptAppData->ptGfx = ptGfx;
    ptAppData->ptProtoApi = ptProtoApi;
    ptAppData->ptDrawApi = ptDrawApi;
    ptAppData->ptVulkanDrawApi = ptVulkanDrawApi;
    ptAppData->ptUiApi = ptUi;

    // subscribe to api updates
    ptApiRegistry->subscribe(ptAppData->ptUiApi, pl__api_update_callback, ptAppData);
    ptApiRegistry->subscribe(ptAppData->ptDrawApi, pl__api_update_callback, ptAppData);

    // setup renderer
    ptGfx->setup_graphics(&ptAppData->tGraphics, ptApiRegistry);

    plDeviceApiI* ptDeviceApi = ptAppData->tGraphics.ptDeviceApi;
    plResourceManager0ApiI* ptResourceApi = ptApiRegistry->first(PL_API_RESOURCE_MANAGER_0);
    ptAppData->ptResourceApi = ptResourceApi;

    // create ui context
    plUiContext* ptUiContext = ptUi->create_context(ptIoI, ptDrawApi);
    ptDataRegistry->set_data("ui", ptUiContext);

    // setup drawing api
    const plVulkanInit tVulkanInit = {
        .tPhysicalDevice  = ptAppData->tGraphics.tDevice.tPhysicalDevice,
        .tLogicalDevice   = ptAppData->tGraphics.tDevice.tLogicalDevice,
        .uImageCount      = ptAppData->tGraphics.tSwapchain.uImageCount,
        .tRenderPass      = ptAppData->tGraphics.tRenderPass,
        .tMSAASampleCount = ptAppData->tGraphics.tSwapchain.tMsaaSamples,
        .uFramesInFlight  = ptAppData->tGraphics.uFramesInFlight
    };
    ptVulkanDrawApi->initialize_context(ptUi->get_draw_context(NULL), &tVulkanInit);
    ptDrawApi->register_drawlist(ptUi->get_draw_context(NULL), &ptAppData->drawlist);
    ptDrawApi->register_drawlist(ptUi->get_draw_context(NULL), &ptAppData->drawlist2);
    ptDrawApi->register_3d_drawlist(ptUi->get_draw_context(NULL), &ptAppData->drawlist3d);
    ptAppData->bgDrawLayer = ptDrawApi->request_layer(&ptAppData->drawlist, "Background Layer");
    ptAppData->fgDrawLayer = ptDrawApi->request_layer(&ptAppData->drawlist, "Foreground Layer");
    ptAppData->offscreenDrawLayer = ptDrawApi->request_layer(&ptAppData->drawlist2, "Foreground Layer");

    // create font atlas
    ptDrawApi->add_default_font(&ptAppData->fontAtlas);
    ptDrawApi->build_font_atlas(ptUi->get_draw_context(NULL), &ptAppData->fontAtlas);
    ptUi->set_default_font(&ptAppData->fontAtlas.sbFonts[0]);
    ptDataRegistry->set_data("draw", ptUi->get_draw_context(NULL));

    // renderer
    ptProtoApi->setup_renderer(ptApiRegistry, &ptAppData->tGraphics, &ptAppData->tRenderer);
    ptProtoApi->create_scene(&ptAppData->tRenderer, &ptAppData->tScene);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~entity IDs~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    // cameras
    plIOContext* ptIOCtx = ptAppData->ptIoI->get_context();
    ptAppData->tOffscreenCameraEntity = ptProtoApi->ecs_create_camera(&ptAppData->tScene, "offscreen camera", (plVec3){0.0f, 0.35f, 1.2f}, PL_PI_3, 1280.0f / 720.0f, 0.1f, 10.0f);
    ptAppData->tCameraEntity = ptProtoApi->ecs_create_camera(&ptAppData->tScene, "main camera", (plVec3){-6.211f, 3.647f, 0.827f}, PL_PI_3, ptIOCtx->afMainViewportSize[0] / ptIOCtx->afMainViewportSize[1], 0.01f, 400.0f);
    plCameraComponent* ptCamera = ptProtoApi->ecs_get_component(&ptAppData->tScene.tComponentLibrary.tCameraComponentManager, ptAppData->tCameraEntity);
    plCameraComponent* ptCamera2 = ptProtoApi->ecs_get_component(&ptAppData->tScene.tComponentLibrary.tCameraComponentManager, ptAppData->tOffscreenCameraEntity);
    ptProtoApi->camera_set_pitch_yaw(ptCamera, -0.244f, 1.488f);
    ptProtoApi->camera_set_pitch_yaw(ptCamera2, 0.0f, -PL_PI);

    // objects
    ptAppData->tStlEntity   = ptProtoApi->ecs_create_object(&ptAppData->tScene, "stl object");
    ptAppData->tStl2Entity  = ptProtoApi->ecs_create_object(&ptAppData->tScene, "stl object 2");
    ptAppData->tGrassEntity = ptProtoApi->ecs_create_object(&ptAppData->tScene, "grass object");
    pl_sb_push(ptAppData->tRenderer.sbtObjectEntities, ptAppData->tGrassEntity);
    pl_sb_push(ptAppData->tRenderer.sbtObjectEntities, ptAppData->tStlEntity);
    pl_sb_push(ptAppData->tRenderer.sbtObjectEntities, ptAppData->tStl2Entity);

    ptGltfApi->load(&ptAppData->tScene, "../data/glTF-Sample-Models-master/2.0/FlightHelmet/glTF/FlightHelmet.gltf");
    // pl_ext_load_gltf(&ptAppData->tScene, "../data/glTF-Sample-Models-master/2.0/sponza/glTF/sponza.gltf");

    // materials
    ptAppData->tGrassMaterial   = ptProtoApi->ecs_create_material(&ptAppData->tScene, "grass material");
    ptAppData->tSolidMaterial   = ptProtoApi->ecs_create_material(&ptAppData->tScene, "solid material");
    ptAppData->tSolid2Material  = ptProtoApi->ecs_create_material(&ptAppData->tScene, "solid material 2");

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~materials~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // grass
    plShaderDesc tGrassShaderDesc = {
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

    ptAppData->uGrassShader   = ptGfx->create_shader(&ptAppData->tGraphics.tResourceManager, &tGrassShaderDesc);

    // grass material
    int texWidth, texHeight, texNumChannels;
    int texForceNumChannels = 4;
    unsigned char* rawBytes = ptImageApi->load("../data/pilotlight-assets-master/images/grass.png", &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
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

	uint32_t uGrassTexture = ptResourceApi->create_texture(&ptAppData->tGraphics.tResourceManager, tTextureDesc, sizeof(unsigned char) * texWidth * texHeight * 4, rawBytes, "../data/pilotlight-assets-master/images/grass.png");
	
    // grass material
    plMaterialComponent* ptGrassMaterial = ptProtoApi->ecs_get_component(&ptAppData->tScene.tComponentLibrary.tMaterialComponentManager, ptAppData->tGrassMaterial);
    ptGrassMaterial->uShader = ptAppData->uGrassShader;
    ptGrassMaterial->tShaderType = PL_SHADER_TYPE_CUSTOM;
    ptGrassMaterial->bDoubleSided = true;
    ptGrassMaterial->uAlbedoMap = ptResourceApi->create_texture_view(&ptAppData->tGraphics.tResourceManager, &tView, &tSampler, uGrassTexture, "grass texture");

    ptImageApi->free(rawBytes);

    // solid materials
    ptProtoApi->material_outline(&ptAppData->tScene, ptAppData->tSolid2Material);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~STL Model~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    plStlInfo tStlInfo = {0};
    uint32_t uFileSize = 0u;
    ptFileApi->read("../data/pilotlight-assets-master/meshes/monkey.stl", &uFileSize, NULL, "rb");
    char* acFileData = ptMemoryApi->alloc(uFileSize);
    memset(acFileData, 0, uFileSize);
    ptFileApi->read("../data/pilotlight-assets-master/meshes/monkey.stl", &uFileSize, acFileData, "rb");
    pl_load_stl(acFileData, uFileSize, NULL, NULL, NULL, &tStlInfo);

    plSubMesh tSubMesh = {
        .tMaterial = ptAppData->tSolidMaterial
    };

    pl_sb_resize(tSubMesh.sbtVertexPositions, (uint32_t)tStlInfo.szPositionStreamSize / 3);
    pl_sb_resize(tSubMesh.sbtVertexNormals, (uint32_t)tStlInfo.szNormalStreamSize / 3);
    pl_sb_resize(tSubMesh.sbuIndices, (uint32_t)tStlInfo.szIndexBufferSize);
    pl_load_stl(acFileData, (size_t)uFileSize, (float*)tSubMesh.sbtVertexPositions, (float*)tSubMesh.sbtVertexNormals, tSubMesh.sbuIndices, &tStlInfo);
    ptMemoryApi->free(acFileData);
    plMeshComponent* ptMeshComponent = ptProtoApi->ecs_get_component(&ptAppData->tScene.tComponentLibrary.tMeshComponentManager, ptAppData->tStlEntity);
    plMeshComponent* ptMeshComponent2 = ptProtoApi->ecs_get_component(&ptAppData->tScene.tComponentLibrary.tMeshComponentManager, ptAppData->tStl2Entity);
    pl_sb_push(ptMeshComponent->sbtSubmeshes, tSubMesh);
    tSubMesh.tMaterial = ptAppData->tSolid2Material;
    pl_sb_push(ptMeshComponent2->sbtSubmeshes, tSubMesh);

    {
		plTransformComponent* ptTransformComponent = ptProtoApi->ecs_get_component(ptAppData->tScene.ptTransformComponentManager, ptAppData->tStlEntity);

		ptTransformComponent->tRotation    = (plVec4){.w = 1.0f};
		ptTransformComponent->tScale       = (plVec3){1.0f, 1.0f, 1.0f};
		ptTransformComponent->tTranslation = (plVec3){0};
		ptTransformComponent->tWorld = pl_rotation_translation_scale(ptTransformComponent->tRotation, ptTransformComponent->tTranslation, ptTransformComponent->tScale);
		ptTransformComponent->tFinalTransform = ptTransformComponent->tWorld;
    }

    {
		plTransformComponent* ptTransformComponent = ptProtoApi->ecs_get_component(ptAppData->tScene.ptTransformComponentManager, ptAppData->tStl2Entity);

		ptTransformComponent->tRotation    = (plVec4){.w = 1.0f};
		ptTransformComponent->tScale       = (plVec3){1.0f, 1.0f, 1.0f};
		ptTransformComponent->tTranslation = (plVec3){0};
		ptTransformComponent->tWorld = pl_rotation_translation_scale(ptTransformComponent->tRotation, ptTransformComponent->tTranslation, ptTransformComponent->tScale);
		ptTransformComponent->tFinalTransform = ptTransformComponent->tWorld;
    }
    
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~grass~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const float fGrassX = 0.0f;
    const float fGrassY = 0.0f;
    const float fGrassZ = 0.0f;
    const float fGrassHeight = 1.0f;

    const plVec3 atVertexBuffer[12] = {

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
    const plVec2 atUVs[12] = 
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

    const plVec3 atNormals[12] = 
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

    const uint32_t auGrassIndexBuffer[] = {
        0,  3,  2, 0,  2, 1,
        4,  7,  6, 4,  6, 5,
        8, 11, 10, 8, 10, 9
    };

    plObjectComponent* ptGrassObjectComponent = ptProtoApi->ecs_get_component(&ptAppData->tScene.tComponentLibrary.tObjectComponentManager, ptAppData->tGrassEntity);
    plMeshComponent* ptGrassMeshComponent = ptProtoApi->ecs_get_component(&ptAppData->tScene.tComponentLibrary.tMeshComponentManager, ptGrassObjectComponent->tMesh);
    plTransformComponent* ptGrassTransformComponent = ptProtoApi->ecs_get_component(&ptAppData->tScene.tComponentLibrary.tTransformComponentManager, ptGrassObjectComponent->tTransform);
    plSubMesh tGrassSubMesh = {
        .tMaterial = ptAppData->tGrassMaterial
    };

    pl_sb_resize(tGrassSubMesh.sbtVertexPositions, 12);
    pl_sb_resize(tGrassSubMesh.sbtVertexTextureCoordinates0, 12);
    pl_sb_resize(tGrassSubMesh.sbtVertexNormals, 12);
    pl_sb_resize(tGrassSubMesh.sbuIndices, 18);

    memcpy(tGrassSubMesh.sbtVertexPositions, atVertexBuffer, sizeof(plVec3) * pl_sb_size(tGrassSubMesh.sbtVertexPositions)); //-V1004
    memcpy(tGrassSubMesh.sbtVertexTextureCoordinates0, atUVs, sizeof(plVec2) * pl_sb_size(tGrassSubMesh.sbtVertexTextureCoordinates0)); //-V1004
    memcpy(tGrassSubMesh.sbtVertexNormals, atNormals, sizeof(plVec3) * pl_sb_size(tGrassSubMesh.sbtVertexNormals)); //-V1004
    memcpy(tGrassSubMesh.sbuIndices, auGrassIndexBuffer, sizeof(uint32_t) * pl_sb_size(tGrassSubMesh.sbuIndices)); //-V1004
    pl_sb_push(ptGrassMeshComponent->sbtSubmeshes, tGrassSubMesh);

    ptProtoApi->ecs_attach_component(&ptAppData->tScene, ptAppData->tStl2Entity, ptAppData->tStlEntity);

    {
        plObjectComponent* ptObjectComponent = ptProtoApi->ecs_get_component(&ptAppData->tScene.tComponentLibrary.tObjectComponentManager, ptAppData->tStl2Entity);
        plTransformComponent* ptTransformComponent = ptProtoApi->ecs_get_component(&ptAppData->tScene.tComponentLibrary.tTransformComponentManager, ptObjectComponent->tTransform);
        const plMat4 tStlTranslation = pl_mat4_translate_xyz(4.0f, 0.0f, 0.0f);
        ptTransformComponent->tWorld = tStlTranslation;
    }

    {
        plObjectComponent* ptObjectComponent = ptProtoApi->ecs_get_component(&ptAppData->tScene.tComponentLibrary.tObjectComponentManager, ptAppData->tGrassEntity);
		plTransformComponent* ptTransformComponent = ptProtoApi->ecs_get_component(ptAppData->tScene.ptTransformComponentManager, ptObjectComponent->tTransform);

		ptTransformComponent->tRotation    = (plVec4){.w = 1.0f};
		ptTransformComponent->tScale       = (plVec3){1.0f, 1.0f, 1.0f};
		ptTransformComponent->tTranslation = (plVec3){2.0f, 0.0f, 2.0f};
		ptTransformComponent->tWorld = pl_rotation_translation_scale(ptTransformComponent->tRotation, ptTransformComponent->tTranslation, ptTransformComponent->tScale);
		ptTransformComponent->tFinalTransform = ptTransformComponent->tWorld;
    }

    // offscreen
    plRenderPassDesc tRenderPassDesc = {
        .tColorFormat = VK_FORMAT_R8G8B8A8_UNORM,
        .tDepthFormat = ptDeviceApi->find_depth_stencil_format(&ptAppData->tGraphics.tDevice)
    };
    ptProtoApi->create_render_pass(&ptAppData->tGraphics, &tRenderPassDesc, &ptAppData->tOffscreenPass);

    plRenderTargetDesc tRenderTargetDesc = {
        .tRenderPass = ptAppData->tOffscreenPass,
        .tSize = {1280.0f, 720.0f},
    };
    ptProtoApi->create_render_target(ptResourceApi, &ptAppData->tGraphics, &tRenderTargetDesc, &ptAppData->tOffscreenTarget);

    for(uint32_t i = 0; i < ptAppData->tGraphics.tSwapchain.uImageCount; i++)
    {
        plTextureView* ptColorTextureView = &ptAppData->tGraphics.tResourceManager.sbtTextureViews[ptAppData->tOffscreenTarget.sbuColorTextureViews[i]];
        pl_sb_push(ptAppData->sbtTextures, ptVulkanDrawApi->add_texture(ptUi->get_draw_context(NULL), ptColorTextureView->_tImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
    }

    ptProtoApi->create_main_render_target(&ptAppData->tGraphics, &ptAppData->tMainTarget);

    ptProtoApi->scene_prepare(&ptAppData->tScene);

    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(void* pAppData)
{
    plAppData* ptAppData = pAppData; 
    plGraphicsApiI* ptGfx = ptAppData->ptGfx;
    plProtoApiI* ptProtoApi = ptAppData->ptProtoApi;
    plDrawApiI* ptDrawApi = ptAppData->ptDrawApi;
    plUiApiI* ptUi = ptAppData->ptUiApi;
    vkDeviceWaitIdle(ptAppData->tGraphics.tDevice.tLogicalDevice);
    ptDrawApi->cleanup_font_atlas(&ptAppData->fontAtlas);
    ptUi->destroy_context(NULL);
    ptProtoApi->cleanup_render_pass(&ptAppData->tGraphics, &ptAppData->tOffscreenPass);
    ptProtoApi->cleanup_render_target(&ptAppData->tGraphics, &ptAppData->tOffscreenTarget);
    ptProtoApi->cleanup_renderer(&ptAppData->tRenderer);
    ptGfx->cleanup_graphics(&ptAppData->tGraphics);
    pl_cleanup_profile_context();
    pl_cleanup_log_context();
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(void* pAppData)
{
    plAppData* ptAppData = pAppData;
    plGraphicsApiI* ptGfx = ptAppData->ptGfx;
    plProtoApiI* ptProtoApi = ptAppData->ptProtoApi;
    plIOContext* ptIOCtx = ptAppData->ptIoI->get_context();
    ptGfx->resize_graphics(&ptAppData->tGraphics);
    plCameraComponent* ptCamera = ptProtoApi->ecs_get_component(&ptAppData->tScene.tComponentLibrary.tCameraComponentManager, ptAppData->tCameraEntity);
    ptProtoApi->camera_set_aspect(ptCamera, ptIOCtx->afMainViewportSize[0] / ptIOCtx->afMainViewportSize[1]);
    ptProtoApi->create_main_render_target(&ptAppData->tGraphics, &ptAppData->tMainTarget);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    plGraphicsApiI* ptGfx = ptAppData->ptGfx;
    plResourceManager0ApiI* ptResourceApi = ptAppData->ptResourceApi;
    plProtoApiI* ptProtoApi = ptAppData->ptProtoApi;
    plDrawApiI* ptDrawApi = ptAppData->ptDrawApi;
    plVulkanDrawApiI* ptVulkanDrawApi = ptAppData->ptVulkanDrawApi;
    plUiApiI* ptUi = ptAppData->ptUiApi;

    static bool bVSyncChanged = false;

    if(bVSyncChanged)
    {
        ptGfx->resize_graphics(&ptAppData->tGraphics);
        ptProtoApi->create_main_render_target(&ptAppData->tGraphics, &ptAppData->tMainTarget);
        bVSyncChanged = false;
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~frame setup~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    plIOApiI* pTIoI = ptAppData->ptIoI;
    plIOContext* ptIOCtx = pTIoI->get_context();
    pl_begin_profile_frame();
    ptResourceApi->process_cleanup_queue(&ptAppData->tGraphics.tResourceManager, 1);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~input handling~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    static const float fCameraTravelSpeed = 8.0f;
    plCameraComponent* ptCamera = ptProtoApi->ecs_get_component(&ptAppData->tScene.tComponentLibrary.tCameraComponentManager, ptAppData->tCameraEntity);
    plCameraComponent* ptOffscreenCamera = ptProtoApi->ecs_get_component(&ptAppData->tScene.tComponentLibrary.tCameraComponentManager, ptAppData->tOffscreenCameraEntity);

    // camera space
    if(pTIoI->is_key_pressed(PL_KEY_W, true)) ptProtoApi->camera_translate(ptCamera,  0.0f,  0.0f,  fCameraTravelSpeed * ptIOCtx->fDeltaTime);
    if(pTIoI->is_key_pressed(PL_KEY_S, true)) ptProtoApi->camera_translate(ptCamera,  0.0f,  0.0f, -fCameraTravelSpeed* ptIOCtx->fDeltaTime);
    if(pTIoI->is_key_pressed(PL_KEY_A, true)) ptProtoApi->camera_translate(ptCamera, -fCameraTravelSpeed * ptIOCtx->fDeltaTime,  0.0f,  0.0f);
    if(pTIoI->is_key_pressed(PL_KEY_D, true)) ptProtoApi->camera_translate(ptCamera,  fCameraTravelSpeed * ptIOCtx->fDeltaTime,  0.0f,  0.0f);

    // world space
    if(pTIoI->is_key_pressed(PL_KEY_F, true)) ptProtoApi->camera_translate(ptCamera,  0.0f, -fCameraTravelSpeed * ptIOCtx->fDeltaTime,  0.0f);
    if(pTIoI->is_key_pressed(PL_KEY_R, true)) ptProtoApi->camera_translate(ptCamera,  0.0f,  fCameraTravelSpeed * ptIOCtx->fDeltaTime,  0.0f);

    plFrameContext* ptCurrentFrame = ptGfx->get_frame_resources(&ptAppData->tGraphics);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~begin frame~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    if(ptGfx->begin_frame(&ptAppData->tGraphics))
    {
        ptUi->new_frame();

        if(!ptUi->is_mouse_owned() && pTIoI->is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 1.0f))
        {
            const plVec2 tMouseDelta = pTIoI->get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f);
            ptProtoApi->camera_rotate(ptCamera,  -tMouseDelta.y * 0.1f * ptIOCtx->fDeltaTime,  -tMouseDelta.x * 0.1f * ptIOCtx->fDeltaTime);
            pTIoI->reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
        }
        ptProtoApi->camera_update(ptCamera);
        ptProtoApi->camera_update(ptOffscreenCamera);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~3D drawing api~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        ptDrawApi->add_3d_transform(&ptAppData->drawlist3d, &ptOffscreenCamera->tTransformMat, 0.2f, 0.02f);
        ptDrawApi->add_3d_frustum(&ptAppData->drawlist3d, 
            &ptOffscreenCamera->tTransformMat, ptOffscreenCamera->fFieldOfView, ptOffscreenCamera->fAspectRatio, 
            ptOffscreenCamera->fNearZ, ptOffscreenCamera->fFarZ, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 0.02f);

        const plMat4 tTransform0 = pl_identity_mat4();
        ptDrawApi->add_3d_transform(&ptAppData->drawlist3d, &tTransform0, 10.0f, 0.02f);
        ptDrawApi->add_3d_bezier_quad(&ptAppData->drawlist3d, (plVec3){0.0f,0.0f,0.0f}, (plVec3){5.0f,5.0f,5.0f}, (plVec3){3.0f,4.0f,3.0f}, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 0.02f, 20);
        ptDrawApi->add_3d_bezier_cubic(&ptAppData->drawlist3d, (plVec3){0.0f,0.0f,0.0f}, (plVec3){-0.5f,1.0f,-0.5f}, (plVec3){5.0f,3.5f,5.0f}, (plVec3){3.0f,4.0f,3.0f}, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 0.02f, 20);

        // ui

        if(ptUi->begin_window("Offscreen", NULL, true))
        {
            ptUi->layout_static(720.0f / 2.0f, 1280.0f / 2.0f, 1);
            ptUi->image(ptAppData->sbtTextures[ptAppData->tGraphics.tSwapchain.uCurrentImageIndex], (plVec2){1280.0f / 2.0f, 720.0f / 2.0f});
            ptUi->end_window();
        }

        static int iSelectedEntity = 1;

        ptUi->set_next_window_pos((plVec2){0, 0}, PL_UI_COND_ONCE);

        if(ptUi->begin_window("Pilot Light", NULL, false))
        {
    
            const float pfRatios[] = {1.0f};
            ptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
            
            ptUi->checkbox("UI Debug", &ptAppData->bShowUiDebug);
            ptUi->checkbox("UI Demo", &ptAppData->bShowUiDemo);
            ptUi->checkbox("UI Style", &ptAppData->bShowUiStyle);
            ptUi->checkbox("Device Memory", &ptAppData->bShowUiMemory);
            
            if(ptUi->checkbox("VSync", &ptAppData->tGraphics.tSwapchain.bVSync))
            {
                bVSyncChanged = true;
            }

            if(ptUi->collapsing_header("Renderer"))
            {
                ptUi->text("Dynamic Buffers");
                ptUi->progress_bar((float)ptAppData->tScene.uDynamicBuffer0_Offset / (float)ptAppData->tGraphics.tResourceManager._uDynamicBufferSize, (plVec2){-1.0f, 0.0f}, NULL);
                // ptUi->progress_bar((float)ptAppData->tScene.uDynamicBuffer1_Offset / (float)ptAppData->tRenderer.uDynamicBufferSize, (plVec2){-1.0f, 0.0f}, NULL);
                // ptUi->progress_bar((float)ptAppData->tScene.uDynamicBuffer2_Offset / (float)ptAppData->tScene.uDynamicBufferSize, (plVec2){-1.0f, 0.0f}, NULL);
                ptUi->end_collapsing_header();
            }

            if(ptUi->collapsing_header("Entities"))
            {
                plTagComponent* sbtTagComponents = ptAppData->tScene.ptTagComponentManager->pData;
                for(uint32_t i = 0; i < pl_sb_size(sbtTagComponents); i++)
                {
                    plTagComponent* ptTagComponent = &sbtTagComponents[i];
                    ptUi->radio_button(ptTagComponent->acName, &iSelectedEntity, i + 1);
                }

                ptUi->end_collapsing_header();
            }
            ptUi->end_window();
        }

        if(ptUi->begin_window("Components", NULL, false))
        {
            const float pfRatios[] = {1.0f};
            ptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);

            if(ptUi->collapsing_header("Tag"))
            {
                plTagComponent* ptTagComponent = ptProtoApi->ecs_get_component(ptAppData->tScene.ptTagComponentManager, iSelectedEntity);
                ptUi->text("Name: %s", ptTagComponent->acName);
                ptUi->end_collapsing_header();
            }

            if(ptProtoApi->ecs_has_entity(ptAppData->tScene.ptTransformComponentManager, iSelectedEntity))
            {
                
                if(ptUi->collapsing_header("Transform"))
                {
                    plTransformComponent* ptTransformComponent = ptProtoApi->ecs_get_component(ptAppData->tScene.ptTransformComponentManager, iSelectedEntity);
                    ptUi->text("Rotation: %0.3f, %0.3f, %0.3f, %0.3f", ptTransformComponent->tRotation.x, ptTransformComponent->tRotation.y, ptTransformComponent->tRotation.z, ptTransformComponent->tRotation.w);
                    ptUi->text("Scale: %0.3f, %0.3f, %0.3f", ptTransformComponent->tScale.x, ptTransformComponent->tScale.y, ptTransformComponent->tScale.z);
                    ptUi->text("Translation: %0.3f, %0.3f, %0.3f", ptTransformComponent->tTranslation.x, ptTransformComponent->tTranslation.y, ptTransformComponent->tTranslation.z);
                    ptUi->end_collapsing_header();
                }  
            }

            if(ptProtoApi->ecs_has_entity(ptAppData->tScene.ptMeshComponentManager, iSelectedEntity))
            {
                
                if(ptUi->collapsing_header("Mesh"))
                {
                    // plMeshComponent* ptMeshComponent = pl_ecs_get_component(ptAppData->tScene.ptMeshComponentManager, iSelectedEntity);
                    ptUi->end_collapsing_header();
                }  
            }

            if(ptProtoApi->ecs_has_entity(ptAppData->tScene.ptMaterialComponentManager, iSelectedEntity))
            {
                if(ptUi->collapsing_header("Material"))
                {
                    plMaterialComponent* ptMaterialComponent = ptProtoApi->ecs_get_component(ptAppData->tScene.ptMaterialComponentManager, iSelectedEntity);
                    ptUi->text("Albedo: %0.3f, %0.3f, %0.3f, %0.3f", ptMaterialComponent->tAlbedo.r, ptMaterialComponent->tAlbedo.g, ptMaterialComponent->tAlbedo.b, ptMaterialComponent->tAlbedo.a);
                    ptUi->text("Alpha Cutoff: %0.3f", ptMaterialComponent->fAlphaCutoff);
                    ptUi->text("Double Sided: %s", ptMaterialComponent->bDoubleSided ? "true" : "false");
                    ptUi->text("Outline: %s", ptMaterialComponent->bOutline ? "true" : "false");
                    ptUi->end_collapsing_header();
                }  
            }

            if(ptProtoApi->ecs_has_entity(ptAppData->tScene.ptObjectComponentManager, iSelectedEntity))
            {
                if(ptUi->collapsing_header("Object"))
                {
                    plObjectComponent* ptObjectComponent = ptProtoApi->ecs_get_component(ptAppData->tScene.ptObjectComponentManager, iSelectedEntity);
                    plTagComponent* ptTransformTag = ptProtoApi->ecs_get_component(ptAppData->tScene.ptTagComponentManager, ptObjectComponent->tTransform);
                    plTagComponent* ptMeshTag = ptProtoApi->ecs_get_component(ptAppData->tScene.ptTagComponentManager, ptObjectComponent->tMesh);
                    ptUi->text("Mesh: %s", ptMeshTag->acName);
                    ptUi->text("Transform: %s", ptTransformTag->acName);

                    ptUi->end_collapsing_header();
                }  
            }

            if(ptProtoApi->ecs_has_entity(ptAppData->tScene.ptCameraComponentManager, iSelectedEntity))
            {
                if(ptUi->collapsing_header("Camera"))
                {
                    plCameraComponent* ptCameraComponent = ptProtoApi->ecs_get_component(ptAppData->tScene.ptCameraComponentManager, iSelectedEntity);
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

            if(ptProtoApi->ecs_has_entity(ptAppData->tScene.ptHierarchyComponentManager, iSelectedEntity))
            {
                if(ptUi->collapsing_header("Hierarchy"))
                {
                    plHierarchyComponent* ptHierarchyComponent = ptProtoApi->ecs_get_component(ptAppData->tScene.ptHierarchyComponentManager, iSelectedEntity);
                    plTagComponent* ptParent = ptProtoApi->ecs_get_component(ptAppData->tScene.ptTagComponentManager, ptHierarchyComponent->tParent);
                    ptUi->text("Parent: %s", ptParent->acName);
                    ptUi->end_collapsing_header();
                }  
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

                ptUi->layout_template_begin(30.0f);
                ptUi->layout_template_push_static(150.0f);
                ptUi->layout_template_push_variable(300.0f);
                ptUi->layout_template_end();

                ptUi->button("Block 0: 256MB");

                plVec2 tCursor0 = ptUi->get_cursor_pos();
                float fWidthAvailable = tWindowEnd.x - tCursor0.x;
                ptUi->invisible_button("Block 0", (plVec2){fWidthAvailable - 5.0f, 30.0f});
                if(ptUi->was_last_item_active())
                {
                    ptDrawApi->add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x +  + fWidthAvailable - 5.0f, 30.0f + tCursor0.y}, (plVec4){0.234f, 0.703f, 0.234f, 1.0f});
                    ptDrawApi->add_rect(ptFgLayer, tCursor0, (plVec2){tCursor0.x +  + fWidthAvailable - 5.0f, 30.0f + tCursor0.y}, (plVec4){ 1.f, 1.0f, 1.0f, 1.0f}, 2.0f);
                }
                else
                    ptDrawApi->add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x +  + fWidthAvailable - 5.0f, 30.0f + tCursor0.y}, (plVec4){0.234f, 0.703f, 0.234f, 1.0f});

                ptUi->button("Block 1: 256MB");
                tCursor0 = ptUi->get_cursor_pos();
                fWidthAvailable = tWindowEnd.x - tCursor0.x;
                ptUi->dummy((plVec2){fWidthAvailable - 5.0f, 30.0f});
                ptDrawApi->add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x +  + fWidthAvailable - 5.0f, 30.0f + tCursor0.y}, (plVec4){0.234f, 0.703f, 0.234f, 1.0f});
                ptDrawApi->add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x +  + fWidthAvailable / 3.0f - 5.0f, 30.0f + tCursor0.y}, (plVec4){0.703f, 0.234f, 0.234f, 1.0f}); // red
                ptDrawApi->add_rect_filled(ptFgLayer, (plVec2){tCursor0.x +  + fWidthAvailable / 2.0f - 5.0f, tCursor0.y}, (plVec2){tCursor0.x +  fWidthAvailable - 5.0f, 30.0f + tCursor0.y}, (plVec4){0.703f, 0.703f, 0.234f, 1.0f}); // yellow

                ptUi->end_window();
            }
        }

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~renderer begin frame~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // rotate stl model
        {
            plObjectComponent* ptObjectComponent = ptProtoApi->ecs_get_component(&ptAppData->tScene.tComponentLibrary.tObjectComponentManager, ptAppData->tStlEntity);
            plTransformComponent* ptTransformComponent = ptProtoApi->ecs_get_component(&ptAppData->tScene.tComponentLibrary.tTransformComponentManager, ptObjectComponent->tTransform);
            const plMat4 tStlRotation = pl_mat4_rotate_xyz(PL_PI * (float)ptAppData->ptIoI->get_context()->dTime, 0.0f, 1.0f, 0.0f);
            const plMat4 tStlTranslation = pl_mat4_translate_xyz(0.0f, 2.0f, 0.0f);
            const plMat4 tStlTransform = pl_mul_mat4(&tStlTranslation, &tStlRotation);
            ptTransformComponent->tFinalTransform = tStlTransform;
        }

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~submit draws~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // submit draw layers
        pl_begin_profile_sample("Submit draw layers");
        ptDrawApi->submit_layer(ptAppData->bgDrawLayer);
        ptDrawApi->submit_layer(ptAppData->fgDrawLayer);
        pl_end_profile_sample();

        ptGfx->begin_recording(&ptAppData->tGraphics);

        ptProtoApi->reset_scene(&ptAppData->tScene);

        ptProtoApi->begin_render_target(ptGfx, &ptAppData->tGraphics, &ptAppData->tOffscreenTarget);
        ptProtoApi->scene_bind_target(&ptAppData->tScene, &ptAppData->tOffscreenTarget);
        ptProtoApi->scene_update_ecs(&ptAppData->tScene);
        ptProtoApi->scene_bind_camera(&ptAppData->tScene, ptOffscreenCamera);
        ptProtoApi->draw_scene(&ptAppData->tScene);
        ptProtoApi->draw_sky(&ptAppData->tScene);

        ptDrawApi->submit_layer(ptAppData->offscreenDrawLayer);
        ptVulkanDrawApi->submit_drawlist_ex(&ptAppData->drawlist2, 1280.0f, 720.0f, ptCurrentFrame->tCmdBuf, (uint32_t)ptAppData->tGraphics.szCurrentFrameIndex, ptAppData->tOffscreenPass._tRenderPass, VK_SAMPLE_COUNT_1_BIT);
        ptProtoApi->end_render_target(ptGfx, &ptAppData->tGraphics);

        ptProtoApi->begin_render_target(ptGfx, &ptAppData->tGraphics, &ptAppData->tMainTarget);
        ptProtoApi->scene_bind_target(&ptAppData->tScene, &ptAppData->tMainTarget);
        ptProtoApi->scene_update_ecs(&ptAppData->tScene);
        ptProtoApi->scene_bind_camera(&ptAppData->tScene, ptCamera);
        ptProtoApi->draw_scene(&ptAppData->tScene);
        ptProtoApi->draw_sky(&ptAppData->tScene);

        // submit 3D draw list
        const plMat4 tMVP = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);
        ptVulkanDrawApi->submit_3d_drawlist(&ptAppData->drawlist3d, (float)ptIOCtx->afMainViewportSize[0], (float)ptIOCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptAppData->tGraphics.szCurrentFrameIndex, &tMVP, PL_PIPELINE_FLAG_DEPTH_TEST);

        ptUi->get_draw_context(NULL)->tFrameBufferScale.x = ptIOCtx->afMainFramebufferScale[0];
        ptUi->get_draw_context(NULL)->tFrameBufferScale.y = ptIOCtx->afMainFramebufferScale[1];

        // submit draw lists
        ptVulkanDrawApi->submit_drawlist(&ptAppData->drawlist, (float)ptIOCtx->afMainViewportSize[0], (float)ptIOCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptAppData->tGraphics.szCurrentFrameIndex);

        // submit ui drawlist
        ptUi->render();

        ptVulkanDrawApi->submit_drawlist(ptUi->get_draw_list(NULL), (float)ptIOCtx->afMainViewportSize[0], (float)ptIOCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptAppData->tGraphics.szCurrentFrameIndex);
        ptVulkanDrawApi->submit_drawlist(ptUi->get_debug_draw_list(NULL), (float)ptIOCtx->afMainViewportSize[0], (float)ptIOCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptAppData->tGraphics.szCurrentFrameIndex);
        ptProtoApi->end_render_target(ptGfx, &ptAppData->tGraphics);
        ptGfx->end_recording(&ptAppData->tGraphics);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~end frame~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        ptGfx->end_frame(&ptAppData->tGraphics);
    } 
    pl_end_profile_frame();
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#define PL_STL_IMPLEMENTATION
#include "pl_stl.h"
#undef PL_STL_IMPLEMENTATION
