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
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <string.h> // memset
#include "pilotlight.h"
#include "pl_graphics_vulkan.h"
#include "pl_profile.h"
#include "pl_log.h"
#include "pl_ds.h"
#include "pl_io.h"
#include "pl_memory.h"
#include "pl_vulkan.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"
#include "pl_registry.h"
#include "pl_ext.h"
#include "pl_ui.h"
#include "pl_stl.h"
#include "pl_prototype.h"
#include "pl_gltf.h"

// extensions
#include "pl_draw_extension.h"
#include "stb_image.h"

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
    plProfileContext*   ptProfileCtx;
    plLogContext*       ptLogCtx;
    plMemoryContext*    ptMemoryCtx;
    plDataRegistry*     ptDataRegistryCtx;
    plExtensionRegistry tExtensionRegistryCtx;
    plUiContext*        ptUiContext;
    bool                bShowUiDemo;
    bool                bShowUiDebug;
    bool                bShowUiStyle;

    // extension apis
    plDrawExtension* ptDrawExtApi;

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

} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plIOContext* ptIOCtx, void* pAppData)
{

    plAppData* ptAppData = pAppData;

    if(ptAppData) // reload
    {
        pl_set_log_context(ptAppData->ptLogCtx);
        pl_set_profile_context(ptAppData->ptProfileCtx);
        pl_set_memory_context(ptAppData->ptMemoryCtx);
        pl_set_data_registry(ptAppData->ptDataRegistryCtx);
        pl_set_extension_registry(&ptAppData->tExtensionRegistryCtx);
        pl_set_io_context(ptIOCtx);
        pl_ui_set_context(ptAppData->ptUiContext);

        plExtension* ptExtension = pl_get_extension(PL_EXT_DRAW);
        ptAppData->ptDrawExtApi = pl_get_api(ptExtension, PL_EXT_API_DRAW);

        return ptAppData;
    }

    ptAppData = malloc(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    // set contexts
    pl_set_io_context(ptIOCtx);
    ptAppData->ptMemoryCtx = pl_create_memory_context();
    ptAppData->ptProfileCtx = pl_create_profile_context();
    ptAppData->ptDataRegistryCtx = pl_create_data_registry();

    // setup logging
    ptAppData->ptLogCtx = pl_create_log_context();
    pl_add_log_channel("Default", PL_CHANNEL_TYPE_CONSOLE);
    pl_log_info("Setup logging");

    // setup extension registry
    pl_initialize_extension_registry(&ptAppData->tExtensionRegistryCtx);
    pl_register_data("memory", ptAppData->ptMemoryCtx);
    pl_register_data("profile", ptAppData->ptProfileCtx);
    pl_register_data("log", ptAppData->ptLogCtx);
    pl_register_data("io", ptIOCtx);
    
    // load extensions
    plExtension tExtension = {0};
    pl_get_draw_extension_info(&tExtension);
    pl_load_extension(&tExtension);

    // load extension apis
    ptAppData->ptDrawExtApi = pl_get_api(pl_get_extension(PL_EXT_DRAW), PL_EXT_API_DRAW);

    // setup renderer
    pl_setup_graphics(&ptAppData->tGraphics);

    // ui
    ptAppData->ptUiContext = pl_ui_create_context();

    // setup drawing api
    const plVulkanInit tVulkanInit = {
        .tPhysicalDevice  = ptAppData->tGraphics.tDevice.tPhysicalDevice,
        .tLogicalDevice   = ptAppData->tGraphics.tDevice.tLogicalDevice,
        .uImageCount      = ptAppData->tGraphics.tSwapchain.uImageCount,
        .tRenderPass      = ptAppData->tGraphics.tRenderPass,
        .tMSAASampleCount = ptAppData->tGraphics.tSwapchain.tMsaaSamples
    };
    pl_initialize_draw_context_vulkan(pl_ui_get_draw_context(NULL), &tVulkanInit);
    pl_register_drawlist(pl_ui_get_draw_context(NULL), &ptAppData->drawlist);
    pl_register_drawlist(pl_ui_get_draw_context(NULL), &ptAppData->drawlist2);
    pl_register_3d_drawlist(pl_ui_get_draw_context(NULL), &ptAppData->drawlist3d);
    ptAppData->bgDrawLayer = pl_request_draw_layer(&ptAppData->drawlist, "Background Layer");
    ptAppData->fgDrawLayer = pl_request_draw_layer(&ptAppData->drawlist, "Foreground Layer");
    ptAppData->offscreenDrawLayer = pl_request_draw_layer(&ptAppData->drawlist2, "Foreground Layer");

    // create font atlas
    pl_add_default_font(&ptAppData->fontAtlas);
    pl_build_font_atlas(pl_ui_get_draw_context(NULL), &ptAppData->fontAtlas);
    pl_ui_set_default_font(&ptAppData->fontAtlas.sbFonts[0]);
    pl_register_data("draw", pl_ui_get_draw_context(NULL));

    // renderer
    pl_setup_renderer(&ptAppData->tGraphics, &ptAppData->tRenderer);
    pl_create_scene(&ptAppData->tRenderer, &ptAppData->tScene);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~entity IDs~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    // cameras
    ptAppData->tOffscreenCameraEntity = pl_ecs_create_camera(&ptAppData->tScene, "offscreen camera", (plVec3){0.0f, 0.35f, 0.8f}, PL_PI_3, 1280.0f / 720.0f, 0.1f, 10.0f);
    ptAppData->tCameraEntity = pl_ecs_create_camera(&ptAppData->tScene, "main camera", (plVec3){-6.211f, 3.647f, 0.827f}, PL_PI_3, ptIOCtx->afMainViewportSize[0] / ptIOCtx->afMainViewportSize[1], 0.01f, 400.0f);
    plCameraComponent* ptCamera = pl_ecs_get_component(&ptAppData->tScene.tComponentLibrary.tCameraComponentManager, ptAppData->tCameraEntity);
    plCameraComponent* ptCamera2 = pl_ecs_get_component(&ptAppData->tScene.tComponentLibrary.tCameraComponentManager, ptAppData->tOffscreenCameraEntity);
    pl_camera_set_pitch_yaw(ptCamera, -0.244f, 1.488f);
    pl_camera_set_pitch_yaw(ptCamera2, 0.0f, -PL_PI);

    // objects
    ptAppData->tStlEntity   = pl_ecs_create_object(&ptAppData->tScene, "stl object");
    ptAppData->tStl2Entity  = pl_ecs_create_object(&ptAppData->tScene, "stl object 2");
    ptAppData->tGrassEntity = pl_ecs_create_object(&ptAppData->tScene, "grass object");
    pl_sb_push(ptAppData->tRenderer.sbtObjectEntities, ptAppData->tGrassEntity);
    pl_sb_push(ptAppData->tRenderer.sbtObjectEntities, ptAppData->tStlEntity);
    pl_sb_push(ptAppData->tRenderer.sbtObjectEntities, ptAppData->tStl2Entity);

    pl_ext_load_gltf(&ptAppData->tScene, "../data/glTF-Sample-Models-master/2.0/FlightHelmet/glTF/FlightHelmet.gltf");

    // materials
    ptAppData->tGrassMaterial   = pl_ecs_create_material(&ptAppData->tScene, "grass material");
    ptAppData->tSolidMaterial   = pl_ecs_create_material(&ptAppData->tScene, "solid material");
    ptAppData->tSolid2Material  = pl_ecs_create_material(&ptAppData->tScene, "solid material 2");

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

    ptAppData->uGrassShader   = pl_create_shader(&ptAppData->tGraphics.tResourceManager, &tGrassShaderDesc);

    // grass material
    int texWidth, texHeight, texNumChannels;
    int texForceNumChannels = 4;
    unsigned char* rawBytes = stbi_load("../data/pilotlight-assets-master/images/grass.png", &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
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

	uint32_t uGrassTexture = pl_create_texture(&ptAppData->tGraphics.tResourceManager, tTextureDesc, sizeof(unsigned char) * texWidth * texHeight * 4, rawBytes, "../data/pilotlight-assets-master/images/grass.png");
	
    // grass material
    plMaterialComponent* ptGrassMaterial = pl_ecs_get_component(&ptAppData->tScene.tComponentLibrary.tMaterialComponentManager, ptAppData->tGrassMaterial);
    ptGrassMaterial->uShader = ptAppData->uGrassShader;
    ptGrassMaterial->tShaderType = PL_SHADER_TYPE_CUSTOM;
    ptGrassMaterial->bDoubleSided = true;
    ptGrassMaterial->uAlbedoMap = pl_create_texture_view(&ptAppData->tGraphics.tResourceManager, &tView, &tSampler, uGrassTexture, "grass texture");

    stbi_image_free(rawBytes);

    // solid materials
    pl_material_outline(&ptAppData->tScene, ptAppData->tSolid2Material);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~STL Model~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    plStlInfo tStlInfo = {0};
    uint32_t uFileSize = 0u;
    pl_read_file("../data/pilotlight-assets-master/meshes/monkey.stl", &uFileSize, NULL, "rb");
    char* acFileData = pl_alloc(uFileSize);
    memset(acFileData, 0, uFileSize);
    pl_read_file("../data/pilotlight-assets-master/meshes/monkey.stl", &uFileSize, acFileData, "rb");
    pl_load_stl(acFileData, uFileSize, NULL, NULL, NULL, &tStlInfo);

    plSubMesh tSubMesh = {
        .tMaterial = ptAppData->tSolidMaterial
    };

    pl_sb_resize(tSubMesh.sbtVertexPositions, (uint32_t)tStlInfo.szPositionStreamSize / 3);
    pl_sb_resize(tSubMesh.sbtVertexNormals, (uint32_t)tStlInfo.szNormalStreamSize / 3);
    pl_sb_resize(tSubMesh.sbuIndices, (uint32_t)tStlInfo.szIndexBufferSize);
    pl_load_stl(acFileData, (size_t)uFileSize, (float*)tSubMesh.sbtVertexPositions, (float*)tSubMesh.sbtVertexNormals, tSubMesh.sbuIndices, &tStlInfo);
    pl_free(acFileData);
    plMeshComponent* ptMeshComponent = pl_ecs_get_component(&ptAppData->tScene.tComponentLibrary.tMeshComponentManager, ptAppData->tStlEntity);
    plMeshComponent* ptMeshComponent2 = pl_ecs_get_component(&ptAppData->tScene.tComponentLibrary.tMeshComponentManager, ptAppData->tStl2Entity);
    pl_sb_push(ptMeshComponent->sbtSubmeshes, tSubMesh);
    tSubMesh.tMaterial = ptAppData->tSolid2Material;
    pl_sb_push(ptMeshComponent2->sbtSubmeshes, tSubMesh);

    {
		plTransformComponent* ptTransformComponent = pl_ecs_get_component(ptAppData->tScene.ptTransformComponentManager, ptAppData->tStlEntity);

		ptTransformComponent->tRotation    = (plVec4){.w = 1.0f};
		ptTransformComponent->tScale       = (plVec3){1.0f, 1.0f, 1.0f};
		ptTransformComponent->tTranslation = (plVec3){0};
		ptTransformComponent->tWorld = pl_rotation_translation_scale(ptTransformComponent->tRotation, ptTransformComponent->tTranslation, ptTransformComponent->tScale);
		ptTransformComponent->tFinalTransform = ptTransformComponent->tWorld;
    }

    {
		plTransformComponent* ptTransformComponent = pl_ecs_get_component(ptAppData->tScene.ptTransformComponentManager, ptAppData->tStl2Entity);

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

    plObjectComponent* ptGrassObjectComponent = pl_ecs_get_component(&ptAppData->tScene.tComponentLibrary.tObjectComponentManager, ptAppData->tGrassEntity);
    plMeshComponent* ptGrassMeshComponent = pl_ecs_get_component(&ptAppData->tScene.tComponentLibrary.tMeshComponentManager, ptGrassObjectComponent->tMesh);
    plTransformComponent* ptGrassTransformComponent = pl_ecs_get_component(&ptAppData->tScene.tComponentLibrary.tTransformComponentManager, ptGrassObjectComponent->tTransform);
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

    pl_ecs_attach_component(&ptAppData->tScene, ptAppData->tStl2Entity, ptAppData->tStlEntity);

    {
        plObjectComponent* ptObjectComponent = pl_ecs_get_component(&ptAppData->tScene.tComponentLibrary.tObjectComponentManager, ptAppData->tStl2Entity);
        plTransformComponent* ptTransformComponent = pl_ecs_get_component(&ptAppData->tScene.tComponentLibrary.tTransformComponentManager, ptObjectComponent->tTransform);
        const plMat4 tStlTranslation = pl_mat4_translate_xyz(4.0f, 0.0f, 0.0f);
        ptTransformComponent->tWorld = tStlTranslation;
    }

    {
        plObjectComponent* ptObjectComponent = pl_ecs_get_component(&ptAppData->tScene.tComponentLibrary.tObjectComponentManager, ptAppData->tGrassEntity);
		plTransformComponent* ptTransformComponent = pl_ecs_get_component(ptAppData->tScene.ptTransformComponentManager, ptObjectComponent->tTransform);

		ptTransformComponent->tRotation    = (plVec4){.w = 1.0f};
		ptTransformComponent->tScale       = (plVec3){1.0f, 1.0f, 1.0f};
		ptTransformComponent->tTranslation = (plVec3){2.0f, 0.0f, 2.0f};
		ptTransformComponent->tWorld = pl_rotation_translation_scale(ptTransformComponent->tRotation, ptTransformComponent->tTranslation, ptTransformComponent->tScale);
		ptTransformComponent->tFinalTransform = ptTransformComponent->tWorld;
    }

    // offscreen
    plRenderPassDesc tRenderPassDesc = {
        .tColorFormat = VK_FORMAT_R8G8B8A8_UNORM,
        .tDepthFormat = pl_find_depth_stencil_format(&ptAppData->tGraphics.tDevice)
    };
    pl_create_render_pass(&ptAppData->tGraphics, &tRenderPassDesc, &ptAppData->tOffscreenPass);

    plRenderTargetDesc tRenderTargetDesc = {
        .tRenderPass = ptAppData->tOffscreenPass,
        .tSize = {1280.0f, 720.0f},
    };
    pl_create_render_target(&ptAppData->tGraphics, &tRenderTargetDesc, &ptAppData->tOffscreenTarget);

    for(uint32_t i = 0; i < ptAppData->tGraphics.tSwapchain.uImageCount; i++)
    {
        plTextureView* ptColorTextureView = &ptAppData->tGraphics.tResourceManager.sbtTextureViews[ptAppData->tOffscreenTarget.sbuColorTextureViews[i]];
        pl_sb_push(ptAppData->sbtTextures, pl_add_texture(pl_ui_get_draw_context(NULL), ptColorTextureView->_tImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
    }

    pl_create_main_render_target(&ptAppData->tGraphics, &ptAppData->tMainTarget);

    pl_scene_prepare(&ptAppData->tScene);

    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(void* pAppData)
{
    plAppData* ptAppData = pAppData; 
    vkDeviceWaitIdle(ptAppData->tGraphics.tDevice.tLogicalDevice);
    pl_cleanup_font_atlas(&ptAppData->fontAtlas);
    pl_ui_destroy_context(NULL);
    pl_cleanup_render_pass(&ptAppData->tGraphics, &ptAppData->tOffscreenPass);
    pl_cleanup_render_target(&ptAppData->tGraphics, &ptAppData->tOffscreenTarget);
    pl_cleanup_renderer(&ptAppData->tRenderer);
    pl_cleanup_graphics(&ptAppData->tGraphics);
    pl_cleanup_profile_context();
    pl_cleanup_extension_registry();
    pl_cleanup_log_context();
    pl_cleanup_data_registry();
    pl_cleanup_memory_context();
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(void* pAppData)
{
    plAppData* ptAppData = pAppData;
    plIOContext* ptIOCtx = pl_get_io_context();
    pl_resize_graphics(&ptAppData->tGraphics);
    plCameraComponent* ptCamera = pl_ecs_get_component(&ptAppData->tScene.tComponentLibrary.tCameraComponentManager, ptAppData->tCameraEntity);
    pl_camera_set_aspect(ptCamera, ptIOCtx->afMainViewportSize[0] / ptIOCtx->afMainViewportSize[1]);
    pl_create_main_render_target(&ptAppData->tGraphics, &ptAppData->tMainTarget);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~frame setup~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    plIOContext* ptIOCtx = pl_get_io_context();
    pl_begin_profile_frame();
    pl_handle_extension_reloads();
    pl_process_cleanup_queue(&ptAppData->tGraphics.tResourceManager, 1);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~input handling~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    static const float fCameraTravelSpeed = 8.0f;
    plCameraComponent* ptCamera = pl_ecs_get_component(&ptAppData->tScene.tComponentLibrary.tCameraComponentManager, ptAppData->tCameraEntity);
    plCameraComponent* ptOffscreenCamera = pl_ecs_get_component(&ptAppData->tScene.tComponentLibrary.tCameraComponentManager, ptAppData->tOffscreenCameraEntity);

    // camera space
    if(pl_is_key_pressed(PL_KEY_W, true)) pl_camera_translate(ptCamera,  0.0f,  0.0f,  fCameraTravelSpeed * ptIOCtx->fDeltaTime);
    if(pl_is_key_pressed(PL_KEY_S, true)) pl_camera_translate(ptCamera,  0.0f,  0.0f, -fCameraTravelSpeed* ptIOCtx->fDeltaTime);
    if(pl_is_key_pressed(PL_KEY_A, true)) pl_camera_translate(ptCamera, -fCameraTravelSpeed * ptIOCtx->fDeltaTime,  0.0f,  0.0f);
    if(pl_is_key_pressed(PL_KEY_D, true)) pl_camera_translate(ptCamera,  fCameraTravelSpeed * ptIOCtx->fDeltaTime,  0.0f,  0.0f);

    // world space
    if(pl_is_key_pressed(PL_KEY_F, true)) pl_camera_translate(ptCamera,  0.0f, -fCameraTravelSpeed * ptIOCtx->fDeltaTime,  0.0f);
    if(pl_is_key_pressed(PL_KEY_R, true)) pl_camera_translate(ptCamera,  0.0f,  fCameraTravelSpeed * ptIOCtx->fDeltaTime,  0.0f);

    plFrameContext* ptCurrentFrame = pl_get_frame_resources(&ptAppData->tGraphics);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~begin frame~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    if(pl_begin_frame(&ptAppData->tGraphics))
    {
        pl_ui_new_frame();

        if(!pl_ui_is_mouse_owned() && pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 1.0f))
        {
            const plVec2 tMouseDelta = pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f);
            pl_camera_rotate(ptCamera,  -tMouseDelta.y * 0.1f * ptIOCtx->fDeltaTime,  -tMouseDelta.x * 0.1f * ptIOCtx->fDeltaTime);
            pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
        }
        pl_camera_update(ptCamera);
        pl_camera_update(ptOffscreenCamera);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~drawing api~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        ptAppData->ptDrawExtApi->pl_add_text(ptAppData->fgDrawLayer, &ptAppData->fontAtlas.sbFonts[0], 13.0f, (plVec2){100.0f, 100.0f}, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, "Drawn from extension!");
        ptAppData->ptDrawExtApi->pl_add_text(ptAppData->offscreenDrawLayer, &ptAppData->fontAtlas.sbFonts[0], 42.0f, (plVec2){100.0f, 100.0f}, (plVec4){1.0f, 0.0f, 1.0f, 1.0f}, "Drawn from extension offscreen!");

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~3D drawing api~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        pl_add_3d_transform(&ptAppData->drawlist3d, &ptOffscreenCamera->tTransformMat, 0.2f, 0.02f);
        pl_add_3d_frustum(&ptAppData->drawlist3d, 
            &ptOffscreenCamera->tTransformMat, ptOffscreenCamera->fFieldOfView, ptOffscreenCamera->fAspectRatio, 
            ptOffscreenCamera->fNearZ, ptOffscreenCamera->fFarZ, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 0.02f);

        const plMat4 tTransform0 = pl_identity_mat4();
        pl_add_3d_transform(&ptAppData->drawlist3d, &tTransform0, 10.0f, 0.02f);
        pl_add_3d_bezier_quad(&ptAppData->drawlist3d, (plVec3){0.0f,0.0f,0.0f}, (plVec3){5.0f,5.0f,5.0f}, (plVec3){3.0f,4.0f,3.0f}, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 0.02f, 20);
        pl_add_3d_bezier_cubic(&ptAppData->drawlist3d, (plVec3){0.0f,0.0f,0.0f}, (plVec3){-0.5f,1.0f,-0.5f}, (plVec3){5.0f,3.5f,5.0f}, (plVec3){3.0f,4.0f,3.0f}, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 0.02f, 20);

        // ui

        if(pl_ui_begin_window("Offscreen", NULL, true))
        {
            pl_ui_layout_static(720.0f / 2.0f, 1280.0f / 2.0f, 1);
            pl_ui_image(ptAppData->sbtTextures[ptAppData->tGraphics.szCurrentFrameIndex], (plVec2){1280.0f / 2.0f, 720.0f / 2.0f});
            pl_ui_end_window();
        }

        static int iSelectedEntity = 1;

        pl_ui_set_next_window_pos((plVec2){0, 0}, PL_UI_COND_ONCE);

        if(pl_ui_begin_window("Pilot Light", NULL, false))
        {
    
            const float pfRatios[] = {1.0f};
            pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
            
            pl_ui_checkbox("UI Debug", &ptAppData->bShowUiDebug);
            pl_ui_checkbox("UI Demo", &ptAppData->bShowUiDemo);
            pl_ui_checkbox("UI Style", &ptAppData->bShowUiStyle);

            if(pl_ui_collapsing_header("Renderer"))
            {
                pl_ui_text("Dynamic Buffers");
                pl_ui_progress_bar((float)ptAppData->tScene.uDynamicBuffer0_Offset / (float)ptAppData->tScene.uDynamicBufferSize, (plVec2){-1.0f, 0.0f}, NULL);
                pl_ui_progress_bar((float)ptAppData->tScene.uDynamicBuffer1_Offset / (float)ptAppData->tScene.uDynamicBufferSize, (plVec2){-1.0f, 0.0f}, NULL);
                pl_ui_progress_bar((float)ptAppData->tScene.uDynamicBuffer2_Offset / (float)ptAppData->tScene.uDynamicBufferSize, (plVec2){-1.0f, 0.0f}, NULL);
                pl_ui_end_collapsing_header();
            }

            if(pl_ui_collapsing_header("Entities"))
            {
                plTagComponent* sbtTagComponents = ptAppData->tScene.ptTagComponentManager->pData;
                for(uint32_t i = 0; i < pl_sb_size(sbtTagComponents); i++)
                {
                    plTagComponent* ptTagComponent = &sbtTagComponents[i];
                    pl_ui_radio_button(ptTagComponent->acName, &iSelectedEntity, i + 1);
                }

                pl_ui_end_collapsing_header();
            }
            pl_ui_end_window();
        }

        if(pl_ui_begin_window("Components", NULL, false))
        {
            const float pfRatios[] = {1.0f};
            pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);

            if(pl_ui_collapsing_header("Tag"))
            {
                plTagComponent* ptTagComponent = pl_ecs_get_component(ptAppData->tScene.ptTagComponentManager, iSelectedEntity);
                pl_ui_text("Name: %s", ptTagComponent->acName);
                pl_ui_end_collapsing_header();
            }

            if(pl_ecs_has_entity(ptAppData->tScene.ptTransformComponentManager, iSelectedEntity))
            {
                
                if(pl_ui_collapsing_header("Transform"))
                {
                    plTransformComponent* ptTransformComponent = pl_ecs_get_component(ptAppData->tScene.ptTransformComponentManager, iSelectedEntity);
                    pl_ui_text("Rotation: %0.3f, %0.3f, %0.3f, %0.3f", ptTransformComponent->tRotation.x, ptTransformComponent->tRotation.y, ptTransformComponent->tRotation.z, ptTransformComponent->tRotation.w);
                    pl_ui_text("Scale: %0.3f, %0.3f, %0.3f", ptTransformComponent->tScale.x, ptTransformComponent->tScale.y, ptTransformComponent->tScale.z);
                    pl_ui_text("Translation: %0.3f, %0.3f, %0.3f", ptTransformComponent->tTranslation.x, ptTransformComponent->tTranslation.y, ptTransformComponent->tTranslation.z);
                    pl_ui_end_collapsing_header();
                }  
            }

            if(pl_ecs_has_entity(ptAppData->tScene.ptMeshComponentManager, iSelectedEntity))
            {
                
                if(pl_ui_collapsing_header("Mesh"))
                {
                    // plMeshComponent* ptMeshComponent = pl_ecs_get_component(ptAppData->tScene.ptMeshComponentManager, iSelectedEntity);
                    pl_ui_end_collapsing_header();
                }  
            }

            if(pl_ecs_has_entity(ptAppData->tScene.ptMaterialComponentManager, iSelectedEntity))
            {
                if(pl_ui_collapsing_header("Material"))
                {
                    plMaterialComponent* ptMaterialComponent = pl_ecs_get_component(ptAppData->tScene.ptMaterialComponentManager, iSelectedEntity);
                    pl_ui_text("Albedo: %0.3f, %0.3f, %0.3f, %0.3f", ptMaterialComponent->tAlbedo.r, ptMaterialComponent->tAlbedo.g, ptMaterialComponent->tAlbedo.b, ptMaterialComponent->tAlbedo.a);
                    pl_ui_text("Alpha Cutoff: %0.3f", ptMaterialComponent->fAlphaCutoff);
                    pl_ui_text("Double Sided: %s", ptMaterialComponent->bDoubleSided ? "true" : "false");
                    pl_ui_text("Outline: %s", ptMaterialComponent->bOutline ? "true" : "false");
                    pl_ui_end_collapsing_header();
                }  
            }

            if(pl_ecs_has_entity(ptAppData->tScene.ptObjectComponentManager, iSelectedEntity))
            {
                if(pl_ui_collapsing_header("Object"))
                {
                    plObjectComponent* ptObjectComponent = pl_ecs_get_component(ptAppData->tScene.ptObjectComponentManager, iSelectedEntity);
                    plTagComponent* ptTransformTag = pl_ecs_get_component(ptAppData->tScene.ptTagComponentManager, ptObjectComponent->tTransform);
                    plTagComponent* ptMeshTag = pl_ecs_get_component(ptAppData->tScene.ptTagComponentManager, ptObjectComponent->tMesh);
                    pl_ui_text("Mesh: %s", ptMeshTag->acName);
                    pl_ui_text("Transform: %s", ptTransformTag->acName);

                    pl_ui_end_collapsing_header();
                }  
            }

            if(pl_ecs_has_entity(ptAppData->tScene.ptCameraComponentManager, iSelectedEntity))
            {
                if(pl_ui_collapsing_header("Camera"))
                {
                    plCameraComponent* ptCameraComponent = pl_ecs_get_component(ptAppData->tScene.ptCameraComponentManager, iSelectedEntity);
                    pl_ui_text("Pitch: %0.3f", ptCameraComponent->fPitch);
                    pl_ui_text("Yaw: %0.3f", ptCameraComponent->fYaw);
                    pl_ui_text("Roll: %0.3f", ptCameraComponent->fRoll);
                    pl_ui_text("Near Z: %0.3f", ptCameraComponent->fNearZ);
                    pl_ui_text("Far Z: %0.3f", ptCameraComponent->fFarZ);
                    pl_ui_text("Y Field Of View: %0.3f", ptCameraComponent->fFieldOfView);
                    pl_ui_text("Aspect Ratio: %0.3f", ptCameraComponent->fAspectRatio);
                    pl_ui_text("Up Vector: %0.3f, %0.3f, %0.3f", ptCameraComponent->_tUpVec.x, ptCameraComponent->_tUpVec.y, ptCameraComponent->_tUpVec.z);
                    pl_ui_text("Forward Vector: %0.3f, %0.3f, %0.3f", ptCameraComponent->_tForwardVec.x, ptCameraComponent->_tForwardVec.y, ptCameraComponent->_tForwardVec.z);
                    pl_ui_text("Right Vector: %0.3f, %0.3f, %0.3f", ptCameraComponent->_tRightVec.x, ptCameraComponent->_tRightVec.y, ptCameraComponent->_tRightVec.z);
                    pl_ui_end_collapsing_header();
                }  
            }

            if(pl_ecs_has_entity(ptAppData->tScene.ptHierarchyComponentManager, iSelectedEntity))
            {
                if(pl_ui_collapsing_header("Hierarchy"))
                {
                    plHierarchyComponent* ptHierarchyComponent = pl_ecs_get_component(ptAppData->tScene.ptHierarchyComponentManager, iSelectedEntity);
                    plTagComponent* ptParent = pl_ecs_get_component(ptAppData->tScene.ptTagComponentManager, ptHierarchyComponent->tParent);
                    pl_ui_text("Parent: %s", ptParent->acName);
                    pl_ui_end_collapsing_header();
                }  
            }
            pl_ui_end_window();
        }

        if(ptAppData->bShowUiDemo)
            pl_ui_demo(&ptAppData->bShowUiDemo);
            
        if(ptAppData->bShowUiStyle)
            pl_ui_style(&ptAppData->bShowUiStyle);

        if(ptAppData->bShowUiDebug)
            pl_ui_debug(&ptAppData->bShowUiDebug);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~renderer begin frame~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // rotate stl model
        {
            plObjectComponent* ptObjectComponent = pl_ecs_get_component(&ptAppData->tScene.tComponentLibrary.tObjectComponentManager, ptAppData->tStlEntity);
            plTransformComponent* ptTransformComponent = pl_ecs_get_component(&ptAppData->tScene.tComponentLibrary.tTransformComponentManager, ptObjectComponent->tTransform);
            const plMat4 tStlRotation = pl_mat4_rotate_xyz(PL_PI * (float)pl_get_io_context()->dTime, 0.0f, 1.0f, 0.0f);
            const plMat4 tStlTranslation = pl_mat4_translate_xyz(0.0f, 2.0f, 0.0f);
            const plMat4 tStlTransform = pl_mul_mat4(&tStlTranslation, &tStlRotation);
            ptTransformComponent->tFinalTransform = tStlTransform;
        }

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~submit draws~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // submit draw layers
        pl_begin_profile_sample("Submit draw layers");
        pl_submit_draw_layer(ptAppData->bgDrawLayer);
        pl_submit_draw_layer(ptAppData->fgDrawLayer);
        pl_end_profile_sample();

        pl_begin_recording(&ptAppData->tGraphics);

        pl_begin_render_target(&ptAppData->tGraphics, &ptAppData->tOffscreenTarget);
        pl_reset_scene(&ptAppData->tScene);
        pl_scene_bind_target(&ptAppData->tScene, &ptAppData->tOffscreenTarget);
        pl_scene_update_ecs(&ptAppData->tScene);
        pl_scene_bind_camera(&ptAppData->tScene, ptOffscreenCamera);
        pl_draw_scene(&ptAppData->tScene);
        pl_draw_sky(&ptAppData->tScene);

        pl_submit_draw_layer(ptAppData->offscreenDrawLayer);
        pl_submit_drawlist_vulkan_ex(&ptAppData->drawlist2, 1280.0f, 720.0f, ptCurrentFrame->tCmdBuf, (uint32_t)ptAppData->tGraphics.szCurrentFrameIndex, ptAppData->tOffscreenPass._tRenderPass, VK_SAMPLE_COUNT_1_BIT);
        pl_end_render_target(&ptAppData->tGraphics);

        pl_begin_render_target(&ptAppData->tGraphics, &ptAppData->tMainTarget);
        pl_scene_bind_target(&ptAppData->tScene, &ptAppData->tMainTarget);
        pl_scene_update_ecs(&ptAppData->tScene);
        pl_scene_bind_camera(&ptAppData->tScene, ptCamera);
        pl_draw_scene(&ptAppData->tScene);
        pl_draw_sky(&ptAppData->tScene);

        // submit 3D draw list
        const plMat4 tMVP = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);
        pl_submit_3d_drawlist_vulkan(&ptAppData->drawlist3d, (float)ptIOCtx->afMainViewportSize[0], (float)ptIOCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptAppData->tGraphics.szCurrentFrameIndex, &tMVP, PL_PIPELINE_FLAG_DEPTH_TEST);

        // submit draw lists
        pl_submit_drawlist_vulkan(&ptAppData->drawlist, (float)ptIOCtx->afMainViewportSize[0], (float)ptIOCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptAppData->tGraphics.szCurrentFrameIndex);

        // submit ui drawlist
        pl_ui_render();
        pl_submit_drawlist_vulkan(pl_ui_get_draw_list(NULL), (float)ptIOCtx->afMainViewportSize[0], (float)ptIOCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptAppData->tGraphics.szCurrentFrameIndex);
        pl_submit_drawlist_vulkan(pl_ui_get_debug_draw_list(NULL), (float)ptIOCtx->afMainViewportSize[0], (float)ptIOCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptAppData->tGraphics.szCurrentFrameIndex);
        pl_end_render_target(&ptAppData->tGraphics);
        pl_end_recording(&ptAppData->tGraphics);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~end frame~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        pl_end_frame(&ptAppData->tGraphics);
    } 
    pl_end_profile_frame();
}
