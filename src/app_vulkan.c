/*
   vulkan_app.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] helpers
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
// [SECTION] helper implementations
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
#include "pl_draw_vulkan.h"
#include "pl_math.h"
#include "pl_camera.h"
#include "pl_registry.h"
#include "pl_ext.h"
#include "pl_ui.h"
#include "pl_renderer.h"

// extensions
#include "pl_draw_extension.h"
#include "pl_gltf_extension.h"
#include "pl_stl_extension.h"
#include "stb_image.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAppData
{
    plGraphics          tGraphics;
    plDrawContext       tCtx;
    plDrawList          drawlist;
    plDrawLayer*        fgDrawLayer;
    plDrawLayer*        bgDrawLayer;
    plFontAtlas         fontAtlas;
    plProfileContext    tProfileCtx;
    plLogContext        tLogCtx;
    plMemoryContext     tMemoryCtx;
    plDataRegistry      tDataRegistryCtx;
    plExtensionRegistry tExtensionRegistryCtx;
    plCamera            tCamera;
    plUiContext         tUiContext;

    // extension apis
    plDrawExtension* ptDrawExtApi;

    // renderer
    plRenderer      tRenderer;
    plAssetRegistry tAssetRegistry;

    // materials
    uint32_t uGrassMaterial;
    uint32_t uSolidMaterial;

    // gltf
    plGltf          tGltf;
    uint32_t*       sbuMaterialIndices;
    plBindGroup*    sbtBindGroups;
    uint32_t        uConstantBuffer; 

    // stl
    plBindGroup     tStlBindGroup2;
    plMesh          tStlMesh;
    uint32_t        uStlConstantBuffer;

    // grass
    plVec3*         sbtGrassVertexData;
    uint32_t*       sbuGrassMaterialIndices;
    uint32_t        uGrassIndexBuffer;
    uint32_t        uGrassMeshCount;
    uint32_t        uGrassShader;
    uint32_t        uGrassTexture;
    uint32_t        uGrassConstantBuffer;
    plBindGroup*    sbtGrassBindGroup2;
    plMesh*         sbtGrassMeshes;
    plTextureId     tGrassTextureId;

    // skybox
    uint32_t        uSkyboxTexture;
    uint32_t        uSkyboxShader;
    plBindGroup     tSkyboxBindGroup2;
    plMesh          tSkyboxMesh;
    uint32_t        uSkyboxConstantBuffer;

} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] helpers
//-----------------------------------------------------------------------------

static inline float frandom(float fMax){ return (float)rand()/(float)(RAND_MAX/fMax);}
void     pl_setup_grass(plAppData* ptAppData, uint32_t uMaxGrassMeshes);
void     pl_load_single_grass(plAppData* ptAppData, float fX, float fY, float fZ, float fHeight);
uint32_t pl_load_shader(plAppData* ptAppData);
uint32_t pl_load_skybox_shader(plAppData* ptAppData);

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
    pl_register_data("draw", &ptAppData->tCtx);

    // load extensions
    plExtension tExtension = {0};
    pl_get_draw_extension_info(&tExtension);
    pl_load_extension(&tExtension);

    // load extension apis
    plExtension* ptExtension = pl_get_extension(PL_EXT_DRAW);
    ptAppData->ptDrawExtApi = pl_get_api(ptExtension, PL_EXT_API_DRAW);

    // setup renderer
    pl_setup_graphics(&ptAppData->tGraphics);
    
    // setup drawing api
    pl_initialize_draw_context_vulkan(&ptAppData->tCtx, ptAppData->tGraphics.tDevice.tPhysicalDevice, ptAppData->tGraphics.tSwapchain.uImageCount, ptAppData->tGraphics.tDevice.tLogicalDevice);
    pl_register_drawlist(&ptAppData->tCtx, &ptAppData->drawlist);
    pl_setup_drawlist_vulkan(&ptAppData->drawlist, ptAppData->tGraphics.tRenderPass, ptAppData->tGraphics.tSwapchain.tMsaaSamples);
    ptAppData->bgDrawLayer = pl_request_draw_layer(&ptAppData->drawlist, "Background Layer");
    ptAppData->fgDrawLayer = pl_request_draw_layer(&ptAppData->drawlist, "Foreground Layer");

    // create font atlas
    pl_add_default_font(&ptAppData->fontAtlas);
    pl_build_font_atlas(&ptAppData->tCtx, &ptAppData->fontAtlas);

    // ui
    pl_ui_setup_context(&ptAppData->tCtx, &ptAppData->tUiContext);
    pl_setup_drawlist_vulkan(ptAppData->tUiContext.ptDrawlist, ptAppData->tGraphics.tRenderPass, ptAppData->tGraphics.tSwapchain.tMsaaSamples);
    ptAppData->tUiContext.ptFont = &ptAppData->fontAtlas.sbFonts[0];

    // renderer
    pl_setup_asset_registry(&ptAppData->tGraphics, &ptAppData->tAssetRegistry);
    pl_setup_renderer(&ptAppData->tGraphics, &ptAppData->tAssetRegistry, &ptAppData->tRenderer);

    // camera
    ptAppData->tCamera = pl_create_perspective_camera((plVec3){0.0f, 2.0f, 8.5f}, PL_PI_3, ptIOCtx->afMainViewportSize[0] / ptIOCtx->afMainViewportSize[1], 0.01f, 400.0f);

    // create shader
    const uint32_t uShader = pl_load_shader(ptAppData);
    ptAppData->uSkyboxShader = pl_load_skybox_shader(ptAppData);

    // load skybox
    ptAppData->tSkyboxMesh.uIndexCount = 36;
    ptAppData->tSkyboxMesh.uVertexCount = 24;
    const float fCubeSide = 0.5f;
    float acSkyBoxVertices[] = {
        -fCubeSide, -fCubeSide, -fCubeSide,
         fCubeSide, -fCubeSide, -fCubeSide,
        -fCubeSide,  fCubeSide, -fCubeSide,
         fCubeSide,  fCubeSide, -fCubeSide,
        -fCubeSide, -fCubeSide,  fCubeSide,
         fCubeSide, -fCubeSide,  fCubeSide,
        -fCubeSide,  fCubeSide,  fCubeSide,
         fCubeSide,  fCubeSide,  fCubeSide 
    };

    uint32_t acSkyboxIndices[] =
    {
        0, 2, 1, 2, 3, 1,
        1, 3, 5, 3, 7, 5,
        2, 6, 3, 3, 6, 7,
        4, 5, 7, 4, 7, 6,
        0, 4, 2, 2, 4, 6,
        0, 1, 4, 1, 5, 4
    };
    ptAppData->tSkyboxMesh.ulVertexStreamMask0 = PL_MESH_FORMAT_FLAG_HAS_POSITION;
    ptAppData->tSkyboxMesh.ulVertexStreamMask1 = PL_MESH_FORMAT_FLAG_NONE;
    ptAppData->tSkyboxMesh.uVertexBuffer = pl_create_vertex_buffer(&ptAppData->tGraphics.tResourceManager, sizeof(float) * 24, sizeof(float), acSkyBoxVertices);
    ptAppData->tSkyboxMesh.uIndexBuffer = pl_create_index_buffer(&ptAppData->tGraphics.tResourceManager, sizeof(uint32_t) * 36, acSkyboxIndices);
    ptAppData->uSkyboxConstantBuffer = pl_create_constant_buffer(&ptAppData->tGraphics.tResourceManager, sizeof(plGlobalInfo), ptAppData->tGraphics.uFramesInFlight);
    
    plBindGroupLayout tSkyboxGroupLayout0 = {
        .uBufferCount = 1,
        .aBuffers      = {
            { .tType       = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}
        },
        .uTextureCount = 1,
        .aTextures     = {
            { .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uSlot = 1, .tStageFlags = VK_SHADER_STAGE_FRAGMENT_BIT }
        }
    };
    pl_create_bind_group(&ptAppData->tGraphics, &tSkyboxGroupLayout0, &ptAppData->tSkyboxBindGroup2, "skybox bind group");
    pl_update_bind_group(&ptAppData->tGraphics, &ptAppData->tSkyboxBindGroup2, 1, &ptAppData->uSkyboxConstantBuffer, 1, &ptAppData->uSkyboxTexture);

    // load gltf meshes
    // pl_ext_load_gltf(&ptAppData->tRenderer, "../data/glTF-Sample-Models-master/2.0/DamagedHelmet/glTF/DamagedHelmet.gltf", &ptAppData->tGltf);
    pl_ext_load_gltf(&ptAppData->tRenderer, "../data/glTF-Sample-Models-master/2.0/Sponza/glTF/Sponza.gltf", &ptAppData->tGltf);
    pl_sb_reserve(ptAppData->sbtBindGroups, pl_sb_size(ptAppData->tGltf.sbtMeshes));
    pl_sb_reserve(ptAppData->sbuMaterialIndices, pl_sb_size(ptAppData->tGltf.sbtMeshes));

    ptAppData->uConstantBuffer = pl_create_constant_buffer(&ptAppData->tGraphics.tResourceManager, sizeof(plObjectInfo), ptAppData->tGraphics.uFramesInFlight * pl_sb_size(ptAppData->tGltf.sbtMeshes));
    plBindGroupLayout tGroupLayout0 = {
        .uBufferCount = 1,
        .aBuffers      = {
            { .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}  
        }
    };
    for(uint32_t i = 0; i < pl_sb_size(ptAppData->tGltf.sbtMeshes); i++)
    {
        pl_sb_push(ptAppData->sbtBindGroups, (plBindGroup){0});
        pl_create_bind_group(&ptAppData->tGraphics, &tGroupLayout0, &pl_sb_last(ptAppData->sbtBindGroups), "gltf");
        pl_update_bind_group(&ptAppData->tGraphics, &pl_sb_last(ptAppData->sbtBindGroups), 1, &ptAppData->uConstantBuffer, 0, NULL);
    }

    // const plMat4 tGltfRotation = pl_mat4_scale_xyz(0.008f, 0.008f, 0.008f);
    plMat4 tGltfRotation = pl_identity_mat4();
    // const plMat4 tGltfTranslation = pl_mat4_translate_xyz(0.0f, 0.0f, 0.0f);
    for(uint32_t i = 0; i < pl_sb_size(ptAppData->tGltf.sbtScenes[ptAppData->tGltf.uScene].sbuRootNodes); i++)
    {
        const uint32_t uRootNode = ptAppData->tGltf.sbtScenes[ptAppData->tGltf.uScene].sbuRootNodes[i];
        pl_update_nodes(&ptAppData->tGraphics, ptAppData->tGltf.sbuVertexOffsets, ptAppData->tGltf.sbtNodes, uRootNode, ptAppData->uConstantBuffer, &tGltfRotation);
    }
    
    // for(uint32_t j = 0; j < ptAppData->tGraphics.uFramesInFlight; j++)
    // {

    //     for(uint32_t i = 0; i < pl_sb_size(ptAppData->tGltf.sbtNodes); i++)
    //     {

    //         plObjectInfo* ptObjectInfo = pl_get_constant_buffer_data_ex(&ptAppData->tGraphics.tResourceManager, ptAppData->uConstantBuffer, (size_t)j, i);
    //         // ptObjectInfo->tModel = pl_mul_mat4(&tGltfTranslation, &tGltfRotation);
    //         ptObjectInfo->uVertexOffset = ptAppData->tGltf.sbuVertexOffsets[i];
    //   }
    // }

    // load grass
    const uint32_t uGrassRows = 10;
    const uint32_t uGrassColumns = 10;
    const float fGrassSpacing = 0.25f;
    pl_setup_grass(ptAppData, uGrassRows * uGrassColumns);
    const plVec3 tGrassCenterPoint = {(float)uGrassColumns * fGrassSpacing / 2.0f};

    pl_sb_reserve(ptAppData->sbtGrassVertexData, uGrassRows * uGrassColumns * 12);
    for(uint32_t i = 0; i < uGrassRows; i++)
    {
        for(uint32_t j = 0; j < uGrassColumns; j++)
        {
            pl_load_single_grass(ptAppData, 
            tGrassCenterPoint.x + (float)j * -fGrassSpacing + frandom(fGrassSpacing * 8.0f) - fGrassSpacing * 4.0f, 
            tGrassCenterPoint.y + 0.0f, 
            tGrassCenterPoint.z + i * fGrassSpacing + frandom(fGrassSpacing * 8.0f) - fGrassSpacing * 4.0f, 
            frandom(0.5f));
        }
    }
    const uint32_t uGrassVertexBuffer = pl_create_vertex_buffer(&ptAppData->tGraphics.tResourceManager, sizeof(plVec3) * 12 * ptAppData->uGrassMeshCount, sizeof(plVec3), ptAppData->sbtGrassVertexData);

    for(uint32_t i = 0; i < ptAppData->uGrassMeshCount; i++)
        ptAppData->sbtGrassMeshes[i].uVertexBuffer = uGrassVertexBuffer;
    
   
    // load stl
    const plStlOptions tStlOptions = {
        .bIncludeColor   = true,
        .bIncludeNormals = true,
        .tColor = { .r = 1.0f, .a = 1.0f}
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

    ptAppData->uStlConstantBuffer = pl_create_constant_buffer(&ptAppData->tGraphics.tResourceManager, sizeof(plObjectInfo), ptAppData->tGraphics.uFramesInFlight);

    const plMat4 tStlRotation = pl_mat4_rotate_xyz(PL_PI, 0.0f, 1.0f, 0.0f);
    const plMat4 tStlTranslation = pl_mat4_translate_xyz(0.0f, 2.0f, 0.0f);
    const plMat4 tStlTransform = pl_mul_mat4(&tStlTranslation, &tStlRotation);
    for(uint32_t i = 0; i < ptAppData->tGraphics.uFramesInFlight; i++)
    {
        plObjectInfo* ptObjectInfo = pl_get_constant_buffer_data_ex(&ptAppData->tGraphics.tResourceManager, ptAppData->uStlConstantBuffer, (size_t)i, 0);
        ptObjectInfo->uVertexOffset = pl_sb_size(ptAppData->tRenderer.sbfStorageBuffer) / 4;
        ptObjectInfo->tModel = tStlTransform;
    }
    
    // create bind group layouts
    plBindGroupLayout tStlGroupLayout = {
        .uBufferCount = 1,
        .aBuffers      = {
             { .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}  
        }
    };

    pl_create_bind_group(&ptAppData->tGraphics, &tStlGroupLayout, &ptAppData->tStlBindGroup2, "stl bind group");
    pl_update_bind_group(&ptAppData->tGraphics, &ptAppData->tStlBindGroup2, 1, &ptAppData->uStlConstantBuffer, 0, NULL);
    ptAppData->tStlMesh.uIndexCount   = (uint32_t)tStlInfo.szIndexBufferSize;
    ptAppData->tStlMesh.uVertexCount  = (uint32_t)tStlInfo.szVertexStream0Size / 3;
    ptAppData->tStlMesh.uIndexBuffer  = pl_create_index_buffer(&ptAppData->tGraphics.tResourceManager, sizeof(uint32_t) * tStlInfo.szIndexBufferSize, auIndexBuffer);
    ptAppData->tStlMesh.uVertexBuffer = pl_create_vertex_buffer(&ptAppData->tGraphics.tResourceManager, tStlInfo.szVertexStream0Size * sizeof(float), sizeof(plVec3), afVertexBuffer0);
    ptAppData->tStlMesh.ulVertexStreamMask0 = PL_MESH_FORMAT_FLAG_HAS_POSITION;
    ptAppData->tStlMesh.ulVertexStreamMask1 = PL_MESH_FORMAT_FLAG_HAS_NORMAL | PL_MESH_FORMAT_FLAG_HAS_COLOR_0;

    const uint32_t uPrevIndex = pl_sb_add_n(ptAppData->tRenderer.sbfStorageBuffer, (uint32_t)tStlInfo.szVertexStream1Size);
    memcpy(&ptAppData->tRenderer.sbfStorageBuffer[uPrevIndex], afVertexBuffer1, tStlInfo.szVertexStream1Size * sizeof(float));
    pl_free(acFileData);
    pl_free(afVertexBuffer0);
    pl_free(afVertexBuffer1);
    pl_free(auIndexBuffer);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~materials~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // create gltf materials
    for(uint32_t i = 0; i < pl_sb_size(ptAppData->tGltf.sbtMaterials); i++)
    {
        ptAppData->tGltf.sbtMaterials[i].uShader = uShader;
        pl_sb_push(ptAppData->sbuMaterialIndices, pl_create_material(ptAppData->tRenderer.ptAssetRegistry, &ptAppData->tGltf.sbtMaterials[i]));
    }

    // update gltf material indices
    const uint32_t uGltfMaterialOffset = ptAppData->sbuMaterialIndices[0];
    for(uint32_t i = 0; i < pl_sb_size(ptAppData->tGltf.sbuMaterialIndices); i++)
        ptAppData->tGltf.sbuMaterialIndices[i] += uGltfMaterialOffset;

    plMaterial tSolidMaterial = {0};
    pl_initialize_material(&tSolidMaterial, "solid");
    tSolidMaterial.bDoubleSided = true;
    tSolidMaterial.uShader = uShader;
    ptAppData->uSolidMaterial = pl_create_material(&ptAppData->tAssetRegistry, &tSolidMaterial);

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
    pl_cleanup_draw_context(&ptAppData->tCtx);
    pl_ui_cleanup_context(&ptAppData->tUiContext);
    pl_cleanup_renderer(&ptAppData->tRenderer);
    pl_cleanup_asset_registry(&ptAppData->tAssetRegistry);
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
    pl_camera_set_aspect(&ptAppData->tCamera, ptIOCtx->afMainViewportSize[0] / ptIOCtx->afMainViewportSize[1]);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~frame setup~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    pl_begin_profile_frame(ptAppData->tCtx.frameCount);
    plIOContext* ptIOCtx = pl_get_io_context();
    pl_handle_extension_reloads();
    pl_new_io_frame();
    pl_new_draw_frame(&ptAppData->tCtx);
    pl_ui_new_frame(&ptAppData->tUiContext);
    pl_process_cleanup_queue(&ptAppData->tGraphics.tResourceManager, 1);

    plFrameContext* ptCurrentFrame = pl_get_frame_resources(&ptAppData->tGraphics);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~begin frame~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    if(pl_begin_frame(&ptAppData->tGraphics))
    {
        pl_begin_recording(&ptAppData->tGraphics);

        pl_begin_main_pass(&ptAppData->tGraphics);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~input handling~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        static const float fCameraTravelSpeed = 8.0f;
        if(pl_is_key_pressed(PL_KEY_W, true)) pl_camera_translate(&ptAppData->tCamera,  0.0f,  0.0f,  fCameraTravelSpeed * ptIOCtx->fDeltaTime);
        if(pl_is_key_pressed(PL_KEY_S, true)) pl_camera_translate(&ptAppData->tCamera,  0.0f,  0.0f, -fCameraTravelSpeed* ptIOCtx->fDeltaTime);
        if(pl_is_key_pressed(PL_KEY_A, true)) pl_camera_translate(&ptAppData->tCamera, -fCameraTravelSpeed * ptIOCtx->fDeltaTime,  0.0f,  0.0f);
        if(pl_is_key_pressed(PL_KEY_D, true)) pl_camera_translate(&ptAppData->tCamera,  fCameraTravelSpeed * ptIOCtx->fDeltaTime,  0.0f,  0.0f);
        if(pl_is_key_pressed(PL_KEY_F, true)) pl_camera_translate(&ptAppData->tCamera,  0.0f, -fCameraTravelSpeed * ptIOCtx->fDeltaTime,  0.0f);
        if(pl_is_key_pressed(PL_KEY_R, true)) pl_camera_translate(&ptAppData->tCamera,  0.0f,  fCameraTravelSpeed * ptIOCtx->fDeltaTime,  0.0f);

        if(pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, -0.0f))
        {
            const plVec2 tMouseDelta = pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, -0.0f);
            pl_camera_rotate(&ptAppData->tCamera,  -tMouseDelta.y * 0.1f * ptIOCtx->fDeltaTime,  -tMouseDelta.x * 0.1f * ptIOCtx->fDeltaTime);
            pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
        }

        pl_camera_update(&ptAppData->tCamera);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~update constant buffers~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // update global constant buffer
        plGlobalInfo* ptGlobalInfo = pl_get_constant_buffer_data(&ptAppData->tGraphics.tResourceManager, ptAppData->tRenderer.uGlobalConstantBuffer, 0);
        ptGlobalInfo->tAmbientColor   = (plVec4){0.0f, 0.0f, 0.0f, 1.0f};
        ptGlobalInfo->tCameraPos      = (plVec4){.xyz = ptAppData->tCamera.tPos, .w = 0.0f};
        ptGlobalInfo->tCameraView     = ptAppData->tCamera.tViewMat;
        ptGlobalInfo->tCameraViewProj = pl_mul_mat4(&ptAppData->tCamera.tProjMat, &ptAppData->tCamera.tViewMat);
        ptGlobalInfo->fTime           = (float)pl_get_io_context()->dTime;

        plGlobalInfo* ptSkyboxInfo = pl_get_constant_buffer_data(&ptAppData->tGraphics.tResourceManager, ptAppData->uSkyboxConstantBuffer, 0);
        ptSkyboxInfo->tAmbientColor = ptGlobalInfo->tAmbientColor;
        ptSkyboxInfo->tCameraPos      = (plVec4){.xyz = ptAppData->tCamera.tPos, .w = 0.0f};
        ptSkyboxInfo->tCameraView     = ptAppData->tCamera.tViewMat;

        plMat4 tReverseTransform = pl_mat4_translate_xyz(ptAppData->tCamera.tPos.x, ptAppData->tCamera.tPos.y, ptAppData->tCamera.tPos.z);
        ptSkyboxInfo->tCameraViewProj = pl_mul_mat4(&ptAppData->tCamera.tViewMat, &tReverseTransform);
        ptSkyboxInfo->tCameraViewProj = pl_mul_mat4(&ptAppData->tCamera.tProjMat, &ptSkyboxInfo->tCameraViewProj);

        plMat4 tGltfRotation = pl_identity_mat4();
        for(uint32_t i = 0; i < pl_sb_size(ptAppData->tGltf.sbtScenes[ptAppData->tGltf.uScene].sbuRootNodes); i++)
        {
            const uint32_t uRootNode = ptAppData->tGltf.sbtScenes[ptAppData->tGltf.uScene].sbuRootNodes[i];
            pl_update_nodes(&ptAppData->tGraphics, ptAppData->tGltf.sbuVertexOffsets, ptAppData->tGltf.sbtNodes, uRootNode, ptAppData->uConstantBuffer, &tGltfRotation);
        }

        pl_renderer_begin_frame(&ptAppData->tRenderer);

        pl_begin_profile_sample("Submit Meshes");
        // pl_renderer_submit_meshes(&ptAppData->tRenderer, ptAppData->sbtGrassMeshes, ptAppData->sbuGrassMaterialIndices, ptAppData->sbtGrassBindGroup2, ptAppData->uGrassConstantBuffer, ptAppData->uGrassMeshCount);
        // pl_renderer_submit_meshes(&ptAppData->tRenderer, &ptAppData->tStlMesh, &ptAppData->uSolidMaterial, &ptAppData->tStlBindGroup2, ptAppData->uStlConstantBuffer, 1);
        pl_renderer_submit_meshes(&ptAppData->tRenderer, ptAppData->tGltf.sbtMeshes, ptAppData->tGltf.sbuMaterialIndices, ptAppData->sbtBindGroups, ptAppData->uConstantBuffer, pl_sb_size(ptAppData->tGltf.sbtMeshes));

        pl_sb_push(ptAppData->tRenderer.sbtDrawAreas, ((plDrawArea){
            .ptBindGroup0 = &ptAppData->tRenderer.tGlobalBindGroup,
            .uDrawOffset  = 0,
            .uDrawCount   = pl_sb_size(ptAppData->tRenderer.sbtDraws),
            .uDynamicBufferOffset0 = pl_get_constant_buffer_offset(&ptAppData->tGraphics.tResourceManager, ptAppData->tRenderer.uGlobalConstantBuffer, 0)
        }));

        const uint32_t uDrawOffset = pl_sb_size(ptAppData->tRenderer.sbtDraws);

        pl_sb_push(ptAppData->tRenderer.sbtDraws, ((plDraw){
            .uShader               = ptAppData->uSkyboxShader,
            .uShaderVariant        = 0,
            .ptMesh                = &ptAppData->tSkyboxMesh,
            .ptBindGroup1          = NULL,
            .ptBindGroup2          = NULL,
            .uDynamicBufferOffset1 = 0,
            .uDynamicBufferOffset2 = 0
            }));

        pl_sb_push(ptAppData->tRenderer.sbtDrawAreas, ((plDrawArea){
            .ptBindGroup0          = &ptAppData->tSkyboxBindGroup2,
            .uDrawOffset           = uDrawOffset,
            .uDrawCount            = 1,
            .uDynamicBufferOffset0 = pl_get_constant_buffer_offset(&ptAppData->tGraphics.tResourceManager, ptAppData->uSkyboxConstantBuffer, 0)
        }));
        pl_end_profile_sample();

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~submit draws~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        pl_draw_areas(&ptAppData->tGraphics, pl_sb_size(ptAppData->tRenderer.sbtDrawAreas), ptAppData->tRenderer.sbtDrawAreas, ptAppData->tRenderer.sbtDraws);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~drawing api~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        ptAppData->ptDrawExtApi->pl_add_text(ptAppData->fgDrawLayer, &ptAppData->fontAtlas.sbFonts[0], 13.0f, (plVec2){100.0f, 100.0f}, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, "Drawn from extension!");

        // pl_add_image(ptAppData->fgDrawLayer, ptAppData->tGrassTextureId, (plVec2){0.0f, 0.0f}, (plVec2){600.0f, 600.0f});

        // draw profiling info

        pl_begin_profile_sample("Draw Profiling Info");
        char cPProfileValue[64] = {0};
        for(uint32_t i = 0u; i < pl_sb_size(ptAppData->tProfileCtx.ptLastFrame->sbtSamples); i++)
        {
            plProfileSample* tPSample = &ptAppData->tProfileCtx.ptLastFrame->sbtSamples[i];
            pl_add_text(ptAppData->fgDrawLayer, &ptAppData->fontAtlas.sbFonts[0], 13.0f, (plVec2){10.0f + (float)tPSample->uDepth * 15.0f, 50.0f + (float)i * 15.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, tPSample->pcName, 0.0f);
            plVec2 sampleTextSize = pl_calculate_text_size(&ptAppData->fontAtlas.sbFonts[0], 13.0f, tPSample->pcName, 0.0f);
            pl_sprintf(cPProfileValue, ": %0.5f", tPSample->dDuration);
            pl_add_text(ptAppData->fgDrawLayer, &ptAppData->fontAtlas.sbFonts[0], 13.0f, (plVec2){sampleTextSize.x + 15.0f + (float)tPSample->uDepth * 15.0f, 50.0f + (float)i * 15.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, cPProfileValue, 0.0f);
        }
        pl_end_profile_sample();

        // ui

        static bool bOpen = true;
        static bool bDemoOpen = false;

        pl_ui_set_next_window_pos((plVec2){0, 0}, PL_UI_COND_ONCE);

        if(pl_ui_begin_window("Pilot Light", NULL, false))
        {
            pl_ui_text("%.6f ms", ptIOCtx->fDeltaTime);
            pl_ui_checkbox("Camera Info", &bOpen);
            pl_ui_checkbox("UI Demo", &bDemoOpen);

            if(pl_ui_button("Move"))
            {
                pl_ui_set_next_window_collapse(true, PL_UI_COND_ONCE);
            }

            static bool bMaterialsOpen = false;
            static bool bOpenValues[512] = {0};
            if(pl_ui_collapsing_header("Materials", &bMaterialsOpen))
            {
                for(uint32_t i = 0; i < pl_sb_size(ptAppData->tAssetRegistry.sbtMaterials); i++)
                {
                    plMaterial* ptMaterial = &ptAppData->tAssetRegistry.sbtMaterials[i];

                    static bool bOpen0 = false;
                    if(pl_ui_tree_node(ptMaterial->acName, &bOpenValues[i]))
                    {
                        pl_ui_text("Double Sided: %s", ptMaterial->bDoubleSided ? "true" : "false");
                        pl_ui_text("Alpha cutoff: %0.1f", ptMaterial->fAlphaCutoff);
                        ptMaterial->uShader      == 0 ? pl_ui_text("Shader: None")       : pl_ui_text("Shader: %u", ptMaterial->uShader);
                        ptMaterial->uAlbedoMap   == 0 ? pl_ui_text("Albedo Map: None")   : pl_ui_text("Albedo Map: %u", ptMaterial->uAlbedoMap);
                        ptMaterial->uNormalMap   == 0 ? pl_ui_text("Normal Map: None")   : pl_ui_text("Normal Map: %u", ptMaterial->uNormalMap);
                        pl_ui_tree_pop();
                    }
                }
            }

        }
        pl_ui_end_window();

        if(bOpen)
        {
            if(pl_ui_begin_window("Camera Info", &bOpen, true))
            {
                pl_ui_text("Pos: %.3f, %.3f, %.3f", ptAppData->tCamera.tPos.x, ptAppData->tCamera.tPos.y, ptAppData->tCamera.tPos.z);
                pl_ui_text("Pitch: %.3f, Yaw: %.3f, Roll:%.3f", ptAppData->tCamera.fPitch, ptAppData->tCamera.fYaw, ptAppData->tCamera.fRoll);
                pl_ui_text("Up: %.3f, %.3f, %.3f", ptAppData->tCamera._tUpVec.x, ptAppData->tCamera._tUpVec.y, ptAppData->tCamera._tUpVec.z);
                pl_ui_text("Forward: %.3f, %.3f, %.3f", ptAppData->tCamera._tForwardVec.x, ptAppData->tCamera._tForwardVec.y, ptAppData->tCamera._tForwardVec.z);
                pl_ui_text("Right: %.3f, %.3f, %.3f", ptAppData->tCamera._tRightVec.x, ptAppData->tCamera._tRightVec.y, ptAppData->tCamera._tRightVec.z);  
            }
            pl_ui_end_window();
        }

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~UI DEMO~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        if(bDemoOpen)
        {
            if(pl_ui_begin_window("UI Demo", &bDemoOpen, false))
            {
                pl_ui_progress_bar(0.75f, (plVec2){-1.0f, 0.0f}, NULL);
                if(pl_ui_button("Hover me!"))
                    bOpen = true;

                if(pl_ui_was_last_item_hovered())
                {
                    pl_ui_begin_tooltip();
                    pl_ui_text("Clicking this button will reshow the camera window!", ptIOCtx->fDeltaTime);
                    pl_ui_end_tooltip();
                }
                static int iValue = 0;
                pl_ui_text("Radio Buttons");
                pl_ui_radio_button("Option 1", &iValue, 0);
                pl_ui_same_line(0.0f, -1.0f);
                pl_ui_radio_button("Option 2", &iValue, 1);
                pl_ui_same_line(0.0f, -1.0f);
                pl_ui_radio_button("Option 3", &iValue, 2);
                pl_ui_text("Selectables");
                static bool bSelectable0 = false;
                static bool bSelectable1 = false;
                static bool bSelectable2 = false;
                pl_ui_selectable("Selectable 1", &bSelectable0);
                pl_ui_selectable("Selectable 2", &bSelectable1);
                pl_ui_selectable("Selectable 3", &bSelectable2);
                static bool bOpen0 = false;
                if(pl_ui_tree_node("Root Node", &bOpen0))
                {
                    static bool bOpen1 = false;
                    if(pl_ui_tree_node("Child 1", &bOpen1))
                    {
                        if(pl_ui_button("Press me"))
                            bOpen = true;
                        pl_ui_same_line(-1.0f, -1.0f);
                        pl_ui_button("Press me#2");
                        pl_ui_tree_pop();
                    }
                    static bool bOpen2 = false;
                    if(pl_ui_tree_node("Child 2", &bOpen2))
                    {
                        pl_ui_button("Press me");
                        pl_ui_tree_pop();
                    }
                    pl_ui_tree_pop();
                }
                static bool bOpen3 = false;
                if(pl_ui_collapsing_header("Collapsing Header", &bOpen3))
                {
                    pl_ui_checkbox("Camera window2", &bOpen);
                }

                if(pl_ui_begin_tab_bar("Tabs1"))
                {
                    if(pl_ui_begin_tab("Tab 0"))
                    {
                        pl_ui_selectable("Selectable 1", &bSelectable0);
                        pl_ui_selectable("Selectable 2##2", &bSelectable1);
                        pl_ui_selectable("Selectable 3", &bSelectable2);
                        pl_ui_vertical_spacing();
                        pl_ui_image_ex(ptAppData->tGrassTextureId, (plVec2){200.0f, 200.0f}, (plVec2){0}, (plVec2){1.0f, 1.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f});
                    }
                    pl_ui_end_tab();

                    if(pl_ui_begin_tab("Tab 1"))
                    {
                        pl_ui_radio_button("Option 1", &iValue, 0);
                        pl_ui_radio_button("Option 2", &iValue, 1);
                        pl_ui_radio_button("Option 3", &iValue, 2);
                    }
                    pl_ui_end_tab();

                    if(pl_ui_begin_tab("Tab 2"))
                    {
                        pl_ui_radio_button("Option 1", &iValue, 0);
                        pl_ui_selectable("Selectable 2", &bSelectable1);
                    }
                    pl_ui_end_tab();
                }
                pl_ui_end_tab_bar();
            }
            pl_ui_end_window();
        }

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~submit draws~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // submit draw layers
        pl_begin_profile_sample("Submit draw layers");
        pl_submit_draw_layer(ptAppData->bgDrawLayer);
        pl_submit_draw_layer(ptAppData->fgDrawLayer);
        pl_end_profile_sample();

        pl_ui_render();

        // submit draw lists
        pl_submit_drawlist_vulkan(&ptAppData->drawlist, (float)ptIOCtx->afMainViewportSize[0], (float)ptIOCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptAppData->tGraphics.szCurrentFrameIndex);
        pl_submit_drawlist_vulkan(ptAppData->tUiContext.ptDrawlist, (float)ptIOCtx->afMainViewportSize[0], (float)ptIOCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptAppData->tGraphics.szCurrentFrameIndex);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~end frame~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        pl_end_main_pass(&ptAppData->tGraphics);
        pl_end_recording(&ptAppData->tGraphics);
        pl_end_frame(&ptAppData->tGraphics);
    }
    pl_end_io_frame();
    pl_ui_end_frame();
    pl_end_profile_frame();
}

//-----------------------------------------------------------------------------
// [SECTION] helper implementations
//-----------------------------------------------------------------------------

uint32_t
pl_load_shader(plAppData* ptAppData)
{
    plShaderDesc tShaderDesc = {
        ._tRenderPass                        = ptAppData->tGraphics.tRenderPass,
        .pcPixelShader                       = "phong.frag.spv",
        .pcVertexShader                      = "primitive.vert.spv",
        .tGraphicsState.ulVertexStreamMask0  = PL_MESH_FORMAT_FLAG_HAS_POSITION,
        .tGraphicsState.ulVertexStreamMask1  = PL_MESH_FORMAT_FLAG_HAS_NORMAL | PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0,
        .tGraphicsState.ulDepthMode          = PL_DEPTH_MODE_LESS,
        .tGraphicsState.ulBlendMode          = PL_BLEND_MODE_ALPHA,
        .tGraphicsState.ulCullMode           = VK_CULL_MODE_NONE,
        .tGraphicsState.ulDepthWriteEnabled  = VK_TRUE,
        .tGraphicsState.ulShaderTextureFlags = PL_SHADER_TEXTURE_FLAG_BINDING_0,
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

    pl_create_bind_group(&ptAppData->tGraphics, &tShaderDesc.atBindGroupLayouts[0], NULL, "nullg material");
    pl_create_bind_group(&ptAppData->tGraphics, &tShaderDesc.atBindGroupLayouts[1], NULL, "main shader");
    pl_create_bind_group(&ptAppData->tGraphics, &tShaderDesc.atBindGroupLayouts[2], NULL, "null material");

    return pl_create_shader(&ptAppData->tGraphics.tResourceManager, &tShaderDesc);
}

uint32_t
pl_load_skybox_shader(plAppData* ptAppData)
{
    plShaderDesc tShaderDesc = {
        ._tRenderPass                        = ptAppData->tGraphics.tRenderPass,
        .pcPixelShader                       = "skybox.frag.spv",
        .pcVertexShader                      = "skybox.vert.spv",
        .tGraphicsState.ulVertexStreamMask0  = PL_MESH_FORMAT_FLAG_HAS_POSITION,
        .tGraphicsState.ulVertexStreamMask1  = PL_MESH_FORMAT_FLAG_NONE,
        .tGraphicsState.ulDepthMode          = PL_DEPTH_MODE_LESS_OR_EQUAL,
        .tGraphicsState.ulBlendMode          = PL_BLEND_MODE_NONE,
        .tGraphicsState.ulCullMode           = VK_CULL_MODE_NONE,
        .tGraphicsState.ulDepthWriteEnabled  = VK_FALSE,
        .tGraphicsState.ulShaderTextureFlags = PL_SHADER_TEXTURE_FLAG_BINDING_NONE,
        .uBindGroupLayoutCount               = 1,
        .atBindGroupLayouts                  = {
            {
                .uBufferCount = 1,
                .aBuffers = {
                    { .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}
                },
                .uTextureCount = 1,
                .aTextures     = {
                    { .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uSlot = 1, .tStageFlags = VK_SHADER_STAGE_FRAGMENT_BIT }
                }
            }
        },   
    };

    pl_create_bind_group(&ptAppData->tGraphics, &tShaderDesc.atBindGroupLayouts[0], NULL, "skybox material");

    int texWidth, texHeight, texNumChannels;
    int texForceNumChannels = 4;
    unsigned char* rawBytes0 = stbi_load("../data/pilotlight-assets-master/SkyBox/right.png", &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
    unsigned char* rawBytes1 = stbi_load("../data/pilotlight-assets-master/SkyBox/left.png", &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
    unsigned char* rawBytes2 = stbi_load("../data/pilotlight-assets-master/SkyBox/top.png", &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
    unsigned char* rawBytes3 = stbi_load("../data/pilotlight-assets-master/SkyBox/bottom.png", &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
    unsigned char* rawBytes4 = stbi_load("../data/pilotlight-assets-master/SkyBox/front.png", &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
    unsigned char* rawBytes5 = stbi_load("../data/pilotlight-assets-master/SkyBox/back.png", &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
    PL_ASSERT(rawBytes0);
    PL_ASSERT(rawBytes1);
    PL_ASSERT(rawBytes2);
    PL_ASSERT(rawBytes3);
    PL_ASSERT(rawBytes4);
    PL_ASSERT(rawBytes5);

    unsigned char* rawBytes = pl_alloc(texWidth * texHeight * texNumChannels * 6);
    memcpy(&rawBytes[texWidth * texHeight * texNumChannels * 0], rawBytes0, texWidth * texHeight * texNumChannels);
    memcpy(&rawBytes[texWidth * texHeight * texNumChannels * 1], rawBytes1, texWidth * texHeight * texNumChannels);
    memcpy(&rawBytes[texWidth * texHeight * texNumChannels * 2], rawBytes2, texWidth * texHeight * texNumChannels);
    memcpy(&rawBytes[texWidth * texHeight * texNumChannels * 3], rawBytes3, texWidth * texHeight * texNumChannels);
    memcpy(&rawBytes[texWidth * texHeight * texNumChannels * 4], rawBytes4, texWidth * texHeight * texNumChannels);
    memcpy(&rawBytes[texWidth * texHeight * texNumChannels * 5], rawBytes5, texWidth * texHeight * texNumChannels);

    stbi_image_free(rawBytes0);
    stbi_image_free(rawBytes1);
    stbi_image_free(rawBytes2);
    stbi_image_free(rawBytes3);
    stbi_image_free(rawBytes4);
    stbi_image_free(rawBytes5);

    const plTextureDesc tTextureDesc = {
        .tDimensions = {.x = (float)texWidth, .y = (float)texHeight, .z = 1.0f},
        .tFormat     = VK_FORMAT_R8G8B8A8_UNORM,
        .tUsage      = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .uLayers     = 6,
        .uMips       = 1, // means all mips
        .tType       = VK_IMAGE_TYPE_2D,
        .tViewType   = VK_IMAGE_VIEW_TYPE_CUBE
    };
    ptAppData->uSkyboxTexture  = pl_create_texture(&ptAppData->tGraphics.tResourceManager, tTextureDesc, sizeof(unsigned char) * texHeight * texHeight * texNumChannels * 6, rawBytes);
    pl_free(rawBytes);
    return pl_create_shader(&ptAppData->tGraphics.tResourceManager, &tShaderDesc);   
}

void
pl_setup_grass(plAppData* ptAppData, uint32_t uMaxGrassMeshes)
{
    // grass shader
    plShaderDesc tShaderDesc = {
        ._tRenderPass                        = ptAppData->tGraphics.tRenderPass,
        .pcPixelShader                       = "grass.frag.spv",
        .pcVertexShader                      = "grass.vert.spv",
        .tGraphicsState.ulVertexStreamMask0  = PL_MESH_FORMAT_FLAG_HAS_POSITION,
        .tGraphicsState.ulVertexStreamMask1  = PL_MESH_FORMAT_FLAG_HAS_NORMAL | PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0,
        .tGraphicsState.ulDepthMode          = PL_DEPTH_MODE_LESS,
        .tGraphicsState.ulBlendMode          = PL_BLEND_MODE_ALPHA,
        .tGraphicsState.ulCullMode           = VK_CULL_MODE_NONE,
        .tGraphicsState.ulDepthWriteEnabled  = VK_TRUE,
        .tGraphicsState.ulShaderTextureFlags = PL_SHADER_TEXTURE_FLAG_BINDING_0,
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

    pl_create_bind_group(&ptAppData->tGraphics, &tShaderDesc.atBindGroupLayouts[0], NULL, "nullg material");
    pl_create_bind_group(&ptAppData->tGraphics, &tShaderDesc.atBindGroupLayouts[1], NULL, "main shader");
    pl_create_bind_group(&ptAppData->tGraphics, &tShaderDesc.atBindGroupLayouts[2], NULL, "null material");

    ptAppData->uGrassShader = pl_create_shader(&ptAppData->tGraphics.tResourceManager, &tShaderDesc);   

    ptAppData->uGrassConstantBuffer = pl_create_constant_buffer(&ptAppData->tGraphics.tResourceManager, sizeof(plObjectInfo), ptAppData->tGraphics.uFramesInFlight * uMaxGrassMeshes);

    // create bind group layouts
    plBindGroupLayout tGroupLayout0 = {
        .uBufferCount = 1,
        .aBuffers      = {
            { .tType       = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}
        }
    };

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
    ptAppData->uGrassTexture  = pl_create_texture(&ptAppData->tGraphics.tResourceManager, tTextureDesc, sizeof(unsigned char) * texHeight * texHeight * 4, rawBytes);
    ptAppData->tGrassTextureId = pl_add_texture(&ptAppData->tCtx, ptAppData->tGraphics.tResourceManager.sbtTextures[ptAppData->uGrassTexture].tImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    pl_sb_resize(ptAppData->sbtGrassBindGroup2, uMaxGrassMeshes);

    // create material for grass
    plMaterial tGrassMaterial = {0};
    pl_initialize_material(&tGrassMaterial, "grass");
    tGrassMaterial.bDoubleSided = true;
    tGrassMaterial.uAlbedoMap = ptAppData->uGrassTexture;
    tGrassMaterial.ulShaderTextureFlags = PL_SHADER_TEXTURE_FLAG_BINDING_0;
    tGrassMaterial.uShader = ptAppData->uGrassShader;
    ptAppData->uGrassMaterial = pl_create_material(&ptAppData->tAssetRegistry, &tGrassMaterial);

    for(uint32_t i = 0; i < uMaxGrassMeshes; i++)
    {
        pl_create_bind_group(&ptAppData->tGraphics, &tGroupLayout0, &ptAppData->sbtGrassBindGroup2[i], "grass bind group");
        pl_update_bind_group(&ptAppData->tGraphics, &ptAppData->sbtGrassBindGroup2[i], 1, &ptAppData->uGrassConstantBuffer, 0, NULL);
        pl_sb_push(ptAppData->sbuGrassMaterialIndices, ptAppData->uGrassMaterial);
    }

    const uint32_t auIndexBuffer[] = {
        0, 3, 2, 0, 2, 1,
        4, 7, 6, 4, 6, 5,
        8, 11, 10, 8, 10, 9
    };

    ptAppData->uGrassIndexBuffer = pl_create_index_buffer(&ptAppData->tGraphics.tResourceManager, sizeof(uint32_t) * 18, auIndexBuffer);
}

void
pl_load_single_grass(plAppData* ptAppData, float fX, float fY, float fZ, float fHeight)
{

    // first quad
    pl_sb_push(ptAppData->sbtGrassVertexData, ((plVec3){ -0.5f + fX, fHeight + fY, 0.0f + fZ}));
    pl_sb_push(ptAppData->sbtGrassVertexData, ((plVec3){  0.5f + fX, fHeight + fY, 0.0f + fZ}));
    pl_sb_push(ptAppData->sbtGrassVertexData, ((plVec3){  0.5f + fX,    0.0f + fY, 0.0f + fZ}));
    pl_sb_push(ptAppData->sbtGrassVertexData, ((plVec3){ -0.5f + fX,    0.0f + fY,  0.0f + fZ}));

    // second quad
    pl_sb_push(ptAppData->sbtGrassVertexData, ((plVec3){ -0.35f + fX,    1.0f + fY, -0.35f + fZ}));
    pl_sb_push(ptAppData->sbtGrassVertexData, ((plVec3){  0.35f + fX, fHeight + fY,  0.35f + fZ}));
    pl_sb_push(ptAppData->sbtGrassVertexData, ((plVec3){  0.35f + fX,    0.0f + fY,  0.35f + fZ}));
    pl_sb_push(ptAppData->sbtGrassVertexData, ((plVec3){ -0.35f + fX,    0.0f + fY, -0.35f + fZ}));

    // third quad
    pl_sb_push(ptAppData->sbtGrassVertexData, ((plVec3){ -0.35f + fX, fHeight + fY,  0.35f + fZ}));
    pl_sb_push(ptAppData->sbtGrassVertexData, ((plVec3){  0.35f + fX, fHeight + fY, -0.35f + fZ}));
    pl_sb_push(ptAppData->sbtGrassVertexData, ((plVec3){  0.35f + fX,    0.0f + fY, -0.35f + fZ}));
    pl_sb_push(ptAppData->sbtGrassVertexData, ((plVec3){ -0.35f + fX,    0.0f + fY,  0.35f + fZ}));

    const plVec4 acStorageBuffer[] = 
    {
        {.y = 1.0f},
        {.x = 0.0f, .y = 0.0f},
        {.y = 1.0f},
        {.x = 1.0f, .y = 0.0f},
        {.y = 1.0f},
        {.x = 1.0f, .y = 1.0f},
        {.y = 1.0f},
        {.x = 0.0f, .y = 1.0f},
        {.y = 1.0f},
        {.x = 0.0f, .y = 0.0f},
        {.y = 1.0f},
        {.x = 1.0f, .y = 0.0f},
        {.y = 1.0f},
        {.x = 1.0f, .y = 1.0f},
        {.y = 1.0f},
        {.x = 0.0f, .y = 1.0f},
        {.y = 1.0f},
        {.x = 0.0f, .y = 0.0f},
        {.y = 1.0f},
        {.x = 1.0f, .y = 0.0f},
        {.y = 1.0f},
        {.x = 1.0f, .y = 1.0f},
        {.y = 1.0f},
        {.x = 0.0f, .y = 1.0f},
    };

    const plMesh tMesh = {
        .uIndexCount         = 18,
        .uVertexCount        = 36,
        // .uVertexBuffer    // filled out in pl_finalize_grass
        .uIndexBuffer        = ptAppData->uGrassIndexBuffer,
        .ulVertexStreamMask0 = PL_MESH_FORMAT_FLAG_HAS_POSITION,
        .ulVertexStreamMask1 = PL_MESH_FORMAT_FLAG_HAS_NORMAL | PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0,
        .uVertexOffset       = ptAppData->uGrassMeshCount * 12
    };

    const uint32_t uStorageOffset = pl_sb_size(ptAppData->tRenderer.sbfStorageBuffer) / 4;
    pl_sb_reserve(ptAppData->tRenderer.sbfStorageBuffer, pl_sb_size(ptAppData->tRenderer.sbfStorageBuffer) + 24);
    for(uint32_t i = 0; i < 24; i++)
    {
        pl_sb_push(ptAppData->tRenderer.sbfStorageBuffer, acStorageBuffer[i].x);
        pl_sb_push(ptAppData->tRenderer.sbfStorageBuffer, acStorageBuffer[i].y);
        pl_sb_push(ptAppData->tRenderer.sbfStorageBuffer, acStorageBuffer[i].z);
        pl_sb_push(ptAppData->tRenderer.sbfStorageBuffer, acStorageBuffer[i].w);
    }

    const plBuffer* ptObjectConstBuffer = &ptAppData->tGraphics.tResourceManager.sbtBuffers[ptAppData->uGrassConstantBuffer];
    for(uint32_t j = 0; j < ptAppData->tGraphics.uFramesInFlight; j++)
    {
        plObjectInfo* ptObjectInfo = pl_get_constant_buffer_data_ex(&ptAppData->tGraphics.tResourceManager, ptAppData->uGrassConstantBuffer, (size_t)j, ptAppData->uGrassMeshCount);
        ptObjectInfo->tModel = pl_identity_mat4();
        ptObjectInfo->uVertexOffset = uStorageOffset;
    }

    ptAppData->uGrassMeshCount++;
    pl_sb_push(ptAppData->sbtGrassMeshes, tMesh);

    PL_ASSERT(ptAppData->uGrassMeshCount <= pl_sb_size(ptAppData->sbuGrassMaterialIndices));
}