/*
   pl_renderer_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] public api
// [SECTION] implementation
// [SECTION] extension loading
// [SECTION] unity
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_renderer_internal.h"

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

void pl_renderer_reload_scene_shaders(plScene*);
void pl_renderer_add_materials_to_scene(plScene*, uint32_t, const plEntity* atMaterials);

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

static void
pl__renderer_console_shader_reload(const char* pcName, void* pData)
{
    for(uint32_t i = 0; i < pl_sb_size(gptData->sbptScenes); i++)
        pl_renderer_reload_scene_shaders(gptData->sbptScenes[i]);
}

static void
pl__renderer_skin_cleanup(plComponentLibrary* ptLibrary)
{
    plSkinComponent* ptComponents = NULL;
    const uint32_t uComponentCount = gptECS->get_components(ptLibrary, gptData->tSkinComponentType, (void**)&ptComponents, NULL);
    for(uint32_t i = 0; i < uComponentCount; i++)
    {
        pl_sb_free(ptComponents[i].sbtTextureData);
        pl_sb_free(ptComponents[i].sbtJoints);
        pl_sb_free(ptComponents[i].sbtInverseBindMatrices);
    }
}

plEntity
pl_renderer_create_directional_light(plComponentLibrary* ptLibrary, const char* pcName, plVec3 tDirection, plLightComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed directional light";
    pl_log_debug_f(gptLog, gptECS->get_log_channel(), "created directional light: '%s'", pcName);
    plEntity tNewEntity = gptECS->create_entity(ptLibrary, pcName);
    plLightComponent* ptLight =  gptECS->add_component(ptLibrary, gptData->tLightComponentType, tNewEntity);
    ptLight->tDirection = tDirection;
    ptLight->tType = PL_LIGHT_TYPE_DIRECTIONAL;

    if(pptCompOut)
        *pptCompOut = ptLight;
    return tNewEntity;
}

plEntity
pl_renderer_create_point_light(plComponentLibrary* ptLibrary, const char* pcName, plVec3 tPosition, plLightComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed point light";
    pl_log_debug_f(gptLog, gptECS->get_log_channel(), "created point light: '%s'", pcName);
    plEntity tNewEntity = gptECS->create_entity(ptLibrary, pcName);
    plLightComponent* ptLight =  gptECS->add_component(ptLibrary, gptData->tLightComponentType, tNewEntity);
    ptLight->tPosition = tPosition;
    ptLight->tType = PL_LIGHT_TYPE_POINT;

    if(pptCompOut)
        *pptCompOut = ptLight;
    return tNewEntity;
}

plEntity
pl_renderer_create_environment_probe(plComponentLibrary* ptLibrary, const char* pcName, plVec3 tPosition, plEnvironmentProbeComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed environment probe";
    pl_log_debug_f(gptLog, gptECS->get_log_channel(), "created environment probe: '%s'", pcName);
    plEntity tNewEntity = gptECS->create_entity(ptLibrary, pcName);

    plTransformComponent* ptProbeTransform = gptECS->add_component(ptLibrary, gptECS->get_ecs_type_key_transform(), tNewEntity);
    ptProbeTransform->tTranslation = tPosition;

    plEnvironmentProbeComponent* ptProbe =  gptECS->add_component(ptLibrary, gptData->tEnvironmentProbeComponentType, tNewEntity);

    if(pptCompOut)
        *pptCompOut = ptProbe;
    return tNewEntity;
}

plEntity
pl_renderer_create_spot_light(plComponentLibrary* ptLibrary, const char* pcName, plVec3 tPosition, plVec3 tDirection, plLightComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed spot light";
    pl_log_debug_f(gptLog, gptECS->get_log_channel(), "created spot light: '%s'", pcName);
    plEntity tNewEntity = gptECS->create_entity(ptLibrary, pcName);
    plLightComponent* ptLight =  gptECS->add_component(ptLibrary, gptData->tLightComponentType, tNewEntity);
    ptLight->tPosition = tPosition;
    ptLight->tDirection = tDirection;
    ptLight->tType = PL_LIGHT_TYPE_SPOT;

    if(pptCompOut)
        *pptCompOut = ptLight;
    return tNewEntity;
}

plEntity
pl_renderer_create_object(plComponentLibrary* ptLibrary, const char* pcName, plObjectComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed object";
    pl_log_debug_f(gptLog, gptECS->get_log_channel(), "created object: '%s'", pcName);
    plEntity tNewEntity = gptECS->create_entity(ptLibrary, pcName);

    plObjectComponent* ptObject = gptECS->add_component(ptLibrary, gptData->tObjectComponentType, tNewEntity);
    (void)gptECS->add_component(ptLibrary, gptECS->get_ecs_type_key_transform(), tNewEntity);
    (void)gptECS->add_component(ptLibrary, gptMesh->get_ecs_type_key_mesh(), tNewEntity);

    ptObject->tTransform = tNewEntity;
    ptObject->tMesh = tNewEntity;

    if(pptCompOut)
        *pptCompOut = ptObject;

    return tNewEntity;    
}

plEntity
pl_renderer_copy_object(plComponentLibrary* ptLibrary, const char* pcName, plEntity tOriginalObject, plObjectComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed object copy";
    pl_log_debug_f(gptLog, gptECS->get_log_channel(), "copied object: '%s'", pcName);

    plEntity tNewEntity = gptECS->create_entity(ptLibrary, pcName);

    plObjectComponent* ptOriginalObject = gptECS->get_component(ptLibrary, gptData->tObjectComponentType, tOriginalObject);
    plEntity tExistingMesh = ptOriginalObject->tMesh;


    plObjectComponent* ptObject = gptECS->add_component(ptLibrary, gptData->tObjectComponentType, tNewEntity);
    (void)gptECS->add_component(ptLibrary, gptECS->get_ecs_type_key_transform(), tNewEntity);

    ptObject->tTransform = tNewEntity;
    ptObject->tMesh = tExistingMesh;

    if(pptCompOut)
        *pptCompOut = ptObject;

    return tNewEntity; 
}

plEntity
pl_renderer_create_material(plComponentLibrary* ptLibrary, const char* pcName, plMaterialComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed material";
    pl_log_debug_f(gptLog, gptECS->get_log_channel(), "created material: '%s'", pcName);
    plEntity tNewEntity = gptECS->create_entity(ptLibrary, pcName);

    plMaterialComponent* ptCompOut = gptECS->add_component(ptLibrary, gptData->tMaterialComponentType, tNewEntity);

    if(pptCompOut)
        *pptCompOut = ptCompOut;

    return tNewEntity;    
}

plEntity
pl_renderer_create_skin(plComponentLibrary* ptLibrary, const char* pcName, plSkinComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed skin";
    pl_log_debug_f(gptLog, gptECS->get_log_channel(), "created skin: '%s'", pcName);
    plEntity tNewEntity = gptECS->create_entity(ptLibrary, pcName);

    plSkinComponent* ptSkin = gptECS->add_component(ptLibrary, gptData->tSkinComponentType, tNewEntity);

    if(pptCompOut)
        *pptCompOut = ptSkin;

    return tNewEntity;
}

plEcsTypeKey
pl_renderer_get_ecs_type_key_object(void)
{
    return gptData->tObjectComponentType;
}

plEcsTypeKey
pl_renderer_get_ecs_type_key_material(void)
{
    return gptData->tMaterialComponentType;
}

plEcsTypeKey
pl_renderer_get_ecs_type_key_skin(void)
{
    return gptData->tSkinComponentType;
}

plEcsTypeKey
pl_renderer_get_ecs_type_key_light(void)
{
    return gptData->tLightComponentType;
}

plEcsTypeKey
pl_renderer_get_ecs_type_key_environment_probe(void)
{
    return gptData->tEnvironmentProbeComponentType;
}

void
pl_renderer_initialize(plRendererSettings tSettings)
{

    // allocate renderer data
    gptData = PL_ALLOC(sizeof(plRefRendererData));
    memset(gptData, 0, sizeof(plRefRendererData));

    gptData->ptDevice = tSettings.ptDevice;
    gptData->tDeviceInfo = *gptGfx->get_device_info(gptData->ptDevice);
    gptData->ptSwap = tSettings.ptSwap;

    // register data with registry (for reloads)
    gptDataRegistry->set_data("ref renderer data", gptData);

    // register console variables
    gptConsole->add_toggle_variable("r.DrawSelectedBoundBoxes", &gptData->tRuntimeOptions.bShowSelectedBoundingBox, "draw selected bounding boxes", PL_CONSOLE_VARIABLE_FLAGS_NONE);
    gptConsole->add_toggle_variable("r.ShowProbes", &gptData->tRuntimeOptions.bShowProbes, "show environment probes", PL_CONSOLE_VARIABLE_FLAGS_NONE);
    gptConsole->add_toggle_variable("r.ShowOrigin", &gptData->tRuntimeOptions.bShowOrigin, "show world origin", PL_CONSOLE_VARIABLE_FLAGS_NONE);
    gptConsole->add_float_variable("r.ShadowConstantDepthBias", &gptData->tRuntimeOptions.fShadowConstantDepthBias, "shadow constant depth bias", PL_CONSOLE_VARIABLE_FLAGS_NONE);
    gptConsole->add_float_variable("r.ShadowSlopeDepthBias", &gptData->tRuntimeOptions.fShadowSlopeDepthBias, "shadow slope depth bias", PL_CONSOLE_VARIABLE_FLAGS_NONE | PL_CONSOLE_VARIABLE_FLAGS_READ_ONLY);
    gptConsole->add_uint_variable("r.OutlineWidth", &gptData->tRuntimeOptions.uOutlineWidth, "selection outline width", PL_CONSOLE_VARIABLE_FLAGS_NONE);
    gptConsole->add_toggle_variable_ex("r.Wireframe", &gptData->tRuntimeOptions.bWireframe, "wireframe rendering", PL_CONSOLE_VARIABLE_FLAGS_NONE, pl__renderer_console_shader_reload, NULL);
    gptConsole->add_toggle_variable_ex("r.IBL", &gptData->tRuntimeOptions.bImageBasedLighting, "image based lighting", PL_CONSOLE_VARIABLE_FLAGS_NONE, pl__renderer_console_shader_reload, NULL);
    gptConsole->add_toggle_variable_ex("r.PunctualLighting", &gptData->tRuntimeOptions.bPunctualLighting, "punctual lighting", PL_CONSOLE_VARIABLE_FLAGS_NONE, pl__renderer_console_shader_reload, NULL);
    gptConsole->add_toggle_variable_ex("r.MultiViewportShadows", &gptData->tRuntimeOptions.bMultiViewportShadows, "utilize multiviewport features", PL_CONSOLE_VARIABLE_FLAGS_NONE, pl__renderer_console_shader_reload, NULL);
    // add specific log channel for renderer
    plLogExtChannelInit tLogInit = {
        .tType       = PL_LOG_CHANNEL_TYPE_CYCLIC_BUFFER,
        .uEntryCount = 1024
    };
    gptData->uLogChannel = gptLog->add_channel("Renderer", tLogInit);

    // default options
    gptData->pdDrawCalls = gptStats->get_counter("draw calls");
    gptData->uMaxTextureResolution = tSettings.uMaxTextureResolution > 0 ? tSettings.uMaxTextureResolution : 1024;
    gptData->tRuntimeOptions.uOutlineWidth = 4;
    gptData->tRuntimeOptions.bShowSelectedBoundingBox = true;
    gptData->tRuntimeOptions.bImageBasedLighting = true;
    gptData->tRuntimeOptions.bPunctualLighting = true;
    gptData->tRuntimeOptions.fShadowConstantDepthBias = -1.25f;
    gptData->tRuntimeOptions.fShadowSlopeDepthBias = -10.75f;

    gptResource->initialize((plResourceManagerInit){.ptDevice = gptData->ptDevice, .uMaxTextureResolution = tSettings.uMaxTextureResolution});

    if(gptData->tDeviceInfo.tCapabilities & PL_DEVICE_CAPABILITY_MULTIPLE_VIEWPORTS)
        gptData->tRuntimeOptions.bMultiViewportShadows = true;

    // create main bind group pool
    const plBindGroupPoolDesc tBindGroupPoolDesc = {
        .tFlags                      = PL_BIND_GROUP_POOL_FLAGS_INDIVIDUAL_RESET,
        .szSamplerBindings           = 100000,
        .szUniformBufferBindings     = 100000,
        .szStorageBufferBindings     = 100000,
        .szSampledTextureBindings    = 100000,
        .szStorageTextureBindings    = 100000,
        .szAttachmentTextureBindings = 100000
    };
    gptData->ptBindGroupPool = gptGfx->create_bind_group_pool(gptData->ptDevice, &tBindGroupPoolDesc);

    bool bManifestResult = gptShaderVariant->load_manifest("/shaders/shaders.pls");
    PL_ASSERT(bManifestResult);

    gptData->tSceneBGLayout = gptShaderVariant->get_bind_group_layout("scene");
    gptData->tShadowGlobalBGLayout = gptShaderVariant->get_bind_group_layout("shadow");

    // create pools
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        plBindGroupPoolDesc tPoolDesc = {
            .tFlags                      = PL_BIND_GROUP_POOL_FLAGS_NONE,
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

    // create dummy textures
    const plTextureDesc tDummyTextureDesc = {
        .tDimensions   = {2, 2, 1},
        .tFormat       = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED,
        .pcDebugName   = "dummy"
    };

    const float afDummyTextureData[] = {
        1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 1.0f
    };
    gptData->tDummyTexture = pl__renderer_create_texture_with_data(&tDummyTextureDesc, "dummy", 0, afDummyTextureData, sizeof(afDummyTextureData));

    const plTextureDesc tSkyboxTextureDesc = {
        .tDimensions = {1, 1, 1},
        .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers     = 6,
        .uMips       = 1,
        .tType       = PL_TEXTURE_TYPE_CUBE,
        .tUsage      = PL_TEXTURE_USAGE_SAMPLED,
        .pcDebugName = "dummy cube"
    };
    gptData->tDummyTextureCube = pl__renderer_create_texture(&tSkyboxTextureDesc, "dummy cube", 0, PL_TEXTURE_USAGE_SAMPLED);

    // create samplers
    const plSamplerDesc tSamplerDesc = {
        .tMagFilter      = PL_FILTER_LINEAR,
        .tMinFilter      = PL_FILTER_LINEAR,
        .fMinMip         = 0.0f,
        .fMaxMip         = 64.0f,
        .tVAddressMode   = PL_ADDRESS_MODE_WRAP,
        .tUAddressMode   = PL_ADDRESS_MODE_WRAP,
        .tMipmapMode     = PL_MIPMAP_MODE_NEAREST,
        .pcDebugName     = "default sampler"
    };
    gptData->tDefaultSampler = gptGfx->create_sampler(gptData->ptDevice, &tSamplerDesc);
    
    const plSamplerDesc tShadowSamplerDesc = {
        .tMagFilter      = PL_FILTER_NEAREST,
        .tMinFilter      = PL_FILTER_NEAREST,
        .fMinMip         = 0.0f,
        .fMaxMip         = 1.0f,
        .fMaxAnisotropy  = 1.0f,
        .tVAddressMode   = PL_ADDRESS_MODE_CLAMP,
        .tUAddressMode   = PL_ADDRESS_MODE_CLAMP,
        .tMipmapMode     = PL_MIPMAP_MODE_NEAREST,
        .pcDebugName     = "shadow sampler"
    };
    gptData->tShadowSampler = gptGfx->create_sampler(gptData->ptDevice, &tShadowSamplerDesc);

    const plSamplerDesc tSkyboxSamplerDesc = {
        .tMagFilter      = PL_FILTER_LINEAR,
        .tMinFilter      = PL_FILTER_LINEAR,
        .fMinMip         = 0.0f,
        .fMaxMip         = 64.0f,
        .fMaxAnisotropy  = 1.0f,
        .tVAddressMode   = PL_ADDRESS_MODE_WRAP,
        .tUAddressMode   = PL_ADDRESS_MODE_WRAP,
        .tMipmapMode     = PL_MIPMAP_MODE_LINEAR,
        .pcDebugName     = "skybox sampler"
    };
    gptData->tSkyboxSampler = gptGfx->create_sampler(gptData->ptDevice, &tSkyboxSamplerDesc);

    const plSamplerDesc tEnvSamplerDesc = {
        .tMagFilter      = PL_FILTER_NEAREST,
        .tMinFilter      = PL_FILTER_NEAREST,
        .fMinMip         = 0.0f,
        .fMaxMip         = 64.0f,
        .fMaxAnisotropy  = 1.0f,
        .tVAddressMode   = PL_ADDRESS_MODE_WRAP,
        .tUAddressMode   = PL_ADDRESS_MODE_WRAP,
        .tMipmapMode     = PL_MIPMAP_MODE_LINEAR,
        .pcDebugName     = "ENV sampler"
    };
    gptData->tEnvSampler = gptGfx->create_sampler(gptData->ptDevice, &tEnvSamplerDesc);

    // create deferred render pass layout
    const plRenderPassLayoutDesc tRenderPassLayoutDesc = {
        .atRenderTargets = {
            { .tFormat = PL_FORMAT_D32_FLOAT_S8_UINT, .bDepth = true },  // depth buffer
            { .tFormat = PL_FORMAT_R16G16B16A16_FLOAT }, // final output
            { .tFormat = PL_FORMAT_R8G8B8A8_UNORM },      // albedo
            { .tFormat = PL_FORMAT_R16G16_FLOAT }, // normal
            { .tFormat = PL_FORMAT_R16G16B16A16_FLOAT }, // AO, roughness, metallic, mip count
        },
        .atSubpasses = {
            { // G-buffer fill

                .uRenderTargetCount = 4,
                .auRenderTargets = {0, 2, 3, 4},
            },
            { // lighting
                .uRenderTargetCount = 1,
                .auRenderTargets = {1},
                .uSubpassInputCount = 4,
                .auSubpassInputs = {0, 2, 3, 4},
            },
            { // transparencies
                .uRenderTargetCount = 2,
                .auRenderTargets = {0, 1},
                .uSubpassInputCount = 0,
                .auSubpassInputs = {0},
            },
        },
        .atSubpassDependencies = {
            {
                .uSourceSubpass = UINT32_MAX,
                .uDestinationSubpass = 0,
                .tSourceStageMask = PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS | PL_PIPELINE_STAGE_COMPUTE_SHADER,
                .tDestinationStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tSourceAccessMask = PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
                .tDestinationAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
            },
            {
                .uSourceSubpass = 0,
                .uDestinationSubpass = 1,
                .tSourceStageMask = PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tDestinationStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tSourceAccessMask = PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE,
                .tDestinationAccessMask = PL_ACCESS_INPUT_ATTACHMENT_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE,
            },
            {
                .uSourceSubpass = 1,
                .uDestinationSubpass = 2,
                .tSourceStageMask = PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tDestinationStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tSourceAccessMask = PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE,
                .tDestinationAccessMask = PL_ACCESS_INPUT_ATTACHMENT_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE,
            },
            {
                .uSourceSubpass = 2,
                .uDestinationSubpass = UINT32_MAX,
                .tSourceStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tDestinationStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS | PL_PIPELINE_STAGE_COMPUTE_SHADER,
                .tSourceAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
                .tDestinationAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
            },
        }
    };
    gptData->tRenderPassLayout = gptGfx->create_render_pass_layout(gptData->ptDevice, &tRenderPassLayoutDesc);

    // create depth render pass layout
    const plRenderPassLayoutDesc tDepthRenderPassLayoutDesc = {
        .atRenderTargets = {
            { .tFormat = PL_FORMAT_D16_UNORM, .bDepth = true },  // depth buffer
        },
        .atSubpasses = {
            {
                .uRenderTargetCount = 1,
                .auRenderTargets = {0},
            }
        },
        .atSubpassDependencies = {
            {
                .uSourceSubpass = UINT32_MAX,
                .uDestinationSubpass = 0,
                .tSourceStageMask = PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS | PL_PIPELINE_STAGE_COMPUTE_SHADER,
                .tDestinationStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tSourceAccessMask = PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
                .tDestinationAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
            },
            {
                .uSourceSubpass = 0,
                .uDestinationSubpass = UINT32_MAX,
                .tSourceStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tDestinationStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS | PL_PIPELINE_STAGE_COMPUTE_SHADER,
                .tSourceAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
                .tDestinationAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
            },
        }
    };
    gptData->tDepthRenderPassLayout = gptGfx->create_render_pass_layout(gptData->ptDevice, &tDepthRenderPassLayoutDesc);

    // create pick render pass layout
    const plRenderPassLayoutDesc tPickRenderPassLayoutDesc = {
        .atRenderTargets = {
            { .tFormat = PL_FORMAT_D32_FLOAT_S8_UINT, .bDepth = true },  // depth buffer
            { .tFormat = PL_FORMAT_R8G8B8A8_UNORM }, // final output
        },
        .atSubpasses = {
            {
                .uRenderTargetCount = 2,
                .auRenderTargets = {0, 1}
            }
        },
        .atSubpassDependencies = {
            {
                .uSourceSubpass = UINT32_MAX,
                .uDestinationSubpass = 0,
                .tSourceStageMask = PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS | PL_PIPELINE_STAGE_COMPUTE_SHADER,
                .tDestinationStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tSourceAccessMask = PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
                .tDestinationAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
            },
            {
                .uSourceSubpass = 0,
                .uDestinationSubpass = UINT32_MAX,
                .tSourceStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tDestinationStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS | PL_PIPELINE_STAGE_COMPUTE_SHADER,
                .tSourceAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
                .tDestinationAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
            },
        }
    };
    gptData->tPickRenderPassLayout = gptGfx->create_render_pass_layout(gptData->ptDevice, &tPickRenderPassLayoutDesc);

    // create post processing render pass
    const plRenderPassLayoutDesc tPostProcessRenderPassLayoutDesc = {
        .atRenderTargets = {
            { .tFormat = PL_FORMAT_D32_FLOAT_S8_UINT, .bDepth = true }, // depth
            { .tFormat = PL_FORMAT_R16G16B16A16_FLOAT },
        },
        .atSubpasses = {
            {
                .uRenderTargetCount = 2,
                .auRenderTargets = {0, 1},
            },
        },
        .atSubpassDependencies = {
            {
                .uSourceSubpass = UINT32_MAX,
                .uDestinationSubpass = 0,
                .tSourceStageMask = PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS | PL_PIPELINE_STAGE_COMPUTE_SHADER,
                .tDestinationStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tSourceAccessMask = PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
                .tDestinationAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
            },
            {
                .uSourceSubpass = 0,
                .uDestinationSubpass = UINT32_MAX,
                .tSourceStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tDestinationStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS | PL_PIPELINE_STAGE_COMPUTE_SHADER,
                .tSourceAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
                .tDestinationAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
            },
        }
    };
    gptData->tPostProcessRenderPassLayout = gptGfx->create_render_pass_layout(gptData->ptDevice, &tPostProcessRenderPassLayoutDesc);

    const plRenderPassLayoutDesc tUVRenderPassLayoutDesc = {
        .atRenderTargets = {
            { .tFormat = PL_FORMAT_D32_FLOAT_S8_UINT, .bDepth = true }, // depth
            { .tFormat = PL_FORMAT_R32G32_FLOAT},
        },
        .atSubpasses = {
            {
                .uRenderTargetCount = 2,
                .auRenderTargets = {0, 1},
            },
        },
        .atSubpassDependencies = {
            {
                .uSourceSubpass = UINT32_MAX,
                .uDestinationSubpass = 0,
                .tSourceStageMask = PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS | PL_PIPELINE_STAGE_COMPUTE_SHADER,
                .tDestinationStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tSourceAccessMask = PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
                .tDestinationAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
            },
            {
                .uSourceSubpass = 0,
                .uDestinationSubpass = UINT32_MAX,
                .tSourceStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tDestinationStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS | PL_PIPELINE_STAGE_COMPUTE_SHADER,
                .tSourceAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
                .tDestinationAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
            },
        }
    };
    gptData->tUVRenderPassLayout = gptGfx->create_render_pass_layout(gptData->ptDevice, &tUVRenderPassLayoutDesc);

    pl_temp_allocator_reset(&gptData->tTempAllocator);

    // create full quad
    const uint32_t auFullQuadIndexBuffer[] = {0, 1, 2, 0, 2, 3};
    const plBufferDesc tFullQuadIndexBufferDesc = {
        .tUsage     = PL_BUFFER_USAGE_INDEX,
        .szByteSize = sizeof(uint32_t) * 6,
        .pcDebugName = "Renderer Quad Index Buffer"
    };
    gptData->tFullQuadIndexBuffer = pl__renderer_create_local_buffer(&tFullQuadIndexBufferDesc, "full quad index buffer", 0, auFullQuadIndexBuffer, tFullQuadIndexBufferDesc.szByteSize);

    const float afFullQuadVertexBuffer[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 0.0f
    };
    const plBufferDesc tFullQuadVertexBufferDesc = {
        .tUsage     = PL_BUFFER_USAGE_VERTEX,
        .szByteSize = sizeof(float) * 16,
        .pcDebugName = "Renderer Quad Vertex Buffer"
    };
    gptData->tFullQuadVertexBuffer = pl__renderer_create_local_buffer(&tFullQuadVertexBufferDesc, "full quad vertex buffer", 0, afFullQuadVertexBuffer, tFullQuadVertexBufferDesc.szByteSize);

    // create semaphores
    gptData->ptClickSemaphore = gptGfx->create_semaphore(gptData->ptDevice, false);
};

plScene*
pl_renderer_create_scene(plSceneInit tInit)
{

    plScene* ptScene = PL_ALLOC(sizeof(plScene));
    memset(ptScene, 0, sizeof(plScene));
    pl_sb_push(gptData->sbptScenes, ptScene);

    ptScene->ptComponentLibrary = tInit.ptComponentLibrary;
    ptScene->pcName = "unnamed scene";
    ptScene->bActive                    = true;
    ptScene->tIndexBuffer               = (plBufferHandle){.uData = UINT32_MAX};
    ptScene->tVertexBuffer              = (plBufferHandle){.uData = UINT32_MAX};
    ptScene->tStorageBuffer             = (plBufferHandle){.uData = UINT32_MAX};
    ptScene->tSkinStorageBuffer         = (plBufferHandle){.uData = UINT32_MAX};
    ptScene->uGPUMaterialBufferCapacity = 512;

    // create global bindgroup
    ptScene->uTextureIndexCount = 0;

    plBindGroupLayoutHandle tGlobalSceneBindGroupLayout = gptShaderVariant->get_bind_group_layout("global");

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        // create global bindgroup
        const plBindGroupDesc tGlobalBindGroupDesc = {
            .ptPool      = gptData->ptBindGroupPool,
            .tLayout     = tGlobalSceneBindGroupLayout,
            .pcDebugName = "global bind group"
        };
        ptScene->atGlobalBindGroup[i] = gptGfx->create_bind_group(gptData->ptDevice, &tGlobalBindGroupDesc);

        // partially update global bindgroup (just samplers)
        plBindGroupUpdateSamplerData tGlobalSamplerData[] = {
            {
                .tSampler = gptData->tDefaultSampler,
                .uSlot    = 3
            },
            {
                .tSampler = gptData->tEnvSampler,
                .uSlot    = 4
            }
        };

        plBindGroupUpdateData tGlobalBindGroupData = {
            .uSamplerCount     = 2,
            .atSamplerBindings = tGlobalSamplerData,
        };
        gptGfx->update_bind_group(gptData->ptDevice, ptScene->atGlobalBindGroup[i], &tGlobalBindGroupData);
    }

    // pre-create some global buffers, later we should defer this
    const plBufferDesc atLightShadowDataBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_STAGING,
        .szByteSize = PL_MAX_LIGHTS * sizeof(plGPULightShadowData),
        .pcDebugName = "shadow data buffer"
    };

    const plBufferDesc atCameraBuffersDesc = {
        .tUsage     = PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_STAGING,
        .szByteSize = 4096,
        .pcDebugName = "camera buffers"
    };

    const plBufferDesc atProbeDataBufferDesc = {
        .tUsage     = PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_STAGING,
        .szByteSize = 4096,
        .pcDebugName = "probe buffers"
    };

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        ptScene->atLightShadowDataBuffer[i] = pl__renderer_create_staging_buffer(&atLightShadowDataBufferDesc, "shadow buffer", i);
        ptScene->atShadowCameraBuffers[i]   = pl__renderer_create_staging_buffer(&atCameraBuffersDesc, "shadow camera buffer", i);
        ptScene->atGPUProbeDataBuffers[i]   = pl__renderer_create_staging_buffer(&atProbeDataBufferDesc, "probe buffer", i);
    }

    // pre-create working buffers for environment filtering, later we should defer this
    for(uint32_t i = 0; i < 7; i++)
    {
        const size_t uMaxFaceSize = ((size_t)1024 * (size_t)1024) * 4 * sizeof(float);

        const plBufferDesc tInputBufferDesc = {
            .tUsage    = PL_BUFFER_USAGE_STORAGE,
            .szByteSize = uMaxFaceSize,
            .pcDebugName = "filter buffers"
        };
        ptScene->atFilterWorkingBuffers[i] = pl__renderer_create_local_buffer(&tInputBufferDesc, "filter buffer", i, NULL, 0);
    }

    // create probe material & mesh
    plMaterialComponent* ptMaterial = NULL;
    plEntity tMaterial = pl_renderer_create_material(ptScene->ptComponentLibrary, "environment probe material", &ptMaterial);
    ptMaterial->tBlendMode = PL_BLEND_MODE_OPAQUE;
    ptMaterial->tShaderType = PL_SHADER_TYPE_PBR;
    ptMaterial->tFlags = PL_MATERIAL_FLAG_CAST_RECEIVE_SHADOW;
    ptMaterial->tBaseColor = (plVec4){1.0f, 1.0f, 1.0f, 1.0f};
    ptMaterial->fRoughness = 0.0f;
    ptMaterial->fMetalness = 1.0f;

    plMeshComponent* ptMesh = NULL;
    ptScene->tProbeMesh = gptMesh->create_sphere_mesh(ptScene->ptComponentLibrary, "environment probe mesh", 0.25f, 32, 32, &ptMesh);
    ptMesh->tMaterial = tMaterial;

    // create shadow atlas
    ptScene->uShadowAtlasResolution = 1024 * 8;
    const plTextureDesc tShadowDepthTextureDesc = {
        .tDimensions   = {(float)ptScene->uShadowAtlasResolution, (float)ptScene->uShadowAtlasResolution, 1},
        .tFormat       = PL_FORMAT_D16_UNORM,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT | PL_TEXTURE_USAGE_SAMPLED,
        .pcDebugName   = "shadow map"
    };
    ptScene->tShadowTexture = pl__renderer_create_local_texture(&tShadowDepthTextureDesc, "shadow map", 0, PL_TEXTURE_USAGE_SAMPLED);

    // create shadow map render passes
    const plRenderPassDesc tDepthRenderPassDesc = {
        .tLayout = gptData->tDepthRenderPassLayout,
        .tDepthTarget = {
                .tLoadOp         = PL_LOAD_OP_LOAD,
                .tStoreOp        = PL_STORE_OP_STORE,
                .tStencilLoadOp  = PL_LOAD_OP_CLEAR,
                .tStencilStoreOp = PL_STORE_OP_DONT_CARE,
                .tCurrentUsage   = PL_TEXTURE_USAGE_SAMPLED,
                .tNextUsage      = PL_TEXTURE_USAGE_SAMPLED,
                .fClearZ         = 0.0f
        },
        .tDimensions = {.x = (float)ptScene->uShadowAtlasResolution, .y = (float)ptScene->uShadowAtlasResolution}
    };

    const plRenderPassDesc tFirstDepthRenderPassDesc = {
        .tLayout = gptData->tDepthRenderPassLayout,
        .tDepthTarget = {
                .tLoadOp         = PL_LOAD_OP_CLEAR,
                .tStoreOp        = PL_STORE_OP_STORE,
                .tStencilLoadOp  = PL_LOAD_OP_CLEAR,
                .tStencilStoreOp = PL_STORE_OP_DONT_CARE,
                .tCurrentUsage   = PL_TEXTURE_USAGE_SAMPLED,
                .tNextUsage      = PL_TEXTURE_USAGE_SAMPLED,
                .fClearZ         = 0.0f
        },
        .tDimensions = {.x = (float)ptScene->uShadowAtlasResolution, .y = (float)ptScene->uShadowAtlasResolution}
    };

    plRenderPassAttachments atShadowAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
    ptScene->atShadowTextureBindlessIndices = pl__renderer_get_bindless_texture_index(ptScene, ptScene->tShadowTexture);
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        atShadowAttachmentSets[i].atViewAttachments[0] = ptScene->tShadowTexture;
    }
    ptScene->tShadowRenderPass = gptGfx->create_render_pass(gptData->ptDevice, &tDepthRenderPassDesc, atShadowAttachmentSets);
    ptScene->tFirstShadowRenderPass = gptGfx->create_render_pass(gptData->ptDevice, &tFirstDepthRenderPassDesc, atShadowAttachmentSets);
    return ptScene;
}

void
pl_renderer_cleanup_view(plView* ptView)
{

    pl_sb_free(ptView->sbtVisibleDrawables);
    pl_sb_free(ptView->sbuVisibleDeferredEntities);
    pl_sb_free(ptView->sbuVisibleForwardEntities);
    pl_sb_free(ptView->tDirectionLightShadowData.sbtDLightShadowData);

    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->tAlbedoTexture);
    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->tNormalTexture);
    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->tAOMetalRoughnessTexture);
    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->tRawOutputTexture);
    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->tDepthTexture);
    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->atUVMaskTexture0);
    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->atUVMaskTexture1);
    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->tFinalTexture);
    gptGfx->queue_render_pass_for_deletion(gptData->ptDevice, ptView->tRenderPass);
    gptGfx->queue_render_pass_for_deletion(gptData->ptDevice, ptView->tPostProcessRenderPass);
    gptGfx->queue_render_pass_for_deletion(gptData->ptDevice, ptView->tPickRenderPass);
    gptGfx->queue_render_pass_for_deletion(gptData->ptDevice, ptView->tUVRenderPass);
    gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, ptView->tFinalTextureHandle);
    gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, ptView->tLightingBindGroup);
    
    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->tPickTexture);

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptView->atGlobalBuffers[i]);
        gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptView->tDirectionLightShadowData.atDLightShadowDataBuffer[i]);
        gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptView->tDirectionLightShadowData.atDShadowCameraBuffers[i]);
        gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptView->atPickBuffer[i]);
        gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, ptView->atPickBindGroup[i]);
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

void
pl_renderer_cleanup_scene(plScene* ptScene)
{
    if(ptScene == NULL)
        return;

    for(uint32_t j = 0; j < pl_sb_size(ptScene->sbtProbeData); j++)
    {
        plEnvironmentProbeData* ptProbe = &ptScene->sbtProbeData[j];
        
        gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptProbe->tGGXLUTTexture);
        gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptProbe->tLambertianEnvTexture);
        gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptProbe->tGGXEnvTexture);
        gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptProbe->tAlbedoTexture);
        gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptProbe->tNormalTexture);
        gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptProbe->tAOMetalRoughnessTexture);
        gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptProbe->tRawOutputTexture);
        gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptProbe->tDepthTexture);

        for(uint32_t i = 0; i < 6; i++)
        {
            gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptProbe->atAlbedoTextureViews[i]);
            gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptProbe->atNormalTextureViews[i]);
            gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptProbe->atAOMetalRoughnessTextureViews[i]);
            gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptProbe->atRawOutputTextureViews[i]);
            gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptProbe->atDepthTextureViews[i]);
            gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, ptProbe->atLightingBindGroup[i]);
            gptGfx->queue_render_pass_for_deletion(gptData->ptDevice, ptProbe->atRenderPasses[i]);
        }

        for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        {
            gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptProbe->atGlobalBuffers[i]);
            gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptProbe->tDirectionLightShadowData.atDLightShadowDataBuffer[i]);
            gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptProbe->tDirectionLightShadowData.atDShadowCameraBuffers[i]);
        }

        for(uint32_t k = 0; k < 6; k++)
        {
            pl_sb_free(ptProbe->sbuVisibleDeferredEntities[k]);
            pl_sb_free(ptProbe->sbuVisibleForwardEntities[k]);
        }
        pl_sb_free(ptProbe->tDirectionLightShadowData.sbtDLightShadowData);
    }

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptScene->atLightBuffer[i]);
        gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptScene->atShadowCameraBuffers[i]);
        gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptScene->atLightShadowDataBuffer[i]);
        gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptScene->atGPUProbeDataBuffers[i]);
        gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, ptScene->atGlobalBindGroup[i]);
        gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptScene->atMaterialDataBuffer[i]);
    }

    for(uint32_t i = 0; i < 7; i++)
    {
        gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptScene->atFilterWorkingBuffers[i]);
    }

    gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptScene->tVertexBuffer);
    gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptScene->tIndexBuffer);
    gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptScene->tStorageBuffer);
    if(ptScene->tSkinStorageBuffer.uData != UINT32_MAX)
    {
        gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptScene->tSkinStorageBuffer);
        for(uint32_t i = 0; i < pl_sb_size(ptScene->sbtSkinData); i++)
        {
            for(uint32_t j = 0; j < gptGfx->get_frames_in_flight(); j++)
                gptGfx->queue_buffer_for_deletion(gptData->ptDevice,ptScene->sbtSkinData[i].atDynamicSkinBuffer[j]);
                
        }

    }
    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptScene->tShadowTexture);
    gptGfx->queue_render_pass_for_deletion(gptData->ptDevice, ptScene->tShadowRenderPass);
    gptGfx->queue_render_pass_for_deletion(gptData->ptDevice, ptScene->tFirstShadowRenderPass);
    gptShaderVariant->unload_manifest("/shaders/shaders.pls");

    if(ptScene->tSkyboxTexture.uIndex != 0)
    {
        gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptScene->tSkyboxTexture);
        gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, ptScene->tSkyboxBindGroup);
    }

    gptResource->clear();

    gptBvh->cleanup(&ptScene->tBvh);
    pl_sb_free(ptScene->sbtBvhAABBs);
    pl_sb_free(ptScene->sbtNodeStack);
    pl_sb_free(ptScene->sbtGPUProbeData);
    pl_sb_free(ptScene->sbtProbeData);
    pl_sb_free(ptScene->sbtShadowRects);
    pl_sb_free(ptScene->sbuShadowDeferredDrawables);
    pl_sb_free(ptScene->sbuShadowForwardDrawables);
    pl_sb_free(ptScene->sbtLightShadowData);
    pl_sb_free(ptScene->sbtLightData);
    pl_sb_free(ptScene->sbtVertexPosBuffer);
    pl_sb_free(ptScene->sbtVertexDataBuffer);
    pl_sb_free(ptScene->sbuIndexBuffer);
    pl_sb_free(ptScene->sbtMaterialBuffer);
    pl_sb_free(ptScene->sbtDrawables);
    pl_sb_free(ptScene->sbtStagedEntities);
    pl_sb_free(ptScene->sbtSkinData);
    pl_sb_free(ptScene->sbtSkinVertexDataBuffer);
    pl_sb_free(ptScene->sbtOutlinedEntities);
    pl_sb_free(ptScene->sbtOutlineDrawablesOldShaders);
    pl_sb_free(ptScene->sbtOutlineDrawablesOldEnvShaders);
    pl_hm_free(&ptScene->tDrawableHashmap);
    pl_hm_free(&ptScene->tMaterialHashmap);
    pl_hm_free(&ptScene->tTextureIndexHashmap);
    pl_hm_free(&ptScene->tCubeTextureIndexHashmap);

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
pl_renderer_create_view(plScene* ptScene, plVec2 tDimensions)
{

    plView* ptView = PL_ALLOC(sizeof(plView));
    memset(ptView, 0, sizeof(plView));

    pl_sb_push(ptScene->sbptViews, ptView);

    ptView->ptParentScene = ptScene;
    ptView->tTargetSize = tDimensions;

    // picking defaults
    ptView->tHoveredEntity.uData = 0;
    ptView->bRequestHoverCheck = false;

    // create offscreen per-frame resources
    const plTextureDesc tRawOutputTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R16G16B16A16_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .pcDebugName   = "offscreen final"
    };

    const plTextureDesc tPickTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R8G8B8A8_UNORM,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
        .pcDebugName   = "pick original"
    };

    const plTextureDesc tNormalTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R16G16_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .pcDebugName   = "g-buffer normal"
    };

    const plTextureDesc tAlbedoTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R8G8B8A8_UNORM,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .pcDebugName   = "albedo texture"
    };

    const plTextureDesc tDepthTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_D32_FLOAT_S8_UINT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT | PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .pcDebugName   = "offscreen depth texture"
    };

    const plTextureDesc tMaskTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R32G32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_STORAGE,
        .pcDebugName   = "mask texture"
    };

    const plTextureDesc tEmmissiveTexDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R16G16B16A16_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .pcDebugName   = "emissive texture"
    };

    const plBufferDesc atGlobalBuffersDesc = {
        .tUsage     = PL_BUFFER_USAGE_UNIFORM | PL_BUFFER_USAGE_STAGING,
        .szByteSize = 4096,
        .pcDebugName = "global buffer"
    };

    const plBufferDesc atCameraBuffersDesc = {
        .tUsage     = PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_STAGING,
        .szByteSize = 4096,
        .pcDebugName = "camera buffers"
    };

    const plBufferDesc atLightShadowDataBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_STAGING,
        .szByteSize = PL_MAX_LIGHTS * sizeof(plGPULightShadowData),
        .pcDebugName = "shadow data buffer"
    };

    // create offscreen render pass
    plRenderPassAttachments atPickAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
    plRenderPassAttachments atAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
    plRenderPassAttachments atUVAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
    plRenderPassAttachments atPostProcessAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
    plRenderPassAttachments atShadowAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};

    // pick bind group

    ptView->tRawOutputTexture        = pl__renderer_create_texture(&tRawOutputTextureDesc,  "offscreen raw", 0, PL_TEXTURE_USAGE_SAMPLED);
    ptView->tAlbedoTexture           = pl__renderer_create_texture(&tAlbedoTextureDesc, "albedo original", 0, PL_TEXTURE_USAGE_COLOR_ATTACHMENT);
    ptView->tNormalTexture           = pl__renderer_create_texture(&tNormalTextureDesc, "normal original", 0, PL_TEXTURE_USAGE_COLOR_ATTACHMENT);
    ptView->tAOMetalRoughnessTexture = pl__renderer_create_texture(&tEmmissiveTexDesc, "metalroughness original", 0, PL_TEXTURE_USAGE_COLOR_ATTACHMENT);
    ptView->tDepthTexture            = pl__renderer_create_texture(&tDepthTextureDesc, "offscreen depth original", 0, PL_TEXTURE_USAGE_SAMPLED);
    ptView->atUVMaskTexture0         = pl__renderer_create_texture(&tMaskTextureDesc, "uv mask texture 0", 0, PL_TEXTURE_USAGE_STORAGE);
    ptView->atUVMaskTexture1         = pl__renderer_create_texture(&tMaskTextureDesc, "uv mask texture 1", 0, PL_TEXTURE_USAGE_STORAGE);
    ptView->tFinalTexture            = pl__renderer_create_texture(&tRawOutputTextureDesc,  "offscreen final", 0, PL_TEXTURE_USAGE_SAMPLED);
    ptView->tFinalTextureHandle      = gptDrawBackend->create_bind_group_for_texture(ptView->tFinalTexture);

    // lighting bind group
    const plBindGroupDesc tLightingBindGroupDesc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = gptShaderVariant->get_bind_group_layout("deferred lighting 1"),
        .pcDebugName = "lighting bind group"
    };
    ptView->tLightingBindGroup = gptGfx->create_bind_group(gptData->ptDevice, &tLightingBindGroupDesc);

    const plBindGroupUpdateTextureData atBGTextureData[] = {
        {
            .tTexture = ptView->tAlbedoTexture,
            .uSlot    = 0,
            .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
        },
        {
            .tTexture = ptView->tNormalTexture,
            .uSlot    = 1,
            .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
        },
        {
            .tTexture = ptView->tAOMetalRoughnessTexture,
            .uSlot    = 2,
            .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
        },
        {
            .tTexture = ptView->tDepthTexture,
            .uSlot    = 3,
            .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
        }
    };
    const plBindGroupUpdateData tBGData = {
        .uTextureCount = 4,
        .atTextureBindings = atBGTextureData
    };
    gptGfx->update_bind_group(gptData->ptDevice, ptView->tLightingBindGroup, &tBGData);

    ptView->tPickTexture = pl__renderer_create_texture(&tPickTextureDesc, "pick original", 0, PL_TEXTURE_USAGE_SAMPLED);

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {

        const plBufferDesc tPickBufferDesc = {
            .tUsage     = PL_BUFFER_USAGE_STAGING | PL_BUFFER_USAGE_STORAGE,
            .szByteSize = sizeof(uint32_t) * 2,
            .pcDebugName = "Picking buffer"
        };
        ptView->atPickBuffer[i] = pl__renderer_create_cached_staging_buffer(&tPickBufferDesc, "picking buffer", 0);

        const plBindGroupDesc tPickBindGroupDesc = {
            .ptPool = gptData->ptBindGroupPool,
            .tLayout = gptShaderVariant->get_graphics_bind_group_layout("picking", 1),
            .pcDebugName = "pick bind group"
        };
        
        ptView->atPickBindGroup[i] = gptGfx->create_bind_group(gptData->ptDevice, &tPickBindGroupDesc);
    
        const plBindGroupUpdateBufferData atPickBGBufferData[] = {
            { .uSlot = 0, .tBuffer = ptView->atPickBuffer[i], .szBufferRange = sizeof(uint32_t) * 2},
        };
    
        const plBindGroupUpdateData tPickBGData = {
            .uBufferCount = 1,
            .atBufferBindings = atPickBGBufferData
        };
        gptGfx->update_bind_group(gptData->ptDevice, ptView->atPickBindGroup[i], &tPickBGData);

        // buffers
        ptView->atGlobalBuffers[i] = pl__renderer_create_staging_buffer(&atGlobalBuffersDesc, "global", i);
        

        pl_temp_allocator_reset(&gptData->tTempAllocator);

        // attachment sets
        atPickAttachmentSets[i].atViewAttachments[0] = ptView->tDepthTexture;
        atPickAttachmentSets[i].atViewAttachments[1] = ptView->tPickTexture;

        atAttachmentSets[i].atViewAttachments[0] = ptView->tDepthTexture;
        atAttachmentSets[i].atViewAttachments[1] = ptView->tRawOutputTexture;
        atAttachmentSets[i].atViewAttachments[2] = ptView->tAlbedoTexture;
        atAttachmentSets[i].atViewAttachments[3] = ptView->tNormalTexture;
        atAttachmentSets[i].atViewAttachments[4] = ptView->tAOMetalRoughnessTexture;

        atUVAttachmentSets[i].atViewAttachments[0] = ptView->tDepthTexture;
        atUVAttachmentSets[i].atViewAttachments[1] = ptView->atUVMaskTexture0;
        
        atPostProcessAttachmentSets[i].atViewAttachments[0] = ptView->tDepthTexture;
        atPostProcessAttachmentSets[i].atViewAttachments[1] = ptView->tFinalTexture;

        ptView->tDirectionLightShadowData.atDLightShadowDataBuffer[i] = pl__renderer_create_staging_buffer(&atLightShadowDataBufferDesc, "d shadow", i);
        ptView->tDirectionLightShadowData.atDShadowCameraBuffers[i] = pl__renderer_create_staging_buffer(&atCameraBuffersDesc, "d shadow buffer", i);

    }

    const plRenderPassDesc tRenderPassDesc = {
        .tLayout = gptData->tRenderPassLayout,
        .tDepthTarget = {
                .tLoadOp         = PL_LOAD_OP_CLEAR,
                .tStoreOp        = PL_STORE_OP_STORE,
                .tStencilLoadOp  = PL_LOAD_OP_CLEAR,
                .tStencilStoreOp = PL_STORE_OP_STORE,
                .tCurrentUsage   = PL_TEXTURE_USAGE_SAMPLED,
                .tNextUsage      = PL_TEXTURE_USAGE_SAMPLED,
                .fClearZ         = 0.0f
        },
        .atColorTargets = {
            {
                .tLoadOp       = PL_LOAD_OP_CLEAR,
                .tStoreOp      = PL_STORE_OP_STORE,
                .tCurrentUsage = PL_TEXTURE_USAGE_SAMPLED,
                .tNextUsage    = PL_TEXTURE_USAGE_SAMPLED,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
            },
            {
                .tLoadOp       = PL_LOAD_OP_CLEAR,
                .tStoreOp      = PL_STORE_OP_STORE,
                .tCurrentUsage = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .tNextUsage    = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
            },
            {
                .tLoadOp       = PL_LOAD_OP_CLEAR,
                .tStoreOp      = PL_STORE_OP_STORE,
                .tCurrentUsage = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .tNextUsage    = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
            },
            {
                .tLoadOp       = PL_LOAD_OP_CLEAR,
                .tStoreOp      = PL_STORE_OP_STORE,
                .tCurrentUsage = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .tNextUsage    = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
            },
            {
                .tLoadOp       = PL_LOAD_OP_CLEAR,
                .tStoreOp      = PL_STORE_OP_STORE,
                .tCurrentUsage = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .tNextUsage    = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 0.0f}
            }
        },
        .tDimensions = {.x = ptView->tTargetSize.x, .y = ptView->tTargetSize.y}
    };
    ptView->tRenderPass = gptGfx->create_render_pass(gptData->ptDevice, &tRenderPassDesc, atAttachmentSets);

    const plRenderPassDesc tPickRenderPassDesc = {
        .tLayout = gptData->tPickRenderPassLayout,
        .tDepthTarget = {
                .tLoadOp         = PL_LOAD_OP_LOAD,
                .tStoreOp        = PL_STORE_OP_STORE,
                .tStencilLoadOp  = PL_LOAD_OP_LOAD,
                .tStencilStoreOp = PL_STORE_OP_STORE,
                .tCurrentUsage   = PL_TEXTURE_USAGE_SAMPLED,
                .tNextUsage      = PL_TEXTURE_USAGE_SAMPLED,
                .fClearZ         = 0.0f
        },
        .atColorTargets = {
            {
                .tLoadOp       = PL_LOAD_OP_CLEAR,
                .tStoreOp      = PL_STORE_OP_STORE,
                .tCurrentUsage = PL_TEXTURE_USAGE_SAMPLED,
                .tNextUsage    = PL_TEXTURE_USAGE_SAMPLED,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
            }
        },
        .tDimensions = {.x = ptView->tTargetSize.x, .y = ptView->tTargetSize.y}
    };
    ptView->tPickRenderPass = gptGfx->create_render_pass(gptData->ptDevice, &tPickRenderPassDesc, atPickAttachmentSets);

    const plRenderPassDesc tPostProcessRenderPassDesc = {
        .tLayout = gptData->tPostProcessRenderPassLayout,
        .tDepthTarget = {
                .tLoadOp         = PL_LOAD_OP_CLEAR,
                .tStoreOp        = PL_STORE_OP_DONT_CARE,
                .tStencilLoadOp  = PL_LOAD_OP_LOAD,
                .tStencilStoreOp = PL_STORE_OP_STORE,
                .tCurrentUsage   = PL_TEXTURE_USAGE_SAMPLED,
                .tNextUsage      = PL_TEXTURE_USAGE_SAMPLED,
                .fClearZ         = 0.0f
        },
        .atColorTargets = {
            {
                .tLoadOp       = PL_LOAD_OP_CLEAR,
                .tStoreOp      = PL_STORE_OP_STORE,
                .tCurrentUsage = PL_TEXTURE_USAGE_SAMPLED,
                .tNextUsage    = PL_TEXTURE_USAGE_SAMPLED,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
            }
        },
        .tDimensions = {.x = ptView->tTargetSize.x, .y = ptView->tTargetSize.y}
    };
    ptView->tPostProcessRenderPass = gptGfx->create_render_pass(gptData->ptDevice, &tPostProcessRenderPassDesc, atPostProcessAttachmentSets);

    // register debug 3D drawlist
    ptView->pt3DDrawList = gptDraw->request_3d_drawlist();
    ptView->pt3DGizmoDrawList = gptDraw->request_3d_drawlist();
    ptView->pt3DSelectionDrawList = gptDraw->request_3d_drawlist();

    // create offscreen renderpass
    const plRenderPassDesc tUVRenderPass0Desc = {
        .tLayout = gptData->tUVRenderPassLayout,
        .tDepthTarget = {
                .tLoadOp         = PL_LOAD_OP_CLEAR,
                .tStoreOp        = PL_STORE_OP_DONT_CARE,
                .tStencilLoadOp  = PL_LOAD_OP_LOAD,
                .tStencilStoreOp = PL_STORE_OP_STORE,
                .tCurrentUsage   = PL_TEXTURE_USAGE_SAMPLED,
                .tNextUsage      = PL_TEXTURE_USAGE_SAMPLED,
                .fClearZ         = 0.0f
        },
        .atColorTargets = {
            {
                .tLoadOp       = PL_LOAD_OP_CLEAR,
                .tStoreOp      = PL_STORE_OP_STORE,
                .tCurrentUsage = PL_TEXTURE_USAGE_STORAGE,
                .tNextUsage    = PL_TEXTURE_USAGE_STORAGE,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 0.0f}
            }
        },
        .tDimensions = {.x = ptView->tTargetSize.x, .y = ptView->tTargetSize.y}
    };
    ptView->tUVRenderPass = gptGfx->create_render_pass(gptData->ptDevice, &tUVRenderPass0Desc, atUVAttachmentSets);

    return ptView;
}

void
pl_renderer_resize_view(plView* ptView, plVec2 tDimensions)
{
    // for convience
    plDevice* ptDevice = gptData->ptDevice;

    // update offscreen size to match viewport
    ptView->tTargetSize = tDimensions;

    // recreate offscreen color & depth textures
    const plTextureDesc tRawOutputTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R16G16B16A16_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT
    };

    const plTextureDesc tPickTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R8G8B8A8_UNORM,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT
    };

    const plTextureDesc tNormalTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R16G16_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .pcDebugName   = "g-buffer normal"
    };

    const plTextureDesc tAlbedoTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R8G8B8A8_UNORM,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT
    };

    const plTextureDesc tDepthTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_D32_FLOAT_S8_UINT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT | PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_INPUT_ATTACHMENT
    };

    const plTextureDesc tMaskTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R32G32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_STORAGE
    };

    const plTextureDesc tEmmissiveTexDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R16G16B16A16_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT
    };

    // update offscreen render pass attachments
    plRenderPassAttachments atAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
    plRenderPassAttachments atUVAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
    plRenderPassAttachments atPostProcessAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
    plRenderPassAttachments atPickAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};


    // queue old resources for deletion
    gptGfx->queue_texture_for_deletion(ptDevice, ptView->tFinalTexture);
    gptGfx->queue_texture_for_deletion(ptDevice, ptView->atUVMaskTexture0);
    gptGfx->queue_texture_for_deletion(ptDevice, ptView->atUVMaskTexture1);
    gptGfx->queue_texture_for_deletion(ptDevice, ptView->tRawOutputTexture);
    gptGfx->queue_texture_for_deletion(ptDevice, ptView->tAlbedoTexture);
    gptGfx->queue_texture_for_deletion(ptDevice, ptView->tNormalTexture);
    gptGfx->queue_texture_for_deletion(ptDevice, ptView->tAOMetalRoughnessTexture);
    gptGfx->queue_texture_for_deletion(ptDevice, ptView->tDepthTexture);
    gptGfx->queue_bind_group_for_deletion(ptDevice, ptView->tLightingBindGroup);

    gptGfx->queue_texture_for_deletion(ptDevice, ptView->tPickTexture);
    ptView->tPickTexture = pl__renderer_create_texture(&tPickTextureDesc, "pick", 0, PL_TEXTURE_USAGE_SAMPLED);

    // textures
    ptView->tRawOutputTexture        = pl__renderer_create_texture(&tRawOutputTextureDesc, "offscreen raw", 0, PL_TEXTURE_USAGE_SAMPLED);
    ptView->tAlbedoTexture           = pl__renderer_create_texture(&tAlbedoTextureDesc, "albedo original", 0, PL_TEXTURE_USAGE_COLOR_ATTACHMENT);
    ptView->tNormalTexture           = pl__renderer_create_texture(&tNormalTextureDesc, "normal resize", 0, PL_TEXTURE_USAGE_COLOR_ATTACHMENT);
    ptView->tAOMetalRoughnessTexture = pl__renderer_create_texture(&tEmmissiveTexDesc, "metalroughness original", 0, PL_TEXTURE_USAGE_COLOR_ATTACHMENT);
    ptView->tDepthTexture            = pl__renderer_create_texture(&tDepthTextureDesc, "offscreen depth original", 0, PL_TEXTURE_USAGE_SAMPLED);
    ptView->atUVMaskTexture0         = pl__renderer_create_texture(&tMaskTextureDesc, "uv mask texture 0", 0, PL_TEXTURE_USAGE_STORAGE);
    ptView->atUVMaskTexture1         = pl__renderer_create_texture(&tMaskTextureDesc, "uv mask texture 1", 0, PL_TEXTURE_USAGE_STORAGE);
    ptView->tFinalTexture            = pl__renderer_create_texture(&tRawOutputTextureDesc,  "offscreen final", 0, PL_TEXTURE_USAGE_SAMPLED);
    ptView->tFinalTextureHandle      = gptDrawBackend->create_bind_group_for_texture(ptView->tFinalTexture);

    // lighting bind group
    const plBindGroupDesc tLightingBindGroupDesc = {
        .ptPool = gptData->ptBindGroupPool,
        .tLayout = gptShaderVariant->get_graphics_bind_group_layout("deferred_lighting", 1),
        .pcDebugName = "lighting bind group"
    };
    ptView->tLightingBindGroup = gptGfx->create_bind_group(gptData->ptDevice, &tLightingBindGroupDesc);
    const plBindGroupUpdateTextureData atBGTextureData[] = {
        {
            .tTexture = ptView->tAlbedoTexture,
            .uSlot    = 0,
            .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
        },
        {
            .tTexture = ptView->tNormalTexture,
            .uSlot    = 1,
            .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
        },
        {
            .tTexture = ptView->tAOMetalRoughnessTexture,
            .uSlot    = 2,
            .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
        },
        {
            .tTexture = ptView->tDepthTexture,
            .uSlot    = 3,
            .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
        }
    };
    const plBindGroupUpdateData tBGData = {
        .uTextureCount = 4,
        .atTextureBindings = atBGTextureData
    };
    gptGfx->update_bind_group(gptData->ptDevice, ptView->tLightingBindGroup, &tBGData);

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        // attachment sets
        atPickAttachmentSets[i].atViewAttachments[0] = ptView->tDepthTexture;
        atPickAttachmentSets[i].atViewAttachments[1] = ptView->tPickTexture;

        atAttachmentSets[i].atViewAttachments[0] = ptView->tDepthTexture;
        atAttachmentSets[i].atViewAttachments[1] = ptView->tRawOutputTexture;
        atAttachmentSets[i].atViewAttachments[2] = ptView->tAlbedoTexture;
        atAttachmentSets[i].atViewAttachments[3] = ptView->tNormalTexture;
        atAttachmentSets[i].atViewAttachments[4] = ptView->tAOMetalRoughnessTexture;
        
        atPostProcessAttachmentSets[i].atViewAttachments[0] = ptView->tDepthTexture;
        atPostProcessAttachmentSets[i].atViewAttachments[1] = ptView->tFinalTexture;

        atUVAttachmentSets[i].atViewAttachments[0] = ptView->tDepthTexture;
        atUVAttachmentSets[i].atViewAttachments[1] = ptView->atUVMaskTexture0;
    }
    gptGfx->update_render_pass_attachments(ptDevice, ptView->tRenderPass, ptView->tTargetSize, atAttachmentSets);
    gptGfx->update_render_pass_attachments(ptDevice, ptView->tPostProcessRenderPass, ptView->tTargetSize, atPostProcessAttachmentSets);
    gptGfx->update_render_pass_attachments(ptDevice, ptView->tPickRenderPass, ptView->tTargetSize, atPickAttachmentSets);
    gptGfx->update_render_pass_attachments(ptDevice, ptView->tUVRenderPass, ptView->tTargetSize, atUVAttachmentSets);
}

void
pl_renderer_cleanup(void)
{
    pl_temp_allocator_free(&gptData->tTempAllocator);
    gptGfx->cleanup_draw_stream(&gptData->tDrawStream);

    pl_sb_free(gptData->sbptScenes);
    gptResource->cleanup();
    gptGfx->flush_device(gptData->ptDevice);

    gptGfx->cleanup_semaphore(gptData->ptClickSemaphore);

    gptGfx->cleanup_bind_group_pool(gptData->ptBindGroupPool);
    gptGpuAllocators->cleanup(gptData->ptDevice);
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        gptGfx->cleanup_bind_group_pool(gptData->aptTempGroupPools[i]);
    }

    PL_FREE(gptData);
}

void
pl_renderer_load_skybox_from_panorama(plScene* ptScene, const char* pcPath, int iResolution)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);
    const int iSamples = 512;
    plDevice* ptDevice = gptData->ptDevice;
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();

    int iPanoramaWidth = 0;
    int iPanoramaHeight = 0;
    int iUnused = 0;
    pl_begin_cpu_sample(gptProfile, 0, "load image");
    size_t szImageFileSize = gptVfs->get_file_size_str(pcPath);
    unsigned char* pucBuffer = PL_ALLOC(szImageFileSize);
    plVfsFileHandle tEnvMapHandle = gptVfs->open_file(pcPath, PL_VFS_FILE_MODE_READ);
    gptVfs->read_file(tEnvMapHandle, pucBuffer, &szImageFileSize);
    gptVfs->close_file(tEnvMapHandle);
    float* pfPanoramaData = gptImage->load_hdr(pucBuffer, (int)szImageFileSize, &iPanoramaWidth, &iPanoramaHeight, &iUnused, 4);
    PL_FREE(pucBuffer);
    pl_end_cpu_sample(gptProfile, 0);
    PL_ASSERT(pfPanoramaData);

    const size_t uFaceSize = ((size_t)iResolution * (size_t)iResolution) * 4 * sizeof(float);
    {
        int aiSkyboxSpecializationData[] = {iResolution, iPanoramaWidth, iPanoramaHeight};
        plComputeShaderHandle tPanoramaShader = gptShaderVariant->get_compute_shader("panorama_to_cubemap", aiSkyboxSpecializationData);
        pl_temp_allocator_reset(&gptData->tTempAllocator);

        plBufferHandle atComputeBuffers[7] = {0};
        const uint32_t uPanoramaSize = iPanoramaHeight * iPanoramaWidth * 4 * sizeof(float);
        const plBufferDesc tInputBufferDesc = {
            .tUsage     = PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_STAGING,
            .szByteSize = uPanoramaSize,
            .pcDebugName = "panorama input buffer"
        };
        atComputeBuffers[0] = pl__renderer_create_staging_buffer(&tInputBufferDesc, "panorama input", 0);
        plBuffer* ptComputeBuffer = gptGfx->get_buffer(ptDevice, atComputeBuffers[0]);
        memcpy(ptComputeBuffer->tMemoryAllocation.pHostMapped, pfPanoramaData, uPanoramaSize);
        
        gptImage->free(pfPanoramaData);

        const plBufferDesc tOutputBufferDesc = {
            .tUsage    = PL_BUFFER_USAGE_STORAGE,
            .szByteSize = uFaceSize,
            .pcDebugName = "panorama output buffer"
        };
        
        for(uint32_t i = 0; i < 6; i++)
            atComputeBuffers[i + 1] = pl__renderer_create_local_buffer(&tOutputBufferDesc, "panorama output", i, NULL, 0);

        const plBindGroupDesc tComputeBindGroupDesc = {
            .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
            .tLayout     = gptShaderVariant->get_compute_bind_group_layout("panorama_to_cubemap", 0),
            .pcDebugName = "compute bind group"
        };
        plBindGroupHandle tComputeBindGroup = gptGfx->create_bind_group(ptDevice, &tComputeBindGroupDesc);
        const plBindGroupUpdateBufferData atBGBufferData[] = { 
            { .uSlot = 0, .tBuffer = atComputeBuffers[0], .szBufferRange = uPanoramaSize},
            { .uSlot = 1, .tBuffer = atComputeBuffers[1], .szBufferRange = uFaceSize},
            { .uSlot = 2, .tBuffer = atComputeBuffers[2], .szBufferRange = uFaceSize},
            { .uSlot = 3, .tBuffer = atComputeBuffers[3], .szBufferRange = uFaceSize},
            { .uSlot = 4, .tBuffer = atComputeBuffers[4], .szBufferRange = uFaceSize},
            { .uSlot = 5, .tBuffer = atComputeBuffers[5], .szBufferRange = uFaceSize},
            { .uSlot = 6, .tBuffer = atComputeBuffers[6], .szBufferRange = uFaceSize}
        };
        const plBindGroupUpdateData tBGData = {
            .uBufferCount = 7,
            .atBufferBindings = atBGBufferData
        };
        gptGfx->update_bind_group(ptDevice, tComputeBindGroup, &tBGData);
        gptGfx->queue_bind_group_for_deletion(ptDevice, tComputeBindGroup);

        // calculate cubemap data
        const plDispatch tDispach = {
            .uGroupCountX     = (uint32_t)iResolution / 16,
            .uGroupCountY     = (uint32_t)iResolution / 16,
            .uGroupCountZ     = 2,
            .uThreadPerGroupX = 16,
            .uThreadPerGroupY = 16,
            .uThreadPerGroupZ = 3
        };
        
        plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
        gptGfx->begin_command_recording(ptCommandBuffer, NULL);

        const plPassBufferResource atPassBuffers[] = {
            { .tHandle = atComputeBuffers[0], .tStages = PL_SHADER_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_READ },
            { .tHandle = atComputeBuffers[1], .tStages = PL_SHADER_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
            { .tHandle = atComputeBuffers[2], .tStages = PL_SHADER_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
            { .tHandle = atComputeBuffers[3], .tStages = PL_SHADER_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
            { .tHandle = atComputeBuffers[4], .tStages = PL_SHADER_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
            { .tHandle = atComputeBuffers[5], .tStages = PL_SHADER_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
            { .tHandle = atComputeBuffers[6], .tStages = PL_SHADER_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
        };

        const plPassResources tPassResources = {
            .uBufferCount = 7,
            .atBuffers = atPassBuffers
        };

        plComputeEncoder* ptComputeEncoder = gptGfx->begin_compute_pass(ptCommandBuffer, &tPassResources);
        gptGfx->pipeline_barrier_compute(ptComputeEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_READ, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_WRITE);
        gptGfx->bind_compute_bind_groups(ptComputeEncoder, tPanoramaShader, 0, 1, &tComputeBindGroup, 0, NULL);
        gptGfx->bind_compute_shader(ptComputeEncoder, tPanoramaShader);
        gptGfx->dispatch(ptComputeEncoder, 1, &tDispach);
        gptGfx->pipeline_barrier_compute(ptComputeEncoder, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_READ);
        gptGfx->end_compute_pass(ptComputeEncoder);
        gptGfx->end_command_recording(ptCommandBuffer);
        gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
        gptGfx->wait_on_command_buffer(ptCommandBuffer);
        gptGfx->return_command_buffer(ptCommandBuffer);
        gptGfx->queue_compute_shader_for_deletion(ptDevice, tPanoramaShader);

        const plTextureDesc tSkyboxTextureDesc = {
            .tDimensions = {(float)iResolution, (float)iResolution, 1},
            .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
            .uLayers     = 6,
            .uMips       = 1,
            .tType       = PL_TEXTURE_TYPE_CUBE,
            .tUsage      = PL_TEXTURE_USAGE_SAMPLED
        };
        ptScene->tSkyboxTexture = pl__renderer_create_texture(&tSkyboxTextureDesc, "skybox texture", 0, PL_TEXTURE_USAGE_SAMPLED);

        ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
        gptGfx->begin_command_recording(ptCommandBuffer, NULL);
        plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
        gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);

        for(uint32_t i = 0; i < 6; i++)
        {
            const plBufferImageCopy tBufferImageCopy = {
                .uImageWidth = iResolution,
                .uImageHeight = iResolution,
                .uImageDepth = 1,
                .uLayerCount    = 1,
                .szBufferOffset = 0,
                .uBaseArrayLayer = i,
            };
            gptGfx->copy_buffer_to_texture(ptBlitEncoder, atComputeBuffers[i + 1], ptScene->tSkyboxTexture, 1, &tBufferImageCopy);
        }

        gptGfx->generate_mipmaps(ptBlitEncoder, ptScene->tSkyboxTexture);

        gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
        gptGfx->end_blit_pass(ptBlitEncoder);
        gptGfx->end_command_recording(ptCommandBuffer);
        gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
        gptGfx->wait_on_command_buffer(ptCommandBuffer);
        gptGfx->return_command_buffer(ptCommandBuffer);
        
        for(uint32_t i = 0; i < 7; i++)
            gptGfx->destroy_buffer(ptDevice, atComputeBuffers[i]);

    
        const plBindGroupDesc tSkyboxBindGroupDesc = {
            .ptPool = gptData->ptBindGroupPool,
            .tLayout = gptShaderVariant->get_graphics_bind_group_layout("skybox", 1),
            .pcDebugName = "skybox bind group"
        };
        ptScene->tSkyboxBindGroup = gptGfx->create_bind_group(ptDevice, &tSkyboxBindGroupDesc);

        {
            const plBindGroupUpdateTextureData tTextureData1 = {.tTexture = ptScene->tSkyboxTexture, .uSlot = 0, .uIndex = 0, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED};
            const plBindGroupUpdateData tBGData1 = {
                .uTextureCount = 1,
                .atTextureBindings = &tTextureData1
            };
            gptGfx->update_bind_group(ptDevice, ptScene->tSkyboxBindGroup, &tBGData1);
        }

        pl__renderer_add_skybox_drawable(ptScene);
    }

    pl_end_cpu_sample(gptProfile, 0);
}

void
pl_renderer_outline_entities(plScene* ptScene, uint32_t uCount, plEntity* atEntities)
{
    // for convience
    plDevice* ptDevice = gptData->ptDevice;

    int iSceneWideRenderingFlags = 0;
    if(gptData->tRuntimeOptions.bPunctualLighting)
        iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_PUNCTUAL;
    if(gptData->tRuntimeOptions.bImageBasedLighting)
        iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_IBL;

    // reset old entities
    const uint32_t uOldSelectedEntityCount = pl_sb_size(ptScene->sbtOutlinedEntities);
    const plEcsTypeKey tMeshComponentType = gptMesh->get_ecs_type_key_mesh();
    for(uint32_t i = 0; i < uOldSelectedEntityCount; i++)
    {
        plEntity tEntity = ptScene->sbtOutlinedEntities[i];

        uint64_t ulIndex = 0;
        if(pl_hm_has_key_ex(&ptScene->tDrawableHashmap, tEntity.uData, &ulIndex))
        {
            plDrawable* ptDrawable = &ptScene->sbtDrawables[ulIndex];
            ptDrawable->tShader = ptScene->sbtOutlineDrawablesOldShaders[i];
            ptDrawable->tEnvShader = ptScene->sbtOutlineDrawablesOldEnvShaders[i];

            // if instanced, find parent
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
                        ptParentDrawable->tShader = ptDrawable->tShader;
                        break;
                    }
                }
            }
        }
    }
    pl_sb_reset(ptScene->sbtOutlinedEntities);
    pl_sb_reset(ptScene->sbtOutlineDrawablesOldShaders);
    pl_sb_reset(ptScene->sbtOutlineDrawablesOldEnvShaders);

    for(uint32_t i = 0; i < uCount; i++)
    {
        plEntity tEntity = atEntities[i];

        plObjectComponent* ptObject   = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tEntity);
        if(ptObject == NULL)
            continue;
        plMeshComponent*     ptMesh     = gptECS->get_component(ptScene->ptComponentLibrary, tMeshComponentType, ptObject->tMesh);
        plMaterialComponent* ptMaterial = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tMaterialComponentType, ptMesh->tMaterial);

        ptMaterial->tFlags |= PL_MATERIAL_FLAG_OUTLINE;

        int iDataStride = 0;
        int iFlagCopy0 = (int)ptMesh->ulVertexStreamMask;
        while(iFlagCopy0)
        {
            iDataStride += iFlagCopy0 & 1;
            iFlagCopy0 >>= 1;
        }

        int iTextureMappingFlags = 0;
        for(uint32_t j = 0; j < PL_TEXTURE_SLOT_COUNT; j++)
        {
            if((ptMaterial->atTextureMaps[j].acName[0] != 0))
                iTextureMappingFlags |= 1 << j; 
        }

        int iObjectRenderingFlags = iSceneWideRenderingFlags;

        if(ptMaterial->tFlags & PL_MATERIAL_FLAG_CAST_RECEIVE_SHADOW)
        {
            iObjectRenderingFlags |= PL_RENDERING_FLAG_SHADOWS;
        }

        // choose shader variant
        int aiConstantData0[] = {
            (int)ptMesh->ulVertexStreamMask,
            iDataStride,
            iTextureMappingFlags,
            PL_INFO_MATERIAL_METALLICROUGHNESS,
            iObjectRenderingFlags,
            pl_sb_capacity(ptScene->sbtLightData),
            pl_sb_capacity(ptScene->sbtProbeData),
        };

        uint64_t ulIndex = 0;
        if(pl_hm_has_key_ex(&ptScene->tDrawableHashmap, tEntity.uData, &ulIndex))
        {
            plDrawable* ptDrawable = &ptScene->sbtDrawables[ulIndex];
            plShader* ptOldShader = gptGfx->get_shader(ptDevice, ptDrawable->tShader);
            plGraphicsState tVariantTemp = ptOldShader->tDesc.tGraphicsState;

            // write into stencil buffer
            tVariantTemp.ulStencilTestEnabled = 1;
            tVariantTemp.ulStencilMode        = PL_COMPARE_MODE_ALWAYS;
            tVariantTemp.ulStencilRef         = 0xff;
            tVariantTemp.ulStencilMask        = 0xff;
            tVariantTemp.ulStencilOpFail      = PL_STENCIL_OP_REPLACE;
            tVariantTemp.ulStencilOpDepthFail = PL_STENCIL_OP_REPLACE;
            tVariantTemp.ulStencilOpPass      = PL_STENCIL_OP_REPLACE;

            pl_sb_push(ptScene->sbtOutlinedEntities, ptDrawable->tEntity);
            pl_sb_push(ptScene->sbtOutlineDrawablesOldShaders, ptDrawable->tShader);
            pl_sb_push(ptScene->sbtOutlineDrawablesOldEnvShaders, ptDrawable->tEnvShader);

            if(ptDrawable->tFlags & PL_DRAWABLE_FLAG_FORWARD)
                ptDrawable->tShader = gptShaderVariant->get_shader("forward", &tVariantTemp, aiConstantData0, NULL);
            else if(ptDrawable->tFlags & PL_DRAWABLE_FLAG_DEFERRED)
                ptDrawable->tShader = gptShaderVariant->get_shader("gbuffer_fill", &tVariantTemp, aiConstantData0, NULL);

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
                        ptParentDrawable->tShader = ptDrawable->tShader;
                        break;
                    }
                }
            }
        }
    }
}

void
pl_renderer_reload_scene_shaders(plScene* ptScene)
{

    if(ptScene == NULL)
        return;

    // fill CPU buffers & drawable list
    pl_begin_cpu_sample(gptProfile, 0, "recreate shaders");

    gptScreenLog->add_message_ex(0, 15.0, PL_COLOR_32_CYAN, 1.0f, "%s", "reloaded shaders");

    gptShaderVariant->unload_manifest("/shaders/shaders.pls");
    gptShaderVariant->load_manifest("/shaders/shaders.pls");
    gptData->tSceneBGLayout = gptShaderVariant->get_bind_group_layout("scene");
    gptData->tShadowGlobalBGLayout = gptShaderVariant->get_bind_group_layout("shadow");

    if(!ptScene->bActive)
    {
        pl_end_cpu_sample(gptProfile, 0);
        return;
    }

    plDevice* ptDevice = gptData->ptDevice;

    pl_log_info_f(gptLog, gptData->uLogChannel, "reload shaders for scene %s", ptScene->pcName);

    plShaderOptions tOriginalOptions = *gptShader->get_options();

    plShaderOptions tNewDefaultShaderOptions = {
        .apcIncludeDirectories = {
            "../shaders/"
        },
        .apcDirectories = {
            "../shaders/"
        },
        .tFlags = PL_SHADER_FLAGS_AUTO_OUTPUT | PL_SHADER_FLAGS_INCLUDE_DEBUG | PL_SHADER_FLAGS_ALWAYS_COMPILE

    };
    gptShader->set_options(&tNewDefaultShaderOptions);

    pl_sb_reset(ptScene->sbtOutlinedEntities);
    pl_sb_reset(ptScene->sbtOutlineDrawablesOldShaders);
    pl_sb_reset(ptScene->sbtOutlineDrawablesOldEnvShaders);

    int iSceneWideRenderingFlags = PL_RENDERING_FLAG_SHADOWS;
    if(gptData->tRuntimeOptions.bPunctualLighting)
        iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_PUNCTUAL;
    if(gptData->tRuntimeOptions.bImageBasedLighting)
        iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_IBL;

    plLightComponent* ptLights = NULL;
    const uint32_t uLightCount = gptECS->get_components(ptScene->ptComponentLibrary, gptData->tLightComponentType, (void**)&ptLights, NULL);
    int aiLightingConstantData[] = {iSceneWideRenderingFlags, pl_sb_capacity(ptScene->sbtLightData), pl_sb_size(ptScene->sbtProbeData)};
    ptScene->tLightingShader = gptShaderVariant->get_shader("deferred_lighting", NULL, aiLightingConstantData, &gptData->tRenderPassLayout);
    aiLightingConstantData[0] = gptData->tRuntimeOptions.bPunctualLighting ? (PL_RENDERING_FLAG_USE_PUNCTUAL | PL_RENDERING_FLAG_SHADOWS) : 0;
    ptScene->tEnvLightingShader = gptShaderVariant->get_shader("deferred_lighting", NULL, aiLightingConstantData, &gptData->tRenderPassLayout);

    pl__renderer_unstage_drawables(ptScene);
    pl__renderer_set_drawable_shaders(ptScene);
    pl__renderer_sort_drawables(ptScene);

    gptShader->set_options(&tOriginalOptions);
    pl_end_cpu_sample(gptProfile, 0);
}

void
pl_renderer_finalize_scene(plScene* ptScene)
{
    // for convience
    plDevice* ptDevice = gptData->ptDevice;

    const plEntity* ptProbes = NULL;
    const uint32_t uProbeCount = gptECS->get_components(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, NULL, &ptProbes);

    for(uint32_t i = 0; i < uProbeCount; i++)
        pl__renderer_create_probe_data(ptScene, ptProbes[i]);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~textures~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    plMaterialComponent* ptMaterials = NULL;
    const plEntity* ptMaterialEntities = NULL;
    const uint32_t uMaterialCount = gptECS->get_components(ptScene->ptComponentLibrary, gptData->tMaterialComponentType, (void**)&ptMaterials, &ptMaterialEntities);
    pl_renderer_add_materials_to_scene(ptScene, uMaterialCount, ptMaterialEntities);

    plLightComponent* ptLights = NULL;
    const uint32_t uLightCount = gptECS->get_components(ptScene->ptComponentLibrary, gptData->tLightComponentType, (void**)&ptLights, NULL);

    pl_sb_reserve(ptScene->sbtVertexDataBuffer, 40000000);
    pl_sb_reserve(ptScene->sbtVertexPosBuffer, 15000000);
    int iLightCount = (int)uLightCount;

    pl_sb_reserve(ptScene->sbtLightData, iLightCount);

    pl__renderer_unstage_drawables(ptScene);
    pl__renderer_set_drawable_shaders(ptScene);
    pl__renderer_sort_drawables(ptScene);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~GPU Buffers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const plBufferDesc tMaterialDataBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_STORAGE,
        .szByteSize = sizeof(plGPUMaterial) * ptScene->uGPUMaterialBufferCapacity,
        .pcDebugName = "material buffer"
    };
    
    const plBufferDesc tLightBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_UNIFORM,
        .szByteSize = sizeof(plGPULight) * PL_MAX_LIGHTS,
        .pcDebugName = "light buffer"
    };

    const plBufferDesc tTransformBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_STORAGE,
        .szByteSize = sizeof(plMat4) * 10000,
        .pcDebugName = "transform buffer"
    };

    const plBufferDesc tInstanceBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_STORAGE,
        .szByteSize = sizeof(uint32_t) * 10000 * 2,
        .pcDebugName = "instance buffer"
    };

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        ptScene->atLightBuffer[i]        = pl__renderer_create_staging_buffer(&tLightBufferDesc, "light", i);
        ptScene->atTransformBuffer[i]    = pl__renderer_create_staging_buffer(&tTransformBufferDesc, "transform", i);
        ptScene->atInstanceBuffer[i]     = pl__renderer_create_staging_buffer(&tInstanceBufferDesc, "instance", i);
        ptScene->atMaterialDataBuffer[i] = pl__renderer_create_local_buffer(&tMaterialDataBufferDesc,  "material buffer", 0, ptScene->sbtMaterialBuffer, pl_sb_size(ptScene->sbtMaterialBuffer) * sizeof(plGPUMaterial));
    }

    int iSceneWideRenderingFlags = PL_RENDERING_FLAG_SHADOWS;
    if(gptData->tRuntimeOptions.bPunctualLighting)
        iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_PUNCTUAL;
    if(gptData->tRuntimeOptions.bImageBasedLighting)
        iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_IBL;

    // create lighting shader
    int aiLightingConstantData[] = {iSceneWideRenderingFlags, pl_sb_capacity(ptScene->sbtLightData), pl_sb_size(ptScene->sbtProbeData)};
    ptScene->tLightingShader = gptShaderVariant->get_shader("deferred_lighting", NULL, aiLightingConstantData, &gptData->tRenderPassLayout);
    aiLightingConstantData[0] = gptData->tRuntimeOptions.bPunctualLighting ? (PL_RENDERING_FLAG_USE_PUNCTUAL | PL_RENDERING_FLAG_SHADOWS) : 0;
    ptScene->tEnvLightingShader = gptShaderVariant->get_shader("deferred_lighting", NULL, aiLightingConstantData, &gptData->tRenderPassLayout);


    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        const plBindGroupUpdateBufferData atGlobalBufferData[] = 
        {
            {
                .tBuffer       = ptScene->atTransformBuffer[i],
                .uSlot         = 1,
                .szBufferRange = sizeof(plMat4) * 10000
            },
            {
                .tBuffer       = ptScene->atMaterialDataBuffer[i],
                .uSlot         = 2,
                .szBufferRange = sizeof(plGPUMaterial) * ptScene->uGPUMaterialBufferCapacity
            },
        };

        plBindGroupUpdateData tGlobalBindGroupData = {
            .uBufferCount = 2,
            .atBufferBindings = atGlobalBufferData,
        };

        gptGfx->update_bind_group(gptData->ptDevice, ptScene->atGlobalBindGroup[i], &tGlobalBindGroupData);
    }
    
}

void
pl_renderer_update_hovered_entity(plView* ptView, plVec2 tOffset, plVec2 tWindowScale)
{
    ptView->bRequestHoverCheck = true;
    ptView->tHoverOffset = tOffset;
    ptView->tHoverWindowRatio = tWindowScale;
}

bool
pl_renderer_get_hovered_entity(plView* ptView, plEntity* ptEntityOut)
{
    if(ptEntityOut)
        *ptEntityOut = ptView->tHoveredEntity;
    bool bNewValue = ptView->auHoverResultReady[gptGfx->get_current_frame_index()];
    ptView->auHoverResultReady[gptGfx->get_current_frame_index()] = false;
    return bNewValue;
}

void
pl_renderer_prepare_scene(plScene* ptScene)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    // for convience
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    plDevice*      ptDevice  = gptData->ptDevice;

    //~~~~~~~~~~~~~~~~~~~~~~transform & instance buffer update~~~~~~~~~~~~~~~~~~~~~

    // update transform & instance buffers since we are now using indices
    plBuffer* ptTransformBuffer = gptGfx->get_buffer(ptDevice, ptScene->atTransformBuffer[uFrameIdx]);
    plBuffer* ptInstanceBuffer = gptGfx->get_buffer(ptDevice, ptScene->atInstanceBuffer[uFrameIdx]);

    uint32_t uInstanceOffset = 0;
    const uint32_t uObjectCount = pl_sb_size(ptScene->sbtDrawables);
    const plEcsTypeKey tTransformComponentType = gptECS->get_ecs_type_key_transform();
    for(uint32_t i = 0; i < uObjectCount; i++)
    {

        plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtDrawables[i].tEntity);

        // copy transform into proper location in CPU side buffer
        plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
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

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~perform skinning~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const plBeginCommandInfo tSkinningBeginInfo = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auWaitSemaphoreValues = {gptStarter->get_current_timeline_value()},
    };

    plCommandBuffer* ptSkinningCmdBuffer = gptGfx->request_command_buffer(ptCmdPool);
    gptGfx->begin_command_recording(ptSkinningCmdBuffer, &tSkinningBeginInfo);

    const uint32_t uSkinCount = pl_sb_size(ptScene->sbtSkinData);
    for(uint32_t i = 0; i < uSkinCount; i++)
    {
        plBuffer* ptStagingBuffer = gptGfx->get_buffer(ptDevice, ptScene->sbtSkinData[i].atDynamicSkinBuffer[uFrameIdx]);
        plSkinComponent* ptSkinComponent = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tSkinComponentType, ptScene->sbtSkinData[i].tEntity);
        memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, ptSkinComponent->sbtTextureData, sizeof(plMat4) * pl_sb_size(ptSkinComponent->sbtTextureData));
    }

    pl__renderer_perform_skinning(ptSkinningCmdBuffer, ptScene);
    gptGfx->end_command_recording(ptSkinningCmdBuffer);

    const plSubmitInfo tSkinningSubmitInfo = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
    };
    gptGfx->submit_command_buffer(ptSkinningCmdBuffer, &tSkinningSubmitInfo);
    gptGfx->return_command_buffer(ptSkinningCmdBuffer);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~add scene lights~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    pl_sb_reset(ptScene->sbtLightData);
    ptScene->uShadowOffset = 0;
    ptScene->uShadowIndex = 0;
    ptScene->uDShadowIndex = 0;
    ptScene->uDShadowOffset = 0;

    // update CPU side light buffers
    plLightComponent* ptLights = NULL;
    const uint32_t uLightCount = gptECS->get_components(ptScene->ptComponentLibrary, gptData->tLightComponentType, (void**)&ptLights, NULL);
    for(uint32_t i = 0; i < uLightCount; i++)
    {
        const plLightComponent* ptLight = &ptLights[i];

        if(ptLight->tType == PL_LIGHT_TYPE_POINT)
        {  

            if(ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW)
                ptScene->uShadowIndex++;

            const plGPULight tLight = {
                .fIntensity   = ptLight->fIntensity,
                .fRange       = ptLight->fRange,
                .tPosition    = ptLight->tPosition,
                .tDirection   = ptLight->tDirection,
                .tColor       = ptLight->tColor,
                .iShadowIndex = ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW ? ptScene->uShadowIndex - 1 : 0,
                .iCastShadow  = ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW,
                .iType        = ptLight->tType
            };
            pl_sb_push(ptScene->sbtLightData, tLight);
        }
        else if(ptLight->tType == PL_LIGHT_TYPE_SPOT)
        {  

            if(ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW)
                ptScene->uShadowIndex++;

            const plGPULight tLight = {
                .fIntensity    = ptLight->fIntensity,
                .fRange        = ptLight->fRange,
                .tPosition     = ptLight->tPosition,
                .tDirection    = ptLight->tDirection,
                .tColor        = ptLight->tColor,
                .iShadowIndex  = ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW ? ptScene->uShadowIndex - 1 : 0,
                .iCastShadow   = ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW,
                .iType         = ptLight->tType,
                .fInnerConeCos = cosf(ptLight->fInnerConeAngle),
                .fOuterConeCos = cosf(ptLight->fOuterConeAngle),
            };
            pl_sb_push(ptScene->sbtLightData, tLight);
        }
        else if(ptLight->tType == PL_LIGHT_TYPE_DIRECTIONAL)
        {   

            if(ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW)
                ptScene->uDShadowIndex++;

            const plGPULight tLight = {
                .fIntensity    = ptLight->fIntensity,
                .fRange        = ptLight->fRange,
                .tPosition     = ptLight->tPosition,
                .tDirection    = ptLight->tDirection,
                .tColor        = ptLight->tColor,
                .iShadowIndex  = ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW ? ptScene->uDShadowIndex - 1 : 0,
                .iCastShadow   = ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW,
                .iCascadeCount = (int)ptLight->uCascadeCount,
                .iType         = ptLight->tType
            };
            pl_sb_push(ptScene->sbtLightData, tLight);
        }
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~generate shadow maps~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // prep
    pl__renderer_pack_shadow_atlas(ptScene);
    pl_sb_reset(ptScene->sbtLightShadowData);

    const plBeginCommandInfo tShadowBeginInfo = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auWaitSemaphoreValues = {gptStarter->get_current_timeline_value()},
    };

    plCommandBuffer* ptShadowCmdBuffer = gptGfx->request_command_buffer(ptCmdPool);
    gptGfx->begin_command_recording(ptShadowCmdBuffer, &tShadowBeginInfo);

    plRenderEncoder* ptShadowEncoder = gptGfx->begin_render_pass(ptShadowCmdBuffer, ptScene->tFirstShadowRenderPass, NULL);

    pl__renderer_generate_shadow_maps(ptShadowEncoder, ptShadowCmdBuffer, ptScene);

    gptGfx->end_render_pass(ptShadowEncoder);
    gptGfx->end_command_recording(ptShadowCmdBuffer);

    const plSubmitInfo tShadowSubmitInfo = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
    };
    gptGfx->submit_command_buffer(ptShadowCmdBuffer, &tShadowSubmitInfo);
    gptGfx->return_command_buffer(ptShadowCmdBuffer);

    plBuffer* ptShadowDataBuffer = gptGfx->get_buffer(ptDevice, ptScene->atLightShadowDataBuffer[uFrameIdx]);
    memcpy(ptShadowDataBuffer->tMemoryAllocation.pHostMapped, ptScene->sbtLightShadowData, sizeof(plGPULightShadowData) * pl_sb_size(ptScene->sbtLightShadowData));
    
    const uint32_t uProbeCount = pl_sb_size(ptScene->sbtProbeData);
    for(uint32_t uProbeIndex = 0; uProbeIndex < uProbeCount; uProbeIndex++)
    {
        plEnvironmentProbeData* ptProbe = &ptScene->sbtProbeData[uProbeIndex];
        plEnvironmentProbeComponent* ptProbeComp = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, ptProbe->tEntity);
        
        if(!((ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_REALTIME) || (ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_DIRTY)))
        {
            continue;
        }

        plTransformComponent* ptProbeTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptProbe->tEntity);

        // reset probe data
        pl_sb_reset(ptProbe->tDirectionLightShadowData.sbtDLightShadowData);
        ptProbe->tDirectionLightShadowData.uOffset = 0;
        ptProbe->tDirectionLightShadowData.uOffsetIndex = 0;

        plCamera atEnvironmentCamera[6] = {0};

        const plVec3 atPitchYawRoll[6] = {
            { 0.0f,    PL_PI_2 },
            { 0.0f,    -PL_PI_2 },
            { PL_PI_2,    PL_PI },
            { -PL_PI_2,    PL_PI },
            { PL_PI,    0.0f, PL_PI },
            { 0.0f,    0.0f },
        };

        for(uint32_t uFace = 0; uFace < 6; uFace++)
        {

            atEnvironmentCamera[uFace] = (plCamera){
                .tType        = PL_CAMERA_TYPE_PERSPECTIVE_REVERSE_Z,
                .tPos         = ptProbeTransform->tTranslation,
                .fNearZ       = 0.26f,
                .fFarZ        = ptProbeComp->fRange,
                .fFieldOfView = PL_PI_2,
                .fAspectRatio = 1.0f,
                .fRoll        = atPitchYawRoll[uFace].z
            };
            gptCamera->set_pitch_yaw(&atEnvironmentCamera[uFace], atPitchYawRoll[uFace].x, atPitchYawRoll[uFace].y);
            gptCamera->update(&atEnvironmentCamera[uFace]);

            const plBeginCommandInfo tBeginCSMInfo = {
                .uWaitSemaphoreCount   = 1,
                .atWaitSempahores      = {gptStarter->get_current_timeline_semaphore()},
                .auWaitSemaphoreValues = {gptStarter->get_current_timeline_value()},
            };

            plCommandBuffer* ptCSMCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
            gptGfx->begin_command_recording(ptCSMCommandBuffer, &tBeginCSMInfo);

            plRenderEncoder* ptCSMEncoder = gptGfx->begin_render_pass(ptCSMCommandBuffer, ptScene->tShadowRenderPass, NULL);

            pl__renderer_generate_cascaded_shadow_map(ptCSMEncoder, ptCSMCommandBuffer, ptScene, uFace, uProbeIndex, 1, &ptProbe->tDirectionLightShadowData,  &atEnvironmentCamera[uFace]);

            gptGfx->end_render_pass(ptCSMEncoder);
            gptGfx->end_command_recording(ptCSMCommandBuffer);

            const plSubmitInfo tSubmitCSMInfo = {
                .uSignalSemaphoreCount   = 1,
                .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
                .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
            };
            gptGfx->submit_command_buffer(ptCSMCommandBuffer, &tSubmitCSMInfo);
            gptGfx->return_command_buffer(ptCSMCommandBuffer);

            const BindGroup_0 tProbeBindGroupBuffer = {
                .tViewportSize         = {.x = ptProbe->tTargetSize.x, .y = ptProbe->tTargetSize.y, .z = 1.0f, .w = 1.0f},
                .tViewportInfo         = {0},
                .tCameraPos            = atEnvironmentCamera[uFace].tPos,
                .tCameraProjection     = atEnvironmentCamera[uFace].tProjMat,
                .tCameraView           = atEnvironmentCamera[uFace].tViewMat,
                .tCameraViewProjection = pl_mul_mat4(&atEnvironmentCamera[uFace].tProjMat, &atEnvironmentCamera[uFace].tViewMat)
            };

            // copy global buffer data for probe rendering
            const uint32_t uProbeGlobalBufferOffset = sizeof(BindGroup_0) * uFace;
            plBuffer* ptProbeGlobalBuffer = gptGfx->get_buffer(ptDevice, ptProbe->atGlobalBuffers[uFrameIdx]);
            memcpy(&ptProbeGlobalBuffer->tMemoryAllocation.pHostMapped[uProbeGlobalBufferOffset], &tProbeBindGroupBuffer, sizeof(BindGroup_0));
        }

        // copy probe shadow data to GPU buffer
        plBuffer* ptDShadowDataBuffer = gptGfx->get_buffer(ptDevice, ptProbe->tDirectionLightShadowData.atDLightShadowDataBuffer[uFrameIdx]);
        memcpy(ptDShadowDataBuffer->tMemoryAllocation.pHostMapped, ptProbe->tDirectionLightShadowData.sbtDLightShadowData, sizeof(plGPULightShadowData) * pl_sb_size(ptProbe->tDirectionLightShadowData.sbtDLightShadowData));

    }

    // update light GPU side buffers
    plBuffer* ptLightingBuffer = gptGfx->get_buffer(ptDevice, ptScene->atLightBuffer[uFrameIdx]);
    memcpy(ptLightingBuffer->tMemoryAllocation.pHostMapped, ptScene->sbtLightData, sizeof(plGPULight) * pl_sb_size(ptScene->sbtLightData));

    pl_sb_reset(ptScene->sbtGPUProbeData);
    for(uint32_t i = 0; i < pl_sb_size(ptScene->sbtProbeData); i++)
    {
        plEnvironmentProbeComponent* ptProbe = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, ptScene->sbtProbeData[i].tEntity);
        plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtProbeData[i].tEntity);
        plTransformComponent* ptProbeTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptScene->sbtProbeData[i].tEntity);
        plGPUProbeData tProbeData = {
            .tPosition              = ptProbeTransform->tTranslation,
            .fRangeSqr              = ptProbe->fRange * ptProbe->fRange,
            .uGGXEnvSampler         = ptScene->sbtProbeData[i].uGGXEnvSampler,
            .uLambertianEnvSampler  = ptScene->sbtProbeData[i].uLambertianEnvSampler,
            .uGGXLUT                = ptScene->sbtProbeData[i].uGGXLUT,
            .tMin.xyz               = ptObject->tAABB.tMin,
            .tMax.xyz               = ptObject->tAABB.tMax,
            .iParallaxCorrection    = (int)(ptProbe->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_PARALLAX_CORRECTION_BOX)
        };
        pl_sb_push(ptScene->sbtGPUProbeData, tProbeData);
    }

    if(pl_sb_size(ptScene->sbtGPUProbeData) > 0)
    {

        plBuffer* ptProbeDataBuffer = gptGfx->get_buffer(ptDevice, ptScene->atGPUProbeDataBuffers[uFrameIdx]);
        memcpy(ptProbeDataBuffer->tMemoryAllocation.pHostMapped, ptScene->sbtGPUProbeData, sizeof(plGPUProbeData) * pl_sb_size(ptScene->sbtGPUProbeData));
    }

    // update environment probes
    pl__renderer_update_probes(ptScene);

    pl_end_cpu_sample(gptProfile, 0);
}

void
pl_renderer_prepare_view(plView* ptView, plCamera* ptCamera)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    // for convience
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    plDevice*      ptDevice  = gptData->ptDevice;
    plDrawStream*  ptStream  = &gptData->tDrawStream;

    plScene* ptScene = ptView->ptParentScene;
    ptScene->uDShadowIndex = 0;
    ptScene->uDShadowOffset = 0;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~generate CSMs~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const plBeginCommandInfo tCSMBeginInfo = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auWaitSemaphoreValues = {gptStarter->get_current_timeline_value()},
    };

    plCommandBuffer* ptCSMCmdBuffer = gptGfx->request_command_buffer(ptCmdPool);
    gptGfx->begin_command_recording(ptCSMCmdBuffer, &tCSMBeginInfo);

    pl_sb_reset(ptView->tDirectionLightShadowData.sbtDLightShadowData);
    ptView->tDirectionLightShadowData.uOffset = 0;
    ptView->tDirectionLightShadowData.uOffsetIndex = 0;

    plRenderEncoder* ptCSMEncoder = gptGfx->begin_render_pass(ptCSMCmdBuffer, ptScene->tShadowRenderPass, NULL);
    
    uint32_t uViewIndex = 0;
    pl__renderer_generate_cascaded_shadow_map(ptCSMEncoder, ptCSMCmdBuffer, ptScene, uViewIndex, 0, 0, &ptView->tDirectionLightShadowData,  ptCamera);

    gptGfx->end_render_pass(ptCSMEncoder);
    gptGfx->end_command_recording(ptCSMCmdBuffer);

    const plSubmitInfo tCSMSubmitInfo = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
    };
    gptGfx->submit_command_buffer(ptCSMCmdBuffer, &tCSMSubmitInfo);
    gptGfx->return_command_buffer(ptCSMCmdBuffer);

    plBuffer* ptDShadowDataBuffer = gptGfx->get_buffer(ptDevice, ptView->tDirectionLightShadowData.atDLightShadowDataBuffer[uFrameIdx]);
    memcpy(ptDShadowDataBuffer->tMemoryAllocation.pHostMapped, ptView->tDirectionLightShadowData.sbtDLightShadowData, sizeof(plGPULightShadowData) * pl_sb_size(ptView->tDirectionLightShadowData.sbtDLightShadowData));

    if(gptData->tRuntimeOptions.bShowProbes)
    {
        const uint32_t uProbeCount = pl_sb_size(ptScene->sbtProbeData);
        for(uint32_t uProbeIndex = 0; uProbeIndex < uProbeCount; uProbeIndex++)
        {
            plEnvironmentProbeData* ptProbe = &ptScene->sbtProbeData[uProbeIndex];
            plEnvironmentProbeComponent* ptProbeComp = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, ptProbe->tEntity);
            plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptProbe->tEntity);
            gptDraw->add_3d_aabb(ptView->pt3DDrawList, ptObject->tAABB.tMin, ptObject->tAABB.tMax, (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(0.0f, 1.0f, 0.0f), .fThickness = 0.02f});
        }
    }

    pl_end_cpu_sample(gptProfile, 0);
}

void
pl_renderer_render_view(plView* ptView, plCamera* ptCamera, plCamera* ptCullCamera)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    // for convience
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    plDevice*      ptDevice  = gptData->ptDevice;
    plDrawStream*  ptStream  = &gptData->tDrawStream;
    plScene*       ptScene   = ptView->ptParentScene;

    const plEcsTypeKey tTransformComponentType = gptECS->get_ecs_type_key_transform();

    // common
    const plBindGroupUpdateSamplerData tSkyboxSamplerData = {
        .tSampler = gptData->tSkyboxSampler,
        .uSlot    = 1
    };

    const plBindGroupDesc tSkyboxBG0Desc = {
        .ptPool      = gptData->aptTempGroupPools[uFrameIdx],
        .tLayout     = gptShaderVariant->get_graphics_bind_group_layout("skybox", 0),
        .pcDebugName = "skybox view specific bindgroup"
    };

    const plBindGroupDesc tDeferredBG1Desc = {
        .ptPool      = gptData->aptTempGroupPools[uFrameIdx],
        .tLayout     = gptShaderVariant->get_graphics_bind_group_layout("gbuffer_fill", 1),
        .pcDebugName = "view specific bindgroup"
    };

    const plBindGroupDesc tSceneBGDesc = {
        .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
        .tLayout     = gptData->tSceneBGLayout,
        .pcDebugName = "light bind group 2"
    };

    const plBindGroupUpdateSamplerData tShadowSamplerData = {
        .tSampler = gptData->tShadowSampler,
        .uSlot    = 5
    };

    const plBindGroupDesc tPickBGDesc = {
        .ptPool      = gptData->aptTempGroupPools[uFrameIdx],
        .tLayout     = gptShaderVariant->get_graphics_bind_group_layout("picking", 0),
        .pcDebugName = "temp pick bind group"
    };

    const plBindGroupDesc tJFABindGroupDesc = {
        .ptPool      = gptData->aptTempGroupPools[uFrameIdx],
        .tLayout     = gptShaderVariant->get_compute_bind_group_layout("jumpfloodalgo", 0),
        .pcDebugName = "temp jfa bind group"
    };

    const plMat4 tMVP = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);

    pl_begin_cpu_sample(gptProfile, 0, "Scene Prep");

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~culling~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    const uint32_t uDrawableCount = pl_sb_size(ptScene->sbtDrawables);

    plAtomicCounter* ptCullCounter = NULL;
    
    plCullData tCullData = {
        .ptScene      = ptScene,
        .ptCullCamera = ptCullCamera,
        .atDrawables  = ptScene->sbtDrawables
    };

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
        if(pl_sb_size(ptView->sbtVisibleDrawables) != uDrawableCount)
        {
            pl_sb_resize(ptView->sbtVisibleDrawables, uDrawableCount);
            for(uint32_t i = 0; i < uDrawableCount; i++)
                ptView->sbtVisibleDrawables[i] = i;
        }
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~update bind groups~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const BindGroup_0 tBindGroupBuffer = {
        .tViewportSize         = {.xy = ptView->tTargetSize, .ignored0_ = 1.0f, .ignored1_ = 1.0f},
        .tViewportInfo         = {0},
        .tCameraPos            = ptCamera->tPos,
        .tCameraProjection     = ptCamera->tProjMat,
        .tCameraView           = ptCamera->tViewMat,
        .tCameraViewProjection = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat)
    };
    memcpy(gptGfx->get_buffer(ptDevice, ptView->atGlobalBuffers[uFrameIdx])->tMemoryAllocation.pHostMapped, &tBindGroupBuffer, sizeof(BindGroup_0));

    const plBindGroupUpdateBufferData tDeferredBG1BufferData = {
        .tBuffer       = ptView->atGlobalBuffers[uFrameIdx],
        .uSlot         = 0,
        .szBufferRange = sizeof(BindGroup_0)
    };

    plBindGroupUpdateData tDeferredBG1Data = {
        .uBufferCount    = 1,
        .atBufferBindings = &tDeferredBG1BufferData
    };
    
    plBindGroupHandle tDeferredBG1 = gptGfx->create_bind_group(ptDevice, &tDeferredBG1Desc);
    gptGfx->update_bind_group(gptData->ptDevice, tDeferredBG1, &tDeferredBG1Data);
    gptGfx->queue_bind_group_for_deletion(ptDevice, tDeferredBG1);
    
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~render scene~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    gptJob->wait_for_counter(ptCullCounter);
    pl_end_cpu_sample(gptProfile, 0); // prep scene
    if(ptCullCamera)
    {
        pl_sb_reset(ptView->sbuVisibleDeferredEntities);
        pl_sb_reset(ptView->sbuVisibleForwardEntities);
        pl_sb_reset(ptView->sbtVisibleDrawables);

        for(uint32_t uDrawableIndex = 0; uDrawableIndex < uDrawableCount; uDrawableIndex++)
        {
            const plDrawable tDrawable = ptScene->sbtDrawables[uDrawableIndex];
            if(!tDrawable.bCulled)
            {
                if(tDrawable.tFlags & PL_DRAWABLE_FLAG_DEFERRED)
                {
                    pl_sb_push(ptView->sbuVisibleDeferredEntities, uDrawableIndex);
                    pl_sb_push(ptView->sbtVisibleDrawables, uDrawableIndex);
                }
                else if(tDrawable.tFlags & PL_DRAWABLE_FLAG_PROBE)
                {
                    if(gptData->tRuntimeOptions.bShowProbes)
                    {
                        pl_sb_push(ptView->sbuVisibleForwardEntities, uDrawableIndex);
                        pl_sb_push(ptView->sbtVisibleDrawables, uDrawableIndex);
                    }
                }
                else if(tDrawable.tFlags & PL_DRAWABLE_FLAG_FORWARD)
                {
                    pl_sb_push(ptView->sbuVisibleForwardEntities, uDrawableIndex);
                    pl_sb_push(ptView->sbtVisibleDrawables, uDrawableIndex);
                }
                
            }
        }
    }

    *gptData->pdDrawCalls += (double)(pl_sb_size(ptView->sbuVisibleDeferredEntities) + pl_sb_size(ptView->sbuVisibleForwardEntities) + 1);

    const plVec2 tDimensions = gptGfx->get_render_pass(ptDevice, ptView->tRenderPass)->tDesc.tDimensions;

    plDrawArea tArea = {
        .ptDrawStream = ptStream,
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
    
    const plBeginCommandInfo tSceneBeginInfo = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auWaitSemaphoreValues = {gptStarter->get_current_timeline_value()},
    };

    plCommandBuffer* ptSceneCmdBuffer = gptGfx->request_command_buffer(ptCmdPool);
    gptGfx->begin_command_recording(ptSceneCmdBuffer, &tSceneBeginInfo);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~subpass 0 - g buffer fill~~~~~~~~~~~~~~~~~~~~~~~~~~

    plRenderEncoder* ptSceneEncoder = gptGfx->begin_render_pass(ptSceneCmdBuffer, ptView->tRenderPass, NULL);
    gptGfx->set_depth_bias(ptSceneEncoder, 0.0f, 0.0f, 0.0f);

    const uint32_t uVisibleDeferredDrawCount = pl_sb_size(ptView->sbuVisibleDeferredEntities);
    gptGfx->reset_draw_stream(ptStream, uVisibleDeferredDrawCount);
    for(uint32_t i = 0; i < uVisibleDeferredDrawCount; i++)
    {
        const plDrawable tDrawable = ptScene->sbtDrawables[ptView->sbuVisibleDeferredEntities[i]];

        if(tDrawable.uInstanceCount != 0)
        {
            plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
            plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
            
            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

            DynamicData* ptDynamicData = (DynamicData*)tDynamicBinding.pcData;
            ptDynamicData->iDataOffset = tDrawable.uDataOffset;
            ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
            ptDynamicData->iMaterialOffset = tDrawable.uMaterialIndex;
            ptDynamicData->uGlobalIndex = 0;

            pl_add_to_draw_stream(ptStream, (plDrawStreamData)
            {
                .tShader        = tDrawable.tShader,
                .auDynamicBuffers = {
                    tDynamicBinding.uBufferHandle
                },
                .atVertexBuffers = {
                    ptScene->tVertexBuffer,
                },
                .tIndexBuffer         = tDrawable.tIndexBuffer,
                .uIndexOffset         = tDrawable.uIndexOffset,
                .uTriangleCount       = tDrawable.uTriangleCount,
                .uVertexOffset        = tDrawable.uStaticVertexOffset,
                .atBindGroups = {
                    ptScene->atGlobalBindGroup[uFrameIdx],
                    tDeferredBG1
                },
                .auDynamicBufferOffsets = {
                    tDynamicBinding.uByteOffset
                },
                .uInstanceOffset = tDrawable.uTransformIndex,
                .uInstanceCount  = tDrawable.uInstanceCount
            });
        }
    }

    gptGfx->draw_stream(ptSceneEncoder, 1, &tArea);
    
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~subpass 1 - lighting~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    gptGfx->next_subpass(ptSceneEncoder, NULL);

    const plBindGroupUpdateBufferData atSceneBGBufferData[] = 
    {
        { .uSlot = 0, .tBuffer = ptView->atGlobalBuffers[uFrameIdx], .szBufferRange = sizeof(BindGroup_0) },
        { .uSlot = 1, .tBuffer = ptScene->atLightBuffer[uFrameIdx], .szBufferRange = sizeof(plGPULight) * pl_sb_size(ptScene->sbtLightData)},
        { .uSlot = 2, .tBuffer = ptView->tDirectionLightShadowData.atDLightShadowDataBuffer[uFrameIdx], .szBufferRange = sizeof(plGPULightShadowData) * pl_sb_size(ptView->tDirectionLightShadowData.sbtDLightShadowData)},
        { .uSlot = 3, .tBuffer = ptScene->atLightShadowDataBuffer[uFrameIdx], .szBufferRange = sizeof(plGPULightShadowData) * pl_sb_size(ptScene->sbtLightShadowData)},
        { .uSlot = 4, .tBuffer = ptScene->atGPUProbeDataBuffers[uFrameIdx], .szBufferRange = sizeof(plGPUProbeData) * pl_sb_size(ptScene->sbtGPUProbeData)},
    };

    const plBindGroupUpdateData tSceneBGData = {
        .uBufferCount      = 5,
        .atBufferBindings  = atSceneBGBufferData,
        .uSamplerCount     = 1,
        .atSamplerBindings = &tShadowSamplerData
    };
    plBindGroupHandle tSceneBG = gptGfx->create_bind_group(ptDevice, &tSceneBGDesc);
    gptGfx->update_bind_group(gptData->ptDevice, tSceneBG, &tSceneBGData);
    gptGfx->queue_bind_group_for_deletion(ptDevice, tSceneBG);

    plDynamicBinding tLightingDynamicData = pl__allocate_dynamic_data(ptDevice);
    plLightingDynamicData* ptLightingDynamicData = (plLightingDynamicData*)tLightingDynamicData.pcData;
    ptLightingDynamicData->uGlobalIndex = 0;

    gptGfx->reset_draw_stream(ptStream, 1);
    pl_add_to_draw_stream(ptStream, (plDrawStreamData)
    {
        .tShader = ptScene->tLightingShader,
        .auDynamicBuffers = {
            tLightingDynamicData.uBufferHandle
        },
        .atVertexBuffers = {
            gptData->tFullQuadVertexBuffer
        },
        .tIndexBuffer   = gptData->tFullQuadIndexBuffer,
        .uIndexOffset   = 0,
        .uTriangleCount = 2,
        .atBindGroups = {
            ptScene->atGlobalBindGroup[uFrameIdx],
            ptView->tLightingBindGroup,
            tSceneBG
        },
        .auDynamicBufferOffsets = {
            tLightingDynamicData.uByteOffset
        },
        .uInstanceOffset = 0,
        .uInstanceCount  = 1
    });
    gptGfx->draw_stream(ptSceneEncoder, 1, &tArea);
    
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~subpass 2 - forward~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    gptGfx->next_subpass(ptSceneEncoder, NULL);

    if(ptScene->tSkyboxTexture.uIndex != 0 && ptView->bShowSkybox)
    {
        ptView->bShowSkybox = false;

        const plBindGroupUpdateBufferData tSkyboxBG0BufferData = {
            .tBuffer       = ptView->atGlobalBuffers[uFrameIdx],
            .uSlot         = 0,
            .szBufferRange = sizeof(BindGroup_0)
        };

        plBindGroupUpdateData tSkyboxBG0Data = {
            .uBufferCount = 1,
            .atBufferBindings = &tSkyboxBG0BufferData,
            .uSamplerCount = 1,
            .atSamplerBindings = &tSkyboxSamplerData,
        };

        plBindGroupHandle tSkyboxBG0 = gptGfx->create_bind_group(ptDevice, &tSkyboxBG0Desc);
        gptGfx->update_bind_group(gptData->ptDevice, tSkyboxBG0, &tSkyboxBG0Data);
        gptGfx->queue_bind_group_for_deletion(ptDevice, tSkyboxBG0);

        plDynamicBinding tSkyboxDynamicData = pl__allocate_dynamic_data(ptDevice);
        plSkyboxDynamicData* ptSkyboxDynamicData = (plSkyboxDynamicData*)tSkyboxDynamicData.pcData;
        ptSkyboxDynamicData->tModel = pl_mat4_translate_vec3(ptCamera->tPos);
        ptSkyboxDynamicData->uGlobalIndex = 0;

        gptGfx->reset_draw_stream(ptStream, 1);
        pl_add_to_draw_stream(ptStream, (plDrawStreamData)
        {
            .tShader        = gptShaderVariant->get_shader("skybox", NULL, NULL, &gptData->tRenderPassLayout),
            .auDynamicBuffers = {
                tSkyboxDynamicData.uBufferHandle
            },
            .atVertexBuffers = {
                ptScene->tVertexBuffer,
            },
            .tIndexBuffer   = ptScene->tIndexBuffer,
            .uIndexOffset   = ptScene->tSkyboxDrawable.uIndexOffset,
            .uTriangleCount = ptScene->tSkyboxDrawable.uIndexCount / 3,
            .atBindGroups = {
                tSkyboxBG0,
                ptScene->tSkyboxBindGroup
            },
            .auDynamicBufferOffsets = {
                tSkyboxDynamicData.uByteOffset
            },
            .uInstanceOffset = 0,
            .uInstanceCount  = 1
        });
        gptGfx->draw_stream(ptSceneEncoder, 1, &tArea);
    }
    
    const uint32_t uVisibleForwardDrawCount = pl_sb_size(ptView->sbuVisibleForwardEntities);
    gptGfx->reset_draw_stream(ptStream, uVisibleForwardDrawCount);
    for(uint32_t i = 0; i < uVisibleForwardDrawCount; i++)
    {
        const plDrawable tDrawable = ptScene->sbtDrawables[ptView->sbuVisibleForwardEntities[i]];

        if(tDrawable.uInstanceCount != 0)
        {
            plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
            plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
            
            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

            DynamicData* ptDynamicData = (DynamicData*)tDynamicBinding.pcData;
            ptDynamicData->iDataOffset = tDrawable.uDataOffset;
            ptDynamicData->iVertexOffset = tDrawable.uDynamicVertexOffset;
            ptDynamicData->iMaterialOffset = tDrawable.uMaterialIndex;
            ptDynamicData->uGlobalIndex = 0;

            pl_add_to_draw_stream(ptStream, (plDrawStreamData)
            {
                .tShader        = tDrawable.tShader,
                .auDynamicBuffers = {
                    tDynamicBinding.uBufferHandle
                },
                .atVertexBuffers = {
                    ptScene->tVertexBuffer,
                },
                .tIndexBuffer   = tDrawable.tIndexBuffer,
                .uIndexOffset   = tDrawable.uIndexOffset,
                .uTriangleCount = tDrawable.uTriangleCount,
                .uVertexOffset  = tDrawable.uStaticVertexOffset,
                .atBindGroups = {
                    ptScene->atGlobalBindGroup[uFrameIdx],
                    tSceneBG
                },
                .auDynamicBufferOffsets = {
                    tDynamicBinding.uByteOffset
                },
                .uInstanceOffset = tDrawable.uTransformIndex,
                .uInstanceCount = tDrawable.uInstanceCount
            });
        }
    }
    gptGfx->draw_stream(ptSceneEncoder, 1, &tArea);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~debug drawing~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // bounding boxes
    const uint32_t uOutlineDrawableCount = pl_sb_size(ptScene->sbtOutlinedEntities);
    if(uOutlineDrawableCount > 0 && gptData->tRuntimeOptions.bShowSelectedBoundingBox)
    {
        const plVec4 tOutlineColor = (plVec4){0.0f, (float)sin(gptIOI->get_io()->dTime * 3.0) * 0.25f + 0.75f, 0.0f, 1.0f};
        for(uint32_t i = 0; i < uOutlineDrawableCount; i++)
        {
            plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtOutlinedEntities[i]);
            gptDraw->add_3d_aabb(ptView->pt3DSelectionDrawList, ptObject->tAABB.tMin, ptObject->tAABB.tMax, (plDrawLineOptions){.uColor = PL_COLOR_32_VEC4(tOutlineColor), .fThickness = 0.01f});
            
        }
    }

    if(gptData->tRuntimeOptions.bShowOrigin)
    {
        const plMat4 tTransform = pl_identity_mat4();
        gptDraw->add_3d_transform(ptView->pt3DDrawList, &tTransform, 10.0f, (plDrawLineOptions){.fThickness = 0.02f});
    }

    if(ptCullCamera && ptCullCamera != ptCamera)
    {
        const plDrawFrustumDesc tFrustumDesc = {
            .fAspectRatio = ptCullCamera->fAspectRatio,
            .fFarZ        = ptCullCamera->fFarZ,
            .fNearZ       = ptCullCamera->fNearZ,
            .fYFov        = ptCullCamera->fFieldOfView
        };
        gptDraw->add_3d_frustum(ptView->pt3DSelectionDrawList, &ptCullCamera->tTransformMat, tFrustumDesc, (plDrawLineOptions){.uColor = PL_COLOR_32_YELLOW, .fThickness = 0.02f});
    }

    gptDrawBackend->submit_3d_drawlist(ptView->pt3DDrawList, ptSceneEncoder, tDimensions.x, tDimensions.y, &tMVP, PL_DRAW_FLAG_REVERSE_Z_DEPTH | PL_DRAW_FLAG_DEPTH_TEST, 1);
    gptDrawBackend->submit_3d_drawlist(ptView->pt3DSelectionDrawList, ptSceneEncoder, tDimensions.x, tDimensions.y, &tMVP, 0, 1);
    gptGfx->end_render_pass(ptSceneEncoder);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~entity selection~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    if(ptView->bRequestHoverCheck)
    {
        pl_begin_cpu_sample(gptProfile, 0, "Picking Submission");

        plShaderHandle tPickShader = gptShaderVariant->get_shader("picking", NULL, NULL, &gptData->tPickRenderPassLayout);

        plBuffer* ptPickBuffer = gptGfx->get_buffer(ptDevice, ptView->atPickBuffer[uFrameIdx]);
        // memset(ptPickBuffer->tMemoryAllocation.pHostMapped, 0, sizeof(uint32_t) * 2);

        ptView->auHoverResultProcessing[uFrameIdx] = true;
        ptView->auHoverResultReady[uFrameIdx] = false;
        ptView->bRequestHoverCheck = false;

        const plBindGroupUpdateBufferData tPickBG0BufferData = {
            .tBuffer       = ptView->atGlobalBuffers[uFrameIdx],
            .uSlot         = 0,
            .szBufferRange = sizeof(BindGroup_0)
        };

        const plBindGroupUpdateData tPickBGData0 = {
            .uBufferCount     = 1,
            .atBufferBindings = &tPickBG0BufferData,
        };

        plBindGroupHandle tPickBG0 = gptGfx->create_bind_group(ptDevice, &tPickBGDesc);
        gptGfx->update_bind_group(gptData->ptDevice, tPickBG0, &tPickBGData0);
        gptGfx->queue_bind_group_for_deletion(ptDevice, tPickBG0);

        plRenderEncoder* ptPickEncoder = gptGfx->begin_render_pass(ptSceneCmdBuffer, ptView->tPickRenderPass, NULL);
        gptGfx->set_depth_bias(ptPickEncoder, 0.0f, 0.0f, 0.0f);
        gptGfx->bind_shader(ptPickEncoder, tPickShader);
        gptGfx->bind_vertex_buffer(ptPickEncoder, ptScene->tVertexBuffer);

        plBindGroupHandle atBindGroups[2] = {tPickBG0, ptView->atPickBindGroup[uFrameIdx]};
        gptGfx->bind_graphics_bind_groups(ptPickEncoder, tPickShader, 0, 2, atBindGroups, 0, NULL);

        const uint32_t uVisibleDrawCount = pl_sb_size(ptView->sbtVisibleDrawables);
        *gptData->pdDrawCalls += (double)uVisibleDrawCount;

        plVec2 tMousePos = gptIOI->get_mouse_pos();
        tMousePos = pl_sub_vec2(tMousePos, ptView->tHoverOffset);
        tMousePos = pl_div_vec2(tMousePos, ptView->tHoverWindowRatio);
        
        for(uint32_t i = 0; i < uVisibleDrawCount; i++)
        {
            const plDrawable tDrawable = ptScene->sbtDrawables[ptView->sbtVisibleDrawables[i]];

            uint32_t uId = tDrawable.tEntity.uIndex;
            
            plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, tDrawable.tEntity);
            plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptObject->tTransform);
            
            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);
            plPickDynamicData* ptDynamicData = (plPickDynamicData*)tDynamicBinding.pcData;
            
            ptDynamicData->uID = uId;
            ptDynamicData->tModel = ptTransform->tWorld;
            ptDynamicData->tMousePos.xy = tMousePos;
            
            gptGfx->bind_graphics_bind_groups(ptPickEncoder, tPickShader, 0, 0, NULL, 1, &tDynamicBinding);

            if(tDrawable.uIndexCount > 0)
            {
                plDrawIndex tDraw = {
                    .tIndexBuffer   = ptScene->tIndexBuffer,
                    .uIndexCount    = tDrawable.uIndexCount,
                    .uIndexStart    = tDrawable.uIndexOffset,
                    .uInstanceCount = 1
                };
                gptGfx->draw_indexed(ptPickEncoder, 1, &tDraw);
            }
            else
            {
                plDraw tDraw = {
                    .uVertexStart   = tDrawable.uVertexOffset,
                    .uInstanceCount = 1,
                    .uVertexCount   = tDrawable.uVertexCount
                };
                gptGfx->draw(ptPickEncoder, 1, &tDraw);
            }
        }
        gptGfx->end_render_pass(ptPickEncoder);

        pl_end_cpu_sample(gptProfile, 0);
    }

    gptGfx->end_command_recording(ptSceneCmdBuffer);

    const plSubmitInfo tSceneSubmitInfo = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
    };
    gptGfx->submit_command_buffer(ptSceneCmdBuffer, &tSceneSubmitInfo);
    gptGfx->return_command_buffer(ptSceneCmdBuffer);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~uv map pass for JFA~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const plBeginCommandInfo tUVBeginInfo = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auWaitSemaphoreValues = {gptStarter->get_current_timeline_value()},
    };

    plCommandBuffer* ptUVCmdBuffer = gptGfx->request_command_buffer(ptCmdPool);
    gptGfx->begin_command_recording(ptUVCmdBuffer, &tUVBeginInfo);

    plRenderEncoder* ptUVEncoder = gptGfx->begin_render_pass(ptUVCmdBuffer, ptView->tUVRenderPass, NULL);

    // submit nonindexed draw using basic API
    plShaderHandle tUVShader = gptShaderVariant->get_shader("uvmap", NULL, NULL, &gptData->tUVRenderPassLayout);
    gptGfx->bind_shader(ptUVEncoder, tUVShader);
    gptGfx->bind_vertex_buffer(ptUVEncoder, gptData->tFullQuadVertexBuffer);

    plDrawIndex tDraw = {
        .tIndexBuffer   = gptData->tFullQuadIndexBuffer,
        .uIndexCount    = 6,
        .uInstanceCount = 1,
    };
    *gptData->pdDrawCalls += 1.0;
    gptGfx->draw_indexed(ptUVEncoder, 1, &tDraw);

    // end render pass
    gptGfx->end_render_pass(ptUVEncoder);

    gptGfx->end_command_recording(ptUVCmdBuffer);

    const plSubmitInfo tUVSubmitInfo = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
    };
    gptGfx->submit_command_buffer(ptUVCmdBuffer, &tUVSubmitInfo);
    gptGfx->return_command_buffer(ptUVCmdBuffer);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~jump flood~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // find next power of 2
    uint32_t uJumpDistance = 1;
    uint32_t uHalfWidth = gptData->tRuntimeOptions.uOutlineWidth / 2;
    if (uHalfWidth && !(uHalfWidth & (uHalfWidth - 1))) 
        uJumpDistance = uHalfWidth;
    while(uJumpDistance < uHalfWidth)
        uJumpDistance <<= 1;

    // calculate number of jumps necessary
    uint32_t uJumpSteps = 0;
    while(uJumpDistance)
    {
        uJumpSteps++;
        uJumpDistance >>= 1;
    }

    float fJumpDistance = (float)uHalfWidth;
    if(uOutlineDrawableCount == 0)
        uJumpSteps = 1;

    const plDispatch tDispach = {
        .uGroupCountX     = (uint32_t)ceilf(tDimensions.x / 8.0f),
        .uGroupCountY     = (uint32_t)ceilf(tDimensions.y / 8.0f),
        .uGroupCountZ     = 1,
        .uThreadPerGroupX = 8,
        .uThreadPerGroupY = 8,
        .uThreadPerGroupZ = 1
    };

    plBindGroupHandle atJFABindGroups[] = {
        gptGfx->create_bind_group(gptData->ptDevice, &tJFABindGroupDesc),
        gptGfx->create_bind_group(gptData->ptDevice, &tJFABindGroupDesc)
    };

    const plBindGroupUpdateTextureData atJFATextureData0[] = 
    {
        {
            .tTexture = ptView->atUVMaskTexture0,
            .uSlot    = 0,
            .tType    = PL_TEXTURE_BINDING_TYPE_STORAGE,
                .tCurrentUsage = PL_TEXTURE_USAGE_STORAGE
        },
        {
            .tTexture = ptView->atUVMaskTexture1,
            .uSlot    = 1,
            .tType    = PL_TEXTURE_BINDING_TYPE_STORAGE,
            .tCurrentUsage = PL_TEXTURE_USAGE_STORAGE
        }
    };

    const plBindGroupUpdateTextureData atJFATextureData1[] = 
    {
        {
            .tTexture = ptView->atUVMaskTexture1,
            .uSlot    = 0,
            .tType    = PL_TEXTURE_BINDING_TYPE_STORAGE,
            .tCurrentUsage = PL_TEXTURE_USAGE_STORAGE
        },
        {
            .tTexture = ptView->atUVMaskTexture0,
            .uSlot    = 1,
            .tType    = PL_TEXTURE_BINDING_TYPE_STORAGE,
            .tCurrentUsage = PL_TEXTURE_USAGE_STORAGE
        }
    };

    const plBindGroupUpdateData tJFABGData0 = {
        .uTextureCount = 2,
        .atTextureBindings = atJFATextureData0
    };
    const plBindGroupUpdateData tJFABGData1 = {
        .uTextureCount = 2,
        .atTextureBindings = atJFATextureData1
    };
    gptGfx->update_bind_group(gptData->ptDevice, atJFABindGroups[0], &tJFABGData0);
    gptGfx->update_bind_group(gptData->ptDevice, atJFABindGroups[1], &tJFABGData1);
    gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, atJFABindGroups[0]);
    gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, atJFABindGroups[1]);

    plComputeShaderHandle tJFAShader = gptShaderVariant->get_compute_shader("jumpfloodalgo", NULL);
    for(uint32_t i = 0; i < uJumpSteps; i++)
    {
        const plBeginCommandInfo tJumpBeginInfo = {
            .uWaitSemaphoreCount   = 1,
            .atWaitSempahores      = {gptStarter->get_current_timeline_semaphore()},
            .auWaitSemaphoreValues = {gptStarter->get_current_timeline_value()},
        };

        plCommandBuffer* ptJumpCmdBuffer = gptGfx->request_command_buffer(ptCmdPool);
        gptGfx->begin_command_recording(ptJumpCmdBuffer, &tJumpBeginInfo);

        // begin main renderpass (directly to swapchain)
        plComputeEncoder* ptJumpEncoder = gptGfx->begin_compute_pass(ptJumpCmdBuffer, NULL);
        gptGfx->pipeline_barrier_compute(ptJumpEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_READ, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_WRITE);

        ptView->tLastUVMask = (i % 2 == 0) ? ptView->atUVMaskTexture1 : ptView->atUVMaskTexture0;

        // submit nonindexed draw using basic API
        gptGfx->bind_compute_shader(ptJumpEncoder, tJFAShader);

        plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);
        plVec4* ptJumpDistance = (plVec4*)tDynamicBinding.pcData;
        ptJumpDistance->x = tDimensions.x;
        ptJumpDistance->y = tDimensions.y;
        ptJumpDistance->z = fJumpDistance;

        gptGfx->bind_compute_bind_groups(ptJumpEncoder, tJFAShader, 0, 1, &atJFABindGroups[i % 2], 1, &tDynamicBinding);
        gptGfx->dispatch(ptJumpEncoder, 1, &tDispach);

        // end render pass
        gptGfx->pipeline_barrier_compute(ptJumpEncoder, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_FRAGMENT_SHADER, PL_ACCESS_SHADER_READ);
        gptGfx->end_compute_pass(ptJumpEncoder);

        // end recording
        gptGfx->end_command_recording(ptJumpCmdBuffer);

        const plSubmitInfo tJumpSubmitInfo = {
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
            .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()},
        };
        gptGfx->submit_command_buffer(ptJumpCmdBuffer, &tJumpSubmitInfo);
        gptGfx->return_command_buffer(ptJumpCmdBuffer);

        fJumpDistance = fJumpDistance / 2.0f;
        if(fJumpDistance < 1.0f)
            fJumpDistance = 1.0f;
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~post process~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const plBeginCommandInfo tPostBeginInfo = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auWaitSemaphoreValues = {gptStarter->get_current_timeline_value()},
    };

    plCommandBuffer* ptPostCmdBuffer = gptGfx->request_command_buffer(ptCmdPool);
    gptGfx->begin_command_recording(ptPostCmdBuffer, &tPostBeginInfo);

    pl__renderer_post_process_scene(ptPostCmdBuffer, ptView, &tMVP);
    gptGfx->end_command_recording(ptPostCmdBuffer);

    const plSubmitInfo tPostSubmitInfo = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
    };
    gptGfx->submit_command_buffer(ptPostCmdBuffer, &tPostSubmitInfo);
    gptGfx->return_command_buffer(ptPostCmdBuffer);

    // update stats
    static double* pdVisibleOpaqueObjects = NULL;
    static double* pdVisibleTransparentObjects = NULL;
    if(!pdVisibleOpaqueObjects)
    {
        pdVisibleOpaqueObjects = gptStats->get_counter("visible deferred objects");
        pdVisibleTransparentObjects = gptStats->get_counter("visible forward objects");
    }

    // only record stats for first scene
    if(ptScene == gptData->sbptScenes[0])
    {
        *pdVisibleOpaqueObjects = (double)(pl_sb_size(ptView->sbuVisibleDeferredEntities));
        *pdVisibleTransparentObjects = (double)(pl_sb_size(ptView->sbuVisibleForwardEntities));
    }

    pl_end_cpu_sample(gptProfile, 0);
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
                    .fRadius = tanf(sbtLights[i].fOuterConeAngle) * sbtLights[i].fRange,
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
    plScene* ptScene = ptView->ptParentScene;
    const uint32_t uDrawableCount = pl_sb_size(ptScene->sbtDrawables);
    for(uint32_t i = 0; i < uDrawableCount; i++)
    {
        plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtDrawables[i].tEntity);

        gptDraw->add_3d_aabb(ptView->pt3DDrawList, ptObject->tAABB.tMin, ptObject->tAABB.tMax, (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(1.0f, 0.0f, 0.0f), .fThickness = 0.02f});
    }
}

void
pl_renderer_debug_draw_bvh(plView* ptView)
{
    plScene* ptScene = ptView->ptParentScene;
    pl_begin_cpu_sample(gptProfile, 0, "draw BVH");

    plObjectComponent* ptComponents = NULL;
    gptECS->get_components(ptScene->ptComponentLibrary, gptData->tObjectComponentType, (void**)&ptComponents, NULL);

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
    pl_end_cpu_sample(gptProfile, 0);
}

bool
pl_renderer_begin_frame(void)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    plDevice* ptDevice = gptData->ptDevice;

    gptGfx->reset_bind_group_pool(gptData->aptTempGroupPools[gptGfx->get_current_frame_index()]);
    gptData->tCurrentDynamicDataBlock = gptGfx->allocate_dynamic_data_block(gptData->ptDevice);


    // perform GPU buffer updates
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    uint64_t ulValue = gptStarter->get_current_timeline_value();
    plTimelineSemaphore* tSemHandle = gptStarter->get_current_timeline_semaphore();

    for(uint32_t uSceneIndex = 0; uSceneIndex < pl_sb_size(gptData->sbptScenes); uSceneIndex++)
    {
        plScene* ptScene = gptData->sbptScenes[uSceneIndex];
        if(!ptScene->bActive)
            continue;

        if(ptScene->uGPUMaterialDirty)
        {

            plBufferHandle tStagingBuffer = gptData->atStagingBufferHandle[uFrameIdx].tStagingBufferHandle;

            if(!gptGfx->is_buffer_valid(ptDevice, tStagingBuffer))
            {
                const plBufferDesc tStagingBufferDesc = {
                    .tUsage      = PL_BUFFER_USAGE_STAGING,
                    .szByteSize  = 268435456,
                    .pcDebugName = "Renderer Staging Buffer"
                };
    
                gptData->atStagingBufferHandle[uFrameIdx].tStagingBufferHandle = pl__renderer_create_staging_buffer(&tStagingBufferDesc, "staging", uFrameIdx);
                tStagingBuffer = gptData->atStagingBufferHandle[uFrameIdx].tStagingBufferHandle;
                gptData->atStagingBufferHandle[uFrameIdx].szOffset = 0;
                gptData->atStagingBufferHandle[uFrameIdx].szSize = tStagingBufferDesc.szByteSize;
            }
            gptData->atStagingBufferHandle[uFrameIdx].dLastTimeActive = gptIO->dTime;

            const plBeginCommandInfo tSkinUpdateBeginInfo = {
                .uWaitSemaphoreCount   = 1,
                .atWaitSempahores      = {tSemHandle},
                .auWaitSemaphoreValues = {ulValue},
            };
        
            plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
            gptGfx->begin_command_recording(ptCommandBuffer, &tSkinUpdateBeginInfo);

            plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
            gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);
        
            plBuffer* ptStagingBuffer = gptGfx->get_buffer(ptDevice, tStagingBuffer);
            memcpy(&ptStagingBuffer->tMemoryAllocation.pHostMapped[gptData->atStagingBufferHandle[uFrameIdx].szOffset], ptScene->sbtMaterialBuffer, sizeof(plGPUMaterial) * pl_sb_size(ptScene->sbtMaterialBuffer));
            gptGfx->copy_buffer(ptBlitEncoder, tStagingBuffer, ptScene->atMaterialDataBuffer[uFrameIdx], (uint32_t)gptData->atStagingBufferHandle[uFrameIdx].szOffset, 0, sizeof(plGPUMaterial) * pl_sb_size(ptScene->sbtMaterialBuffer));
            gptData->atStagingBufferHandle[uFrameIdx].szOffset += sizeof(plGPUMaterial) * pl_sb_size(ptScene->sbtMaterialBuffer);

            gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
            gptGfx->end_blit_pass(ptBlitEncoder);
            gptGfx->end_command_recording(ptCommandBuffer);

            const plSubmitInfo tSkinUpdateSubmitInfo = {
                .uSignalSemaphoreCount   = 1,
                .atSignalSempahores      = {tSemHandle},
                .auSignalSemaphoreValues = {++ulValue}
            };
            gptGfx->submit_command_buffer(ptCommandBuffer, &tSkinUpdateSubmitInfo);
            
            gptGfx->return_command_buffer(ptCommandBuffer);
            ptScene->uGPUMaterialDirty--;
        }
        else if(gptGfx->is_buffer_valid(ptDevice, gptData->atStagingBufferHandle[uFrameIdx].tStagingBufferHandle))
        {
            if(gptIO->dTime - gptData->atStagingBufferHandle[uFrameIdx].dLastTimeActive > 30.0)
            {
                gptGfx->queue_buffer_for_deletion(ptDevice, gptData->atStagingBufferHandle[uFrameIdx].tStagingBufferHandle);
            }
        }

        for(uint32_t uViewIndex = 0; uViewIndex < pl_sb_size(ptScene->sbptViews); uViewIndex++)
        {
            plView* ptView = ptScene->sbptViews[uViewIndex];

            if(ptView->auHoverResultProcessing[uFrameIdx])
            {
                pl_begin_cpu_sample(gptProfile, 0, "Picking Retrieval");
                
                plBuffer* ptPickBuffer = gptGfx->get_buffer(ptDevice, ptView->atPickBuffer[uFrameIdx]);
                const uint32_t uNewID = *(uint32_t*)ptPickBuffer->tMemoryAllocation.pHostMapped;
                plEntity tNewEntity = gptECS->get_current_entity(ptScene->ptComponentLibrary, (plEntity){.uIndex = uNewID});
                ptView->tHoveredEntity = tNewEntity;

                ptView->auHoverResultProcessing[uFrameIdx] = false;
                ptView->auHoverResultReady[uFrameIdx] = true;
                memset(ptPickBuffer->tMemoryAllocation.pHostMapped, 0, sizeof(uint32_t) * 2);
        
                pl_end_cpu_sample(gptProfile, 0);
            }
        }
    }
    pl_end_cpu_sample(gptProfile, 0);
    return true;
}

plDrawList3D*
pl_renderer_get_debug_drawlist(plView* ptView)
{
    return ptView->pt3DDrawList;
}

plDrawList3D*
pl_renderer_get_gizmo_drawlist(plView* ptView)
{
    return ptView->pt3DGizmoDrawList;
}

plBindGroupHandle
pl_renderer_get_view_color_texture(plView* ptView)
{
    return ptView->tFinalTextureHandle;
}

void
pl_renderer_add_drawable_objects_to_scene(plScene* ptScene, uint32_t uObjectCount, const plEntity* atObjects)
{

    if(uObjectCount == 0)
        return;

    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    uint32_t uStart = pl_sb_size(ptScene->sbtStagedEntities);
    pl_sb_add_n(ptScene->sbtStagedEntities, uObjectCount);

    const plEcsTypeKey tMeshComponentType = gptMesh->get_ecs_type_key_mesh();

    for(uint32_t i = 0; i < uObjectCount; i++)
    {
        ptScene->sbtStagedEntities[uStart + i] = atObjects[i];
    }

    // sort to group entities for instances (slow bubble sort, improve later)
    bool bSwapped = false;
    for (uint32_t i = 0; i < uObjectCount - 1; i++)
    {
        bSwapped = false;
        for (uint32_t j = 0; j < uObjectCount - i - 1; j++)
        {
            plObjectComponent* ptObjectA = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtStagedEntities[uStart + j]);
            plObjectComponent* ptObjectB = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtStagedEntities[uStart + j + 1]);
            if (ptObjectA->tMesh.uIndex > ptObjectB->tMesh.uIndex)
            {
                plEntity tA = ptScene->sbtStagedEntities[uStart + j];
                plEntity tB = ptScene->sbtStagedEntities[uStart + j + 1];
                ptScene->sbtStagedEntities[uStart + j] = tB;
                ptScene->sbtStagedEntities[uStart + j + 1] = tA;
                bSwapped = true;
            }
        }
      
        // If no two elements were swapped, then break
        if (!bSwapped)
            break;
    }

    pl_end_cpu_sample(gptProfile, 0);
}

void
pl_renderer_update_scene_materials(plScene* ptScene, uint32_t uMaterialCount, const plEntity* atMaterials)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);
    for(uint32_t i = 0; i < uMaterialCount; i++)
    {
        const plEntity tMaterialComp = atMaterials[i];
        plMaterialComponent* ptMaterial = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tMaterialComponentType, tMaterialComp);

        uint32_t uMaterialIndex = (uint32_t)pl_hm_lookup(&ptScene->tMaterialHashmap, tMaterialComp.uData);
        if(uMaterialIndex == UINT32_MAX)
        {
            PL_ASSERT(false && "material isn't in scene");
        }
        else
        {
            // load textures
            plTextureHandle tBaseColorTex = gptData->tDummyTexture;
            plTextureHandle tNormalTex = gptData->tDummyTexture;
            plTextureHandle tEmissiveTex = gptData->tDummyTexture;
            plTextureHandle tMetallicRoughnessTex = gptData->tDummyTexture;
            plTextureHandle tOcclusionTex = gptData->tDummyTexture;

            if(gptResource->is_valid(ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_BASE_COLOR_MAP].tResource))
                tBaseColorTex = gptResource->get_texture(ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_BASE_COLOR_MAP].tResource);

            if(gptResource->is_valid(ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_NORMAL_MAP].tResource))
                tNormalTex = gptResource->get_texture(ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_NORMAL_MAP].tResource);

            if(gptResource->is_valid(ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_EMISSIVE_MAP].tResource))
                tEmissiveTex = gptResource->get_texture(ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_EMISSIVE_MAP].tResource);

            if(gptResource->is_valid(ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_METAL_ROUGHNESS_MAP].tResource))
                tMetallicRoughnessTex = gptResource->get_texture(ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_METAL_ROUGHNESS_MAP].tResource);

            if(gptResource->is_valid(ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_OCCLUSION_MAP].tResource))
                tOcclusionTex = gptResource->get_texture(ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_OCCLUSION_MAP].tResource);

            int iBaseColorTexIdx         = (int)pl__renderer_get_bindless_texture_index(ptScene, tBaseColorTex);
            int iNormalTexIdx            = (int)pl__renderer_get_bindless_texture_index(ptScene, tNormalTex);
            int iEmissiveTexIdx          = (int)pl__renderer_get_bindless_texture_index(ptScene, tEmissiveTex);
            int iMetallicRoughnessTexIdx = (int)pl__renderer_get_bindless_texture_index(ptScene, tMetallicRoughnessTex);
            int iOcclusionTexIdx         = (int)pl__renderer_get_bindless_texture_index(ptScene, tOcclusionTex);

            plGPUMaterial tGPUMaterial = {
                .fMetallicFactor          = ptMaterial->fMetalness,
                .fRoughnessFactor         = ptMaterial->fRoughness,
                .tBaseColorFactor         = ptMaterial->tBaseColor,
                .tEmissiveFactor          = ptMaterial->tEmissiveColor.rgb,
                .fAlphaCutoff             = ptMaterial->fAlphaCutoff,
                .fOcclusionStrength       = 1.0f,
                .fEmissiveStrength        = 1.0f,
                .iBaseColorUVSet          = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_BASE_COLOR_MAP].uUVSet,
                .iNormalUVSet             = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_NORMAL_MAP].uUVSet,
                .iEmissiveUVSet           = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_EMISSIVE_MAP].uUVSet,
                .iOcclusionUVSet          = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_OCCLUSION_MAP].uUVSet,
                .iMetallicRoughnessUVSet  = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_METAL_ROUGHNESS_MAP].uUVSet,
                .iBaseColorTexIdx         = iBaseColorTexIdx,
                .iNormalTexIdx            = iNormalTexIdx,
                .iEmissiveTexIdx          = iEmissiveTexIdx,
                .iMetallicRoughnessTexIdx = iMetallicRoughnessTexIdx,
                .iOcclusionTexIdx         = iOcclusionTexIdx
            };
            ptScene->sbtMaterialBuffer[uMaterialIndex] = tGPUMaterial;
        }
    }
    ptScene->uGPUMaterialDirty = gptGfx->get_frames_in_flight();
    pl_end_cpu_sample(gptProfile, 0);
}

void
pl_renderer_remove_objects_from_scene(plScene* ptScene, uint32_t uObjectCount, const plEntity* atObjects)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    for(uint32_t i = 0; i < uObjectCount; i++)
    {
        const plEntity tObject = atObjects[i];

        uint32_t uDrawableCount = pl_sb_size(ptScene->sbtStagedEntities);
        for(uint32_t j = 0; j < uDrawableCount; j++)
        {
            if(ptScene->sbtStagedEntities[j].uData == tObject.uData)
            {
                pl_hm_remove(&ptScene->tDrawableHashmap, tObject.uData);
                pl_sb_del_swap(ptScene->sbtStagedEntities, j);
                j--;
                uDrawableCount--;
            }
        }
    }

    pl__renderer_unstage_drawables(ptScene);
    pl__renderer_set_drawable_shaders(ptScene);
    pl__renderer_sort_drawables(ptScene);
    
    pl_end_cpu_sample(gptProfile, 0);
}

void
pl_renderer_update_scene_objects(plScene* ptScene, uint32_t uObjectCount, const plEntity* atObjects)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);
    pl__renderer_sort_drawables(ptScene);
    pl_end_cpu_sample(gptProfile, 0);
}

void
pl_renderer_add_materials_to_scene(plScene* ptScene, uint32_t uMaterialCount, const plEntity* atMaterials)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    pl_sb_reset(ptScene->sbtMaterialBuffer);
    for(uint32_t i = 0; i < uMaterialCount; i++)
    {
        const plEntity tMaterialComp = atMaterials[i];
        plMaterialComponent* ptMaterial = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tMaterialComponentType, tMaterialComp);

        // load textures from disk
        for(uint32_t uTextureSlot = 0; uTextureSlot < PL_TEXTURE_SLOT_COUNT; uTextureSlot++)
        {

            if(gptResource->is_valid(ptMaterial->atTextureMaps[uTextureSlot].tResource))
            {
                plTextureHandle tTextureHandle = gptResource->get_texture(ptMaterial->atTextureMaps[uTextureSlot].tResource);
                plTexture* ptTexture = gptGfx->get_texture(gptData->ptDevice, tTextureHandle);
                ptMaterial->atTextureMaps[uTextureSlot].uWidth = (uint32_t)ptTexture->tDesc.tDimensions.x;
                ptMaterial->atTextureMaps[uTextureSlot].uHeight = (uint32_t)ptTexture->tDesc.tDimensions.y;
            }
        }

        uint64_t uMaterialIndex = UINT64_MAX;

        // see if material already exists
        if(!pl_hm_has_key_ex(&ptScene->tMaterialHashmap, tMaterialComp.uData, &uMaterialIndex))
        {
            uint64_t ulValue = pl_hm_get_free_index(&ptScene->tMaterialHashmap);
            if(ulValue == PL_DS_HASH_INVALID)
            {
                ulValue = pl_sb_size(ptScene->sbtMaterialBuffer);
                pl_sb_add(ptScene->sbtMaterialBuffer);
            }

            uMaterialIndex = ulValue;

            // load textures
            plTextureHandle tBaseColorTex = gptData->tDummyTexture;
            plTextureHandle tNormalTex = gptData->tDummyTexture;
            plTextureHandle tEmissiveTex = gptData->tDummyTexture;
            plTextureHandle tMetallicRoughnessTex = gptData->tDummyTexture;
            plTextureHandle tOcclusionTex = gptData->tDummyTexture;

            if(gptResource->is_valid(ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_BASE_COLOR_MAP].tResource))
                tBaseColorTex = gptResource->get_texture(ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_BASE_COLOR_MAP].tResource);

            if(gptResource->is_valid(ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_NORMAL_MAP].tResource))
                tNormalTex = gptResource->get_texture(ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_NORMAL_MAP].tResource);

            if(gptResource->is_valid(ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_EMISSIVE_MAP].tResource))
                tEmissiveTex = gptResource->get_texture(ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_EMISSIVE_MAP].tResource);

            if(gptResource->is_valid(ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_METAL_ROUGHNESS_MAP].tResource))
                tMetallicRoughnessTex = gptResource->get_texture(ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_METAL_ROUGHNESS_MAP].tResource);

            if(gptResource->is_valid(ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_OCCLUSION_MAP].tResource))
                tOcclusionTex = gptResource->get_texture(ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_OCCLUSION_MAP].tResource);

            int iBaseColorTexIdx         = (int)pl__renderer_get_bindless_texture_index(ptScene, tBaseColorTex);
            int iNormalTexIdx            = (int)pl__renderer_get_bindless_texture_index(ptScene, tNormalTex);
            int iEmissiveTexIdx          = (int)pl__renderer_get_bindless_texture_index(ptScene, tEmissiveTex);
            int iMetallicRoughnessTexIdx = (int)pl__renderer_get_bindless_texture_index(ptScene, tMetallicRoughnessTex);
            int iOcclusionTexIdx         = (int)pl__renderer_get_bindless_texture_index(ptScene, tOcclusionTex);

            // create GPU material
            plGPUMaterial tGPUMaterial = {
                .fMetallicFactor          = ptMaterial->fMetalness,
                .fRoughnessFactor         = ptMaterial->fRoughness,
                .tBaseColorFactor         = ptMaterial->tBaseColor,
                .tEmissiveFactor          = ptMaterial->tEmissiveColor.rgb,
                .fAlphaCutoff             = ptMaterial->fAlphaCutoff,
                .fOcclusionStrength       = 1.0f,
                .fEmissiveStrength        = 1.0f,
                .iBaseColorUVSet          = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_BASE_COLOR_MAP].uUVSet,
                .iNormalUVSet             = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_NORMAL_MAP].uUVSet,
                .iEmissiveUVSet           = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_EMISSIVE_MAP].uUVSet,
                .iOcclusionUVSet          = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_OCCLUSION_MAP].uUVSet,
                .iMetallicRoughnessUVSet  = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_METAL_ROUGHNESS_MAP].uUVSet,
                .iBaseColorTexIdx         = iBaseColorTexIdx,
                .iNormalTexIdx            = iNormalTexIdx,
                .iEmissiveTexIdx          = iEmissiveTexIdx,
                .iMetallicRoughnessTexIdx = iMetallicRoughnessTexIdx,
                .iOcclusionTexIdx         = iOcclusionTexIdx
            };
            ptScene->sbtMaterialBuffer[uMaterialIndex] = tGPUMaterial;
            pl_hm_insert(&ptScene->tMaterialHashmap, tMaterialComp.uData, ulValue);
        }
    }
    pl_end_cpu_sample(gptProfile, 0);
}

plRendererRuntimeOptions*
pl_renderer_get_runtime_options(void)
{
    return &gptData->tRuntimeOptions;
}

void
pl_renderer_rebuild_bvh(plScene* ptScene)
{
    plComponentLibrary* ptLibrary = ptScene->ptComponentLibrary;

    plObjectComponent* ptComponents = NULL;
    const uint32_t uObjectCount = gptECS->get_components(ptScene->ptComponentLibrary, gptData->tObjectComponentType, (void**)&ptComponents, NULL);

    pl_sb_resize(ptScene->sbtBvhAABBs, uObjectCount);
    for(uint32_t j = 0; j < uObjectCount; j++)
    {
        plObjectComponent* ptObject = &ptComponents[j];
        ptScene->sbtBvhAABBs[j] = ptObject->tAABB;
    }

    gptBvh->build(&ptScene->tBvh, ptScene->sbtBvhAABBs, uObjectCount);

    pl_sb_reset(ptScene->sbtBvhAABBs); 
}

void
pl_renderer_show_skybox(plView* ptView)
{
    ptView->bShowSkybox = true;
}

static void
pl__object_update_job(plInvocationData tInvoData, void* pData, void* pGroupSharedMemory)
{
    plComponentLibrary* ptLibrary = pData;

    plObjectComponent* ptComponents = NULL;
    const uint32_t uComponentCount = gptECS->get_components(ptLibrary, gptData->tObjectComponentType, (void**)&ptComponents, NULL);

    plObjectComponent* ptObject = &ptComponents[tInvoData.uGlobalIndex];
    plTransformComponent* ptTransform = gptECS->get_component(ptLibrary, gptECS->get_ecs_type_key_transform(), ptObject->tTransform);
    plMeshComponent* ptMesh = gptECS->get_component(ptLibrary, gptMesh->get_ecs_type_key_mesh(), ptObject->tMesh);
    plSkinComponent* ptSkinComponent = gptECS->get_component(ptLibrary, gptData->tSkinComponentType, ptMesh->tSkinComponent);

    plMat4 tTransform = ptTransform->tWorld;

    const plVec3 tVerticies[] = {
        pl_mul_mat4_vec3(&tTransform, (plVec3){  ptMesh->tAABB.tMin.x, ptMesh->tAABB.tMin.y, ptMesh->tAABB.tMin.z }),
        pl_mul_mat4_vec3(&tTransform, (plVec3){  ptMesh->tAABB.tMax.x, ptMesh->tAABB.tMin.y, ptMesh->tAABB.tMin.z }),
        pl_mul_mat4_vec3(&tTransform, (plVec3){  ptMesh->tAABB.tMax.x, ptMesh->tAABB.tMax.y, ptMesh->tAABB.tMin.z }),
        pl_mul_mat4_vec3(&tTransform, (plVec3){  ptMesh->tAABB.tMin.x, ptMesh->tAABB.tMax.y, ptMesh->tAABB.tMin.z }),
        pl_mul_mat4_vec3(&tTransform, (plVec3){  ptMesh->tAABB.tMin.x, ptMesh->tAABB.tMin.y, ptMesh->tAABB.tMax.z }),
        pl_mul_mat4_vec3(&tTransform, (plVec3){  ptMesh->tAABB.tMax.x, ptMesh->tAABB.tMin.y, ptMesh->tAABB.tMax.z }),
        pl_mul_mat4_vec3(&tTransform, (plVec3){  ptMesh->tAABB.tMax.x, ptMesh->tAABB.tMax.y, ptMesh->tAABB.tMax.z }),
        pl_mul_mat4_vec3(&tTransform, (plVec3){  ptMesh->tAABB.tMin.x, ptMesh->tAABB.tMax.y, ptMesh->tAABB.tMax.z }),
    };

    // calculate AABB
    ptObject->tAABB.tMax = (plVec3){-FLT_MAX, -FLT_MAX, -FLT_MAX};
    ptObject->tAABB.tMin = (plVec3){FLT_MAX, FLT_MAX, FLT_MAX};
    
    for(uint32_t i = 0; i < 8; i++)
    {
        if(tVerticies[i].x > ptObject->tAABB.tMax.x) ptObject->tAABB.tMax.x = tVerticies[i].x;
        if(tVerticies[i].y > ptObject->tAABB.tMax.y) ptObject->tAABB.tMax.y = tVerticies[i].y;
        if(tVerticies[i].z > ptObject->tAABB.tMax.z) ptObject->tAABB.tMax.z = tVerticies[i].z;
        if(tVerticies[i].x < ptObject->tAABB.tMin.x) ptObject->tAABB.tMin.x = tVerticies[i].x;
        if(tVerticies[i].y < ptObject->tAABB.tMin.y) ptObject->tAABB.tMin.y = tVerticies[i].y;
        if(tVerticies[i].z < ptObject->tAABB.tMin.z) ptObject->tAABB.tMin.z = tVerticies[i].z;
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
pl_run_skin_update_system(plComponentLibrary* ptLibrary)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);
    plSkinComponent* ptComponents = NULL;
    const plEntity* ptEntities = NULL;
    const uint32_t uComponentCount = gptECS->get_components(ptLibrary, gptData->tSkinComponentType, (void**)&ptComponents, &ptEntities);

    const plEcsTypeKey tTransformComponentType = gptECS->get_ecs_type_key_transform();

    for(uint32_t i = 0; i < uComponentCount; i++)
    {
        plSkinComponent* ptSkinComponent = &ptComponents[i];

        // calculate AABB
        ptSkinComponent->tAABB.tMax = (plVec3){-FLT_MAX, -FLT_MAX, -FLT_MAX};
        ptSkinComponent->tAABB.tMin = (plVec3){FLT_MAX, FLT_MAX, FLT_MAX};

        plTransformComponent* ptTransform = gptECS->get_component(ptLibrary, tTransformComponentType, ptEntities[i]);
        if(ptTransform)
        {
            plMat4 tInverseWorldTransform = pl_mat4_invert(&ptTransform->tWorld);
            for(uint32_t j = 0; j < pl_sb_size(ptSkinComponent->sbtJoints); j++)
            {
                plEntity tJointEntity = ptSkinComponent->sbtJoints[j];
                plTransformComponent* ptJointComponent = gptECS->get_component(ptLibrary, tTransformComponentType, tJointEntity);

                const plMat4* ptIBM = &ptSkinComponent->sbtInverseBindMatrices[j];

                plMat4 tJointMatrix = pl_mul_mat4_3(&tInverseWorldTransform, &ptJointComponent->tWorld, ptIBM);

                plMat4 tInvertJoint = pl_mat4_invert(&tJointMatrix);
                plMat4 tNormalMatrix = pl_mat4_transpose(&tInvertJoint);
                ptSkinComponent->sbtTextureData[j*2] = tJointMatrix;
                ptSkinComponent->sbtTextureData[j*2 + 1] = tNormalMatrix;

                plVec3 tBonePos = ptJointComponent->tWorld.col[3].xyz;

                const float fBoneRadius = 1.0f;
                plAABB tBoneAABB = {
                    .tMin = {tBonePos.x - fBoneRadius, tBonePos.y - fBoneRadius, tBonePos.z - fBoneRadius},
                    .tMax = {tBonePos.x + fBoneRadius, tBonePos.y + fBoneRadius, tBonePos.z + fBoneRadius},
                };

                if(tBoneAABB.tMin.x < ptSkinComponent->tAABB.tMin.x) ptSkinComponent->tAABB.tMin.x = tBoneAABB.tMin.x;
                if(tBoneAABB.tMin.y < ptSkinComponent->tAABB.tMin.y) ptSkinComponent->tAABB.tMin.y = tBoneAABB.tMin.y;
                if(tBoneAABB.tMin.z < ptSkinComponent->tAABB.tMin.z) ptSkinComponent->tAABB.tMin.z = tBoneAABB.tMin.z;
                if(tBoneAABB.tMax.x > ptSkinComponent->tAABB.tMax.x) ptSkinComponent->tAABB.tMax.x = tBoneAABB.tMax.x;
                if(tBoneAABB.tMax.y > ptSkinComponent->tAABB.tMax.y) ptSkinComponent->tAABB.tMax.y = tBoneAABB.tMax.y;
                if(tBoneAABB.tMax.z > ptSkinComponent->tAABB.tMax.z) ptSkinComponent->tAABB.tMax.z = tBoneAABB.tMax.z;
            }
        }
    }

    pl_end_cpu_sample(gptProfile, 0);
}

void
pl_run_object_update_system(plComponentLibrary* ptLibrary)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);
    
    plObjectComponent* ptComponents = NULL;
    const uint32_t uComponentCount = gptECS->get_components(ptLibrary, gptData->tObjectComponentType, (void**)&ptComponents, NULL);

    plAtomicCounter* ptCounter = NULL;
    plJobDesc tJobDesc = {
        .task = pl__object_update_job,
        .pData = ptLibrary
    };
    gptJob->dispatch_batch(uComponentCount, 0, tJobDesc, &ptCounter);
    gptJob->wait_for_counter(ptCounter);

    pl_end_cpu_sample(gptProfile, 0);
}

void
pl_renderer_register_system(void)
{

    const plComponentDesc tObjectDesc = {
        .pcName = "Object",
        .szSize = sizeof(plObjectComponent)
    };

    static const plObjectComponent tObjectComponentDefault = {
        .tFlags     = PL_OBJECT_FLAGS_RENDERABLE | PL_OBJECT_FLAGS_CAST_SHADOW | PL_OBJECT_FLAGS_DYNAMIC,
        .tMesh      = {UINT32_MAX, UINT32_MAX},
        .tTransform = {UINT32_MAX, UINT32_MAX}
    };
    gptData->tObjectComponentType = gptECS->register_type(tObjectDesc, &tObjectComponentDefault);

    const plComponentDesc tMaterialDesc = {
        .pcName = "Material",
        .szSize = sizeof(plMaterialComponent)
    };

    static const plMaterialComponent tMaterialComponentDefault = {
        .tBlendMode            = PL_BLEND_MODE_OPAQUE,
        .tFlags                = PL_MATERIAL_FLAG_CAST_SHADOW | PL_MATERIAL_FLAG_CAST_RECEIVE_SHADOW,
        .tShaderType           = PL_SHADER_TYPE_PBR,
        .tBaseColor            = {1.0f, 1.0f, 1.0f, 1.0f},
        .tEmissiveColor        = {0.0f, 0.0f, 0.0f, 0.0f},
        .fRoughness            = 1.0f,
        .fMetalness            = 1.0f,
        .fNormalMapStrength    = 1.0f,
        .fOcclusionMapStrength = 1.0f,
        .fAlphaCutoff          = 0.5f,
        .atTextureMaps         = {0}
    };
    gptData->tMaterialComponentType = gptECS->register_type(tMaterialDesc, &tMaterialComponentDefault);

    const plComponentDesc tSkinDesc = {
        .pcName = "Skin",
        .szSize = sizeof(plSkinComponent),
        .cleanup = pl__renderer_skin_cleanup,
        .reset = pl__renderer_skin_cleanup,
    };

    gptData->tSkinComponentType = gptECS->register_type(tSkinDesc, NULL);

    const plComponentDesc tLightDesc = {
        .pcName = "Light",
        .szSize = sizeof(plLightComponent)
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
        .uCascadeCount       = 0,
        .tFlags              = 0,
        .afCascadeSplits     = {0}
    };
    gptData->tLightComponentType = gptECS->register_type(tLightDesc, &tLightComponentDefault);

    const plComponentDesc tProbeDesc = {
        .pcName = "Environment Probe",
        .szSize = sizeof(plEnvironmentProbeComponent)
    };

    static const plEnvironmentProbeComponent tProbeDefaultComponent = {
        .fRange      = 10.0f,
        .uResolution = 128,
        .uSamples    = 128,
        .uInterval   = 1,
        .tFlags      = PL_ENVIRONMENT_PROBE_FLAGS_DIRTY
    };
    gptData->tEnvironmentProbeComponentType = gptECS->register_type(tProbeDesc, &tProbeDefaultComponent);
}

void
pl_run_light_update_system(plComponentLibrary* ptLibrary)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    plLightComponent* ptComponents = NULL;
    const plEntity* ptEntities = NULL;
    const uint32_t uComponentCount = gptECS->get_components(ptLibrary, gptData->tLightComponentType, (void**)&ptComponents, &ptEntities);

    const plEcsTypeKey tTransformKey = gptECS->get_ecs_type_key_transform();
    for(uint32_t i = 0; i < uComponentCount; i++)
    {
        plEntity tEntity = ptEntities[i];
        if(gptECS->has_component(ptLibrary, tTransformKey, tEntity))
        {
            plLightComponent* ptLight = &ptComponents[i];
            plTransformComponent* ptTransform = gptECS->get_component(ptLibrary, tTransformKey, tEntity);
            ptLight->tPosition = ptTransform->tWorld.col[3].xyz;

            // TODO: direction
        }
    }
    pl_end_cpu_sample(gptProfile, 0);
}

void
pl_run_probe_update_system(plComponentLibrary* ptLibrary)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    // plEnvironmentProbeComponent* sbtComponents = ptLibrary->tEnvironmentProbeCompManager.pComponents;

    // const uint32_t uComponentCount = pl_sb_size(sbtComponents);
    // for(uint32_t i = 0; i < uComponentCount; i++)
    // {
    //     plEntity tEntity = ptLibrary->tEnvironmentProbeCompManager.sbtEntities[i];
    //     if(pl_ecs_has_entity(&ptLibrary->tTransformComponentManager, tEntity))
    //     {
    //         plEnvironmentProbeComponent* ptProbe = &sbtComponents[i];
    //         plTransformComponent* ptTransform = gptECS->get_component(ptLibrary, gptEcsCoreCtx->tTransformComponentType, tEntity);
    //     }
    // }
    pl_end_cpu_sample(gptProfile, 0); 
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_renderer_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    plRendererI tApi = {0};
    tApi.initialize                    = pl_renderer_initialize;
    tApi.cleanup                       = pl_renderer_cleanup;
    tApi.create_scene                  = pl_renderer_create_scene;
    tApi.cleanup_scene                 = pl_renderer_cleanup_scene;
    tApi.add_drawable_objects_to_scene = pl_renderer_add_drawable_objects_to_scene;
    tApi.add_materials_to_scene        = pl_renderer_add_materials_to_scene;
    tApi.remove_objects_from_scene     = pl_renderer_remove_objects_from_scene;
    tApi.update_scene_objects          = pl_renderer_update_scene_objects;
    tApi.update_scene_materials        = pl_renderer_update_scene_materials;
    tApi.create_view                   = pl_renderer_create_view;
    tApi.cleanup_view                  = pl_renderer_cleanup_view;
    tApi.begin_frame                   = pl_renderer_begin_frame;
    tApi.load_skybox_from_panorama     = pl_renderer_load_skybox_from_panorama;
    tApi.finalize_scene                = pl_renderer_finalize_scene;
    tApi.reload_scene_shaders          = pl_renderer_reload_scene_shaders;
    tApi.outline_entities              = pl_renderer_outline_entities;
    tApi.prepare_scene                 = pl_renderer_prepare_scene;
    tApi.prepare_view                  = pl_renderer_prepare_view;
    tApi.render_view                   = pl_renderer_render_view;
    tApi.get_view_color_texture        = pl_renderer_get_view_color_texture;
    tApi.resize_view                   = pl_renderer_resize_view;
    tApi.get_debug_drawlist            = pl_renderer_get_debug_drawlist;
    tApi.get_gizmo_drawlist            = pl_renderer_get_gizmo_drawlist;
    tApi.update_hovered_entity         = pl_renderer_update_hovered_entity;
    tApi.get_hovered_entity            = pl_renderer_get_hovered_entity;
    tApi.get_runtime_options           = pl_renderer_get_runtime_options;
    tApi.rebuild_scene_bvh             = pl_renderer_rebuild_bvh;
    tApi.debug_draw_lights             = pl_renderer_debug_draw_lights;
    tApi.debug_draw_all_bound_boxes    = pl_renderer_debug_draw_all_bound_boxes;
    tApi.debug_draw_bvh                = pl_renderer_debug_draw_bvh;
    tApi.show_skybox                   = pl_renderer_show_skybox;
    tApi.register_ecs_system                = pl_renderer_register_system;
    tApi.create_material                    = pl_renderer_create_material;
    tApi.create_skin                        = pl_renderer_create_skin;
    tApi.create_directional_light           = pl_renderer_create_directional_light;
    tApi.create_point_light                 = pl_renderer_create_point_light;
    tApi.create_spot_light                  = pl_renderer_create_spot_light;
    tApi.create_environment_probe           = pl_renderer_create_environment_probe;
    tApi.create_object                      = pl_renderer_create_object;
    tApi.copy_object                        = pl_renderer_copy_object;
    tApi.get_ecs_type_key_material          = pl_renderer_get_ecs_type_key_material;
    tApi.get_ecs_type_key_skin              = pl_renderer_get_ecs_type_key_skin;
    tApi.get_ecs_type_key_light             = pl_renderer_get_ecs_type_key_light;
    tApi.get_ecs_type_key_environment_probe = pl_renderer_get_ecs_type_key_environment_probe;
    tApi.run_light_update_system               = pl_run_light_update_system;
    tApi.run_environment_probe_update_system   = pl_run_probe_update_system;
    tApi.get_ecs_type_key_object             = pl_renderer_get_ecs_type_key_object;
    tApi.run_skin_update_system                = pl_run_skin_update_system;
    tApi.run_object_update_system              = pl_run_object_update_system;
    pl_set_api(ptApiRegistry, plRendererI, &tApi);

    // core apis
    #ifndef PL_UNITY_BUILD
        gptDataRegistry  = pl_get_api_latest(ptApiRegistry, plDataRegistryI);
        gptIOI           = pl_get_api_latest(ptApiRegistry, plIOI);
        gptImage         = pl_get_api_latest(ptApiRegistry, plImageI);
        gptMemory        = pl_get_api_latest(ptApiRegistry, plMemoryI);
        gptGpuAllocators = pl_get_api_latest(ptApiRegistry, plGPUAllocatorsI);
        gptIO            = gptIOI->get_io();
        gptStats         = pl_get_api_latest(ptApiRegistry, plStatsI);
        gptImage         = pl_get_api_latest(ptApiRegistry, plImageI);
        gptJob           = pl_get_api_latest(ptApiRegistry, plJobI);
        gptProfile       = pl_get_api_latest(ptApiRegistry, plProfileI);
        gptLog           = pl_get_api_latest(ptApiRegistry, plLogI);
        gptRect          = pl_get_api_latest(ptApiRegistry, plRectPackI);
        gptECS           = pl_get_api_latest(ptApiRegistry, plEcsI);
        gptCamera        = pl_get_api_latest(ptApiRegistry, plCameraI);
        gptDraw          = pl_get_api_latest(ptApiRegistry, plDrawI);
        gptDrawBackend   = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
        gptGfx           = pl_get_api_latest(ptApiRegistry, plGraphicsI);
        gptResource      = pl_get_api_latest(ptApiRegistry, plResourceI);
        gptShader        = pl_get_api_latest(ptApiRegistry, plShaderI);
        gptConsole       = pl_get_api_latest(ptApiRegistry, plConsoleI);
        gptScreenLog     = pl_get_api_latest(ptApiRegistry, plScreenLogI);
        gptBvh           = pl_get_api_latest(ptApiRegistry, plBVHI);
        gptAnimation     = pl_get_api_latest(ptApiRegistry, plAnimationI);
        gptMesh          = pl_get_api_latest(ptApiRegistry, plMeshI);
        gptShaderVariant   = pl_get_api_latest(ptApiRegistry, plShaderVariantI);
        gptVfs           = pl_get_api_latest(ptApiRegistry, plVfsI);
        gptStarter       = pl_get_api_latest(ptApiRegistry, plStarterI);
    #endif

    if(bReload)
    {
        gptData = gptDataRegistry->get_data("ref renderer data");
    }   
}

PL_EXPORT void
pl_unload_renderer_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;
        
    const plRendererI* ptApi = pl_get_api_latest(ptApiRegistry, plRendererI);
    ptApiRegistry->remove_api(ptApi);
}

//-----------------------------------------------------------------------------
// [SECTION] unity
//-----------------------------------------------------------------------------

#include "pl_renderer_internal.c"

#ifndef PL_UNITY_BUILD

    #define PL_MEMORY_IMPLEMENTATION
    #include "pl_memory.h"
    #undef PL_MEMORY_IMPLEMENTATION

    #ifdef PL_USE_STB_SPRINTF
        #define STB_SPRINTF_IMPLEMENTATION
        #include "stb_sprintf.h"
        #undef STB_SPRINTF_IMPLEMENTATION
    #endif

#endif
