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
#include "pl_vulkan.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"
#include "pl_camera.h"
#include "pl_registry.h"
#include "pl_ext.h"
#include "pl_ui.h"
#include "pl_stl.h"
#include "pl_renderer.h"
#include "pl_prototype.h"

// extensions
#include "pl_draw_extension.h"
#include "pl_gltf_extension.h"
#include "stb_image.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct 
{
    uint32_t    uTexture;
    plBindGroup tBindGroup0;
    plMesh      tMesh;
} plSkybox;

typedef struct 
{
    plBindGroup tBindGroup2;
    plMesh      tMesh;
    uint32_t    uVertexOffset;
} plStlModel;

typedef struct 
{
    plVec3*         sbtVertexData;
    uint32_t*       sbuMaterialIndices;
    uint32_t        uIndexBuffer;
    uint32_t        uTexture;
    uint32_t        uMeshCount;
    plBindGroup*    sbtBindGroup2;
    plMesh*         sbtMeshes;
    uint32_t*       sbuVertexOffsets;
} plGrass;

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
    plCamera            tCamera;
    plUiContext         tUiContext;
    bool                bShowUiDemo;
    bool                bShowUiDebug;
    bool                bShowUiStyle;

    // extension apis
    plDrawExtension* ptDrawExtApi;

    // renderer
    plRenderer      tRenderer;
    plAssetRegistry tAssetRegistry;
    uint32_t        uDynamicBuffer0;
    uint32_t        uDynamicBuffer1;
    uint32_t        uDynamicBuffer2;

    // misc
    plSkybox        tSkybox;
    plStlModel      tStlModel;
    plGrass         tGrass;

    // shaders
    uint32_t uMainShader;
    uint32_t uSkyboxShader;
    uint32_t uGrassShader;
    uint32_t uOutlineShader;

    // materials
    uint32_t uSolidMaterial;
    uint32_t uOutlineMaterial;
    uint32_t uGrassMaterial;

    // gltf
    plGltf          tGltf;
    uint32_t*       sbuMaterialIndices;
    uint32_t*       sbuMaterialOffsets;
    plBindGroup*    sbtBindGroups;

} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] helpers
//-----------------------------------------------------------------------------

static inline float frandom(float fMax){ return (float)rand()/(float)(RAND_MAX/fMax);}

void pl_load_shaders(plAppData* ptAppData);
void pl_load_textures(plAppData* ptAppData);
void pl_load_skybox_mesh(plAppData* ptAppData);
void pl_load_stl_mesh(plAppData* ptAppData);
void pl_load_single_grass(plAppData* ptAppData, float fX, float fY, float fZ, float fHeight);

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
        pl_ui_set_context(&ptAppData->tUiContext);

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
    plExtension* ptExtension = pl_get_extension(PL_EXT_DRAW);
    ptAppData->ptDrawExtApi = pl_get_api(ptExtension, PL_EXT_API_DRAW);

    // setup renderer
    pl_setup_graphics(&ptAppData->tGraphics);

    // ui
    pl_ui_setup_context(&ptAppData->tUiContext);

    // setup drawing api
    pl_initialize_draw_context_vulkan(ptAppData->tUiContext.ptDrawCtx, ptAppData->tGraphics.tDevice.tPhysicalDevice, ptAppData->tGraphics.tSwapchain.uImageCount, ptAppData->tGraphics.tDevice.tLogicalDevice);
    pl_register_drawlist(ptAppData->tUiContext.ptDrawCtx, &ptAppData->drawlist);
    pl_setup_drawlist_vulkan(&ptAppData->drawlist, ptAppData->tGraphics.tRenderPass, ptAppData->tGraphics.tSwapchain.tMsaaSamples);
    ptAppData->bgDrawLayer = pl_request_draw_layer(&ptAppData->drawlist, "Background Layer");
    ptAppData->fgDrawLayer = pl_request_draw_layer(&ptAppData->drawlist, "Foreground Layer");

    // create font atlas
    pl_add_default_font(&ptAppData->fontAtlas);
    pl_build_font_atlas(ptAppData->tUiContext.ptDrawCtx, &ptAppData->fontAtlas);

    pl_setup_drawlist_vulkan(ptAppData->tUiContext.ptDrawlist, ptAppData->tGraphics.tRenderPass, ptAppData->tGraphics.tSwapchain.tMsaaSamples);
    pl_setup_drawlist_vulkan(ptAppData->tUiContext.ptDebugDrawlist, ptAppData->tGraphics.tRenderPass, ptAppData->tGraphics.tSwapchain.tMsaaSamples);
    ptAppData->tUiContext.ptFont = &ptAppData->fontAtlas.sbFonts[0];

    pl_register_data("draw", ptAppData->tUiContext.ptDrawCtx);

    // renderer
    pl_setup_asset_registry(&ptAppData->tGraphics, &ptAppData->tAssetRegistry);
    pl_setup_renderer(&ptAppData->tGraphics, &ptAppData->tAssetRegistry, &ptAppData->tRenderer);

    // camera
    ptAppData->tCamera = pl_create_perspective_camera((plVec3){-6.211f, 3.647f, 0.827f}, PL_PI_3, ptIOCtx->afMainViewportSize[0] / ptIOCtx->afMainViewportSize[1], 0.01f, 400.0f);
    pl_camera_set_pitch_yaw(&ptAppData->tCamera, -0.244f, -1.488f);

    // create shaders
    pl_load_shaders(ptAppData);
    pl_load_textures(ptAppData);
    pl_load_skybox_mesh(ptAppData);

    // buffers
    ptAppData->uDynamicBuffer0 = pl_create_constant_buffer_ex(&ptAppData->tGraphics.tResourceManager, pl_minu(ptAppData->tGraphics.tDevice.tDeviceProps.limits.maxUniformBufferRange, 65536));
    ptAppData->uDynamicBuffer1 = pl_create_constant_buffer_ex(&ptAppData->tGraphics.tResourceManager, pl_minu(ptAppData->tGraphics.tDevice.tDeviceProps.limits.maxUniformBufferRange, 65536));
    ptAppData->uDynamicBuffer2 = pl_create_constant_buffer_ex(&ptAppData->tGraphics.tResourceManager, pl_minu(ptAppData->tGraphics.tDevice.tDeviceProps.limits.maxUniformBufferRange, 65536));

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~gltf~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    pl_ext_load_gltf(&ptAppData->tRenderer, "../data/glTF-Sample-Models-master/2.0/Sponza/glTF/Sponza.gltf", &ptAppData->tGltf);
    pl_sb_reserve(ptAppData->sbtBindGroups, pl_sb_size(ptAppData->tGltf.sbtMeshes));
    pl_sb_reserve(ptAppData->sbuMaterialIndices, pl_sb_size(ptAppData->tGltf.sbtMeshes));
    pl_sb_reserve(ptAppData->sbuMaterialOffsets, pl_sb_size(ptAppData->tGltf.sbtMeshes));

    plBindGroupLayout tGroupLayout0 = {
        .uBufferCount = 1,
        .aBuffers      = {
            { .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}  
        }
    };

    size_t szObjectRangeSize = sizeof(plObjectInfo);
    for(uint32_t i = 0; i < pl_sb_size(ptAppData->tGltf.sbtMeshes); i++)
    {
        pl_sb_push(ptAppData->sbtBindGroups, (plBindGroup){0});
        pl_create_bind_group(&ptAppData->tGraphics, &tGroupLayout0, &pl_sb_last(ptAppData->sbtBindGroups), "gltf");
        pl_update_bind_group(&ptAppData->tGraphics, &pl_sb_last(ptAppData->sbtBindGroups), 1, &ptAppData->uDynamicBuffer2, &szObjectRangeSize, 0, NULL);
    }
    
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~skybox~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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
    pl_create_bind_group(&ptAppData->tGraphics, &tSkyboxGroupLayout0, &ptAppData->tSkybox.tBindGroup0, "skybox bind group");
    size_t szSkyboxRangeSize = sizeof(plGlobalInfo);
    pl_update_bind_group(&ptAppData->tGraphics, &ptAppData->tSkybox.tBindGroup0, 1, &ptAppData->uDynamicBuffer0, &szSkyboxRangeSize, 1, &ptAppData->tSkybox.uTexture);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~STL Model~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


    // create bind group layouts
    plBindGroupLayout tStlGroupLayout = {
        .uBufferCount = 1,
        .aBuffers      = {
             { .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}  
        }
    };

    size_t szStlRangeSize = sizeof(plObjectInfo);
    pl_create_bind_group(&ptAppData->tGraphics, &tStlGroupLayout, &ptAppData->tStlModel.tBindGroup2, "stl bind group");
    pl_update_bind_group(&ptAppData->tGraphics, &ptAppData->tStlModel.tBindGroup2, 1, &ptAppData->uDynamicBuffer2, &szStlRangeSize, 0, NULL);

    pl_load_stl_mesh(ptAppData);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~grass~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const uint32_t auGrassIndexBuffer[] = {
        0, 3, 2, 0, 2, 1,
        4, 7, 6, 4, 6, 5,
        8, 11, 10, 8, 10, 9
    };

    ptAppData->tGrass.uIndexBuffer = pl_create_index_buffer(&ptAppData->tGraphics.tResourceManager, sizeof(uint32_t) * 18, auGrassIndexBuffer);

    const uint32_t uGrassRows = 10;
    const uint32_t uGrassColumns = 10;
    const float fGrassSpacing = 0.25f;
    const plVec3 tGrassCenterPoint = {(float)uGrassColumns * fGrassSpacing / 2.0f};

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

    // create bind group layouts
    plBindGroupLayout tGrassGroupLayout0 = {
        .uBufferCount = 1,
        .aBuffers      = {
            { .tType       = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}
        }
    };
    const uint32_t uGrassVertexBuffer = pl_create_vertex_buffer(&ptAppData->tGraphics.tResourceManager, sizeof(plVec3) * 12 * uGrassColumns * uGrassRows, sizeof(plVec3), ptAppData->tGrass.sbtVertexData);
    size_t sGrassRangeSize = sizeof(plObjectInfo);
    pl_sb_resize(ptAppData->tGrass.sbtBindGroup2, pl_sb_size(ptAppData->tGrass.sbtMeshes));
    for(uint32_t i = 0; i < pl_sb_size(ptAppData->tGrass.sbtMeshes); i++)
    {
        ptAppData->tGrass.sbtMeshes[i].uVertexBuffer = uGrassVertexBuffer;      
        pl_create_bind_group(&ptAppData->tGraphics, &tGrassGroupLayout0, &ptAppData->tGrass.sbtBindGroup2[i], "grass bind group");
        pl_update_bind_group(&ptAppData->tGraphics, &ptAppData->tGrass.sbtBindGroup2[i], 1, &ptAppData->uDynamicBuffer2, &sGrassRangeSize, 0, NULL);
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~materials~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // gltf materials
    uint32_t* sbuTextures = NULL;
    uint32_t uDataOffset = 0;
    size_t szRangeSize = sizeof(plMaterialInfo);
    {
        
        const uint32_t uMaterialCount = pl_sb_size(ptAppData->tGltf.sbtMaterials);
        const plBuffer* ptBuffer = &ptAppData->tGraphics.tResourceManager.sbtBuffers[ptAppData->uDynamicBuffer1];
        for(uint32_t i = 0; i < pl_sb_size(ptAppData->tGltf.sbtMaterials); i++)
        {
            plMaterial* ptMaterial = &ptAppData->tGltf.sbtMaterials[i];

            const uint32_t uIndex = pl_sb_size(ptAppData->tRenderer.ptAssetRegistry->sbtMaterials);
            pl_sb_push(ptAppData->sbuMaterialIndices, uIndex);

            pl_create_bind_group(&ptAppData->tGraphics, pl_get_bind_group_layout(&ptAppData->tRenderer.ptAssetRegistry->ptGraphics->tResourceManager, ptAppData->uMainShader, 1), &ptMaterial->tMaterialBindGroup, ptMaterial->acName);

            ptAppData->tGltf.sbtMaterials[i].uShader = ptAppData->uMainShader;
            pl_sb_push(sbuTextures, ptMaterial->uAlbedoMap);
            pl_sb_push(sbuTextures, ptMaterial->uNormalMap);
            pl_sb_push(sbuTextures, ptMaterial->uEmissiveMap);
            pl_update_bind_group(&ptAppData->tGraphics, &ptMaterial->tMaterialBindGroup, 1, &ptAppData->uDynamicBuffer1, &szRangeSize, pl_sb_size(sbuTextures), sbuTextures);
            pl_sb_free(sbuTextures);
            
            for(uint32_t j = 0; j < ptAppData->tGraphics.uFramesInFlight; j++)
            {
                const uint32_t uBufferFrameOffset = ((uint32_t)ptBuffer->szSize/ptAppData->tGraphics.uFramesInFlight) * j;

                plMaterialInfo* ptMaterialInfo = (plMaterialInfo*)(ptBuffer->pucMapping + uDataOffset + uBufferFrameOffset);
                ptMaterialInfo->tAlbedo = ptMaterial->tAlbedo;
            }
            pl_sb_push(ptAppData->sbuMaterialOffsets, uDataOffset);

            uDataOffset = (uint32_t)pl_align_up((size_t)uDataOffset + sizeof(plMaterialInfo), ptAppData->tGraphics.tDevice.tDeviceProps.limits.minUniformBufferOffsetAlignment);

            pl_sb_reset(sbuTextures);
            pl_sb_push(ptAppData->tRenderer.ptAssetRegistry->sbtMaterials, *ptMaterial);
        }
        

        for(uint32_t i = 0; i < pl_sb_size(ptAppData->tGltf.sbtMaterials); i++)
            ptAppData->tGltf.sbuMaterialIndices[i] += ptAppData->sbuMaterialIndices[0];
    }

    // grass material
    {
        plMaterial tGrassMaterial = {0};
        pl_initialize_material(&tGrassMaterial, "grass");
        tGrassMaterial.bDoubleSided = true;
        tGrassMaterial.uAlbedoMap = ptAppData->tGrass.uTexture;
        tGrassMaterial.ulShaderTextureFlags = PL_SHADER_TEXTURE_FLAG_BINDING_0;

        const plBuffer* ptBuffer = &ptAppData->tGraphics.tResourceManager.sbtBuffers[ptAppData->uDynamicBuffer1];

        plMaterial* ptMaterial = &tGrassMaterial;

        const uint32_t uIndex = pl_sb_size(ptAppData->tRenderer.ptAssetRegistry->sbtMaterials);
        pl_sb_push(ptAppData->sbuMaterialIndices, uIndex);
        ptAppData->uGrassMaterial = uIndex;

        pl_create_bind_group(&ptAppData->tGraphics, pl_get_bind_group_layout(&ptAppData->tRenderer.ptAssetRegistry->ptGraphics->tResourceManager, ptAppData->uGrassShader, 1), &ptMaterial->tMaterialBindGroup, ptMaterial->acName);

        ptMaterial->uShader = ptAppData->uGrassShader;
        pl_sb_push(sbuTextures, ptMaterial->uAlbedoMap);
        pl_sb_push(sbuTextures, ptMaterial->uNormalMap);
        pl_sb_push(sbuTextures, ptMaterial->uEmissiveMap);
        pl_update_bind_group(&ptAppData->tGraphics, &ptMaterial->tMaterialBindGroup, 1, &ptAppData->uDynamicBuffer1, &szRangeSize, pl_sb_size(sbuTextures), sbuTextures);
        
        for(uint32_t j = 0; j < ptAppData->tGraphics.uFramesInFlight; j++)
        {
            const uint32_t uBufferFrameOffset = ((uint32_t)ptBuffer->szSize/ptAppData->tGraphics.uFramesInFlight) * j;

            plMaterialInfo* ptMaterialInfo = (plMaterialInfo*)(ptBuffer->pucMapping + uDataOffset + uBufferFrameOffset);
            ptMaterialInfo->tAlbedo = ptMaterial->tAlbedo;
        }
        pl_sb_push(ptAppData->sbuMaterialOffsets, uDataOffset);

        uDataOffset = (uint32_t)pl_align_up((size_t)uDataOffset + sizeof(plMaterialInfo), ptAppData->tGraphics.tDevice.tDeviceProps.limits.minUniformBufferOffsetAlignment);

        pl_sb_reset(sbuTextures);
        pl_sb_push(ptAppData->tRenderer.ptAssetRegistry->sbtMaterials, *ptMaterial);
    }

    // solid materials
    {
        plMaterial tSolidMaterial = {0};
        pl_initialize_material(&tSolidMaterial, "solid");

        const plBuffer* ptBuffer = &ptAppData->tGraphics.tResourceManager.sbtBuffers[ptAppData->uDynamicBuffer1];

        plMaterial* ptMaterial = &tSolidMaterial;

        const uint32_t uIndex = pl_sb_size(ptAppData->tRenderer.ptAssetRegistry->sbtMaterials);
        pl_sb_push(ptAppData->sbuMaterialIndices, uIndex);
        ptAppData->uSolidMaterial = uIndex;

        pl_create_bind_group(&ptAppData->tGraphics, pl_get_bind_group_layout(&ptAppData->tRenderer.ptAssetRegistry->ptGraphics->tResourceManager, ptAppData->uMainShader, 1), &ptMaterial->tMaterialBindGroup, ptMaterial->acName);

        ptMaterial->uShader = ptAppData->uMainShader;
        pl_sb_push(sbuTextures, ptMaterial->uAlbedoMap);
        pl_sb_push(sbuTextures, ptMaterial->uNormalMap);
        pl_sb_push(sbuTextures, ptMaterial->uEmissiveMap);
        pl_update_bind_group(&ptAppData->tGraphics, &ptMaterial->tMaterialBindGroup, 1, &ptAppData->uDynamicBuffer1, &szRangeSize, pl_sb_size(sbuTextures), sbuTextures);
        
        for(uint32_t j = 0; j < ptAppData->tGraphics.uFramesInFlight; j++)
        {
            const uint32_t uBufferFrameOffset = ((uint32_t)ptBuffer->szSize/ptAppData->tGraphics.uFramesInFlight) * j;

            plMaterialInfo* ptMaterialInfo = (plMaterialInfo*)(ptBuffer->pucMapping + uDataOffset + uBufferFrameOffset);
            ptMaterialInfo->tAlbedo = ptMaterial->tAlbedo;
        }
        pl_sb_push(ptAppData->sbuMaterialOffsets, uDataOffset);

        uDataOffset = (uint32_t)pl_align_up((size_t)uDataOffset + sizeof(plMaterialInfo), ptAppData->tGraphics.tDevice.tDeviceProps.limits.minUniformBufferOffsetAlignment);

        pl_sb_reset(sbuTextures);
        pl_sb_push(ptAppData->tRenderer.ptAssetRegistry->sbtMaterials, *ptMaterial);
    }

    // outline material
    {
        plMaterial tOutlineMaterial = {0};
        pl_initialize_material(&tOutlineMaterial, "outline");
        tOutlineMaterial.bDoubleSided = false;
        tOutlineMaterial.tAlbedo.r = 1.0f;
        tOutlineMaterial.tAlbedo.g = 1.0f;
        tOutlineMaterial.tAlbedo.b = 1.0f;
        tOutlineMaterial.tAlbedo.a = 1.0f;

        const plBuffer* ptBuffer = &ptAppData->tGraphics.tResourceManager.sbtBuffers[ptAppData->uDynamicBuffer1];

        plMaterial* ptMaterial = &tOutlineMaterial;

        const uint32_t uIndex = pl_sb_size(ptAppData->tRenderer.ptAssetRegistry->sbtMaterials);
        pl_sb_push(ptAppData->sbuMaterialIndices, uIndex);
        ptAppData->uOutlineMaterial = uIndex;

        pl_create_bind_group(&ptAppData->tGraphics, pl_get_bind_group_layout(&ptAppData->tRenderer.ptAssetRegistry->ptGraphics->tResourceManager, ptAppData->uOutlineShader, 1), &ptMaterial->tMaterialBindGroup, ptMaterial->acName);

        ptMaterial->uShader = ptAppData->uOutlineShader;
        pl_sb_push(sbuTextures, ptMaterial->uAlbedoMap);
        pl_sb_push(sbuTextures, ptMaterial->uNormalMap);
        pl_sb_push(sbuTextures, ptMaterial->uEmissiveMap);
        pl_update_bind_group(&ptAppData->tGraphics, &ptMaterial->tMaterialBindGroup, 1, &ptAppData->uDynamicBuffer1, &szRangeSize, pl_sb_size(sbuTextures), sbuTextures);
        
        for(uint32_t j = 0; j < ptAppData->tGraphics.uFramesInFlight; j++)
        {
            const uint32_t uBufferFrameOffset = ((uint32_t)ptBuffer->szSize/ptAppData->tGraphics.uFramesInFlight) * j;

            plMaterialInfo* ptMaterialInfo = (plMaterialInfo*)(ptBuffer->pucMapping + uDataOffset + uBufferFrameOffset);
            ptMaterialInfo->tAlbedo = ptMaterial->tAlbedo;
        }
        pl_sb_push(ptAppData->sbuMaterialOffsets, uDataOffset);

        uDataOffset = (uint32_t)pl_align_up((size_t)uDataOffset + sizeof(plMaterialInfo), ptAppData->tGraphics.tDevice.tDeviceProps.limits.minUniformBufferOffsetAlignment);

        pl_sb_reset(sbuTextures);
        pl_sb_push(ptAppData->tRenderer.ptAssetRegistry->sbtMaterials, *ptMaterial);
    }

    pl_sb_free(sbuTextures);
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
    
    pl_ui_cleanup_context();
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
    
    plIOContext* ptIOCtx = pl_get_io_context();
    pl_begin_profile_frame(ptIOCtx->ulFrameCount);
    pl_handle_extension_reloads();
    pl_process_cleanup_queue(&ptAppData->tGraphics.tResourceManager, 1);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~input handling~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    static const float fCameraTravelSpeed = 8.0f;
    if(pl_is_key_pressed(PL_KEY_W, true)) pl_camera_translate(&ptAppData->tCamera,  0.0f,  0.0f,  fCameraTravelSpeed * ptIOCtx->fDeltaTime);
    if(pl_is_key_pressed(PL_KEY_S, true)) pl_camera_translate(&ptAppData->tCamera,  0.0f,  0.0f, -fCameraTravelSpeed* ptIOCtx->fDeltaTime);
    if(pl_is_key_pressed(PL_KEY_A, true)) pl_camera_translate(&ptAppData->tCamera, -fCameraTravelSpeed * ptIOCtx->fDeltaTime,  0.0f,  0.0f);
    if(pl_is_key_pressed(PL_KEY_D, true)) pl_camera_translate(&ptAppData->tCamera,  fCameraTravelSpeed * ptIOCtx->fDeltaTime,  0.0f,  0.0f);
    if(pl_is_key_pressed(PL_KEY_F, true)) pl_camera_translate(&ptAppData->tCamera,  0.0f, -fCameraTravelSpeed * ptIOCtx->fDeltaTime,  0.0f);
    if(pl_is_key_pressed(PL_KEY_R, true)) pl_camera_translate(&ptAppData->tCamera,  0.0f,  fCameraTravelSpeed * ptIOCtx->fDeltaTime,  0.0f);

    plFrameContext* ptCurrentFrame = pl_get_frame_resources(&ptAppData->tGraphics);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~begin frame~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    if(pl_begin_frame(&ptAppData->tGraphics))
    {
        pl_ui_new_frame();

        pl_begin_recording(&ptAppData->tGraphics);

        pl_begin_main_pass(&ptAppData->tGraphics);

        if(!ptAppData->tUiContext.bMouseOwned && pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 1.0f))
        {
            const plVec2 tMouseDelta = pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f);
            pl_camera_rotate(&ptAppData->tCamera,  -tMouseDelta.y * 0.1f * ptIOCtx->fDeltaTime,  -tMouseDelta.x * 0.1f * ptIOCtx->fDeltaTime);
            pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
        }

        pl_camera_update(&ptAppData->tCamera);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~update node transforms~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        plMat4 tGltfRotation = pl_identity_mat4();
        for(uint32_t i = 0; i < pl_sb_size(ptAppData->tGltf.sbtScenes[ptAppData->tGltf.uScene].sbuRootNodes); i++)
        {
            const uint32_t uRootNode = ptAppData->tGltf.sbtScenes[ptAppData->tGltf.uScene].sbuRootNodes[i];
            pl_update_nodes(&ptAppData->tGraphics, ptAppData->tGltf.sbtNodes, uRootNode, &tGltfRotation);
        }

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~renderer begin frame~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        pl_sb_reset(ptAppData->tRenderer.sbtDraws);
        pl_sb_reset(ptAppData->tRenderer.sbtDrawAreas);

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

        pl_sb_reserve(ptAppData->tRenderer.sbtDraws, pl_sb_size(ptAppData->tRenderer.sbtDraws) + pl_sb_size(ptAppData->tGltf.sbtMeshes));
        const plBuffer* ptBuffer0 = &ptAppData->tGraphics.tResourceManager.sbtBuffers[ptAppData->uDynamicBuffer0];
        const plBuffer* ptBuffer1 = &ptAppData->tGraphics.tResourceManager.sbtBuffers[ptAppData->uDynamicBuffer1];
        const plBuffer* ptBuffer2 = &ptAppData->tGraphics.tResourceManager.sbtBuffers[ptAppData->uDynamicBuffer2];
        const uint32_t uBufferFrameOffset0 = ((uint32_t)ptBuffer0->szSize/ptAppData->tGraphics.uFramesInFlight) * (uint32_t)ptAppData->tGraphics.szCurrentFrameIndex;
        const uint32_t uBufferFrameOffset1 = ((uint32_t)ptBuffer1->szSize/ptAppData->tGraphics.uFramesInFlight) * (uint32_t)ptAppData->tGraphics.szCurrentFrameIndex;
        const uint32_t uBufferFrameOffset2 = ((uint32_t)ptBuffer2->szSize/ptAppData->tGraphics.uFramesInFlight) * (uint32_t)ptAppData->tGraphics.szCurrentFrameIndex;
        uint32_t uDataOffset0 = 0;
        uint32_t uDataOffset2 = 0;
        for(uint32_t i = 0; i < pl_sb_size(ptAppData->tGltf.sbtMeshes); i++)
        {
            const uint32_t uMaterialIndex = ptAppData->tGltf.sbuMaterialIndices[i];
            plMaterial* ptMaterial = &ptAppData->tRenderer.ptAssetRegistry->sbtMaterials[uMaterialIndex];
            uint32_t uFillShaderVariant = UINT32_MAX;
            const plShader* ptShader = &ptAppData->tRenderer.ptGraphics->tResourceManager.sbtShaders[ptMaterial->uShader];   
            const plMesh* ptMesh = &ptAppData->tGltf.sbtMeshes[i];
            const uint32_t uFillVariantCount = pl_sb_size(ptShader->tDesc.sbtVariants);

            plGraphicsState tFillStateTemplate = {
                .ulVertexStreamMask   = ptMesh->ulVertexStreamMask,
                .ulDepthMode          = PL_DEPTH_MODE_LESS_OR_EQUAL,
                .ulDepthWriteEnabled  = true,
                .ulCullMode           = ptMaterial->bDoubleSided ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT,
                .ulBlendMode          = PL_BLEND_MODE_ALPHA,
                .ulShaderTextureFlags = 0,
                .ulStencilMode        = PL_STENCIL_MODE_ALWAYS,
                .ulStencilRef         = 0xff,
                .ulStencilMask        = 0xff,
                .ulStencilOpFail      = VK_STENCIL_OP_KEEP,
                .ulStencilOpDepthFail = VK_STENCIL_OP_KEEP,
                .ulStencilOpPass      = VK_STENCIL_OP_KEEP
            };

            if(ptMaterial->uAlbedoMap > 0)   tFillStateTemplate.ulShaderTextureFlags |= PL_SHADER_TEXTURE_FLAG_BINDING_0;
            if(ptMaterial->uNormalMap > 0)   tFillStateTemplate.ulShaderTextureFlags |= PL_SHADER_TEXTURE_FLAG_BINDING_1;
            if(ptMaterial->uEmissiveMap > 0) tFillStateTemplate.ulShaderTextureFlags |= PL_SHADER_TEXTURE_FLAG_BINDING_2;

            for(uint32_t j = 0; j < uFillVariantCount; j++)
            {
                plGraphicsState ptVariant = ptShader->tDesc.sbtVariants[j];
                if(ptVariant.ulValue == tFillStateTemplate.ulValue)
                {
                        uFillShaderVariant = ptShader->_sbuVariantPipelines[j];
                        break;
                }
            }

            // create variant that matches texture count, vertex stream, and culling
            if(uFillShaderVariant == UINT32_MAX)
            {
                uFillShaderVariant = pl_add_shader_variant(&ptAppData->tRenderer.ptGraphics->tResourceManager, ptMaterial->uShader, tFillStateTemplate);
            }

            pl_sb_push(ptAppData->tRenderer.sbtDraws, ((plDraw){
                .uShaderVariant        = uFillShaderVariant,
                .ptMesh                = &ptAppData->tGltf.sbtMeshes[i],
                .ptBindGroup1          = &ptMaterial->tMaterialBindGroup,
                .ptBindGroup2          = &ptAppData->sbtBindGroups[i],
                .uDynamicBufferOffset1 = uBufferFrameOffset1 + ptAppData->sbuMaterialOffsets[uMaterialIndex],
                .uDynamicBufferOffset2 = uBufferFrameOffset2 + uDataOffset2
                }));

            plObjectInfo* ptObjectInfo = (plObjectInfo*)(ptBuffer2->pucMapping + uDataOffset2 + uBufferFrameOffset2);
            ptObjectInfo->tModel = ptAppData->tGltf.sbtNodes[ptAppData->tGltf.sbuMeshNodeMap[i]].tFinalTransform;
            ptObjectInfo->uVertexOffset = ptAppData->tGltf.sbuVertexOffsets[i];

            uDataOffset2 = (uint32_t)pl_align_up((size_t)uDataOffset2 + sizeof(plObjectInfo), ptAppData->tGraphics.tDevice.tDeviceProps.limits.minUniformBufferOffsetAlignment);
        }

        pl_sb_push(ptAppData->tRenderer.sbtDrawAreas, ((plDrawArea){
            .ptBindGroup0          = &ptAppData->tRenderer.tGlobalBindGroup,
            .uDrawOffset           = 0,
            .uDrawCount            = pl_sb_size(ptAppData->tRenderer.sbtDraws),
            .uDynamicBufferOffset0 = uBufferFrameOffset0 + uDataOffset0
        }));

        uint32_t uDrawOffset = pl_sb_size(ptAppData->tRenderer.sbtDraws);

        // grass
        for(uint32_t i = 0; i < pl_sb_size(ptAppData->tGrass.sbtMeshes); i++)
        {
            const uint32_t uMaterialIndex = ptAppData->uGrassMaterial;
            plMaterial* ptMaterial = &ptAppData->tRenderer.ptAssetRegistry->sbtMaterials[uMaterialIndex];
            uint32_t uFillShaderVariant = UINT32_MAX;
            const plShader* ptShader = &ptAppData->tRenderer.ptGraphics->tResourceManager.sbtShaders[ptMaterial->uShader];   
            const plMesh* ptMesh = &ptAppData->tGrass.sbtMeshes[i];
            const uint32_t uFillVariantCount = pl_sb_size(ptShader->tDesc.sbtVariants);

            plGraphicsState tFillStateTemplate = {
                .ulVertexStreamMask   = ptMesh->ulVertexStreamMask,
                .ulDepthMode          = PL_DEPTH_MODE_LESS_OR_EQUAL,
                .ulDepthWriteEnabled  = true,
                .ulCullMode           = VK_CULL_MODE_NONE,
                .ulBlendMode          = PL_BLEND_MODE_ALPHA,
                .ulShaderTextureFlags = PL_SHADER_TEXTURE_FLAG_BINDING_0,
                .ulStencilMode        = PL_STENCIL_MODE_ALWAYS,
                .ulStencilRef         = 0xff,
                .ulStencilMask        = 0xff,
                .ulStencilOpFail      = VK_STENCIL_OP_KEEP,
                .ulStencilOpDepthFail = VK_STENCIL_OP_KEEP,
                .ulStencilOpPass      = VK_STENCIL_OP_KEEP
            };

            for(uint32_t j = 0; j < uFillVariantCount; j++)
            {
                plGraphicsState ptVariant = ptShader->tDesc.sbtVariants[j];
                if(ptVariant.ulValue == tFillStateTemplate.ulValue)
                {
                        uFillShaderVariant = ptShader->_sbuVariantPipelines[j];
                        break;
                }
            }

            // create variant that matches texture count, vertex stream, and culling
            if(uFillShaderVariant == UINT32_MAX)
            {
                uFillShaderVariant = pl_add_shader_variant(&ptAppData->tRenderer.ptGraphics->tResourceManager, ptMaterial->uShader, tFillStateTemplate);
            }

            pl_sb_push(ptAppData->tRenderer.sbtDraws, ((plDraw){
                .uShaderVariant        = uFillShaderVariant,
                .ptMesh                = &ptAppData->tGrass.sbtMeshes[i],
                .ptBindGroup1          = &ptMaterial->tMaterialBindGroup,
                .ptBindGroup2          = &ptAppData->tGrass.sbtBindGroup2[i],
                .uDynamicBufferOffset1 = uBufferFrameOffset1 + ptAppData->sbuMaterialOffsets[uMaterialIndex],
                .uDynamicBufferOffset2 = uBufferFrameOffset2 + uDataOffset2
                }));

            plObjectInfo* ptObjectInfo = (plObjectInfo*)(ptBuffer2->pucMapping + uDataOffset2 + uBufferFrameOffset2);
            ptObjectInfo->tModel = pl_identity_mat4();
            ptObjectInfo->uVertexOffset = ptAppData->tGrass.sbuVertexOffsets[i];

            uDataOffset2 = (uint32_t)pl_align_up((size_t)uDataOffset2 + sizeof(plObjectInfo), ptAppData->tGraphics.tDevice.tDeviceProps.limits.minUniformBufferOffsetAlignment);
        }

        pl_sb_push(ptAppData->tRenderer.sbtDrawAreas, ((plDrawArea){
            .ptBindGroup0          = &ptAppData->tRenderer.tGlobalBindGroup,
            .uDrawOffset           = uDrawOffset,
            .uDrawCount            = pl_sb_size(ptAppData->tGrass.sbtMeshes),
            .uDynamicBufferOffset0 = uBufferFrameOffset0 + uDataOffset0
        }));

        uDrawOffset = pl_sb_size(ptAppData->tRenderer.sbtDraws);

        // stl
        {
            const plShader* ptOutlineShader = &ptAppData->tGraphics.tResourceManager.sbtShaders[ptAppData->uOutlineShader];
            const uint32_t uOutlineVariantCount = pl_sb_size(ptOutlineShader->tDesc.sbtVariants);
            plMaterial* ptMaterial = &ptAppData->tRenderer.ptAssetRegistry->sbtMaterials[ptAppData->uSolidMaterial];
            uint32_t uFillShaderVariant = UINT32_MAX;
            uint32_t uOutlineShaderVariant = UINT32_MAX;
            const plShader* ptShader = &ptAppData->tRenderer.ptGraphics->tResourceManager.sbtShaders[ptMaterial->uShader];   
            plMesh* ptMesh = &ptAppData->tStlModel.tMesh;
            const uint32_t uFillVariantCount = pl_sb_size(ptShader->tDesc.sbtVariants);

            plGraphicsState tFillStateTemplate = {
                .ulVertexStreamMask   = ptMesh->ulVertexStreamMask,
                .ulDepthMode          = PL_DEPTH_MODE_LESS_OR_EQUAL,
                .ulDepthWriteEnabled  = true,
                .ulCullMode           = VK_CULL_MODE_NONE,
                .ulBlendMode          = PL_BLEND_MODE_ALPHA,
                .ulShaderTextureFlags = 0,
                .ulStencilMode        = PL_STENCIL_MODE_ALWAYS,
                .ulStencilRef         = 0xff,
                .ulStencilMask        = 0xff,
                .ulStencilOpFail      = VK_STENCIL_OP_REPLACE,
                .ulStencilOpDepthFail = VK_STENCIL_OP_REPLACE,
                .ulStencilOpPass      = VK_STENCIL_OP_REPLACE
            };

            plGraphicsState tOutlineStateTemplate = {
                .ulVertexStreamMask   = ptMesh->ulVertexStreamMask,
                .ulDepthMode          = PL_DEPTH_MODE_ALWAYS,
                .ulDepthWriteEnabled  = false,
                .ulCullMode           = VK_CULL_MODE_FRONT_BIT,
                .ulBlendMode          = PL_BLEND_MODE_ALPHA,
                .ulShaderTextureFlags = 0,
                .ulStencilMode        = PL_STENCIL_MODE_NOT_EQUAL,
                .ulStencilRef         = 0xff,
                .ulStencilMask        = 0xff,
                .ulStencilOpFail      = VK_STENCIL_OP_KEEP,
                .ulStencilOpDepthFail = VK_STENCIL_OP_KEEP,
                .ulStencilOpPass      = VK_STENCIL_OP_REPLACE
            };

            if(ptMaterial->uAlbedoMap > 0)   tFillStateTemplate.ulShaderTextureFlags |= PL_SHADER_TEXTURE_FLAG_BINDING_0;
            if(ptMaterial->uNormalMap > 0)   tFillStateTemplate.ulShaderTextureFlags |= PL_SHADER_TEXTURE_FLAG_BINDING_1;
            if(ptMaterial->uEmissiveMap > 0) tFillStateTemplate.ulShaderTextureFlags |= PL_SHADER_TEXTURE_FLAG_BINDING_2;

            tOutlineStateTemplate.ulShaderTextureFlags = tFillStateTemplate.ulShaderTextureFlags;

            for(uint32_t j = 0; j < uFillVariantCount; j++)
            {
                plGraphicsState ptVariant = ptShader->tDesc.sbtVariants[j];
                if(ptVariant.ulValue == tFillStateTemplate.ulValue)
                {
                        uFillShaderVariant = ptShader->_sbuVariantPipelines[j];
                        break;
                }
            }

            for(uint32_t j = 0; j < uOutlineVariantCount; j++)
            {
                plGraphicsState ptVariant = ptOutlineShader->tDesc.sbtVariants[j];
                if(ptVariant.ulValue == tOutlineStateTemplate.ulValue)
                {
                        uOutlineShaderVariant = ptOutlineShader->_sbuVariantPipelines[j];
                        break;
                }
            }

            // create variant that matches texture count, vertex stream, and culling
            if(uFillShaderVariant == UINT32_MAX)
            {
                uFillShaderVariant = pl_add_shader_variant(&ptAppData->tRenderer.ptGraphics->tResourceManager, ptMaterial->uShader, tFillStateTemplate);
            }

            if(uOutlineShaderVariant == UINT32_MAX)
            {
                uOutlineShaderVariant = pl_add_shader_variant(&ptAppData->tRenderer.ptGraphics->tResourceManager, ptAppData->uOutlineShader, tOutlineStateTemplate);
            }

            const plMat4 tStlRotation = pl_mat4_rotate_xyz(PL_PI * (float)pl_get_io_context()->dTime, 0.0f, 1.0f, 0.0f);
            const plMat4 tStlTranslation = pl_mat4_translate_xyz(0.0f, 2.0f, 0.0f);
            const plMat4 tStlTransform = pl_mul_mat4(&tStlTranslation, &tStlRotation);

            plObjectInfo* ptObjectInfo = (plObjectInfo*)(ptBuffer2->pucMapping + uDataOffset2 + uBufferFrameOffset2);
            ptObjectInfo->tModel = tStlTransform;
            ptObjectInfo->uVertexOffset = ptAppData->tStlModel.uVertexOffset;

            pl_sb_push(ptAppData->tRenderer.sbtDraws, ((plDraw){
                .uShaderVariant        = uFillShaderVariant,
                .ptMesh                = ptMesh,
                .ptBindGroup1          = &ptMaterial->tMaterialBindGroup,
                .ptBindGroup2          = &ptAppData->tStlModel.tBindGroup2,
                .uDynamicBufferOffset1 = uBufferFrameOffset1 + ptAppData->sbuMaterialOffsets[ptAppData->uSolidMaterial],
                .uDynamicBufferOffset2 = uBufferFrameOffset2 + uDataOffset2
                }));

            pl_sb_push(ptAppData->tRenderer.sbtDraws, ((plDraw){
                .uShaderVariant        = uOutlineShaderVariant,
                .ptMesh                = ptMesh,
                .ptBindGroup1          = &ptMaterial->tMaterialBindGroup,
                .ptBindGroup2          = &ptAppData->tStlModel.tBindGroup2,
                .uDynamicBufferOffset1 = uBufferFrameOffset1 + ptAppData->sbuMaterialOffsets[ptAppData->uOutlineMaterial],
                .uDynamicBufferOffset2 = uBufferFrameOffset2 + uDataOffset2
                }));

            pl_sb_push(ptAppData->tRenderer.sbtDrawAreas, ((plDrawArea){
                .ptBindGroup0          = &ptAppData->tRenderer.tGlobalBindGroup,
                .uDrawOffset           = uDrawOffset,
                .uDrawCount            = 2,
                .uDynamicBufferOffset0 = uBufferFrameOffset0 + uDataOffset0
            }));
        }

        plGlobalInfo* ptGlobalInfo    = (plGlobalInfo*)&ptBuffer0->pucMapping[uBufferFrameOffset0 + uDataOffset0];
        ptGlobalInfo->tAmbientColor   = (plVec4){0.0f, 0.0f, 0.0f, 1.0f};
        ptGlobalInfo->tCameraPos      = (plVec4){.xyz = ptAppData->tCamera.tPos, .w = 0.0f};
        ptGlobalInfo->tCameraView     = ptAppData->tCamera.tViewMat;
        ptGlobalInfo->tCameraViewProj = pl_mul_mat4(&ptAppData->tCamera.tProjMat, &ptAppData->tCamera.tViewMat);
        ptGlobalInfo->fTime           = (float)pl_get_io_context()->dTime;

        uDataOffset0 = (uint32_t)pl_align_up((size_t)uDataOffset0 + sizeof(plGlobalInfo), ptAppData->tGraphics.tDevice.tDeviceProps.limits.minUniformBufferOffsetAlignment);

        plGlobalInfo* ptSkyboxGlobalInfo    = (plGlobalInfo*)&ptBuffer0->pucMapping[uBufferFrameOffset0 + uDataOffset0];
        ptSkyboxGlobalInfo->tAmbientColor   = ptGlobalInfo->tAmbientColor;
        ptSkyboxGlobalInfo->tCameraPos      = (plVec4){.xyz = ptAppData->tCamera.tPos, .w = 0.0f};
        ptSkyboxGlobalInfo->tCameraView     = ptAppData->tCamera.tViewMat;
        plMat4 tReverseTransform = pl_mat4_translate_xyz(ptAppData->tCamera.tPos.x, ptAppData->tCamera.tPos.y, ptAppData->tCamera.tPos.z);
        ptSkyboxGlobalInfo->tCameraViewProj = pl_mul_mat4(&ptAppData->tCamera.tViewMat, &tReverseTransform);
        ptSkyboxGlobalInfo->tCameraViewProj = pl_mul_mat4(&ptAppData->tCamera.tProjMat, &ptSkyboxGlobalInfo->tCameraViewProj);

        uint32_t uSkyboxShaderVariant = UINT32_MAX;
        uDrawOffset = pl_sb_size(ptAppData->tRenderer.sbtDraws);

        {
            // const uint32_t uMaterialIndex = ptAppData->tGltf.sbuMaterialIndices[i];
            // plMaterial* ptMaterial = &ptAppData->tRenderer.ptAssetRegistry->sbtMaterials[uMaterialIndex];
            // uint32_t uFillShaderVariant = UINT32_MAX;
            const plShader* ptShader = &ptAppData->tRenderer.ptGraphics->tResourceManager.sbtShaders[ptAppData->uSkyboxShader];   
            // const plMesh* ptMesh = &ptAppData->tSkybox.tMesh;
            const uint32_t uFillVariantCount = pl_sb_size(ptShader->tDesc.sbtVariants);

            plGraphicsState tFillStateTemplate = {
                .ulVertexStreamMask   = ptAppData->tSkybox.tMesh.ulVertexStreamMask,
                .ulDepthMode          = PL_DEPTH_MODE_LESS_OR_EQUAL,
                .ulDepthWriteEnabled  = true,
                .ulCullMode           = VK_CULL_MODE_NONE,
                .ulBlendMode          = PL_BLEND_MODE_ALPHA,
                .ulShaderTextureFlags = PL_SHADER_TEXTURE_FLAG_BINDING_0,
                .ulStencilMode        = PL_STENCIL_MODE_ALWAYS,
                .ulStencilRef         = 0xff,
                .ulStencilMask        = 0xff,
                .ulStencilOpFail      = VK_STENCIL_OP_KEEP,
                .ulStencilOpDepthFail = VK_STENCIL_OP_KEEP,
                .ulStencilOpPass      = VK_STENCIL_OP_KEEP
            };

            tFillStateTemplate.ulShaderTextureFlags |= PL_SHADER_TEXTURE_FLAG_BINDING_0;

            for(uint32_t j = 0; j < uFillVariantCount; j++)
            {
                plGraphicsState ptVariant = ptShader->tDesc.sbtVariants[j];
                if(ptVariant.ulValue == tFillStateTemplate.ulValue)
                {
                        uSkyboxShaderVariant = ptShader->_sbuVariantPipelines[j];
                        break;
                }
            }

            // create variant that matches texture count, vertex stream, and culling
            if(uSkyboxShaderVariant == UINT32_MAX)
            {
                uSkyboxShaderVariant = pl_add_shader_variant(&ptAppData->tRenderer.ptGraphics->tResourceManager, ptAppData->uSkyboxShader, tFillStateTemplate);
            }
        }

        pl_sb_push(ptAppData->tRenderer.sbtDraws, ((plDraw){
            .uShaderVariant        = uSkyboxShaderVariant,
            .ptMesh                = &ptAppData->tSkybox.tMesh,
            .ptBindGroup1          = NULL,
            .ptBindGroup2          = NULL,
            .uDynamicBufferOffset1 = 0,
            .uDynamicBufferOffset2 = 0
            }));

        pl_sb_push(ptAppData->tRenderer.sbtDrawAreas, ((plDrawArea){
            .ptBindGroup0          = &ptAppData->tSkybox.tBindGroup0,
            .uDrawOffset           = uDrawOffset,
            .uDrawCount            = 1,
            .uDynamicBufferOffset0 = uBufferFrameOffset0 + uDataOffset0
        }));

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~submit draws~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        pl_draw_areas(&ptAppData->tGraphics, pl_sb_size(ptAppData->tRenderer.sbtDrawAreas), ptAppData->tRenderer.sbtDrawAreas, ptAppData->tRenderer.sbtDraws);

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
            static bool bOpenValues[512] = {0};
            if(pl_ui_collapsing_header("Materials"))
            {
                for(uint32_t i = 0; i < pl_sb_size(ptAppData->tAssetRegistry.sbtMaterials); i++)
                {
                    plMaterial* ptMaterial = &ptAppData->tAssetRegistry.sbtMaterials[i];

                    static bool bOpen0 = false;
                    if(pl_ui_tree_node(ptMaterial->acName))
                    {
                        pl_ui_text("Double Sided: %s", ptMaterial->bDoubleSided ? "true" : "false");
                        pl_ui_text("Alpha cutoff: %0.1f", ptMaterial->fAlphaCutoff);
                        ptMaterial->uShader      == 0 ? pl_ui_text("Shader: None")       : pl_ui_text("Shader: %u", ptMaterial->uShader);
                        ptMaterial->uAlbedoMap   == 0 ? pl_ui_text("Albedo Map: None")   : pl_ui_text("Albedo Map: %u", ptMaterial->uAlbedoMap);
                        ptMaterial->uNormalMap   == 0 ? pl_ui_text("Normal Map: None")   : pl_ui_text("Normal Map: %u", ptMaterial->uNormalMap);
                        pl_ui_tree_pop();
                    }
                }
                pl_ui_end_collapsing_header();
            }
            pl_ui_end_window();
        }
        
        if(bOpen)
        {
            if(pl_ui_begin_window("Camera Info", &bOpen, true))
            {
                pl_ui_text("Pos: %.3f, %.3f, %.3f", ptAppData->tCamera.tPos.x, ptAppData->tCamera.tPos.y, ptAppData->tCamera.tPos.z);
                pl_ui_text("Pitch: %.3f, Yaw: %.3f, Roll:%.3f", ptAppData->tCamera.fPitch, ptAppData->tCamera.fYaw, ptAppData->tCamera.fRoll);
                pl_ui_text("Up: %.3f, %.3f, %.3f", ptAppData->tCamera._tUpVec.x, ptAppData->tCamera._tUpVec.y, ptAppData->tCamera._tUpVec.z);
                pl_ui_text("Forward: %.3f, %.3f, %.3f", ptAppData->tCamera._tForwardVec.x, ptAppData->tCamera._tForwardVec.y, ptAppData->tCamera._tForwardVec.z);
                pl_ui_text("Right: %.3f, %.3f, %.3f", ptAppData->tCamera._tRightVec.x, ptAppData->tCamera._tRightVec.y, ptAppData->tCamera._tRightVec.z);  
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
        pl_submit_drawlist_vulkan(ptAppData->tUiContext.ptDrawlist, (float)ptIOCtx->afMainViewportSize[0], (float)ptIOCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptAppData->tGraphics.szCurrentFrameIndex);
        pl_submit_drawlist_vulkan(ptAppData->tUiContext.ptDebugDrawlist, (float)ptIOCtx->afMainViewportSize[0], (float)ptIOCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptAppData->tGraphics.szCurrentFrameIndex);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~end frame~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        pl_end_main_pass(&ptAppData->tGraphics);
        pl_end_recording(&ptAppData->tGraphics);
        pl_end_frame(&ptAppData->tGraphics);
    } 
    pl_end_profile_frame();
}

//-----------------------------------------------------------------------------
// [SECTION] helper implementations
//-----------------------------------------------------------------------------

void
pl_load_shaders(plAppData* ptAppData)
{
    //~~~~~~~~~~~~~~~~~~~~~~~~create shader descriptions~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // main
    plShaderDesc tMainShaderDesc = {
        ._tRenderPass                        = ptAppData->tGraphics.tRenderPass,
        .pcPixelShader                       = "phong.frag.spv",
        .pcVertexShader                      = "primitive.vert.spv",
        .tGraphicsState.ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_HAS_NORMAL | PL_MESH_FORMAT_FLAG_HAS_COLOR_0,
        .tGraphicsState.ulDepthMode          = PL_DEPTH_MODE_LESS,
        .tGraphicsState.ulBlendMode          = PL_BLEND_MODE_ALPHA,
        .tGraphicsState.ulCullMode           = VK_CULL_MODE_BACK_BIT,
        .tGraphicsState.ulDepthWriteEnabled  = VK_TRUE,
        .tGraphicsState.ulShaderTextureFlags = PL_SHADER_TEXTURE_FLAG_BINDING_NONE,
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

    // skybox
    plShaderDesc tSkyboxShaderDesc = {
        ._tRenderPass                        = ptAppData->tGraphics.tRenderPass,
        .pcPixelShader                       = "skybox.frag.spv",
        .pcVertexShader                      = "skybox.vert.spv",
        .tGraphicsState.ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_NONE,
        .tGraphicsState.ulDepthMode          = PL_DEPTH_MODE_LESS_OR_EQUAL,
        .tGraphicsState.ulBlendMode          = PL_BLEND_MODE_ADDITIVE,
        .tGraphicsState.ulCullMode           = VK_CULL_MODE_NONE,
        .tGraphicsState.ulDepthWriteEnabled  = VK_FALSE,
        .tGraphicsState.ulShaderTextureFlags = PL_SHADER_TEXTURE_FLAG_BINDING_NONE,
        .tGraphicsState.ulStencilMode        = PL_STENCIL_MODE_ALWAYS,
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

    // outline
    plShaderDesc tOutlineShaderDesc = {
        ._tRenderPass                        = ptAppData->tGraphics.tRenderPass,
        .pcPixelShader                       = "outline.frag.spv",
        .pcVertexShader                      = "outline.vert.spv",
        .tGraphicsState.ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_HAS_NORMAL,
        .tGraphicsState.ulDepthMode          = PL_DEPTH_MODE_ALWAYS,
        .tGraphicsState.ulBlendMode          = PL_BLEND_MODE_ALPHA,
        .tGraphicsState.ulCullMode           = VK_CULL_MODE_FRONT_BIT,
        .tGraphicsState.ulDepthWriteEnabled  = VK_TRUE,
        .tGraphicsState.ulShaderTextureFlags = PL_SHADER_TEXTURE_FLAG_BINDING_NONE,
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

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~create bind groups~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // main
    pl_create_bind_group(&ptAppData->tGraphics, &tMainShaderDesc.atBindGroupLayouts[0], NULL, "nullg material");
    pl_create_bind_group(&ptAppData->tGraphics, &tMainShaderDesc.atBindGroupLayouts[1], NULL, "main shader");
    pl_create_bind_group(&ptAppData->tGraphics, &tMainShaderDesc.atBindGroupLayouts[2], NULL, "null material");

    // skybox
    pl_create_bind_group(&ptAppData->tGraphics, &tSkyboxShaderDesc.atBindGroupLayouts[0], NULL, "skybox material");

    // outline
    pl_create_bind_group(&ptAppData->tGraphics, &tOutlineShaderDesc.atBindGroupLayouts[0], NULL, "nullg material");
    pl_create_bind_group(&ptAppData->tGraphics, &tOutlineShaderDesc.atBindGroupLayouts[1], NULL, "outline shader");
    pl_create_bind_group(&ptAppData->tGraphics, &tOutlineShaderDesc.atBindGroupLayouts[2], NULL, "null material");

    // grass
    pl_create_bind_group(&ptAppData->tGraphics, &tGrassShaderDesc.atBindGroupLayouts[0], NULL, "nullg material");
    pl_create_bind_group(&ptAppData->tGraphics, &tGrassShaderDesc.atBindGroupLayouts[1], NULL, "grass shader");
    pl_create_bind_group(&ptAppData->tGraphics, &tGrassShaderDesc.atBindGroupLayouts[2], NULL, "null material");

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~create shaders~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    ptAppData->uMainShader    = pl_create_shader(&ptAppData->tGraphics.tResourceManager, &tMainShaderDesc);
    ptAppData->uSkyboxShader  = pl_create_shader(&ptAppData->tGraphics.tResourceManager, &tSkyboxShaderDesc);
    ptAppData->uOutlineShader = pl_create_shader(&ptAppData->tGraphics.tResourceManager, &tOutlineShaderDesc);
    ptAppData->uGrassShader = pl_create_shader(&ptAppData->tGraphics.tResourceManager, &tGrassShaderDesc);
}

void
pl_load_textures(plAppData* ptAppData)
{
    // skybox
    {
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
            .uMips       = 1,
            .tType       = VK_IMAGE_TYPE_2D,
            .tViewType   = VK_IMAGE_VIEW_TYPE_CUBE
        };
        ptAppData->tSkybox.uTexture  = pl_create_texture(&ptAppData->tGraphics.tResourceManager, tTextureDesc, sizeof(unsigned char) * texHeight * texHeight * texNumChannels * 6, rawBytes);
        pl_free(rawBytes);
    }

    // grass
    {
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
        ptAppData->tGrass.uTexture  = pl_create_texture(&ptAppData->tGraphics.tResourceManager, tTextureDesc, sizeof(unsigned char) * texHeight * texHeight * 4, rawBytes);
    }
}

void
pl_load_skybox_mesh(plAppData* ptAppData)
{
    ptAppData->tSkybox.tMesh.uIndexCount = 36;
    ptAppData->tSkybox.tMesh.uVertexCount = 24;
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
    ptAppData->tSkybox.tMesh.ulVertexStreamMask = PL_MESH_FORMAT_FLAG_NONE;
    ptAppData->tSkybox.tMesh.uVertexBuffer = pl_create_vertex_buffer(&ptAppData->tGraphics.tResourceManager, sizeof(float) * 24, sizeof(float), acSkyBoxVertices);
    ptAppData->tSkybox.tMesh.uIndexBuffer = pl_create_index_buffer(&ptAppData->tGraphics.tResourceManager, sizeof(uint32_t) * 36, acSkyboxIndices);
}

void
pl_load_stl_mesh(plAppData* ptAppData)
{
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
    ptAppData->tStlModel.uVertexOffset = pl_sb_size(ptAppData->tRenderer.sbfStorageBuffer) / 4;

    const uint32_t uPrevIndex = pl_sb_add_n(ptAppData->tRenderer.sbfStorageBuffer, (uint32_t)tStlInfo.szVertexStream1Size);
    memcpy(&ptAppData->tRenderer.sbfStorageBuffer[uPrevIndex], afVertexBuffer1, tStlInfo.szVertexStream1Size * sizeof(float));

    ptAppData->tStlModel.tMesh.uIndexCount   = (uint32_t)tStlInfo.szIndexBufferSize;
    ptAppData->tStlModel.tMesh.uVertexCount  = (uint32_t)tStlInfo.szVertexStream0Size / 3;
    ptAppData->tStlModel.tMesh.uIndexBuffer  = pl_create_index_buffer(&ptAppData->tGraphics.tResourceManager, sizeof(uint32_t) * tStlInfo.szIndexBufferSize, auIndexBuffer);
    ptAppData->tStlModel.tMesh.uVertexBuffer = pl_create_vertex_buffer(&ptAppData->tGraphics.tResourceManager, tStlInfo.szVertexStream0Size * sizeof(float), sizeof(plVec3), afVertexBuffer0);
    ptAppData->tStlModel.tMesh.ulVertexStreamMask = PL_MESH_FORMAT_FLAG_HAS_NORMAL | PL_MESH_FORMAT_FLAG_HAS_COLOR_0;

    pl_free(acFileData);
    pl_free(afVertexBuffer0);
    pl_free(afVertexBuffer1);
    pl_free(auIndexBuffer);

}

void
pl_load_single_grass(plAppData* ptAppData, float fX, float fY, float fZ, float fHeight)
{

    // first quad
    pl_sb_push(ptAppData->tGrass.sbtVertexData, ((plVec3){ -0.5f + fX, fHeight + fY, 0.0f + fZ}));
    pl_sb_push(ptAppData->tGrass.sbtVertexData, ((plVec3){  0.5f + fX, fHeight + fY, 0.0f + fZ}));
    pl_sb_push(ptAppData->tGrass.sbtVertexData, ((plVec3){  0.5f + fX,    0.0f + fY, 0.0f + fZ}));
    pl_sb_push(ptAppData->tGrass.sbtVertexData, ((plVec3){ -0.5f + fX,    0.0f + fY,  0.0f + fZ}));

    // second quad
    pl_sb_push(ptAppData->tGrass.sbtVertexData, ((plVec3){ -0.35f + fX,    1.0f + fY, -0.35f + fZ}));
    pl_sb_push(ptAppData->tGrass.sbtVertexData, ((plVec3){  0.35f + fX, fHeight + fY,  0.35f + fZ}));
    pl_sb_push(ptAppData->tGrass.sbtVertexData, ((plVec3){  0.35f + fX,    0.0f + fY,  0.35f + fZ}));
    pl_sb_push(ptAppData->tGrass.sbtVertexData, ((plVec3){ -0.35f + fX,    0.0f + fY, -0.35f + fZ}));

    // third quad
    pl_sb_push(ptAppData->tGrass.sbtVertexData, ((plVec3){ -0.35f + fX, fHeight + fY,  0.35f + fZ}));
    pl_sb_push(ptAppData->tGrass.sbtVertexData, ((plVec3){  0.35f + fX, fHeight + fY, -0.35f + fZ}));
    pl_sb_push(ptAppData->tGrass.sbtVertexData, ((plVec3){  0.35f + fX,    0.0f + fY, -0.35f + fZ}));
    pl_sb_push(ptAppData->tGrass.sbtVertexData, ((plVec3){ -0.35f + fX,    0.0f + fY,  0.35f + fZ}));

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
        .uIndexBuffer        = ptAppData->tGrass.uIndexBuffer,
        .ulVertexStreamMask  = PL_MESH_FORMAT_FLAG_HAS_NORMAL | PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0,
        .uVertexOffset       = ptAppData->tGrass.uMeshCount * 12
    };

    const uint32_t uStorageOffset = pl_sb_size(ptAppData->tRenderer.sbfStorageBuffer) / 4;
    pl_sb_push(ptAppData->tGrass.sbuVertexOffsets, uStorageOffset);
    pl_sb_reserve(ptAppData->tRenderer.sbfStorageBuffer, pl_sb_size(ptAppData->tRenderer.sbfStorageBuffer) + 24);
    for(uint32_t i = 0; i < 24; i++)
    {
        pl_sb_push(ptAppData->tRenderer.sbfStorageBuffer, acStorageBuffer[i].x);
        pl_sb_push(ptAppData->tRenderer.sbfStorageBuffer, acStorageBuffer[i].y);
        pl_sb_push(ptAppData->tRenderer.sbfStorageBuffer, acStorageBuffer[i].z);
        pl_sb_push(ptAppData->tRenderer.sbfStorageBuffer, acStorageBuffer[i].w);
    }

    ptAppData->tGrass.uMeshCount++;
    pl_sb_push(ptAppData->tGrass.sbtMeshes, tMesh);

    // PL_ASSERT(ptAppData->uGrassMeshCount <= pl_sb_size(ptAppData->sbuGrassMaterialIndices));
}