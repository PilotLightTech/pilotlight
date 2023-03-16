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
    plDrawLayer*        fgDrawLayer;
    plDrawLayer*        bgDrawLayer;
    plFontAtlas         fontAtlas;
    plProfileContext    tProfileCtx;
    plLogContext        tLogCtx;
    plMemoryContext     tMemoryCtx;
    plDataRegistry      tDataRegistryCtx;
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
    uint32_t        uDynamicBuffer0;
    plScene         tScene;

    // shaders
    uint32_t uGrassShader;

    // cameras
    plEntity tCameraEntity;

    // objects
    plEntity tGrassEntity;
    plEntity tStlEntity;
    plEntity tStl2Entity;

    // materials
    plEntity tGrassMaterial;
    plEntity tSolidMaterial;
    plEntity tSolid2Material;

} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plIOContext* ptIOCtx, plAppData* ptAppData)
{

    if(ptAppData) // reload
    {
        pl_set_log_context(&ptAppData->tLogCtx);
        pl_set_profile_context(&ptAppData->tProfileCtx);
        pl_set_memory_context(&ptAppData->tMemoryCtx);
        pl_set_data_registry(&ptAppData->tDataRegistryCtx);
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
    pl_initialize_memory_context(&ptAppData->tMemoryCtx);
    pl_initialize_profile_context(&ptAppData->tProfileCtx);
    pl_initialize_data_registry(&ptAppData->tDataRegistryCtx);

    // setup logging
    pl_initialize_log_context(&ptAppData->tLogCtx);
    pl_add_log_channel("Default", PL_CHANNEL_TYPE_CONSOLE);
    pl_log_info(0, "Setup logging");

    // setup extension registry
    pl_initialize_extension_registry(&ptAppData->tExtensionRegistryCtx);
    pl_register_data("memory", &ptAppData->tMemoryCtx);
    pl_register_data("profile", &ptAppData->tProfileCtx);
    pl_register_data("log", &ptAppData->tLogCtx);
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
    ptAppData->bgDrawLayer = pl_request_draw_layer(&ptAppData->drawlist, "Background Layer");
    ptAppData->fgDrawLayer = pl_request_draw_layer(&ptAppData->drawlist, "Foreground Layer");

    // create font atlas
    pl_add_default_font(&ptAppData->fontAtlas);
    pl_build_font_atlas(pl_ui_get_draw_context(NULL), &ptAppData->fontAtlas);
    pl_ui_set_default_font(&ptAppData->fontAtlas.sbFonts[0]);
    pl_register_data("draw", pl_ui_get_draw_context(NULL));

    // renderer
    pl_setup_renderer(&ptAppData->tGraphics, &ptAppData->tRenderer);
    pl_create_scene(&ptAppData->tRenderer, &ptAppData->tScene);
    
    // buffers
    ptAppData->uDynamicBuffer0 = pl_create_constant_buffer_ex(&ptAppData->tGraphics.tResourceManager, pl_minu(ptAppData->tGraphics.tDevice.tDeviceProps.limits.maxUniformBufferRange, 65536));

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~entity IDs~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    // cameras
    ptAppData->tCameraEntity = pl_ecs_create_camera(&ptAppData->tScene, "main camera", (plVec3){-6.211f, 3.647f, 0.827f}, PL_PI_3, ptIOCtx->afMainViewportSize[0] / ptIOCtx->afMainViewportSize[1], 0.01f, 400.0f);
    plCameraComponent* ptCamera = pl_ecs_get_component(&ptAppData->tScene.tComponentLibrary.tCameraComponentManager, ptAppData->tCameraEntity);
    pl_camera_set_pitch_yaw(ptCamera, -0.244f, -1.488f);

    // objects
    ptAppData->tStlEntity   = pl_ecs_create_object(&ptAppData->tScene, "stl object");
    ptAppData->tStl2Entity  = pl_ecs_create_object(&ptAppData->tScene, "stl object");
    ptAppData->tGrassEntity = pl_ecs_create_object(&ptAppData->tScene, "grass object");
    pl_sb_push(ptAppData->tRenderer.sbtObjectEntities, ptAppData->tGrassEntity);
    pl_sb_push(ptAppData->tRenderer.sbtObjectEntities, ptAppData->tStlEntity);
    pl_sb_push(ptAppData->tRenderer.sbtObjectEntities, ptAppData->tStl2Entity);

    pl_ext_load_gltf(&ptAppData->tScene, "../data/glTF-Sample-Models-master/2.0/FlightHelmet/glTF/FlightHelmet.gltf");

    // materials
    ptAppData->tGrassMaterial   = pl_ecs_create_material(&ptAppData->tScene, "grass material");
    ptAppData->tSolidMaterial   = pl_ecs_create_material(&ptAppData->tScene, "solid material");
    ptAppData->tSolid2Material  = pl_ecs_create_material(&ptAppData->tScene, "solid material");

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~materials~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // grass
    plShaderDesc tGrassShaderDesc = {
        ._tRenderPass                        = ptAppData->tGraphics.tRenderPass,
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
        .tType       = VK_IMAGE_TYPE_2D,
        .tViewType   = VK_IMAGE_VIEW_TYPE_2D
    };

    // grass material
    plMaterialComponent* ptGrassMaterial = pl_ecs_get_component(&ptAppData->tScene.tComponentLibrary.tMaterialComponentManager, ptAppData->tGrassMaterial);
    ptGrassMaterial->uShader = ptAppData->uGrassShader;
    ptGrassMaterial->tShaderType = PL_SHADER_TYPE_CUSTOM;
    ptGrassMaterial->bDoubleSided = true;
    ptGrassMaterial->uAlbedoMap = pl_create_texture(&ptAppData->tGraphics.tResourceManager, tTextureDesc, sizeof(unsigned char) * texHeight * texHeight * 4, rawBytes);
    ptGrassMaterial->tGraphicsState.ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_HAS_NORMAL | PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0;
    ptGrassMaterial->tGraphicsState.ulCullMode           = VK_CULL_MODE_NONE;
    ptGrassMaterial->tGraphicsState.ulShaderTextureFlags = PL_SHADER_TEXTURE_FLAG_BINDING_0;

    // solid materials
    plMaterialComponent* ptSolidMaterial = pl_ecs_get_component(&ptAppData->tScene.tComponentLibrary.tMaterialComponentManager, ptAppData->tSolidMaterial);
    ptSolidMaterial->tGraphicsState.ulVertexStreamMask = PL_MESH_FORMAT_FLAG_HAS_NORMAL | PL_MESH_FORMAT_FLAG_HAS_COLOR_0;

    plMaterialComponent* ptSolidMaterial2 = pl_ecs_get_component(&ptAppData->tScene.tComponentLibrary.tMaterialComponentManager, ptAppData->tSolid2Material);
    ptSolidMaterial2->tGraphicsState.ulVertexStreamMask = PL_MESH_FORMAT_FLAG_HAS_NORMAL | PL_MESH_FORMAT_FLAG_HAS_COLOR_0;
    pl_material_outline(&ptAppData->tScene, ptAppData->tSolid2Material);


    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~STL Model~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const plStlOptions tStlOptions = {
        .bIncludeColor   = true,
        .bIncludeNormals = true,
        .afColor = { 1.0f, 0.0f, 0.0f, 1.0f }
    };
    plStlInfo tStlInfo = {0};
    uint32_t uFileSize = 0u;
    pl_read_file("../data/pilotlight-assets-master/meshes/monkey.stl", &uFileSize, NULL, "rb");
    char* acFileData = pl_alloc(uFileSize);
    memset(acFileData, 0, uFileSize);
    pl_read_file("../data/pilotlight-assets-master/meshes/monkey.stl", &uFileSize, acFileData, "rb");
    pl_load_stl(acFileData, uFileSize, tStlOptions, NULL, NULL, NULL, &tStlInfo);
    float* afVertexBuffer0    = pl_alloc(sizeof(float) * tStlInfo.szVertexStream0Size);
    float* afVertexBuffer1    = pl_alloc(sizeof(float) * tStlInfo.szVertexStream1Size);
    memset(afVertexBuffer1, 0, sizeof(float) * tStlInfo.szVertexStream1Size);
    uint32_t* auIndexBuffer = pl_alloc(sizeof(uint32_t) * tStlInfo.szIndexBufferSize);
    pl_load_stl(acFileData, uFileSize, tStlOptions, afVertexBuffer0, afVertexBuffer1, auIndexBuffer, &tStlInfo);
    const uint32_t uStlStorageOffset = pl_sb_size(ptAppData->tRenderer.sbfStorageBuffer) / 4;

    const uint32_t uPrevIndex = pl_sb_add_n(ptAppData->tRenderer.sbfStorageBuffer, (uint32_t)tStlInfo.szVertexStream1Size);
    memcpy(&ptAppData->tRenderer.sbfStorageBuffer[uPrevIndex], afVertexBuffer1, tStlInfo.szVertexStream1Size * sizeof(float));

    plMeshComponent* ptMeshComponent = pl_ecs_get_component(&ptAppData->tScene.tComponentLibrary.tMeshComponentManager, ptAppData->tStlEntity);
    plMeshComponent* ptMeshComponent2 = pl_ecs_get_component(&ptAppData->tScene.tComponentLibrary.tMeshComponentManager, ptAppData->tStl2Entity);
    plSubMesh tSubMesh = {
        .tMesh = {
            .uIndexCount   = (uint32_t)tStlInfo.szIndexBufferSize,
            .uVertexCount  = (uint32_t)tStlInfo.szVertexStream0Size / 3,
            .uIndexBuffer  = pl_create_index_buffer(&ptAppData->tGraphics.tResourceManager, sizeof(uint32_t) * tStlInfo.szIndexBufferSize, auIndexBuffer),
            .uVertexBuffer = pl_create_vertex_buffer(&ptAppData->tGraphics.tResourceManager, tStlInfo.szVertexStream0Size * sizeof(float), sizeof(plVec3), afVertexBuffer0),
            .ulVertexStreamMask = PL_MESH_FORMAT_FLAG_HAS_NORMAL | PL_MESH_FORMAT_FLAG_HAS_COLOR_0
        },
        .tMaterial = ptAppData->tSolidMaterial,
        .uStorageOffset = uStlStorageOffset
    };
    pl_sb_push(ptMeshComponent->sbtSubmeshes, tSubMesh);
    tSubMesh.tMaterial = ptAppData->tSolid2Material;
    pl_sb_push(ptMeshComponent2->sbtSubmeshes, tSubMesh);

    pl_free(acFileData);
    pl_free(afVertexBuffer0);
    pl_free(afVertexBuffer1);
    pl_free(auIndexBuffer);
    
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~grass~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const float fGrassX = 0.0f;
    const float fGrassY = 0.0f;
    const float fGrassZ = 0.0f;
    const float fGrassHeight = 1.0f;

    const plVec3 atVertexBuffer[] = {

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
    const plVec4 atStorageBuffer[] = 
    {
        {.y = 1.0f}, {.x = 0.0f, .y = 0.0f}, {.y = 1.0f},
        {.x = 1.0f, .y = 0.0f}, {.y = 1.0f}, {.x = 1.0f, .y = 1.0f},
        {.y = 1.0f}, {.x = 0.0f, .y = 1.0f}, {.y = 1.0f},
        {.x = 0.0f, .y = 0.0f}, {.y = 1.0f}, {.x = 1.0f, .y = 0.0f},
        {.y = 1.0f}, {.x = 1.0f, .y = 1.0f}, {.y = 1.0f},
        {.x = 0.0f, .y = 1.0f}, {.y = 1.0f}, {.x = 0.0f, .y = 0.0f},
        {.y = 1.0f}, {.x = 1.0f, .y = 0.0f}, {.y = 1.0f},
        {.x = 1.0f, .y = 1.0f}, {.y = 1.0f},{.x = 0.0f, .y = 1.0f},
    };

    const uint32_t auGrassIndexBuffer[] = {
        0,  3,  2, 0,  2, 1,
        4,  7,  6, 4,  6, 5,
        8, 11, 10, 8, 10, 9
    };

    const uint32_t uStorageOffset = pl_sb_size(ptAppData->tRenderer.sbfStorageBuffer) / 4;
    pl_sb_reserve(ptAppData->tRenderer.sbfStorageBuffer, pl_sb_size(ptAppData->tRenderer.sbfStorageBuffer) + 24);
    for(uint32_t i = 0; i < 24; i++)
    {
        pl_sb_push(ptAppData->tRenderer.sbfStorageBuffer, atStorageBuffer[i].x);
        pl_sb_push(ptAppData->tRenderer.sbfStorageBuffer, atStorageBuffer[i].y);
        pl_sb_push(ptAppData->tRenderer.sbfStorageBuffer, atStorageBuffer[i].z);
        pl_sb_push(ptAppData->tRenderer.sbfStorageBuffer, atStorageBuffer[i].w);
    }

    plMeshComponent* ptGrassMeshComponent = pl_ecs_get_component(&ptAppData->tScene.tComponentLibrary.tMeshComponentManager, ptAppData->tGrassEntity);
    const plSubMesh tGrassSubMesh = {
        .tMesh = {
            .uIndexCount         = 18,
            .uVertexCount        = 36,
            .uVertexBuffer       = pl_create_vertex_buffer(&ptAppData->tGraphics.tResourceManager, sizeof(plVec3) * 12, sizeof(plVec3), atVertexBuffer),
            .uIndexBuffer        = pl_create_index_buffer(&ptAppData->tGraphics.tResourceManager, sizeof(uint32_t) * 18, auGrassIndexBuffer),
            .ulVertexStreamMask  = PL_MESH_FORMAT_FLAG_HAS_NORMAL | PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0,
            .uVertexOffset       = 0
        },
        .tMaterial = ptAppData->tGrassMaterial,
        .uStorageOffset = uStorageOffset
    };
    pl_sb_push(ptGrassMeshComponent->sbtSubmeshes, tGrassSubMesh);

    pl_ecs_attach_component(&ptAppData->tScene, ptAppData->tStl2Entity, ptAppData->tStlEntity);

    {
        plObjectComponent* ptObjectComponent = pl_ecs_get_component(&ptAppData->tScene.tComponentLibrary.tObjectComponentManager, ptAppData->tStl2Entity);
        plTransformComponent* ptTransformComponent = pl_ecs_get_component(&ptAppData->tScene.tComponentLibrary.tTransformComponentManager, ptObjectComponent->tTransform);
        const plMat4 tStlTranslation = pl_mat4_translate_xyz(4.0f, 0.0f, 0.0f);
        ptTransformComponent->tWorld = tStlTranslation;
    }

    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    vkDeviceWaitIdle(ptAppData->tGraphics.tDevice.tLogicalDevice);
    pl_cleanup_font_atlas(&ptAppData->fontAtlas);
    pl_ui_destroy_context(NULL);
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
pl_app_resize(plAppData* ptAppData)
{
    plIOContext* ptIOCtx = pl_get_io_context();
    pl_resize_graphics(&ptAppData->tGraphics);
    plCameraComponent* ptCamera = pl_ecs_get_component(&ptAppData->tScene.tComponentLibrary.tCameraComponentManager, ptAppData->tCameraEntity);
    pl_camera_set_aspect(ptCamera, ptIOCtx->afMainViewportSize[0] / ptIOCtx->afMainViewportSize[1]);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~frame setup~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    plIOContext* ptIOCtx = pl_get_io_context();
    pl_begin_profile_frame(ptIOCtx->ulFrameCount);
    pl_handle_extension_reloads();
    pl_process_cleanup_queue(&ptAppData->tGraphics.tResourceManager, 1);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~input handling~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    static const float fCameraTravelSpeed = 8.0f;
    plCameraComponent* ptCamera = pl_ecs_get_component(&ptAppData->tScene.tComponentLibrary.tCameraComponentManager, ptAppData->tCameraEntity);
    if(pl_is_key_pressed(PL_KEY_W, true)) pl_camera_translate(ptCamera,  0.0f,  0.0f,  fCameraTravelSpeed * ptIOCtx->fDeltaTime);
    if(pl_is_key_pressed(PL_KEY_S, true)) pl_camera_translate(ptCamera,  0.0f,  0.0f, -fCameraTravelSpeed* ptIOCtx->fDeltaTime);
    if(pl_is_key_pressed(PL_KEY_A, true)) pl_camera_translate(ptCamera, -fCameraTravelSpeed * ptIOCtx->fDeltaTime,  0.0f,  0.0f);
    if(pl_is_key_pressed(PL_KEY_D, true)) pl_camera_translate(ptCamera,  fCameraTravelSpeed * ptIOCtx->fDeltaTime,  0.0f,  0.0f);
    if(pl_is_key_pressed(PL_KEY_F, true)) pl_camera_translate(ptCamera,  0.0f, -fCameraTravelSpeed * ptIOCtx->fDeltaTime,  0.0f);
    if(pl_is_key_pressed(PL_KEY_R, true)) pl_camera_translate(ptCamera,  0.0f,  fCameraTravelSpeed * ptIOCtx->fDeltaTime,  0.0f);

    plFrameContext* ptCurrentFrame = pl_get_frame_resources(&ptAppData->tGraphics);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~begin frame~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    if(pl_begin_frame(&ptAppData->tGraphics))
    {
        pl_ui_new_frame();

        pl_begin_recording(&ptAppData->tGraphics);

        if(!pl_ui_is_mouse_owned() && pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 1.0f))
        {
            const plVec2 tMouseDelta = pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f);
            pl_camera_rotate(ptCamera,  -tMouseDelta.y * 0.1f * ptIOCtx->fDeltaTime,  -tMouseDelta.x * 0.1f * ptIOCtx->fDeltaTime);
            pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
        }

        pl_camera_update(ptCamera);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~renderer begin frame~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        pl_scene_update_ecs(&ptAppData->tScene);
        pl_bind_camera(&ptAppData->tRenderer, ptCamera);

        // storage buffer has items
        if(pl_sb_size(ptAppData->tRenderer.sbfStorageBuffer) > 0)
        {
            if(ptAppData->tRenderer.uGlobalStorageBuffer != UINT32_MAX)
            {
                pl_submit_buffer_for_deletion(&ptAppData->tGraphics.tResourceManager, ptAppData->tRenderer.uGlobalStorageBuffer);
            }
            ptAppData->tRenderer.uGlobalStorageBuffer = pl_create_storage_buffer(&ptAppData->tGraphics.tResourceManager, pl_sb_size(ptAppData->tRenderer.sbfStorageBuffer) * sizeof(float), ptAppData->tRenderer.sbfStorageBuffer);
            pl_sb_reset(ptAppData->tRenderer.sbfStorageBuffer);

            uint32_t atBuffers0[] = {ptAppData->uDynamicBuffer0, ptAppData->tRenderer.uGlobalStorageBuffer};
            size_t aszRangeSizes[] = {sizeof(plGlobalInfo), VK_WHOLE_SIZE};
            pl_update_bind_group(&ptAppData->tGraphics, &ptAppData->tRenderer.tGlobalBindGroup, 2, atBuffers0, aszRangeSizes, 0, NULL);
        }

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~submit meshes~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // rotate stl model
        {
            plObjectComponent* ptObjectComponent = pl_ecs_get_component(&ptAppData->tScene.tComponentLibrary.tObjectComponentManager, ptAppData->tStlEntity);
            plTransformComponent* ptTransformComponent = pl_ecs_get_component(&ptAppData->tScene.tComponentLibrary.tTransformComponentManager, ptObjectComponent->tTransform);
            const plMat4 tStlRotation = pl_mat4_rotate_xyz(PL_PI * (float)pl_get_io_context()->dTime, 0.0f, 1.0f, 0.0f);
            const plMat4 tStlTranslation = pl_mat4_translate_xyz(0.0f, 2.0f, 0.0f);
            const plMat4 tStlTransform = pl_mul_mat4(&tStlTranslation, &tStlRotation);
            ptTransformComponent->tFinalTransform = tStlTransform;
        }

        pl_bind_common_buffer(&ptAppData->tRenderer, ptAppData->uDynamicBuffer0);
        
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~submit draws~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        const plBuffer* ptBuffer0 = &ptAppData->tGraphics.tResourceManager.sbtBuffers[ptAppData->uDynamicBuffer0];
        const uint32_t uBufferFrameOffset0 = ((uint32_t)ptBuffer0->szSize / ptAppData->tGraphics.uFramesInFlight) * (uint32_t)ptAppData->tGraphics.szCurrentFrameIndex;

        plGlobalInfo* ptGlobalInfo    = (plGlobalInfo*)&ptBuffer0->pucMapping[uBufferFrameOffset0];
        ptGlobalInfo->tAmbientColor   = (plVec4){0.0f, 0.0f, 0.0f, 1.0f};
        ptGlobalInfo->tCameraPos      = (plVec4){.xyz = ptCamera->tPos, .w = 0.0f};
        ptGlobalInfo->tCameraView     = ptCamera->tViewMat;
        ptGlobalInfo->tCameraViewProj = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);
        ptGlobalInfo->fTime           = (float)ptIOCtx->dTime;

        pl_draw_scene(&ptAppData->tScene);
        pl_draw_sky(&ptAppData->tScene);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~drawing api~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        ptAppData->ptDrawExtApi->pl_add_text(ptAppData->fgDrawLayer, &ptAppData->fontAtlas.sbFonts[0], 13.0f, (plVec2){100.0f, 100.0f}, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, "Drawn from extension!");

        // ui

        static bool bOpen = false;

        pl_ui_set_next_window_pos((plVec2){0, 0}, PL_UI_COND_ONCE);

        if(pl_ui_begin_window("Pilot Light", NULL, false))
        {
    
            const float pfRatios[] = {1.0f};
            pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
            
            pl_ui_checkbox("UI Debug", &ptAppData->bShowUiDebug);
            pl_ui_checkbox("Camera Info", &bOpen);
            pl_ui_checkbox("UI Demo", &ptAppData->bShowUiDemo);
            pl_ui_checkbox("UI Style", &ptAppData->bShowUiStyle);
            

            const float pfRatios2[] = {100.0f};
            pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios2);
            if(pl_ui_button("Move"))
            {
                pl_ui_set_next_window_collapse(true, PL_UI_COND_ONCE);
            }

            pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
            // if(pl_ui_collapsing_header("Materials"))
            // {
            //     for(uint32_t i = 0; i < pl_sb_size(ptAppData->tAssetRegistry.sbtMaterials); i++)
            //     {
            //         plMaterialComponent* ptMaterial = &ptAppData->tAssetRegistry.sbtMaterials[i];

            //         static bool bOpen0 = false;
            //         if(pl_ui_tree_node(ptMaterial->acName))
            //         {
            //             pl_ui_text("Double Sided: %s", ptMaterial->bDoubleSided ? "true" : "false");
            //             pl_ui_text("Alpha cutoff: %0.1f", ptMaterial->fAlphaCutoff);
            //             ptMaterial->uShader      == 0 ? pl_ui_text("Shader: None")       : pl_ui_text("Shader: %u", ptMaterial->uShader);
            //             ptMaterial->uAlbedoMap   == 0 ? pl_ui_text("Albedo Map: None")   : pl_ui_text("Albedo Map: %u", ptMaterial->uAlbedoMap);
            //             ptMaterial->uNormalMap   == 0 ? pl_ui_text("Normal Map: None")   : pl_ui_text("Normal Map: %u", ptMaterial->uNormalMap);
            //             pl_ui_tree_pop();
            //         }
            //     }
            //     pl_ui_end_collapsing_header();
            // }
            pl_ui_end_window();
        }
        
        if(bOpen)
        {
            if(pl_ui_begin_window("Camera Info", &bOpen, true))
            {
                pl_ui_text("Pos: %.3f, %.3f, %.3f", ptCamera->tPos.x, ptCamera->tPos.y, ptCamera->tPos.z);
                pl_ui_text("Pitch: %.3f, Yaw: %.3f, Roll:%.3f", ptCamera->fPitch, ptCamera->fYaw, ptCamera->fRoll);
                pl_ui_text("Up: %.3f, %.3f, %.3f", ptCamera->_tUpVec.x, ptCamera->_tUpVec.y, ptCamera->_tUpVec.z);
                pl_ui_text("Forward: %.3f, %.3f, %.3f", ptCamera->_tForwardVec.x, ptCamera->_tForwardVec.y, ptCamera->_tForwardVec.z);
                pl_ui_text("Right: %.3f, %.3f, %.3f", ptCamera->_tRightVec.x, ptCamera->_tRightVec.y, ptCamera->_tRightVec.z);  
                pl_ui_end_window();
            }
            
        }

        if(ptAppData->bShowUiDemo)
            pl_ui_demo(&ptAppData->bShowUiDemo);
            
        if(ptAppData->bShowUiStyle)
            pl_ui_style(&ptAppData->bShowUiStyle);

        if(ptAppData->bShowUiDebug)
            pl_ui_debug(&ptAppData->bShowUiDebug);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~submit draws~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // submit draw layers
        pl_begin_profile_sample("Submit draw layers");
        pl_submit_draw_layer(ptAppData->bgDrawLayer);
        pl_submit_draw_layer(ptAppData->fgDrawLayer);
        pl_end_profile_sample();

        pl_ui_render();

        // submit draw lists
        pl_submit_drawlist_vulkan(&ptAppData->drawlist, (float)ptIOCtx->afMainViewportSize[0], (float)ptIOCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptAppData->tGraphics.szCurrentFrameIndex);

        // submit ui drawlist
        pl_submit_drawlist_vulkan(pl_ui_get_draw_list(NULL), (float)ptIOCtx->afMainViewportSize[0], (float)ptIOCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptAppData->tGraphics.szCurrentFrameIndex);
        pl_submit_drawlist_vulkan(pl_ui_get_debug_draw_list(NULL), (float)ptIOCtx->afMainViewportSize[0], (float)ptIOCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptAppData->tGraphics.szCurrentFrameIndex);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~end frame~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        pl_end_main_pass(&ptAppData->tGraphics);
        pl_end_recording(&ptAppData->tGraphics);
        pl_end_frame(&ptAppData->tGraphics);
    } 
    pl_end_profile_frame();
}
