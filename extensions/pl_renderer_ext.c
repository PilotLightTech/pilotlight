/*
   pl_renderer_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] implementation
// [SECTION] extension loading
// [SECTION] unity
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_renderer_internal.h"
#include "pl_renderer_terrain.c"
#include "pl_renderer_internal.c"


//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

static void
pl__renderer_console_shader_reload(const char* pcName, void* pData)
{
    for(uint32_t i = 0; i < pl_sb_size(gptData->sbptScenes); i++)
        pl_renderer_editor_reload_scene_shaders(gptData->sbptScenes[i]);
}

plEntity
pl_renderer_ecs_create_directional_light(plComponentLibrary* ptLibrary, const char* pcName, plVec3 tDirection, plLightComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed directional light";
    PL_LOG_DEBUG_API_F(gptLog, gptData->uLogChannel, "created directional light: '%s'", pcName);
    plEntity tNewEntity = gptEcs->create_entity(ptLibrary, pcName);
    plLightComponent* ptLight =  gptEcs->add_component(ptLibrary, gptData->tLightComponentType, tNewEntity);
    ptLight->tDirection = tDirection;
    ptLight->tType = PL_LIGHT_TYPE_DIRECTIONAL;

    if(pptCompOut)
        *pptCompOut = ptLight;
    return tNewEntity;
}

plEntity
pl_renderer_ecs_create_point_light(plComponentLibrary* ptLibrary, const char* pcName, plVec3 tPosition, plLightComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed point light";
    PL_LOG_DEBUG_API_F(gptLog, gptData->uLogChannel, "created point light: '%s'", pcName);
    plEntity tNewEntity = gptEcs->create_entity(ptLibrary, pcName);
    plLightComponent* ptLight =  gptEcs->add_component(ptLibrary, gptData->tLightComponentType, tNewEntity);
    ptLight->tPosition = tPosition;
    ptLight->tType = PL_LIGHT_TYPE_POINT;

    if(pptCompOut)
        *pptCompOut = ptLight;
    return tNewEntity;
}

plEntity
pl_renderer_ecs_create_environment_probe(plComponentLibrary* ptLibrary, const char* pcName, plVec3 tPosition, plEnvironmentProbeComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed environment probe";
    PL_LOG_DEBUG_API_F(gptLog, gptData->uLogChannel, "created environment probe: '%s'", pcName);
    plEntity tNewEntity = gptEcs->create_entity(ptLibrary, pcName);

    plTransformComponent* ptProbeTransform = gptEcs->add_component(ptLibrary, gptTransform->get_ecs_type_key_transform(), tNewEntity);
    ptProbeTransform->tTranslation = tPosition;

    plEnvironmentProbeComponent* ptProbe =  gptEcs->add_component(ptLibrary, gptData->tEnvironmentProbeComponentType, tNewEntity);

    if(pptCompOut)
        *pptCompOut = ptProbe;
    return tNewEntity;
}

plEntity
pl_renderer_ecs_create_spot_light(plComponentLibrary* ptLibrary, const char* pcName, plVec3 tPosition, plVec3 tDirection, plLightComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed spot light";
    PL_LOG_DEBUG_API_F(gptLog, gptData->uLogChannel, "created spot light: '%s'", pcName);
    plEntity tNewEntity = gptEcs->create_entity(ptLibrary, pcName);
    plLightComponent* ptLight =  gptEcs->add_component(ptLibrary, gptData->tLightComponentType, tNewEntity);
    ptLight->tPosition = tPosition;
    ptLight->tDirection = tDirection;
    ptLight->tType = PL_LIGHT_TYPE_SPOT;

    if(pptCompOut)
        *pptCompOut = ptLight;
    return tNewEntity;
}

plEntity
pl_renderer_ecs_create_object(plComponentLibrary* ptLibrary, const char* pcName, plObjectComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed object";
    PL_LOG_DEBUG_API_F(gptLog, gptData->uLogChannel, "created object: '%s'", pcName);
    plEntity tNewEntity = gptEcs->create_entity(ptLibrary, pcName);

    plObjectComponent* ptObject = gptEcs->add_component(ptLibrary, gptData->tObjectComponentType, tNewEntity);
    (void)gptEcs->add_component(ptLibrary, gptTransform->get_ecs_type_key_transform(), tNewEntity);

    ptObject->tTransform = tNewEntity;

    if(pptCompOut)
        *pptCompOut = ptObject;

    return tNewEntity;    
}

plEntity
pl_renderer_ecs_copy_object(plComponentLibrary* ptLibrary, const char* pcName, plEntity tOriginalObject, plObjectComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed object copy";
    PL_LOG_DEBUG_API_F(gptLog, gptData->uLogChannel, "copied object: '%s'", pcName);

    plEntity tNewEntity = gptEcs->create_entity(ptLibrary, pcName);

    plObjectComponent* ptOriginalObject = gptEcs->get_component(ptLibrary, gptData->tObjectComponentType, tOriginalObject);


    plObjectComponent* ptObject = gptEcs->add_component(ptLibrary, gptData->tObjectComponentType, tNewEntity);
    (void)gptEcs->add_component(ptLibrary, gptTransform->get_ecs_type_key_transform(), tNewEntity);

    ptObject->tTransform = tNewEntity;
    ptObject->tMesh = ptOriginalObject->tMesh;

    if(pptCompOut)
        *pptCompOut = ptObject;

    return tNewEntity; 
}

plEcsTypeKey
pl_renderer_ecs_get_type_key_object(void)
{
    return gptData->tObjectComponentType;
}

plEcsTypeKey
pl_renderer_ecs_get_type_key_light(void)
{
    return gptData->tLightComponentType;
}

plEcsTypeKey
pl_renderer_ecs_get_type_key_environment_probe(void)
{
    return gptData->tEnvironmentProbeComponentType;
}

plEcsTypeKey
pl_renderer_ecs_get_type_key_terrain(void)
{
    return gptData->tTerrainComponentType;
}

bool
pl_renderer_initialize(const plRendererSettings* ptSettings)
{
    // allocate renderer data
    gptData = PL_ALLOC(sizeof(plRefRendererData));
    memset(gptData, 0, sizeof(plRefRendererData));

    if(gptConsole->add_bool_variable)
    {
        gptConsole->add_toggle_variable("r.Tools", &gptData->bShowTools, "shows renderer tools", PL_CONSOLE_VARIABLE_FLAGS_CLOSE_CONSOLE);
    }

    gptData->ptDevice = ptSettings->ptDevice;
    gptData->tDeviceInfo = *gptGfx->get_device_info(gptData->ptDevice);
    gptData->ptSwap = ptSettings->ptSwapchain;

    gptStage->initialize((plStageInit){.ptDevice = gptData->ptDevice});

    // register data with registry (for reloads)
    gptDataRegistry->set_data("ref renderer data", gptData);

    // add specific log channel for renderer
    plLogExtChannelInit tLogInit = {
        .tType       = PL_LOG_CHANNEL_TYPE_CYCLIC_BUFFER,
        .uEntryCount = 1024
    };
    gptData->uLogChannel = gptLog->add_channel("Renderer", tLogInit);

    // default options
    gptData->pdDrawCalls = gptStats->get_counter("draw calls");

    // create main bind group pool
    const plBindGroupPoolDesc tBindGroupPoolDesc = {
        .eFlags                      = PL_BIND_GROUP_POOL_FLAGS_INDIVIDUAL_RESET,
        .szSamplerBindings           = 100000,
        .szUniformBufferBindings     = 100000,
        .szStorageBufferBindings     = 100000,
        .szSampledTextureBindings    = 100000,
        .szStorageTextureBindings    = 100000,
        .szAttachmentTextureBindings = 100000
    };
    gptData->ptBindGroupPool = gptGfx->create_bind_group_pool(gptData->ptDevice, &tBindGroupPoolDesc);

    // create pools
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        plBindGroupPoolDesc tPoolDesc = {
            .eFlags                      = PL_BIND_GROUP_POOL_FLAGS_NONE,
            .szSamplerBindings           = 10000,
            .szUniformBufferBindings     = 10000,
            .szStorageBufferBindings     = 10000,
            .szSampledTextureBindings    = 10000,
            .szStorageTextureBindings    = 10000,
            .szAttachmentTextureBindings = 10000
        };
        gptData->aptTempGroupPools[i] = gptGfx->create_bind_group_pool(gptData->ptDevice, &tPoolDesc);
    }

    // load gpu allocators
    gptData->ptLocalBuddyAllocator           = gptGpuAllocators->get_local_buddy_allocator(gptData->ptDevice);
    gptData->ptLocalDedicatedAllocator       = gptGpuAllocators->get_local_dedicated_allocator(gptData->ptDevice);
    gptData->ptStagingUnCachedAllocator      = gptGpuAllocators->get_staging_uncached_allocator(gptData->ptDevice);
    gptData->ptStagingUnCachedBuddyAllocator = gptGpuAllocators->get_staging_uncached_buddy_allocator(gptData->ptDevice);
    gptData->ptStagingCachedAllocator        = gptGpuAllocators->get_staging_cached_allocator(gptData->ptDevice);

    return true;
};

plRenderScene*
pl_renderer_create_scene(const plSceneDesc* ptInit)
{
    if(!gptData->bFullyInitialized)
    {
        pl__renderer_initialize();
    }

    plSceneDesc tInit = *ptInit;

    if(tInit.szIndexBufferSize == 0)    tInit.szIndexBufferSize = 64000000;
    if(tInit.szVertexBufferSize == 0)   tInit.szVertexBufferSize = 64000000;
    if(tInit.szDataBufferSize == 0)     tInit.szDataBufferSize = 64000000;
    if(tInit.szMaterialBufferSize == 0) tInit.szMaterialBufferSize = 8000000;
    if(tInit.szSkinBufferSize == 0)     tInit.szSkinBufferSize = 8000000;

    plRenderScene* ptScene = PL_ALLOC(sizeof(plRenderScene));
    memset(ptScene, 0, sizeof(plRenderScene));
    pl_sb_push(gptData->sbptScenes, ptScene);

    ptScene->tInit = tInit;


    ptScene->tEnvironment = gptAsset->load("/assets/environments/skybox.plenvironment");
    ptScene->tSettings = gptAsset->load("/assets/settings/default.plrenderer");
    ptScene->uSunShadowAtlasResolution = 4096 * 4; // TODO: make this an option


    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        ptScene->abSunTransmissionLutDirty[i] = true;

    ptScene->tSunTransmissionLutShader = gptShaderVariant->get_compute_shader("sky_transmission_lut", NULL);
    ptScene->tSunMultiscatterShader = gptShaderVariant->get_compute_shader("sky_multiscatter_lut", NULL);
    ptScene->tSkyViewLutShader = gptShaderVariant->get_compute_shader("sky_lut", NULL);
    ptScene->tSkyAerialLutShader = gptShaderVariant->get_compute_shader("sky_aerial_lut", NULL);
    ptScene->tSkyAerialShader = gptShaderVariant->get_compute_shader("sky_aerial", NULL);
    ptScene->tSkyShader = gptShaderVariant->get_shader("sky", NULL, NULL, NULL, &gptData->tTransparentRenderPassLayout);

    ptScene->ptComponentLibrary = tInit.ptComponentLibrary;
    ptScene->pcName = "unnamed scene";
    ptScene->tInternalFlags = PL_SCENE_INTERNAL_FLAG_ACTIVE;

    ptScene->uShadowAtlasResolution = ptInit->uShadowAtlasResolution;
    if(ptScene->uShadowAtlasResolution == 0)
        ptScene->uShadowAtlasResolution = 4092;

    // create scene freelists
    //-----------------------------------------------------------------------------

    gptFreeList->create((uint64_t)tInit.szMaterialBufferSize, sizeof(plGpuMaterial), &ptScene->tMaterialFreeList);
    gptFreeList->create((uint64_t)tInit.szSkinBufferSize, 4096, &ptScene->tSkinBufferFreeList);
    gptFreeList->create(tInit.szIndexBufferSize, 128, &ptScene->tIndexBufferFreeList);
    gptFreeList->create(tInit.szVertexBufferSize, 128, &ptScene->tVertexBufferFreeList);
    gptFreeList->create(tInit.szDataBufferSize, 128, &ptScene->tStorageBufferFreeList);
    gptFreeList->create(4096, sizeof(plMat4), &ptScene->tShadowCameraFreeList);

    // create global bindgroup
    ptScene->uTextureIndexCount = 0;

    // create scene resources
    //-----------------------------------------------------------------------------

    pl__renderer_scene_create_textures(ptScene);
    pl__renderer_scene_create_buffers(ptScene);
    pl__renderer_scene_create_bindgroups(ptScene);
    pl__renderer_scene_update_bindgroups(ptScene);
    pl__renderer_scene_create_brdf_lut(ptScene);

    // bindless resource indices
    ptScene->uShadowAtlasIndex = pl__renderer_get_bindless_texture_index(ptScene, ptScene->tShadowTexture);
    ptScene->uSunShadowAtlasIndex = pl__renderer_get_bindless_texture_index(ptScene, ptScene->tSunShadowTexture);
    ptScene->tSceneData.iBrdfLutIndex = pl__renderer_get_bindless_texture_index(ptScene, ptScene->tBrdfLutTexture);

    // create initial shaders
    //-----------------------------------------------------------------------------

    int iSceneWideRenderingFlags = PL_RENDERING_FLAG_SHADOWS | PL_RENDERING_FLAG_PCF_SHADOWS | PL_RENDERING_FLAG_USE_IBL | PL_RENDERING_FLAG_USE_NORMAL_MAPS | PL_RENDERING_FLAG_PUNCTUAL;

    // create lighting shader
    int aiLightingConstantData[] = {iSceneWideRenderingFlags, ptScene->tDebugOptions.tShaderDebugMode};

    if(ptScene->tDebugOptions.tShaderDebugMode)
        ptScene->tDirectionalLightingShader = gptShaderVariant->get_shader("deferred_lighting_debug", NULL, NULL, aiLightingConstantData, &gptData->tDeferredLightingRenderPassLayout);
    else
    {
        ptScene->tSunShader = gptShaderVariant->get_shader("deferred_lighting_sun", NULL, NULL, aiLightingConstantData, &gptData->tDeferredLightingRenderPassLayout);
        ptScene->tDirectionalLightingShader = gptShaderVariant->get_shader("deferred_lighting_directional", NULL, NULL, aiLightingConstantData, &gptData->tDeferredLightingRenderPassLayout);
    }
    ptScene->tSpotLightingShader = gptShaderVariant->get_shader("deferred_lighting_spot", NULL, NULL, aiLightingConstantData, &gptData->tDeferredLightingRenderPassLayout);
    ptScene->tPointLightingShader = gptShaderVariant->get_shader("deferred_lighting_point", NULL, NULL, aiLightingConstantData, &gptData->tDeferredLightingRenderPassLayout);
    ptScene->tProbeLightingShader = gptShaderVariant->get_shader("deferred_lighting", NULL, NULL, NULL, &gptData->tDeferredLightingRenderPassLayout);
    ptScene->tTerrainShader = gptShaderVariant->get_shader("terrain", NULL, NULL, NULL, &gptData->tRenderPassLayout);
    ptScene->tTerrainShadowShader = gptShaderVariant->get_shader("terrain_shadow", NULL, NULL, NULL, &gptData->tDepthRenderPassLayout);

    plGraphicsState tTerrainVariantTemp = {
        .bDepthWriteEnabled  = 1,
        .eDepthMode          = PL_COMPARE_MODE_GREATER,
        .eCullMode           = PL_CULL_MODE_NONE,
        .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
        .uStencilRef         = 0xff,
        .uStencilMask        = 0xff,
        .eStencilOpFail      = PL_STENCIL_OP_KEEP,
        .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
        .eStencilOpPass      = PL_STENCIL_OP_KEEP,
        .bWireframe          = 1
    };
    ptScene->tTerrainWireframeShader = gptShaderVariant->get_shader("terrain", &tTerrainVariantTemp, NULL, NULL, &gptData->tRenderPassLayout);
    

    // create probe meshes & resources
    //-----------------------------------------------------------------------------

    plMesh tUnitSphere = {0};
    gptMesh->create_sphere(0.5f, 16, 16, &tUnitSphere);
    tUnitSphere.atSubmeshes[0].tMaterial = gptAsset->load("/assets/materials/default.plmaterial");
    plAssetDesc tMeshAsset = {
        .tType = gptMesh->get_asset_type_key(),
        .pcPath = "/assets/meshes/sphere.plmesh"
    };
    plAssetHandle tBlah = gptAsset->create(&tMeshAsset, &tUnitSphere);
    gptAsset->save(tBlah, PL_ASSET_ENCODING_TEXT);

    // create probe material & mesh
    ptScene->tSphereMesh = gptAsset->load("/assets/meshes/sphere.plmesh");
    ptScene->tProbeMesh = gptAsset->load("/assets/meshes/probe.plmesh");
    plMesh* ptMesh = gptAsset->get_data(ptScene->tSphereMesh);

    plFreeListNode* ptIndexBufferNode = gptFreeList->get_node(&ptScene->tIndexBufferFreeList, (uint32_t)ptMesh->atSubmeshes->szIndexCount * sizeof(uint32_t));
    plFreeListNode* ptVertexBufferNode = gptFreeList->get_node(&ptScene->tVertexBufferFreeList, (uint32_t)ptMesh->atSubmeshes->szVertexCount * sizeof(plVec3));

    const plDrawable tDrawable = {
        .uIndexCount     = (uint32_t)ptMesh->atSubmeshes->szIndexCount,
        .uVertexCount    = (uint32_t)ptMesh->atSubmeshes->szVertexCount,
        .uIndexOffset    = (uint32_t)(ptIndexBufferNode->uOffset / sizeof(uint32_t)),
        .uVertexOffset   = (uint32_t)(ptVertexBufferNode->uOffset / sizeof(plVec3)),
        .uTransformIndex = ptScene->uNextTransformIndex++,
        .uTriangleCount  = (uint32_t)ptMesh->atSubmeshes->szIndexCount / 3,
        .uSubmeshIndex = 0,
    };
    ptScene->tUnitSphereDrawable = tDrawable;

    gptStage->stage_buffer_upload(ptScene->tIndexBuffer, ptIndexBufferNode->uOffset, ptMesh->atSubmeshes->puIndices, sizeof(uint32_t) * ptMesh->atSubmeshes->szIndexCount);
    gptStage->stage_buffer_upload(ptScene->tVertexBuffer, ptVertexBufferNode->uOffset, ptMesh->atSubmeshes->ptVertexPositions, sizeof(plVec3) * ptMesh->atSubmeshes->szVertexCount);
    gptStage->flush();

    // create probe material & mesh
    pl__renderer_add_material_to_scene(ptScene, ptMesh->atSubmeshes->tMaterial);
    return ptScene;
}

void
pl_renderer_destroy_view(plView* ptView)
{

    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->tAlbedoTexture);
    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->tNormalTexture);
    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->tAOMetalRoughnessTexture);
    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->tRawOutputTexture);
    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->tDepthTexture);
    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->atUVMaskTexture0);
    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->atUVMaskTexture1);
    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->tFinalTexture);
    if(ptView->ptParentScene->tInternalFlags & PL_SCENE_INTERNAL_FLAG_TRANSMISSION_REQUIRED)
        gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->tTransmissionTexture);
    gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, ptView->tFinalTextureHandle);
    gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, ptView->tLightingBindGroup);
    gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, ptView->tTonemapBG);
    gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, ptView->atOutlineBG[0]);
    gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, ptView->atOutlineBG[1]);

    for(uint32_t i = 0; i < pl_sb_size(ptView->sbtBloomDownChain); i++)
    {
        gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->sbtBloomDownChain[i]);
        gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->sbtBloomUpChain[i]);
    }
    pl_sb_free(ptView->sbtBloomDownChain);
    pl_sb_free(ptView->sbtBloomUpChain);
    pl_sb_free(ptView->sbtDLightShadowData);
    
    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->tPickTexture);

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        gptStarter->return_staging_buffer(&ptView->atSunCameraBuffers[i]);
        gptStarter->return_staging_buffer(&ptView->atDShadowCameraBuffers[i]);
        gptStarter->return_staging_buffer(&ptView->atDLightShadowDataBuffer[i]);
        gptStarter->return_staging_buffer(&ptView->atViewBuffers[i]);
        gptStarter->return_readback_buffer(&ptView->atPickBuffer[i]);
        gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, ptView->atPickBindGroup[i]);
        gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, ptView->atDeferredBG1[i]);
        gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, ptView->atViewBG[i]);
    }

    gptDraw->return_3d_drawlist(ptView->pt3DDrawList);
    gptDraw->return_3d_drawlist(ptView->pt3DGizmoDrawList);
    gptDraw->return_3d_drawlist(ptView->pt3DSelectionDrawList);

    for(uint32_t i = 0; i < pl_sb_size(ptView->ptParentScene->sbptViews); i++)
    {
        if(ptView->ptParentScene->sbptViews[i] == ptView)
        {
            pl_sb_del(ptView->ptParentScene->sbptViews, i);
            break;
        }
    }

    PL_FREE(ptView);
}

plRenderSceneFlags
pl_renderer_get_scene_flags(const plRenderScene* ptScene)
{
    if(ptScene)
        return ptScene->tFlags;
    return 0;
}

void
pl_renderer_set_scene_flags(plRenderScene* ptScene, plRenderSceneFlags tFlags)
{
    if(ptScene)
        ptScene->tFlags = tFlags;
}

void
pl_renderer_destroy_scene(plRenderScene* ptScene)
{
    if(ptScene == NULL)
        return;

    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptScene->tBrdfLutTexture);
    for(uint32_t j = 0; j < pl_sb_size(ptScene->sbtProbeDataPacks); j++)
    {
        plEnvironmentProbeDataPack* ptProbePack = &ptScene->sbtProbeDataPacks[j];
        gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptProbePack->tAlbedoTexture);
        gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptProbePack->tNormalTexture);
        gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptProbePack->tAOMetalRoughnessTexture);
        gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptProbePack->tDepthTexture);


        for(uint32_t i = 0; i < 6; i++)
        {
            gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptProbePack->atAlbedoTextureViews[i]);
            gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptProbePack->atNormalTextureViews[i]);
            gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptProbePack->atAOMetalRoughnessTextureViews[i]);
            gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptProbePack->atDepthTextureViews[i]);
            gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, ptProbePack->atLightingBindGroup[i]);
        }
    }
    for(uint32_t j = 0; j < pl_sb_size(ptScene->sbtProbeData); j++)
    {
        plEnvironmentProbeData* ptProbe = &ptScene->sbtProbeData[j];
            
        gptStarter->return_staging_buffer(&ptProbe->tDShadowCameraBuffers);
        gptStarter->return_staging_buffer(&ptProbe->tDLightShadowDataBuffer);

        gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptProbe->tLambertianEnvTexture);
        gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptProbe->tGGXEnvTexture);

        if(ptScene->tInternalFlags & PL_SCENE_INTERNAL_FLAG_SHEEN_REQUIRED)
            gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptProbe->tSheenEnvTexture);

        pl_sb_free(ptProbe->sbtDLightShadowData);

        for(uint32_t i = 0; i < 6; i++)
        {
            gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptProbe->atRawOutputTextureViews[i]);
            
        }

        gptStarter->return_staging_buffer(&ptProbe->tViewBuffer);

        gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, ptProbe->tViewBG);
        gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, ptProbe->tGBufferBG);

    }

    pl_sb_free(ptScene->sbuVisibleDeferredEntities);
    pl_sb_free(ptScene->sbuVisibleForwardEntities);
    pl_sb_free(ptScene->sbuVisibleTransmissionEntities);
    pl_sb_free(ptScene->sbtVisibleDrawables);

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        gptStarter->return_staging_buffer(&ptScene->atSceneBuffer[i]);
        gptStarter->return_staging_buffer(&ptScene->atPointLightBuffer[i]);
        gptStarter->return_staging_buffer(&ptScene->atSpotLightBuffer[i]);
        gptStarter->return_staging_buffer(&ptScene->atDynamicSkinBuffer[i]);
        gptStarter->return_staging_buffer(&ptScene->atTransformBuffer[i]);
        gptStarter->return_staging_buffer(&ptScene->atInstanceBuffer[i]);
        gptStarter->return_staging_buffer(&ptScene->atPointLightShadowDataBuffer[i]);
        gptStarter->return_staging_buffer(&ptScene->atSpotLightShadowDataBuffer[i]);
        gptStarter->return_staging_buffer(&ptScene->atShadowCameraBuffers[i]);
        gptStarter->return_staging_buffer(&ptScene->atDirectionLightBuffer[i]);
        gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, ptScene->atSceneBindGroups[i]);
    }
    gptStarter->return_staging_buffer(&ptScene->tGPUProbeDataBuffers);

    gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptScene->tMaterialDataBuffer);
    
    for(uint32_t i = 0; i < 7; i++)
    {
        gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptScene->atFilterWorkingBuffers[i]);
    }

    gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptScene->tVertexBuffer);
    gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptScene->tIndexBuffer);
    gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptScene->tStorageBuffer);

    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptScene->tShadowTexture);
    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptScene->tSunShadowTexture);

    if(ptScene->tSkyboxTexture.uIndex != 0)
    {
        gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptScene->tSkyboxTexture);
        gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, ptScene->tSkyboxBindGroup);
    }

    gptResource->clear();

    gptBvh->cleanup(&ptScene->tBvh);
    pl_sb_free(ptScene->sbtVisibleDrawables0);
    pl_sb_free(ptScene->sbtVisibleDrawables1);
    pl_sb_free(ptScene->sbtRegularShaders);
    pl_sb_free(ptScene->sbtProbeDataPacks);
    pl_sb_free(ptScene->sbtShadowShaders);
    pl_sb_free(ptScene->sbtProbeShaders);
    pl_sb_free(ptScene->sbtOutlineShaders);
    pl_sb_free(ptScene->sbtBvhAABBs);
    pl_sb_free(ptScene->sbtNodeStack);
    pl_sb_free(ptScene->sbtGPUProbeData);
    pl_sb_free(ptScene->sbtProbeData);
    pl_sb_free(ptScene->sbtShadowRects);
    pl_sb_free(ptScene->sbtShadowRectData);
    pl_sb_free(ptScene->sbtShadowViewRects);
    pl_sb_free(ptScene->sbtPointLights);
    pl_sb_free(ptScene->sbtSpotLights);
    pl_sb_free(ptScene->sbtDirectionLights);
    pl_sb_free(ptScene->sbtPointLightShadowData);
    pl_sb_free(ptScene->sbtSpotLightShadowData);
    pl_sb_free(ptScene->sbtPointLightData);
    pl_sb_free(ptScene->sbtSpotLightData);
    pl_sb_free(ptScene->sbtDirectionLightData);
    pl_sb_free(ptScene->sbtVertexPosBuffer);
    pl_sb_free(ptScene->sbtVertexDataBuffer);
    pl_sb_free(ptScene->sbuIndexBuffer);
    pl_sb_free(ptScene->sbtMaterialNodes)
    pl_sb_free(ptScene->sbtDrawables);
    pl_sb_free(ptScene->sbtDrawableResources);
    pl_sb_free(ptScene->sbtSkinData);
    pl_sb_free(ptScene->sbtSkinVertexDataBuffer);
    pl_sb_free(ptScene->sbtOutlinedEntities);
    pl_hm_free(&ptScene->tDrawableHashmap);
    pl_hm_free(&ptScene->tMaterialHashmap);
    pl_hm_free(&ptScene->tTextureIndexHashmap);
    pl_hm_free(&ptScene->tCubeTextureIndexHashmap);


    gptFreeList->cleanup(&ptScene->tMaterialFreeList);
    gptFreeList->cleanup(&ptScene->tIndexBufferFreeList);
    gptFreeList->cleanup(&ptScene->tVertexBufferFreeList);
    gptFreeList->cleanup(&ptScene->tStorageBufferFreeList);
    gptFreeList->cleanup(&ptScene->tSkinBufferFreeList);
    gptFreeList->cleanup(&ptScene->tShadowCameraFreeList);

    pl_sb_free(ptScene->sbptViews);

    for(uint32_t i = 0; i < pl_sb_size(gptData->sbptScenes); i++)
    {
        if(gptData->sbptScenes[i] == ptScene)
        {
            pl_sb_del(gptData->sbptScenes, i);
            break;
        }
    }

    PL_FREE(ptScene);
    ptScene = NULL;
}

plView*
pl_renderer_create_view(plRenderScene* ptScene, const plViewDesc* ptDesc)
{

    plView* ptView = PL_ALLOC(sizeof(plView));
    memset(ptView, 0, sizeof(plView));

    ptView->uIndex = pl_sb_size(ptScene->sbptViews);
    pl_sb_push(ptScene->sbptViews, ptView);

    // default editor options
    ptView->tEditorOptions.uOutlineWidth = 4;
    ptView->tEditorOptions.bShowSelectedBoundingBox = true;
    ptView->tEditorOptions.fGridCellSize = 0.025f;
    ptView->tEditorOptions.fGridMinPixelsBetweenCells = 2.0f;
    ptView->tEditorOptions.tGridColorThin = (plVec4){0.5f, 0.5f, 0.5f, 1.0f};
    ptView->tEditorOptions.tGridColorThick = (plVec4){0.75f, 0.75f, 0.75f, 1.0f};

    ptView->ptParentScene = ptScene;
    ptView->tTargetSize.x = (float)ptDesc->uWidth;
    ptView->tTargetSize.y = (float)ptDesc->uHeight;
    ptView->tViewData.tViewportSize.xy = (plVec2){(float)ptDesc->uWidth, (float)ptDesc->uHeight};

    // picking defaults
    ptView->tHoveredEntity.uData = 0;
    ptView->bRequestHoverCheck = false;

    pl__renderer_view_create_textures(ptView);
    pl__renderer_view_create_buffers(ptView);
    pl__renderer_view_create_bindgroups(ptView);
    pl__renderer_view_update_bindgroups(ptView);

    // register debug 3D drawlist
    ptView->pt3DDrawList = gptDraw->request_3d_drawlist();
    ptView->pt3DGizmoDrawList = gptDraw->request_3d_drawlist();
    ptView->pt3DSelectionDrawList = gptDraw->request_3d_drawlist();
    
    return ptView;
}

void
pl_renderer_resize_view(plView* ptView, plVec2 tDimensions)
{

    plTexture* ptTexture = gptGfx->get_texture(gptData->ptDevice, ptView->tFinalTexture);

    // update offscreen size to match viewport
    ptView->tTargetSize = tDimensions;
    ptView->tViewData.tViewportSize.xy = ptView->tTargetSize;

    for(uint32_t i = 0; i < pl_sb_size(ptView->sbtBloomDownChain); i++)
    {
        gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->sbtBloomDownChain[i]);
        gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->sbtBloomUpChain[i]);
    }
    pl_sb_reset(ptView->sbtBloomDownChain);
    pl_sb_reset(ptView->sbtBloomUpChain);
    
    // for convience
    plDevice* ptDevice = gptData->ptDevice;

    if(tDimensions.x > ptTexture->tDesc.tDimensions.x || tDimensions.y > ptTexture->tDesc.tDimensions.y)
    {
        pl__renderer_view_create_textures(ptView);
        ptView->tFinalTextureHandle = gptDraw->create_bind_group_for_texture(ptView->tFinalTexture);
        pl__renderer_view_update_bindgroups(ptView);
    }
}

void
pl_renderer_cleanup(void)
{
    pl_temp_allocator_free(&gptData->tTempAllocator);
    gptGfx->cleanup_draw_stream(&gptData->tDrawStream);

    pl_sb_free(gptData->sbptScenes);
    gptShaderVariant->unload_manifest("/shaders/shaders.pls");
    gptStage->cleanup();
    gptGfx->flush_device(gptData->ptDevice);

    gptGfx->cleanup_bind_group_pool(gptData->ptBindGroupPool);
    gptGpuAllocators->cleanup(gptData->ptDevice);
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        gptGfx->cleanup_bind_group_pool(gptData->aptTempGroupPools[i]);
    }

    PL_FREE(gptData);
}

void
pl_renderer_editor_outline_entities(plRenderScene* ptScene, uint32_t uCount, const plEntity* atEntities)
{
    // for convience
    plDevice* ptDevice = gptData->ptDevice;

    // reset old entities
    const uint32_t uOldSelectedEntityCount = pl_sb_size(ptScene->sbtOutlinedEntities);
    for(uint32_t i = 0; i < uOldSelectedEntityCount; i++)
    {
        plEntity tEntity = ptScene->sbtOutlinedEntities[i];
        plObjectComponent* ptObject = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tEntity);
        plMesh* ptMainMesh = gptAsset->get_data(ptObject->tMesh);

        for(uint32_t j = 0; j < ptMainMesh->uSubmeshCount; j++)
        {

            uint64_t uDrawHash = pl_hm_hash(&tEntity.uData, sizeof(uint64_t), j);
            uint64_t ulIndex = 0;
            if(pl_hm_has_key_ex(&ptScene->tDrawableHashmap, uDrawHash, &ulIndex))
            {
                plDrawable* ptDrawable = &ptScene->sbtDrawables[ulIndex];
                plShaderHandle tRegularShader = ptScene->sbtOutlineShaders[ulIndex];
                plShaderHandle tOutlineShader = ptScene->sbtRegularShaders[ulIndex];

                ptScene->sbtRegularShaders[ulIndex] = tRegularShader;
                ptScene->sbtOutlineShaders[ulIndex] = tOutlineShader;

                // if instanced, find parent (draw is never actually called on instanced children)
                if(ptDrawable->uInstanceCount == 0)
                {
                    while(true)
                    {
                        ulIndex--;
                        plDrawable* ptParentDrawable = &ptScene->sbtDrawables[ulIndex];
                        if(ptParentDrawable->uInstanceCount == 0)
                            ptParentDrawable = &ptScene->sbtDrawables[ulIndex];
                        else
                        {
                            ptScene->sbtRegularShaders[ulIndex] = tRegularShader;
                            ptScene->sbtOutlineShaders[ulIndex] = tOutlineShader;
                            break;
                        }
                    }
                }
            }
        }
    }
    pl_sb_reset(ptScene->sbtOutlinedEntities);

    for(uint32_t i = 0; i < uCount; i++)
    {
        plEntity tEntity = atEntities[i];

        plObjectComponent* ptObject   = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tEntity);
        if(ptObject == NULL)
            continue;

        plMesh* ptMainMesh = gptAsset->get_data(ptObject->tMesh);
        // plMaterial* ptMaterial = gptAsset->get_material(ptMesh->tMaterial);

        ptObject->tFlags |= PL_OBJECT_FLAGS_OUTLINE;


        for(uint32_t j = 0; j < ptMainMesh->uSubmeshCount; j++)
        {

            uint64_t uDrawHash = pl_hm_hash(&tEntity.uData, sizeof(uint64_t), j);
            uint64_t ulIndex = 0;
            if(pl_hm_has_key_ex(&ptScene->tDrawableHashmap, uDrawHash, &ulIndex))
            {
                plDrawable* ptDrawable = &ptScene->sbtDrawables[ulIndex];
                plShader* ptOldShader = gptGfx->get_shader(ptDevice, ptScene->sbtRegularShaders[ulIndex]);


                pl_sb_push(ptScene->sbtOutlinedEntities, ptDrawable->tEntity);

                plShaderHandle tRegularShader = ptScene->sbtRegularShaders[ulIndex];
                plShaderHandle tOutlineShader = ptScene->sbtOutlineShaders[ulIndex];

                ptScene->sbtRegularShaders[ulIndex] = tOutlineShader;
                ptScene->sbtOutlineShaders[ulIndex] = tRegularShader;

                if(ptDrawable->uInstanceCount == 0)
                {
                    while(true)
                    {
                        ulIndex--;
                        plDrawable* ptParentDrawable = &ptScene->sbtDrawables[ulIndex];
                        if(ptParentDrawable->uInstanceCount == 0)
                            ptParentDrawable = &ptScene->sbtDrawables[ulIndex];
                        else
                        {
                            ptScene->sbtRegularShaders[ulIndex] = tOutlineShader;
                            ptScene->sbtOutlineShaders[ulIndex] = tRegularShader;
                            break;
                        }
                    }
                }
            }
        }
    }
}

void
pl_renderer_editor_reload_scene_shaders(plRenderScene* ptScene)
{

    if(ptScene == NULL)
        return;

    // fill CPU buffers & drawable list
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, "recreate shaders");

    plRenderSettings* ptSettings = gptAsset->get_data(ptScene->tSettings);

    gptScreenLog->add_message_ex(0, 15.0, PL_COLOR_32_CYAN, 1.0f, "%s", "reloaded shaders");

    gptShaderVariant->unload_manifest("/shaders/shaders.pls");
    gptShaderVariant->load_manifest("/shaders/shaders.pls");
    gptData->tViewBGLayout = gptShaderVariant->get_bind_group_layout("view");
    gptData->tShadowGlobalBGLayout = gptShaderVariant->get_bind_group_layout("shadow");

    if(!(ptScene->tInternalFlags & PL_SCENE_INTERNAL_FLAG_ACTIVE))
    {
        PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
        return;
    }

    plDevice* ptDevice = gptData->ptDevice;

    PL_LOG_INFO_API_F(gptLog, gptData->uLogChannel, "reload shaders for scene %s", ptScene->pcName);

    plShaderOptions tOriginalOptions = *gptShader->get_options();

    plShaderOptions tNewDefaultShaderOptions = {
        .apcIncludeDirectories = {
            "../shaders/"
        },
        .apcDirectories = {
            "../shaders/"
        },
        .eFlags = PL_SHADER_FLAGS_AUTO_OUTPUT | PL_SHADER_FLAGS_INCLUDE_DEBUG | PL_SHADER_FLAGS_ALWAYS_COMPILE

    };
    gptShader->set_options(&tNewDefaultShaderOptions);

    pl_sb_reset(ptScene->sbtOutlinedEntities);

    int iSceneWideRenderingFlags = PL_RENDERING_FLAG_SHADOWS;
    if(ptSettings->tLighting.tFlags & PL_RENDERER_LIGHTING_FLAGS_NO_SHADOWS)      iSceneWideRenderingFlags &= ~PL_RENDERING_FLAG_SHADOWS;
    if(ptSettings->tLighting.tFlags & PL_RENDERER_LIGHTING_FLAGS_IMAGE_BASED)     iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_IBL;
    if(ptSettings->tLighting.tFlags & PL_RENDERER_LIGHTING_FLAGS_NORMAL_MAPPING)  iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_NORMAL_MAPS;
    if(ptSettings->tLighting.tFlags & PL_RENDERER_LIGHTING_FLAGS_PUNCTUAL_LIGHTS) iSceneWideRenderingFlags |= PL_RENDERING_FLAG_PUNCTUAL;
    if(ptSettings->tShadows.tFlags & PL_RENDERER_SHADOW_FLAGS_PCF)                    iSceneWideRenderingFlags |= PL_RENDERING_FLAG_PCF_SHADOWS;
        
    int aiLightingConstantData[] = {iSceneWideRenderingFlags, ptScene->tDebugOptions.tShaderDebugMode};
    
    if(ptScene->tDebugOptions.tShaderDebugMode)
        ptScene->tDirectionalLightingShader = gptShaderVariant->get_shader("deferred_lighting_debug", NULL, NULL, aiLightingConstantData, &gptData->tDeferredLightingRenderPassLayout);
    else
    {
        ptScene->tSunShader = gptShaderVariant->get_shader("deferred_lighting_sun", NULL, NULL, aiLightingConstantData, &gptData->tDeferredLightingRenderPassLayout);
        ptScene->tDirectionalLightingShader = gptShaderVariant->get_shader("deferred_lighting_directional", NULL, NULL, aiLightingConstantData, &gptData->tDeferredLightingRenderPassLayout);
    }

    ptScene->tSpotLightingShader = gptShaderVariant->get_shader("deferred_lighting_spot", NULL, NULL, aiLightingConstantData, &gptData->tDeferredLightingRenderPassLayout);
    ptScene->tPointLightingShader = gptShaderVariant->get_shader("deferred_lighting_point", NULL, NULL, aiLightingConstantData, &gptData->tDeferredLightingRenderPassLayout);
    ptScene->tProbeLightingShader = gptShaderVariant->get_shader("deferred_lighting", NULL, NULL, NULL, &gptData->tDeferredLightingRenderPassLayout);
    ptScene->tTerrainShader = gptShaderVariant->get_shader("terrain", NULL, NULL, NULL, &gptData->tRenderPassLayout);
    ptScene->tTerrainShadowShader = gptShaderVariant->get_shader("terrain_shadow", NULL, NULL, NULL, &gptData->tDepthRenderPassLayout);
    ptScene->tSunTransmissionLutShader = gptShaderVariant->get_compute_shader("sky_transmission_lut", NULL);
    ptScene->tSunMultiscatterShader = gptShaderVariant->get_compute_shader("sky_multiscatter_lut", NULL);
    ptScene->tSkyViewLutShader = gptShaderVariant->get_compute_shader("sky_lut", NULL);
    ptScene->tSkyAerialLutShader = gptShaderVariant->get_compute_shader("sky_aerial_lut", NULL);
    ptScene->tSkyAerialShader = gptShaderVariant->get_compute_shader("sky_aerial", NULL);
    ptScene->tSkyShader = gptShaderVariant->get_shader("sky", NULL, NULL, NULL, &gptData->tTransparentRenderPassLayout);

    plGraphicsState tTerrainVariantTemp = {
        .bDepthWriteEnabled  = 1,
        .eDepthMode          = PL_COMPARE_MODE_GREATER,
        .eCullMode           = PL_CULL_MODE_NONE,
        .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
        .uStencilRef         = 0xff,
        .uStencilMask        = 0xff,
        .eStencilOpFail      = PL_STENCIL_OP_KEEP,
        .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
        .eStencilOpPass      = PL_STENCIL_OP_KEEP,
        .bWireframe          = 1
    };
    ptScene->tTerrainWireframeShader = gptShaderVariant->get_shader("terrain", &tTerrainVariantTemp, NULL, NULL, &gptData->tRenderPassLayout);
    

    gptShader->set_options(&tOriginalOptions);

    const uint32_t uDrawableCount = pl_sb_size(ptScene->sbtDrawables);
    for (uint32_t uDrawableIndex = 0; uDrawableIndex < uDrawableCount; uDrawableIndex++)
    {
        plObjectComponent*           ptObject    = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtDrawables[uDrawableIndex].tEntity);
        plMesh*                      ptMainMesh   = gptAsset->get_data(ptObject->tMesh);
        plSubmesh* ptMesh = &ptMainMesh->atSubmeshes[ptScene->sbtDrawables[uDrawableIndex].uSubmeshIndex];
        plMaterial*                  ptMaterial  = gptAsset->get_data(ptMesh->tMaterial);
        plEnvironmentProbeComponent* ptProbeComp = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, ptScene->sbtDrawables[uDrawableIndex].tEntity);
        
        int iDataStride = 0;
        int iFlagCopy0 = (int)ptMesh->uVertexStreamMask;
        while(iFlagCopy0)
        {
            iDataStride += iFlagCopy0 & 1;
            iFlagCopy0 >>= 1;
        }

        int iTextureMappingFlags = 0;
        for(uint32_t j = 0; j < PL_MATERIAL_TEXTURE_SLOT_COUNT; j++)
        {
            if(gptAsset->is_valid(ptMaterial->atTextures[j].tTexture))
                iTextureMappingFlags |= 1 << j; 
        }

        int iObjectRenderingFlags = iSceneWideRenderingFlags;

        if(ptObject->tFlags & PL_OBJECT_FLAGS_RECEIVE_SHADOW)
        {
            iObjectRenderingFlags |= PL_RENDERING_FLAG_SHADOWS;
        }

        // choose shader variant
        int aiForwardFragmentConstantData0[] = {
            (int)ptMesh->uVertexStreamMask,
            iTextureMappingFlags,
            ptMaterial->eFlags,
            iObjectRenderingFlags,
            ptScene->tDebugOptions.tShaderDebugMode // debug only
        };

        int aiGBufferFragmentConstantData0[] = {
            (int)ptMesh->uVertexStreamMask,
            iTextureMappingFlags,
            ptMaterial->eFlags,
            ptScene->tDebugOptions.tShaderDebugMode,
            iObjectRenderingFlags
        };

        int aiVertexConstantData0[] = {
            (int)ptMesh->uVertexStreamMask,
            iDataStride
        };

        if(ptScene->sbtDrawables[uDrawableIndex].tFlags & PL_DRAWABLE_FLAG_DEFERRED)
        {
            plGraphicsState tVariantTemp = {
                .bDepthWriteEnabled  = 1,
                .eDepthMode          = PL_COMPARE_MODE_GREATER,
                .eCullMode           = PL_CULL_MODE_CULL_BACK,
                .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .uStencilRef         = 0xff,
                .uStencilMask        = 0xff,
                .eStencilOpFail      = PL_STENCIL_OP_KEEP,
                .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .eStencilOpPass      = PL_STENCIL_OP_KEEP,
                .bWireframe          = ptScene->tDebugOptions.bWireframe
            };

            if(ptMaterial->eFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED)
                tVariantTemp.eCullMode = PL_CULL_MODE_NONE;

            if(ptScene->tDebugOptions.tShaderDebugMode == PL_SHADER_DEBUG_MODE_NONE)
            {
                aiGBufferFragmentConstantData0[3] = iObjectRenderingFlags;
                ptScene->sbtRegularShaders[uDrawableIndex] = gptShaderVariant->get_shader("gbuffer_fill", &tVariantTemp, aiVertexConstantData0, aiGBufferFragmentConstantData0, &gptData->tDeferredLightingRenderPassLayout);
                ptScene->sbtProbeShaders[uDrawableIndex] = gptShaderVariant->get_shader("gbuffer_fill", &tVariantTemp, aiVertexConstantData0, aiGBufferFragmentConstantData0, NULL);
            }
            else
            {
                ptScene->sbtRegularShaders[uDrawableIndex] = gptShaderVariant->get_shader("gbuffer_fill_debug", &tVariantTemp, aiVertexConstantData0, aiGBufferFragmentConstantData0, &gptData->tDeferredLightingRenderPassLayout);
                ptScene->sbtProbeShaders[uDrawableIndex] = gptShaderVariant->get_shader("gbuffer_fill_debug", &tVariantTemp, aiVertexConstantData0, aiGBufferFragmentConstantData0, NULL);
            }

            // write into stencil buffer
            tVariantTemp.bStencilTestEnabled = 1;
            tVariantTemp.eStencilMode        = PL_COMPARE_MODE_ALWAYS;
            tVariantTemp.uStencilRef         = 0xff;
            tVariantTemp.uStencilMask        = 0xff;
            tVariantTemp.eStencilOpFail      = PL_STENCIL_OP_REPLACE;
            tVariantTemp.eStencilOpDepthFail = PL_STENCIL_OP_REPLACE;
            tVariantTemp.eStencilOpPass      = PL_STENCIL_OP_REPLACE;
            aiGBufferFragmentConstantData0[3] = iObjectRenderingFlags;
            ptScene->sbtOutlineShaders[uDrawableIndex] = gptShaderVariant->get_shader("gbuffer_fill", &tVariantTemp, aiVertexConstantData0, aiGBufferFragmentConstantData0, &gptData->tDeferredLightingRenderPassLayout);
        }
        else if(ptScene->sbtDrawables[uDrawableIndex].tFlags & PL_DRAWABLE_FLAG_FORWARD)
        {

            plGraphicsState tVariantTemp = {
                .bDepthWriteEnabled  = 1,
                .eDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
                .eCullMode           = PL_CULL_MODE_NONE,
                .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .uStencilRef         = 0xff,
                .uStencilMask        = 0xff,
                .eStencilOpFail      = PL_STENCIL_OP_KEEP,
                .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .eStencilOpPass      = PL_STENCIL_OP_KEEP,
                .bWireframe          = ptScene->tDebugOptions.bWireframe
            };

            if(ptMaterial->eFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED)
                tVariantTemp.eCullMode = PL_CULL_MODE_NONE;

            if(ptScene->tDebugOptions.tShaderDebugMode == PL_SHADER_DEBUG_MODE_NONE)
            {
                ptScene->sbtRegularShaders[uDrawableIndex] = gptShaderVariant->get_shader("forward", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, &gptData->tTransparentRenderPassLayout);
                aiForwardFragmentConstantData0[3] = ptSettings->tLighting.tFlags & PL_RENDERER_LIGHTING_FLAGS_PUNCTUAL_LIGHTS ? PL_RENDERING_FLAG_SHADOWS : 0; // remove ibl
                ptScene->sbtProbeShaders[uDrawableIndex] = gptShaderVariant->get_shader("forward", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, &gptData->tTransparentRenderPassLayout);
            }
            else
            {
                ptScene->sbtRegularShaders[uDrawableIndex] = gptShaderVariant->get_shader("forward_debug", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, &gptData->tTransparentRenderPassLayout);
                aiForwardFragmentConstantData0[3] = ptSettings->tLighting.tFlags & PL_RENDERER_LIGHTING_FLAGS_PUNCTUAL_LIGHTS ? PL_RENDERING_FLAG_SHADOWS : 0; // remove ibl
                ptScene->sbtProbeShaders[uDrawableIndex] = gptShaderVariant->get_shader("forward_debug", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, &gptData->tTransparentRenderPassLayout);
            }

            // write into stencil buffer
            tVariantTemp.bStencilTestEnabled = 1;
            tVariantTemp.eStencilMode        = PL_COMPARE_MODE_ALWAYS;
            tVariantTemp.uStencilRef         = 0xff;
            tVariantTemp.uStencilMask        = 0xff;
            tVariantTemp.eStencilOpFail      = PL_STENCIL_OP_REPLACE;
            tVariantTemp.eStencilOpDepthFail = PL_STENCIL_OP_REPLACE;
            tVariantTemp.eStencilOpPass      = PL_STENCIL_OP_REPLACE;
            aiForwardFragmentConstantData0[3] = iObjectRenderingFlags; // normal
            ptScene->sbtOutlineShaders[uDrawableIndex] = gptShaderVariant->get_shader("forward", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, &gptData->tTransparentRenderPassLayout);

        }

        else if(ptScene->sbtDrawables[uDrawableIndex].tFlags & PL_DRAWABLE_FLAG_TRANSMISSION)
        {

            plGraphicsState tVariantTemp = {
                .bDepthWriteEnabled  = 1,
                .eDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
                .eCullMode           = PL_CULL_MODE_NONE,
                .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .uStencilRef         = 0xff,
                .uStencilMask        = 0xff,
                .eStencilOpFail      = PL_STENCIL_OP_KEEP,
                .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .eStencilOpPass      = PL_STENCIL_OP_KEEP,
                .bWireframe          = ptScene->tDebugOptions.bWireframe
            };

            if(ptMaterial->eFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED)
                tVariantTemp.eCullMode = PL_CULL_MODE_NONE;

            if(ptScene->tDebugOptions.tShaderDebugMode == PL_SHADER_DEBUG_MODE_NONE)
            {
                ptScene->sbtRegularShaders[uDrawableIndex] = gptShaderVariant->get_shader("transmission", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, &gptData->tTransparentRenderPassLayout);
                aiForwardFragmentConstantData0[3] = ptSettings->tLighting.tFlags & PL_RENDERER_LIGHTING_FLAGS_PUNCTUAL_LIGHTS ? PL_RENDERING_FLAG_SHADOWS : 0; // remove ibl
                ptScene->sbtProbeShaders[uDrawableIndex] = gptShaderVariant->get_shader("forward", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, &gptData->tTransparentRenderPassLayout);
            }
            else
            {
                aiForwardFragmentConstantData0[4] = ptScene->tDebugOptions.tShaderDebugMode;
                ptScene->sbtRegularShaders[uDrawableIndex] = gptShaderVariant->get_shader("forward_debug", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, &gptData->tTransparentRenderPassLayout);
                aiForwardFragmentConstantData0[3] = ptSettings->tLighting.tFlags & PL_RENDERER_LIGHTING_FLAGS_PUNCTUAL_LIGHTS? PL_RENDERING_FLAG_SHADOWS : 0; // remove ibl
                ptScene->sbtProbeShaders[uDrawableIndex] = gptShaderVariant->get_shader("forward_debug", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, &gptData->tTransparentRenderPassLayout);
            }
            
            // write into stencil buffer
            tVariantTemp.bStencilTestEnabled = 1;
            tVariantTemp.eStencilMode        = PL_COMPARE_MODE_ALWAYS;
            tVariantTemp.uStencilRef         = 0xff;
            tVariantTemp.uStencilMask        = 0xff;
            tVariantTemp.eStencilOpFail      = PL_STENCIL_OP_REPLACE;
            tVariantTemp.eStencilOpDepthFail = PL_STENCIL_OP_REPLACE;
            tVariantTemp.eStencilOpPass      = PL_STENCIL_OP_REPLACE;
            aiForwardFragmentConstantData0[3] = iObjectRenderingFlags; // normal
            ptScene->sbtOutlineShaders[uDrawableIndex] = gptShaderVariant->get_shader("transmission", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, &gptData->tTransparentRenderPassLayout);
        }

        if(ptMaterial->eAlphaMode != PL_MATERIAL_ALPHA_MODE_OPAQUE)
        {
            plGraphicsState tShadowVariant = {
                .bDepthWriteEnabled  = 1,
                .eDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
                .eCullMode           = PL_CULL_MODE_NONE,
                .bWireframe          = 0,
                .bDepthClampEnabled  = 1,
                .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .uStencilRef         = 0xff,
                .uStencilMask        = 0xff,
                .eStencilOpFail      = PL_STENCIL_OP_KEEP,
                .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .eStencilOpPass      = PL_STENCIL_OP_KEEP
            };
            ptScene->sbtShadowShaders[uDrawableIndex] = gptShaderVariant->get_shader("alphashadow", &tShadowVariant, aiVertexConstantData0, &aiForwardFragmentConstantData0[1], &gptData->tDepthRenderPassLayout);
        }

        if(!gptEcs->has_component(ptScene->ptComponentLibrary, gptSkeleton->get_ecs_type_key_skin(), ptScene->sbtDrawables[uDrawableIndex].tEntity))
            continue;

        // stride within storage buffer
        uint32_t uStride = 0;

        uint64_t uVertexStreamMask = 0;

        // calculate vertex stream mask based on provided data
        if(ptMesh->ptVertexPositions)  { uStride += 1; uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_POSITION; }
        if(ptMesh->ptVertexNormals)    { uStride += 1; uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_NORMAL; }
        if(ptMesh->ptVertexTangents)   { uStride += 1; uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TANGENT; }
        if(ptMesh->ptVertexWeights[0]) { uStride += 1; uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0; }
        if(ptMesh->ptVertexWeights[1]) { uStride += 1; uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1; }
        if(ptMesh->ptVertexJoints[0])  { uStride += 1; uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_JOINTS_0; }
        if(ptMesh->ptVertexJoints[1])  { uStride += 1; uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_JOINTS_1; }

        // stride within storage buffer
        uint32_t uDestStride = 0;

        // calculate vertex stream mask based on provided data
        if(ptMesh->ptVertexNormals)               { uDestStride += 1; }
        if(ptMesh->ptVertexTangents)              { uDestStride += 1; }
        if(ptMesh->ptVertexColors[0])             { uDestStride += 1; }
        if(ptMesh->ptVertexColors[1])             { uDestStride += 1; }
        if(ptMesh->ptVertexTextureCoordinates[0]) { uDestStride += 1; }

        int aiSpecializationData[] = {(int)uVertexStreamMask, (int)uStride, (int)ptMesh->uVertexStreamMask, (int)uDestStride};
        ptScene->sbtSkinData[ptScene->sbtDrawables[uDrawableIndex].uSkinIndex].tShader = gptShaderVariant->get_compute_shader("skinning", aiSpecializationData); 
    }


    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

void
pl_renderer_ecs_add_probes_to_scene(plRenderScene* ptScene, uint32_t uCount, const plEntity* atProbes)
{
    ptScene->uLastProbeAddFrame = gptIO->ulFrameCount;
    for(uint32_t i = 0; i < uCount; i++)
    {
        plEnvironmentProbeComponent* ptProbe = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, atProbes[i]);

        plEnvironmentProbeData tProbeData = {
            .tEntity = atProbes[i],
            .tTargetSize = {(float)ptProbe->uResolution, (float)ptProbe->uResolution}
        };

        pl__renderer_probe_create_textures(ptScene, &tProbeData);
        pl__renderer_probe_create_buffers(ptScene, &tProbeData);
        pl__renderer_probe_create_bindgroups(ptScene, &tProbeData);
        pl__renderer_probe_update_bindgroups(ptScene, &tProbeData);
        tProbeData.uDataPackIndex = pl__renderer_probe_data_pack_index(ptScene, ptProbe->uResolution);
        tProbeData.iMips = (int)(uint32_t)floorf(log2f((float)ptProbe->uResolution)) - 3; // guarantee final dispatch during filtering is 16 threads
        
        plObjectComponent* ptProbeObj = gptEcs->add_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, atProbes[i]);
        ptProbeObj->tMesh = ptScene->tProbeMesh;
        ptProbeObj->tTransform = atProbes[i];
        ptProbeObj->tFlags |= PL_OBJECT_FLAGS_RECEIVE_SHADOW;
        ptProbeObj->tFlags &= ~PL_OBJECT_FLAGS_CAST_SHADOW;
        ptProbeObj->uSubmeshCount = 1;
        pl_sb_push(ptScene->sbtProbeData, tProbeData);
    }
    pl_renderer_ecs_add_drawable_objects_to_scene(ptScene, uCount, atProbes);
}

void
pl_renderer_ecs_add_terrain_to_scene(plRenderScene* ptScene, plEntity tTerrainEntity)
{
    plCommandBuffer* ptCmdBuffer = gptStarter->get_temporary_command_buffer();

    plTerrainComponent* ptTerrainComp = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tTerrainComponentType, tTerrainEntity);
    plTerrainAsset* ptTerrain = gptAsset->get_data(ptTerrainComp->tTerrain);
    ptScene->ptTerrain = pl__renderer_terrain_create(ptCmdBuffer, ptTerrain);
    gptStarter->submit_temporary_command_buffer(ptCmdBuffer);
}

void
pl_renderer_ecs_add_lights_to_scene(plRenderScene* ptScene, uint32_t uCount, const plEntity* atLights)
{
    for(uint32_t i = 0; i < uCount; i++)
    {
        plRendererLight tRendererLight = {
            .tEntity = atLights[i]
        };

        plLightComponent* ptLight = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, atLights[i]);

        if(ptLight->tType == PL_LIGHT_TYPE_POINT)
        {  
            if(ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW)
            {
                tRendererLight.uShadowBufferOffset = (uint32_t)gptFreeList->get_node(&ptScene->tShadowCameraFreeList, sizeof(plMat4) * 6)->uOffset;
            }
            pl_sb_push(ptScene->sbtPointLights, tRendererLight);
        }
        else if(ptLight->tType == PL_LIGHT_TYPE_SPOT)
        {  
            if(ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW)
            {
                tRendererLight.uShadowBufferOffset = (uint32_t)gptFreeList->get_node(&ptScene->tShadowCameraFreeList, sizeof(plMat4))->uOffset;
            }
            pl_sb_push(ptScene->sbtSpotLights, tRendererLight);
        }
        else
        {
            pl_sb_push(ptScene->sbtDirectionLights, tRendererLight);
        }
    }
    
}

void
pl_renderer_editor_update_hovered_entity(plView* ptView, plVec2 tOffset, plVec2 tWindowScale)
{
    ptView->bRequestHoverCheck = true;
    ptView->tHoverOffset = tOffset;
    ptView->tHoverWindowRatio = tWindowScale;
}

bool
pl_renderer_editor_get_hovered_entity(plView* ptView, plEntity* ptEntityOut)
{
    if(ptEntityOut)
        *ptEntityOut = ptView->tHoveredEntity;
    bool bNewValue = ptView->auHoverResultReady[gptGfx->get_current_frame_index()];
    ptView->auHoverResultReady[gptGfx->get_current_frame_index()] = false;
    return bNewValue;
}

void
pl_renderer_prepare_scene(plRenderScene* ptScene, const plCamera** atCameras, uint32_t uCameraCount)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    if(ptScene->ptTerrain)
        pl_prepare_terrain(ptScene->ptTerrain);

    // for convience
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    plDevice*      ptDevice  = gptData->ptDevice;
    plRenderEnvironment* ptEnvironment = gptAsset->get_data(ptScene->tEnvironment);
    plRenderSettings* ptSettings = gptAsset->get_data(ptScene->tSettings);

    // update light CPU side light buffers
    //-----------------------------------------------------------------------------

    const uint32_t uPointLightCount = pl_sb_size(ptScene->sbtPointLights);
    pl_sb_reset(ptScene->sbtPointLightData);
    pl_sb_reset(ptScene->sbtPointLightShadowData);
    for(uint32_t i = 0; i < uPointLightCount; i++)
    {
        plLightComponent* ptLight = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, ptScene->sbtPointLights[i].tEntity);

        const plGpuPointLight tLight = {
            .fIntensity   = ptLight->fIntensity,
            .fRange       = ptLight->fRange,
            .tPosition    = ptLight->tPosition,
            .tColor       = ptLight->tColor,
            .iShadowIndex = (int)pl_sb_size(ptScene->sbtPointLightShadowData),
            .iCastShadow  = ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW
        };
        pl_sb_push(ptScene->sbtPointLightData, tLight);

        if(ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW)
        {
            pl_sb_add(ptScene->sbtPointLightShadowData);
        }
    }

    const uint32_t uSpotLightCount = pl_sb_size(ptScene->sbtSpotLights);
    pl_sb_reset(ptScene->sbtSpotLightData);
    pl_sb_reset(ptScene->sbtSpotLightShadowData);
    for(uint32_t i = 0; i < uSpotLightCount; i++)
    {
        plLightComponent* ptLight = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, ptScene->sbtSpotLights[i].tEntity);
        const plGpuSpotLight tLight = {
            .fIntensity    = ptLight->fIntensity,
            .fRange        = ptLight->fRange,
            .tPosition     = ptLight->tPosition,
            .tDirection    = ptLight->tDirection,
            .tColor        = ptLight->tColor,
            .iShadowIndex  = (int)pl_sb_size(ptScene->sbtSpotLightShadowData),
            .iCastShadow   = ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW,
            .fInnerConeCos = cosf(ptLight->fInnerConeAngle),
            .fOuterConeCos = cosf(ptLight->fOuterConeAngle),
        };
        pl_sb_push(ptScene->sbtSpotLightData, tLight);

        if(ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW)
        {
            pl_sb_add(ptScene->sbtSpotLightShadowData);
        }
    }

    uint32_t uDOffset = 0; // directional light buffer offset (synced across views & probes)
    const uint32_t uDirectionLightCount = pl_sb_size(ptScene->sbtDirectionLights);
    pl_sb_reset(ptScene->sbtDirectionLightData);

    for(uint32_t j = 0; j < pl_sb_size(ptScene->sbptViews); j++)
    {
        pl_sb_reset(ptScene->sbptViews[j]->sbtDLightShadowData);
    }

    for(uint32_t j = 0; j < pl_sb_size(ptScene->sbtProbeData); j++)
    {
        pl_sb_reset(ptScene->sbtProbeData[j].sbtDLightShadowData);
    }

    for(uint32_t i = 0; i < uDirectionLightCount; i++)
    {
        plLightComponent* ptLight = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, ptScene->sbtDirectionLights[i].tEntity);

        if(ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW)
        {
            const plGpuDirectionLight tLight = {
                .fIntensity    = ptLight->fIntensity,
                .tDirection    = ptLight->tDirection,
                .tColor        = ptLight->tColor,
                .iShadowIndex  = (int)uDOffset++,
                .iCastShadow   = 1,
            };
            pl_sb_push(ptScene->sbtDirectionLightData, tLight);

            for(uint32_t j = 0; j < pl_sb_size(ptScene->sbptViews); j++)
            {
                pl_sb_add(ptScene->sbptViews[j]->sbtDLightShadowData);
            }

            for(uint32_t j = 0; j < pl_sb_size(ptScene->sbtProbeData); j++)
            {
                pl_sb_add_n(ptScene->sbtProbeData[j].sbtDLightShadowData, 6);
            }
        }
        else
        {
            const plGpuDirectionLight tLight = {
                .fIntensity    = ptLight->fIntensity,
                .tDirection    = ptLight->tDirection,
                .tColor        = ptLight->tColor,
                .iShadowIndex  = 0,
                .iCastShadow   = ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW
            };
            pl_sb_push(ptScene->sbtDirectionLightData, tLight);
        }
    }

    // update light GPU side light buffers
    //-----------------------------------------------------------------------------

    plBuffer* ptPointLightingBuffer = gptGfx->get_buffer(ptDevice, ptScene->atPointLightBuffer[uFrameIdx]);
    plBuffer* ptSpotLightingBuffer = gptGfx->get_buffer(ptDevice, ptScene->atSpotLightBuffer[uFrameIdx]);
    plBuffer* ptDirectionLightingBuffer = gptGfx->get_buffer(ptDevice, ptScene->atDirectionLightBuffer[uFrameIdx]);
    memcpy(ptPointLightingBuffer->tMemoryAllocation.pHostMapped, ptScene->sbtPointLightData, sizeof(plGpuPointLight) * pl_sb_size(ptScene->sbtPointLightData));
    memcpy(ptSpotLightingBuffer->tMemoryAllocation.pHostMapped, ptScene->sbtSpotLightData, sizeof(plGpuSpotLight) * pl_sb_size(ptScene->sbtSpotLightData));
    memcpy(ptDirectionLightingBuffer->tMemoryAllocation.pHostMapped, ptScene->sbtDirectionLightData, sizeof(plGpuDirectionLight) * pl_sb_size(ptScene->sbtDirectionLightData));

    // check transmission backing textures
    //-----------------------------------------------------------------------------

    // if transmission is required, ensure we have the backing textures needed
    // for the calculations
    if(ptScene->tInternalFlags & PL_SCENE_INTERNAL_FLAG_TRANSMISSION_REQUIRED)
    {
        for(uint32_t i = 0; i < pl_sb_size(ptScene->sbptViews); i++)
        {
            plView* ptView = ptScene->sbptViews[i];

            if(!gptGfx->is_texture_valid(ptDevice, ptView->tTransmissionTexture))
            {
                PL_LOG_INFO_API(gptLog, gptData->uLogChannel, "creating required transmission texture");
                const plTextureDesc tRawOutput2TextureDesc = {
                    .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
                    .eFormat       = PL_FORMAT_R16G16B16A16_FLOAT,
                    .uLayers       = 1,
                    .uMips         = 0,
                    .eType         = PL_TEXTURE_TYPE_2D,
                    .eUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT | PL_TEXTURE_USAGE_STORAGE,
                    .pcDebugName   = "offscreen transmission"
                };
                gptStarter->create_texture(&tRawOutput2TextureDesc, NULL, 0, &ptView->tTransmissionTexture);
                ptView->tViewData.iTransmissionFrameBufferIndex = pl__renderer_get_bindless_texture_index(ptScene, ptView->tTransmissionTexture);
            }
        }
    }
    else // transmission not required, free resources no longer needed
    {
        for(uint32_t i = 0; i < pl_sb_size(ptScene->sbptViews); i++)
        {
            plView* ptView = ptScene->sbptViews[i];

            if(gptGfx->is_texture_valid(ptDevice, ptView->tTransmissionTexture))
            {
                PL_LOG_INFO_API(gptLog, gptData->uLogChannel, "freeing unneeded transmission texture");
                gptGfx->queue_texture_for_deletion(ptDevice, ptView->tTransmissionTexture);
            }
        }
    }

    // transform & instance buffer updates
    //-----------------------------------------------------------------------------

    // update transform & instance buffers since we are now using indices
    plBuffer* ptTransformBuffer = gptGfx->get_buffer(ptDevice, ptScene->atTransformBuffer[uFrameIdx]);
    plBuffer* ptInstanceBuffer  = gptGfx->get_buffer(ptDevice, ptScene->atInstanceBuffer[uFrameIdx]);

    uint32_t uInstanceOffset = 0;
    const uint32_t uObjectCount = pl_sb_size(ptScene->sbtDrawables);
    const plEcsTypeKey tTransformComponentType = gptTransform->get_ecs_type_key_transform();

    if(ptSettings->tShadows.tFlags & PL_RENDERER_SHADOW_FLAGS_MULTI_VIEWPORT)
    {
        for(uint32_t i = 0; i < uObjectCount; i++)
        {

            plObjectComponent* ptObject = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtDrawables[i].tEntity);

            // copy transform into proper location in CPU side buffer
            plTransformComponent* ptTransform = gptEcs->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
            memcpy(&ptTransformBuffer->tMemoryAllocation.pHostMapped[ptScene->sbtDrawables[i].uTransformIndex * sizeof(plMat4)], &ptTransform->tWorld, sizeof(plMat4));

            // if using instancing, set index into instance buffer and
            // this includes setting the viewport instance data for multiviewport
            // shadow technique
            if(ptScene->sbtDrawables[i].uInstanceCount != 0)
            {
                ptScene->sbtDrawables[i].uInstanceIndex = uInstanceOffset;

                for(int32_t iViewport = 0; iViewport < 6; iViewport++)
                {
                    for(uint32_t uInstance = 0; uInstance < ptScene->sbtDrawables[i].uInstanceCount; uInstance++)
                    {
                        uint32_t uTransformIndex = ptScene->sbtDrawables[i + uInstance].uTransformIndex;

                        plShadowInstanceBufferData tShadowInstanceData = {
                            .uTransformIndex = uTransformIndex,
                            .iViewportIndex  = iViewport
                        };
                        
                        memcpy(&ptInstanceBuffer->tMemoryAllocation.pHostMapped[uInstanceOffset * sizeof(tShadowInstanceData)], &tShadowInstanceData, sizeof(tShadowInstanceData));
                        uInstanceOffset++;
                    }
                }
            }
        }
    }
    else
    {
        for(uint32_t i = 0; i < uObjectCount; i++)
        {

            plObjectComponent* ptObject = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtDrawables[i].tEntity);

            // copy transform into proper location in CPU side buffer
            plTransformComponent* ptTransform = gptEcs->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
            memcpy(&ptTransformBuffer->tMemoryAllocation.pHostMapped[ptScene->sbtDrawables[i].uTransformIndex * sizeof(plMat4)], &ptTransform->tWorld, sizeof(plMat4));

            // if using instancing, set index into instance buffer and
            // this includes setting the viewport instance data for multiviewport
            // shadow technique
            if(ptScene->sbtDrawables[i].uInstanceCount != 0)
            {
                ptScene->sbtDrawables[i].uInstanceIndex = uInstanceOffset;

                for(int32_t iViewport = 0; iViewport < 6; iViewport++)
                {
                    for(uint32_t uInstance = 0; uInstance < ptScene->sbtDrawables[i].uInstanceCount; uInstance++)
                    {
                        uint32_t uTransformIndex = ptScene->sbtDrawables[i + uInstance].uTransformIndex;

                        plShadowInstanceBufferData tShadowInstanceData = {
                            .uTransformIndex = uTransformIndex,
                            .iViewportIndex  = 0
                        };
                        
                        memcpy(&ptInstanceBuffer->tMemoryAllocation.pHostMapped[uInstanceOffset * sizeof(tShadowInstanceData)], &tShadowInstanceData, sizeof(tShadowInstanceData));
                        uInstanceOffset++;
                    }
                }
            }

        }
    }

    pl__renderer_pack_view_shadow_atlas(ptScene);

    // update CPU side scene data
    //-----------------------------------------------------------------------------

    if(ptSettings->tFog.tFlags & PL_RENDERER_FOG_FLAGS_ACTIVE)
    {
        if(ptSettings->tFog.tMode == PL_RENDERER_FOG_MODE_LINEAR)
            ptScene->tSceneData.iSceneFlags |= PL_SCENE_FLAG_LINEAR_FOG;
        else
            ptScene->tSceneData.iSceneFlags |= PL_SCENE_FLAG_HEIGHT_FOG;
    }
    else
    {
        ptScene->tSceneData.iSceneFlags &= ~PL_SCENE_FLAG_LINEAR_FOG;
        ptScene->tSceneData.iSceneFlags &= ~PL_SCENE_FLAG_HEIGHT_FOG;
    }

    // atmosphere options

    ptScene->tSceneData.fSunRadius = ptEnvironment->fSunRadius;
    ptScene->tSceneData.planetRadius = ptEnvironment->fPlanetRadius;
    ptScene->tSceneData.tAerialInfo.x = ptEnvironment->fMaxAerialDistance;
    ptScene->tSceneData.tAerialInfo.y = ptEnvironment->fAerialDepthExponent;
    ptScene->tSceneData.tAerialInfo.z = (float)ptEnvironment->uAerialSamplesPerSlice;
    ptScene->tSceneData.tAerialInfo.w = ptEnvironment->fSunIntensity;
    ptScene->tSceneData.fIntensity = ptEnvironment->fSunIntensity;
    ptScene->tSceneData.mieScatteringExponent = ptEnvironment->fMieScatteringExponent;
    ptScene->tSceneData.extinctionMieGround = ptEnvironment->fExtinctionMieGround;
    ptScene->tSceneData.scatteringMieGround = ptEnvironment->fScatteringMieGround;
    ptScene->tSceneData.ozoneExtinction = ptEnvironment->tOzoneExtinction;
    ptScene->tSceneData.extinctionRayleighGround = ptEnvironment->tExtinctionRayleighGround;
    ptScene->tSceneData.scatteringRayleighGround = ptEnvironment->tScatteringRayleighGround;
    ptScene->tSceneData.atmosphereHeight = ptEnvironment->fAtmosphereHeight;
    ptScene->tSceneData.fAtmosphereConversion = ptEnvironment->fAtmosphereConversion;
    ptScene->tSceneData.bMultiScatter = (bool)(ptEnvironment->eFlags & PL_RENDERER_SKY_FLAGS_MULTISCATTER) ? 1 : 0;
    ptScene->tSceneData.bCascadeDebug = (bool)(ptEnvironment->eFlags & PL_RENDERER_SKY_FLAGS_DEBUG_CASCADES) ? 1 : 0;
    ptScene->tSceneData.tColor = ptEnvironment->tSunColor;
    ptScene->tSceneData.tDirection = ptEnvironment->tSunDirection;
    ptScene->tSceneData.iCascadeCount = (int)ptSettings->tShadows.uShadowCascadeCount;

    ptScene->tSceneData.g_time = (float)gptIO->dTime;
    ptScene->tSceneData.fFogDensity = ptSettings->tFog.fDensity;
    ptScene->tSceneData.fFogHeight = ptSettings->tFog.fHeight;
    ptScene->tSceneData.fFogCutOffDistance = ptSettings->tFog.fCutOffDistance;
    ptScene->tSceneData.fFogMaxOpacity = ptSettings->tFog.fMaxOpacity;
    ptScene->tSceneData.fFogStart = ptSettings->tFog.fStart;
    ptScene->tSceneData.fFogHeightFalloff = pl_maxf(0.0f, ptSettings->tFog.fHeightFalloff);
    ptScene->tSceneData.tFogColor = ptSettings->tFog.tColor;
    ptScene->tSceneData.fFogLinearParam0 = 1.0f / (ptScene->tSceneData.fFogCutOffDistance - ptScene->tSceneData.fFogStart);
    ptScene->tSceneData.fFogLinearParam1 = -ptScene->tSceneData.fFogStart / (ptScene->tSceneData.fFogCutOffDistance - ptScene->tSceneData.fFogStart);
    ptScene->tSceneData.iShadowMapTexIdx = ptScene->uSunShadowAtlasIndex;
    ptScene->tSceneData.iCastShadow = 1;
    ptScene->tSceneData.fFactor = (float)ptSettings->tShadows.uShadowResolution / (float)ptScene->uSunShadowAtlasResolution;

    uint32_t uShadowResolution = (uint32_t)ptScene->sbtShadowViewRects[0].iHeight;
    int iX = ptScene->sbtShadowViewRects[0].iX;
    int iY = ptScene->sbtShadowViewRects[0].iY;
    ptScene->tSceneData.iShadowMapTexIdx = ptScene->uSunShadowAtlasIndex;
    ptScene->tSceneData.fFactor          = (float)uShadowResolution / (float)ptScene->uSunShadowAtlasResolution;
    ptScene->tSceneData.fXOffset         = (float)iX / (float)ptScene->uSunShadowAtlasResolution;
    ptScene->tSceneData.fYOffset         = (float)iY / (float)ptScene->uSunShadowAtlasResolution;


    plBuffer* ptSceneBuffer = gptGfx->get_buffer(ptDevice, ptScene->atSceneBuffer[uFrameIdx]);
    memcpy(ptSceneBuffer->tMemoryAllocation.pHostMapped, &ptScene->tSceneData, sizeof(plGpuSceneData));

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~perform skinning~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    plCommandBuffer* ptSkinningCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "skinning");
    gptGfx->begin_command_recording(ptSkinningCmdBuffer);

    const uint32_t uSkinCount = pl_sb_size(ptScene->sbtSkinData);
    plBuffer* ptStagingBuffer = gptGfx->get_buffer(ptDevice, ptScene->atDynamicSkinBuffer[uFrameIdx]);
    for(uint32_t i = 0; i < uSkinCount; i++)
    {
        plSkinComponent* ptSkinComponent = gptEcs->get_component(ptScene->ptComponentLibrary, gptSkeleton->get_ecs_type_key_skin(), ptScene->sbtSkinData[i].tEntity);
        plSkin* ptSkin = gptAsset->get_data(ptSkinComponent->tSkin);
        memcpy(&ptStagingBuffer->tMemoryAllocation.pHostMapped[ptScene->sbtSkinData[i].ptFreeListNode->uOffset], ptSkinComponent->_atTextureData, sizeof(plMat4) * ptSkin->uJointCount * 2);
    }

    pl__renderer_perform_skinning(ptSkinningCmdBuffer, ptScene);

    gptGfx->end_command_recording(ptSkinningCmdBuffer);

    const plSubmitInfo tSkinningSubmitInfo = {
        .uWaitSemaphoreCount     = pl_min(2, PL_MAX_FRAMES_IN_FLIGHT),
        .atWaitSempahores        = {gptStarter->get_current_timeline_semaphore(), gptStarter->get_last_timeline_semaphore()},
        .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value(), gptStarter->get_last_timeline_value()},
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
    };
    gptGfx->submit_command_buffer(ptSkinningCmdBuffer, &tSkinningSubmitInfo);
    gptGfx->return_command_buffer(ptSkinningCmdBuffer);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~generate shadow maps~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // prep
    pl__renderer_pack_shadow_atlas(ptScene);
    
    plCommandBuffer* ptShadowCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "scene shadows");
    gptGfx->begin_command_recording(ptShadowCmdBuffer);

    plRenderInfo tFirstShadowRenderInfo = {
        .tRenderArea = {
            .tMin = {0},
            .tMax = {.x = (float)ptScene->uShadowAtlasResolution, .y = (float)ptScene->uShadowAtlasResolution}
        },
        .tDepthAttachment = {
            .tTexture        = ptScene->tShadowTexture,
            .eLoadOp         = PL_LOAD_OP_CLEAR,
            .eStoreOp        = PL_STORE_OP_STORE,
            .eUsage          = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
            .fClearZ         = 0.0f
        }
    };

    gptGfx->begin_render_pass(ptShadowCmdBuffer, &tFirstShadowRenderInfo, NULL);
    gptGfx->push_debug_group(ptShadowCmdBuffer, "First Shadow", (plVec4){0.33f, 0.02f, 0.10f, 1.0f});

    pl__renderer_generate_shadow_maps(ptShadowCmdBuffer, ptScene, atCameras, uCameraCount);

    gptGfx->pop_debug_group(ptShadowCmdBuffer);
    gptGfx->end_render_pass(ptShadowCmdBuffer);
    gptGfx->end_command_recording(ptShadowCmdBuffer);

    const plSubmitInfo tShadowSubmitInfo = {
        .uWaitSemaphoreCount     = 1,
        .atWaitSempahores        = {gptStarter->get_current_timeline_semaphore()},
        .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
    };
    gptGfx->submit_command_buffer(ptShadowCmdBuffer, &tShadowSubmitInfo);
    gptGfx->return_command_buffer(ptShadowCmdBuffer);

    plBuffer* ptShadowDataBuffer = gptGfx->get_buffer(ptDevice, ptScene->atPointLightShadowDataBuffer[uFrameIdx]);
    memcpy(ptShadowDataBuffer->tMemoryAllocation.pHostMapped, ptScene->sbtPointLightShadowData, sizeof(plGpuPointLightShadow) * pl_sb_size(ptScene->sbtPointLightShadowData));
    ptShadowDataBuffer = gptGfx->get_buffer(ptDevice, ptScene->atSpotLightShadowDataBuffer[uFrameIdx]);
    memcpy(ptShadowDataBuffer->tMemoryAllocation.pHostMapped, ptScene->sbtSpotLightShadowData, sizeof(plGpuSpotLightShadow) * pl_sb_size(ptScene->sbtSpotLightShadowData));
    
    // atmospheric rendering - sun transmission
    //-----------------------------------------------------------------------------
    if(ptEnvironment->eMode == PL_RENDERER_SKY_MODE_REALISTIC && ptScene->abSunTransmissionLutDirty[uFrameIdx])
    {
        plCommandBuffer* ptSunCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "sun transmission");
        gptGfx->begin_command_recording(ptSunCmdBuffer);

        // must declare resources & how they are used
        plPassResources tPassResources0 = {
            .atTextures = {
                {
                    .eUsage  = PL_TEXTURE_USAGE_STORAGE,
                    .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE,
                    .eStages = PL_PIPELINE_STAGE_COMPUTE,
                    .tHandle = ptScene->atSunTransmissionLut[uFrameIdx]
                },
                {
                    .eUsage  = PL_TEXTURE_USAGE_STORAGE,
                    .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE,
                    .eStages = PL_PIPELINE_STAGE_COMPUTE,
                    .tHandle = ptScene->atSunMultiscatterLut[uFrameIdx]
                }
            }
        };

        // begin compute pass
        gptGfx->begin_compute_pass(ptSunCmdBuffer, &tPassResources0);

        // transmission lut
        plDispatch tDispatch0 = {
            .uGroupCountX = (uint32_t)ceilf(ptSettings->tSky.tTransmissionLutResolution.x / 8.0f),
            .uGroupCountY = (uint32_t)ceilf(ptSettings->tSky.tTransmissionLutResolution.y / 8.0f),
            .uGroupCountZ = 1,
            .uThreadPerGroupX = 8,
            .uThreadPerGroupY = 8,
            .uThreadPerGroupZ = 1
        };

        // vertical blur
        gptGfx->bind_compute_shader(ptSunCmdBuffer, ptScene->tSunTransmissionLutShader);
        plBindGroupHandle atTransmissionBGs[] = {ptScene->atSceneBindGroups[uFrameIdx], ptScene->atSunTransmissionBG1[uFrameIdx] };
        gptGfx->bind_compute_bind_groups(ptSunCmdBuffer, ptScene->tSunTransmissionLutShader, 0, PL_ARRAYSIZE(atTransmissionBGs), atTransmissionBGs, 0, NULL);
        gptGfx->dispatch(ptSunCmdBuffer, 1, &tDispatch0);

        plPassResources tPassResources1 = {
            .atTextures = {
                {
                    .eUsage  = PL_TEXTURE_USAGE_SAMPLED,
                    .eAccess = PL_PASS_RESOURCE_ACCESS_READ,
                    .eStages = PL_PIPELINE_STAGE_COMPUTE,
                    .tHandle = ptScene->atSunTransmissionLut[uFrameIdx]
                }
            }
        };

        gptGfx->intra_pass_barrier(ptSunCmdBuffer, PL_PIPELINE_STAGE_COMPUTE,  PL_PIPELINE_STAGE_COMPUTE, PL_BARRIER_SCOPE_ALL, &tPassResources1);

        // multiscatter lut
        plDispatch tDispatch1 = {
            .uGroupCountX = (uint32_t)ceilf(ptSettings->tSky.tMultiscatterLutResolution.x / 8.0f),
            .uGroupCountY = (uint32_t)ceilf(ptSettings->tSky.tMultiscatterLutResolution.y / 8.0f),
            .uGroupCountZ = 1,
            .uThreadPerGroupX = 8,
            .uThreadPerGroupY = 8,
            .uThreadPerGroupZ = 1
        };

        // vertical blur
        gptGfx->bind_compute_shader(ptSunCmdBuffer, ptScene->tSunMultiscatterShader);
        plBindGroupHandle atMultiscatterBGs[] = {ptScene->atSceneBindGroups[uFrameIdx], ptScene->atSunMultiscatterBG1[uFrameIdx] };
        gptGfx->bind_compute_bind_groups(ptSunCmdBuffer, ptScene->tSunMultiscatterShader, 0, PL_ARRAYSIZE(atMultiscatterBGs), atMultiscatterBGs, 0, NULL);
        gptGfx->dispatch(ptSunCmdBuffer, 1, &tDispatch1);

        // end compute pass
        gptGfx->end_compute_pass(ptSunCmdBuffer);

        gptGfx->end_command_recording(ptSunCmdBuffer);

        const plSubmitInfo tSunTransmissionSubmitInfo = {
            .uWaitSemaphoreCount     = 1,
            .atWaitSempahores        = {gptStarter->get_current_timeline_semaphore()},
            .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
            .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
        };
        gptGfx->submit_command_buffer(ptSunCmdBuffer, &tSunTransmissionSubmitInfo);
        gptGfx->return_command_buffer(ptSunCmdBuffer);

        // ptScene->abSunTransmissionLutDirty[uFrameIdx] = false;
    }
    
    // update environment probes
    //-----------------------------------------------------------------------------

    if(uFrameIdx == 0) // multiple frames in flight may fight
        pl__renderer_probe_update_all(ptScene);

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

void
pl_renderer_prepare_view(plView* ptView, const plCamera* ptCamera)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    // for convience
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    plDevice*      ptDevice  = gptData->ptDevice;
    plDrawStream*  ptStream  = &gptData->tDrawStream;

    plRenderScene* ptScene = ptView->ptParentScene;
    plRenderSettings* ptSettings = gptAsset->get_data(ptScene->tSettings);

    // update view data
    //-----------------------------------------------------------------------------

    ptView->tViewData.tCameraPos.xyz = ptCamera->tPositionF;
    ptView->tViewData.fCameraRange = ptCamera->fFarZ - ptCamera->fNearZ;
    ptView->tViewData.fCameraNearZ = ptCamera->fNearZ;
    ptView->tViewData.fAspectRatio = ptCamera->fAspectRatio;
    ptView->tViewData.iCameraProjectType = ptCamera->eProjectionType;
    ptView->tViewData.tViewportSize = (plVec4){.xy = ptView->tTargetSize, .ignored0_ = 1.0f, .ignored1_ = 1.0f};
    ptView->tViewData.tCameraProjection[0]     = ptCamera->tProjMat;
    ptView->tViewData.tInvViewMatNoTranslation[0] = ptCamera->tInvViewMatNoTranslation;
    ptView->tViewData.tCameraProjectionInv[0]  = pl_mat4_invert(&ptCamera->tProjMat);
    ptView->tViewData.tCameraViewInv[0]        = ptCamera->tInvViewMat;
    ptView->tViewData.tCameraView[0]           = ptCamera->tViewMat;
    ptView->tViewData.tCameraViewProjection[0] = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);
    
    // const float fFogDensity = -(float)(ptView->tViewData.fFogHeightFalloff * (ptCamera->tPositionF.y - ptView->tViewData.fFogHeight));
    // ptView->tViewData.tFogDensity = (plVec3){ptSettings->tFog.fDensity, fFogDensity, ptSettings->tFog.fDensity * expf(fFogDensity)};

    plBuffer* ptViewBuffer = gptGfx->get_buffer(ptDevice, ptView->atViewBuffers[uFrameIdx]);
    memcpy(ptViewBuffer->tMemoryAllocation.pHostMapped, &ptView->tViewData, sizeof(plGpuViewData));

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~generate CSMs~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    plCommandBuffer* ptCSMCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "sun view csm");
    gptGfx->begin_command_recording(ptCSMCmdBuffer);

    plRenderInfo tShadowRenderInfo = {
        .tRenderArea = {
            .tMin = {0},
            .tMax = {.x = (float)ptScene->uSunShadowAtlasResolution, .y = (float)ptScene->uSunShadowAtlasResolution}
        },
        .tDepthAttachment = {
            .tTexture        = ptScene->tSunShadowTexture,
            .eLoadOp         = PL_LOAD_OP_CLEAR,
            .eStoreOp        = PL_STORE_OP_STORE,
            .eUsage          = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
            .fClearZ         = 0.0f
        }
    };

    gptGfx->begin_render_pass(ptCSMCmdBuffer, &tShadowRenderInfo, NULL);
    
    gptGfx->push_debug_group(ptCSMCmdBuffer, "Sun View CSM", (plVec4){0.33f, 0.02f, 0.10f, 1.0f});

    pl__renderer_generate_sun_shadow_map(ptCSMCmdBuffer, ptScene, &ptView->tViewData, ptView->atViewBuffers[uFrameIdx], ptView->atSunCameraBuffers[uFrameIdx], ptCamera, ptView->atSunCSMBG[uFrameIdx], false);

    plCSMInfo tCSMInfo = {
        .tBindGroup = ptView->atDShadowBG[uFrameIdx],
        .tDShadowCameraBuffer = ptView->atDShadowCameraBuffers[uFrameIdx],
        .tDLightShadowDataBuffer = ptView->atDLightShadowDataBuffer[uFrameIdx],
        .sbtDLightShadowData = ptView->sbtDLightShadowData
    };
    pl__renderer_generate_direction_view_map(ptCSMCmdBuffer, ptScene, ptCamera, tCSMInfo);

    gptGfx->producer_barrier(ptCSMCmdBuffer, PL_PIPELINE_STAGE_FRAGMENT | PL_PIPELINE_STAGE_VERTEX, PL_PIPELINE_STAGE_VERTEX, PL_BARRIER_SCOPE_ALL);
    gptGfx->pop_debug_group(ptCSMCmdBuffer);
    gptGfx->end_render_pass(ptCSMCmdBuffer);
    gptGfx->end_command_recording(ptCSMCmdBuffer);

    const plSubmitInfo tCSMSubmitInfo = {
        .uWaitSemaphoreCount     = 1,
        .atWaitSempahores        = {gptStarter->get_current_timeline_semaphore()},
        .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
    };
    gptGfx->submit_command_buffer(ptCSMCmdBuffer, &tCSMSubmitInfo);
    gptGfx->return_command_buffer(ptCSMCmdBuffer);

    // debug drawing environment probes
    //-----------------------------------------------------------------------------

    if(ptScene->tDebugOptions.bShowProbes)
    {
        const uint32_t uProbeCount = pl_sb_size(ptScene->sbtProbeData);
        for(uint32_t uProbeIndex = 0; uProbeIndex < uProbeCount; uProbeIndex++)
        {
            plEnvironmentProbeData* ptProbe = &ptScene->sbtProbeData[uProbeIndex];
            plEnvironmentProbeComponent* ptProbeComp = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, ptProbe->tEntity);
            plObjectComponent* ptObject = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptProbe->tEntity);
            plTransformComponent* ptTransform = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptObject->tTransform);
            // gptDraw->add_3d_aabb(ptView->pt3DDrawList, ptObject->tAABB.tMin, ptObject->tAABB.tMax, (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(0.0f, 1.0f, 0.0f), .fThickness = 0.02f});

            if(ptScene->tDebugOptions.bShowProbeRange)
            {
                plSphere tSphere = {
                    .fRadius = ptProbeComp->fRange,
                    .tCenter = ptTransform->tTranslation
                };
                gptDraw->add_3d_sphere(ptView->pt3DDrawList, tSphere, 6, 6, (plDrawLineOptions){.uColor = PL_COLOR_32_LIGHT_GREY, .fThickness = 0.005f});
            }
        }
    }

    // check if bloom mip chain textures are needed
    //-----------------------------------------------------------------------------

    uint32_t uBloomTextureCount = pl_sb_size(ptView->sbtBloomDownChain);

    if(ptSettings->tBloom.tFlags & PL_RENDERER_BLOOM_FLAGS_ACTIVE && uBloomTextureCount < ptSettings->tBloom.uChainLength)
    {

        uint32_t uNewTexturesNeeded = ptSettings->tBloom.uChainLength - uBloomTextureCount;

        plTextureDesc tBloomTextureDesc = {
            .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
            .eFormat       = PL_FORMAT_R16G16B16A16_FLOAT,
            .uLayers       = 1,
            .uMips         = 1,
            .eType         = PL_TEXTURE_TYPE_2D,
            .eUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_STORAGE,
            .pcDebugName   = "bloom"
        };
    
        tBloomTextureDesc.tDimensions.x *= powf(0.5f, (float)uBloomTextureCount);
        tBloomTextureDesc.tDimensions.y *= powf(0.5f, (float)uBloomTextureCount);

        for(uint32_t i = 0; i < uNewTexturesNeeded; i++)
        {

            if(tBloomTextureDesc.tDimensions.x < 1.0f || tBloomTextureDesc.tDimensions.y < 1.0f)
            {
                ptSettings->tBloom.uChainLength -= uNewTexturesNeeded - i;
                break;
            }

            plTextureHandle tBloomUpTexture = {0};
            plTextureHandle tBloomDownTexture = {0};

            gptStarter->create_texture(&tBloomTextureDesc, NULL, 0, &tBloomUpTexture);
            gptStarter->create_texture(&tBloomTextureDesc, NULL, 0, &tBloomDownTexture);

            pl_sb_push(ptView->sbtBloomDownChain, tBloomDownTexture);
            pl_sb_push(ptView->sbtBloomUpChain, tBloomUpTexture);

            tBloomTextureDesc.tDimensions.x *= 0.5f;
            tBloomTextureDesc.tDimensions.y *= 0.5f;
        }
    }

    plRenderEnvironment* ptEnvironment = gptAsset->get_data(ptScene->tEnvironment);
    if(ptEnvironment->eMode & PL_RENDERER_SKY_MODE_REALISTIC)
    {
        plCommandBuffer* ptSunCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "sun transmission");
        gptGfx->begin_command_recording(ptSunCmdBuffer);

        plPassResources tPassResources0 = {
            .atTextures = {
                {
                    .eUsage  = PL_TEXTURE_USAGE_SAMPLED,
                    .eAccess = PL_PASS_RESOURCE_ACCESS_READ,
                    .eStages = PL_PIPELINE_STAGE_COMPUTE,
                    .tHandle = ptScene->atSunTransmissionLut[uFrameIdx]
                },
                {
                    .eUsage  = PL_TEXTURE_USAGE_SAMPLED,
                    .eAccess = PL_PASS_RESOURCE_ACCESS_READ,
                    .eStages = PL_PIPELINE_STAGE_COMPUTE,
                    .tHandle = ptScene->atSunMultiscatterLut[uFrameIdx]
                },
                {
                    .eUsage  = PL_TEXTURE_USAGE_STORAGE,
                    .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE,
                    .eStages = PL_PIPELINE_STAGE_COMPUTE,
                    .tHandle = ptView->atSkyLut[uFrameIdx]
                }
            }
        };

        gptGfx->begin_compute_pass(ptSunCmdBuffer, &tPassResources0);
        gptGfx->push_debug_group(ptSunCmdBuffer, "Sky LUT", (plVec4){0.33f, 0.42f, 0.70f, 1.0f});

        plDispatch tDispatch3 = {
            .uGroupCountX = (uint32_t)ceilf(ptSettings->tSky.tSkyLutResolution.x / 8.0f),
            .uGroupCountY = (uint32_t)ceilf(ptSettings->tSky.tSkyLutResolution.y / 8.0f),
            .uGroupCountZ = 1,
            .uThreadPerGroupX = 8,
            .uThreadPerGroupY = 8,
            .uThreadPerGroupZ = 1
        };

        // vertical blur
        gptGfx->bind_compute_shader(ptSunCmdBuffer, ptScene->tSkyViewLutShader);
        plBindGroupHandle atSkyLutBGs[] = {ptScene->atSceneBindGroups[uFrameIdx], ptView->atViewBG[uFrameIdx], ptView->atSkyLutBG2[uFrameIdx] };
        gptGfx->bind_compute_bind_groups(ptSunCmdBuffer, ptScene->tSkyViewLutShader, 0, PL_ARRAYSIZE(atSkyLutBGs), atSkyLutBGs, 0, NULL);

        gptGfx->dispatch(ptSunCmdBuffer, 1, &tDispatch3);

        gptGfx->pop_debug_group(ptSunCmdBuffer);
        gptGfx->end_compute_pass(ptSunCmdBuffer);

        gptGfx->end_command_recording(ptSunCmdBuffer);

        const plSubmitInfo tSunTransmissionSubmitInfo = {
            .uWaitSemaphoreCount     = 1,
            .atWaitSempahores        = {gptStarter->get_current_timeline_semaphore()},
            .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
            .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
        };
        gptGfx->submit_command_buffer(ptSunCmdBuffer, &tSunTransmissionSubmitInfo);
        gptGfx->return_command_buffer(ptSunCmdBuffer);
    }

    if(ptEnvironment->eMode == PL_RENDERER_SKY_MODE_REALISTIC && ptEnvironment->eFlags & PL_RENDERER_SKY_FLAGS_AERIAL_PERSPECTIVE)
    {
        plCommandBuffer* ptSunCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "sun aerial lut");
        gptGfx->begin_command_recording(ptSunCmdBuffer);

        plPassResources tPassResources0 = {
            .atTextures = {
                {
                    .eUsage  = PL_TEXTURE_USAGE_SAMPLED,
                    .eAccess = PL_PASS_RESOURCE_ACCESS_READ,
                    .eStages = PL_PIPELINE_STAGE_COMPUTE,
                    .tHandle = ptScene->atSunTransmissionLut[uFrameIdx]
                },
                {
                    .eUsage  = PL_TEXTURE_USAGE_SAMPLED,
                    .eAccess = PL_PASS_RESOURCE_ACCESS_READ,
                    .eStages = PL_PIPELINE_STAGE_COMPUTE,
                    .tHandle = ptScene->atSunMultiscatterLut[uFrameIdx]
                },
                {
                    .eUsage  = PL_TEXTURE_USAGE_STORAGE,
                    .eAccess = PL_PASS_RESOURCE_ACCESS_WRITE,
                    .eStages = PL_PIPELINE_STAGE_COMPUTE,
                    .tHandle = ptView->atSkyAerialLut[uFrameIdx]
                }
            }
        };

        gptGfx->begin_compute_pass(ptSunCmdBuffer, &tPassResources0);
        gptGfx->push_debug_group(ptSunCmdBuffer, "Sky aerial LUT", (plVec4){0.33f, 0.72f, 0.70f, 1.0f});

        plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(uint32_t) * 8);
        uint32_t* ptDynamicData = (uint32_t*)tDynamicBinding.pcData;
        *ptDynamicData = 0; // camera index

        plDispatch tDispatch2 = {
            .uGroupCountX = (uint32_t)roundf(ptSettings->tSky.tAerialLutResolution.x / 8.0f),
            .uGroupCountY = (uint32_t)roundf(ptSettings->tSky.tAerialLutResolution.y / 8.0f),
            .uGroupCountZ = 1,
            .uThreadPerGroupX = 8,
            .uThreadPerGroupY = 8,
            .uThreadPerGroupZ = 1
        };

        gptGfx->bind_compute_shader(ptSunCmdBuffer, ptScene->tSkyAerialLutShader);
        plBindGroupHandle atSkyLutBGs[] = {ptScene->atSceneBindGroups[uFrameIdx], ptView->atViewBG[uFrameIdx], ptView->atSkyAerialLutBG2[uFrameIdx] };
        gptGfx->bind_compute_bind_groups(ptSunCmdBuffer, ptScene->tSkyAerialLutShader, 0, PL_ARRAYSIZE(atSkyLutBGs), atSkyLutBGs, 1, &tDynamicBinding);

        gptGfx->dispatch(ptSunCmdBuffer, 1, &tDispatch2);

        gptGfx->pop_debug_group(ptSunCmdBuffer);
        gptGfx->end_compute_pass(ptSunCmdBuffer);

        gptGfx->end_command_recording(ptSunCmdBuffer);

        const plSubmitInfo tSunTransmissionSubmitInfo = {
            .uWaitSemaphoreCount     = 1,
            .atWaitSempahores        = {gptStarter->get_current_timeline_semaphore()},
            .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
            .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
        };
        gptGfx->submit_command_buffer(ptSunCmdBuffer, &tSunTransmissionSubmitInfo);
        gptGfx->return_command_buffer(ptSunCmdBuffer);
    }

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

void
pl_renderer_render_view(plView* ptView, const plRenderViewDesc* ptViewDesc)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    const plCamera* ptCamera = ptViewDesc->ptCamera;
    const plCamera* ptCullCamera = ptViewDesc->ptCullCamera;

    // for convience
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    plDevice*      ptDevice  = gptData->ptDevice;
    plRenderScene* ptScene   = ptView->ptParentScene;
    plRenderSettings* ptSettings = gptAsset->get_data(ptScene->tSettings);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~culling~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, "culling");
    
    const uint32_t uDrawableCount = pl_sb_size(ptScene->sbtDrawables);

    plAtomicCounter* ptCullCounter = NULL;
    
    plCullData tCullData = {
        .ptScene      = ptScene,
        .ptCullCamera = ptCullCamera,
        .atDrawables  = ptScene->sbtDrawables
    };
    if(ptCullCamera && ptCullCamera->eProjectionType == PL_CAMERA_PROJECTION_TYPE_PERSPECTIVE)
        pl__camera_build_perspective_frustum(ptCullCamera, &tCullData.tFrustum);
    else if(ptCullCamera)
        pl__camera_build_orthographic_frustum(ptCullCamera, &tCullData.tFrustum);

    if(ptCullCamera)
    {
        plJobDesc tJobDesc = {
            .task  = pl__renderer_cull_job,
            .pData = &tCullData
        };

        gptJob->dispatch_batch(uDrawableCount, 0, tJobDesc, &ptCullCounter);
    }
    else // no culling, just copy drawables over
    {
        if(pl_sb_size(ptScene->sbtVisibleDrawables) != uDrawableCount)
        {
            pl_sb_resize(ptScene->sbtVisibleDrawables, uDrawableCount);
            for(uint32_t i = 0; i < uDrawableCount; i++)
                ptScene->sbtVisibleDrawables[i] = i;
        }
    }
    gptJob->wait_for_counter(ptCullCounter);
    PL_PROFILE_END_SAMPLE_API(gptProfile, 0); // culling

    //~~~~~~~~~~~~~~~~~~~~~~~~~~binning based on pass type~~~~~~~~~~~~~~~~~~~~~~~~~
    
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, "binning");

    if(ptCullCamera)
    {
        pl_sb_reset(ptScene->sbuVisibleDeferredEntities);
        pl_sb_reset(ptScene->sbuVisibleForwardEntities);
        pl_sb_reset(ptScene->sbuVisibleTransmissionEntities);
        pl_sb_reset(ptScene->sbtVisibleDrawables);

        for(uint32_t uDrawableIndex = 0; uDrawableIndex < uDrawableCount; uDrawableIndex++)
        {
            const plDrawable tDrawable = ptScene->sbtDrawables[uDrawableIndex];
            if(!tDrawable.bCulled)
            {
                if(tDrawable.tFlags & PL_DRAWABLE_FLAG_DEFERRED)
                {
                    pl_sb_push(ptScene->sbuVisibleDeferredEntities, uDrawableIndex);
                    pl_sb_push(ptScene->sbtVisibleDrawables, uDrawableIndex);
                }
                else if(tDrawable.tFlags & PL_DRAWABLE_FLAG_PROBE)
                {
                    if(ptScene->tDebugOptions.bShowProbes)
                    {
                        pl_sb_push(ptScene->sbuVisibleForwardEntities, uDrawableIndex);
                        pl_sb_push(ptScene->sbtVisibleDrawables, uDrawableIndex);
                    }
                }
                else if(tDrawable.tFlags & PL_DRAWABLE_FLAG_FORWARD)
                {
                    pl_sb_push(ptScene->sbuVisibleForwardEntities, uDrawableIndex);
                    pl_sb_push(ptScene->sbtVisibleDrawables, uDrawableIndex);
                }
                else if(tDrawable.tFlags & PL_DRAWABLE_FLAG_TRANSMISSION)
                {
                    pl_sb_push(ptScene->sbuVisibleTransmissionEntities, uDrawableIndex);
                    pl_sb_push(ptScene->sbtVisibleDrawables, uDrawableIndex);
                }
                
            }
        }
    }
    PL_PROFILE_END_SAMPLE_API(gptProfile, 0); // binning

    // main rendering work
    //-----------------------------------------------------------------------------

    plCommandBuffer* ptSceneCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "main scene");
    gptGfx->begin_command_recording(ptSceneCmdBuffer);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~main render pass work~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    plRenderInfo tMainRenderInfo = {
        .tRenderArea = {
            .tMin = {0},
            .tMax = ptView->tTargetSize
        },
        .atColorAttachments = {
            {
                .tTexture       = ptView->tRawOutputTexture,
                .eLoadOp        = PL_LOAD_OP_CLEAR,
                .eStoreOp       = PL_STORE_OP_STORE,
                .eUsage         = PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
                .tClearColor    = {0.0f, 0.0f, 0.0f, 1.0f}
            },
            {
                .tTexture       = ptView->tAlbedoTexture,
                .eLoadOp        = PL_LOAD_OP_CLEAR,
                .eStoreOp       = PL_STORE_OP_STORE,
                .eUsage         = PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
                .tClearColor    = {0.0f, 0.0f, 0.0f, 1.0f}
            },
            {
                .tTexture       = ptView->tNormalTexture,
                .eLoadOp        = PL_LOAD_OP_CLEAR,
                .eStoreOp       = PL_STORE_OP_STORE,
                .eUsage         = PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
                .tClearColor    = {0.0f, 0.0f, 0.0f, 1.0f}
            },
            {
                .tTexture       = ptView->tAOMetalRoughnessTexture,
                .eLoadOp        = PL_LOAD_OP_CLEAR,
                .eStoreOp       = PL_STORE_OP_STORE,
                .eUsage         = PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
                .tClearColor    = {0.0f, 0.0f, 0.0f, 0.0f}
            }
        },
        .tDepthAttachment = {
            .tTexture        = ptView->tDepthTexture,
            .eLoadOp         = PL_LOAD_OP_CLEAR,
            .eStoreOp        = PL_STORE_OP_STORE,
            .eUsage          = PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
            .fClearZ         = 0.0f
        },
        .tStencilAttachment = {
            .tTexture        = ptView->tDepthTexture,
            .eLoadOp         = PL_LOAD_OP_CLEAR,
            .eStoreOp        = PL_STORE_OP_STORE,
            .eUsage          = PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
            .uClearStencil   = 0
        }
    };

    gptGfx->consumer_barrier(ptSceneCmdBuffer, PL_PIPELINE_STAGE_FRAGMENT | PL_PIPELINE_STAGE_VERTEX, PL_PIPELINE_STAGE_VERTEX | PL_PIPELINE_STAGE_FRAGMENT, PL_BARRIER_SCOPE_ALL);

    // gbuffer fill
    //-----------------------------------------------------------------------------

    gptGfx->begin_render_pass(ptSceneCmdBuffer, &tMainRenderInfo, NULL);
    gptGfx->set_depth_bias(ptSceneCmdBuffer, 0.0f, 0.0f, 0.0f);
    
    const plVec2 tDimensions = ptView->tTargetSize;

    plDrawArea tArea = {
        .ptDrawStream = &gptData->tDrawStream,
        .atScissors = 
        {
            {
                .uWidth  = (uint32_t)tDimensions.x,
                .uHeight = (uint32_t)tDimensions.y,
            }
        },
        .atViewports =
        {
            {
                .fWidth  = tDimensions.x,
                .fHeight = tDimensions.y,
                .fMaxDepth = 1.0f
            }
        }
    };

    plGbufferFillPassInfo tGbufferFillPassInfo = {
        .sbuVisibleDeferredEntities = ptScene->sbuVisibleDeferredEntities,
        .uGlobalIndex = 0,
        .tBG2 = ptView->atDeferredBG1[uFrameIdx],
        .ptArea = &tArea,
        .sbtShaders = ptScene->sbtRegularShaders
    };
    pl__render_view_gbuffer_fill_pass(ptScene, ptSceneCmdBuffer, &tGbufferFillPassInfo);

    if(ptScene->ptTerrain)
    {
        gptGfx->push_debug_group(ptSceneCmdBuffer, "Terrain", (plVec4){0.33f, 0.42f, 0.20f, 1.0f});
        plShaderHandle tTerrainShader = ptScene->tTerrainShader;

        if(ptScene->ptTerrain->tRuntimeOptions.tFlags & PL_TERRAIN_FLAGS_WIREFRAME)
            tTerrainShader = ptScene->tTerrainWireframeShader;

        gptGfx->set_depth_bias(ptSceneCmdBuffer, 0.0f, 0.0f, 0.0f);
        gptGfx->bind_shader(ptSceneCmdBuffer, tTerrainShader);
        gptGfx->bind_vertex_buffer(ptSceneCmdBuffer, ptScene->ptTerrain->tVertexBuffer);
        plBindGroupHandle atBindGroups[] = {ptScene->atSceneBindGroups[uFrameIdx], ptView->atDeferredBG1[uFrameIdx]};
        gptGfx->bind_graphics_bind_groups(
            ptSceneCmdBuffer,
            tTerrainShader,
            0, 2,
            atBindGroups,
            0, NULL
        );

        for(uint32_t i = 0; i < pl_sb_size(ptScene->ptTerrain->sbtChunkFiles); i++)
            pl__render_chunk(ptScene, ptScene->ptTerrain, ptCamera, ptSceneCmdBuffer, &ptScene->ptTerrain->sbtChunkFiles[i].tFile.atChunks[0], &ptScene->ptTerrain->sbtChunkFiles[i].tFile, 0);

        gptGfx->pop_debug_group(ptSceneCmdBuffer);
    }
    
    // deferred lighting
    //-----------------------------------------------------------------------------

    const plPassResources tResourceUpdates = {
        .atTextures = {
            {
                .tHandle = ptView->tAlbedoTexture,
                .eAccess = PL_PASS_RESOURCE_ACCESS_READ,
                .eUsage = PL_TEXTURE_USAGE_INPUT_ATTACHMENT
            },
            {
                .tHandle = ptView->tNormalTexture,
                .eAccess = PL_PASS_RESOURCE_ACCESS_READ,
                .eUsage = PL_TEXTURE_USAGE_INPUT_ATTACHMENT
            },
            {
                .tHandle = ptView->tAOMetalRoughnessTexture,
                .eAccess = PL_PASS_RESOURCE_ACCESS_READ,
                .eUsage = PL_TEXTURE_USAGE_INPUT_ATTACHMENT
            },
            {
                .tHandle = ptView->tDepthTexture,
                .eAccess = PL_PASS_RESOURCE_ACCESS_READ,
                .eUsage = PL_TEXTURE_USAGE_INPUT_ATTACHMENT
            }
        }
    };

    gptGfx->intra_pass_barrier(ptSceneCmdBuffer, PL_PIPELINE_STAGE_FRAGMENT, PL_PIPELINE_STAGE_FRAGMENT, PL_BARRIER_SCOPE_ALL, &tResourceUpdates);

    plDeferredLightingPassInfo tDeferredLightingPassInfo = {
        .tBG2 = ptView->tLightingBindGroup,
        .ptArea = &tArea,
        .uProbe = 0,
        .uGlobalIndex = 0,
    };

    if(ptScene->tDebugOptions.tShaderDebugMode == PL_SHADER_DEBUG_MODE_NONE)
        pl__render_view_deferred_lighting_pass(ptScene, ptSceneCmdBuffer, ptView->atViewBG[uFrameIdx], &tDeferredLightingPassInfo);
    else
        pl__render_view_deferred_lighting_debug_pass(ptView, ptSceneCmdBuffer, ptView->atViewBG[uFrameIdx]);

    gptGfx->producer_barrier(ptSceneCmdBuffer, PL_PIPELINE_STAGE_FRAGMENT | PL_PIPELINE_STAGE_VERTEX, PL_PIPELINE_STAGE_VERTEX, PL_BARRIER_SCOPE_ALL);
    gptGfx->end_render_pass(ptSceneCmdBuffer);

    // forward pass
    //-----------------------------------------------------------------------------

    plRenderInfo tForwardRenderInfo = {
        .tRenderArea = {
            .tMin = {0},
            .tMax = ptView->tTargetSize
        },
        .atColorAttachments = {
            {
                .tTexture       = ptView->tRawOutputTexture,
                .eLoadOp        = PL_LOAD_OP_LOAD,
                .eStoreOp       = PL_STORE_OP_STORE,
                .eUsage         = PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
                .tClearColor    = {0.0f, 0.0f, 0.0f, 0.0f}
            }
        },
        .tDepthAttachment = {
            .tTexture        = ptView->tDepthTexture,
            .eLoadOp         = PL_LOAD_OP_LOAD,
            .eStoreOp        = PL_STORE_OP_STORE,
            .eUsage          = PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
            .fClearZ         = 0.0f
        },
        .tStencilAttachment = {
            .tTexture        = ptView->tDepthTexture,
            .eLoadOp         = PL_LOAD_OP_LOAD,
            .eStoreOp        = PL_STORE_OP_STORE,
            .eUsage          = PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
            .uClearStencil   = 0
        }
    };

    gptGfx->consumer_barrier(ptSceneCmdBuffer, PL_PIPELINE_STAGE_FRAGMENT | PL_PIPELINE_STAGE_VERTEX, PL_PIPELINE_STAGE_VERTEX | PL_PIPELINE_STAGE_FRAGMENT, PL_BARRIER_SCOPE_ALL);
    gptGfx->begin_render_pass(ptSceneCmdBuffer, &tForwardRenderInfo, NULL);
    gptGfx->set_depth_bias(ptSceneCmdBuffer, 0.0f, 0.0f, 0.0f);
    
    gptGfx->push_debug_group(ptSceneCmdBuffer, "Forward Lighting", (plVec4){0.33f, 0.02f, 0.20f, 1.0f});

    // skybox
    plRenderEnvironment* ptEnvironment = gptAsset->get_data(ptScene->tEnvironment);
    if(ptEnvironment->eMode == PL_RENDERER_SKY_MODE_SKYBOX && ptScene->tSkyboxTexture.uIndex != 0)
    {
        plMat4 tTransformMat = pl_mat4_translate_vec3(ptCamera->tPositionF);
        pl__render_view_skybox_pass(ptScene, ptSceneCmdBuffer, ptView->atViewBG[uFrameIdx], &tTransformMat, &tArea, 0);
    }
    else if(ptEnvironment->eMode == PL_RENDERER_SKY_MODE_REALISTIC)
    {
        plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice, sizeof(uint32_t) * 16);
        uint32_t* ptDynamicData = (uint32_t*)tDynamicBinding.pcData;
        *ptDynamicData = 0; // camera index
        gptGfx->bind_shader(ptSceneCmdBuffer, ptScene->tSkyShader);
        plBindGroupHandle atSkyBGs[] = {ptScene->atSceneBindGroups[uFrameIdx], ptView->atViewBG[uFrameIdx], ptView->atSkyBG2[uFrameIdx] };
        gptGfx->bind_graphics_bind_groups(ptSceneCmdBuffer, ptScene->tSkyShader, 0, PL_ARRAYSIZE(atSkyBGs), atSkyBGs, 1, &tDynamicBinding);
        plDraw tDraw = {
            .uInstanceCount = 1,
            .uVertexCount = 3
        };
        gptGfx->draw(ptSceneCmdBuffer, 1, &tDraw);
    }
    
    plForwardPassInfo tForwardPassInfo = {
        .sbuVisibleEntities = ptScene->sbuVisibleForwardEntities,
        .ptArea = &tArea,
        .uGlobalIndex = 0,
        .sbtShaders = ptScene->sbtRegularShaders
    };
    pl__render_view_forward_pass(ptScene, ptSceneCmdBuffer, ptView->atViewBG[uFrameIdx], &tForwardPassInfo);

    gptGfx->pop_debug_group(ptSceneCmdBuffer);

    // end main render pass
    gptGfx->producer_barrier(ptSceneCmdBuffer, PL_PIPELINE_STAGE_FRAGMENT | PL_PIPELINE_STAGE_VERTEX, PL_PIPELINE_STAGE_VERTEX | PL_PIPELINE_STAGE_COMPUTE, PL_BARRIER_SCOPE_ALL);
    gptGfx->end_render_pass(ptSceneCmdBuffer);

    // blit for transmission
    //-----------------------------------------------------------------------------

    // if transmission is required, we will blit whole screen to
    // offscreen texture used for transmission calculations.
    // The separation of the command buffers is just for sync
    // purposes & should probably be reworked soon.
    if(ptView->ptParentScene->tInternalFlags & PL_SCENE_INTERNAL_FLAG_TRANSMISSION_REQUIRED)
    {
        gptGfx->end_command_recording(ptSceneCmdBuffer);

        const plSubmitInfo tScenePreSubmitInfo = {
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
            .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
        };
        gptGfx->submit_command_buffer(ptSceneCmdBuffer, &tScenePreSubmitInfo);
        gptGfx->return_command_buffer(ptSceneCmdBuffer);

        ptSceneCmdBuffer = pl__render_view_full_screen_blit(ptView);
    }

    // secondary render pass work (transmission & post processing effects)
    //-----------------------------------------------------------------------------

    plRenderInfo tTransparentRenderInfo = {
        .tRenderArea = {
            .tMin = {0},
            .tMax = ptView->tTargetSize
        },
        .atColorAttachments = {
            {
                .tTexture       = ptView->tRawOutputTexture,
                .eLoadOp        = PL_LOAD_OP_LOAD,
                .eStoreOp       = PL_STORE_OP_STORE,
                .eUsage         = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .tClearColor    = {0.0f, 0.0f, 0.0f, 0.0f}
            }
        },
        .tDepthAttachment = {
            .tTexture        = ptView->tDepthTexture,
            .eLoadOp         = PL_LOAD_OP_LOAD,
            .eStoreOp        = PL_STORE_OP_STORE,
            .eUsage          = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
            .fClearZ         = 0.0f
        },
        .tStencilAttachment = {
            .tTexture        = ptView->tDepthTexture,
            .eLoadOp         = PL_LOAD_OP_LOAD,
            .eStoreOp        = PL_STORE_OP_STORE,
            .eUsage          = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
            .uClearStencil   = 0
        }
    };

    gptGfx->begin_render_pass(ptSceneCmdBuffer, &tTransparentRenderInfo, NULL);
    gptGfx->set_depth_bias(ptSceneCmdBuffer, 0.0f, 0.0f, 0.0f);
    
    if(pl_sb_size(ptScene->sbuVisibleTransmissionEntities) > 0)
        pl__render_view_transmission_pass(ptView, ptSceneCmdBuffer, ptView->atViewBG[uFrameIdx]);

    if(ptView->tEditorOptions.bShowGrid)
        pl__render_view_grid_pass(ptView, ptSceneCmdBuffer, ptCamera);

    gptGfx->end_render_pass(ptSceneCmdBuffer);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~entity selection~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    if(ptView->bRequestHoverCheck)
        pl__render_view_pick_pass(ptView, ptView->atViewBG[uFrameIdx], ptSceneCmdBuffer);

    // finished core rendering work (mostly separating command buffers for sync purposes)
    gptGfx->end_command_recording(ptSceneCmdBuffer);

    const plSubmitInfo tSceneSubmitInfo = {
        .uWaitSemaphoreCount     = 1,
        .atWaitSempahores        = {gptStarter->get_current_timeline_semaphore()},
        .auWaitSemaphoreValues   = {gptStarter->get_current_timeline_value()},
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
    };
    gptGfx->submit_command_buffer(ptSceneCmdBuffer, &tSceneSubmitInfo);
    gptGfx->return_command_buffer(ptSceneCmdBuffer);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~jump flood outline work~~~~~~~~~~~~~~~~~~~~~~~~~~~

    pl__render_view_uv_pass(ptView);
    pl__render_view_jfa_pass(ptView);
    pl__render_view_outline_pass(ptView, ptCamera);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~post processing work~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    if(ptSettings->tBloom.tFlags & PL_RENDERER_BLOOM_FLAGS_ACTIVE && pl_sb_size(ptView->sbtBloomDownChain) >= ptSettings->tBloom.uChainLength)
        pl__render_view_bloom_pass(ptView);
    else if(!(ptSettings->tBloom.tFlags & PL_RENDERER_BLOOM_FLAGS_ACTIVE))
    {
        for(uint32_t i = 0; i < pl_sb_size(ptView->sbtBloomDownChain); i++)
        {
            gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->sbtBloomDownChain[i]);
            gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->sbtBloomUpChain[i]);
        }
        pl_sb_reset(ptView->sbtBloomDownChain);
        pl_sb_reset(ptView->sbtBloomUpChain);
    }

    // TODO: move before outline
    if(ptEnvironment->eMode == PL_RENDERER_SKY_MODE_REALISTIC && ptEnvironment->eFlags & PL_RENDERER_SKY_FLAGS_AERIAL_PERSPECTIVE)
        pl__render_view_aerial_pass(ptView);

    pl__render_view_tonemap_pass(ptView);

    // add debug stuff after post processing so things like bloom & color don't get
    // affected
    pl__render_view_debug_pass(ptView, ptCamera, ptCullCamera);

    // update stats
    static double* pdVisibleOpaqueObjects = NULL;
    static double* pdVisibleTransparentObjects = NULL;

    // recording draw call stats
    *gptData->pdDrawCalls += (double)(pl_sb_size(ptScene->sbuVisibleDeferredEntities) + pl_sb_size(ptScene->sbuVisibleForwardEntities) + pl_sb_size(ptScene->sbuVisibleTransmissionEntities) + 1);
    
    if(!pdVisibleOpaqueObjects)
    {
        pdVisibleOpaqueObjects = gptStats->get_counter("visible deferred objects");
        pdVisibleTransparentObjects = gptStats->get_counter("visible forward objects");
    }

    // only record stats for first scene
    if(ptScene == gptData->sbptScenes[0])
    {
        *pdVisibleOpaqueObjects = (double)(pl_sb_size(ptScene->sbuVisibleDeferredEntities));
        *pdVisibleTransparentObjects = (double)(pl_sb_size(ptScene->sbuVisibleForwardEntities));
    }

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

void
pl_renderer_debug_draw_lights(plView* ptView, const plLightComponent* sbtLights, uint32_t uLightCount)
{
    for(uint32_t i = 0; i < uLightCount; i++)
    {
        if(sbtLights[i].tFlags & PL_LIGHT_FLAG_VISUALIZER)
        {
            const plVec4 tColor = {.rgb = sbtLights[i].tColor, .a = 1.0f};
            if(sbtLights[i].tType == PL_LIGHT_TYPE_POINT)
            {
                plSphere tSphere = {
                    .fRadius = sbtLights[i].fRadius,
                    .tCenter = sbtLights[i].tPosition
                };
                gptDraw->add_3d_sphere(ptView->pt3DDrawList, tSphere, 6, 6, (plDrawLineOptions){.uColor = PL_COLOR_32_VEC4(tColor), .fThickness = 0.005f});
                tSphere.fRadius = sbtLights[i].fRange;
                plSphere tSphere2 = {
                    .fRadius = sbtLights[i].fRange,
                    .tCenter = sbtLights[i].tPosition
                };
                gptDraw->add_3d_sphere(ptView->pt3DDrawList, tSphere2, 0, 0, (plDrawLineOptions){.uColor = PL_COLOR_32_VEC4(tColor), .fThickness = 0.01f});
            }
            else if(sbtLights[i].tType == PL_LIGHT_TYPE_SPOT)
            {
                plCone tCone0 = {
                    .fRadius = tanf(sbtLights[i].fOuterConeAngle * 0.5f) * sbtLights[i].fRange,
                    .tTipPos = sbtLights[i].tPosition,
                    .tBasePos = pl_add_vec3(sbtLights[i].tPosition, pl_mul_vec3_scalarf(sbtLights[i].tDirection, sbtLights[i].fRange))
                };
                gptDraw->add_3d_cone(ptView->pt3DDrawList, tCone0, 0, (plDrawLineOptions){.uColor = PL_COLOR_32_VEC4(tColor), .fThickness = 0.01f});

                if(sbtLights[i].fInnerConeAngle > 0.0f)
                {
                    plCone tCone1 = {
                        .fRadius = tanf(sbtLights[i].fInnerConeAngle) * sbtLights[i].fRange,
                        .tTipPos = sbtLights[i].tPosition,
                        .tBasePos = pl_add_vec3(sbtLights[i].tPosition, pl_mul_vec3_scalarf(sbtLights[i].tDirection, sbtLights[i].fRange))
                    };
                    gptDraw->add_3d_cone(ptView->pt3DDrawList, tCone1, 0, (plDrawLineOptions){.uColor = PL_COLOR_32_VEC4(tColor), .fThickness = 0.01f});
                }
            }

            else if(sbtLights[i].tType == PL_LIGHT_TYPE_DIRECTIONAL)
            {
                plVec3 tDirection = pl_norm_vec3(sbtLights[i].tDirection);
                plCone tCone0 = {
                    .fRadius = 0.125f,
                    .tBasePos = (plVec3){0.0f, 3.0f, 0.0f},
                    .tTipPos = pl_add_vec3((plVec3){0.0f, 3.0f, 0.0f}, pl_mul_vec3_scalarf(tDirection, 0.25f))
                };
                gptDraw->add_3d_cone(ptView->pt3DDrawList, tCone0, 0, (plDrawLineOptions){.uColor = PL_COLOR_32_VEC4(tColor), .fThickness = 0.01f});
                plCylinder tCylinder = {
                    .fRadius = 0.0625f,
                    .tBasePos = tCone0.tBasePos,
                    .tTipPos = pl_add_vec3((plVec3){0.0f, 3.0f, 0.0f}, pl_mul_vec3_scalarf(tDirection, -0.25f))
                };
                gptDraw->add_3d_cylinder(ptView->pt3DDrawList, tCylinder, 0, (plDrawLineOptions){.uColor = PL_COLOR_32_VEC4(tColor), .fThickness = 0.01f});
            }
        }
    }
}

void
pl_renderer_debug_draw_all_bound_boxes(plView* ptView)
{
    plRenderScene* ptScene = ptView->ptParentScene;
    const uint32_t uDrawableCount = pl_sb_size(ptScene->sbtDrawables);
    for(uint32_t i = 0; i < uDrawableCount; i++)
    {
        plObjectComponent* ptObject = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtDrawables[i].tEntity);

        plMesh* ptMesh = gptAsset->get_data(ptObject->tMesh);
        gptDraw->add_3d_aabb(ptView->pt3DDrawList, ptObject->tAABB.tMin, ptObject->tAABB.tMax, (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(1.0f, 0.2f, 0.0f), .fThickness = 0.02f});
    }
}

void
pl_renderer_debug_draw_bvh(plView* ptView)
{
    plRenderScene* ptScene = ptView->ptParentScene;
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, "draw BVH");

    plObjectComponent* ptComponents = NULL;
    gptEcs->get_components(ptScene->ptComponentLibrary, gptData->tObjectComponentType, (void**)&ptComponents, NULL);

    plBVHNode* ptNode = NULL;
    uint32_t uLeafIndex = UINT32_MAX;
    while (gptBvh->traverse(&ptScene->tBvh, &ptNode, &uLeafIndex))
    {
        if(uLeafIndex != UINT32_MAX)
        {
            plObjectComponent* ptObject = &ptComponents[uLeafIndex];
            gptDraw->add_3d_aabb(ptView->pt3DDrawList, ptObject->tAABB.tMin, ptObject->tAABB.tMax, (plDrawLineOptions){.fThickness = 0.02f, .uColor = PL_COLOR_32_DARK_BLUE});
        }
        else
        {
            gptDraw->add_3d_aabb(ptView->pt3DDrawList, ptNode->tAABB.tMin, ptNode->tAABB.tMax, (plDrawLineOptions){.fThickness = 0.02f, .uColor = PL_COLOR_32_WHITE});
        }
    }
    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

bool
pl_renderer_begin_frame(void)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    plDevice* ptDevice = gptData->ptDevice;

    gptStage->flush();
    gptGfx->reset_bind_group_pool(gptData->aptTempGroupPools[gptGfx->get_current_frame_index()]);
    gptData->tCurrentDynamicDataBlock = gptGfx->allocate_dynamic_data_block(gptData->ptDevice);

    if(gptData->bShowTools)
    {
        for(uint32_t uSceneIndex = 0; uSceneIndex < pl_sb_size(gptData->sbptScenes); uSceneIndex++)
        {
            plRenderScene* ptScene = gptData->sbptScenes[uSceneIndex];
            if(!(ptScene->tInternalFlags & PL_SCENE_INTERNAL_FLAG_ACTIVE))
                continue;

            if(gptUI->begin_window("Renderer Extension", &gptData->bShowTools, 0))
            {
                const float pfRatios[] = {1.0f};
                const float pfRatios2[] = {0.5f, 0.5f};
                gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);

                if(gptUI->begin_collapsing_header("Information", 0))
                {
                    gptUI->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
                    gptUI->text("Graphics Backend: %s", gptGfx->get_backend_string());
                    gptUI->end_collapsing_header();
                }

                static const char* apcShaderDebugModeText[] = {
                    "None",
                    "Base Color",
                    "Metallic",
                    "Roughness",
                    "Alpha",
                    "Emissive",
                    "Occlusion",
                    "Shading Normal",
                    "Texture Normal",
                    "Geometry Normal",
                    "Geometry Tangent",
                    "Geometry Bitangent",
                    "UV 0",
                    "Clearcoat",
                    "Clearcoat Roughness",
                    "Clearcoat Normal",
                    "Sheen Color",
                    "Sheen Roughness",
                    "Iridescence Factor",
                    "Iridescence Thickness",
                    "Anisotropy Strength",
                    "Anisotropy Direction",
                    "Transmission Strength",
                    "Volume Thickness",
                    "Diffuse Transmission Strength",
                    "Diffuse Transmission Color",
                };
                bool abShaderDebugMode[PL_ARRAYSIZE(apcShaderDebugModeText)] = {0};
                abShaderDebugMode[ptScene->tDebugOptions.tShaderDebugMode] = true;
                if(gptUI->begin_combo("Shader Debug Mode", apcShaderDebugModeText[ptScene->tDebugOptions.tShaderDebugMode], PL_UI_COMBO_FLAGS_HEIGHT_REGULAR))
                {
                    plRenderSettings* ptSettings = gptAsset->get_data(ptScene->tSettings);
                    for(uint32_t i = 0; i < PL_ARRAYSIZE(apcShaderDebugModeText); i++)
                    {
                        if(gptUI->selectable(apcShaderDebugModeText[i], &abShaderDebugMode[i], 0))
                        {
                            if(i == 0)
                                ptSettings->tTonemap.tMode = PL_TONEMAP_MODE_SIMPLE;
                            else
                                ptSettings->tTonemap.tMode = PL_TONEMAP_MODE_NONE;
                            ptScene->tDebugOptions.tShaderDebugMode = i;
                            gptUI->close_current_popup();
                            pl_renderer_editor_reload_scene_shaders(ptScene);
                        } 
                    }
                    gptUI->end_combo();
                }
                gptUI->end_window();
            }
        }
    }


    // perform GPU buffer updates
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    plTimelineSemaphore* tSemHandle = gptStarter->get_current_timeline_semaphore();

    for(uint32_t uSceneIndex = 0; uSceneIndex < pl_sb_size(gptData->sbptScenes); uSceneIndex++)
    {
        plRenderScene* ptScene = gptData->sbptScenes[uSceneIndex];
        if(!(ptScene->tInternalFlags & PL_SCENE_INTERNAL_FLAG_ACTIVE))
            continue;


        plRenderEnvironment* ptEnvironment = gptAsset->get_data(ptScene->tEnvironment);
        if(ptEnvironment->eFlags & PL_RENDERER_SKY_FLAGS_LUTS_DIRTY)
        {
            pl__renderer_scene_create_sky_luts_textures(ptScene);
            pl__renderer_scene_update_sky_luts_bindgroups(ptScene);
        }

        if(ptEnvironment->eMode == PL_RENDERER_SKY_MODE_SKYBOX && ptEnvironment->eFlags & PL_RENDERER_SKY_FLAGS_SKYBOX_DIRTY)
        {
            if(gptGfx->is_texture_valid(ptDevice, ptScene->tSkyboxTexture))
                gptGfx->queue_texture_for_deletion(ptDevice, ptScene->tSkyboxTexture);
            plTextureAsset* ptSkyboxTexture = gptAsset->get_data(ptEnvironment->tSkyboxTexture);
            pl__renderer_scene_load_skybox_from_panorama(ptScene, ptSkyboxTexture->pcSourceFile, ptEnvironment->uSkyboxResolution);
            ptEnvironment->eFlags &= ~PL_RENDERER_SKY_FLAGS_SKYBOX_DIRTY;
            ptScene->tFlags |= PL_RENDERER_SCENE_FLAGS_ALL_PROBES_DIRTY;
        }
        

        if(ptScene->uMaterialDirtyValue > 0)
        {
            const uint32_t uDrawableCount = pl_sb_size(ptScene->sbtDrawables);
            for(uint32_t i = 0; i < uDrawableCount; i++)
            {
                plObjectComponent* ptObject = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtDrawables[i].tEntity);
                plMesh* ptMesh = gptAsset->get_data(ptObject->tMesh);

                uint32_t uMaterialIndex = (uint32_t)pl_hm_lookup(&ptScene->tMaterialHashmap, ptMesh->atSubmeshes[ptScene->sbtDrawables[i].uSubmeshIndex].tMaterial.uData);
                ptScene->sbtDrawables[i].uMaterialIndex = (uint32_t)ptScene->sbtMaterialNodes[uMaterialIndex]->uOffset / sizeof(plGpuMaterial);
            }
            ptScene->uMaterialDirtyValue = 0;
        }

        if(ptScene->tInternalFlags & PL_SCENE_INTERNAL_FLAG_OBJECT_COUNT_DIRTY)
        {
            ptScene->tInternalFlags &= ~PL_SCENE_INTERNAL_FLAG_OBJECT_COUNT_DIRTY;
            const uint32_t uDrawableCount = pl_sb_size(ptScene->sbtDrawables);

            // sort drawables by mesh to prepare for instancing (consecutive drawables with same mesh will be instanced together)
            for (uint32_t i = 0; i < uDrawableCount; i++)
            {
                ptScene->sbtDrawables[i].uInstanceCount = 1;
                ptScene->sbtDrawables[i].uTransformIndex = ptScene->uNextTransformIndex++;
                // plObjectComponent* ptObjectA = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtDrawables[i].tEntity);
                // for (uint32_t j = i; j < uDrawableCount - 1; j++)
                // {
                //     plObjectComponent* ptObjectB = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtDrawables[j + 1].tEntity);
                //     if(ptObjectA->tMesh.uData == ptObjectB->tMesh.uData)
                //     {
                //         // increment instance count of first drawable
                //         ptScene->sbtDrawables[i].uInstanceCount++;

                //         // set instance count of duplicate drawable to 0 so it won't be rendered separately
                //         ptScene->sbtDrawables[j + 1].uInstanceCount = 0;
                //         ptScene->sbtDrawables[j + 1].uTransformIndex = ptScene->uNextTransformIndex++;
                //     }
                //     else // found end of duplicates
                //         break;
                    
                // }
                i += ptScene->sbtDrawables[i].uInstanceCount;
                i--;

            }

            // free CPU buffers (keep drawable list since it contains per-drawable metadata like material index and mesh info)
            pl_sb_free(ptScene->sbtVertexPosBuffer);
            pl_sb_free(ptScene->sbtVertexDataBuffer);
            pl_sb_free(ptScene->sbuIndexBuffer);
            pl_sb_free(ptScene->sbtSkinVertexDataBuffer);
        }

        for(uint32_t uViewIndex = 0; uViewIndex < pl_sb_size(ptScene->sbptViews); uViewIndex++)
        {
            plView* ptView = ptScene->sbptViews[uViewIndex];

            if(ptEnvironment->eFlags & PL_RENDERER_SKY_FLAGS_LUTS_DIRTY)
            {
                pl__renderer_view_create_sky_luts_textures(ptView);
                pl__renderer_view_update_sky_luts_bindgroups(ptView);
            }

            if(ptView->auHoverResultProcessing[uFrameIdx])
            {
                PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, "Picking Retrieval");
                
                plBuffer* ptPickBuffer = gptGfx->get_buffer(ptDevice, ptView->atPickBuffer[uFrameIdx]);
                const uint32_t uNewID = *(uint32_t*)ptPickBuffer->tMemoryAllocation.pHostMapped;
                plEntity tNewEntity = gptEcs->get_current_entity(ptScene->ptComponentLibrary, (plEntity){.uIndex = uNewID});
                ptView->tHoveredEntity = tNewEntity;

                ptView->auHoverResultProcessing[uFrameIdx] = false;
                ptView->auHoverResultReady[uFrameIdx] = true;
                memset(ptPickBuffer->tMemoryAllocation.pHostMapped, 0, sizeof(uint32_t) * 2);
        
                PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
            }
        }

        ptEnvironment->eFlags &= ~PL_RENDERER_SKY_FLAGS_LUTS_DIRTY;
    }

    // gptStarter->return_readback_buffer(&gptData->tReadbackBuffer);
    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
    return true;
}

plDrawList3D*
pl_renderer_debug_get_drawlist(plView* ptView)
{
    return ptView->pt3DDrawList;
}

plDrawList3D*
pl_renderer_editor_get_gizmo_drawlist(plView* ptView)
{
    return ptView->pt3DGizmoDrawList;
}

plBindGroupHandle
pl_renderer_get_view_color_bind_group(plView* ptView, plVec2* ptMaxUVOut)
{
    if(ptMaxUVOut)
    {
        plTexture* ptTexture = gptGfx->get_texture(gptData->ptDevice, ptView->tFinalTexture);
        *ptMaxUVOut = (plVec2){ptView->tTargetSize.x / ptTexture->tDesc.tDimensions.x, ptView->tTargetSize.y / ptTexture->tDesc.tDimensions.y};
    }
    return ptView->tFinalTextureHandle;
}

static bool
pl__renderer_serialize_environment(const char* pcName, const void* pEnvironment, plAssetEncoding eEncoding)
{
    return false;
}

static bool
pl__renderer_deserialize_environment(const char* pcName, void* pEnvironment)
{
    plRenderEnvironment* ptEnvironment = pEnvironment;

    if(!gptVfs->does_file_exist(pcName))
        return false;

    plVfsFileHandle tFileHandle = gptVfs->open_file(pcName, PL_VFS_FILE_MODE_READ);

    plAssetFileHeader tAssetHeader = {0};
    gptVfs->read_file_stream(tFileHandle, sizeof(plAssetFileHeader), 1, &tAssetHeader);

    if(tAssetHeader.uMagic == PL_ASSET_MAGIC)
    {
        PL_ASSERT(false && "binary environment serialization not implemented");
    }
    else // json
    {
        char acTempBuffer[256] = {0};

        gptVfs->set_file_stream_position(tFileHandle, 0);
        size_t szJsonFileSize = gptVfs->get_file_size_str(pcName);
        uint8_t* puFileBuffer = (uint8_t*)PL_ALLOC(szJsonFileSize + 1);
        memset(puFileBuffer, 0, szJsonFileSize + 1);
        
        gptVfs->read_file(tFileHandle, puFileBuffer, &szJsonFileSize);

        plJsonObject* ptRoot = NULL;
        gptJson->load((const char*)puFileBuffer, &ptRoot);

        plJsonObject* ptJsonSun = gptJson->member(ptRoot, "sun");
        if(ptJsonSun)
        {
            gptJson->float_array_member(ptJsonSun, "color", ptEnvironment->tSunColor.d, NULL);
            gptJson->float_array_member(ptJsonSun, "direction", ptEnvironment->tSunDirection.d, NULL);
            ptEnvironment->fSunIntensity = gptJson->float_member(ptJsonSun, "intensity", 1.0f);
            if(gptJson->bool_member(ptJsonSun, "cast_shadows", false))
            {
                ptEnvironment->eFlags |= PL_RENDERER_SKY_FLAGS_SHADOWS;
            }
            else
            {
                ptEnvironment->eFlags &= ~PL_RENDERER_SKY_FLAGS_SHADOWS;
            }
        }

        plJsonObject* ptJsonSky = gptJson->member(ptRoot, "sky");
        if(ptJsonSky)
        {
            gptJson->string_member(ptJsonSky, "mode", acTempBuffer, 256);
            if     (acTempBuffer[0] == 's') ptEnvironment->eMode = PL_RENDERER_SKY_MODE_SKYBOX;
            else if(acTempBuffer[0] == 'r') ptEnvironment->eMode = PL_RENDERER_SKY_MODE_REALISTIC;
            ptEnvironment->eFlags |= PL_RENDERER_SKY_FLAGS_SKYBOX_DIRTY;

            if(ptEnvironment->eMode == PL_RENDERER_SKY_MODE_SKYBOX)
            {
                plJsonObject* ptJsonSkybox = gptJson->member(ptJsonSky, "skybox");
                if(ptJsonSkybox)
                {
                    gptJson->string_member(ptJsonSkybox, "texture", acTempBuffer, 256);
                    ptEnvironment->tSkyboxTexture = gptAsset->load(acTempBuffer);
                    ptEnvironment->uSkyboxResolution = gptJson->uint32_member(ptJsonSkybox, "resolution", 1024);
                    ptEnvironment->eFlags |= PL_RENDERER_SKY_FLAGS_SKYBOX_DIRTY;
                }
            }
            else if(ptEnvironment->eMode == PL_RENDERER_SKY_MODE_REALISTIC)
            {
                ptEnvironment->eFlags |= PL_RENDERER_SKY_FLAGS_LUTS_DIRTY;
                plJsonObject* ptJsonRealistic = gptJson->member(ptJsonSky, "realistic");
                if(ptJsonRealistic)
                {
                    ptEnvironment->fAtmosphereConversion = gptJson->float_member(ptJsonRealistic, "atmosphere_scale", 0.001f);
                    ptEnvironment->fSunRadius = gptJson->float_member(ptJsonRealistic, "sun_radius", 0.00465f);
                    ptEnvironment->fPlanetRadius = gptJson->float_member(ptJsonRealistic, "planet_radius", 6371.0f);
                    ptEnvironment->fAtmosphereHeight = gptJson->float_member(ptJsonRealistic, "atmosphere_height", 100.0f);

                    ptEnvironment->fScatteringMieGround = gptJson->float_member(ptJsonRealistic, "mie_scattering", 0.006f);
                    ptEnvironment->fExtinctionMieGround = gptJson->float_member(ptJsonRealistic, "mie_extinction", 0.00666f);
                    ptEnvironment->fMieScatteringExponent = gptJson->float_member(ptJsonRealistic, "mie_exponent", 0.76f);

                    gptJson->float_array_member(ptJsonRealistic, "rayleigh_scattering", ptEnvironment->tScatteringRayleighGround.d, NULL);
                    gptJson->float_array_member(ptJsonRealistic, "rayleigh_extinction", ptEnvironment->tExtinctionRayleighGround.d, NULL);
                    gptJson->float_array_member(ptJsonRealistic, "ozone_extinction", ptEnvironment->tOzoneExtinction.d, NULL);

                    if(gptJson->bool_member(ptJsonRealistic, "multiscatter", true))
                    {
                        ptEnvironment->eFlags |= PL_RENDERER_SKY_FLAGS_MULTISCATTER;
                    }
                    else
                    {
                        ptEnvironment->eFlags &= ~PL_RENDERER_SKY_FLAGS_MULTISCATTER;
                    }
                    
                    plJsonObject* ptJsonAerial = gptJson->member(ptJsonRealistic, "aerial_perspective");
                    if(ptJsonAerial)
                    {
                        ptEnvironment->fMaxAerialDistance = gptJson->float_member(ptJsonAerial, "max_distance", 10.0f);
                        ptEnvironment->fAerialDepthExponent = gptJson->float_member(ptJsonAerial, "depth_exponent", 2.0f);
                        ptEnvironment->uAerialSamplesPerSlice = gptJson->uint32_member(ptJsonAerial, "samples_per_slice", 8);

                        if(gptJson->bool_member(ptJsonAerial, "enabled", true))
                        {
                            ptEnvironment->eFlags |= PL_RENDERER_SKY_FLAGS_AERIAL_PERSPECTIVE;
                        }
                        else
                        {
                            ptEnvironment->eFlags &= ~PL_RENDERER_SKY_FLAGS_AERIAL_PERSPECTIVE;
                        }
                    }
                }
            }
        }
        gptJson->unload(&ptRoot);
        PL_FREE(puFileBuffer);
    }

    gptVfs->close_file(tFileHandle);
    return true;
}

static bool
pl__renderer_serialize_settings(const char* pcName, const void* pSettings, plAssetEncoding eEncoding)
{
    return false;
}

static bool
pl__renderer_deserialize_settings(const char* pcName, void* pSettings)
{
    plRenderSettings* ptSettings = pSettings;

    if(!gptVfs->does_file_exist(pcName))
        return false;

    plVfsFileHandle tFileHandle = gptVfs->open_file(pcName, PL_VFS_FILE_MODE_READ);

    plAssetFileHeader tAssetHeader = {0};
    gptVfs->read_file_stream(tFileHandle, sizeof(plAssetFileHeader), 1, &tAssetHeader);

    if(tAssetHeader.uMagic == PL_ASSET_MAGIC)
    {
        PL_ASSERT(false && "binary environment serialization not implemented");
    }
    else // json
    {
        char acTempBuffer[256] = {0};

        gptVfs->set_file_stream_position(tFileHandle, 0);
        size_t szJsonFileSize = gptVfs->get_file_size_str(pcName);
        uint8_t* puFileBuffer = (uint8_t*)PL_ALLOC(szJsonFileSize + 1);
        memset(puFileBuffer, 0, szJsonFileSize + 1);
        
        gptVfs->read_file(tFileHandle, puFileBuffer, &szJsonFileSize);

        plJsonObject* ptRoot = NULL;
        gptJson->load((const char*)puFileBuffer, &ptRoot);

        plJsonObject* ptJsonLighting = gptJson->member(ptRoot, "lighting");
        if(ptJsonLighting)
        {
            if(gptJson->bool_member(ptJsonLighting, "image_based", false))
                ptSettings->tLighting.tFlags |= PL_RENDERER_LIGHTING_FLAGS_IMAGE_BASED;
            else
                ptSettings->tLighting.tFlags &= ~PL_RENDERER_LIGHTING_FLAGS_IMAGE_BASED;

            if(gptJson->bool_member(ptJsonLighting, "normal_mapping", true))
                ptSettings->tLighting.tFlags |= PL_RENDERER_LIGHTING_FLAGS_NORMAL_MAPPING;
            else
                ptSettings->tLighting.tFlags &= ~PL_RENDERER_LIGHTING_FLAGS_NORMAL_MAPPING;

            if(gptJson->bool_member(ptJsonLighting, "punctual_lights", true))
                ptSettings->tLighting.tFlags |= PL_RENDERER_LIGHTING_FLAGS_PUNCTUAL_LIGHTS;
            else
                ptSettings->tLighting.tFlags &= ~PL_RENDERER_LIGHTING_FLAGS_PUNCTUAL_LIGHTS;

            if(gptJson->bool_member(ptJsonLighting, "shadows", true))
                ptSettings->tLighting.tFlags &= ~PL_RENDERER_LIGHTING_FLAGS_NO_SHADOWS;
            else
                ptSettings->tLighting.tFlags |= PL_RENDERER_LIGHTING_FLAGS_NO_SHADOWS;     
        }

        plJsonObject* ptJsonSky = gptJson->member(ptRoot, "sky");
        if(ptJsonSky)
        {
            gptJson->float_array_member(ptJsonSky, "sky_lut_resolution", ptSettings->tSky.tSkyLutResolution.d, NULL);
            gptJson->float_array_member(ptJsonSky, "transmission_lut_resolution", ptSettings->tSky.tTransmissionLutResolution.d, NULL);
            gptJson->float_array_member(ptJsonSky, "multiscatter_lut_resolution", ptSettings->tSky.tMultiscatterLutResolution.d, NULL);
            gptJson->float_array_member(ptJsonSky, "aerial_lut_resolution", ptSettings->tSky.tAerialLutResolution.d, NULL);
        }

        plJsonObject* ptJsonShadows = gptJson->member(ptRoot, "shadows");
        if(ptJsonShadows)
        {
            ptSettings->tShadows.fMaxShadowRange = gptJson->float_member(ptJsonShadows, "max_range", 100.0f);
            ptSettings->tShadows.fConstantDepthBias = gptJson->float_member(ptJsonShadows, "constant_bias", -1.25f);
            ptSettings->tShadows.fSlopeDepthBias = gptJson->float_member(ptJsonShadows, "slope_bias", -10.75f);
            ptSettings->tShadows.uShadowResolution = gptJson->uint32_member(ptJsonShadows, "resolution", 2048);
            ptSettings->tShadows.uShadowCascadeCount = gptJson->uint32_member(ptJsonShadows, "cascade_count", 4);


            if(gptJson->bool_member(ptJsonShadows, "pcf", true))
                ptSettings->tShadows.tFlags |= PL_RENDERER_SHADOW_FLAGS_PCF;
            else
                ptSettings->tShadows.tFlags &= ~PL_RENDERER_SHADOW_FLAGS_PCF;

            if(gptJson->bool_member(ptJsonShadows, "multiviewport", true))
                ptSettings->tShadows.tFlags |= PL_RENDERER_SHADOW_FLAGS_MULTI_VIEWPORT;
            else
                ptSettings->tShadows.tFlags &= ~PL_RENDERER_SHADOW_FLAGS_MULTI_VIEWPORT;
        }

        plJsonObject* ptJsonBloom = gptJson->member(ptRoot, "bloom");
        if(ptJsonBloom)
        {
            ptSettings->tBloom.fRadius = gptJson->float_member(ptJsonBloom, "radius", 1.5f);
            ptSettings->tBloom.fStrength = gptJson->float_member(ptJsonBloom, "strength", 0.05f);
            ptSettings->tBloom.uChainLength = gptJson->uint32_member(ptJsonBloom, "chain_length", 5);

            if(gptJson->bool_member(ptJsonBloom, "enabled", false))
                ptSettings->tBloom.tFlags |= PL_RENDERER_BLOOM_FLAGS_ACTIVE;
            else
                ptSettings->tBloom.tFlags &= ~PL_RENDERER_BLOOM_FLAGS_ACTIVE;
        }

        plJsonObject* ptJsonFog = gptJson->member(ptRoot, "fog");
        if(ptJsonFog)
        {
            if(gptJson->bool_member(ptJsonFog, "enabled", false))
                ptSettings->tFog.tFlags |= PL_RENDERER_FOG_FLAGS_ACTIVE;
            else
                ptSettings->tFog.tFlags &= ~PL_RENDERER_FOG_FLAGS_ACTIVE;

            gptJson->string_member(ptJsonFog, "mode", acTempBuffer, 256);
            if     (acTempBuffer[0] == 'e') ptSettings->tFog.tMode = PL_RENDERER_FOG_MODE_EXPONENTIAL;
            else if(acTempBuffer[0] == 'l') ptSettings->tFog.tMode = PL_RENDERER_FOG_MODE_LINEAR;

            ptSettings->tFog.fStart = gptJson->float_member(ptJsonFog, "start", 1.0f);
            ptSettings->tFog.fCutOffDistance = gptJson->float_member(ptJsonFog, "cutoff_distance", 1000.0f);
            gptJson->float_array_member(ptJsonFog, "color", ptSettings->tFog.tColor.d, NULL);

            ptSettings->tFog.fDensity = gptJson->float_member(ptJsonFog, "density", 0.1f);
            ptSettings->tFog.fHeight = gptJson->float_member(ptJsonFog, "height", 0.0f);
            ptSettings->tFog.fMaxOpacity = gptJson->float_member(ptJsonFog, "max_opacity", 0.1f);
            ptSettings->tFog.fHeightFalloff = gptJson->float_member(ptJsonFog, "height_falloff", 0.1f);
        }

        plJsonObject* ptJsonTonemap = gptJson->member(ptRoot, "tonemap");
        if(ptJsonTonemap)
        {
            // pbr_neutral
            // simple
            // reinhard
            // aces_narkowicz
            // aces_hill
            // aces_hill_exposure_boost
            gptJson->string_member(ptJsonTonemap, "mode", acTempBuffer, 256);
            if     (acTempBuffer[0] == 's') ptSettings->tTonemap.tMode = PL_TONEMAP_MODE_SIMPLE;
            else if(acTempBuffer[0] == 'r') ptSettings->tTonemap.tMode = PL_TONEMAP_MODE_REINHARD;
            else if(acTempBuffer[0] == 'p') ptSettings->tTonemap.tMode = PL_TONEMAP_MODE_KHRONOS_PBR_NEUTRAL;
            else if(acTempBuffer[0] == 'a')
            {
                if     (acTempBuffer[5] == 'n')  ptSettings->tTonemap.tMode = PL_TONEMAP_MODE_ACES_NARKOWICZ;
                else if(acTempBuffer[10] == 'e') ptSettings->tTonemap.tMode = PL_TONEMAP_MODE_ACES_HILL_EXPOSURE_BOOST;
                else                             ptSettings->tTonemap.tMode = PL_TONEMAP_MODE_ACES_HILL;
            }

            ptSettings->tTonemap.fExposure = gptJson->float_member(ptJsonTonemap, "exposure", 1.0f);
            ptSettings->tTonemap.fBrightness = gptJson->float_member(ptJsonTonemap, "brightness", 0.0f);
            ptSettings->tTonemap.fContrast = gptJson->float_member(ptJsonTonemap, "contrast", 1.0f);
            ptSettings->tTonemap.fSaturation= gptJson->float_member(ptJsonTonemap, "saturation", 1.0f);
        }

        PL_FREE(puFileBuffer);
        gptJson->unload(&ptRoot);
    }

    gptVfs->close_file(tFileHandle);
    return true;
}

bool
pl_renderer_ecs_add_drawable_objects_to_scene(plRenderScene* ptScene, uint32_t uObjectCount, const plEntity* atObjects)
{

    if(uObjectCount == 0)
        return true;

    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    plRenderSettings* ptSettings = gptAsset->get_data(ptScene->tSettings);

    int iSceneWideRenderingFlags = PL_RENDERING_FLAG_SHADOWS;
    if(ptSettings->tLighting.tFlags & PL_RENDERER_LIGHTING_FLAGS_NO_SHADOWS)      iSceneWideRenderingFlags &= ~PL_RENDERING_FLAG_SHADOWS;
    if(ptSettings->tLighting.tFlags & PL_RENDERER_LIGHTING_FLAGS_IMAGE_BASED)     iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_IBL;
    if(ptSettings->tLighting.tFlags & PL_RENDERER_LIGHTING_FLAGS_NORMAL_MAPPING)  iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_NORMAL_MAPS;
    if(ptSettings->tLighting.tFlags & PL_RENDERER_LIGHTING_FLAGS_PUNCTUAL_LIGHTS) iSceneWideRenderingFlags |= PL_RENDERING_FLAG_PUNCTUAL;
    if(ptSettings->tShadows.tFlags & PL_RENDERER_SHADOW_FLAGS_PCF)                iSceneWideRenderingFlags |= PL_RENDERING_FLAG_PCF_SHADOWS;
        
    ptScene->tInternalFlags |= PL_SCENE_INTERNAL_FLAG_OBJECT_COUNT_DIRTY;

    uint32_t uStart = pl_sb_size(ptScene->sbtDrawables);
    uint32_t uNewDrawablesNeeded = 0;
    for(uint32_t i = 0; i < uObjectCount; i++)
    {
        plObjectComponent* ptObject = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, atObjects[i]);
        plMesh* ptMesh = gptAsset->get_data(ptObject->tMesh);
        uNewDrawablesNeeded += ptMesh->uSubmeshCount;
    }
    pl_sb_add_n(ptScene->sbtDrawables, uNewDrawablesNeeded);
    pl_sb_add_n(ptScene->sbtDrawableResources, uNewDrawablesNeeded);
    pl_sb_add_n(ptScene->sbtRegularShaders, uNewDrawablesNeeded);
    pl_sb_add_n(ptScene->sbtShadowShaders, uNewDrawablesNeeded);
    pl_sb_add_n(ptScene->sbtProbeShaders, uNewDrawablesNeeded);
    pl_sb_add_n(ptScene->sbtOutlineShaders, uNewDrawablesNeeded);

    uint32_t uCurrentIndex = 0;
    for(uint32_t i = 0; i < uObjectCount; i++)
    {
        plObjectComponent* ptObject = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, atObjects[i]);
        plMesh* ptMesh = gptAsset->get_data(ptObject->tMesh);
        for(uint32_t j = 0; j < ptMesh->uSubmeshCount; j++)
        {
            ptScene->sbtDrawables[uStart + uCurrentIndex].tEntity = atObjects[i];
            ptScene->sbtDrawables[uStart + uCurrentIndex].uSkinIndex = UINT32_MAX;
            ptScene->sbtDrawables[uStart + uCurrentIndex].uSubmeshIndex = j;
            uCurrentIndex++;
        }
    }

    // sort to group entities for instances (slow bubble sort, improve later)
    // bool bSwapped = false;
    // for (uint32_t i = 0; i < uObjectCount - 1; i++)
    // {
    //     bSwapped = false;
    //     for (uint32_t j = 0; j < uObjectCount - i - 1; j++)
    //     {
    //         plObjectComponent* ptObjectA = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtDrawables[uStart + j].tEntity);
    //         plObjectComponent* ptObjectB = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtDrawables[uStart + j + 1].tEntity);
    //         if (ptObjectA->tMesh.uIndex > ptObjectB->tMesh.uIndex)
    //         {
    //             plEntity tA = ptScene->sbtDrawables[uStart + j].tEntity;
    //             plEntity tB = ptScene->sbtDrawables[uStart + j + 1].tEntity;
    //             ptScene->sbtDrawables[uStart + j].tEntity = tB;
    //             ptScene->sbtDrawables[uStart + j + 1].tEntity = tA;
    //             bSwapped = true;
    //         }
    //     }
      
    //     // if no two elements were swapped, then break
    //     if (!bSwapped)
    //         break;
    // }

    uCurrentIndex = 0;
    for(uint32_t i = 0; i < uObjectCount; i++)
    {
        plObjectComponent* ptObject = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, atObjects[i]);
        plMesh* ptMainMesh = gptAsset->get_data(ptObject->tMesh);
        for(uint32_t uSubmeshIndex = 0; uSubmeshIndex < ptMainMesh->uSubmeshCount; uSubmeshIndex++)
        {
            plSubmesh* ptMesh = &ptMainMesh->atSubmeshes[uSubmeshIndex];

            uint32_t uDrawableIndex = uStart + uCurrentIndex;
            uint64_t uDrawHash = pl_hm_hash(&ptScene->sbtDrawables[uDrawableIndex].tEntity.uData, sizeof(uint64_t), uSubmeshIndex);
            pl_hm_insert(&ptScene->tDrawableHashmap, uDrawHash, uDrawableIndex);

            uCurrentIndex++;

            bool bResult = pl__renderer_add_drawable_data_to_global_buffer(ptScene, uDrawableIndex, uSubmeshIndex);
            if(!bResult)
            {
                pl_sb_del_n(ptScene->sbtDrawables, uStart, uObjectCount);
                pl_sb_del_n(ptScene->sbtDrawableResources, uStart, uObjectCount);
                PL_ASSERT(false && "Renderer Buffers Full");
                PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
                return false;
            }

            plMaterial*                  ptMaterial  = gptAsset->get_data(ptMesh->tMaterial);
            plEnvironmentProbeComponent* ptProbeComp = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, ptScene->sbtDrawables[uDrawableIndex].tEntity);
            
            if(ptProbeComp)
                ptScene->sbtDrawables[uDrawableIndex].tFlags = PL_DRAWABLE_FLAG_PROBE | PL_DRAWABLE_FLAG_FORWARD;
            else // regular object
            {
                bool bForward = false;

                // use forward renderer if material is transparent
                if(ptMaterial->eAlphaMode == PL_MATERIAL_ALPHA_MODE_BLEND)
                    bForward = true;

                if(ptMaterial->eAlphaMode != PL_MATERIAL_ALPHA_MODE_OPAQUE)
                {
                    ptScene->sbtDrawables[uDrawableIndex].tFlags |= PL_DRAWABLE_FLAG_HAS_ALPHA;
                }

                // use forward renderer if material is advanced (can't fit required properties in gbuffer)
                if(
                    ptMaterial->fEmissiveStrength > 0.0f ||
                    ptMaterial->eFlags & PL_MATERIAL_FLAG_CLEARCOAT ||
                    ptMaterial->eFlags & PL_MATERIAL_FLAG_SHEEN ||
                    ptMaterial->eFlags & PL_MATERIAL_FLAG_IRIDESCENCE ||
                    ptMaterial->eFlags & PL_MATERIAL_FLAG_ANISOTROPY ||
                    ptMaterial->eFlags & PL_MATERIAL_FLAG_TRANSMISSION ||
                    ptMaterial->eFlags & PL_MATERIAL_FLAG_VOLUME ||
                    ptMaterial->eFlags & PL_MATERIAL_FLAG_DIFFUSE_TRANSMISSION ||
                    ptMaterial->eFlags & PL_MATERIAL_FLAG_DISPERSION
                )
                    bForward = true;

                // check if transmission textures are required
                if(ptMaterial->eFlags & PL_MATERIAL_FLAG_TRANSMISSION || ptMaterial->eFlags & PL_MATERIAL_FLAG_VOLUME || ptMaterial->eFlags & PL_MATERIAL_FLAG_DIFFUSE_TRANSMISSION)
                {
                    ptScene->sbtDrawables[uDrawableIndex].tFlags = PL_DRAWABLE_FLAG_TRANSMISSION;
                    ptScene->tInternalFlags |= PL_SCENE_INTERNAL_FLAG_TRANSMISSION_REQUIRED;
                }
                else if(bForward)
                {
                    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_SHEEN) // check if sheen is required (additional scene textures)
                        ptScene->tInternalFlags |= PL_SCENE_INTERNAL_FLAG_SHEEN_REQUIRED;
                    ptScene->sbtDrawables[uDrawableIndex].tFlags = PL_DRAWABLE_FLAG_FORWARD;
                }
                else
                    ptScene->sbtDrawables[uDrawableIndex].tFlags = PL_DRAWABLE_FLAG_DEFERRED;
            }

            uint64_t uMaterialIndex = pl__renderer_add_material_to_scene(ptScene, ptMesh->tMaterial);

            // ptScene->sbtDrawables[uDrawableIndex].uMaterialIndex = (uint32_t)ptScene->sbtMaterialNodes[uMaterialIndex]->uOffset/ sizeof(plGpuMaterial);
            // (uint32_t)ptScene->sbtMaterialNodes[uMaterialIndex2]->uOffset / sizeof(plGpuMaterial);

            int iDataStride = 0;
            int iFlagCopy0 = (int)ptMesh->uVertexStreamMask;
            while(iFlagCopy0)
            {
                iDataStride += iFlagCopy0 & 1;
                iFlagCopy0 >>= 1;
            }

            int iTextureMappingFlags = 0;
            for(uint32_t j = 0; j < PL_MATERIAL_TEXTURE_SLOT_COUNT; j++)
            {
                if(gptAsset->is_valid(ptMaterial->atTextures[j].tTexture))
                    iTextureMappingFlags |= 1 << j; 
            }

            int iObjectRenderingFlags = iSceneWideRenderingFlags;

            if(ptObject->tFlags & PL_OBJECT_FLAGS_RECEIVE_SHADOW)
            {
                iObjectRenderingFlags |= PL_RENDERING_FLAG_SHADOWS;
            }

            // choose shader variant
            int aiForwardFragmentConstantData0[] = {
                (int)ptMesh->uVertexStreamMask,
                iTextureMappingFlags,
                ptMaterial->eFlags,
                iObjectRenderingFlags,
                ptScene->tDebugOptions.tShaderDebugMode
            };

            int aiGBufferFragmentConstantData0[] = {
                (int)ptMesh->uVertexStreamMask,
                iTextureMappingFlags,
                ptMaterial->eFlags,
                ptScene->tDebugOptions.tShaderDebugMode,
                iObjectRenderingFlags
            };

            int aiVertexConstantData0[] = {
                (int)ptMesh->uVertexStreamMask,
                iDataStride
            };

            if(ptScene->sbtDrawables[uDrawableIndex].tFlags & PL_DRAWABLE_FLAG_DEFERRED)
            {

                plGraphicsState tVariantTemp = {
                    .bDepthWriteEnabled  = 1,
                    .eDepthMode          = PL_COMPARE_MODE_GREATER,
                    .eCullMode           = PL_CULL_MODE_CULL_BACK,
                    .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
                    .uStencilRef         = 0xff,
                    .uStencilMask        = 0xff,
                    .eStencilOpFail      = PL_STENCIL_OP_KEEP,
                    .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                    .eStencilOpPass      = PL_STENCIL_OP_KEEP,
                    .bWireframe          = ptScene->tDebugOptions.bWireframe
                };

                if(ptMaterial->eFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED)
                    tVariantTemp.eCullMode = PL_CULL_MODE_NONE;


                if(ptScene->tDebugOptions.tShaderDebugMode == PL_SHADER_DEBUG_MODE_NONE)
                {
                    aiGBufferFragmentConstantData0[3] = iObjectRenderingFlags;
                    ptScene->sbtRegularShaders[uDrawableIndex] = gptShaderVariant->get_shader("gbuffer_fill", &tVariantTemp, aiVertexConstantData0, aiGBufferFragmentConstantData0, &gptData->tDeferredLightingRenderPassLayout);
                    ptScene->sbtProbeShaders[uDrawableIndex] = gptShaderVariant->get_shader("gbuffer_fill", &tVariantTemp, aiVertexConstantData0, aiGBufferFragmentConstantData0, NULL);
                }
                else
                {
                    ptScene->sbtRegularShaders[uDrawableIndex] = gptShaderVariant->get_shader("gbuffer_fill_debug", &tVariantTemp, aiVertexConstantData0, aiGBufferFragmentConstantData0, &gptData->tDeferredLightingRenderPassLayout);
                    ptScene->sbtProbeShaders[uDrawableIndex] = gptShaderVariant->get_shader("gbuffer_fill_debug", &tVariantTemp, aiVertexConstantData0, aiGBufferFragmentConstantData0, NULL);
                }

                // write into stencil buffer
                tVariantTemp.bStencilTestEnabled = 1;
                tVariantTemp.eStencilMode        = PL_COMPARE_MODE_ALWAYS;
                tVariantTemp.uStencilRef         = 0xff;
                tVariantTemp.uStencilMask        = 0xff;
                tVariantTemp.eStencilOpFail      = PL_STENCIL_OP_REPLACE;
                tVariantTemp.eStencilOpDepthFail = PL_STENCIL_OP_REPLACE;
                tVariantTemp.eStencilOpPass      = PL_STENCIL_OP_REPLACE;
                aiGBufferFragmentConstantData0[3] = iObjectRenderingFlags;
                ptScene->sbtOutlineShaders[uDrawableIndex] = gptShaderVariant->get_shader("gbuffer_fill", &tVariantTemp, aiVertexConstantData0, aiGBufferFragmentConstantData0, &gptData->tDeferredLightingRenderPassLayout);
            }

            else if(ptScene->sbtDrawables[uDrawableIndex].tFlags & PL_DRAWABLE_FLAG_FORWARD)
            {

                plGraphicsState tVariantTemp = {
                    .bDepthWriteEnabled  = 1,
                    .eDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
                    .eCullMode           = PL_CULL_MODE_NONE,
                    .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
                    .uStencilRef         = 0xff,
                    .uStencilMask        = 0xff,
                    .eStencilOpFail      = PL_STENCIL_OP_KEEP,
                    .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                    .eStencilOpPass      = PL_STENCIL_OP_KEEP,
                    .bWireframe          = ptScene->tDebugOptions.bWireframe
                };

                if(ptMaterial->eFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED)
                    tVariantTemp.eCullMode = PL_CULL_MODE_NONE;

                if(ptScene->tDebugOptions.tShaderDebugMode == PL_SHADER_DEBUG_MODE_NONE)
                {
                    ptScene->sbtRegularShaders[uDrawableIndex] = gptShaderVariant->get_shader("forward", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, &gptData->tTransparentRenderPassLayout);
                    aiForwardFragmentConstantData0[3] = ptSettings->tLighting.tFlags & PL_RENDERER_LIGHTING_FLAGS_PUNCTUAL_LIGHTS ? PL_RENDERING_FLAG_SHADOWS : 0;
                    ptScene->sbtProbeShaders[uDrawableIndex] = gptShaderVariant->get_shader("forward", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, &gptData->tTransparentRenderPassLayout);
                }
                else
                {
                    aiForwardFragmentConstantData0[4] = ptScene->tDebugOptions.tShaderDebugMode;
                    ptScene->sbtRegularShaders[uDrawableIndex] = gptShaderVariant->get_shader("forward_debug", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, &gptData->tTransparentRenderPassLayout);
                    aiForwardFragmentConstantData0[3] = ptSettings->tLighting.tFlags & PL_RENDERER_LIGHTING_FLAGS_PUNCTUAL_LIGHTS ? PL_RENDERING_FLAG_SHADOWS : 0;
                    ptScene->sbtProbeShaders[uDrawableIndex] = gptShaderVariant->get_shader("forward_debug", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, &gptData->tTransparentRenderPassLayout);
                }


                // write into stencil buffer
                tVariantTemp.bStencilTestEnabled = 1;
                tVariantTemp.eStencilMode        = PL_COMPARE_MODE_ALWAYS;
                tVariantTemp.uStencilRef         = 0xff;
                tVariantTemp.uStencilMask        = 0xff;
                tVariantTemp.eStencilOpFail      = PL_STENCIL_OP_REPLACE;
                tVariantTemp.eStencilOpDepthFail = PL_STENCIL_OP_REPLACE;
                tVariantTemp.eStencilOpPass      = PL_STENCIL_OP_REPLACE;
                aiForwardFragmentConstantData0[3] = iObjectRenderingFlags;
                ptScene->sbtOutlineShaders[uDrawableIndex] = gptShaderVariant->get_shader("forward", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, &gptData->tTransparentRenderPassLayout);

            }

            else if(ptScene->sbtDrawables[uDrawableIndex].tFlags & PL_DRAWABLE_FLAG_TRANSMISSION)
            {

                plGraphicsState tVariantTemp = {
                    .bDepthWriteEnabled  = 1,
                    .eDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
                    .eCullMode           = PL_CULL_MODE_NONE,
                    .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
                    .uStencilRef         = 0xff,
                    .uStencilMask        = 0xff,
                    .eStencilOpFail      = PL_STENCIL_OP_KEEP,
                    .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                    .eStencilOpPass      = PL_STENCIL_OP_KEEP,
                    .bWireframe          = ptScene->tDebugOptions.bWireframe
                };

                if(ptMaterial->eFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED)
                    tVariantTemp.eCullMode = PL_CULL_MODE_NONE;

                if(ptScene->tDebugOptions.tShaderDebugMode == PL_SHADER_DEBUG_MODE_NONE)
                {
                    ptScene->sbtRegularShaders[uDrawableIndex] = gptShaderVariant->get_shader("transmission", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, &gptData->tTransparentRenderPassLayout);
                    aiForwardFragmentConstantData0[3] = ptSettings->tLighting.tFlags & PL_RENDERER_LIGHTING_FLAGS_PUNCTUAL_LIGHTS ? PL_RENDERING_FLAG_SHADOWS : 0; // remove ibl
                    ptScene->sbtProbeShaders[uDrawableIndex] = gptShaderVariant->get_shader("forward", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, &gptData->tTransparentRenderPassLayout);
                }
                else
                {
                    aiForwardFragmentConstantData0[4] = ptScene->tDebugOptions.tShaderDebugMode;
                    ptScene->sbtRegularShaders[uDrawableIndex] = gptShaderVariant->get_shader("forward_debug", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, &gptData->tTransparentRenderPassLayout);
                    aiForwardFragmentConstantData0[3] = ptSettings->tLighting.tFlags & PL_RENDERER_LIGHTING_FLAGS_PUNCTUAL_LIGHTS ? PL_RENDERING_FLAG_SHADOWS : 0; // remove ibl
                    ptScene->sbtProbeShaders[uDrawableIndex] = gptShaderVariant->get_shader("forward_debug", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, &gptData->tTransparentRenderPassLayout);
                }

                // write into stencil buffer
                tVariantTemp.bStencilTestEnabled = 1;
                tVariantTemp.eStencilMode        = PL_COMPARE_MODE_ALWAYS;
                tVariantTemp.uStencilRef         = 0xff;
                tVariantTemp.uStencilMask        = 0xff;
                tVariantTemp.eStencilOpFail      = PL_STENCIL_OP_REPLACE;
                tVariantTemp.eStencilOpDepthFail = PL_STENCIL_OP_REPLACE;
                tVariantTemp.eStencilOpPass      = PL_STENCIL_OP_REPLACE;
                aiForwardFragmentConstantData0[3] = iObjectRenderingFlags;
                ptScene->sbtOutlineShaders[uDrawableIndex] = gptShaderVariant->get_shader("transmission", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, &gptData->tTransparentRenderPassLayout);
            }

            if(ptMaterial->eAlphaMode != PL_MATERIAL_ALPHA_MODE_OPAQUE)
            {
                plGraphicsState tShadowVariant = {
                    .bDepthWriteEnabled  = 1,
                    .eDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
                    .eCullMode           = PL_CULL_MODE_NONE,
                    .bWireframe          = 0,
                    .bDepthClampEnabled  = 1,
                    .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
                    .uStencilRef         = 0xff,
                    .uStencilMask        = 0xff,
                    .eStencilOpFail      = PL_STENCIL_OP_KEEP,
                    .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                    .eStencilOpPass      = PL_STENCIL_OP_KEEP
                };
                ptScene->sbtShadowShaders[uDrawableIndex] = gptShaderVariant->get_shader("alphashadow", &tShadowVariant, aiVertexConstantData0, &aiForwardFragmentConstantData0[1], &gptData->tDepthRenderPassLayout);
            }
            ptScene->sbtDrawables[uDrawableIndex].tIndexBuffer = ptScene->sbtDrawables[uDrawableIndex].uIndexCount == 0 ? (plBufferHandle){0} : ptScene->tIndexBuffer;
        }
    }

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
    return true;
}

void
pl_renderer_ecs_update_scene_materials(plRenderScene* ptScene, uint32_t uCount, const plAssetHandle* atMaterials)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    for(uint32_t i = 0; i < uCount; i++)
    {
        plMaterial* ptMaterial = gptAsset->get_data(atMaterials[i]);

        uint32_t uMaterialIndex = (uint32_t)pl_hm_lookup(&ptScene->tMaterialHashmap, atMaterials[i].uData);
        uint32_t uOldMaterialIndex = 0;
        uint32_t uNewMaterialIndex = 0;
        plFreeListNode* ptOldNode = NULL;
        plFreeListNode* ptNewNode = NULL;
        if(uMaterialIndex == UINT32_MAX)
        {
            PL_ASSERT(false && "material isn't in scene");
            PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
            return;
        }
        else
        {
            ptOldNode = ptScene->sbtMaterialNodes[uMaterialIndex];
            ptNewNode = gptFreeList->get_node(&ptScene->tMaterialFreeList, sizeof(plGpuMaterial));
            
            gptFreeList->return_node(&ptScene->tMaterialFreeList, ptOldNode);

            ptScene->sbtMaterialNodes[uMaterialIndex] = ptNewNode;

            plGpuMaterial tGPUMaterial = {0};

            tGPUMaterial.fMetallicFactor           = ptMaterial->fMetalness;
            tGPUMaterial.fRoughnessFactor          = ptMaterial->fRoughness;
            tGPUMaterial.tBaseColorFactor          = ptMaterial->tBaseColor;
            tGPUMaterial.tEmissiveFactor           = ptMaterial->tEmissiveColor;
            tGPUMaterial.fAlphaCutoff              = ptMaterial->fAlphaCutoff;
            tGPUMaterial.fClearcoatFactor          = ptMaterial->tClearcoat.fFactor;
            tGPUMaterial.fClearcoatRoughnessFactor = ptMaterial->tClearcoat.fRoughness;
            tGPUMaterial.fSheenRoughnessFactor     = ptMaterial->tSheen.fRoughness;
            tGPUMaterial.fNormalMapStrength        = ptMaterial->fNormalMapStrength;
            tGPUMaterial.fEmissiveStrength         = ptMaterial->fEmissiveStrength;
            tGPUMaterial.tSheenColorFactor         = ptMaterial->tSheen.tColor;
            tGPUMaterial.fIridescenceFactor        = ptMaterial->tIridescence.fFactor;
            tGPUMaterial.fIridescenceIor           = ptMaterial->tIridescence.fIor;
            tGPUMaterial.fIridescenceThicknessMin  = ptMaterial->tIridescence.fThicknessMin;
            tGPUMaterial.fIridescenceThicknessMax  = ptMaterial->tIridescence.fThicknessMax;
            tGPUMaterial.tAnisotropy.x             = cosf(ptMaterial->tAnisotropy.fRotation);
            tGPUMaterial.tAnisotropy.y             = sinf(ptMaterial->tAnisotropy.fRotation);
            tGPUMaterial.tAnisotropy.z             = ptMaterial->tAnisotropy.fStrength;
            tGPUMaterial.fOcclusionStrength        = ptMaterial->fOcclusionStrength;
            tGPUMaterial.eAlphaMode                = ptMaterial->eAlphaMode;
            tGPUMaterial.fTransmissionFactor       = ptMaterial->tTransmission.fFactor;
            tGPUMaterial.fThickness                = ptMaterial->tVolume.fThickness;
            tGPUMaterial.fAttenuationDistance      = ptMaterial->tVolume.fAttenuationDistance;
            tGPUMaterial.tAttenuationColor         = ptMaterial->tVolume.tAttenuationColor;
            tGPUMaterial.fDispersion               = ptMaterial->tDispersion.fDispersion;
            tGPUMaterial.fIor                      = ptMaterial->fIor;
            tGPUMaterial.fDiffuseTransmission      = ptMaterial->tDiffuseTransmission.fFactor;
            tGPUMaterial.tDiffuseTransmissionColor = ptMaterial->tDiffuseTransmission.tColor;

            const int iDummyIndex = (int)pl__renderer_get_bindless_texture_index(ptScene, gptData->tDummyTexture);
            for(uint32_t uTextureIndex = 0; uTextureIndex < PL_MATERIAL_TEXTURE_SLOT_COUNT; uTextureIndex++)
            {
                tGPUMaterial.aiTextureUVSet[uTextureIndex] = (int)ptMaterial->atTextures[uTextureIndex].uUVSet;

                const float fSin = sinf(ptMaterial->atTextures[uTextureIndex].fRotation);
                const float fCos = cosf(ptMaterial->atTextures[uTextureIndex].fRotation);
                plMat3 tRotation = pl_create_mat3_diag(fCos, fCos, 1.0f);
                tRotation.x12 = fSin;
                tRotation.x21 = -fSin;

                const plMat3 tScale = pl_create_mat3_diag(ptMaterial->atTextures[uTextureIndex].tScale.x, ptMaterial->atTextures[uTextureIndex].tScale.y, 1.0f);

                plMat3 tTranslation = pl_create_mat3_diag(1.0f, 1.0f, 1.0f);
                tTranslation.x13 = ptMaterial->atTextures[uTextureIndex].tOffset.x;
                tTranslation.x23 = ptMaterial->atTextures[uTextureIndex].tOffset.y;

                plMat3 tTransform = pl_mul_mat3(&tRotation, &tScale);
                tTransform = pl_mul_mat3(&tTranslation, &tTransform);
                plMat4 tFinalTransform = pl_identity_mat4();
                memcpy(&tFinalTransform.col[0], &tTransform.col[0], sizeof(plVec3));
                memcpy(&tFinalTransform.col[1], &tTransform.col[1], sizeof(plVec3));
                memcpy(&tFinalTransform.col[2], &tTransform.col[2], sizeof(plVec3));

                tGPUMaterial.atTextureTransforms[uTextureIndex] = tFinalTransform;

                if(gptAsset->is_valid(ptMaterial->atTextures[uTextureIndex].tTexture))
                {
                    // const char* pcSourceFile = gptAsset->get_source_path(ptMaterial->atTextures[uTextureIndex].tTexture);
                    plTextureAsset* ptTextureAsset = gptAsset->get_data(ptMaterial->atTextures[uTextureIndex].tTexture);
                    plResourceHandle tTextureResource = gptResource->load(ptTextureAsset->pcSourceFile, PL_RESOURCE_LOAD_FLAG_BLOCK_COMPRESSED);
                    plTextureHandle tValidTexture = gptResource->get_texture(tTextureResource);
                    tGPUMaterial.aiTextureIndices[uTextureIndex] = (int)pl__renderer_get_bindless_texture_index(ptScene, tValidTexture);
                }
                else
                    tGPUMaterial.aiTextureIndices[uTextureIndex] = iDummyIndex;
            }

            ptScene->uMaterialDirtyValue = 1;
            gptStage->stage_buffer_upload(ptScene->tMaterialDataBuffer,
                ptNewNode->uOffset,
                &tGPUMaterial,
                sizeof(plGpuMaterial));
        }
    }

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

void
pl_renderer_ecs_add_materials_to_scene(plRenderScene* ptScene, uint32_t uMaterialCount, const plAssetHandle* atMaterials)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    for(uint32_t i = 0; i < uMaterialCount; i++)
    {
        pl__renderer_add_material_to_scene(ptScene, atMaterials[i]);
    }

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

plAssetHandle
pl_renderer_get_settings(plRenderScene* ptScene)
{
    if(ptScene)
        return ptScene->tSettings;
    return (plAssetHandle){0};
}

void
pl_renderer_set_settings(plRenderScene* ptScene, plAssetHandle tHandle)
{
    if(ptScene)
        ptScene->tSettings = tHandle;
}

plAssetHandle
pl_renderer_get_environment(plRenderScene* ptScene)
{
    if(ptScene)
        return ptScene->tEnvironment;
    return (plAssetHandle){0};
}

void
pl_renderer_set_environment(plRenderScene* ptScene, plAssetHandle tHandle)
{
    if(ptScene)
    {
        ptScene->tEnvironment = tHandle;
        plRenderEnvironment* ptEnvironment = gptAsset->get_data(ptScene->tEnvironment);
        ptEnvironment->eFlags |= PL_RENDERER_SKY_FLAGS_SKYBOX_DIRTY;
    }
}

void
pl_renderer_debug_get_scene_options(plRenderScene* ptScene, plRendererDebugSceneOptions* ptOptions)
{
    if(ptScene)
        *ptOptions = ptScene->tDebugOptions;
}

void
pl_renderer_debug_set_scene_options(plRenderScene* ptScene, const plRendererDebugSceneOptions* ptOptions)
{
    if(ptScene)
        ptScene->tDebugOptions = *ptOptions;
}

void
pl_renderer_editor_get_scene_options(plRenderScene* ptScene, plRendererEditorSceneOptions* ptOptions)
{
    if(ptScene)
        *ptOptions = ptScene->tEditorOptions;
}

void
pl_renderer_editor_set_scene_options(plRenderScene* ptScene, const plRendererEditorSceneOptions* ptOptions)
{
    if(ptScene)
        ptScene->tEditorOptions = *ptOptions;
}

void
pl_renderer_editor_get_view_options(plView* ptView, plRendererEditorViewOptions* ptOptions)
{
    if(ptView)
        *ptOptions = ptView->tEditorOptions;
}

void
pl_renderer_editor_set_view_options(plView* ptView, const plRendererEditorViewOptions* ptOptions)
{
    if(ptView)
        ptView->tEditorOptions = *ptOptions;
}

void
pl_renderer_debug_get_view_options(plView* ptView, plRendererDebugViewOptions* ptOptions)
{
    if(ptView)
        *ptOptions = ptView->tDebugOptions;
}

void
pl_renderer_debug_set_view_options(plView* ptView, const plRendererDebugViewOptions* ptOptions)
{
    if(ptView)
        ptView->tDebugOptions = *ptOptions;
}

void
pl_renderer_editor_rebuild_scene_bvh(plRenderScene* ptScene)
{
    plComponentLibrary* ptLibrary = ptScene->ptComponentLibrary;

    plObjectComponent* ptComponents = NULL;
    const uint32_t uObjectCount = gptEcs->get_components(ptScene->ptComponentLibrary, gptData->tObjectComponentType, (void**)&ptComponents, NULL);

    pl_sb_resize(ptScene->sbtBvhAABBs, uObjectCount);
    for(uint32_t j = 0; j < uObjectCount; j++)
    {
        plObjectComponent* ptObject = &ptComponents[j];
        ptScene->sbtBvhAABBs[j] = ptObject->tAABB;
    }

    gptBvh->build(&ptScene->tBvh, ptScene->sbtBvhAABBs, uObjectCount);

    pl_sb_reset(ptScene->sbtBvhAABBs); 
}

static void
pl__object_update_job(plInvocationData tInvoData, void* pData, void* pGroupSharedMemory)
{
    plComponentLibrary* ptLibrary = pData;

    plObjectComponent* ptComponents = NULL;
    const plEntity* ptEntities = NULL;
    const uint32_t uComponentCount = gptEcs->get_components(ptLibrary, gptData->tObjectComponentType, (void**)&ptComponents, &ptEntities);

    plObjectComponent* ptObject = &ptComponents[tInvoData.uGlobalIndex];
    plTransformComponent* ptTransform = gptEcs->get_component(ptLibrary,gptTransform->get_ecs_type_key_transform(), ptObject->tTransform);
    plMesh* ptMesh = gptAsset->get_data(ptObject->tMesh);
    plSkinComponent* ptSkinComponent = gptEcs->get_component(ptLibrary, gptSkeleton->get_ecs_type_key_skin(), ptEntities[tInvoData.uGlobalIndex]);

    plMat4 tTransform = ptTransform->tWorld;

    // calculate AABB
    ptObject->tAABB.tMax = (plVec3){-FLT_MAX, -FLT_MAX, -FLT_MAX};
    ptObject->tAABB.tMin = (plVec3){FLT_MAX, FLT_MAX, FLT_MAX};
    
    for(uint32_t u = 0; u < ptObject->uSubmeshCount; u++)
    {
        const plVec3 tVerticies2[] = {
            pl_mul_mat4_vec3(&tTransform, (plVec3){  ptMesh->atSubmeshes[ptObject->uFirstSubmesh + u].tAABB.tMin.x, ptMesh->atSubmeshes[ptObject->uFirstSubmesh + u].tAABB.tMin.y, ptMesh->atSubmeshes[ptObject->uFirstSubmesh + u].tAABB.tMin.z }),
            pl_mul_mat4_vec3(&tTransform, (plVec3){  ptMesh->atSubmeshes[ptObject->uFirstSubmesh + u].tAABB.tMax.x, ptMesh->atSubmeshes[ptObject->uFirstSubmesh + u].tAABB.tMin.y, ptMesh->atSubmeshes[ptObject->uFirstSubmesh + u].tAABB.tMin.z }),
            pl_mul_mat4_vec3(&tTransform, (plVec3){  ptMesh->atSubmeshes[ptObject->uFirstSubmesh + u].tAABB.tMax.x, ptMesh->atSubmeshes[ptObject->uFirstSubmesh + u].tAABB.tMax.y, ptMesh->atSubmeshes[ptObject->uFirstSubmesh + u].tAABB.tMin.z }),
            pl_mul_mat4_vec3(&tTransform, (plVec3){  ptMesh->atSubmeshes[ptObject->uFirstSubmesh + u].tAABB.tMin.x, ptMesh->atSubmeshes[ptObject->uFirstSubmesh + u].tAABB.tMax.y, ptMesh->atSubmeshes[ptObject->uFirstSubmesh + u].tAABB.tMin.z }),
            pl_mul_mat4_vec3(&tTransform, (plVec3){  ptMesh->atSubmeshes[ptObject->uFirstSubmesh + u].tAABB.tMin.x, ptMesh->atSubmeshes[ptObject->uFirstSubmesh + u].tAABB.tMin.y, ptMesh->atSubmeshes[ptObject->uFirstSubmesh + u].tAABB.tMax.z }),
            pl_mul_mat4_vec3(&tTransform, (plVec3){  ptMesh->atSubmeshes[ptObject->uFirstSubmesh + u].tAABB.tMax.x, ptMesh->atSubmeshes[ptObject->uFirstSubmesh + u].tAABB.tMin.y, ptMesh->atSubmeshes[ptObject->uFirstSubmesh + u].tAABB.tMax.z }),
            pl_mul_mat4_vec3(&tTransform, (plVec3){  ptMesh->atSubmeshes[ptObject->uFirstSubmesh + u].tAABB.tMax.x, ptMesh->atSubmeshes[ptObject->uFirstSubmesh + u].tAABB.tMax.y, ptMesh->atSubmeshes[ptObject->uFirstSubmesh + u].tAABB.tMax.z }),
            pl_mul_mat4_vec3(&tTransform, (plVec3){  ptMesh->atSubmeshes[ptObject->uFirstSubmesh + u].tAABB.tMin.x, ptMesh->atSubmeshes[ptObject->uFirstSubmesh + u].tAABB.tMax.y, ptMesh->atSubmeshes[ptObject->uFirstSubmesh + u].tAABB.tMax.z }),
        };
        
        for(uint32_t i = 0; i < 8; i++)
        {
            if(tVerticies2[i].x > ptObject->tAABB.tMax.x) ptObject->tAABB.tMax.x = tVerticies2[i].x;
            if(tVerticies2[i].y > ptObject->tAABB.tMax.y) ptObject->tAABB.tMax.y = tVerticies2[i].y;
            if(tVerticies2[i].z > ptObject->tAABB.tMax.z) ptObject->tAABB.tMax.z = tVerticies2[i].z;
            if(tVerticies2[i].x < ptObject->tAABB.tMin.x) ptObject->tAABB.tMin.x = tVerticies2[i].x;
            if(tVerticies2[i].y < ptObject->tAABB.tMin.y) ptObject->tAABB.tMin.y = tVerticies2[i].y;
            if(tVerticies2[i].z < ptObject->tAABB.tMin.z) ptObject->tAABB.tMin.z = tVerticies2[i].z;
        }
    }

    // merge
    if(ptSkinComponent)
    {
        if(ptSkinComponent->tAABB.tMin.x < ptObject->tAABB.tMin.x) ptObject->tAABB.tMin.x = ptSkinComponent->tAABB.tMin.x;
        if(ptSkinComponent->tAABB.tMin.y < ptObject->tAABB.tMin.y) ptObject->tAABB.tMin.y = ptSkinComponent->tAABB.tMin.y;
        if(ptSkinComponent->tAABB.tMin.z < ptObject->tAABB.tMin.z) ptObject->tAABB.tMin.z = ptSkinComponent->tAABB.tMin.z;
        if(ptSkinComponent->tAABB.tMax.x > ptObject->tAABB.tMax.x) ptObject->tAABB.tMax.x = ptSkinComponent->tAABB.tMax.x;
        if(ptSkinComponent->tAABB.tMax.y > ptObject->tAABB.tMax.y) ptObject->tAABB.tMax.y = ptSkinComponent->tAABB.tMax.y;
        if(ptSkinComponent->tAABB.tMax.z > ptObject->tAABB.tMax.z) ptObject->tAABB.tMax.z = ptSkinComponent->tAABB.tMax.z;
    }
}

void
pl_renderer_ecs_run_object_update_system(plComponentLibrary* ptLibrary)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);
    
    plObjectComponent* ptComponents = NULL;
    const uint32_t uComponentCount = gptEcs->get_components(ptLibrary, gptData->tObjectComponentType, (void**)&ptComponents, NULL);

    plAtomicCounter* ptCounter = NULL;
    plJobDesc tJobDesc = {
        .task = pl__object_update_job,
        .pData = ptLibrary
    };
    gptJob->dispatch_batch(uComponentCount, 0, tJobDesc, &ptCounter);
    gptJob->wait_for_counter(ptCounter);

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

static void
pl__ecs_object_serialize(void* pComponent, plJsonObject* ptJson)
{
    plObjectComponent* ptComponent = pComponent;
    if(gptAsset->is_valid(ptComponent->tMesh))
    {
        gptJson->add_string_member(ptJson, "mesh", gptAsset->get_path(ptComponent->tMesh));
        gptJson->add_uint32_member(ptJson, "first_submesh", ptComponent->uFirstSubmesh);
        gptJson->add_uint32_member(ptJson, "submesh_count", ptComponent->uSubmeshCount);
    }
    gptJson->add_uint64_member(ptJson, "transform", ptComponent->tTransformId);
    gptJson->add_bool_member(ptJson, "casts_shadow", ptComponent->tFlags & PL_OBJECT_FLAGS_CAST_SHADOW);
    gptJson->add_bool_member(ptJson, "receives_shadow", ptComponent->tFlags & PL_OBJECT_FLAGS_RECEIVE_SHADOW);
    gptJson->add_bool_member(ptJson, "renderable", ptComponent->tFlags & PL_OBJECT_FLAGS_RENDERABLE);
}

static void
pl__ecs_object_deserialize(plJsonObject* ptJson, void* pComponent)
{
    plObjectComponent* ptComponent = pComponent;

    char acTempBuffer0[1024] = {0};

    gptJson->string_member(ptJson, "mesh", acTempBuffer0, 1024);
    ptComponent->tMesh = gptAsset->load(acTempBuffer0);
    ptComponent->uFirstSubmesh = gptJson->uint32_member(ptJson, "first_submesh", 0);
    ptComponent->uSubmeshCount = gptJson->uint32_member(ptJson, "submesh_count", 1);

    ptComponent->tTransformId = gptJson->uint64_member(ptJson, "transform", 0);

    if(gptJson->bool_member(ptJson, "receives_shadow", true)) ptComponent->tFlags |= PL_OBJECT_FLAGS_RECEIVE_SHADOW;
    if(gptJson->bool_member(ptJson, "casts_shadow", true)) ptComponent->tFlags |= PL_OBJECT_FLAGS_CAST_SHADOW;
    if(gptJson->bool_member(ptJson, "renderable", true)) ptComponent->tFlags |= PL_OBJECT_FLAGS_RENDERABLE;
}

static void
pl__ecs_light_serialize(void* pComponent, plJsonObject* ptJson)
{
    plLightComponent* ptComponent = pComponent;
    if(ptComponent->tType == PL_LIGHT_TYPE_SPOT)
    {
        gptJson->add_string_member(ptJson, "type", "spot");
        gptJson->add_float_member(ptJson, "inner_cone_angle", ptComponent->fInnerConeAngle);
        gptJson->add_float_member(ptJson, "outer_cone_angle", ptComponent->fOuterConeAngle);
        gptJson->add_float_member(ptJson, "radius", ptComponent->fRadius);
        gptJson->add_float_array(ptJson, "position", ptComponent->tPosition.d, 3);
        gptJson->add_float_array(ptJson, "direction", ptComponent->tDirection.d, 3);
    }
    else if(ptComponent->tType == PL_LIGHT_TYPE_POINT)
    {
        gptJson->add_string_member(ptJson, "type", "point");
        gptJson->add_float_array(ptJson, "position", ptComponent->tPosition.d, 3);
        gptJson->add_float_member(ptJson, "radius", ptComponent->fRadius);
    }
    else if(ptComponent->tType == PL_LIGHT_TYPE_DIRECTIONAL)
    {
        gptJson->add_string_member(ptJson, "type", "direction");
        gptJson->add_float_array(ptJson, "direction", ptComponent->tDirection.d, 3);
    }
    gptJson->add_uint32_member(ptJson, "shadow_resolution", ptComponent->uShadowResolution);
    gptJson->add_float_member(ptJson, "intensity", ptComponent->fIntensity);
    gptJson->add_float_member(ptJson, "range", ptComponent->fRange);
    gptJson->add_bool_member(ptJson, "casts_shadow", ptComponent->tFlags & PL_LIGHT_FLAG_CAST_SHADOW);
    gptJson->add_bool_member(ptJson, "visualizer", ptComponent->tFlags & PL_LIGHT_FLAG_VISUALIZER);
}

static void
pl__ecs_light_deserialize(plJsonObject* ptJson, void* pComponent)
{
    plLightComponent* ptComponent = pComponent;

    char acTempBuffer0[1024] = {0};

    gptJson->string_member(ptJson, "type", acTempBuffer0, 1024);

    if(acTempBuffer0[0] == 'd')
    {
        ptComponent->tType = PL_LIGHT_TYPE_DIRECTIONAL;
    }
    else if(acTempBuffer0[0] == 'p')
    {
        ptComponent->tType = PL_LIGHT_TYPE_POINT;
    }
    else if(acTempBuffer0[0] == 's')
    {
        ptComponent->tType = PL_LIGHT_TYPE_SPOT;
    }

    ptComponent->fIntensity = gptJson->float_member(ptJson, "intensity", 1.0f);
    ptComponent->fRadius = gptJson->float_member(ptJson, "radius", 0.025f);
    ptComponent->fRange = gptJson->float_member(ptJson, "range", 10.0f);
    ptComponent->fInnerConeAngle = gptJson->float_member(ptJson, "inner_cone_angle", 0.0f);
    ptComponent->fOuterConeAngle = gptJson->float_member(ptJson, "outer_cone_angle", 0.392699f);
    ptComponent->uShadowResolution = gptJson->uint32_member(ptJson, "shadow_resolution", 512);

    gptJson->float_array_member(ptJson, "direction", ptComponent->tDirection.d, NULL);
    gptJson->float_array_member(ptJson, "color", ptComponent->tColor.d, NULL);
    gptJson->float_array_member(ptJson, "position", ptComponent->tPosition.d, NULL);

    if(gptJson->bool_member(ptJson, "casts_shadow", true)) ptComponent->tFlags |= PL_LIGHT_FLAG_CAST_SHADOW;
    if(gptJson->bool_member(ptJson, "visualizer", true))   ptComponent->tFlags |= PL_LIGHT_FLAG_VISUALIZER;

}

static void
pl__ecs_probe_serialize(void* pComponent, plJsonObject* ptJson)
{
    plEnvironmentProbeComponent* ptComponent = pComponent;

    gptJson->add_uint32_member(ptJson, "resolution", ptComponent->uResolution);
    gptJson->add_uint32_member(ptJson, "samples", ptComponent->uSamples);
    gptJson->add_uint32_member(ptJson, "interval", ptComponent->uInterval);
    gptJson->add_float_member(ptJson, "range", ptComponent->fRange);

    if(ptComponent->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_REALTIME)
        gptJson->add_bool_member(ptJson, "realtime", true);
    else
        gptJson->add_bool_member(ptJson, "realtime", false);

    if(ptComponent->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_INCLUDE_SKY)
        gptJson->add_bool_member(ptJson, "include_sky", true);
    else
        gptJson->add_bool_member(ptJson, "include_sky", false);
}

static void
pl__ecs_probe_deserialize(plJsonObject* ptJson, void* pComponent)
{
    plEnvironmentProbeComponent* ptComponent = pComponent;

    ptComponent->fRange = gptJson->float_member(ptJson, "range", 10.0f);
    ptComponent->uResolution = gptJson->uint32_member(ptJson, "resolution", 128);
    ptComponent->uSamples = gptJson->uint32_member(ptJson, "samples", 128);
    ptComponent->uInterval = gptJson->uint32_member(ptJson, "interval", 1);
    ptComponent->tFlags |= PL_ENVIRONMENT_PROBE_FLAGS_DIRTY;
    if(gptJson->bool_member(ptJson, "include_sky", false)) ptComponent->tFlags |= PL_ENVIRONMENT_PROBE_FLAGS_INCLUDE_SKY;
    if(gptJson->bool_member(ptJson, "realtime", false))    ptComponent->tFlags |= PL_ENVIRONMENT_PROBE_FLAGS_REALTIME;
}

static void
pl__ecs_terrain_serialize(void* pComponent, plJsonObject* ptJson)
{
    plTerrainComponent* ptComponent = pComponent;
    gptJson->add_string_member(ptJson, "terrain", gptAsset->get_path(ptComponent->tTerrain));
}

static void
pl__ecs_terrain_deserialize(plJsonObject* ptJson, void* pComponent)
{
    plTerrainComponent* ptComponent = pComponent;
    char acTempBuffer0[1024] = {0};
    gptJson->string_member(ptJson, "terrain", acTempBuffer0, 1024);
    ptComponent->tTerrain = gptAsset->load(acTempBuffer0);
}

void
pl_renderer_ecs_register_ecs_components(void)
{

    const plComponentDesc tObjectDesc = {
        .pcDisplayName = "Object",
        .pcName = "object",
        .szSize = sizeof(plObjectComponent),
        .serialize = pl__ecs_object_serialize,
        .deserialize = pl__ecs_object_deserialize
    };

    static const plObjectComponent tObjectComponentDefault = {
        .tFlags     = PL_OBJECT_FLAGS_RENDERABLE | PL_OBJECT_FLAGS_CAST_SHADOW | PL_OBJECT_FLAGS_DYNAMIC,
        .tMesh      = {UINT32_MAX, UINT32_MAX},
        .tTransform = {UINT32_MAX, UINT32_MAX}
    };
    gptData->tObjectComponentType = gptEcs->register_type(tObjectDesc, &tObjectComponentDefault);

    const plComponentDesc tLightDesc = {
        .pcDisplayName = "Light",
        .pcName = "light",
        .szSize = sizeof(plLightComponent),
        .serialize = pl__ecs_light_serialize,        
        .deserialize = pl__ecs_light_deserialize        
    };

    static const plLightComponent tLightComponentDefault = {
        .tPosition           = {0.0f, 0.0f, 0.0f},
        .tColor              = {1.0f, 1.0f, 1.0f},
        .tDirection          = {0.0f, -1.0f, 0.0f},
        .fIntensity          = 1.0f,
        .fRange              = 5.0f,
        .fRadius             = 0.025f,
        .fInnerConeAngle     = 0.0f,
        .fOuterConeAngle     = PL_PI_4 * 0.5f,
        .tType               = PL_LIGHT_TYPE_DIRECTIONAL,
        .tFlags              = 0,
    };
    gptData->tLightComponentType = gptEcs->register_type(tLightDesc, &tLightComponentDefault);

    const plComponentDesc tProbeDesc = {
        .pcDisplayName = "Environment Probe",
        .pcName = "probe",
        .szSize = sizeof(plEnvironmentProbeComponent),
        .serialize = pl__ecs_probe_serialize,
        .deserialize = pl__ecs_probe_deserialize,
    };

    static const plEnvironmentProbeComponent tProbeDefaultComponent = {
        .fRange      = 10.0f,
        .uResolution = 128,
        .uSamples    = 128,
        .uInterval   = 1,
        .tFlags      = PL_ENVIRONMENT_PROBE_FLAGS_DIRTY
    };
    gptData->tEnvironmentProbeComponentType = gptEcs->register_type(tProbeDesc, &tProbeDefaultComponent);

    const plComponentDesc tTerrainDesc = {
        .pcDisplayName = "Terrain",
        .pcName = "terrain",
        .szSize = sizeof(plTerrainComponent),
        .serialize = pl__ecs_terrain_serialize,
        .deserialize = pl__ecs_terrain_deserialize,
    };

    static const plTerrainComponent tTerrainDefaultComponent = {
        0
    };
    gptData->tTerrainComponentType = gptEcs->register_type(tTerrainDesc, &tTerrainDefaultComponent);
}

void
pl_renderer_ecs_run_light_update_system(plComponentLibrary* ptLibrary)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    plLightComponent* ptComponents = NULL;
    const plEntity* ptEntities = NULL;
    const uint32_t uComponentCount = gptEcs->get_components(ptLibrary, gptData->tLightComponentType, (void**)&ptComponents, &ptEntities);

    const plEcsTypeKey tTransformKey = gptTransform->get_ecs_type_key_transform();
    for(uint32_t i = 0; i < uComponentCount; i++)
    {
        plEntity tEntity = ptEntities[i];
        if(gptEcs->has_component(ptLibrary, tTransformKey, tEntity))
        {
            plLightComponent* ptLight = &ptComponents[i];
            plTransformComponent* ptTransform = gptEcs->get_component(ptLibrary, tTransformKey, tEntity);
            ptLight->tPosition = ptTransform->tWorld.col[3].xyz;

            // TODO: direction
        }
    }
    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

void
pl_renderer_ecs_run_environment_probe_update_system(plComponentLibrary* ptLibrary)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    // not used yet

    // plEnvironmentProbeComponent* sbtComponents = ptLibrary->tEnvironmentProbeCompManager.pComponents;

    // const uint32_t uComponentCount = pl_sb_size(sbtComponents);
    // for(uint32_t i = 0; i < uComponentCount; i++)
    // {
    //     plEntity tEntity = ptLibrary->tEnvironmentProbeCompManager.sbtEntities[i];
    //     if(pl_ecs_has_entity(&ptLibrary->tTransformComponentManager, tEntity))
    //     {
    //         plEnvironmentProbeComponent* ptProbe = &sbtComponents[i];
    //         plTransformComponent* ptTransform = gptEcs->get_component(ptLibrary, gptEcsCoreCtx->tTransformComponentType, tEntity);
    //     }
    // }
    PL_PROFILE_END_SAMPLE_API(gptProfile, 0); 
}

void
pl_renderer_register_asset_types(void)
{
    static const plAssetTypeDesc tDesc0 = {
        .pcName          = "Environment",
        .pcFileExtension = "plenvironment",
        .szSize          = sizeof(plRenderEnvironment),
        .serialize       = pl__renderer_serialize_environment,
        .deserialize     = pl__renderer_deserialize_environment,
    };
    gptData->tAssetTypeKeyEnvironment = gptAsset->register_type(tDesc0);

    static const plAssetTypeDesc tDesc1 = {
        .pcName          = "Renderer",
        .pcFileExtension = "plrenderer",
        .szSize          = sizeof(plRenderSettings),
        .serialize       = pl__renderer_serialize_settings,
        .deserialize     = pl__renderer_deserialize_settings,
    };
    gptData->tAssetTypeKeyEnvironment = gptAsset->register_type(tDesc1);
}

plAssetTypeKey
pl_renderer_get_asset_type_key_environment(void)
{
    return gptData->tAssetTypeKeyEnvironment;
}

plAssetTypeKey
pl_renderer_get_asset_type_key_settings(void)
{
    return gptData->tAssetTypeKeySettings;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_renderer_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    plRendererI tApi0 = {0};
    tApi0.initialize                     = pl_renderer_initialize;
    tApi0.cleanup                        = pl_renderer_cleanup;
    tApi0.create_scene                   = pl_renderer_create_scene;
    tApi0.destroy_scene                  = pl_renderer_destroy_scene;
    tApi0.create_view                    = pl_renderer_create_view;
    tApi0.destroy_view                   = pl_renderer_destroy_view;
    tApi0.begin_frame                    = pl_renderer_begin_frame;
    tApi0.prepare_scene                  = pl_renderer_prepare_scene;
    tApi0.prepare_view                   = pl_renderer_prepare_view;
    tApi0.render_view                    = pl_renderer_render_view;
    tApi0.get_view_color_bind_group      = pl_renderer_get_view_color_bind_group;
    tApi0.resize_view                    = pl_renderer_resize_view;
    tApi0.set_scene_flags                = pl_renderer_set_scene_flags;
    tApi0.get_scene_flags                = pl_renderer_get_scene_flags;
    tApi0.set_settings                   = pl_renderer_set_settings;
    tApi0.get_settings                   = pl_renderer_get_settings;
    tApi0.get_environment                = pl_renderer_get_environment;
    tApi0.set_environment                = pl_renderer_set_environment;
    tApi0.register_asset_types           = pl_renderer_register_asset_types;
    tApi0.get_asset_type_key_environment = pl_renderer_get_asset_type_key_environment;
    tApi0.get_asset_type_key_settings    = pl_renderer_get_asset_type_key_settings;
    tApi0.get_terrain_options            = pl_renderer_get_terrain_options;

    plRendererEcsI tApi2 = {0};
    tApi2.get_ecs_type_key_terrain            = pl_renderer_ecs_get_type_key_terrain;
    tApi2.get_ecs_type_key_light              = pl_renderer_ecs_get_type_key_light;
    tApi2.get_ecs_type_key_environment_probe  = pl_renderer_ecs_get_type_key_environment_probe;
    tApi2.run_light_update_system             = pl_renderer_ecs_run_light_update_system;
    tApi2.run_environment_probe_update_system = pl_renderer_ecs_run_environment_probe_update_system;
    tApi2.get_ecs_type_key_object             = pl_renderer_ecs_get_type_key_object;
    tApi2.run_object_update_system            = pl_renderer_ecs_run_object_update_system;
    tApi2.register_ecs_components             = pl_renderer_ecs_register_ecs_components;
    tApi2.create_directional_light            = pl_renderer_ecs_create_directional_light;
    tApi2.create_point_light                  = pl_renderer_ecs_create_point_light;
    tApi2.create_spot_light                   = pl_renderer_ecs_create_spot_light;
    tApi2.create_environment_probe            = pl_renderer_ecs_create_environment_probe;
    tApi2.create_object                       = pl_renderer_ecs_create_object;
    tApi2.copy_object                         = pl_renderer_ecs_copy_object;
    tApi2.add_drawable_objects_to_scene       = pl_renderer_ecs_add_drawable_objects_to_scene;
    tApi2.add_probes_to_scene                 = pl_renderer_ecs_add_probes_to_scene;
    tApi2.add_lights_to_scene                 = pl_renderer_ecs_add_lights_to_scene;
    tApi2.add_terrain_to_scene                = pl_renderer_ecs_add_terrain_to_scene;
    tApi2.add_materials_to_scene              = pl_renderer_ecs_add_materials_to_scene;
    tApi2.update_scene_materials              = pl_renderer_ecs_update_scene_materials;

    plRendererDebugI tApi3 = {0};
    tApi3.get_drawlist            = pl_renderer_debug_get_drawlist;
    tApi3.draw_lights             = pl_renderer_debug_draw_lights;
    tApi3.draw_all_bound_boxes    = pl_renderer_debug_draw_all_bound_boxes;
    tApi3.draw_bvh                = pl_renderer_debug_draw_bvh;
    tApi3.set_scene_options = pl_renderer_debug_set_scene_options;
    tApi3.get_scene_options = pl_renderer_debug_get_scene_options;
    tApi3.set_view_options  = pl_renderer_debug_set_view_options;
    tApi3.get_view_options  = pl_renderer_debug_get_view_options;

    plRendererEditorI tApi4 = {0};
    tApi4.update_hovered_entity = pl_renderer_editor_update_hovered_entity;
    tApi4.get_hovered_entity    = pl_renderer_editor_get_hovered_entity;
    tApi4.reload_scene_shaders  = pl_renderer_editor_reload_scene_shaders;
    tApi4.outline_entities      = pl_renderer_editor_outline_entities;
    tApi4.get_gizmo_drawlist    = pl_renderer_editor_get_gizmo_drawlist;
    tApi4.rebuild_scene_bvh     = pl_renderer_editor_rebuild_scene_bvh;
    tApi4.set_scene_options     = pl_renderer_editor_set_scene_options;
    tApi4.get_scene_options     = pl_renderer_editor_get_scene_options;
    tApi4.set_view_options      = pl_renderer_editor_set_view_options;
    tApi4.get_view_options      = pl_renderer_editor_get_view_options;


    pl_set_api(ptApiRegistry, plRendererI, &tApi0);
    pl_set_api(ptApiRegistry, plRendererEcsI, &tApi2);
    pl_set_api(ptApiRegistry, plRendererDebugI, &tApi3);
    pl_set_api(ptApiRegistry, plRendererEditorI, &tApi4);

    // core apis
    #ifndef PL_UNITY_BUILD
        gptDataRegistry     = pl_get_api_latest(ptApiRegistry, plDataRegistryI);
        gptIOI              = pl_get_api_latest(ptApiRegistry, plIOI);
        gptImage            = pl_get_api_latest(ptApiRegistry, plImageI);
        gptMemory           = pl_get_api_latest(ptApiRegistry, plMemoryI);
        gptGpuAllocators    = pl_get_api_latest(ptApiRegistry, plGPUAllocatorsI);
        gptIO               = gptIOI->get_io();
        gptStats            = pl_get_api_latest(ptApiRegistry, plStatsI);
        gptImage            = pl_get_api_latest(ptApiRegistry, plImageI);
        gptJob              = pl_get_api_latest(ptApiRegistry, plJobI);
        gptProfile          = pl_get_api_latest(ptApiRegistry, plProfileI);
        gptLog              = pl_get_api_latest(ptApiRegistry, plLogI);
        gptRect             = pl_get_api_latest(ptApiRegistry, plRectPackI);
        gptEcs              = pl_get_api_latest(ptApiRegistry, plEcsI);
        gptCamera           = pl_get_api_latest(ptApiRegistry, plCameraI);
        gptCameraEcs        = pl_get_api_latest(ptApiRegistry, plCameraEcsI);
        gptDraw             = pl_get_api_latest(ptApiRegistry, plDrawI);
        gptGfx              = pl_get_api_latest(ptApiRegistry, plGraphicsI);
        gptResource         = pl_get_api_latest(ptApiRegistry, plResourceI);
        gptShader           = pl_get_api_latest(ptApiRegistry, plShaderI);
        gptConsole          = pl_get_api_latest(ptApiRegistry, plConsoleI);
        gptScreenLog        = pl_get_api_latest(ptApiRegistry, plScreenLogI);
        gptBvh              = pl_get_api_latest(ptApiRegistry, plBVHI);
        gptAnimation        = pl_get_api_latest(ptApiRegistry, plAnimationI);
        gptMesh             = pl_get_api_latest(ptApiRegistry, plMeshI);
        gptShaderVariant    = pl_get_api_latest(ptApiRegistry, plShaderVariantI);
        gptVfs              = pl_get_api_latest(ptApiRegistry, plVfsI);
        gptStarter          = pl_get_api_latest(ptApiRegistry, plStarterI);
        gptMaterial         = pl_get_api_latest(ptApiRegistry, plMaterialI);
        gptTerrain          = pl_get_api_latest(ptApiRegistry, plTerrainI);
        gptStage            = pl_get_api_latest(ptApiRegistry, plStageI);
        gptFreeList         = pl_get_api_latest(ptApiRegistry, plFreeListI);
        gptCollision        = pl_get_api_latest(ptApiRegistry, plCollisionI);
        gptImageOps         = pl_get_api_latest(ptApiRegistry, plImageOpsI);
        gptScript           = pl_get_api_latest(ptApiRegistry, plScriptI);
        gptGltf             = pl_get_api_latest(ptApiRegistry, plGltfI);
        gptGjk              = pl_get_api_latest(ptApiRegistry, plGjkI);
        gptUI               = pl_get_api_latest(ptApiRegistry, plUiI);
        gptAsset            = pl_get_api_latest(ptApiRegistry, plAssetI);
        gptTransform        = pl_get_api_latest(ptApiRegistry, plTransformI);
        // gptStl              = pl_get_api_latest(ptApiRegistry, plStlI);
        gptScene            = pl_get_api_latest(ptApiRegistry, plSceneI);
        gptSkeleton         = pl_get_api_latest(ptApiRegistry, plSkeletonI);
        gptTexture          = pl_get_api_latest(ptApiRegistry, plTextureI);
        gptJson             = pl_get_api_latest(ptApiRegistry, plJsonI);
    #endif

    if(bReload)
    {
        gptData = gptDataRegistry->get_data("ref renderer data");
    }   
}

void
pl_unload_renderer_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;
        
    const plRendererI* ptApi0 = pl_get_api_latest(ptApiRegistry, plRendererI);
    const plRendererEcsI* ptApi2 = pl_get_api_latest(ptApiRegistry, plRendererEcsI);
    const plRendererDebugI* ptApi3 = pl_get_api_latest(ptApiRegistry, plRendererDebugI);
    const plRendererEditorI* ptApi4 = pl_get_api_latest(ptApiRegistry, plRendererEditorI);

    ptApiRegistry->remove_api(ptApi0);
    ptApiRegistry->remove_api(ptApi2);
    ptApiRegistry->remove_api(ptApi3);
    ptApiRegistry->remove_api(ptApi4);
}

//-----------------------------------------------------------------------------
// [SECTION] unity
//-----------------------------------------------------------------------------

#ifndef PL_UNITY_BUILD

    #define PL_MEMORY_IMPLEMENTATION
    #include "pl_memory.h"
    #undef PL_MEMORY_IMPLEMENTATION

    #ifdef PL_USE_STB_SPRINTF
        #define STB_SPRINTF_IMPLEMENTATION
        #include "stb_sprintf.h"
        #undef STB_SPRINTF_IMPLEMENTATION
    #endif

    #define PL_STRING_IMPLEMENTATION
    #include "pl_string.h"
    #undef PL_STRING_IMPLEMENTATION

#endif
